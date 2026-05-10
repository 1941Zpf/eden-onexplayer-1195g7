// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/game_settings.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "common/logging.h"
#include "common/settings.h"
#include "video_core/renderer_base.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace Core::GameSettings {

namespace {

enum class ActiveProfile {
    None,
    Onexplayer1195G7,
};

std::atomic<ActiveProfile> active_profile{ActiveProfile::None};
std::atomic_size_t vulkan_pipeline_worker_limit{0};
constexpr const char* onexplayer_profile_version = "004";

bool IsTruthyEnvironmentVariable(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return false;
    }

    return std::strcmp(value, "0") != 0 && std::strcmp(value, "false") != 0 &&
           std::strcmp(value, "FALSE") != 0;
}

std::size_t ReadWorkerLimitFromEnvironment(std::size_t fallback) {
    const char* value = std::getenv("EDEN_1195G7_SHADER_WORKERS");
    if (value == nullptr) {
        value = std::getenv("EDEN_TOTK_1195G7_SHADER_WORKERS");
    }
    if (value == nullptr) {
        return fallback;
    }

    char* end = nullptr;
    const auto parsed = std::strtoull(value, &end, 10);
    if (end == value || parsed == 0) {
        return fallback;
    }

    return std::clamp<std::size_t>(static_cast<std::size_t>(parsed), 1, 4);
}

bool IsProfileDisabled() {
    return IsTruthyEnvironmentVariable("EDEN_1195G7_DISABLE_PROFILE") ||
           IsTruthyEnvironmentVariable("EDEN_TOTK_1195G7_DISABLE_PROFILE");
}

template <typename Setting, typename Value>
void ForceCustomSetting(Setting& setting, const Value& value) {
    setting.SetGlobal(false);
    setting.SetValue(value);
}

bool ShouldUse1195G7Profile() {
    return !IsProfileDisabled();
}

} // Anonymous namespace

static GPUVendor GetGPU(const std::string& gpu_vendor_string) {
    struct Entry { const char* name; GPUVendor vendor; };
    static constexpr Entry GpuVendor[] = {
        // NVIDIA
        {"NVIDIA",   GPUVendor::Nvidia},
        {"Nouveau",  GPUVendor::Nvidia},
        {"NVK",      GPUVendor::Nvidia},
        {"Tegra",    GPUVendor::Nvidia},
        // AMD
        {"AMD",       GPUVendor::AMD},
        {"RadeonSI",  GPUVendor::AMD},
        {"RADV",      GPUVendor::AMD},
        {"AMDVLK",    GPUVendor::AMD},
        {"R600",      GPUVendor::AMD},
        // Intel
        {"Intel",     GPUVendor::Intel},
        {"ANV",       GPUVendor::Intel},
        {"i965",      GPUVendor::Intel},
        {"i915",      GPUVendor::Intel},
        {"OpenSWR",   GPUVendor::Intel},
        // Apple
        {"Apple",     GPUVendor::Apple},
        {"MoltenVK",  GPUVendor::Apple},
        // Qualcomm / Adreno
        {"Qualcomm",  GPUVendor::Qualcomm},
        {"Turnip",    GPUVendor::Qualcomm},
        // ARM / Mali
        {"Mali",      GPUVendor::ARM},
        {"PanVK",     GPUVendor::ARM},
        // Imagination / PowerVR
        {"PowerVR",   GPUVendor::Imagination},
        {"PVR",       GPUVendor::Imagination},
        // Microsoft / WARP / D3D12 GL
        {"D3D12",     GPUVendor::Microsoft},
        {"Microsoft", GPUVendor::Microsoft},
        {"WARP",      GPUVendor::Microsoft},
    };

    for (const auto& entry : GpuVendor) {
        if (gpu_vendor_string == entry.name) {
            return entry.vendor;
        }
    }

    // legacy (shouldn't be needed anymore, but just in case)
    std::string gpu = gpu_vendor_string;
    std::transform(gpu.begin(), gpu.end(), gpu.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    if (gpu.find("geforce") != std::string::npos) {
        return GPUVendor::Nvidia;
    }
    if (gpu.find("radeon") != std::string::npos || gpu.find("ati") != std::string::npos) {
        return GPUVendor::AMD;
    }

    return GPUVendor::Unknown;
}

static OS DetectOS() {
#if defined(_WIN32)
    return OS::Windows;
#elif defined(__FIREOS__)
    return OS::FireOS;
#elif defined(__ANDROID__)
    return OS::Android;
#elif defined(__OHOS__)
    return OS::HarmonyOS;
#elif defined(__HAIKU__)
    return OS::HaikuOS;
#elif defined(__DragonFly__)
    return OS::DragonFlyBSD;
#elif defined(__NetBSD__)
    return OS::NetBSD;
#elif defined(__OpenBSD__)
    return OS::OpenBSD;
#elif defined(_AIX)
    return OS::AIX;
#elif defined(__managarm__)
    return OS::Managarm;
#elif defined(__redox__)
    return OS::RedoxOS;
#elif defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return OS::IOS;
#elif defined(__APPLE__)
    return OS::MacOS;
#elif defined(__FreeBSD__)
    return OS::FreeBSD;
#elif defined(__sun) && defined(__SVR4)
    return OS::Solaris;
#elif defined(__linux__)
    return OS::Linux;
#else
    return OS::Unknown;
#endif
}

EnvironmentInfo DetectEnvironment(const VideoCore::RendererBase& renderer) {
    EnvironmentInfo env{};
    env.os = DetectOS();
    env.vendor_string = renderer.GetDeviceVendor();
    env.vendor = GetGPU(env.vendor_string);
    return env;
}

bool LoadEarlyOverrides(std::uint64_t program_id) {
    ResetOverrides();

    if (!ShouldUse1195G7Profile()) {
        return false;
    }

    active_profile.store(ActiveProfile::Onexplayer1195G7, std::memory_order_release);
    const std::size_t worker_limit = ReadWorkerLimitFromEnvironment(3);
    vulkan_pipeline_worker_limit.store(worker_limit, std::memory_order_release);

    ForceCustomSetting(Settings::values.renderer_backend, Settings::RendererBackend::Vulkan);
    ForceCustomSetting(Settings::values.cpu_accuracy, Settings::CpuAccuracy::Auto);
    ForceCustomSetting(Settings::values.use_asynchronous_gpu_emulation, true);
    ForceCustomSetting(Settings::values.async_presentation, true);
    ForceCustomSetting(Settings::values.renderer_force_max_clock, false);
    ForceCustomSetting(Settings::values.use_docked_mode, Settings::ConsoleMode::Docked);
    ForceCustomSetting(Settings::values.gpu_accuracy, Settings::GpuAccuracy::Medium);
    ForceCustomSetting(Settings::values.fast_gpu_time, Settings::GpuOverclock::Normal);
    ForceCustomSetting(Settings::values.vram_usage_mode, Settings::VramUsageMode::Aggressive);
    ForceCustomSetting(Settings::values.accelerate_astc, Settings::AstcDecodeMode::Gpu);
    ForceCustomSetting(Settings::values.astc_recompression, Settings::AstcRecompression::Uncompressed);
    ForceCustomSetting(Settings::values.max_anisotropy, Settings::AnisotropyMode::Automatic);
    ForceCustomSetting(Settings::values.optimize_spirv_output, Settings::SpirvOptimizeMode::Never);
    ForceCustomSetting(Settings::values.dyna_state, Settings::ExtendedDynamicState::EDS2);
    ForceCustomSetting(Settings::values.vertex_input_dynamic_state, true);
    ForceCustomSetting(Settings::values.descriptor_indexing, false);
    ForceCustomSetting(Settings::values.sync_memory_operations, false);
    ForceCustomSetting(Settings::values.use_reactive_flushing, true);
    ForceCustomSetting(Settings::values.barrier_feedback_loops, true);
    ForceCustomSetting(Settings::values.use_disk_shader_cache, true);
    ForceCustomSetting(Settings::values.use_vulkan_driver_pipeline_cache, true);
    ForceCustomSetting(Settings::values.use_speed_limit, true);
    ForceCustomSetting(Settings::values.speed_limit, static_cast<u16>(100));
    ForceCustomSetting(Settings::values.gpu_unswizzle_enabled, true);
    ForceCustomSetting(Settings::values.gpu_unswizzle_texture_size,
                       Settings::GpuUnswizzleSize::Small);
    ForceCustomSetting(Settings::values.gpu_unswizzle_stream_size, Settings::GpuUnswizzle::Normal);
    ForceCustomSetting(Settings::values.gpu_unswizzle_chunk_size,
                       Settings::GpuUnswizzleChunk::Normal);

    if (IsTruthyEnvironmentVariable("EDEN_1195G7_UNSAFE_CACHE") ||
        IsTruthyEnvironmentVariable("EDEN_TOTK_1195G7_UNSAFE_CACHE")) {
        ForceCustomSetting(Settings::values.skip_cpu_inner_invalidation, true);
    }

    if (IsTruthyEnvironmentVariable("EDEN_1195G7_ASYNC_SHADERS") ||
        IsTruthyEnvironmentVariable("EDEN_TOTK_1195G7_ASYNC_SHADERS")) {
        ForceCustomSetting(Settings::values.use_asynchronous_shaders, true);
    }

    if (IsTruthyEnvironmentVariable("EDEN_1195G7_UNSAFE_CPU") ||
        IsTruthyEnvironmentVariable("EDEN_TOTK_1195G7_UNSAFE_CPU")) {
        ForceCustomSetting(Settings::values.cpu_accuracy, Settings::CpuAccuracy::Unsafe);
    }

    LOG_INFO(Core,
             "Enabled OneXPlayer i7-1195G7/Iris Xe performance profile {} for {:016X}: Vulkan "
             "pipeline workers capped at {}; resolution and frame pacing follow UI settings; "
             "Vulkan submission gets a reserved primary core and prewarmed command chunks",
             onexplayer_profile_version,
             program_id,
             worker_limit);
    return true;
}

void LoadOverrides(std::uint64_t program_id, const VideoCore::RendererBase& renderer) {
    const auto env = DetectEnvironment(renderer);

    switch (static_cast<TitleID>(program_id)) {
        case TitleID::NinjaGaidenRagebound:
            Settings::values.use_squashed_iterated_blend = true;
            break;
        default:
            break;
    }

    LOG_INFO(Core, "Applied game settings for title ID {:016X} on OS {}, GPU vendor {} ({})",
             program_id,
             static_cast<int>(env.os),
             static_cast<int>(env.vendor),
             env.vendor_string);
}

void ResetOverrides() {
    const ActiveProfile previous = active_profile.exchange(ActiveProfile::None,
                                                           std::memory_order_acq_rel);
    vulkan_pipeline_worker_limit.store(0, std::memory_order_release);
    Settings::values.use_squashed_iterated_blend = false;

    if (previous == ActiveProfile::None) {
        return;
    }

    Settings::values.cpu_accuracy.SetGlobal(true);
    Settings::values.renderer_backend.SetGlobal(true);
    Settings::values.use_asynchronous_gpu_emulation.SetGlobal(true);
    Settings::values.use_asynchronous_shaders.SetGlobal(true);
    Settings::values.async_presentation.SetGlobal(true);
    Settings::values.renderer_force_max_clock.SetGlobal(true);
    Settings::values.use_docked_mode.SetGlobal(true);
    Settings::values.gpu_accuracy.SetGlobal(true);
    Settings::values.fast_gpu_time.SetGlobal(true);
    Settings::values.vram_usage_mode.SetGlobal(true);
    Settings::values.accelerate_astc.SetGlobal(true);
    Settings::values.astc_recompression.SetGlobal(true);
    Settings::values.max_anisotropy.SetGlobal(true);
    Settings::values.optimize_spirv_output.SetGlobal(true);
    Settings::values.dyna_state.SetGlobal(true);
    Settings::values.vertex_input_dynamic_state.SetGlobal(true);
    Settings::values.descriptor_indexing.SetGlobal(true);
    Settings::values.sync_memory_operations.SetGlobal(true);
    Settings::values.use_reactive_flushing.SetGlobal(true);
    Settings::values.barrier_feedback_loops.SetGlobal(true);
    Settings::values.use_disk_shader_cache.SetGlobal(true);
    Settings::values.use_vulkan_driver_pipeline_cache.SetGlobal(true);
    Settings::values.use_speed_limit.SetGlobal(true);
    Settings::values.speed_limit.SetGlobal(true);
    Settings::values.gpu_unswizzle_enabled.SetGlobal(true);
    Settings::values.gpu_unswizzle_texture_size.SetGlobal(true);
    Settings::values.gpu_unswizzle_stream_size.SetGlobal(true);
    Settings::values.gpu_unswizzle_chunk_size.SetGlobal(true);
    Settings::values.skip_cpu_inner_invalidation.SetGlobal(true);
}

std::size_t GetVulkanPipelineWorkerCount(std::size_t default_workers) {
    const std::size_t safe_default = std::max<std::size_t>(default_workers, 1);
    if (active_profile.load(std::memory_order_acquire) != ActiveProfile::Onexplayer1195G7) {
        return safe_default;
    }

    const std::size_t limit = vulkan_pipeline_worker_limit.load(std::memory_order_acquire);
    if (limit == 0) {
        return safe_default;
    }

    return std::clamp<std::size_t>(limit, 1, safe_default);
}

bool UseThermalAwareThreadScheduling() {
    return active_profile.load(std::memory_order_acquire) == ActiveProfile::Onexplayer1195G7 ||
           ShouldUse1195G7Profile();
}

bool ReservePrimaryCoreForVulkanSubmission() {
    return UseThermalAwareThreadScheduling();
}

std::size_t GetTextureWorkerCount(std::size_t default_workers) {
    const std::size_t safe_default = std::max<std::size_t>(default_workers, 1);
    if (active_profile.load(std::memory_order_acquire) != ActiveProfile::Onexplayer1195G7) {
        return safe_default;
    }

    return std::min<std::size_t>(safe_default, 2);
}

std::size_t GetVulkanUploadStreamBufferSize(std::size_t default_size) {
    if (active_profile.load(std::memory_order_acquire) != ActiveProfile::Onexplayer1195G7) {
        return default_size;
    }

    constexpr std::size_t onexplayer_upload_stream_size = 256ULL * 1024ULL * 1024ULL;
    return std::max(default_size, onexplayer_upload_stream_size);
}

std::uint32_t GetDynarmicCodeCacheSize(std::uint32_t default_size) {
#if defined(ARCHITECTURE_x86_64)
    if (active_profile.load(std::memory_order_acquire) != ActiveProfile::Onexplayer1195G7) {
        return default_size;
    }

    constexpr std::uint32_t onexplayer_code_cache_size = 768U * 1024U * 1024U;
    return std::max(default_size, onexplayer_code_cache_size);
#else
    return default_size;
#endif
}

} // namespace Core::GameSettings
