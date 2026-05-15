#pragma once
#include "core/Component.h"
#include "config/ConfigLoader.h"
#include <string>
#include <vector>
#include <functional>

class StartMenu : public Component {
public:
  StartMenu();
  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return "Start Menu"; }

  void setApps(const std::vector<AppConfig>& apps) { apps_ = apps; }
  void toggle() { open_ = !open_; }
  void open() { open_ = true; }
  void close() { open_ = false; }
  bool isOpen() const { return open_; }

  std::string selectedCommand() const;
  std::string selectedName() const;

  std::function<void()> onLaunch;

private:
  std::vector<AppConfig> apps_;
  std::vector<AppConfig> filteredApps_;
  std::string search_;
  int selected_idx_ = 0;
  bool open_ = false;

  void updateFilter();
  int hitTest(int mx, int my) const;
  int menuX_ = 0, menuY_ = 0, menuW_ = 0, menuH_ = 0;
};
