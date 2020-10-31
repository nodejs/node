/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/base/build_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_TP_HTTPD)

#include "src/trace_processor/rpc/httpd.h"

#include <map>
#include <string>

#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/rpc/rpc.h"

#include "protos/perfetto/trace_processor/trace_processor.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {

constexpr char kBindAddr[] = "127.0.0.1:9001";

// 32 MiB payload + 128K for HTTP headers.
constexpr size_t kMaxRequestSize = (32 * 1024 + 128) * 1024;

// Owns the socket and data for one HTTP client connection.
struct Client {
  Client(std::unique_ptr<base::UnixSocket> s)
      : sock(std::move(s)),
        rxbuf(base::PagedMemory::Allocate(kMaxRequestSize)) {}
  size_t rxbuf_avail() { return rxbuf.size() - rxbuf_used; }

  std::unique_ptr<base::UnixSocket> sock;
  base::PagedMemory rxbuf;
  size_t rxbuf_used = 0;
};

struct HttpRequest {
  base::StringView method;
  base::StringView uri;
  base::StringView origin;
  base::StringView body;
};

class HttpServer : public base::UnixSocket::EventListener {
 public:
  explicit HttpServer(std::unique_ptr<TraceProcessor>);
  ~HttpServer() override;
  void Run();

 private:
  size_t ParseOneHttpRequest(Client* client);
  void HandleRequest(Client*, const HttpRequest&);

  void OnNewIncomingConnection(base::UnixSocket*,
                               std::unique_ptr<base::UnixSocket>) override;
  void OnConnect(base::UnixSocket* self, bool connected) override;
  void OnDisconnect(base::UnixSocket* self) override;
  void OnDataAvailable(base::UnixSocket* self) override;

  Rpc trace_processor_rpc_;
  base::UnixTaskRunner task_runner_;
  std::unique_ptr<base::UnixSocket> sock_;
  std::vector<Client> clients_;
};

void Append(std::vector<char>& buf, const char* str) {
  buf.insert(buf.end(), str, str + strlen(str));
}

void Append(std::vector<char>& buf, const std::string& str) {
  buf.insert(buf.end(), str.begin(), str.end());
}

void HttpReply(base::UnixSocket* sock,
               const char* http_code,
               std::initializer_list<const char*> headers = {},
               const uint8_t* body = nullptr,
               size_t body_len = 0) {
  std::vector<char> response;
  response.reserve(4096);
  Append(response, "HTTP/1.1 ");
  Append(response, http_code);
  Append(response, "\r\n");
  for (const char* hdr : headers) {
    Append(response, hdr);
    Append(response, "\r\n");
  }
  Append(response, "Content-Length: ");
  Append(response, std::to_string(body_len));
  Append(response, "\r\n\r\n");  // End-of-headers marker.
  sock->Send(response.data(), response.size());
  if (body_len)
    sock->Send(body, body_len);
}

void ShutdownBadRequest(base::UnixSocket* sock, const char* reason) {
  HttpReply(sock, "500 Bad Request", {},
            reinterpret_cast<const uint8_t*>(reason), strlen(reason));
  sock->Shutdown(/*notify=*/true);
}

HttpServer::HttpServer(std::unique_ptr<TraceProcessor> preloaded_instance)
    : trace_processor_rpc_(std::move(preloaded_instance)) {}
HttpServer::~HttpServer() = default;

void HttpServer::Run() {
  PERFETTO_ILOG("[HTTP] Starting RPC server on %s", kBindAddr);
  sock_ = base::UnixSocket::Listen(kBindAddr, this, &task_runner_,
                                   base::SockFamily::kInet,
                                   base::SockType::kStream);
  task_runner_.Run();
}

void HttpServer::OnNewIncomingConnection(
    base::UnixSocket*,
    std::unique_ptr<base::UnixSocket> sock) {
  PERFETTO_DLOG("[HTTP] New connection");
  clients_.emplace_back(std::move(sock));
}

void HttpServer::OnConnect(base::UnixSocket*, bool) {}

void HttpServer::OnDisconnect(base::UnixSocket* sock) {
  PERFETTO_DLOG("[HTTP] Client disconnected");
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->sock.get() == sock) {
      clients_.erase(it);
      return;
    }
  }
  PERFETTO_DFATAL("[HTTP] untracked client in OnDisconnect()");
}

void HttpServer::OnDataAvailable(base::UnixSocket* sock) {
  Client* client = nullptr;
  for (auto it = clients_.begin(); it != clients_.end() && !client; ++it)
    client = (it->sock.get() == sock) ? &*it : nullptr;
  PERFETTO_CHECK(client);

  char* rxbuf = reinterpret_cast<char*>(client->rxbuf.Get());
  for (;;) {
    size_t avail = client->rxbuf_avail();
    PERFETTO_CHECK(avail <= kMaxRequestSize);
    if (avail == 0)
      return ShutdownBadRequest(sock, "Request body too big");
    size_t rsize = sock->Receive(&rxbuf[client->rxbuf_used], avail);
    client->rxbuf_used += rsize;
    if (rsize == 0 || client->rxbuf_avail() == 0)
      break;
  }

  // At this point |rxbuf| can contain a partial HTTP request, a full one or
  // more (in case of HTTP Keepalive pipelining).
  for (;;) {
    size_t bytes_consumed = ParseOneHttpRequest(client);
    if (bytes_consumed == 0)
      break;
    memmove(rxbuf, &rxbuf[bytes_consumed], client->rxbuf_used - bytes_consumed);
    client->rxbuf_used -= bytes_consumed;
  }
}

// Parses the HTTP request and invokes HandleRequest(). It returns the size of
// the HTTP header + body that has been processed or 0 if there isn't enough
// data for a full HTTP request in the buffer.
size_t HttpServer::ParseOneHttpRequest(Client* client) {
  auto* rxbuf = reinterpret_cast<char*>(client->rxbuf.Get());
  base::StringView buf_view(rxbuf, client->rxbuf_used);
  size_t pos = 0;
  size_t body_offset = 0;
  size_t body_size = 0;
  bool has_parsed_first_line = false;
  HttpRequest http_req;

  // This loop parses the HTTP request headers and sets the |body_offset|.
  for (;;) {
    size_t next = buf_view.find("\r\n", pos);
    size_t col;
    if (next == std::string::npos)
      break;

    if (!has_parsed_first_line) {
      // Parse the "GET /xxx HTTP/1.1" line.
      has_parsed_first_line = true;
      size_t space = buf_view.find(' ');
      if (space == std::string::npos || space + 2 >= client->rxbuf_used) {
        ShutdownBadRequest(client->sock.get(), "Malformed HTTP request");
        return 0;
      }
      http_req.method = buf_view.substr(0, space);
      size_t uri_size = buf_view.find(' ', space + 1) - space - 1;
      http_req.uri = buf_view.substr(space + 1, uri_size);
    } else if (next == pos) {
      // The CR-LF marker that separates headers from body.
      body_offset = next + 2;
      break;
    } else if ((col = buf_view.find(':', pos)) < next) {
      // Parse HTTP headers. They look like: "Content-Length: 1234".
      auto hdr_name = buf_view.substr(pos, col - pos);
      auto hdr_value = buf_view.substr(col + 2, next - col - 2);
      if (hdr_name.CaseInsensitiveEq("content-length")) {
        body_size = static_cast<size_t>(atoi(hdr_value.ToStdString().c_str()));
      } else if (hdr_name.CaseInsensitiveEq("origin")) {
        http_req.origin = hdr_value;
      }
    }
    pos = next + 2;
  }

  // If we have a full header but not yet the full body, return and try again
  // next time we receive some more data.
  size_t http_req_size = body_offset + body_size;
  if (!body_offset || client->rxbuf_used < http_req_size)
    return 0;

  http_req.body = base::StringView(&rxbuf[body_offset], body_size);
  HandleRequest(client, http_req);
  return http_req_size;
}

void HttpServer::HandleRequest(Client* client, const HttpRequest& req) {
  PERFETTO_LOG("[HTTP] %s %s (body: %zu bytes)",
               req.method.ToStdString().c_str(), req.uri.ToStdString().c_str(),
               req.body.size());
  std::string allow_origin_hdr =
      "Access-Control-Allow-Origin: " + req.origin.ToStdString();
  std::initializer_list<const char*> headers = {
      "Connection: Keep-Alive",                //
      "Access-Control-Expose-Headers: *",      //
      "Keep-Alive: timeout=5, max=1000",       //
      "Content-Type: application/x-protobuf",  //
      allow_origin_hdr.c_str()};

  if (req.method == "OPTIONS") {
    // CORS headers.
    return HttpReply(client->sock.get(), "204 No Content",
                     {
                         "Access-Control-Allow-Methods: POST, GET, OPTIONS",
                         "Access-Control-Allow-Headers: *",
                         "Access-Control-Max-Age: 600",
                         allow_origin_hdr.c_str(),
                     });
  }

  if (req.uri == "/parse") {
    trace_processor_rpc_.Parse(
        reinterpret_cast<const uint8_t*>(req.body.data()), req.body.size());
    return HttpReply(client->sock.get(), "200 OK", headers);
  }

  if (req.uri == "/notify_eof") {
    trace_processor_rpc_.NotifyEndOfFile();
    return HttpReply(client->sock.get(), "200 OK", headers);
  }

  if (req.uri == "/restore_initial_tables") {
    trace_processor_rpc_.RestoreInitialTables();
    return HttpReply(client->sock.get(), "200 OK", headers);
  }

  if (req.uri == "/raw_query") {
    PERFETTO_CHECK(req.body.size() > 0u);
    std::vector<uint8_t> response = trace_processor_rpc_.RawQuery(
        reinterpret_cast<const uint8_t*>(req.body.data()), req.body.size());
    return HttpReply(client->sock.get(), "200 OK", headers, response.data(),
                     response.size());
  }

  if (req.uri == "/status") {
    protozero::HeapBuffered<protos::pbzero::StatusResult> res;
    res->set_loaded_trace_name(
        trace_processor_rpc_.GetCurrentTraceName().c_str());
    std::vector<uint8_t> buf = res.SerializeAsArray();
    return HttpReply(client->sock.get(), "200 OK", headers, buf.data(),
                     buf.size());
  }

  if (req.uri == "/compute_metric") {
    std::vector<uint8_t> res = trace_processor_rpc_.ComputeMetric(
        reinterpret_cast<const uint8_t*>(req.body.data()), req.body.size());
    return HttpReply(client->sock.get(), "200 OK", headers, res.data(),
                     res.size());
  }

  return HttpReply(client->sock.get(), "404 Not Found", headers);
}

}  // namespace

void RunHttpRPCServer(std::unique_ptr<TraceProcessor> preloaded_instance) {
  HttpServer srv(std::move(preloaded_instance));
  srv.Run();
}

}  // namespace trace_processor
}  // namespace perfetto

#endif  // PERFETTO_TP_HTTPD
