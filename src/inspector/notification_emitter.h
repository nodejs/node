#ifndef SRC_INSPECTOR_NOTIFICATION_EMITTER_H_
#define SRC_INSPECTOR_NOTIFICATION_EMITTER_H_

#include <string>
#include <unordered_map>
#include "node/inspector/protocol/Protocol.h"
#include "v8.h"

namespace node {
namespace inspector {

class NotificationEmitter {
 public:
  using EventNotifier = void (NotificationEmitter::*)(
      v8::Local<v8::Context> context, v8::Local<v8::Object>);
  NotificationEmitter();
  virtual ~NotificationEmitter() = default;

  void emitNotification(v8::Local<v8::Context> context,
                        const protocol::String& event,
                        v8::Local<v8::Object> params);
  virtual bool canEmit(const std::string& domain) = 0;

  NotificationEmitter(const NotificationEmitter&) = delete;
  NotificationEmitter& operator=(const NotificationEmitter&) = delete;

 protected:
  void addEventNotifier(const protocol::String& event, EventNotifier notifier);

 private:
  std::unordered_map<protocol::String, EventNotifier> event_notifier_map_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NOTIFICATION_EMITTER_H_
