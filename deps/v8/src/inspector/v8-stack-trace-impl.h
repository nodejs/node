// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_STACK_TRACE_IMPL_H_
#define V8_INSPECTOR_V8_STACK_TRACE_IMPL_H_

#include <memory>
#include <vector>

#include "include/v8-inspector.h"
#include "include/v8-local-handle.h"
#include "src/base/macros.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/string-16.h"

namespace v8 {
class StackFrame;
class StackTrace;
}  // namespace v8

namespace v8_inspector {

class AsyncStackTrace;
class V8Debugger;
struct V8StackTraceId;

class StackFrame {
 public:
  StackFrame(String16&& functionName, int scriptId, String16&& sourceURL,
             int lineNumber, int columnNumber, bool hasSourceURLComment);
  ~StackFrame() = default;

  const String16& functionName() const;
  int scriptId() const;
  const String16& sourceURL() const;
  int lineNumber() const;    // 0-based.
  int columnNumber() const;  // 0-based.
  std::unique_ptr<protocol::Runtime::CallFrame> buildInspectorObject(
      V8InspectorClient* client) const;
  bool isEqual(StackFrame* frame) const;

 private:
  String16 m_functionName;
  int m_scriptId;
  String16 m_sourceURL;
  int m_lineNumber;    // 0-based.
  int m_columnNumber;  // 0-based.
  bool m_hasSourceURLComment;
};

class V8StackTraceImpl : public V8StackTrace {
 public:
  static constexpr int kDefaultMaxCallStackSizeToCapture = 200;

  static std::unique_ptr<V8StackTraceImpl> create(V8Debugger*,
                                                  v8::Local<v8::StackTrace>,
                                                  int maxStackSize);
  static std::unique_ptr<V8StackTraceImpl> capture(V8Debugger*,
                                                   int maxStackSize);

  ~V8StackTraceImpl() override;
  V8StackTraceImpl(const V8StackTraceImpl&) = delete;
  V8StackTraceImpl& operator=(const V8StackTraceImpl&) = delete;
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
  int topScriptId() const override;
  StringView topFunctionName() const override;
  std::unique_ptr<protocol::Runtime::API::StackTrace> buildInspectorObject(
      int maxAsyncDepth) const override;
  std::unique_ptr<StringBuffer> toString() const override;

  bool isEqualIgnoringTopFrame(V8StackTraceImpl* stackTrace) const;

  std::vector<V8StackFrame> frames() const override;

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
};

class AsyncStackTrace {
 public:
  AsyncStackTrace(const AsyncStackTrace&) = delete;
  AsyncStackTrace& operator=(const AsyncStackTrace&) = delete;
  static std::shared_ptr<AsyncStackTrace> capture(V8Debugger*,
                                                  const String16& description,
                                                  bool skipTopFrame = false);
  static uintptr_t store(V8Debugger* debugger,
                         std::shared_ptr<AsyncStackTrace> stack);

  std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObject(
      V8Debugger* debugger, int maxAsyncDepth) const;

  const String16& description() const;
  std::weak_ptr<AsyncStackTrace> parent() const;
  bool isEmpty() const;
  const V8StackTraceId& externalParent() const { return m_externalParent; }

  const std::vector<std::shared_ptr<StackFrame>>& frames() const {
    return m_frames;
  }

 private:
  AsyncStackTrace(const String16& description,
                  std::vector<std::shared_ptr<StackFrame>> frames,
                  std::shared_ptr<AsyncStackTrace> asyncParent,
                  const V8StackTraceId& externalParent);

  uintptr_t m_id;
  String16 m_description;

  std::vector<std::shared_ptr<StackFrame>> m_frames;
  std::weak_ptr<AsyncStackTrace> m_asyncParent;
  V8StackTraceId m_externalParent;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_STACK_TRACE_IMPL_H_
