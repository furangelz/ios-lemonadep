#!/usr/bin/env python3
"""Move the 0081 kernel's early framebuffer console to the OnePlus 9 Pro panel.

  lemonadep-kpatch.py KERNEL_IN KERNEL_OUT

Stopgap until a source patch (early console from boot_args->Video) is built by
CI. Patches 0010/0012 hard-code the sweet splash buffer 0x9c000000 in start.s,
arm_vm_init.c and sweet_early.c; on lemonadep the panel scans out
0xe4d00000 (1080x2412, stride 4320, same width/stride as sweet), and
0x9c000000 is an unknown SM8350 carve-out. Only the base changes:

  start.s stage-0 band      0x9c000000 + 2368*4320 -> 0xe4d00000 + 2380*4320
  start.s V=P 32MB block    0x9c000000 -> 0xe4000000 (block-aligned)
  arm_vm_init final alias   0x9c000000 -> 0xe4000000 (block-aligned)
  early_relocate(VA)        +0x2000000 -> +0x2d00000 (alias base + fb offset)
  early_fb initial value    0x9c000000 -> 0xe4d00000 (V=P address)

Refuses any kernel other than the 0081 CI build (run 39158169212) and checks
every original word before writing.
"""
import hashlib, struct, sys

KERNEL_0081_SHA256 = 'd9cb893bd2baf841e94cd2001f0ec70c0fea42f045f87f14ae807e4ebc50e1e5'
TEXT_EXEC_VA, TEXT_EXEC_OFF = 0xfffffe00072e4000, 0x2e0000
DATA_VA, DATA_OFF = 0xfffffe0007b44000, 0xb40000

# (va, old u32, new u32, what)
CODE = [
    (0xfffffe0007483c9c, 0xf2b38008, 0xf2bc8008, 'arm_vm_init: movk x8, #0xe400, lsl #16'),
    (0xfffffe0007483dd8, 0xf2a04000, 0xf2a05a00, 'arm_vm_init: movk x0, #0x2d0, lsl #16'),
    (0xfffffe000748d104, 0xf2b39380, 0xf2bcad20, 'start_first_cpu: movk x0, #0xe569, lsl #16'),
    (0xfffffe000748d108, 0xf2830000, 0xf29c5000, 'start_first_cpu: movk x0, #0xe280'),
    (0xfffffe000748d340, 0xf2b3800a, 0xf2bc800a, 'start_first_cpu: movk x10, #0xe400, lsl #16'),
]
# (va, old u64, new u64, what)
DATA = [
    (0xfffffe0007b616d8, 0x9c000000, 0xe4d00000, 'early_fb'),
]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    buf = bytearray(open(sys.argv[1], 'rb').read())
    digest = hashlib.sha256(buf).hexdigest()
    if digest != KERNEL_0081_SHA256:
        sys.exit(f'{sys.argv[1]}: sha256 {digest} is not the 0081 kernel')
    for va, old, new, what in CODE:
        off = va - TEXT_EXEC_VA + TEXT_EXEC_OFF
        cur, = struct.unpack_from('<I', buf, off)
        if cur != old:
            sys.exit(f'{hex(va)}: found {cur:#010x}, expected {old:#010x}')
        struct.pack_into('<I', buf, off, new)
        print(f'{va:#x}: {old:#010x} -> {new:#010x}  {what}')
    for va, old, new, what in DATA:
        off = va - DATA_VA + DATA_OFF
        cur, = struct.unpack_from('<Q', buf, off)
        if cur != old:
            sys.exit(f'{hex(va)}: found {cur:#x}, expected {old:#x}')
        struct.pack_into('<Q', buf, off, new)
        print(f'{va:#x}: {old:#x} -> {new:#x}  {what}')
    open(sys.argv[2], 'wb').write(buf)
    print(f'{sys.argv[2]}: sha256 {hashlib.sha256(buf).hexdigest()}')


if __name__ == '__main__':
    main()
