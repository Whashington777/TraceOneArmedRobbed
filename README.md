# trace.

One-armed Robber tooling for Linux / Proton. C++ runtime, ImGui menu and a console loader.

## Build

CMake 3.20+, a C++17 compiler and SDL2 development files:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

For the loader alone, add `-DTRACE_BUILD_PREVIEW=OFF`. It produces `TraceLoader`, `libTraceLoader.so` and `bootstrap/main.lua`. Keep them together.

The game runs as a Windows process under Proton. Build its runtime with MinGW:

```sh
cmake -S . -B build-win64 \
  -DCMAKE_TOOLCHAIN_FILE=tools/cmake/mingw64.cmake \
  -DTRACE_BUILD_PREVIEW=OFF -DTRACE_BUILD_RUNTIME=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-win64 -j 4
```

## Run

Requires UE4SS 3.0.1 and DX11. Keep the compiled runtime, injector and fonts together in any folder. The loader installs or updates the TraceOAR bootstrap in the game directory. Restart the game after that first installation; the bootstrap waits for manual injection and does not load the DLL on its own.

Steam launch options:

```text
WINEDLLOVERRIDES="dwmapi=n,b" %command% -dx11
```

Start a local host level, then run:

```sh
./build/TraceLoader --check \
  --dll '/path/to/runtime/TraceRuntime.dll' \
  --injector '/path/to/runtime/TraceInject.exe'
```

Remove `--check` to load. The loader uses the running game's Wine binary and prefix. Use `--pid` if more than one game process is running. DLL updates require a game restart. Successful gameplay attachment prints `Gameplay bridge connected to the local host.`

Insert / End toggles the menu. F10 resets changes. Profiles are stored beside the runtime. Switch RU / EN in the header or Settings; the language is saved with the profile.

## Scope

The runtime targets local host sessions and this game executable SHA256:

```text
66ee255697a8167525c6e78b744525fd38aa643f2878e4c03a63330584248280
```

A different executable disables gameplay changes. Unconnected controls stay disabled. Universal keycard support covers card readers.

Verified on Linux / Proton: source builds, four automated checks, manual DLL loading and local-host gameplay.

This repository contains source only. Releases contain the Linux console loader; no Windows binaries or game assets are distributed. Linux builds use system glibc and libstdc++.

## Dependencies

Dear ImGui and MinHook are bundled for building. Their licenses are kept in their directories. Font and GUI reference licenses are included separately. UE4SS is an external runtime dependency.
