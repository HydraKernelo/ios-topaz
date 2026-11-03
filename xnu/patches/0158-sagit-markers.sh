#!/bin/bash
# SAGIT: inject early bsd_init progress markers before dxb applies *.patch.
# build.sh sources every patches/*.sh from the XNU source root (before git apply),
# so this reaches the source even when a hand-made patch would conflict.
f=bsd/kern/bsd_init.c
if [ ! -f "$f" ]; then
    echo "SAGIT: bsd_init.c not found from $(pwd)" >&2
    exit 0
fi
python3 - "$f" <<'PY'
import sys
p = sys.argv[1]
data = open(p, "rb").read()
nl = b"\r\n" if b"\r\n" in data else b"\n"
marks = [
    (b"\tdev_kmem_init();",     b"SAGIT: stepA dev_kmem (sagit)"),
    (b"\tkauth_init();",        b"SAGIT: stepB kauth (sagit)"),
    (b"\tmcache_init();",       b"SAGIT: stepC mcache (sagit)"),
    (b"\tbsd_autoconf();",      b"SAGIT: stepD autoconf (sagit)"),
    (b"\tmemorystatus_init();", b"SAGIT: stepE memorystatus (sagit)"),
    (b"\tinittodr(0);",         b"SAGIT: stepF inittodr (sagit)"),
    (b"\tkminit();",            b"SAGIT: d1 kminit (sagit)"),
    (b"\t\tfor (pi = pseudo_inits; pi->ps_func; pi++) {", b"SAGIT: d2 pseudo inits (sagit)"),
    (b"\treturn IOKitBSDInit();", b"SAGIT: d3 IOKitBSDInit (sagit)"),
]
changed = 0
for anchor, text in marks:
    full = anchor + nl
    if text in data:
        print("SAGIT: already present:", text.decode())
        continue
    n = data.count(full)
    if n != 1:
        print("SAGIT: anchor not unique (%d):" % n, anchor.decode())
        continue
    data = data.replace(full, b"\tprintf(\"%s\\n\");" % text + nl + full, 1)
    changed += 1
    print("SAGIT: injected:", text.decode())
if changed:
    open(p, "wb").write(data)
print("SAGIT: injected %d marker(s)" % changed)

# fine-grained bsd_init markers between kauth_init and mcache_init
_f = "bsd/kern/bsd_init.c"
_d = open(_f, "rb").read()
for _anchor, _tag in [
    (b"\tbsd_bufferinit();", b"e1 bufferinit"),
    (b"\tIOKitInitializeTime();", b"e2 IOKitInitializeTime"),
    (b"\tproc_uuid_policy_init();", b"e6 uuid_policy"),
]:
    _line = b'\tprintf("SAGIT: ' + _tag + b' (sagit)\\n");'
    if _line in _d:
        continue
    if _d.count(_anchor) != 1:
        print("SAGIT: fine anchor not unique:", _tag.decode())
        continue
    _d = _d.replace(_anchor, _line + b"\n" + _anchor, 1)
    print("SAGIT: fine injected:", _tag.decode())
open(_f, "wb").write(_d)


# sagitjump: allow skipping vfsinit/amfi/hfs block to test the hang point
_f = "bsd/kern/bsd_init.c"
_d = open(_f, "rb").read()
if b"sagit_after_hfs" not in _d:
    _a = b"\tIOKitInitializeTime();"
    _g = (b'\t{ int _sj = 0; if (PE_parse_boot_argn("sagitjump", &_sj, sizeof(_sj)) && _sj) goto sagit_after_hfs; }\n')
    _d = _d.replace(_a, _a + b"\n" + _g, 1)
    _b = b"\tproc_uuid_policy_init();"
    _l = b"sagit_after_hfs: ;\n"
    _d = _d.replace(_b, _l + _b, 1)
    open(_f, "wb").write(_d)
    print("SAGIT: sagitjump guard injected")


# fine markers around proc_uuid_policy_init / mcache_init
_f = "bsd/kern/bsd_init.c"
_d = open(_f, "rb").read()
def _ins(anchor, text):
    global _d
    line = b'\tprintf("SAGIT: ' + text + b' (sagit)\\n");'
    if line in _d:
        return
    if _d.count(anchor) != 1:
        print("SAGIT: skip anchor", text.decode(), "count", _d.count(anchor))
        return
    _d = _d.replace(anchor, anchor + b"\n" + line, 1)
_ins(b"\tproc_uuid_policy_init();", b"e7 after uuid")
_ins(b"\tmcache_init();", b"e9 after mcache")
_b = b"\tmcache_init();"
if b"e8 before mcache" not in _d:
    _l = b'\tprintf("SAGIT: e8 before mcache (sagit)\\n");\n'
    _d = _d.replace(_b, _l + _b, 1)
open(_f, "wb").write(_d)
print("SAGIT: e7/e8/e9 injected")


# skip switches: sagitskipuuid / sagitskipmcache
_f = "bsd/kern/bsd_init.c"
_d = open(_f, "rb").read()
if b"sagit_after_uuid" not in _d:
    _u = b"\tproc_uuid_policy_init();"
    _g = (b'\t{ int _s1 = 0; if (PE_parse_boot_argn("sagitskipuuid", &_s1, sizeof(_s1)) && _s1) goto sagit_after_uuid; }\n')
    _d = _d.replace(_u, _g + _u, 1)
    _d = _d.replace(_u, _u + b"\nsagit_after_uuid: ;\n", 1)
    _m = b"\tmcache_init();"
    _g2 = (b'\t{ int _s2 = 0; if (PE_parse_boot_argn("sagitskipmcache", &_s2, sizeof(_s2)) && _s2) goto sagit_after_mcache; }\n')
    _d = _d.replace(_m, _g2 + _m, 1)
    _d = _d.replace(_m, _m + b"\nsagit_after_mcache: ;\n", 1)
    open(_f, "wb").write(_d)
    print("SAGIT: skip switches injected")


# mcache_init waits for every CPU in the ADT to become available; on sagit only
# the boot CPU runs, so replace the wait with the live count.
_f = "bsd/kern/mcache.c"
_d = open(_f, "rb").read()
_nl = b"\r\n" if b"\r\n" in _d else b"\n"
_old = b"\tncpu = ml_wait_max_cpus();" + _nl
if _old in _d and b"mcache using" not in _d:
    _new = (b'\tncpu = ml_get_cpu_count();' + _nl +
            b'\tprintf("SAGIT: mcache using %d cpus without waiting (sagit)\\n", ncpu);' + _nl)
    _d = _d.replace(_old, _new, 1)
    open(_f, "wb").write(_d)
    print("SAGIT: mcache wait patched")

PY

# sagitjumptime: wrap IOKitInitializeTime so it can be skipped on msm8998.
# Runs in the XNU source root (build.sh cds there before sourcing patches/*.sh).
python3 - <<'PYJT'
import sys
p = 'bsd/kern/bsd_init.c'
data = open(p, 'rb').read()
nl = b'\r\n' if b'\r\n' in data else b'\n'
old = b'\tIOKitInitializeTime();' + nl
if b'sagitjumptime' in data:
    print('SAGIT: IOKitInitializeTime already wrapped')
elif old in data:
    new = (b'\t{' + nl +
           b'\t\tint _sjt = 0;' + nl +
           b'\t\t(void)PE_parse_boot_argn("sagitjumptime", &_sjt, sizeof(_sjt));' + nl +
           b'\t\tif (!_sjt) {' + nl +
           b'\t\t\tprintf("SAGIT: IOKitInitializeTime begin (sagit)\\n");' + nl +
           b'\t\t\tIOKitInitializeTime();' + nl +
           b'\t\t\tprintf("SAGIT: IOKitInitializeTime done (sagit)\\n");' + nl +
           b'\t\t} else {' + nl +
           b'\t\t\tprintf("SAGIT: IOKitInitializeTime skipped (sagit)\\n");' + nl +
           b'\t\t}' + nl +
           b'\t}' + nl)
    data = data.replace(old, new, 1)
    open(p, 'wb').write(data)
    print('SAGIT: wrapped IOKitInitializeTime')
else:
    print('SAGIT: IOKitInitializeTime anchor missing')
PYJT

# sagitnortc: guard the IORTC wait + clock_initialize_calendar on msm8998 (no RTC service).
python3 - <<'PYRTC'
import io
p = 'iokit/Kernel/IOStartIOKit.cpp'
data = open(p, 'rb').read()
nl = b'\r\n' if b'\r\n' in data else b'\n'
if b'sagit RTC wait' in data:
    print('SAGIT: RTC guard already applied')
else:
    lines = data.split(b'\n')
    out = []
    for line in lines:
        if b'IORTC' in line:
            j = len(out) - 1
            while j >= 0 and not out[j].lstrip().startswith(b'IOService::waitForService('):
                j -= 1
            if j >= 0:
                out.insert(j, b'\tprintf("SAGIT: RTC wait begin (sagit)\\n");')
            out.append(line)
            out.append(b'\tprintf("SAGIT: RTC wait done (sagit)\\n");')
            continue
        out.append(line)
    data = b'\n'.join(out)
    old = b'\tclock_initialize_calendar();' + nl
    new = (b'\t{ int _nortc = 0; (void)PE_parse_boot_argn("sagitnortc", &_nortc, sizeof(_nortc));\n'
           b'\t  if (_nortc) { printf("SAGIT: clock_initialize_calendar skipped (sagit)\\n"); }\n'
           b'\t  else { printf("SAGIT: clock_initialize_calendar begin (sagit)\\n");\n'
           b'\t         clock_initialize_calendar();\n'
           b'\t         printf("SAGIT: clock_initialize_calendar done (sagit)\\n"); } }\n')
    if old in data:
        data = data.replace(old, new, 1)
        print('SAGIT: RTC wait + calendar guarded')
    else:
        print('SAGIT: clock_initialize_calendar anchor missing')
    open(p, 'wb').write(data)
PYRTC

python3 - <<'PYIOBD'
import io
p = 'iokit/bsddev/IOKitBSDInit.cpp'
data = open(p, 'rb').read()
nl = b'\r\n' if b'\r\n' in data else b'\n'
if b'SAGIT: IOKitBSDInit enter' in data:
    print('SAGIT: IOKitBSDInit block already applied')
else:
    old = b'\tIOService::publishResource("IOBSD");'
    new = (b'\tprintf("SAGIT: IOKitBSDInit enter (sagit)\\n");' + nl +
           b'\tIOService::publishResource("IOBSD");' + nl +
           b'\tprintf("SAGIT: IOBSD resource published (sagit)\\n");')
    if old in data:
        data = data.replace(old, new, 1)
        open(p, 'wb').write(data)
        print('SAGIT: IOKitBSDInit markers injected')
    else:
        print('SAGIT: IOKitBSDInit anchor missing')
PYIOBD

python3 - <<'PYDBI'
import io
p = 'bsd/kern/bsd_init.c'
data = open(p, 'rb').read()
if b'#define DEBUG_BSDINIT 1' in data:
    print('SAGIT: DEBUG_BSDINIT already 1')
else:
    old = b'#define DEBUG_BSDINIT 0'
    new = b'#define DEBUG_BSDINIT 1'
    if old in data:
        data = data.replace(old, new, 1)
        open(p, 'wb').write(data)
        print('SAGIT: DEBUG_BSDINIT enabled')
    else:
        print('SAGIT: DEBUG_BSDINIT anchor missing')
PYDBI

python3 - <<'PYPANIC'
import io

p = 'osfmk/kern/debug.c'
data = open(p, 'rb').read()
nl = b'\r\n' if b'\r\n' in data else b'\n'
if b'SAGIT PANIC' in data:
    print('SAGIT: panic hook already applied')
else:
    anchors = [b'panic(const char *str, ...)', b'panic(const char *str, __unused ...']
    hit = None
    for a in anchors:
        if a in data:
            hit = a
            break
    if hit is None:
        print('SAGIT: panic anchor missing')
    else:
        idx = data.find(hit)
        brace = data.find(b'{', idx)
        if brace == -1:
            print('SAGIT: panic body brace missing')
        else:
            ins = b'\n\tprintf("SAGIT PANIC: %s\\n", str);'
            data = data[:brace + 1] + ins + data[brace + 1:]
            open(p, 'wb').write(data)
            print('SAGIT: panic visible hook injected')
PYPANIC

exit 0
