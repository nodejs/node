// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteObjectId_h
#define RemoteObjectId_h

#include "platform/inspector_protocol/InspectorProtocol.h"

namespace v8_inspector {

namespace protocol = blink::protocol;

class RemoteObjectIdBase {
public:
    int contextId() const { return m_injectedScriptId; }

protected:
    RemoteObjectIdBase();
    ~RemoteObjectIdBase() { }

    std::unique_ptr<protocol::DictionaryValue> parseInjectedScriptId(const String16&);

    int m_injectedScriptId;
};

class RemoteObjectId final : public RemoteObjectIdBase {
public:
    static std::unique_ptr<RemoteObjectId> parse(ErrorString*, const String16&);
    ~RemoteObjectId() { }
    int id() const { return m_id; }

private:
    RemoteObjectId();

    int m_id;
};

class RemoteCallFrameId final : public RemoteObjectIdBase {
public:
    static std::unique_ptr<RemoteCallFrameId> parse(ErrorString*, const String16&);
    ~RemoteCallFrameId() { }

    int frameOrdinal() const { return m_frameOrdinal; }

    static String16 serialize(int injectedScriptId, int frameOrdinal);
private:
    RemoteCallFrameId();

    int m_frameOrdinal;
};

} // namespace v8_inspector

#endif // !defined(RemoteObjectId_h)
