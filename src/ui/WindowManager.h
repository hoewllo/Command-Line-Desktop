#pragma once
#include "core/Component.h"
#include <vector>
#include <memory>
#include <functional>

class WindowFrame;

class WindowManager : public Component {
public:
  WindowManager();

  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return "WindowManager"; }

  void addWindow(std::unique_ptr<WindowFrame> window);
  void removeWindow(WindowFrame* window);
  void closeFocused();
  void cycleFocus(bool reverse = false);
  void focusWindow(WindowFrame* window);

  WindowFrame* focusedWindow() const;
  std::vector<WindowFrame*> windows() const;

  std::function<void()> onWindowClosed;

  int windowCount() const { return static_cast<int>(windows_.size()); }

private:
  std::vector<std::unique_ptr<WindowFrame>> windows_;
  int focused_idx_ = -1;
  int prev_focused_idx_ = -1;
  bool dragging_ = false;
  int dragOffsetX_ = 0, dragOffsetY_ = 0;
  bool resizing_ = false;
  int resizeEdge_ = 0;
  WindowFrame* dragWindow_ = nullptr;

  void handleDrag(ftxui::Event event);
  void handleResize(ftxui::Event event);
  void trySnap(WindowFrame* win);

  static constexpr int resize_margin_ = 3;
  static constexpr int snap_margin_ = 3;
};
