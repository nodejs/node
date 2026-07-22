// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/remote-object-id.h"

#include "../../third_party/inspector_protocol/crdtp/json.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/string-util.h"

namespace v8_inspector {

namespace {

String16 serializeId(uint64_t isolateId, int injectedScriptId, int id) {
  return String16::concat(
      String16::fromInteger64(static_cast<int64_t>(isolateId)), ".",
      String16::fromInteger(injectedScriptId), ".", String16::fromInteger(id));
}

}  // namespace

RemoteObjectIdBase::RemoteObjectIdBase()
    : m_isolateId(0), m_injectedScriptId(0), m_id(0) {}

bool RemoteObjectIdBase::parseId(const String16& objectId) {
  const UChar dot = '.';
  size_t firstDotPos = objectId.find(dot);
  if (firstDotPos == String16::kNotFound) return false;
  bool ok = false;
  int64_t isolateId = objectId.substring(0, firstDotPos).toInteger64(&ok);
  if (!ok) return false;
  firstDotPos++;
  size_t secondDotPos = objectId.find(dot, firstDotPos);
  if (secondDotPos == String16::kNotFound) return false;
  int injectedScriptId =
      objectId.substring(firstDotPos, secondDotPos - firstDotPos)
          .toInteger(&ok);
  if (!ok) return false;
  secondDotPos++;
  int id = objectId.substring(secondDotPos).toInteger(&ok);
  if (!ok) return false;
  m_isolateId = static_cast<uint64_t>(isolateId);
  m_injectedScriptId = injectedScriptId;
  m_id = id;
  return true;
}

Response RemoteObjectId::parse(const String16& objectId,
                               std::unique_ptr<RemoteObjectId>* result) {
  std::unique_ptr<RemoteObjectId> remoteObjectId(new RemoteObjectId());
  if (!remoteObjectId->parseId(objectId))
    return Response::ServerError("Invalid remote object id");
  *result = std::move(remoteObjectId);
  return Response::Success();
}

String16 RemoteObjectId::serialize(uint64_t isolateId, int injectedScriptId,
                                   int id) {
  return serializeId(isolateId, injectedScriptId, id);
}

Response RemoteCallFrameId::parse(const String16& objectId,
                                  std::unique_ptr<RemoteCallFrameId>* result) {
  std::unique_ptr<RemoteCallFrameId> remoteCallFrameId(new RemoteCallFrameId());
  if (!remoteCallFrameId->parseId(objectId))
    return Response::ServerError("Invalid call frame id");
  *result = std::move(remoteCallFrameId);
  return Response::Success();
}

String16 RemoteCallFrameId::serialize(uint64_t isolateId, int injectedScriptId,
                                      int frameOrdinal) {
  return serializeId(isolateId, injectedScriptId, frameOrdinal);
}

}  // namespace v8_inspector
