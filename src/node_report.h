#ifndef SRC_NODE_REPORT_H_
#define SRC_NODE_REPORT_H_

#include "node.h"

namespace node {
using v8::Isolate;
using v8::Local;
using v8::Message;

// Bit-flags for NodeReport trigger options
#define NR_EXCEPTION  0x01
#define NR_FATALERROR 0x02
#define NR_SIGUSR2    0x04
#define NR_SIGQUIT    0x08

enum DumpEvent {kException, kFatalError, kSignal_JS, kSignal_UV};

void TriggerNodeReport(Isolate* isolate, DumpEvent event,
                       const char *message, const char *location);

void TriggerNodeReport(Isolate* isolate, DumpEvent event,
                       const char *message, const char *location,
                       Local<Message> message_handle);

unsigned int ProcessNodeReportArgs(const char *args);

}  // namespace node

#endif  // SRC_NODE_REPORT_H_
