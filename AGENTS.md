# AGENTS.md — Command Line Desktop (cld)

A terminal-based desktop environment simulator in C++.

## Tech Stack

- **Language**: C++17
- **TUI**: FTXUI v5.0 (`ScreenInteractive::Fullscreen` mode, `Canvas` pixel rendering)
- **Build**: CMake + system `libftxui-dev`
- **Config**: YAML (lightweight parser, no yaml-cpp dependency)

## Setup & Build

```bash
# Install dependency
apt install libftxui-dev       # Debian/Ubuntu
# or: brew install ftxui        # macOS
# or: FetchContent in CMake     # other platforms

cmake -B build && cmake --build build
./build/cld
```

## Architecture

All custom components inherit from a `Component` base class:

```
Component (abstract)
  ├── draw(Canvas&) = 0
  ├── handleEvent(Event) -> bool
  ├── title() -> string
  └── x_, y_, width_, height_ members
```

Rendering uses `ftxui::Canvas` with character-cell coordinate helpers in `CanvasHelpers.h` (maps char coords to Canvas braille coords via `x*2, y*4`).

| Module | Role |
|---|---|
| `EventLoop` | Thin `ScreenInteractive` wrapper |
| `Desktop` | Root `ftxui::ComponentBase`: background + compositor + event dispatch |
| `WindowManager` | Z-ordered `Component` list; focus, drag, resize |
| `Dock` | Fixed bottom bar (start button, app icons, clock) |
| `StartMenu` | Popup launcher with search + auto-detected apps |
| `FileManager` | File browser in a resizable window |
| `AppRunner` | Runs CLI commands via `popen`, captures stdout |
| `WindowFrame` | Titlebar + borders wrapping any `Component` |
| `ConfigLoader` | Reads `config.yaml` |
| `AppDetector` | Scans PATH + `.desktop` files (Linux) for apps |

## Canvas Coordinate System

`Canvas` uses braille pixels (2 wide × 4 tall per terminal cell). `CanvasHelpers.h` provides:
- `canvas::fill(c, x, y, w, h, bg)` — fill char-cell rect
- `canvas::write(c, x, y, text, fg, bg)` — draw colored text at char coords

## Key Bindings

| Shortcut | Action |
|---|---|
| `F1` | Toggle Start Menu |
| `F4` | Close focused window |
| `Tab` | Cycle window focus forward |
| `Shift+Tab` | Cycle window focus backward |
| `Ctrl+Q` | Quit |
| Mouse drag titlebar | Move window |
| Mouse drag edge | Resize window |
| Mouse click | Focus window |

Mouse position comes from terminal SGR mouse tracking — works over SSH xterm-forwarding.

## Auto-Detection

- **Linux**: parse `/usr/share/applications/*.desktop`
- **All**: scan `$PATH` for common tools
- **User-defined**: `config.yaml` under `apps:` key

## Conventions

- Source layout: `src/core/`, `src/ui/`, `src/apps/`, `src/config/`
- Headers `.h`, implementations `.cpp`
- Prefer `auto`, `const`, and RAII; no raw `new`/`delete`
- Events return `bool` (true = consumed, false = propagate)
- Window coords: `x, y, width, height`, top-left origin
- Keyboards-first design; mouse is enhancement, not required

## Status

**Implemented** — MVP working. Desktop renders background + dock. Windows with titlebar drag/resize/closing. Start menu with search. File manager with directory navigation. App runner with threaded stdout capture. App detection via PATH + .desktop files. Config loaded from `config.yaml`.
