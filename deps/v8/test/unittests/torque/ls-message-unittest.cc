// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/ls/json.h"
#include "src/torque/ls/message-handler.h"
#include "src/torque/ls/message.h"
#include "src/torque/server-data.h"
#include "src/torque/source-positions.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

TEST(LanguageServerMessage, InitializeRequest) {
  InitializeRequest request;
  request.set_id(5);
  request.set_method("initialize");
  request.params();

  HandleMessage(request.GetJsonValue(), [](JsonValue& raw_response) {
    InitializeResponse response(raw_response);

    // Check that the response id matches up with the request id, and that
    // the language server signals its support for definitions.
    EXPECT_EQ(response.id(), 5);
    EXPECT_EQ(response.result().capabilities().definitionProvider(), true);
  });
}

TEST(LanguageServerMessage,
     RegisterDynamicCapabilitiesAfterInitializedNotification) {
  Request<bool> notification;
  notification.set_method("initialized");

  HandleMessage(notification.GetJsonValue(), [](JsonValue& raw_request) {
    RegistrationRequest request(raw_request);

    ASSERT_EQ(request.method(), "client/registerCapability");
    ASSERT_EQ(request.params().registrations_size(), (size_t)1);

    Registration registration = request.params().registrations(0);
    ASSERT_EQ(registration.method(), "workspace/didChangeWatchedFiles");

    auto options =
        registration
            .registerOptions<DidChangeWatchedFilesRegistrationOptions>();
    ASSERT_EQ(options.watchers_size(), (size_t)1);
  });
}

TEST(LanguageServerMessage, GotoDefinitionUnkownFile) {
  SourceFileMap::Scope source_file_map_scope;

  GotoDefinitionRequest request;
  request.set_id(42);
  request.set_method("textDocument/definition");
  request.params().textDocument().set_uri("file:///unknown.tq");

  HandleMessage(request.GetJsonValue(), [](JsonValue& raw_response) {
    GotoDefinitionResponse response(raw_response);
    EXPECT_EQ(response.id(), 42);
    EXPECT_TRUE(response.IsNull("result"));
  });
}

TEST(LanguageServerMessage, GotoDefinition) {
  SourceFileMap::Scope source_file_map_scope;
  SourceId test_id = SourceFileMap::AddSource("file://test.tq");
  SourceId definition_id = SourceFileMap::AddSource("file://base.tq");

  LanguageServerData::Scope server_data_scope;
  LanguageServerData::AddDefinition({test_id, {1, 0}, {1, 10}},
                                    {definition_id, {4, 1}, {4, 5}});

  // First, check a unknown definition. The result must be null.
  GotoDefinitionRequest request;
  request.set_id(42);
  request.set_method("textDocument/definition");
  request.params().textDocument().set_uri("file://test.tq");
  request.params().position().set_line(2);
  request.params().position().set_character(0);

  HandleMessage(request.GetJsonValue(), [](JsonValue& raw_response) {
    GotoDefinitionResponse response(raw_response);
    EXPECT_EQ(response.id(), 42);
    EXPECT_TRUE(response.IsNull("result"));
  });

  // Second, check a known defintion.
  request = GotoDefinitionRequest();
  request.set_id(43);
  request.set_method("textDocument/definition");
  request.params().textDocument().set_uri("file://test.tq");
  request.params().position().set_line(1);
  request.params().position().set_character(5);

  HandleMessage(request.GetJsonValue(), [](JsonValue& raw_response) {
    GotoDefinitionResponse response(raw_response);
    EXPECT_EQ(response.id(), 43);
    ASSERT_FALSE(response.IsNull("result"));

    Location location = response.result();
    EXPECT_EQ(location.uri(), "file://base.tq");
    EXPECT_EQ(location.range().start().line(), 4);
    EXPECT_EQ(location.range().start().character(), 1);
    EXPECT_EQ(location.range().end().line(), 4);
    EXPECT_EQ(location.range().end().character(), 5);
  });
}

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8
