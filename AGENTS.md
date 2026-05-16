# AGENTS.md — Command Line Desktop (cld)

A full desktop environment experience in your terminal.
Use it on headless servers, WSL, or any terminal — no X11/Wayland needed.

## Tech Stack

- **Language**: C++17
- **TUI**: FTXUI v5.0 (`ScreenInteractive::Fullscreen` mode, `Canvas` pixel rendering)
- **Build**: CMake + system `libftxui-dev`
- **Config**: YAML (lightweight parser, no yaml-cpp dependency)

## Setup & Build

```bash
# Install dependency (optional — FetchContent fallback auto-downloads FTXUI)
apt install libftxui-dev       # Debian/Ubuntu
# or: brew install ftxui        # macOS

cmake -B build && cmake --build build
./build/cld
```

CMake tries `find_package(ftxui CONFIG)` first, then falls back to
FetchContent. No system dep strictly required.

## Architecture

```
Desktop (ftxui::ComponentBase)
  ├── Compositor (z-ordered layers)
  │   ├── WallpaperLayer       z=0   — gradient background
  │   ├── DesktopIconsLayer    z=10  — clickable app icons
  │   ├── WindowsLayer         z=20  — WindowManager (current workspace)
  │   ├── MenuLayer            z=30  — StartMenu popup
  │   ├── PanelLayer           z=50  — status bar (segments)
  │   └── SwitcherLayer        z=100 — Alt+Tab overlay
  ├── WorkspaceManager (4 virtual desktops)
  │   └── WindowManager per workspace
  │       └── WindowFrame + Component content
  ├── Panel (segment-based status bar)
  │   ├── StartSegment    — [Start] button
  │   ├── TaskSegment     — running windows (◉ active, ● normal, ○ minimized)
  │   ├── WorkspaceSegment — "1/4" indicator
  │   └── ClockSegment    — HH:MM display
  └── Event dispatch: context menu > overlays > menu > panel > windows
```

### Layer system (`Layer.h`, `Compositor.cpp`)
- Each UI feature is a `Layer` with `draw()` and optional `handleEvent()`
- `SimpleLayer` wraps lambdas for quick layer creation
- Z-order determines draw order (low → back, high → front)
- Events are processed front-to-back (highest z first)

### Component base (`core/Component.h`)
```
Component (abstract)
  ├── draw(Canvas&) = 0
  ├── handleEvent(Event) -> bool
  ├── title() -> string
  └── x_, y_, width_, height_, visible_, focused_
```

All window content inherits from Component. WindowFrame wraps any Component with a titlebar + borders.

### Panel segments (`Panel.h`)
The status bar (replaces old Dock) uses a segment architecture inspired by tmux status bar:
- Segments are independent drawable units
- Layout: `Start(10) | sep(3) | Taskbar(flex) | Workspace(7) | Clock(9)`
- Adding a new segment = subclass PanelSegment + add to layout

### Workspaces (`WorkspaceManager.h`)
- 4 virtual desktops, each with its own WindowManager
- Switch: Ctrl+Alt+Left/Right arrows (terminal-dependent)
- Each workspace has independent window list
- Panel shows "current/total" indicator

## Canvas Coordinate System

`Canvas` uses braille pixels (2 wide × 4 tall per terminal cell). `CanvasHelpers.h` provides:
- `canvas::fill(c, x, y, w, h, bg)` — fill char-cell rect
- `canvas::write(c, x, y, text, fg, bg)` — draw colored text at char coords
- `canvas::hline`, `canvas::vline` — unicode line drawing

All coordinates are in **character cells** (top-left origin). Functions internally map to
braille coordinates via `x*2, y*4`.

## Key Bindings

| Shortcut | Action |
|---|---|
| `F1` | Toggle Start Menu |
| `F2` | Toggle Window Switcher (Alt+Tab style) |
| `F4` | Close focused window |
| `Tab` / `Shift+Tab` | Cycle window focus (in switcher) |
| `Ctrl+Q` | Quit |
| `Alt+1..4` | Switch workspace (terminal-dependent) |
| Mouse drag titlebar | Move window |
| Mouse drag edge | Resize window |
| Mouse click | Focus window |
| Right-click desktop | Context menu |
| Click desktop icon | Launch app |

Mouse position comes from terminal SGR mouse tracking — works over SSH xterm-forwarding.

## Auto-Detection

- **Linux**: parse `/usr/share/applications/*.desktop`
- **macOS**: scan `/Applications/*.app`, `~/Applications/*.app`
- **All**: scan `$PATH` for common tools (works on Linux, macOS, Windows)
- **User-defined**: `config.yaml` under `apps:` key

## Conventions

- Source layout: `src/core/`, `src/ui/`, `src/apps/`, `src/config/`
- Headers `.h`, implementations `.cpp`
- Prefer `auto`, `const`, and RAII; no raw `new`/`delete`
- Events return `bool` (true = consumed, false = propagate)
- Window coords: `x, y, width, height`, top-left origin
- Keyboards-first design; mouse is enhancement, not required
- Segment-based architecture for panel features (easy to extend)
- New panel segments: subclass `PanelSegment`, add to `Panel` layout

## Status

**v0.2.x** — Desktop with wallpaper gradient, desktop icons, tmux-style panel,
4 virtual workspaces, Alt+Tab window switcher, window snapping, floating windows
with drag/resize/minimize/close, file manager, app runner with PTY, start menu
with search, config editor, app detection.
