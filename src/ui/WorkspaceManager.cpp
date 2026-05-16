#include "WorkspaceManager.h"
#include "WindowManager.h"
#include "WindowFrame.h"

WorkspaceManager::WorkspaceManager(int count)
  : count_(std::max(1, count)) {
  for (int i = 0; i < count_; ++i)
    workspaces_.push_back(std::make_unique<WindowManager>());
}

WindowManager* WorkspaceManager::currentWM() {
  return getWM(active_);
}

WindowManager* WorkspaceManager::getWM(int idx) {
  if (idx >= 0 && idx < count_)
    return workspaces_[static_cast<size_t>(idx)].get();
  return nullptr;
}

const WindowManager* WorkspaceManager::getWM(int idx) const {
  if (idx >= 0 && idx < count_)
    return workspaces_[static_cast<size_t>(idx)].get();
  return nullptr;
}

void WorkspaceManager::switchTo(int idx) {
  if (idx < 0 || idx >= count_ || idx == active_) return;
  active_ = idx;
  if (onWorkspaceChanged) onWorkspaceChanged(active_);
}

bool WorkspaceManager::moveWindowTo(int targetWorkspace) {
  if (targetWorkspace < 0 || targetWorkspace >= count_ ||
      targetWorkspace == active_) return false;

  auto* src = currentWM();
  auto* dst = getWM(targetWorkspace);
  if (!src || !dst) return false;

  auto* focused = src->focusedWindow();
  if (!focused) return false;

  // Collect all windows to find the focused one
  auto wins = src->windows();
  for (auto* win : wins) {
    if (win == focused) {
      // Remove from source, add to destination
      // We need to transfer the unique_ptr. The WindowManager uses
      // unique_ptr<WindowFrame>, so we can't just move a pointer.
      // Instead: close in source, reopen in dest (simple but loses state)
      // Better: expose a release/take method on WindowManager.

      // For now: close focused window in current workspace
      // and signal to Desktop to reopen it
      return false; // Not implemented - will be handled by Desktop
    }
  }
  return false;
}
