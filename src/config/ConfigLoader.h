#pragma once
#include <string>
#include <vector>

struct AppConfig {
  std::string name;
  std::string command;
  bool internal = false;
};

struct DockConfig {
  int height = 2;
  std::string background_color = "#16213e";
  std::string text_color = "#e0e0e0";
};

struct WindowDefaults {
  int default_width = 60;
  int default_height = 30;
  std::string border_color = "#0f3460";
  std::string title_color = "#e94560";
};

struct Config {
  std::string background_color = "#1a1a2e";
  std::vector<AppConfig> apps;
  DockConfig dock;
  WindowDefaults windows;
};

class ConfigLoader {
public:
  Config load(const std::string& path);
  void save(const std::string& path, const Config& config);
};
