// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <mutex>
#include <utility>
#include <vector>

#include "core/device_memory_manager.h"

namespace Core {

class GPUDirtyMemoryManager {
public:
    GPUDirtyMemoryManager() : current{default_transform} {
        back_buffer.reserve(256);
        front_buffer.reserve(256);
    }

    ~GPUDirtyMemoryManager() = default;

    void Collect(PAddr address, size_t size) {
        while (size != 0) {
            const size_t page_offset = address & page_mask;
            const size_t page_size_remaining = page_size - page_offset;
            const size_t collect_size = std::min(size, page_size_remaining);
            CollectPage(address, collect_size);
            address += collect_size;
            size -= collect_size;
        }
    }

    bool HasPending() const {
        return collect_generation.load(std::memory_order_acquire) !=
               gathered_generation.load(std::memory_order_acquire);
    }

    template <typename Callback>
    void Gather(Callback&& callback) {
        const u64 gathered = collect_generation.load(std::memory_order_acquire);
        {
            std::scoped_lock lk(guard);
            TransformAddress t = current.exchange(default_transform, std::memory_order_relaxed);
            front_buffer.swap(back_buffer);
            if (IsValid(t)) {
                front_buffer.emplace_back(t);
            }
        }
        if (front_buffer.empty()) {
            gathered_generation.store(gathered, std::memory_order_release);
            return;
        }

        if (front_buffer.size() > 1) {
            std::sort(front_buffer.begin(), front_buffer.end(), [](const auto& lhs,
                                                                   const auto& rhs) {
                return lhs.address < rhs.address;
            });
        }

        PAddr pending_address{};
        size_t pending_size{};
        const auto flush_pending = [&] {
            if (pending_size != 0) {
                callback(pending_address, pending_size);
                pending_size = 0;
            }
        };

        for (auto& transform : front_buffer) {
            size_t offset = 0;
            u64 mask = transform.mask;
            while (mask != 0) {
                const size_t empty_bits = std::countr_zero(mask);
                offset += empty_bits << align_bits;
                mask = mask >> empty_bits;

                const size_t continuous_bits = std::countr_one(mask);
                const PAddr address = (static_cast<PAddr>(transform.address) << page_bits) + offset;
                const size_t range_size = continuous_bits << align_bits;
                if (pending_size != 0 && pending_address + pending_size == address) {
                    pending_size += range_size;
                } else {
                    flush_pending();
                    pending_address = address;
                    pending_size = range_size;
                }
                mask = continuous_bits < align_size ? (mask >> continuous_bits) : 0;
                offset += continuous_bits << align_bits;
            }
        }
        flush_pending();
        front_buffer.clear();
        gathered_generation.store(gathered, std::memory_order_release);
    }

private:
    struct alignas(8) TransformAddress {
        u32 address;
        u32 mask;
    };

    void CollectPage(PAddr address, size_t size) {
        TransformAddress t = BuildTransform(address, size);
        TransformAddress tmp, original;
        do {
            tmp = current.load(std::memory_order_acquire);
            original = tmp;
            if (tmp.address != t.address) {
                if (IsValid(tmp)) {
                    std::scoped_lock lk(guard);
                    back_buffer.emplace_back(tmp);
                    current.exchange(t, std::memory_order_relaxed);
                    collect_generation.fetch_add(1, std::memory_order_release);
                    return;
                }
                tmp.address = t.address;
                tmp.mask = 0;
            }
            if ((tmp.mask | t.mask) == tmp.mask) {
                return;
            }
            tmp.mask |= t.mask;
        } while (!current.compare_exchange_weak(original, tmp, std::memory_order_release,
                                                std::memory_order_relaxed));
        collect_generation.fetch_add(1, std::memory_order_release);
    }

    constexpr static size_t page_bits = DEVICE_PAGEBITS - 1;
    constexpr static size_t page_size = 1ULL << page_bits;
    constexpr static size_t page_mask = page_size - 1;

    constexpr static size_t align_bits = 6U;
    constexpr static size_t align_size = 1U << align_bits;
    constexpr static size_t align_mask = align_size - 1;
    constexpr static TransformAddress default_transform = {.address = ~0U, .mask = 0U};

    static bool IsValid(TransformAddress transform) {
        return transform.mask != 0 && transform.address < (1ULL << (39 - page_bits));
    }

    template <typename T>
    T CreateMask(size_t top_bit, size_t minor_bit) {
        top_bit = std::min(top_bit, sizeof(T) * 8);
        T mask = ~T(0);
        mask <<= (sizeof(T) * 8 - top_bit);
        mask >>= (sizeof(T) * 8 - top_bit);
        mask >>= minor_bit;
        mask <<= minor_bit;
        return mask;
    }

    TransformAddress BuildTransform(PAddr address, size_t size) {
        const size_t minor_address = address & page_mask;
        const size_t minor_bit = minor_address >> align_bits;
        const size_t top_bit = (minor_address + size + align_mask) >> align_bits;
        TransformAddress result{};
        result.address = static_cast<u32>(address >> page_bits);
        result.mask = CreateMask<u32>(top_bit, minor_bit);
        return result;
    }

    std::atomic<TransformAddress> current{};
    std::atomic<u64> collect_generation{};
    std::atomic<u64> gathered_generation{};
    std::mutex guard;
    std::vector<TransformAddress> back_buffer;
    std::vector<TransformAddress> front_buffer;
};

} // namespace Core
