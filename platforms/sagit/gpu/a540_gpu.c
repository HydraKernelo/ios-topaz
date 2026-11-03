/* SPDX-License-Identifier: MIT
 * Adreno 540 (A5XX) bring-up skeleton - implementation.
 * Sequences ported from Linux drivers/gpu/drm/msm/adreno/a5xx_power.c and
 * a5xx_gpu.c (see the header for the exact citation).
 */
#include "a540_gpu.h"

/* ---- AGC mailbox (a5xx_power.c:14-24). The GPMU data RAM block is a plain
 * dword window; these are its conventional slots, not chip registers. ---- */
#define AGC_INIT_MSG_MAGIC     (A540_GPMU_DATA_RAM_BASE + 5u)
#define AGC_MSG_STATE          (A540_GPMU_DATA_RAM_BASE + 7u)
#define AGC_MSG_COMMAND        (A540_GPMU_DATA_RAM_BASE + 8u)
#define AGC_MSG_PAYLOAD_SIZE   (A540_GPMU_DATA_RAM_BASE + 10u)
#define AGC_MSG_PAYLOAD(o)     (A540_GPMU_DATA_RAM_BASE + 12u + (o))

#define AGC_INIT_MSG_VALUE             0xBABEFACEu
#define AGC_POWER_CONFIG_PRODUCTION_ID 1u
#define AGC_LM_CONFIG        (136u / 4u)  /* A540+: 34 dwords */
#define AGC_LEVEL_CONFIG     (140u / 4u)  /* A540+: 35 dwords */
#define AGC_LM_CONFIG_GPU_VERSION_SHIFT 17u
#define AGC_LM_CONFIG_THROTTLE_DISABLE  (2u << 8)
#define AGC_LM_CONFIG_BCL_DISABLED      (1u << 24)
#define AGC_LEVEL_CONFIG_VALUE          (~0x303u)

int a540_wait_reg(a540_dev *d, uint32_t off, uint32_t mask, uint32_t want,
                  uint32_t spins)
{
    uint32_t i;
    for (i = 0; i < spins; i++) {
        if ((a540_rd(d, off) & mask) == want)
            return A540_OK;
    }
    return A540_ERR_TIMEOUT;
}

int a540_rbbm_idle(a540_dev *d, uint32_t spins)
{
    /* A5XX RBBM_STATUS bit 0 is the "GPU busy" flag in Mesa's a5xx.xml
     * consumers.  TODO(sagit): read RBBM_STATUS back on silicon once the shim
     * can touch the GPU window, and pin the exact idle bit before using this
     * as a gate for register programming. */
    return a540_wait_reg(d, A540_RBBM_STATUS, 0x1u, 0x0u, spins);
}

int a540_program_ucode_bases(a540_dev *d, uint64_t pm4_iova, uint64_t pfp_iova,
                             uint64_t rb_iova, uint32_t rb_cntl, uint32_t int_mask)
{
    /* a5xx_gpu.c:926-953 */
    a540_wr(d, A540_CP_ME_INSTR_BASE_LO,  (uint32_t)(pm4_iova & 0xFFFFFFFFu));
    a540_wr(d, A540_CP_ME_INSTR_BASE_HI,  (uint32_t)(pm4_iova >> 32));
    a540_wr(d, A540_CP_PFP_INSTR_BASE_LO, (uint32_t)(pfp_iova & 0xFFFFFFFFu));
    a540_wr(d, A540_CP_PFP_INSTR_BASE_HI, (uint32_t)(pfp_iova >> 32));
    a540_wr(d, A540_CP_RB_BASE,           (uint32_t)(rb_iova & 0xFFFFFFFFu));
    a540_wr(d, A540_CP_RB_BASE_HI,        (uint32_t)(rb_iova >> 32));
    a540_wr(d, A540_CP_RB_CNTL,           rb_cntl);
    a540_wr(d, A540_RBBM_INT_0_MASK,      int_mask);
    a540_wr(d, A540_CP_PFP_ME_CNTL,       0u);  /* clear ME_HALT */
    return A540_OK;
}

void a540_lm_setup_a540(a540_dev *d)
{
    /* a5xx_power.c:175 a540_lm_setup(). A540 keeps the battery current limiter
     * off, disables GPMU-side throttling and programs a single power level. */
    uint32_t config = AGC_LM_CONFIG_BCL_DISABLED
                    | ((d->patchid & 0x3FFu) << AGC_LM_CONFIG_GPU_VERSION_SHIFT)
                    | AGC_LM_CONFIG_THROTTLE_DISABLE;

    a540_wr(d, A540_GPMU_GPMU_VOLTAGE,       0x80000000u | 0u);
    a540_wr(d, A540_GPMU_GPMU_PWR_THRESHOLD, 0x80000000u | 6000u);
    a540_wr(d, AGC_MSG_STATE,                0x80000001u);
    a540_wr(d, AGC_MSG_COMMAND,              AGC_POWER_CONFIG_PRODUCTION_ID);
    a540_wr(d, AGC_MSG_PAYLOAD(0),           5448u);   /* max power, A530/A540 */
    a540_wr(d, AGC_MSG_PAYLOAD(1),           1u);
    a540_wr(d, AGC_MSG_PAYLOAD(2),           d->mvolts);
    a540_wr(d, AGC_MSG_PAYLOAD(3),           d->freq_mhz);
    a540_wr(d, AGC_MSG_PAYLOAD(AGC_LM_CONFIG),    config);
    a540_wr(d, AGC_MSG_PAYLOAD(AGC_LEVEL_CONFIG), AGC_LEVEL_CONFIG_VALUE);
    a540_wr(d, AGC_MSG_PAYLOAD_SIZE,         4u * (AGC_LEVEL_CONFIG + 1u));
    a540_wr(d, AGC_INIT_MSG_MAGIC,           AGC_INIT_MSG_VALUE);
}

void a540_pc_init(a540_dev *d)
{
    /* a5xx_power.c:211 enable SP/TP power collapse */
    a540_wr(d, A540_GPMU_PWR_COL_INTER_FRAME_CTRL, 0x0000007Fu);
    a540_wr(d, A540_GPMU_PWR_COL_BINNING_CTRL,     0x00000000u);
    a540_wr(d, A540_GPMU_PWR_COL_INTER_FRAME_HYST, 0x000A0080u);
    a540_wr(d, A540_GPMU_PWR_COL_STAGGER_DELAY,    0x00600040u);
}

void a540_lm_enable_a540(a540_dev *d)
{
    /* a5xx_power.c:278 a5xx_lm_enable() returns early unless the chip is A530,
     * so on A540 this stage is intentionally a no-op. */
    (void)d;
}

int a540_gpmu_kick(a540_dev *d, uint32_t spins)
{
    /* a5xx_power.c:220 a5xx_gpmu_init() tail. The GPMU microcode itself is
     * loaded by the caller as a CP INDIRECT_BUFFER (a530_gpmu.fw); only the
     * release + handshake are done here. A540 must NOT write WFI_CONFIG
     * (that is an A530-only step). */
    int rc;

    a540_wr(d, A540_GPMU_CM3_SYSRESET, 0u);
    rc = a540_wait_reg(d, A540_GPMU_GENERAL_0, 0xFFFFFFFFu, AGC_INIT_MSG_VALUE,
                       spins);
    if (rc != A540_OK)
        return rc;
    if (a540_rd(d, A540_GPMU_GENERAL_1) != 0u)
        return A540_ERR_GPMU;
    return A540_OK;
}

int a540_ring_pkt7(a540_ring *r, uint16_t opcode, uint16_t count)
{
    if (r->words + 1u > r->cap)
        return A540_ERR_RING_FULL;
    r->ptr[r->words++] = ((uint32_t)opcode << 16) | (uint32_t)count;
    return A540_OK;
}

int a540_ring_word(a540_ring *r, uint32_t v)
{
    if (r->words + 1u > r->cap)
        return A540_ERR_RING_FULL;
    r->ptr[r->words++] = v;
    return A540_OK;
}

int a540_me_init(a540_ring *r)
{
    /* a5xx_gpu.c:477 a5xx_me_init(). A540 takes the "no workarounds" branch:
     * the 0x0B token/sync workaround is only for A505/A506/A530. */
    static const uint32_t init[8] = {
        0x0000002Fu,  /* ME init flags */
        0x00000003u,  /* enable multiple hardware contexts */
        0x20000000u,  /* enable error detection */
        0x00000000u,  /* no header dump */
        0x00000000u,
        0x00000000u,  /* A540: no token/sync workaround */
        0x00000000u,
        0x00000000u
    };
    int i, rc = a540_ring_pkt7(r, A540_CP_ME_INIT, 8u);
    if (rc != A540_OK)
        return rc;
    for (i = 0; i < 8; i++) {
        rc = a540_ring_word(r, init[i]);
        if (rc != A540_OK)
            return rc;
    }
    return A540_OK;
}

void a540_fb_put(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= A540_FB_WIDTH || y >= A540_FB_HEIGHT)
        return;
    a540_fb()[y * A540_FB_WIDTH + x] = color;
}

void a540_scanout_solid(a540_dev *d, uint32_t color)
{
    uint32_t x, y;
    (void)d;  /* pure CPU path: proves the panel/scanout window, no GPU needed */
    for (y = 0; y < A540_FB_HEIGHT; y++)
        for (x = 0; x < A540_FB_WIDTH; x++)
            a540_fb()[y * A540_FB_WIDTH + x] = color;
}
