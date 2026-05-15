#include "StartMenu.h"
#include "CanvasHelpers.h"
#include <algorithm>
#include <cctype>

StartMenu::StartMenu() {
  setPos(0, 0, 30, 0);
}

void StartMenu::updateFilter() {
  if (search_.empty()) {
    filteredApps_ = apps_;
  } else {
    filteredApps_.clear();
    std::string lower = search_;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& app : apps_) {
      std::string nameLower = app.name;
      std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
      if (nameLower.find(lower) != std::string::npos) {
        filteredApps_.push_back(app);
      }
    }
  }
  if (selected_idx_ >= (int)filteredApps_.size())
    selected_idx_ = std::max(0, (int)filteredApps_.size() - 1);
}

std::string StartMenu::selectedCommand() const {
  if (selected_idx_ >= 0 && selected_idx_ < (int)filteredApps_.size())
    return filteredApps_[selected_idx_].command;
  return "";
}

std::string StartMenu::selectedName() const {
  if (selected_idx_ >= 0 && selected_idx_ < (int)filteredApps_.size())
    return filteredApps_[selected_idx_].name;
  return "";
}

void StartMenu::draw(ftxui::Canvas& canvas) {
  if (!open_ || !visible_) return;

  int menuW = std::min(width_, 35);
  int maxItems = std::min((int)filteredApps_.size() + 2, 22);
  int menuH = maxItems + 3;

  auto bg = ftxui::Color::RGB(30, 30, 50);
  auto borderColor = ftxui::Color::RGB(233, 69, 96);
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto selectedBg = ftxui::Color::RGB(233, 69, 96);

  canvas::fill(canvas, 0, y_, menuW, menuH, bg);

  for (int cx = 0; cx < menuW; ++cx) {
    canvas::write(canvas, cx, y_, "\u2500", borderColor, borderColor);
    canvas::write(canvas, cx, y_ + menuH - 1, "\u2500", borderColor, borderColor);
  }
  for (int cy = y_; cy < y_ + menuH; ++cy) {
    canvas::write(canvas, 0, cy, "\u2502", borderColor,
      cy == y_ ? borderColor : bg);
    canvas::write(canvas, menuW - 1, cy, "\u2502", borderColor,
      cy == y_ ? borderColor : bg);
  }

  canvas::write(canvas, 2, y_ + 1, "Search: ", borderColor, bg);
  std::string displaySearch = search_;
  if ((int)displaySearch.size() > menuW - 12)
    displaySearch = displaySearch.substr(0, menuW - 15) + "...";
  canvas::write(canvas, 10, y_ + 1, displaySearch, textColor, bg);

  int itemY = y_ + 3;
  for (int i = 0; i < (int)filteredApps_.size() && itemY < y_ + menuH - 1; ++i) {
    auto& app = filteredApps_[i];
    auto fg = textColor;
    auto itemBg = bg;
    if (i == selected_idx_) {
      fg = ftxui::Color::RGB(255, 255, 255);
      itemBg = selectedBg;
    }
    std::string label = app.name;
    if ((int)label.size() > menuW - 4)
      label = label.substr(0, menuW - 7) + "...";
    canvas::write(canvas, 2, itemY, label, fg, itemBg);
    itemY++;
  }

  canvas::write(canvas, 2, y_ + menuH - 1,
    "Type to search | Enter to launch",
    ftxui::Color::RGB(150, 150, 150), bg);
}

bool StartMenu::handleEvent(ftxui::Event event) {
  if (!open_) return false;

  if (event == ftxui::Event::Escape) {
    open_ = false;
    search_ = "";
    return true;
  }

  if (event == ftxui::Event::ArrowUp) {
    if (selected_idx_ > 0) selected_idx_--;
    return true;
  }

  if (event == ftxui::Event::ArrowDown) {
    if (selected_idx_ < (int)filteredApps_.size() - 1) selected_idx_++;
    return true;
  }

  if (event == ftxui::Event::Return) {
    open_ = false;
    return true;
  }

  if (event == ftxui::Event::Backspace) {
    if (!search_.empty()) {
      search_.pop_back();
      updateFilter();
    }
    return true;
  }

  if (event.is_character()) {
    auto character = event.character();
    if (!character.empty() && character[0] >= 32 && character[0] < 127) {
      search_ += character;
      updateFilter();
    }
    return true;
  }

  return false;
}
