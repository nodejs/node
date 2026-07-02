// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <span>

#include "include/v8-inspector.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-runtime-agent-impl.h"
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

      // Allocate a few small dummy blocks of varying sizes
      // and prevent the allocator from recycling the same memory address,
      // otherwise our test NoUAFWhenResettingContextGroupDuringArgumentWrapping
      // will actually report the message to the front-end.
      for (size_t size : {32, 48, 64, 80, 96, 128, 256}) {
        m_dummies.push_back(std::make_unique<std::vector<uint8_t>>(size, 0));
      }

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
  std::vector<std::unique_ptr<std::vector<uint8_t>>> m_dummies;
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
}  // namespace internal
}  // namespace v8
