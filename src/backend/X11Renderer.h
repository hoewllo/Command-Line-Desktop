#pragma once
#include "Renderer.h"

class X11Renderer : public Renderer {
public:
  X11Renderer(int cell_w, int cell_h);
  ~X11Renderer() override;
  int run(std::shared_ptr<Desktop> desktop) override;

private:
  bool initSDL();
  void renderFrame(Desktop& desktop);
  void shutdownSDL();

  int cell_w_ = 10;
  int cell_h_ = 18;
  int win_w_ = 800;
  int win_h_ = 600;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};
