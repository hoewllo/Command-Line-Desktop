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
  } catch (const std::filesystem::filesystem_error&) {}
  std::sort(entries_.begin(), entries_.end());
  if (path.has_parent_path() && fs::exists(path.parent_path())) {
    entries_.insert(entries_.begin(), path / "..");
  }
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
  if (!visible()) return;
  int list_x = x();
  int list_w = width();

  auto bg = ftxui::Color::RGB(22, 33, 62);
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto dirColor = ftxui::Color::RGB(100, 200, 255);
  auto selectedBg = ftxui::Color::RGB(233, 69, 96);

  canvas::fill(canvas, list_x, y(), list_w, height(), bg);

  std::string pathStr = current_path_.string();
  if (static_cast<int>(pathStr.size()) > list_w - 4)
    pathStr = "..." + pathStr.substr(pathStr.size() - static_cast<size_t>(list_w) + 7);
  canvas::write(canvas, list_x + 1, y(),
    " " + pathStr + " ",
    ftxui::Color::RGB(233, 69, 96), bg);

  int line_y = y() + 2;
  int maxVisible = height() - 3;
  for (int i = scroll_offset_; i < static_cast<int>(entries_.size()) && line_y - y() < maxVisible + 2; ++i) {
    auto entry = entries_[static_cast<size_t>(i)];
    auto name = entry.filename().string();
    if (static_cast<int>(name.size()) > list_w - 3)
      name = name.substr(0, static_cast<size_t>(list_w - 4)) + "~";

    auto fg = fs::is_directory(entry) ? dirColor : textColor;

    if (i == selected_idx_) {
      canvas::write(canvas, list_x + 1, line_y,
        "> " + name, ftxui::Color::White, selectedBg);
    } else {
      canvas::write(canvas, list_x + 1, line_y,
        "  " + name, fg, bg);
    }
    line_y++;
  }
}

bool FileManager::handleEvent(ftxui::Event event) {
  if (!visible()) return false;

  if (event == ftxui::Event::ArrowUp) {
    if (selected_idx_ > 0) selected_idx_--;
    if (selected_idx_ < scroll_offset_) scroll_offset_--;
    return true;
  }
  if (event == ftxui::Event::ArrowDown) {
    if (selected_idx_ < static_cast<int>(entries_.size()) - 1) {
      selected_idx_++;
      int visHeight = height() - 3;
      if (selected_idx_ >= scroll_offset_ + visHeight)
        scroll_offset_++;
    }
    return true;
  }
  if (event == ftxui::Event::Return) {
    if (selected_idx_ >= 0 && selected_idx_ < static_cast<int>(entries_.size())) {
      auto& entry = entries_[static_cast<size_t>(selected_idx_)];
      auto name = entry.filename().string();
      if (name == "..") {
        navigateTo(current_path_.parent_path().string());
      } else if (fs::is_directory(entry)) {
        navigateTo(entry.string());
      }
    }
    return true;
  }
  return false;
}
