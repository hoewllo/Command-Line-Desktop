#include "ConfigEditor.h"
#include "ui/CanvasHelpers.h"
#include "config/ConfigLoader.h"
#include <algorithm>

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

static ftxui::Color parseHex(const std::string& hex) {
  std::string h = hex;
  if (!h.empty() && h[0] == '#') h = h.substr(1);
  if (h.size() >= 6) {
    try {
      auto r = std::stoi(h.substr(0, 2), nullptr, 16);
      auto g = std::stoi(h.substr(2, 2), nullptr, 16);
      auto b = std::stoi(h.substr(4, 2), nullptr, 16);
      return ftxui::Color::RGB(r, g, b);
    } catch (...) {}
  }
  return ftxui::Color::RGB(200, 200, 200);
}

void ConfigEditor::draw(ftxui::Canvas& canvas) {
  if (!visible_) return;

  auto bg = ftxui::Color::RGB(20, 20, 35);
  auto textColor = ftxui::Color::RGB(224, 224, 224);
  auto labelColor = ftxui::Color::RGB(150, 200, 255);
  auto selectedBg = ftxui::Color::RGB(233, 69, 96);
  auto editingBg = ftxui::Color::RGB(60, 60, 100);
  auto savedColor = ftxui::Color::RGB(0, 255, 128);

  canvas::fill(canvas, x_, y_, width_, height_, bg);

  int line = y_ + 1;
  canvas::write(canvas, x_ + 2, line, "Config Editor  [S: save] [Esc: close]",
    ftxui::Color::RGB(233, 69, 96), bg);
  line += 2;

  for (int i = 0; i < fieldCount(); ++i) {
    if (line - y_ >= height_ - 1) break;
    auto& field = fields_[i];

    bool isSel = (i == selected_);

    std::string display;
    if (field.type == Field::IntVal) {
      if (i == 1) display = std::to_string(config_.dock.height);
      else if (i == 4) display = std::to_string(config_.windows.default_width);
      else if (i == 5) display = std::to_string(config_.windows.default_height);
    } else if (field.value) {
      display = *field.value;
    }

    std::string label = field.label + ":";
    if ((int)label.size() < 18) label.append(18 - label.size(), ' ');

    if (editing_ && isSel) {
      auto editBg = editingBg;
      canvas::write(canvas, x_ + 2, line, label, labelColor, bg);
      std::string val = edit_buffer_ + "_";
      if ((int)val.size() > width_ - 22)
        val = val.substr(0, width_ - 25) + "...";
      canvas::write(canvas, x_ + 20, line, val, textColor, editBg);
    } else if (isSel) {
      canvas::write(canvas, x_ + 2, line, label, labelColor, bg);
      canvas::write(canvas, x_ + 20, line, display, ftxui::Color::White, selectedBg);
    } else {
      canvas::write(canvas, x_ + 2, line, label, labelColor, bg);
      auto fg = (field.type == Field::Color) ? parseHex(display) : textColor;
      canvas::write(canvas, x_ + 20, line, display, fg, bg);
    }
    line++;
  }

  line++;
  if (line - y_ < height_ - 1) {
    std::string appsLabel = "Apps (" + std::to_string(config_.apps.size()) + "):";
    canvas::write(canvas, x_ + 2, line, appsLabel, ftxui::Color::RGB(233, 69, 96), bg);
    line++;
    for (int i = 0; i < (int)config_.apps.size() && line - y_ < height_ - 1; ++i) {
      auto& app = config_.apps[i];
      std::string entry = "  [" + std::to_string(i) + "] " + app.name;
      if (!app.command.empty()) entry += ": " + app.command;
      if (app.internal) entry += " (internal)";
      if ((int)entry.size() > width_ - 4)
        entry = entry.substr(0, width_ - 7) + "...";
      canvas::write(canvas, x_ + 2, line, entry, textColor, bg);
      line++;
    }
  }

  if (saved_msg_timer_ > 0) {
    saved_msg_timer_--;
    canvas::write(canvas, x_ + 2, y_ + height_ - 1, saved_msg_, savedColor, bg);
  }
}

void ConfigEditor::save() {
  ConfigLoader loader;
  loader.save(config_path_, config_);
  dirty_ = false;
  saved_msg_ = "Saved!";
  saved_msg_timer_ = 30;
  if (onSaved) onSaved();
}

void ConfigEditor::confirmEdit() {
  if (selected_ < fieldCount()) {
    auto& field = fields_[selected_];
    if (field.type == Field::IntVal) {
      int val = 1;
      try { val = std::stoi(edit_buffer_); } catch (...) {}
      if (selected_ == 1) config_.dock.height = std::max(1, val);
      else if (selected_ == 4) config_.windows.default_width = std::max(20, val);
      else if (selected_ == 5) config_.windows.default_height = std::max(10, val);
    } else if (field.value) {
      *field.value = edit_buffer_;
    }
    dirty_ = true;
  }
  editing_ = false;
  edit_buffer_.clear();
}

bool ConfigEditor::handleEvent(ftxui::Event event) {
  if (!visible_) return false;

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
      if (selected_ < fieldCount()) {
        editing_ = true;
        auto& field = fields_[selected_];
        if (field.type == Field::IntVal) {
          if (selected_ == 1) edit_buffer_ = std::to_string(config_.dock.height);
          else if (selected_ == 4) edit_buffer_ = std::to_string(config_.windows.default_width);
          else if (selected_ == 5) edit_buffer_ = std::to_string(config_.windows.default_height);
        } else if (field.value) {
          edit_buffer_ = *field.value;
        }
      }
    }
    return true;
  }

  if (event == ftxui::Event::Tab) {
    if (!editing_) {
      selected_ = (selected_ + 1) % fieldCount();
    }
    return true;
  }

  if (event == ftxui::Event::ArrowUp && !editing_) {
    if (selected_ > 0) selected_--;
    return true;
  }

  if (event == ftxui::Event::ArrowDown && !editing_) {
    if (selected_ < fieldCount() - 1) selected_++;
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
    if (editing_) {
      int maxLen = (ch == "s") ? 1 : 30;
      if (!ch.empty() && ch[0] >= 32 && ch[0] < 127 &&
          (int)edit_buffer_.size() < maxLen) {
        edit_buffer_ += ch;
      }
      return true;
    }
  }

  return false;
}
