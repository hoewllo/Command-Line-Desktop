#include "FileManager.h"
#include "ui/CanvasHelpers.h"
#include <algorithm>
#include <cctype>

FileManager::FileManager() {
  current_path_ = fs::current_path();
  loadDirectory(current_path_);
}

void FileManager::loadDirectory(const fs::path& path) {
  entries_.clear();
  try {
    for (const auto& entry : fs::directory_iterator(path)) {
      entries_.push_back(entry.path());
    }
  } catch (...) {}
  std::sort(entries_.begin(), entries_.end());
}

void FileManager::refresh() {
  loadDirectory(current_path_);
}

void FileManager::navigateTo(const std::string& path) {
  fs::path newPath(path);
  if (fs::exists(newPath) && fs::is_directory(newPath)) {
    current_path_ = newPath;
    loadDirectory(current_path_);
    scroll_offset_ = 0;
    selected_idx_ = 0;
  }
}

void FileManager::draw(ftxui::Canvas& canvas) {
  if (!visible_) return;
  int list_x = x_;
  int list_w = width_;

  auto borderColor = ftxui::Color::RGB(15, 52, 96);
  auto bg = ftxui::Color::RGB(22, 33, 62);
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto dirColor = ftxui::Color::RGB(100, 200, 255);
  auto selectedBg = ftxui::Color::RGB(233, 69, 96);

  canvas::fill(canvas, list_x, y_, list_w, height_, bg);

  canvas::write(canvas, list_x + 1, y_,
    " " + current_path_.filename().string() + " ",
    ftxui::Color::RGB(233, 69, 96), bg);

  int line_y = y_ + 2;
  int maxVisible = height_ - 3;
  for (int i = scroll_offset_; i < (int)entries_.size() && line_y - y_ < maxVisible + 2; ++i) {
    auto entry = entries_[i];
    auto name = entry.filename().string();
    if ((int)name.size() > list_w - 3)
      name = name.substr(0, list_w - 4) + "~";

    auto fg = fs::is_directory(entry) ? dirColor : textColor;
    auto lineBg = (i == selected_idx_) ? selectedBg : bg;

    if (i == selected_idx_) {
      canvas::write(canvas, list_x + 1, line_y,
        "> " + name, ftxui::Color::White, selectedBg);
    } else {
      std::string prefix = fs::is_directory(entry) ? "D " : "  ";
      canvas::write(canvas, list_x + 1, line_y,
        "  " + name, fg, bg);
    }
    line_y++;
  }
}

bool FileManager::handleEvent(ftxui::Event event) {
  if (!visible_) return false;

  if (event == ftxui::Event::ArrowUp) {
    if (selected_idx_ > 0) selected_idx_--;
    if (selected_idx_ < scroll_offset_) scroll_offset_--;
    return true;
  }
  if (event == ftxui::Event::ArrowDown) {
    if (selected_idx_ < (int)entries_.size() - 1) {
      selected_idx_++;
      int visHeight = height_ - 3;
      if (selected_idx_ >= scroll_offset_ + visHeight)
        scroll_offset_++;
    }
    return true;
  }
  if (event == ftxui::Event::Return) {
    if (selected_idx_ >= 0 && selected_idx_ < (int)entries_.size()) {
      auto& entry = entries_[selected_idx_];
      if (fs::is_directory(entry)) {
        navigateTo(entry.string());
      }
    }
    return true;
  }
  return false;
}
