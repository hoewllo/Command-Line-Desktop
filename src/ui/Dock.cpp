#include "Dock.h"
#include "WindowManager.h"
#include "WindowFrame.h"
#include "CanvasHelpers.h"
#include "core/ColorUtils.h"
#include <algorithm>
#include <ctime>
#include <ftxui/screen/terminal.hpp>

namespace {
std::tm portable_localtime(const std::time_t& t) {
  std::tm result = {};
#ifdef _WIN32
  localtime_s(&result, &t);
#else
  localtime_r(&t, &result);
#endif
  return result;
}
}

Dock::Dock() {}

void Dock::setBackgroundColor(const std::string& hex) {
  bgColor_ = color::parseHex(hex, bgColor_);
}

void Dock::setTextColor(const std::string& hex) {
  textColor_ = color::parseHex(hex, textColor_);
}

void Dock::removeApp(const std::string& name) {
  apps_.erase(
    std::remove_if(apps_.begin(), apps_.end(),
      [&](const DockApp& a) { return a.name == name; }),
    apps_.end());
}

void Dock::draw(ftxui::Canvas& canvas) {
  if (!visible()) return;

  auto accent = ftxui::Color::RGB(233, 69, 96);
  auto dimText = ftxui::Color::RGB(140, 140, 160);
  auto activeFg = ftxui::Color::RGB(255, 255, 255);
  auto normalFg = ftxui::Color::RGB(200, 200, 220);
  auto minFg = ftxui::Color::RGB(100, 100, 120);
  auto runFg = ftxui::Color::RGB(0, 255, 128);

  int sw = canvas.width() / 2;
  int sh = canvas.height() / 4;
  int dockY = sh - height_;

  // Fill dock bar
  canvas::fill(canvas, 0, dockY, sw, height_, bgColor_);

  // "Start" with run/restart indicators  
  if (flash_ > 0) {
    canvas::write(canvas, 1, dockY, " [Start] ", bgColor_, accent);
    flash_--;
  } else {
    canvas::write(canvas, 1, dockY, " [Start] ", accent, bgColor_);
  }

  // Separator
  canvas::write(canvas, 10, dockY, " \u2502 ", dimText, bgColor_);

  int x = 14;
  int clockW = 8;
  int maxX = sw - 2 - clockW;

  // Draw windows from WindowManager
  if (wm_) {
    auto wins = wm_->windows();
    for (size_t i = 0; i < wins.size(); ++i) {
      auto* win = wins[i];
      std::string label = win->title();
      bool isFocused = win->focused();
      bool isMin = win->isMinimized();

      // Truncate label
      if (static_cast<int>(label.size()) > 14)
        label = label.substr(0, 13) + "\u2026";
      int entryW = static_cast<int>(label.size()) + 3;

      if (x + entryW + 1 > maxX) break;

      // Indicator
      std::string ind;
      if (isFocused) ind = "\u25C9";       // ◉ focused
      else if (isMin) ind = "\u25CB";      // ○ minimized
      else ind = "\u25CF";                 // ● normal

      auto fg = isFocused ? activeFg : (isMin ? minFg : normalFg);

      canvas::write(canvas, x, dockY, " " + ind + label + " ", fg, bgColor_);

      // Active window underline
      if (isFocused) {
        for (int ch = x; ch < x + entryW; ++ch) {
          canvas::write(canvas, ch, dockY + 1, "\u2500", accent, bgColor_);
        }
      }

      x += entryW + 1;
    }
  }

  // Clock on the right
  std::time_t t = std::time(nullptr);
  auto tm_result = portable_localtime(t);
  char timeBuf[16];
  std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", &tm_result);
  canvas::write(canvas, sw - 7, dockY, " " + std::string(timeBuf) + " ", textColor_, bgColor_);
}

bool Dock::handleEvent(ftxui::Event event) {
  if (!visible()) return false;

  if (event.is_mouse()) {
    auto& mouse = event.mouse();
    int sh = ftxui::Terminal::Size().dimy;
    int dockY = sh - height_;

    if (mouse.y >= dockY && mouse.y < dockY + height_ &&
        mouse.button == ftxui::Mouse::Left &&
        mouse.motion == ftxui::Mouse::Pressed) {
      // Start button
      if (mouse.x >= 1 && mouse.x <= 9) {
        if (onStartClick) onStartClick();
        flash_ = 2;
        return true;
      }

      // Window entries
      if (wm_) {
        auto wins = wm_->windows();
        int x = 14;
        int clockW = 8;
        int sw = ftxui::Terminal::Size().dimx;
        int maxX = sw - 2 - clockW;

        for (size_t i = 0; i < wins.size(); ++i) {
          auto* win = wins[i];
          std::string label = win->title();
          if (static_cast<int>(label.size()) > 14)
            label = label.substr(0, 13) + "\u2026";
          int entryW = static_cast<int>(label.size()) + 3;

          if (x + entryW + 1 > maxX) break;

          if (mouse.x >= x && mouse.x < x + entryW) {
            if (win->isMinimized())
              win->restore();
            wm_->focusWindow(win);
            return true;
          }
          x += entryW + 1;
        }
      }
    }
  }

  return false;
}
