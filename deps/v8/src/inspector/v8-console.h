// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8CONSOLE_H_
#define V8_INSPECTOR_V8CONSOLE_H_

#include "src/base/macros.h"

#include "include/v8.h"

namespace v8_inspector {

class InspectedContext;
class V8InspectorImpl;

// Console API
// https://console.spec.whatwg.org/#console-interface
class V8Console {
 public:
  v8::Local<v8::Object> createConsole(v8::Local<v8::Context> context);
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
  void debugCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void errorCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void infoCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void logCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void warnCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void dirCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void dirxmlCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void tableCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void traceCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void groupCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void groupCollapsedCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void groupEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void clearCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void countCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void assertCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void markTimelineCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void profileCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void profileEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void timelineCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void timelineEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void timeCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void timeEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void timeStampCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  template <void (V8Console::*func)(const v8::FunctionCallbackInfo<v8::Value>&)>
  static void call(const v8::FunctionCallbackInfo<v8::Value>& info) {
    V8Console* console =
        static_cast<V8Console*>(info.Data().As<v8::External>()->Value());
    (console->*func)(info);
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
