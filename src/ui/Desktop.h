#pragma once
#include <memory>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/color.hpp>
#include "config/ConfigLoader.h"

class WindowManager;
class Dock;
class StartMenu;

class Desktop : public ftxui::ComponentBase {
public:
  Desktop();
  ~Desktop() override = default;

  ftxui::Element Render() override;
  bool OnEvent(ftxui::Event event) override;

  WindowManager* windowManager() { return wm_.get(); }
  Dock* dock() { return dock_.get(); }
  StartMenu* startMenu() { return menu_.get(); }

  void loadConfig(const Config& config);
  void setScreen(ftxui::ScreenInteractive* s) { screen_ = s; }
  void setConfigPath(const std::string& path) { config_path_ = path; }

private:
  void launchApp(const std::string& name, const std::string& command);
  void openConfigEditor();
  void removeClosedWindowsFromDock();
  void loadConfigFromFile();

  std::shared_ptr<WindowManager> wm_;
  std::shared_ptr<Dock> dock_;
  std::shared_ptr<StartMenu> menu_;
  ftxui::ScreenInteractive* screen_ = nullptr;
  ftxui::Color bgColor_{ftxui::Color::RGB(26, 26, 46)};
  std::string config_path_ = "config.yaml";
  Config current_config_;
};
