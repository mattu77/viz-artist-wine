#!/bin/bash
# Install / remove the opengl32 "swap copy" proxy for Viz.exe in the vizartist prefix.
#   opengl32-swapcopy.sh install   -> copy proxy + renamed builtin into the VizEngine dir, add the Viz.exe DllOverride
#   opengl32-swapcopy.sh remove    -> undo both
# Takes effect for the NEXT Viz.exe start only (a running engine keeps its loaded opengl32).
# Undo with "remove"; the registry value is per-app (Viz.exe) only.
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
export WINEPREFIX=${WINEPREFIX:-$HOME/.local/share/wineprefixes/vizartist} WINEDEBUG=-all
VIZ="$WINEPREFIX/drive_c/Program Files/Vizrt/VizEngine"
BUILTIN=/usr/lib64/wine-wow64/wine/x86_64-windows/opengl32.dll   # Fedora wine-11.0 (wow64 build)
KEY='HKCU\Software\Wine\AppDefaults\Viz.exe\DllOverrides'
case "$1" in
install)
    [ -f "$here/build/opengl32.dll" ] || { echo "build first: make -C $here"; exit 1; }
    cp "$here/build/opengl32.dll" "$VIZ/opengl32.dll"
    cp "$BUILTIN" "$VIZ/opengl32_wine.dll"      # must be refreshed after every wine package update
    wine reg add "$KEY" /v opengl32 /d native,builtin /f
    echo "installed; restart the engine (viz-wine engine). Optional: VIZ_GL_SWAPCOPY_LOG=2 viz-wine engine";;
remove)
    wine reg delete "$KEY" /v opengl32 /f || true
    rm -f "$VIZ/opengl32.dll" "$VIZ/opengl32_wine.dll"
    echo "removed";;
*) echo "usage: $0 install|remove"; exit 1;;
esac
