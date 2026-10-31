# Adreno 540 (A5XX) 移植文档 — 小米6 / sagit
更新：2026-10-22 夜

## 0. 一句话
A540 是 **A5XX**，官方 NativeAdreno/NativeMetal 是 **A6XX/A7XX** 实现；A5XX 没有任何一行可复用，必须自己写——本文给出**已写好的骨架**、与之对应的 Linux 官方参考序列、以及上真机后的验证顺序。

## 1. 与 A740（官方实现）的差异表

| 项目 | A740（NativeAdreno 已实现） | A540（小米6，本骨架） | 影响 |
|---|---|---|---|
| 家族 | A6XX–A7XX | **A5XX** | 寄存器表不同（本文用 mesa a5xx.xml，1004 个 reg32） |
| 电源/时钟单元 | **GMU**（独立固件，管 CP 环 + per-context SMMU） | **GPMU**（功率/热管理单片机，不拥有 CP 环） | 不能照搬 GMU 启动代码；GPMU 用 CP 间接缓冲加载 `a530_gpmu.fw` |
| 上电/时钟序列 | GMU 固件负责 | RBBM + GPMU 数据 RAM 邮箱（AGC 消息） | 本文 `a540_lm_setup_a540()` 逐条移植自 `a5xx_power.c:175` |
| SMMU | "native 4 KiB SMMU contexts"（A740 代码内） | **Qualcomm SMMUv1**（GPU MMIO 窗口之外） | 页表/上下文初始化需要单独实现，不能复用 A740 的 4K 上下文代码 |
| 微码 | 各机型自带 | **a530_pfp.fw + a530_pm4.fw + a530_gpmu.fw**（A540 与 A530 共用） | 三个都要，缺一个 CP 起不来 |
| USER 态后端 | metal2vulkan → Turnip（A6XX/A7XX） | **Turnip 明确不支持 5xx**（无 Vulkan） | Metal 链路在小米6 上断头，只能走 GL/CPU 路径 |
| GPMU 加载方式 | — | CP `INDIRECT_BUFFER_PFE` 把微码当 IB 提交，然后 `CM3_SYSRESET=0` 释放，回读 `GPMU_GENERAL_0 == 0xBABEFACE` | 握手失败 = GPMU 没跑起来，`GENERAL_1` 非 0 = 固件报错 |

## 2. 已实现的骨架（platforms/sagit/gpu/）

| 文件 | 内容 | 状态 |
|---|---|---|
| `a540_regs.h` | 1004 个 A5XX reg32（mesa `a5xx.xml` 生成，dword 偏移） | 生成完毕 |
| `a540_gpu.h` | 设备抽象（MMIO 读写回调）、环形缓冲、状态码、API | 完成 |
| `a540_gpu.c` | `a540_lm_setup_a540` / `a540_pc_init` / `a540_lm_enable_a540` / `a540_gpmu_kick` / `a540_program_ucode_bases` / `a540_me_init` / `a540_scanout_solid` | 完成，CI 编译 + 主机单测 |
| `test_a540_gpu.c` | 假 MMIO 窗口上断言寄存器序列/数值/顺序（12 条 LM 写入、4 条 PC、9 条基址、CP_ME_INIT 包、GPMU 握手成功/超时/错误码、scanout 像素） | 完成 |

序列来源（逐条对应）：
- `a540_lm_setup_a540()` ← `a5xx_power.c:175`（A540 专属：BCL 关闭、GPMU 侧节流关闭、单功率档、AGC 邮箱 magic 0xBABEFACE）
- `a540_pc_init()` ← `a5xx_power.c:211`
- `a540_lm_enable_a540()` ← `a5xx_power.c:278`（**A540 上是 no-op**，只有 A530 才执行）
- `a540_gpmu_kick()` ← `a5xx_power.c:220`（A540 不写 `GPMU_WFI_CONFIG`，那是 A530 专属）
- `a540_program_ucode_bases()` ← `a5xx_gpu.c:926-953`
- `a540_me_init()` ← `a5xx_gpu.c:477`（A540 走"无 workaround"分支，不是 0x0B）

## 3. 尚未实现（诚实清单）
1. **SMMUv1**：上下文/页表/故障中断。A5XX 的 GPU 通过 Qualcomm SMMUv1 访问内存，必须先把这段做出来，CP 才能读微码/环。
2. **微码文件**：`a530_pfp.fw`/`a530_pm4.fw`/`a530_gpmu.fw` 需要从 MIUI 的 vendor 分区（或 linux-firmware）取出，放到内核可读的文件系统/负载里。
3. **IB 提交**：`a540_gpmu_kick()` 之前要用 CP 环提交 `CP_INDIRECT_BUFFER_PFE` 装载 GPMU 微码（Linux 在 `a5xx_gpmu_init()` 里做），本骨架只做了释放与握手。
4. **scanout**：目前 `a540_scanout_solid()` 直接把像素写进 ABL/UEFI 留下的 framebuffer（0x9D400000，1080×1920，stride 4320，a8r8g8b8），这是**不需要 GPU** 的路径；GPU 合成桌面需要 Metal 前端 + GL/CPU 后端，属后续。
5. **时钟/电压**：`d->mvolts`/`d->freq_mhz` 现在由调用者填；XNU 侧要么读 OPP 表，要么先用固定值（670MHz/900mV 量级）验证。
6. **中断**：RBBM/CP 错误中断未接（bring-up 阶段可先屏蔽）。

## 4. 上真机后的验证顺序（每步都要回读）
| 步骤 | 动作 | 判据 |
|---|---|---|
| G0 | 从 shim 访问 GPU 窗口（0x0C800000/MMSS 区域），读 `RBBM_STATUS`(0x4f5)、`RBBM_CLOCK_STATUS` | 读数非 0xFFFFFFFF/0，说明时钟/总线通 |
| G1 | 打开 MMSS/GPU 电源与时钟（sagit 在 RPM/时钟控制器侧，需另实现） | `RBBM_STATUS` 变化符合预期 |
| G2 | `a540_lm_setup_a540()` + `a540_pc_init()` | 写后回读 AGC magic 位；GPMU 数据 RAM 可写 |
| G3 | 装载 `a530_gpmu.fw` 并 `a540_gpmu_kick()` | `GPMU_GENERAL_0 == 0xBABEFACE`，`GENERAL_1 == 0` |
| G4 | 装载 `a530_pfp.fw`/`a530_pm4.fw`，`a540_program_ucode_bases()` + `a540_me_init()` | `CP_ME_STATUS` 无 fault，环 wptr/rptr 能推进 |
| G5 | 提交一个空 IB，等待完成 | CP 中断/`CP_SCRATCH` 计数增加 |
| G6 | `a540_scanout_solid()` 涂色 | 屏幕变色（证明显示路径，不依赖 GPU） |
| G7 | 简单 PM4 绘制（清屏/画三角） | 像素出现在 framebuffer |

## 5. 参考来源
- Linux: `drivers/gpu/drm/msm/adreno/a5xx_power.c`（A540 LM/GPMU/PC 初始化）
- Linux: `drivers/gpu/drm/msm/adreno/a5xx_gpu.c`（hw_init 尾部、me_init、微码基址）
- Mesa: `src/freedreno/registers/adreno/a5xx.xml`（寄存器定义，本仓库 `a540_regs.h` 由此生成）
- linux-firmware: `qcom/a530_pfp.fw`、`qcom/a530_pm4.fw`、`qcom/a530_gpmu.fw`
- 本仓库：`platforms/sagit/gpu/*`、`docs/PORT-SAGIT.md`

## 6. 底线
骨架 ≠ 驱动可用。A5XX 的 Vulkan 后端在 Mesa 里**明确没有计划**，所以"GPU 合成 iOS 桌面"在小米6 上的现实上限是：**CPU/GL 软渲染 + MDP 扫描**，GPU 加速需要自写 GL 后端，属于数月级工作。
