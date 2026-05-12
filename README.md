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
    Result-focused Eden fork optimized for OneXPlayer 1S, Intel Core i7-1195G7, and Intel Iris Xe.
  </p>

  <p>
    <a href="#english"><img alt="English default" src="https://img.shields.io/badge/English-default-2f81f7?style=for-the-badge"></a>
    <a href="#zh-cn"><img alt="Chinese" src="https://img.shields.io/badge/%E4%B8%AD%E6%96%87-%E5%AF%B9%E7%85%A7-34a853?style=for-the-badge"></a>
  </p>

  <p>
    <img alt="Target hardware" src="https://img.shields.io/badge/Target-OneXPlayer%201S-1f6feb?style=flat-square">
    <img alt="CPU" src="https://img.shields.io/badge/CPU-i7--1195G7-0071c5?style=flat-square&logo=intel&logoColor=white">
    <img alt="GPU" src="https://img.shields.io/badge/GPU-Iris%20Xe-5e5ce6?style=flat-square">
    <img alt="Focus" src="https://img.shields.io/badge/Focus-performance%20%2B%20stability-f97316?style=flat-square">
  </p>
</div>

---

<a id="english"></a>

## English

### Performance Improvements

| Area | Result |
| --- | --- |
| Overall smoothness | Improved frame pacing and reduced heavy stutter on the target OneXPlayer 1S hardware. |
| CPU pressure | Reduced avoidable CPU load during sustained gameplay. |
| Thermal behavior | Lowered background heat pressure to help the i7-1195G7 hold performance more steadily under long sessions. |
| Shader-related interruptions | Reduced shader-related interruption during gameplay. |
| Graphics rendering | Improved rendering stability and reduced unnecessary overhead on Intel Iris Xe. |
| Scene streaming workload | Reduced avoidable workload during scene and asset streaming. |
| Handheld responsiveness | Improved responsiveness of critical emulation and rendering work on a 4-core / 8-thread handheld CPU. |
| Configuration flexibility | Frame limit and internal resolution can be adjusted through the graphical settings instead of being fixed by this fork. |

### Fixed Issues

| Game / Area | Result |
| --- | --- |
| Splatoon 3 | Fixed the severe full-screen flashing texture corruption seen in some scenes and angles. |
| Splatoon 3 | Reduced remaining small-area flicker on specific surfaces or scene regions. |
| Splatoon 3 | Fixed shader-related visual corruption affecting special texture effects. |
| Rendering stability | Improved cases that could previously cause flashing or unstable visuals. |
| Texture correctness | Improved correctness in complex surface and texture reuse scenes. |
| Frame pacing | Reduced unusual stalls in several rendering-heavy situations. |

<p align="right"><a href="#readme-top">Back to top</a></p>

---

<a id="zh-cn"></a>

## 中文

### 性能优化结果

| 方向 | 结果 |
| --- | --- |
| 整体流畅度 | 在目标 OneXPlayer 1S 硬件上提升帧时间稳定性，减少明显卡顿。 |
| CPU 压力 | 降低持续游戏时不必要的 CPU 负载。 |
| 温度表现 | 降低后台热压力，帮助 i7-1195G7 在长时间运行时更稳定地维持性能。 |
| 着色器相关中断 | 减少游戏过程中的 shader 相关中断感。 |
| 图形渲染 | 提升 Intel Iris Xe 上的渲染稳定性，并减少不必要开销。 |
| 场景流式加载负载 | 降低场景和资源流式加载时的不必要负载。 |
| 掌机响应性 | 改善 4 核 8 线程掌机 CPU 上关键模拟与渲染任务的响应性。 |
| 配置灵活性 | 帧率限制和内部分辨率可通过图形界面设置，不再由本 fork 固定。 |

### 已修复问题

| 游戏 / 方向 | 结果 |
| --- | --- |
| Splatoon 3 | 修复部分场景和角度下出现的严重满屏贴图闪烁/画面污染。 |
| Splatoon 3 | 减轻特定表面或局部画面区域的小范围闪烁。 |
| Splatoon 3 | 修复影响特殊贴图效果的 shader 相关画面错误。 |
| 渲染稳定性 | 改善此前可能出现的闪烁或画面不稳定。 |
| 纹理正确性 | 改善复杂表面和纹理复用场景下可能出现的错误贴图。 |
| 帧时间稳定性 | 减少若干高渲染压力场景中的异常停顿。 |

<p align="right"><a href="#readme-top">回到顶部</a></p>
