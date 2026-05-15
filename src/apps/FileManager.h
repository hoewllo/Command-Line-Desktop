#pragma once
#include "core/Component.h"
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class FileManager : public Component {
public:
  FileManager();
  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return "File Manager"; }

  void navigateTo(const std::string& path);
  void refresh();

private:
  void drawDirTree(ftxui::Canvas& canvas, int x, int y, int w, int h);
  void drawFileList(ftxui::Canvas& canvas, int x, int y, int w, int h);
  void loadDirectory(const fs::path& path);

  fs::path current_path_;
  std::vector<fs::path> entries_;
  std::vector<fs::path> dir_tree_;
  int tree_width_ = 20;
  int scroll_offset_ = 0;
  int selected_idx_ = 0;
};
