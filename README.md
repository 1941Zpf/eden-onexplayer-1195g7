<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

# Eden OneXPlayer 1S i7-1195G7 Optimized Fork

**Language / 语言:** [English](#english) | [中文](#zh-cn)

<a id="english"></a>

## English

This is a study and experimentation fork based on [Eden](https://git.eden-emu.dev/eden-emu/eden).

The purpose of this repository is not to reintroduce Eden's general emulator features. Instead, it focuses on source-level performance and stability experiments for **One-Netbook OneXPlayer 1S / Intel Core i7-1195G7 / Intel Iris Xe** class handheld hardware.

Target hardware:

- CPU: Intel Core i7-1195G7, 4 cores / 8 threads, Tiger Lake
- GPU: Intel Iris Xe integrated graphics with shared system memory
- Typical environment: Windows handheld, limited cooling capacity, sustained load can reach the 100 C thermal wall and trigger frequency drops
- Main goal: reduce avoidable CPU overhead, soften thermal throttling pressure, and improve Vulkan, memory, and shader-path smoothness without intentionally sacrificing visual correctness

### Project Scope

This fork is intended for source study, performance tuning experiments, and hardware-specific discussion.

It does not include games, keys, firmware, or copyrighted content. Only test with content you legally own and have dumped yourself.

This is not an official Eden build and does not represent the default behavior of upstream Eden. The OneXPlayer profile is intentionally enabled by default because this fork is built around that specific hardware target.

### Release Lag Notice

Published release builds may lag behind the source tree. A release package may not contain every feature, fix, or optimization described in this README. For any specific binary release, the release notes are the authoritative source.

The source code is fully public. If you want the latest available source-level changes, you may build from the repository yourself. Please note that unreleased code can also contain bugs, regressions, or unfinished experiments.

### Optimization Highlights

#### 1. OneXPlayer 1195G7 Profile

The fork adds a dedicated OneXPlayer i7-1195G7/Iris Xe profile and applies it at startup. The profile prefers paths that better fit this machine:

- Vulkan backend by default
- Docked mode by default
- Frame limit and internal resolution follow the graphical configuration instead of being hard-locked in source
- Safer CPU/cache defaults, avoiding performance gains that depend on obvious visual correctness tradeoffs
- Disk shader cache and Vulkan driver pipeline cache enabled by default

#### 2. 4C/8T Handheld Thread Scheduling

The i7-1195G7 has strong burst clocks, but handheld cooling makes sustained all-core load expensive. This fork tries to keep critical emulator threads responsive while moving background work to less disruptive lanes:

- Guest CPU core threads use higher priority and disable Windows execution-speed throttling
- Vulkan, GPU, and background workers are shifted toward SMT sibling lanes where appropriate
- Vulkan pipeline workers are capped by default to reduce shader-compilation heat on a 4C/8T CPU
- Texture decode and pipeline serialization workers use lower priority
- Spin locks yield under longer contention to reduce pure busy-wait heat buildup

Shader compilation worker count can be adjusted with:

```powershell
$env:EDEN_1195G7_SHADER_WORKERS="4"
```

Valid values are `1` to `4`. More workers may reduce shader compilation waits, but can also raise temperature and worsen throttling.

#### 3. Vulkan Submission and Pipeline Cache

Several Vulkan-side changes target the Iris Xe Windows path:

- Pre-reserved Vulkan command chunks to reduce runtime allocation churn
- Prefer native single-draw commands on the Iris Xe path where a one-entry multi-draw wrapper is not helpful
- Larger Vulkan upload stream buffer to reduce upload jitter in asset-heavy scenes
- Vulkan driver pipeline cache retained to reduce repeated pipeline construction
- SPIR-V output optimization disabled by default to reduce runtime shader compilation CPU cost

#### 4. GPU Cache Invalidation and Memory Sync

Open-world games can trigger frequent CPU/GPU shared-memory invalidation. This fork reduces avoidable work in that path:

- Fast skip when there is no pending GPU dirty memory
- Queue selected cache invalidations to the GPU thread in asynchronous GPU mode
- Merge adjacent or overlapping invalidation ranges before touching texture, buffer, and pipeline caches
- Bound the invalidation queue to avoid uncontrolled latency
- Cache OneXPlayer profile flags and runtime parameters in atomics, avoiding repeated environment-variable reads from hot paths

These changes aim to reduce CPU overhead and lock contention without skipping required synchronization.

#### 5. Texture, ASTC, and Unswizzle Paths

Intel Iris Xe uses shared memory, so texture work can pressure the GPU, CPU, and memory bandwidth at the same time. The fork uses conservative defaults:

- GPU ASTC decoding by default to reduce CPU-side texture decode pressure
- GPU unswizzle enabled with conservative size and streaming settings
- Texture-related background workers kept at limited parallelism and lower priority to reduce thermal pressure

#### 6. Dynarmic Code Cache

For x86_64 builds under the OneXPlayer profile, Dynarmic code cache size is increased to at least 1 GiB.

This is intended to reduce repeated JIT pressure during longer play sessions when memory capacity is sufficient.

#### 7. Splatoon 3 Shader Correctness Fix

While investigating a Splatoon 3 red fuzzy-ooze texture issue, the Maxwell shader recompiler's `VOTE_vtg` instruction path was found to be stubbed.

This fork connects `VOTE_vtg` to the existing `VOTE` implementation so it can emit subgroup vote/ballot IR instead of silently doing nothing. This is a correctness fix, not a conservative rendering workaround that trades performance for safety.

### Safety and Experimental Switches

For debugging, parts of the profile can be made stricter:

```powershell
$env:EDEN_1195G7_DISABLE_PROFILE="1"
$env:EDEN_1195G7_STRICT_CPU="1"
$env:EDEN_1195G7_STRICT_CACHE="1"
$env:EDEN_1195G7_STRICT_WFI="1"
$env:EDEN_1195G7_STRICT_DIRTY="1"
```

Experimental switches:

```powershell
$env:EDEN_1195G7_ASYNC_SHADERS="1"
$env:EDEN_1195G7_UNSAFE_CPU="1"
$env:EDEN_1195G7_UNSAFE_CACHE="1"
```

These options may improve performance in some cases, but can also cause visual issues, crashes, or unusual stutter. They are not enabled by default.

### Upstream and License

This fork is based on the Eden Emulator Project. Original copyright notices from Eden and its predecessors are preserved in source files.

Eden is licensed under GPLv3 or any later version. See [LICENSE.txt](./LICENSE.txt).

<a id="zh-cn"></a>

## 中文

这是一个基于 [Eden](https://git.eden-emu.dev/eden-emu/eden) 的学习交流用 fork。

本仓库的重点不是重新介绍 Eden 原本已有的通用模拟器功能，而是在 Eden 的基础上，针对 **壹号本 OneXPlayer 1S / Intel Core i7-1195G7 / Intel Iris Xe** 这类掌机硬件，做源码级性能与稳定性优化实验。

目标硬件：

- CPU：Intel Core i7-1195G7，4 核 8 线程，Tiger Lake
- GPU：Intel Iris Xe 核显，共享系统内存
- 典型环境：Windows 掌机，散热空间有限，持续高负载容易触及 100 度温度墙并降频
- 主要目标：在不主动牺牲画面正确性的前提下，降低不必要 CPU 开销，缓解热降频压力，改善 Vulkan、内存和着色器路径的流畅度

### 项目定位

本 fork 供源码学习、性能调优实验和硬件专项适配交流使用。

仓库不包含游戏、密钥、固件或任何受版权保护的内容；请只使用你依法拥有并自行备份的内容进行测试。

这不是 Eden 官方版本，也不代表 Eden 上游项目的默认行为。本仓库里的 OneXPlayer profile 被有意设置为默认启用，因为这个 fork 就是围绕这台硬件目标制作的。

### 发布版滞后说明

发布版本可能会滞后于源码主分支。某个发布包并不一定包含 README 中描述的所有功能、修复和优化；具体以对应发布版本的发布说明为准。

本仓库全面公开源码。如果需要当前源码中的全部功能和优化，可以自行从源码编译。需要注意的是，未发布版本也可能存在 bug、回归或尚未完成的实验性改动。

### 当前优化重点

#### 1. OneXPlayer 1195G7 专用 Profile

源码中加入了 OneXPlayer i7-1195G7/Iris Xe 专用 profile，并在启动时默认应用。它会优先选择更适合这台机器的运行路径：

- 默认使用 Vulkan 后端
- 默认主机模式
- 帧率限制和内部分辨率交给图形界面设置，不再在源码里锁死
- 默认保持偏安全的 CPU/cache 设置，避免用明显牺牲画面正确性的方式换性能
- 默认开启磁盘 shader cache 和 Vulkan driver pipeline cache

#### 2. 针对 4C/8T 掌机的线程调度

i7-1195G7 的短时睿频能力很强，但掌机散热下持续满载代价很高。这个 fork 尝试让关键模拟器线程保持响应，同时把后台工作移动到更不干扰的位置：

- Guest CPU 核心线程使用较高优先级，并关闭 Windows 执行速度节流
- Vulkan、GPU 和后台 worker 在合适场景下偏向 SMT sibling lanes
- Vulkan pipeline worker 默认限制数量，降低 4C/8T CPU 在 shader 编译时的热压力
- 纹理解码和管线序列化 worker 使用较低优先级
- spin lock 在较长时间竞争时主动让出，减少纯空转带来的热量堆积

shader 编译 worker 数量可以通过环境变量调整：

```powershell
$env:EDEN_1195G7_SHADER_WORKERS="4"
```

有效范围为 `1` 到 `4`。更多 worker 可能减少 shader 编译等待，但也可能提高温度并加重降频。

#### 3. Vulkan 提交与管线缓存

针对 Iris Xe Windows Vulkan 路径做了多处调整：

- 预留 Vulkan command chunk，减少运行时频繁分配
- Iris Xe 路径在合适场景偏向原生单 draw，避免单 draw 走 multi-draw wrapper 的额外开销
- 增大 Vulkan upload stream buffer，降低资源密集场景的上传抖动
- 保留 Vulkan driver pipeline cache，减少重复 pipeline 构建
- 默认关闭 SPIR-V 输出优化，减少运行期 shader 编译 CPU 成本

#### 4. GPU Cache Invalidation 与内存同步

开放世界游戏会频繁触发 CPU/GPU 共享内存失效处理。这个 fork 对相关路径减少不必要工作：

- GPU dirty memory 为空时快速跳过不必要的 cache invalidation
- 异步 GPU 模式下将部分 cache invalidation 排队到 GPU 线程处理
- 对连续或重叠 invalidation range 做合并，再触发 texture、buffer 和 pipeline cache 处理
- 对 invalidation 队列设置上限，避免延迟失控
- 将 OneXPlayer profile 的 flags 和运行参数缓存为原子变量，避免高频路径反复读取环境变量

这些改动的目标是减少 CPU 消耗和锁竞争，而不是跳过必要同步。

#### 5. 纹理、ASTC 与 Unswizzle 路径

Intel Iris Xe 是共享内存核显，纹理处理可能同时压到 GPU、CPU 和内存带宽。本 fork 采用相对保守的默认策略：

- 默认使用 GPU ASTC 解码，减少 CPU 侧纹理解码压力
- 默认开启 GPU unswizzle，并使用较保守的大小和流式参数
- 纹理相关后台 worker 保持有限并行度和较低优先级，减少热压力

#### 6. Dynarmic 代码缓存

在 x86_64 + OneXPlayer profile 下，Dynarmic code cache 至少提升到 1 GiB。

这有助于在内存容量足够时，降低长时间游玩中的重复 JIT 压力。

#### 7. Splatoon 3 Shader 正确性修复

在排查 Splatoon 3 红色绒毛贴图问题时，发现 Maxwell shader recompiler 的 `VOTE_vtg` 指令路径原本是 stub。

本 fork 已将 `VOTE_vtg` 接入现有 `VOTE` 实现链路，使其能够生成 subgroup vote/ballot IR，而不是静默忽略。这个修复属于画面正确性修复，不是用保守渲染同步换性能稳定的 workaround。

### 保守与实验开关

调试时可以通过环境变量让部分 profile 行为更保守：

```powershell
$env:EDEN_1195G7_DISABLE_PROFILE="1"
$env:EDEN_1195G7_STRICT_CPU="1"
$env:EDEN_1195G7_STRICT_CACHE="1"
$env:EDEN_1195G7_STRICT_WFI="1"
$env:EDEN_1195G7_STRICT_DIRTY="1"
```

实验性开关：

```powershell
$env:EDEN_1195G7_ASYNC_SHADERS="1"
$env:EDEN_1195G7_UNSAFE_CPU="1"
$env:EDEN_1195G7_UNSAFE_CACHE="1"
```

这些选项在某些场景可能提升性能，也可能造成画面错误、闪退或奇怪卡顿，默认不启用。

### 上游与许可证

本 fork 基于 Eden Emulator Project。原项目及其衍生来源的版权声明保留在源码文件中。

Eden 使用 GPLv3 或更高版本授权，详见 [LICENSE.txt](./LICENSE.txt)。
