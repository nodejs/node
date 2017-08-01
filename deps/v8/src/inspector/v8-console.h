// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8CONSOLE_H_
#define V8_INSPECTOR_V8CONSOLE_H_

#include "src/base/macros.h"

#include "include/v8.h"
#include "src/debug/interface-types.h"

namespace v8_inspector {

class InspectedContext;
class V8InspectorImpl;

// Console API
// https://console.spec.whatwg.org/#console-interface
class V8Console : public v8::debug::ConsoleDelegate {
 public:
  v8::Local<v8::Object> createCommandLineAPI(v8::Local<v8::Context> context);
  void installMemoryGetter(v8::Local<v8::Context> context,
                           v8::Local<v8::Object> console);

  class CommandLineAPIScope {
   public:
    CommandLineAPIScope(v8::Local<v8::Context>,
                        v8::Local<v8::Object> commandLineAPI,
                        v8::Local<v8::Object> global);
    ~CommandLineAPIScope();

   private:
    static void accessorGetterCallback(
        v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
    static void accessorSetterCallback(v8::Local<v8::Name>,
                                       v8::Local<v8::Value>,
                                       const v8::PropertyCallbackInfo<void>&);

    v8::Local<v8::Context> m_context;
    v8::Local<v8::Object> m_commandLineAPI;
    v8::Local<v8::Object> m_global;
    v8::Local<v8::Set> m_installedMethods;
    bool m_cleanup;

    DISALLOW_COPY_AND_ASSIGN(CommandLineAPIScope);
  };

  explicit V8Console(V8InspectorImpl* inspector);

 private:
  void Debug(const v8::debug::ConsoleCallArguments&) override;
  void Error(const v8::debug::ConsoleCallArguments&) override;
  void Info(const v8::debug::ConsoleCallArguments&) override;
  void Log(const v8::debug::ConsoleCallArguments&) override;
  void Warn(const v8::debug::ConsoleCallArguments&) override;
  void Dir(const v8::debug::ConsoleCallArguments&) override;
  void DirXml(const v8::debug::ConsoleCallArguments&) override;
  void Table(const v8::debug::ConsoleCallArguments&) override;
  void Trace(const v8::debug::ConsoleCallArguments&) override;
  void Group(const v8::debug::ConsoleCallArguments&) override;
  void GroupCollapsed(const v8::debug::ConsoleCallArguments&) override;
  void GroupEnd(const v8::debug::ConsoleCallArguments&) override;
  void Clear(const v8::debug::ConsoleCallArguments&) override;
  void Count(const v8::debug::ConsoleCallArguments&) override;
  void Assert(const v8::debug::ConsoleCallArguments&) override;
  void MarkTimeline(const v8::debug::ConsoleCallArguments&) override;
  void Profile(const v8::debug::ConsoleCallArguments&) override;
  void ProfileEnd(const v8::debug::ConsoleCallArguments&) override;
  void Timeline(const v8::debug::ConsoleCallArguments&) override;
  void TimelineEnd(const v8::debug::ConsoleCallArguments&) override;
  void Time(const v8::debug::ConsoleCallArguments&) override;
  void TimeEnd(const v8::debug::ConsoleCallArguments&) override;
  void TimeStamp(const v8::debug::ConsoleCallArguments&) override;

  template <void (V8Console::*func)(const v8::FunctionCallbackInfo<v8::Value>&)>
  static void call(const v8::FunctionCallbackInfo<v8::Value>& info) {
    V8Console* console =
        static_cast<V8Console*>(info.Data().As<v8::External>()->Value());
    (console->*func)(info);
  }
  template <void (V8Console::*func)(const v8::debug::ConsoleCallArguments&)>
  static void call(const v8::FunctionCallbackInfo<v8::Value>& info) {
    V8Console* console =
        static_cast<V8Console*>(info.Data().As<v8::External>()->Value());
    v8::debug::ConsoleCallArguments args(info);
    (console->*func)(args);
  }

  // TODO(foolip): There is no spec for the Memory Info API, see blink-dev:
  // https://groups.google.com/a/chromium.org/d/msg/blink-dev/g5YRCGpC9vs/b4OJz71NmPwJ
  void memoryGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void memorySetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  // CommandLineAPI
  void keysCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void valuesCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void debugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void undebugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void monitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void unmonitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void lastEvaluationResultCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void inspectCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void copyCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void inspectedObject(const v8::FunctionCallbackInfo<v8::Value>&,
                       unsigned num);
  void inspectedObject0(const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 0);
  }
  void inspectedObject1(const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 1);
  }
  void inspectedObject2(const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 2);
  }
  void inspectedObject3(const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 3);
  }
  void inspectedObject4(const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 4);
  }

  V8InspectorImpl* m_inspector;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8CONSOLE_H_
