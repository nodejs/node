// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8.h"
#include "src/d8-debug.h"

namespace v8 {

void PrintPrompt(bool is_running) {
  const char* prompt = is_running? "> " : "dbg> ";
  printf("%s", prompt);
  fflush(stdout);
}


void HandleDebugEvent(const Debug::EventDetails& event_details) {
  // TODO(svenpanne) There should be a way to retrieve this in the callback.
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  DebugEvent event = event_details.GetEvent();
  // Check for handled event.
  if (event != Break && event != Exception && event != AfterCompile) {
    return;
  }

  TryCatch try_catch;

  // Get the toJSONProtocol function on the event and get the JSON format.
  Local<String> to_json_fun_name =
      String::NewFromUtf8(isolate, "toJSONProtocol");
  Handle<Object> event_data = event_details.GetEventData();
  Local<Function> to_json_fun =
      Local<Function>::Cast(event_data->Get(to_json_fun_name));
  Local<Value> event_json = to_json_fun->Call(event_data, 0, NULL);
  if (try_catch.HasCaught()) {
    Shell::ReportException(isolate, &try_catch);
    return;
  }

  // Print the event details.
  Handle<Object> details =
      Shell::DebugMessageDetails(isolate, Handle<String>::Cast(event_json));
  if (try_catch.HasCaught()) {
    Shell::ReportException(isolate, &try_catch);
    return;
  }
  String::Utf8Value str(details->Get(String::NewFromUtf8(isolate, "text")));
  if (str.length() == 0) {
    // Empty string is used to signal not to process this event.
    return;
  }
  printf("%s\n", *str);

  // Get the debug command processor.
  Local<String> fun_name =
      String::NewFromUtf8(isolate, "debugCommandProcessor");
  Handle<Object> exec_state = event_details.GetExecutionState();
  Local<Function> fun = Local<Function>::Cast(exec_state->Get(fun_name));
  Local<Object> cmd_processor =
      Local<Object>::Cast(fun->Call(exec_state, 0, NULL));
  if (try_catch.HasCaught()) {
    Shell::ReportException(isolate, &try_catch);
    return;
  }

  static const int kBufferSize = 256;
  bool running = false;
  while (!running) {
    char command[kBufferSize];
    PrintPrompt(running);
    char* str = fgets(command, kBufferSize, stdin);
    if (str == NULL) break;

    // Ignore empty commands.
    if (strlen(command) == 0) continue;

    TryCatch try_catch;

    // Convert the debugger command to a JSON debugger request.
    Handle<Value> request = Shell::DebugCommandToJSONRequest(
        isolate, String::NewFromUtf8(isolate, command));
    if (try_catch.HasCaught()) {
      Shell::ReportException(isolate, &try_catch);
      continue;
    }

    // If undefined is returned the command was handled internally and there is
    // no JSON to send.
    if (request->IsUndefined()) {
      continue;
    }

    Handle<String> fun_name;
    Handle<Function> fun;
    // All the functions used below take one argument.
    static const int kArgc = 1;
    Handle<Value> args[kArgc];

    // Invoke the JavaScript to convert the debug command line to a JSON
    // request, invoke the JSON request and convert the JSON respose to a text
    // representation.
    fun_name = String::NewFromUtf8(isolate, "processDebugRequest");
    fun = Handle<Function>::Cast(cmd_processor->Get(fun_name));
    args[0] = request;
    Handle<Value> response_val = fun->Call(cmd_processor, kArgc, args);
    if (try_catch.HasCaught()) {
      Shell::ReportException(isolate, &try_catch);
      continue;
    }
    Handle<String> response = Handle<String>::Cast(response_val);

    // Convert the debugger response into text details and the running state.
    Handle<Object> response_details =
        Shell::DebugMessageDetails(isolate, response);
    if (try_catch.HasCaught()) {
      Shell::ReportException(isolate, &try_catch);
      continue;
    }
    String::Utf8Value text_str(
        response_details->Get(String::NewFromUtf8(isolate, "text")));
    if (text_str.length() > 0) {
      printf("%s\n", *text_str);
    }
    running = response_details->Get(String::NewFromUtf8(isolate, "running"))
                  ->ToBoolean(isolate)
                  ->Value();
  }
}

}  // namespace v8
