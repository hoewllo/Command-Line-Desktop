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

  void setPos(int x, int y, int w, int h) {
    x_ = x; y_ = y; width_ = w; height_ = h;
  }

  int x_ = 0;
  int y_ = 0;
  int width_ = 0;
  int height_ = 0;
  bool visible_ = true;
  bool focused_ = false;
};
