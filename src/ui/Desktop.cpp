#include "Desktop.h"
#include "WindowManager.h"
#include "WindowFrame.h"
#include "Panel.h"
#include "StartMenu.h"
#include "WorkspaceManager.h"
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
  ws_mgr_ = std::make_shared<WorkspaceManager>(4);
  panel_ = std::make_shared<Panel>();
  menu_ = std::make_shared<StartMenu>();

  panel_->setPanelHeight(2);
  syncPanelWM();
  menu_->setDockHeight(2);

  // Forward onWindowClosed from all workspaces
  auto onWinClosed = [this]() {
    syncPanelWM();
  };
  for (int i = 0; i < ws_mgr_->count(); ++i) {
    auto* wm = ws_mgr_->getWM(i);
    if (wm) wm->onWindowClosed = onWinClosed;
  }

  ws_mgr_->onWorkspaceChanged = [this](int idx) {
    if (panel_) {
      panel_->setWorkspace(idx, ws_mgr_->count());
      syncPanelWM();
    }
  };
  // Initialize workspace indicator
  if (panel_) panel_->setWorkspace(0, ws_mgr_->count());

  setupCompositor();
}

void Desktop::setupCompositor() {
  compositor_.addLayer(std::make_unique<SimpleLayer>(
    "wallpaper", [this](ftxui::Canvas& c) {
      auto dim = ftxui::Terminal::Size();
      drawWallpaper(c, dim.dimx, dim.dimy);
    }), 0);

  compositor_.addLayer(std::make_unique<SimpleLayer>(
    "icons", [this](ftxui::Canvas& c) {
      if (desktop_icons_.empty()) populateDesktopIcons();
      drawDesktopIcons(c);
    }), 10);

  compositor_.addLayer(std::make_unique<SimpleLayer>(
    "windows", [this](ftxui::Canvas& c) {
      auto* wm = currentWM();
      if (wm) wm->draw(c);
    }), 20);

  compositor_.addLayer(std::make_unique<SimpleLayer>(
    "menu", [this](ftxui::Canvas& c) {
      if (menu_ && menu_->isOpen()) menu_->draw(c);
    }), 30);

  compositor_.addLayer(std::make_unique<SimpleLayer>(
    "notifications", [this](ftxui::Canvas& c) {
      auto dim = ftxui::Terminal::Size();
      drawNotifications(c, dim.dimx, dim.dimy);
    }), 70);

  compositor_.addLayer(std::make_unique<SimpleLayer>(
    "panel", [this](ftxui::Canvas& c) {
      if (panel_) panel_->draw(c);
    }), 50);

  compositor_.addLayer(std::make_unique<SimpleLayer>(
    "switcher", [this](ftxui::Canvas& c) {
      auto dim = ftxui::Terminal::Size();
      drawSwitcher(c, dim.dimx, dim.dimy);
    }), 100);
}

WindowManager* Desktop::currentWM() {
  return ws_mgr_ ? ws_mgr_->currentWM() : nullptr;
}

void Desktop::syncPanelWM() {
  if (panel_ && ws_mgr_)
    panel_->setWindowManager(ws_mgr_->currentWM());
}

void Desktop::populateDesktopIcons() {
  desktop_icons_.clear();
  int iconW = 14, iconH = 3;
  int gapX = 2;
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
  std::string h = config.background_color;
  if (!h.empty() && h[0] == '#') h = h.substr(1);
  if (h.size() >= 6) {
    try {
      bg_r_ = static_cast<uint8_t>(std::stoi(h.substr(0, 2), nullptr, 16));
      bg_g_ = static_cast<uint8_t>(std::stoi(h.substr(2, 2), nullptr, 16));
      bg_b_ = static_cast<uint8_t>(std::stoi(h.substr(4, 2), nullptr, 16));
    } catch (...) {}
  }

  panel_->setPanelHeight(config.dock.height);
  panel_->setBackgroundColor(config.dock.background_color);
  panel_->setTextColor(config.dock.text_color);
  menu_->setDockHeight(config.dock.height);
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
  auto* wm = currentWM();
  if (wm) wm->addWindow(std::move(frame));
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
  auto* wm = currentWM();
  if (wm) wm->addWindow(std::move(frame));
}

void Desktop::showNotification(const std::string& text,
    const std::string& icon, ftxui::Color color) {
  Notification n;
  n.text = text;
  n.icon = icon;
  n.color = color;
  n.lifetime = 150;
  notifications_.push_back(n);
  if (notifications_.size() > 5)
    notifications_.erase(notifications_.begin());
}

void Desktop::drawNotifications(ftxui::Canvas& c, int w, int h) {
  // Tick notification lifetimes
  for (auto it = notifications_.begin(); it != notifications_.end(); ) {
    it->lifetime--;
    if (it->lifetime <= 0) it = notifications_.erase(it);
    else ++it;
  }
  if (notifications_.empty()) return;

  auto notifBg = ftxui::Color::RGB(25, 25, 50);
  auto textFg = ftxui::Color::RGB(224, 224, 224);

  int ny = 2;
  for (size_t i = 0; i < notifications_.size(); ++i) {
    auto& n = notifications_[i];
    int alpha = std::min(255, n.lifetime * 4);
    auto bg = n.lifetime < 20 ? ftxui::Color::RGB(
      static_cast<uint8_t>(25 * alpha / 255),
      static_cast<uint8_t>(25 * alpha / 255),
      static_cast<uint8_t>(50 * alpha / 255)) : notifBg;

    std::string msg = " " + n.icon + " " + n.text + " ";
    int msgW = static_cast<int>(msg.size());
    int nx = std::max(0, w - msgW - 4);

    // Draw notification box
    canvas::fill(c, nx, ny, msgW + 2, 3, bg);
    canvas::write(c, nx, ny, "\u250c", n.color, n.color);
    canvas::write(c, nx + msgW + 1, ny, "\u2510", n.color, n.color);
    canvas::write(c, nx, ny + 2, "\u2514", n.color, bg);
    canvas::write(c, nx + msgW + 1, ny + 2, "\u2518", n.color, bg);
    for (int ix = 1; ix <= msgW; ++ix) {
      canvas::write(c, nx + ix, ny, "\u2500", n.color, n.color);
      canvas::write(c, nx + ix, ny + 2, "\u2500", n.color, bg);
    }
    canvas::write(c, nx, ny + 1, "\u2502", n.color, bg);
    canvas::write(c, nx + msgW + 1, ny + 1, "\u2502", n.color, bg);
    canvas::write(c, nx + 1, ny + 1, msg, textFg, bg);

    ny += 4;
    if (ny + 4 > h) break;
  }
}

void Desktop::openContextMenu(int mx, int my) {
  ctx_items_ = {
    "Terminal",
    "File Manager",
    "Edit Config",
    "Reload Config",
    "Power...",
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

void Desktop::showPowerDialog() {
  power_dialog_ = true;
  power_sel_ = 0;
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
    std::string iconChar;
    if (ic.name == "Terminal") iconChar = "\u25A0";
    else if (ic.name == "File Manager") iconChar = "\u25B6";
    else if (ic.name == "Calculator") iconChar = "\u25C6";
    else iconChar = "\u25CF";
    canvas::write(c, ic.x + ic.w / 2 - 1, ic.y + 1, iconChar, fg, bg);
    std::string label = ic.name;
    if (static_cast<int>(label.size()) > ic.w - 2)
      label = label.substr(0, static_cast<size_t>(std::max(0, ic.w - 4))) + ".";
    canvas::write(c, ic.x + (ic.w - static_cast<int>(label.size())) / 2, ic.y + 2, label, fg, bg);
  }
}

void Desktop::drawSwitcher(ftxui::Canvas& c, int sw, int sh) {
  if (!switcher_active_) return;
  auto* wm = currentWM();
  if (!wm) { switcher_active_ = false; return; }
  auto wins = wm->windows();
  if (wins.empty()) { switcher_active_ = false; return; }

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
  canvas::write(c, boxX + 2, boxY + 1, "  Window Switcher  ", border, bg);

  int visible = std::min(count, boxH - 3);
  int startIdx = 0;
  if (switcher_selected_ >= visible) startIdx = switcher_selected_ - visible + 1;
  for (int i = 0; i < visible; ++i) {
    int idx = startIdx + i;
    if (idx >= count) break;
    auto* win = wins[static_cast<size_t>(idx)];
    std::string winTitle = win->title();
    if (win->isMinimized()) winTitle += " (min)";
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
    compositor_.draw(canvas);

    // Context menu drawn last (highest z) since it's a quick overlay
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

    // Power dialog
    if (power_dialog_) {
      int pw = 28, ph = 6;
      int px = (dim.dimx - pw) / 2;
      int py = (dim.dimy - ph) / 3;
      auto pBg = ftxui::Color::RGB(30, 30, 55);
      auto pBorder = ftxui::Color::RGB(233, 69, 96);
      auto pText = ftxui::Color::RGB(200, 200, 220);
      auto pSelBg = ftxui::Color::RGB(233, 69, 96);
      auto pSelFg = ftxui::Color::RGB(255, 255, 255);
      canvas::fill(canvas, px + 1, py + 1, pw - 2, ph - 2, pBg);
      for (int i = 0; i < pw; ++i) {
        canvas::write(canvas, px + i, py, "\u2500", pBorder, pBorder);
        canvas::write(canvas, px + i, py + ph - 1, "\u2500", pBorder, pBg);
      }
      for (int i = 0; i < ph; ++i) {
        canvas::write(canvas, px, py + i, "\u2502", pBorder, i == 0 ? pBorder : pBg);
        canvas::write(canvas, px + pw - 1, py + i, "\u2502", pBorder, i == 0 ? pBorder : pBg);
      }
      canvas::write(canvas, px, py, "\u250c", pBorder, pBorder);
      canvas::write(canvas, px + pw - 1, py, "\u2510", pBorder, pBorder);
      canvas::write(canvas, px, py + ph - 1, "\u2514", pBorder, pBg);
      canvas::write(canvas, px + pw - 1, py + ph - 1, "\u2518", pBorder, pBg);
      canvas::write(canvas, px + 2, py + 1, " Power Off ", ftxui::Color::RGB(255, 255, 255), pBorder);
      const char* powerOpts[] = {"Log Out", "Restart", "Shutdown", "Cancel"};
      for (int i = 0; i < 4; ++i) {
        auto fg = (i == power_sel_) ? pSelFg : pText;
        auto bg = (i == power_sel_) ? pSelBg : pBg;
        std::string opt = powerOpts[i];
        canvas::write(canvas, px + 2, py + 2 + i, " " + opt + " ", fg, bg);
      }
    }
  });
}

bool Desktop::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) {
    mouse_x_ = event.mouse().x;
    mouse_y_ = event.mouse().y;
  }

  auto* wm = currentWM();

  // ------ Context menu ------
  if (ctx_open_) {
    if (event == ftxui::Event::Escape) { closeContextMenu(); return true; }
    if (event == ftxui::Event::ArrowUp) { if (ctx_sel_ > 0) ctx_sel_--; return true; }
    if (event == ftxui::Event::ArrowDown) { if (ctx_sel_ < static_cast<int>(ctx_items_.size()) - 1) ctx_sel_++; return true; }
    if (event == ftxui::Event::Return) {
      std::string sel = ctx_items_[static_cast<size_t>(ctx_sel_)];
      closeContextMenu();
      if (sel == "Terminal") launchApp("Terminal", "xterm");
      else if (sel == "File Manager") launchApp("File Manager", "", true);
      else if (sel == "Edit Config") openConfigEditor();
      else if (sel == "Reload Config") loadConfigFromFile();
      else if (sel == "Power...") showPowerDialog();
      else if (sel == "Exit") {
        showNotification("Goodbye!", "\u23FB", ftxui::Color::RGB(100, 100, 255));
        if (screen_) screen_->Exit();
      }
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
            else if (sel == "Power...") showPowerDialog();
            else if (sel == "Exit") {
              showNotification("Goodbye!", "\u23FB", ftxui::Color::RGB(100, 100, 255));
              if (screen_) screen_->Exit();
            }
          }
        } else { closeContextMenu(); }
        return true;
      }
    }
    return true;
  }

  // ------ Power dialog ------
  if (power_dialog_) {
    if (event == ftxui::Event::Escape) { power_dialog_ = false; return true; }
    if (event == ftxui::Event::ArrowUp && power_sel_ > 0) { power_sel_--; return true; }
    if (event == ftxui::Event::ArrowDown && power_sel_ < 3) { power_sel_++; return true; }
    if (event == ftxui::Event::Return) {
      power_dialog_ = false;
      if (power_sel_ == 0) {
        showNotification("Logging out...", "\u23FB", ftxui::Color::RGB(255, 200, 100));
      } else if (power_sel_ == 1) {
        showNotification("Restarting...", "\u21BB", ftxui::Color::RGB(255, 200, 100));
        if (screen_) screen_->Exit();
      } else if (power_sel_ == 2) {
        showNotification("Shutting down...", "\u2B1C", ftxui::Color::RGB(255, 100, 100));
        if (screen_) screen_->Exit();
      } else if (power_sel_ == 3) {
        // Cancel
      }
      return true;
    }
    if (event.is_mouse()) {
      auto& mouse = event.mouse();
      if (mouse.motion == ftxui::Mouse::Pressed && mouse.button == ftxui::Mouse::Left) {
        auto dim = ftxui::Terminal::Size();
        int pw = 28, ph = 6;
        int px = (dim.dimx - pw) / 2;
        int py = (dim.dimy - ph) / 3;
        if (mouse.x >= px && mouse.x < px + pw && mouse.y >= py + 2 && mouse.y < py + ph - 1) {
          power_sel_ = mouse.y - py - 2;
          if (power_sel_ >= 0 && power_sel_ <= 3) {
            power_dialog_ = false;
            if (power_sel_ == 0) showNotification("Logging out...", "\u23FB", ftxui::Color::RGB(255, 200, 100));
            else if (power_sel_ == 1) { showNotification("Restarting...", "\u21BB", ftxui::Color::RGB(255, 200, 100)); if (screen_) screen_->Exit(); }
            else if (power_sel_ == 2) { showNotification("Shutting down...", "\u2B1C", ftxui::Color::RGB(255, 100, 100)); if (screen_) screen_->Exit(); }
          }
        } else { power_dialog_ = false; }
        return true;
      }
    }
    return true;
  }

  // ------ Switcher ------
  if (switcher_active_) {
    if (event == ftxui::Event::Escape) {
      if (wm) {
        auto wins = wm->windows();
        if (switcher_original_focus_ >= 0 && switcher_original_focus_ < static_cast<int>(wins.size()))
          wm->focusWindow(wins[static_cast<size_t>(switcher_original_focus_)]);
      }
      switcher_active_ = false;
      return true;
    }
    if (event == ftxui::Event::Return) {
      if (wm) {
        auto wins = wm->windows();
        if (switcher_selected_ >= 0 && switcher_selected_ < static_cast<int>(wins.size()))
          wm->focusWindow(wins[static_cast<size_t>(switcher_selected_)]);
      }
      switcher_active_ = false;
      return true;
    }
    if (event == ftxui::Event::Tab) {
      if (wm) {
        auto wins = wm->windows();
        if (!wins.empty())
          switcher_selected_ = (switcher_selected_ + 1) % static_cast<int>(wins.size());
      }
      return true;
    }
    if (event == ftxui::Event::TabReverse) {
      if (wm) {
        auto wins = wm->windows();
        if (!wins.empty())
          switcher_selected_ = (switcher_selected_ - 1 + static_cast<int>(wins.size())) % static_cast<int>(wins.size());
      }
      return true;
    }
    if (event == ftxui::Event::F2) {
      if (switcher_cycle_ && wm) {
        auto wins = wm->windows();
        if (switcher_selected_ >= 0 && switcher_selected_ < static_cast<int>(wins.size()))
          wm->focusWindow(wins[static_cast<size_t>(switcher_selected_)]);
      }
      switcher_active_ = false;
      return true;
    }
    if (event.is_mouse() && event.mouse().motion == ftxui::Mouse::Pressed && event.mouse().button == ftxui::Mouse::Left) {
      switcher_active_ = false;
      return true;
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
    if (!wm) return true;
    auto wins = wm->windows();
    if (wins.size() > 1) {
      if (!switcher_active_) {
        switcher_original_focus_ = -1;
        for (int i = 0; i < static_cast<int>(wins.size()); ++i)
          if (wins[static_cast<size_t>(i)]->focused()) { switcher_original_focus_ = i; break; }
        switcher_selected_ = (switcher_original_focus_ + 1) % static_cast<int>(wins.size());
        switcher_cycle_ = false;
        switcher_active_ = true;
      } else {
        if (switcher_cycle_ && switcher_selected_ >= 0 && switcher_selected_ < static_cast<int>(wins.size()))
          wm->focusWindow(wins[static_cast<size_t>(switcher_selected_)]);
        switcher_active_ = false;
      }
    }
    return true;
  }

  if (event == ftxui::Event::F4) {
    if (wm) wm->closeFocused();
    syncPanelWM();
    return true;
  }

  // ------ Workspace switching: Alt+1..4 ------
  if (event.is_character()) {
    auto ch = event.character();
    if (ch.size() == 1 && ch[0] >= '1' && ch[0] <= '4') {
      // Check for Alt modifier: input string starts with ESC
      if (event.input().size() > 1 && event.input()[0] == '\x1b') {
        int ws = ch[0] - '1';
        ws_mgr_->switchTo(ws);
        syncPanelWM();
        return true;
      }
    }
  }

  // Ctrl+Alt+Left / Ctrl+Alt+Right → switch workspace
  // These come through escape sequences
  if (event.input() == "\x1b\x1b\x5b\x44" ||  // ESC ESC [ D
      event.input() == "\x1b\x5b\x31\x3b\x35\x44") {  // ESC [ 1 ; 5 D
    int next = (ws_mgr_->currentIndex() - 1 + ws_mgr_->count()) % ws_mgr_->count();
    ws_mgr_->switchTo(next);
    syncPanelWM();
    return true;
  }
  if (event.input() == "\x1b\x1b\x5b\x43" ||
      event.input() == "\x1b\x5b\x31\x3b\x35\x43") {
    int next = (ws_mgr_->currentIndex() + 1) % ws_mgr_->count();
    ws_mgr_->switchTo(next);
    syncPanelWM();
    return true;
  }

  // ------ Desktop icons ------
  if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Left &&
      event.mouse().motion == ftxui::Mouse::Pressed) {
    int mx = event.mouse().x, my = event.mouse().y;
    auto dim = ftxui::Terminal::Size();
    int panelH = panel_ ? panel_->panelHeight() : 2;
    if (my < dim.dimy - panelH) {
      for (auto& ic : desktop_icons_) {
        if (mx >= ic.x && mx < ic.x + ic.w && my >= ic.y && my < ic.y + ic.h) {
          if (ic.internal) launchApp(ic.name, ic.command, true);
          else launchApp(ic.name, ic.command);
          return true;
        }
      }
    }
  }

  if (menu_ && menu_->isOpen()) {
    if (menu_->handleEvent(event)) return true;
  }

  if (panel_ && panel_->handleEvent(event)) return true;

  if (wm && wm->handleEvent(event)) return true;

  if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Right &&
      event.mouse().motion == ftxui::Mouse::Pressed) {
    openContextMenu(event.mouse().x, event.mouse().y);
    return true;
  }

  return ftxui::ComponentBase::OnEvent(event);
}
