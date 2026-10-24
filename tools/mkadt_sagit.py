#!/usr/bin/env python3
"""Build the ADT for Xiaomi Mi 6 (sagit, MSM8998) on the ios-sweet/topaz base.

The Mi 6 runs the same shim + DarwinBoot + XNU pipeline as the Redmi Note 12 4G
(topaz), but every physical constant differs. This wrapper imports the upstream
mkadt.py, overrides the module-level hardware constants with sagit values
(mirrored in platforms/sagit/platform_sagit.h), and reuses build() unchanged.

Usage: mkadt_sagit.py OUT.adt [--dump] [--native]

Identity note: the userland is currently the iPhone X (D22AP / iPhone10,3)
IPSW that the topaz lineage boots. The Mi 6 panel is 1080x1920 while the
iPhone X renders 1125x2436, so bring-up keeps the iPhone X tree first; the
display milestone decides between (a) keeping iPhone X identity with scaling/
clipping or (b) switching to iPhone 8 Plus (D21AP / iPhone10,2), whose
1080x1920 panel matches the Mi 6 exactly.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkadt  # noqa: E402

# ---- sagit hardware constants (mirror of platforms/sagit/platform_sagit.h)
mkadt.GICD, mkadt.GICD_SIZE = 0x17A00000, 0x10000     # APCS_GIC500_GICD
mkadt.GICR, mkadt.GICR_SIZE = 0x17B00000, 0x100000    # APCS_GIC500_GICR
mkadt.UFS, mkadt.UFS_SIZE = 0x01DA0000, 0x20000       # "UFS UFS REGS"
mkadt.FB, mkadt.FB_SIZE = 0x9D400000, 0x02400000      # ABL simple-framebuffer
mkadt.FB_W, mkadt.FB_H, mkadt.FB_STRIDE = 1080, 1920, 4320  # Mi 6 panel
mkadt.TIMEBASE = 19200000                             # 19.2 MHz
mkadt.SWEET_ECID = 0x5341474954000001                 # 'SAGIT' + 1, synthetic
# 6 GB phone: use the clean 1.5 GB Mem6G bank at 0xA0000000. The second bank
# (0x100000000 + 0xBD8C0000) is left for later memory work.
mkadt.PHYS_BASE, mkadt.MEM_SIZE = 0xA0000000, 0x60000000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--dump', action='store_true')
    ap.add_argument('--native', action='store_true')
    args = ap.parse_args()
    root = mkadt.build(native=args.native)

    # interrupt-controller alias: XNU's pe_arm_map_interrupt_controller() looks up a
    # node literally named "interrupt-controller"; mkadt names the GIC node "gic".
    # Publish a copy under the expected name so both lookups succeed.
    def _find(node, name):
        if node.name == name:
            return node
        for c in node.children:
            r = _find(c, name)
            if r is not None:
                return r
        return None

    io_node = _find(root, "arm-io")
    gic = _find(root, "gic")
    if io_node is not None and gic is not None:
        alias = mkadt.adt.Node()
        alias.props = [(("name", 0, b"interrupt-controller") if n == "name" else (n, f, v))
                       for (n, f, v) in gic.props]
        maxph = 0
        def scan(n):
            nonlocal maxph
            for (pn, pf, pv) in n.props:
                if pn == "AAPL,phandle" and len(pv) >= 4:
                    maxph = max(maxph, int.from_bytes(pv[:4], "big"))
            for c in n.children:
                scan(c)
        scan(root)
        alias.props = [pr for pr in alias.props if pr[0] != "AAPL,phandle"]
        alias.props.append(("AAPL,phandle", 0, (maxph + 1).to_bytes(4, "big")))
        io_node.children.append(alias)
        print("added interrupt-controller alias", file=sys.stderr)

    blob = mkadt.adt.serialize(root)
    with open(args.out, 'wb') as f:
        f.write(blob)
    if args.dump:
        mkadt.adt.dump(mkadt.adt.parse(blob)[0])
    print(f'{args.out}: {len(blob)} bytes (sagit)', file=sys.stderr)


if __name__ == '__main__':
    main()
