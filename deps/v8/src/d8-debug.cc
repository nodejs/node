// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef ENABLE_DEBUGGER_SUPPORT

#include "d8.h"
#include "d8-debug.h"
#include "platform.h"
#include "debug-agent.h"


namespace v8 {

static bool was_running = true;

void PrintPrompt(bool is_running) {
  const char* prompt = is_running? "> " : "dbg> ";
  was_running = is_running;
  printf("%s", prompt);
  fflush(stdout);
}


void PrintPrompt() {
  PrintPrompt(was_running);
}


void HandleDebugEvent(DebugEvent event,
                      Handle<Object> exec_state,
                      Handle<Object> event_data,
                      Handle<Value> data) {
  HandleScope scope;

  // Check for handled event.
  if (event != Break && event != Exception && event != AfterCompile) {
    return;
  }

  TryCatch try_catch;

  // Get the toJSONProtocol function on the event and get the JSON format.
  Local<String> to_json_fun_name = String::New("toJSONProtocol");
  Local<Function> to_json_fun =
      Function::Cast(*event_data->Get(to_json_fun_name));
  Local<Value> event_json = to_json_fun->Call(event_data, 0, NULL);
  if (try_catch.HasCaught()) {
    Shell::ReportException(&try_catch);
    return;
  }

  // Print the event details.
  Handle<Object> details =
      Shell::DebugMessageDetails(Handle<String>::Cast(event_json));
  if (try_catch.HasCaught()) {
    Shell::ReportException(&try_catch);
    return;
  }
  String::Utf8Value str(details->Get(String::New("text")));
  if (str.length() == 0) {
    // Empty string is used to signal not to process this event.
    return;
  }
  printf("%s\n", *str);

  // Get the debug command processor.
  Local<String> fun_name = String::New("debugCommandProcessor");
  Local<Function> fun = Function::Cast(*exec_state->Get(fun_name));
  Local<Object> cmd_processor =
      Object::Cast(*fun->Call(exec_state, 0, NULL));
  if (try_catch.HasCaught()) {
    Shell::ReportException(&try_catch);
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
    Handle<Value> request =
        Shell::DebugCommandToJSONRequest(String::New(command));
    if (try_catch.HasCaught()) {
      Shell::ReportException(&try_catch);
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
    fun_name = String::New("processDebugRequest");
    fun = Handle<Function>::Cast(cmd_processor->Get(fun_name));
    args[0] = request;
    Handle<Value> response_val = fun->Call(cmd_processor, kArgc, args);
    if (try_catch.HasCaught()) {
      Shell::ReportException(&try_catch);
      continue;
    }
    Handle<String> response = Handle<String>::Cast(response_val);

    // Convert the debugger response into text details and the running state.
    Handle<Object> response_details = Shell::DebugMessageDetails(response);
    if (try_catch.HasCaught()) {
      Shell::ReportException(&try_catch);
      continue;
    }
    String::Utf8Value text_str(response_details->Get(String::New("text")));
    if (text_str.length() > 0) {
      printf("%s\n", *text_str);
    }
    running =
        response_details->Get(String::New("running"))->ToBoolean()->Value();
  }
}


void RunRemoteDebugger(int port) {
  RemoteDebugger debugger(port);
  debugger.Run();
}


void RemoteDebugger::Run() {
  bool ok;

  // Make sure that socket support is initialized.
  ok = i::Socket::SetUp();
  if (!ok) {
    printf("Unable to initialize socket support %d\n", i::Socket::LastError());
    return;
  }

  // Connect to the debugger agent.
  conn_ = i::OS::CreateSocket();
  static const int kPortStrSize = 6;
  char port_str[kPortStrSize];
  i::OS::SNPrintF(i::Vector<char>(port_str, kPortStrSize), "%d", port_);
  ok = conn_->Connect("localhost", port_str);
  if (!ok) {
    printf("Unable to connect to debug agent %d\n", i::Socket::LastError());
    return;
  }

  // Start the receiver thread.
  ReceiverThread receiver(this);
  receiver.Start();

  // Start the keyboard thread.
  KeyboardThread keyboard(this);
  keyboard.Start();
  PrintPrompt();

  // Process events received from debugged VM and from the keyboard.
  bool terminate = false;
  while (!terminate) {
    event_available_->Wait();
    RemoteDebuggerEvent* event = GetEvent();
    switch (event->type()) {
      case RemoteDebuggerEvent::kMessage:
        HandleMessageReceived(event->data());
        break;
      case RemoteDebuggerEvent::kKeyboard:
        HandleKeyboardCommand(event->data());
        break;
      case RemoteDebuggerEvent::kDisconnect:
        terminate = true;
        break;

      default:
        UNREACHABLE();
    }
    delete event;
  }

  // Wait for the receiver thread to end.
  receiver.Join();
}


void RemoteDebugger::MessageReceived(i::SmartArrayPointer<char> message) {
  RemoteDebuggerEvent* event =
      new RemoteDebuggerEvent(RemoteDebuggerEvent::kMessage, message);
  AddEvent(event);
}


void RemoteDebugger::KeyboardCommand(i::SmartArrayPointer<char> command) {
  RemoteDebuggerEvent* event =
      new RemoteDebuggerEvent(RemoteDebuggerEvent::kKeyboard, command);
  AddEvent(event);
}


void RemoteDebugger::ConnectionClosed() {
  RemoteDebuggerEvent* event =
      new RemoteDebuggerEvent(RemoteDebuggerEvent::kDisconnect,
                              i::SmartArrayPointer<char>());
  AddEvent(event);
}


void RemoteDebugger::AddEvent(RemoteDebuggerEvent* event) {
  i::ScopedLock lock(event_access_);
  if (head_ == NULL) {
    ASSERT(tail_ == NULL);
    head_ = event;
    tail_ = event;
  } else {
    ASSERT(tail_ != NULL);
    tail_->set_next(event);
    tail_ = event;
  }
  event_available_->Signal();
}


RemoteDebuggerEvent* RemoteDebugger::GetEvent() {
  i::ScopedLock lock(event_access_);
  ASSERT(head_ != NULL);
  RemoteDebuggerEvent* result = head_;
  head_ = head_->next();
  if (head_ == NULL) {
    ASSERT(tail_ == result);
    tail_ = NULL;
  }
  return result;
}


void RemoteDebugger::HandleMessageReceived(char* message) {
  Locker lock;
  HandleScope scope;

  // Print the event details.
  TryCatch try_catch;
  Handle<Object> details =
      Shell::DebugMessageDetails(Handle<String>::Cast(String::New(message)));
  if (try_catch.HasCaught()) {
    Shell::ReportException(&try_catch);
    PrintPrompt();
    return;
  }
  String::Utf8Value str(details->Get(String::New("text")));
  if (str.length() == 0) {
    // Empty string is used to signal not to process this event.
    return;
  }
  if (*str != NULL) {
    printf("%s\n", *str);
  } else {
    printf("???\n");
  }

  bool is_running = details->Get(String::New("running"))->ToBoolean()->Value();
  PrintPrompt(is_running);
}


void RemoteDebugger::HandleKeyboardCommand(char* command) {
  Locker lock;
  HandleScope scope;

  // Convert the debugger command to a JSON debugger request.
  TryCatch try_catch;
  Handle<Value> request =
      Shell::DebugCommandToJSONRequest(String::New(command));
  if (try_catch.HasCaught()) {
    Shell::ReportException(&try_catch);
    PrintPrompt();
    return;
  }

  // If undefined is returned the command was handled internally and there is
  // no JSON to send.
  if (request->IsUndefined()) {
    PrintPrompt();
    return;
  }

  // Send the JSON debugger request.
  i::DebuggerAgentUtil::SendMessage(conn_, Handle<String>::Cast(request));
}


void ReceiverThread::Run() {
  // Receive the connect message (with empty body).
  i::SmartArrayPointer<char> message =
      i::DebuggerAgentUtil::ReceiveMessage(remote_debugger_->conn());
  ASSERT(*message == NULL);

  while (true) {
    // Receive a message.
    i::SmartArrayPointer<char> message =
        i::DebuggerAgentUtil::ReceiveMessage(remote_debugger_->conn());
    if (*message == NULL) {
      remote_debugger_->ConnectionClosed();
      return;
    }

    // Pass the message to the main thread.
    remote_debugger_->MessageReceived(message);
  }
}


void KeyboardThread::Run() {
  static const int kBufferSize = 256;
  while (true) {
    // read keyboard input.
    char command[kBufferSize];
    char* str = fgets(command, kBufferSize, stdin);
    if (str == NULL) {
      break;
    }

    // Pass the keyboard command to the main thread.
    remote_debugger_->KeyboardCommand(
        i::SmartArrayPointer<char>(i::StrDup(command)));
  }
}


}  // namespace v8

#endif  // ENABLE_DEBUGGER_SUPPORT
