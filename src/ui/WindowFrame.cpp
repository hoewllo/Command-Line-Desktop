#include "WindowFrame.h"
#include "CanvasHelpers.h"
#include <algorithm>

WindowFrame::WindowFrame(std::unique_ptr<Component> content)
  : content_(std::move(content)) {
}

std::string WindowFrame::title() const {
  return content_ ? content_->title() : "Window";
}

void WindowFrame::draw(ftxui::Canvas& canvas) {
  if (!visible_ || minimized_) return;

  auto borderColor = ftxui::Color::RGB(15, 52, 96);
  auto titleBg = ftxui::Color::RGB(233, 69, 96);
  auto titleFg = ftxui::Color::RGB(255, 255, 255);
  auto bg = ftxui::Color::RGB(22, 33, 62);

  int maxW = std::min(x_ + width_, canvas.width() / 2);
  int maxH = std::min(y_ + height_, canvas.height() / 4);

  canvas::fill(canvas, x_, y_, width_, titlebar_height_, titleBg);
  canvas::fill(canvas, x_, y_ + titlebar_height_, width_, height_ - titlebar_height_, bg);

  for (int cx = x_; cx < maxW; ++cx) {
    canvas::write(canvas, cx, y_, "-", borderColor, titleBg);
    canvas::write(canvas, cx, y_ + height_ - 1, "-", borderColor, bg);
  }
  for (int cy = y_; cy < maxH; ++cy) {
    int leftX = x_;
    int rightX = x_ + width_ - 1;
    if (rightX < canvas.width() / 2) {
      canvas::write(canvas, leftX, cy, "|", borderColor,
        cy < y_ + titlebar_height_ ? titleBg : bg);
      canvas::write(canvas, rightX, cy, "|", borderColor,
        cy < y_ + titlebar_height_ ? titleBg : bg);
    }
  }

  auto titleText = title();
  if ((int)titleText.size() > width_ - 6) {
    titleText = titleText.substr(0, width_ - 9) + "...";
  }
  canvas::write(canvas, x_ + 2, y_, " " + titleText + " ", titleFg, titleBg);

  if (closable_) {
    canvas::write(canvas, x_ + width_ - 4, y_, " X ",
      ftxui::Color::RGB(255, 100, 100), titleBg);
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
