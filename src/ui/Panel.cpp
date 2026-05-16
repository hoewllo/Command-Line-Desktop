#include "Panel.h"
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
      // Move window to front if workspace supports it
      return true;
    }
    curX += entryW + 1;
  }
  return false;
}

// ====== ClockSegment ======
void ClockSegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)h;
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto bg = ftxui::Color::RGB(22, 33, 62);

  std::time_t t = std::time(nullptr);
  auto tm_result = portable_localtime(t);
  char timeBuf[16];
  std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", &tm_result);
  canvas::write(c, x, y, " " + std::string(timeBuf) + " ", textColor, bg);
}

// ====== WorkspaceSegment ======
void WorkspaceSegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)h;
  auto textColor = ftxui::Color::RGB(200, 200, 220);
  auto bg = ftxui::Color::RGB(22, 33, 62);
  auto highlight = ftxui::Color::RGB(233, 69, 96);

  std::string label = " " + std::to_string(cur_ + 1) + "/" + std::to_string(total_) + " ";
  // Ensure fits within w
  if (static_cast<int>(label.size()) > w) {
    label = std::to_string(cur_ + 1);
    if (static_cast<int>(label.size()) + 1 > w) label = "";
  }
  canvas::write(c, x, y, label, highlight, bg);
}

// ====== TraySegment ======
void TraySegment::draw(ftxui::Canvas& c, int x, int y, int w, int h) {
  (void)c; (void)x; (void)y; (void)w; (void)h;
  // Placeholder for future system tray icons (battery, network, etc.)
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

  // Layout: Start(10) | sep(3) | Taskbar(flex) | Workspace(7) | Clock(9)
  int curX = 0;
  int segH = height_;

  // Start button
  start_.draw(canvas, curX, panelY, 10, segH);
  curX += 10;

  // Separator
  canvas::write(canvas, curX, panelY, " \u2502 ", ftxui::Color::RGB(140, 140, 160), bgColor_);
  curX += 3;

  int workspaceW = std::min(9, std::max(7, (sw - curX) / 10));
  int clockW = 9;
  int trayW = 0;

  // Taskbar takes remaining space
  int taskW = sw - curX - workspaceW - clockW - trayW - 1;
  if (taskW > 5) {
    task_.draw(canvas, curX, panelY, taskW, segH);
    curX += taskW;
  }

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
  int sh = ftxui::Terminal::Size().dimy;
  int panelY = sh - height_;

  if (mouse.y < panelY || mouse.y >= panelY + height_)
    return false;
  if (mouse.motion != ftxui::Mouse::Pressed || mouse.button != ftxui::Mouse::Left)
    return false;

  int relX = mouse.x;

  // Try segments in layout order
  if (relX < 10) {
    return start_.handleEvent(event);
  }

  // Taskbar (flex between separator and workspace indicator)
  int sw = ftxui::Terminal::Size().dimx;
  int workspaceW = std::min(9, std::max(7, (sw - 13) / 10));
  int clockW = 9;
  int curX = 13;
  int taskW = sw - curX - workspaceW - clockW - 1;

  if (relX >= curX && relX < curX + taskW) {
    return task_.handleEvent(event);
  }

  return false;
}
