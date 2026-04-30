#ifndef SRC_INSPECTOR_TARGET_AGENT_H_
#define SRC_INSPECTOR_TARGET_AGENT_H_

#include <memory>
#include <string_view>
#include "inspector/target_manager.h"
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

  void createAndAttachIfNecessary(std::shared_ptr<MainThreadHandle> worker,
                                  const std::string& title,
                                  const std::string& url);

  DispatchResponse getTargets(
      std::unique_ptr<protocol::Array<Target::TargetInfo>>* out_targetInfos)
      override;
  DispatchResponse setAutoAttach(bool auto_attach,
                                 bool wait_for_debugger_on_start) override;

  void listenWorker(std::weak_ptr<WorkerManager> worker_manager);
  void reset();

 private:
  int getNextTargetId();
  int getNextSessionId();
  void targetCreated(const std::string_view target_id,
                     const std::string_view type,
                     const std::string_view title,
                     const std::string_view url);
  void attachedToTarget(std::shared_ptr<MainThreadHandle> worker,
                        const std::string& target_id,
                        const std::string& type,
                        const std::string& title,
                        const std::string& url);

  std::shared_ptr<Target::Frontend> frontend_;
  std::weak_ptr<WorkerManager> worker_manager_;
  std::unique_ptr<WorkerManagerEventHandle> worker_event_handle_ = nullptr;
  std::unique_ptr<TargetManager> target_manager_ =
      std::make_unique<TargetManager>();
};

}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_TARGET_AGENT_H_
