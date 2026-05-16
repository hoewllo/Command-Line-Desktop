#pragma once
#include "core/Component.h"
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <mutex>

struct TermCell {
  char ch = ' ';
  uint8_t fg_r = 200, fg_g = 200, fg_b = 200;
  uint8_t bg_r = 10, bg_g = 10, bg_b = 15;
};

class TerminalBuffer {
public:
  TerminalBuffer(int cols = 80, int rows = 24);
  void write(const char* data, int len);
  void resize(int cols, int rows);
  void draw(ftxui::Canvas& canvas, int x, int y, int w, int h, bool showCursor, int scrollOffset = 0);
  int visibleRows() const { return rows_; }
  int cols() const { return cols_; }
  int scrollbackRows() const { return static_cast<int>(grid_.size()); }
  bool inAltScreen() const { return alt_screen_; }

private:
  int cols_ = 80, rows_ = 24;
  int scrollback_max_ = 2000;
  std::deque<std::vector<TermCell>> grid_;
  std::deque<std::vector<TermCell>> saved_grid_;
  bool alt_screen_ = false;
  int cx_ = 0, cy_ = 0;
  int saved_cx_ = 0, saved_cy_ = 0;
  uint8_t cur_fg_r = 200, cur_fg_g = 200, cur_fg_b = 200;
  uint8_t cur_bg_r = 10, cur_bg_g = 10, cur_bg_b = 15;
  uint8_t def_fg_r = 200, def_fg_g = 200, def_fg_b = 200;
  uint8_t def_bg_r = 10, def_bg_g = 10, def_bg_b = 15;
  bool cur_bold = false, cur_reverse = false;

  enum State { NORMAL, ESC, CSI, OSC };
  State state_ = NORMAL;
  std::string csi_buf_;
  bool csi_private_ = false;

  TermCell dummy_{};
  TermCell& cell(int x, int y);
  void newline();
  void putChar(char ch);
  void executeCSI(char final_char);
  void applySGR(const std::vector<int>& params);
  void scrollUp();
  void clearLine(int mode);
  void clearScreen(int mode);
  void cursorUp(int n);
  void cursorDown(int n);
  void cursorForward(int n);
  void cursorBack(int n);
  void setCursor(int row, int col);
  void setCursorRow(int row);
  void insertLines(int n);
  void deleteLines(int n);

  static void color256(int idx, uint8_t& r, uint8_t& g, uint8_t& b);
  static void ansiColor(int idx, uint8_t& r, uint8_t& g, uint8_t& b);
};

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
#ifdef _WIN32
  void readOutput();
#else
  void readOutputPty();
  void resizePty();
  int master_fd_ = -1;
  pid_t child_pid_ = -1;
  int wake_pipe_[2] = {-1, -1};
#endif

  std::string command_;
  std::string name_;
  TerminalBuffer term_;
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::mutex mutex_;
  int scroll_offset_ = 0;
  int last_pty_w_ = 0, last_pty_h_ = 0;
};
