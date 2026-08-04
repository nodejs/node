#include "notification_emitter.h"
#include "v8-inspector.h"
#include "v8.h"

namespace node {
namespace inspector {

NotificationEmitter::NotificationEmitter() {}

void NotificationEmitter::emitNotification(v8::Local<v8::Context> context,
                                           const EventKey& event,
                                           v8::Local<v8::Object> params) {
  auto it = event_notifier_map_.find(event);
  if (it != event_notifier_map_.end() && it->second) {
    it->second(context, params);
  }
}

void NotificationEmitter::addEventNotifier(const EventKey& event,
                                           EventNotifier notifier) {
  event_notifier_map_[event] = notifier;
}
}  // namespace inspector
}  // namespace node
