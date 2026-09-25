#include "inspector/target_manager.h"

#include "inspector/main_thread_interface.h"

namespace node {
namespace inspector {

Mutex TargetManager::session_state_lock_;
std::unordered_map<int, std::shared_ptr<MainThreadHandle>>
    TargetManager::session_worker_map_;
int TargetManager::next_session_id_ = 1;

int TargetManager::NextTargetId() {
  return next_target_id_++;
}

int TargetManager::NextSessionId() {
  Mutex::ScopedLock scoped_lock(session_state_lock_);
  return next_session_id_++;
}

void TargetManager::SetAutoAttach(bool auto_attach,
                                  bool wait_for_debugger_on_start) {
  auto_attach_ = auto_attach;
  wait_for_debugger_on_start_ = wait_for_debugger_on_start;
}

void TargetManager::AddTarget(std::shared_ptr<MainThreadHandle> worker,
                              const std::string& target_id,
                              const std::string& type,
                              const std::string& title,
                              const std::string& url,
                              bool attached) {
  targets_.push_back({target_id, type, title, url, worker, attached});
}

std::vector<TargetManager::TargetInfo> TargetManager::GetTargetsSnapshot()
    const {
  std::vector<TargetInfo> result;
  result.reserve(targets_.size());
  for (const auto& target : targets_) {
    if (target.worker && !target.worker->Expired()) {
      result.push_back(target);
    }
  }
  return result;
}

void TargetManager::RegisterSessionWorker(
    int session_id, std::shared_ptr<MainThreadHandle> worker) {
  Mutex::ScopedLock scoped_lock(session_state_lock_);
  session_worker_map_[session_id] = std::move(worker);
}

std::shared_ptr<MainThreadHandle> TargetManager::WorkerForSession(
    int session_id) {
  Mutex::ScopedLock scoped_lock(session_state_lock_);
  auto it = session_worker_map_.find(session_id);
  if (it == session_worker_map_.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace inspector
}  // namespace node
