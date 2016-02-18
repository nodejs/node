#include "persistent-handle-cleanup.h"

#include "v8.h"

#include "env.h"
#include "env-inl.h"
#include "node-contextify.h"
#include "async-wrap.h"

namespace node {

using v8::Persistent;
using v8::Value;
using v8::Isolate;

void PersistentHandleCleanup::VisitPersistentHandle(
    Persistent<Value>* value, uint16_t class_id) {
  Isolate::DisallowJavascriptExecutionScope scope(env()->isolate(),
      Isolate::DisallowJavascriptExecutionScope::CRASH_ON_FAILURE);

  if (value->IsWeak()) {
    switch (static_cast<ClassId>(class_id)) {
      case CONTEXTIFY_SCRIPT:
        value->ClearWeak<ContextifyScript>()->Dispose();
        break;
      case PROVIDER_CLASS_ID_NONE:
        break;
      case PROVIDER_CLASS_ID_CRYPTO:
        break;
      case PROVIDER_CLASS_ID_FSEVENTWRAP:
        break;
      case PROVIDER_CLASS_ID_FSREQWRAP:
        break;
      case PROVIDER_CLASS_ID_GETADDRINFOREQWRAP:
        break;
      case PROVIDER_CLASS_ID_GETNAMEINFOREQWRAP:
        break;
      case PROVIDER_CLASS_ID_JSSTREAM:
        break;
      case PROVIDER_CLASS_ID_PIPEWRAP:
        break;
      case PROVIDER_CLASS_ID_PROCESSWRAP:
        break;
      case PROVIDER_CLASS_ID_QUERYWRAP:
        break;
      case PROVIDER_CLASS_ID_SHUTDOWNWRAP:
        break;
      case PROVIDER_CLASS_ID_SIGNALWRAP:
        break;
      case PROVIDER_CLASS_ID_STATWATCHER:
        break;
      case PROVIDER_CLASS_ID_TCPWRAP:
        break;
      case PROVIDER_CLASS_ID_TIMERWRAP:
        break;
      case PROVIDER_CLASS_ID_TLSWRAP:
        break;
      case PROVIDER_CLASS_ID_TTYWRAP:
        break;
      case PROVIDER_CLASS_ID_UDPWRAP:
        break;
      case PROVIDER_CLASS_ID_WRITEWRAP:
        break;
      case PROVIDER_CLASS_ID_ZLIB:
        break;
      default: {}
        // Addon class id
    }
  }
};

}  // namespace node
