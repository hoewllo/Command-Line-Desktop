#pragma once
#include "core/Component.h"
#include <memory>

class WindowFrame : public Component {
public:
  WindowFrame(std::unique_ptr<Component> content);
  ~WindowFrame() override = default;

  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override;

  Component* content() { return content_.get(); }
  const Component* content() const { return content_.get(); }
  void setClosable(bool c) { closable_ = c; }
  bool isClosable() const { return closable_; }

  void minimize() { minimized_ = true; }
  void restore() { minimized_ = false; }
  bool isMinimized() const { return minimized_; }

private:
  std::unique_ptr<Component> content_;
  bool closable_ = true;
  bool minimized_ = false;

  static constexpr int titlebar_height_ = 2;
};
