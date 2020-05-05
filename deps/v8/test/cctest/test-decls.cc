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

#include "src/init/v8.h"

#include "test/cctest/cctest.h"

namespace v8 {

namespace {

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

  void Check(const char* source, int get, int set, int has,
             Expectations expectations,
             v8::Local<Value> value = Local<Value>());

  int get_count() const { return get_count_; }
  int set_count() const { return set_count_; }
  int query_count() const { return query_count_; }

 protected:
  virtual v8::Local<Value> Get(Local<Name> key);
  virtual v8::Local<Value> Set(Local<Name> key, Local<Value> value);
  virtual v8::Local<Integer> Query(Local<Name> key);

  void InitializeIfNeeded();

  // Perform optional initialization steps on the context after it has
  // been created. Defaults to none but may be overwritten.
  virtual void PostInitializeContext(Local<Context> context) {}

  // Get the holder for the interceptor. Default to the instance template
  // but may be overwritten.
  virtual Local<ObjectTemplate> GetHolder(Local<FunctionTemplate> function) {
    return function->InstanceTemplate();
  }

  // The handlers are called as static functions that forward
  // to the instance specific virtual methods.
  static void HandleGet(Local<Name> key,
                        const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandleSet(Local<Name> key, Local<Value> value,
                        const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandleQuery(Local<Name> key,
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
  GetHolder(function)->SetHandler(v8::NamedPropertyHandlerConfiguration(
      &HandleGet, &HandleSet, &HandleQuery, nullptr, nullptr, data));
  Local<Context> context = Context::New(
      isolate, nullptr, function->InstanceTemplate(), Local<Value>());
  context_.Reset(isolate, context);
  context->Enter();
  is_initialized_ = true;
  // Reset counts. Bootstrapping might have called into the interceptor.
  get_count_ = 0;
  set_count_ = 0;
  query_count_ = 0;
  PostInitializeContext(context);
}


void DeclarationContext::Check(const char* source, int get, int set, int query,
                               Expectations expectations,
                               v8::Local<Value> value) {
  InitializeIfNeeded();
  // A retry after a GC may pollute the counts, so perform gc now
  // to avoid that.
  CcTest::CollectGarbage(v8::internal::NEW_SPACE);
  HandleScope scope(CcTest::isolate());
  TryCatch catcher(CcTest::isolate());
  catcher.SetVerbose(true);
  Local<Context> context = CcTest::isolate()->GetCurrentContext();
  MaybeLocal<Script> script = Script::Compile(
      context, String::NewFromUtf8(CcTest::isolate(), source).ToLocalChecked());
  if (expectations == EXPECT_ERROR) {
    CHECK(script.IsEmpty());
    return;
  }
  CHECK(!script.IsEmpty());
  MaybeLocal<Value> result = script.ToLocalChecked()->Run(context);
  CHECK_EQ(get, get_count());
  CHECK_EQ(set, set_count());
  CHECK_EQ(query, query_count());
  if (expectations == EXPECT_RESULT) {
    CHECK(!catcher.HasCaught());
    if (!value.IsEmpty()) {
      CHECK(value->Equals(context, result.ToLocalChecked()).FromJust());
    }
  } else {
    CHECK(expectations == EXPECT_EXCEPTION);
    CHECK(catcher.HasCaught());
    if (!value.IsEmpty()) {
      CHECK(value->Equals(context, catcher.Exception()).FromJust());
    }
  }
  // Clean slate for the next test.
  CcTest::CollectAllAvailableGarbage();
}


void DeclarationContext::HandleGet(
    Local<Name> key, const v8::PropertyCallbackInfo<v8::Value>& info) {
  DeclarationContext* context = GetInstance(info.Data());
  context->get_count_++;
  info.GetReturnValue().Set(context->Get(key));
}


void DeclarationContext::HandleSet(
    Local<Name> key, Local<Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DeclarationContext* context = GetInstance(info.Data());
  context->set_count_++;
  info.GetReturnValue().Set(context->Set(key, value));
}


void DeclarationContext::HandleQuery(
    Local<Name> key, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  DeclarationContext* context = GetInstance(info.Data());
  context->query_count_++;
  info.GetReturnValue().Set(context->Query(key));
}


DeclarationContext* DeclarationContext::GetInstance(Local<Value> data) {
  void* value = Local<External>::Cast(data)->Value();
  return static_cast<DeclarationContext*>(value);
}


v8::Local<Value> DeclarationContext::Get(Local<Name> key) {
  return v8::Local<Value>();
}


v8::Local<Value> DeclarationContext::Set(Local<Name> key, Local<Value> value) {
  return v8::Local<Value>();
}


v8::Local<Integer> DeclarationContext::Query(Local<Name> key) {
  return v8::Local<Integer>();
}

}  // namespace

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
                  1, 1, EXPECT_RESULT);
  }
}


class AbsentPropertyContext: public DeclarationContext {
 protected:
  v8::Local<Integer> Query(Local<Name> key) override {
    return v8::Local<Integer>();
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
                  1, 1, EXPECT_RESULT);
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
  v8::Local<Integer> Query(Local<Name> key) override {
    switch (state_) {
      case DECLARE:
        // Force declaration by returning that the
        // property is absent.
        state_ = INITIALIZE_IF_ASSIGN;
        return Local<Integer>();
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
    return v8::Local<Integer>();
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
                  1, 1, EXPECT_RESULT);
  }
}



class ExistsInPrototypeContext: public DeclarationContext {
 public:
  ExistsInPrototypeContext() { InitializeIfNeeded(); }
 protected:
  v8::Local<Integer> Query(Local<Name> key) override {
    // Let it seem that the property exists in the prototype object.
    return Integer::New(isolate(), v8::None);
  }

  // Use the prototype as the holder for the interceptors.
  Local<ObjectTemplate> GetHolder(Local<FunctionTemplate> function) override {
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
}



class AbsentInPrototypeContext: public DeclarationContext {
 protected:
  v8::Local<Integer> Query(Local<Name> key) override {
    // Let it seem that the property is absent in the prototype object.
    return Local<Integer>();
  }

  // Use the prototype as the holder for the interceptors.
  Local<ObjectTemplate> GetHolder(Local<FunctionTemplate> function) override {
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

  void Check(const char* source, Expectations expectations,
             v8::Local<Value> value = Local<Value>()) {
    HandleScope scope(context_->GetIsolate());
    TryCatch catcher(context_->GetIsolate());
    catcher.SetVerbose(true);
    MaybeLocal<Script> script = Script::Compile(
        context_,
        String::NewFromUtf8(context_->GetIsolate(), source).ToLocalChecked());
    if (expectations == EXPECT_ERROR) {
      CHECK(script.IsEmpty());
      return;
    }
    CHECK(!script.IsEmpty());
    MaybeLocal<Value> result = script.ToLocalChecked()->Run(context_);
    if (expectations == EXPECT_RESULT) {
      CHECK(!catcher.HasCaught());
      if (!value.IsEmpty()) {
        CHECK(value->Equals(context_, result.ToLocalChecked()).FromJust());
      }
    } else {
      CHECK(expectations == EXPECT_EXCEPTION);
      CHECK(catcher.HasCaught());
      if (!value.IsEmpty()) {
        CHECK(value->Equals(context_, catcher.Exception()).FromJust());
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
    context.Check("x = 5; x",
                  EXPECT_RESULT, Number::New(isolate, 5));
    context.Check("var x = 6; x",
                  EXPECT_RESULT, Number::New(isolate, 6));
    context.Check("this.x",
                  EXPECT_RESULT, Number::New(isolate, 6));
    context.Check("function x() { return 7 }; x()",
                  EXPECT_RESULT, Number::New(isolate, 7));
  }
}


TEST(CrossScriptReferences_Simple) {
  i::FLAG_use_strict = true;

  v8::Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);

  {
    SimpleContext context;
    context.Check("let x = 1; x", EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("let x = 5; x", EXPECT_EXCEPTION);
  }
}


TEST(CrossScriptReferences_Simple2) {
  i::FLAG_use_strict = true;

  v8::Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);

  for (int k = 0; k < 100; k++) {
    SimpleContext context;
    bool cond = (k % 2) == 0;
    if (cond) {
      context.Check("let x = 1; x", EXPECT_RESULT, Number::New(isolate, 1));
      context.Check("let z = 4; z", EXPECT_RESULT, Number::New(isolate, 4));
    } else {
      context.Check("let z = 1; z", EXPECT_RESULT, Number::New(isolate, 1));
      context.Check("let x = 4; x", EXPECT_RESULT, Number::New(isolate, 4));
    }
    context.Check("let y = 2; x", EXPECT_RESULT,
                  Number::New(isolate, cond ? 1 : 4));
  }
}


TEST(CrossScriptReferencesHarmony) {
  v8::Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);

  // Check that simple cross-script global scope access works.
  const char* decs[] = {"'use strict'; var x = 1; x",
                        "x",
                        "'use strict'; function x() { return 1 }; x()",
                        "x()",
                        "'use strict'; let x = 1; x",
                        "x",
                        "'use strict'; const x = 1; x",
                        "x",
                        nullptr};

  for (int i = 0; decs[i] != nullptr; i += 2) {
    SimpleContext context;
    context.Check(decs[i], EXPECT_RESULT, Number::New(isolate, 1));
    context.Check(decs[i+1], EXPECT_RESULT, Number::New(isolate, 1));
  }

  // Check that cross-script global scope access works with late declarations.
  {
    SimpleContext context;
    context.Check("function d0() { return x0 }",  // dynamic lookup
                  EXPECT_RESULT, Undefined(isolate));
    context.Check("this.x0 = -1;"
                  "d0()",
                  EXPECT_RESULT, Number::New(isolate, -1));
    context.Check("'use strict';"
                  "function f0() { let y = 10; return x0 + y }"
                  "function g0() { let y = 10; return eval('x0 + y') }"
                  "function h0() { let y = 10; return (1,eval)('x0') + y }"
                  "x0 + f0() + g0() + h0()",
                  EXPECT_RESULT, Number::New(isolate, 26));

    context.Check("'use strict';"
                  "let x1 = 1;"
                  "function f1() { let y = 10; return x1 + y }"
                  "function g1() { let y = 10; return eval('x1 + y') }"
                  "function h1() { let y = 10; return (1,eval)('x1') + y }"
                  "function i1() { "
                  "  let y = 10; return (typeof x2 === 'undefined' ? 0 : 2) + y"
                  "}"
                  "function j1() { let y = 10; return eval('x2 + y') }"
                  "function k1() { let y = 10; return (1,eval)('x2') + y }"
                  "function cl() { "
                  "  let y = 10; "
                  "  return { "
                  "    f: function(){ return x1 + y },"
                  "    g: function(){ return eval('x1 + y') },"
                  "    h: function(){ return (1,eval)('x1') + y },"
                  "    i: function(){"
                  "      return (typeof x2 == 'undefined' ? 0 : 2) + y"
                  "    },"
                  "    j: function(){ return eval('x2 + y') },"
                  "    k: function(){ return (1,eval)('x2') + y },"
                  "  }"
                  "}"
                  "let o = cl();"
                  "x1 + eval('x1') + (1,eval)('x1') + f1() + g1() + h1();",
                  EXPECT_RESULT, Number::New(isolate, 36));
    context.Check("x1 + eval('x1') + (1,eval)('x1') + f1() + g1() + h1();",
                  EXPECT_RESULT, Number::New(isolate, 36));
    context.Check("o.f() + o.g() + o.h();",
                  EXPECT_RESULT, Number::New(isolate, 33));
    context.Check("i1() + o.i();",
                  EXPECT_RESULT, Number::New(isolate, 20));

    context.Check("'use strict';"
                  "let x2 = 2;"
                  "function f2() { let y = 20; return x2 + y }"
                  "function g2() { let y = 20; return eval('x2 + y') }"
                  "function h2() { let y = 20; return (1,eval)('x2') + y }"
                  "function i2() { let y = 20; return x1 + y }"
                  "function j2() { let y = 20; return eval('x1 + y') }"
                  "function k2() { let y = 20; return (1,eval)('x1') + y }"
                  "x2 + eval('x2') + (1,eval)('x2') + f2() + g2() + h2();",
                  EXPECT_RESULT, Number::New(isolate, 72));
    context.Check("x1 + eval('x1') + (1,eval)('x1') + f1() + g1() + h1();",
                  EXPECT_RESULT, Number::New(isolate, 36));
    context.Check("i1() + j1() + k1();",
                  EXPECT_RESULT, Number::New(isolate, 36));
    context.Check("i2() + j2() + k2();",
                  EXPECT_RESULT, Number::New(isolate, 63));
    context.Check("o.f() + o.g() + o.h();",
                  EXPECT_RESULT, Number::New(isolate, 33));
    context.Check("o.i() + o.j() + o.k();",
                  EXPECT_RESULT, Number::New(isolate, 36));
    context.Check("i1() + o.i();",
                  EXPECT_RESULT, Number::New(isolate, 24));

    context.Check("'use strict';"
                  "let x0 = 100;"
                  "x0 + eval('x0') + (1,eval)('x0') + "
                  "    d0() + f0() + g0() + h0();",
                  EXPECT_RESULT, Number::New(isolate, 730));
    context.Check("x0 + eval('x0') + (1,eval)('x0') + "
                  "    d0() + f0() + g0() + h0();",
                  EXPECT_RESULT, Number::New(isolate, 730));
    context.Check("delete this.x0;"
                  "x0 + eval('x0') + (1,eval)('x0') + "
                  "    d0() + f0() + g0() + h0();",
                  EXPECT_RESULT, Number::New(isolate, 730));
    context.Check("this.x1 = 666;"
                  "x1 + eval('x1') + (1,eval)('x1') + f1() + g1() + h1();",
                  EXPECT_RESULT, Number::New(isolate, 36));
    context.Check("delete this.x1;"
                  "x1 + eval('x1') + (1,eval)('x1') + f1() + g1() + h1();",
                  EXPECT_RESULT, Number::New(isolate, 36));
  }

  // Check that caching does respect scopes.
  {
    SimpleContext context;
    const char* script1 = "(function(){ return y1 })()";
    const char* script2 = "(function(){ return y2 })()";

    context.Check(script1, EXPECT_EXCEPTION);
    context.Check("this.y1 = 1; this.y2 = 2; 0;",
                  EXPECT_RESULT, Number::New(isolate, 0));
    context.Check(script1,
                  EXPECT_RESULT, Number::New(isolate, 1));
    context.Check("'use strict'; let y1 = 3; 0;",
                  EXPECT_RESULT, Number::New(isolate, 0));
    context.Check(script1,
                  EXPECT_RESULT, Number::New(isolate, 3));
    context.Check("y1 = 4;",
                  EXPECT_RESULT, Number::New(isolate, 4));
    context.Check(script1,
                  EXPECT_RESULT, Number::New(isolate, 4));

    context.Check(script2,
                  EXPECT_RESULT, Number::New(isolate, 2));
    context.Check("'use strict'; let y2 = 5; 0;",
                  EXPECT_RESULT, Number::New(isolate, 0));
    context.Check(script1,
                  EXPECT_RESULT, Number::New(isolate, 4));
    context.Check(script2,
                  EXPECT_RESULT, Number::New(isolate, 5));
  }
}


TEST(CrossScriptReferencesHarmonyRegress) {
  v8::Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  SimpleContext context;
  context.Check(
      "'use strict';"
      "function i1() { "
      "  let y = 10; return (typeof x2 === 'undefined' ? 0 : 2) + y"
      "}"
      "i1();"
      "i1();",
      EXPECT_RESULT, Number::New(isolate, 10));
  context.Check(
      "'use strict';"
      "let x2 = 2; i1();",
      EXPECT_RESULT, Number::New(isolate, 12));
}


TEST(GlobalLexicalOSR) {
  i::FLAG_use_strict = true;

  v8::Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  SimpleContext context;

  context.Check("'use strict';"
                "let x = 1; x;",
                EXPECT_RESULT, Number::New(isolate, 1));
  context.Check("'use strict';"
                "let y = 2*x;"
                "++x;"
                "let z = 0;"
                "const limit = 100000;"
                "for (var i = 0; i < limit; ++i) {"
                "  z += x + y;"
                "}"
                "z;",
                EXPECT_RESULT, Number::New(isolate, 400000));
}


TEST(CrossScriptConflicts) {
  i::FLAG_use_strict = true;

  HandleScope scope(CcTest::isolate());

  const char* firsts[] = {"var x = 1; x", "function x() { return 1 }; x()",
                          "let x = 1; x", "const x = 1; x", nullptr};
  const char* seconds[] = {"var x = 2; x", "function x() { return 2 }; x()",
                           "let x = 2; x", "const x = 2; x", nullptr};

  for (int i = 0; firsts[i] != nullptr; ++i) {
    for (int j = 0; seconds[j] != nullptr; ++j) {
      SimpleContext context;
      context.Check(firsts[i], EXPECT_RESULT,
                    Number::New(CcTest::isolate(), 1));
      bool success_case = i < 2 && j < 2;
      Local<Value> success_result;
      if (success_case) success_result = Number::New(CcTest::isolate(), 2);

      context.Check(seconds[j], success_case ? EXPECT_RESULT : EXPECT_EXCEPTION,
                    success_result);
    }
  }
}


TEST(CrossScriptDynamicLookup) {
  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;
    Local<String> undefined_string = String::NewFromUtf8Literal(
        CcTest::isolate(), "undefined", v8::NewStringType::kInternalized);
    Local<String> number_string = String::NewFromUtf8Literal(
        CcTest::isolate(), "number", v8::NewStringType::kInternalized);

    context.Check(
        "function f(o) { with(o) { return x; } }"
        "function g(o) { with(o) { x = 15; } }"
        "function h(o) { with(o) { return typeof x; } }",
        EXPECT_RESULT, Undefined(CcTest::isolate()));
    context.Check("h({})", EXPECT_RESULT, undefined_string);
    context.Check(
        "'use strict';"
        "let x = 1;"
        "f({})",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 1));
    context.Check(
        "'use strict';"
        "g({});0",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 0));
    context.Check("f({})", EXPECT_RESULT, Number::New(CcTest::isolate(), 15));
    context.Check("h({})", EXPECT_RESULT, number_string);
  }
}


TEST(CrossScriptGlobal) {
  HandleScope handle_scope(CcTest::isolate());
  {
    SimpleContext context;

    context.Check(
        "var global = this;"
        "global.x = 255;"
        "x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 255));
    context.Check(
        "'use strict';"
        "let x = 1;"
        "global.x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 255));
    context.Check("global.x = 15; x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 1));
    context.Check("x = 221; global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 15));
    context.Check(
        "z = 15;"
        "function f() { return z; };"
        "for (var k = 0; k < 3; k++) { f(); }"
        "f()",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 15));
    context.Check(
        "'use strict';"
        "let z = 5; f()",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 5));
    context.Check(
        "function f() { konst = 10; return konst; };"
        "f()",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 10));
    context.Check(
        "'use strict';"
        "const konst = 255;"
        "f()",
        EXPECT_EXCEPTION);
  }
}


TEST(CrossScriptStaticLookupUndeclared) {
  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;
    Local<String> undefined_string = String::NewFromUtf8Literal(
        CcTest::isolate(), "undefined", v8::NewStringType::kInternalized);
    Local<String> number_string = String::NewFromUtf8Literal(
        CcTest::isolate(), "number", v8::NewStringType::kInternalized);

    context.Check(
        "function f(o) { return x; }"
        "function g(v) { x = v; }"
        "function h(o) { return typeof x; }",
        EXPECT_RESULT, Undefined(CcTest::isolate()));
    context.Check("h({})", EXPECT_RESULT, undefined_string);
    context.Check(
        "'use strict';"
        "let x = 1;"
        "f({})",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 1));
    context.Check(
        "'use strict';"
        "g(15);x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 15));
    context.Check("h({})", EXPECT_RESULT, number_string);
    context.Check("f({})", EXPECT_RESULT, Number::New(CcTest::isolate(), 15));
    context.Check("h({})", EXPECT_RESULT, number_string);
  }
}


TEST(CrossScriptLoadICs) {
  i::FLAG_allow_natives_syntax = true;

  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;
    context.Check(
        "x = 15;"
        "function f() { return x; };"
        "function g() { return x; };"
        "%PrepareFunctionForOptimization(f);"
        "%PrepareFunctionForOptimization(g);"
        "f()",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 15));
    context.Check(
        "'use strict';"
        "let x = 5;"
        "f()",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 5));
    for (int k = 0; k < 3; k++) {
      context.Check("g()", EXPECT_RESULT, Number::New(CcTest::isolate(), 5));
    }
    for (int k = 0; k < 3; k++) {
      context.Check("f()", EXPECT_RESULT, Number::New(CcTest::isolate(), 5));
    }
    context.Check("%OptimizeFunctionOnNextCall(g); g()", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 5));
    context.Check("%OptimizeFunctionOnNextCall(f); f()", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 5));
  }
  {
    SimpleContext context;
    context.Check(
        "x = 15;"
        "function f() { return x; }"
        "%PrepareFunctionForOptimization(f);"
        "f()",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 15));
    for (int k = 0; k < 3; k++) {
      context.Check("f()", EXPECT_RESULT, Number::New(CcTest::isolate(), 15));
    }
    context.Check("%OptimizeFunctionOnNextCall(f); f()", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 15));
    context.Check(
        "'use strict';"
        "let x = 5;"
        "%PrepareFunctionForOptimization(f);"
        "f()",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 5));
    for (int k = 0; k < 3; k++) {
      context.Check("f()", EXPECT_RESULT, Number::New(CcTest::isolate(), 5));
    }
    context.Check("%OptimizeFunctionOnNextCall(f); f()", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 5));
  }
}


TEST(CrossScriptStoreICs) {
  i::FLAG_allow_natives_syntax = true;

  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;
    context.Check(
        "var global = this;"
        "x = 15;"
        "function f(v) { x = v; };"
        "function g(v) { x = v; };"
        "%PrepareFunctionForOptimization(f);"
        "%PrepareFunctionForOptimization(g);"
        "f(10); x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 10));
    context.Check(
        "'use strict';"
        "let x = 5;"
        "f(7); x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 7));
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 10));
    for (int k = 0; k < 3; k++) {
      context.Check("g(31); x", EXPECT_RESULT,
                    Number::New(CcTest::isolate(), 31));
    }
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 10));
    for (int k = 0; k < 3; k++) {
      context.Check("f(32); x", EXPECT_RESULT,
                    Number::New(CcTest::isolate(), 32));
    }
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 10));
    context.Check("%OptimizeFunctionOnNextCall(g); g(18); x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 18));
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 10));
    context.Check("%OptimizeFunctionOnNextCall(f); f(33); x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 33));
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 10));
  }
  {
    SimpleContext context;
    context.Check(
        "var global = this;"
        "x = 15;"
        "function f(v) { x = v; };"
        "%PrepareFunctionForOptimization(f);"
        "f(10); x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 10));
    for (int k = 0; k < 3; k++) {
      context.Check("f(18); x", EXPECT_RESULT,
                    Number::New(CcTest::isolate(), 18));
    }
    context.Check("%OptimizeFunctionOnNextCall(f); f(20); x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 20));
    context.Check(
        "'use strict';"
        "let x = 5;"
        "f(8); x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 8));
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 20));
    for (int k = 0; k < 3; k++) {
      context.Check("f(13); x", EXPECT_RESULT,
                    Number::New(CcTest::isolate(), 13));
    }
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 20));
    context.Check(
        "%PrepareFunctionForOptimization(f);"
        "%OptimizeFunctionOnNextCall(f); f(41); x",
        EXPECT_RESULT, Number::New(CcTest::isolate(), 41));
    context.Check("global.x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 20));
  }
}


TEST(CrossScriptAssignmentToConst) {
  i::FLAG_allow_natives_syntax = true;

  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;

    context.Check("function f() { x = 27; }", EXPECT_RESULT,
                  Undefined(CcTest::isolate()));
    context.Check("'use strict';const x = 1; x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 1));
    context.Check("%PrepareFunctionForOptimization(f);f();", EXPECT_EXCEPTION);
    context.Check("x", EXPECT_RESULT, Number::New(CcTest::isolate(), 1));
    context.Check("f();", EXPECT_EXCEPTION);
    context.Check("x", EXPECT_RESULT, Number::New(CcTest::isolate(), 1));
    context.Check("%OptimizeFunctionOnNextCall(f);f();", EXPECT_EXCEPTION);
    context.Check("x", EXPECT_RESULT, Number::New(CcTest::isolate(), 1));
  }
}


TEST(Regress425510) {
  i::FLAG_allow_natives_syntax = true;

  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;

    context.Check("'use strict'; o; const o = 10", EXPECT_EXCEPTION);

    for (int i = 0; i < 100; i++) {
      context.Check("o.prototype", EXPECT_EXCEPTION);
    }
  }
}


TEST(Regress3941) {
  i::FLAG_allow_natives_syntax = true;

  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;
    context.Check("function f() { x = 1; }", EXPECT_RESULT,
                  Undefined(CcTest::isolate()));
    context.Check("'use strict'; f(); let x = 2; x", EXPECT_EXCEPTION);
  }


  {
    // Train ICs.
    SimpleContext context;
    context.Check("function f() { x = 1; }", EXPECT_RESULT,
                  Undefined(CcTest::isolate()));
    for (int i = 0; i < 4; i++) {
      context.Check("f(); x", EXPECT_RESULT, Number::New(CcTest::isolate(), 1));
    }
    context.Check("'use strict'; f(); let x = 2; x", EXPECT_EXCEPTION);
  }


  {
    // Optimize.
    SimpleContext context;
    context.Check(
        "function f() { x = 1; };"
        "%PrepareFunctionForOptimization(f);",
        EXPECT_RESULT, Undefined(CcTest::isolate()));
    for (int i = 0; i < 4; i++) {
      context.Check("f(); x", EXPECT_RESULT, Number::New(CcTest::isolate(), 1));
    }
    context.Check("%OptimizeFunctionOnNextCall(f); f(); x", EXPECT_RESULT,
                  Number::New(CcTest::isolate(), 1));

    context.Check("'use strict'; f(); let x = 2; x", EXPECT_EXCEPTION);
  }
}


TEST(Regress3941_Reads) {
  i::FLAG_allow_natives_syntax = true;

  HandleScope handle_scope(CcTest::isolate());

  {
    SimpleContext context;
    context.Check("function f() { return x; }", EXPECT_RESULT,
                  Undefined(CcTest::isolate()));
    context.Check("'use strict'; f(); let x = 2; x", EXPECT_EXCEPTION);
  }


  {
    // Train ICs.
    SimpleContext context;
    context.Check("function f() { return x; }", EXPECT_RESULT,
                  Undefined(CcTest::isolate()));
    for (int i = 0; i < 4; i++) {
      context.Check("f()", EXPECT_EXCEPTION);
    }
    context.Check("'use strict'; f(); let x = 2; x", EXPECT_EXCEPTION);
  }


  {
    // Optimize.
    SimpleContext context;
    context.Check(
        "function f() { return x; };"
        "%PrepareFunctionForOptimization(f);",
        EXPECT_RESULT, Undefined(CcTest::isolate()));
    for (int i = 0; i < 4; i++) {
      context.Check("f()", EXPECT_EXCEPTION);
    }
    context.Check("%OptimizeFunctionOnNextCall(f);", EXPECT_RESULT,
                  Undefined(CcTest::isolate()));

    context.Check("'use strict'; f(); let x = 2; x", EXPECT_EXCEPTION);
  }
}

}  // namespace v8
