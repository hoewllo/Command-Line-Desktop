# AGENTS.md — Command Line Desktop (cld)

C++17 TUI desktop for headless servers / WSL. Renders via FTXUI Canvas. No X11/Wayland needed.

## Setup & Build

```bash
# Optional: apt install libftxui-dev (FetchContent fallback auto-downloads)
cmake -B build && cmake --build build
./build/cld

# X11/SDL2 window mode:
cmake -B build-x11 -DENABLE_X11_BACKEND=ON && cmake --build build-x11
./build-x11/cld --x11
```

- `file(GLOB_RECURSE SOURCES src/*.cpp)` — new `.cpp` files auto-detected
- `X11Renderer.cpp` explicitly excluded unless `ENABLE_X11_BACKEND=ON`
- CI builds Linux/macOS/Windows with smoke test on `./build/cld` for 2s (`build.yml`)
- No tests or lint/format config present

## Source Layout

| Dir | Contents |
|---|---|
| `src/backend/` | `Renderer.h` (abstract), `TerminalRenderer.cpp` (no header!), `X11Renderer.h/.cpp` |
| `src/core/` | `Component.h` (all inline), `ColorUtils.h` (all inline) |
| `src/ui/` | `Desktop.h/.cpp`, `Layer.h` + `Compositor.cpp`, `Panel.h/.cpp`, `WindowManager.h/.cpp`, `WindowFrame.h/.cpp`, `WorkspaceManager.h/.cpp`, `StartMenu.h/.cpp`, `CanvasHelpers.h` (all inline) |
| `src/apps/` | `FileManager.h/.cpp`, `AppRunner.h/.cpp`, `AppDetector.h/.cpp`, `ConfigEditor.h/.cpp` |
| `src/config/` | `ConfigLoader.h/.cpp` (config structs live here, NOT a separate Config.h) |

## Architecture

```
Desktop (ftxui::ComponentBase)
  ├── Compositor (z-ordered layers)
  │   ├── WallpaperLayer       z=0   — gradient background
  │   ├── DesktopIconsLayer    z=10  — clickable app icons
  │   ├── WindowsLayer         z=20  — WindowManager (current workspace)
  │   ├── MenuLayer            z=30  — StartMenu popup
  │   ├── PanelLayer           z=50  — segment-based status bar
  │   ├── NotificationLayer    z=70  — toast popups (top-right, auto-fade ~5s)
  │   └── SwitcherLayer        z=100 — Alt+Tab overlay
  ├── WorkspaceManager (4 virtual desktops)
  │   └── WindowManager per workspace → WindowFrame → Component content
  └── Panel segments: Start(10) | sep(3) | Taskbar(flex) | Workspace(7) | Clock(9) | Tray(5-12)
```

### Segment-based Panel
Inspired by tmux status bar. Each `PanelSegment` subclass provides `draw()` + `minWidth()`.
Existing: `StartSegment`, `TaskSegment`, `WorkspaceSegment`, `ClockSegment`, `TraySegment` (battery + network from `/sys`).

### Layer system
`Layer` interface with `draw()`, `handleEvent()`, `name()`.
`SimpleLayer` wraps lambdas. `Compositor` sorts by z-order (low→back, high→front).
Events dispatch front-to-back (highest z first).

### Component model
All window content inherits from `Component` (abstract: `draw(Canvas&)`, `handleEvent(Event)`, `title()`).
`WindowFrame` wraps any Component with titlebar + borders + minimize/close.
Canvas coords in character cells (top-left origin). Helpers at `src/ui/CanvasHelpers.h` map to braille pixels via `x*2, y*4`.

## Key Bindings
| Shortcut | Action |
|---|---|
| `F1` | Toggle Start Menu |
| `F2` | Toggle Window Switcher (Alt+Tab) |
| `F4` | Close focused window |
| `Ctrl+Q` | Quit |
| `Alt+1..4` | Switch workspace (terminal-dependent) |
| `Alt+←/→` | Back/Forward (File Manager) |
| Right-click desktop | Context menu (Terminal, FM, Edit Config, Power...) |
| Right-click file | File context menu (Open, Rename, Copy, Delete, Properties) |

## Renderer Backend
- **Terminal** (default): wraps `ftxui::ScreenInteractive::Fullscreen()` — `desktop->setScreen(&screen_)` + `screen_.Loop()`
- **X11** (`--x11` flag): renders FTXUI `Screen::ToString()` ANSI output → parses true-color SGR codes → renders each cell via SDL_ttf. No private `ftxui::Color` member access needed.
- **Factory**: `Renderer::create(Renderer::Type)` in `TerminalRenderer.cpp` (section guarded by `#ifdef ENABLE_X11_BACKEND`)

## Gotchas
- `ftxui::Color` has private `red_`/`green_`/`blue_` — no public RGB accessors. X11 renderer works around via ANSI parsing.
- `Config` struct (AppConfig, DockConfig, WindowDefaults) defined in `src/config/ConfigLoader.h`, NOT `Config.h`
- `TerminalRenderer` has no separate header — class defined + factory function both in `TerminalRenderer.cpp`
- `config.yaml` lives at project root, loaded via `--config PATH` (default: `config.yaml`)
- App detection: Linux `.desktop` files, macOS `.app` bundles, `$PATH` scan, user-defined in `config.yaml apps:`
- Workspace switching via terminal escape sequences — not all terminals pass Alt+1..4 / Ctrl+Alt+arrows reliably
- Canvas coordinate system: `x*2, y*4` braille pixel mapping in `CanvasHelpers.h`

## Notifications & Power Dialog
- Notifications: `Desktop::showNotification(text, icon, color)` → compositor layer z=70, top-right, auto-fade ~120 frames (~5s at 16ms)
- Power Dialog: right-click desktop → "Power..." → centered popup with Log Out / Restart / Shutdown / Cancel

## Version
v0.3.x — Desktop with wallpaper, desktop icons, segment panel, 4 workspaces, Alt+Tab, window snap, FM, app runner (PTY), start menu with search, config editor, X11 backend, notifications, power dialog.
