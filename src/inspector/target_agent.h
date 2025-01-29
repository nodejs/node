#ifndef SRC_INSPECTOR_TARGET_AGENT_H_
#define SRC_INSPECTOR_TARGET_AGENT_H_

#include "inspector/worker_inspector.h"
#include "node/inspector/protocol/Target.h"

namespace node {

namespace inspector {
class TargetInspector;

namespace protocol {

class TargetAgent : public Target::Backend,
                    public std::enable_shared_from_this<TargetAgent> {
 public:
  void Wire(UberDispatcher* dispatcher);

  void targetCreated(const std::string& target_id,
                     const std::string& type,
                     const std::string& title,
                     const std::string& url);
  void attachedToTarget(std::shared_ptr<MainThreadHandle> worker,
                        const std::string& target_id,
                        const std::string& type,
                        const std::string& title,
                        const std::string& url);

  int getNextTargetId();

  void listenWorker(std::weak_ptr<WorkerManager> worker_manager);
  void reset();
  static std::unordered_map<int, std::shared_ptr<MainThreadHandle>>
      target_session_id_worker_map_;

 private:
  int getNextSessionId();
  std::shared_ptr<Target::Frontend> frontend_;
  std::weak_ptr<WorkerManager> worker_manager_;
  static int next_session_id_;
  int next_target_id_ = 1;
  std::unique_ptr<WorkerManagerEventHandle> worker_event_handle_ = nullptr;
};

}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_TARGET_AGENT_H_
