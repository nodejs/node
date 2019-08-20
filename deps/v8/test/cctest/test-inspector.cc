// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "test/cctest/cctest.h"

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/string-16.h"

using v8_inspector::String16;
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

TEST(String16EndianTest) {
  const v8_inspector::UChar* expected =
      reinterpret_cast<const v8_inspector::UChar*>(u"Hello, \U0001F30E.");
  const uint16_t* utf16le = reinterpret_cast<const uint16_t*>(
      "H\0e\0l\0l\0o\0,\0 \0\x3c\xd8\x0e\xdf.\0");  // Same text in UTF16LE
                                                    // encoding

  String16 utf16_str = String16::fromUTF16LE(utf16le, 10);
  String16 expected_str = expected;

  CHECK_EQ(utf16_str, expected_str);
}
