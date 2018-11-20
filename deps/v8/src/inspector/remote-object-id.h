// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_REMOTE_OBJECT_ID_H_
#define V8_INSPECTOR_REMOTE_OBJECT_ID_H_

#include "src/inspector/protocol/Forward.h"

namespace v8_inspector {

using protocol::Response;

class RemoteObjectIdBase {
 public:
  int contextId() const { return m_injectedScriptId; }

 protected:
  RemoteObjectIdBase();
  ~RemoteObjectIdBase() {}

  std::unique_ptr<protocol::DictionaryValue> parseInjectedScriptId(
      const String16&);

  int m_injectedScriptId;
};

class RemoteObjectId final : public RemoteObjectIdBase {
 public:
  static Response parse(const String16&, std::unique_ptr<RemoteObjectId>*);
  ~RemoteObjectId() {}
  int id() const { return m_id; }

 private:
  RemoteObjectId();

  int m_id;
};

class RemoteCallFrameId final : public RemoteObjectIdBase {
 public:
  static Response parse(const String16&, std::unique_ptr<RemoteCallFrameId>*);
  ~RemoteCallFrameId() {}

  int frameOrdinal() const { return m_frameOrdinal; }

  static String16 serialize(int injectedScriptId, int frameOrdinal);

 private:
  RemoteCallFrameId();

  int m_frameOrdinal;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_REMOTE_OBJECT_ID_H_
