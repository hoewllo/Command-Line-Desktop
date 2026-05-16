#include "X11Renderer.h"
#include "ui/Desktop.h"
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>
#include <memory>
#include <cstdlib>
#include <sstream>

struct CellData {
  std::string ch = " ";
  uint8_t fg_r = 200, fg_g = 200, fg_b = 220;
  uint8_t bg_r = 16, bg_g = 16, bg_b = 32;
};
using CellGrid = std::vector<std::vector<CellData>>;

static void parseAnsi(const std::string& ansi, int cols, int rows,
                      CellGrid& cells);

struct X11Renderer::Impl {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  TTF_Font* font = nullptr;
  bool quit = false;
  int cells_x = 100;
  int cells_y = 37;

  // Parsed cell buffer
  CellGrid cells;
};

X11Renderer::X11Renderer(int cell_w, int cell_h)
  : cell_w_(cell_w), cell_h_(cell_h),
    impl_(std::make_unique<Impl>()) {}

X11Renderer::~X11Renderer() { shutdownSDL(); }

bool X11Renderer::initSDL() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }
  if (TTF_Init() < 0) {
    std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    SDL_Quit();
    return false;
  }

  // Find monospace font
  const char* fontPaths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
    nullptr
  };
  for (const char** p = fontPaths; *p; ++p) {
    impl_->font = TTF_OpenFont(*p, cell_h_ - 4);
    if (impl_->font) break;
  }
  if (!impl_->font) {
    std::fprintf(stderr, "Warning: No monospace font found. Install fonts-dejavu-core.\n");
    // Fall back to built-in rendering without text
  }

  win_w_ = impl_->cells_x * cell_w_;
  win_h_ = impl_->cells_y * cell_h_;

  impl_->window = SDL_CreateWindow("Command Line Desktop",
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    win_w_, win_h_,
    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!impl_->window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    TTF_Quit(); SDL_Quit();
    return false;
  }

  impl_->renderer = SDL_CreateRenderer(impl_->window, -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!impl_->renderer) {
    SDL_DestroyWindow(impl_->window);
    TTF_Quit(); SDL_Quit();
    return false;
  }

  // Init cell buffer
  impl_->cells.resize(impl_->cells_y);
  for (auto& row : impl_->cells)
    row.resize(impl_->cells_x);

  return true;
}

void X11Renderer::shutdownSDL() {
  if (impl_->font) TTF_CloseFont(impl_->font);
  if (impl_->renderer) SDL_DestroyRenderer(impl_->renderer);
  if (impl_->window) SDL_DestroyWindow(impl_->window);
  TTF_Quit();
  SDL_Quit();
}

// Parse FTXUI ANSI output into our cell buffer
static void parseAnsi(const std::string& ansi, int cols, int rows,
                      CellGrid& cells) {
  // Default colors (FTXUI defaults)
  uint8_t fg_r = 200, fg_g = 200, fg_b = 220;
  uint8_t bg_r = 16, bg_g = 16, bg_b = 32;

  int x = 0, y = 0;
  size_t i = 0;
  while (i < ansi.size() && y < rows) {
    if (ansi[i] == '\x1b') {
      // ANSI escape sequence
      i++; // skip [
      if (i < ansi.size() && ansi[i] == '[') {
        i++;
        // Parse parameters
        std::vector<int> params;
        while (i < ansi.size() && ansi[i] != 'm' && ansi[i] != 'H') {
          if (ansi[i] == ';') { i++; continue; }
          if (ansi[i] >= '0' && ansi[i] <= '9') {
            int val = 0;
            while (i < ansi.size() && ansi[i] >= '0' && ansi[i] <= '9') {
              val = val * 10 + (ansi[i] - '0');
              i++;
            }
            params.push_back(val);
          } else i++;
        }
        if (i < ansi.size() && ansi[i] == 'm') {
          // SGR sequence
          for (size_t p = 0; p < params.size(); ) {
            if (params[p] == 38 && p + 2 < params.size() && params[p+1] == 2) {
              // TrueColor foreground: 38;2;R;G;B
              fg_r = static_cast<uint8_t>(params[p+2]);
              fg_g = static_cast<uint8_t>(params[p+3]);
              fg_b = static_cast<uint8_t>(params[p+4]);
              p += 5;
            } else if (params[p] == 48 && p + 2 < params.size() && params[p+1] == 2) {
              // TrueColor background: 48;2;R;G;B
              bg_r = static_cast<uint8_t>(params[p+2]);
              bg_g = static_cast<uint8_t>(params[p+3]);
              bg_b = static_cast<uint8_t>(params[p+4]);
              p += 5;
            } else if (params[p] == 0) {
              // Reset
              fg_r = 200; fg_g = 200; fg_b = 220;
              bg_r = 16; bg_g = 16; bg_b = 32;
              p++;
            } else p++;
          }
        } else if (i < ansi.size() && ansi[i] == 'H') {
          // Cursor position (not used)
        }
        i++;
      } else {
        i++;
      }
    } else if (ansi[i] == '\n') {
      x = 0; y++;
      if (y >= rows) break;
      i++;
    } else if (ansi[i] == '\r') {
      x = 0;
      i++;
    } else if (ansi[i] == '\t') {
      x = (x + 8) & ~7;
      i++;
    } else {
      // Regular character
      if (y < rows && x < cols) {
        auto& cell = cells[static_cast<size_t>(y)][static_cast<size_t>(x)];
        cell.ch = ansi[i];
        cell.fg_r = fg_r; cell.fg_g = fg_g; cell.fg_b = fg_b;
        cell.bg_r = bg_r; cell.bg_g = bg_g; cell.bg_b = bg_b;
      }
      x++;
      if (x >= cols) { x = 0; y++; }
      i++;
    }
  }
}

namespace {

ftxui::Event sdlKeyToEvent(SDL_Keycode key, bool shift, bool ctrl, bool alt) {
  static const std::map<SDL_Keycode, ftxui::Event> special = {
    {SDLK_UP, ftxui::Event::ArrowUp},
    {SDLK_DOWN, ftxui::Event::ArrowDown},
    {SDLK_LEFT, ftxui::Event::ArrowLeft},
    {SDLK_RIGHT, ftxui::Event::ArrowRight},
    {SDLK_RETURN, ftxui::Event::Return},
    {SDLK_BACKSPACE, ftxui::Event::Backspace},
    {SDLK_TAB, shift ? ftxui::Event::TabReverse : ftxui::Event::Tab},
    {SDLK_ESCAPE, ftxui::Event::Escape},
    {SDLK_DELETE, ftxui::Event::Delete},
    {SDLK_HOME, ftxui::Event::Home},
    {SDLK_END, ftxui::Event::End},
    {SDLK_PAGEUP, ftxui::Event::PageUp},
    {SDLK_PAGEDOWN, ftxui::Event::PageDown},
    {SDLK_F1, ftxui::Event::F1},
    {SDLK_F2, ftxui::Event::F2},
    {SDLK_F3, ftxui::Event::F3},
    {SDLK_F4, ftxui::Event::F4},
    {SDLK_F5, ftxui::Event::F5},
    {SDLK_F6, ftxui::Event::F6},
    {SDLK_F7, ftxui::Event::F7},
    {SDLK_F8, ftxui::Event::F8},
    {SDLK_F9, ftxui::Event::F9},
    {SDLK_F10, ftxui::Event::F10},
    {SDLK_F11, ftxui::Event::F11},
    {SDLK_F12, ftxui::Event::F12},
  };
  auto it = special.find(key);
  if (it != special.end()) return it->second;
  if (ctrl && key >= SDLK_a && key <= SDLK_z) {
    char c = static_cast<char>(key - SDLK_a + 1);
    return ftxui::Event::Special({c});
  }
  if (!ctrl) {
    SDL_Keymod mod = SDL_GetModState();
    bool caps = (mod & KMOD_CAPS) != 0;
    if (key >= SDLK_a && key <= SDLK_z) {
      char c = static_cast<char>((shift || caps) ? (key - SDLK_a + 'A') : (key - SDLK_a + 'a'));
      return ftxui::Event::Character(c);
    }
    static const std::map<SDL_Keycode, std::pair<char, char>> sym = {
      {SDLK_SPACE, {' ', ' '}}, {SDLK_0, {'0', ')'}}, {SDLK_1, {'1', '!'}},
      {SDLK_2, {'2', '@'}}, {SDLK_3, {'3', '#'}}, {SDLK_4, {'4', '$'}},
      {SDLK_5, {'5', '%'}}, {SDLK_6, {'6', '^'}}, {SDLK_7, {'7', '&'}},
      {SDLK_8, {'8', '*'}}, {SDLK_9, {'9', '('}},
      {SDLK_MINUS, {'-', '_'}}, {SDLK_EQUALS, {'=', '+'}},
      {SDLK_LEFTBRACKET, {'[', '{'}}, {SDLK_RIGHTBRACKET, {']', '}'}},
      {SDLK_BACKSLASH, {'\\', '|'}}, {SDLK_SEMICOLON, {';', ':'}},
      {SDLK_QUOTE, {'\'', '"'}}, {SDLK_COMMA, {',', '<'}},
      {SDLK_PERIOD, {'.', '>'}}, {SDLK_SLASH, {'/', '?'}},
      {SDLK_BACKQUOTE, {'`', '~'}},
    };
    auto sit = sym.find(key);
    if (sit != sym.end()) {
      char c = shift ? sit->second.second : sit->second.first;
      return ftxui::Event::Character(c);
    }
  }
  return ftxui::Event::Special({});
}

ftxui::Event sdlMouseToEvent(SDL_Event& ev, int cell_w, int cell_h) {
  ftxui::Mouse mouse;
  int mx, my;

  if (ev.type == SDL_MOUSEMOTION) {
    mx = ev.motion.x; my = ev.motion.y;
  } else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
    mx = ev.button.x; my = ev.button.y;
  } else if (ev.type == SDL_MOUSEWHEEL) {
    SDL_GetMouseState(&mx, &my);
    mouse.button = (ev.wheel.y > 0) ? ftxui::Mouse::WheelUp : ftxui::Mouse::WheelDown;
    mouse.motion = ftxui::Mouse::Pressed;
    mouse.x = mx / cell_w;
    mouse.y = my / cell_h;
    return ftxui::Event::Mouse("", mouse);
  } else {
    return ftxui::Event::Special({});
  }

  mouse.x = mx / cell_w;
  mouse.y = my / cell_h;

  if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
    switch (ev.button.button) {
      case SDL_BUTTON_LEFT: mouse.button = ftxui::Mouse::Left; break;
      case SDL_BUTTON_MIDDLE: mouse.button = ftxui::Mouse::Middle; break;
      case SDL_BUTTON_RIGHT: mouse.button = ftxui::Mouse::Right; break;
      default: mouse.button = ftxui::Mouse::None; break;
    }
    mouse.motion = (ev.type == SDL_MOUSEBUTTONDOWN) ? ftxui::Mouse::Pressed : ftxui::Mouse::Released;
  } else {
    Uint32 state = SDL_GetMouseState(nullptr, nullptr);
    mouse.button = (state & SDL_BUTTON(SDL_BUTTON_LEFT)) ? ftxui::Mouse::Left :
                   (state & SDL_BUTTON(SDL_BUTTON_RIGHT)) ? ftxui::Mouse::Right :
                   ftxui::Mouse::None;
    mouse.motion = (mouse.button != ftxui::Mouse::None) ? ftxui::Mouse::Pressed : ftxui::Mouse::Released;
  }

  SDL_Keymod mod = SDL_GetModState();
  mouse.shift = (mod & KMOD_SHIFT) != 0;
  mouse.control = (mod & KMOD_CTRL) != 0;
  mouse.meta = (mod & KMOD_ALT) != 0;

  return ftxui::Event::Mouse("", mouse);
}

} // namespace

int X11Renderer::run(std::shared_ptr<Desktop> desktop) {
  if (!initSDL()) return 1;
  desktop->setScreen(nullptr);

  while (!impl_->quit) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_QUIT: impl_->quit = true; break;

        case SDL_KEYDOWN: {
          bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
          bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
          bool alt = (SDL_GetModState() & KMOD_ALT) != 0;
          if (ctrl && ev.key.keysym.sym == SDLK_q) { impl_->quit = true; break; }
          auto ftxui_ev = sdlKeyToEvent(ev.key.keysym.sym, shift, ctrl, alt);
          desktop->OnEvent(ftxui_ev);
          if (ev.key.keysym.sym == SDLK_F10) impl_->quit = true;
          break;
        }

        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL: {
          auto ftxui_ev = sdlMouseToEvent(ev, cell_w_, cell_h_);
          desktop->OnEvent(ftxui_ev);
          break;
        }

        case SDL_WINDOWEVENT:
          if (ev.window.event == SDL_WINDOWEVENT_RESIZED ||
              ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            win_w_ = ev.window.data1;
            win_h_ = ev.window.data2;
            int new_cx = std::max(20, win_w_ / cell_w_);
            int new_cy = std::max(10, win_h_ / cell_h_);
            if (new_cx != impl_->cells_x || new_cy != impl_->cells_y) {
              impl_->cells_x = new_cx;
              impl_->cells_y = new_cy;
              impl_->cells.resize(static_cast<size_t>(impl_->cells_y));
              for (auto& row : impl_->cells)
                row.resize(static_cast<size_t>(impl_->cells_x));
            }
          }
          break;
      }
    }

    renderFrame(*desktop);
    SDL_Delay(16);
  }
  return 0;
}

void X11Renderer::renderFrame(Desktop& desktop) {
  if (impl_->cells_x <= 0 || impl_->cells_y <= 0) return;

  // Render desktop to FTXUI Screen
  auto screen = ftxui::Screen(impl_->cells_x, impl_->cells_y);
  auto element = desktop.Render();
  ftxui::Render(screen, element);

  // Get ANSI string and parse into our cell buffer
  std::string ansi = screen.ToString();
  parseAnsi(ansi, impl_->cells_x, impl_->cells_y, impl_->cells);

  // Clear SDL
  SDL_SetRenderDrawColor(impl_->renderer, 16, 16, 32, 255);
  SDL_RenderClear(impl_->renderer);

  // Render each cell
  SDL_Rect cellRect;
  cellRect.w = cell_w_;
  cellRect.h = cell_h_;

  for (int y = 0; y < impl_->cells_y; ++y) {
    for (int x = 0; x < impl_->cells_x; ++x) {
      auto& cell = impl_->cells[static_cast<size_t>(y)][static_cast<size_t>(x)];
      cellRect.x = x * cell_w_;
      cellRect.y = y * cell_h_;

      // Background
      SDL_SetRenderDrawColor(impl_->renderer,
        cell.bg_r, cell.bg_g, cell.bg_b, 255);
      SDL_RenderFillRect(impl_->renderer, &cellRect);

      // Character
      if (impl_->font && cell.ch != " " && !cell.ch.empty()) {
        SDL_Color sdlFg = {cell.fg_r, cell.fg_g, cell.fg_b, 255};
        auto surf = TTF_RenderUTF8_Blended(impl_->font, cell.ch.c_str(), sdlFg);
        if (surf) {
          auto tex = SDL_CreateTextureFromSurface(impl_->renderer, surf);
          if (tex) {
            SDL_Rect dst = {cellRect.x + 1, cellRect.y + 1, surf->w, surf->h};
            SDL_RenderCopy(impl_->renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
          }
          SDL_FreeSurface(surf);
        }
      }
    }
  }

  SDL_RenderPresent(impl_->renderer);
}
