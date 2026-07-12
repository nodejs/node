// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_INSPECTED_CONTEXT_H_
#define V8_INSPECTOR_INSPECTED_CONTEXT_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "include/v8-local-handle.h"
#include "include/v8-persistent-handle.h"
#include "src/base/macros.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/string-16.h"
#include "src/inspector/v8-debugger-id.h"

namespace v8 {
class Context;
class Object;
}  // namespace v8

namespace v8_inspector {

class InjectedScript;
class InjectedScriptHost;
class V8ContextInfo;
class V8InspectorImpl;

enum class V8InternalValueType {
  kNone,
  kEntry,
  kScope,
  kScopeList,
  kPrivateMethodList,
  kPrivateMethod
};

class InspectedContext {
 public:
  ~InspectedContext();
  InspectedContext(const InspectedContext&) = delete;
  InspectedContext& operator=(const InspectedContext&) = delete;

  static int contextId(v8::Local<v8::Context>);

  v8::Local<v8::Context> context() const;
  int contextId() const { return m_contextId; }
  int contextGroupId() const { return m_contextGroupId; }
  String16 origin() const { return m_origin; }
  String16 humanReadableName() const { return m_humanReadableName; }
  internal::V8DebuggerId uniqueId() const { return m_uniqueId; }
  String16 auxData() const { return m_auxData; }

  bool isReported(int sessionId) const;
  void setReported(int sessionId, bool reported);

  v8::Isolate* isolate() const;
  V8InspectorImpl* inspector() const { return m_inspector; }

  InjectedScript* getInjectedScript(int sessionId);
  InjectedScript* createInjectedScript(int sessionId);
  void discardInjectedScript(int sessionId);

  bool addInternalObject(v8::Local<v8::Object> object,
                         V8InternalValueType type);
  V8InternalValueType getInternalType(v8::Local<v8::Object> object);

 private:
  friend class V8InspectorImpl;
  InspectedContext(V8InspectorImpl*, const V8ContextInfo&, int contextId);

  class ContextCollectedCallbacks;
  class WeakCallbackData;

  V8InspectorImpl* m_inspector;
  v8::Global<v8::Context> m_context;
  int m_contextId;
  int m_contextGroupId;
  const String16 m_origin;
  const String16 m_humanReadableName;
  const String16 m_auxData;
  const internal::V8DebuggerId m_uniqueId;
  std::unordered_set<int> m_reportedSessionIds;
  std::unordered_map<int, std::unique_ptr<InjectedScript>> m_injectedScripts;
  WeakCallbackData* m_weakCallbackData;
  v8::Global<v8::debug::EphemeronTable> m_internalObjects;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_INSPECTED_CONTEXT_H_
