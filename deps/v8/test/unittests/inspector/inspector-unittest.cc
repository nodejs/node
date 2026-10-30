// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <span>
#include <string>

#include "include/v8-external.h"
#include "include/v8-function.h"
#include "include/v8-inspector.h"
#include "include/v8-json.h"
#include "include/v8-local-handle.h"
#include "include/v8-microtask-queue.h"
#include "include/v8-primitive.h"
#include "src/heap/heap.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "test/common/flag-utils.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using v8_inspector::String16;
using v8_inspector::StringBuffer;
using v8_inspector::StringView;
using v8_inspector::toString16;
using v8_inspector::toStringView;
using v8_inspector::V8ContextInfo;
using v8_inspector::V8Inspector;
using v8_inspector::V8InspectorSession;

namespace v8 {
namespace internal {

using InspectorTest = TestWithContext;

namespace {

class NoopChannel : public V8Inspector::Channel {
 public:
  ~NoopChannel() override = default;
  void sendResponse(int callId,
                    std::unique_ptr<StringBuffer> message) override {}
  void sendNotification(std::unique_ptr<StringBuffer> message) override {}
  void flushProtocolNotifications() override {}
};

void WrapOnInterrupt(v8::Isolate* isolate, void* data) {
  const char* object_group = "";
  StringView object_group_view(reinterpret_cast<const uint8_t*>(object_group),
                               strlen(object_group));
  reinterpret_cast<V8InspectorSession*>(data)->wrapObject(
      isolate->GetCurrentContext(), v8::Null(isolate), object_group_view,
      false);
}

}  // namespace

TEST_F(InspectorTest, WrapInsideWrapOnInterrupt) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  const char* name = "";
  StringView name_view(reinterpret_cast<const uint8_t*>(name), strlen(name));
  V8ContextInfo context_info(v8_context(), 1, name_view);
  inspector->contextCreated(context_info);

  NoopChannel channel;
  const char* state = "{}";
  StringView state_view(reinterpret_cast<const uint8_t*>(state), strlen(state));
  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, state_view, v8_inspector::V8Inspector::kFullyTrusted);

  const char* object_group = "";
  StringView object_group_view(reinterpret_cast<const uint8_t*>(object_group),
                               strlen(object_group));
  isolate->RequestInterrupt(&WrapOnInterrupt, session.get());
  session->wrapObject(v8_context(), v8::Null(isolate), object_group_view,
                      false);
}

using InspectorShadowRealmTest = TestShadowRealmWithContext;

namespace {

v8::MaybeLocal<v8::Context> CreateShadowRealmContext(
    v8::Local<v8::Context> initiator_context) {
  return v8::Context::New(v8::Isolate::GetCurrent());
}

// Collects every protocol response so the test can inspect the JSON.
class CollectingChannel : public V8Inspector::Channel {
 public:
  ~CollectingChannel() override = default;
  void sendResponse(int callId,
                    std::unique_ptr<StringBuffer> message) override {
    m_responses.push_back(toString16(message->string()));
  }
  void sendNotification(std::unique_ptr<StringBuffer> message) override {}
  void flushProtocolNotifications() override {}

  const String16& lastResponse() const { return m_responses.back(); }

 private:
  std::vector<String16> m_responses;
};

}  // namespace

// Runtime.getProperties must not invoke a getter that is a ShadowRealm wrapped
// function: such a getter runs user JavaScript in the shadow realm.
TEST_F(InspectorShadowRealmTest,
       GetPropertiesDoesNotCallWrappedFunctionGetter) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);
  isolate->SetHostCreateShadowRealmContextCallback(CreateShadowRealmContext);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, StringView());
  inspector->contextCreated(context_info);

  CollectingChannel channel;
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);

  // The holder is itself callable so that the wrapped getter's receiver can
  // be wrapped for the shadow realm (GetWrappedValue requires a callable or a
  // primitive).
  RunJS(
      "globalThis.realm = new ShadowRealm();"
      "realm.evaluate('globalThis.calls = 0');"
      "globalThis.obj = function holder() {};"
      "Object.defineProperty(obj, 'x', {"
      "  configurable: true, enumerable: true,"
      "  get: realm.evaluate('() => { globalThis.calls++; return 42; }')"
      "});");

  session->dispatchProtocolMessage(
      toStringView("{\"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": "
                   "{\"expression\": \"obj\", \"contextId\": 1}}"));
  std::string response = channel.lastResponse().utf8();
  const char kObjectIdKey[] = "\"objectId\":\"";
  size_t object_id_start = response.find(kObjectIdKey);
  CHECK_NE(object_id_start, std::string::npos);
  object_id_start += strlen(kObjectIdKey);
  size_t object_id_end = response.find("\"", object_id_start);
  CHECK_NE(object_id_end, std::string::npos);
  std::string object_id =
      response.substr(object_id_start, object_id_end - object_id_start);

  std::string command =
      "{\"id\": 2, \"method\": \"Runtime.getProperties\", \"params\": "
      "{\"ownProperties\": true, \"objectId\": \"" +
      object_id + "\"}}";
  session->dispatchProtocolMessage(toStringView(String16(command.c_str())));

  // The property must be reported as an accessor, not as an eagerly evaluated
  // value: the inspector must not run the wrapped getter.
  CHECK_EQ(channel.lastResponse().utf8().find(
               "\"name\":\"x\",\"value\":{\"type\":\"number\",\"value\":42"),
           std::string::npos);

  // And the wrapped function must not have run.
  v8::Local<v8::Value> calls = RunJS("realm.evaluate('String(calls)')");
  v8::String::Utf8Value calls_utf8(isolate, calls);
  CHECK_EQ(std::string(*calls_utf8), std::string("0"));
}

// Runtime.getProperties still eagerly evaluates builtin getters (e.g.
// Map.prototype.size).
TEST_F(InspectorTest, GetPropertiesEagerlyEvaluatesBuiltinGetter) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, StringView());
  inspector->contextCreated(context_info);

  CollectingChannel channel;
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);

  RunJS("globalThis.map = new Map([['a', 1], ['b', 2]]);");

  session->dispatchProtocolMessage(
      toStringView("{\"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": "
                   "{\"expression\": \"map\", \"contextId\": 1}}"));
  std::string response = channel.lastResponse().utf8();
  const char kObjectIdKey[] = "\"objectId\":\"";
  size_t object_id_start = response.find(kObjectIdKey);
  CHECK_NE(object_id_start, std::string::npos);
  object_id_start += strlen(kObjectIdKey);
  size_t object_id_end = response.find("\"", object_id_start);
  CHECK_NE(object_id_end, std::string::npos);
  std::string object_id =
      response.substr(object_id_start, object_id_end - object_id_start);

  std::string command =
      "{\"id\": 2, \"method\": \"Runtime.getProperties\", \"params\": "
      "{\"ownProperties\": false, \"accessorPropertiesOnly\": false, "
      "\"objectId\": \"" +
      object_id + "\"}}";
  session->dispatchProtocolMessage(toStringView(String16(command.c_str())));

  // Map.prototype.size is a builtin getter, so it must be eagerly evaluated to
  // value: 2.
  CHECK_NE(channel.lastResponse().utf8().find(
               "\"name\":\"size\",\"value\":{\"type\":\"number\",\"value\":2"),
           std::string::npos);
}

TEST_F(InspectorTest, BinaryFromBase64) {
  auto checkBinary = [](const v8_inspector::protocol::Binary& binary,
                        const std::vector<uint8_t>& values) {
    std::vector<uint8_t> binary_vector(binary.data(),
                                       binary.data() + binary.size());
    CHECK_EQ(binary_vector, values);
  };

  {
    bool success;
    auto binary = v8_inspector::protocol::Binary::fromBase64("", &success);
    CHECK(success);
    checkBinary(binary, {});
  }
  {
    bool success;
    auto binary = v8_inspector::protocol::Binary::fromBase64("YQ==", &success);
    CHECK(success);
    checkBinary(binary, {'a'});
  }
  {
    bool success;
    auto binary = v8_inspector::protocol::Binary::fromBase64("YWI=", &success);
    CHECK(success);
    checkBinary(binary, {'a', 'b'});
  }
  {
    bool success;
    auto binary = v8_inspector::protocol::Binary::fromBase64("YWJj", &success);
    CHECK(success);
    checkBinary(binary, {'a', 'b', 'c'});
  }
  {
    bool success;
    // Wrong input length:
    auto binary = v8_inspector::protocol::Binary::fromBase64("Y", &success);
    CHECK(!success);
  }
  {
    bool success;
    // Invalid space:
    auto binary = v8_inspector::protocol::Binary::fromBase64("=AAA", &success);
    CHECK(!success);
  }
  {
    bool success;
    // Invalid space in a non-final block of four:
    auto binary =
        v8_inspector::protocol::Binary::fromBase64("AAA=AAAA", &success);
    CHECK(!success);
  }
  {
    bool success;
    // Invalid invalid space in second to last position:
    auto binary = v8_inspector::protocol::Binary::fromBase64("AA=A", &success);
    CHECK(!success);
  }
  {
    bool success;
    // Invalid character:
    auto binary = v8_inspector::protocol::Binary::fromBase64(" ", &success);
    CHECK(!success);
  }
}

namespace {

class CustomPreviewChannel : public v8_inspector::V8Inspector::Channel {
 public:
  ~CustomPreviewChannel() override = default;
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    responses_[callId] = v8_inspector::toString16(message->string());
  }
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {}
  void flushProtocolNotifications() override {}

  std::unordered_map<int, v8_inspector::String16> responses_;
};

}  // namespace

TEST_F(InspectorTest, CrossContextCustomPreviewBody) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context_a = v8_context();
  v8::Local<v8::Context> context_b = v8::Context::New(isolate);
  v8::Local<v8::String> token = NewString("same-token");
  context_a->SetSecurityToken(token);
  context_b->SetSecurityToken(token);

  {
    v8::Context::Scope scope_b(context_b);
    v8::Local<v8::Value> target_obj =
        RunJS(context_b,
              "globalThis.devtoolsFormatters = [{\n"
              "  header: (x) => x && x.isTarget ? ['span', {}, 'custom "
              "header'] : null,\n"
              "  hasBody: (x) => x && x.isTarget,\n"
              "  body: (x) => ['span', {}, 'custom body']\n"
              "}];\n"
              "({isTarget: true})");
    context_a->Global()
        ->Set(context_a, NewString("target"), target_obj)
        .ToChecked();
  }

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<v8_inspector::V8InspectorImpl> inspector(
      new v8_inspector::V8InspectorImpl(isolate, &default_client));
  const char kContextAName[] = "contextA";
  StringView name_a(reinterpret_cast<const uint8_t*>(kContextAName),
                    strlen(kContextAName));
  V8ContextInfo context_info_a(context_a, 1, name_a);
  inspector->contextCreated(context_info_a);

  const char kContextBName[] = "contextB";
  StringView name_b(reinterpret_cast<const uint8_t*>(kContextBName),
                    strlen(kContextBName));
  V8ContextInfo context_info_b(context_b, 1, name_b);
  inspector->contextCreated(context_info_b);

  CustomPreviewChannel channel;
  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kFullyTrusted,
      v8_inspector::V8Inspector::kNotWaitingForDebugger);

  session->dispatchProtocolMessage(
      toStringView(R"({"id": 1, "method": "Runtime.enable"})"));
  session->dispatchProtocolMessage(toStringView(
      R"({"id": 2, "method": "Runtime.setCustomObjectFormatterEnabled", "params": {"enabled": true}})"));

  session->dispatchProtocolMessage(toStringView(
      R"({"id": 3, "method": "Runtime.evaluate", "params": {"expression": "target", "contextId": 1, "generatePreview": true}})"));

  CHECK(channel.responses_.find(3) != channel.responses_.end());
  v8_inspector::String16 evaluate_response = channel.responses_[3];

  v8::Context::Scope scope_a(context_a);
  v8::Local<v8::Object> eval_json =
      v8::JSON::Parse(context_a, NewString(evaluate_response.utf8().c_str()))
          .ToLocalChecked()
          .As<v8::Object>();
  v8::Local<v8::Object> result_obj =
      eval_json->Get(context_a, NewString("result"))
          .ToLocalChecked()
          .As<v8::Object>()
          ->Get(context_a, NewString("result"))
          .ToLocalChecked()
          .As<v8::Object>();
  v8::Local<v8::String> object_id =
      result_obj->Get(context_a, NewString("objectId"))
          .ToLocalChecked()
          .As<v8::String>();
  v8::Local<v8::Object> custom_preview =
      result_obj->Get(context_a, NewString("customPreview"))
          .ToLocalChecked()
          .As<v8::Object>();
  v8::Local<v8::String> body_getter_id =
      custom_preview->Get(context_a, NewString("bodyGetterId"))
          .ToLocalChecked()
          .As<v8::String>();

  v8::String::Utf8Value object_id_utf8(isolate, object_id);
  v8::String::Utf8Value body_getter_id_utf8(isolate, body_getter_id);

  std::string call_command =
      std::string(
          R"({"id": 4, "method": "Runtime.callFunctionOn", "params": {)") +
      R"("objectId": ")" + *object_id_utf8 + R"(", )" +
      R"("functionDeclaration": "function(bodyGetter) { return bodyGetter.call(this); }", )" +
      R"("arguments": [{"objectId": ")" + *body_getter_id_utf8 + R"("}], )" +
      R"("returnByValue": true}})";

  session->dispatchProtocolMessage(
      toStringView(String16(call_command.c_str())));

  CHECK(channel.responses_.find(4) != channel.responses_.end());
  v8_inspector::String16 call_response = channel.responses_[4];
  CHECK_NE(call_response.find(v8_inspector::String16("custom body")),
           v8_inspector::String16::kNotFound);
}

TEST_F(InspectorTest, BinaryToBase64) {
  uint8_t input[] = {'a', 'b', 'c'};
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(
        std::span<const uint8_t>(input, 0));
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "");
  }
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(
        std::span<const uint8_t>(input, 1));
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "YQ==");
  }
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(
        std::span<const uint8_t>(input, 2));
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "YWI=");
  }
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(
        std::span<const uint8_t>(input, 3));
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "YWJj");
  }
}

TEST_F(InspectorTest, BinaryBase64RoundTrip) {
  std::array<uint8_t, 256> values;
  for (uint16_t b = 0x0; b <= 0xFF; ++b) values[b] = b;
  auto binary = v8_inspector::protocol::Binary::fromSpan(
      std::span<const uint8_t>(values));
  v8_inspector::protocol::String base64 = binary.toBase64();
  bool success = false;
  auto roundtrip_binary =
      v8_inspector::protocol::Binary::fromBase64(base64, &success);
  CHECK(success);
  CHECK_EQ(values.size(), roundtrip_binary.size());
  for (size_t i = 0; i < values.size(); ++i) {
    CHECK_EQ(values[i], roundtrip_binary.data()[i]);
  }
}

TEST_F(InspectorTest, NoInterruptOnGetAssociatedData) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<v8_inspector::V8InspectorImpl> inspector(
      new v8_inspector::V8InspectorImpl(isolate, &default_client));

  v8::Local<v8::Value> error = v8::Exception::Error(NewString("custom error"));
  v8::Local<v8::Name> key = NewString("key");
  v8::Local<v8::Value> value = NewString("value");
  inspector->associateExceptionData(v8_context(), error, key, value);

  struct InterruptRecorder {
    static void handler(v8::Isolate* isolate, void* data) {
      reinterpret_cast<InterruptRecorder*>(data)->WasInvoked = true;
    }

    bool WasInvoked = false;
  } recorder;

  isolate->RequestInterrupt(&InterruptRecorder::handler, &recorder);

  v8::Local<v8::Object> data =
      inspector->getAssociatedExceptionData(error).ToLocalChecked();
  CHECK(!recorder.WasInvoked);

  CHECK_EQ(data->Get(v8_context(), key).ToLocalChecked(), value);

  TryRunJS("0");
  CHECK(recorder.WasInvoked);
}

class TestChannel : public V8Inspector::Channel {
 public:
  ~TestChannel() override = default;
  void sendResponse(int callId,
                    std::unique_ptr<StringBuffer> message) override {
    CHECK_EQ(callId, 1);
    CHECK_NE(toString16(message->string()).find(expected_response_matcher_),
             String16::kNotFound);
  }
  void sendNotification(std::unique_ptr<StringBuffer> message) override {}
  void flushProtocolNotifications() override {}
  v8_inspector::String16 expected_response_matcher_;
};

class DeepSerializeEmbedderClient : public v8_inspector::V8InspectorClient {
 public:
  std::unique_ptr<v8_inspector::DeepSerializationResult> deepSerialize(
      v8::Local<v8::Value> v8Value, int maxDepth,
      v8::Local<v8::Object> additionalParameters) override {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    if (v8Value->IsObject()) {
      v8::Local<v8::Object> obj = v8Value.As<v8::Object>();
      v8::Local<v8::Value> marker;
      if (obj->Get(context,
                   v8::String::NewFromUtf8Literal(isolate, "embedder_marker"))
              .ToLocal(&marker) &&
          marker->IsTrue()) {
        auto deep_val = std::make_unique<v8_inspector::DeepSerializedValue>(
            v8_inspector::StringBuffer::create(toStringView("embedder_node")),
            v8::String::NewFromUtf8Literal(isolate, "a"));
        return std::make_unique<v8_inspector::DeepSerializationResult>(
            std::move(deep_val));
      }
      if (obj->Get(context,
                   v8::String::NewFromUtf8Literal(isolate, "embedder_exceed"))
              .ToLocal(&marker) &&
          marker->IsTrue()) {
        v8::Local<v8::Array> arr = v8::Array::New(isolate, 2);
        v8::Local<v8::Array> a1 = v8::Array::New(isolate, 1);
        v8::Local<v8::Array> a2 = v8::Array::New(isolate, 10'000'000 - 3);
        CHECK(arr->Set(context, 0, a1).FromJust());
        CHECK(arr->Set(context, 1, a2).FromJust());
        auto deep_val = std::make_unique<v8_inspector::DeepSerializedValue>(
            v8_inspector::StringBuffer::create(toStringView("embedder_node")),
            arr);
        return std::make_unique<v8_inspector::DeepSerializationResult>(
            std::move(deep_val));
      }
      if (obj->Get(context,
                   v8::String::NewFromUtf8Literal(isolate, "embedder_one"))
              .ToLocal(&marker) &&
          marker->IsTrue()) {
        v8::Local<v8::Array> arr = v8::Array::New(isolate, 1);
        auto deep_val = std::make_unique<v8_inspector::DeepSerializedValue>(
            v8_inspector::StringBuffer::create(toStringView("embedder_node")),
            arr);
        return std::make_unique<v8_inspector::DeepSerializationResult>(
            std::move(deep_val));
      }
    }
    return nullptr;
  }
};

class SubtypeDescriptionClient : public v8_inspector::V8InspectorClient {
 public:
  std::unique_ptr<v8_inspector::StringBuffer> valueSubtype(
      v8::Local<v8::Value> value) override {
    if (value->IsObject() && !value->IsArray() && !value->IsMap()) {
      return v8_inspector::StringBuffer::create(toStringView("node"));
    }
    return nullptr;
  }

  std::unique_ptr<v8_inspector::StringBuffer> descriptionForValueSubtype(
      v8::Local<v8::Context>, v8::Local<v8::Value>) override {
    description_calls_++;
    return v8_inspector::StringBuffer::create(toStringView("node_desc"));
  }

  int description_calls_ = 0;
};

class OOMTestChannel : public v8_inspector::V8Inspector::Channel {
 public:
  ~OOMTestChannel() override = default;
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    response_ = v8_inspector::toString16(message->string());
  }
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {}
  void flushProtocolNotifications() override {}

  v8_inspector::String16 response_;
};

TEST_F(InspectorTest, RegressDeepSerializeBudget) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  DeepSerializeEmbedderClient client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  OOMTestChannel channel;
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);

  std::string command =
      "{\"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": "
      "{\"expression\": "
      "\"[{embedder_marker: true}, 'x'.repeat(" +
      std::to_string(v8::String::kMaxLength) +
      ")]\", \"contextId\": 1, \"generatePreview\": false, "
      "\"serializationOptions\": "
      "{\"serialization\": \"deep\"}}}";

  session->dispatchProtocolMessage(toStringView(String16(command.c_str())));

  CHECK_NE(
      channel.response_.find(toString16(toStringView("Object is too large"))),
      String16::kNotFound);
}

TEST_F(InspectorTest, RegressDeepSerializeValuesBudget) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  DeepSerializeEmbedderClient client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  std::vector<std::string> expressions = {
      "[new Array(1), {embedder_exceed: true}]",
      "[{a: 1}, {embedder_exceed: true}]",
      "[new Map([[1, 1]]), {embedder_exceed: true}]",
      "[{embedder_one: true}, new Array(10000000)]",
  };

  for (const auto& expr : expressions) {
    OOMTestChannel channel;
    std::unique_ptr<V8InspectorSession> session =
        inspector->connect(1, &channel, toStringView("{}"),
                           v8_inspector::V8Inspector::kFullyTrusted);

    std::string command =
        "{\"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": "
        "{\"expression\": \"" +
        expr +
        "\", \"contextId\": 1, \"generatePreview\": false, "
        "\"serializationOptions\": {\"serialization\": \"deep\"}}}";

    session->dispatchProtocolMessage(toStringView(String16(command.c_str())));

    CHECK_NE(
        channel.response_.find(toString16(toStringView("Object is too large"))),
        String16::kNotFound);
  }
}

TEST_F(InspectorTest, DeepSerializeDoesNotDescribeChildObjects) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  SubtypeDescriptionClient client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  OOMTestChannel channel;
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);

  std::string command =
      "{\"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": "
      "{\"expression\": \"const x = {}; [x, x, {a: x}, new Map([[x, x]])]\","
      "\"contextId\": 1, \"generatePreview\": false, "
      "\"serializationOptions\": {\"serialization\": \"deep\"}}}";

  session->dispatchProtocolMessage(toStringView(String16(command.c_str())));

  CHECK_EQ(client.description_calls_, 0);
}

TEST_F(InspectorTest, NoConsoleAPIForUntrustedClient) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  TestChannel channel;
  const char kCommand[] = R"({
    "id": 1,
    "method": "Runtime.evaluate",
    "params": {
      "expression": "$0 || 42",
      "contextId": 1,
      "includeCommandLineAPI": true
    }
  })";
  std::unique_ptr<V8InspectorSession> trusted_session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);
  channel.expected_response_matcher_ = R"("value":42)";
  trusted_session->dispatchProtocolMessage(toStringView(kCommand));

  std::unique_ptr<V8InspectorSession> untrusted_session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kUntrusted);
  channel.expected_response_matcher_ = R"("className":"ReferenceError")";
  untrusted_session->dispatchProtocolMessage(toStringView(kCommand));
}

TEST_F(InspectorTest, CanHandleMalformedCborMessage) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  TestChannel channel;
  const unsigned char kCommand[] = {0xD8, 0x5A, 0x00, 0xBA, 0xDB, 0xEE, 0xF0};
  std::unique_ptr<V8InspectorSession> trusted_session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);
  channel.expected_response_matcher_ = R"("value":42)";
  trusted_session->dispatchProtocolMessage(
      StringView(kCommand, sizeof(kCommand)));
}

TEST_F(InspectorTest, Evaluate) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  TestChannel channel;
  std::unique_ptr<V8InspectorSession> trusted_session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);

  {
    auto result =
        trusted_session->evaluate(v8_context(), toStringView("21 + 21"));
    CHECK_EQ(
        result.type,
        v8_inspector::V8InspectorSession::EvaluateResult::ResultType::kSuccess);
    CHECK_EQ(result.value->IntegerValue(v8_context()).FromJust(), 42);
  }
  {
    auto result = trusted_session->evaluate(
        v8_context(), toStringView("throw new Error('foo')"));
    CHECK_EQ(result.type, v8_inspector::V8InspectorSession::EvaluateResult::
                              ResultType::kException);
    CHECK(result.value->IsNativeError());
  }
  {
    // Unknown context.
    v8::Local<v8::Context> ctx = v8::Context::New(v8_isolate());
    auto result = trusted_session->evaluate(ctx, toStringView("21 + 21"));
    CHECK_EQ(
        result.type,
        v8_inspector::V8InspectorSession::EvaluateResult::ResultType::kNotRun);
  }
  {
    // CommandLine API
    auto result = trusted_session->evaluate(v8_context(),
                                            toStringView("debug(console.log)"),
                                            /*includeCommandLineAPI=*/true);
    CHECK_EQ(
        result.type,
        v8_inspector::V8InspectorSession::EvaluateResult::ResultType::kSuccess);
    CHECK(result.value->IsUndefined());
  }
}

// Regression test for crbug.com/323813642.
TEST_F(InspectorTest, NoInterruptWhileBuildingConsoleMessages) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<v8_inspector::V8InspectorImpl> inspector(
      new v8_inspector::V8InspectorImpl(isolate, &default_client));
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  TestChannel channel;
  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kFullyTrusted,
      v8_inspector::V8Inspector::kNotWaitingForDebugger);
  reinterpret_cast<v8_inspector::V8InspectorSessionImpl*>(session.get())
      ->runtimeAgent()
      ->enable();

  struct InterruptRecorder {
    static void handler(v8::Isolate* isolate, void* data) {
      reinterpret_cast<InterruptRecorder*>(data)->WasInvoked = true;
    }

    bool WasInvoked = false;
  } recorder;

  isolate->RequestInterrupt(&InterruptRecorder::handler, &recorder);

  v8::Local<v8::Value> error = v8::Exception::Error(NewString("custom error"));
  inspector->exceptionThrown(v8_context(), toStringView("message"), error,
                             toStringView("detailed message"),
                             toStringView("https://example.com/script.js"), 42,
                             21, std::unique_ptr<v8_inspector::V8StackTrace>(),
                             0);

  CHECK(!recorder.WasInvoked);

  TryRunJS("0");
  CHECK(recorder.WasInvoked);
}

class UAFTriggerChannel : public V8Inspector::Channel {
 public:
  UAFTriggerChannel(v8::Isolate* isolate, V8Inspector* inspector,
                    int contextGroupId, v8::Local<v8::Context> context,
                    const std::string& targetNotification)
      : m_isolate(isolate),
        m_inspector(inspector),
        m_contextGroupId(contextGroupId),
        m_context(isolate, context),
        m_targetNotification(targetNotification) {}
  ~UAFTriggerChannel() override = default;

  void sendResponse(int callId,
                    std::unique_ptr<StringBuffer> message) override {}

  void sendNotification(std::unique_ptr<StringBuffer> message) override {
    String16 message_str = toString16(message->string());
    if (message_str.find("Runtime.consoleAPICalled") != String16::kNotFound) {
      m_consoleAPICalled = true;
    }
    if (message_str.find(m_targetNotification.c_str()) != String16::kNotFound) {
      m_triggerCount++;
      m_inspector->resetContextGroup(m_contextGroupId);

      // Recreate storage using public exceptionThrown API.
      // This avoids depending on internal non-exported V8InspectorImpl methods.
      v8::HandleScope handle_scope(m_isolate);
      v8::Local<v8::Context> context = m_context.Get(m_isolate);
      v8::Context::Scope context_scope(context);

      v8::Local<v8::Value> error = v8::Exception::Error(
          v8::String::NewFromUtf8(m_isolate, "dummy").ToLocalChecked());
      const char* dummy_str = "dummy";
      StringView dummy_view(reinterpret_cast<const uint8_t*>(dummy_str),
                            strlen(dummy_str));

      m_inspector->exceptionThrown(context, dummy_view, error, dummy_view,
                                   dummy_view, 0, 0, nullptr, 0);
    }
  }

  void flushProtocolNotifications() override {}

  // triggerCount() tracks how many times the target notification was
  // intercepted. Asserting that triggerCount() == 1 in the tests serves a dual
  // purpose:
  // 1. Verifies Activation: Confirms the UAF sequence (reset and recreate)
  //    successfully ran, preventing false positives.
  // 2. Verifies Early Termination: In loop tests (Test 1 & 2), confirms that
  //    the loop successfully aborted after the first message and did not
  //    process subsequent queued messages (which would trigger a second count).
  int triggerCount() const { return m_triggerCount; }
  bool consoleAPICalled() const { return m_consoleAPICalled; }

 private:
  v8::Isolate* m_isolate;
  V8Inspector* m_inspector;
  int m_contextGroupId;
  v8::Global<v8::Context> m_context;
  std::string m_targetNotification;
  int m_triggerCount = 0;
  bool m_consoleAPICalled = false;
};

TEST_F(InspectorTest, NoUAFWhenResettingContextGroupDuringMessageReporting) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  UAFTriggerChannel channel(isolate, inspector.get(), 1, v8_context(),
                            "Runtime.consoleAPICalled");
  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kFullyTrusted,
      v8_inspector::V8Inspector::kNotWaitingForDebugger);

  // Log messages to accumulate them in storage while Runtime agent is disabled.
  TryRunJS("console.log(1); console.log(2);");

  // Enable Runtime agent, which will trigger reporting of accumulated messages.
  // The first message will trigger UAFTriggerChannel::sendNotification,
  // which will reset the context group and destroy the storage.
  // The loop in V8RuntimeAgentImpl::enable should safely break.
  const char kEnableCommand[] = R"({
    "id": 1,
    "method": "Runtime.enable"
  })";
  session->dispatchProtocolMessage(toStringView(kEnableCommand));

  // Verify that we only received one notification (for the first message)
  // and did not crash due to UAF on the second message.
  CHECK_EQ(channel.triggerCount(), 1);
}

TEST_F(InspectorTest,
       NoUAFWhenResettingContextGroupDuringConsoleMessageReporting) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  UAFTriggerChannel channel(isolate, inspector.get(), 1, v8_context(),
                            "Console.messageAdded");
  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kFullyTrusted,
      v8_inspector::V8Inspector::kNotWaitingForDebugger);

  // Log messages to accumulate them in storage while Console agent is disabled.
  TryRunJS("console.log(1); console.log(2);");

  // Enable Console agent, which will trigger reporting of accumulated messages.
  // The first message will trigger UAFTriggerChannel::sendNotification, which
  // will reset the context group and destroy the storage. The loop in
  // V8ConsoleAgentImpl::reportAllMessages should safely break.
  const char kEnableCommand[] = R"({
    "id": 1,
    "method": "Console.enable"
  })";
  session->dispatchProtocolMessage(toStringView(kEnableCommand));

  // Verify that we only received one notification (for the first message)
  // and did not crash due to UAF on the second message.
  CHECK_EQ(channel.triggerCount(), 1);
}

TEST_F(InspectorTest, NoUAFWhenResettingContextGroupDuringArgumentWrapping) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  UAFTriggerChannel channel(isolate, inspector.get(), 1, v8_context(),
                            "Debugger.paused");
  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kFullyTrusted,
      v8_inspector::V8Inspector::kNotWaitingForDebugger);

  // Enable Runtime and Debugger agents.
  session->dispatchProtocolMessage(toStringView(R"({
    "id": 1,
    "method": "Runtime.enable"
  })"));
  session->dispatchProtocolMessage(toStringView(R"({
    "id": 2,
    "method": "Debugger.enable"
  })"));

  // Run JS that overrides Error.prepareStackTrace and logs an Error object.
  // This will trigger wrapArguments -> wrapObject -> reads 'stack' ->
  // Error.prepareStackTrace -> debugger; Debugger.paused will be sent
  // synchronously during wrapping, and our channel will reset the context
  // group. reportToFrontend and addMessage should detect the changed storage
  // and abort early.
  TryRunJS(R"(
    Error.prepareStackTrace = function(error, stack) {
      debugger;
      return stack;
    };
    const err = new Error("Trigger UAF");
    console.log(err);
  )");

  // Verify that we paused once and did not crash.
  CHECK_EQ(channel.triggerCount(), 1);
  // Verify that reportToFrontend was aborted and did not send the notification.
  CHECK(!channel.consoleAPICalled());
}

TEST_F(InspectorTest, WrapObjectWithCustomPreviewNoPreview) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  UAFTriggerChannel channel(isolate, inspector.get(), 1, v8_context(),
                            "Runtime.consoleAPICalled");
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);

  // Enable Runtime agent.
  session->dispatchProtocolMessage(toStringView(R"({
    "id": 1,
    "method": "Runtime.enable"
  })"));
  session->dispatchProtocolMessage(toStringView(R"({
    "id": 2,
    "method": "Runtime.setCustomObjectFormatterEnabled",
    "params": {
      "enabled": true
    }
  })"));

  TryRunJS(R"(
    this.devtoolsFormatters = [{
      header: (x) => {
        console.log(new Error("FAILED: custom preview logic executed"));
        return ['span', {}, 'custom_preview_header'];
      },
      hasBody: (x) => false,
    }];
    this.obj = { a: 1 };
  )");

  v8::Local<v8::Value> obj = v8_context()
                                 ->Global()
                                 ->Get(v8_context(), NewString("obj"))
                                 .ToLocalChecked();

  std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
      remote_object_api =
          session->wrapObject(v8_context(), obj, toStringView("testGroup"),
                              /*generatePreview=*/false);

  CHECK(remote_object_api);
  auto* remote_object =
      static_cast<v8_inspector::protocol::Runtime::RemoteObject*>(
          remote_object_api.get());

  EXPECT_FALSE(remote_object->hasPreview());
  EXPECT_FALSE(remote_object->hasCustomPreview());
  EXPECT_EQ(channel.triggerCount(), 0);
}

namespace {

void TerminateExecutionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->TerminateExecution();
}

}  // namespace

// Regression test for the dangling termination exception left behind by
// generateCustomPreview() when it runs at call depth zero without an outer
// v8::TryCatch (e.g. an embedder calling V8InspectorSession::wrapObject with
// generatePreview=true from C++).
TEST_F(InspectorTest, TerminationInCustomPreviewAtCallDepthZero) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  NoopChannel channel;
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);
  session->dispatchProtocolMessage(toStringView(R"({
    "id": 1,
    "method": "Runtime.enable"
  })"));
  session->dispatchProtocolMessage(toStringView(R"({
    "id": 2,
    "method": "Runtime.setCustomObjectFormatterEnabled",
    "params": {"enabled": true}
  })"));

  v8::Local<v8::Function> terminate =
      v8::Function::New(v8_context(), &TerminateExecutionCallback)
          .ToLocalChecked();
  CHECK(v8_context()
            ->Global()
            ->Set(v8_context(), NewString("terminate"), terminate)
            .FromJust());

  // The custom formatter terminates execution while generateCustomPreview()
  // calls into it.
  TryRunJS(R"(
    this.devtoolsFormatters = [{
      header: (x) => { terminate(); return ['span', 'header']; },
      hasBody: (x) => false,
    }];
    this.obj = {a: 1};
  )");
  CHECK(!isolate->IsExecutionTerminating());

  v8::Local<v8::Value> obj = v8_context()
                                 ->Global()
                                 ->Get(v8_context(), NewString("obj"))
                                 .ToLocalChecked();

  // No enclosing v8::TryCatch and no JavaScript on the stack: call depth is
  // zero here.
  session->wrapObject(v8_context(), obj, toStringView("testGroup"),
                      /*generatePreview=*/true);

  // The termination must not be left pending on the isolate after the API
  // call returned at call depth zero: the next V8 API call would hit
  // DCHECK(!is_execution_terminating()) in EnterV8InternalScope.
  CHECK(!isolate->IsExecutionTerminating());
}

namespace {

class DefaultContextClient : public v8_inspector::V8InspectorClient {
 public:
  DefaultContextClient(v8::Isolate* isolate, v8::Local<v8::Context> context)
      : m_isolate(isolate), m_context(isolate, context) {}
  v8::Local<v8::Context> ensureDefaultContextInGroup(
      int contextGroupId) override {
    return m_context.Get(m_isolate);
  }

 private:
  v8::Isolate* m_isolate;
  v8::Global<v8::Context> m_context;
};

}  // namespace

TEST_F(InspectorTest, TerminateExecutionWithDetachedDefaultContext) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = v8::Context::New(isolate);
  DefaultContextClient client(isolate, context);
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &client);
  V8ContextInfo context_info(context, 1, toStringView(""));
  inspector->contextCreated(context_info);

  NoopChannel channel;
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, toStringView("{}"),
                         v8_inspector::V8Inspector::kFullyTrusted);

  // 1. Detach default context while terminateExecution callback is registered.
  session->dispatchProtocolMessage(toStringView(R"({
    "id": 1,
    "method": "Runtime.terminateExecution"
  })"));
  context->DetachGlobal();
  // Destroying session and inspector should not crash when removing the
  // callback from the detached context's microtask queue.
  session.reset();
  inspector.reset();

  // 2. Calling Runtime.terminateExecution when the default context is already
  // detached.
  std::unique_ptr<V8Inspector> inspector2 =
      V8Inspector::create(isolate, &client);
  std::unique_ptr<V8InspectorSession> session2 =
      inspector2->connect(1, &channel, toStringView("{}"),
                          v8_inspector::V8Inspector::kFullyTrusted);
  session2->dispatchProtocolMessage(toStringView(R"({
    "id": 2,
    "method": "Runtime.terminateExecution"
  })"));
  session2.reset();
  inspector2.reset();
}

TEST_F(InspectorTest, TerminateExecutionWithDetachedCustomMicrotaskQueue) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  // 1. Destroying session and inspector after DetachGlobal + GC.
  {
    v8::MicrotaskQueue* microtask_queue = v8::MicrotaskQueue::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(
        isolate, nullptr, MaybeLocal<ObjectTemplate>(), MaybeLocal<Value>(),
        DeserializeInternalFieldsCallback(), microtask_queue);
    DefaultContextClient client(isolate, context);
    std::unique_ptr<V8Inspector> inspector =
        V8Inspector::create(isolate, &client);
    V8ContextInfo context_info(context, 1, toStringView(""));
    inspector->contextCreated(context_info);

    NoopChannel channel;
    std::unique_ptr<V8InspectorSession> session =
        inspector->connect(1, &channel, toStringView("{}"),
                           v8_inspector::V8Inspector::kFullyTrusted);

    session->dispatchProtocolMessage(toStringView(R"({
      "id": 1,
      "method": "Runtime.terminateExecution"
    })"));
    context->DetachGlobal();
    {
      EmbedderStackStateScope stack_scope(
          i_isolate()->heap(), EmbedderStackStateOrigin::kExplicitInvocation,
          StackState::kNoHeapPointers);
      ::v8::internal::InvokeMajorGC(i_isolate(), GCFlag::kForced);
    }
    session.reset();
    inspector.reset();
  }

  // 2. Running JS to trigger reportTermination after DetachGlobal + GC.
  {
    v8::MicrotaskQueue* microtask_queue = v8::MicrotaskQueue::New(isolate);
    v8::Local<v8::Context> context = v8::Context::New(
        isolate, nullptr, MaybeLocal<ObjectTemplate>(), MaybeLocal<Value>(),
        DeserializeInternalFieldsCallback(), microtask_queue);
    DefaultContextClient client(isolate, context);
    std::unique_ptr<V8Inspector> inspector =
        V8Inspector::create(isolate, &client);
    V8ContextInfo context_info(context, 1, toStringView(""));
    inspector->contextCreated(context_info);

    NoopChannel channel;
    std::unique_ptr<V8InspectorSession> session =
        inspector->connect(1, &channel, toStringView("{}"),
                           v8_inspector::V8Inspector::kFullyTrusted);

    session->dispatchProtocolMessage(toStringView(R"({
      "id": 1,
      "method": "Runtime.terminateExecution"
    })"));
    context->DetachGlobal();
    {
      EmbedderStackStateScope stack_scope(
          i_isolate()->heap(), EmbedderStackStateOrigin::kExplicitInvocation,
          StackState::kNoHeapPointers);
      ::v8::internal::InvokeMajorGC(i_isolate(), GCFlag::kForced);
    }
    TryRunJS("1");
    session.reset();
    inspector.reset();
  }
}

namespace {

// Calls into JavaScript for every Debugger.paused and Debugger.resumed
// notification, like an embedder that exposes an in-process session to JS.
class JSCallbackChannel : public V8Inspector::Channel {
 public:
  JSCallbackChannel(v8::Isolate* isolate, v8::Local<v8::Context> context,
                    v8::Local<v8::Function> callback)
      : m_isolate(isolate),
        m_context(isolate, context),
        m_callback(isolate, callback) {}
  ~JSCallbackChannel() override = default;

  void sendResponse(int callId,
                    std::unique_ptr<StringBuffer> message) override {}

  void sendNotification(std::unique_ptr<StringBuffer> message) override {
    String16 message_str = toString16(message->string());
    if (message_str.find("Debugger.paused") == String16::kNotFound &&
        message_str.find("Debugger.resumed") == String16::kNotFound) {
      return;
    }
    v8::HandleScope handle_scope(m_isolate);
    v8::Local<v8::Context> context = m_context.Get(m_isolate);
    v8::Context::Scope context_scope(context);
    m_callback.Get(m_isolate)
        ->Call(context, v8::Undefined(m_isolate), 0, nullptr)
        .ToLocalChecked();
  }

  void flushProtocolNotifications() override {}

 private:
  v8::Isolate* m_isolate;
  v8::Global<v8::Context> m_context;
  v8::Global<v8::Function> m_callback;
};

void DispatchPause(const v8::FunctionCallbackInfo<v8::Value>& info) {
  static_cast<V8InspectorSession*>(info.DataV2().As<v8::External>()->Value(
                                       v8::kExternalPointerTypeTagDefault))
      ->dispatchProtocolMessage(
          toStringView(R"({"id": 2, "method": "Debugger.pause"})"));
}

void CaptureIsTopScriptOpaque(const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto* inspector =
      static_cast<V8Inspector*>(info.DataV2().As<v8::External>()->Value(
          v8::kExternalPointerTypeTagDefault));
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      inspector->captureStackTrace(/*fullStack=*/false);
  CHECK(stack_trace);
  CHECK(!stack_trace->isEmpty());
  info.GetReturnValue().Set(stack_trace->isTopScriptOpaque());
}

}  // namespace

TEST_F(InspectorTest, ChannelCanRunJSWhenPausedOnInterrupt) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  v8::Local<v8::Function> callback =
      RunJS("globalThis.notifications = 0; () => notifications++")
          .As<v8::Function>();
  JSCallbackChannel channel(isolate, v8_context(), callback);
  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kFullyTrusted,
      v8_inspector::V8Inspector::kNotWaitingForDebugger);
  session->dispatchProtocolMessage(
      toStringView(R"({"id": 1, "method": "Debugger.enable"})"));

  v8::Local<v8::Function> pause =
      v8::Function::New(v8_context(), &DispatchPause,
                        v8::External::New(isolate, session.get(),
                                          v8::kExternalPointerTypeTagDefault))
          .ToLocalChecked();
  CHECK(v8_context()
            ->Global()
            ->Set(v8_context(), NewString("pause"), pause)
            .FromJust());

  // Debugger.pause while JavaScript runs schedules the break through an API
  // interrupt, where JavaScript execution is disallowed. The loop spins until
  // it takes that interrupt on a back edge. Taken on function entry instead,
  // V8 would step in and pause at the next statement, outside the interrupt,
  // so the count is read in the same script.
  v8::Local<v8::Value> result = RunJS(
      "pause();"
      "for (let i = 0; i < 1e7 && notifications === 0; i++) {}"
      "notifications");
  CHECK_EQ(result->Int32Value(v8_context()).FromJust(), 2);
}

namespace {

class DeoptOnRestartFrameChannel : public V8Inspector::Channel,
                                   public v8_inspector::V8InspectorClient {
 public:
  DeoptOnRestartFrameChannel(v8::Isolate* isolate,
                             v8::Local<v8::Context> context)
      : isolate_(isolate), context_(isolate, context) {}
  ~DeoptOnRestartFrameChannel() override = default;

  void set_session(V8InspectorSession* session) { session_ = session; }

  void DispatchRestartFrame() {
    if (!call_frame_id_.empty() && session_) {
      std::string msg =
          "{\"id\": 2, \"method\": \"Debugger.restartFrame\", \"params\": "
          "{\"callFrameId\": \"" +
          call_frame_id_ + "\", \"mode\": \"StepInto\"}}";
      StringView msg_view(reinterpret_cast<const uint8_t*>(msg.data()),
                          msg.size());
      session_->dispatchProtocolMessage(msg_view);
    }
  }

  void sendResponse(int callId,
                    std::unique_ptr<StringBuffer> message) override {
    if (callId == 2) {
      i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
      CHECK(!i_isolate->debug()->IsRestartFrameScheduled());
      response_received_ = true;
    }
  }

  void sendNotification(std::unique_ptr<StringBuffer> message) override {
    StringView str_view = message->string();
    std::string message_str;
    if (str_view.is8Bit()) {
      message_str =
          std::string(reinterpret_cast<const char*>(str_view.characters8()),
                      str_view.length());
    } else {
      for (size_t i = 0; i < str_view.length(); i++) {
        message_str += static_cast<char>(str_view.characters16()[i]);
      }
    }
    if (message_str.find("Debugger.paused") != std::string::npos) {
      const char kTarget[] = "\"callFrameId\":\"";
      size_t pos = message_str.find(kTarget);
      if (pos != std::string::npos) {
        size_t start = pos + strlen(kTarget);
        size_t end = message_str.find("\"", start);
        if (end != std::string::npos) {
          call_frame_id_ = message_str.substr(start, end - start);
        }
      }
    }
  }

  void runMessageLoopOnPause(int contextGroupId) override {
    if (!restarted_ && !call_frame_id_.empty() && session_) {
      restarted_ = true;
      DispatchRestartFrame();
    }
  }

  void quitMessageLoopOnPause() override {}

  void flushProtocolNotifications() override {}

  bool response_received() const { return response_received_; }

 private:
  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  V8InspectorSession* session_ = nullptr;
  std::string call_frame_id_;
  bool restarted_ = false;
  bool response_received_ = false;
};

}  // namespace

TEST_F(InspectorTest, ChannelCanDeoptWhenRestartingFrame) {
  FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax, true);

  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  DeoptOnRestartFrameChannel channel(isolate, v8_context());
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &channel);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  std::unique_ptr<V8InspectorSession> session = inspector->connect(
      1, &channel, toStringView("{}"), v8_inspector::V8Inspector::kFullyTrusted,
      v8_inspector::V8Inspector::kNotWaitingForDebugger);
  channel.set_session(session.get());

  session->dispatchProtocolMessage(
      toStringView(R"({"id": 1, "method": "Debugger.enable"})"));

  TryRunJS(R"(
    function target() { debugger; return 42; }
  )");

  v8::Local<v8::Value> result = TryRunJS("target()").ToLocalChecked();
  CHECK_EQ(result->Int32Value(v8_context()).FromJust(), 42);
  CHECK(channel.response_received());
}

TEST_F(InspectorTest, IsTopScriptOpaque) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  V8ContextInfo context_info(v8_context(), 1, toStringView(""));
  inspector->contextCreated(context_info);

  v8::Local<v8::Function> capture =
      v8::Function::New(v8_context(), &CaptureIsTopScriptOpaque,
                        v8::External::New(isolate, inspector.get(),
                                          v8::kExternalPointerTypeTagDefault))
          .ToLocalChecked();
  CHECK(v8_context()
            ->Global()
            ->Set(v8_context(), NewString("captureIsTopScriptOpaque"), capture)
            .FromJust());

  auto compile_and_run = [&](const char* source, const char* url,
                             bool is_opaque) -> v8::Local<v8::Value> {
    v8::ScriptOrigin origin(NewString(url), /*resource_line_offset=*/0,
                            /*resource_column_offset=*/0,
                            /*resource_is_shared_cross_origin=*/false,
                            /*script_id=*/-1, /*source_map_url=*/{},
                            /*resource_is_opaque=*/is_opaque);
    v8::Local<v8::Script> script =
        v8::Script::Compile(v8_context(), NewString(source), &origin)
            .ToLocalChecked();
    return script->Run(v8_context()).ToLocalChecked();
  };

  compile_and_run(
      "function nonOpaqueFn() { return captureIsTopScriptOpaque(); }"
      "function nonOpaqueEvalFn() { return eval('captureIsTopScriptOpaque()'); "
      "}",
      "https://example.com/non-opaque.js", /*is_opaque=*/false);
  compile_and_run(
      "function opaqueFn() { return captureIsTopScriptOpaque(); }"
      "function opaqueEvalFn() { return eval('captureIsTopScriptOpaque()'); }",
      "https://other.example/opaque.js", /*is_opaque=*/true);

  // Exercise both scripts (including repeated calls that hit
  // V8Debugger::symbolize()'s m_cachedStackFrames cache) and eval inside each.
  EXPECT_FALSE(compile_and_run("nonOpaqueFn()",
                               "https://example.com/caller1.js",
                               /*is_opaque=*/false)
                   ->BooleanValue(isolate));
  EXPECT_FALSE(compile_and_run("nonOpaqueFn()",
                               "https://other.example/caller2.js",
                               /*is_opaque=*/true)
                   ->BooleanValue(isolate));
  EXPECT_TRUE(compile_and_run("opaqueFn()", "https://example.com/caller3.js",
                              /*is_opaque=*/false)
                  ->BooleanValue(isolate));
  EXPECT_TRUE(compile_and_run("opaqueFn()", "https://other.example/caller4.js",
                              /*is_opaque=*/true)
                  ->BooleanValue(isolate));
  EXPECT_FALSE(compile_and_run("nonOpaqueEvalFn()",
                               "https://other.example/caller5.js",
                               /*is_opaque=*/true)
                   ->BooleanValue(isolate));
  EXPECT_TRUE(compile_and_run("opaqueEvalFn()",
                              "https://example.com/caller6.js",
                              /*is_opaque=*/false)
                  ->BooleanValue(isolate));
}
}  // namespace internal
}  // namespace v8
