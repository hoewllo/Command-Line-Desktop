#include "WindowFrame.h"
#include "CanvasHelpers.h"
#include <algorithm>

WindowFrame::WindowFrame(std::unique_ptr<Component> content)
  : content_(std::move(content)) {}

std::string WindowFrame::title() const {
  return content_ ? content_->title() : "Window";
}

ftxui::Color WindowFrame::parseHex(const std::string& hex, ftxui::Color fallback) {
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

void WindowFrame::setBorderColor(const std::string& hex) {
  borderColor_ = parseHex(hex, borderColor_);
}

void WindowFrame::setTitleColor(const std::string& hex) {
  titleColor_ = parseHex(hex, titleColor_);
}

void WindowFrame::draw(ftxui::Canvas& canvas) {
  if (!visible_ || minimized_) return;

  auto titleFg = ftxui::Color::RGB(255, 255, 255);
  auto bg = ftxui::Color::RGB(22, 33, 62);

  int maxW = std::min(x_ + width_, canvas.width() / 2);
  int maxH = std::min(y_ + height_, canvas.height() / 4);

  canvas::fill(canvas, x_, y_, width_, titlebar_height_, titleColor_);
  canvas::fill(canvas, x_, y_ + titlebar_height_, width_, height_ - titlebar_height_, bg);

  for (int cx = x_; cx < maxW; ++cx) {
    canvas::write(canvas, cx, y_, "-", borderColor_, titleColor_);
    canvas::write(canvas, cx, y_ + height_ - 1, "-", borderColor_, bg);
  }
  for (int cy = y_; cy < maxH; ++cy) {
    int leftX = x_;
    int rightX = x_ + width_ - 1;
    if (rightX < canvas.width() / 2) {
      canvas::write(canvas, leftX, cy, "|", borderColor_,
        cy < y_ + titlebar_height_ ? titleColor_ : bg);
      canvas::write(canvas, rightX, cy, "|", borderColor_,
        cy < y_ + titlebar_height_ ? titleColor_ : bg);
    }
  }

  auto titleText = title();
  if ((int)titleText.size() > width_ - 6)
    titleText = titleText.substr(0, width_ - 9) + "...";
  canvas::write(canvas, x_ + 2, y_, " " + titleText + " ", titleFg, titleColor_);

  if (closable_) {
    canvas::write(canvas, x_ + width_ - 4, y_, " X ",
      ftxui::Color::RGB(255, 100, 100), titleColor_);
  }

  if (content_) {
    int contentX = x_ + 1;
    int contentY = y_ + titlebar_height_ + 1;
    int contentW = width_ - 3;
    int contentH = height_ - titlebar_height_ - 2;
    content_->setPos(contentX, contentY, contentW, contentH);
    content_->draw(canvas);
  }
}

bool WindowFrame::handleEvent(ftxui::Event event) {
  if (!visible_ || minimized_) return false;
  if (content_ && content_->handleEvent(event))
    return true;
  return false;
}
