/*
 * Xiaomi Mi 6 (sagit) platform constants for the ios-sweet / topaz lineage.
 *
 * Everything here was measured, not guessed:
 *   - GIC / UFS / IMEM / SOC window: Silicon/QC/Msm8998/QcomPkg/Library/
 *     PlatformMemoryMapLib/PlatformMemoryMapLib.c (edk2-porting/edk2-msm8998,
 *     the UEFI that already boots on this exact phone).
 *   - Framebuffer: the ABL simple-framebuffer node in the sagit DTB
 *     (framebuffer@9d400000, 1080x1920, stride 4320, a8r8g8b8) and
 *     edk2 PcdMipiFrameBufferAddress.
 *   - RAM: the Mem6G entries of the same memory map.
 *   - CPUs: sagit DTB /cpus (4x Kryo280 Silver MPIDR 0x0-0x3,
 *     4x Kryo280 Gold MPIDR 0x100-0x103), PSCI enable-method.
 *
 * Device data only; no topaz/sweet code depends on it unless
 * PLATFORM_SAGIT is defined at build time.
 */
#ifndef PLATFORM_SAGIT_H
#define PLATFORM_SAGIT_H

#define PLATFORM_SAGIT_NAME		"sagit"
#define PLATFORM_SAGIT_SOC		"msm8998"

/* ---- GIC-500 (GICv3), 8 CPUs ---------------------------------------- */
#define SAGIT_GICD_BASE			0x17A00000UL	/* APCS_GIC500_GICD */
#define SAGIT_GICD_SIZE			0x00010000UL
#define SAGIT_GICR_BASE			0x17B00000UL	/* APCS_GIC500_GICR */
#define SAGIT_GICR_SIZE			0x00100000UL
#define SAGIT_GICR_PE_SIZE		0x00020000UL	/* 128 KiB per redistributor */
#define SAGIT_SOC_BASE			0x01000000UL	/* ADT /arm-io ranges base */

/* ---- UFS host controller (ABL leaves the link trained) --------------- */
#define SAGIT_UFS_BASE			0x01DA0000UL	/* "UFS UFS REGS" */
#define SAGIT_UFS_SIZE			0x00020000UL

/* ---- IMEM restart reason + power hold -------------------------------- */
#define SAGIT_IMEM_RESTART_REASON	0x146BF65CUL	/* msm-imem@146bf000, restart_reason@65c */
#define SAGIT_RESTART_BOOTLOADER	0x77665500U
#define SAGIT_PSHOLD			0x010AC000UL	/* MPM2_PSHOLD (provisional) */

/* ---- ABL framebuffer (simple-framebuffer@9d400000) ------------------- */
#define SAGIT_FB_BASE			0x9D400000UL
#define SAGIT_FB_SIZE			0x02400000UL
#define SAGIT_FB_WIDTH			1080
#define SAGIT_FB_HEIGHT			1920
#define SAGIT_FB_STRIDE			4320		/* 1080 px * 4 B */
#define SAGIT_FB_FORMAT			"a8r8g8b8"

/* ---- RAM: 6 GB phone. Clean window above every ABL carve-out. -------- */
#define SAGIT_DDR_BASE			0xA0000000UL	/* Mem6G bank: 0xA0000000..0xFFFFFFFF */
#define SAGIT_DDR_SIZE			0x60000000UL
#define SAGIT_DDR_HIGH_BASE		0x100000000UL	/* second Mem6G bank, not used yet */
#define SAGIT_DDR_HIGH_SIZE		0xBD8C0000UL

/* ---- CPUs: boot on the first Gold core, park the Silver cluster ------ */
#define SAGIT_BOOT_MPIDR		0x100
#define SAGIT_GOLD_MPIDR_FIRST		0x100
#define SAGIT_GOLD_MPIDR_LAST		0x103
#define SAGIT_SILVER_MPIDR_FIRST	0x000
#define SAGIT_SILVER_MPIDR_LAST		0x003
#define SAGIT_TIMEBASE			19200000UL	/* 19.2 MHz arch timer */

/* ---- Identity used while bringing up the iPhone X userland ----------- */
#define SAGIT_ECID			0x5341474954000001ULL	/* 'SAGIT' + 1, synthetic */

#endif /* PLATFORM_SAGIT_H */
