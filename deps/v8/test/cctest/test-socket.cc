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
#include "platform.h"
#include "platform/socket.h"
#include "cctest.h"


using namespace ::v8::internal;


class SocketListenerThread : public Thread {
 public:
  SocketListenerThread(int port, int data_size)
      : Thread("SocketListenerThread"),
        port_(port),
        data_size_(data_size),
        server_(NULL),
        client_(NULL),
        listening_(0) {
    data_ = new char[data_size_];
  }
  ~SocketListenerThread() {
    // Close both sockets.
    delete client_;
    delete server_;
    delete[] data_;
  }

  void Run();
  void WaitForListening() { listening_.Wait(); }
  char* data() { return data_; }

 private:
  int port_;
  char* data_;
  int data_size_;
  Socket* server_;  // Server socket used for bind/accept.
  Socket* client_;  // Single client connection used by the test.
  Semaphore listening_;  // Signalled when the server socket is in listen mode.
};


void SocketListenerThread::Run() {
  bool ok;

  // Create the server socket and bind it to the requested port.
  server_ = new Socket;
  server_->SetReuseAddress(true);
  CHECK(server_ != NULL);
  ok = server_->Bind(port_);
  CHECK(ok);

  // Listen for new connections.
  ok = server_->Listen(1);
  CHECK(ok);
  listening_.Signal();

  // Accept a connection.
  client_ = server_->Accept();
  CHECK(client_ != NULL);

  // Read the expected niumber of bytes of data.
  int bytes_read = 0;
  while (bytes_read < data_size_) {
    bytes_read += client_->Receive(data_ + bytes_read, data_size_ - bytes_read);
  }
}


static bool SendAll(Socket* socket, const char* data, int len) {
  int sent_len = 0;
  while (sent_len < len) {
    int status = socket->Send(data, len);
    if (status <= 0) {
      return false;
    }
    sent_len += status;
  }
  return true;
}


static void SendAndReceive(int port, char *data, int len) {
  static const char* kLocalhost = "localhost";

  bool ok;

  // Make a string with the port number.
  const int kPortBuferLen = 6;
  char port_str[kPortBuferLen];
  OS::SNPrintF(Vector<char>(port_str, kPortBuferLen), "%d", port);

  // Create a socket listener.
  SocketListenerThread* listener = new SocketListenerThread(port, len);
  listener->Start();
  listener->WaitForListening();

  // Connect and write some data.
  Socket* client = new Socket;
  CHECK(client != NULL);
  ok = client->Connect(kLocalhost, port_str);
  CHECK(ok);

  // Send all the data.
  ok = SendAll(client, data, len);
  CHECK(ok);

  // Wait until data is received.
  listener->Join();

  // Check that data received is the same as data send.
  for (int i = 0; i < len; i++) {
    CHECK(data[i] == listener->data()[i]);
  }

  // Close the client before the listener to avoid TIME_WAIT issues.
  client->Shutdown();
  delete client;
  delete listener;
}


TEST(Socket) {
  // Make sure this port is not used by other tests to allow tests to run in
  // parallel.
  static const int kPort = 5859 + FlagDependentPortOffset();

  // Send and receive some data.
  static const int kBufferSizeSmall = 20;
  char small_data[kBufferSizeSmall + 1] = "1234567890abcdefghij";
  SendAndReceive(kPort, small_data, kBufferSizeSmall);

  // Send and receive some more data.
  static const int kBufferSizeMedium = 10000;
  char* medium_data = new char[kBufferSizeMedium];
  for (int i = 0; i < kBufferSizeMedium; i++) {
    medium_data[i] = i % 256;
  }
  SendAndReceive(kPort, medium_data, kBufferSizeMedium);
  delete[] medium_data;

  // Send and receive even more data.
  static const int kBufferSizeLarge = 1000000;
  char* large_data = new char[kBufferSizeLarge];
  for (int i = 0; i < kBufferSizeLarge; i++) {
    large_data[i] = i % 256;
  }
  SendAndReceive(kPort, large_data, kBufferSizeLarge);
  delete[] large_data;
}
