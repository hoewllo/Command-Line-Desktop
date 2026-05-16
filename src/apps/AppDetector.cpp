#include "AppDetector.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

AppDetector::AppDetector() {}

static bool commandExistsInPath(const std::string& cmd) {
  if (cmd.empty()) return false;
  const char* path = std::getenv("PATH");
  if (!path) return false;
  std::string pathStr(path);
#ifdef _WIN32
  char sep = ';';
#else
  char sep = ':';
#endif
  std::stringstream ss(pathStr);
  std::string dir;
  while (std::getline(ss, dir, sep)) {
    if (dir.empty()) continue;
    fs::path full = fs::path(dir) / cmd;
#ifdef _WIN32
    std::string withExt = full.string() + ".exe";
    if (fs::exists(withExt) && !fs::is_directory(withExt))
      return true;
    withExt = full.string() + ".com";
    if (fs::exists(withExt) && !fs::is_directory(withExt))
      return true;
#else
    if (fs::exists(full) && !fs::is_directory(full) &&
        (fs::status(full).permissions() & fs::perms::owner_exec) != fs::perms::none)
      return true;
#endif
  }
  return false;
}

std::vector<AppConfig> AppDetector::scanPATH() {
  std::vector<AppConfig> apps;
  std::vector<std::string> known = {
    "xterm", "gnome-terminal", "konsole", "terminal",
    "firefox", "chromium", "nautilus", "thunar", "pcmanfm",
    "nano", "vim", "emacs",
    "htop", "btop", "calc", "bc",
    "Terminal", "Finder", "Safari", "TextEdit",
  };

  for (const auto& cmd : known) {
    if (commandExistsInPath(cmd)) {
      AppConfig app;
      app.name = cmd;
      app.command = cmd;
      app.internal = false;
      apps.push_back(app);
    }
  }
  return apps;
}

#ifdef __linux__
static std::vector<AppConfig> scanDesktopFilesImpl() {
  std::vector<AppConfig> apps;
  const char* dirs[] = {
    "/usr/share/applications/",
    "/usr/local/share/applications/",
    nullptr
  };

  for (const char** dir = dirs; *dir; ++dir) {
    if (!fs::is_directory(*dir)) continue;
    for (const auto& entry : fs::directory_iterator(*dir)) {
      auto path = entry.path();
      if (path.extension() != ".desktop") continue;
      std::ifstream f(path);
      std::string line;
      AppConfig app;
      bool no_display = false;
      std::string try_exec;
      while (std::getline(f, line)) {
        if (line.find("Name=") == 0 && app.name.empty())
          app.name = line.substr(5);
        if (line.find("Exec=") == 0 && app.command.empty()) {
          app.command = line.substr(5);
          auto space = app.command.find(' ');
          if (space != std::string::npos)
            app.command = app.command.substr(0, space);
          auto percent = app.command.find('%');
          if (percent != std::string::npos)
            app.command = app.command.substr(0, percent);
        }
        if (line.find("TryExec=") == 0) {
          try_exec = line.substr(8);
          auto space = try_exec.find(' ');
          if (space != std::string::npos)
            try_exec = try_exec.substr(0, space);
        }
        if (line.find("NoDisplay=true") == 0) no_display = true;
      }
      if (!try_exec.empty() && !commandExistsInPath(try_exec))
        continue;
      if (!app.name.empty() && !no_display) {
        app.internal = false;
        apps.push_back(app);
      }
    }
  }
  return apps;
}
#endif

#ifdef __APPLE__
static std::vector<AppConfig> scanMacAppsImpl() {
  std::vector<AppConfig> apps;
  const char* appDirs[] = {
    "/Applications/",
    "/System/Applications/",
    nullptr
  };
  const char* home = std::getenv("HOME");
  std::string userApps;
  if (home) {
    userApps = std::string(home) + "/Applications/";
  }

  for (const char** dir = appDirs; *dir; ++dir) {
    if (!fs::is_directory(*dir)) continue;
    for (const auto& entry : fs::directory_iterator(*dir)) {
      auto path = entry.path();
      if (path.extension() != ".app") continue;
      AppConfig app;
      app.name = path.stem().string();
      app.command = "open -a \"" + app.name + "\"";
      app.internal = false;
      apps.push_back(app);
    }
  }

  if (!userApps.empty() && fs::is_directory(userApps)) {
    for (const auto& entry : fs::directory_iterator(userApps)) {
      auto path = entry.path();
      if (path.extension() != ".app") continue;
      AppConfig app;
      app.name = path.stem().string();
      app.command = "open -a \"" + app.name + "\"";
      app.internal = false;
      apps.push_back(app);
    }
  }

  return apps;
}
#endif

std::vector<AppConfig> AppDetector::scanDesktopFiles() {
  std::vector<AppConfig> apps;
#ifdef __linux__
  auto linuxApps = scanDesktopFilesImpl();
  apps.insert(apps.end(), linuxApps.begin(), linuxApps.end());
#elif __APPLE__
  auto macApps = scanMacAppsImpl();
  apps.insert(apps.end(), macApps.begin(), macApps.end());
#endif
  return apps;
}

std::vector<AppConfig> AppDetector::scanAll() {
  auto apps = scanDesktopFiles();
  auto pathApps = scanPATH();
  apps.insert(apps.end(), pathApps.begin(), pathApps.end());
  return apps;
}
