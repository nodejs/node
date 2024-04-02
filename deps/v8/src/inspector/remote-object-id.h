// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_REMOTE_OBJECT_ID_H_
#define V8_INSPECTOR_REMOTE_OBJECT_ID_H_

#include <memory>

#include "src/inspector/protocol/Forward.h"

namespace v8_inspector {

using protocol::Response;

class RemoteObjectIdBase {
 public:
  uint64_t isolateId() const { return m_isolateId; }
  int contextId() const { return m_injectedScriptId; }

 protected:
  RemoteObjectIdBase();
  ~RemoteObjectIdBase() = default;

  bool parseId(const String16&);

  uint64_t m_isolateId;
  int m_injectedScriptId;
  int m_id;
};

class RemoteObjectId final : public RemoteObjectIdBase {
 public:
  static Response parse(const String16&, std::unique_ptr<RemoteObjectId>*);
  ~RemoteObjectId() = default;
  int id() const { return m_id; }

  static String16 serialize(uint64_t isolateId, int injectedScriptId, int id);
};

class RemoteCallFrameId final : public RemoteObjectIdBase {
 public:
  static Response parse(const String16&, std::unique_ptr<RemoteCallFrameId>*);
  ~RemoteCallFrameId() = default;

  int frameOrdinal() const { return m_id; }

  static String16 serialize(uint64_t isolateId, int injectedScriptId,
                            int frameOrdinal);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_REMOTE_OBJECT_ID_H_
