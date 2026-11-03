/* SAGIT_DEBUG breadcrumbs: paint coloured bars on the ABL splash framebuffer
 * so a photo of the screen tells us how far the shim got. */
#ifndef SHIM_DBG_H
#define SHIM_DBG_H

#define DBG_FB_BASE   0x9D400000UL
#define DBG_FB_STRIDE 4320

#ifdef __ASSEMBLER__
/* asm breadcrumb: 8 rows of 1080 px at y = 1600 + idx*12 */
.macro DBG_C idx, color
	movz	x9, #((\color) & 0xffff)
	movk	x9, #(((\color) >> 16) & 0xffff), lsl #16
	movz	x10, #0x9D40, lsl #16
	movz	x11, #((((1600 + (\idx) * 12) * 4320)) & 0xffff)
	movk	x11, #(((((1600 + (\idx) * 12) * 4320)) >> 16) & 0xffff), lsl #16
	add	x10, x10, x11
	mov	x12, #8
9001:
	mov	x13, #1080
9002:
	str	w9, [x10], #4
	subs	x13, x13, #1
	b.ne	9002b
	subs	x12, x12, #1
	b.ne	9001b
.endm
#else
#include <stdint.h>
static inline void dbg_crumb(int idx, uint32_t color)
{
	volatile uint32_t *fb = (volatile uint32_t *)DBG_FB_BASE;
	for (int y = 0; y < 8; y++) {
		volatile uint32_t *line = fb + (1600 + idx * 12 + y) * (DBG_FB_STRIDE / 4);
		for (int x = 0; x < 1080; x++)
			line[x] = color;
	}
}
#endif

#endif
