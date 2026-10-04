#!/usr/bin/env python3
"""Apply bundled 3.1.2 customizations to an explicitly supplied core directory.
Back up originals once; refuse a core with any other version. Does not download,
flash, or discover/modify a global Arduino installation automatically.
"""
import argparse
import shutil
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('core_directory', type=Path)
a = p.parse_args()
core = a.core_directory.resolve()
platform = core / 'platform.txt'
if not platform.is_file() or 'version=3.1.2' not in platform.read_text():
    p.error('Expected an installed ESP8266 core 3.1.2 directory containing platform.txt')
src = Path(__file__).resolve().parents[1] / 'vendor/core-customizations/3.1.2'
for f in sorted(src.rglob('*')):
    if not f.is_file() or f.name in ('LICENSE', 'README.MD'): continue
    target = core / f.relative_to(src)
    if target.exists():
        backup = target.with_name(target.name + '.stargate-original')
        if not backup.exists(): shutil.copy2(target, backup)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(f, target)
print('Applied bundled 3.1.2 files. Existing files backed up as *.stargate-original.')
