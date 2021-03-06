// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/string-util.h"
#include "test/cctest/cctest.h"

using v8_inspector::StringBuffer;
using v8_inspector::StringView;
using v8_inspector::V8ContextInfo;
using v8_inspector::V8Inspector;
using v8_inspector::V8InspectorSession;

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

TEST(WrapInsideWrapOnInterrupt) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8_inspector::V8InspectorClient default_client;
  std::unique_ptr<V8Inspector> inspector =
      V8Inspector::create(isolate, &default_client);
  const char* name = "";
  StringView name_view(reinterpret_cast<const uint8_t*>(name), strlen(name));
  V8ContextInfo context_info(env.local(), 1, name_view);
  inspector->contextCreated(context_info);

  NoopChannel channel;
  const char* state = "{}";
  StringView state_view(reinterpret_cast<const uint8_t*>(state), strlen(state));
  std::unique_ptr<V8InspectorSession> session =
      inspector->connect(1, &channel, state_view);

  const char* object_group = "";
  StringView object_group_view(reinterpret_cast<const uint8_t*>(object_group),
                               strlen(object_group));
  isolate->RequestInterrupt(&WrapOnInterrupt, session.get());
  session->wrapObject(env.local(), v8::Null(isolate), object_group_view, false);
}

TEST(BinaryFromBase64) {
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

TEST(BinaryToBase64) {
  uint8_t input[] = {'a', 'b', 'c'};
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(input, 0);
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "");
  }
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(input, 1);
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "YQ==");
  }
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(input, 2);
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "YWI=");
  }
  {
    auto binary = v8_inspector::protocol::Binary::fromSpan(input, 3);
    v8_inspector::protocol::String base64 = binary.toBase64();
    CHECK_EQ(base64.utf8(), "YWJj");
  }
}

TEST(BinaryBase64RoundTrip) {
  std::array<uint8_t, 256> values;
  for (uint16_t b = 0x0; b <= 0xFF; ++b) values[b] = b;
  auto binary =
      v8_inspector::protocol::Binary::fromSpan(values.data(), values.size());
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
