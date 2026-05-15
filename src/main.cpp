#include "ui/Desktop.h"
#include "config/ConfigLoader.h"
#include "apps/AppDetector.h"
#include <ftxui/component/screen_interactive.hpp>
#include <memory>

int main() {
  auto desktop = std::make_shared<Desktop>();

  ConfigLoader configLoader;
  auto config = configLoader.load("config.yaml");

  AppDetector detector;
  auto detectedApps = detector.scanAll();

  for (const auto& app : detectedApps) {
    bool exists = false;
    for (const auto& existing : config.apps) {
      if (existing.name == app.name) { exists = true; break; }
    }
    if (!exists) config.apps.push_back(app);
  }

  desktop->loadConfig(config);

  auto screen = ftxui::ScreenInteractive::Fullscreen();
  screen.TrackMouse(true);

  desktop->setScreen(&screen);

  screen.Loop(desktop);

  return 0;
}
