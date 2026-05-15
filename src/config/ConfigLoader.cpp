#include "ConfigLoader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::string ConfigLoader::trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::string ConfigLoader::stripQuotes(const std::string& s) {
  auto t = trim(s);
  if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') ||
                         (t.front() == '\'' && t.back() == '\''))) {
    return t.substr(1, t.size() - 2);
  }
  return t;
}

Config ConfigLoader::load(const std::string& path) {
  Config config;
  std::ifstream file(path);
  if (!file.is_open()) return config;

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    auto trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') continue;
    lines.push_back(line);
  }

  int i = 0;
  while (i < (int)lines.size()) {
    auto t = trim(lines[i]);
    if (t == "apps:") {
      ++i;
      while (i < (int)lines.size()) {
        auto inner = trim(lines[i]);
        if (inner[0] != '-') break;
        auto name_line = trim(lines[i]).substr(1);
        ++i;
        AppConfig app;
        while (i < (int)lines.size()) {
          auto kv = trim(lines[i]);
          if (kv.empty() || kv[0] == '-') break;
          auto colon = kv.find(':');
          if (colon != std::string::npos) {
            auto key = trim(kv.substr(0, colon));
            auto val = stripQuotes(kv.substr(colon + 1));
            if (key == "name") app.name = val;
            else if (key == "command") app.command = val;
            else if (key == "internal") app.internal = (val == "true");
          }
          ++i;
        }
        if (!app.name.empty()) config.apps.push_back(app);
      }
    } else if (t == "dock:") {
      ++i;
      while (i < (int)lines.size()) {
        auto kv = trim(lines[i]);
        if (kv.empty() || kv.back() == ':') {
          if (kv.back() == ':' && kv != "dock:") break;
        }
        auto colon = kv.find(':');
        if (colon == std::string::npos) { ++i; continue; }
        auto key = trim(kv.substr(0, colon));
        auto val = stripQuotes(kv.substr(colon + 1));
        if (key == "height") config.dock.height = std::stoi(val);
        else if (key == "background_color") config.dock.background_color = val;
        else if (key == "text_color") config.dock.text_color = val;
        ++i;
      }
    } else if (t == "windows:") {
      ++i;
      while (i < (int)lines.size()) {
        auto kv = trim(lines[i]);
        if (kv.empty() || kv.back() == ':') break;
        auto colon = kv.find(':');
        if (colon == std::string::npos) { ++i; continue; }
        auto key = trim(kv.substr(0, colon));
        auto val = stripQuotes(kv.substr(colon + 1));
        if (key == "default_width") config.windows.default_width = std::stoi(val);
        else if (key == "default_height") config.windows.default_height = std::stoi(val);
        else if (key == "border_color") config.windows.border_color = val;
        else if (key == "title_color") config.windows.title_color = val;
        ++i;
      }
    } else {
      auto colon = t.find(':');
      if (colon != std::string::npos) {
        auto key = trim(t.substr(0, colon));
        auto val = stripQuotes(t.substr(colon + 1));
        if (key == "background_color") config.background_color = val;
      }
      ++i;
    }
  }

  return config;
}
