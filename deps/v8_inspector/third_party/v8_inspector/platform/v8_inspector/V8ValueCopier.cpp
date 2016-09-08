// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8ValueCopier.h"

namespace v8_inspector {

namespace {

static int kMaxDepth = 20;
static int kMaxCalls = 1000;

class V8ValueCopier {
public:
    v8::MaybeLocal<v8::Value> copy(v8::Local<v8::Value> value, int depth)
    {
        if (++m_calls > kMaxCalls || depth > kMaxDepth)
            return v8::MaybeLocal<v8::Value>();

        if (value.IsEmpty())
            return v8::MaybeLocal<v8::Value>();
        if (value->IsNull() || value->IsUndefined() || value->IsBoolean() || value->IsString() || value->IsNumber())
            return value;
        if (!value->IsObject())
            return v8::MaybeLocal<v8::Value>();
        v8::Local<v8::Object> object = value.As<v8::Object>();
        if (object->CreationContext() != m_from)
            return value;

        if (object->IsArray()) {
            v8::Local<v8::Array> array = object.As<v8::Array>();
            v8::Local<v8::Array> result = v8::Array::New(m_isolate, array->Length());
            if (!result->SetPrototype(m_to, v8::Null(m_isolate)).FromMaybe(false))
                return v8::MaybeLocal<v8::Value>();
            for (size_t i = 0; i < array->Length(); ++i) {
                v8::Local<v8::Value> item;
                if (!array->Get(m_from, i).ToLocal(&item))
                    return v8::MaybeLocal<v8::Value>();
                v8::Local<v8::Value> copied;
                if (!copy(item, depth + 1).ToLocal(&copied))
                    return v8::MaybeLocal<v8::Value>();
                if (!result->Set(m_to, i, copied).FromMaybe(false))
                    return v8::MaybeLocal<v8::Value>();
            }
            return result;
        }


        v8::Local<v8::Object> result = v8::Object::New(m_isolate);
        if (!result->SetPrototype(m_to, v8::Null(m_isolate)).FromMaybe(false))
            return v8::MaybeLocal<v8::Value>();
        v8::Local<v8::Array> properties;
        if (!object->GetOwnPropertyNames(m_from).ToLocal(&properties))
            return v8::MaybeLocal<v8::Value>();
        for (size_t i = 0; i < properties->Length(); ++i) {
            v8::Local<v8::Value> name;
            if (!properties->Get(m_from, i).ToLocal(&name) || !name->IsString())
                return v8::MaybeLocal<v8::Value>();
            v8::Local<v8::Value> property;
            if (!object->Get(m_from, name).ToLocal(&property))
                return v8::MaybeLocal<v8::Value>();
            v8::Local<v8::Value> copied;
            if (!copy(property, depth + 1).ToLocal(&copied))
                return v8::MaybeLocal<v8::Value>();
            if (!result->Set(m_to, name, copied).FromMaybe(false))
                return v8::MaybeLocal<v8::Value>();
        }
        return result;
    }

    v8::Isolate* m_isolate;
    v8::Local<v8::Context> m_from;
    v8::Local<v8::Context> m_to;
    int m_calls;
};

} // namespace

v8::MaybeLocal<v8::Value> copyValueFromDebuggerContext(v8::Isolate* isolate, v8::Local<v8::Context> debuggerContext, v8::Local<v8::Context> toContext, v8::Local<v8::Value> value)
{
    V8ValueCopier copier;
    copier.m_isolate = isolate;
    copier.m_from = debuggerContext;
    copier.m_to = toContext;
    copier.m_calls = 0;
    return copier.copy(value, 0);
}

} // namespace v8_inspector
