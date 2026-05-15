# command-desktop

A terminal-based desktop environment simulator in C++17.

Turn your terminal into a pseudo-desktop with overlapping windows, a taskbar, start menu, file manager, and app launcher — all rendered via FTXUI over SSH-capable terminal escape sequences.

## Quick Start

```bash
# Debian/Ubuntu
apt install libftxui-dev

# Build
cmake -B build && cmake --build build

# Run
./build/command-desktop
```

## Usage

| Key | Action |
|---|---|
| `F1` | Open Start Menu |
| `F4` | Close focused window |
| `Tab` / `Shift+Tab` | Cycle window focus |
| `Ctrl+Q` | Quit |
| Mouse drag titlebar | Move window |
| Mouse drag edge | Resize window |

The Start Menu supports type-to-search filtering. File Manager supports arrow navigation and Enter to enter directories.

## Configuration

Edit `config.yaml` to customize colors, dock height, and app list. Apps are also auto-detected from `$PATH` and `.desktop` files.

```yaml
background_color: "#1a1a2e"
apps:
  - name: Terminal
    command: xterm
  - name: File Manager
    command: ""
    internal: true
dock:
  height: 2
```

## Requirements

- C++17 compiler
- CMake 3.14+
- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) v5.x (`libftxui-dev` on Debian)

## License

MIT
