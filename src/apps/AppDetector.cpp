#include "AppDetector.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unistd.h>

AppDetector::AppDetector() {}

bool AppDetector::commandExists(const std::string& cmd) {
  if (cmd.empty()) return false;
  std::string check = "command -v " + cmd + " >/dev/null 2>&1";
  return (std::system(check.c_str()) == 0);
}

std::vector<AppConfig> AppDetector::scanPATH() {
  std::vector<AppConfig> apps;
  const char* path = std::getenv("PATH");
  if (!path) return apps;

  std::string pathStr(path);
  std::stringstream ss(pathStr);
  std::string dir;
  std::vector<std::string> known = {
    "xterm", "gnome-terminal", "konsole", "terminal", "firefox",
    "chromium", "nautilus", "thunar", "pcmanfm", "nano", "vim",
    "emacs", "htop", "btop", "calc", "bc"
  };

  for (const auto& cmd : known) {
    if (commandExists(cmd)) {
      AppConfig app;
      app.name = cmd;
      app.command = cmd;
      app.internal = false;
      apps.push_back(app);
    }
  }
  return apps;
}

std::vector<AppConfig> AppDetector::scanDesktopFiles() {
  std::vector<AppConfig> apps;
  const char* dirs[] = {
    "/usr/share/applications/",
    "/usr/local/share/applications/",
    nullptr
  };

  for (const char** dir = dirs; *dir; ++dir) {
    std::string cmd = "ls " + std::string(*dir) + "*.desktop 2>/dev/null | head -50";
    std::string result;
    char buf[4096];
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) continue;
    while (fgets(buf, sizeof(buf), fp)) {
      std::string filepath(buf);
      filepath.erase(filepath.find_last_not_of("\n") + 1);
      std::ifstream f(filepath);
      std::string line;
      AppConfig app;
      bool no_display = false;
      while (std::getline(f, line)) {
        if (line.find("Name=") == 0 && app.name.empty()) {
          app.name = line.substr(5);
        }
        if (line.find("Exec=") == 0 && app.command.empty()) {
          app.command = line.substr(5);
          auto space = app.command.find(' ');
          if (space != std::string::npos)
            app.command = app.command.substr(0, space);
          auto percent = app.command.find('%');
          if (percent != std::string::npos)
            app.command = app.command.substr(0, percent);
        }
        if (line.find("NoDisplay=true") == 0) no_display = true;
      }
      if (!app.name.empty() && !no_display) {
        app.internal = false;
        apps.push_back(app);
      }
    }
    pclose(fp);
  }

  return apps;
}

std::vector<AppConfig> AppDetector::scanAll() {
  auto apps = scanDesktopFiles();
  auto pathApps = scanPATH();
  apps.insert(apps.end(), pathApps.begin(), pathApps.end());
  return apps;
}
