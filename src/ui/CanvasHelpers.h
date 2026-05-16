#pragma once
#include <ftxui/dom/canvas.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <string>
#include <algorithm>

namespace canvas {

inline void fill(ftxui::Canvas& c, int x, int y, int w, int h, ftxui::Color bg) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (w <= 0 || h <= 0) return;
  int canvasW = c.width();
  int canvasH = c.height();
  for (int row = y; row < y + h; ++row) {
    if (row * 4 >= canvasH) break;
    int len = std::min(w, std::max(0, (canvasW - x * 2 + 1) / 2));
    if (len <= 0) break;
    c.DrawText(x * 2, row * 4, std::string(static_cast<size_t>(len), ' '),
      [bg](ftxui::Pixel& p) { p.background_color = bg; });
  }
}

inline void write(ftxui::Canvas& c, int x, int y, const std::string& text, ftxui::Color fg, ftxui::Color bg) {
  if (x < 0 || y < 0) return;
  int canvasW = c.width();
  int maxLen = std::max(0, (canvasW - x * 2) / 2);
  auto t = static_cast<int>(text.size()) > maxLen ? text.substr(0, static_cast<size_t>(maxLen)) : text;
  if (t.empty()) return;
  c.DrawText(x * 2, y * 4, t, [fg, bg](ftxui::Pixel& p) {
    p.foreground_color = fg;
    p.background_color = bg;
  });
}

inline void write(ftxui::Canvas& c, int x, int y, const std::string& text, ftxui::Color fg) {
  write(c, x, y, text, fg, ftxui::Color::Default);
}

inline void hline(ftxui::Canvas& c, int x, int y, int w, ftxui::Color fg, ftxui::Color bg) {
  if (x < 0 || y < 0 || w <= 0) return;
  int canvasW = c.width();
  int maxW = std::max(0, (canvasW - x * 2) / 2);
  int drawW = std::min(w, maxW);
  if (drawW <= 0) return;
  std::string line;
  for (int i = 0; i < drawW; ++i) line += "\u2500";
  write(c, x, y, line, fg, bg);
}

inline void vline(ftxui::Canvas& c, int x, int y, int h, ftxui::Color fg, ftxui::Color bg) {
  if (x < 0 || y < 0 || h <= 0) return;
  int canvasH = c.height();
  int maxH = std::max(0, (canvasH - y * 4) / 4);
  int drawH = std::min(h, maxH);
  if (drawH <= 0) return;
  for (int i = 0; i < drawH; ++i) {
    c.DrawText(x * 2, (y + i) * 4, "\u2502", [fg, bg](ftxui::Pixel& p) {
      p.foreground_color = fg;
      p.background_color = bg;
    });
  }
}

} // namespace canvas
