#include "Dock.h"
#include "WindowManager.h"
#include "CanvasHelpers.h"
#include <algorithm>
#include <ctime>

Dock::Dock() {}

void Dock::removeApp(const std::string& name) {
  apps_.erase(
    std::remove_if(apps_.begin(), apps_.end(),
      [&](const DockApp& a) { return a.name == name; }),
    apps_.end());
}

void Dock::draw(ftxui::Canvas& canvas) {
  if (!visible_) return;

  auto bg = ftxui::Color::RGB(22, 33, 62);
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto accentColor = ftxui::Color::RGB(233, 69, 96);
  auto runningColor = ftxui::Color::RGB(0, 255, 128);

  int screenW = canvas.width() / 2;
  int screenH = canvas.height() / 4;

  int dockY = screenH - height_;
  setPos(0, dockY, screenW, height_);

  canvas::fill(canvas, 0, dockY, screenW, height_, bg);

  canvas::write(canvas, 1, dockY, " [Start] ", accentColor, bg);
  canvas::write(canvas, 11, dockY, "\u2502", textColor, bg);

  int x = 14;
  for (const auto& app : apps_) {
    if (x + (int)app.name.size() + 3 >= screenW) break;
    auto fg = app.is_running ? runningColor : textColor;
    canvas::write(canvas, x, dockY, " " + app.name + " ", fg, bg);
    x += app.name.size() + 3;
  }

  std::time_t t = std::time(nullptr);
  std::tm* tm = std::localtime(&t);
  char timeBuf[16];
  std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", tm);

  if (screenW > 20) {
    canvas::write(canvas, screenW - 6, dockY, std::string(" ") + timeBuf + " ",
      textColor, bg);
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
        return true;
      }
      for (int i = 0; i < (int)apps_.size(); ++i) {
        int appX = 14;
        for (int j = 0; j < i; ++j)
          appX += apps_[j].name.size() + 3;
        if (mouse.x >= appX && mouse.x < appX + (int)apps_[i].name.size() + 2) {
          return true;
        }
      }
    }
  }

  return false;
}
