/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/ipc/client_impl.h"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <utility>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/ipc/service_descriptor.h"
#include "perfetto/ext/ipc/service_proxy.h"

#include "protos/perfetto/ipc/wire_protocol.gen.h"

// TODO(primiano): Add ThreadChecker everywhere.

// TODO(primiano): Add timeouts.

namespace perfetto {
namespace ipc {

// static
std::unique_ptr<Client> Client::CreateInstance(const char* socket_name,
                                               base::TaskRunner* task_runner) {
  std::unique_ptr<Client> client(new ClientImpl(socket_name, task_runner));
  return client;
}

ClientImpl::ClientImpl(const char* socket_name, base::TaskRunner* task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  sock_ = base::UnixSocket::Connect(socket_name, this, task_runner,
                                    base::SockFamily::kUnix,
                                    base::SockType::kStream);
}

ClientImpl::~ClientImpl() {
  // Ensure we are not destroyed in the middle of invoking a reply.
  PERFETTO_DCHECK(!invoking_method_reply_);
  OnDisconnect(
      nullptr);  // The base::UnixSocket* ptr is not used in OnDisconnect().
}

void ClientImpl::BindService(base::WeakPtr<ServiceProxy> service_proxy) {
  if (!service_proxy)
    return;
  if (!sock_->is_connected()) {
    queued_bindings_.emplace_back(service_proxy);
    return;
  }
  RequestID request_id = ++last_request_id_;
  Frame frame;
  frame.set_request_id(request_id);
  Frame::BindService* req = frame.mutable_msg_bind_service();
  const char* const service_name = service_proxy->GetDescriptor().service_name;
  req->set_service_name(service_name);
  if (!SendFrame(frame)) {
    PERFETTO_DLOG("BindService(%s) failed", service_name);
    return service_proxy->OnConnect(false /* success */);
  }
  QueuedRequest qr;
  qr.type = Frame::kMsgBindServiceFieldNumber;
  qr.request_id = request_id;
  qr.service_proxy = service_proxy;
  queued_requests_.emplace(request_id, std::move(qr));
}

void ClientImpl::UnbindService(ServiceID service_id) {
  service_bindings_.erase(service_id);
}

RequestID ClientImpl::BeginInvoke(ServiceID service_id,
                                  const std::string& method_name,
                                  MethodID remote_method_id,
                                  const ProtoMessage& method_args,
                                  bool drop_reply,
                                  base::WeakPtr<ServiceProxy> service_proxy,
                                  int fd) {
  RequestID request_id = ++last_request_id_;
  Frame frame;
  frame.set_request_id(request_id);
  Frame::InvokeMethod* req = frame.mutable_msg_invoke_method();
  req->set_service_id(service_id);
  req->set_method_id(remote_method_id);
  req->set_drop_reply(drop_reply);
  req->set_args_proto(method_args.SerializeAsString());
  if (!SendFrame(frame, fd)) {
    PERFETTO_DLOG("BeginInvoke() failed while sending the frame");
    return 0;
  }
  if (drop_reply)
    return 0;
  QueuedRequest qr;
  qr.type = Frame::kMsgInvokeMethodFieldNumber;
  qr.request_id = request_id;
  qr.method_name = method_name;
  qr.service_proxy = std::move(service_proxy);
  queued_requests_.emplace(request_id, std::move(qr));
  return request_id;
}

bool ClientImpl::SendFrame(const Frame& frame, int fd) {
  // Serialize the frame into protobuf, add the size header, and send it.
  std::string buf = BufferedFrameDeserializer::Serialize(frame);

  // TODO(primiano): this should do non-blocking I/O. But then what if the
  // socket buffer is full? We might want to either drop the request or throttle
  // the send and PostTask the reply later? Right now we are making Send()
  // blocking as a workaround. Propagate bakpressure to the caller instead.
  bool res = sock_->Send(buf.data(), buf.size(), fd);
  PERFETTO_CHECK(res || !sock_->is_connected());
  return res;
}

void ClientImpl::OnConnect(base::UnixSocket*, bool connected) {
  // Drain the BindService() calls that were queued before establishig the
  // connection with the host.
  for (base::WeakPtr<ServiceProxy>& service_proxy : queued_bindings_) {
    if (connected) {
      BindService(service_proxy);
    } else if (service_proxy) {
      service_proxy->OnConnect(false /* success */);
    }
  }
  queued_bindings_.clear();
}

void ClientImpl::OnDisconnect(base::UnixSocket*) {
  for (auto it : service_bindings_) {
    base::WeakPtr<ServiceProxy>& service_proxy = it.second;
    task_runner_->PostTask([service_proxy] {
      if (service_proxy)
        service_proxy->OnDisconnect();
    });
  }
  service_bindings_.clear();
  queued_bindings_.clear();
}

void ClientImpl::OnDataAvailable(base::UnixSocket*) {
  size_t rsize;
  do {
    auto buf = frame_deserializer_.BeginReceive();
    base::ScopedFile fd;
    rsize = sock_->Receive(buf.data, buf.size, &fd);
    if (fd) {
      PERFETTO_DCHECK(!received_fd_);
      int res = fcntl(*fd, F_SETFD, FD_CLOEXEC);
      PERFETTO_DCHECK(res == 0);
      received_fd_ = std::move(fd);
    }
    if (!frame_deserializer_.EndReceive(rsize)) {
      // The endpoint tried to send a frame that is way too large.
      return sock_->Shutdown(true);  // In turn will trigger an OnDisconnect().
      // TODO(fmayer): check this.
    }
  } while (rsize > 0);

  while (std::unique_ptr<Frame> frame = frame_deserializer_.PopNextFrame())
    OnFrameReceived(*frame);
}

void ClientImpl::OnFrameReceived(const Frame& frame) {
  auto queued_requests_it = queued_requests_.find(frame.request_id());
  if (queued_requests_it == queued_requests_.end()) {
    PERFETTO_DLOG("OnFrameReceived(): got invalid request_id=%" PRIu64,
                  static_cast<uint64_t>(frame.request_id()));
    return;
  }
  QueuedRequest req = std::move(queued_requests_it->second);
  queued_requests_.erase(queued_requests_it);

  if (req.type == Frame::kMsgBindServiceFieldNumber &&
      frame.has_msg_bind_service_reply()) {
    return OnBindServiceReply(std::move(req), frame.msg_bind_service_reply());
  }
  if (req.type == Frame::kMsgInvokeMethodFieldNumber &&
      frame.has_msg_invoke_method_reply()) {
    return OnInvokeMethodReply(std::move(req), frame.msg_invoke_method_reply());
  }
  if (frame.has_msg_request_error()) {
    PERFETTO_DLOG("Host error: %s", frame.msg_request_error().error().c_str());
    return;
  }

  PERFETTO_DLOG(
      "OnFrameReceived() request type=%d, received unknown frame in reply to "
      "request_id=%" PRIu64,
      req.type, static_cast<uint64_t>(frame.request_id()));
}

void ClientImpl::OnBindServiceReply(QueuedRequest req,
                                    const Frame::BindServiceReply& reply) {
  base::WeakPtr<ServiceProxy>& service_proxy = req.service_proxy;
  if (!service_proxy)
    return;
  const char* svc_name = service_proxy->GetDescriptor().service_name;
  if (!reply.success()) {
    PERFETTO_DLOG("BindService(): unknown service_name=\"%s\"", svc_name);
    return service_proxy->OnConnect(false /* success */);
  }

  auto prev_service = service_bindings_.find(reply.service_id());
  if (prev_service != service_bindings_.end() && prev_service->second.get()) {
    PERFETTO_DLOG(
        "BindService(): Trying to bind service \"%s\" but another service "
        "named \"%s\" is already bound with the same ID.",
        svc_name, prev_service->second->GetDescriptor().service_name);
    return service_proxy->OnConnect(false /* success */);
  }

  // Build the method [name] -> [remote_id] map.
  std::map<std::string, MethodID> methods;
  for (const auto& method : reply.methods()) {
    if (method.name().empty() || method.id() <= 0) {
      PERFETTO_DLOG("OnBindServiceReply(): invalid method \"%s\" -> %" PRIu64,
                    method.name().c_str(), static_cast<uint64_t>(method.id()));
      continue;
    }
    methods[method.name()] = method.id();
  }
  service_proxy->InitializeBinding(weak_ptr_factory_.GetWeakPtr(),
                                   reply.service_id(), std::move(methods));
  service_bindings_[reply.service_id()] = service_proxy;
  service_proxy->OnConnect(true /* success */);
}

void ClientImpl::OnInvokeMethodReply(QueuedRequest req,
                                     const Frame::InvokeMethodReply& reply) {
  base::WeakPtr<ServiceProxy> service_proxy = req.service_proxy;
  if (!service_proxy)
    return;
  std::unique_ptr<ProtoMessage> decoded_reply;
  if (reply.success()) {
    // If this becomes a hotspot, optimize by maintaining a dedicated hashtable.
    for (const auto& method : service_proxy->GetDescriptor().methods) {
      if (req.method_name == method.name) {
        decoded_reply = method.reply_proto_decoder(reply.reply_proto());
        break;
      }
    }
  }
  const RequestID request_id = req.request_id;
  invoking_method_reply_ = true;
  service_proxy->EndInvoke(request_id, std::move(decoded_reply),
                           reply.has_more());
  invoking_method_reply_ = false;

  // If this is a streaming method and future replies will be resolved, put back
  // the |req| with the callback into the set of active requests.
  if (reply.has_more())
    queued_requests_.emplace(request_id, std::move(req));
}

ClientImpl::QueuedRequest::QueuedRequest() = default;

base::ScopedFile ClientImpl::TakeReceivedFD() {
  return std::move(received_fd_);
}

}  // namespace ipc
}  // namespace perfetto
