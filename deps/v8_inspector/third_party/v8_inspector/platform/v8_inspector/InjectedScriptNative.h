// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InjectedScriptNative_h
#define InjectedScriptNative_h

#include "platform/inspector_protocol/InspectorProtocol.h"
#include <v8.h>

#include <vector>

namespace v8_inspector {

namespace protocol = blink::protocol;

class InjectedScriptNative final {
public:
    explicit InjectedScriptNative(v8::Isolate*);
    ~InjectedScriptNative();

    void setOnInjectedScriptHost(v8::Local<v8::Object>);
    static InjectedScriptNative* fromInjectedScriptHost(v8::Local<v8::Object>);

    int bind(v8::Local<v8::Value>, const String16& groupName);
    void unbind(int id);
    v8::Local<v8::Value> objectForId(int id);

    void releaseObjectGroup(const String16& groupName);
    String16 groupName(int objectId) const;

private:
    void addObjectToGroup(int objectId, const String16& groupName);

    int m_lastBoundObjectId;
    v8::Isolate* m_isolate;
    protocol::HashMap<int, std::unique_ptr<v8::Global<v8::Value>>> m_idToWrappedObject;
    typedef protocol::HashMap<int, String16> IdToObjectGroupName;
    IdToObjectGroupName m_idToObjectGroupName;
    typedef protocol::HashMap<String16, std::vector<int>> NameToObjectGroup;
    NameToObjectGroup m_nameToObjectGroup;
};

} // namespace v8_inspector

#endif
