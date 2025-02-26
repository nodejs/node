#ifndef SRC_INSPECTOR_NETWORK_AGENT_H_
#define SRC_INSPECTOR_NETWORK_AGENT_H_

#include "node/inspector/protocol/Network.h"

#include <map>
#include <unordered_map>

namespace node {
namespace inspector {

class NetworkInspector;

struct RequestEntry {
  double timestamp;
  bool is_finished;
  bool is_streaming;
  std::vector<protocol::Binary> response_data_blobs;
};

class NetworkAgent : public protocol::Network::Backend {
 public:
  explicit NetworkAgent(NetworkInspector* inspector,
                        v8_inspector::V8Inspector* v8_inspector);

  void Wire(protocol::UberDispatcher* dispatcher);

  protocol::DispatchResponse enable() override;

  protocol::DispatchResponse disable() override;

  protocol::DispatchResponse streamResourceContent(
      const protocol::String& in_requestId,
      protocol::Binary* out_bufferedData) override;

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
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_AGENT_H_
