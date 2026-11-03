/* SPDX-License-Identifier: MIT
 * Host-side test for the A540 (A5XX) bring-up skeleton.
 *
 * It runs a540_gpu.c against a fake dword MMIO window and asserts the exact
 * sequences/values that were ported from the Linux msm driver, so a regression
 * in the order of power/GPMU/CP programming shows up in CI instead of on the
 * phone.  It does NOT talk to hardware: the "GPU" here is an array.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define A540_HOST_TEST 1
#include "a540_gpu.h"

#define MAXLOG 64
typedef struct { uint32_t off, val; } wr_entry;

static wr_entry g_log[MAXLOG];
static int g_nlog;
static uint32_t g_regs[0x10000];
static uint32_t g_fb[A540_FB_WIDTH * A540_FB_HEIGHT];

uint32_t a540_host_fb[A540_FB_WIDTH * A540_FB_HEIGHT];

static void fake_wr(a540_dev *d, uint32_t off, uint32_t val)
{
    (void)d;
    if (g_nlog < MAXLOG) {
        g_log[g_nlog].off = off;
        g_log[g_nlog].val = val;
        g_nlog++;
    }
    if (off < 0x10000u)
        g_regs[off] = val;
}

static uint32_t fake_rd(a540_dev *d, uint32_t off)
{
    (void)d;
    return (off < 0x10000u) ? g_regs[off] : 0u;
}

static void reset(void)
{
    g_nlog = 0;
    memset(g_regs, 0, sizeof(g_regs));
    memset(g_fb, 0, sizeof(g_fb));
}

static a540_dev mkdev(void)
{
    a540_dev d;
    d.mmio_write = fake_wr;
    d.mmio_read  = fake_rd;
    d.ctx        = NULL;
    d.patchid    = 0u;
    d.freq_mhz   = 670u;
    d.mvolts     = 900u;
    d.writes     = 0u;
    d.reads      = 0u;
    return d;
}

static int g_fail;

static void check(int cond, const char *what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_fail++;
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    a540_dev d;
    uint32_t ring_mem[16];
    a540_ring ring;

    /* --- register map sanity (generated from mesa a5xx.xml) --- */
    check(A540_REG_COUNT > 900, "register map generated");
    check(A540_RBBM_STATUS == 0x4f5u, "RBBM_STATUS offset");
    check(A540_CP_RB_CNTL == 0x802u, "CP_RB_CNTL offset");
    check(A540_GPMU_DATA_RAM_BASE == 0x9800u, "GPMU_DATA_RAM_BASE offset");

    /* --- a540_lm_setup_a540: order and values (a5xx_power.c:175) --- */
    reset();
    d = mkdev();
    a540_lm_setup_a540(&d);
    check(g_nlog == 12, "lm_setup wrote 12 registers");
    check(g_log[0].off == A540_GPMU_GPMU_VOLTAGE && g_log[0].val == 0x80000000u,
          "lm_setup[0] GPMU_VOLTAGE");
    check(g_log[1].off == A540_GPMU_GPMU_PWR_THRESHOLD &&
          g_log[1].val == (0x80000000u | 6000u), "lm_setup[1] PWR_THRESHOLD 6000");
    check(g_log[2].val == 0x80000001u, "lm_setup[2] AGC_MSG_STATE");
    check(g_log[3].val == 1u, "lm_setup[3] AGC POWER_CONFIG_PRODUCTION_ID");
    check(g_log[4].val == 5448u, "lm_setup[4] max power 5448");
    check(g_log[5].val == 1u, "lm_setup[5] payload[1]");
    check(g_log[6].val == 900u && g_log[7].val == 670u,
          "lm_setup[6..7] voltage/frequency payload");
    check(g_log[8].val == ((1u << 24) | (2u << 8)),
          "lm_setup[8] LM config (BCL off, throttle off, patchid 0)");
    check(g_log[9].val == (~0x303u), "lm_setup[9] level config");
    check(g_log[10].val == (4u * 36u), "lm_setup[10] payload size = 144");
    check(g_log[11].off == (A540_GPMU_DATA_RAM_BASE + 5u) &&
          g_log[11].val == 0xBABEFACEu, "lm_setup[11] AGC magic");


    reset();
    d = mkdev();
    d.patchid = 7u;
    a540_lm_setup_a540(&d);
    check(g_log[8].val == ((1u << 24) | (2u << 8) | (7u << 17)),
          "lm_setup encodes patchid in LM config");

    /* --- a540_pc_init: four exact writes (a5xx_power.c:211) --- */
    reset();
    d = mkdev();
    a540_pc_init(&d);
    check(g_nlog == 4, "pc_init wrote 4 registers");
    check(g_log[0].off == A540_GPMU_PWR_COL_INTER_FRAME_CTRL && g_log[0].val == 0x7Fu,
          "pc_init[0] inter-frame ctrl");
    check(g_log[1].off == A540_GPMU_PWR_COL_BINNING_CTRL && g_log[1].val == 0u,
          "pc_init[1] binning ctrl");
    check(g_log[2].off == A540_GPMU_PWR_COL_INTER_FRAME_HYST && g_log[2].val == 0xA0080u,
          "pc_init[2] inter-frame hysteresis");
    check(g_log[3].off == A540_GPMU_PWR_COL_STAGGER_DELAY && g_log[3].val == 0x600040u,
          "pc_init[3] stagger delay");

    /* --- a540_program_ucode_bases: 64-bit split + ME_HALT cleared --- */
    reset();
    d = mkdev();
    a540_program_ucode_bases(&d, 0x123456789ab000ull, 0xdeadbeef0000ull,
                             0x100000000ull, 0x00000008u, 0x0000FFFFu);
    check(g_nlog == 9, "ucode_bases wrote 9 registers");
    check(g_log[0].off == A540_CP_ME_INSTR_BASE_LO && g_log[0].val == 0x789ab000u,
          "pm4 base low");
    check(g_log[1].off == A540_CP_ME_INSTR_BASE_HI && g_log[1].val == (uint32_t)(0x123456789ab000ull >> 32),
          "pm4 base high");
    check(g_log[2].val == 0xbeef0000u && g_log[3].val == 0xdeadu, "pfp base split");
    check(g_log[4].val == 0u && g_log[5].val == 1u, "rb base split");
    check(g_log[6].off == A540_CP_RB_CNTL && g_log[6].val == 8u, "rb cntl");
    check(g_log[7].off == A540_RBBM_INT_0_MASK && g_log[7].val == 0xFFFFu, "int mask");
    check(g_log[8].off == A540_CP_PFP_ME_CNTL && g_log[8].val == 0u, "ME_HALT cleared");

    /* --- a540_me_init: CP_ME_INIT packet (a5xx_gpu.c:477) --- */
    memset(ring_mem, 0, sizeof(ring_mem));
    ring.ptr = ring_mem;
    ring.words = 0;
    ring.cap = 16;
    check(a540_me_init(&ring) == A540_OK, "me_init returns OK");
    check(ring.words == 9u, "me_init wrote header + 8 dwords");
    check(ring_mem[0] == ((0x48u << 16) | 8u), "CP_ME_INIT type7 header");
    check(ring_mem[1] == 0x2Fu, "ME init flags");
    check(ring_mem[2] == 3u, "multiple hardware contexts");
    check(ring_mem[3] == 0x20000000u, "error detection");
    check(ring_mem[6] == 0u, "A540: no token/sync workaround");

    /* ring capacity guard */
    ring.words = 16;
    check(a540_ring_word(&ring, 1u) == A540_ERR_RING_FULL, "ring full detected");

    /* --- a540_gpmu_kick: handshake against the AGC magic --- */
    reset();
    d = mkdev();
    g_regs[A540_GPMU_GENERAL_0] = 0xBABEFACEu;
    g_regs[A540_GPMU_GENERAL_1] = 0u;
    check(a540_gpmu_kick(&d, 4u) == A540_OK, "gpmu handshake OK");
    check(g_nlog == 1 && g_log[0].off == A540_GPMU_CM3_SYSRESET && g_log[0].val == 0u,
          "gpmu releases CM3 via SYSRESET=0");

    reset();
    d = mkdev();
    g_regs[A540_GPMU_GENERAL_0] = 0u;   /* firmware never answers */
    check(a540_gpmu_kick(&d, 3u) == A540_ERR_TIMEOUT, "gpmu timeout detected");

    reset();
    d = mkdev();
    g_regs[A540_GPMU_GENERAL_0] = 0xBABEFACEu;
    g_regs[A540_GPMU_GENERAL_1] = 5u;   /* firmware reported an error */
    check(a540_gpmu_kick(&d, 3u) == A540_ERR_GPMU, "gpmu error code propagated");

    /* --- wait helper --- */
    reset();
    d = mkdev();
    g_regs[A540_RBBM_STATUS] = 1u;
    check(a540_rbbm_idle(&d, 2u) == A540_ERR_TIMEOUT, "rbbm busy -> timeout");
    g_regs[A540_RBBM_STATUS] = 0u;
    check(a540_rbbm_idle(&d, 2u) == A540_OK, "rbbm idle detected");

    /* --- CPU scanout path (no GPU needed) --- */
    a540_fb_put(1u, 2u, 0xFF00FF00u);
    check(a540_host_fb[2u * A540_FB_WIDTH + 1u] == 0xFF00FF00u, "fb_put pixel");
    a540_fb_put(A540_FB_WIDTH, 0u, 0xFFFFFFFFu);   /* out of range: ignored */
    d = mkdev();
    a540_scanout_solid(&d, 0xFF102030u);
    check(a540_host_fb[0] == 0xFF102030u, "scanout_solid first pixel");
    check(a540_host_fb[A540_FB_WIDTH * A540_FB_HEIGHT - 1u] == 0xFF102030u,
          "scanout_solid last pixel");

    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    printf("a540 host test: all checks passed (register map, lm_setup, pc_init, "
           "ucode bases, me_init, gpmu handshake, scanout)\n");
    return 0;
}
