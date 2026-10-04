# PGR3-recomp

English | [日本語](README.ja.md)

Turns the Xbox 360 game *Project Gotham Racing 3* into a native Windows program by static recompilation.
The game's PowerPC code is translated function by function into C++ and compiled; the Xbox 360 kernel, GPU,
audio and input are provided by the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) runtime (derived
from Xenia), with a set of fixes kept in this repository.

This repository holds only the project files, tools and SDK patch. **It contains no game code, data or
assets.** You convert your own copy of the game on your own computer.

## Legal notice and responsibility

- This project does not contain or distribute any code, data, assets or screenshots of Project Gotham
  Racing 3, and it does not contain any console keys or firmware.
- You must provide your own lawfully obtained copy of the game. Whether dumping, converting and running it is
  permitted depends on the laws of your country and on the terms you agreed to; **checking that and complying
  with it is entirely your responsibility.**
- **Do not distribute** anything the conversion produces: the generated C++ (`generated\`), the built
  `pgr3.exe`, extracted game files (`titles\`, `assets`), decrypted images or analysis output. All of it is
  derived from the game and must stay on your own computer.
- The software is provided "as is", without warranty of any kind (see [LICENSE](LICENSE)). **You use it
  entirely at your own risk and responsibility.** The author accepts no liability for any damage, data loss,
  legal claim or other consequence arising from its use.
- This is an independent, non-commercial compatibility and preservation project. It is not affiliated with,
  authorized or endorsed by Microsoft or Bizarre Creations. Project Gotham Racing, Xbox and Xbox 360 are
  trademarks of their respective owners.

## Supported version: Japanese release only

At present **only the Japan / Asia release of PGR3 is supported.**

| Item | Value |
|---|---|
| Release | Japan, Asia (En, Ja, Fr, De, Es, It, Zh, Ko) |
| Title ID | `4D5307D1` |
| `default.xex` build time | 2005-10-30 23:26:30 UTC |
| Disc image | XGD2 `.iso` |

Other regional releases (North America, Europe, ...) have **not** been tested and are expected not to work
as they are: `pgr3_config.toml` lists function addresses that are specific to this executable, and every
analysis note in this repository refers to it. Supporting another release means redoing that step (see
[Conversion gaps](#conversion-gaps)).

The bundled *Geometry Wars* (`gw.xex`) is not supported.

## Status

Works, as checked with scripted input and captured frames/audio:

- Boot, intro movies, menus, profile creation, career, garage, car purchase, races (London)
- Languages: the eight on the disc, selectable at startup
- Rendering resolution up to 4K (the game's 720p output rendered at 2x or 3x)
- Background music, engine sounds, song title display
- All camera views including the in-car view, photo mode
- Xbox controllers (via SDL)

Known problems and things not yet checked:

- The blurred background of the pause screen shows fine horizontal lines at higher resolutions
- Replay folder creation fails (`GAME:\Game\Replay`), so saving replays is untested
- Online play is not available
- Only a small part of the game has been played; expect more bugs

## Requirements

- Windows 10/11 x64, a Direct3D 12 capable GPU
- Visual Studio 2022 Build Tools (MSVC and the Windows SDK; used for headers and libraries)
- LLVM/Clang 20 or newer targeting MSVC (`x86_64-pc-windows-msvc`)
- CMake 3.25+, Ninja, Git, Python 3
- About 15 GB of free disk space (game files, SDK build, generated code)

## Setup

The scripts expect this layout (paths can be changed with environment variables, see `tools\env.bat`):

```
<root>\
  pgr3_recomp\        this repository
  ext\
    rexglue-sdk\      ReXGlue SDK, patched and built
    llvm\             LLVM (bin\clang.exe ...)
```

`tools\env.bat` sets up the compiler environment. Override its defaults with `PGR3_LLVM` (LLVM folder),
`PGR3_TOOLS` (a folder containing `cmake\bin` and `ninja`) and `PGR3_VCVARS` (path of `vcvars64.bat`).

### 1. Build the patched SDK

```bat
cd <root>\ext
git clone --recursive https://github.com/rexglue/rexglue-sdk.git
cd rexglue-sdk
git checkout c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git submodule update --init --recursive
git apply ..\..\pgr3_recomp\patches\rexglue-sdk.patch

..\..\pgr3_recomp\tools\env.bat cmake --preset win-amd64 ^
    -DCMAKE_CONFIGURATION_TYPES=Release;RelWithDebInfo -DCMAKE_DEFAULT_BUILD_TYPE=Release
..\..\pgr3_recomp\tools\env.bat cmake --build out/build/win-amd64 --config Release --target install
```

The patch was made against the commit above. If Git on Windows checks out the symbolic links inside the SDK's
submodules as small text files (the build then fails in `libmspack`), enable symlink support in Git or replace
those files with copies of what they point to.

### 2. Extract your game

```bat
cd <root>\pgr3_recomp
python tools\xdvdfs.py "<your PGR3 disc image>.iso" extract titles\pgr3\game
```

### 3. Convert and build

```bat
tools\build.bat
```

This runs the SDK's code generator on `titles\pgr3\game\default.xex` (about 65 MB of C++ in
`generated\default\`) and compiles it at low priority with two jobs. The result is
`out\build\win-amd64-release\pgr3.exe`.

## Playing

Start `play.bat`. A settings window opens first:

- **Game folder** — the extracted disc (the folder that contains `default.xex`)
- **Language** — Japanese, English, French, German, Spanish, Italian, Korean, Traditional Chinese
- **Resolution** — 720p, 1080p, 1440p or 4K
- **Fullscreen**

The choices are saved next to the executable in `pgr3_launcher.toml`. Tick "do not show this window again" to
skip it; hold Shift while starting to bring it back. Options given on the command line
(`play.bat <game folder> --fullscreen=false ...`) take priority.

The log is written to `pgr3.log`, and a crash report to `pgr3_crash.txt`.

## How it works

```
ISO --xdvdfs.py--> titles\pgr3\game\default.xex
                        |
     rexglue codegen (pgr3_manifest.toml + pgr3_config.toml)
                        v
              generated\default\*.cpp --Clang--> pgr3.exe + rexruntime.dll + rexgpu-xenos.dll
```

- **CPU**: each PowerPC function becomes a C++ function; guest memory and calling conventions stay as on
  the console.
- **Kernel / XAM**: implemented natively by the SDK runtime.
- **GPU**: the game's own Direct3D 9 library is recompiled too; the command stream it writes for the console
  GPU is executed on Direct3D 12 by the SDK's Xenos backend.
- **This repository's code** (`src\`): the application shell, startup settings window, crash logger and an
  unattended test harness.

### What the SDK patch changes

`patches\rexglue-sdk.patch` is a set of fixes found while bringing this game up:

- `vupkd3d128` (64-bit packed format) was translated wrongly, which broke animation data and blacked out the
  in-car view
- XMP music player: playback of WMA/MP3 title playlists and `XMPCaptureOutput` were missing (adds FFmpeg's
  libavformat to the build); song information layout
- `XMASetLoopData` read its argument as the wrong structure, breaking looped sounds
- `XGetLanguage` always returned English
- Floating-point exceptions were left unmasked on threads created by the runtime
- Debug aids: audio dump, per-frame draw-call log, draw skipping, floating-point trap

### Conversion gaps

Small functions without unwind data that are only reached through pointers are missed by the SDK's analysis.
`tools\find_entries.py` finds them from data references; the result for the supported executable is in
`pgr3_config.toml` (addresses only).

### Tools

| File | Purpose |
|---|---|
| `tools\xdvdfs.py` | List / extract files from an Xbox or Xbox 360 disc image |
| `tools\xex.py` | Print XEX2 headers; dump the image (needs the key in `XEX_RETAIL_KEY`, not included) |
| `tools\imports.py` | Kernel imports of a XEX and whether the SDK implements them |
| `tools\find_entries.py` | Find function entries the code generator missed |
| `tools\gfunc.py`, `maprip.py`, `gpudbg.py`, `audiodump.py`, `contact.py` | Debugging helpers |
| `tools\run.sh`, `cockpit_test.sh` | Unattended runs with scripted input |

## License and acknowledgements

The files in this repository are released under the [MIT License](LICENSE).

`patches\rexglue-sdk.patch`, `generated\rexglue.cmake` and the application scaffold in `src\` modify or
derive from the ReXGlue SDK, which is BSD 3-Clause licensed (Copyright (c) 2026 Tom Clay; portions
Copyright (c) Ben Vanik and the Xenia project contributors). The SDK itself is not included here; its
license applies to it and to the patched parts. The SDK builds FFmpeg (LGPL) for audio decoding.

Thanks to the ReXGlue SDK, Xenia and XenonRecomp projects, whose work makes this possible.
