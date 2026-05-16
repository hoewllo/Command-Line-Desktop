#pragma once
#include "core/Component.h"
#include <memory>
#include <ftxui/screen/color.hpp>

class WindowFrame : public Component {
public:
  WindowFrame(std::unique_ptr<Component> content);
  ~WindowFrame() override = default;

  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override;
  void setPos(int x, int y, int w, int h) override;

  Component* content() { return content_.get(); }
  const Component* content() const { return content_.get(); }
  void setClosable(bool c) { closable_ = c; }
  bool isClosable() const { return closable_; }

  void minimize() { minimized_ = true; }
  void restore() { minimized_ = false; }
  bool isMinimized() const { return minimized_; }

  void setBorderColor(const std::string& hex);
  void setTitleColor(const std::string& hex);

private:
  std::unique_ptr<Component> content_;
  bool closable_ = true;
  bool minimized_ = false;
  ftxui::Color borderColor_{ftxui::Color::RGB(15, 52, 96)};
  ftxui::Color titleColor_{ftxui::Color::RGB(233, 69, 96)};

  static constexpr int titlebar_height_ = 2;
  static constexpr int close_btn_w_ = 4;
  static constexpr int minimize_btn_w_ = 3;
};
