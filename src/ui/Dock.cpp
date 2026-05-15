#include "Dock.h"
#include "WindowManager.h"
#include "CanvasHelpers.h"
#include <algorithm>
#include <ctime>

Dock::Dock() {}

ftxui::Color Dock::parseHex(const std::string& hex, ftxui::Color fallback) {
  std::string h = hex;
  if (!h.empty() && h[0] == '#') h = h.substr(1);
  if (h.size() >= 6) {
    try {
      auto r = std::stoi(h.substr(0, 2), nullptr, 16);
      auto g = std::stoi(h.substr(2, 2), nullptr, 16);
      auto b = std::stoi(h.substr(4, 2), nullptr, 16);
      return ftxui::Color::RGB(r, g, b);
    } catch (...) {}
  }
  return fallback;
}

void Dock::setBackgroundColor(const std::string& hex) {
  bgColor_ = parseHex(hex, bgColor_);
}

void Dock::setTextColor(const std::string& hex) {
  textColor_ = parseHex(hex, textColor_);
}

void Dock::removeApp(const std::string& name) {
  apps_.erase(
    std::remove_if(apps_.begin(), apps_.end(),
      [&](const DockApp& a) { return a.name == name; }),
    apps_.end());
}

void Dock::draw(ftxui::Canvas& canvas) {
  if (!visible_) return;

  auto accentColor = ftxui::Color::RGB(233, 69, 96);
  auto runningColor = ftxui::Color::RGB(0, 255, 128);

  int screenW = canvas.width() / 2;
  int screenH = canvas.height() / 4;

  int dockY = screenH - height_;
  setPos(0, dockY, screenW, height_);

  canvas::fill(canvas, 0, dockY, screenW, height_, bgColor_);

  if (flash_ > 0) {
    canvas::write(canvas, 1, dockY, " [Start] ", bgColor_, accentColor);
    flash_--;
  } else {
    canvas::write(canvas, 1, dockY, " [Start] ", accentColor, bgColor_);
  }
  canvas::write(canvas, 11, dockY, "\u2502", textColor_, bgColor_);

  int x = 14;
  for (const auto& app : apps_) {
    if (x + (int)app.name.size() + 3 >= screenW) break;
    auto fg = app.is_running ? runningColor : textColor_;
    canvas::write(canvas, x, dockY, " " + app.name + " ", fg, bgColor_);
    x += app.name.size() + 3;
  }

  std::time_t t = std::time(nullptr);
  std::tm* tm = std::localtime(&t);
  char timeBuf[16];
  std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", tm);

  if (screenW > 20) {
    canvas::write(canvas, screenW - 6, dockY, std::string(" ") + timeBuf + " ",
      textColor_, bgColor_);
  }
}

bool Dock::handleEvent(ftxui::Event event) {
  if (!visible_) return false;

  if (event.is_mouse()) {
    auto& mouse = event.mouse();
    int screenH = ftxui::Terminal::Size().dimy;
    int screenW = ftxui::Terminal::Size().dimx;

    if (mouse.y >= screenH - height_ && mouse.button == ftxui::Mouse::Left &&
        mouse.motion == ftxui::Mouse::Pressed) {
      if (mouse.x >= 1 && mouse.x <= 10) {
        if (onStartClick) onStartClick();
        flash_ = 2;
        return true;
      }
      for (int i = 0; i < (int)apps_.size(); ++i) {
        int appX = 14;
        for (int j = 0; j < i; ++j)
          appX += apps_[j].name.size() + 3;
        if (mouse.x >= appX && mouse.x < appX + (int)apps_[i].name.size() + 2)
          return true;
      }
    }
  }

  return false;
}
