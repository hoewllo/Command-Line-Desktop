#pragma once
#include <memory>
#include <string>

class Desktop;

class Renderer {
public:
  virtual ~Renderer() = default;
  virtual int run(std::shared_ptr<Desktop> desktop) = 0;

  enum Type { Terminal, X11 };
  static std::unique_ptr<Renderer> create(Type type);
};
