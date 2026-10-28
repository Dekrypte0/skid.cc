# skeet-ui

A standalone ImGui menu with the classic skeet layout, rebuilt from scratch.
Windows x64, Win32 + Direct3D 11, static CRT, no runtime dependencies besides
the system DLLs.

The menu core is host-agnostic. `src/main.cpp` is only the demo host; the same
menu can be drawn inside any ImGui frame. The demo opens on a Home dashboard
with quick actions, profile status, and links into every settings page.

## Build

```powershell
# Release x64 (default)
.\build.ps1

# build and launch
.\build.ps1 -Run

# clean rebuild
.\build.ps1 -Clean

# debug build
.\build.ps1 -Configuration Debug
```

`build.ps1` finds MSBuild through `vswhere`, falls back to the usual Visual
Studio install paths, stops a running `skeet-ui.exe` (LNK1104 guard), then
builds. Output: `bin\x64\<Configuration>\skeet-ui.exe`.

Manual build:

```powershell
MSBuild.exe skeet-ui.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Requirements: Visual Studio 2022+ with the C++ desktop workload, Windows 10 SDK.
The project targets toolset `v145`; change `<PlatformToolset>` for older VS.

## Controls

| Input | Action |
| --- | --- |
| `INSERT` | hide / show the menu |
| `Ctrl+F` | open the settings search palette |
| `Up / Down` | move through search results |
| `Enter` | jump to the selected search result |
| `Home / End` | select the first / last result |
| `Esc` | close the palette / cancel a key capture |
| drag the header | move the window |
| `X` in the header | close the app |

Search indexes every setting on every page, including pages that have not been
opened yet. Clicking a result switches page and subtab, then flashes the row.

Every checkbox, slider, dropdown, color row and keybind is clickable. Key rows
capture the next key or mouse button; a click outside cancels the capture.
`Esc` closes the palette or cancels a key capture.

## Integration

Include the UI sources (`src/ui/*.cpp`, `imgui` + backends) in the target
project, then:

```cpp
#include "ui/ui.h"

// once, after ImGui::CreateContext() and the backend init
ui::Initialize(scale);   // scale = dpi / 96.0f

// every frame, between NewFrame() and Render()
if (ui::IsVisible())
{
    ui::Render();

    ImVec2 drag;
    if (ui::TakeDrag(drag))
        MoveOverlayWindow(drag.x, drag.y);   // your window, your rules
}

if (ui::ConsumeCloseRequest())
    RequestShutdown();
```

`ui::Render()` draws an 780x620 logical menu using the current ImGui window as
its surface; the demo host sizes its window to that. `ui::Toast(title, detail,
good)` pushes a notification into the stack at the bottom right.

Adapt these two spots for a different host:

- `main.cpp`: window creation, DPI awareness, D3D11 device, INSERT hotkey.
- `ui/theme.cpp`: fonts (Verdana, sized by DPI scale) and the accent color.

## Config

Settings live in the plain-text registry behind `cfg::Bind*`. Widgets bind
their value once, scoped by page / subtab / group / label:

```cpp
cfg::SetPage("Visuals", sub, kTabs[1].subs[sub]);
cfg::SetGroup("ESP");
CheckboxRow("Box", gs.vBox);       // serialized automatically
```

That gives defaults, save/load, and the search index without per-widget code.

Config files are written next to the executable:

```
skeet-ui\bin\x64\Release\configs\<name>.cfg
```

The Home dashboard and `Config > Profile` page expose quick save, reset, folder
access, and profile status. Writes use a temporary file plus an atomic replace;
a failed write cannot leave the selected profile half-written. `default` is
loaded at startup when present.

## Layout

```
src/
  main.cpp          Win32 + D3D11 host, DPI, hotkey, drag, close
  ui/
    ui.cpp          pages, widget library, search palette, toasts
    ui.h            public menu API
    theme.cpp/.h    fonts, palette, style, accent, DPI scale
    icons.cpp/.h    vector sidebar and UI icons
    config.cpp/.h   binding registry, save/load, palette metadata
  (shared) Lefrizzel AI/external/imgui   ImGui v1.92.8 (win32 + dx11 backends, pinned)
```

## Notes

- The demo pages are a layout showcase; values are stored and restored, but
  nothing touches a game process.
- Fonts are loaded from `C:\Windows\Fonts\verdana*.ttf`; the built-in ImGui
  font is used if they are missing.
- ImGui is pinned to the shared `v1.92.8` copy in `Lefrizzel AI/external/imgui`.
  Update that vendor copy and rebuild; widget code only uses stable API, but
  re-check `imgui_internal.h` usage in `icons.cpp` / `ui.cpp` after an update.
