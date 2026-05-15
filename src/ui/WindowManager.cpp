#include "WindowManager.h"
#include "WindowFrame.h"
#include <algorithm>
#include <ftxui/screen/terminal.hpp>

WindowManager::WindowManager() {}

void WindowManager::addWindow(std::unique_ptr<WindowFrame> window) {
  if (!window) return;

  auto dim = ftxui::Terminal::Size();
  int w = std::min(window->width_ > 0 ? window->width_ : 60, dim.dimx - 10);
  int h = std::min(window->height_ > 0 ? window->height_ : 30, dim.dimy - 10);
  int x = (dim.dimx - w) / 2;
  int y = (dim.dimy - h) / 4;
  if (focused_idx_ >= 0 && focused_idx_ < (int)windows_.size()) {
    x = windows_[focused_idx_]->x_ + 3;
    y = windows_[focused_idx_]->y_ + 2;
  }
  x = std::max(0, std::min(x, dim.dimx - w - 5));
  y = std::max(0, std::min(y, dim.dimy - h - 5));

  window->setPos(x, y, w, h);
  window->focused_ = true;

  for (auto& w : windows_) w->focused_ = false;
  windows_.push_back(std::move(window));
  focused_idx_ = (int)windows_.size() - 1;
}

void WindowManager::removeWindow(WindowFrame* window) {
  for (int i = 0; i < (int)windows_.size(); ++i) {
    if (windows_[i].get() == window) {
      windows_.erase(windows_.begin() + i);
      focused_idx_ = (int)windows_.size() - 1;
      if (focused_idx_ >= 0)
        windows_[focused_idx_]->focused_ = true;
      return;
    }
  }
}

void WindowManager::closeFocused() {
  if (focused_idx_ >= 0 && focused_idx_ < (int)windows_.size()) {
    windows_.erase(windows_.begin() + focused_idx_);
    focused_idx_ = (int)windows_.size() - 1;
    if (focused_idx_ >= 0)
      windows_[focused_idx_]->focused_ = true;
  }
}

void WindowManager::cycleFocus() {
  if (windows_.empty()) return;
  if (focused_idx_ >= 0)
    windows_[focused_idx_]->focused_ = false;
  focused_idx_ = (focused_idx_ + 1) % windows_.size();
  windows_[focused_idx_]->focused_ = true;
}

void WindowManager::focusWindow(WindowFrame* window) {
  for (int i = 0; i < (int)windows_.size(); ++i) {
    windows_[i]->focused_ = (windows_[i].get() == window);
    if (windows_[i].get() == window) focused_idx_ = i;
  }
}

WindowFrame* WindowManager::focusedWindow() {
  if (focused_idx_ >= 0 && focused_idx_ < (int)windows_.size())
    return windows_[focused_idx_].get();
  return nullptr;
}

std::vector<WindowFrame*> WindowManager::windows() {
  std::vector<WindowFrame*> result;
  for (auto& w : windows_) result.push_back(w.get());
  return result;
}

void WindowManager::draw(ftxui::Canvas& canvas) {
  if (!visible_ || windows_.empty()) return;

  for (auto& win : windows_) {
    win->draw(canvas);
  }
}

void WindowManager::handleDrag(ftxui::Event event) {
  if (!event.is_mouse()) return;
  auto& mouse = event.mouse();

  if (mouse.motion == ftxui::Mouse::Released) {
    dragging_ = false;
    dragWindow_ = nullptr;
    return;
  }

  if (dragging_ && dragWindow_) {
    auto dim = ftxui::Terminal::Size();
    int newX = mouse.x - dragOffsetX_;
    int newY = mouse.y - dragOffsetY_;
    newX = std::max(0, std::min(newX, dim.dimx - dragWindow_->width_));
    newY = std::max(0, std::min(newY, dim.dimy - dragWindow_->height_));
    dragWindow_->x_ = newX;
    dragWindow_->y_ = newY;
  }
}

void WindowManager::handleResize(ftxui::Event event) {
  if (!event.is_mouse()) return;
  auto& mouse = event.mouse();

  if (mouse.motion == ftxui::Mouse::Released) {
    resizing_ = false;
    dragWindow_ = nullptr;
    return;
  }

  if (resizing_ && dragWindow_) {
    auto dim = ftxui::Terminal::Size();
    int newW = mouse.x - dragWindow_->x_ + 1;
    int newH = mouse.y - dragWindow_->y_ + 1;
    dragWindow_->width_ = std::max(20, std::min(newW, dim.dimx - dragWindow_->x_));
    dragWindow_->height_ = std::max(10, std::min(newH, dim.dimy - dragWindow_->y_));
  }
}

bool WindowManager::handleEvent(ftxui::Event event) {
  if (!visible_) return false;

  if (resizing_) { handleResize(event); return true; }
  if (dragging_) { handleDrag(event); return true; }

  if (event == ftxui::Event::F4) {
    closeFocused();
    return true;
  }

  if (event == ftxui::Event::Tab || event == ftxui::Event::TabReverse) {
    if (windows_.size() > 1) {
      cycleFocus();
      return true;
    }
  }

  if (event.is_mouse()) {
    auto& mouse = event.mouse();

    if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
      for (int i = (int)windows_.size() - 1; i >= 0; --i) {
        auto* win = windows_[i].get();
        if (mouse.x >= win->x_ && mouse.x < win->x_ + win->width_ &&
            mouse.y >= win->y_ && mouse.y < win->y_ + win->height_) {

          for (auto& w : windows_) w->focused_ = false;
          win->focused_ = true;
          focused_idx_ = i;

          bool onX = (mouse.y >= win->y_ && mouse.y < win->y_ + 2 &&
                      mouse.x >= win->x_ + win->width_ - 4 &&
                      mouse.x < win->x_ + win->width_);
          if (onX) {
            closeFocused();
            return true;
          }

          if (mouse.y >= win->y_ && mouse.y < win->y_ + 2 &&
              mouse.x < win->x_ + win->width_ - 4) {
            dragging_ = true;
            dragWindow_ = win;
            dragOffsetX_ = mouse.x - win->x_;
            dragOffsetY_ = mouse.y - win->y_;
          }

          if (mouse.x >= win->x_ + win->width_ - 3 ||
              mouse.x <= win->x_ + 3 ||
              mouse.y >= win->y_ + win->height_ - 3 ||
              mouse.y <= win->y_ + 3) {
            resizing_ = true;
            dragWindow_ = win;
          }

          return true;
        }
      }
    }
  }

  if (focused_idx_ >= 0 && focused_idx_ < (int)windows_.size()) {
    if (windows_[focused_idx_]->handleEvent(event))
      return true;
  }

  return false;
}
