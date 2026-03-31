#include "target_agent.h"
#include <string_view>
#include "crdtp/dispatch.h"
#include "inspector/worker_inspector.h"
#include "main_thread_interface.h"

namespace node {
namespace inspector {
namespace protocol {

class WorkerTargetDelegate : public WorkerDelegate {
 public:
  explicit WorkerTargetDelegate(std::shared_ptr<TargetAgent> target_agent)
      : target_agent_(target_agent) {}

  void WorkerCreated(const std::string& title,
                     const std::string& url,
                     bool waiting,
                     std::shared_ptr<MainThreadHandle> worker) override {
    target_agent_->createAndAttachIfNecessary(worker, title, url);
  }

 private:
  const std::shared_ptr<TargetAgent> target_agent_;
};

std::unique_ptr<Target::TargetInfo> createTargetInfo(
    const std::string_view target_id,
    const std::string_view type,
    const std::string_view title,
    const std::string_view url,
    bool attached = false) {
  return Target::TargetInfo::create()
      .setTargetId(std::string(target_id))
      .setType(std::string(type))
      .setTitle(std::string(title))
      .setUrl(std::string(url))
      .setAttached(attached)
      .setCanAccessOpener(true)
      .build();
}

void TargetAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Target::Frontend>(dispatcher->channel());
  Target::Dispatcher::wire(dispatcher, this);
}

void TargetAgent::createAndAttachIfNecessary(
    std::shared_ptr<MainThreadHandle> worker,
    const std::string& title,
    const std::string& url) {
  std::string target_id = std::to_string(getNextTargetId());
  std::string type = "node_worker";

  targetCreated(target_id, type, title, url);
  bool attached = false;
  if (target_manager_->auto_attach()) {
    attached = true;
    attachedToTarget(worker, target_id, type, title, url);
  }
  target_manager_->AddTarget(worker, target_id, type, title, url, attached);
}

void TargetAgent::listenWorker(std::weak_ptr<WorkerManager> worker_manager) {
  auto manager = worker_manager.lock();
  if (!manager) {
    return;
  }
  std::unique_ptr<WorkerDelegate> delegate(
      new WorkerTargetDelegate(shared_from_this()));
  worker_event_handle_ = manager->SetAutoAttach(std::move(delegate));
}

void TargetAgent::reset() {
  if (worker_event_handle_) {
    worker_event_handle_.reset();
  }
}

void TargetAgent::targetCreated(const std::string_view target_id,
                                const std::string_view type,
                                const std::string_view title,
                                const std::string_view url) {
  frontend_->targetCreated(createTargetInfo(target_id, type, title, url));
}

crdtp::DispatchResponse TargetAgent::getTargets(
    std::unique_ptr<protocol::Array<Target::TargetInfo>>* out_targetInfos) {
  auto target_infos = std::make_unique<protocol::Array<Target::TargetInfo>>();
  for (const auto& target : target_manager_->GetTargetsSnapshot()) {
    target_infos->push_back(createTargetInfo(target.target_id,
                                             target.type,
                                             target.title,
                                             target.url,
                                             target.attached));
  }
  *out_targetInfos = std::move(target_infos);
  return DispatchResponse::Success();
}

int TargetAgent::getNextSessionId() {
  return target_manager_->NextSessionId();
}

int TargetAgent::getNextTargetId() {
  return target_manager_->NextTargetId();
}

void TargetAgent::attachedToTarget(std::shared_ptr<MainThreadHandle> worker,
                                   const std::string& target_id,
                                   const std::string& type,
                                   const std::string& title,
                                   const std::string& url) {
  int session_id = getNextSessionId();
  TargetManager::RegisterSessionWorker(session_id, worker);
  worker->SetTargetSessionId(session_id);
  frontend_->attachedToTarget(std::to_string(session_id),
                              createTargetInfo(target_id, type, title, url),
                              true);
}

// TODO(islandryu): Currently, setAutoAttach applies the main thread's value to
// all threads. Modify it to be managed per worker thread.
crdtp::DispatchResponse TargetAgent::setAutoAttach(
    bool auto_attach, bool wait_for_debugger_on_start) {
  target_manager_->SetAutoAttach(auto_attach, wait_for_debugger_on_start);

  if (auto_attach) {
    for (auto& target : target_manager_->targets()) {
      if (!target.attached) {
        target.attached = true;
        attachedToTarget(target.worker,
                         target.target_id,
                         target.type,
                         target.title,
                         target.url);
      }
    }
  }

  return DispatchResponse::Success();
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
