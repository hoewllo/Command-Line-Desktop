#include "ui/Desktop.h"
#include "config/ConfigLoader.h"
#include "apps/AppDetector.h"
#include <ftxui/component/screen_interactive.hpp>
#include <memory>
#include <cstdio>

static void printHelp(const char* argv0) {
  fprintf(stderr, "Usage: %s [options]\n", argv0);
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, "  --config FILE   Path to config file (default: config.yaml)\n");
  fprintf(stderr, "  --help          Show this help\n");
}

int main(int argc, char* argv[]) {
  std::string configPath = "config.yaml";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help") {
      printHelp(argv[0]);
      return 0;
    } else if (arg == "--config" && i + 1 < argc) {
      configPath = argv[++i];
    } else {
      fprintf(stderr, "Unknown option: %s\n", arg.c_str());
      printHelp(argv[0]);
      return 1;
    }
  }

  auto desktop = std::make_shared<Desktop>();

  ConfigLoader configLoader;
  auto config = configLoader.load(configPath);

  AppDetector detector;
  auto detectedApps = detector.scanAll();

  for (const auto& app : detectedApps) {
    bool exists = false;
    for (const auto& existing : config.apps) {
      if (existing.name == app.name) { exists = true; break; }
    }
    if (!exists) config.apps.push_back(app);
  }

  desktop->setConfigPath(configPath);
  desktop->loadConfig(config);

  auto screen = ftxui::ScreenInteractive::Fullscreen();
  screen.TrackMouse(true);

  desktop->setScreen(&screen);

  screen.Loop(desktop);

  return 0;
}
