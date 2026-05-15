#include "Desktop.h"
#include "WindowManager.h"
#include "WindowFrame.h"
#include "Dock.h"
#include "StartMenu.h"
#include "CanvasHelpers.h"
#include "config/ConfigLoader.h"
#include "apps/AppRunner.h"
#include "apps/FileManager.h"
#include "apps/ConfigEditor.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <algorithm>

Desktop::Desktop() {
  wm_ = std::make_shared<WindowManager>();
  dock_ = std::make_shared<Dock>();
  menu_ = std::make_shared<StartMenu>();

  dock_->setDockHeight(2);
  dock_->setWindowManager(wm_.get());

  dock_->onStartClick = [this]() { menu_->toggle(); };

  menu_->onLaunch = [this]() {
    auto cmd = menu_->selectedCommand();
    auto name = menu_->selectedName();
    if (!name.empty()) launchApp(name, cmd);
  };

  menu_->onConfigClick = [this]() {
    openConfigEditor();
  };

  menu_->onExitClick = [this]() {
    if (screen_) screen_->Exit();
  };
}

void Desktop::loadConfig(const Config& config) {
  current_config_ = config;
  auto hex = config.background_color;
  if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);
  if (hex.size() >= 6) {
    try {
      auto r = std::stoi(hex.substr(0, 2), nullptr, 16);
      auto g = std::stoi(hex.substr(2, 2), nullptr, 16);
      auto b = std::stoi(hex.substr(4, 2), nullptr, 16);
      bgColor_ = ftxui::Color::RGB(r, g, b);
    } catch (...) {}
  }

  dock_->setDockHeight(config.dock.height);
  dock_->setBackgroundColor(config.dock.background_color);
  dock_->setTextColor(config.dock.text_color);
  menu_->setApps(config.apps);
}

void Desktop::loadConfigFromFile() {
  ConfigLoader loader;
  auto config = loader.load(config_path_);
  loadConfig(config);
}

void Desktop::openConfigEditor() {
  ConfigLoader loader;
  auto config = loader.load(config_path_);

  auto editor = std::make_unique<ConfigEditor>(config, config_path_);
  editor->onSaved = [this]() {
    loadConfigFromFile();
  };

  auto frame = std::make_unique<WindowFrame>(std::move(editor));
  frame->setBorderColor(current_config_.windows.border_color);
  frame->setTitleColor(current_config_.windows.title_color);
  frame->setPos(5, 3, 50, 22);
  wm_->addWindow(std::move(frame));
}

void Desktop::launchApp(const std::string& name, const std::string& command) {
  std::unique_ptr<Component> content;

  if (name == "File Manager") {
    content = std::make_unique<FileManager>();
  } else if (!command.empty()) {
    auto runner = std::make_unique<AppRunner>(command, name);
    runner->start();
    content = std::move(runner);
  } else {
    return;
  }

  auto frame = std::make_unique<WindowFrame>(std::move(content));
  frame->setBorderColor(current_config_.windows.border_color);
  frame->setTitleColor(current_config_.windows.title_color);
  wm_->addWindow(std::move(frame));

  DockApp dapp;
  dapp.name = name;
  dapp.command = command;
  dapp.is_running = true;
  dock_->addApp(dapp);
}

void Desktop::removeClosedWindowsFromDock() {
  auto wins = wm_->windows();
  std::vector<std::string> winNames;
  for (auto* w : wins) winNames.push_back(w->title());

  std::vector<DockApp> updated;
  for (auto& app : dock_->apps()) {
    if (std::find(winNames.begin(), winNames.end(), app.name) != winNames.end())
      updated.push_back(app);
  }
  dock_->setApps(updated);
}

ftxui::Element Desktop::Render() {
  auto dim = ftxui::Terminal::Size();
  if (dim.dimx == 0 || dim.dimy == 0)
    return ftxui::emptyElement();

  return ftxui::canvas(dim.dimx * 2, dim.dimy * 4,
    [this, dim](ftxui::Canvas& canvas) {
    canvas::fill(canvas, 0, 0, dim.dimx, dim.dimy, bgColor_);

    if (wm_) wm_->draw(canvas);
    if (menu_ && menu_->isOpen()) menu_->draw(canvas);
    if (dock_) dock_->draw(canvas);
  });
}

bool Desktop::OnEvent(ftxui::Event event) {
  if (event.input() == "\x11") {
    if (screen_) screen_->Exit();
    return true;
  }

  if (event == ftxui::Event::F1) {
    menu_->toggle();
    return true;
  }

  if (event == ftxui::Event::F4) {
    wm_->closeFocused();
    removeClosedWindowsFromDock();
    return true;
  }

  if (event.input() == "\x1b" || event.input() == "\x1b[11~") {
    wm_->closeFocused();
    removeClosedWindowsFromDock();
    return true;
  }

  if (menu_ && menu_->isOpen()) {
    if (menu_->handleEvent(event))
      return true;
  }

  if (dock_ && dock_->handleEvent(event))
    return true;

  if (wm_ && wm_->handleEvent(event))
    return true;

  return ftxui::ComponentBase::OnEvent(event);
}
