#!/bin/bash
# sagit: make StartIOKitMatching idempotent, drop the SOCD trace, and probe gRootNub.
# Runs before the .patch files are applied (build.sh executes patches/*.sh first).
python3 - <<'PY'
import io, os, sys

p = os.path.join(os.getcwd(), "iokit", "Kernel", "IOStartIOKit.cpp")
if not os.path.exists(p):
    print("sagit match guard: file not found: %s" % p)
    sys.exit(0)

s = io.open(p, encoding="utf-8", newline="").read()
if "SAGIT: matching first entry" in s:
    print("sagit match guard: already applied")
    sys.exit(0)

nl = "\r\n" if "\r\n" in s else "\n"
old = nl.join([
    "StartIOKitMatching(void)",
    "{",
    "\tSOCD_TRACE_XNU(START_IOKIT);",
    "\tassert(gRootNub != NULL);",
    "",
])
new = nl.join([
    "StartIOKitMatching(void)",
    "{",
    "\tstatic int sagit_done = 0;",
    "\tif (sagit_done++) {",
    "\t\tprintf(\"SAGIT: matching re-entry skipped (sagit)\\n\");",
    "\t\treturn;",
    "\t}",
    "\tprintf(\"SAGIT: matching first entry (sagit)\\n\");",
    "\tprintf(\"SAGIT: SOCD trace skipped (sagit)\\n\");",
    "\tprintf(\"SAGIT: gRootNub=%p (sagit)\\n\", (void *)gRootNub);",
    "\tif (gRootNub == NULL) {",
    "\t\tprintf(\"SAGIT: no root nub, skipping matching (sagit)\\n\");",
    "\t\treturn;",
    "\t}",
    "",
])
if old not in s:
    print("sagit match guard: ANCHOR MISS")
    sys.exit(0)

io.open(p, "w", encoding="utf-8", newline="").write(s.replace(old, new, 1))
print("sagit match guard: applied")
PY