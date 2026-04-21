#include "contextify.h"
#include <string>

void ContextifyContext::Initialize(v8::Local<v8::Object> target) {
    v8::Local<v8::FunctionTemplate> t = Nan::New<v8::FunctionTemplate>(Nan::New<v8::FunctionTemplate>([](const v8::FunctionCallbackInfo<v8::Value>& args) {
        if (args.IsConstructCall()) {
            ContextifyContext* context = new ContextifyContext();
            context->Wrap(args.This());
            Nan::SetPrototypeMethod(args.Callee(), "createContext", createContext);
            Nan::SetPrototypeMethod(args.Callee(), "definePropertyOnContext", definePropertyOnContext);
        }
    }));

    v8::Local<v8::FunctionTemplate> proto = Nan::New<v8::FunctionTemplate>([](const v8::FunctionCallbackInfo<v8::Value>& args) {
        if (args.IsConstructCall()) {
            ContextifyContext* context = new ContextifyContext();
            context->Wrap(args.This());
            Nan::SetPrototypeMethod(args.Callee(), "createContext", createContext);
            Nan::SetPrototypeMethod(args.Callee(), "definePropertyOnContext", definePropertyOnContext);
        }
    });

    Nan::Set(target, Nan::New("ContextifyContext").ToLocalChecked(), t->GetFunction());
}

v8::Local<v8::Context> ContextifyContext::CreateContext(v8::Local<v8::Object> sandbox) {
    v8::Local<v8::Context> context = v8::Context::New(Nan::GetCurrentContext()->GetIsolate());
    v8::Local<v8::Object> global = context->Global();

    // Copy properties from sandbox to context
    v8::Local<v8::Array> props = sandbox->GetPropertyNames();
    for (uint32_t i = 0; i < props->Length(); i++) {
        v8::Local<v8::Value> prop = props->Get(i);
        v8::Local<v8::Value> value = sandbox->Get(prop);
        context->Global()->Set(context, prop, value);
    }

    return context;
}

void ContextifyContext::PropertyDefinerCallback(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> global = context->Global();

    // Check if property exists in global
    if (global->Has(context, property).FromJust()) {
        v8::Local<v8::Value> value = global->Get(context, property).ToLocalChecked();
        info.GetReturnValue().Set(value);
    } else {
        // Intercept the request and return undefined
        info.GetReturnValue().SetUndefined();
    }
}