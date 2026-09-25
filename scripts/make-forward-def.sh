#!/bin/bash
# Generate a .def file that forwards every export of a DLL to another DLL, except the names given
# as extra arguments (those are left plain so the proxy's own implementation is exported instead).
#   make-forward-def.sh <dll-to-copy-exports-from> <LIBRARY name> <forward-to name> [override ...]
# Example (the iphlpapi proxy):
#   make-forward-def.sh /usr/lib64/wine-wow64/wine/x86_64-windows/iphlpapi.dll iphlpapi.dll iphlpapi_wine GetAdaptersAddresses > iphlpapi.def
set -e
src=$1; lib=$2; fwd=$3; shift 3
echo "LIBRARY $lib"; echo EXPORTS
winedump -j export "$src" | awk '/^ *[0-9]+ +[0-9A-Fa-f]+ +[A-Za-z_]/ {print $3}' | sort -u | while read -r name; do
  skip=0; for o in "$@"; do [ "$name" = "$o" ] && skip=1; done
  if [ $skip = 1 ]; then echo "  $name"; else echo "  $name = $fwd.$name"; fi
done
