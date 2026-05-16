#include "Panel.h"
#include "WindowManager.h"
#include "WindowFrame.h"
#include "CanvasHelpers.h"
#include "core/ColorUtils.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <string>
#include <filesystem>
#include <ftxui/screen/terminal.hpp>

namespace fs = std::filesystem;

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

std::string readSysFile(const std::string& path) {
  std::ifstream f(path);
  std::string line;
  if (std::getline(f, line)) return line;
  return "";
}
}

// ====== StartSegment ======
void StartSegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)h;
  if (flash_ > 0) {
    canvas::write(c, x, y, " [Start] ", bg_, accent_);
    flash_--;
  } else {
    canvas::write(c, x, y, " [Start] ", accent_, bg_);
  }
}

bool StartSegment::handleEvent(ftxui::Event event) {
  if (!event.is_mouse()) return false;
  auto& mouse = event.mouse();
  if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
    if (mouse.x >= 0 && mouse.x < 10 && onClick) {
      onClick();
      flash_ = 10;
      return true;
    }
  }
  return false;
}

// ====== TaskSegment ======
void TaskSegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)h;
  if (!wm_) return;

  auto activeFg = ftxui::Color::RGB(255, 255, 255);
  auto normalFg = ftxui::Color::RGB(200, 200, 220);
  auto minFg = ftxui::Color::RGB(100, 100, 120);
  auto accent = ftxui::Color::RGB(233, 69, 96);
  auto bg = ftxui::Color::RGB(22, 33, 62);

  auto wins = wm_->windows();
  int curX = x;
  int maxX = x + w;

  for (size_t i = 0; i < wins.size(); ++i) {
    auto* win = wins[i];
    std::string label = win->title();
    if (static_cast<int>(label.size()) > 14)
      label = label.substr(0, 12) + "\u2026";
    int entryW = static_cast<int>(label.size()) + 3;

    if (curX + entryW > maxX) break;

    bool isFocused = win->focused();
    bool isMin = win->isMinimized();

    std::string ind;
    if (isFocused) ind = "\u25C9";
    else if (isMin) ind = "\u25CB";
    else ind = "\u25CF";

    auto fg = isFocused ? activeFg : (isMin ? minFg : normalFg);
    canvas::write(c, curX, y, " " + ind + label + " ", fg, bg);

    if (isFocused && y + 1 < c.height() / 4) {
      int len = entryW;
      for (int ch = 0; ch < len; ++ch)
        canvas::write(c, curX + ch, y + 1, "\u2500", accent, bg);
    }

    curX += entryW + 1;
  }
}

bool TaskSegment::handleEvent(ftxui::Event event) {
  if (!event.is_mouse() || !wm_) return false;
  auto& mouse = event.mouse();
  if (mouse.motion != ftxui::Mouse::Pressed || mouse.button != ftxui::Mouse::Left)
    return false;

  auto wins = wm_->windows();
  int curX = 0;

  for (size_t i = 0; i < wins.size(); ++i) {
    auto* win = wins[i];
    std::string label = win->title();
    if (static_cast<int>(label.size()) > 14)
      label = label.substr(0, 12) + "\u2026";
    int entryW = static_cast<int>(label.size()) + 3;

    if (mouse.x >= curX && mouse.x < curX + entryW) {
      if (win->isMinimized()) win->restore();
      wm_->focusWindow(win);
      return true;
    }
    curX += entryW + 1;
  }
  return false;
}

// ====== ClockSegment (enhanced with date) ======
void ClockSegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)h;
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto dateColor = ftxui::Color::RGB(150, 150, 180);
  auto bg = ftxui::Color::RGB(22, 33, 62);

  std::time_t t = std::time(nullptr);
  auto tm_result = portable_localtime(t);

  char timeBuf[16];
  std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", &tm_result);

  // Alternate between showing just time and time+date every 30 frames
  tick_++;
  bool showDate = (tick_ / 30) % 2 == 1;

  if (showDate && w >= 16) {
    char dateBuf[12];
    std::strftime(dateBuf, sizeof(dateBuf), "%d %b", &tm_result);
    std::string full = std::string(timeBuf) + " " + std::string(dateBuf);
    if (static_cast<int>(full.size()) + 2 <= w)
      canvas::write(c, x, y, " " + full + " ", textColor, bg);
    else
      canvas::write(c, x, y, " " + std::string(timeBuf) + " ", textColor, bg);
  } else {
    canvas::write(c, x, y, " " + std::string(timeBuf) + " ", textColor, bg);
  }
}

// ====== WorkspaceSegment ======
void WorkspaceSegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)h;
  auto textColor = ftxui::Color::RGB(200, 200, 220);
  auto bg = ftxui::Color::RGB(22, 33, 62);
  auto highlight = ftxui::Color::RGB(233, 69, 96);

  std::string label = " " + std::to_string(cur_ + 1) + "/" + std::to_string(total_) + " ";
  if (static_cast<int>(label.size()) > w) {
    label = std::to_string(cur_ + 1);
    if (static_cast<int>(label.size()) + 1 > w) label = "";
  }
  canvas::write(c, x, y, label, highlight, bg);
}

// ====== TraySegment (battery + network) ======
std::string TraySegment::batteryIcon() {
  int pct = batteryPercent();
  if (pct < 0) return "";
  if (pct >= 90) return "\u2588";
  if (pct >= 70) return "\u2589";
  if (pct >= 50) return "\u2588";
  if (pct >= 30) return "\u2586";
  if (pct >= 10) return "\u2584";
  return "\u2581";
}

int TraySegment::batteryPercent() {
  try {
    std::string now = readSysFile("/sys/class/power_supply/BAT0/energy_now");
    std::string full = readSysFile("/sys/class/power_supply/BAT0/energy_full");
    if (now.empty() || full.empty()) {
      now = readSysFile("/sys/class/power_supply/BAT0/charge_now");
      full = readSysFile("/sys/class/power_supply/BAT0/charge_full");
    }
    if (now.empty() || full.empty()) return -1;
    double n = std::stod(now);
    double f = std::stod(full);
    if (f > 0) return static_cast<int>(n * 100.0 / f);
  } catch (...) {}
  return -1;
}

bool TraySegment::hasNetwork() {
  try {
    for (auto& p : fs::directory_iterator("/sys/class/net")) {
      std::string name = p.path().filename().string();
      if (name == "lo") continue;
      std::string state = readSysFile("/sys/class/net/" + name + "/operstate");
      if (state == "up" || state == "UP\n") return true;
    }
  } catch (...) {}
  return false;
}

void TraySegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)h;
  auto bg = ftxui::Color::RGB(22, 33, 62);
  auto green = ftxui::Color::RGB(100, 200, 100);
  auto white = ftxui::Color::RGB(200, 200, 220);
  auto yellow = ftxui::Color::RGB(255, 200, 100);
  auto red = ftxui::Color::RGB(255, 100, 100);

  tick_++;
  int curX = x;
  int maxX = x + w;

  // Network indicator
  if (curX + 3 <= maxX) {
    bool net = hasNetwork();
    auto nFg = net ? green : red;
    std::string netIcon = net ? "\u25C8" : "\u25CB";
    canvas::write(c, curX, y, " " + netIcon + " ", nFg, bg);
    curX += 3;
  }

  // Battery indicator (only draw every other frame to avoid excessive reads)
  if (curX + 4 <= maxX && (tick_ % 6 < 3)) {
    int pct = batteryPercent();
    if (pct >= 0) {
      auto bFg = pct > 20 ? white : (pct > 10 ? yellow : red);
      std::string bIcon = batteryIcon();
      char pctStr[8];
      std::snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
      canvas::write(c, curX, y, " " + bIcon + " " + pctStr + " ", bFg, bg);
    }
  }
}

// ====== Panel ======
Panel::Panel() {
  start_.onClick = [this]() {
    if (onStartClick) onStartClick();
  };
}

void Panel::setWindowManager(WindowManager* wm) {
  task_.setWindowManager(wm);
}

void Panel::setBackgroundColor(const std::string& hex) {
  bgColor_ = color::parseHex(hex, bgColor_);
}

void Panel::setTextColor(const std::string& hex) {
  textColor_ = color::parseHex(hex, textColor_);
}

void Panel::draw(ftxui::Canvas& canvas) {
  if (!visible()) return;

  int sw = canvas.width() / 2;
  int sh = canvas.height() / 4;
  int panelY = sh - height_;

  // Panel background
  canvas::fill(canvas, 0, panelY, sw, height_, bgColor_);

  // Layout: Start(10) | sep(3) | Taskbar(flex) | Tray(8) | Workspace(7) | Clock(9)
  int curX = 0;
  int segH = height_;

  // Start button
  start_.draw(canvas, curX, panelY, 10, segH);
  curX += 10;

  // Separator
  canvas::write(canvas, curX, panelY, " \u2502 ", ftxui::Color::RGB(140, 140, 160), bgColor_);
  curX += 3;

  int trayW = 8;
  int workspaceW = std::min(9, std::max(7, (sw - curX - trayW) / 12));
  int clockW = std::min(16, std::max(9, (sw - curX - trayW) / 8));

  // Taskbar takes remaining space
  int taskW = sw - curX - trayW - workspaceW - clockW - 1;
  if (taskW > 5) {
    task_.draw(canvas, curX, panelY, taskW, segH);
    curX += taskW;
  }

  // Separator
  canvas::write(canvas, curX, panelY, " \u2502 ", ftxui::Color::RGB(140, 140, 160), bgColor_);
  curX += 3;

  // System tray
  tray_.draw(canvas, curX, panelY, trayW, segH);
  curX += trayW;

  // Workspace indicator
  ws_.draw(canvas, curX, panelY, workspaceW, segH);
  curX += workspaceW;

  // Clock
  clock_.draw(canvas, curX, panelY, clockW, segH);
}

bool Panel::handleEvent(ftxui::Event event) {
  if (!visible()) return false;
  if (!event.is_mouse()) return false;

  auto& mouse = event.mouse();
  int sw = ftxui::Terminal::Size().dimx;
  int sh = ftxui::Terminal::Size().dimy;
  int panelY = sh - height_;

  if (mouse.y < panelY || mouse.y >= panelY + height_)
    return false;
  if (mouse.motion != ftxui::Mouse::Pressed || mouse.button != ftxui::Mouse::Left)
    return false;

  int relX = mouse.x;

  // Start button
  if (relX < 10) return start_.handleEvent(event);

  // Taskbar (between Start+sep and Tray)
  int trayW = 8;
  int workspaceW = std::min(9, std::max(7, (sw - 13 - trayW) / 12));
  int clockW = std::min(16, std::max(9, (sw - 13 - trayW) / 8));
  int curX = 13;
  int taskW = sw - curX - trayW - workspaceW - clockW - 4;

  if (relX >= curX && relX < curX + taskW) return task_.handleEvent(event);

  return false;
}
