#include "Renderer.h"
#include "ui/Desktop.h"
#include <ftxui/component/screen_interactive.hpp>

#ifdef ENABLE_X11_BACKEND
#include "X11Renderer.h"
#endif

class TerminalRenderer : public Renderer {
public:
  TerminalRenderer()
    : screen_(ftxui::ScreenInteractive::Fullscreen()) {
    screen_.TrackMouse(true);
  }

  int run(std::shared_ptr<Desktop> desktop) override {
    desktop->setScreen(&screen_);
    screen_.Loop(std::move(desktop));
    return 0;
  }

private:
  ftxui::ScreenInteractive screen_;
};

std::unique_ptr<Renderer> Renderer::create(Type type) {
  switch (type) {
    case Terminal:
      return std::make_unique<TerminalRenderer>();
    case X11:
#ifdef ENABLE_X11_BACKEND
      return std::make_unique<X11Renderer>(10, 18);
#else
      std::fprintf(stderr, "X11 backend not enabled. Rebuild with -DENABLE_X11_BACKEND=ON\n");
      return nullptr;
#endif
  }
  return nullptr;
}
