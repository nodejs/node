// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8STACKTRACEIMPL_H_
#define V8_INSPECTOR_V8STACKTRACEIMPL_H_

#include <vector>

#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

class TracedValue;
class V8Debugger;

// Note: async stack trace may have empty top stack with non-empty tail to
// indicate
// that current native-only state had some async story.
// On the other hand, any non-top async stack is guaranteed to be non-empty.
class V8StackTraceImpl final : public V8StackTrace {
 public:
  static const size_t maxCallStackSizeToCapture = 200;

  class Frame {
   public:
    Frame();
    Frame(const String16& functionName, const String16& scriptId,
          const String16& scriptName, int lineNumber, int column = 0);
    ~Frame();

    const String16& functionName() const { return m_functionName; }
    const String16& scriptId() const { return m_scriptId; }
    const String16& sourceURL() const { return m_scriptName; }
    int lineNumber() const { return m_lineNumber; }
    int columnNumber() const { return m_columnNumber; }
    Frame clone() const;

   private:
    friend class V8StackTraceImpl;
    std::unique_ptr<protocol::Runtime::CallFrame> buildInspectorObject() const;
    void toTracedValue(TracedValue*) const;

    String16 m_functionName;
    String16 m_scriptId;
    String16 m_scriptName;
    int m_lineNumber;
    int m_columnNumber;
  };

  static void setCaptureStackTraceForUncaughtExceptions(v8::Isolate*,
                                                        bool capture);
  static std::unique_ptr<V8StackTraceImpl> create(
      V8Debugger*, int contextGroupId, v8::Local<v8::StackTrace>,
      size_t maxStackSize, const String16& description = String16());
  static std::unique_ptr<V8StackTraceImpl> capture(
      V8Debugger*, int contextGroupId, size_t maxStackSize,
      const String16& description = String16());

  // This method drops the async chain. Use cloneImpl() instead.
  std::unique_ptr<V8StackTrace> clone() override;
  std::unique_ptr<V8StackTraceImpl> cloneImpl();
  std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObjectForTail(
      V8Debugger*) const;
  std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObjectImpl()
      const;
  ~V8StackTraceImpl() override;

  // V8StackTrace implementation.
  bool isEmpty() const override { return !m_frames.size(); };
  StringView topSourceURL() const override;
  int topLineNumber() const override;
  int topColumnNumber() const override;
  StringView topScriptId() const override;
  StringView topFunctionName() const override;
  std::unique_ptr<protocol::Runtime::API::StackTrace> buildInspectorObject()
      const override;
  std::unique_ptr<StringBuffer> toString() const override;

 private:
  V8StackTraceImpl(int contextGroupId, const String16& description,
                   std::vector<Frame>& frames,
                   std::unique_ptr<V8StackTraceImpl> parent);

  int m_contextGroupId;
  String16 m_description;
  std::vector<Frame> m_frames;
  std::unique_ptr<V8StackTraceImpl> m_parent;

  DISALLOW_COPY_AND_ASSIGN(V8StackTraceImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8STACKTRACEIMPL_H_
