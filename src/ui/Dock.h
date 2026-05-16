#pragma once
#include "core/Component.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <ftxui/screen/color.hpp>

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

  const std::vector<DockApp>& apps() const { return apps_; }
  void setApps(const std::vector<DockApp>& apps) { apps_ = apps; }
  void addApp(const DockApp& app) { apps_.push_back(app); }
  void removeApp(const std::string& name);
  void setWindowManager(WindowManager* wm) { wm_ = wm; }

  int dockHeight() const { return height_; }
  void setDockHeight(int h) { height_ = h; }

  void setBackgroundColor(const std::string& hex);
  void setTextColor(const std::string& hex);

  std::function<void()> onStartClick;

private:
  std::vector<DockApp> apps_;
  WindowManager* wm_ = nullptr;
  int height_ = 2;
  int flash_ = 0;
  ftxui::Color bgColor_{ftxui::Color::RGB(22, 33, 62)};
  ftxui::Color textColor_{ftxui::Color::RGB(224, 224, 224)};

};
