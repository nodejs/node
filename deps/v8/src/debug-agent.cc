// Copyright 2009 the V8 project authors. All rights reserved.
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


#include "v8.h"
#include "debug.h"
#include "debug-agent.h"

#ifdef ENABLE_DEBUGGER_SUPPORT

namespace v8 {
namespace internal {

// Public V8 debugger API message handler function. This function just delegates
// to the debugger agent through it's data parameter.
void DebuggerAgentMessageHandler(const v8::Debug::Message& message) {
  DebuggerAgent* agent = Isolate::Current()->debugger_agent_instance();
  ASSERT(agent != NULL);
  agent->DebuggerMessage(message);
}


// Debugger agent main thread.
void DebuggerAgent::Run() {
  const int kOneSecondInMicros = 1000000;

  // Allow this socket to reuse port even if still in TIME_WAIT.
  server_->SetReuseAddress(true);

  // First bind the socket to the requested port.
  bool bound = false;
  while (!bound && !terminate_) {
    bound = server_->Bind(port_);

    // If an error occurred wait a bit before retrying. The most common error
    // would be that the port is already in use so this avoids a busy loop and
    // make the agent take over the port when it becomes free.
    if (!bound) {
      PrintF("Failed to open socket on port %d, "
          "waiting %d ms before retrying\n", port_, kOneSecondInMicros / 1000);
      terminate_now_->Wait(kOneSecondInMicros);
    }
  }

  // Accept connections on the bound port.
  while (!terminate_) {
    bool ok = server_->Listen(1);
    listening_->Signal();
    if (ok) {
      // Accept the new connection.
      Socket* client = server_->Accept();
      ok = client != NULL;
      if (ok) {
        // Create and start a new session.
        CreateSession(client);
      }
    }
  }
}


void DebuggerAgent::Shutdown() {
  // Set the termination flag.
  terminate_ = true;

  // Signal termination and make the server exit either its listen call or its
  // binding loop. This makes sure that no new sessions can be established.
  terminate_now_->Signal();
  server_->Shutdown();
  Join();

  // Close existing session if any.
  CloseSession();
}


void DebuggerAgent::WaitUntilListening() {
  listening_->Wait();
}

static const char* kCreateSessionMessage =
    "Remote debugging session already active\r\n";

void DebuggerAgent::CreateSession(Socket* client) {
  ScopedLock with(session_access_);

  // If another session is already established terminate this one.
  if (session_ != NULL) {
    client->Send(kCreateSessionMessage, StrLength(kCreateSessionMessage));
    delete client;
    return;
  }

  // Create a new session and hook up the debug message handler.
  session_ = new DebuggerAgentSession(this, client);
  isolate_->debugger()->SetMessageHandler(DebuggerAgentMessageHandler);
  session_->Start();
}


void DebuggerAgent::CloseSession() {
  ScopedLock with(session_access_);

  // Terminate the session.
  if (session_ != NULL) {
    session_->Shutdown();
    session_->Join();
    delete session_;
    session_ = NULL;
  }
}


void DebuggerAgent::DebuggerMessage(const v8::Debug::Message& message) {
  ScopedLock with(session_access_);

  // Forward the message handling to the session.
  if (session_ != NULL) {
    v8::String::Value val(message.GetJSON());
    session_->DebuggerMessage(Vector<uint16_t>(const_cast<uint16_t*>(*val),
                              val.length()));
  }
}


void DebuggerAgent::OnSessionClosed(DebuggerAgentSession* session) {
  // Don't do anything during termination.
  if (terminate_) {
    return;
  }

  // Terminate the session.
  ScopedLock with(session_access_);
  ASSERT(session == session_);
  if (session == session_) {
    CloseSession();
  }
}


void DebuggerAgentSession::Run() {
  // Send the hello message.
  bool ok = DebuggerAgentUtil::SendConnectMessage(client_, *agent_->name_);
  if (!ok) return;

  while (true) {
    // Read data from the debugger front end.
    SmartArrayPointer<char> message =
        DebuggerAgentUtil::ReceiveMessage(client_);

    const char* msg = *message;
    bool is_closing_session = (msg == NULL);

    if (msg == NULL) {
      // If we lost the connection, then simulate a disconnect msg:
      msg = "{\"seq\":1,\"type\":\"request\",\"command\":\"disconnect\"}";

    } else {
      // Check if we're getting a disconnect request:
      const char* disconnectRequestStr =
          "\"type\":\"request\",\"command\":\"disconnect\"}";
      const char* result = strstr(msg, disconnectRequestStr);
      if (result != NULL) {
        is_closing_session = true;
      }
    }

    // Convert UTF-8 to UTF-16.
    unibrow::Utf8InputBuffer<> buf(msg, StrLength(msg));
    int len = 0;
    while (buf.has_more()) {
      buf.GetNext();
      len++;
    }
    ScopedVector<int16_t> temp(len + 1);
    buf.Reset(msg, StrLength(msg));
    for (int i = 0; i < len; i++) {
      temp[i] = buf.GetNext();
    }

    // Send the request received to the debugger.
    v8::Debug::SendCommand(reinterpret_cast<const uint16_t *>(temp.start()),
                           len,
                           NULL,
                           reinterpret_cast<v8::Isolate*>(agent_->isolate()));

    if (is_closing_session) {
      // Session is closed.
      agent_->OnSessionClosed(this);
      return;
    }
  }
}


void DebuggerAgentSession::DebuggerMessage(Vector<uint16_t> message) {
  DebuggerAgentUtil::SendMessage(client_, message);
}


void DebuggerAgentSession::Shutdown() {
  // Shutdown the socket to end the blocking receive.
  client_->Shutdown();
}


const char* const DebuggerAgentUtil::kContentLength = "Content-Length";


SmartArrayPointer<char> DebuggerAgentUtil::ReceiveMessage(const Socket* conn) {
  int received;

  // Read header.
  int content_length = 0;
  while (true) {
    const int kHeaderBufferSize = 80;
    char header_buffer[kHeaderBufferSize];
    int header_buffer_position = 0;
    char c = '\0';  // One character receive buffer.
    char prev_c = '\0';  // Previous character.

    // Read until CRLF.
    while (!(c == '\n' && prev_c == '\r')) {
      prev_c = c;
      received = conn->Receive(&c, 1);
      if (received <= 0) {
        PrintF("Error %d\n", Socket::LastError());
        return SmartArrayPointer<char>();
      }

      // Add character to header buffer.
      if (header_buffer_position < kHeaderBufferSize) {
        header_buffer[header_buffer_position++] = c;
      }
    }

    // Check for end of header (empty header line).
    if (header_buffer_position == 2) {  // Receive buffer contains CRLF.
      break;
    }

    // Terminate header.
    ASSERT(header_buffer_position > 1);  // At least CRLF is received.
    ASSERT(header_buffer_position <= kHeaderBufferSize);
    header_buffer[header_buffer_position - 2] = '\0';

    // Split header.
    char* key = header_buffer;
    char* value = NULL;
    for (int i = 0; header_buffer[i] != '\0'; i++) {
      if (header_buffer[i] == ':') {
        header_buffer[i] = '\0';
        value = header_buffer + i + 1;
        while (*value == ' ') {
          value++;
        }
        break;
      }
    }

    // Check that key is Content-Length.
    if (strcmp(key, kContentLength) == 0) {
      // Get the content length value if present and within a sensible range.
      if (value == NULL || strlen(value) > 7) {
        return SmartArrayPointer<char>();
      }
      for (int i = 0; value[i] != '\0'; i++) {
        // Bail out if illegal data.
        if (value[i] < '0' || value[i] > '9') {
          return SmartArrayPointer<char>();
        }
        content_length = 10 * content_length + (value[i] - '0');
      }
    } else {
      // For now just print all other headers than Content-Length.
      PrintF("%s: %s\n", key, value != NULL ? value : "(no value)");
    }
  }

  // Return now if no body.
  if (content_length == 0) {
    return SmartArrayPointer<char>();
  }

  // Read body.
  char* buffer = NewArray<char>(content_length + 1);
  received = ReceiveAll(conn, buffer, content_length);
  if (received < content_length) {
    PrintF("Error %d\n", Socket::LastError());
    return SmartArrayPointer<char>();
  }
  buffer[content_length] = '\0';

  return SmartArrayPointer<char>(buffer);
}


bool DebuggerAgentUtil::SendConnectMessage(const Socket* conn,
                                           const char* embedding_host) {
  static const int kBufferSize = 80;
  char buffer[kBufferSize];  // Sending buffer.
  bool ok;
  int len;

  // Send the header.
  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize),
                     "Type: connect\r\n");
  ok = conn->Send(buffer, len);
  if (!ok) return false;

  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize),
                     "V8-Version: %s\r\n", v8::V8::GetVersion());
  ok = conn->Send(buffer, len);
  if (!ok) return false;

  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize),
                     "Protocol-Version: 1\r\n");
  ok = conn->Send(buffer, len);
  if (!ok) return false;

  if (embedding_host != NULL) {
    len = OS::SNPrintF(Vector<char>(buffer, kBufferSize),
                       "Embedding-Host: %s\r\n", embedding_host);
    ok = conn->Send(buffer, len);
    if (!ok) return false;
  }

  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize),
                     "%s: 0\r\n", kContentLength);
  ok = conn->Send(buffer, len);
  if (!ok) return false;

  // Terminate header with empty line.
  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize), "\r\n");
  ok = conn->Send(buffer, len);
  if (!ok) return false;

  // No body for connect message.

  return true;
}


bool DebuggerAgentUtil::SendMessage(const Socket* conn,
                                    const Vector<uint16_t> message) {
  static const int kBufferSize = 80;
  char buffer[kBufferSize];  // Sending buffer both for header and body.

  // Calculate the message size in UTF-8 encoding.
  int utf8_len = 0;
  int previous = unibrow::Utf16::kNoPreviousCharacter;
  for (int i = 0; i < message.length(); i++) {
    uint16_t character = message[i];
    utf8_len += unibrow::Utf8::Length(character, previous);
    previous = character;
  }

  // Send the header.
  int len;
  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize),
                     "%s: %d\r\n", kContentLength, utf8_len);
  conn->Send(buffer, len);

  // Terminate header with empty line.
  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize), "\r\n");
  conn->Send(buffer, len);

  // Send message body as UTF-8.
  int buffer_position = 0;  // Current buffer position.
  previous = unibrow::Utf16::kNoPreviousCharacter;
  for (int i = 0; i < message.length(); i++) {
    // Write next UTF-8 encoded character to buffer.
    uint16_t character = message[i];
    buffer_position +=
        unibrow::Utf8::Encode(buffer + buffer_position, character, previous);
    ASSERT(buffer_position < kBufferSize);

    // Send buffer if full or last character is encoded.
    if (kBufferSize - buffer_position <
          unibrow::Utf16::kMaxExtraUtf8BytesForOneUtf16CodeUnit ||
        i == message.length() - 1) {
      if (unibrow::Utf16::IsLeadSurrogate(character)) {
        const int kEncodedSurrogateLength =
            unibrow::Utf16::kUtf8BytesToCodeASurrogate;
        ASSERT(buffer_position >= kEncodedSurrogateLength);
        conn->Send(buffer, buffer_position - kEncodedSurrogateLength);
        for (int i = 0; i < kEncodedSurrogateLength; i++) {
          buffer[i] = buffer[buffer_position + i];
        }
        buffer_position = kEncodedSurrogateLength;
      } else {
        conn->Send(buffer, buffer_position);
        buffer_position = 0;
      }
    }
    previous = character;
  }

  return true;
}


bool DebuggerAgentUtil::SendMessage(const Socket* conn,
                                    const v8::Handle<v8::String> request) {
  static const int kBufferSize = 80;
  char buffer[kBufferSize];  // Sending buffer both for header and body.

  // Convert the request to UTF-8 encoding.
  v8::String::Utf8Value utf8_request(request);

  // Send the header.
  int len;
  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize),
                     "Content-Length: %d\r\n", utf8_request.length());
  conn->Send(buffer, len);

  // Terminate header with empty line.
  len = OS::SNPrintF(Vector<char>(buffer, kBufferSize), "\r\n");
  conn->Send(buffer, len);

  // Send message body as UTF-8.
  conn->Send(*utf8_request, utf8_request.length());

  return true;
}


// Receive the full buffer before returning unless an error occours.
int DebuggerAgentUtil::ReceiveAll(const Socket* conn, char* data, int len) {
  int total_received = 0;
  while (total_received < len) {
    int received = conn->Receive(data + total_received, len - total_received);
    if (received <= 0) {
      return total_received;
    }
    total_received += received;
  }
  return total_received;
}

} }  // namespace v8::internal

#endif  // ENABLE_DEBUGGER_SUPPORT
