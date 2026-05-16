#include "ConfigEditor.h"
#include "ui/CanvasHelpers.h"
#include "core/ColorUtils.h"
#include "config/ConfigLoader.h"
#include <algorithm>
#include <stdexcept>

enum FieldIndex {
  FIELD_BACKGROUND = 0,
  FIELD_DOCK_HEIGHT = 1,
  FIELD_DOCK_BG = 2,
  FIELD_DOCK_TEXT = 3,
  FIELD_WIN_WIDTH = 4,
  FIELD_WIN_HEIGHT = 5,
  FIELD_WIN_BORDER = 6,
  FIELD_WIN_TITLE = 7
};

ConfigEditor::ConfigEditor(const Config& config, const std::string& configPath)
  : config_(config), config_path_(configPath) {
  setupFields();
}

void ConfigEditor::setupFields() {
  fields_.clear();
  fields_.push_back({"Background", Field::Color, &config_.background_color});
  fields_.push_back({"Dock Height", Field::IntVal, nullptr});
  fields_.push_back({"Dock BG Color", Field::Color, &config_.dock.background_color});
  fields_.push_back({"Dock Text Color", Field::Color, &config_.dock.text_color});
  fields_.push_back({"Win Width", Field::IntVal, nullptr});
  fields_.push_back({"Win Height", Field::IntVal, nullptr});
  fields_.push_back({"Win Border Color", Field::Color, &config_.windows.border_color});
  fields_.push_back({"Win Title Color", Field::Color, &config_.windows.title_color});
}

static std::string safeSubstr(const std::string& s, int maxLen) {
  if (maxLen <= 0) return "";
  if (static_cast<int>(s.size()) <= maxLen) return s;
  if (maxLen > 3) return s.substr(0, static_cast<size_t>(maxLen - 3)) + "...";
  return s.substr(0, static_cast<size_t>(maxLen));
}

void ConfigEditor::draw(ftxui::Canvas& canvas) {
  if (!visible()) return;

  auto bg = ftxui::Color::RGB(20, 20, 35);
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto labelColor = ftxui::Color::RGB(150, 200, 255);
  auto selectedBg = ftxui::Color::RGB(233, 69, 96);
  auto editingBg = ftxui::Color::RGB(60, 60, 100);
  auto savedColor = ftxui::Color::RGB(0, 255, 128);

  canvas::fill(canvas, x(), y(), width(), height(), bg);

  int line = y() + 1;
  canvas::write(canvas, x() + 2, line, "Config Editor  [S:save] [A:add] [D:del] [Esc:close]",
    ftxui::Color::RGB(233, 69, 96), bg);
  line += 2;

  for (int i = 0; i < fieldCount(); ++i) {
    if (line - y() >= height() - 1) break;
    auto& field = fields_[static_cast<size_t>(i)];

    bool isSel = (i == selected_);

    std::string display;
    if (field.type == Field::IntVal) {
      if (i == FIELD_DOCK_HEIGHT) display = std::to_string(config_.dock.height);
      else if (i == FIELD_WIN_WIDTH) display = std::to_string(config_.windows.default_width);
      else if (i == FIELD_WIN_HEIGHT) display = std::to_string(config_.windows.default_height);
    } else if (field.value) {
      display = *field.value;
    }

    std::string label = field.label + ":";
    if (static_cast<int>(label.size()) < 18) label.append(18 - label.size(), ' ');

    if (editing_ && isSel) {
      auto editBg = editingBg;
      canvas::write(canvas, x() + 2, line, label, labelColor, bg);
      int valMax = std::max(1, width() - 22);
      std::string val = safeSubstr(edit_buffer_, valMax - 1) + "_";
      canvas::write(canvas, x() + 20, line, val, textColor, editBg);
    } else if (isSel) {
      canvas::write(canvas, x() + 2, line, label, labelColor, bg);
      canvas::write(canvas, x() + 20, line, display, ftxui::Color::White, selectedBg);
    } else {
      canvas::write(canvas, x() + 2, line, label, labelColor, bg);
      auto fg = (field.type == Field::Color) ? color::parseHex(display) : textColor;
      canvas::write(canvas, x() + 20, line, display, fg, bg);
    }
    line++;
  }

  line++;
  if (line - y() < height() - 1) {
    std::string appsLabel = "Apps (" + std::to_string(config_.apps.size()) + "):";
    canvas::write(canvas, x() + 2, line, appsLabel, ftxui::Color::RGB(233, 69, 96), bg);
    line++;
    for (int i = 0; i < static_cast<int>(config_.apps.size()) && line - y() < height() - 1; ++i) {
      auto& app = config_.apps[static_cast<size_t>(i)];
      std::string entry = "  [" + std::to_string(i) + "] " + app.name;
      if (!app.command.empty()) entry += ": " + app.command;
      if (app.internal) entry += " (internal)";
      entry = safeSubstr(entry, width() - 4);
      int appSelIdx = fieldCount() + i;
      if (appSelIdx == selected_) {
        canvas::write(canvas, x() + 2, line, entry, ftxui::Color::White, selectedBg);
      } else {
        canvas::write(canvas, x() + 2, line, entry, textColor, bg);
      }
      line++;
    }
  }

  if (saved_msg_timer_ > 0) {
    saved_msg_timer_--;
    canvas::write(canvas, x() + 2, y() + height() - 1, saved_msg_, savedColor, bg);
  }
}

void ConfigEditor::save() {
  ConfigLoader loader;
  if (loader.save(config_path_, config_)) {
    dirty_ = false;
    saved_msg_ = "Saved!";
  } else {
    saved_msg_ = "Save failed!";
  }
  saved_msg_timer_ = 30;
  if (onSaved) onSaved();
}

void ConfigEditor::confirmEdit() {
  if (selected_ < fieldCount()) {
    auto& field = fields_[static_cast<size_t>(selected_)];
    if (field.type == Field::IntVal) {
      int val = 1;
      try { val = std::stoi(edit_buffer_); } catch (const std::invalid_argument&) { } catch (const std::out_of_range&) { }
      if (selected_ == FIELD_DOCK_HEIGHT) config_.dock.height = std::max(1, val);
      else if (selected_ == FIELD_WIN_WIDTH) config_.windows.default_width = std::max(20, val);
      else if (selected_ == FIELD_WIN_HEIGHT) config_.windows.default_height = std::max(10, val);
    } else if (field.value) {
      *field.value = edit_buffer_;
    }
    dirty_ = true;
  }
  editing_ = false;
  edit_buffer_.clear();
}

bool ConfigEditor::handleEvent(ftxui::Event event) {
  if (!visible()) return false;

  if (event == ftxui::Event::Escape) {
    if (editing_) {
      editing_ = false;
      edit_buffer_.clear();
    }
    return true;
  }

  if (event == ftxui::Event::Return) {
    if (editing_) {
      confirmEdit();
    } else {
      if (selected_ >= 0 && selected_ < fieldCount()) {
        editing_ = true;
        auto& field = fields_[static_cast<size_t>(selected_)];
        if (field.type == Field::IntVal) {
          if (selected_ == FIELD_DOCK_HEIGHT) edit_buffer_ = std::to_string(config_.dock.height);
          else if (selected_ == FIELD_WIN_WIDTH) edit_buffer_ = std::to_string(config_.windows.default_width);
          else if (selected_ == FIELD_WIN_HEIGHT) edit_buffer_ = std::to_string(config_.windows.default_height);
        } else if (field.value) {
          edit_buffer_ = *field.value;
        }
      }
    }
    return true;
  }

  if (event == ftxui::Event::Tab) {
    if (!editing_) {
      int total = totalSelectable();
      if (total > 0) selected_ = (selected_ + 1) % total;
    }
    return true;
  }

  if (event == ftxui::Event::ArrowUp && !editing_) {
    if (selected_ > 0) selected_--;
    return true;
  }

  if (event == ftxui::Event::ArrowDown && !editing_) {
    if (selected_ < totalSelectable() - 1) selected_++;
    return true;
  }

  if (event == ftxui::Event::Backspace && editing_) {
    if (!edit_buffer_.empty()) edit_buffer_.pop_back();
    return true;
  }

  if (event.is_character()) {
    auto ch = event.character();
    if (ch == "s" || ch == "S") {
      if (!editing_) {
        save();
        return true;
      }
    }
    if ((ch == "d" || ch == "D") && !editing_) {
      int appIdx = selected_ - fieldCount();
      if (appIdx >= 0 && appIdx < static_cast<int>(config_.apps.size())) {
        config_.apps.erase(config_.apps.begin() + appIdx);
        dirty_ = true;
        if (selected_ >= totalSelectable()) selected_ = std::max(0, totalSelectable() - 1);
        return true;
      }
    }
    if ((ch == "a" || ch == "A") && !editing_) {
      AppConfig newApp;
      newApp.name = "New App";
      newApp.command = "";
      config_.apps.push_back(newApp);
      selected_ = fieldCount() + static_cast<int>(config_.apps.size()) - 1;
      dirty_ = true;
      return true;
    }
    if (editing_) {
      if (!ch.empty() && ch[0] >= 32 && ch[0] < 127 &&
          static_cast<int>(edit_buffer_.size()) < 30) {
        edit_buffer_ += ch;
      }
      return true;
    }
  }

  return false;
}
