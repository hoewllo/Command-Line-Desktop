#pragma once
#include <ftxui/dom/canvas.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <string>

namespace canvas {

inline void fill(ftxui::Canvas& c, int x, int y, int w, int h, ftxui::Color bg) {
  for (int row = y; row < y + h; ++row) {
    if (row * 4 >= c.height()) break;
    int len = std::min(w, (c.width() - x * 2 + 1) / 2);
    if (len <= 0) break;
    c.DrawText(x * 2, row * 4, std::string(static_cast<size_t>(len), ' '),
      [bg](ftxui::Pixel& p) { p.background_color = bg; });
  }
}

inline void write(ftxui::Canvas& c, int x, int y, const std::string& text, ftxui::Color fg, ftxui::Color bg) {
  int maxLen = (c.width() - x * 2) / 2;
  auto t = static_cast<int>(text.size()) > maxLen ? text.substr(0, static_cast<size_t>(maxLen)) : text;
  c.DrawText(x * 2, y * 4, t, [fg, bg](ftxui::Pixel& p) {
    p.foreground_color = fg;
    p.background_color = bg;
  });
}

inline void write(ftxui::Canvas& c, int x, int y, const std::string& text, ftxui::Color fg) {
  write(c, x, y, text, fg, ftxui::Color::Default);
}

inline void hline(ftxui::Canvas& c, int x, int y, int w, ftxui::Color fg, ftxui::Color bg) {
  std::string line;
  for (int i = 0; i < w && i < 200; ++i) line += "\u2500";
  write(c, x, y, line, fg, bg);
}

inline void vline(ftxui::Canvas& c, int x, int y, int h, ftxui::Color fg, ftxui::Color bg) {
  for (int i = 0; i < h; ++i) {
    c.DrawText(x * 2, (y + i) * 4, "\u2502", [fg, bg](ftxui::Pixel& p) {
      p.foreground_color = fg;
      p.background_color = bg;
    });
  }
}

} // namespace canvas
