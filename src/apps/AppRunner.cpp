#include "AppRunner.h"
#include "ui/CanvasHelpers.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

AppRunner::AppRunner(const std::string& command, const std::string& appName)
  : command_(command), name_(appName) {
}

AppRunner::~AppRunner() {
  stop();
}

void AppRunner::start() {
  if (running_) return;
  running_ = true;
  worker_ = std::thread([this]() { readOutput(); });
}

void AppRunner::stop() {
  running_ = false;
  if (worker_.joinable()) worker_.join();
}

void AppRunner::readOutput() {
#ifdef _WIN32
  FILE* fp = POPEN(command_.c_str(), "r");
#else
  FILE* fp = POPEN(command_.c_str(), "r");
#endif
  if (!fp) {
    output_lines_.push_back("Failed to run: " + command_);
    running_ = false;
    return;
  }

  char buf[4096];
  std::string partial;
  while (running_ && fgets(buf, sizeof(buf), fp)) {
    partial += buf;
    size_t pos;
    while ((pos = partial.find('\n')) != std::string::npos) {
      auto line = partial.substr(0, pos);
      if ((int)line.size() > 200) line = line.substr(0, 200);
      output_lines_.push_back(line);
      if (output_lines_.size() > 1000)
        output_lines_.erase(output_lines_.begin());
      partial.erase(0, pos + 1);
    }
  }
  PCLOSE(fp);
  running_ = false;
}

void AppRunner::draw(ftxui::Canvas& canvas) {
  if (!visible_) return;

  auto bg = ftxui::Color::RGB(10, 10, 15);
  auto textColor = ftxui::Color::RGB(0, 255, 128);
  auto promptColor = ftxui::Color::RGB(255, 255, 255);

  canvas::fill(canvas, x_, y_, width_, height_, bg);

  canvas::write(canvas, x_ + 1, y_ + 1, "$ " + command_, promptColor, bg);

  int line_y = y_ + 3;
  int maxVisible = height_ - 4;
  for (int i = scroll_offset_;
       i < (int)output_lines_.size() && line_y < y_ + (int)output_lines_.size() - scroll_offset_ + 3 && line_y < y_ + height_ - 1;
       ++i) {
    if (line_y - y_ >= maxVisible + 3) break;
    auto& line = output_lines_[i];
    if ((int)line.size() > width_ - 2)
      line = line.substr(0, width_ - 4);
    canvas::write(canvas, x_ + 1, line_y, line, textColor, bg);
    line_y++;
  }

  if (!running_) {
    canvas::write(canvas, x_ + 1, y_ + height_ - 1,
      " [Process exited]", ftxui::Color::RGB(255, 100, 100), bg);
  }
}

bool AppRunner::handleEvent(ftxui::Event event) {
  if (!visible_) return false;

  if (event == ftxui::Event::ArrowUp) {
    if (scroll_offset_ > 0) scroll_offset_--;
    return true;
  }
  if (event == ftxui::Event::ArrowDown) {
    int visLines = height_ - 4;
    if (scroll_offset_ + visLines < (int)output_lines_.size())
      scroll_offset_++;
    return true;
  }
  return false;
}
