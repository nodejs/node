// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_CONSOLE_H_
#define V8_INSPECTOR_V8_CONSOLE_H_

#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/member.h"
#include "include/v8-array-buffer.h"
#include "include/v8-container.h"
#include "include/v8-external.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-traced-handle.h"
#include "src/base/macros.h"
#include "src/debug/interface-types.h"
#include "v8-isolate.h"

namespace v8 {
class ObjectTemplate;
class Set;
}  // namespace v8

namespace v8_inspector {

class InspectedContext;
class TaskInfo;
class V8InspectorImpl;

// Console API
// https://console.spec.whatwg.org/#console-namespace
class V8Console final : public v8::Object::Wrappable,
                        public v8::debug::ConsoleDelegate {
 public:
  static constexpr v8::CppHeapPointerTag kPointerTag =
      v8::CppHeapPointerTag::kInspectorV8ConsoleTag;

  v8::Local<v8::Object> wrapConsole(v8::Local<v8::Context> context);
  void Trace(cppgc::Visitor* visitor) const override;

  v8::Local<v8::Object> createCommandLineAPI(v8::Local<v8::Context> context,
                                             int sessionId);
  void installMemoryGetter(v8::Local<v8::Context> context,
                           v8::Local<v8::Object> console);
  void installAsyncStackTaggingAPI(v8::Local<v8::Context> context,
                                   v8::Local<v8::Object> console);

  class V8_NODISCARD CommandLineAPIScope {
   public:
    CommandLineAPIScope(v8::Local<v8::Context>,
                        v8::Local<v8::Object> commandLineAPI,
                        v8::Local<v8::Object> global);
    ~CommandLineAPIScope();
    CommandLineAPIScope(const CommandLineAPIScope&) = delete;
    CommandLineAPIScope& operator=(const CommandLineAPIScope&) = delete;

   private:
    constexpr static uint32_t kCommandLineAPIIndex = 0;
    constexpr static uint32_t kFirstInstalledMethodIndex = 1;
    constexpr static uint32_t kHeaderLength = kFirstInstalledMethodIndex;

    static void accessorGetterCallback(
        v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
    static void accessorSetterCallback(
        v8::Local<v8::Name>, v8::Local<v8::Value>,
        const v8::PropertyCallbackInfo<v8::Boolean>&);

    v8::Isolate* isolate() const { return m_isolate; }
    v8::Local<v8::Context> context() const { return m_context.Get(isolate()); }
    v8::Local<v8::Array> data() const { return m_data.Get(isolate()); }
    v8::Local<v8::Object> global() const { return m_global.Get(isolate()); }

    v8::Isolate* m_isolate;
    v8::Global<v8::Context> m_context;
    v8::Global<v8::Array> m_data;
    v8::Global<v8::Object> m_global;
  };

  explicit V8Console(V8InspectorImpl* inspector);
  void clearInspector() {
    m_inspector = nullptr;
    m_taskTemplate.Reset();
    m_consoleTemplate.Reset();
    m_consoleWrapper.Reset();
    m_consoleContext.Reset();
  }

 private:
  friend class TaskInfo;

  void Debug(const v8::debug::ConsoleCallArguments&,
             const v8::debug::ConsoleContext& consoleContext) override;
  void Error(const v8::debug::ConsoleCallArguments&,
             const v8::debug::ConsoleContext& consoleContext) override;
  void Info(const v8::debug::ConsoleCallArguments&,
            const v8::debug::ConsoleContext& consoleContext) override;
  void Log(const v8::debug::ConsoleCallArguments&,
           const v8::debug::ConsoleContext& consoleContext) override;
  void Warn(const v8::debug::ConsoleCallArguments&,
            const v8::debug::ConsoleContext& consoleContext) override;
  void Dir(const v8::debug::ConsoleCallArguments&,
           const v8::debug::ConsoleContext& consoleContext) override;
  void DirXml(const v8::debug::ConsoleCallArguments&,
              const v8::debug::ConsoleContext& consoleContext) override;
  void Table(const v8::debug::ConsoleCallArguments&,
             const v8::debug::ConsoleContext& consoleContext) override;
  void Trace(const v8::debug::ConsoleCallArguments&,
             const v8::debug::ConsoleContext& consoleContext) override;
  void Group(const v8::debug::ConsoleCallArguments&,
             const v8::debug::ConsoleContext& consoleContext) override;
  void GroupCollapsed(const v8::debug::ConsoleCallArguments&,
                      const v8::debug::ConsoleContext& consoleContext) override;
  void GroupEnd(const v8::debug::ConsoleCallArguments&,
                const v8::debug::ConsoleContext& consoleContext) override;
  void Clear(const v8::debug::ConsoleCallArguments&,
             const v8::debug::ConsoleContext& consoleContext) override;
  void Count(const v8::debug::ConsoleCallArguments&,
             const v8::debug::ConsoleContext& consoleContext) override;
  void CountReset(const v8::debug::ConsoleCallArguments&,
                  const v8::debug::ConsoleContext& consoleContext) override;
  void Assert(const v8::debug::ConsoleCallArguments&,
              const v8::debug::ConsoleContext& consoleContext) override;
  void Profile(const v8::debug::ConsoleCallArguments&,
               const v8::debug::ConsoleContext& consoleContext) override;
  void ProfileEnd(const v8::debug::ConsoleCallArguments&,
                  const v8::debug::ConsoleContext& consoleContext) override;
  void Time(const v8::debug::ConsoleCallArguments&,
            const v8::debug::ConsoleContext& consoleContext) override;
  void TimeLog(const v8::debug::ConsoleCallArguments&,
               const v8::debug::ConsoleContext& consoleContext) override;
  void TimeEnd(const v8::debug::ConsoleCallArguments&,
               const v8::debug::ConsoleContext& consoleContext) override;
  void TimeStamp(const v8::debug::ConsoleCallArguments&,
                 const v8::debug::ConsoleContext& consoleContext) override;

  template <void (V8Console::*func)(const v8::FunctionCallbackInfo<v8::Value>&)>
  static void call(const v8::FunctionCallbackInfo<v8::Value>& info) {
    V8Console* console = v8::Object::Unwrap<kPointerTag, V8Console>(
        info.GetIsolate(), info.Data().As<v8::Object>());
    CHECK_NOT_NULL(console);
    CHECK_NOT_NULL(console->m_inspector);
    (console->*func)(info);
  }
  template <void (V8Console::*func)(const v8::FunctionCallbackInfo<v8::Value>&,
                                    int)>
  static void call(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Local<v8::Array> data = info.Data().As<v8::Array>();
    v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
    v8::Local<v8::Value> console_value;
    if (!data->Get(context, 0).ToLocal(&console_value) ||
        !console_value->IsObject()) {
      return;
    }
    V8Console* console = v8::Object::Unwrap<kPointerTag, V8Console>(
        info.GetIsolate(), console_value.As<v8::Object>());
    CHECK_NOT_NULL(console);
    v8::Local<v8::Value> session_value;
    if (!data->Get(context, 1).ToLocal(&session_value) ||
        !session_value->IsInt32()) {
      return;
    }
    int sessionId = session_value.As<v8::Int32>()->Value();
    (console->*func)(info, sessionId);
  }

  template <void (V8Console::*func)(const v8::debug::ConsoleCallArguments&,
                                    const v8::debug::ConsoleContext&)>
  static void call(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Local<v8::Array> data = info.Data().As<v8::Array>();
    v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
    v8::Local<v8::Value> console_value;
    if (!data->Get(context, 0).ToLocal(&console_value) ||
        !console_value->IsObject()) {
      return;
    }
    V8Console* console = v8::Object::Unwrap<kPointerTag, V8Console>(
        info.GetIsolate(), console_value.As<v8::Object>());
    CHECK_NOT_NULL(console);
    v8::debug::ConsoleCallArguments args(info);
    (console->*func)(args, v8::debug::ConsoleContext());
  }

  // TODO(foolip): There is no spec for the Memory Info API, see blink-dev:
  // https://groups.google.com/a/chromium.org/d/msg/blink-dev/g5YRCGpC9vs/b4OJz71NmPwJ
  void memoryGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void memorySetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  void createTask(const v8::FunctionCallbackInfo<v8::Value>&);

  // CommandLineAPI
  void keysCallback(const v8::FunctionCallbackInfo<v8::Value>&, int sessionId);
  void valuesCallback(const v8::FunctionCallbackInfo<v8::Value>&,
                      int sessionId);
  void debugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&,
                             int sessionId);
  void undebugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&,
                               int sessionId);
  void monitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&,
                               int sessionId);
  void unmonitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&,
                                 int sessionId);
  void lastEvaluationResultCallback(const v8::FunctionCallbackInfo<v8::Value>&,
                                    int sessionId);
  void inspectCallback(const v8::FunctionCallbackInfo<v8::Value>&,
                       int sessionId);
  void copyCallback(const v8::FunctionCallbackInfo<v8::Value>&, int sessionId);
  void inspectedObject(const v8::FunctionCallbackInfo<v8::Value>&,
                       int sessionId, unsigned num);
  void inspectedObject0(const v8::FunctionCallbackInfo<v8::Value>& info,
                        int sessionId) {
    inspectedObject(info, sessionId, 0);
  }
  void inspectedObject1(const v8::FunctionCallbackInfo<v8::Value>& info,
                        int sessionId) {
    inspectedObject(info, sessionId, 1);
  }
  void inspectedObject2(const v8::FunctionCallbackInfo<v8::Value>& info,
                        int sessionId) {
    inspectedObject(info, sessionId, 2);
  }
  void inspectedObject3(const v8::FunctionCallbackInfo<v8::Value>& info,
                        int sessionId) {
    inspectedObject(info, sessionId, 3);
  }
  void inspectedObject4(const v8::FunctionCallbackInfo<v8::Value>& info,
                        int sessionId) {
    inspectedObject(info, sessionId, 4);
  }
  void queryObjectsCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                            int sessionId);

  // Lazily creates m_taskTemplate and returns a local handle to it.
  // We can't create the template upfront as to not be part of Chromium's
  // context snapshot.
  v8::Local<v8::ObjectTemplate> taskTemplate();

  V8InspectorImpl* m_inspector;

  // We cache the task template for the async stack tagging API for faster
  // instantiation. Use `taskTemplate()` to retrieve the lazily created
  // template.
  v8::TracedReference<v8::ObjectTemplate> m_taskTemplate;
  v8::TracedReference<v8::ObjectTemplate> m_consoleTemplate;

  // Weak references:
  //
  // We cache the V8Console wrapper object for the current context in
  // `m_consoleWrapper`. `m_consoleContext` holds a weak reference to the
  // context for which `m_consoleWrapper` was created, ensuring we only reuse
  // the cached wrapper if the context remains the same.
  v8::Global<v8::Object> m_consoleWrapper;
  v8::Global<v8::Context> m_consoleContext;
};

/**
 * Each JS task object created via `console.createTask` has a corresponding
 * `TaskInfo` object on the C++ side (in a 1:1 relationship).
 *
 * `TaskInfo` is an Oilpan object wrapped in the JS task object.
 *
 * `TaskInfo` holds all the necessary information we need to properly cancel
 * the corresponding async task when the JS task object gets GC'ed and its
 * destructor runs.
 */
class TaskInfo : public v8::Object::Wrappable {
 public:
  static constexpr v8::CppHeapPointerTag kPointerTag =
      v8::CppHeapPointerTag::kInspectorTaskInfoTag;

  TaskInfo(v8::Isolate* isolate, V8Console* console);
  ~TaskInfo() override;

  // For these task IDs we duplicate the ID logic from blink and use even
  // pointers compared to the odd IDs we use for promises. This guarantees that
  // we don't have any conflicts between task IDs.
  void* Id() const {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_id.get())
                                   << 1);
  }

  void Trace(cppgc::Visitor* visitor) const override;
  static void runTask(const v8::FunctionCallbackInfo<v8::Value>& info);

  // We store an off-heap pointer to the isolate so we can safely look up the
  // inspector during ~TaskInfo() via v8::debug::GetInspector(m_isolate).
  // Dereferencing m_console during sweeping is prohibited by Oilpan rules
  // since V8Console is an on-heap object that might have already been swept.
  // Furthermore, GetInspector provides robust protection against teardown
  // races: if the inspector shuts down before TaskInfo is destructed, V8
  // automatically clears the isolate's inspector pointer, ensuring
  // GetInspector safely returns nullptr.
  v8::Isolate* m_isolate;
  cppgc::Member<V8Console> m_console;
  // Task IDs must be unique across the entire process, but they are generated
  // in multiple places (e.g. in Blink), making synchronization complex. To
  // avoid coordinated ID generation, we use the memory allocator as an ID
  // generator by using an allocation address as the ID. In principle, we could
  // use the `this` pointer, but since TaskInfo is an Oilpan object, a future
  // moving GC could cause the `this` pointer address to change over time.
  // Therefore, we perform the smallest possible allocation and use the address
  // of that memory. It is stored in a std::unique_ptr so that the memory gets
  // deallocated eventually.
  std::unique_ptr<char> m_id;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_CONSOLE_H_
