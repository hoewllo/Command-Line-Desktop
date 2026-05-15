#pragma once
#include <string>
#include <vector>
#include "config/ConfigLoader.h"

class AppDetector {
public:
  AppDetector();
  std::vector<AppConfig> scanPATH();
  std::vector<AppConfig> scanDesktopFiles();
  std::vector<AppConfig> scanAll();

private:
  bool commandExists(const std::string& cmd);
};
