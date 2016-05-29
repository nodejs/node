// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorWrapper_h
#define InspectorWrapper_h

#include "platform/inspector_protocol/Collections.h"
#include <v8.h>

namespace blink {

class InspectorWrapperBase {
public:
    struct V8MethodConfiguration {
        const char* name;
        v8::FunctionCallback callback;
    };

    struct V8AttributeConfiguration {
        const char* name;
        v8::AccessorNameGetterCallback callback;
    };

    static v8::Local<v8::FunctionTemplate> createWrapperTemplate(v8::Isolate*, const char* className, const protocol::Vector<V8MethodConfiguration>& methods, const protocol::Vector<V8AttributeConfiguration>& attributes);

protected:
    static v8::Local<v8::Object> createWrapper(v8::Local<v8::FunctionTemplate>, v8::Local<v8::Context>);
    static void* unwrap(v8::Local<v8::Context>, v8::Local<v8::Object>, const char* name);

    static v8::Local<v8::String> v8InternalizedString(v8::Isolate*, const char* name);
};

template<class T, char* const hiddenPropertyName, char* const className>
class InspectorWrapper final : public InspectorWrapperBase {
public:
    class WeakCallbackData final {
    public:
        WeakCallbackData(v8::Isolate* isolate, T* impl, v8::Local<v8::Object> wrapper)
            : m_impl(impl)
            , m_persistent(isolate, wrapper)
        {
            m_persistent.SetWeak(this, &WeakCallbackData::weakCallback, v8::WeakCallbackType::kParameter);
        }

        T* m_impl;
        std::unique_ptr<T> m_implOwn;

    private:
        static void weakCallback(const v8::WeakCallbackInfo<WeakCallbackData>& info)
        {
            delete info.GetParameter();
        }

        v8::Global<v8::Object> m_persistent;
    };

    static v8::Local<v8::FunctionTemplate> createWrapperTemplate(v8::Isolate* isolate, const protocol::Vector<V8MethodConfiguration>& methods, const protocol::Vector<V8AttributeConfiguration>& attributes)
    {
        return InspectorWrapperBase::createWrapperTemplate(isolate, className, methods, attributes);
    }

    static v8::Local<v8::Object> wrap(v8::Local<v8::FunctionTemplate> constructorTemplate, v8::Local<v8::Context> context, T* object)
    {
        v8::Context::Scope contextScope(context);
        v8::Local<v8::Object> result = InspectorWrapperBase::createWrapper(constructorTemplate, context);
        if (result.IsEmpty())
            return v8::Local<v8::Object>();
        v8::Isolate* isolate = context->GetIsolate();
        v8::Local<v8::External> objectReference = v8::External::New(isolate, new WeakCallbackData(isolate, object, result));

        v8::Local<v8::Private> privateKey = v8::Private::ForApi(isolate, v8::String::NewFromUtf8(isolate, hiddenPropertyName, v8::NewStringType::kInternalized).ToLocalChecked());
        result->SetPrivate(context, privateKey, objectReference);
        return result;
    }

    static T* unwrap(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
    {
        void* data = InspectorWrapperBase::unwrap(context, object, hiddenPropertyName);
        if (!data)
            return nullptr;
        return reinterpret_cast<WeakCallbackData*>(data)->m_impl;
    }
};

} // namespace blink

#endif // InspectorWrapper_h
