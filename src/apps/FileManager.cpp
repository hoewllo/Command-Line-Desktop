#include "FileManager.h"
#include "ui/CanvasHelpers.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <set>
#include <map>
#include <cstring>

namespace {
std::tm portable_localtime(const std::time_t& t) {
  std::tm result = {};
#ifdef _WIN32
  localtime_s(&result, &t);
#else
  localtime_r(&t, &result);
#endif
  return result;
}

std::string trimStr(const std::string& s, int maxLen) {
  if (static_cast<int>(s.size()) <= maxLen) return s;
  int keep = std::max(0, maxLen - 2);
  return s.substr(0, static_cast<size_t>(keep)) + "..";
}
}

FileManager::FileManager() {
  current_path_ = fs::current_path();
  rebuildSidebar();
  loadDirectory();
}

// ── Sidebar ────────────────────────────────────────────────

void FileManager::rebuildSidebar() {
  sidebar_items_.clear();
  sidebar_items_.push_back({"Home", fs::path(getenv("HOME") ? getenv("HOME") : "/"), "\u2302"});
  sidebar_items_.push_back({"Desktop", fs::path(getenv("HOME") ? getenv("HOME") : "/") / "Desktop", "\u25A0"});
  sidebar_items_.push_back({"Documents", fs::path(getenv("HOME") ? getenv("HOME") : "/") / "Documents", "\u25A0"});
  sidebar_items_.push_back({"Downloads", fs::path(getenv("HOME") ? getenv("HOME") : "/") / "Downloads", "\u25BC"});
  sidebar_items_.push_back({"Pictures", fs::path(getenv("HOME") ? getenv("HOME") : "/") / "Pictures", "\u25A3"});
  sidebar_items_.push_back({"Trash", fs::path(getenv("HOME") ? getenv("HOME") : "/") / ".local/share/Trash/files", "\u2423"});
  sidebar_items_.push_back({"root", "/", "\u25C8"});
  // Add mount points from /etc/mtab
  try {
    std::ifstream mtab("/etc/mtab");
    std::string line;
    while (std::getline(mtab, line)) {
      auto sp = line.find(' ');
      if (sp == std::string::npos) continue;
      auto rest = line.substr(sp + 1);
      auto sp2 = rest.find(' ');
      if (sp2 == std::string::npos) continue;
      std::string mount = rest.substr(0, sp2);
      if (mount.size() > 1 && mount[0] == '/' && mount.find("/dev") != 0 && mount.find("/proc") != 0 && mount.find("/sys") != 0 && mount.find("/run") != 0) {
        std::string devName;
        auto slash = mount.rfind('/');
        if (slash != std::string::npos && slash + 1 < mount.size()) devName = mount.substr(slash + 1);
        else devName = mount;
        if (devName.empty()) devName = "/";
        sidebar_items_.push_back({devName, mount, "\u25C8"});
      }
    }
  } catch (...) {}
}

// ── Directory loading ─────────────────────────────────────

void FileManager::loadDirectory() {
  entries_.clear();
  selected_ = 0;
  scroll_offset_ = 0;
  try {
    for (const auto& entry : fs::directory_iterator(current_path_))
      entries_.push_back(entry.path());
  } catch (const std::filesystem::filesystem_error&) {}
  sortEntries();
}

void FileManager::sortEntries() {
  std::sort(entries_.begin(), entries_.end(),
    [this](const fs::path& a, const fs::path& b) {
      bool aDir = fs::is_directory(a);
      bool bDir = fs::is_directory(b);
      if (aDir != bDir) return aDir > bDir;
      int cmp = 0;
      try {
        switch (sort_col_) {
          case 0: cmp = a.filename().string().compare(b.filename().string()); break;
          case 1: {
            auto sa = fs::is_directory(a) ? 0 : fs::file_size(a);
            auto sb = fs::is_directory(b) ? 0 : fs::file_size(b);
            cmp = (sa < sb) ? -1 : (sa > sb) ? 1 : 0; break;
          }
          case 2: {
            cmp = fileType(a.filename().string()).compare(fileType(b.filename().string()));
            break;
          }
          case 3: {
            auto ta = fs::exists(a) ? fs::last_write_time(a) : fs::file_time_type::min();
            auto tb = fs::exists(b) ? fs::last_write_time(b) : fs::file_time_type::min();
            cmp = (ta < tb) ? -1 : (ta > tb) ? 1 : 0; break;
          }
        }
      } catch (...) {}
      return sort_asc_ ? (cmp < 0) : (cmp > 0);
    });
}

void FileManager::loadFileList() {
  file_entries_.clear();
  for (size_t i = 0; i < entries_.size(); ++i) {
    auto& p = entries_[i];
    FileEntry e;
    e.name = p.filename().string();
    e.is_dir = fs::is_directory(p);
    e.is_link = fs::is_symlink(p);
    try {
      if (fs::exists(p)) {
        e.size = fs::file_size(p);
        auto ft = fs::last_write_time(p);
        auto sct = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        e.write_time = std::chrono::system_clock::to_time_t(sct);
        e.date_str = formatTime(e.write_time);
      }
    } catch (...) {}
    if (e.is_dir) e.size_str = "  <DIR>";
    else e.size_str = formatSize(e.size);
    e.type_str = e.is_dir ? "folder" : fileType(e.name);
    try {
      auto perms = fs::status(p).permissions();
      e.perms = "";
      e.perms += (perms & fs::perms::owner_read) != fs::perms::none ? "r" : "-";
      e.perms += (perms & fs::perms::owner_write) != fs::perms::none ? "w" : "-";
      e.perms += (perms & fs::perms::owner_exec) != fs::perms::none ? "x" : "-";
      e.perms += (perms & fs::perms::group_read) != fs::perms::none ? "r" : "-";
      e.perms += (perms & fs::perms::group_write) != fs::perms::none ? "w" : "-";
      e.perms += (perms & fs::perms::group_exec) != fs::perms::none ? "x" : "-";
    } catch (...) { e.perms = "------"; }
    file_entries_.push_back(e);
  }
}

void FileManager::refresh() {
  loadDirectory();
  loadFileList();
  selected_ = std::min(selected_, std::max(0, static_cast<int>(entries_.size()) - 1));
  scroll_offset_ = std::min(scroll_offset_, selected_);
}

void FileManager::navigateTo(const fs::path& path) {
  try {
    if (fs::exists(path) && fs::is_directory(path)) {
      back_stack_.push_back(current_path_);
      fwd_stack_.clear();
      current_path_ = fs::canonical(path);
      refresh();
    }
  } catch (...) {}
}

void FileManager::goBack() {
  if (back_stack_.empty()) return;
  fwd_stack_.push_front(current_path_);
  current_path_ = back_stack_.back();
  back_stack_.pop_back();
  refresh();
}

void FileManager::goForward() {
  if (fwd_stack_.empty()) return;
  back_stack_.push_back(current_path_);
  current_path_ = fwd_stack_.front();
  fwd_stack_.pop_front();
  refresh();
}

void FileManager::goUp() {
  try {
    if (current_path_.has_parent_path()) {
      navigateTo(current_path_.parent_path());
      // Select the directory we came from
      for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].filename().string() == current_path_.filename().string()) {
          selected_ = static_cast<int>(i); break;
        }
      }
    }
  } catch (...) {}
}

void FileManager::goHome() {
  const char* home = getenv("HOME");
  if (home) navigateTo(home);
}

void FileManager::setSort(int col) {
  if (sort_col_ == col) sort_asc_ = !sort_asc_;
  else { sort_col_ = col; sort_asc_ = true; }
  loadDirectory();
  loadFileList();
}

void FileManager::toggleMark(size_t idx) {
  if (marked_.count(idx)) marked_.erase(idx);
  else marked_.insert(idx);
}

void FileManager::clearMarks() { marked_.clear(); }

// ── Formatting ────────────────────────────────────────────

std::string FileManager::formatSize(uintmax_t bytes) {
  if (bytes == 0) return "  0 B";
  const char* units[] = {"B", "K", "M", "G", "T"};
  int unit = 0;
  double size = static_cast<double>(bytes);
  while (size >= 1024.0 && unit < 4) { size /= 1024.0; unit++; }
  char buf[16];
  if (unit == 0) std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
  else if (size < 10.0) std::snprintf(buf, sizeof(buf), "%.1f%s", size, units[unit]);
  else std::snprintf(buf, sizeof(buf), "%.0f%s", size, units[unit]);
  return buf;
}

std::string FileManager::formatTime(std::time_t t) {
  auto tm = portable_localtime(t);
  char buf[16];
  if (std::time(nullptr) - t < 86400 * 180)
    std::strftime(buf, sizeof(buf), "%d %b %H:%M", &tm);
  else
    std::strftime(buf, sizeof(buf), "%d %b %Y", &tm);
  return buf;
}

std::string FileManager::fileType(const std::string& name) {
  auto dot = name.rfind('.');
  if (dot == std::string::npos) return "file";
  std::string ext = name.substr(dot + 1);
  for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  static const std::map<std::string, std::string> types = {
    {"txt", "text"}, {"md", "markdown"}, {"cpp", "source"}, {"h", "header"},
    {"c", "source"}, {"hpp", "header"}, {"py", "python"}, {"js", "javascript"},
    {"html", "html"}, {"css", "css"}, {"json", "json"}, {"yaml", "yaml"},
    {"yml", "yaml"}, {"toml", "toml"}, {"xml", "xml"}, {"sh", "shell script"},
    {"png", "image"}, {"jpg", "image"}, {"jpeg", "image"}, {"gif", "image"},
    {"svg", "image"}, {"ico", "image"}, {"bmp", "image"}, {"webp", "image"},
    {"pdf", "pdf"}, {"doc", "word"}, {"docx", "word"}, {"xls", "excel"},
    {"xlsx", "excel"}, {"zip", "archive"}, {"tar", "archive"},
    {"gz", "archive"}, {"bz2", "archive"}, {"xz", "archive"},
    {"7z", "archive"}, {"rar", "archive"}, {"so", "shared lib"},
    {"o", "object"}, {"deb", "package"}, {"rpm", "package"},
    {"conf", "config"}, {"cfg", "config"}, {"ini", "config"},
    {"log", "log"}, {"lock", "lock"},
  };
  auto it = types.find(ext);
  return it != types.end() ? it->second : ext + " file";
}

// ── Drawing ────────────────────────────────────────────────

void FileManager::draw(ftxui::Canvas& canvas) {
  if (!visible()) return;
  auto bg = ftxui::Color::RGB(16, 16, 32);
  auto surfaceBg = ftxui::Color::RGB(22, 22, 42);
  auto toolbarBg = ftxui::Color::RGB(28, 28, 50);
  auto textColor = ftxui::Color::RGB(200, 200, 220);

  int w = width();
  int h = height();
  int toolbarH = 1;
  int statusH = 1;
  int contentH = h - toolbarH - statusH;

  // Full background
  canvas::fill(canvas, x(), y(), w, h, bg);

  // ── Toolbar ──
  {
    int ty = y();
    canvas::fill(canvas, x(), ty, w, toolbarH, toolbarBg);

    // Navigation buttons
    struct Btn { const char* label; bool enabled; };
    Btn btns[] = {
      {"\u2190", !back_stack_.empty()},
      {"\u2192", !fwd_stack_.empty()},
      {"\u2191", current_path_.has_parent_path()},
      {"\u2302", true},
    };
    int bx = 1;
    for (int bi = 0; bi < 4; ++bi) {
      auto fg = btns[bi]. enabled ? ftxui::Color::RGB(200, 200, 220)
                                  : ftxui::Color::RGB(80, 80, 100);
      auto btnBg = toolbarBg;
      canvas::write(canvas, x() + bx, ty, " " + std::string(btns[bi].label) + " ", fg, btnBg);
      bx += 4;
    }

    // View toggle button
    canvas::write(canvas, x() + bx, ty, compact_mode_ ? " \u2630 " : " \u2637 ",
      ftxui::Color::RGB(150, 200, 255), toolbarBg);
    bx += 4;

    // Path bar (breadcrumbs or editable path)
    std::string pathStr = current_path_.string();
    int pathMax = std::max(1, w - bx - 6);
    if (static_cast<int>(pathStr.size()) > pathMax) {
      int keep = std::max(0, pathMax - 4);
      pathStr = "..." + pathStr.substr(pathStr.size() - static_cast<size_t>(keep));
    }
    canvas::write(canvas, x() + bx, ty, " " + pathStr + " ",
      ftxui::Color::RGB(255, 200, 100), ftxui::Color::RGB(20, 20, 40));

    // Search toggle hint
    canvas::write(canvas, x() + w - 6, ty, " \u2315 ",
      search_mode_ ? ftxui::Color::RGB(233, 69, 96) : ftxui::Color::RGB(100, 100, 130), toolbarBg);
  }

  // ── Separator under toolbar ──
  for (int i = 0; i < w; ++i)
    canvas::write(canvas, x() + i, y() + toolbarH, "\u2500",
      ftxui::Color::RGB(50, 50, 70), bg);

  // ── Search bar ──
  int searchY = y() + toolbarH + 1;
  if (search_mode_) {
    canvas::fill(canvas, x(), searchY, w, 1, ftxui::Color::RGB(30, 30, 50));
    std::string searchLabel = " Search: " + search_query_ + "\u258C";
    if (static_cast<int>(searchLabel.size()) > w - 2)
      searchLabel = searchLabel.substr(0, static_cast<size_t>(w - 5)) + "...";
    canvas::write(canvas, x() + 1, searchY, searchLabel,
      ftxui::Color::RGB(255, 200, 100), ftxui::Color::RGB(30, 30, 50));
  }

  // ── Load file entries ──
  loadFileList();

  int listStartY = y() + toolbarH + 1 + (search_mode_ ? 1 : 0);
  int listEndY = y() + h - statusH;
  int sw = sidebarW();

  // ── Sidebar ──
  if (sidebar_visible_) {
    int sx = x();
    int sy = listStartY;
    int sw = sidebarW();
    int sh = listEndY - listStartY;

    auto sideBg = ftxui::Color::RGB(20, 20, 38);
    canvas::fill(canvas, sx, sy, sw, sh, sideBg);

    // Sidebar border (right side)
    for (int row = 0; row < sh; ++row)
      canvas::write(canvas, sx + sw - 1, sy + row, "\u2502",
        ftxui::Color::RGB(50, 50, 70), sideBg);

    int itemY = sy;
    for (int i = 0; i < static_cast<int>(sidebar_items_.size()); ++i) {
      if (itemY - sy >= sh) break;
      auto& item = sidebar_items_[static_cast<size_t>(i)];
      bool isActive = (current_path_ == item.path);
      auto fg = isActive ? ftxui::Color::RGB(255, 255, 255)
                 : (i == sidebar_sel_ ? ftxui::Color::RGB(233, 69, 96) : ftxui::Color::RGB(150, 150, 180));
      auto iBg = (i == sidebar_sel_) ? ftxui::Color::RGB(40, 40, 60) : sideBg;
      if (isActive) iBg = ftxui::Color::RGB(50, 50, 75);
      std::string label = item.icon + " " + trimStr(item.label, sw - 3);
      canvas::write(canvas, sx + 1, itemY, " " + label, fg, iBg);
      itemY++;
    }
  }

  // ── Content area (column header + file list) ──
  int cx = listX();
  int cy = listStartY;
  int cw = listW();
  int ch = listEndY - listStartY;

  // Column headers
  {
    auto hdrBg = ftxui::Color::RGB(26, 26, 45);
    canvas::fill(canvas, cx, cy, cw, 1, hdrBg);
    int col2 = std::max(12, cw - 40);
    int col3 = std::max(col2 + 8, cw - 24);
    int col4 = cw - 12;
    auto hdrFg = ftxui::Color::RGB(150, 150, 180);
    auto sortFg = ftxui::Color::RGB(255, 200, 100);

    auto drawCol = [&](const char* label, int col, int xp) {
      auto fg = (sort_col_ == col) ? sortFg : hdrFg;
      std::string s = label;
      if (sort_col_ == col) s += sort_asc_ ? " \u25B2" : " \u25BC";
      canvas::write(canvas, cx + xp, cy, " " + s + " ", fg, hdrBg);
    };
    drawCol("Name", 0, 1);
    drawCol("Size", 1, col2);
    drawCol("Type", 2, col3);
    drawCol("Date", 3, col4);
  }

  // Separator under headers
  for (int i = 0; i < cw; ++i)
    canvas::write(canvas, cx + i, cy + 1, "\u2500",
      ftxui::Color::RGB(50, 50, 70), bg);

  // File list
  int listRow = cy + 2;
  int visRows = ch - 2;
  auto dirColor = ftxui::Color::RGB(100, 200, 255);
  auto linkColor = ftxui::Color::RGB(255, 200, 100);
  auto markColor = ftxui::Color::RGB(255, 255, 100);
  auto selBg = ftxui::Color::RGB(233, 69, 96);
  auto selFg = ftxui::Color::RGB(255, 255, 255);
  auto markBg = ftxui::Color::RGB(80, 50, 30);

  // Filter by search query
  auto filtered = file_entries_;
  if (!search_query_.empty()) {
    std::string query = search_query_;
    for (auto& c : query) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
      [&](const FileEntry& e) {
        std::string name = e.name;
        for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return name.find(query) == std::string::npos;
      }), filtered.end());
  }

  int col2 = std::max(12, cw - 40);
  int col3 = std::max(col2 + 8, cw - 24);
  int col4 = cw - 12;

  for (int i = scroll_offset_; i < static_cast<int>(filtered.size()) && (i - scroll_offset_) < visRows; ++i) {
    const auto& e = filtered[static_cast<size_t>(i)];
    int row = listRow + (i - scroll_offset_);
    if (row >= listEndY) break;

    bool isSel = (i == selected_);
    bool isMarked = marked_.count(static_cast<size_t>(i)) > 0;
    auto rowBg = isSel ? (isMarked ? ftxui::Color::RGB(200, 100, 50) : selBg)
                       : (isMarked ? markBg : surfaceBg);

    std::string prefix = e.is_dir ? (e.is_link ? "\u25B6" : "\u25C9")
                                  : (e.is_link ? "@" : " ");
    if (isMarked) prefix = "\u2713";

    std::string nameStr = prefix + " " + e.name;
    int nameMax = col2 - 3;
    if (static_cast<int>(nameStr.size()) > nameMax)
      nameStr = nameStr.substr(0, static_cast<size_t>(std::max(0, nameMax - 1))) + "\u2026";

    auto nameFg = isMarked ? markColor : (e.is_link ? linkColor : (e.is_dir ? dirColor : textColor));
    auto valFg = isMarked ? markColor : (isSel ? selFg : textColor);
    canvas::write(canvas, cx + 1, row, " " + nameStr, isSel ? selFg : nameFg, rowBg);
    canvas::write(canvas, cx + col2, row, " " + e.size_str, valFg, rowBg);
    canvas::write(canvas, cx + col3, row, " " + trimStr(e.type_str, col4 - col3 - 2), valFg, rowBg);
    canvas::write(canvas, cx + col4, row, " " + e.date_str, valFg, rowBg);
  }

  // ── Status bar ──
  {
    int sy = y() + h - statusH;
    auto statusBg = ftxui::Color::RGB(28, 28, 50);
    canvas::fill(canvas, x(), sy, w, statusH, statusBg);

    int total = static_cast<int>(filtered.size());
    int dirs = 0, files = 0;
    for (auto& p : filtered) { if (p.is_dir) dirs++; else files++; }
    size_t markCount = marked_.size();

    char left[128];
    if (markCount > 0)
      std::snprintf(left, sizeof(left), " %d/%d items \u2502 %d dirs, %d files \u2502 %zu selected",
        selected_ + 1, total, dirs, files, markCount);
    else if (total > 0)
      std::snprintf(left, sizeof(left), " %d/%d items \u2502 %d dirs, %d files",
        selected_ + 1, total, dirs, files);
    else
      std::snprintf(left, sizeof(left), " (empty) ");

    canvas::write(canvas, x() + 1, sy, left, ftxui::Color::RGB(150, 150, 180), statusBg);

    // Disk free space
    try {
      auto space = fs::space(current_path_);
      std::string freeStr = "Free: " + formatSize(space.free);
      canvas::write(canvas, x() + w - static_cast<int>(freeStr.size()) - 2, sy,
        " " + freeStr + " ", ftxui::Color::RGB(100, 180, 100), statusBg);
    } catch (...) {}
  }

  // ── Context menu ──
  if (ctx_open_) {
    int mw = 20;
    int mh = static_cast<int>(ctx_items_.size()) + 2;
    int mx = std::min(ctx_x_, x() + w - mw);
    int my = std::min(ctx_y_, y() + h - mh);
    auto menuBg = ftxui::Color::RGB(35, 35, 55);
    auto menuBorder = ftxui::Color::RGB(233, 69, 96);
    auto menuText = ftxui::Color::RGB(200, 200, 220);
    canvas::fill(canvas, mx + 1, my + 1, mw - 2, mh - 2, menuBg);
    for (int i = 0; i < mw; ++i) {
      canvas::write(canvas, mx + i, my, "\u2500", menuBorder, menuBorder);
      canvas::write(canvas, mx + i, my + mh - 1, "\u2500", menuBorder, menuBg);
    }
    for (int i = 0; i < mh; ++i) {
      canvas::write(canvas, mx, my + i, "\u2502", menuBorder, i == 0 ? menuBorder : menuBg);
      canvas::write(canvas, mx + mw - 1, my + i, "\u2502", menuBorder, i == 0 ? menuBorder : menuBg);
    }
    canvas::write(canvas, mx, my, "\u250c", menuBorder, menuBorder);
    canvas::write(canvas, mx + mw - 1, my, "\u2510", menuBorder, menuBorder);
    canvas::write(canvas, mx, my + mh - 1, "\u2514", menuBorder, menuBg);
    canvas::write(canvas, mx + mw - 1, my + mh - 1, "\u2518", menuBorder, menuBg);
    for (int i = 0; i < static_cast<int>(ctx_items_.size()); ++i) {
      auto itemBg = (i == ctx_sel_) ? selBg : menuBg;
      auto itemFg = (i == ctx_sel_) ? selFg : menuText;
      std::string label = ctx_items_[static_cast<size_t>(i)];
      if (static_cast<int>(label.size()) > mw - 3)
        label = label.substr(0, static_cast<size_t>(mw - 6)) + "...";
      canvas::write(canvas, mx + 1, my + 1 + i, " " + label + " ", itemFg, itemBg);
    }
  }

  // ── Command prompt overlay ──
  if (cmd_mode_) {
    auto promptBg = ftxui::Color::RGB(20, 20, 40);
    int py = y() + h - 1;
    canvas::fill(canvas, x(), py, w, 1, promptBg);
    std::string prompt = cmd_prompt_ + cmd_buffer_;
    if (static_cast<int>(prompt.size()) > w - 3)
      prompt = prompt.substr(0, static_cast<size_t>(w - 6)) + "...";
    canvas::write(canvas, x() + 1, py, " " + prompt, ftxui::Color::RGB(255, 255, 255), promptBg);
    if (!cmd_error_.empty()) {
      canvas::write(canvas, x() + w - static_cast<int>(cmd_error_.size()) - 3, py,
        " " + cmd_error_, ftxui::Color::RGB(255, 100, 100), promptBg);
    }
  }

  // ── File viewer ──
  if (view_mode_) {
    view_panel_w_ = w;
    auto vBg = ftxui::Color::RGB(16, 16, 32);
    auto vTitleBg = ftxui::Color::RGB(30, 30, 55);
    auto vText = ftxui::Color::RGB(200, 200, 220);
    canvas::fill(canvas, x(), y(), w, h, vBg);
    canvas::fill(canvas, x(), y(), w, 1, vTitleBg);
    std::string title = " \u25B6 " + view_title_ + "  (Esc to close, PgUp/PgDn to scroll) ";
    canvas::write(canvas, x(), y(), trimStr(title, w - 1), ftxui::Color::RGB(233, 69, 96), vTitleBg);
    int lineY = y() + 1;
    int maxLines = h - 2;
    for (int i = view_scroll_; i < static_cast<int>(view_lines_.size()) && (i - view_scroll_) < maxLines; ++i) {
      canvas::write(canvas, x() + 1, lineY, trimStr(view_lines_[static_cast<size_t>(i)], w - 2), vText, vBg);
      lineY++;
    }
    if (!view_lines_.empty()) {
      int total = static_cast<int>(view_lines_.size());
      int percent = (view_scroll_ + maxLines) * 100 / total;
      char pct[16];
      std::snprintf(pct, sizeof(pct), "%d%%", std::min(percent, 100));
      canvas::write(canvas, x() + w - 5, y() + h - 1, pct, ftxui::Color::RGB(233, 69, 96), vBg);
    }
  }
}

// ── Event handling ────────────────────────────────────────

bool FileManager::handleEvent(ftxui::Event event) {
  if (!visible()) return false;
  int w = width();
  int h = height();

  // View mode
  if (view_mode_) {
    if (event == ftxui::Event::Escape) { view_mode_ = false; view_lines_.clear(); return true; }
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::PageUp) {
      int step = (event == ftxui::Event::PageUp) ? (h - 3) : 1;
      view_scroll_ = std::max(0, view_scroll_ - step); return true;
    }
    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::PageDown) {
      int step = (event == ftxui::Event::PageDown) ? (h - 3) : 1;
      view_scroll_ = std::min(std::max(0, static_cast<int>(view_lines_.size()) - (h - 2)), view_scroll_ + step);
      return true;
    }
    if (event == ftxui::Event::Home) { view_scroll_ = 0; return true; }
    if (event == ftxui::Event::End) { view_scroll_ = std::max(0, static_cast<int>(view_lines_.size()) - (h - 2)); return true; }
    return true;
  }

  // Mouse
  if (event.is_mouse()) {
    auto& mouse = event.mouse();
    int mx = mouse.x;
    int my = mouse.y;
    int toolbarH = 1;
    int statusH = 1;
    int contentTop = y() + toolbarH + 1 + (search_mode_ ? 1 : 0);
    int contentEnd = y() + h - statusH;
    int sw = sidebarW();
    int cx = listX();

    // Context menu click
    if (ctx_open_) {
      if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
        int mw = 20;
        int mh = static_cast<int>(ctx_items_.size()) + 2;
        int cmx = std::min(ctx_x_, x() + w - mw);
        int cmy = std::min(ctx_y_, y() + h - mh);
        if (mx >= cmx && mx < cmx + mw && my >= cmy && my < cmy + mh) {
          int idx = my - cmy - 1;
          if (idx >= 0 && idx < static_cast<int>(ctx_items_.size())) {
            ctx_sel_ = idx; closeContextMenu(); doContextAction(); return true;
          }
        } else { closeContextMenu(); return true; }
      }
      return true;
    }

    if (mouse.motion == ftxui::Mouse::Pressed) {
      // Toolbar buttons
      if (my == y()) {
        int bx = 1;
        if (mx >= x() + bx && mx < x() + bx + 4) { goBack(); return true; }
        bx += 4;
        if (mx >= x() + bx && mx < x() + bx + 4) { goForward(); return true; }
        bx += 4;
        if (mx >= x() + bx && mx < x() + bx + 4) { goUp(); return true; }
        bx += 4;
        if (mx >= x() + bx && mx < x() + bx + 4) { goHome(); return true; }
        bx += 4;
        if (mx >= x() + bx && mx < x() + bx + 4) { compact_mode_ = !compact_mode_; return true; }
        bx += 4;
        // Path bar click → enable editing
        if (mx >= x() + bx && mx < x() + w - 6 && mouse.button == ftxui::Mouse::Left) {
          path_editing_ = true;
          path_buffer_ = current_path_.string();
          return true;
        }
        // Search toggle
        if (mx >= x() + w - 6 && mx < x() + w) { search_mode_ = !search_mode_; if (!search_mode_) search_query_.clear(); return true; }
        return true;
      }

      // Search bar click
      if (search_mode_ && my == y() + toolbarH + 1) {
        // Focus search (just ensure it's active - keyboard will type)
        return true;
      }

      // Column header click → sort
      if (my == contentTop && mx >= cx) {
        int cw = listW();
        int col2 = std::max(12, cw - 40);
        int col3 = std::max(col2 + 8, cw - 24);
        int col4 = cw - 12;
        int rlx = mx - cx;
        if (rlx < col2) setSort(0);
        else if (rlx < col3) setSort(1);
        else if (rlx < col4) setSort(2);
        else setSort(3);
        return true;
      }

      // Sidebar click
      if (sidebar_visible_ && mx >= x() && mx < x() + sw && my >= contentTop && my < contentEnd) {
        int idx = my - contentTop;
        if (idx >= 0 && idx < static_cast<int>(sidebar_items_.size())) {
          sidebar_sel_ = idx;
          auto& item = sidebar_items_[static_cast<size_t>(idx)];
          try {
            if (fs::exists(item.path)) navigateTo(item.path);
          } catch (...) {}
          return true;
        }
      }

      // File list click
      if (mx >= cx && mx < x() + w && my >= contentTop + 2 && my < contentEnd) {
        auto filtered = file_entries_;
        if (!search_query_.empty()) {
          std::string query = search_query_;
          for (auto& c : query) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
          filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
            [&](const FileEntry& e) {
              std::string name = e.name;
              for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
              return name.find(query) == std::string::npos;
            }), filtered.end());
        }
        int row = my - (contentTop + 2);
        int idx = row + scroll_offset_;
        if (idx >= 0 && idx < static_cast<int>(filtered.size())) {
          selected_ = idx;
          if (mouse.button == ftxui::Mouse::Right) {
            ctx_sel_ = 0;
            openContextMenu(mx, my);
          }
        }

        // Double-click detection via Enter-in-mouse
        return true;
      }

      // Right-click on empty area
      if (mouse.button == ftxui::Mouse::Right && my >= contentTop && my < contentEnd) {
        ctx_sel_ = 0;
        openContextMenu(mx, my);
        return true;
      }
    }

    // Mouse wheel
    if (mouse.button == ftxui::Mouse::WheelUp && mouse.motion == ftxui::Mouse::Pressed) {
      auto filtered = file_entries_;
      if (!search_query_.empty()) {
        std::string query = search_query_;
        for (auto& c : query) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
          [&](const FileEntry& e) {
            std::string name = e.name;
            for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return name.find(query) == std::string::npos;
          }), filtered.end());
      }
      if (selected_ > 0) { selected_--; if (selected_ < scroll_offset_) scroll_offset_--; }
      return true;
    }
    if (mouse.button == ftxui::Mouse::WheelDown && mouse.motion == ftxui::Mouse::Pressed) {
      auto filtered = file_entries_;
      if (!search_query_.empty()) {
        std::string query = search_query_;
        for (auto& c : query) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
          [&](const FileEntry& e) {
            std::string name = e.name;
            for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return name.find(query) == std::string::npos;
          }), filtered.end());
      }
      if (selected_ < static_cast<int>(filtered.size()) - 1) {
        selected_++;
        int visRows = (contentEnd - contentTop) - 2;
        if (selected_ >= scroll_offset_ + visRows) scroll_offset_++;
      }
      return true;
    }
    return true;
  }

  // ---- Keyboard ----

  // Path editing mode
  if (path_editing_) {
    if (event == ftxui::Event::Escape) { path_editing_ = false; return true; }
    if (event == ftxui::Event::Return) {
      path_editing_ = false;
      navigateTo(path_buffer_);
      return true;
    }
    if (event == ftxui::Event::Backspace && !path_buffer_.empty()) {
      path_buffer_.pop_back(); return true;
    }
    if (event.is_character()) {
      auto ch = event.character();
      if (!ch.empty() && ch[0] >= 32 && ch[0] < 127 && path_buffer_.size() < 200)
        path_buffer_ += ch;
      return true;
    }
    return true;
  }

  // Search mode
  if (search_mode_) {
    if (event == ftxui::Event::Escape) { search_mode_ = false; search_query_.clear(); return true; }
    if (event == ftxui::Event::Return) { search_mode_ = false; return true; }
    if (event == ftxui::Event::Backspace && !search_query_.empty()) {
      search_query_.pop_back(); return true;
    }
    if (event.is_character()) {
      auto ch = event.character();
      if (!ch.empty() && ch[0] >= 32 && ch[0] < 127)
        search_query_ += ch;
      return true;
    }
    return true;
  }

  // Command mode
  if (cmd_mode_) {
    if (event == ftxui::Event::Escape) { cmd_mode_ = false; cmd_buffer_.clear(); cmd_error_.clear(); return true; }
    if (event == ftxui::Event::Return) {
      cmd_mode_ = false;
      if (!cmd_buffer_.empty()) {
        switch (cmd_type_) {
          case CMD_MKDIR: mkdir(); break;
          case CMD_RENAME: renameSelected(); break;
          default: break;
        }
      }
      cmd_buffer_.clear(); cmd_error_.clear(); return true;
    }
    if (event == ftxui::Event::Backspace) { if (!cmd_buffer_.empty()) cmd_buffer_.pop_back(); return true; }
    if (event.is_character()) {
      auto ch = event.character();
      if (!ch.empty() && ch[0] >= 32 && ch[0] < 127 && cmd_buffer_.size() < 60)
        cmd_buffer_ += ch;
      return true;
    }
    return true;
  }

  // ---- Normal navigation ----

  // Filtered view for navigation
  auto filtered = file_entries_;
  if (!search_query_.empty()) {
    std::string query = search_query_;
    for (auto& c : query) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
      [&](const FileEntry& e) {
        std::string name = e.name;
        for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return name.find(query) == std::string::npos;
      }), filtered.end());
  }

  if (event == ftxui::Event::Tab || event == ftxui::Event::TabReverse) {
    toggleMark(static_cast<size_t>(selected_));
    if (selected_ < static_cast<int>(filtered.size()) - 1) { selected_++; }
    return true;
  }

  if (event == ftxui::Event::ArrowUp) {
    if (selected_ > 0) { selected_--; if (selected_ < scroll_offset_) scroll_offset_--; }
    return true;
  }
  if (event == ftxui::Event::ArrowDown) {
    if (selected_ < static_cast<int>(filtered.size()) - 1) { selected_++; }
    int visRows = (h - 2 - (search_mode_ ? 1 : 0));
    if (selected_ >= scroll_offset_ + visRows) scroll_offset_++;
    return true;
  }
  if (event == ftxui::Event::PageUp) {
    int step = std::max(1, (h - 4));
    selected_ = std::max(0, selected_ - step);
    scroll_offset_ = std::max(0, scroll_offset_ - step);
    return true;
  }
  if (event == ftxui::Event::PageDown) {
    int step = std::max(1, (h - 4));
    selected_ = std::min(static_cast<int>(filtered.size()) - 1, selected_ + step);
    scroll_offset_ = std::max(0, std::min(scroll_offset_ + step, static_cast<int>(filtered.size()) - step));
    return true;
  }
  if (event == ftxui::Event::Home) { selected_ = 0; scroll_offset_ = 0; return true; }
  if (event == ftxui::Event::End) {
    selected_ = static_cast<int>(filtered.size()) - 1;
    int visRows = std::max(1, h - 4);
    scroll_offset_ = std::max(0, static_cast<int>(filtered.size()) - visRows);
    return true;
  }

  if (event == ftxui::Event::Return) {
    if (selected_ >= 0 && selected_ < static_cast<int>(filtered.size())) {
      auto& e = filtered[static_cast<size_t>(selected_)];
      if (e.is_dir) navigateTo(entries_[static_cast<size_t>(selected_)]);
      else viewFile();
    }
    return true;
  }

  if (event == ftxui::Event::Backspace) { goUp(); return true; }

  // F-keys
  if (event == ftxui::Event::F3) { viewFile(); return true; }
  if (event == ftxui::Event::F5) { copySelected(); return true; }
  if (event == ftxui::Event::F6) {
    if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size())) {
      cmd_type_ = CMD_RENAME;
      cmd_prompt_ = "Rename to: ";
      cmd_buffer_ = entries_[static_cast<size_t>(selected_)].filename().string();
      cmd_mode_ = true;
      cmd_error_.clear();
    }
    return true;
  }
  if (event == ftxui::Event::F7) {
    cmd_type_ = CMD_MKDIR;
    cmd_prompt_ = "Create directory: ";
    cmd_buffer_.clear();
    cmd_mode_ = true;
    cmd_error_.clear();
    return true;
  }
  if (event == ftxui::Event::F8) { deleteSelected(); return true; }
  if (event == ftxui::Event::F9) { sidebar_visible_ = !sidebar_visible_; return true; }
  if (event == ftxui::Event::F10) { return false; }

  // Ctrl+L = edit path
  if (event.is_character() && event.character() == "\x0c") {
    path_editing_ = true;
    path_buffer_ = current_path_.string();
    return true;
  }

  // Ctrl+F = search
  if (event.is_character() && event.character() == "\x06") {
    search_mode_ = !search_mode_;
    if (search_mode_) search_query_.clear();
    return true;
  }

  // Ctrl+R = refresh
  if (event.is_character() && event.character() == "\x12") {
    refresh(); return true;
  }

  // Alt+Left / Alt+Right = back / forward
  if (event.input() == "\x1b\x5b\x44" || event.input() == "\x1b\x4f\x44") { goBack(); return true; }
  if (event.input() == "\x1b\x5b\x43" || event.input() == "\x1b\x4f\x43") { goForward(); return true; }

  return false;
}

// ── File operations ───────────────────────────────────────

void FileManager::copySelected() {
  if (selected_ < 0 || selected_ >= static_cast<int>(entries_.size())) return;
  auto src = entries_[static_cast<size_t>(selected_)];
  std::string baseName = src.filename().string();
  auto dst = current_path_ / (baseName + ".copy");
  try {
    if (fs::is_directory(src)) fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    else fs::copy(src, dst, fs::copy_options::overwrite_existing);
    refresh();
  } catch (const std::exception& e) {
    cmd_error_ = e.what();
    cmd_prompt_ = "Copy error: ";
    cmd_buffer_ = cmd_error_;
    cmd_mode_ = true;
  }
}

void FileManager::moveSelected() {
  if (selected_ < 0 || selected_ >= static_cast<int>(entries_.size())) return;
  auto src = entries_[static_cast<size_t>(selected_)];
  std::string baseName = src.filename().string();
  auto dst = current_path_ / (std::string("_") + baseName);
  try {
    fs::rename(src, dst);
    refresh();
  } catch (const std::exception& e) {
    cmd_error_ = e.what();
    cmd_prompt_ = "Move error: ";
    cmd_buffer_ = cmd_error_;
    cmd_mode_ = true;
  }
}

void FileManager::deleteSelected() {
  std::vector<size_t> toDelete;
  if (marked_.empty()) {
    if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size()))
      toDelete.push_back(static_cast<size_t>(selected_));
  } else {
    for (auto idx : marked_) toDelete.push_back(idx);
  }
  std::sort(toDelete.rbegin(), toDelete.rend());
  int errCount = 0;
  for (auto idx : toDelete) {
    if (idx >= entries_.size()) continue;
    try {
      if (fs::is_directory(entries_[idx])) fs::remove_all(entries_[idx]);
      else fs::remove(entries_[idx]);
    } catch (...) { errCount++; }
  }
  marked_.clear();
  refresh();
  if (errCount > 0) {
    cmd_error_ = std::to_string(errCount) + " error(s)";
    cmd_prompt_ = "Delete: ";
    cmd_buffer_ = cmd_error_;
    cmd_mode_ = true;
  }
}

void FileManager::mkdir() {
  try {
    fs::create_directory(current_path_ / cmd_buffer_);
    refresh();
  } catch (const std::exception& e) {
    cmd_error_ = e.what();
    cmd_prompt_ = "Mkdir error: ";
    cmd_buffer_ = cmd_error_;
    cmd_mode_ = true;
  }
}

void FileManager::renameSelected() {
  if (selected_ < 0 || selected_ >= static_cast<int>(entries_.size())) return;
  auto src = entries_[static_cast<size_t>(selected_)];
  auto dst = current_path_ / cmd_buffer_;
  try {
    fs::rename(src, dst);
    refresh();
  } catch (const std::exception& e) {
    cmd_error_ = e.what();
    cmd_prompt_ = "Rename error: ";
    cmd_buffer_ = cmd_error_;
    cmd_mode_ = true;
  }
}

void FileManager::viewFile() {
  if (selected_ < 0 || selected_ >= static_cast<int>(entries_.size())) return;
  auto target = entries_[static_cast<size_t>(selected_)];
  if (fs::is_directory(target)) return;

  view_lines_.clear();
  view_title_ = target.filename().string();
  view_scroll_ = 0;

  bool isBinary = false;
  try {
    std::ifstream f(target, std::ios::binary);
    if (f) {
      char buf[1024];
      f.read(buf, sizeof(buf));
      for (int i = 0; i < f.gcount(); ++i) { if (buf[i] == 0) { isBinary = true; break; } }
    }
  } catch (...) {}

  if (isBinary) {
    try {
      std::ifstream f(target, std::ios::binary);
      if (f) {
        char buf[16];
        unsigned long long offset = 0;
        while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
          std::ostringstream oss;
          char hex[32];
          std::snprintf(hex, sizeof(hex), "%08llx  ", offset);
          oss << hex;
          int count = static_cast<int>(f.gcount() > 0 ? f.gcount() : 16);
          for (int i = 0; i < 16; ++i) {
            if (i < count) oss << std::hex << std::setw(2) << std::setfill('0')
                              << (static_cast<unsigned>(static_cast<unsigned char>(buf[i]))) << " ";
            else oss << "   ";
            if (i == 7) oss << " ";
          }
          oss << "|";
          for (int i = 0; i < count; ++i) {
            unsigned char c = static_cast<unsigned char>(buf[i]);
            oss << ((c >= 32 && c < 127) ? static_cast<char>(c) : '.');
          }
          oss << "|";
          view_lines_.push_back(oss.str());
          offset += 16;
          if (f.gcount() < 16 && !f) break;
        }
      }
    } catch (...) {}
    view_title_ = "[HEX] " + view_title_;
  } else {
    try {
      std::ifstream f(target);
      std::string line;
      while (std::getline(f, line)) view_lines_.push_back(line);
    } catch (...) {}
  }

  if (!view_lines_.empty()) view_mode_ = true;
}

// ── Context menu ──────────────────────────────────────────

void FileManager::openContextMenu(int mx, int my) {
  ctx_items_.clear();
  if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size())) {
    auto& e = file_entries_[static_cast<size_t>(selected_)];
    if (e.is_dir) {
      ctx_items_.push_back("Open");
    } else {
      ctx_items_.push_back("View (F3)");
    }
    ctx_items_.push_back("Rename (F6)");
    ctx_items_.push_back("Copy here");
    ctx_items_.push_back("Move");
    ctx_items_.push_back("Delete (F8)");
  }
  ctx_items_.push_back("New folder (F7)");
  ctx_items_.push_back("---");
  ctx_items_.push_back("Refresh (^R)");
  ctx_items_.push_back("Properties");

  ctx_x_ = mx;
  ctx_y_ = my;
  ctx_sel_ = 0;
  ctx_open_ = true;
}

void FileManager::closeContextMenu() {
  ctx_open_ = false;
}

void FileManager::doContextAction() {
  if (ctx_sel_ < 0 || ctx_sel_ >= static_cast<int>(ctx_items_.size())) return;
  std::string action = ctx_items_[static_cast<size_t>(ctx_sel_)];
  if (action == "Open" || action == "View (F3)") {
    if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size())) {
      auto target = entries_[static_cast<size_t>(selected_)];
      if (fs::is_directory(target)) navigateTo(target);
      else viewFile();
    }
  } else if (action == "Rename (F6)") {
    if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size())) {
      cmd_type_ = CMD_RENAME;
      cmd_prompt_ = "Rename to: ";
      cmd_buffer_ = entries_[static_cast<size_t>(selected_)].filename().string();
      cmd_mode_ = true;
      cmd_error_.clear();
    }
  } else if (action == "Copy here") {
    copySelected();
  } else if (action == "Move") {
    moveSelected();
  } else if (action == "Delete (F8)") {
    deleteSelected();
  } else if (action == "New folder (F7)") {
    cmd_type_ = CMD_MKDIR;
    cmd_prompt_ = "Create directory: ";
    cmd_buffer_.clear();
    cmd_mode_ = true;
    cmd_error_.clear();
  } else if (action == "Refresh (^R)") {
    refresh();
  } else if (action == "Properties") {
    // Show properties - simple info display
    if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size())) {
      auto& e = file_entries_[static_cast<size_t>(selected_)];
      std::string info = e.name + " | " + e.size_str + " | " + e.date_str + " | " + e.perms;
      cmd_error_ = info;
      cmd_prompt_ = "Properties: ";
      cmd_buffer_ = "";
      cmd_mode_ = true;
    }
  }
}
