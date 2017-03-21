// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/remote-object-id.h"

#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/string-util.h"

namespace v8_inspector {

RemoteObjectIdBase::RemoteObjectIdBase() : m_injectedScriptId(0) {}

std::unique_ptr<protocol::DictionaryValue>
RemoteObjectIdBase::parseInjectedScriptId(const String16& objectId) {
  std::unique_ptr<protocol::Value> parsedValue =
      protocol::StringUtil::parseJSON(objectId);
  if (!parsedValue || parsedValue->type() != protocol::Value::TypeObject)
    return nullptr;

  std::unique_ptr<protocol::DictionaryValue> parsedObjectId(
      protocol::DictionaryValue::cast(parsedValue.release()));
  bool success =
      parsedObjectId->getInteger("injectedScriptId", &m_injectedScriptId);
  if (success) return parsedObjectId;
  return nullptr;
}

RemoteObjectId::RemoteObjectId() : RemoteObjectIdBase(), m_id(0) {}

Response RemoteObjectId::parse(const String16& objectId,
                               std::unique_ptr<RemoteObjectId>* result) {
  std::unique_ptr<RemoteObjectId> remoteObjectId(new RemoteObjectId());
  std::unique_ptr<protocol::DictionaryValue> parsedObjectId =
      remoteObjectId->parseInjectedScriptId(objectId);
  if (!parsedObjectId) return Response::Error("Invalid remote object id");

  bool success = parsedObjectId->getInteger("id", &remoteObjectId->m_id);
  if (!success) return Response::Error("Invalid remote object id");
  *result = std::move(remoteObjectId);
  return Response::OK();
}

RemoteCallFrameId::RemoteCallFrameId()
    : RemoteObjectIdBase(), m_frameOrdinal(0) {}

Response RemoteCallFrameId::parse(const String16& objectId,
                                  std::unique_ptr<RemoteCallFrameId>* result) {
  std::unique_ptr<RemoteCallFrameId> remoteCallFrameId(new RemoteCallFrameId());
  std::unique_ptr<protocol::DictionaryValue> parsedObjectId =
      remoteCallFrameId->parseInjectedScriptId(objectId);
  if (!parsedObjectId) return Response::Error("Invalid call frame id");

  bool success =
      parsedObjectId->getInteger("ordinal", &remoteCallFrameId->m_frameOrdinal);
  if (!success) return Response::Error("Invalid call frame id");
  *result = std::move(remoteCallFrameId);
  return Response::OK();
}

String16 RemoteCallFrameId::serialize(int injectedScriptId, int frameOrdinal) {
  return "{\"ordinal\":" + String16::fromInteger(frameOrdinal) +
         ",\"injectedScriptId\":" + String16::fromInteger(injectedScriptId) +
         "}";
}

}  // namespace v8_inspector
