// Copyright 2009 the V8 project authors. All rights reserved.

#include "v8.h"
#include "platform.h"
#include "cctest.h"


using namespace ::v8::internal;


class SocketListenerThread : public Thread {
 public:
  explicit SocketListenerThread(int port, int data_size)
      : port_(port), data_size_(data_size), server_(NULL), client_(NULL),
        listening_(OS::CreateSemaphore(0)) {
    data_ = new char[data_size_];
  }
  ~SocketListenerThread() {
    // Close both sockets.
    delete client_;
    delete server_;
    delete listening_;
    delete[] data_;
  }

  void Run();
  void WaitForListening() { listening_->Wait(); }
  char* data() { return data_; }

 private:
  int port_;
  char* data_;
  int data_size_;
  Socket* server_;  // Server socket used for bind/accept.
  Socket* client_;  // Single client connection used by the test.
  Semaphore* listening_;  // Signalled when the server socket is in listen mode.
};


void SocketListenerThread::Run() {
  bool ok;

  // Create the server socket and bind it to the requested port.
  server_ = OS::CreateSocket();
  server_->SetReuseAddress(true);
  CHECK(server_ != NULL);
  ok = server_->Bind(port_);
  CHECK(ok);

  // Listen for new connections.
  ok = server_->Listen(1);
  CHECK(ok);
  listening_->Signal();

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
  Socket* client = OS::CreateSocket();
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
  static const int kPort = 5859;

  bool ok;

  // Initialize socket support.
  ok = Socket::Setup();
  CHECK(ok);

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


TEST(HToNNToH) {
  uint16_t x = 1234;
  CHECK_EQ(x, Socket::NToH(Socket::HToN(x)));

  uint32_t y = 12345678;
  CHECK(y == Socket::NToH(Socket::HToN(y)));
}
