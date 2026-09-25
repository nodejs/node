#ifndef SRC_INSPECTOR_NOTIFICATION_EMITTER_H_
#define SRC_INSPECTOR_NOTIFICATION_EMITTER_H_

#include <string>
#include <unordered_map>
#include "v8.h"

namespace node {
namespace inspector {

class NotificationEmitter {
 public:
  using EventKey = std::string;
  using EventNotifier =
      std::function<void(v8::Local<v8::Context>, v8::Local<v8::Object>)>;

  NotificationEmitter();
  virtual ~NotificationEmitter() = default;

  void emitNotification(v8::Local<v8::Context> context,
                        const EventKey& event,
                        v8::Local<v8::Object> params);
  virtual bool canEmit(const std::string& domain) = 0;

  NotificationEmitter(const NotificationEmitter&) = delete;
  NotificationEmitter& operator=(const NotificationEmitter&) = delete;

 protected:
  void addEventNotifier(const EventKey& event, EventNotifier notifier);

 private:
  std::unordered_map<EventKey, EventNotifier> event_notifier_map_ = {};
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NOTIFICATION_EMITTER_H_
