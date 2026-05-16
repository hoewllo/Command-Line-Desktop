#pragma once
#include <string>
#include <stdexcept>
#include <cstdio>
#include <ftxui/screen/color.hpp>

namespace color {

inline ftxui::Color parseHex(const std::string& hex, ftxui::Color fallback = ftxui::Color::RGB(200, 200, 200)) {
  std::string h = hex;
  if (!h.empty() && h[0] == '#') h = h.substr(1);

  if (h.size() == 3) {
    h = std::string(2, h[0]) + std::string(2, h[1]) + std::string(2, h[2]);
  }

  if (h.size() >= 8) {
    h = h.substr(0, 6);
  }

  if (h.size() >= 6) {
    try {
      auto r = std::stoi(h.substr(0, 2), nullptr, 16);
      auto g = std::stoi(h.substr(2, 2), nullptr, 16);
      auto b = std::stoi(h.substr(4, 2), nullptr, 16);
      return ftxui::Color::RGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    } catch (const std::invalid_argument&) {
      fprintf(stderr, "Warning: invalid hex color: %s\n", hex.c_str());
    } catch (const std::out_of_range&) {
      fprintf(stderr, "Warning: hex color out of range: %s\n", hex.c_str());
    }
  }
  return fallback;
}

}
