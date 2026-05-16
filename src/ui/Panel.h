#pragma once
#include "core/Component.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <ftxui/screen/color.hpp>

class WindowManager;

// ----- Panel Segment base -----
class PanelSegment {
public:
  virtual ~PanelSegment() = default;
  virtual void draw(ftxui::Canvas& c, int x, int y, int w, int h) = 0;
  virtual bool handleEvent(ftxui::Event event) { return false; }
  virtual int minWidth() const = 0;
  virtual int maxWidth() const { return 999; }
};

// ----- Start button segment -----
class StartSegment : public PanelSegment {
public:
  void draw(ftxui::Canvas& c, int x, int y, int w, int h) override;
  bool handleEvent(ftxui::Event event) override;
  int minWidth() const override { return 10; }
  int maxWidth() const override { return 10; }

  std::function<void()> onClick;
  void flash() { flash_ = 10; }

private:
  int flash_ = 0;
  ftxui::Color accent_{ftxui::Color::RGB(233, 69, 96)};
  ftxui::Color bg_{ftxui::Color::RGB(22, 33, 62)};
};

// ----- Taskbar segment (windows from WM) -----
class TaskSegment : public PanelSegment {
public:
  void setWindowManager(WindowManager* wm) { wm_ = wm; }
  void draw(ftxui::Canvas& c, int x, int y, int w, int h) override;
  bool handleEvent(ftxui::Event event) override;
  int minWidth() const override { return 5; }

private:
  WindowManager* wm_ = nullptr;
};

// ----- Clock segment -----
class ClockSegment : public PanelSegment {
public:
  void draw(ftxui::Canvas& c, int x, int y, int w, int h) override;
  int minWidth() const override { return 9; }
  int maxWidth() const override { return 9; }
};

// ----- Workspace indicator segment -----
class WorkspaceSegment : public PanelSegment {
public:
  void setWorkspace(int current, int total) { cur_ = current; total_ = total; }
  void draw(ftxui::Canvas& c, int x, int y, int w, int h) override;
  int minWidth() const override { return 7; }
  int maxWidth() const override { return 9; }
private:
  int cur_ = 0, total_ = 4;
};

// ----- System tray segment (placeholder) -----
class TraySegment : public PanelSegment {
public:
  void draw(ftxui::Canvas& c, int x, int y, int w, int h) override;
  int minWidth() const override { return 0; }
};

// ----- Panel (replaces Dock) -----
class Panel : public Component {
public:
  Panel();

  void draw(ftxui::Canvas& canvas) override;
  bool handleEvent(ftxui::Event event) override;
  std::string title() const override { return "Panel"; }

  void setWindowManager(WindowManager* wm);
  void setPanelHeight(int h) { height_ = h; }
  int panelHeight() const { return height_; }

  void setBackgroundColor(const std::string& hex);
  void setTextColor(const std::string& hex);

  StartSegment* startSegment() { return &start_; }
  TaskSegment* taskSegment() { return &task_; }
  WorkspaceSegment* workspaceSegment() { return &ws_; }

  void setWorkspace(int current, int total) { ws_.setWorkspace(current, total); }

  std::function<void()> onStartClick;

private:
  int height_ = 2;
  ftxui::Color bgColor_{ftxui::Color::RGB(22, 33, 62)};
  ftxui::Color textColor_{ftxui::Color::RGB(224, 224, 224)};

  StartSegment start_;
  TaskSegment task_;
  WorkspaceSegment ws_;
  ClockSegment clock_;
  TraySegment tray_;
};
