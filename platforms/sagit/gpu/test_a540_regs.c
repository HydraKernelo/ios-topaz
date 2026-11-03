/* SPDX-License-Identifier: MIT */
/* Off-target sanity checks for the generated Adreno 540 register map.
 * These do NOT touch hardware: they pin the offsets that the generator must
 * keep stable so a bad regeneration fails CI instead of a kernel panic. */
#include <stdio.h>
#include "NativeAdrenoA540Regs.h"

static int failures;

#define CHECK_OFF(name, want) do { \
    if ((name) != (want)) { printf("FAIL %-34s = 0x%05x, want 0x%05x\n", #name, (unsigned)(name), (unsigned)(want)); failures++; } \
} while (0)

#define CHECK_RANGE(name) do { \
    if ((name) >= 0x40000u) { printf("FAIL %-34s = 0x%05x out of GPU register space\n", #name, (unsigned)(name)); failures++; } \
} while (0)

int main(void)
{
    CHECK_OFF(A540_RBBM_STATUS, 0x004f5u);
    CHECK_OFF(A540_RBBM_STATUS3, 0x00530u);
    CHECK_OFF(A540_RBBM_INT_0_MASK, 0x00038u);
    CHECK_OFF(A540_RBBM_INT_0_STATUS, 0x004e1u);
    CHECK_OFF(A540_RBBM_CLOCK_CNTL, 0x00067u);
    CHECK_OFF(A540_CP_RB_BASE, 0x00800u);
    CHECK_OFF(A540_CP_RB_CNTL, 0x00802u);
    CHECK_OFF(A540_CP_RB_RPTR, 0x00806u);
    CHECK_OFF(A540_CP_RB_WPTR, 0x00807u);
    CHECK_OFF(A540_CP_ME_CNTL, 0x01f6u);
    CHECK_OFF(A540_UCHE_MODE_CNTL, 0x00e81u);

    CHECK_OFF(A540_SAGIT_FB_BASE, 0x9d400000u);
    CHECK_OFF(A540_SAGIT_FB_WIDTH, 1080u);
    CHECK_OFF(A540_SAGIT_FB_HEIGHT, 1920u);
    CHECK_OFF(A540_SAGIT_FB_STRIDE, 4320u);
    /* ABL reserves 0x2400000 (36 MiB) for the splash/framebuffer LUN: that is
     * several scanout buffers, so it must be at least one visible frame. */
    if (A540_SAGIT_FB_SIZE < A540_SAGIT_FB_STRIDE * A540_SAGIT_FB_HEIGHT) {
        printf("FAIL framebuffer carve-out is smaller than one frame\n");
        failures++;
    }

    CHECK_RANGE(A540_RBBM_STATUS);
    CHECK_RANGE(A540_CP_RB_BASE);
    CHECK_RANGE(A540_UCHE_MODE_CNTL);

    printf("a540 register map: %d defines, %d failure(s)\n", (int)A540_REG_COUNT, failures);
    return failures ? 1 : 0;
}