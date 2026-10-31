import re, io, os
XML = r"E:\MI6-iOS\refs\mesa\a5xx.xml"
GPU = r"E:\MI6-iOS\work\ios-sagit\platforms\sagit\gpu"
os.makedirs(GPU, exist_ok=True)
xml = io.open(XML, encoding="utf-8").read()
regs = {}
for off, name in re.findall(r'<reg32\s+offset="(0x[0-9a-fA-F]+)"\s+name="([A-Z0-9_]+)"', xml):
    regs.setdefault(name, int(off, 16))
L = []
L.append("/* SPDX-License-Identifier: MIT")
L.append(" * GENERATED - do not edit.  Source: mesa/mesa main,")
L.append(" * src/freedreno/registers/adreno/a5xx.xml (local copy E:\\MI6-iOS\\refs\\mesa\\a5xx.xml).")
L.append(" * Adreno 530/540 = A5XX.  Offsets are dword offsets into the GPU MMIO window.")
L.append(" * Regenerate with: python tools/gen-a540-regs-full.py <a5xx.xml> <out.h>")
L.append(" */")
L.append("#ifndef A540_REGS_H")
L.append("#define A540_REGS_H")
L.append("")
for name in sorted(regs):
    L.append("#define A540_%-42s 0x%05xu" % (name, regs[name]))
L.append("")
L.append("#define A540_REG_COUNT %d" % len(regs))
L.append("")
L.append("/* sagit simple-framebuffer window (edk2 MSM8998 memory map:")
L.append(" * Display Reserved 0x9D400000 + 0x02400000). */")
L.append("#define A540_FB_BASE   0x9d400000u")
L.append("#define A540_FB_SIZE   0x02400000u")
L.append("#define A540_FB_WIDTH  1080u")
L.append("#define A540_FB_HEIGHT 1920u")
L.append("#define A540_FB_STRIDE 4320u")
L.append("#endif")
io.open(os.path.join(GPU, "a540_regs.h"), "w", encoding="utf-8", newline="\n").write("\n".join(L) + "\n")
print("a540_regs.h:", len(regs), "regs")
needed = ["RBBM_STATUS","CP_RB_BASE","CP_RB_BASE_HI","CP_RB_CNTL","CP_RB_RPTR_ADDR","CP_RB_RPTR_ADDR_HI",
          "CP_RB_WPTR","CP_RB_RPTR","CP_PFP_INSTR_BASE_LO","CP_PFP_INSTR_BASE_HI","CP_ME_INSTR_BASE_LO",
          "CP_ME_INSTR_BASE_HI","CP_PFP_ME_CNTL","RBBM_INT_0_MASK","GPMU_DATA_RAM_BASE","GPMU_INST_RAM_BASE",
          "GPMU_CM3_SYSRESET","GPMU_GENERAL_0","GPMU_GENERAL_1","GPMU_GPMU_VOLTAGE","GPMU_GPMU_PWR_THRESHOLD",
          "GPMU_WFI_CONFIG","GPMU_PWR_COL_INTER_FRAME_CTRL","GPMU_PWR_COL_BINNING_CTRL",
          "GPMU_PWR_COL_INTER_FRAME_HYST","GPMU_PWR_COL_STAGGER_DELAY","GDPM_INT_MASK","GDPM_INT_EN",
          "GPMU_GPMU_VOLTAGE_INTR_EN_MASK","GPMU_TEMP_THRESHOLD_INTR_EN_MASK",
          "GPMU_THROTTLE_UNMASK_FORCE_CTRL","GPMU_CLOCK_THROTTLE_CTRL","UCHE_MODE_CNTL","UCHE_CACHE_WAYS"]
miss = [n for n in needed if n not in regs]
print("missing:", miss)