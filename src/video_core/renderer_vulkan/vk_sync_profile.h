// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/game_settings.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan::SyncProfile {

inline constexpr VkPipelineStageFlags LegacyUploadDstStages =
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

inline constexpr VkPipelineStageFlags LegacyTextureUseStages =
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
    VK_PIPELINE_STAGE_TRANSFER_BIT;

inline constexpr VkPipelineStageFlags GraphicsTextureUseStages =
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
    VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
    VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

inline constexpr VkPipelineStageFlags ConservativeTextureUseStages =
    GraphicsTextureUseStages | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
    VK_PIPELINE_STAGE_TRANSFER_BIT;

inline constexpr VkPipelineStageFlags LegacyRenderPassSrcStages =
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

inline constexpr VkPipelineStageFlags ConservativeRenderPassSrcStages =
    LegacyRenderPassSrcStages | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;

inline constexpr VkPipelineStageFlags LegacyRenderPassDstStages =
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

inline constexpr VkPipelineStageFlags ConservativeRenderPassDstStages =
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
    VK_PIPELINE_STAGE_TRANSFER_BIT;

inline constexpr VkAccessFlags TextureReadAccess =
    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT |
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

inline constexpr VkAccessFlags TextureWriteAccess =
    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

inline constexpr VkAccessFlags LegacyRenderPassSrcAccess =
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

inline constexpr VkAccessFlags ConservativeRenderPassSrcAccess =
    TextureReadAccess | TextureWriteAccess | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

inline constexpr VkAccessFlags LegacyRenderPassDstAccess =
    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

inline constexpr VkAccessFlags ConservativeRenderPassDstAccess =
    LegacyRenderPassDstAccess | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

[[nodiscard]] inline bool UseConservativeBarriers() noexcept {
    return Core::GameSettings::UseConservativeVulkanUploadBarriers();
}

[[nodiscard]] inline VkPipelineStageFlags UploadDstStages() noexcept {
    return UseConservativeBarriers() ? ConservativeTextureUseStages : LegacyUploadDstStages;
}

[[nodiscard]] inline VkPipelineStageFlags TextureUseStages() noexcept {
    return UseConservativeBarriers() ? ConservativeTextureUseStages : LegacyTextureUseStages;
}

[[nodiscard]] inline VkAccessFlags PreUploadAccess() noexcept {
    return UseConservativeBarriers() ? (TextureReadAccess | TextureWriteAccess)
                                     : TextureWriteAccess;
}

[[nodiscard]] inline VkPipelineStageFlags RenderPassSrcStages() noexcept {
    return UseConservativeBarriers() ? ConservativeRenderPassSrcStages
                                     : LegacyRenderPassSrcStages;
}

[[nodiscard]] inline VkAccessFlags RenderPassSrcAccess() noexcept {
    return UseConservativeBarriers() ? ConservativeRenderPassSrcAccess
                                     : LegacyRenderPassSrcAccess;
}

[[nodiscard]] inline VkPipelineStageFlags RenderPassDstStages() noexcept {
    return UseConservativeBarriers() ? ConservativeRenderPassDstStages
                                     : LegacyRenderPassDstStages;
}

[[nodiscard]] inline VkAccessFlags RenderPassDstAccess() noexcept {
    return UseConservativeBarriers() ? ConservativeRenderPassDstAccess
                                     : LegacyRenderPassDstAccess;
}

[[nodiscard]] inline VkPipelineStageFlags ExternalWaitStages() noexcept {
    return UseConservativeBarriers() ? ConservativeTextureUseStages : LegacyUploadDstStages;
}

} // namespace Vulkan::SyncProfile
