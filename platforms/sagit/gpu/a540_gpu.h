/* SPDX-License-Identifier: MIT
 * Adreno 540 (A5XX, Xiaomi Mi 6 / MSM8998 / sagit) bring-up skeleton.
 *
 * The register NAMES/OFFSETS come from a540_regs.h (generated from Mesa's
 * a5xx.xml).  The SEQUENCES and VALUES are a port of the Linux msm driver:
 *   drivers/gpu/drm/msm/adreno/a5xx_gpu.c    (hw_init / me_init / ucode bases)
 *   drivers/gpu/drm/msm/adreno/a5xx_power.c  (a540_lm_setup / pc_init / gpmu)
 * Linux is GPL-2.0; this file is MIT and contains only the hardware sequences,
 * which are facts about the chip.
 *
 * STATUS: host-tested skeleton only.  Nothing here has run on silicon yet.
 * A540 differences vs the shipping NativeAdreno A740 driver:
 *   - A5XX has a GPMU (power/thermal microcontroller), NOT the A6XX/A7XX "GMU"
 *     that owns the CP ring and per-context SMMU.  GPMU is kicked by a CP IB.
 *   - SMMU is Qualcomm SMMUv1 (outside the GPU MMIO window), not the A740
 *     "native 4 KiB SMMU contexts".
 *   - microcode: a530_pfp.fw + a530_pm4.fw + a530_gpmu.fw (A540 shares the
 *     A530 firmware images; there is no separate a540_*.fw in linux-firmware).
 *   - no Turnip/Vulkan: freedreno has no A5XX Vulkan backend at all.
 */
#ifndef A540_GPU_H
#define A540_GPU_H

#include <stdint.h>
#include "a540_regs.h"

/* CP packet opcodes (Mesa src/freedreno/common/pm4.h) */
#define A540_CP_ME_INIT              0x48
#define A540_CP_SET_PROTECTED_MODE   0x63
#define A540_CP_INDIRECT_BUFFER_PFE  0x1f

typedef struct a540_dev a540_dev;

struct a540_dev {
    void (*mmio_write)(a540_dev *d, uint32_t off, uint32_t val);
    uint32_t (*mmio_read)(a540_dev *d, uint32_t off);
    void *ctx;          /* caller data (fake MMIO on host, MMIO window on XNU) */
    uint32_t patchid;   /* ADRENO_CHIP_PATCHID; 0 on production A540 */
    uint32_t freq_mhz;  /* active GPU clock, for the GPMU voltage table */
    uint32_t mvolts;    /* rail voltage in mV (Linux takes it from the OPP table) */
    uint32_t writes;    /* diagnostics */
    uint32_t reads;
};

typedef struct {
    uint32_t *ptr;
    uint32_t words;
    uint32_t cap;
} a540_ring;

enum a540_status {
    A540_OK            =  0,
    A540_ERR_TIMEOUT   = -1,
    A540_ERR_RING_FULL = -2,
    A540_ERR_GPMU      = -3
};

static inline void a540_wr(a540_dev *d, uint32_t off, uint32_t v)
{
    d->mmio_write(d, off, v);
    d->writes++;
}

static inline uint32_t a540_rd(a540_dev *d, uint32_t off)
{
    uint32_t v = d->mmio_read(d, off);
    d->reads++;
    return v;
}

/* Ports driver-provided ops only; no MMU/Linux helpers. */
int  a540_wait_reg(a540_dev *d, uint32_t off, uint32_t mask, uint32_t want, uint32_t spins);
int  a540_rbbm_idle(a540_dev *d, uint32_t spins);

/* a5xx_hw_init() tail: microcode + ring bases (see a5xx_gpu.c:926-953). */
int  a540_program_ucode_bases(a540_dev *d, uint64_t pm4_iova, uint64_t pfp_iova,
                              uint64_t rb_iova, uint32_t rb_cntl, uint32_t int_mask);

/* a5xx_power.c: a540_lm_setup / a5xx_pc_init / a5xx_lm_enable(a540 = no-op). */
void a540_lm_setup_a540(a540_dev *d);
void a540_pc_init(a540_dev *d);
void a540_lm_enable_a540(a540_dev *d);

/* a5xx_power.c: a5xx_gpmu_init() tail + readback handshake. */
int  a540_gpmu_kick(a540_dev *d, uint32_t spins);

/* a5xx_gpu.c: a5xx_me_init() - the CP_ME_INIT packet. */
int  a540_ring_pkt7(a540_ring *r, uint16_t opcode, uint16_t count);
int  a540_ring_word(a540_ring *r, uint32_t v);
int  a540_me_init(a540_ring *r);

/* Bring-up display path that does NOT need the GPU: write the simple
 * framebuffer that ABL/UEFI leaves at 0x9D400000 (1080x1920, stride 4320,
 * a8r8g8b8).  This is what can be validated without any GPU programming. */
void a540_fb_put(uint32_t x, uint32_t y, uint32_t color);
void a540_scanout_solid(a540_dev *d, uint32_t color);

#ifdef A540_HOST_TEST
extern uint32_t a540_host_fb[A540_FB_WIDTH * A540_FB_HEIGHT];
static inline uint32_t *a540_fb(void) { return a540_host_fb; }
#else
static inline uint32_t *a540_fb(void)
{
    return (uint32_t *)(uintptr_t)A540_FB_BASE;
}
#endif

#endif
