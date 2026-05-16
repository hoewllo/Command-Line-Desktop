#pragma once
#include <string>
#include <ftxui/screen/color.hpp>

namespace color {

inline ftxui::Color parseHex(const std::string& hex, ftxui::Color fallback = ftxui::Color::RGB(200, 200, 200)) {
  std::string h = hex;
  if (!h.empty() && h[0] == '#') h = h.substr(1);
  if (h.size() >= 6) {
    try {
      auto r = std::stoi(h.substr(0, 2), nullptr, 16);
      auto g = std::stoi(h.substr(2, 2), nullptr, 16);
      auto b = std::stoi(h.substr(4, 2), nullptr, 16);
      return ftxui::Color::RGB(r, g, b);
    } catch (...) {}
  }
  return fallback;
}

}
