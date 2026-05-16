#include "ConfigLoader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stack>
#include <list>
#include <cstdio>

static std::string trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

static std::string stripQuotes(const std::string& s) {
  auto t = trim(s);
  if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') ||
                         (t.front() == '\'' && t.back() == '\''))) {
    return t.substr(1, t.size() - 2);
  }
  return t;
}

static int indentLevel(const std::string& line) {
  int count = 0;
  for (char c : line) {
    if (c == ' ') count++;
    else if (c == '\t') count += 2;
    else break;
  }
  return count;
}

struct YamlNode {
  enum Type { Scalar, Map, Seq };
  Type type = Scalar;
  std::string value;
  std::list<std::pair<std::string, YamlNode>> children;
  std::list<YamlNode> items;

  std::string getString(const std::string& key, const std::string& def = "") const {
    for (auto& [k, v] : children)
      if (k == key && v.type == Scalar) return v.value;
    return def;
  }

  int getInt(const std::string& key, int def = 0) const {
    auto s = getString(key, "");
    if (s.empty()) return def;
    try { return std::stoi(s); } catch (...) { return def; }
  }

  bool getBool(const std::string& key, bool def = false) const {
    auto s = getString(key, "");
    if (s == "true" || s == "yes") return true;
    if (s == "false" || s == "no") return false;
    return def;
  }

  const YamlNode* child(const std::string& key) const {
    for (auto& [k, v] : children)
      if (k == key) return &v;
    return nullptr;
  }
};

static YamlNode parseYaml(const std::vector<std::string>& lines) {
  YamlNode root;
  root.type = YamlNode::Map;

  struct Frame {
    YamlNode* node;
    int indent;
    std::string pending_key;
    enum { ExpectKey, ExpectValue, ExpectSeqItem } state;
  };

  std::vector<Frame> stack;
  stack.push_back({&root, -1, "", Frame::ExpectKey});

  for (const auto& raw : lines) {
    std::string trimmed = trim(raw);
    if (trimmed.empty() || trimmed[0] == '#') continue;

    int indent = indentLevel(raw);

    while (!stack.empty() && indent <= stack.back().indent)
      stack.pop_back();

    auto& frame = stack.back();

    if (trimmed[0] == '-') {
      if (frame.node->type != YamlNode::Seq)
        frame.node->type = YamlNode::Seq;

      auto rest = trim(trimmed.substr(1));
      auto colon = rest.find(':');

      if (colon != std::string::npos) {
        auto key = trim(rest.substr(0, colon));
        auto val = trim(rest.substr(colon + 1));

        YamlNode item;
        item.type = YamlNode::Map;
        if (!val.empty()) {
          YamlNode scalar;
          scalar.type = YamlNode::Scalar;
          scalar.value = stripQuotes(val);
          item.children.push_back({key, scalar});
        }
        frame.node->items.push_back(item);
        Frame newFrame;
        newFrame.node = &frame.node->items.back();
        newFrame.indent = indent;
        stack.push_back(newFrame);
      } else {
        YamlNode item;
        item.type = YamlNode::Scalar;
        item.value = stripQuotes(rest);
        frame.node->items.push_back(item);
        // Don't push frame for scalar items
      }
    } else {
      auto colon = trimmed.find(':');
      if (colon == std::string::npos) {
        fprintf(stderr, "YAML warning: skipping malformed line (no colon): %s\n", trimmed.c_str());
        continue;
      }

      auto key = trim(trimmed.substr(0, colon));
      auto rest = trim(trimmed.substr(colon + 1));

      if (rest.empty()) {
        YamlNode child;
        child.type = YamlNode::Map;
        frame.node->children.push_back({key, child});
        Frame newFrame;
        newFrame.node = &frame.node->children.back().second;
        newFrame.indent = indent;
        newFrame.state = Frame::ExpectKey;
        stack.push_back(newFrame);
      } else {
        YamlNode child;
        child.type = YamlNode::Scalar;
        child.value = stripQuotes(rest);
        frame.node->children.push_back({key, child});
      }
    }
  }

  return root;
}

Config ConfigLoader::load(const std::string& path) {
  Config config;
  std::ifstream file(path);
  if (!file.is_open()) {
    fprintf(stderr, "Warning: could not open config file: %s\n", path.c_str());
    return config;
  }

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) lines.push_back(line);

  auto doc = parseYaml(lines);

  config.background_color = doc.getString("background_color", "#1a1a2e");

  auto* appsNode = doc.child("apps");
  if (appsNode && appsNode->type == YamlNode::Seq) {
    for (auto& item : appsNode->items) {
      if (item.type == YamlNode::Map) {
        AppConfig app;
        app.name = item.getString("name");
        app.command = item.getString("command");
        app.internal = item.getBool("internal");
        if (!app.name.empty()) config.apps.push_back(app);
      }
    }
  }

  auto* dockNode = doc.child("dock");
  if (dockNode) {
    config.dock.height = dockNode->getInt("height", 2);
    config.dock.background_color = dockNode->getString("background_color", "#16213e");
    config.dock.text_color = dockNode->getString("text_color", "#e0e0e0");
  }

  auto* winNode = doc.child("windows");
  if (winNode) {
    config.windows.default_width = winNode->getInt("default_width", 60);
    config.windows.default_height = winNode->getInt("default_height", 30);
    config.windows.border_color = winNode->getString("border_color", "#0f3460");
    config.windows.title_color = winNode->getString("title_color", "#e94560");
  }

  return config;
}

void ConfigLoader::save(const std::string& path, const Config& config) {
  std::ofstream f(path);
  if (!f.is_open()) return;

  auto q = [](const std::string& s) -> std::string {
    if (s.find(' ') != std::string::npos || s.empty())
      return "\"" + s + "\"";
    return s;
  };

  f << "background_color: " << q(config.background_color) << "\n\n";

  f << "apps:\n";
  for (const auto& app : config.apps) {
    f << "  - name: " << q(app.name) << "\n";
    f << "    command: " << q(app.command) << "\n";
    if (app.internal)
      f << "    internal: true\n";
  }

  f << "\ndock:\n";
  f << "  height: " << config.dock.height << "\n";
  f << "  background_color: " << q(config.dock.background_color) << "\n";
  f << "  text_color: " << q(config.dock.text_color) << "\n";

  f << "\nwindows:\n";
  f << "  default_width: " << config.windows.default_width << "\n";
  f << "  default_height: " << config.windows.default_height << "\n";
  f << "  border_color: " << q(config.windows.border_color) << "\n";
  f << "  title_color: " << q(config.windows.title_color) << "\n";
}
