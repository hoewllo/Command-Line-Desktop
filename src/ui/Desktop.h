#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/color.hpp>
#include "Layer.h"
#include "config/ConfigLoader.h"

class WindowManager;
class WorkspaceManager;
class Panel;
class StartMenu;

struct DesktopIcon {
  std::string name;
  std::string command;
  bool internal = false;
  int x = 0, y = 0;
  int w = 14, h = 3;
};

struct Notification {
  std::string text;
  std::string icon;
  int lifetime = 120;  // frames remaining
  ftxui::Color color{ftxui::Color::RGB(233, 69, 96)};
};

class Desktop : public ftxui::ComponentBase {
public:
  Desktop();
  ~Desktop() override = default;

  ftxui::Element Render() override;
  bool OnEvent(ftxui::Event event) override;

  WindowManager* currentWM();
  WorkspaceManager* workspaceManager() { return ws_mgr_.get(); }
  Panel* panel() { return panel_.get(); }
  StartMenu* startMenu() { return menu_.get(); }

  void loadConfig(const Config& config);
  void setScreen(ftxui::ScreenInteractive* s) { screen_ = s; }
  void setConfigPath(const std::string& path) { config_path_ = path; }

  void showNotification(const std::string& text,
    const std::string& icon = "\u24D8",
    ftxui::Color color = ftxui::Color::RGB(233, 69, 96));

private:
  void launchApp(const std::string& name, const std::string& command, bool internal = false);
  void openConfigEditor();
  void loadConfigFromFile();
  void openContextMenu(int mx, int my);
  void closeContextMenu();
  void showPowerDialog();
  void drawNotifications(ftxui::Canvas& c, int w, int h);

  void populateDesktopIcons();
  void drawWallpaper(ftxui::Canvas& c, int w, int h);
  void drawDesktopIcons(ftxui::Canvas& c);
  void drawSwitcher(ftxui::Canvas& c, int sw, int sh);
  void syncPanelWM();

  Compositor compositor_;
  void setupCompositor();
  void rebuildCompositor();

  std::shared_ptr<WorkspaceManager> ws_mgr_;
  std::shared_ptr<Panel> panel_;
  std::shared_ptr<StartMenu> menu_;
  ftxui::ScreenInteractive* screen_ = nullptr;
  ftxui::Color bgColor_{ftxui::Color::RGB(26, 26, 46)};
  uint8_t bg_r_ = 26, bg_g_ = 26, bg_b_ = 46;
  std::string config_path_ = "config.yaml";
  Config current_config_;

  bool ctx_open_ = false;
  int ctx_x_ = 0, ctx_y_ = 0;
  int ctx_sel_ = 0;
  std::vector<std::string> ctx_items_;

  // Power dialog
  bool power_dialog_ = false;
  int power_sel_ = 0;

  int mouse_x_ = -1, mouse_y_ = -1;

  std::vector<DesktopIcon> desktop_icons_;
  bool switcher_active_ = false;
  bool switcher_cycle_ = false;
  int switcher_selected_ = 0;
  int switcher_original_focus_ = -1;

  // Notifications
  std::vector<Notification> notifications_;
  bool notify_layer_dirty_ = true;
};
