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
    auto internal = menu_->selectedIsInternal();
    if (!name.empty()) launchApp(name, cmd, internal);
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

void Desktop::launchApp(const std::string& name, const std::string& command, bool internal) {
  std::unique_ptr<Component> content;

  if (internal && name == "File Manager") {
    content = std::make_unique<FileManager>();
  } else if (internal && name == "Config Editor") {
    openConfigEditor();
    return;
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

void Desktop::openContextMenu(int mx, int my) {
  ctx_items_ = {
    "Terminal",
    "File Manager",
    "Edit Config",
    "Reload Config",
    "Exit",
  };
  ctx_x_ = mx;
  ctx_y_ = my;
  ctx_sel_ = 0;
  ctx_open_ = true;
}

void Desktop::closeContextMenu() {
  ctx_open_ = false;
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

    if (ctx_open_) {
      int cw = 22;
      int ch = (int)ctx_items_.size() + 2;
      int cx = std::min(ctx_x_, dim.dimx - cw - 1);
      int cy = std::min(ctx_y_, dim.dimy - ch - 1);
      auto ctxBg = ftxui::Color::RGB(35, 35, 55);
      auto ctxBorder = ftxui::Color::RGB(233, 69, 96);
      auto ctxText = ftxui::Color::RGB(224, 224, 224);
      canvas::fill(canvas, cx, cy, cw, ch, ctxBg);
      for (int i = 0; i < cw; ++i) {
        canvas::write(canvas, cx + i, cy, "\u2500", ctxBorder, ctxBorder);
        canvas::write(canvas, cx + i, cy + ch - 1, "\u2500", ctxBorder, ctxBorder);
      }
      for (int i = 0; i < ch; ++i) {
        canvas::write(canvas, cx, cy + i, "\u2502", ctxBorder,
          i == 0 ? ctxBorder : ctxBg);
        canvas::write(canvas, cx + cw - 1, cy + i, "\u2502", ctxBorder,
          i == 0 ? ctxBorder : ctxBg);
      }
      for (int i = 0; i < (int)ctx_items_.size(); ++i) {
        auto fg = (i == ctx_sel_) ? ftxui::Color::White : ctxText;
        auto bg = (i == ctx_sel_) ? ftxui::Color::RGB(233, 69, 96) : ctxBg;
        std::string label = ctx_items_[i];
        if ((int)label.size() > cw - 3) label = label.substr(0, cw - 6) + "...";
        canvas::write(canvas, cx + 1, cy + 1 + i, " " + label + " ", fg, bg);
      }
    }
  });
}

bool Desktop::OnEvent(ftxui::Event event) {
  if (ctx_open_) {
    if (event == ftxui::Event::Escape) {
      closeContextMenu();
      return true;
    }
    if (event == ftxui::Event::ArrowUp) {
      if (ctx_sel_ > 0) ctx_sel_--;
      return true;
    }
    if (event == ftxui::Event::ArrowDown) {
      if (ctx_sel_ < (int)ctx_items_.size() - 1) ctx_sel_++;
      return true;
    }
    if (event == ftxui::Event::Return) {
      std::string sel = ctx_items_[ctx_sel_];
      closeContextMenu();
      if (sel == "Terminal") launchApp("Terminal", "xterm");
      else if (sel == "File Manager") launchApp("File Manager", "", true);
      else if (sel == "Edit Config") openConfigEditor();
      else if (sel == "Reload Config") loadConfigFromFile();
      else if (sel == "Exit" && screen_) screen_->Exit();
      return true;
    }
    if (event.is_mouse()) {
      auto& mouse = event.mouse();
      if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
        int cw = 22;
        int ch = (int)ctx_items_.size() + 2;
        auto dim = ftxui::Terminal::Size();
        int cx = std::min(ctx_x_, dim.dimx - cw - 1);
        int cy = std::min(ctx_y_, dim.dimy - ch - 1);
        if (mouse.x >= cx && mouse.x < cx + cw && mouse.y >= cy && mouse.y < cy + ch) {
          int idx = mouse.y - cy - 1;
          if (idx >= 0 && idx < (int)ctx_items_.size()) {
            ctx_sel_ = idx;
            std::string sel = ctx_items_[ctx_sel_];
            closeContextMenu();
            if (sel == "Terminal") launchApp("Terminal", "xterm");
            else if (sel == "File Manager") launchApp("File Manager", "", true);
            else if (sel == "Edit Config") openConfigEditor();
            else if (sel == "Reload Config") loadConfigFromFile();
            else if (sel == "Exit" && screen_) screen_->Exit();
          }
        } else {
          closeContextMenu();
        }
        return true;
      }
    }
    return true;
  }

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

  if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Right &&
      event.mouse().motion == ftxui::Mouse::Pressed) {
    openContextMenu(event.mouse().x, event.mouse().y);
    return true;
  }

  return ftxui::ComponentBase::OnEvent(event);
}
