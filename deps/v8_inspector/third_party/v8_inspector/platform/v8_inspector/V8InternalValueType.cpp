// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8InternalValueType.h"

#include "platform/v8_inspector/V8StringUtil.h"

namespace v8_inspector {

namespace {

v8::Local<v8::Private> internalSubtypePrivate(v8::Isolate* isolate)
{
    return v8::Private::ForApi(isolate, toV8StringInternalized(isolate, "V8InternalType#internalSubtype"));
}

v8::Local<v8::String> subtypeForInternalType(v8::Isolate* isolate, V8InternalValueType type)
{
    switch (type) {
    case V8InternalValueType::kEntry:
        return toV8StringInternalized(isolate, "internal#entry");
    case V8InternalValueType::kLocation:
        return toV8StringInternalized(isolate, "internal#location");
    case V8InternalValueType::kScope:
        return toV8StringInternalized(isolate, "internal#scope");
    case V8InternalValueType::kScopeList:
        return toV8StringInternalized(isolate, "internal#scopeList");
    }
    NOTREACHED();
    return v8::Local<v8::String>();
}

} // namespace

bool markAsInternal(v8::Local<v8::Context> context, v8::Local<v8::Object> object, V8InternalValueType type)
{
    v8::Isolate* isolate = context->GetIsolate();
    v8::Local<v8::Private> privateValue = internalSubtypePrivate(isolate);
    v8::Local<v8::String> subtype = subtypeForInternalType(isolate, type);
    return object->SetPrivate(context, privateValue, subtype).FromMaybe(false);
}

bool markArrayEntriesAsInternal(v8::Local<v8::Context> context, v8::Local<v8::Array> array, V8InternalValueType type)
{
    v8::Isolate* isolate = context->GetIsolate();
    v8::Local<v8::Private> privateValue = internalSubtypePrivate(isolate);
    v8::Local<v8::String> subtype = subtypeForInternalType(isolate, type);
    for (size_t i = 0; i < array->Length(); ++i) {
        v8::Local<v8::Value> entry;
        if (!array->Get(context, i).ToLocal(&entry) || !entry->IsObject())
            return false;
        if (!entry.As<v8::Object>()->SetPrivate(context, privateValue, subtype).FromMaybe(false))
            return false;
    }
    return true;
}

v8::Local<v8::Value> v8InternalValueTypeFrom(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    v8::Isolate* isolate = context->GetIsolate();
    v8::Local<v8::Private> privateValue = internalSubtypePrivate(isolate);
    if (!object->HasPrivate(context, privateValue).FromMaybe(false))
        return v8::Null(isolate);
    v8::Local<v8::Value> subtypeValue;
    if (!object->GetPrivate(context, privateValue).ToLocal(&subtypeValue) || !subtypeValue->IsString())
        return v8::Null(isolate);
    return subtypeValue;
}

} // namespace v8_inspector
