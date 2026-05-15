#pragma once
#include "core/Component.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

class WindowManager;
class StartMenu;

struct DockApp {
  std::string name;
  std::string command;
  bool is_running = false;
};

class Dock : public Component {
public:
  Dock();

  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return "Dock"; }

  void setApps(const std::vector<DockApp>& apps) { apps_ = apps; }
  void addApp(const DockApp& app) { apps_.push_back(app); }
  void removeApp(const std::string& name);
  void setWindowManager(WindowManager* wm) { wm_ = wm; }

  int dockHeight() const { return height_; }
  void setDockHeight(int h) { height_ = h; }

  std::function<void()> onStartClick;

private:
  std::vector<DockApp> apps_;
  WindowManager* wm_ = nullptr;
  int height_ = 2;
  int flash_ = 0;
};
