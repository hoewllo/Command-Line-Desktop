#pragma once
#include <memory>
#include <ftxui/component/screen_interactive.hpp>

class Desktop;

class EventLoop {
public:
  EventLoop();
  void run(std::shared_ptr<Desktop> desktop);
  void quit();

  ftxui::ScreenInteractive& screen() { return screen_; }
  const ftxui::ScreenInteractive& screen() const { return screen_; }

private:
  ftxui::ScreenInteractive screen_;
};
