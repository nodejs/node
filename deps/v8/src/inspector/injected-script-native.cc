// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/injected-script-native.h"

namespace v8_inspector {

InjectedScriptNative::InjectedScriptNative(v8::Isolate* isolate)
    : m_lastBoundObjectId(1), m_isolate(isolate) {}

static const char privateKeyName[] = "v8-inspector#injectedScript";

InjectedScriptNative::~InjectedScriptNative() {}

void InjectedScriptNative::setOnInjectedScriptHost(
    v8::Local<v8::Object> injectedScriptHost) {
  v8::HandleScope handleScope(m_isolate);
  v8::Local<v8::External> external = v8::External::New(m_isolate, this);
  v8::Local<v8::Private> privateKey = v8::Private::ForApi(
      m_isolate, v8::String::NewFromUtf8(m_isolate, privateKeyName,
                                         v8::NewStringType::kInternalized)
                     .ToLocalChecked());
  injectedScriptHost->SetPrivate(m_isolate->GetCurrentContext(), privateKey,
                                 external);
}

InjectedScriptNative* InjectedScriptNative::fromInjectedScriptHost(
    v8::Isolate* isolate, v8::Local<v8::Object> injectedScriptObject) {
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Private> privateKey = v8::Private::ForApi(
      isolate, v8::String::NewFromUtf8(isolate, privateKeyName,
                                       v8::NewStringType::kInternalized)
                   .ToLocalChecked());
  v8::Local<v8::Value> value =
      injectedScriptObject->GetPrivate(context, privateKey).ToLocalChecked();
  DCHECK(value->IsExternal());
  v8::Local<v8::External> external = value.As<v8::External>();
  return static_cast<InjectedScriptNative*>(external->Value());
}

int InjectedScriptNative::bind(v8::Local<v8::Value> value,
                               const String16& groupName) {
  if (m_lastBoundObjectId <= 0) m_lastBoundObjectId = 1;
  int id = m_lastBoundObjectId++;
  m_idToWrappedObject.insert(
      std::make_pair(id, v8::Global<v8::Value>(m_isolate, value)));
  addObjectToGroup(id, groupName);
  return id;
}

void InjectedScriptNative::unbind(int id) {
  m_idToWrappedObject.erase(id);
  m_idToObjectGroupName.erase(id);
}

v8::Local<v8::Value> InjectedScriptNative::objectForId(int id) {
  auto iter = m_idToWrappedObject.find(id);
  return iter != m_idToWrappedObject.end() ? iter->second.Get(m_isolate)
                                           : v8::Local<v8::Value>();
}

void InjectedScriptNative::addObjectToGroup(int objectId,
                                            const String16& groupName) {
  if (groupName.isEmpty()) return;
  if (objectId <= 0) return;
  m_idToObjectGroupName[objectId] = groupName;
  m_nameToObjectGroup[groupName].push_back(
      objectId);  // Creates an empty vector if key is not there
}

void InjectedScriptNative::releaseObjectGroup(const String16& groupName) {
  if (groupName.isEmpty()) return;
  NameToObjectGroup::iterator groupIt = m_nameToObjectGroup.find(groupName);
  if (groupIt == m_nameToObjectGroup.end()) return;
  for (int id : groupIt->second) unbind(id);
  m_nameToObjectGroup.erase(groupIt);
}

String16 InjectedScriptNative::groupName(int objectId) const {
  if (objectId <= 0) return String16();
  IdToObjectGroupName::const_iterator iterator =
      m_idToObjectGroupName.find(objectId);
  return iterator != m_idToObjectGroupName.end() ? iterator->second
                                                 : String16();
}

}  // namespace v8_inspector
