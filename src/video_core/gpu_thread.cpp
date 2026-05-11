// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <optional>
#include <utility>

#include "common/assert.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/frontend/graphics_context.h"
#include "core/game_settings.h"
#include "core/hardware_properties.h"
#include "video_core/control/scheduler.h"
#include "video_core/dma_pusher.h"
#include "video_core/gpu.h"
#include "video_core/gpu_thread.h"
#include "video_core/host1x/host1x.h"
#include "video_core/renderer_base.h"

namespace VideoCommon::GPUThread {

namespace {
constexpr size_t default_max_queued_cache_invalidations = 64;
constexpr u64 default_max_coalesced_cache_invalidation_span = 256ULL * 1024ULL;

bool IsCacheInvalidationCommand(const CommandData& command) {
    return std::holds_alternative<InvalidateRegionCommand>(command) ||
           std::holds_alternative<FlushAndInvalidateRegionCommand>(command);
}

std::optional<std::pair<DAddr, u64>> GetCacheInvalidationRange(const CommandData& command) {
    if (const auto* invalidate = std::get_if<InvalidateRegionCommand>(&command)) {
        return std::pair{invalidate->addr, invalidate->size};
    }
    if (const auto* flush_invalidate = std::get_if<FlushAndInvalidateRegionCommand>(&command)) {
        return std::pair{flush_invalidate->addr, flush_invalidate->size};
    }
    return std::nullopt;
}

bool TryMergeCacheInvalidation(DAddr& pending_addr, u64& pending_size, DAddr addr, u64 size,
                               u64 max_coalesced_span) {
    const DAddr pending_end = pending_addr + pending_size;
    const DAddr range_end = addr + size;
    if (pending_end < addr || range_end < pending_addr) {
        return false;
    }

    const DAddr merged_addr = std::min(pending_addr, addr);
    const DAddr merged_end = std::max(pending_end, range_end);
    if (merged_end - merged_addr > max_coalesced_span) {
        return false;
    }

    pending_addr = merged_addr;
    pending_size = merged_end - merged_addr;
    return true;
}
} // Anonymous namespace

/// Runs the GPU thread
static void RunThread(std::stop_token stop_token, Core::System& system,
                      VideoCore::RendererBase& renderer, Core::Frontend::GraphicsContext& context,
                      Tegra::Control::Scheduler& scheduler, SynchState& state) {
    Common::SetCurrentThreadName("GPU");
    Common::SetCurrentThreadPriority(Core::GameSettings::UseThermalAwareThreadScheduling()
                                         ? Common::ThreadPriority::High
                                         : Common::ThreadPriority::Critical);
    if (Core::GameSettings::UseThermalAwareThreadScheduling()) {
        Common::SetCurrentThreadPowerThrottling(false);
        if (Core::GameSettings::ReservePrimaryCoreForVulkanSubmission()) {
            Common::PinCurrentThreadToPhysicalCoreSibling(Core::Hardware::NUM_CPU_CORES - 1);
        } else {
            Common::PinCurrentThreadToPhysicalCoreSiblings();
        }
    }
    system.RegisterHostThread();

    auto current_context = context.Acquire();
    VideoCore::RasterizerInterface* const rasterizer = renderer.ReadRasterizer();

    CommandDataContainer next;

    const auto signal_command = [&](u64 fence, bool block) {
        state.signaled_fence.store(fence);
        if (block) {
            // We have to lock the write_lock to ensure that the condition_variable wait not get a
            // race between the check and the lock itself.
            std::scoped_lock lk{state.write_lock};
            state.cv.notify_all();
        }
    };

    const auto process_non_cache_command = [&](CommandDataContainer& command) {
        if (auto* submit_list = std::get_if<SubmitListCommand>(&command.data)) {
            scheduler.Push(submit_list->channel, std::move(submit_list->entries));
        } else if (std::holds_alternative<GPUTickCommand>(command.data)) {
            system.GPU().TickWork();
        } else if (const auto* flush = std::get_if<FlushRegionCommand>(&command.data)) {
            rasterizer->FlushRegion(flush->addr, flush->size);
        } else {
            ASSERT(false);
        }
        signal_command(command.fence, command.block);
    };

    const auto process_cache_commands = [&](CommandDataContainer& command) {
        DAddr pending_addr{};
        u64 pending_size{};
        u64 last_fence{};
        bool notify_block = false;
        const u64 max_coalesced_span = Core::GameSettings::GetGpuCacheInvalidationCoalesceSpan(
            default_max_coalesced_cache_invalidation_span);

        const auto flush_pending = [&] {
            if (pending_size != 0) {
                rasterizer->OnCacheInvalidation(pending_addr, pending_size);
                pending_size = 0;
            }
        };

        const auto absorb_cache_command = [&](CommandDataContainer& cache_command) {
            const auto range = GetCacheInvalidationRange(cache_command.data);
            ASSERT(range.has_value());
            const auto [addr, size] = *range;
            if (addr != 0 && size != 0) {
                if (pending_size == 0) {
                    pending_addr = addr;
                    pending_size = size;
                } else if (!TryMergeCacheInvalidation(pending_addr, pending_size, addr, size,
                                                      max_coalesced_span)) {
                    flush_pending();
                    pending_addr = addr;
                    pending_size = size;
                }
            }
            last_fence = cache_command.fence;
            notify_block = notify_block || cache_command.block;
            state.pending_invalidation_count.fetch_sub(1, std::memory_order_relaxed);
        };

        absorb_cache_command(command);
        while (state.queue.TryPop(next)) {
            if (!IsCacheInvalidationCommand(next.data)) {
                flush_pending();
                signal_command(last_fence, notify_block);
                process_non_cache_command(next);
                return;
            }
            absorb_cache_command(next);
        }

        flush_pending();
        signal_command(last_fence, notify_block);
    };

    const auto process_command = [&](CommandDataContainer& command) {
        if (IsCacheInvalidationCommand(command.data)) {
            process_cache_commands(command);
            return;
        }
        process_non_cache_command(command);
    };

    while (!stop_token.stop_requested()) {
        state.queue.PopWait(next, stop_token);
        if (stop_token.stop_requested()) {
            break;
        }
        process_command(next);
        while (state.queue.TryPop(next)) {
            process_command(next);
        }
    }
}

ThreadManager::ThreadManager(Core::System& system_, bool is_async_)
    : system{system_}, is_async{is_async_} {}

ThreadManager::~ThreadManager() = default;

void ThreadManager::StartThread(VideoCore::RendererBase& renderer,
                                Core::Frontend::GraphicsContext& context,
                                Tegra::Control::Scheduler& scheduler) {
    rasterizer = renderer.ReadRasterizer();
    thread = std::jthread(RunThread, std::ref(system), std::ref(renderer), std::ref(context),
                          std::ref(scheduler), std::ref(state));
}

void ThreadManager::SubmitList(s32 channel, Tegra::CommandList&& entries) {
    PushCommand(SubmitListCommand(channel, std::move(entries)));
}

void ThreadManager::FlushRegion(DAddr addr, u64 size) {
    if (!is_async) {
        // Always flush with synchronous GPU mode
        PushCommand(FlushRegionCommand(addr, size));
    }
    return;
}

void ThreadManager::TickGPU() {
    PushCommand(GPUTickCommand());
}

void ThreadManager::InvalidateRegion(DAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    if (is_async && Core::GameSettings::UseQueuedGpuCacheInvalidation() &&
        TryPushCommand(InvalidateRegionCommand(addr, size))) {
        return;
    }
    rasterizer->OnCacheInvalidation(addr, size);
}

void ThreadManager::FlushAndInvalidateRegion(DAddr addr, u64 size) {
    // Skip flush on asynch mode, as FlushAndInvalidateRegion is not used for anything too important
    if (addr == 0 || size == 0) {
        return;
    }
    rasterizer->OnCacheInvalidation(addr, size);
}

u64 ThreadManager::PushCommand(CommandData&& command_data, bool block) {
    if (!is_async) {
        // In synchronous GPU mode, block the caller until the command has executed
        block = true;
    }

    std::unique_lock lk(state.write_lock);
    const u64 fence{++state.last_fence};
    const bool is_cache_invalidation = IsCacheInvalidationCommand(command_data);
    if (is_cache_invalidation) {
        state.pending_invalidation_count.fetch_add(1, std::memory_order_relaxed);
    }
    state.queue.EmplaceWait(std::move(command_data), fence, block);

    if (block) {
        state.cv.wait(lk, thread.get_stop_token(), [this, fence] {
            return fence <= state.signaled_fence.load(std::memory_order_relaxed);
        });
    }

    return fence;
}

bool ThreadManager::TryPushCommand(CommandData&& command_data) {
    if (!is_async) {
        return false;
    }

    const bool is_cache_invalidation = IsCacheInvalidationCommand(command_data);
    std::unique_lock lk(state.write_lock);
    if (is_cache_invalidation) {
        const size_t pending =
            state.pending_invalidation_count.load(std::memory_order_relaxed);
        const size_t limit = Core::GameSettings::GetQueuedGpuCacheInvalidationLimit(
            default_max_queued_cache_invalidations);
        if (limit == 0 || pending >= limit) {
            return false;
        }
        state.pending_invalidation_count.fetch_add(1, std::memory_order_relaxed);
    }

    const u64 fence{++state.last_fence};
    if (state.queue.TryEmplace(std::move(command_data), fence, false)) {
        return true;
    }

    if (is_cache_invalidation) {
        state.pending_invalidation_count.fetch_sub(1, std::memory_order_relaxed);
    }
    return false;
}

} // namespace VideoCommon::GPUThread
