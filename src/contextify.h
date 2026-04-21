#ifndef CONTEXTIFY_H
#define CONTEXTIFY_H

#include <v8.h>
#include <nan.h>

class ContextifyContext {
public:
    static void Initialize(v8::Local<v8::Object> target);
    static v8::Local<v8::Context> CreateContext(v8::Local<v8::Object> sandbox);
    static void PropertyDefinerCallback(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
};

#endif  // CONTEXTIFY_H