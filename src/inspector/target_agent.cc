#include "target_agent.h"
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include "inspector/node_string.h"
#include "inspector/worker_inspector.h"
#include "main_thread_interface.h"

namespace node {
namespace inspector {
namespace protocol {

std::unordered_map<int,std::shared_ptr<MainThreadHandle>> TargetAgent::target_session_id_worker_map_ = std::unordered_map<int, std::shared_ptr<MainThreadHandle>>();


class WorkerTargetDelegate : public WorkerDelegate {
 public:
  explicit WorkerTargetDelegate(std::shared_ptr<TargetAgent> target_agent)
      : target_agent_(target_agent) {}


  void WorkerCreated(const std::string& title,
                     const std::string& url,
                     bool waiting,
                     std::shared_ptr<MainThreadHandle> worker) override {
    std::string target_id = std::to_string(target_agent_->getNextTargetId());
    std::string type = "worker";
    
    target_agent_->targetCreated(target_id, type, title, url);
    target_agent_->attachedToTarget(worker,target_id, type, title, url);
  }

 private:
  const std::shared_ptr<TargetAgent> target_agent_;
};

std::unique_ptr<Target::TargetInfo> createTargetInfo(
    const String& target_id,
    const String& type,
    const String& title,
    const String& url) {
  return Target::TargetInfo::create()
      .setTargetId(target_id)
      .setType(type)
      .setTitle(title)
      .setUrl(url)
      .setAttached(false)
      .setCanAccessOpener(true)
      .build();
}

void TargetAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Target::Frontend>(dispatcher->channel());
  Target::Dispatcher::wire(dispatcher, this);
}

void TargetAgent::listenWorker(std::weak_ptr<WorkerManager> worker_manager) {
  auto manager = worker_manager.lock();
  if (!manager) {
    return;
  }
  std::unique_ptr<WorkerDelegate> delegate(new WorkerTargetDelegate(shared_from_this()));
  worker_event_handle_ = manager->SetAutoAttach(std::move(delegate));
}

void TargetAgent::reset() {
  if (worker_event_handle_) {
    worker_event_handle_.reset();
  }
}

void TargetAgent::targetCreated(const std::string &target_id,const std::string &type,const std::string &title,const std::string &url) {
  frontend_->targetCreated(createTargetInfo(target_id, type, title, url));
}

int TargetAgent::getNextSessionId() {
  return next_session_id_++;
}

int TargetAgent::getNextTargetId() {
  return next_target_id_++;
}

void TargetAgent::attachedToTarget(std::shared_ptr<MainThreadHandle> worker,const std::string &target_id, const std::string &type,const  std::string &title,const  std::string &url) {
  int session_id = getNextSessionId();
  target_session_id_worker_map_[session_id] = worker;
  worker->SetTargetSessionId(session_id);
  frontend_->attachedToTarget(std::to_string(session_id), createTargetInfo(target_id, type, title, url), true);
}

} // namespace protocol
}  // namespace inspector
}  // namespace node