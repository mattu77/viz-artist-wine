# Viz Artist / Viz Engine / Viz Graphic Hub under Wine on Linux

Recipe, shims and launcher that get **Vizrt Viz Artist 5.3, 5.2 and 5.1** (Viz Engine + Viz Artist GUI) and
**Viz Graphic Hub 3.1.1** running on a Linux desktop with plain Wine. Everything here was worked out by
tracing failures one at a time (WINEDEBUG channels, strace, gdb); the README lists the fixes in the order
they are needed so nobody has to re-derive them.

> ## ⚠️ This is NOT a license crack
> Nothing in this repository bypasses, patches, emulates or weakens Vizrt's licensing. Viz Artist under
> Wine talks to the **genuine CodeMeter runtime**, exactly as it does on Windows, and it will not start
> without a **valid license issued by Vizrt** in a CodeMeter container (for example the free
> "Free Viz Artist" license Vizrt hands out, or a paid one). You must obtain the software and the license
> from Vizrt yourself. This repository contains **no Vizrt files, no installers, no fonts and no
> CodeMeter files** — only the author's own small C shims, registry snippets, a shell launcher and notes.
> Using the Vizrt software is subject to Vizrt's own license terms.

Tested on Fedora 44, `wine-11.0 (Staging)` (wow64 build), KDE Plasma on X11, NVIDIA GPU with the
proprietary driver, native Linux CodeMeter 8.40. Everything is 64-bit.

---

## What you get

| Component | Status |
|---|---|
| Viz Engine 5.3 / 5.2 / 5.1 (`Viz.exe -u1 -y`) | runs, renders, licenses via native Linux CodeMeter |
| Viz Artist GUI 5.3 / 5.2 / 5.1 (`vizgui.exe`) | full workspace: scene tree, editor, plugins, asset view |
| Viz Config (`Viz.exe -u1 -y -c`) | works (needed to pick the license) |
| Viz Graphic Hub 3.1.1 server + Terminal | runs as a console process; Artist logs in; old 2.4.2 databases upgrade in place |
| Video I/O boards, NDI, CUDA plugins | not tested / stubbed (no CUDA) |

## Layout

```
bin/viz-wine              launcher (config | artist | engine | gh | gh-start | gh-stop | gh-status)
src/nvcuda-stub/          nvcuda.dll stub: "no CUDA device" (vml_clip_player hard-imports nvcuda)
src/vizasn1/              vizasn1.dll: real SpcStatementType DER decoder replacing Wine's wintrust stub
src/iphlpapi-proxy/       iphlpapi.dll proxy: marks address-less adapters Down (ACE.dll null deref)
src/svcshim/              ADVAPI3Z.dll: fake SCM dispatcher so the GH Terminal runs as a console process
src/shel32z/              SHEL32Z.dll: SHGetStockIconInfo with real icons for Qt's qwindows.dll (5.1)
src/stubsvc/              CmWebAdmin.exe stand-in service so the CodeMeter MSI's StartServices passes
src/httpapi-proxy/        httpapi.dll proxy (HttpInitialize no-op) — optional, only for the SCM path
src/okclick/, src/okqt/   press OK / Return on a dialog from inside Wine (no X fake input needed)
src/diag/                 dwtest.exe (DirectWrite lookup), adapters.exe (GetAdaptersAddresses dump)
scripts/patch-import.py   rename an imported DLL inside a PE file (same-length names)
scripts/make-forward-def.sh  generate a forwarding .def from a DLL's export table
reg/*.reg                 registry snippets referenced below
Makefile                  builds everything into build/ with x86_64-w64-mingw32-gcc
```

Build: `make` (needs `mingw64-gcc` on Fedora / `gcc-mingw-w64-x86-64` on Debian, plus `winedump` from
wine-devel for the def-generator script).

---

## 0. Prerequisites

```bash
sudo dnf install wine wine-ldap winetricks mingw64-gcc p7zip msitools   # Fedora names
```

* `wine-ldap` (wldap32) is required: Viz's libcurl imports it and Wine's DLL is in a separate package.
  Run `wineboot -u` after installing it.
* **Native Linux CodeMeter runtime** from WIBU (`CodeMeter-*.x86_64.rpm` / `.deb`), running as the
  `codemeter` systemd service. Wine's WibuCm64.dll talks to it on `localhost:22350`. Import your Vizrt
  license into it (`cmu --import`, or the CodeMeter WebAdmin on port 22352) and verify with
  `cmu --list-network`.
* A desktop running **X11**, or at least XWayland. The launcher forces Wine's X11 driver.

Create the prefix (one per Viz version; the launcher picks `~/.local/share/wineprefixes/vizartist`
by default and `vizartist51` when invoked as `viz-wine51`):

```bash
export WINEPREFIX=~/.local/share/wineprefixes/vizartist
WINEARCH=win64 wineboot -u
winetricks -q dotnet48 corefonts tahoma
wine regedit reg/engine.reg
wine regedit reg/tahoma.reg
```

`dotnet48` + `corefonts`: the Vizrt bundle installer is a WPF application and FailFasts inside
`TypefaceMap` without them. `tahoma` + `reg/tahoma.reg`: see §5.

**After winetricks, set the Windows version back to 10** (`winecfg -v win10`): the dotnet48 verb leaves the
prefix on an older version and the CodeMeter MSI then aborts in `CA_OSBelowWinVerX` ("This application is
only supported on Windows 10 or higher", msiexec exit code 67 = 1603).

## 1. Getting the MSIs out of the bundle

Some Vizrt downloads ship an `Individual Installers/` folder with the MSIs (5.2.1 does) — then skip this
step and only take `vcredist*_x64.exe` and `CodeMeterRuntime64.exe` from a bundle.

`VizArtistBundle-x64-*.exe` is a dotNetInstaller/WPF bundle. It cannot run the MSIs itself under Wine
reliably (and its `dumpTo=` verb does nothing), so extract the payload:

1. `wine VizArtistBundle-x64-5.3.0.60024.exe` and wait for the first option screen.
2. While it is open, copy the extracted payload from `$WINEPREFIX/drive_c/users/$USER/Temp/<random>/`
   (the folder that contains `VizEngine-*.msi`, `VizArtist-*.msi`, `VizPlugins-*.msi`,
   `vcredist*_x64.exe`, `CodeMeterRuntime64.exe`) to e.g. `$WINEPREFIX/drive_c/viz-setup/`.
3. Cancel the bundle.

`7z x` also unpacks the Graphic Hub bundle and the CodeMeter self-extractor
(`7z x CodeMeterRuntime64.exe` yields `CodeMeterRuntime64.msi`).

## 2. CodeMeter runtime MSI

The CodeMeter MSI (8.10) fails in `StartServices` (error 1053 / 1627) because `CmWebAdmin.exe` cannot
start as a service under Wine. Editing the MSI tables with msibuild/JScript does not persist in Wine, so
patch the **source tree** instead:

```bash
cd $WINEPREFIX/drive_c/viz-setup
wine CodeMeterRuntime64.exe /q /ExtractCab      # or: 7z x CodeMeterRuntime64.exe -ocm
# find the CmWebAdmin.exe files in the extracted tree and replace them with the stub service:
find cm -iname CmWebAdmin.exe -exec cp build/CmWebAdmin.exe {} \;
wine msiexec /i cm/CodeMeterRuntime64.msi /qn /norestart PROP_CMCC=None MSIRESTARTMANAGERCONTROL=Disable
```

**Do not start Wine's `CodeMeter.exe` service** (reg/engine.reg sets it to disabled): the native Linux
CodeMeter serves WibuCm64.dll fine, and two servers on port 22350 conflict.

## 3. Viz Engine / Artist / plugins MSIs

Wine's msi ignores `ADDDEFAULT`, which the bundle relies on, so name the features with `ADDLOCAL`.
Never pass a quoted `INSTALLDIR…=` property: Wine double-quotes it and the install lands in a folder
literally named `"C:\…"`.

```bash
cd $WINEPREFIX/drive_c/viz-setup
for v in vcredist_2015-2022_x64.exe vcredist100_x64.exe vcredist90_x64.exe vcredist80_x64.exe; do wine $v /q /norestart; done
wine msiexec /i VizEngine-5.3.0.60024.msi ADDLOCAL=ProductFeature,CM_C_VizEngine,CM_C_Plugin.SDK,CM_C_API.Tools,CM_C_Documentation REBOOT=R /qn
wine msiexec /i VizArtist-5.3.0.60241.msi REBOOT=R /qn
wine msiexec /i VizPlugins-Basic-5.3.0.60060.msi REBOOT=R /qn
wine msiexec /i VizPlugins-DataPool-5.3.0.60013.msi ADDLOCAL=ProductFeature,CM_C_VizPlugins,CM_C_Configs REBOOT=R /qn
# other plugin MSIs (Extensions, Maps, PixelFx, Socialize) install the same way if you want them
```

5.3 installs to `C:\Program Files\Vizrt\`, 5.2 and 5.1 to `C:\Program Files\vizrt\` (lower case) — the
launcher probes both. Optional plugin packs (Maps, PixelFx, Socialize, Extensions) install the same way;
`msiinfo export <msi> Feature` lists the ADDLOCAL feature names.

## 4. Engine fixes (all needed before `Viz.exe` survives startup)

Copy from `build/` into the VizEngine directory (`$VIZ` below) unless stated otherwise.

| Symptom | Cause | Fix |
|---|---|---|
| `vml_clip_player` fails to load, engine aborts | hard import of `nvcuda.dll` | `nvcuda.dll` stub → `$VIZ/` |
| `EXCEPTION_WINE_STUB 0x80000100` in `wintrust.WVTAsn1SpcStatementTypeDecode` | Viz verifies VizLicense.dll's Authenticode signature; Wine's decoder is a stub | `vizasn1.dll` → `C:\windows\system32\`, then `wine regedit reg/vizasn1.reg` |
| "No matching pixel formats" | Wine 11 X11 EGL backend | `UseEGL=N` (in reg/engine.reg) |
| SIGSEGV in `ACE.dll` (`get_ip_interfaces`, null `FirstUnicastAddress`) | Wine reports address-less interfaces (bridge ports, down Wi-Fi) as Up | `iphlpapi.dll` proxy + `iphlpapi_wine.dll` (= copy of Wine's builtin, e.g. `/usr/lib64/wine-wow64/wine/x86_64-windows/iphlpapi.dll`) → `$VIZ/`; DllOverride in reg/engine.reg |
| ACE fails to resolve the local hostname | hostname resolves IPv6-only | add `127.0.1.1 <hostname>` to `/etc/hosts` |
| engine crashes in `InitVIPPlugin` | `plugin/TextToSpeech.vip` is a C++/CLI (.NET) plugin | `mkdir $VIZ/plugin-disabled && mv $VIZ/plugin/TextToSpeech.vip $VIZ/plugin-disabled/` |
| "Failed to remove system menu item CLOSE." box at every start | Wine's conhost | harmless; the launcher runs `okclick.exe` to press OK |
| Viz starts in configuration mode although you asked for Artist | Wibu error 213 "exclusive access conflicts": the single license is still held by a crashed session | `cmu --list-network` shows `Used=1`; wait, or `sudo systemctl restart codemeter` |

Then install the launcher and pick the license:

```bash
install -m755 bin/viz-wine ~/.local/bin/viz-wine
ln -s viz-wine ~/.local/bin/viz-wine51          # extra prefixes: viz-wine51 / viz-wine52 (optional)
cp build/okclick.exe "$VIZ/"
viz-wine config                                # Viz Config → Viz Licenses: choose your license
```

Viz's default `License_Core = ENG_ENG_CORE` asks for a Viz Engine core license; with a Free Viz Artist
license select it in Config (it ends up as `License_Core = ART_ARTIST_FREE`, `License_Containers = 0,0,0`
in `C:\ProgramData\vizrt\VizEngine\VizEngine-0.cfg`). **This is the genuine licensing UI — there is no
way around a real license.**

`viz-wine artist` now starts the engine plus the Viz Artist GUI.

## 5. Viz Artist GUI stuck at "Initializing Configuration Editor for Viz One"

The GUI's main thread (Chromium renames it `CrBrowserMain`) dies by **stack overflow** and Wine ends it
silently, so the windows stay up but dead. Cause: Qt WebEngine's font fallback recurses on
`IDWriteFontCollection::FindFamilyName(L"Tahoma")` when DirectWrite's system collection has no Tahoma.

Fix: `winetricks tahoma` and register the two files in
`HKLM\Software\Microsoft\Windows NT\CurrentVersion\Fonts` (`reg/tahoma.reg`). Fonts are only picked up by
a fresh wine session (`wineserver -k` first). `build/dwtest.exe` prints what DirectWrite resolves.

Not needed (kept in the launcher, harmless): Chromium `--no-sandbox --disable-gpu …` flags.

Killing a hung GUI: it becomes a zombie leader with live threads; `kill -9` each `/proc/<pid>/task/*`.

## 6. Viz Artist 5.1 extra: endless "QPixmap::fromWinHICON(), failed to GetIconInfo()" dialogs

Wine's `SHGetStockIconInfo` returns `S_OK` with a NULL `hIcon` for `SIID_WARNING/INFO/ERROR`, and Qt 6's
`qwindows.dll` then calls `GetIconInfo(NULL)` for every message box. Fix without touching shell32:

```bash
cd "$WINEPREFIX/drive_c/Program Files/vizrt/VizArtist"
python3 scripts/patch-import.py platforms/qwindows.dll SHELL32.dll SHEL32Z.dll   # keeps qwindows.dll.orig
cp build/SHEL32Z.dll .        # next to VizGui.exe, NOT into platforms/ (the loader searches the exe dir)
```

If `SHEL32Z.dll` is not found, Qt aborts with "no Qt platform plugin could be initialized. Available
platform plugins are: windows." Also applied pre-emptively to 5.2.1 (same Qt 6 `qwindows.dll`).

`SHEL32Z.dll` forwards the dozen shell32 imports qwindows uses and serves those stock icons from user32's
standard icons. (When editing `shel32z.def`: ordinal 727 is `SHGetImageList`, exported `NONAME`.)

## 7. Viz Graphic Hub 3.1.1

Artist needs a Graphic Hub to log in to. The GH bundle's MSI aborts with `missing_httpservice_exe_error`
(a VBScript custom action calls `Shell.Application.IsServiceRunning` for Http/RpcSs/LanmanServer), so
install the MSI directly and pre-answer those checks:

```bash
7z x VizGraphicHubBundle-x64-3.1.1.77542.exe -o$WINEPREFIX/drive_c/viz-setup/ghsrc
cd $WINEPREFIX/drive_c/viz-setup/ghsrc
wine msiexec /i VizGraphicHub.msi GH_MODE=2 DATADIR_DB=C:\\VizGHData TERMINAL_HTTP_PORT=19399 \
     HTTP_SERVICE_RUNNING=TRUE RPCSS_SERVICE_RUNNING=TRUE SERVER_SERVICE_RUNNING=TRUE /qn
wine regedit reg/graphichub.reg
```

`GH_MODE=2` is the "5/4 Free" mode. `VizGH.exe` refuses to be started directly ("Please use Viz GH
Terminal"), and the Terminal is a Windows service whose .NET web server never comes up under Wine's
services.exe. Run it as a plain console process instead:

```bash
cd "$WINEPREFIX/drive_c/Program Files/Vizrt/VizGH/Terminal"
cp VizGHTerminalService.exe VizGHTerminalConsole.exe
python3 scripts/patch-import.py VizGHTerminalConsole.exe ADVAPI32.dll ADVAPI3Z.dll
python3 scripts/patch-import.py ACE.dll ADVAPI32.dll ADVAPI3Z.dll        # ACE calls SetServiceStatus too
cp build/ADVAPI3Z.dll .
```

`ADVAPI3Z.dll` forwards everything to advapi32 except `StartServiceCtrlDispatcherA`,
`RegisterServiceCtrlHandlerA` and `SetServiceStatus`, which it fakes (the real exe exits immediately when
the dispatcher fails). Two more things from `reg/graphichub.reg`: `websocket.dll` must be hidden (Wine's
`WebSocketGetSupportedVersion` returns `E_NOTIMPL`, which makes WCF answer HTTP 500 to every request), and
`vcruntime140` must be native (Wine's builtin lacks `__FrameUnwindFilter`; any handled C++ exception in
Terminal.dll aborted the process).

```bash
viz-wine gh          # Terminal console; web UI at http://127.0.0.1:19399/Terminal.html
viz-wine gh-start    # starts VizGH.exe + VizGHNamingService.exe through the Terminal REST API
viz-wine gh-status   # vizdb=3 vizdbnamingservice=3 means running
viz-wine gh-stop
```

REST notes (port 19399): `status/current.entry` gives the server id; `PUT status/<ID>.payload` with
`vizdb=2`/`vizdbnamingservice=2` starts, `PUT status/current.payload` with `5` stops.
Default hub credentials are `Admin` / `VizDb`; ports 19396 (naming) and 19397 (server).

**Importing an old database** (tested with 2.4.2 data): copy the data directory into the prefix, set
`gh-data-directory` in `C:\ProgramData\vizrt\Viz GH Terminal\ConfigurationFiles\options_installer.xml`
(no trailing backslash), restart `viz-wine gh`, then `gh-start`. GH 3.1.1 upgrades the files in place on
first start (the log prints matching MainIndex/Reference/Folders/Users counts).

## 8. Everyday use

```bash
viz-wine gh & viz-wine gh-start      # once per session
viz-wine artist                      # 5.3
viz-wine52 artist                    # 5.2 (prefix vizartist52)
viz-wine51 artist                    # 5.1 (prefix vizartist51)
```

Only one engine at a time with a single license; the other one fails with Wibu 213. `viz-wine <exe>`
runs any other executable inside the prefix.

## Version notes

| Version | Install dir | Specifics |
|---|---|---|
| 5.3.0.60024 | `C:\Program Files\Vizrt` | reference setup; MSIs extracted from the bundle's Temp dir; no stock-icon problem |
| 5.2.1.60000 | `C:\Program Files\vizrt` | MSIs shipped in `Individual Installers/`; needs `winecfg -v win10` before the CodeMeter MSI; SHEL32Z applied pre-emptively (same Qt 6 `qwindows.dll` as 5.1); Engine, Artist and Basic/DataPool/Maps/PixelFx/Socialize plugins verified |
| 5.1.1.60000 | `C:\Program Files\vizrt` | SHEL32Z stock-icon proxy required (§6) |

All three run side by side in separate prefixes (`viz-wine`, `viz-wine52`, `viz-wine51`) against one
Graphic Hub 3.1.1 instance, but only one engine at a time when there is a single license.

## Diagnostics that paid off

* `WINEDEBUG=+loaddll,+seh` to map crash addresses to modules; `+relay` for the last calls before a crash.
* `strace -f -e trace=exit,kill,exit_group` on `vizgui.exe` to see the silent leader thread death.
* `gdb -p <pid>`, `catch signal SIGSEGV`, `x/64gx $rsp` and match against `+loaddll` load addresses.
* `WINEDEBUG=+dwrite` (huge) revealed the FindFamilyName recursion; `+msi` the ADDDEFAULT issue;
  `+http,+mmdevapi` for the Terminal and audio paths.
* `winedump -j export foo.dll` to verify the proxy DLLs export everything the client imports.

## License

The code and text in this repository are MIT licensed (see `LICENSE`). Viz Artist, Viz Engine, Viz
Graphic Hub are trademarks of Vizrt; CodeMeter is a trademark of WIBU-SYSTEMS. Neither is affiliated with
this project.
