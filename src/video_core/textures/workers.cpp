// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/textures/workers.h"

#include <algorithm>
#include <thread>

#include "core/game_settings.h"

namespace Tegra::Texture {

Common::ThreadWorker& GetThreadWorkers() {
    static Common::ThreadWorker workers{Core::GameSettings::GetTextureWorkerCount(
                                            (std::max)(std::thread::hardware_concurrency(), 2U) /
                                            2),
                                        "ImageTranscode",
                                        {},
                                        Core::GameSettings::UseThermalAwareThreadScheduling()
                                            ? Common::ThreadPriority::Low
                                            : Common::ThreadPriority::Normal,
                                        Core::GameSettings::UseThermalAwareThreadScheduling(),
                                        false,
                                        Core::GameSettings::UseThermalAwareThreadScheduling() ? 2
                                                                                              : 0};

    return workers;
}

} // namespace Tegra::Texture
