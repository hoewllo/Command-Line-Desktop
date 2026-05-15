#pragma once
#include "core/Component.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>

class AppRunner : public Component {
public:
  AppRunner(const std::string& command, const std::string& appName);
  ~AppRunner();

  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return name_; }

  void start();
  void stop();
  bool isRunning() const { return running_; }

private:
  void readOutput();

  std::string command_;
  std::string name_;
  std::vector<std::string> output_lines_;
  std::thread worker_;
  std::atomic<bool> running_{false};
  int scroll_offset_ = 0;
};
