# skeet-reference

Snapshot of <https://github.com/lxcasfr/skeet> at commit `109119a`
("Update README.md"), fetched 2026-10-28.

- Author: lucas (`lxcasfr`)
- License: MIT, see `LICENSE`
- Upstream README: "skeet ui remake" — a from-scratch remake of the
  skeet / gamesense menu, not the original closed-source skeet source.
- Scope: UI only. One source file (`src/main.cpp`) plus PNG icon assets.
  No process access, no hooks, no game code.

## Build (upstream, unchanged)

Windows, CMake 3.16+, MSVC. Dependencies come through `FetchContent`:
ImGui 1.92.5, GLFW 3.4, stb, FreeType.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable lands in `build/` and copies `assets/` next to itself. It opens
a standalone GLFW/OpenGL window; there is no injectable DLL here.

## Notes for this copy

This folder is a read-only reference for the menu in `skeet-ui/` (Win32 +
D3D11). Nothing here is part of that build.

Differences from upstream:

- `agents.md` and `claude.md` were not copied. They are LLM-directed notes;
  keeping them out avoids them being mistaken for workspace instructions.
- The trailing `CMakeLists.txt` comment that pointed at `agents.md` was
  dropped.

Everything else is byte-identical. `src/main.cpp` SHA-256:

```
A6EEB781657C2A76F9C89927BE2994CF818023390F6D129DE29146EB9062E406
```

## Layout

```
src/main.cpp      menu, widgets, pages, fonts, icon loading
assets/icons/     PNG icons (sidebar, weapons, categories)
docs/             preview screenshots
CMakeLists.txt    upstream build
LICENSE           MIT
```
