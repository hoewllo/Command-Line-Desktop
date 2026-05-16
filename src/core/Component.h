#pragma once
#include <string>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/component/event.hpp>

class Component {
public:
  Component() = default;
  virtual ~Component() = default;

  virtual void draw(ftxui::Canvas& canvas) = 0;
  virtual bool handleEvent(ftxui::Event event) = 0;
  virtual std::string title() const = 0;

  virtual void setPos(int x, int y, int w, int h) {
    x_ = x; y_ = y; width_ = w; height_ = h;
  }

  int x() const { return x_; }
  int y() const { return y_; }
  int width() const { return width_; }
  int height() const { return height_; }
  bool visible() const { return visible_; }
  bool focused() const { return focused_; }

  void setX(int x) { x_ = x; }
  void setY(int y) { y_ = y; }
  void setWidth(int w) { width_ = w; }
  void setHeight(int h) { height_ = h; }
  void setVisible(bool v) { visible_ = v; }
  void setFocused(bool f) { focused_ = f; }

private:
  int x_ = 0;
  int y_ = 0;
  int width_ = 0;
  int height_ = 0;
  bool visible_ = true;
  bool focused_ = false;
};
