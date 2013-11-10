// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

#include "api.h"
#include "cctest.h"
#include "frames-inl.h"
#include "string-stream.h"

using ::v8::ObjectTemplate;
using ::v8::Value;
using ::v8::Context;
using ::v8::Local;
using ::v8::String;
using ::v8::Script;
using ::v8::Function;
using ::v8::Extension;

static void handle_property(Local<String> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(v8_num(900));
}

static void handle_property_2(Local<String> name,
                              const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(v8_num(902));
}


static void handle_property(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  CHECK_EQ(0, info.Length());
  info.GetReturnValue().Set(v8_num(907));
}


THREADED_TEST(PropertyHandler) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<v8::FunctionTemplate> fun_templ = v8::FunctionTemplate::New();
  fun_templ->InstanceTemplate()->SetAccessor(v8_str("foo"), handle_property);
  Local<v8::FunctionTemplate> getter_templ =
      v8::FunctionTemplate::New(handle_property);
  getter_templ->SetLength(0);
  fun_templ->
      InstanceTemplate()->SetAccessorProperty(v8_str("bar"), getter_templ);
  fun_templ->InstanceTemplate()->
      SetNativeDataProperty(v8_str("instance_foo"), handle_property);
  fun_templ->SetNativeDataProperty(v8_str("object_foo"), handle_property_2);
  Local<Function> fun = fun_templ->GetFunction();
  env->Global()->Set(v8_str("Fun"), fun);
  Local<Script> getter;
  Local<Script> setter;
  // check function instance accessors
  getter = v8_compile("var obj = new Fun(); obj.instance_foo;");
  CHECK_EQ(900, getter->Run()->Int32Value());
  setter = v8_compile("obj.instance_foo = 901;");
  CHECK_EQ(901, setter->Run()->Int32Value());
  getter = v8_compile("obj.bar;");
  CHECK_EQ(907, getter->Run()->Int32Value());
  setter = v8_compile("obj.bar = 908;");
  CHECK_EQ(908, setter->Run()->Int32Value());
  // check function static accessors
  getter = v8_compile("Fun.object_foo;");
  CHECK_EQ(902, getter->Run()->Int32Value());
  setter = v8_compile("Fun.object_foo = 903;");
  CHECK_EQ(903, setter->Run()->Int32Value());
}


static void GetIntValue(Local<String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  int* value =
      static_cast<int*>(v8::Handle<v8::External>::Cast(info.Data())->Value());
  info.GetReturnValue().Set(v8_num(*value));
}


static void SetIntValue(Local<String> property,
                        Local<Value> value,
                        const v8::PropertyCallbackInfo<void>& info) {
  int* field =
      static_cast<int*>(v8::Handle<v8::External>::Cast(info.Data())->Value());
  *field = value->Int32Value();
}

int foo, bar, baz;

THREADED_TEST(GlobalVariableAccess) {
  foo = 0;
  bar = -4;
  baz = 10;
  v8::HandleScope scope(CcTest::isolate());
  v8::Handle<v8::FunctionTemplate> templ = v8::FunctionTemplate::New();
  templ->InstanceTemplate()->SetAccessor(v8_str("foo"),
                                         GetIntValue,
                                         SetIntValue,
                                         v8::External::New(&foo));
  templ->InstanceTemplate()->SetAccessor(v8_str("bar"),
                                         GetIntValue,
                                         SetIntValue,
                                         v8::External::New(&bar));
  templ->InstanceTemplate()->SetAccessor(v8_str("baz"),
                                         GetIntValue,
                                         SetIntValue,
                                         v8::External::New(&baz));
  LocalContext env(0, templ->InstanceTemplate());
  v8_compile("foo = (++bar) + baz")->Run();
  CHECK_EQ(bar, -3);
  CHECK_EQ(foo, 7);
}


static int x_register[2] = {0, 0};
static v8::Handle<v8::Object> x_receiver;
static v8::Handle<v8::Object> x_holder;

template<class Info>
static void XGetter(const Info& info, int offset) {
  ApiTestFuzzer::Fuzz();
  v8::Isolate* isolate = CcTest::isolate();
  CHECK_EQ(isolate, info.GetIsolate());
  CHECK_EQ(x_receiver, info.This());
  info.GetReturnValue().Set(v8_num(x_register[offset]));
}


static void XGetter(Local<String> name,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK_EQ(x_holder, info.Holder());
  XGetter(info, 0);
}


static void XGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK_EQ(x_receiver, info.Holder());
  XGetter(info, 1);
}


template<class Info>
static void XSetter(Local<Value> value, const Info& info, int offset) {
  v8::Isolate* isolate = CcTest::isolate();
  CHECK_EQ(isolate, info.GetIsolate());
  CHECK_EQ(x_holder, info.This());
  CHECK_EQ(x_holder, info.Holder());
  x_register[offset] = value->Int32Value();
}


static void XSetter(Local<String> name,
                    Local<Value> value,
                    const v8::PropertyCallbackInfo<void>& info) {
  XSetter(value, info, 0);
}


static void XSetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK_EQ(1, info.Length());
  XSetter(info[0], info, 1);
}


THREADED_TEST(AccessorIC) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("x0"), XGetter, XSetter);
  obj->SetAccessorProperty(v8_str("x1"),
                           v8::FunctionTemplate::New(XGetter),
                           v8::FunctionTemplate::New(XSetter));
  x_holder = obj->NewInstance();
  context->Global()->Set(v8_str("holder"), x_holder);
  x_receiver = v8::Object::New();
  context->Global()->Set(v8_str("obj"), x_receiver);
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(CompileRun(
    "obj.__proto__ = holder;"
    "var result = [];"
    "var key_0 = 'x0';"
    "var key_1 = 'x1';"
    "for (var i = 0; i < 10; i++) {"
    "  holder.x0 = i;"
    "  result.push(obj.x0);"
    "  holder.x1 = i;"
    "  result.push(obj.x1);"
    "  holder[key_0] = i;"
    "  result.push(obj[key_0]);"
    "  holder[key_1] = i;"
    "  result.push(obj[key_1]);"
    "}"
    "result"));
  CHECK_EQ(40, array->Length());
  for (int i = 0; i < 40; i++) {
    v8::Handle<Value> entry = array->Get(v8::Integer::New(i));
    CHECK_EQ(v8::Integer::New(i/4), entry);
  }
}


static void AccessorProhibitsOverwritingGetter(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(true);
}


THREADED_TEST(AccessorProhibitsOverwriting) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Local<ObjectTemplate> templ = ObjectTemplate::New();
  templ->SetAccessor(v8_str("x"),
                     AccessorProhibitsOverwritingGetter,
                     0,
                     v8::Handle<Value>(),
                     v8::PROHIBITS_OVERWRITING,
                     v8::ReadOnly);
  Local<v8::Object> instance = templ->NewInstance();
  context->Global()->Set(v8_str("obj"), instance);
  Local<Value> value = CompileRun(
      "obj.__defineGetter__('x', function() { return false; });"
      "obj.x");
  CHECK(value->BooleanValue());
  value = CompileRun(
      "var setter_called = false;"
      "obj.__defineSetter__('x', function() { setter_called = true; });"
      "obj.x = 42;"
      "setter_called");
  CHECK(!value->BooleanValue());
  value = CompileRun(
      "obj2 = {};"
      "obj2.__proto__ = obj;"
      "obj2.__defineGetter__('x', function() { return false; });"
      "obj2.x");
  CHECK(value->BooleanValue());
  value = CompileRun(
      "var setter_called = false;"
      "obj2 = {};"
      "obj2.__proto__ = obj;"
      "obj2.__defineSetter__('x', function() { setter_called = true; });"
      "obj2.x = 42;"
      "setter_called");
  CHECK(!value->BooleanValue());
}


template <int C>
static void HandleAllocatingGetter(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  for (int i = 0; i < C; i++)
    v8::String::New("foo");
  info.GetReturnValue().Set(v8::String::New("foo"));
}


THREADED_TEST(HandleScopePop) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("one"), HandleAllocatingGetter<1>);
  obj->SetAccessor(v8_str("many"), HandleAllocatingGetter<1024>);
  v8::Handle<v8::Object> inst = obj->NewInstance();
  context->Global()->Set(v8::String::New("obj"), inst);
  i::Isolate* isolate = CcTest::i_isolate();
  int count_before = i::HandleScope::NumberOfHandles(isolate);
  {
    v8::HandleScope scope(context->GetIsolate());
    CompileRun(
        "for (var i = 0; i < 1000; i++) {"
        "  obj.one;"
        "  obj.many;"
        "}");
  }
  int count_after = i::HandleScope::NumberOfHandles(isolate);
  CHECK_EQ(count_before, count_after);
}

static void CheckAccessorArgsCorrect(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(info.GetIsolate() == CcTest::isolate());
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()->Equals(v8::String::New("data")));
  ApiTestFuzzer::Fuzz();
  CHECK(info.GetIsolate() == CcTest::isolate());
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()->Equals(v8::String::New("data")));
  CcTest::heap()->CollectAllGarbage(i::Heap::kNoGCFlags);
  CHECK(info.GetIsolate() == CcTest::isolate());
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()->Equals(v8::String::New("data")));
  info.GetReturnValue().Set(17);
}


THREADED_TEST(DirectCall) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("xxx"),
                   CheckAccessorArgsCorrect,
                   NULL,
                   v8::String::New("data"));
  v8::Handle<v8::Object> inst = obj->NewInstance();
  context->Global()->Set(v8::String::New("obj"), inst);
  Local<Script> scr = v8::Script::Compile(v8::String::New("obj.xxx"));
  for (int i = 0; i < 10; i++) {
    Local<Value> result = scr->Run();
    CHECK(!result.IsEmpty());
    CHECK_EQ(17, result->Int32Value());
  }
}

static void EmptyGetter(Local<String> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  CheckAccessorArgsCorrect(name, info);
  ApiTestFuzzer::Fuzz();
  CheckAccessorArgsCorrect(name, info);
  info.GetReturnValue().Set(v8::Handle<v8::Value>());
}


THREADED_TEST(EmptyResult) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("xxx"), EmptyGetter, NULL, v8::String::New("data"));
  v8::Handle<v8::Object> inst = obj->NewInstance();
  context->Global()->Set(v8::String::New("obj"), inst);
  Local<Script> scr = v8::Script::Compile(v8::String::New("obj.xxx"));
  for (int i = 0; i < 10; i++) {
    Local<Value> result = scr->Run();
    CHECK(result == v8::Undefined(isolate));
  }
}


THREADED_TEST(NoReuseRegress) {
  // Check that the IC generated for the one test doesn't get reused
  // for the other.
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  {
    v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
    obj->SetAccessor(v8_str("xxx"), EmptyGetter, NULL, v8::String::New("data"));
    LocalContext context;
    v8::Handle<v8::Object> inst = obj->NewInstance();
    context->Global()->Set(v8::String::New("obj"), inst);
    Local<Script> scr = v8::Script::Compile(v8::String::New("obj.xxx"));
    for (int i = 0; i < 2; i++) {
      Local<Value> result = scr->Run();
      CHECK(result == v8::Undefined(isolate));
    }
  }
  {
    v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
    obj->SetAccessor(v8_str("xxx"),
                     CheckAccessorArgsCorrect,
                     NULL,
                     v8::String::New("data"));
    LocalContext context;
    v8::Handle<v8::Object> inst = obj->NewInstance();
    context->Global()->Set(v8::String::New("obj"), inst);
    Local<Script> scr = v8::Script::Compile(v8::String::New("obj.xxx"));
    for (int i = 0; i < 10; i++) {
      Local<Value> result = scr->Run();
      CHECK(!result.IsEmpty());
      CHECK_EQ(17, result->Int32Value());
    }
  }
}

static void ThrowingGetAccessor(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetIsolate()->ThrowException(v8_str("g"));
}


static void ThrowingSetAccessor(Local<String> name,
                                Local<Value> value,
                                const v8::PropertyCallbackInfo<void>& info) {
  info.GetIsolate()->ThrowException(value);
}


THREADED_TEST(Regress1054726) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("x"),
                   ThrowingGetAccessor,
                   ThrowingSetAccessor,
                   Local<Value>());

  env->Global()->Set(v8_str("obj"), obj->NewInstance());

  // Use the throwing property setter/getter in a loop to force
  // the accessor ICs to be initialized.
  v8::Handle<Value> result;
  result = Script::Compile(v8_str(
      "var result = '';"
      "for (var i = 0; i < 5; i++) {"
      "  try { obj.x; } catch (e) { result += e; }"
      "}; result"))->Run();
  CHECK_EQ(v8_str("ggggg"), result);

  result = Script::Compile(String::New(
      "var result = '';"
      "for (var i = 0; i < 5; i++) {"
      "  try { obj.x = i; } catch (e) { result += e; }"
      "}; result"))->Run();
  CHECK_EQ(v8_str("01234"), result);
}


static void AllocGetter(Local<String> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(v8::Array::New(1000));
}


THREADED_TEST(Gc) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("xxx"), AllocGetter);
  env->Global()->Set(v8_str("obj"), obj->NewInstance());
  Script::Compile(String::New(
      "var last = [];"
      "for (var i = 0; i < 2048; i++) {"
      "  var result = obj.xxx;"
      "  result[0] = last;"
      "  last = result;"
      "}"))->Run();
}


static void StackCheck(Local<String> name,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::StackFrameIterator iter(reinterpret_cast<i::Isolate*>(info.GetIsolate()));
  for (int i = 0; !iter.done(); i++) {
    i::StackFrame* frame = iter.frame();
    CHECK(i != 0 || (frame->type() == i::StackFrame::EXIT));
    i::Code* code = frame->LookupCode();
    CHECK(code->IsCode());
    i::Address pc = frame->pc();
    CHECK(code->contains(pc));
    iter.Advance();
  }
}


THREADED_TEST(StackIteration) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env->GetIsolate());
  i::StringStream::ClearMentionedObjectCache(isolate);
  obj->SetAccessor(v8_str("xxx"), StackCheck);
  env->Global()->Set(v8_str("obj"), obj->NewInstance());
  Script::Compile(String::New(
      "function foo() {"
      "  return obj.xxx;"
      "}"
      "for (var i = 0; i < 100; i++) {"
      "  foo();"
      "}"))->Run();
}


static void AllocateHandles(Local<String> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info) {
  for (int i = 0; i < i::kHandleBlockSize + 1; i++) {
    v8::Local<v8::Value>::New(info.GetIsolate(), name);
  }
  info.GetReturnValue().Set(v8::Integer::New(100));
}


THREADED_TEST(HandleScopeSegment) {
  // Check that we can return values past popping of handle scope
  // segments.
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("xxx"), AllocateHandles);
  env->Global()->Set(v8_str("obj"), obj->NewInstance());
  v8::Handle<v8::Value> result = Script::Compile(String::New(
      "var result;"
      "for (var i = 0; i < 4; i++)"
      "  result = obj.xxx;"
      "result;"))->Run();
  CHECK_EQ(100, result->Int32Value());
}


void JSONStringifyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  v8::Handle<v8::Array> array = v8::Array::New(1);
  array->Set(0, v8_str("regress"));
  info.GetReturnValue().Set(array);
}


void JSONStringifyGetter(Local<String> name,
                         const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(v8_str("crbug-161028"));
}


THREADED_TEST(JSONStringifyNamedInterceptorObject) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetNamedPropertyHandler(
      JSONStringifyGetter, NULL, NULL, NULL, JSONStringifyEnumerator);
  env->Global()->Set(v8_str("obj"), obj->NewInstance());
  v8::Handle<v8::String> expected = v8_str("{\"regress\":\"crbug-161028\"}");
  CHECK(CompileRun("JSON.stringify(obj)")->Equals(expected));
}


THREADED_TEST(AccessorPropertyCrossContext) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::Function> fun = v8::Function::New(isolate, handle_property);
  LocalContext switch_context;
  switch_context->Global()->Set(v8_str("fun"), fun);
  v8::TryCatch try_catch;
  CompileRun(
      "var o = Object.create(null, { n: { get:fun } });"
      "for (var i = 0; i < 10; i++) o.n;");
  CHECK(!try_catch.HasCaught());
}
