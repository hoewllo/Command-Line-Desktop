#include "AppRunner.h"
#include "ui/CanvasHelpers.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <csignal>
#include <sstream>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#endif

// ---- TerminalBuffer ----

static const int ansi_rgb[16][3] = {
  {0,0,0},{128,0,0},{0,128,0},{128,128,0},
  {0,0,128},{128,0,128},{0,128,128},{192,192,192},
  {128,128,128},{255,0,0},{0,255,0},{255,255,0},
  {0,0,255},{255,0,255},{0,255,255},{255,255,255},
};

TerminalBuffer::TerminalBuffer(int cols, int rows) : cols_(cols), rows_(rows) {
  for (int i = 0; i < rows_; ++i)
    grid_.push_back(std::vector<TermCell>(cols_));
}

TermCell& TerminalBuffer::cell(int x, int y) {
  int idx = (int)grid_.size() - rows_ + y;
  if (idx < 0) idx = 0;
  if (idx >= (int)grid_.size()) idx = (int)grid_.size() - 1;
  return grid_[idx][std::max(0, std::min(cols_ - 1, x))];
}

void TerminalBuffer::newline() {
  cx_ = 0;
  if (cy_ == rows_ - 1) { scrollUp(); }
  else { cy_++; }
}

void TerminalBuffer::scrollUp() {
  grid_.push_back(std::vector<TermCell>(cols_));
  if ((int)grid_.size() > scrollback_max_) grid_.pop_front();
}

void TerminalBuffer::putChar(char ch) {
  if (cy_ < 0) cy_ = 0;
  if (cy_ >= rows_) cy_ = rows_ - 1;
  auto& c = cell(cx_, cy_);
  c.ch = ch;
  c.fg_r = cur_fg_r; c.fg_g = cur_fg_g; c.fg_b = cur_fg_b;
  c.bg_r = cur_bg_r; c.bg_g = cur_bg_g; c.bg_b = cur_bg_b;
  cx_++;
  if (cx_ >= cols_) newline();
}

void TerminalBuffer::clearLine(int mode) {
  if (mode == 2) {
    for (int x = 0; x < cols_; ++x) { auto& c = cell(x, cy_); c.ch = ' '; }
  } else if (mode == 1) {
    for (int x = 0; x <= cx_; ++x) { auto& c = cell(x, cy_); c.ch = ' '; }
  } else {
    for (int x = cx_; x < cols_; ++x) { auto& c = cell(x, cy_); c.ch = ' '; }
  }
}

void TerminalBuffer::clearScreen(int mode) {
  if (mode == 2) {
    for (int y = 0; y < rows_; ++y)
      for (int x = 0; x < cols_; ++x)
        { auto& c = cell(x, y); c.ch = ' '; }
  } else if (mode == 1) {
    for (int y = 0; y <= cy_; ++y)
      for (int x = 0; x < cols_; ++x)
        { auto& c = cell(x, y); c.ch = ' '; }
  } else {
    for (int y = cy_; y < rows_; ++y)
      for (int x = 0; x < cols_; ++x)
        { auto& c = cell(x, y); c.ch = ' '; }
  }
}

void TerminalBuffer::cursorUp(int n) { cy_ = std::max(0, cy_ - n); }
void TerminalBuffer::cursorDown(int n) { cy_ = std::min(rows_ - 1, cy_ + n); }
void TerminalBuffer::cursorForward(int n) { cx_ = std::min(cols_ - 1, cx_ + n); }
void TerminalBuffer::cursorBack(int n) { cx_ = std::max(0, cx_ - n); }
void TerminalBuffer::setCursor(int row, int col) {
  cx_ = std::max(0, std::min(cols_ - 1, col));
  cy_ = std::max(0, std::min(rows_ - 1, row));
}
void TerminalBuffer::setCursorRow(int row) { cy_ = std::max(0, std::min(rows_ - 1, row)); }
void TerminalBuffer::insertLines(int n) {
  for (int i = 0; i < n && cy_ < rows_ - 1; ++i) {
    grid_.insert(grid_.end() - rows_ + cy_, std::vector<TermCell>(cols_));
    if ((int)grid_.size() > scrollback_max_) grid_.pop_front();
    rows_ = std::min(rows_, (int)grid_.size());
  }
}
void TerminalBuffer::deleteLines(int n) {
  for (int i = 0; i < n && cy_ < rows_ - 1; ++i) {
    int idx = (int)grid_.size() - rows_ + cy_;
    if (idx >= 0 && idx < (int)grid_.size()) {
      grid_.erase(grid_.begin() + idx);
      grid_.push_back(std::vector<TermCell>(cols_));
    }
  }
}

void TerminalBuffer::ansiColor(int idx, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (idx >= 0 && idx < 16) { r = ansi_rgb[idx][0]; g = ansi_rgb[idx][1]; b = ansi_rgb[idx][2]; }
  else { r = g = b = 200; }
}

void TerminalBuffer::color256(int idx, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (idx < 16) { ansiColor(idx, r, g, b); }
  else if (idx < 232) {
    int i = idx - 16;
    r = (i / 36) % 6 * 51;
    g = (i / 6) % 6 * 51;
    b = i % 6 * 51;
  } else {
    int gray = (idx - 232) * 10 + 8;
    r = g = b = std::min(255, gray);
  }
}

void TerminalBuffer::applySGR(const std::vector<int>& params) {
  for (size_t i = 0; i < params.size(); ++i) {
    int p = params[i];
    if (p == 0) {
      cur_fg_r = def_fg_r; cur_fg_g = def_fg_g; cur_fg_b = def_fg_b;
      cur_bg_r = def_bg_r; cur_bg_g = def_bg_g; cur_bg_b = def_bg_b;
      cur_bold = false; cur_reverse = false;
    } else if (p == 1) { cur_bold = true; }
    else if (p == 22) { cur_bold = false; }
    else if (p == 7) { cur_reverse = true; }
    else if (p == 27) { cur_reverse = false; }
    else if (p >= 30 && p <= 37) { ansiColor(p - 30, cur_fg_r, cur_fg_g, cur_fg_b); }
    else if (p == 38) {
      if (i + 2 < params.size() && params[i+1] == 5) { color256(params[i+2], cur_fg_r, cur_fg_g, cur_fg_b); i += 2; }
      else if (i + 4 < params.size() && params[i+1] == 2) { cur_fg_r = params[i+2]; cur_fg_g = params[i+3]; cur_fg_b = params[i+4]; i += 4; }
    } else if (p == 39) {
      cur_fg_r = def_fg_r; cur_fg_g = def_fg_g; cur_fg_b = def_fg_b;
    } else if (p >= 40 && p <= 47) { ansiColor(p - 40, cur_bg_r, cur_bg_g, cur_bg_b); }
    else if (p == 48) {
      if (i + 2 < params.size() && params[i+1] == 5) { color256(params[i+2], cur_bg_r, cur_bg_g, cur_bg_b); i += 2; }
      else if (i + 4 < params.size() && params[i+1] == 2) { cur_bg_r = params[i+2]; cur_bg_g = params[i+3]; cur_bg_b = params[i+4]; i += 4; }
    } else if (p == 49) {
      cur_bg_r = def_bg_r; cur_bg_g = def_bg_g; cur_bg_b = def_bg_b;
    } else if (p >= 90 && p <= 97) { ansiColor(p - 90 + 8, cur_fg_r, cur_fg_g, cur_fg_b); }
    else if (p >= 100 && p <= 107) { ansiColor(p - 100 + 8, cur_bg_r, cur_bg_g, cur_bg_b); }
  }
}

void TerminalBuffer::executeCSI(char final_char) {
  std::vector<int> params;
  std::stringstream ss(csi_buf_);
  std::string seg;
  while (std::getline(ss, seg, ';')) {
    if (!seg.empty()) {
      try { params.push_back(std::stoi(seg)); } catch (...) { params.push_back(0); }
    } else params.push_back(0);
  }
  if (params.empty()) params.push_back(0);

  switch (final_char) {
    case 'H': case 'f':
      setCursor(params.size() > 0 ? params[0] - 1 : 0,
                params.size() > 1 ? params[1] - 1 : 0);
      break;
    case 'A': cursorUp(params[0]); break;
    case 'B': cursorDown(params[0]); break;
    case 'C': cursorForward(params[0]); break;
    case 'D': cursorBack(params[0]); break;
    case 'E': cursorDown(params[0]); cx_ = 0; break;
    case 'F': cursorUp(params[0]); cx_ = 0; break;
    case 'G': cx_ = std::max(0, std::min(cols_ - 1, params[0] - 1)); break;
    case 'd': setCursorRow(params[0] - 1); break;
    case 'J': clearScreen(params[0]); break;
    case 'K': clearLine(params[0]); break;
    case 'L': insertLines(params[0]); break;
    case 'M': deleteLines(params[0]); break;
    case 'm': applySGR(params); break;
    case 's': saved_cx_ = cx_; saved_cy_ = cy_; break;
    case 'u': cx_ = saved_cx_; cy_ = saved_cy_; break;
    case 'h': case 'l': break;
    default: break;
  }
}

void TerminalBuffer::write(const char* data, int len) {
  for (int i = 0; i < len; ++i) {
    unsigned char ch = (unsigned char)data[i];

    if (state_ == NORMAL) {
      if (ch == '\x1b') { state_ = ESC; }
      else if (ch == '\r') { cx_ = 0; }
      else if (ch == '\n') { newline(); }
      else if (ch == '\b') { if (cx_ > 0) cx_--; }
      else if (ch == '\t') { cx_ = ((cx_ + 8) / 8) * 8; if (cx_ >= cols_) { cx_ = 0; newline(); } }
      else if (ch == '\a') { /* bell - ignore */ }
      else if (ch >= 32) { putChar((char)ch); }
    } else if (state_ == ESC) {
      if (ch == '[') { state_ = CSI; csi_buf_.clear(); csi_private_ = false; }
      else if (ch == ']') { state_ = OSC; }
      else if (ch == '7') { saved_cx_ = cx_; saved_cy_ = cy_; state_ = NORMAL; }
      else if (ch == '8') { cx_ = saved_cx_; cy_ = saved_cy_; state_ = NORMAL; }
      else if (ch == 'c') { /* ris - ignore */ state_ = NORMAL; }
      else { state_ = NORMAL; }
    } else if (state_ == CSI) {
      if (ch == '?') { csi_private_ = true; }
      else if ((ch >= '0' && ch <= '9') || ch == ';') { csi_buf_ += (char)ch; }
      else if (ch >= 0x40 && ch <= 0x7E) { executeCSI((char)ch); state_ = NORMAL; }
      else { state_ = NORMAL; }
    } else if (state_ == OSC) {
      if (ch == '\x1b') { state_ = ESC; }
      else if (ch == '\a') { state_ = NORMAL; }
    }
  }
}

void TerminalBuffer::resize(int cols, int rows) {
  cols_ = cols; rows_ = rows;
  while ((int)grid_.size() < rows_) grid_.push_back(std::vector<TermCell>(cols_));
  while ((int)grid_.size() > scrollback_max_) grid_.pop_front();
  for (auto& row : grid_) row.resize(cols_);
  cx_ = std::min(cx_, cols_ - 1);
  cy_ = std::min(cy_, rows_ - 1);
}

void TerminalBuffer::draw(ftxui::Canvas& canvas, int x, int y, int w, int h, bool showCursor) {
  int drawRows = std::min(h, rows_);
  int drawCols = std::min(w, cols_);

  for (int row = 0; row < drawRows; ++row) {
    int prev_bg_r = -1, prev_bg_g = -1, prev_bg_b = -1;
    int prev_fg_r = -1, prev_fg_g = -1, prev_fg_b = -1;
    std::string buf;
    int buf_start = 0;

    for (int col = 0; col < drawCols; ++col) {
      auto& c = cell(col, row);
      bool isCursor = (showCursor && col == cx_ && row == cy_);
      uint8_t fg_r = isCursor ? 0 : c.fg_r;
      uint8_t fg_g = isCursor ? 0 : c.fg_g;
      uint8_t fg_b = isCursor ? 0 : c.fg_b;
      uint8_t bg_r = isCursor ? 200 : c.bg_r;
      uint8_t bg_g = isCursor ? 200 : c.bg_g;
      uint8_t bg_b = isCursor ? 200 : c.bg_b;

      if (bg_r != prev_bg_r || bg_g != prev_bg_g || bg_b != prev_bg_b ||
          fg_r != prev_fg_r || fg_g != prev_fg_g || fg_b != prev_fg_b) {
        if (!buf.empty()) {
          canvas::write(canvas, x + buf_start, y + row, buf,
            ftxui::Color::RGB(prev_fg_r, prev_fg_g, prev_fg_b),
            ftxui::Color::RGB(prev_bg_r, prev_bg_g, prev_bg_b));
        }
        buf.clear();
        buf_start = col;
        prev_fg_r = fg_r; prev_fg_g = fg_g; prev_fg_b = fg_b;
        prev_bg_r = bg_r; prev_bg_g = bg_g; prev_bg_b = bg_b;
      }
      buf += (isCursor && c.ch == ' ') ? ' ' : c.ch;
    }
    if (!buf.empty()) {
      canvas::write(canvas, x + buf_start, y + row, buf,
        ftxui::Color::RGB(prev_fg_r, prev_fg_g, prev_fg_b),
        ftxui::Color::RGB(prev_bg_r, prev_bg_g, prev_bg_b));
    }
  }

  if (drawRows < h) {
    auto fillBg = ftxui::Color::RGB(10, 10, 15);
    canvas::fill(canvas, x, y + drawRows, w, h - drawRows, fillBg);
  }
}

// ---- AppRunner ----

AppRunner::AppRunner(const std::string& command, const std::string& appName)
  : command_(command), name_(appName), term_(80, 24) {}

AppRunner::~AppRunner() { stop(); }

void AppRunner::start() {
  if (running_) return;
  running_ = true;

#ifdef _WIN32
  worker_ = std::thread([this]() { readOutput(); });
#else
  child_pid_ = forkpty(&master_fd_, nullptr, nullptr, nullptr);
  if (child_pid_ == 0) {
    execl("/bin/sh", "sh", "-c", command_.c_str(), (char*)nullptr);
    _exit(127);
  } else if (child_pid_ > 0) {
    int flags = fcntl(master_fd_, F_GETFL, 0);
    fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK);
    resizePty();
    worker_ = std::thread([this]() { readOutputPty(); });
  } else {
    running_ = false;
  }
#endif
}

void AppRunner::stop() {
  running_ = false;
#ifndef _WIN32
  if (child_pid_ > 0) {
    kill(child_pid_, SIGTERM);
    int status;
    waitpid(child_pid_, &status, WNOHANG);
    child_pid_ = -1;
  }
  if (master_fd_ >= 0) { close(master_fd_); master_fd_ = -1; }
#endif
  if (worker_.joinable()) worker_.join();
}

#ifndef _WIN32
void AppRunner::resizePty() {
  if (master_fd_ < 0) return;
  struct winsize ws;
  ws.ws_col = std::max(20, width_ - 2);
  ws.ws_row = std::max(10, height_ - 2);
  ioctl(master_fd_, TIOCSWINSZ, &ws);
}
#endif

#ifdef _WIN32
void AppRunner::readOutput() {
  FILE* fp = POPEN(command_.c_str(), "r");
  if (!fp) { running_ = false; return; }
  char buf[4096];
  std::lock_guard<std::mutex> lock(mutex_);
  while (running_ && fgets(buf, sizeof(buf), fp)) {
    term_.write(buf, strlen(buf));
  }
  PCLOSE(fp);
  running_ = false;
}
#else
void AppRunner::readOutputPty() {
  char buf[65536];
  while (running_) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(master_fd_, &fds);
    struct timeval tv = {0, 100000};
    int ret = select(master_fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ret > 0) {
      ssize_t n = read(master_fd_, buf, sizeof(buf) - 1);
      if (n > 0) {
        buf[n] = '\0';
        std::lock_guard<std::mutex> lock(mutex_);
        term_.write(buf, (int)n);
      } else if (n == 0) break;
    }
  }
  running_ = false;
}
#endif

void AppRunner::draw(ftxui::Canvas& canvas) {
  if (!visible_) return;

  int termW = std::max(20, width_ - 2);
  int termH = std::max(10, height_ - 2);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (term_.cols() != termW || term_.visibleRows() != termH)
      term_.resize(termW, termH);
  }

#ifndef _WIN32
  resizePty();
#endif

  canvas::fill(canvas, x_, y_, width_, height_, ftxui::Color::RGB(10, 10, 15));

  {
    std::lock_guard<std::mutex> lock(mutex_);
    term_.draw(canvas, x_ + 1, y_ + 1, termW, termH, running_);
  }

  if (!running_) {
    canvas::write(canvas, x_ + 1, y_ + height_ - 1,
      " [Process exited]", ftxui::Color::RGB(255, 100, 100),
      ftxui::Color::RGB(10, 10, 15));
  }
}

bool AppRunner::handleEvent(ftxui::Event event) {
  if (!visible_) return false;

  if (event == ftxui::Event::ArrowUp) {
    if (scroll_offset_ > 0) scroll_offset_--;
    return true;
  }
  if (event == ftxui::Event::ArrowDown) {
    scroll_offset_++;
    return true;
  }

#ifndef _WIN32
  if (running_ && master_fd_ >= 0) {
    if (event.is_character()) {
      auto ch = event.character();
      if (!ch.empty()) write(master_fd_, ch.c_str(), ch.size());
      return true;
    }
    if (event == ftxui::Event::Return) {
      write(master_fd_, "\n", 1);
      return true;
    }
    if (event == ftxui::Event::Backspace) {
      char bs = 127;
      write(master_fd_, &bs, 1);
      return true;
    }
    if (event == ftxui::Event::Tab) {
      write(master_fd_, "\t", 1);
      return true;
    }
  }
#endif

  return false;
}
