#include "Dock.h"
#include "WindowManager.h"
#include "WindowFrame.h"
#include "CanvasHelpers.h"
#include "core/ColorUtils.h"
#include <algorithm>
#include <ctime>
#include <ftxui/screen/terminal.hpp>

#ifdef _WIN32
#define localtime_r(a, b) localtime_s(b, a)
#endif

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

  auto accentColor = ftxui::Color::RGB(233, 69, 96);
  auto runningColor = ftxui::Color::RGB(0, 255, 128);

  int screenW = canvas.width() / 2;
  int screenH = canvas.height() / 4;

  int dockY = screenH - height_;

  canvas::fill(canvas, 0, dockY, screenW, height_, bgColor_);

  if (flash_ > 0) {
    canvas::write(canvas, 1, dockY, " [Start] ", bgColor_, accentColor);
    flash_--;
  } else {
    canvas::write(canvas, 1, dockY, " [Start] ", accentColor, bgColor_);
  }
  canvas::write(canvas, 11, dockY, "\u2502", textColor_, bgColor_);

  int clockWidth = (screenW > 20) ? 8 : 0;
  int maxAppX = screenW - 6 - clockWidth;

  int x = 14;
  bool truncated = false;
  for (const auto& app : apps_) {
    int nextX = x + static_cast<int>(app.name.size()) + 3;
    if (nextX >= maxAppX) { truncated = true; break; }
    auto fg = app.is_running ? runningColor : textColor_;
    canvas::write(canvas, x, dockY, " " + app.name + " ", fg, bgColor_);
    x = nextX;
  }
  if (truncated && x + 2 <= maxAppX) {
    canvas::write(canvas, x, dockY, ">>", textColor_, bgColor_);
  }

  std::time_t t = std::time(nullptr);
  std::tm tm_result;
  localtime_r(&t, &tm_result);
  char timeBuf[16];
  std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", &tm_result);

  if (screenW > 20) {
    canvas::write(canvas, screenW - 6, dockY, std::string(" ") + timeBuf + " ",
      textColor_, bgColor_);
  }
}

bool Dock::handleEvent(ftxui::Event event) {
  if (!visible()) return false;

  if (event.is_mouse()) {
    auto& mouse = event.mouse();
    int screenH = ftxui::Terminal::Size().dimy;
    int mouseY = screenH - height_;

    if (mouse.y >= mouseY && mouse.y < mouseY + height_ &&
        mouse.button == ftxui::Mouse::Left &&
        mouse.motion == ftxui::Mouse::Pressed) {
      if (mouse.x >= 1 && mouse.x <= 10) {
        if (onStartClick) onStartClick();
        flash_ = 2;
        return true;
      }
      int x = 14;
      for (int i = 0; i < static_cast<int>(apps_.size()); ++i) {
        int appW = static_cast<int>(apps_[static_cast<size_t>(i)].name.size()) + 2;
        if (mouse.x >= x && mouse.x < x + appW) {
          if (wm_) {
            for (auto* win : wm_->windows()) {
              if (win->title() == apps_[static_cast<size_t>(i)].name) {
                if (win->isMinimized())
                  win->restore();
                wm_->focusWindow(win);
                break;
              }
            }
          }
          return true;
        }
        x += appW + 3;
      }
    }
  }

  return false;
}
