#include "Desktop.h"
#include "WindowManager.h"
#include "WindowFrame.h"
#include "Dock.h"
#include "StartMenu.h"
#include "CanvasHelpers.h"
#include "core/ColorUtils.h"
#include "config/ConfigLoader.h"
#include "apps/AppRunner.h"
#include "apps/FileManager.h"
#include "apps/ConfigEditor.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <algorithm>
#include <stdexcept>

Desktop::Desktop() {
  wm_ = std::make_shared<WindowManager>();
  dock_ = std::make_shared<Dock>();
  menu_ = std::make_shared<StartMenu>();

  dock_->setDockHeight(2);
  dock_->setWindowManager(wm_.get());
  menu_->setDockHeight(2);

  wm_->onWindowClosed = [this]() {
    removeClosedWindowsFromDock();
  };

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

void Desktop::populateDesktopIcons() {
  desktop_icons_.clear();
  int iconW = 14, iconH = 3;
  int gapX = 2, gapY = 1;
  int startX = 2, startY = 1;

  struct Def {
    const char* name;
    const char* command;
    bool internal;
  };
  Def defs[] = {
    {"Terminal", "xterm", false},
    {"File Manager", "", true},
    {"Calculator", "bc -l", false},
  };

  for (int i = 0; i < 3; ++i) {
    DesktopIcon icon;
    icon.name = defs[i].name;
    icon.command = defs[i].command;
    icon.internal = defs[i].internal;
    icon.x = startX + i * (iconW + gapX);
    icon.y = startY;
    icon.w = iconW;
    icon.h = iconH;
    desktop_icons_.push_back(icon);
  }
}

void Desktop::loadConfig(const Config& config) {
  current_config_ = config;
  bgColor_ = color::parseHex(config.background_color, bgColor_);
  bg_r_ = 26; bg_g_ = 26; bg_b_ = 46;
  // Parse RGB from hex for wallpaper use
  std::string h = config.background_color;
  if (!h.empty() && h[0] == '#') h = h.substr(1);
  if (h.size() >= 6) {
    try {
      bg_r_ = static_cast<uint8_t>(std::stoi(h.substr(0, 2), nullptr, 16));
      bg_g_ = static_cast<uint8_t>(std::stoi(h.substr(2, 2), nullptr, 16));
      bg_b_ = static_cast<uint8_t>(std::stoi(h.substr(4, 2), nullptr, 16));
    } catch (...) {}
  }

  dock_->setDockHeight(config.dock.height);
  dock_->setBackgroundColor(config.dock.background_color);
  dock_->setTextColor(config.dock.text_color);
  menu_->setApps(config.apps);
  populateDesktopIcons();
}

void Desktop::loadConfigFromFile() {
  try {
    ConfigLoader loader;
    auto config = loader.load(config_path_);
    loadConfig(config);
  } catch (const std::exception& e) {
    fprintf(stderr, "Error loading config: %s\n", e.what());
  }
}

void Desktop::openConfigEditor() {
  ConfigLoader loader;
  Config config;
  try {
    config = loader.load(config_path_);
  } catch (const std::exception& e) {
    fprintf(stderr, "Error loading config for editor: %s\n", e.what());
    return;
  }

  auto editor = std::make_unique<ConfigEditor>(config, config_path_);
  editor->onSaved = [this]() {
    loadConfigFromFile();
  };

  auto frame = std::make_unique<WindowFrame>(std::move(editor));
  frame->setBorderColor(current_config_.windows.border_color);
  frame->setTitleColor(current_config_.windows.title_color);
  frame->setBgColor(current_config_.windows.border_color);
  wm_->addWindow(std::move(frame));

  bool alreadyInDock = false;
  for (const auto& a : dock_->apps()) {
    if (a.name == "Config Editor") { alreadyInDock = true; break; }
  }
  if (!alreadyInDock) {
    DockApp dapp;
    dapp.name = "Config Editor";
    dapp.command = "";
    dapp.is_running = true;
    dock_->addApp(dapp);
  }
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
  frame->setBgColor(current_config_.windows.border_color);
  wm_->addWindow(std::move(frame));

  bool alreadyInDock = false;
  for (const auto& a : dock_->apps()) {
    if (a.name == name) { alreadyInDock = true; break; }
  }
  if (!alreadyInDock) {
    DockApp dapp;
    dapp.name = name;
    dapp.command = command;
    dapp.is_running = true;
    dock_->addApp(dapp);
  }
}

void Desktop::removeClosedWindowsFromDock() {
  auto wins = wm_->windows();

  std::vector<DockApp> updated;
  for (auto& app : dock_->apps()) {
    auto it = std::find_if(wins.begin(), wins.end(),
      [&](WindowFrame* w) { return w->title() == app.name; });
    if (it != wins.end()) {
      updated.push_back(app);
    }
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

void Desktop::drawWallpaper(ftxui::Canvas& c, int w, int h) {
  auto base = ftxui::Color::RGB(bg_r_, bg_g_, bg_b_);
  auto dark = ftxui::Color::RGB(
    static_cast<uint8_t>(bg_r_ * 0.7f),
    static_cast<uint8_t>(bg_g_ * 0.7f),
    static_cast<uint8_t>(bg_b_ * 0.7f));

  for (int row = 0; row < h; ++row) {
    float t = static_cast<float>(row) / static_cast<float>(std::max(1, h));
    auto col = ftxui::Color::Interpolate(t, dark, base);
    canvas::fill(c, 0, row, w, 1, col);
  }
}

void Desktop::drawDesktopIcons(ftxui::Canvas& c) {
  auto iconBg = ftxui::Color::RGB(30, 30, 55);
  auto iconFg = ftxui::Color::RGB(200, 200, 220);
  auto hoverBg = ftxui::Color::RGB(233, 69, 96);
  auto hoverFg = ftxui::Color::RGB(255, 255, 255);

  for (auto& ic : desktop_icons_) {
    bool hover = mouse_x_ >= ic.x && mouse_x_ < ic.x + ic.w &&
                 mouse_y_ >= ic.y && mouse_y_ < ic.y + ic.h;

    auto fg = hover ? hoverFg : iconFg;
    auto bg = hover ? hoverBg : iconBg;

    canvas::fill(c, ic.x + 1, ic.y + 1, ic.w - 2, ic.h - 2, bg);

    if (hover) {
      canvas::write(c, ic.x, ic.y, "\u250c", hoverBg, hoverBg);
      canvas::write(c, ic.x + ic.w - 1, ic.y, "\u2510", hoverBg, hoverBg);
      canvas::write(c, ic.x, ic.y + ic.h - 1, "\u2514", hoverBg, bg);
      canvas::write(c, ic.x + ic.w - 1, ic.y + ic.h - 1, "\u2518", hoverBg, bg);
      for (int i = 1; i < ic.w - 1; ++i) {
        canvas::write(c, ic.x + i, ic.y, "\u2500", hoverBg, hoverBg);
        canvas::write(c, ic.x + i, ic.y + ic.h - 1, "\u2500", hoverBg, bg);
      }
      for (int i = 1; i < ic.h - 1; ++i) {
        canvas::write(c, ic.x, ic.y + i, "\u2502", hoverBg, bg);
        canvas::write(c, ic.x + ic.w - 1, ic.y + i, "\u2502", hoverBg, bg);
      }
    }

    // Icon symbol
    std::string iconChar;
    if (ic.name == "Terminal") iconChar = "\u25A0";
    else if (ic.name == "File Manager") iconChar = "\u25B6";
    else if (ic.name == "Calculator") iconChar = "\u25C6";
    else iconChar = "\u25CF";

    canvas::write(c, ic.x + ic.w / 2 - 1, ic.y + 1, iconChar, fg, bg);

    // Label
    std::string label = ic.name;
    if (static_cast<int>(label.size()) > ic.w - 2)
      label = label.substr(0, static_cast<size_t>(std::max(0, ic.w - 4))) + ".";
    canvas::write(c, ic.x + (ic.w - static_cast<int>(label.size())) / 2, ic.y + 2, label, fg, bg);
  }
}

void Desktop::drawSwitcher(ftxui::Canvas& c, int sw, int sh) {
  if (!switcher_active_) return;

  auto wins = wm_->windows();
  if (wins.empty()) {
    switcher_active_ = false;
    return;
  }

  int count = static_cast<int>(wins.size());
  int boxW = std::min(50, sw - 10);
  int boxH = std::min(count + 3, 16);
  int boxX = (sw - boxW) / 2;
  int boxY = (sh - boxH) / 3;

  auto border = ftxui::Color::RGB(233, 69, 96);
  auto bg = ftxui::Color::RGB(20, 20, 40);
  auto textFg = ftxui::Color::RGB(200, 200, 220);
  auto selBg = ftxui::Color::RGB(233, 69, 96);
  auto selFg = ftxui::Color::RGB(255, 255, 255);

  canvas::fill(c, boxX + 1, boxY + 1, boxW - 2, boxH - 2, bg);

  // Border
  canvas::write(c, boxX, boxY, "\u250c", border, border);
  canvas::write(c, boxX + boxW - 1, boxY, "\u2510", border, border);
  canvas::write(c, boxX, boxY + boxH - 1, "\u2514", border, bg);
  canvas::write(c, boxX + boxW - 1, boxY + boxH - 1, "\u2518", border, bg);
  for (int i = 1; i < boxW - 1; ++i) {
    canvas::write(c, boxX + i, boxY, "\u2500", border, border);
    canvas::write(c, boxX + i, boxY + boxH - 1, "\u2500", border, bg);
  }
  for (int i = 1; i < boxH - 1; ++i) {
    canvas::write(c, boxX, boxY + i, "\u2502", border, bg);
    canvas::write(c, boxX + boxW - 1, boxY + i, "\u2502", border, bg);
  }

  int titleY = boxY + 1;
  canvas::write(c, boxX + 2, titleY, "  Window Switcher  ", border, bg);

  int visible = std::min(count, boxH - 3);
  int startIdx = 0;
  if (switcher_selected_ >= visible)
    startIdx = switcher_selected_ - visible + 1;

  for (int i = 0; i < visible; ++i) {
    int idx = startIdx + i;
    if (idx >= count) break;
    auto* win = wins[static_cast<size_t>(idx)];
    std::string winTitle = win->title();
    if (win->isMinimized())
      winTitle += " (min)";
    if (static_cast<int>(winTitle.size()) > boxW - 5)
      winTitle = winTitle.substr(0, static_cast<size_t>(boxW - 8)) + "...";

    bool isSel = (idx == switcher_selected_);
    auto itemFg = isSel ? selFg : textFg;
    auto itemBg = isSel ? selBg : bg;
    std::string prefix = isSel ? " \u25B6 " : "   ";
    canvas::write(c, boxX + 1, boxY + 2 + i, prefix + winTitle, itemFg, itemBg);
  }
}

ftxui::Element Desktop::Render() {
  auto dim = ftxui::Terminal::Size();
  if (dim.dimx == 0 || dim.dimy == 0)
    return ftxui::emptyElement();

  return ftxui::canvas(dim.dimx * 2, dim.dimy * 4,
    [this, dim](ftxui::Canvas& canvas) {
    int sw = dim.dimx, sh = dim.dimy;

    drawWallpaper(canvas, sw, sh);

    if (desktop_icons_.empty() && current_config_.apps.size() > 0)
      populateDesktopIcons();
    drawDesktopIcons(canvas);

    if (wm_) wm_->draw(canvas);
    if (menu_ && menu_->isOpen()) menu_->draw(canvas);
    if (dock_) dock_->draw(canvas);

    drawSwitcher(canvas, sw, sh);

    if (ctx_open_) {
      int cw = 22;
      int ch = static_cast<int>(ctx_items_.size()) + 2;
      int cx = (dim.dimx > cw + 1) ? std::min(ctx_x_, dim.dimx - cw - 1) : 0;
      int cy = (dim.dimy > ch + 1) ? std::min(ctx_y_, dim.dimy - ch - 1) : 0;
      auto ctxBg = ftxui::Color::RGB(35, 35, 55);
      auto ctxBorder = ftxui::Color::RGB(233, 69, 96);
      auto ctxText = ftxui::Color::RGB(224, 224, 224);

      canvas::fill(canvas, cx + 1, cy + 1, cw - 2, ch - 2, ctxBg);

      canvas::write(canvas, cx, cy, "\u250c", ctxBorder, ctxBorder);
      canvas::write(canvas, cx + cw - 1, cy, "\u2510", ctxBorder, ctxBorder);
      canvas::write(canvas, cx, cy + ch - 1, "\u2514", ctxBorder, ctxBg);
      canvas::write(canvas, cx + cw - 1, cy + ch - 1, "\u2518", ctxBorder, ctxBg);

      for (int i = 1; i < cw - 1; ++i) {
        canvas::write(canvas, cx + i, cy, "\u2500", ctxBorder, ctxBorder);
        canvas::write(canvas, cx + i, cy + ch - 1, "\u2500", ctxBorder, ctxBg);
      }
      for (int i = 1; i < ch - 1; ++i) {
        canvas::write(canvas, cx, cy + i, "\u2502", ctxBorder, ctxBg);
        canvas::write(canvas, cx + cw - 1, cy + i, "\u2502", ctxBorder, ctxBg);
      }

      for (int i = 0; i < static_cast<int>(ctx_items_.size()); ++i) {
        auto fg = (i == ctx_sel_) ? ftxui::Color::White : ctxText;
        auto bg = (i == ctx_sel_) ? ftxui::Color::RGB(233, 69, 96) : ctxBg;
        std::string label = ctx_items_[static_cast<size_t>(i)];
        if (static_cast<int>(label.size()) > cw - 3)
          label = label.substr(0, static_cast<size_t>(cw - 6)) + "...";
        canvas::write(canvas, cx + 1, cy + 1 + i, " " + label + " ", fg, bg);
      }
    }
  });
}

bool Desktop::OnEvent(ftxui::Event event) {
  // Track mouse position for icon hover
  if (event.is_mouse()) {
    mouse_x_ = event.mouse().x;
    mouse_y_ = event.mouse().y;
  }

  // ------ Context menu handling ------
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
      if (ctx_sel_ < static_cast<int>(ctx_items_.size()) - 1) ctx_sel_++;
      return true;
    }
    if (event == ftxui::Event::Return) {
      std::string sel = ctx_items_[static_cast<size_t>(ctx_sel_)];
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
        int ch = static_cast<int>(ctx_items_.size()) + 2;
        auto dim = ftxui::Terminal::Size();
        int cx = (dim.dimx > cw + 1) ? std::min(ctx_x_, dim.dimx - cw - 1) : 0;
        int cy = (dim.dimy > ch + 1) ? std::min(ctx_y_, dim.dimy - ch - 1) : 0;
        if (mouse.x >= cx && mouse.x < cx + cw && mouse.y >= cy && mouse.y < cy + ch) {
          int idx = mouse.y - cy - 1;
          if (idx >= 0 && idx < static_cast<int>(ctx_items_.size())) {
            ctx_sel_ = idx;
            std::string sel = ctx_items_[static_cast<size_t>(ctx_sel_)];
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

  // ------ Window Switcher (Alt+Tab style with F2) ------
  if (switcher_active_) {
    if (event == ftxui::Event::Escape) {
      auto wins = wm_->windows();
      if (switcher_original_focus_ >= 0 &&
          switcher_original_focus_ < static_cast<int>(wins.size())) {
        wm_->focusWindow(wins[static_cast<size_t>(switcher_original_focus_)]);
      }
      switcher_active_ = false;
      return true;
    }
    if (event == ftxui::Event::Return) {
      auto wins = wm_->windows();
      if (switcher_selected_ >= 0 &&
          switcher_selected_ < static_cast<int>(wins.size())) {
        wm_->focusWindow(wins[static_cast<size_t>(switcher_selected_)]);
      }
      switcher_active_ = false;
      return true;
    }
    if (event == ftxui::Event::Tab) {
      auto wins = wm_->windows();
      if (!wins.empty()) {
        switcher_cycle_ = true;
        switcher_selected_ = (switcher_selected_ + 1) % static_cast<int>(wins.size());
      }
      return true;
    }
    if (event == ftxui::Event::TabReverse) {
      auto wins = wm_->windows();
      if (!wins.empty()) {
        switcher_cycle_ = true;
        switcher_selected_ = (switcher_selected_ - 1 + static_cast<int>(wins.size())) % static_cast<int>(wins.size());
      }
      return true;
    }
    if (event == ftxui::Event::F2) {
      auto wins = wm_->windows();
      if (switcher_cycle_ && switcher_selected_ >= 0 &&
          switcher_selected_ < static_cast<int>(wins.size())) {
        wm_->focusWindow(wins[static_cast<size_t>(switcher_selected_)]);
      }
      switcher_active_ = false;
      return true;
    }
    if (event.is_mouse()) {
      auto& mouse = event.mouse();
      if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
        switcher_active_ = false;
        return true;
      }
    }
    return true;
  }

  // ------ Global shortcuts ------
  if (event.input() == "\x11") {
    if (screen_) screen_->Exit();
    return true;
  }

  if (event == ftxui::Event::F1) {
    menu_->toggle();
    return true;
  }

  if (event == ftxui::Event::F2) {
    // Toggle window switcher
    auto wins = wm_->windows();
    if (wins.size() > 1) {
      if (!switcher_active_) {
        switcher_original_focus_ = -1;
        for (int i = 0; i < static_cast<int>(wins.size()); ++i) {
          if (wins[static_cast<size_t>(i)]->focused()) {
            switcher_original_focus_ = i;
            break;
          }
        }
        switcher_selected_ = (switcher_original_focus_ + 1) % static_cast<int>(wins.size());
        switcher_cycle_ = false;
        switcher_active_ = true;
      } else {
        if (switcher_cycle_ && switcher_selected_ >= 0 &&
            switcher_selected_ < static_cast<int>(wins.size())) {
          wm_->focusWindow(wins[static_cast<size_t>(switcher_selected_)]);
        }
        switcher_active_ = false;
      }
    }
    return true;
  }

  if (event == ftxui::Event::F4) {
    wm_->closeFocused();
    removeClosedWindowsFromDock();
    return true;
  }

  // ------ Desktop icon clicks ------
  if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Left &&
      event.mouse().motion == ftxui::Mouse::Pressed) {
    int mx = event.mouse().x, my = event.mouse().y;
    auto dim = ftxui::Terminal::Size();
    int dockArea = dock_ ? dock_->dockHeight() : 2;
    if (my < dim.dimy - dockArea) {
      for (auto& ic : desktop_icons_) {
        if (mx >= ic.x && mx < ic.x + ic.w &&
            my >= ic.y && my < ic.y + ic.h) {
          if (ic.internal)
            launchApp(ic.name, ic.command, true);
          else
            launchApp(ic.name, ic.command);
          return true;
        }
      }
    }
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
