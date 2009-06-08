// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_DEBUG_AGENT_H_
#define V8_DEBUG_AGENT_H_

#ifdef ENABLE_DEBUGGER_SUPPORT
#include "../include/v8-debug.h"
#include "platform.h"

namespace v8 {
namespace internal {

// Forward decelrations.
class DebuggerAgentSession;


// Debugger agent which starts a socket listener on the debugger port and
// handles connection from a remote debugger.
class DebuggerAgent: public Thread {
 public:
  explicit DebuggerAgent(const char* name, int port)
      : name_(StrDup(name)), port_(port),
        server_(OS::CreateSocket()), terminate_(false),
        session_access_(OS::CreateMutex()), session_(NULL),
        terminate_now_(OS::CreateSemaphore(0)) {
    ASSERT(instance_ == NULL);
    instance_ = this;
  }
  ~DebuggerAgent() {
     instance_ = NULL;
     delete server_;
  }

  void Shutdown();

 private:
  void Run();
  void CreateSession(Socket* socket);
  void DebuggerMessage(const v8::Debug::Message& message);
  void CloseSession();
  void OnSessionClosed(DebuggerAgentSession* session);

  SmartPointer<const char> name_;  // Name of the embedding application.
  int port_;  // Port to use for the agent.
  Socket* server_;  // Server socket for listen/accept.
  bool terminate_;  // Termination flag.
  Mutex* session_access_;  // Mutex guarging access to session_.
  DebuggerAgentSession* session_;  // Current active session if any.
  Semaphore* terminate_now_;  // Semaphore to signal termination.

  static DebuggerAgent* instance_;

  friend class DebuggerAgentSession;
  friend void DebuggerAgentMessageHandler(const v8::Debug::Message& message);

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgent);
};


// Debugger agent session. The session receives requests from the remote
// debugger and sends debugger events/responses to the remote debugger.
class DebuggerAgentSession: public Thread {
 public:
  DebuggerAgentSession(DebuggerAgent* agent, Socket* client)
      : agent_(agent), client_(client) {}

  void DebuggerMessage(Vector<uint16_t> message);
  void Shutdown();

 private:
  void Run();

  void DebuggerMessage(Vector<char> message);

  DebuggerAgent* agent_;
  Socket* client_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgentSession);
};


// Utility methods factored out to be used by the D8 shell as well.
class DebuggerAgentUtil {
 public:
  static const char* kContentLength;
  static int kContentLengthSize;

  static SmartPointer<char> ReceiveMessage(const Socket* conn);
  static bool SendConnectMessage(const Socket* conn,
                                 const char* embedding_host);
  static bool SendMessage(const Socket* conn, const Vector<uint16_t> message);
  static bool SendMessage(const Socket* conn,
                          const v8::Handle<v8::String> message);
  static int ReceiveAll(const Socket* conn, char* data, int len);
};

} }  // namespace v8::internal

#endif  // ENABLE_DEBUGGER_SUPPORT

#endif  // V8_DEBUG_AGENT_H_
