#!/usr/bin/env python3
"""Rename an imported DLL inside a PE file (same-length names only, e.g. ADVAPI32.dll -> ADVAPI3Z.dll).

The import directory stores DLL names as plain NUL-terminated ASCII, so a byte-for-byte replacement of a
same-length name is a valid PE edit and needs no relocation. A .orig copy of the file is kept.

usage: patch-import.py <file.exe|dll> <OLD.dll> <NEW.dll>
"""
import shutil, sys

if len(sys.argv) != 4:
    sys.exit(__doc__)
path, old, new = sys.argv[1], sys.argv[2].encode(), sys.argv[3].encode()
if len(old) != len(new):
    sys.exit("old and new names must have the same length")
data = open(path, "rb").read()
n = data.count(old + b"\0")
if n == 0:
    sys.exit(f"{old.decode()} not found in {path}")
shutil.copy2(path, path + ".orig")
open(path, "wb").write(data.replace(old + b"\0", new + b"\0"))
print(f"{path}: replaced {n} occurrence(s) of {old.decode()} with {new.decode()} (backup: {path}.orig)")
