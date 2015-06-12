#include "persistent-handle-cleanup.h"

#include "v8.h"

#include "env.h"
#include "env-inl.h"
#include "smalloc.h"
#include "node-contextify.h"

namespace node {

using v8::Persistent;
using v8::Value;
using v8::Isolate;
using smalloc::CallbackInfo;

void PersistentHandleCleanup::VisitPersistentHandle(
    Persistent<Value>* value, uint16_t class_id) {
  Isolate::DisallowJavascriptExecutionScope scope(env()->isolate(),
      Isolate::DisallowJavascriptExecutionScope::CRASH_ON_FAILURE);

  if (value->IsWeak()) {
    switch (static_cast<ClassId>(class_id)) {
      case SMALLOC:
        value->ClearWeak<CallbackInfo>()->DisposeNoAllocation(env()->isolate());
        break;
      case CONTEXTIFY_SCRIPT:
        value->ClearWeak<ContextifyScript>()->Dispose();
        break;
      default: {}
        // Addon class id
    }
  }
};

}  // namespace node
