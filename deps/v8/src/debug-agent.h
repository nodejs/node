// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_AGENT_H_
#define V8_DEBUG_AGENT_H_

#include "../include/v8-debug.h"
#include "platform.h"

namespace v8 {
namespace internal {

// Forward decelrations.
class DebuggerAgentSession;
class Socket;


// Debugger agent which starts a socket listener on the debugger port and
// handles connection from a remote debugger.
class DebuggerAgent: public Thread {
 public:
  DebuggerAgent(Isolate* isolate, const char* name, int port);
  ~DebuggerAgent();

  void Shutdown();
  void WaitUntilListening();

  Isolate* isolate() { return isolate_; }

 private:
  void Run();
  void CreateSession(Socket* socket);
  void DebuggerMessage(const v8::Debug::Message& message);
  void CloseSession();
  void OnSessionClosed(DebuggerAgentSession* session);

  Isolate* isolate_;
  SmartArrayPointer<const char> name_;  // Name of the embedding application.
  int port_;  // Port to use for the agent.
  Socket* server_;  // Server socket for listen/accept.
  bool terminate_;  // Termination flag.
  RecursiveMutex session_access_;  // Mutex guarding access to session_.
  DebuggerAgentSession* session_;  // Current active session if any.
  Semaphore terminate_now_;  // Semaphore to signal termination.
  Semaphore listening_;

  friend class DebuggerAgentSession;
  friend void DebuggerAgentMessageHandler(const v8::Debug::Message& message);

  DISALLOW_COPY_AND_ASSIGN(DebuggerAgent);
};


// Debugger agent session. The session receives requests from the remote
// debugger and sends debugger events/responses to the remote debugger.
class DebuggerAgentSession: public Thread {
 public:
  DebuggerAgentSession(DebuggerAgent* agent, Socket* client)
      : Thread("v8:DbgAgntSessn"),
        agent_(agent), client_(client) {}
  ~DebuggerAgentSession();

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
  static const char* const kContentLength;

  static SmartArrayPointer<char> ReceiveMessage(Socket* conn);
  static bool SendConnectMessage(Socket* conn, const char* embedding_host);
  static bool SendMessage(Socket* conn, const Vector<uint16_t> message);
  static bool SendMessage(Socket* conn, const v8::Handle<v8::String> message);
  static int ReceiveAll(Socket* conn, char* data, int len);
};

} }  // namespace v8::internal

#endif  // V8_DEBUG_AGENT_H_
