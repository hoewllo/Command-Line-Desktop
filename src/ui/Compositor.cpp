#include "Layer.h"
#include <algorithm>

void Compositor::addLayer(std::unique_ptr<Layer> layer, int z) {
  if (!layer) return;
  Entry entry;
  entry.layer = std::move(layer);
  entry.z = z;
  layers_.push_back(std::move(entry));
  sortLayers();
}

void Compositor::removeLayer(const std::string& name) {
  layers_.erase(
    std::remove_if(layers_.begin(), layers_.end(),
      [&](const Entry& e) { return e.layer->name() == name; }),
    layers_.end());
}

Layer* Compositor::getLayer(const std::string& name) {
  for (auto& e : layers_)
    if (e.layer->name() == name) return e.layer.get();
  return nullptr;
}

void Compositor::setLayerZ(const std::string& name, int z) {
  for (auto& e : layers_)
    if (e.layer->name() == name) { e.z = z; break; }
  sortLayers();
}

void Compositor::sortLayers() {
  std::sort(layers_.begin(), layers_.end(),
    [](const Entry& a, const Entry& b) { return a.z < b.z; });
}

void Compositor::draw(ftxui::Canvas& c) {
  for (auto& e : layers_) {
    if (e.layer->enabled())
      e.layer->draw(c);
  }
}

bool Compositor::handleEvent(ftxui::Event event) {
  // Process layers front-to-back (reverse draw order)
  for (int i = static_cast<int>(layers_.size()) - 1; i >= 0; --i) {
    if (layers_[static_cast<size_t>(i)].layer->enabled() &&
        layers_[static_cast<size_t>(i)].layer->handleEvent(event))
      return true;
  }
  return false;
}
