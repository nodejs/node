// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

#include "src/v8.h"

#include "src/heap/heap.h"
#include "test/cctest/cctest.h"

using namespace v8;


enum Expectations {
  EXPECT_RESULT,
  EXPECT_EXCEPTION,
  EXPECT_ERROR
};


// A DeclarationContext holds a reference to a v8::Context and keeps
// track of various declaration related counters to make it easier to
// track if global declarations in the presence of interceptors behave
// the right way.
class DeclarationContext {
 public:
  DeclarationContext();

  virtual ~DeclarationContext() {
    if (is_initialized_) {
      Isolate* isolate = CcTest::isolate();
      HandleScope scope(isolate);
      Local<Context> context = Local<Context>::New(isolate, context_);
      context->Exit();
      context_.Reset();
    }
  }

  void Check(const char* source,
             int get, int set, int has,
             Expectations expectations,
             v8::Handle<Value> value = Local<Value>());

  int get_count() const { return get_count_; }
  int set_count() const { return set_count_; }
  int query_count() const { return query_count_; }

 protected:
  virtual v8::Handle<Value> Get(Local<String> key);
  virtual v8::Handle<Value> Set(Local<String> key, Local<Value> value);
  virtual v8::Handle<Integer> Query(Local<String> key);

  void InitializeIfNeeded();

  // Perform optional initialization steps on the context after it has
  // been created. Defaults to none but may be overwritten.
  virtual void PostInitializeContext(Handle<Context> context) {}

  // Get the holder for the interceptor. Default to the instance template
  // but may be overwritten.
  virtual Local<ObjectTemplate> GetHolder(Local<FunctionTemplate> function) {
    return function->InstanceTemplate();
  }

  // The handlers are called as static functions that forward
  // to the instance specific virtual methods.
  static void HandleGet(Local<String> key,
                        const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandleSet(Local<String> key,
                        Local<Value> value,
                        const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandleQuery(Local<String> key,
                          const v8::PropertyCallbackInfo<v8::Integer>& info);

  v8::Isolate* isolate() const { return CcTest::isolate(); }

 private:
  bool is_initialized_;
  Persistent<Context> context_;

  int get_count_;
  int set_count_;
  int query_count_;

  static DeclarationContext* GetInstance(Local<Value> data);
};


DeclarationContext::DeclarationContext()
    : is_initialized_(false), get_count_(0), set_count_(0), query_count_(0) {
  // Do nothing.
}


void DeclarationContext::InitializeIfNeeded() {
  if (is_initialized_) return;
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  Local<FunctionTemplate> function = FunctionTemplate::New(isolate);
  Local<Value> data = External::New(CcTest::isolate(), this);
  GetHolder(function)->SetNamedPropertyHandler(&HandleGet,
                                               &HandleSet,
                                               &HandleQuery,
                                               0, 0,
                                               data);
  Local<Context> context = Context::New(isolate,
                                        0,
                                        function->InstanceTemplate(),
                                        Local<Value>());
  context_.Reset(isolate, context);
  context->Enter();
  is_initialized_ = true;
  PostInitializeContext(context);
}


void DeclarationContext::Check(const char* source,
                               int get, int set, int query,
                               Expectations expectations,
                               v8::Handle<Value> value) {
  InitializeIfNeeded();
  // A retry after a GC may pollute the counts, so perform gc now
  // to avoid that.
  CcTest::heap()->CollectGarbage(v8::internal::NEW_SPACE);
  HandleScope scope(CcTest::isolate());
  TryCatch catcher;
  catcher.SetVerbose(true);
  Local<Script> script =
      Script::Compile(String::NewFromUtf8(CcTest::isolate(), source));
  if (expectations == EXPECT_ERROR) {
    CHECK(script.IsEmpty());
    return;
  }
  CHECK(!script.IsEmpty());
  Local<Value> result = script->Run();
  CHECK_EQ(get, get_count());
  CHECK_EQ(set, set_count());
  CHECK_EQ(query, query_count());
  if (expectations == EXPECT_RESULT) {
    CHECK(!catcher.HasCaught());
    if (!value.IsEmpty()) {
      CHECK_EQ(value, result);
    }
  } else {
    CHECK(expectations == EXPECT_EXCEPTION);
    CHECK(catcher.HasCaught());
    if (!value.IsEmpty()) {
      CHECK_EQ(value, catcher.Exception());
    }
  }
  // Clean slate for the next test.
  CcTest::heap()->CollectAllAvailableGarbage();
}


void DeclarationContext::HandleGet(
    Local<String> key,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DeclarationContext* context = GetInstance(info.Data());
  context->get_count_++;
  info.GetReturnValue().Set(context->Get(key));
}


void DeclarationContext::HandleSet(
    Local<String> key,
    Local<Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DeclarationContext* context = GetInstance(info.Data());
  context->set_count_++;
  info.GetReturnValue().Set(context->Set(key, value));
}


void DeclarationContext::HandleQuery(
    Local<String> key,
    const v8::PropertyCallbackInfo<v8::Integer>& info) {
  DeclarationContext* context = GetInstance(info.Data());
  context->query_count_++;
  info.GetReturnValue().Set(context->Query(key));
}


DeclarationContext* DeclarationContext::GetInstance(Local<Value> data) {
  void* value = Local<External>::Cast(data)->Value();
  return static_cast<DeclarationContext*>(value);
}


v8::Handle<Value> DeclarationContext::Get(Local<String> key) {
  return v8::Handle<Value>();
}


v8::Handle<Value> DeclarationContext::Set(Local<String> key,
                                          Local<Value> value) {
  return v8::Handle<Value>();
}


v8::Handle<Integer> DeclarationContext::Query(Local<String> key) {
  return v8::Handle<Integer>();
}


// Test global declaration of a property the interceptor doesn't know
// about and doesn't handle.
TEST(Unknown) {
  HandleScope scope(CcTest::isolate());
  v8::V8::Initialize();

  { DeclarationContext context;
    context.Check("var x; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Undefined(CcTest::isolate()));
  }

  { DeclarationContext context;
    context.Check("var x = 0; x",
                  1,  // access
                  1,  // initialization
                  0, EXPECT_RESULT, Number::New(CcTest::isolate(), 0));
  }

  { DeclarationContext context;
    context.Check("function x() { }; x",
                  1,  // access
                  0,
                  0,
                  EXPECT_RESULT);
  }

  { DeclarationContext context;
    context.Check("const x; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Undefined(CcTest::isolate()));
  }

  { DeclarationContext context;
    context.Check("const x = 0; x",
                  1,  // access
                  0,
                  0,
                  EXPECT_RESULT, Number::New(CcTest::isolate(), 0));
  }
}


class AbsentPropertyContext: public DeclarationContext {
 protected:
  virtual v8::Handle<Integer> Query(Local<String> key) {
    return v8::Handle<Integer>();
  }
};


TEST(Absent) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::V8::Initialize();
  HandleScope scope(isolate);

  { AbsentPropertyContext context;
    context.Check("var x; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Undefined(isolate));
  }

  { AbsentPropertyContext context;
    context.Check("var x = 0; x",
                  1,  // access
                  1,  // initialization
                  0, EXPECT_RESULT, Number::New(isolate, 0));
  }

  { AbsentPropertyContext context;
    context.Check("function x() { }; x",
                  1,  // access
                  0,
                  0,
                  EXPECT_RESULT);
  }

  { AbsentPropertyContext context;
    context.Check("const x; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Undefined(isolate));
  }

  { AbsentPropertyContext context;
    context.Check("const x = 0; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Number::New(isolate, 0));
  }

  { AbsentPropertyContext context;
    context.Check("if (false) { var x = 0 }; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Undefined(isolate));
  }
}



class AppearingPropertyContext: public DeclarationContext {
 public:
  enum State {
    DECLARE,
    INITIALIZE_IF_ASSIGN,
    UNKNOWN
  };

  AppearingPropertyContext() : state_(DECLARE) { }

 protected:
  virtual v8::Handle<Integer> Query(Local<String> key) {
    switch (state_) {
      case DECLARE:
        // Force declaration by returning that the
        // property is absent.
        state_ = INITIALIZE_IF_ASSIGN;
        return Handle<Integer>();
      case INITIALIZE_IF_ASSIGN:
        // Return that the property is present so we only get the
        // setter called when initializing with a value.
        state_ = UNKNOWN;
        return Integer::New(isolate(), v8::None);
      default:
        CHECK(state_ == UNKNOWN);
        break;
    }
    // Do the lookup in the object.
    return v8::Handle<Integer>();
  }

 private:
  State state_;
};


TEST(Appearing) {
  v8::V8::Initialize();
  HandleScope scope(CcTest::isolate());

  { AppearingPropertyContext context;
    context.Check("var x; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Undefined(CcTest::isolate()));
  }

  { AppearingPropertyContext context;
    context.Check("var x = 0; x",
                  1,  // access
                  1,  // initialization
                  0, EXPECT_RESULT, Number::New(CcTest::isolate(), 0));
  }

  { AppearingPropertyContext context;
    context.Check("function x() { }; x",
                  1,  // access
                  0,
                  0,
                  EXPECT_RESULT);
  }

  { AppearingPropertyContext context;
    context.Check("const x; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Undefined(CcTest::isolate()));
  }

  { AppearingPropertyContext context;
    context.Check("const x = 0; x",
                  1,  // access
                  0, 0, EXPECT_RESULT, Number::New(CcTest::isolate(), 0));
  }
}



class ExistsInPrototypeContext: public DeclarationContext {
 public:
  ExistsInPrototypeContext() { InitializeIfNeeded(); }
 protected:
  virtual v8::Handle<Integer> Query(Local<String> key) {
    // Let it seem that the property exists in the prototype object.
    return Integer::New(isolate(), v8::None);
  }

  // Use the prototype as the holder for the interceptors.
  virtual Local<ObjectTemplate> GetHolder(Local<FunctionTemplate> function) {
    return function->PrototypeTemplate();
  }
};


TEST(ExistsInPrototype) {
  HandleScope scope(CcTest::isolate());

  // Sanity check to make sure that the holder of the interceptor
  // really is the prototype object.
  { ExistsInPrototypeContext context;
    context.Check("this.x = 87; this.x", 0, 0, 1, EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 87));
  }

  { ExistsInPrototypeContext context;
    context.Check("var x; x",
                  0,
                  0,
                  0,
                  EXPECT_RESULT, Undefined(CcTest::isolate()));
  }

  { ExistsInPrototypeContext context;
    context.Check("var x = 0; x",
                  0,
                  0,
                  0,
                  EXPECT_RESULT, Number::New(CcTest::isolate(), 0));
  }

  { ExistsInPrototypeContext context;
    context.Check("const x; x",
                  0,
                  0,
                  0,
                  EXPECT_RESULT, Undefined(CcTest::isolate()));
  }

  { ExistsInPrototypeContext context;
    context.Check("const x = 0; x",
                  0,
                  0,
                  0,
                  EXPECT_RESULT, Number::New(CcTest::isolate(), 0));
  }
}



class AbsentInPrototypeContext: public DeclarationContext {
 protected:
  virtual v8::Handle<Integer> Query(Local<String> key) {
    // Let it seem that the property is absent in the prototype object.
    return Handle<Integer>();
  }

  // Use the prototype as the holder for the interceptors.
  virtual Local<ObjectTemplate> GetHolder(Local<FunctionTemplate> function) {
    return function->PrototypeTemplate();
  }
};


TEST(AbsentInPrototype) {
  v8::V8::Initialize();
  HandleScope scope(CcTest::isolate());

  { AbsentInPrototypeContext context;
    context.Check("if (false) { var x = 0; }; x",
                  0,
                  0,
                  0,
                  EXPECT_RESULT, Undefined(CcTest::isolate()));
  }
}



class ExistsInHiddenPrototypeContext: public DeclarationContext {
 public:
  ExistsInHiddenPrototypeContext() {
    hidden_proto_ = FunctionTemplate::New(CcTest::isolate());
    hidden_proto_->SetHiddenPrototype(true);
  }

 protected:
  virtual v8::Handle<Integer> Query(Local<String> key) {
    // Let it seem that the property exists in the hidden prototype object.
    return Integer::New(isolate(), v8::None);
  }

  // Install the hidden prototype after the global object has been created.
  virtual void PostInitializeContext(Handle<Context> context) {
    Local<Object> global_object = context->Global();
    Local<Object> hidden_proto = hidden_proto_->GetFunction()->NewInstance();
    Local<Object> inner_global =
        Local<Object>::Cast(global_object->GetPrototype());
    inner_global->SetPrototype(hidden_proto);
  }

  // Use the hidden prototype as the holder for the interceptors.
  virtual Local<ObjectTemplate> GetHolder(Local<FunctionTemplate> function) {
    return hidden_proto_->InstanceTemplate();
  }

 private:
  Local<FunctionTemplate> hidden_proto_;
};


TEST(ExistsInHiddenPrototype) {
  HandleScope scope(CcTest::isolate());

  { ExistsInHiddenPrototypeContext context;
    context.Check("var x; x", 0, 0, 0, EXPECT_RESULT,
                  Undefined(CcTest::isolate()));
  }

  { ExistsInHiddenPrototypeContext context;
    context.Check("var x = 0; x", 0, 0, 0, EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 0));
  }

  { ExistsInHiddenPrototypeContext context;
    context.Check("function x() { }; x",
                  0,
                  0,
                  0,
                  EXPECT_RESULT);
  }

  // TODO(mstarzinger): The semantics of global const is vague.
  { ExistsInHiddenPrototypeContext context;
    context.Check("const x; x", 0, 0, 0, EXPECT_RESULT,
                  Undefined(CcTest::isolate()));
  }

  // TODO(mstarzinger): The semantics of global const is vague.
  { ExistsInHiddenPrototypeContext context;
    context.Check("const x = 0; x", 0, 0, 0, EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 0));
  }
}



class SimpleContext {
 public:
  SimpleContext()
      : handle_scope_(CcTest::isolate()),
        context_(Context::New(CcTest::isolate())) {
    context_->Enter();
  }

  ~SimpleContext() {
    context_->Exit();
  }

  void Check(const char* source,
             Expectations expectations,
             v8::Handle<Value> value = Local<Value>()) {
    HandleScope scope(context_->GetIsolate());
    TryCatch catcher;
    catcher.SetVerbose(true);
    Local<Script> script =
        Script::Compile(String::NewFromUtf8(context_->GetIsolate(), source));
    if (expectations == EXPECT_ERROR) {
      CHECK(script.IsEmpty());
      return;
    }
    CHECK(!script.IsEmpty());
    Local<Value> result = script->Run();
    if (expectations == EXPECT_RESULT) {
      CHECK(!catcher.HasCaught());
      if (!value.IsEmpty()) {
        CHECK_EQ(value, result);
      }
    } else {
      CHECK(expectations == EXPECT_EXCEPTION);
      CHECK(catcher.HasCaught());
      if (!value.IsEmpty()) {
        CHECK_EQ(value, catcher.Exception());
      }
    }
  }

 private:
  HandleScope handle_scope_;
  Local<Context> context_;
};


TEST(CrossScriptReferences) {
  v8::Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);

  { SimpleContext context;
    context.Check("var x = 1; x",
                  EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("var x = 2; x",
                  EXPECT_RESULT, Number::New(isolate, 2));
    context.Check("const x = 3; x", EXPECT_EXCEPTION);
    context.Check("const x = 4; x", EXPECT_EXCEPTION);
    context.Check("x = 5; x",
                  EXPECT_RESULT, Number::New(isolate, 5));
    context.Check("var x = 6; x",
                  EXPECT_RESULT, Number::New(isolate, 6));
    context.Check("this.x",
                  EXPECT_RESULT, Number::New(isolate, 6));
    context.Check("function x() { return 7 }; x()",
                  EXPECT_RESULT, Number::New(isolate, 7));
  }

  { SimpleContext context;
    context.Check("const x = 1; x",
                  EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("var x = 2; x",  // assignment ignored
                  EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("const x = 3; x", EXPECT_EXCEPTION);
    context.Check("x = 4; x",  // assignment ignored
                  EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("var x = 5; x",  // assignment ignored
                  EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("this.x",
                  EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("function x() { return 7 }; x",
                  EXPECT_EXCEPTION);
  }
}


TEST(CrossScriptReferencesHarmony) {
  i::FLAG_use_strict = true;
  i::FLAG_harmony_scoping = true;
  i::FLAG_harmony_modules = true;

  v8::Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);

  const char* decs[] = {
    "var x = 1; x", "x", "this.x",
    "function x() { return 1 }; x()", "x()", "this.x()",
    "let x = 1; x", "x", "this.x",
    "const x = 1; x", "x", "this.x",
    "module x { export let a = 1 }; x.a", "x.a", "this.x.a",
    NULL
  };

  for (int i = 0; decs[i] != NULL; i += 3) {
    SimpleContext context;
    context.Check(decs[i], EXPECT_RESULT, Number::New(isolate, 1));
    context.Check(decs[i+1], EXPECT_RESULT, Number::New(isolate, 1));
    // TODO(rossberg): The current ES6 draft spec does not reflect lexical
    // bindings on the global object. However, this will probably change, in
    // which case we reactivate the following test.
    if (i/3 < 2) {
      context.Check(decs[i+2], EXPECT_RESULT, Number::New(isolate, 1));
    }
  }
}


TEST(CrossScriptConflicts) {
  i::FLAG_use_strict = true;
  i::FLAG_harmony_scoping = true;
  i::FLAG_harmony_modules = true;

  HandleScope scope(CcTest::isolate());

  const char* firsts[] = {
    "var x = 1; x",
    "function x() { return 1 }; x()",
    "let x = 1; x",
    "const x = 1; x",
    "module x { export let a = 1 }; x.a",
    NULL
  };
  const char* seconds[] = {
    "var x = 2; x",
    "function x() { return 2 }; x()",
    "let x = 2; x",
    "const x = 2; x",
    "module x { export let a = 2 }; x.a",
    NULL
  };

  for (int i = 0; firsts[i] != NULL; ++i) {
    for (int j = 0; seconds[j] != NULL; ++j) {
      SimpleContext context;
      context.Check(firsts[i], EXPECT_RESULT,
                    Number::New(CcTest::isolate(), 1));
      // TODO(rossberg): All tests should actually be errors in Harmony,
      // but we currently do not detect the cases where the first declaration
      // is not lexical.
      context.Check(seconds[j],
                    i < 2 ? EXPECT_RESULT : EXPECT_ERROR,
                    Number::New(CcTest::isolate(), 2));
    }
  }
}
