// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_INJECTEDSCRIPTNATIVE_H_
#define V8_INSPECTOR_INJECTEDSCRIPTNATIVE_H_

#include <vector>

#include "src/inspector/protocol/Protocol.h"

#include "include/v8.h"

namespace v8_inspector {

class InjectedScriptNative final {
 public:
  explicit InjectedScriptNative(v8::Isolate*);
  ~InjectedScriptNative();

  void setOnInjectedScriptHost(v8::Local<v8::Object>);
  static InjectedScriptNative* fromInjectedScriptHost(v8::Isolate* isolate,
                                                      v8::Local<v8::Object>);

  int bind(v8::Local<v8::Value>, const String16& groupName);
  void unbind(int id);
  v8::Local<v8::Value> objectForId(int id);

  void releaseObjectGroup(const String16& groupName);
  String16 groupName(int objectId) const;

 private:
  void addObjectToGroup(int objectId, const String16& groupName);

  int m_lastBoundObjectId;
  v8::Isolate* m_isolate;
  protocol::HashMap<int, v8::Global<v8::Value>> m_idToWrappedObject;
  typedef protocol::HashMap<int, String16> IdToObjectGroupName;
  IdToObjectGroupName m_idToObjectGroupName;
  typedef protocol::HashMap<String16, std::vector<int>> NameToObjectGroup;
  NameToObjectGroup m_nameToObjectGroup;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_INJECTEDSCRIPTNATIVE_H_
