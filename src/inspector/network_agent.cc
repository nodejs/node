#include "network_agent.h"
#include <string>
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "inspector/network_resource_manager.h"
#include "inspector/protocol_helper.h"
#include "network_inspector.h"
#include "node_metadata.h"
#include "util-inl.h"
#include "uv.h"
#include "v8-context.h"
#include "v8.h"

namespace node {
namespace inspector {

using v8::EscapableHandleScope;
using v8::HandleScope;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint8Array;
using v8::Value;

// Get a protocol string property from the object.
Maybe<protocol::String> ObjectGetProtocolString(v8::Local<v8::Context> context,
                                                Local<Object> object,
                                                Local<v8::String> property) {
  HandleScope handle_scope(context->GetIsolate());
  Local<Value> value;
  if (!object->Get(context, property).ToLocal(&value) || !value->IsString()) {
    return Nothing<protocol::String>();
  }
  Local<v8::String> str = value.As<v8::String>();
  return Just(ToProtocolString(context->GetIsolate(), str));
}

// Get a protocol string property from the object.
Maybe<protocol::String> ObjectGetProtocolString(v8::Local<v8::Context> context,
                                                Local<Object> object,
                                                const char* property) {
  HandleScope handle_scope(context->GetIsolate());
  return ObjectGetProtocolString(
      context, object, OneByteString(context->GetIsolate(), property));
}

// Get a protocol double property from the object.
Maybe<double> ObjectGetDouble(v8::Local<v8::Context> context,
                              Local<Object> object,
                              const char* property) {
  HandleScope handle_scope(context->GetIsolate());
  Local<Value> value;
  if (!object->Get(context, OneByteString(context->GetIsolate(), property))
           .ToLocal(&value) ||
      !value->IsNumber()) {
    return Nothing<double>();
  }
  return Just(value.As<v8::Number>()->Value());
}

// Get a protocol int property from the object.
Maybe<int> ObjectGetInt(v8::Local<v8::Context> context,
                        Local<Object> object,
                        const char* property) {
  HandleScope handle_scope(context->GetIsolate());
  Local<Value> value;
  if (!object->Get(context, OneByteString(context->GetIsolate(), property))
           .ToLocal(&value) ||
      !value->IsInt32()) {
    return Nothing<int>();
  }
  return Just(value.As<v8::Int32>()->Value());
}

// Get a protocol bool property from the object.
Maybe<bool> ObjectGetBool(v8::Local<v8::Context> context,
                          Local<Object> object,
                          const char* property) {
  HandleScope handle_scope(context->GetIsolate());
  Local<Value> value;
  if (!object->Get(context, OneByteString(context->GetIsolate(), property))
           .ToLocal(&value) ||
      !value->IsBoolean()) {
    return Nothing<bool>();
  }
  return Just(value.As<v8::Boolean>()->Value());
}

// Get an object property from the object.
MaybeLocal<v8::Object> ObjectGetObject(v8::Local<v8::Context> context,
                                       Local<Object> object,
                                       const char* property) {
  EscapableHandleScope handle_scope(context->GetIsolate());
  Local<Value> value;
  if (!object->Get(context, OneByteString(context->GetIsolate(), property))
           .ToLocal(&value) ||
      !value->IsObject()) {
    return {};
  }
  return handle_scope.Escape(value.As<v8::Object>());
}

// Create a protocol::Network::Headers from the v8 object.
std::unique_ptr<protocol::Network::Headers> createHeadersFromObject(
    v8::Local<v8::Context> context, Local<Object> headers_obj) {
  HandleScope handle_scope(context->GetIsolate());

  std::unique_ptr<protocol::DictionaryValue> dict =
      protocol::DictionaryValue::create();
  Local<v8::Array> property_names;
  if (!headers_obj->GetOwnPropertyNames(context).ToLocal(&property_names)) {
    return {};
  }

  for (size_t idx = 0; idx < property_names->Length(); idx++) {
    Local<v8::Value> property_name_val;
    if (!property_names->Get(context, idx).ToLocal(&property_name_val) ||
        !property_name_val->IsString()) {
      return {};
    }
    Local<v8::String> property_name = property_name_val.As<v8::String>();
    protocol::String property_value;
    if (!ObjectGetProtocolString(context, headers_obj, property_name)
             .To(&property_value)) {
      return {};
    }
    dict->setString(ToProtocolString(context->GetIsolate(), property_name),
                    property_value);
  }

  return std::make_unique<protocol::Network::Headers>(std::move(dict));
}

// Create a protocol::Network::Request from the v8 object.
std::unique_ptr<protocol::Network::Request> createRequestFromObject(
    v8::Local<v8::Context> context, Local<Object> request) {
  HandleScope handle_scope(context->GetIsolate());
  protocol::String url;
  if (!ObjectGetProtocolString(context, request, "url").To(&url)) {
    return {};
  }
  protocol::String method;
  if (!ObjectGetProtocolString(context, request, "method").To(&method)) {
    return {};
  }
  Local<Object> headers_obj;
  if (!ObjectGetObject(context, request, "headers").ToLocal(&headers_obj)) {
    return {};
  }
  std::unique_ptr<protocol::Network::Headers> headers =
      createHeadersFromObject(context, headers_obj);
  if (!headers) {
    return {};
  }
  bool has_post_data =
      ObjectGetBool(context, request, "hasPostData").FromMaybe(false);

  return protocol::Network::Request::create()
      .setUrl(url)
      .setMethod(method)
      .setHasPostData(has_post_data)
      .setHeaders(std::move(headers))
      .build();
}

// Create a protocol::Network::Response from the v8 object.
std::unique_ptr<protocol::Network::Response> createResponseFromObject(
    v8::Local<v8::Context> context, Local<Object> response) {
  HandleScope handle_scope(context->GetIsolate());
  protocol::String url;
  if (!ObjectGetProtocolString(context, response, "url").To(&url)) {
    return {};
  }
  int status;
  if (!ObjectGetInt(context, response, "status").To(&status)) {
    return {};
  }
  protocol::String statusText;
  if (!ObjectGetProtocolString(context, response, "statusText")
           .To(&statusText)) {
    return {};
  }
  Local<Object> headers_obj;
  if (!ObjectGetObject(context, response, "headers").ToLocal(&headers_obj)) {
    return {};
  }
  std::unique_ptr<protocol::Network::Headers> headers =
      createHeadersFromObject(context, headers_obj);
  if (!headers) {
    return {};
  }

  protocol::String mimeType =
      ObjectGetProtocolString(context, response, "mimeType").FromMaybe("");
  protocol::String charset =
      ObjectGetProtocolString(context, response, "charset").FromMaybe("");

  return protocol::Network::Response::create()
      .setUrl(url)
      .setStatus(status)
      .setStatusText(statusText)
      .setHeaders(std::move(headers))
      .setMimeType(mimeType)
      .setCharset(charset)
      .build();
}

NetworkAgent::NetworkAgent(
    NetworkInspector* inspector,
    v8_inspector::V8Inspector* v8_inspector,
    Environment* env,
    std::shared_ptr<NetworkResourceManager> network_resource_manager)
    : inspector_(inspector),
      v8_inspector_(v8_inspector),
      env_(env),
      network_resource_manager_(std::move(network_resource_manager)) {
  event_notifier_map_["requestWillBeSent"] = &NetworkAgent::requestWillBeSent;
  event_notifier_map_["responseReceived"] = &NetworkAgent::responseReceived;
  event_notifier_map_["loadingFailed"] = &NetworkAgent::loadingFailed;
  event_notifier_map_["loadingFinished"] = &NetworkAgent::loadingFinished;
  event_notifier_map_["dataSent"] = &NetworkAgent::dataSent;
  event_notifier_map_["dataReceived"] = &NetworkAgent::dataReceived;
}

void NetworkAgent::emitNotification(v8::Local<v8::Context> context,
                                    const protocol::String& event,
                                    v8::Local<v8::Object> params) {
  if (!inspector_->IsEnabled()) return;
  auto it = event_notifier_map_.find(event);
  if (it != event_notifier_map_.end()) {
    (this->*(it->second))(context, params);
  }
}

void NetworkAgent::Wire(protocol::UberDispatcher* dispatcher) {
  frontend_ =
      std::make_unique<protocol::Network::Frontend>(dispatcher->channel());
  protocol::Network::Dispatcher::wire(dispatcher, this);
}

protocol::DispatchResponse NetworkAgent::enable() {
  inspector_->Enable();
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse NetworkAgent::disable() {
  inspector_->Disable();
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse NetworkAgent::getRequestPostData(
    const protocol::String& in_requestId, protocol::String* out_postData) {
  auto request_entry = requests_.find(in_requestId);
  if (request_entry == requests_.end()) {
    // Request not found, ignore it.
    return protocol::DispatchResponse::InvalidParams("Request not found");
  }

  if (!request_entry->second.is_request_finished) {
    // Request not finished yet.
    return protocol::DispatchResponse::InvalidParams(
        "Request data is not finished yet");
  }
  if (request_entry->second.request_charset == Charset::kBinary) {
    // The protocol does not support binary request bodies yet.
    return protocol::DispatchResponse::ServerError(
        "Unable to serialize binary request body");
  }
  // If the response is UTF-8, we return it as a concatenated string.
  CHECK_EQ(request_entry->second.request_charset, Charset::kUTF8);

  // Concat response bodies.
  protocol::Binary buf =
      protocol::Binary::concat(request_entry->second.request_data_blobs);
  *out_postData = protocol::StringUtil::fromUTF8(buf.data(), buf.size());
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse NetworkAgent::getResponseBody(
    const protocol::String& in_requestId,
    protocol::String* out_body,
    bool* out_base64Encoded) {
  auto request_entry = requests_.find(in_requestId);
  if (request_entry == requests_.end()) {
    // Request not found, ignore it.
    return protocol::DispatchResponse::InvalidParams("Request not found");
  }

  if (request_entry->second.is_streaming) {
    // Streaming request, data is not buffered.
    return protocol::DispatchResponse::InvalidParams(
        "Response body of the request is been streamed");
  }

  if (!request_entry->second.is_response_finished) {
    // Response not finished yet.
    return protocol::DispatchResponse::InvalidParams(
        "Response data is not finished yet");
  }

  // Concat response bodies.
  protocol::Binary buf =
      protocol::Binary::concat(request_entry->second.response_data_blobs);
  if (request_entry->second.response_charset == Charset::kBinary) {
    // If the response is binary, we return base64 encoded data.
    *out_body = buf.toBase64();
    *out_base64Encoded = true;
  } else if (request_entry->second.response_charset == Charset::kUTF8) {
    // If the response is UTF-8, we return it as a concatenated string.
    *out_body = protocol::StringUtil::fromUTF8(buf.data(), buf.size());
    *out_base64Encoded = false;
  } else {
    UNREACHABLE("Response charset not implemented");
  }

  requests_.erase(request_entry);
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse NetworkAgent::streamResourceContent(
    const protocol::String& in_requestId, protocol::Binary* out_bufferedData) {
  auto it = requests_.find(in_requestId);
  if (it == requests_.end()) {
    // Request not found, ignore it.
    return protocol::DispatchResponse::InvalidParams("Request not found");
  }
  auto& request_entry = it->second;

  request_entry.is_streaming = true;

  // Concat response bodies.
  *out_bufferedData =
      protocol::Binary::concat(request_entry.response_data_blobs);
  // Clear buffered data.
  request_entry.response_data_blobs.clear();

  if (request_entry.is_response_finished) {
    // If the request is finished, remove the entry.
    requests_.erase(in_requestId);
  }
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse NetworkAgent::loadNetworkResource(
    const protocol::String& in_url,
    std::unique_ptr<protocol::Network::LoadNetworkResourcePageResult>*
        out_resource) {
  if (!env_->options()->experimental_inspector_network_resource) {
    return protocol::DispatchResponse::ServerError(
        "Network resource loading is not enabled. This feature is "
        "experimental and requires --experimental-inspector-network-resource "
        "flag to be set.");
  }
  CHECK_NOT_NULL(network_resource_manager_);
  std::string data = network_resource_manager_->Get(in_url);
  bool found = !data.empty();
  if (found) {
    uint64_t stream_id = network_resource_manager_->GetStreamId(in_url);
    auto result = protocol::Network::LoadNetworkResourcePageResult::create()
                      .setSuccess(true)
                      .setStream(std::to_string(stream_id))
                      .build();
    *out_resource = std::move(result);
    return protocol::DispatchResponse::Success();
  } else {
    auto result = protocol::Network::LoadNetworkResourcePageResult::create()
                      .setSuccess(false)
                      .build();
    *out_resource = std::move(result);
    return protocol::DispatchResponse::Success();
  }
}

void NetworkAgent::requestWillBeSent(v8::Local<v8::Context> context,
                                     v8::Local<v8::Object> params) {
  protocol::String request_id;
  if (!ObjectGetProtocolString(context, params, "requestId").To(&request_id)) {
    return;
  }
  double timestamp;
  if (!ObjectGetDouble(context, params, "timestamp").To(&timestamp)) {
    return;
  }
  double wall_time;
  if (!ObjectGetDouble(context, params, "wallTime").To(&wall_time)) {
    return;
  }
  protocol::String charset =
      ObjectGetProtocolString(context, params, "charset").FromMaybe("");
  Local<v8::Object> request_obj;
  if (!ObjectGetObject(context, params, "request").ToLocal(&request_obj)) {
    return;
  }
  std::unique_ptr<protocol::Network::Request> request =
      createRequestFromObject(context, request_obj);
  if (!request) {
    return;
  }

  std::unique_ptr<protocol::Network::Initiator> initiator =
      protocol::Network::Initiator::create()
          .setType(protocol::Network::Initiator::TypeEnum::Script)
          .setStack(
              v8_inspector_->captureStackTrace(true)->buildInspectorObject(0))
          .build();

  if (requests_.contains(request_id)) {
    // Duplicate entry, ignore it.
    return;
  }

  auto request_charset = charset == "utf-8" ? Charset::kUTF8 : Charset::kBinary;
  requests_.emplace(
      request_id,
      RequestEntry(timestamp, request_charset, request->getHasPostData()));
  frontend_->requestWillBeSent(request_id,
                               std::move(request),
                               std::move(initiator),
                               timestamp,
                               wall_time);
}

void NetworkAgent::responseReceived(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> params) {
  protocol::String request_id;
  if (!ObjectGetProtocolString(context, params, "requestId").To(&request_id)) {
    return;
  }
  double timestamp;
  if (!ObjectGetDouble(context, params, "timestamp").To(&timestamp)) {
    return;
  }
  protocol::String type;
  if (!ObjectGetProtocolString(context, params, "type").To(&type)) {
    return;
  }
  Local<Object> response_obj;
  if (!ObjectGetObject(context, params, "response").ToLocal(&response_obj)) {
    return;
  }
  auto response = createResponseFromObject(context, response_obj);
  if (!response) {
    return;
  }

  auto request_entry = requests_.find(request_id);
  if (request_entry == requests_.end()) {
    // No entry found. Ignore it.
    return;
  }
  request_entry->second.response_charset =
      response->getCharset() == "utf-8" ? Charset::kUTF8 : Charset::kBinary;
  frontend_->responseReceived(request_id, timestamp, type, std::move(response));
}

void NetworkAgent::loadingFailed(v8::Local<v8::Context> context,
                                 v8::Local<v8::Object> params) {
  protocol::String request_id;
  if (!ObjectGetProtocolString(context, params, "requestId").To(&request_id)) {
    return;
  }
  double timestamp;
  if (!ObjectGetDouble(context, params, "timestamp").To(&timestamp)) {
    return;
  }
  protocol::String type;
  if (!ObjectGetProtocolString(context, params, "type").To(&type)) {
    return;
  }
  protocol::String error_text;
  if (!ObjectGetProtocolString(context, params, "errorText").To(&error_text)) {
    return;
  }

  frontend_->loadingFailed(request_id, timestamp, type, error_text);

  requests_.erase(request_id);
}

void NetworkAgent::loadingFinished(v8::Local<v8::Context> context,
                                   Local<v8::Object> params) {
  protocol::String request_id;
  if (!ObjectGetProtocolString(context, params, "requestId").To(&request_id)) {
    return;
  }
  double timestamp;
  if (!ObjectGetDouble(context, params, "timestamp").To(&timestamp)) {
    return;
  }

  frontend_->loadingFinished(request_id, timestamp);

  auto request_entry = requests_.find(request_id);
  if (request_entry == requests_.end()) {
    // No entry found. Ignore it.
    return;
  }

  if (request_entry->second.is_streaming) {
    // Streaming finished, remove the entry.
    requests_.erase(request_id);
  } else {
    request_entry->second.is_response_finished = true;
  }
}

void NetworkAgent::dataSent(v8::Local<v8::Context> context,
                            v8::Local<v8::Object> params) {
  protocol::String request_id;
  if (!ObjectGetProtocolString(context, params, "requestId").To(&request_id)) {
    return;
  }

  auto request_entry = requests_.find(request_id);
  if (request_entry == requests_.end()) {
    // No entry found. Ignore it.
    return;
  }

  bool is_finished =
      ObjectGetBool(context, params, "finished").FromMaybe(false);
  if (is_finished) {
    request_entry->second.is_request_finished = true;
    return;
  }

  double timestamp;
  if (!ObjectGetDouble(context, params, "timestamp").To(&timestamp)) {
    return;
  }
  int data_length;
  if (!ObjectGetInt(context, params, "dataLength").To(&data_length)) {
    return;
  }
  Local<Object> data_obj;
  if (!ObjectGetObject(context, params, "data").ToLocal(&data_obj)) {
    return;
  }
  if (!data_obj->IsUint8Array()) {
    return;
  }
  Local<Uint8Array> data = data_obj.As<Uint8Array>();
  auto data_bin = protocol::Binary::fromUint8Array(data);
  request_entry->second.request_data_blobs.push_back(data_bin);
}

void NetworkAgent::dataReceived(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> params) {
  protocol::String request_id;
  if (!ObjectGetProtocolString(context, params, "requestId").To(&request_id)) {
    return;
  }
  double timestamp;
  if (!ObjectGetDouble(context, params, "timestamp").To(&timestamp)) {
    return;
  }
  int data_length;
  if (!ObjectGetInt(context, params, "dataLength").To(&data_length)) {
    return;
  }
  int encoded_data_length;
  if (!ObjectGetInt(context, params, "encodedDataLength")
           .To(&encoded_data_length)) {
    return;
  }
  Local<Object> data_obj;
  if (!ObjectGetObject(context, params, "data").ToLocal(&data_obj)) {
    return;
  }
  if (!data_obj->IsUint8Array()) {
    return;
  }
  Local<Uint8Array> data = data_obj.As<Uint8Array>();
  auto data_bin = protocol::Binary::fromUint8Array(data);

  auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    // No entry found. Ignore it.
    return;
  }
  auto& request_entry = it->second;
  if (request_entry.is_streaming) {
    frontend_->dataReceived(
        request_id, timestamp, data_length, encoded_data_length, data_bin);
  } else {
    request_entry.response_data_blobs.push_back(data_bin);
  }
}

}  // namespace inspector
}  // namespace node
