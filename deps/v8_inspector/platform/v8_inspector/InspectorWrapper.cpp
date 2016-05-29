// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/InspectorWrapper.h"

#include "platform/v8_inspector/V8Compat.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "wtf/Assertions.h"

#include <v8-debug.h>

namespace blink {

v8::Local<v8::FunctionTemplate> InspectorWrapperBase::createWrapperTemplate(v8::Isolate* isolate, const char* className, const protocol::Vector<V8MethodConfiguration>& methods, const protocol::Vector<V8AttributeConfiguration>& attributes)
{
    v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate);

    functionTemplate->SetClassName(v8::String::NewFromUtf8(isolate, className, v8::NewStringType::kInternalized).ToLocalChecked());
    v8::Local<v8::ObjectTemplate> instanceTemplate = functionTemplate->InstanceTemplate();

    for (auto& config : attributes) {
        v8::Local<v8::Name> v8name = v8::String::NewFromUtf8(isolate, config.name, v8::NewStringType::kInternalized).ToLocalChecked();
        instanceTemplate->SetAccessor(v8name, config.callback);
    }

    for (auto& config : methods) {
        v8::Local<v8::Name> v8name = v8::String::NewFromUtf8(isolate, config.name, v8::NewStringType::kInternalized).ToLocalChecked();
        v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate, config.callback);
        functionTemplate->RemovePrototype();
        instanceTemplate->Set(v8name, functionTemplate, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::DontEnum | v8::ReadOnly));
    }

    return functionTemplate;
}

v8::Local<v8::Object> InspectorWrapperBase::createWrapper(v8::Local<v8::FunctionTemplate> constructorTemplate, v8::Local<v8::Context> context)
{
    v8::MicrotasksScope microtasks(context->GetIsolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Function> function;
    if (!constructorTemplate->GetFunction(context).ToLocal(&function))
        return v8::Local<v8::Object>();

    v8::Local<v8::Object> result;
    if (!function->NewInstance(context).ToLocal(&result))
        return v8::Local<v8::Object>();
    return result;
}

void* InspectorWrapperBase::unwrap(v8::Local<v8::Context> context, v8::Local<v8::Object> object, const char* name)
{
    v8::Isolate* isolate = context->GetIsolate();
    DCHECK(context != v8::Debug::GetDebugContext(isolate));

    v8::Local<v8::Private> privateKey = v8::Private::ForApi(isolate, v8::String::NewFromUtf8(isolate, name, v8::NewStringType::kInternalized).ToLocalChecked());

    v8::Local<v8::Value> value;
    if (!object->GetPrivate(context, privateKey).ToLocal(&value))
        return nullptr;
    if (!value->IsExternal())
        return nullptr;
    return value.As<v8::External>()->Value();
}

v8::Local<v8::String> InspectorWrapperBase::v8InternalizedString(v8::Isolate* isolate, const char* name)
{
    return v8::String::NewFromUtf8(isolate, name, v8::NewStringType::kInternalized).ToLocalChecked();
}

} // namespace blink
