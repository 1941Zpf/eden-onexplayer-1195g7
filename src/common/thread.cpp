// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "common/error.h"
#include "common/logging.h"
#include "common/assert.h"
#include "common/thread.h"
#ifdef __APPLE__
#include <mach/mach.h>
#elif defined(__HAIKU__)
#include <kernel/OS.h>
#elif defined(_WIN32)
#include <windows.h>
#include "common/string_util.h"
#else
#if defined(__FreeBSD__)
#include <sys/cpuset.h>
#include <sys/_cpuset.h>
#include <pthread_np.h>
#elif defined(__DragonFly__) || defined(__OpenBSD__) || defined(__Bitrig__)
#include <pthread_np.h>
#endif
#include <pthread.h>
#include <sched.h>
#endif
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef __FreeBSD__
#   define cpu_set_t cpuset_t
#endif

namespace Common {

#ifdef _WIN32
namespace {

DWORD_PTR LowestSetBit(DWORD_PTR mask) {
    return mask & (~mask + 1);
}

std::vector<DWORD_PTR> GetPhysicalCoreAffinityMasks() {
    DWORD buffer_size = 0;
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &buffer_size) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer_size == 0) {
        return {};
    }

    std::vector<u8> buffer(buffer_size);
    auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &buffer_size)) {
        return {};
    }

    std::vector<DWORD_PTR> masks;
    for (DWORD offset = 0; offset < buffer_size;) {
        const auto* current =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() +
                                                                             offset);
        if (current->Relationship == RelationProcessorCore &&
            current->Processor.GroupCount > 0 &&
            current->Processor.GroupMask[0].Group == 0 &&
            current->Processor.GroupMask[0].Mask != 0) {
            masks.push_back(static_cast<DWORD_PTR>(current->Processor.GroupMask[0].Mask));
        }
        offset += current->Size;
    }

    return masks;
}

std::vector<DWORD_PTR> GetPhysicalCoreSiblingAffinityMasks() {
    std::vector<DWORD_PTR> sibling_masks;
    for (const DWORD_PTR core_mask : GetPhysicalCoreAffinityMasks()) {
        const DWORD_PTR sibling_mask = core_mask & ~LowestSetBit(core_mask);
        if (sibling_mask != 0) {
            sibling_masks.push_back(sibling_mask);
        }
    }
    return sibling_masks;
}

DWORD_PTR GetPhysicalCoreSiblingAffinityMask() {
    DWORD_PTR sibling_mask = 0;
    for (const DWORD_PTR core_mask : GetPhysicalCoreSiblingAffinityMasks()) {
        sibling_mask |= core_mask;
    }
    return sibling_mask;
}

} // Anonymous namespace
#endif

void SetCurrentThreadPriority(ThreadPriority new_priority) {
#ifdef _WIN32
    int windows_priority = [&]() {
        switch (new_priority) {
        case ThreadPriority::Low: return THREAD_PRIORITY_BELOW_NORMAL;
        case ThreadPriority::Normal: return THREAD_PRIORITY_NORMAL;
        case ThreadPriority::High: return THREAD_PRIORITY_ABOVE_NORMAL;
        case ThreadPriority::VeryHigh: return THREAD_PRIORITY_HIGHEST;
        case ThreadPriority::Critical: return THREAD_PRIORITY_TIME_CRITICAL;
        default: return THREAD_PRIORITY_NORMAL;
        }
    }();
    SetThreadPriority(GetCurrentThread(), windows_priority);
#elif defined(__HAIKU__)
    // TODO: We have priorities for 3D rendering applications - may help lavapipe?
    int priority = [&]() {
        switch (new_priority) {
        case ThreadPriority::Low: return B_LOW_PRIORITY;
        case ThreadPriority::Normal: return B_NORMAL_PRIORITY;
        case ThreadPriority::High: return B_DISPLAY_PRIORITY;
        case ThreadPriority::VeryHigh: return B_URGENT_DISPLAY_PRIORITY;
        case ThreadPriority::Critical: return B_URGENT_PRIORITY;
        default: return B_NORMAL_PRIORITY;
        }
    }();
    set_thread_priority(find_thread(NULL), priority);
#else
    pthread_t this_thread = pthread_self();
    const auto scheduling_type = SCHED_OTHER;
    s32 max_prio = sched_get_priority_max(scheduling_type);
    s32 min_prio = sched_get_priority_min(scheduling_type);
    u32 level = (std::max)(u32(new_priority) + 1, 4U);

    struct sched_param params;
    if (max_prio > min_prio) {
        params.sched_priority = min_prio + ((max_prio - min_prio) * level) / 4;
    } else {
        params.sched_priority = min_prio - ((min_prio - max_prio) * level) / 4;
    }

    pthread_setschedparam(this_thread, scheduling_type, &params);
#endif
}

void SetCurrentThreadPowerThrottling(bool throttled) {
#if defined(_WIN32) && defined(THREAD_POWER_THROTTLING_CURRENT_VERSION)
    THREAD_POWER_THROTTLING_STATE state{};
    state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = throttled ? THREAD_POWER_THROTTLING_EXECUTION_SPEED : 0;
    SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &state, sizeof(state));
#else
    (void)throttled;
#endif
}

void SetCurrentThreadName(const char* name) {
#ifdef _MSC_VER
    // Sets the debugger-visible name of the current thread.
    if (auto pf = (decltype(&SetThreadDescription))(void*)GetProcAddress(GetModuleHandle(TEXT("KernelBase.dll")), "SetThreadDescription"); pf)
        pf(GetCurrentThread(), UTF8ToUTF16W(name).data()); // Windows 10+
    else
        ; // No-op
#elif  defined(__APPLE__)
    pthread_setname_np(name);
#elif defined(__HAIKU__)
    rename_thread(find_thread(NULL), name);
#elif defined(__Bitrig__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    pthread_set_name_np(pthread_self(), name);
#elif defined(__NetBSD__)
    pthread_setname_np(pthread_self(), "%s", (void*)name);
#elif defined(__linux__) || defined(__CYGWIN__) || defined(__sun__) || defined(__glibc__) || defined(__managarm__)
    int ret = pthread_setname_np(pthread_self(), name);
    if (ret == ERANGE) {
        // Linux limits thread names to 15 characters and will outright reject any
        // attempt to set a longer name with ERANGE.
        char buf[16];
        size_t const len = std::min<size_t>(std::strlen(name), sizeof(buf) - 1);
        std::memcpy(buf, name, len);
        buf[len] = '\0';
        pthread_setname_np(pthread_self(), buf);
    }
#elif defined(_WIN32)
    // MinGW with the POSIX threading model does not support pthread_setname_np
    // See for reference
    // https://gitlab.freedesktop.org/mesa/mesa/-/blame/main/src/util/u_thread.c?ref_type=heads#L75
    (void)name;
#else
    pthread_setname_np(pthread_self(), name);
#endif
}

void PinCurrentThreadToPerformanceCore(size_t core_id) {
    ASSERT(core_id < 4);
    // If we set a flag for a CPU that doesn't exist, the thread may not be allowed to
    // run in ANY processor!
    auto const total_cores = std::thread::hardware_concurrency();
    if (core_id < total_cores) {
#if defined(__ANDROID__)
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(core_id, &set);
        sched_setaffinity(pthread_self(), sizeof(set), &set);
#elif defined(__linux__) || defined(__FreeBSD__)
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(core_id, &set);
        pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#elif defined(_WIN32)
        DWORD set = 1UL << core_id;
        SetThreadAffinityMask(GetCurrentThread(), set);
#else
        // No pin functionality implemented
#endif
    }
}

void PinCurrentThreadToPhysicalCore(size_t core_id) {
#ifdef _WIN32
    static const std::vector<DWORD_PTR> physical_core_masks = GetPhysicalCoreAffinityMasks();
    if (core_id < physical_core_masks.size()) {
        SetThreadAffinityMask(GetCurrentThread(), physical_core_masks[core_id]);
    }
#else
    (void)core_id;
#endif
}

void PinCurrentThreadToPrimaryPhysicalCore(size_t core_id) {
#ifdef _WIN32
    static const std::vector<DWORD_PTR> physical_core_masks = GetPhysicalCoreAffinityMasks();
    if (core_id < physical_core_masks.size()) {
        SetThreadAffinityMask(GetCurrentThread(), LowestSetBit(physical_core_masks[core_id]));
    }
#else
    (void)core_id;
#endif
}

void PinCurrentThreadToPhysicalCoreSibling(size_t core_id) {
#ifdef _WIN32
    static const std::vector<DWORD_PTR> physical_core_sibling_masks =
        GetPhysicalCoreSiblingAffinityMasks();
    if (!physical_core_sibling_masks.empty()) {
        SetThreadAffinityMask(GetCurrentThread(),
                              physical_core_sibling_masks[core_id %
                                                          physical_core_sibling_masks.size()]);
    }
#else
    (void)core_id;
#endif
}

void PinCurrentThreadToPhysicalCoreSiblings() {
#ifdef _WIN32
    static const DWORD_PTR sibling_mask = GetPhysicalCoreSiblingAffinityMask();
    if (sibling_mask != 0) {
        SetThreadAffinityMask(GetCurrentThread(), sibling_mask);
    }
#endif
}

} // namespace Common
