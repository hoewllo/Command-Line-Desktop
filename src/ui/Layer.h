#pragma once
#include <ftxui/dom/canvas.hpp>
#include <ftxui/component/event.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdio>

class Layer {
public:
  virtual ~Layer() = default;
  virtual void draw(ftxui::Canvas& c) = 0;
  virtual bool handleEvent(ftxui::Event event) { return false; }
  virtual std::string name() const = 0;
  virtual bool enabled() const { return true; }
  virtual void setEnabled(bool v) { (void)v; }
};

// Convenience layer wrapping lambdas
class SimpleLayer : public Layer {
public:
  SimpleLayer(const std::string& name,
              std::function<void(ftxui::Canvas&)> drawFn,
              std::function<bool(ftxui::Event)> eventFn = nullptr)
    : name_(name), drawFn_(drawFn), eventFn_(eventFn) {}
  void draw(ftxui::Canvas& c) override { if (drawFn_) drawFn_(c); }
  bool handleEvent(ftxui::Event e) override { return eventFn_ ? eventFn_(e) : false; }
  std::string name() const override { return name_; }
private:
  std::string name_;
  std::function<void(ftxui::Canvas&)> drawFn_;
  std::function<bool(ftxui::Event)> eventFn_;
};

// Z-ordered compositor: layers draw from back to front
class Compositor {
public:
  void addLayer(std::unique_ptr<Layer> layer, int z = 0);
  void removeLayer(const std::string& name);
  Layer* getLayer(const std::string& name);
  void setLayerZ(const std::string& name, int z);

  void draw(ftxui::Canvas& c);
  bool handleEvent(ftxui::Event event);

private:
  struct Entry {
    std::unique_ptr<Layer> layer;
    int z = 0;
  };
  std::vector<Entry> layers_;
  void sortLayers();
};
