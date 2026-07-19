#ifndef SRC_INSPECTOR_TARGET_MANAGER_H_
#define SRC_INSPECTOR_TARGET_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "node_mutex.h"

namespace node {
namespace inspector {

class MainThreadHandle;

class TargetManager {
 public:
  struct TargetInfo {
    std::string target_id;
    std::string type;
    std::string title;
    std::string url;
    std::shared_ptr<MainThreadHandle> worker;
    bool attached;
  };

  TargetManager() = default;

  int NextTargetId();
  int NextSessionId();

  void SetAutoAttach(bool auto_attach, bool wait_for_debugger_on_start);
  bool auto_attach() const { return auto_attach_; }
  bool wait_for_debugger_on_start() const {
    return wait_for_debugger_on_start_;
  }

  void AddTarget(std::shared_ptr<MainThreadHandle> worker,
                 const std::string& target_id,
                 const std::string& type,
                 const std::string& title,
                 const std::string& url,
                 bool attached);
  std::vector<TargetInfo> GetTargetsSnapshot() const;
  std::vector<TargetInfo>& targets() { return targets_; }
  const std::vector<TargetInfo>& targets() const { return targets_; }

  static void RegisterSessionWorker(int session_id,
                                    std::shared_ptr<MainThreadHandle> worker);
  static std::shared_ptr<MainThreadHandle> WorkerForSession(int session_id);

 private:
  static Mutex session_state_lock_;
  static std::unordered_map<int, std::shared_ptr<MainThreadHandle>>
      session_worker_map_;
  static int next_session_id_;

  int next_target_id_ = 1;
  bool auto_attach_ = false;
  // TODO(islandryu): Honor this flag for worker targets. It is stored here
  // so Target.setAutoAttach() state can be tracked, but worker startup pause
  // behavior does not change based on it yet.
  bool wait_for_debugger_on_start_ = true;
  std::vector<TargetInfo> targets_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_TARGET_MANAGER_H_
