#ifndef SRC_INSPECTOR_NETWORK_AGENT_H_
#define SRC_INSPECTOR_NETWORK_AGENT_H_

#include "env.h"
#include "io_agent.h"
#include "network_requests_buffer.h"
#include "network_resource_manager.h"
#include "node/inspector/protocol/Network.h"

#include <map>
#include <memory>
#include <unordered_map>

namespace node {
namespace inspector {

class NetworkInspector;

class NetworkAgent : public protocol::Network::Backend {
 public:
  explicit NetworkAgent(
      NetworkInspector* inspector,
      v8_inspector::V8Inspector* v8_inspector,
      Environment* env,
      std::shared_ptr<NetworkResourceManager> network_resource_manager);

  void Wire(protocol::UberDispatcher* dispatcher);

  protocol::DispatchResponse enable(
      std::optional<int> in_maxTotalBufferSize,
      std::optional<int> in_maxResourceBufferSize) override;

  protocol::DispatchResponse disable() override;

  protocol::DispatchResponse getRequestPostData(
      const protocol::String& in_requestId,
      protocol::String* out_postData) override;

  protocol::DispatchResponse getResponseBody(
      const protocol::String& in_requestId,
      protocol::String* out_body,
      bool* out_base64Encoded) override;

  protocol::DispatchResponse streamResourceContent(
      const protocol::String& in_requestId,
      protocol::Binary* out_bufferedData) override;

  protocol::DispatchResponse loadNetworkResource(
      const protocol::String& in_url,
      std::unique_ptr<protocol::Network::LoadNetworkResourcePageResult>*
          out_resource) override;

  void emitNotification(v8::Local<v8::Context> context,
                        const protocol::String& event,
                        v8::Local<v8::Object> params);

  void requestWillBeSent(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> params);

  void responseReceived(v8::Local<v8::Context> context,
                        v8::Local<v8::Object> params);

  void loadingFailed(v8::Local<v8::Context> context,
                     v8::Local<v8::Object> params);

  void loadingFinished(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> params);

  void dataSent(v8::Local<v8::Context> context, v8::Local<v8::Object> params);

  void dataReceived(v8::Local<v8::Context> context,
                    v8::Local<v8::Object> params);

  void webSocketCreated(v8::Local<v8::Context> context,
                        v8::Local<v8::Object> params);
  void webSocketClosed(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> params);
  void webSocketHandshakeResponseReceived(v8::Local<v8::Context> context,
                                          v8::Local<v8::Object> params);

 private:
  NetworkInspector* inspector_;
  v8_inspector::V8Inspector* v8_inspector_;
  std::shared_ptr<protocol::Network::Frontend> frontend_;

  using EventNotifier = void (NetworkAgent::*)(v8::Local<v8::Context> context,
                                               v8::Local<v8::Object>);
  std::unordered_map<protocol::String, EventNotifier> event_notifier_map_;
  Environment* env_;
  std::shared_ptr<NetworkResourceManager> network_resource_manager_;

  // Per-resource buffer size in bytes to use when preserving network payloads
  // (XHRs, etc).
  size_t max_resource_buffer_size_ = 5 * 1024 * 1024;  // 5MB
  RequestsBuffer requests_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_AGENT_H_
