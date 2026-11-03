# 小米6（sagit）移植 iOS：设备层移植说明

> 分支：`sagit`（基于 `topaz` / ios-sweet 血统）
> 目标：把 Redmi Note 12 4G（topaz，SM6225）上已验证的 shim → DarwinBoot → XNU → iOS 用户态 链路，搬到小米6（MSM8998 + Adreno 540，1080x1920）。
> 状态：**P0 设备常量成文化 + shim 可按 PLATFORM=sagit 交叉编译**（本分支）。真机验证待设备接回。

---

## 1. 为什么走 topaz 血统而不是 UEFI

| | UEFI 路线（UniXNU / edk2） | shim 路线（ios-sweet / topaz） |
|---|---|---|
| 引导层 | 先换 edk2 UEFI，再在 UEFI 里加载 payload | 小米 ABL 直接 `fastboot boot` 一个 boot image v4，裸机 shim 里自己找 payload |
| 小米 ABL 兼容 | 需要 UEFI 固件把设备描述全（我们已让 edk2 在小米6 跑起来过） | 已经解决：**v4 头 + payload 追加在 kernel Image 的 `_end` 之后**（ABL 会丢 v4 ramdisk） |
| 是否需要重写内核 | 要（XNU 不能在 UEFI 下跑） | 不需要（XNU 直接跑在 EL1） |
| 结论 | 参考用 | **主线** |

已实测：小米6 可以 `fastboot boot`（不是 flash）非 Android 载荷，且能回滚；userdata 会被清，boot 分区可随时用 MIUI 线刷包恢复。

## 2. 已完成（本分支内容）

- `platforms/sagit/platform_sagit.h`：小米6 全部硬件常量（来源：edk2-msm8998 内存映射表 + sagit 设备树 DTB 实测解析）。
- `tools/mkadt_sagit.py`：小米6 版 ADT（Apple Device Tree）生成器，复用上游 `mkadt.py` 的 `build()`，只覆盖硬件常量。
- `shim/shim.c`、`shim/ufs.c`、`shim/build.sh`：支持 `PLATFORM=sagit` 编译（`-DPLATFORM_SAGIT -I../platforms/sagit`），默认仍是 topaz，互不影响。
- `.github/workflows/sagit-shim.yml`：无手机即可交叉编译 shim（ubuntu + `gcc-aarch64-linux-gnu`）。
- 参考件：iPhone X iOS 16.7.16 (20H392) IPSW 已下载（6,171,035,648 B）；edk2-msm8998 与 LineageOS sagit 设备树已拉到本地。

## 3. 硬件常量对照

| 项 | topaz（Redmi Note 12 4G） | sagit（小米6） | 来源 |
|---|---|---|---|
| SoC | SM6225 khaje | MSM8998 | — |
| GICD | 0x0f200000 | **0x17A00000** | edk2 PlatformMemoryMapLib |
| GICR | 0x0f300000 / 1 MiB | **0x17B00000 / 1 MiB**（stride 0x20000） | 同上 |
| UFS 主机 | 0x04804000 | **0x01DA0000**（size 0x20000） | 同上 "UFS UFS REGS" |
| IMEM 重启原因 | 0x0c12565c | **0x146BF65C**（msm-imem@146bf000 + 0x65c） | 同上 IMEM Cookie Base |
| PS_HOLD | 0x0440b000 | **0x010AC000**（MPM2_PSHOLD，暂定） | 同上 |
| 帧缓冲 | 0x5c000000 / 0xf00000 | **0x9D400000 / 0x02400000**，1080x1920，stride 4320，a8r8g8b8 | sagit DTB `/chosen/framebuffer@9d400000` |
| RAM | 0xb0000000 / 0x4d000000 | **0xA0000000 / 0x60000000**（1.5 GB 干净窗口）；另一段 0x100000000 / 0xBD8C0000 备用 | Mem6G 条目 |
| CPU | 4×A73 Gold MPIDR 0x100-0x103 | **同构**：Gold 0x100-0x103（Kryo280 A73），Silver 0x0-0x3 停 | sagit DTB `/cpus` |
| 定时器 | 19.2 MHz | **19.2 MHz** | QTIMER / DTB |
| ECID | 'TOPAZ'+1 | **'SAGIT'+1 = 0x5341474954000001**（合成） | 本仓库 |

## 4. 需要改/验证的代码点（按里程碑）

1. **shim 启动**：`shim/shim.c` 的 GIC/GICR/IMEM/PSHOLD 常量已条件化；需真机验证 GICv3 sysreg 接口（ICC_SRE_EL2）在 MSM8998 上可用、Gold GICR 能唤醒。
2. **payload 定位**：`shim/loader.c` 在 ABL 给的 ramdisk 区里找 `SWEETXNU`。小米6 的 ABL 行为需要用 `shim/bootv2`/`unpack_bootv2.py` 核对（topaz 已解决，预期可复用）。
3. **UFS**：`shim/ufs.c` 只读探测 LUN0。MSM8998 的 UFSHCI 与 SM6225 同代（JESD223），预期可用；需确认 ABL 留下的 link 状态与 `UFS_BASE` 对齐。
4. **ADT**：`tools/mkadt_sagit.py` 生成；需从 iPhone X IPSW 抽真实 `/product`（`tools/remotezip.py` + `ipsw img4 im4p extract`）否则 iOS 用户态的设备身份不完整。
5. **内核补丁**：topaz 的 139 个补丁里 0001–0079 是通用 sweet 栈，0080+ 是 topaz 专有。小米6 需要：
   - 复用 0081（ARMv8.0 无 LSE/无 PAN 模拟）、0082（只在 Gold 核启动）；
   - 重写 0083（触控）：小米6 是 **Synaptics RMI4**，不是 FocalTech FT5452；
   - 重写 0092/0093（MDP scanout）：小米6 是 **MSM8998 MDSS/MDP5**，不是 SM6225 的 MDP；
   - 重写 0104–0112（DSI 亮度/DCS）：面板是 JDI R63452；
   - 显示/GPU 完全不适用（Adreno 540 = A5XX，原生无 Metal 驱动）。
6. **显示身份**：小米6 原生 1080x1920 与 iPhone X（1125x2436）不一致。两条路：
   (a) 保留 iPhone X 身份 → 需要缩放/裁切，SpringBoard 可能布局错位；
   (b) 换 **iPhone 8 Plus（D21AP / iPhone10,2，1080x1920 原生物理尺寸）** 身份 → 需要对应 IPSW 的 userland，改动更大但显示最正。
   建议：M1–M3 先用 (a) 把链路跑通，M4 显示里程碑再决定。

## 5. 里程碑（对齐 UniXNU 的 7 阶段 + 真机判据）

| # | 里程碑 | 判据 |
|---|---|---|
| M1 | shim 在小米6 跑起来 | `fastboot boot` 后屏幕出现 shim 控制台/串口有 `PAYLOAD` 字样 |
| M2 | payload 定位与拷贝 | 日志出现 `PAYLOAD at=... to=...`、`SEGS n=...` |
| M3 | DarwinBoot 打印 | 屏幕出现 vbase/pbase/cmdline/entry/framebuffer，EL1 |
| M4 | XNU 内核入口 | 内核早期日志（platform expert / UART） |
| M5 | 用户态 | launchd 起来，日志出现 dyld 映射 |
| M6 | 显示 | 屏幕出现 framebuffer 画面（先黑底白字控制台也算） |
| M7 | SpringBoard | 桌面能画出来（CPU 渲染，帧率低） |

## 6. 首轮真机命令（待设备接回）

```bash
# 1) 确认 ABL 在
fastboot devices                      # 期望 17eeed3f fastboot
# 2) 只 boot 不 flash（可回滚）
fastboot boot shim-boot.img
# 3) 看屏幕 / 串口（UART: TP707 TX / TP708 RX, 115200）
```

打包（需要 payload；没有内核时先用空 payload 验证 shim 自检）：

```bash
MKBOOTIMG=/path/to/mkbootimg.py PLATFORM=sagit shim/build.sh
```

## 7. 风险与已知边界

- **GPU 无解（现阶段）**：Adreno 540 是 A5XX；HackiOS 的 GPU 补丁是 Adreno 610（A6XX 家族），Turnip 官方明确不支持 5xx。M7 的 SpringBoard 只能 CPU 渲染，浏览器/动画会非常慢。
- **iPhone X 面板不匹配**：见 §4.6。
- **AMFI/AppleKeyStore 是 stub**：无 `data protection`、无 SEP；部分系统服务会降级或崩溃，属预期。
- **UFS 写**：topaz 的 `xxx-ufs-writes` 补丁假定 HFS+ root 在 userdata；小米6 分区表不同（userdata 52.57 GiB，LBA 114688 起），需在 `[userdata]` 窗口里切一块给 iOS root。
- **变砖风险**：不改 boot 分区，只 `fastboot boot`；userdata 会被清（已是备用机）。

## 8. 下一步（P1）

1. 在 Actions 里跑 `sagit-shim`，确认交叉编译通过（无手机）。
2. 设备接回后：`fastboot boot` 只含 shim 的 v4 镜像，抓 M1/M2 日志。
3. 同时准备 iPhone X IPSW 的 HFS+ root（`tools/mkiosroot.sh` 需 Linux + apfs-fuse + hfsprogs；可在 Actions 的 ubuntu runner 里跑）。
4. iOSurface/IOMobileFramebuffer 侧先按 topaz 的 SweetSurface 复用，显示里程碑再改 MDSS/MDP。

---

### 参考

- `MakrSas/ios-topaz`、`dipa-studio/ios-sweet`（上游血统）
- `edk2-porting/edk2-msm8998`（GIC/UFS/IMEM/RAM 常量、sagit DTB）
- `LineageOS/android_device_xiaomi_sagit`（分区、面板、触控名）
- iPhone X / iPhone 8 Plus IPSW（Apple 公开 restore 镜像）

## 状态更新（2026-10-22 凌晨）
- shim（PLATFORM=sagit）已在真机跑通到"加载内核负载"这一步：SAGIT_DEBUG 六条面包屑全亮（_start → spsel → vbar → shim_main → 内核拷贝完成回写）。
- 修复 CI "调试版默认开启" bug（`SAGIT_DEBUG` 判空逻辑），重建出干净 shim。
- XNU 内核已用 GitHub macOS runner（macOS 14 + Xcode 15，`DEVELOPMENT_ARM64_BCM2837` / `SWEET` 配置）编译成功：`kernel.development.sweet` 17,817,888 B。
- 负载打包：`mkadt_sagit.py` → ADT 9,688 B；`mkpayload.py` → payload 17,206,000 B（virt_base 0xfffffe0006000000，entry 0xfffffe00074b6488）。
- 可刷镜像：`out/sagit-kernel-boot-clean.img` 11,548,673 B，sha256 99F01DB3…C9CDB8。
- GPU：`platforms/sagit/gpu/` 新增 A540（A5XX）骨架 + 主机单测；见 `docs/GPU-A540-PORT.md`。
