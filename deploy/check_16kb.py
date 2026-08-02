#!/usr/bin/env python3
"""
Проверка 16КБ-выравнивания нативных .so в APK/AAB (требование Google Play для targetSdk 35).
Каждый LOAD-сегмент каждой .so должен иметь p_align >= 16384 (0x4000). Иначе AAB отклонят.

Использование: python3 check_16kb.py <path-to.apk|.aab>
Код выхода: 0 — все выровнены; 1 — есть 4КБ (0x1000) или иные < 16КБ.
"""
import struct
import sys
import zipfile


def max_load_align(data: bytes):
    """Максимальный p_align среди PT_LOAD-сегментов ELF64. None если не ELF64."""
    if data[:4] != b"\x7fELF" or data[4] != 2:  # только ELF64
        return None
    end = "<" if data[5] == 1 else ">"
    e_phoff = struct.unpack_from(end + "Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from(end + "H", data, 0x36)[0]
    e_phnum = struct.unpack_from(end + "H", data, 0x38)[0]
    maxa = 0
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        if struct.unpack_from(end + "I", data, off)[0] == 1:  # PT_LOAD
            maxa = max(maxa, struct.unpack_from(end + "Q", data, off + 0x30)[0])
    return maxa


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: check_16kb.py <apk|aab>")
        return 2
    path = sys.argv[1]
    bad = []
    checked = 0
    with zipfile.ZipFile(path) as z:
        for name in z.namelist():
            # APK: lib/<abi>/*.so ; AAB: base/lib/<abi>/*.so
            if name.endswith(".so") and "/lib/" in ("/" + name):
                align = max_load_align(z.read(name))
                if align is None:
                    continue
                checked += 1
                if align < 16384:
                    bad.append((name, hex(align)))
    print(f"Проверено .so: {checked} в {path}")
    if bad:
        print(f"✗ НЕ 16КБ-выровнены ({len(bad)}):")
        for name, align in bad:
            print(f"   align={align}  {name}")
        return 1
    print("✓ все .so 16КБ-выровнены — Google Play OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
