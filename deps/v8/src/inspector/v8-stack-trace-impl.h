// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_STACK_TRACE_IMPL_H_
#define V8_INSPECTOR_V8_STACK_TRACE_IMPL_H_

#include <memory>
#include <vector>

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/string-16.h"

namespace v8_inspector {

class AsyncStackTrace;
class V8Debugger;
class WasmTranslation;
struct V8StackTraceId;

class StackFrame {
 public:
  explicit StackFrame(v8::Isolate* isolate, v8::Local<v8::StackFrame> frame);
  ~StackFrame() = default;

  void translate(WasmTranslation* wasmTranslation);

  const String16& functionName() const;
  const String16& scriptId() const;
  const String16& sourceURL() const;
  int lineNumber() const;    // 0-based.
  int columnNumber() const;  // 0-based.
  std::unique_ptr<protocol::Runtime::CallFrame> buildInspectorObject(
      V8InspectorClient* client) const;
  bool isEqual(StackFrame* frame) const;

 private:
  String16 m_functionName;
  String16 m_scriptId;
  String16 m_sourceURL;
  int m_lineNumber;    // 0-based.
  int m_columnNumber;  // 0-based.
  bool m_hasSourceURLComment;
};

class V8StackTraceImpl : public V8StackTrace {
 public:
  static void setCaptureStackTraceForUncaughtExceptions(v8::Isolate*,
                                                        bool capture);
  static int maxCallStackSizeToCapture;
  static std::unique_ptr<V8StackTraceImpl> create(V8Debugger*,
                                                  int contextGroupId,
                                                  v8::Local<v8::StackTrace>,
                                                  int maxStackSize);
  static std::unique_ptr<V8StackTraceImpl> capture(V8Debugger*,
                                                   int contextGroupId,
                                                   int maxStackSize);

  ~V8StackTraceImpl() override;
  std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObjectImpl(
      V8Debugger* debugger) const;

  std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObjectImpl(
      V8Debugger* debugger, int maxAsyncDepth) const;

  // V8StackTrace implementation.
  // This method drops the async stack trace.
  std::unique_ptr<V8StackTrace> clone() override;
  StringView firstNonEmptySourceURL() const override;
  bool isEmpty() const override;
  StringView topSourceURL() const override;
  int topLineNumber() const override;    // 1-based.
  int topColumnNumber() const override;  // 1-based.
  StringView topScriptId() const override;
  StringView topFunctionName() const override;
  std::unique_ptr<protocol::Runtime::API::StackTrace> buildInspectorObject()
      const override;
  std::unique_ptr<protocol::Runtime::API::StackTrace> buildInspectorObject(
      int maxAsyncDepth) const override;
  std::unique_ptr<StringBuffer> toString() const override;

  bool isEqualIgnoringTopFrame(V8StackTraceImpl* stackTrace) const;

 private:
  V8StackTraceImpl(std::vector<std::shared_ptr<StackFrame>> frames,
                   int maxAsyncDepth,
                   std::shared_ptr<AsyncStackTrace> asyncParent,
                   const V8StackTraceId& externalParent);

  class StackFrameIterator {
   public:
    explicit StackFrameIterator(const V8StackTraceImpl* stackTrace);

    void next();
    StackFrame* frame();
    bool done();

   private:
    std::vector<std::shared_ptr<StackFrame>>::const_iterator m_currentIt;
    std::vector<std::shared_ptr<StackFrame>>::const_iterator m_currentEnd;
    AsyncStackTrace* m_parent;
  };

  std::vector<std::shared_ptr<StackFrame>> m_frames;
  int m_maxAsyncDepth;
  std::weak_ptr<AsyncStackTrace> m_asyncParent;
  V8StackTraceId m_externalParent;

  DISALLOW_COPY_AND_ASSIGN(V8StackTraceImpl);
};

class AsyncStackTrace {
 public:
  static std::shared_ptr<AsyncStackTrace> capture(V8Debugger*,
                                                  int contextGroupId,
                                                  const String16& description,
                                                  int maxStackSize);
  static uintptr_t store(V8Debugger* debugger,
                         std::shared_ptr<AsyncStackTrace> stack);

  std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObject(
      V8Debugger* debugger, int maxAsyncDepth) const;

  // If async stack has suspended task id, it means that at moment when we
  // capture current stack trace we suspended corresponded asynchronous
  // execution flow and it is possible to request pause for a momemnt when
  // that flow is resumed.
  // E.g. every time when we suspend async function we mark corresponded async
  // stack as suspended and every time when this function is resumed we remove
  // suspendedTaskId.
  void setSuspendedTaskId(void* task);
  void* suspendedTaskId() const;

  int contextGroupId() const;
  const String16& description() const;
  std::weak_ptr<AsyncStackTrace> parent() const;
  bool isEmpty() const;
  const V8StackTraceId& externalParent() const { return m_externalParent; }

  const std::vector<std::shared_ptr<StackFrame>>& frames() const {
    return m_frames;
  }

 private:
  AsyncStackTrace(int contextGroupId, const String16& description,
                  std::vector<std::shared_ptr<StackFrame>> frames,
                  std::shared_ptr<AsyncStackTrace> asyncParent,
                  const V8StackTraceId& externalParent);

  int m_contextGroupId;
  uintptr_t m_id;
  void* m_suspendedTaskId;
  String16 m_description;

  std::vector<std::shared_ptr<StackFrame>> m_frames;
  std::weak_ptr<AsyncStackTrace> m_asyncParent;
  V8StackTraceId m_externalParent;

  DISALLOW_COPY_AND_ASSIGN(AsyncStackTrace);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_STACK_TRACE_IMPL_H_
