<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

# Eden OneXPlayer 1S i7-1195G7 优化版

这是一个基于 [Eden](https://git.eden-emu.dev/eden-emu/eden) 的学习交流用 fork。

本仓库的重点不是重新介绍 Eden 原本已有的通用模拟器功能，而是在 Eden 的基础上，针对 **壹号本 OneXPlayer 1S / Intel Core i7-1195G7 / Intel Iris Xe** 这类 4 核 8 线程掌机硬件，做源码级性能与稳定性优化实验。

目标硬件特征：

- CPU：Intel Core i7-1195G7，4C/8T，Tiger Lake
- GPU：Intel Iris Xe 核显，共享内存架构
- 典型运行状态：Windows 10/11，掌机散热空间有限，长时间高负载容易触及 100 度温度墙并降频
- 主要优化目标：在不牺牲画质正确性的前提下，降低不必要 CPU 开销，缓解热降频，减少 Vulkan/内存/着色器路径上的卡顿

## 项目定位

本 fork 供源码学习、性能调优实验和个人硬件适配交流使用。

仓库不包含游戏、密钥、固件或任何受版权保护的内容；请只使用你依法拥有并自行备份的内容进行测试。

这不是 Eden 官方版本，也不代表 Eden 上游项目的默认行为。

本仓库里的 profile 被有意写成默认启用，因为它就是为了 OneXPlayer 1S i7-1195G7 这台机器做的专项版本。

## 当前优化重点

### 1. OneXPlayer 1195G7 专用 profile

源码中加入了 OneXPlayer i7-1195G7/Iris Xe profile，并在启动时默认应用。它会优先选择更适合这台机器的运行路径：

- 默认使用 Vulkan 后端
- 默认主机模式
- 帧率限制和内部分辨率交给图形界面设置，不再在源码里锁死 30 FPS 或 1x
- 默认保持偏安全的 CPU/cache 设置，避免用牺牲画面正确性换性能
- 默认开启磁盘 shader cache 和 Vulkan driver pipeline cache

### 2. 针对 4C/8T 掌机的线程调度

i7-1195G7 的单核睿频很高，但掌机散热下持续负载很容易撞到 100 度温度墙。这个 fork 的重点之一是让关键线程保留响应速度，同时把后台工作尽量压到更合适的位置：

- Guest CPU 核心线程使用较高优先级，并关闭 Windows 执行速度节流
- Vulkan/GPU/后台 worker 尽量放到 SMT sibling lanes，避免和主 CPU 执行线程硬抢前台核心
- Vulkan pipeline worker 默认限制为 3 个，防止开放世界游戏边跑边编译 shader 时把 4C/8T CPU 压到过热
- 纹理解码、管线序列化等后台线程降低优先级，减少对主线程和 GPU 提交线程的干扰
- spin lock 在长时间竞争时主动让出，减少纯空转导致的热量堆积

可通过环境变量临时调整 shader 编译并行度：

```powershell
$env:EDEN_1195G7_SHADER_WORKERS="4"
```

可选范围为 `1` 到 `4`。更高并行度可能减少 shader 编译等待，但也可能提高温度并引发降频，需要按具体游戏测试。

### 3. Vulkan 提交与管线缓存优化

针对 Iris Xe Windows Vulkan 路径做了多处调度层优化：

- Vulkan command chunk 预留，减少运行中频繁分配
- Iris Xe 路径偏向原生单 draw，避免一部分单 draw 场景走多 draw wrapper 的额外开销
- 增大 Vulkan upload stream buffer，降低开放世界场景持续上传资源时的抖动
- 保留 Vulkan driver pipeline cache，减少重复启动和重复场景的 pipeline 构建成本
- 默认关闭 SPIR-V 输出优化，减少运行期 shader 编译 CPU 开销

### 4. GPU cache invalidation 与内存同步优化

模拟器在开放世界游戏里会频繁处理 CPU/GPU 共享内存、纹理、buffer 和 shader cache 的失效。这个 fork 对相关路径做了针对性优化：

- GPU dirty memory 为空时快速跳过不必要的 cache invalidation
- 异步 GPU 模式下将一部分 cache invalidation 排队到 GPU 线程处理
- 对连续或重叠 invalidation range 做合并，减少 texture/buffer/pipeline cache 反复扫描
- 队列长度有上限，避免积压过多导致延迟失控
- 1195G7 profile 的严格模式和运行参数已缓存为原子 flags，避免高频路径反复读取环境变量

这些改动的目标是减少 CPU 消耗和锁竞争，而不是跳过必要同步。

### 5. 纹理、ASTC 与 unswizzle 路径

Iris Xe 是共享内存核显，纹理处理既可能占 GPU，也可能占 CPU 和内存带宽。本 fork 的策略是：

- 默认使用 GPU ASTC 解码，减少 CPU 侧纹理解码压力
- 默认开启 GPU unswizzle，并使用较保守的大小/流式参数，避免把小机器的内存带宽一次吃满
- 纹理相关后台 worker 保持低优先级和有限并行度，降低热降频风险

### 6. Dynarmic 代码缓存

针对内存容量相对够用、但 CPU 编译/JIT 抖动明显的使用场景，增大 Dynarmic code cache：

- x86_64 + OneXPlayer profile 下，代码缓存至少提升到 1 GiB

这有助于减少长期游玩时的重复 JIT 压力。

### 7. Splatoon 3 shader 正确性修复

在排查 Splatoon 3 红色绒毛贴图满屏闪烁问题时，发现 Maxwell shader recompiler 中 `VOTE_vtg` 原本是 stub。

本 fork 已将 `VOTE_vtg` 接入现有 `VOTE` 实现链路，使其能够生成 subgroup vote/ballot IR，而不是静默忽略。

这个修复属于画面正确性修复，不是牺牲性能的同步保守化改动。

## 保守开关

如果要排查某个游戏的异常，可以用这些环境变量临时关闭部分优化：

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

这些实验性开关可能带来额外性能，也可能造成画面错误、闪退或奇怪卡顿，默认不启用。

## Windows 构建

推荐使用 GitHub Actions 的 Windows 构建流程：

1. 打开仓库的 Actions 页面
2. 选择 `Build Windows OneXPlayer 1195G7 013`
3. 点击 `Run workflow`
4. 构建完成后下载 artifact：`Eden-Windows-onexplayer-1195g7-013.zip`

也可以在 Windows 10/11 本机执行：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\windows\build-onexplayer-1195g7-010.ps1
```

输出文件位于：

```text
artifacts\Eden-Windows-onexplayer-1195g7-013.zip
```

## 测试建议

为了判断优化是否有效，建议固定这些变量再对比：

- Windows 电源模式
- OneXPlayer TDP 设置
- 风扇策略和散热状态
- Intel 显卡驱动版本
- 游戏场景和存档位置
- 是否已有 shader cache
- GUI 中的帧率限制、分辨率倍率、主机/掌机模式

如果遇到明显画面异常，优先用 strict 开关逐项排查，而不是直接打开 unsafe 选项。

## 上游与许可证

本 fork 基于 Eden Emulator Project。原项目及其衍生来源的版权声明保留在源码文件中。

Eden 使用 GPLv3 或更高版本授权，详见 [LICENSE.txt](./LICENSE.txt)。
