// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "test/cctest/cctest.h"

namespace {

int32_t g_cross_context_int = 0;

bool g_expect_interceptor_call = false;

void NamedGetter(v8::Local<v8::Name> property,
                 const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (property->Equals(context, v8_str("cross_context_int")).FromJust())
    info.GetReturnValue().Set(g_cross_context_int);
}

void NamedSetter(v8::Local<v8::Name> property, v8::Local<v8::Value> value,
                 const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!property->Equals(context, v8_str("cross_context_int")).FromJust())
    return;
  if (value->IsInt32()) {
    g_cross_context_int = value->ToInt32(context).ToLocalChecked()->Value();
  }
  info.GetReturnValue().Set(value);
}

void NamedQuery(v8::Local<v8::Name> property,
                const v8::PropertyCallbackInfo<v8::Integer>& info) {
  CHECK(g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!property->Equals(context, v8_str("cross_context_int")).FromJust())
    return;
  info.GetReturnValue().Set(v8::DontDelete);
}

void NamedDeleter(v8::Local<v8::Name> property,
                  const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  CHECK(g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!property->Equals(context, v8_str("cross_context_int")).FromJust())
    return;
  info.GetReturnValue().Set(false);
}

void NamedEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  CHECK(g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> names = v8::Array::New(isolate, 1);
  names->Set(context, 0, v8_str("cross_context_int")).FromJust();
  info.GetReturnValue().Set(names);
}

void IndexedGetter(uint32_t index,
                   const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(g_expect_interceptor_call);
  if (index == 7) info.GetReturnValue().Set(g_cross_context_int);
}

void IndexedSetter(uint32_t index, v8::Local<v8::Value> value,
                   const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (index != 7) return;
  if (value->IsInt32()) {
    g_cross_context_int = value->ToInt32(context).ToLocalChecked()->Value();
  }
  info.GetReturnValue().Set(value);
}

void IndexedQuery(uint32_t index,
                  const v8::PropertyCallbackInfo<v8::Integer>& info) {
  CHECK(g_expect_interceptor_call);
  if (index == 7) info.GetReturnValue().Set(v8::DontDelete);
}

void IndexedDeleter(uint32_t index,
                    const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  CHECK(g_expect_interceptor_call);
  if (index == 7) info.GetReturnValue().Set(false);
}

void IndexedEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  CHECK(g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> names = v8::Array::New(isolate, 1);
  names->Set(context, 0, v8_str("7")).FromJust();
  info.GetReturnValue().Set(names);
}

void MethodGetter(v8::Local<v8::Name> property,
                  const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::External> data = info.Data().As<v8::External>();
  v8::Local<v8::FunctionTemplate>& function_template =
      *reinterpret_cast<v8::Local<v8::FunctionTemplate>*>(data->Value());

  info.GetReturnValue().Set(
      function_template->GetFunction(context).ToLocalChecked());
}

void MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(8);
}

void NamedGetterThrowsException(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->ThrowException(v8_str("exception"));
}

void NamedSetterThrowsException(
    v8::Local<v8::Name> property, v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->ThrowException(v8_str("exception"));
}

void IndexedGetterThrowsException(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->ThrowException(v8_str("exception"));
}

void IndexedSetterThrowsException(
    uint32_t index, v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->ThrowException(v8_str("exception"));
}

bool AccessCheck(v8::Local<v8::Context> accessing_context,
                 v8::Local<v8::Object> accessed_object,
                 v8::Local<v8::Value> data) {
  return false;
}

void GetCrossContextInt(v8::Local<v8::String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(!g_expect_interceptor_call);
  info.GetReturnValue().Set(g_cross_context_int);
}

void SetCrossContextInt(v8::Local<v8::String> property,
                        v8::Local<v8::Value> value,
                        const v8::PropertyCallbackInfo<void>& info) {
  CHECK(!g_expect_interceptor_call);
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (value->IsInt32()) {
    g_cross_context_int = value->ToInt32(context).ToLocalChecked()->Value();
  }
}

void Return42(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(42);
}

void CheckCanRunScriptInContext(v8::Isolate* isolate,
                                v8::Local<v8::Context> context) {
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);

  g_expect_interceptor_call = false;
  g_cross_context_int = 0;

  // Running script in this context should work.
  CompileRunChecked(isolate, "this.foo = 42; this[23] = true;");
  ExpectInt32("this.all_can_read", 42);
  CompileRunChecked(isolate, "this.cross_context_int = 23");
  CHECK_EQ(g_cross_context_int, 23);
  ExpectInt32("this.cross_context_int", 23);
}

void CheckCrossContextAccess(v8::Isolate* isolate,
                             v8::Local<v8::Context> accessing_context,
                             v8::Local<v8::Object> accessed_object) {
  v8::HandleScope handle_scope(isolate);
  accessing_context->Global()
      ->Set(accessing_context, v8_str("other"), accessed_object)
      .FromJust();
  v8::Context::Scope context_scope(accessing_context);

  g_expect_interceptor_call = true;
  g_cross_context_int = 23;

  {
    v8::TryCatch try_catch(isolate);
    CHECK(CompileRun(accessing_context, "this.other.foo").IsEmpty());
  }
  {
    v8::TryCatch try_catch(isolate);
    CHECK(CompileRun(accessing_context, "this.other[23]").IsEmpty());
  }

  // AllCanRead properties are also inaccessible.
  {
    v8::TryCatch try_catch(isolate);
    CHECK(CompileRun(accessing_context, "this.other.all_can_read").IsEmpty());
  }

  // Intercepted properties are accessible, however.
  ExpectInt32("this.other.cross_context_int", 23);
  CompileRunChecked(isolate, "this.other.cross_context_int = 42");
  ExpectInt32("this.other[7]", 42);
  ExpectString("JSON.stringify(Object.getOwnPropertyNames(this.other))",
               "[\"7\",\"cross_context_int\"]");
}

void CheckCrossContextAccessWithException(
    v8::Isolate* isolate, v8::Local<v8::Context> accessing_context,
    v8::Local<v8::Object> accessed_object) {
  v8::HandleScope handle_scope(isolate);
  accessing_context->Global()
      ->Set(accessing_context, v8_str("other"), accessed_object)
      .FromJust();
  v8::Context::Scope context_scope(accessing_context);

  {
    v8::TryCatch try_catch(isolate);
    CompileRun("this.other.should_throw");
    CHECK(try_catch.HasCaught());
    CHECK(try_catch.Exception()->IsString());
    CHECK(v8_str("exception")
              ->Equals(accessing_context, try_catch.Exception())
              .FromJust());
  }

  {
    v8::TryCatch try_catch(isolate);
    CompileRun("this.other.should_throw = 8");
    CHECK(try_catch.HasCaught());
    CHECK(try_catch.Exception()->IsString());
    CHECK(v8_str("exception")
              ->Equals(accessing_context, try_catch.Exception())
              .FromJust());
  }

  {
    v8::TryCatch try_catch(isolate);
    CompileRun("this.other[42]");
    CHECK(try_catch.HasCaught());
    CHECK(try_catch.Exception()->IsString());
    CHECK(v8_str("exception")
              ->Equals(accessing_context, try_catch.Exception())
              .FromJust());
  }

  {
    v8::TryCatch try_catch(isolate);
    CompileRun("this.other[42] = 8");
    CHECK(try_catch.HasCaught());
    CHECK(try_catch.Exception()->IsString());
    CHECK(v8_str("exception")
              ->Equals(accessing_context, try_catch.Exception())
              .FromJust());
  }
}

void Ctor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info.IsConstructCall());
}

}  // namespace

TEST(AccessCheckWithInterceptor) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetAccessCheckCallbackAndHandler(
      AccessCheck,
      v8::NamedPropertyHandlerConfiguration(
          NamedGetter, NamedSetter, NamedQuery, NamedDeleter, NamedEnumerator),
      v8::IndexedPropertyHandlerConfiguration(IndexedGetter, IndexedSetter,
                                              IndexedQuery, IndexedDeleter,
                                              IndexedEnumerator));
  global_template->SetNativeDataProperty(
      v8_str("cross_context_int"), GetCrossContextInt, SetCrossContextInt);
  global_template->SetNativeDataProperty(
      v8_str("all_can_read"), Return42, nullptr, v8::Local<v8::Value>(),
      v8::None, v8::Local<v8::AccessorSignature>(), v8::ALL_CAN_READ);

  v8::Local<v8::Context> context0 =
      v8::Context::New(isolate, nullptr, global_template);
  CheckCanRunScriptInContext(isolate, context0);

  // Create another context.
  v8::Local<v8::Context> context1 =
      v8::Context::New(isolate, nullptr, global_template);
  CheckCrossContextAccess(isolate, context1, context0->Global());
}

TEST(CallFunctionWithRemoteContextReceiver) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::FunctionTemplate> global_template =
      v8::FunctionTemplate::New(isolate);

  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, global_template);
  v8::Local<v8::FunctionTemplate> function_template = v8::FunctionTemplate::New(
      isolate, MethodCallback, v8::External::New(isolate, &function_template),
      signature);

  global_template->InstanceTemplate()->SetAccessCheckCallbackAndHandler(
      AccessCheck, v8::NamedPropertyHandlerConfiguration(
                       MethodGetter, nullptr, nullptr, nullptr, nullptr,
                       v8::External::New(isolate, &function_template)),
      v8::IndexedPropertyHandlerConfiguration());

  v8::Local<v8::Object> accessed_object =
      v8::Context::NewRemoteContext(isolate,
                                    global_template->InstanceTemplate())
          .ToLocalChecked();
  v8::Local<v8::Context> accessing_context =
      v8::Context::New(isolate, nullptr, global_template->InstanceTemplate());

  v8::HandleScope handle_scope(isolate);
  accessing_context->Global()
      ->Set(accessing_context, v8_str("other"), accessed_object)
      .FromJust();
  v8::Context::Scope context_scope(accessing_context);

  {
    v8::TryCatch try_catch(isolate);
    ExpectInt32("this.other.method()", 8);
    CHECK(!try_catch.HasCaught());
  }
}

TEST(AccessCheckWithExceptionThrowingInterceptor) {
  v8::Isolate* isolate = CcTest::isolate();
  isolate->SetFailedAccessCheckCallbackFunction([](v8::Local<v8::Object> target,
                                                   v8::AccessType type,
                                                   v8::Local<v8::Value> data) {
    CHECK(false);  // This should never be called.
  });

  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetAccessCheckCallbackAndHandler(
      AccessCheck, v8::NamedPropertyHandlerConfiguration(
                       NamedGetterThrowsException, NamedSetterThrowsException),
      v8::IndexedPropertyHandlerConfiguration(IndexedGetterThrowsException,
                                              IndexedSetterThrowsException));

  // Create two contexts.
  v8::Local<v8::Context> context0 =
      v8::Context::New(isolate, nullptr, global_template);
  v8::Local<v8::Context> context1 =
      v8::Context::New(isolate, nullptr, global_template);

  CheckCrossContextAccessWithException(isolate, context1, context0->Global());
}

TEST(NewRemoteContext) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetAccessCheckCallbackAndHandler(
      AccessCheck,
      v8::NamedPropertyHandlerConfiguration(
          NamedGetter, NamedSetter, NamedQuery, NamedDeleter, NamedEnumerator),
      v8::IndexedPropertyHandlerConfiguration(IndexedGetter, IndexedSetter,
                                              IndexedQuery, IndexedDeleter,
                                              IndexedEnumerator));
  global_template->SetNativeDataProperty(
      v8_str("cross_context_int"), GetCrossContextInt, SetCrossContextInt);
  global_template->SetNativeDataProperty(
      v8_str("all_can_read"), Return42, nullptr, v8::Local<v8::Value>(),
      v8::None, v8::Local<v8::AccessorSignature>(), v8::ALL_CAN_READ);

  v8::Local<v8::Object> global0 =
      v8::Context::NewRemoteContext(isolate, global_template).ToLocalChecked();

  // Create a real context.
  {
    v8::HandleScope other_scope(isolate);
    v8::Local<v8::Context> context1 =
        v8::Context::New(isolate, nullptr, global_template);

    CheckCrossContextAccess(isolate, context1, global0);
  }

  // Create a context using the detached global.
  {
    v8::HandleScope other_scope(isolate);
    v8::Local<v8::Context> context2 =
        v8::Context::New(isolate, nullptr, global_template, global0);

    CheckCanRunScriptInContext(isolate, context2);
  }

  // Turn a regular context into a remote context.
  {
    v8::HandleScope other_scope(isolate);
    v8::Local<v8::Context> context3 =
        v8::Context::New(isolate, nullptr, global_template);

    CheckCanRunScriptInContext(isolate, context3);

    // Turn the global object into a remote context, and try to access it.
    v8::Local<v8::Object> context3_global = context3->Global();
    context3->DetachGlobal();
    v8::Local<v8::Object> global3 =
        v8::Context::NewRemoteContext(isolate, global_template, context3_global)
            .ToLocalChecked();
    v8::Local<v8::Context> context4 =
        v8::Context::New(isolate, nullptr, global_template);

    CheckCrossContextAccess(isolate, context4, global3);

    // Turn it back into a regular context.
    v8::Local<v8::Context> context5 =
        v8::Context::New(isolate, nullptr, global_template, global3);

    CheckCanRunScriptInContext(isolate, context5);
  }
}

TEST(NewRemoteInstance) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::FunctionTemplate> tmpl =
      v8::FunctionTemplate::New(isolate, Ctor);
  v8::Local<v8::ObjectTemplate> instance = tmpl->InstanceTemplate();
  instance->SetAccessCheckCallbackAndHandler(
      AccessCheck,
      v8::NamedPropertyHandlerConfiguration(
          NamedGetter, NamedSetter, NamedQuery, NamedDeleter, NamedEnumerator),
      v8::IndexedPropertyHandlerConfiguration(IndexedGetter, IndexedSetter,
                                              IndexedQuery, IndexedDeleter,
                                              IndexedEnumerator));
  tmpl->SetNativeDataProperty(
      v8_str("all_can_read"), Return42, nullptr, v8::Local<v8::Value>(),
      v8::None, v8::Local<v8::AccessorSignature>(), v8::ALL_CAN_READ);

  v8::Local<v8::Object> obj = tmpl->NewRemoteInstance().ToLocalChecked();

  v8::Local<v8::Context> context = v8::Context::New(isolate);
  CheckCrossContextAccess(isolate, context, obj);
}
