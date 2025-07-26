#ifndef SRC_INSPECTOR_NETWORK_AGENT_H_
#define SRC_INSPECTOR_NETWORK_AGENT_H_

#include "env.h"
#include "io_agent.h"
#include "network_resource_manager.h"
#include "node/inspector/protocol/Network.h"

#include <map>
#include <memory>
#include <unordered_map>

namespace node {
namespace inspector {

class NetworkInspector;

// Supported charsets for devtools frontend on request/response data.
// If the charset is kUTF8, the data is expected to be text and can be
// formatted on the frontend.
enum class Charset {
  kUTF8,
  kBinary,
};

struct RequestEntry {
  double timestamp;
  bool is_request_finished = false;
  bool is_response_finished = false;
  bool is_streaming = false;
  Charset request_charset;
  std::vector<protocol::Binary> request_data_blobs;
  Charset response_charset;
  std::vector<protocol::Binary> response_data_blobs;

  RequestEntry(double timestamp, Charset request_charset, bool has_request_body)
      : timestamp(timestamp),
        is_request_finished(!has_request_body),
        request_charset(request_charset),
        response_charset(Charset::kBinary) {}
};

class NetworkAgent : public protocol::Network::Backend {
 public:
  explicit NetworkAgent(
      NetworkInspector* inspector,
      v8_inspector::V8Inspector* v8_inspector,
      Environment* env,
      std::shared_ptr<NetworkResourceManager> network_resource_manager);

  void Wire(protocol::UberDispatcher* dispatcher);

  protocol::DispatchResponse enable() override;

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

 private:
  NetworkInspector* inspector_;
  v8_inspector::V8Inspector* v8_inspector_;
  std::shared_ptr<protocol::Network::Frontend> frontend_;
  using EventNotifier = void (NetworkAgent::*)(v8::Local<v8::Context> context,
                                               v8::Local<v8::Object>);
  std::unordered_map<protocol::String, EventNotifier> event_notifier_map_;
  std::map<protocol::String, RequestEntry> requests_;
  Environment* env_;
  std::shared_ptr<NetworkResourceManager> network_resource_manager_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_AGENT_H_
