// Copyright 2008 the V8 project authors. All rights reserved.
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

#pragma once
#include <v8.h>

namespace v8 {

// NOT IMPLEMENTED
class V8_EXPORT Debug {
 public:
  class ClientData {
  };

  class Message {
   public:
    virtual ~Message() {}
    virtual Handle<String> GetJSON() const = 0;
    virtual Isolate* GetIsolate() const = 0;
  };

  typedef void (*DebugMessageDispatchHandler)();
  typedef void (*MessageHandler)(const Message& message);

  static void DebugBreak(Isolate *isolate = NULL) {}
  static void SetDebugMessageDispatchHandler(
    DebugMessageDispatchHandler handler, bool provide_locker = false) {}
  static bool EnableAgent(
    const char *name = NULL, int port = 0, bool wait_for_connection = false);
  static void Dispose();
  static void DisableAgent() {}
  static bool IsAgentEnabled();
  static void ProcessDebugMessages() {}
  static Local<Context> GetDebugContext();
  static void SetMessageHandler(MessageHandler handler) {}
  static void SendCommand(Isolate* isolate,
                          const uint16_t* command, int length,
                          ClientData* client_data = NULL) {
  }
  static MaybeLocal<Value> GetMirror(Local<Context> context,
                                     Handle<Value> obj) {
    return MaybeLocal<Value>();
  }
};

}  // namespace v8
