#pragma once
#include "core/Component.h"
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <ctime>
#include <functional>
#include <deque>

namespace fs = std::filesystem;

struct FileEntry {
  std::string name;
  std::string size_str;
  std::string date_str;
  std::string type_str;
  std::string perms;
  bool is_dir = false;
  bool is_link = false;
  uintmax_t size = 0;
  std::time_t write_time = 0;
};

struct SidebarItem {
  std::string label;
  fs::path path;
  std::string icon;
};

class FileManager : public Component {
public:
  FileManager();
  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return "File Manager"; }

private:
  fs::path current_path_;
  std::vector<fs::path> entries_;
  std::vector<FileEntry> file_entries_;
  int selected_ = 0;
  int scroll_offset_ = 0;
  int sort_col_ = 0;
  bool sort_asc_ = true;
  std::set<size_t> marked_;

  // Navigation history
  std::deque<fs::path> back_stack_;
  std::deque<fs::path> fwd_stack_;

  // Sidebar
  bool sidebar_visible_ = true;
  std::vector<SidebarItem> sidebar_items_;
  int sidebar_sel_ = -1;

  // Path bar editing
  bool path_editing_ = false;
  std::string path_buffer_;

  // Search/filter mode
  bool search_mode_ = false;
  std::string search_query_;
  int search_cursor_ = 0;

  // Context menu
  bool ctx_open_ = false;
  int ctx_x_ = 0, ctx_y_ = 0;
  int ctx_sel_ = 0;
  std::vector<std::string> ctx_items_;

  // Command prompts (rename, mkdir)
  bool cmd_mode_ = false;
  std::string cmd_prompt_;
  std::string cmd_buffer_;
  enum CmdType { CMD_NONE, CMD_MKDIR, CMD_RENAME };
  CmdType cmd_type_ = CMD_NONE;
  std::string cmd_error_;

  // F3 viewer
  bool view_mode_ = false;
  std::vector<std::string> view_lines_;
  int view_scroll_ = 0;
  std::string view_title_;
  int view_panel_w_ = 0;

  // Toggle: list vs compact
  bool compact_mode_ = false;

  void loadDirectory();
  void sortEntries();
  void loadFileList();
  void refresh();
  void navigateTo(const fs::path& path);
  void goBack();
  void goForward();
  void goUp();
  void goHome();
  void setSort(int col);

  void toggleMark(size_t idx);
  void clearMarks();

  void copySelected();
  void moveSelected();
  void deleteSelected();
  void mkdir();
  void renameSelected();
  void viewFile();

  void openContextMenu(int mx, int my);
  void closeContextMenu();
  void doContextAction();

  void rebuildSidebar();

  // Layout helpers
  int sidebarW() const { return sidebar_visible_ ? 18 : 0; }
  int listX() const { return x() + sidebarW(); }
  int listW() const { return width() - sidebarW(); }

  static std::string formatSize(uintmax_t bytes);
  static std::string formatTime(std::time_t t);
  static std::string fileType(const std::string& name);
};
