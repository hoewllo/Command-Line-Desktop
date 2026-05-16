#pragma once
#include <memory>
#include <vector>
#include <functional>

class WindowManager;

class WorkspaceManager {
public:
  WorkspaceManager(int count = 4);

  WindowManager* currentWM();
  WindowManager* getWM(int idx);
  const WindowManager* getWM(int idx) const;

  void switchTo(int idx);
  int currentIndex() const { return active_; }
  int count() const { return count_; }

  // Move focused window from current workspace to target
  bool moveWindowTo(int targetWorkspace);

  std::function<void(int)> onWorkspaceChanged;

private:
  int count_;
  int active_ = 0;
  std::vector<std::unique_ptr<WindowManager>> workspaces_;
};
