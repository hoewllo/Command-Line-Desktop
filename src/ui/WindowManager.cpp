#include "WindowManager.h"
#include "WindowFrame.h"
#include <algorithm>
#include <ftxui/screen/terminal.hpp>

WindowManager::WindowManager() {}

void WindowManager::addWindow(std::unique_ptr<WindowFrame> window) {
  if (!window) return;

  auto dim = ftxui::Terminal::Size();
  int w = std::min(window->width() > 0 ? window->width() : 60, dim.dimx - 10);
  int h = std::min(window->height() > 0 ? window->height() : 30, dim.dimy - 10);
  int x = (dim.dimx - w) / 2;
  int y = (dim.dimy - h) / 4;
  if (focused_idx_ >= 0 && focused_idx_ < static_cast<int>(windows_.size())) {
    x = windows_[static_cast<size_t>(focused_idx_)]->x() + 3;
    y = windows_[static_cast<size_t>(focused_idx_)]->y() + 2;
  }
  x = std::max(0, std::min(x, dim.dimx - w - 5));
  y = std::max(0, std::min(y, dim.dimy - h - 5));

  window->setPos(x, y, w, h);
  window->setFocused(true);

  for (auto& win : windows_) win->setFocused(false);
  windows_.push_back(std::move(window));
  focused_idx_ = static_cast<int>(windows_.size()) - 1;
}

void WindowManager::removeWindow(WindowFrame* window) {
  if (dragWindow_ == window) {
    dragging_ = false;
    resizing_ = false;
    dragWindow_ = nullptr;
  }
  for (int i = 0; i < static_cast<int>(windows_.size()); ++i) {
    if (windows_[static_cast<size_t>(i)].get() == window) {
      windows_.erase(windows_.begin() + i);
      if (focused_idx_ == i) {
        focused_idx_ = (prev_focused_idx_ >= 0 && prev_focused_idx_ < static_cast<int>(windows_.size()))
          ? prev_focused_idx_ : static_cast<int>(windows_.size()) - 1;
      } else if (focused_idx_ > i) {
        focused_idx_--;
      }
      prev_focused_idx_ = -1;
      if (focused_idx_ >= 0 && focused_idx_ < static_cast<int>(windows_.size()))
        windows_[static_cast<size_t>(focused_idx_)]->setFocused(true);
      return;
    }
  }
}

void WindowManager::closeFocused() {
  if (focused_idx_ >= 0 && focused_idx_ < static_cast<int>(windows_.size())) {
    if (dragWindow_ == windows_[static_cast<size_t>(focused_idx_)].get()) {
      dragging_ = false;
      resizing_ = false;
      dragWindow_ = nullptr;
    }
    windows_.erase(windows_.begin() + focused_idx_);
    focused_idx_ = (prev_focused_idx_ >= 0 && prev_focused_idx_ < static_cast<int>(windows_.size()))
      ? prev_focused_idx_ : static_cast<int>(windows_.size()) - 1;
    prev_focused_idx_ = -1;
    if (focused_idx_ >= 0 && focused_idx_ < static_cast<int>(windows_.size()))
      windows_[static_cast<size_t>(focused_idx_)]->setFocused(true);
  }
}

void WindowManager::cycleFocus(bool reverse) {
  if (windows_.empty()) return;
  prev_focused_idx_ = focused_idx_;
  if (focused_idx_ >= 0)
    windows_[static_cast<size_t>(focused_idx_)]->setFocused(false);
  if (reverse) {
    focused_idx_ = static_cast<int>((static_cast<size_t>(focused_idx_) + windows_.size() - 1) % windows_.size());
  } else {
    focused_idx_ = static_cast<int>((static_cast<size_t>(focused_idx_) + 1) % windows_.size());
  }
  windows_[static_cast<size_t>(focused_idx_)]->setFocused(true);
}

void WindowManager::focusWindow(WindowFrame* window) {
  prev_focused_idx_ = focused_idx_;
  for (int i = 0; i < static_cast<int>(windows_.size()); ++i) {
    windows_[static_cast<size_t>(i)]->setFocused(windows_[static_cast<size_t>(i)].get() == window);
    if (windows_[static_cast<size_t>(i)].get() == window) focused_idx_ = i;
  }
}

WindowFrame* WindowManager::focusedWindow() const {
  if (focused_idx_ >= 0 && focused_idx_ < static_cast<int>(windows_.size()))
    return windows_[static_cast<size_t>(focused_idx_)].get();
  return nullptr;
}

std::vector<WindowFrame*> WindowManager::windows() const {
  std::vector<WindowFrame*> result;
  for (auto& win : windows_) result.push_back(win.get());
  return result;
}

void WindowManager::draw(ftxui::Canvas& canvas) {
  if (!visible() || windows_.empty()) return;

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
    newX = std::max(0, std::min(newX, dim.dimx - dragWindow_->width()));
    newY = std::max(0, std::min(newY, dim.dimy - dragWindow_->height()));
    dragWindow_->setX(newX);
    dragWindow_->setY(newY);
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
    int newW = mouse.x - dragWindow_->x() + 1;
    int newH = mouse.y - dragWindow_->y() + 1;
    dragWindow_->setWidth(std::max(20, std::min(newW, dim.dimx - dragWindow_->x())));
    dragWindow_->setHeight(std::max(10, std::min(newH, dim.dimy - dragWindow_->y())));
  }
}

bool WindowManager::handleEvent(ftxui::Event event) {
  if (!visible()) return false;

  if (resizing_ && event.is_mouse()) { handleResize(event); return true; }
  if (dragging_ && event.is_mouse()) { handleDrag(event); return true; }

  if (event == ftxui::Event::Tab) {
    if (windows_.size() > 1) {
      cycleFocus(false);
      return true;
    }
  }
  if (event == ftxui::Event::TabReverse) {
    if (windows_.size() > 1) {
      cycleFocus(true);
      return true;
    }
  }

  if (event.is_mouse()) {
    auto& mouse = event.mouse();

    if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
      for (int i = static_cast<int>(windows_.size()) - 1; i >= 0; --i) {
        auto* win = windows_[static_cast<size_t>(i)].get();
        if (mouse.x >= win->x() && mouse.x < win->x() + win->width() &&
            mouse.y >= win->y() && mouse.y < win->y() + win->height()) {

          for (auto& w : windows_) w->setFocused(false);
          win->setFocused(true);
          focused_idx_ = i;

          bool onX = (mouse.y >= win->y() && mouse.y < win->y() + 2 &&
                      mouse.x >= win->x() + win->width() - close_btn_w_ &&
                      mouse.x < win->x() + win->width());
          if (onX) {
            closeFocused();
            return true;
          }

          bool onMin = (mouse.y >= win->y() && mouse.y < win->y() + 2 &&
                        mouse.x >= win->x() + win->width() - close_btn_w_ - minimize_btn_w_ &&
                        mouse.x < win->x() + win->width() - close_btn_w_);
          if (onMin) {
            win->minimize();
            return true;
          }

          if (mouse.y >= win->y() && mouse.y < win->y() + 2 &&
              mouse.x < win->x() + win->width() - close_btn_w_ - minimize_btn_w_) {
            dragging_ = true;
            dragWindow_ = win;
            dragOffsetX_ = mouse.x - win->x();
            dragOffsetY_ = mouse.y - win->y();
          }

          bool inTitlebar = mouse.y >= win->y() && mouse.y < win->y() + 2;
          if (!inTitlebar && (
              mouse.x >= win->x() + win->width() - 3 ||
              mouse.x <= win->x() + 3 ||
              mouse.y >= win->y() + win->height() - 3 ||
              mouse.y <= win->y() + 3)) {
            resizing_ = true;
            dragWindow_ = win;
          }

          return true;
        }
      }
    }
  }

  if (focused_idx_ >= 0 && focused_idx_ < static_cast<int>(windows_.size())) {
    if (windows_[static_cast<size_t>(focused_idx_)]->handleEvent(event))
      return true;
  }

  return false;
}
