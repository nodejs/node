#ifndef SRC_NODE_REPORT_H_
#define SRC_NODE_REPORT_H_

#include "node.h"

namespace node {
using v8::Isolate;
using v8::Local;
using v8::Message;

enum DumpEvent {kException, kFatalError, kSignal_JS, kSignal_UV};

void TriggerNodeReport(Isolate* isolate, DumpEvent event, const char *message, const char *location);

void TriggerNodeReport(Isolate* isolate, DumpEvent event, const char *message, const char *location, Local<Message> message_handle);

}  // namespace node

#endif  // SRC_NODE_REVERT_H_
