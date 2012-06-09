// Copyright 2009 the V8 project authors. All rights reserved.
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
using ::v8::AccessorInfo;
using ::v8::Extension;

static v8::Handle<Value> handle_property(Local<String> name,
                                         const AccessorInfo&) {
  ApiTestFuzzer::Fuzz();
  return v8_num(900);
}


THREADED_TEST(PropertyHandler) {
  v8::HandleScope scope;
  Local<v8::FunctionTemplate> fun_templ = v8::FunctionTemplate::New();
  fun_templ->InstanceTemplate()->SetAccessor(v8_str("foo"), handle_property);
  LocalContext env;
  Local<Function> fun = fun_templ->GetFunction();
  env->Global()->Set(v8_str("Fun"), fun);
  Local<Script> getter = v8_compile("var obj = new Fun(); obj.foo;");
  CHECK_EQ(900, getter->Run()->Int32Value());
  Local<Script> setter = v8_compile("obj.foo = 901;");
  CHECK_EQ(901, setter->Run()->Int32Value());
}


static v8::Handle<Value> GetIntValue(Local<String> property,
                                     const AccessorInfo& info) {
  ApiTestFuzzer::Fuzz();
  int* value =
      static_cast<int*>(v8::Handle<v8::External>::Cast(info.Data())->Value());
  return v8_num(*value);
}


static void SetIntValue(Local<String> property,
                        Local<Value> value,
                        const AccessorInfo& info) {
  int* field =
      static_cast<int*>(v8::Handle<v8::External>::Cast(info.Data())->Value());
  *field = value->Int32Value();
}

int foo, bar, baz;

THREADED_TEST(GlobalVariableAccess) {
  foo = 0;
  bar = -4;
  baz = 10;
  v8::HandleScope scope;
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


static int x_register = 0;
static v8::Handle<v8::Object> x_receiver;
static v8::Handle<v8::Object> x_holder;


static v8::Handle<Value> XGetter(Local<String> name, const AccessorInfo& info) {
  ApiTestFuzzer::Fuzz();
  CHECK_EQ(x_receiver, info.This());
  CHECK_EQ(x_holder, info.Holder());
  return v8_num(x_register);
}


static void XSetter(Local<String> name,
                    Local<Value> value,
                    const AccessorInfo& info) {
  CHECK_EQ(x_holder, info.This());
  CHECK_EQ(x_holder, info.Holder());
  x_register = value->Int32Value();
}


THREADED_TEST(AccessorIC) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("x"), XGetter, XSetter);
  LocalContext context;
  x_holder = obj->NewInstance();
  context->Global()->Set(v8_str("holder"), x_holder);
  x_receiver = v8::Object::New();
  context->Global()->Set(v8_str("obj"), x_receiver);
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(CompileRun(
    "obj.__proto__ = holder;"
    "var result = [];"
    "for (var i = 0; i < 10; i++) {"
    "  holder.x = i;"
    "  result.push(obj.x);"
    "}"
    "result"));
  CHECK_EQ(10, array->Length());
  for (int i = 0; i < 10; i++) {
    v8::Handle<Value> entry = array->Get(v8::Integer::New(i));
    CHECK_EQ(v8::Integer::New(i), entry);
  }
}


static v8::Handle<Value> AccessorProhibitsOverwritingGetter(
    Local<String> name,
    const AccessorInfo& info) {
  ApiTestFuzzer::Fuzz();
  return v8::True();
}


THREADED_TEST(AccessorProhibitsOverwriting) {
  v8::HandleScope scope;
  LocalContext context;
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
static v8::Handle<Value> HandleAllocatingGetter(Local<String> name,
                                                const AccessorInfo& info) {
  ApiTestFuzzer::Fuzz();
  for (int i = 0; i < C; i++)
    v8::String::New("foo");
  return v8::String::New("foo");
}


THREADED_TEST(HandleScopePop) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("one"), HandleAllocatingGetter<1>);
  obj->SetAccessor(v8_str("many"), HandleAllocatingGetter<1024>);
  LocalContext context;
  v8::Handle<v8::Object> inst = obj->NewInstance();
  context->Global()->Set(v8::String::New("obj"), inst);
  int count_before = i::HandleScope::NumberOfHandles();
  {
    v8::HandleScope scope;
    CompileRun(
        "for (var i = 0; i < 1000; i++) {"
        "  obj.one;"
        "  obj.many;"
        "}");
  }
  int count_after = i::HandleScope::NumberOfHandles();
  CHECK_EQ(count_before, count_after);
}

static v8::Handle<Value> CheckAccessorArgsCorrect(Local<String> name,
                                                  const AccessorInfo& info) {
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()->Equals(v8::String::New("data")));
  ApiTestFuzzer::Fuzz();
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()->Equals(v8::String::New("data")));
  HEAP->CollectAllGarbage(i::Heap::kNoGCFlags);
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()->Equals(v8::String::New("data")));
  return v8::Integer::New(17);
}

THREADED_TEST(DirectCall) {
  v8::HandleScope scope;
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

static v8::Handle<Value> EmptyGetter(Local<String> name,
                                     const AccessorInfo& info) {
  CheckAccessorArgsCorrect(name, info);
  ApiTestFuzzer::Fuzz();
  CheckAccessorArgsCorrect(name, info);
  return v8::Handle<v8::Value>();
}

THREADED_TEST(EmptyResult) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("xxx"), EmptyGetter, NULL, v8::String::New("data"));
  LocalContext context;
  v8::Handle<v8::Object> inst = obj->NewInstance();
  context->Global()->Set(v8::String::New("obj"), inst);
  Local<Script> scr = v8::Script::Compile(v8::String::New("obj.xxx"));
  for (int i = 0; i < 10; i++) {
    Local<Value> result = scr->Run();
    CHECK(result == v8::Undefined());
  }
}


THREADED_TEST(NoReuseRegress) {
  // Check that the IC generated for the one test doesn't get reused
  // for the other.
  v8::HandleScope scope;
  {
    v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
    obj->SetAccessor(v8_str("xxx"), EmptyGetter, NULL, v8::String::New("data"));
    LocalContext context;
    v8::Handle<v8::Object> inst = obj->NewInstance();
    context->Global()->Set(v8::String::New("obj"), inst);
    Local<Script> scr = v8::Script::Compile(v8::String::New("obj.xxx"));
    for (int i = 0; i < 2; i++) {
      Local<Value> result = scr->Run();
      CHECK(result == v8::Undefined());
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

static v8::Handle<Value> ThrowingGetAccessor(Local<String> name,
                                             const AccessorInfo& info) {
  ApiTestFuzzer::Fuzz();
  return v8::ThrowException(v8_str("g"));
}


static void ThrowingSetAccessor(Local<String> name,
                                Local<Value> value,
                                const AccessorInfo& info) {
  v8::ThrowException(value);
}


THREADED_TEST(Regress1054726) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("x"),
                   ThrowingGetAccessor,
                   ThrowingSetAccessor,
                   Local<Value>());

  LocalContext env;
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


static v8::Handle<Value> AllocGetter(Local<String> name,
                                     const AccessorInfo& info) {
  ApiTestFuzzer::Fuzz();
  return v8::Array::New(1000);
}


THREADED_TEST(Gc) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("xxx"), AllocGetter);
  LocalContext env;
  env->Global()->Set(v8_str("obj"), obj->NewInstance());
  Script::Compile(String::New(
      "var last = [];"
      "for (var i = 0; i < 2048; i++) {"
      "  var result = obj.xxx;"
      "  result[0] = last;"
      "  last = result;"
      "}"))->Run();
}


static v8::Handle<Value> StackCheck(Local<String> name,
                                    const AccessorInfo& info) {
  i::StackFrameIterator iter;
  for (int i = 0; !iter.done(); i++) {
    i::StackFrame* frame = iter.frame();
    CHECK(i != 0 || (frame->type() == i::StackFrame::EXIT));
    i::Code* code = frame->LookupCode();
    CHECK(code->IsCode());
    i::Address pc = frame->pc();
    CHECK(code->contains(pc));
    iter.Advance();
  }
  return v8::Undefined();
}


THREADED_TEST(StackIteration) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  i::StringStream::ClearMentionedObjectCache();
  obj->SetAccessor(v8_str("xxx"), StackCheck);
  LocalContext env;
  env->Global()->Set(v8_str("obj"), obj->NewInstance());
  Script::Compile(String::New(
      "function foo() {"
      "  return obj.xxx;"
      "}"
      "for (var i = 0; i < 100; i++) {"
      "  foo();"
      "}"))->Run();
}


static v8::Handle<Value> AllocateHandles(Local<String> name,
                                         const AccessorInfo& info) {
  for (int i = 0; i < i::kHandleBlockSize + 1; i++) {
    v8::Local<v8::Value>::New(name);
  }
  return v8::Integer::New(100);
}


THREADED_TEST(HandleScopeSegment) {
  // Check that we can return values past popping of handle scope
  // segments.
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> obj = ObjectTemplate::New();
  obj->SetAccessor(v8_str("xxx"), AllocateHandles);
  LocalContext env;
  env->Global()->Set(v8_str("obj"), obj->NewInstance());
  v8::Handle<v8::Value> result = Script::Compile(String::New(
      "var result;"
      "for (var i = 0; i < 4; i++)"
      "  result = obj.xxx;"
      "result;"))->Run();
  CHECK_EQ(100, result->Int32Value());
}
