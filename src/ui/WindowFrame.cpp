#include "WindowFrame.h"
#include "CanvasHelpers.h"
#include "core/ColorUtils.h"
#include <algorithm>

WindowFrame::WindowFrame(std::unique_ptr<Component> content)
  : content_(std::move(content)) {}

std::string WindowFrame::title() const {
  return content_ ? content_->title() : "Window";
}

void WindowFrame::setPos(int x, int y, int w, int h) {
  Component::setPos(x, y, w, h);
  if (content_) {
    int contentX = x + 1;
    int contentY = y + titlebar_height_ + 1;
    int contentW = w - 3;
    int contentH = h - titlebar_height_ - 2;
    content_->setPos(contentX, contentY, contentW, contentH);
  }
}

void WindowFrame::setBorderColor(const std::string& hex) {
  borderColor_ = color::parseHex(hex, borderColor_);
}

void WindowFrame::setTitleColor(const std::string& hex) {
  titleColor_ = color::parseHex(hex, titleColor_);
}

void WindowFrame::draw(ftxui::Canvas& canvas) {
  if (!visible() || minimized_) return;

  auto titleFg = ftxui::Color::RGB(255, 255, 255);
  auto bg = ftxui::Color::RGB(22, 33, 62);

  canvas::fill(canvas, x(), y(), width(), titlebar_height_, titleColor_);
  canvas::fill(canvas, x(), y() + titlebar_height_, width(), height() - titlebar_height_, bg);

  int maxCx = std::min(x() + width(), canvas.width() / 2);
  for (int cx = x(); cx < maxCx; ++cx) {
    canvas::write(canvas, cx, y(), "\u2500", borderColor_, titleColor_);
    canvas::write(canvas, cx, y() + height() - 1, "\u2500", borderColor_, bg);
  }

  int maxH = y() + height();
  for (int cy = y(); cy < maxH; ++cy) {
    int leftX = x();
    int rightX = x() + width() - 1;
    if (rightX < canvas.width() / 2) {
      canvas::write(canvas, leftX, cy, "\u2502", borderColor_,
        cy < y() + titlebar_height_ ? titleColor_ : bg);
      canvas::write(canvas, rightX, cy, "\u2502", borderColor_,
        cy < y() + titlebar_height_ ? titleColor_ : bg);
    }
  }

  canvas::write(canvas, x(), y(), "\u250c", borderColor_, titleColor_);
  canvas::write(canvas, x() + width() - 1, y(), "\u2510", borderColor_, titleColor_);
  canvas::write(canvas, x(), y() + height() - 1, "\u2514", borderColor_, bg);
  canvas::write(canvas, x() + width() - 1, y() + height() - 1, "\u2518", borderColor_, bg);

  auto titleText = title();
  if (static_cast<int>(titleText.size()) > width() - 6)
    titleText = titleText.substr(0, static_cast<size_t>(width() - 9)) + "...";
  canvas::write(canvas, x() + 2, y(), " " + titleText + " ", titleFg, titleColor_);

  if (closable_) {
    canvas::write(canvas, x() + width() - close_btn_w_ - minimize_btn_w_, y(), " _ ",
      ftxui::Color::RGB(255, 200, 100), titleColor_);
    canvas::write(canvas, x() + width() - close_btn_w_, y(), " X ",
      ftxui::Color::RGB(255, 100, 100), titleColor_);
  }

  if (content_) {
    content_->draw(canvas);
  }
}

bool WindowFrame::handleEvent(ftxui::Event event) {
  if (!visible() || minimized_) return false;
  if (content_ && content_->handleEvent(event))
    return true;
  return false;
}
