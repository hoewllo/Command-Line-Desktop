#pragma once
#include "core/Component.h"
#include "config/ConfigLoader.h"
#include <string>
#include <vector>
#include <functional>

class ConfigEditor : public Component {
public:
  ConfigEditor(const Config& config, const std::string& configPath);
  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return "Config Editor"; }

  bool isDirty() const { return dirty_; }
  std::function<void()> onSaved;

private:
  Config config_;
  std::string config_path_;
  bool dirty_ = false;
  int selected_ = 0;
  bool editing_ = false;
  std::string edit_buffer_;
  std::string saved_msg_;
  int saved_msg_timer_ = 0;

  struct Field {
    std::string label;
    enum Type { Color, IntVal, StringVal };
    Type type;
    std::string* value;
  };

  std::vector<Field> fields_;
  void setupFields();
  int fieldCount() const { return static_cast<int>(fields_.size()); }
  int totalSelectable() const { return fieldCount() + static_cast<int>(config_.apps.size()); }
  void save();
  void confirmEdit();
};
