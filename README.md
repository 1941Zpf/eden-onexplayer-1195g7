<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

<div align="center" id="readme-top">
  <img src="./dist/qt_themes/default/icons/256x256/eden.png" alt="Eden logo" width="112">

  <h1>Eden OneXPlayer 1S i7-1195G7 Optimized Fork</h1>

  <p>
    A source-level Eden fork tuned for OneXPlayer 1S, Intel Core i7-1195G7, and Intel Iris Xe handheld hardware.
  </p>

  <p>
    <a href="#english"><img alt="English default" src="https://img.shields.io/badge/English-default-2f81f7?style=for-the-badge"></a>
    <a href="#zh-cn"><img alt="Chinese" src="https://img.shields.io/badge/%E4%B8%AD%E6%96%87-%E5%AF%B9%E7%85%A7-34a853?style=for-the-badge"></a>
  </p>

  <p>
    <img alt="Target hardware" src="https://img.shields.io/badge/Target-OneXPlayer%201S-1f6feb?style=flat-square">
    <img alt="CPU" src="https://img.shields.io/badge/CPU-i7--1195G7-0071c5?style=flat-square&logo=intel&logoColor=white">
    <img alt="GPU" src="https://img.shields.io/badge/GPU-Iris%20Xe-5e5ce6?style=flat-square">
    <img alt="Focus" src="https://img.shields.io/badge/Focus-thermal%20aware-f97316?style=flat-square">
    <img alt="Source" src="https://img.shields.io/badge/Source-public-22c55e?style=flat-square">
    <img alt="License" src="https://img.shields.io/badge/License-GPLv3%2B-6f42c1?style=flat-square">
  </p>
</div>

---

<a id="english"></a>

## English

### Overview

This is a study and experimentation fork based on [Eden](https://git.eden-emu.dev/eden-emu/eden). It does not try to reintroduce Eden's general emulator feature set. The repository is centered on source-level performance and stability work for **One-Netbook OneXPlayer 1S / Intel Core i7-1195G7 / Intel Iris Xe** handheld systems.

| Item | Target |
| --- | --- |
| CPU | Intel Core i7-1195G7, 4 cores / 8 threads, Tiger Lake |
| GPU | Intel Iris Xe integrated graphics with shared system memory |
| Environment | Windows handheld with limited sustained cooling capacity |
| Main pressure point | 100 C thermal wall, frequency drops under sustained load |
| Optimization goal | Reduce avoidable CPU cost, lower thermal pressure, and improve Vulkan, memory, and shader-path smoothness without intentionally sacrificing visual correctness |

### Project Scope

This fork is for source study, performance tuning experiments, and hardware-specific discussion.

It does not include games, keys, firmware, or copyrighted content. Only test with content you legally own and have dumped yourself.

This is not an official Eden build and does not represent the default behavior of upstream Eden. The OneXPlayer profile is intentionally enabled by default because this fork is built around that specific hardware target.

### Release Lag Notice

> Published release builds may lag behind the source tree. A release package may not contain every feature, fix, or optimization described in this README. For any specific binary release, the release notes are the authoritative source.

The source code is fully public. If you want the latest available source-level changes, you may build from the repository yourself. Please note that unreleased code can also contain bugs, regressions, or unfinished experiments.

### Optimization Map

| Area | Source-level focus | Why it matters on i7-1195G7 / Iris Xe |
| --- | --- | --- |
| OneXPlayer profile | Dedicated startup profile, Vulkan default, docked mode default, UI-controlled frame limit and resolution | Keeps the fork aligned with the target handheld instead of applying generic desktop assumptions |
| Thread scheduling | Higher priority guest CPU threads, shifted Vulkan/GPU/background workers, reduced busy waiting | Helps a 4C/8T handheld keep critical threads responsive while reducing heat from background contention |
| Vulkan submission | Pre-reserved command chunks, native single-draw preference, larger upload stream buffer | Reduces allocation churn and command overhead on the Iris Xe Windows Vulkan path |
| Pipeline cache | Disk shader cache and Vulkan driver pipeline cache retained, runtime SPIR-V optimization disabled by default | Reduces repeated pipeline work and lowers shader compilation CPU spikes |
| GPU cache invalidation | Dirty-memory fast skip, queued invalidation, range coalescing, bounded invalidation queue | Cuts avoidable CPU/cache work without intentionally skipping required synchronization |
| Texture path | GPU ASTC decode, conservative GPU unswizzle settings, limited low-priority texture workers | Reduces CPU texture pressure while respecting shared-memory bandwidth and thermals |
| Dynarmic cache | x86_64 OneXPlayer profile raises code cache to at least 1 GiB | Reduces repeated JIT pressure during longer sessions when memory capacity is sufficient |
| Shader correctness | `VOTE_vtg` is connected to the existing `VOTE` implementation | Fixes a previously stubbed Maxwell shader path needed for correct subgroup vote/ballot IR |

### Hardware-Aware Details

#### OneXPlayer 1195G7 Profile

- Vulkan backend by default
- Docked mode by default
- Frame limit and internal resolution follow the graphical configuration instead of being hard-locked in source
- Safer CPU/cache defaults, avoiding performance gains that depend on obvious visual correctness tradeoffs
- Disk shader cache and Vulkan driver pipeline cache enabled by default

#### 4C/8T Thread Scheduling

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

#### Vulkan, Memory, and Texture Paths

- Vulkan command chunks are pre-reserved to reduce runtime allocation churn
- Native single-draw commands are preferred on Iris Xe when a one-entry multi-draw wrapper is not helpful
- Upload stream buffer size is increased to reduce asset-streaming jitter
- GPU dirty memory checks can fast-skip empty invalidations
- Adjacent or overlapping invalidation ranges are merged before touching texture, buffer, and pipeline caches
- OneXPlayer profile flags and runtime parameters are cached in atomics to avoid repeated environment-variable reads from hot paths
- GPU ASTC decoding and conservative GPU unswizzle settings are used by default

#### Splatoon 3 Shader Correctness Fix

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

<p align="right"><a href="#readme-top">Back to top</a></p>

---

<a id="zh-cn"></a>

## 中文

### 概览

这是一个基于 [Eden](https://git.eden-emu.dev/eden-emu/eden) 的学习交流用 fork。本仓库不重复介绍 Eden 原本已有的通用模拟器功能，而是围绕 **壹号本 OneXPlayer 1S / Intel Core i7-1195G7 / Intel Iris Xe** 掌机硬件进行源码级性能与稳定性优化实验。

| 项目 | 目标 |
| --- | --- |
| CPU | Intel Core i7-1195G7，4 核 8 线程，Tiger Lake |
| GPU | Intel Iris Xe 核显，共享系统内存 |
| 典型环境 | Windows 掌机，持续散热能力有限 |
| 主要压力点 | 100 度温度墙，以及持续负载下的频率下降 |
| 优化目标 | 在不主动牺牲画面正确性的前提下，降低不必要 CPU 成本，缓解热压力，并改善 Vulkan、内存和着色器路径的流畅度 |

### 项目定位

本 fork 供源码学习、性能调优实验和硬件专项适配交流使用。

仓库不包含游戏、密钥、固件或任何受版权保护的内容；请只使用你依法拥有并自行备份的内容进行测试。

这不是 Eden 官方版本，也不代表 Eden 上游项目的默认行为。本仓库里的 OneXPlayer profile 被有意设置为默认启用，因为这个 fork 就是围绕这台硬件目标制作的。

### 发布版滞后说明

> 发布版本可能会滞后于源码主分支。某个发布包并不一定包含 README 中描述的所有功能、修复和优化；具体以对应发布版本的发布说明为准。

本仓库全面公开源码。如果需要当前源码中的全部功能和优化，可以自行从源码编译。需要注意的是，未发布版本也可能存在 bug、回归或尚未完成的实验性改动。

### 优化地图

| 方向 | 源码层重点 | 对 i7-1195G7 / Iris Xe 的意义 |
| --- | --- | --- |
| OneXPlayer profile | 专用启动 profile、默认 Vulkan、默认主机模式、帧率和分辨率交给图形界面 | 让 fork 明确服务目标掌机，而不是套用泛桌面假设 |
| 线程调度 | 提升 guest CPU 线程优先级，移动 Vulkan/GPU/后台 worker，减少长时间忙等 | 帮助 4C/8T 掌机保持关键线程响应，同时减少后台竞争带来的热量 |
| Vulkan 提交 | 预留 command chunk，偏向原生单 draw，增大 upload stream buffer | 降低 Iris Xe Windows Vulkan 路径上的分配和命令开销 |
| 管线缓存 | 保留磁盘 shader cache 和 Vulkan driver pipeline cache，默认关闭运行期 SPIR-V 输出优化 | 减少重复 pipeline 工作，降低 shader 编译 CPU 峰值 |
| GPU cache invalidation | dirty memory 快速跳过、队列化 invalidation、范围合并、有界队列 | 减少不必要 CPU/cache 工作，同时不主动跳过必要同步 |
| 纹理路径 | GPU ASTC 解码、保守 GPU unswizzle、低优先级有限纹理 worker | 降低 CPU 纹理压力，同时顾及共享内存带宽和温度 |
| Dynarmic cache | x86_64 OneXPlayer profile 下代码缓存至少 1 GiB | 在内存足够时减少长时间游玩中的重复 JIT 压力 |
| Shader 正确性 | `VOTE_vtg` 接入现有 `VOTE` 实现 | 修复原本 stub 的 Maxwell shader 路径，使其生成正确 subgroup vote/ballot IR |

### 硬件专项细节

#### OneXPlayer 1195G7 专用 Profile

- 默认使用 Vulkan 后端
- 默认主机模式
- 帧率限制和内部分辨率交给图形界面设置，不再在源码里锁死
- 默认保持偏安全的 CPU/cache 设置，避免用明显牺牲画面正确性的方式换性能
- 默认开启磁盘 shader cache 和 Vulkan driver pipeline cache

#### 4C/8T 线程调度

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

#### Vulkan、内存与纹理路径

- 预留 Vulkan command chunk，减少运行时频繁分配
- Iris Xe 路径在单 draw 场景偏向原生命令，避免不必要的 multi-draw wrapper
- 增大 upload stream buffer，降低资源流式加载抖动
- GPU dirty memory 为空时快速跳过不必要 invalidation
- 对连续或重叠 invalidation range 做合并，再触发 texture、buffer 和 pipeline cache 处理
- 将 OneXPlayer profile 的 flags 和运行参数缓存为原子变量，避免高频路径反复读取环境变量
- 默认使用 GPU ASTC 解码和保守 GPU unswizzle 设置

#### Splatoon 3 Shader 正确性修复

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

<p align="right"><a href="#readme-top">回到顶部</a></p>
