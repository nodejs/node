// Copyright 2007-2010 the V8 project authors. All rights reserved.
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

#include <signal.h>

#include <sys/stat.h>

#include "src/v8.h"

#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/bootstrapper.h"
#include "src/compilation-cache.h"
#include "src/compiler.h"
#include "src/debug/debug.h"
#include "src/heap/spaces.h"
#include "src/macro-assembler-inl.h"
#include "src/objects-inl.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/deserializer.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/snapshot.h"
#include "src/snapshot/startup-serializer.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"
#include "test/cctest/setup-isolate-for-tests.h"

using namespace v8::internal;

void DisableAlwaysOpt() {
  // Isolates prepared for serialization do not optimize. The only exception is
  // with the flag --always-opt.
  FLAG_always_opt = false;
}


// TestIsolate is used for testing isolate serialization.
class TestIsolate : public Isolate {
 public:
  static v8::Isolate* NewInitialized(bool enable_serializer) {
    i::Isolate* isolate = new TestIsolate(enable_serializer);
    isolate->setup_delegate_ = new SetupIsolateDelegateForTests();
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Isolate::Scope isolate_scope(v8_isolate);
    isolate->Init(NULL);
    return v8_isolate;
  }
  // Wraps v8::Isolate::New, but with a TestIsolate under the hood.
  // Allows flexibility to bootstrap with or without snapshot even when
  // the production Isolate class has one or the other behavior baked in.
  static v8::Isolate* New(const v8::Isolate::CreateParams& params) {
    i::Isolate* isolate = new TestIsolate(false);
    isolate->setup_delegate_ = new SetupIsolateDelegateForTests();
    return v8::IsolateNewImpl(isolate, params);
  }
  explicit TestIsolate(bool enable_serializer) : Isolate(enable_serializer) {
    set_array_buffer_allocator(CcTest::array_buffer_allocator());
  }
  void CreateSetupDelegateForTests() {
    setup_delegate_ = new SetupIsolateDelegateForTests();
  }
};

static Vector<const byte> WritePayload(const Vector<const byte>& payload) {
  int length = payload.length();
  byte* blob = NewArray<byte>(length);
  memcpy(blob, payload.begin(), length);
  return Vector<const byte>(const_cast<const byte*>(blob), length);
}

static Vector<const byte> Serialize(v8::Isolate* isolate) {
  // We have to create one context.  One reason for this is so that the builtins
  // can be loaded from v8natives.js and their addresses can be processed.  This
  // will clear the pending fixups array, which would otherwise contain GC roots
  // that would confuse the serialization/deserialization process.
  v8::Isolate::Scope isolate_scope(isolate);
  {
    v8::HandleScope scope(isolate);
    v8::Context::New(isolate);
  }

  Isolate* internal_isolate = reinterpret_cast<Isolate*>(isolate);
  internal_isolate->heap()->CollectAllAvailableGarbage(
      i::GarbageCollectionReason::kTesting);
  StartupSerializer ser(internal_isolate,
                        v8::SnapshotCreator::FunctionCodeHandling::kClear);
  ser.SerializeStrongReferences();
  ser.SerializeWeakReferencesAndDeferred();
  SnapshotData snapshot_data(&ser);
  return WritePayload(snapshot_data.RawData());
}


Vector<const uint8_t> ConstructSource(Vector<const uint8_t> head,
                                      Vector<const uint8_t> body,
                                      Vector<const uint8_t> tail, int repeats) {
  int source_length = head.length() + body.length() * repeats + tail.length();
  uint8_t* source = NewArray<uint8_t>(static_cast<size_t>(source_length));
  CopyChars(source, head.start(), head.length());
  for (int i = 0; i < repeats; i++) {
    CopyChars(source + head.length() + i * body.length(), body.start(),
              body.length());
  }
  CopyChars(source + head.length() + repeats * body.length(), tail.start(),
            tail.length());
  return Vector<const uint8_t>(const_cast<const uint8_t*>(source),
                               source_length);
}

v8::Isolate* InitializeFromBlob(Vector<const byte> blob) {
  v8::Isolate* v8_isolate = NULL;
  {
    SnapshotData snapshot_data(blob);
    Deserializer deserializer(&snapshot_data);
    TestIsolate* isolate = new TestIsolate(false);
    v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Isolate::Scope isolate_scope(v8_isolate);
    isolate->CreateSetupDelegateForTests();
    isolate->Init(&deserializer);
  }
  return v8_isolate;
}

static v8::Isolate* Deserialize(Vector<const byte> blob) {
  v8::Isolate* isolate = InitializeFromBlob(blob);
  CHECK(isolate);
  return isolate;
}


static void SanityCheck(v8::Isolate* v8_isolate) {
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  v8::HandleScope scope(v8_isolate);
#ifdef VERIFY_HEAP
  isolate->heap()->Verify();
#endif
  CHECK(isolate->global_object()->IsJSObject());
  CHECK(isolate->native_context()->IsContext());
  CHECK(isolate->heap()->string_table()->IsStringTable());
  isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("Empty"));
}

UNINITIALIZED_TEST(StartupSerializerOnce) {
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  Vector<const byte> blob = Serialize(isolate);
  isolate = Deserialize(blob);
  blob.Dispose();
  {
    v8::HandleScope handle_scope(isolate);
    v8::Isolate::Scope isolate_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck(isolate);
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(StartupSerializerTwice) {
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  Vector<const byte> blob1 = Serialize(isolate);
  Vector<const byte> blob2 = Serialize(isolate);
  blob1.Dispose();
  isolate = Deserialize(blob2);
  blob2.Dispose();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck(isolate);
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(StartupSerializerOnceRunScript) {
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  Vector<const byte> blob = Serialize(isolate);
  isolate = Deserialize(blob);
  blob.Dispose();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);


    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    const char* c_source = "\"1234\".length";
    v8::Local<v8::Script> script = v8_compile(c_source);
    v8::Maybe<int32_t> result = script->Run(isolate->GetCurrentContext())
                                    .ToLocalChecked()
                                    ->Int32Value(isolate->GetCurrentContext());
    CHECK_EQ(4, result.FromJust());
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(StartupSerializerTwiceRunScript) {
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  Vector<const byte> blob1 = Serialize(isolate);
  Vector<const byte> blob2 = Serialize(isolate);
  blob1.Dispose();
  isolate = Deserialize(blob2);
  blob2.Dispose();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    const char* c_source = "\"1234\".length";
    v8::Local<v8::Script> script = v8_compile(c_source);
    v8::Maybe<int32_t> result = script->Run(isolate->GetCurrentContext())
                                    .ToLocalChecked()
                                    ->Int32Value(isolate->GetCurrentContext());
    CHECK_EQ(4, result.FromJust());
  }
  isolate->Dispose();
}

static void PartiallySerializeObject(Vector<const byte>* startup_blob_out,
                                     Vector<const byte>* partial_blob_out) {
  v8::Isolate* v8_isolate = TestIsolate::NewInitialized(true);
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  v8_isolate->Enter();
  {
    Heap* heap = isolate->heap();

    v8::Persistent<v8::Context> env;
    {
      HandleScope scope(isolate);
      env.Reset(v8_isolate, v8::Context::New(v8_isolate));
    }
    CHECK(!env.IsEmpty());
    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Enter();
    }

    heap->CollectAllAvailableGarbage(i::GarbageCollectionReason::kTesting);
    heap->CollectAllAvailableGarbage(i::GarbageCollectionReason::kTesting);

    Object* raw_foo;
    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::String> foo = v8_str("foo");
      CHECK(!foo.IsEmpty());
      raw_foo = *(v8::Utils::OpenHandle(*foo));
    }

    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Exit();
    }
    env.Reset();

    StartupSerializer startup_serializer(
        isolate, v8::SnapshotCreator::FunctionCodeHandling::kClear);
    startup_serializer.SerializeStrongReferences();

    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         v8::SerializeInternalFieldsCallback());
    partial_serializer.Serialize(&raw_foo, false);

    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(&startup_serializer);
    SnapshotData partial_snapshot(&partial_serializer);

    *partial_blob_out = WritePayload(partial_snapshot.RawData());
    *startup_blob_out = WritePayload(startup_snapshot.RawData());
  }
  v8_isolate->Exit();
  v8_isolate->Dispose();
}

UNINITIALIZED_TEST(PartialSerializerObject) {
  DisableAlwaysOpt();
  Vector<const byte> startup_blob;
  Vector<const byte> partial_blob;
  PartiallySerializeObject(&startup_blob, &partial_blob);

  v8::Isolate* v8_isolate = InitializeFromBlob(startup_blob);
  startup_blob.Dispose();
  CHECK(v8_isolate);
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    HandleScope handle_scope(isolate);
    Handle<Object> root;
    // Intentionally empty handle. The deserializer should not come across
    // any references to the global proxy in this test.
    Handle<JSGlobalProxy> global_proxy = Handle<JSGlobalProxy>::null();
    {
      SnapshotData snapshot_data(partial_blob);
      Deserializer deserializer(&snapshot_data);
      root = deserializer
                 .DeserializePartial(isolate, global_proxy,
                                     v8::DeserializeInternalFieldsCallback())
                 .ToHandleChecked();
      CHECK(root->IsString());
    }

    Handle<Object> root2;
    {
      SnapshotData snapshot_data(partial_blob);
      Deserializer deserializer(&snapshot_data);
      root2 = deserializer
                  .DeserializePartial(isolate, global_proxy,
                                      v8::DeserializeInternalFieldsCallback())
                  .ToHandleChecked();
      CHECK(root2->IsString());
      CHECK(root.is_identical_to(root2));
    }
    partial_blob.Dispose();
  }
  v8_isolate->Dispose();
}

static void PartiallySerializeContext(Vector<const byte>* startup_blob_out,
                                      Vector<const byte>* partial_blob_out) {
  v8::Isolate* v8_isolate = TestIsolate::NewInitialized(true);
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  Heap* heap = isolate->heap();
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    v8::Persistent<v8::Context> env;
    {
      HandleScope scope(isolate);
      env.Reset(v8_isolate, v8::Context::New(v8_isolate));
    }
    CHECK(!env.IsEmpty());
    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Enter();
    }

    // If we don't do this then we end up with a stray root pointing at the
    // context even after we have disposed of env.
    heap->CollectAllAvailableGarbage(i::GarbageCollectionReason::kTesting);

    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Exit();
    }

    i::Object* raw_context = *v8::Utils::OpenPersistent(env);

    env.Reset();

    SnapshotByteSink startup_sink;
    StartupSerializer startup_serializer(
        isolate, v8::SnapshotCreator::FunctionCodeHandling::kClear);
    startup_serializer.SerializeStrongReferences();

    SnapshotByteSink partial_sink;
    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         v8::SerializeInternalFieldsCallback());
    partial_serializer.Serialize(&raw_context, false);
    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(&startup_serializer);
    SnapshotData partial_snapshot(&partial_serializer);

    *partial_blob_out = WritePayload(partial_snapshot.RawData());
    *startup_blob_out = WritePayload(startup_snapshot.RawData());
  }
  v8_isolate->Dispose();
}

UNINITIALIZED_TEST(PartialSerializerContext) {
  DisableAlwaysOpt();
  Vector<const byte> startup_blob;
  Vector<const byte> partial_blob;
  PartiallySerializeContext(&startup_blob, &partial_blob);

  v8::Isolate* v8_isolate = InitializeFromBlob(startup_blob);
  CHECK(v8_isolate);
  startup_blob.Dispose();
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    HandleScope handle_scope(isolate);
    Handle<Object> root;
    Handle<JSGlobalProxy> global_proxy =
        isolate->factory()->NewUninitializedJSGlobalProxy(
            JSGlobalProxy::SizeWithEmbedderFields(0));
    {
      SnapshotData snapshot_data(partial_blob);
      Deserializer deserializer(&snapshot_data);
      root = deserializer
                 .DeserializePartial(isolate, global_proxy,
                                     v8::DeserializeInternalFieldsCallback())
                 .ToHandleChecked();
      CHECK(root->IsContext());
      CHECK(Handle<Context>::cast(root)->global_proxy() == *global_proxy);
    }

    Handle<Object> root2;
    {
      SnapshotData snapshot_data(partial_blob);
      Deserializer deserializer(&snapshot_data);
      root2 = deserializer
                  .DeserializePartial(isolate, global_proxy,
                                      v8::DeserializeInternalFieldsCallback())
                  .ToHandleChecked();
      CHECK(root2->IsContext());
      CHECK(!root.is_identical_to(root2));
    }
    partial_blob.Dispose();
  }
  v8_isolate->Dispose();
}

static void PartiallySerializeCustomContext(
    Vector<const byte>* startup_blob_out,
    Vector<const byte>* partial_blob_out) {
  v8::Isolate* v8_isolate = TestIsolate::NewInitialized(true);
  Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    v8::Persistent<v8::Context> env;
    {
      HandleScope scope(isolate);
      env.Reset(v8_isolate, v8::Context::New(v8_isolate));
    }
    CHECK(!env.IsEmpty());
    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Enter();
      // After execution, e's function context refers to the global object.
      CompileRun(
          "var e;"
          "(function() {"
          "  e = function(s) { return eval (s); }"
          "})();"
          "var o = this;"
          "var r = Math.random();"
          "var c = Math.sin(0) + Math.cos(0);"
          "var f = (function(a, b) { return a + b; }).bind(1, 2, 3);"
          "var s = parseInt('12345');");

      Vector<const uint8_t> source = ConstructSource(
          STATIC_CHAR_VECTOR("function g() { return [,"),
          STATIC_CHAR_VECTOR("1,"),
          STATIC_CHAR_VECTOR("];} a = g(); b = g(); b.push(1);"), 100000);
      v8::MaybeLocal<v8::String> source_str = v8::String::NewFromOneByte(
          v8_isolate, source.start(), v8::NewStringType::kNormal,
          source.length());
      CompileRun(source_str.ToLocalChecked());
      source.Dispose();
    }
    // If we don't do this then we end up with a stray root pointing at the
    // context even after we have disposed of env.
    isolate->heap()->CollectAllAvailableGarbage(
        i::GarbageCollectionReason::kTesting);

    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Exit();
    }

    i::Object* raw_context = *v8::Utils::OpenPersistent(env);

    env.Reset();

    SnapshotByteSink startup_sink;
    StartupSerializer startup_serializer(
        isolate, v8::SnapshotCreator::FunctionCodeHandling::kClear);
    startup_serializer.SerializeStrongReferences();

    SnapshotByteSink partial_sink;
    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         v8::SerializeInternalFieldsCallback());
    partial_serializer.Serialize(&raw_context, false);
    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(&startup_serializer);
    SnapshotData partial_snapshot(&partial_serializer);

    *partial_blob_out = WritePayload(partial_snapshot.RawData());
    *startup_blob_out = WritePayload(startup_snapshot.RawData());
  }
  v8_isolate->Dispose();
}

UNINITIALIZED_TEST(PartialSerializerCustomContext) {
  DisableAlwaysOpt();
  Vector<const byte> startup_blob;
  Vector<const byte> partial_blob;
  PartiallySerializeCustomContext(&startup_blob, &partial_blob);

  v8::Isolate* v8_isolate = InitializeFromBlob(startup_blob);
  CHECK(v8_isolate);
  startup_blob.Dispose();
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    HandleScope handle_scope(isolate);
    Handle<Object> root;
    Handle<JSGlobalProxy> global_proxy =
        isolate->factory()->NewUninitializedJSGlobalProxy(
            JSGlobalProxy::SizeWithEmbedderFields(0));
    {
      SnapshotData snapshot_data(partial_blob);
      Deserializer deserializer(&snapshot_data);
      root = deserializer
                 .DeserializePartial(isolate, global_proxy,
                                     v8::DeserializeInternalFieldsCallback())
                 .ToHandleChecked();
      CHECK(root->IsContext());
      Handle<Context> context = Handle<Context>::cast(root);

      // Add context to the weak native context list
      context->set(Context::NEXT_CONTEXT_LINK,
                   isolate->heap()->native_contexts_list(),
                   UPDATE_WEAK_WRITE_BARRIER);
      isolate->heap()->set_native_contexts_list(*context);

      CHECK(context->global_proxy() == *global_proxy);
      Handle<String> o = isolate->factory()->NewStringFromAsciiChecked("o");
      Handle<JSObject> global_object(context->global_object(), isolate);
      Handle<Object> property = JSReceiver::GetDataProperty(global_object, o);
      CHECK(property.is_identical_to(global_proxy));

      v8::Local<v8::Context> v8_context = v8::Utils::ToLocal(context);
      v8::Context::Scope context_scope(v8_context);
      double r = CompileRun("r")
                     ->ToNumber(v8_isolate->GetCurrentContext())
                     .ToLocalChecked()
                     ->Value();
      CHECK(0.0 <= r && r < 1.0);
      // Math.random still works.
      double random = CompileRun("Math.random()")
                          ->ToNumber(v8_isolate->GetCurrentContext())
                          .ToLocalChecked()
                          ->Value();
      CHECK(0.0 <= random && random < 1.0);
      double c = CompileRun("c")
                     ->ToNumber(v8_isolate->GetCurrentContext())
                     .ToLocalChecked()
                     ->Value();
      CHECK_EQ(1, c);
      int f = CompileRun("f()")
                  ->ToNumber(v8_isolate->GetCurrentContext())
                  .ToLocalChecked()
                  ->Int32Value(v8_isolate->GetCurrentContext())
                  .FromJust();
      CHECK_EQ(5, f);
      f = CompileRun("e('f()')")
              ->ToNumber(v8_isolate->GetCurrentContext())
              .ToLocalChecked()
              ->Int32Value(v8_isolate->GetCurrentContext())
              .FromJust();
      CHECK_EQ(5, f);
      v8::Local<v8::String> s = CompileRun("s")
                                    ->ToString(v8_isolate->GetCurrentContext())
                                    .ToLocalChecked();
      CHECK(s->Equals(v8_isolate->GetCurrentContext(), v8_str("12345"))
                .FromJust());
      int a = CompileRun("a.length")
                  ->ToNumber(v8_isolate->GetCurrentContext())
                  .ToLocalChecked()
                  ->Int32Value(v8_isolate->GetCurrentContext())
                  .FromJust();
      CHECK_EQ(100001, a);
      int b = CompileRun("b.length")
                  ->ToNumber(v8_isolate->GetCurrentContext())
                  .ToLocalChecked()
                  ->Int32Value(v8_isolate->GetCurrentContext())
                  .FromJust();
      CHECK_EQ(100002, b);
    }
    partial_blob.Dispose();
  }
  v8_isolate->Dispose();
}

TEST(CustomSnapshotDataBlob1) {
  DisableAlwaysOpt();
  const char* source1 = "function f() { return 42; }";

  v8::StartupData data1 = v8::V8::CreateSnapshotDataBlob(source1);

  v8::Isolate::CreateParams params1;
  params1.snapshot_blob = &data1;
  params1.array_buffer_allocator = CcTest::array_buffer_allocator();

  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate1 = TestIsolate::New(params1);
  {
    v8::Isolate::Scope i_scope(isolate1);
    v8::HandleScope h_scope(isolate1);
    v8::Local<v8::Context> context = v8::Context::New(isolate1);
    delete[] data1.data;  // We can dispose of the snapshot blob now.
    v8::Context::Scope c_scope(context);
    v8::Maybe<int32_t> result =
        CompileRun("f()")->Int32Value(isolate1->GetCurrentContext());
    CHECK_EQ(42, result.FromJust());
    CHECK(CompileRun("this.g")->IsUndefined());
  }
  isolate1->Dispose();
}

TEST(CustomSnapshotDataBlob2) {
  DisableAlwaysOpt();
  const char* source2 =
      "function f() { return g() * 2; }"
      "function g() { return 43; }"
      "/./.test('a')";

  v8::StartupData data2 = v8::V8::CreateSnapshotDataBlob(source2);

  v8::Isolate::CreateParams params2;
  params2.snapshot_blob = &data2;
  params2.array_buffer_allocator = CcTest::array_buffer_allocator();
  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate2 = TestIsolate::New(params2);
  {
    v8::Isolate::Scope i_scope(isolate2);
    v8::HandleScope h_scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    delete[] data2.data;  // We can dispose of the snapshot blob now.
    v8::Context::Scope c_scope(context);
    v8::Maybe<int32_t> result =
        CompileRun("f()")->Int32Value(isolate2->GetCurrentContext());
    CHECK_EQ(86, result.FromJust());
    result = CompileRun("g()")->Int32Value(isolate2->GetCurrentContext());
    CHECK_EQ(43, result.FromJust());
  }
  isolate2->Dispose();
}

static void SerializationFunctionTemplate(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(args[0]);
}

TEST(CustomSnapshotDataBlobOutdatedContextWithOverflow) {
  DisableAlwaysOpt();
  const char* source1 =
      "var o = {};"
      "(function() {"
      "  function f1(x) { return f2(x) instanceof Array; }"
      "  function f2(x) { return foo.bar(x); }"
      "  o.a = f2.bind(null);"
      "  o.b = 1;"
      "  o.c = 2;"
      "  o.d = 3;"
      "  o.e = 4;"
      "})();\n";

  const char* source2 = "o.a(42)";

  v8::StartupData data = v8::V8::CreateSnapshotDataBlob(source1);

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &data;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();

  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate = TestIsolate::New(params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);

    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::ObjectTemplate> property = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::FunctionTemplate> function =
        v8::FunctionTemplate::New(isolate, SerializationFunctionTemplate);
    property->Set(isolate, "bar", function);
    global->Set(isolate, "foo", property);

    v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
    delete[] data.data;  // We can dispose of the snapshot blob now.
    v8::Context::Scope c_scope(context);
    v8::Local<v8::Value> result = CompileRun(source2);
    v8::Maybe<bool> compare = v8_str("42")->Equals(
        v8::Isolate::GetCurrent()->GetCurrentContext(), result);
    CHECK(compare.FromJust());
  }
  isolate->Dispose();
}

TEST(CustomSnapshotDataBlobWithLocker) {
  DisableAlwaysOpt();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate0 = v8::Isolate::New(create_params);
  {
    v8::Locker locker(isolate0);
    v8::Isolate::Scope i_scope(isolate0);
    v8::HandleScope h_scope(isolate0);
    v8::Local<v8::Context> context = v8::Context::New(isolate0);
    v8::Context::Scope c_scope(context);
    v8::Maybe<int32_t> result =
        CompileRun("Math.cos(0)")->Int32Value(isolate0->GetCurrentContext());
    CHECK_EQ(1, result.FromJust());
  }
  isolate0->Dispose();

  const char* source1 = "function f() { return 42; }";

  v8::StartupData data1 = v8::V8::CreateSnapshotDataBlob(source1);

  v8::Isolate::CreateParams params1;
  params1.snapshot_blob = &data1;
  params1.array_buffer_allocator = CcTest::array_buffer_allocator();
  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate1 = TestIsolate::New(params1);
  {
    v8::Locker locker(isolate1);
    v8::Isolate::Scope i_scope(isolate1);
    v8::HandleScope h_scope(isolate1);
    v8::Local<v8::Context> context = v8::Context::New(isolate1);
    delete[] data1.data;  // We can dispose of the snapshot blob now.
    v8::Context::Scope c_scope(context);
    v8::Maybe<int32_t> result = CompileRun("f()")->Int32Value(context);
    CHECK_EQ(42, result.FromJust());
  }
  isolate1->Dispose();
}

TEST(CustomSnapshotDataBlobStackOverflow) {
  DisableAlwaysOpt();
  const char* source =
      "var a = [0];"
      "var b = a;"
      "for (var i = 0; i < 10000; i++) {"
      "  var c = [i];"
      "  b.push(c);"
      "  b.push(c);"
      "  b = c;"
      "}";

  v8::StartupData data = v8::V8::CreateSnapshotDataBlob(source);

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &data;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();

  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate = TestIsolate::New(params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    delete[] data.data;  // We can dispose of the snapshot blob now.
    v8::Context::Scope c_scope(context);
    const char* test =
        "var sum = 0;"
        "while (a) {"
        "  sum += a[0];"
        "  a = a[1];"
        "}"
        "sum";
    v8::Maybe<int32_t> result =
        CompileRun(test)->Int32Value(isolate->GetCurrentContext());
    CHECK_EQ(9999 * 5000, result.FromJust());
  }
  isolate->Dispose();
}

bool IsCompiled(const char* name) {
  return i::Handle<i::JSFunction>::cast(
             v8::Utils::OpenHandle(*CompileRun(name)))
      ->shared()
      ->is_compiled();
}

TEST(SnapshotDataBlobWithWarmup) {
  DisableAlwaysOpt();
  const char* warmup = "Math.abs(1); Math.random = 1;";

  v8::StartupData cold = v8::V8::CreateSnapshotDataBlob();
  v8::StartupData warm = v8::V8::WarmUpSnapshotDataBlob(cold, warmup);
  delete[] cold.data;

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &warm;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();

  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate = TestIsolate::New(params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    delete[] warm.data;
    v8::Context::Scope c_scope(context);
    // Running the warmup script has effect on whether functions are
    // pre-compiled, but does not pollute the context.
    CHECK(IsCompiled("Math.abs"));
    CHECK(!IsCompiled("String.raw"));
    CHECK(CompileRun("Math.random")->IsFunction());
  }
  isolate->Dispose();
}

TEST(CustomSnapshotDataBlobWithWarmup) {
  DisableAlwaysOpt();
  const char* source =
      "function f() { return Math.abs(1); }\n"
      "function g() { return String.raw(1); }\n"
      "Object.valueOf(1);"
      "var a = 5";
  const char* warmup = "a = f()";

  v8::StartupData cold = v8::V8::CreateSnapshotDataBlob(source);
  v8::StartupData warm = v8::V8::WarmUpSnapshotDataBlob(cold, warmup);
  delete[] cold.data;

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &warm;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();

  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate = TestIsolate::New(params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    delete[] warm.data;
    v8::Context::Scope c_scope(context);
    // Running the warmup script has effect on whether functions are
    // pre-compiled, but does not pollute the context.
    CHECK(IsCompiled("f"));
    CHECK(IsCompiled("Math.abs"));
    CHECK(!IsCompiled("g"));
    CHECK(!IsCompiled("String.raw"));
    CHECK(!IsCompiled("Array.prototype.sort"));
    CHECK_EQ(5, CompileRun("a")->Int32Value(context).FromJust());
  }
  isolate->Dispose();
}

TEST(CustomSnapshotDataBlobImmortalImmovableRoots) {
  DisableAlwaysOpt();
  // Flood the startup snapshot with shared function infos. If they are
  // serialized before the immortal immovable root, the root will no longer end
  // up on the first page.
  Vector<const uint8_t> source =
      ConstructSource(STATIC_CHAR_VECTOR("var a = [];"),
                      STATIC_CHAR_VECTOR("a.push(function() {return 7});"),
                      STATIC_CHAR_VECTOR("\0"), 10000);

  v8::StartupData data = v8::V8::CreateSnapshotDataBlob(
      reinterpret_cast<const char*>(source.start()));

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &data;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();

  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate = TestIsolate::New(params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    delete[] data.data;  // We can dispose of the snapshot blob now.
    v8::Context::Scope c_scope(context);
    CHECK_EQ(7, CompileRun("a[0]()")->Int32Value(context).FromJust());
  }
  isolate->Dispose();
  source.Dispose();
}

TEST(TestThatAlwaysSucceeds) {
}


TEST(TestThatAlwaysFails) {
  bool ArtificialFailure = false;
  CHECK(ArtificialFailure);
}


int CountBuiltins() {
  // Check that we have not deserialized any additional builtin.
  HeapIterator iterator(CcTest::heap());
  DisallowHeapAllocation no_allocation;
  int counter = 0;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsCode() && Code::cast(obj)->kind() == Code::BUILTIN) counter++;
  }
  return counter;
}


static Handle<SharedFunctionInfo> CompileScript(
    Isolate* isolate, Handle<String> source, Handle<String> name,
    ScriptData** cached_data, v8::ScriptCompiler::CompileOptions options) {
  return Compiler::GetSharedFunctionInfoForScript(
      source, name, 0, 0, v8::ScriptOriginOptions(), Handle<Object>(),
      Handle<Context>(isolate->native_context()), NULL, cached_data, options,
      NOT_NATIVES_CODE);
}

TEST(CodeSerializerOnePlusOne) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  const char* source = "1 + 1";

  Handle<String> orig_source = isolate->factory()
                                   ->NewStringFromUtf8(CStrVector(source))
                                   .ToHandleChecked();
  Handle<String> copy_source = isolate->factory()
                                   ->NewStringFromUtf8(CStrVector(source))
                                   .ToHandleChecked();
  CHECK(!orig_source.is_identical_to(copy_source));
  CHECK(orig_source->Equals(*copy_source));

  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, orig_source, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  int builtins_count = CountBuiltins();

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, copy_source, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }

  CHECK_NE(*orig, *copy);
  CHECK(Script::cast(copy->script())->source() == *copy_source);

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());
  Handle<JSObject> global(isolate->context()->global_object());
  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();
  CHECK_EQ(2, Handle<Smi>::cast(copy_result)->value());

  CHECK_EQ(builtins_count, CountBuiltins());

  delete cache;
}

TEST(CodeSerializerPromotedToCompilationCache) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();

  v8::HandleScope scope(CcTest::isolate());

  const char* source = "1 + 1";

  Handle<String> src = isolate->factory()
                           ->NewStringFromUtf8(CStrVector(source))
                           .ToHandleChecked();
  ScriptData* cache = NULL;

  CompileScript(isolate, src, src, &cache,
                v8::ScriptCompiler::kProduceCodeCache);

  DisallowCompilation no_compile_expected(isolate);
  Handle<SharedFunctionInfo> copy = CompileScript(
      isolate, src, src, &cache, v8::ScriptCompiler::kConsumeCodeCache);

  InfoVectorPair pair = isolate->compilation_cache()->LookupScript(
      src, src, 0, 0, v8::ScriptOriginOptions(), isolate->native_context(),
      SLOPPY);

  CHECK(pair.shared() == *copy);

  delete cache;
}

TEST(CodeSerializerInternalizedString) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  const char* source = "'string1'";

  Handle<String> orig_source = isolate->factory()
                                   ->NewStringFromUtf8(CStrVector(source))
                                   .ToHandleChecked();
  Handle<String> copy_source = isolate->factory()
                                   ->NewStringFromUtf8(CStrVector(source))
                                   .ToHandleChecked();
  CHECK(!orig_source.is_identical_to(copy_source));
  CHECK(orig_source->Equals(*copy_source));

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, orig_source, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);
  Handle<JSFunction> orig_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          orig, isolate->native_context());
  Handle<Object> orig_result =
      Execution::Call(isolate, orig_fun, global, 0, NULL).ToHandleChecked();
  CHECK(orig_result->IsInternalizedString());

  int builtins_count = CountBuiltins();

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, copy_source, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);
  CHECK(Script::cast(copy->script())->source() == *copy_source);

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());
  CHECK_NE(*orig_fun, *copy_fun);
  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();
  CHECK(orig_result.is_identical_to(copy_result));
  Handle<String> expected =
      isolate->factory()->NewStringFromAsciiChecked("string1");

  CHECK(Handle<String>::cast(copy_result)->Equals(*expected));
  CHECK_EQ(builtins_count, CountBuiltins());

  delete cache;
}

TEST(CodeSerializerLargeCodeObject) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  // The serializer only tests the shared code, which is always the unoptimized
  // code. Don't even bother generating optimized code to avoid timeouts.
  FLAG_always_opt = false;

  Vector<const uint8_t> source =
      ConstructSource(STATIC_CHAR_VECTOR("var j=1; if (j == 0) {"),
                      STATIC_CHAR_VECTOR("for (let i of Object.prototype);"),
                      STATIC_CHAR_VECTOR("} j=7; j"), 1100);
  Handle<String> source_str =
      isolate->factory()->NewStringFromOneByte(source).ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_str, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  CHECK(isolate->heap()->InSpace(orig->abstract_code(), LO_SPACE));

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, source_str, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());

  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();

  int result_int;
  CHECK(copy_result->ToInt32(&result_int));
  CHECK_EQ(7, result_int);

  delete cache;
  source.Dispose();
}

TEST(CodeSerializerLargeCodeObjectWithIncrementalMarking) {
  FLAG_serialize_toplevel = true;
  FLAG_always_opt = false;
  // This test relies on (full-codegen) code objects going to large object
  // space. Once FCG goes away, it must either be redesigned (to put some
  // other large deserialized object into LO space), or it can be deleted.
  FLAG_ignition = false;
  const char* filter_flag = "--turbo-filter=NOTHING";
  FlagList::SetFlagsFromString(filter_flag, StrLength(filter_flag));
  FLAG_black_allocation = true;
  FLAG_manual_evacuation_candidates_selection = true;

  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  Vector<const uint8_t> source = ConstructSource(
      STATIC_CHAR_VECTOR("var j=1; if (j == 0) {"),
      STATIC_CHAR_VECTOR("for (var i = 0; i < Object.prototype; i++);"),
      STATIC_CHAR_VECTOR("} j=7; var s = 'happy_hippo'; j"), 2100);
  Handle<String> source_str =
      isolate->factory()->NewStringFromOneByte(source).ToHandleChecked();

  // Create a string on an evacuation candidate in old space.
  Handle<String> moving_object;
  Page* ec_page;
  {
    AlwaysAllocateScope always_allocate(isolate);
    heap::SimulateFullSpace(heap->old_space());
    moving_object = isolate->factory()->InternalizeString(
        isolate->factory()->NewStringFromAsciiChecked("happy_hippo"));
    ec_page = Page::FromAddress(moving_object->address());
  }

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_str, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  CHECK(heap->InSpace(orig->abstract_code(), LO_SPACE));

  // Pretend that incremental marking is on when deserialization begins.
  heap::ForceEvacuationCandidate(ec_page);
  heap::SimulateIncrementalMarking(heap, false);
  IncrementalMarking* marking = heap->incremental_marking();
  marking->StartBlackAllocationForTesting();
  CHECK(marking->IsCompacting());
  CHECK(MarkCompactCollector::IsOnEvacuationCandidate(*moving_object));

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, source_str, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);

  // We should have missed a write barrier. Complete incremental marking
  // to flush out the bug.
  heap::SimulateIncrementalMarking(heap, true);
  CcTest::CollectAllGarbage();

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());

  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();

  int result_int;
  CHECK(copy_result->ToInt32(&result_int));
  CHECK_EQ(7, result_int);

  delete cache;
  source.Dispose();
}
TEST(CodeSerializerLargeStrings) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* f = isolate->factory();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  Vector<const uint8_t> source_s = ConstructSource(
      STATIC_CHAR_VECTOR("var s = \""), STATIC_CHAR_VECTOR("abcdef"),
      STATIC_CHAR_VECTOR("\";"), 1000000);
  Vector<const uint8_t> source_t = ConstructSource(
      STATIC_CHAR_VECTOR("var t = \""), STATIC_CHAR_VECTOR("uvwxyz"),
      STATIC_CHAR_VECTOR("\"; s + t"), 999999);
  Handle<String> source_str =
      f->NewConsString(f->NewStringFromOneByte(source_s).ToHandleChecked(),
                       f->NewStringFromOneByte(source_t).ToHandleChecked())
          .ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_str, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, source_str, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());

  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();

  CHECK_EQ(6 * 1999999, Handle<String>::cast(copy_result)->length());
  Handle<Object> property = JSReceiver::GetDataProperty(
      isolate->global_object(), f->NewStringFromAsciiChecked("s"));
  CHECK(isolate->heap()->InSpace(HeapObject::cast(*property), LO_SPACE));
  property = JSReceiver::GetDataProperty(isolate->global_object(),
                                         f->NewStringFromAsciiChecked("t"));
  CHECK(isolate->heap()->InSpace(HeapObject::cast(*property), LO_SPACE));
  // Make sure we do not serialize too much, e.g. include the source string.
  CHECK_LT(cache->length(), 13000000);

  delete cache;
  source_s.Dispose();
  source_t.Dispose();
}

TEST(CodeSerializerThreeBigStrings) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* f = isolate->factory();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  Vector<const uint8_t> source_a =
      ConstructSource(STATIC_CHAR_VECTOR("var a = \""), STATIC_CHAR_VECTOR("a"),
                      STATIC_CHAR_VECTOR("\";"), 700000);
  Handle<String> source_a_str =
      f->NewStringFromOneByte(source_a).ToHandleChecked();

  Vector<const uint8_t> source_b =
      ConstructSource(STATIC_CHAR_VECTOR("var b = \""), STATIC_CHAR_VECTOR("b"),
                      STATIC_CHAR_VECTOR("\";"), 400000);
  Handle<String> source_b_str =
      f->NewStringFromOneByte(source_b).ToHandleChecked();

  Vector<const uint8_t> source_c =
      ConstructSource(STATIC_CHAR_VECTOR("var c = \""), STATIC_CHAR_VECTOR("c"),
                      STATIC_CHAR_VECTOR("\";"), 400000);
  Handle<String> source_c_str =
      f->NewStringFromOneByte(source_c).ToHandleChecked();

  Handle<String> source_str =
      f->NewConsString(
             f->NewConsString(source_a_str, source_b_str).ToHandleChecked(),
             source_c_str).ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_str, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, source_str, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());

  USE(Execution::Call(isolate, copy_fun, global, 0, NULL));

  v8::Maybe<int32_t> result =
      CompileRun("(a + b).length")
          ->Int32Value(v8::Isolate::GetCurrent()->GetCurrentContext());
  CHECK_EQ(400000 + 700000, result.FromJust());
  result = CompileRun("(b + c).length")
               ->Int32Value(v8::Isolate::GetCurrent()->GetCurrentContext());
  CHECK_EQ(400000 + 400000, result.FromJust());
  Heap* heap = isolate->heap();
  v8::Local<v8::String> result_str =
      CompileRun("a")
          ->ToString(CcTest::isolate()->GetCurrentContext())
          .ToLocalChecked();
  CHECK(heap->InSpace(*v8::Utils::OpenHandle(*result_str), LO_SPACE));
  result_str = CompileRun("b")
                   ->ToString(CcTest::isolate()->GetCurrentContext())
                   .ToLocalChecked();
  CHECK(heap->InSpace(*v8::Utils::OpenHandle(*result_str), OLD_SPACE));
  result_str = CompileRun("c")
                   ->ToString(CcTest::isolate()->GetCurrentContext())
                   .ToLocalChecked();
  CHECK(heap->InSpace(*v8::Utils::OpenHandle(*result_str), OLD_SPACE));

  delete cache;
  source_a.Dispose();
  source_b.Dispose();
  source_c.Dispose();
}


class SerializerOneByteResource
    : public v8::String::ExternalOneByteStringResource {
 public:
  SerializerOneByteResource(const char* data, size_t length)
      : data_(data), length_(length) {}
  virtual const char* data() const { return data_; }
  virtual size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};


class SerializerTwoByteResource : public v8::String::ExternalStringResource {
 public:
  SerializerTwoByteResource(const char* data, size_t length)
      : data_(AsciiToTwoByteString(data)), length_(length) {}
  ~SerializerTwoByteResource() { DeleteArray<const uint16_t>(data_); }

  virtual const uint16_t* data() const { return data_; }
  virtual size_t length() const { return length_; }

 private:
  const uint16_t* data_;
  size_t length_;
};

TEST(CodeSerializerExternalString) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  // Obtain external internalized one-byte string.
  SerializerOneByteResource one_byte_resource("one_byte", 8);
  Handle<String> one_byte_string =
      isolate->factory()->NewStringFromAsciiChecked("one_byte");
  one_byte_string = isolate->factory()->InternalizeString(one_byte_string);
  one_byte_string->MakeExternal(&one_byte_resource);
  CHECK(one_byte_string->IsExternalOneByteString());
  CHECK(one_byte_string->IsInternalizedString());

  // Obtain external internalized two-byte string.
  SerializerTwoByteResource two_byte_resource("two_byte", 8);
  Handle<String> two_byte_string =
      isolate->factory()->NewStringFromAsciiChecked("two_byte");
  two_byte_string = isolate->factory()->InternalizeString(two_byte_string);
  two_byte_string->MakeExternal(&two_byte_resource);
  CHECK(two_byte_string->IsExternalTwoByteString());
  CHECK(two_byte_string->IsInternalizedString());

  const char* source =
      "var o = {}               \n"
      "o.one_byte = 7;          \n"
      "o.two_byte = 8;          \n"
      "o.one_byte + o.two_byte; \n";
  Handle<String> source_string = isolate->factory()
                                     ->NewStringFromUtf8(CStrVector(source))
                                     .ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_string, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, source_string, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());

  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();

  CHECK_EQ(15.0, copy_result->Number());

  delete cache;
}

TEST(CodeSerializerLargeExternalString) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  Factory* f = isolate->factory();

  v8::HandleScope scope(CcTest::isolate());

  // Create a huge external internalized string to use as variable name.
  Vector<const uint8_t> string =
      ConstructSource(STATIC_CHAR_VECTOR(""), STATIC_CHAR_VECTOR("abcdef"),
                      STATIC_CHAR_VECTOR(""), 999999);
  Handle<String> name = f->NewStringFromOneByte(string).ToHandleChecked();
  SerializerOneByteResource one_byte_resource(
      reinterpret_cast<const char*>(string.start()), string.length());
  name = f->InternalizeString(name);
  name->MakeExternal(&one_byte_resource);
  CHECK(name->IsExternalOneByteString());
  CHECK(name->IsInternalizedString());
  CHECK(isolate->heap()->InSpace(*name, LO_SPACE));

  // Create the source, which is "var <literal> = 42; <literal>".
  Handle<String> source_str =
      f->NewConsString(
             f->NewConsString(f->NewStringFromAsciiChecked("var "), name)
                 .ToHandleChecked(),
             f->NewConsString(f->NewStringFromAsciiChecked(" = 42; "), name)
                 .ToHandleChecked()).ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_str, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, source_str, Handle<String>(), &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);

  Handle<JSFunction> copy_fun =
      f->NewFunctionFromSharedFunctionInfo(copy, isolate->native_context());

  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();

  CHECK_EQ(42.0, copy_result->Number());

  delete cache;
  string.Dispose();
}

TEST(CodeSerializerExternalScriptName) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  Factory* f = isolate->factory();

  v8::HandleScope scope(CcTest::isolate());

  const char* source =
      "var a = [1, 2, 3, 4];"
      "a.reduce(function(x, y) { return x + y }, 0)";

  Handle<String> source_string =
      f->NewStringFromUtf8(CStrVector(source)).ToHandleChecked();

  const SerializerOneByteResource one_byte_resource("one_byte", 8);
  Handle<String> name =
      f->NewExternalStringFromOneByte(&one_byte_resource).ToHandleChecked();
  CHECK(name->IsExternalOneByteString());
  CHECK(!name->IsInternalizedString());

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_string, name, &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = CompileScript(isolate, source_string, name, &cache,
                         v8::ScriptCompiler::kConsumeCodeCache);
  }
  CHECK_NE(*orig, *copy);

  Handle<JSFunction> copy_fun =
      f->NewFunctionFromSharedFunctionInfo(copy, isolate->native_context());

  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();

  CHECK_EQ(10.0, copy_result->Number());

  delete cache;
}


static bool toplevel_test_code_event_found = false;


static void SerializerCodeEventListener(const v8::JitCodeEvent* event) {
  if (event->type == v8::JitCodeEvent::CODE_ADDED &&
      memcmp(event->name.str, "Script:~test", 12) == 0) {
    toplevel_test_code_event_found = true;
  }
}


v8::ScriptCompiler::CachedData* ProduceCache(const char* source) {
  v8::ScriptCompiler::CachedData* cache;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope iscope(isolate1);
    v8::HandleScope scope(isolate1);
    v8::Local<v8::Context> context = v8::Context::New(isolate1);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source_str = v8_str(source);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin);
    v8::Local<v8::UnboundScript> script =
        v8::ScriptCompiler::CompileUnboundScript(
            isolate1, &source, v8::ScriptCompiler::kProduceCodeCache)
            .ToLocalChecked();
    const v8::ScriptCompiler::CachedData* data = source.GetCachedData();
    CHECK(data);
    // Persist cached data.
    uint8_t* buffer = NewArray<uint8_t>(data->length);
    MemCopy(buffer, data->data, data->length);
    cache = new v8::ScriptCompiler::CachedData(
        buffer, data->length, v8::ScriptCompiler::CachedData::BufferOwned);

    v8::Local<v8::Value> result = script->BindToCurrentContext()
                                      ->Run(isolate1->GetCurrentContext())
                                      .ToLocalChecked();
    v8::Local<v8::String> result_string =
        result->ToString(isolate1->GetCurrentContext()).ToLocalChecked();
    CHECK(result_string->Equals(isolate1->GetCurrentContext(), v8_str("abcdef"))
              .FromJust());
  }
  isolate1->Dispose();
  return cache;
}

TEST(CodeSerializerIsolates) {
  FLAG_serialize_toplevel = true;

  const char* source = "function f() { return 'abc'; }; f() + 'def'";
  v8::ScriptCompiler::CachedData* cache = ProduceCache(source);

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  isolate2->SetJitCodeEventHandler(v8::kJitCodeEventDefault,
                                   SerializerCodeEventListener);
  toplevel_test_code_event_found = false;
  {
    v8::Isolate::Scope iscope(isolate2);
    v8::HandleScope scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source_str = v8_str(source);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin, cache);
    v8::Local<v8::UnboundScript> script;
    {
      DisallowCompilation no_compile(reinterpret_cast<Isolate*>(isolate2));
      script = v8::ScriptCompiler::CompileUnboundScript(
                   isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache)
                   .ToLocalChecked();
    }
    CHECK(!cache->rejected);
    v8::Local<v8::Value> result = script->BindToCurrentContext()
                                      ->Run(isolate2->GetCurrentContext())
                                      .ToLocalChecked();
    CHECK(result->ToString(isolate2->GetCurrentContext())
              .ToLocalChecked()
              ->Equals(isolate2->GetCurrentContext(), v8_str("abcdef"))
              .FromJust());
  }
  CHECK(toplevel_test_code_event_found);
  isolate2->Dispose();
}

TEST(CodeSerializerFlagChange) {
  FLAG_serialize_toplevel = true;

  const char* source = "function f() { return 'abc'; }; f() + 'def'";
  v8::ScriptCompiler::CachedData* cache = ProduceCache(source);

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);

  FLAG_allow_natives_syntax = true;  // Flag change should trigger cache reject.
  FlagList::EnforceFlagImplications();
  {
    v8::Isolate::Scope iscope(isolate2);
    v8::HandleScope scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source_str = v8_str(source);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin, cache);
    v8::ScriptCompiler::CompileUnboundScript(
        isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache)
        .ToLocalChecked();
    CHECK(cache->rejected);
  }
  isolate2->Dispose();
}

TEST(CodeSerializerBitFlip) {
  FLAG_serialize_toplevel = true;

  const char* source = "function f() { return 'abc'; }; f() + 'def'";
  v8::ScriptCompiler::CachedData* cache = ProduceCache(source);

  // Random bit flip.
  const_cast<uint8_t*>(cache->data)[337] ^= 0x40;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope iscope(isolate2);
    v8::HandleScope scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source_str = v8_str(source);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin, cache);
    v8::ScriptCompiler::CompileUnboundScript(
        isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache)
        .ToLocalChecked();
    CHECK(cache->rejected);
  }
  isolate2->Dispose();
}

TEST(CodeSerializerWithHarmonyScoping) {
  FLAG_serialize_toplevel = true;

  const char* source1 = "'use strict'; let x = 'X'";
  const char* source2 = "'use strict'; let y = 'Y'";
  const char* source3 = "'use strict'; x + y";

  v8::ScriptCompiler::CachedData* cache;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope iscope(isolate1);
    v8::HandleScope scope(isolate1);
    v8::Local<v8::Context> context = v8::Context::New(isolate1);
    v8::Context::Scope context_scope(context);

    CompileRun(source1);
    CompileRun(source2);

    v8::Local<v8::String> source_str = v8_str(source3);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin);
    v8::Local<v8::UnboundScript> script =
        v8::ScriptCompiler::CompileUnboundScript(
            isolate1, &source, v8::ScriptCompiler::kProduceCodeCache)
            .ToLocalChecked();
    const v8::ScriptCompiler::CachedData* data = source.GetCachedData();
    CHECK(data);
    // Persist cached data.
    uint8_t* buffer = NewArray<uint8_t>(data->length);
    MemCopy(buffer, data->data, data->length);
    cache = new v8::ScriptCompiler::CachedData(
        buffer, data->length, v8::ScriptCompiler::CachedData::BufferOwned);

    v8::Local<v8::Value> result = script->BindToCurrentContext()
                                      ->Run(isolate1->GetCurrentContext())
                                      .ToLocalChecked();
    v8::Local<v8::String> result_str =
        result->ToString(isolate1->GetCurrentContext()).ToLocalChecked();
    CHECK(result_str->Equals(isolate1->GetCurrentContext(), v8_str("XY"))
              .FromJust());
  }
  isolate1->Dispose();

  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope iscope(isolate2);
    v8::HandleScope scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    v8::Context::Scope context_scope(context);

    // Reverse order of prior running scripts.
    CompileRun(source2);
    CompileRun(source1);

    v8::Local<v8::String> source_str = v8_str(source3);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin, cache);
    v8::Local<v8::UnboundScript> script;
    {
      DisallowCompilation no_compile(reinterpret_cast<Isolate*>(isolate2));
      script = v8::ScriptCompiler::CompileUnboundScript(
                   isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache)
                   .ToLocalChecked();
    }
    v8::Local<v8::Value> result = script->BindToCurrentContext()
                                      ->Run(isolate2->GetCurrentContext())
                                      .ToLocalChecked();
    v8::Local<v8::String> result_str =
        result->ToString(isolate2->GetCurrentContext()).ToLocalChecked();
    CHECK(result_str->Equals(isolate2->GetCurrentContext(), v8_str("XY"))
              .FromJust());
  }
  isolate2->Dispose();
}

TEST(CodeSerializerEagerCompilationAndPreAge) {
  if (FLAG_ignition || FLAG_turbo) return;

  FLAG_lazy = true;
  FLAG_serialize_toplevel = true;
  FLAG_serialize_age_code = true;
  FLAG_serialize_eager = true;

  static const char* source =
      "function f() {"
      "  function g() {"
      "    return 1;"
      "  }"
      "  return g();"
      "}"
      "'abcdef';";

  v8::ScriptCompiler::CachedData* cache = ProduceCache(source);

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope iscope(isolate2);
    v8::HandleScope scope(isolate2);
    v8::Local<v8::Context> context = v8::Context::New(isolate2);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source_str = v8_str(source);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin, cache);
    v8::Local<v8::UnboundScript> unbound =
        v8::ScriptCompiler::CompileUnboundScript(
            isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(!cache->rejected);

    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate2);
    HandleScope i_scope(i_isolate);
    Handle<SharedFunctionInfo> toplevel = v8::Utils::OpenHandle(*unbound);
    Handle<Script> script(Script::cast(toplevel->script()));
    // Every function has been pre-compiled from the code cache.
    int count = 0;
    SharedFunctionInfo::ScriptIterator iterator(script);
    while (SharedFunctionInfo* shared = iterator.Next()) {
      CHECK(shared->is_compiled());
      CHECK_EQ(Code::kPreAgedCodeAge, shared->code()->GetAge());
      count++;
    }
    CHECK_EQ(3, count);
  }
  isolate2->Dispose();
}

TEST(Regress503552) {
  if (!FLAG_incremental_marking) return;
  // Test that the code serializer can deal with weak cells that form a linked
  // list during incremental marking.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  HandleScope scope(isolate);
  Handle<String> source = isolate->factory()->NewStringFromAsciiChecked(
      "function f() {} function g() {}");
  ScriptData* script_data = NULL;
  Handle<SharedFunctionInfo> shared = Compiler::GetSharedFunctionInfoForScript(
      source, Handle<String>(), 0, 0, v8::ScriptOriginOptions(),
      Handle<Object>(), Handle<Context>(isolate->native_context()), NULL,
      &script_data, v8::ScriptCompiler::kProduceCodeCache, NOT_NATIVES_CODE);
  delete script_data;

  heap::SimulateIncrementalMarking(isolate->heap());

  script_data = CodeSerializer::Serialize(isolate, shared, source);
  delete script_data;
}

#if V8_TARGET_ARCH_X64
TEST(CodeSerializerCell) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  size_t actual_size;
  byte* buffer = static_cast<byte*>(v8::base::OS::Allocate(
      Assembler::kMinimalBufferSize, &actual_size, true));
  CHECK(buffer);
  HandleScope handles(isolate);

  MacroAssembler assembler(isolate, buffer, static_cast<int>(actual_size),
                           v8::internal::CodeObjectRequired::kYes);
  assembler.enable_serializer();
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(0.3);
  CHECK(isolate->heap()->InNewSpace(*number));
  Handle<Code> code;
  {
    MacroAssembler* masm = &assembler;
    Handle<Cell> cell = isolate->factory()->NewCell(number);
    masm->Move(rax, cell, RelocInfo::CELL);
    masm->movp(rax, Operand(rax, 0));
    masm->ret(0);
    CodeDesc desc;
    masm->GetCode(&desc);
    code = isolate->factory()->NewCode(desc, Code::ComputeFlags(Code::FUNCTION),
                                       masm->CodeObject());
    code->set_has_reloc_info_for_serialization(true);
  }
  RelocIterator rit1(*code, 1 << RelocInfo::CELL);
  CHECK_EQ(*number, rit1.rinfo()->target_cell()->value());

  Handle<String> source = isolate->factory()->empty_string();
  Handle<SharedFunctionInfo> sfi =
      isolate->factory()->NewSharedFunctionInfo(source, code, false);
  ScriptData* script_data = CodeSerializer::Serialize(isolate, sfi, source);

  Handle<SharedFunctionInfo> copy =
      CodeSerializer::Deserialize(isolate, script_data, source)
          .ToHandleChecked();
  RelocIterator rit2(copy->code(), 1 << RelocInfo::CELL);
  CHECK(rit2.rinfo()->target_cell()->IsCell());
  Handle<Cell> cell(rit2.rinfo()->target_cell());
  CHECK(cell->value()->IsHeapNumber());
  CHECK_EQ(0.3, HeapNumber::cast(cell->value())->value());

  delete script_data;
}
#endif  // V8_TARGET_ARCH_X64

TEST(CodeSerializerEmbeddedObject) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.
  v8::HandleScope scope(CcTest::isolate());

  size_t actual_size;
  byte* buffer = static_cast<byte*>(v8::base::OS::Allocate(
      Assembler::kMinimalBufferSize, &actual_size, true));
  CHECK(buffer);
  HandleScope handles(isolate);

  MacroAssembler assembler(isolate, buffer, static_cast<int>(actual_size),
                           v8::internal::CodeObjectRequired::kYes);
  assembler.enable_serializer();
  Handle<Object> number = isolate->factory()->NewHeapNumber(0.3);
  CHECK(isolate->heap()->InNewSpace(*number));
  Handle<Code> code;
  {
    MacroAssembler* masm = &assembler;
    masm->Push(number);
    CodeDesc desc;
    masm->GetCode(&desc);
    code = isolate->factory()->NewCode(desc, Code::ComputeFlags(Code::FUNCTION),
                                       masm->CodeObject());
    code->set_has_reloc_info_for_serialization(true);
  }
  RelocIterator rit1(*code, RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT));
  CHECK_EQ(*number, rit1.rinfo()->target_object());

  Handle<String> source = isolate->factory()->empty_string();
  Handle<SharedFunctionInfo> sfi =
      isolate->factory()->NewSharedFunctionInfo(source, code, false);
  ScriptData* script_data = CodeSerializer::Serialize(isolate, sfi, source);

  Handle<SharedFunctionInfo> copy =
      CodeSerializer::Deserialize(isolate, script_data, source)
          .ToHandleChecked();
  RelocIterator rit2(copy->code(),
                     RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT));
  CHECK(rit2.rinfo()->target_object()->IsHeapNumber());
  CHECK_EQ(0.3, HeapNumber::cast(rit2.rinfo()->target_object())->value());

  CcTest::CollectAllAvailableGarbage();

  RelocIterator rit3(copy->code(),
                     RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT));
  CHECK(rit3.rinfo()->target_object()->IsHeapNumber());
  CHECK_EQ(0.3, HeapNumber::cast(rit3.rinfo()->target_object())->value());

  delete script_data;
}

TEST(SnapshotCreatorMultipleContexts) {
  DisableAlwaysOpt();
  v8::StartupData blob;
  {
    v8::SnapshotCreator creator;
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      CompileRun("var f = function() { return 1; }");
      creator.SetDefaultContext(context);
    }
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      CompileRun("var f = function() { return 2; }");
      CHECK_EQ(0u, creator.AddContext(context));
    }
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      CHECK_EQ(1u, creator.AddContext(context));
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &blob;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();
  // Test-appropriate equivalent of v8::Isolate::New.
  v8::Isolate* isolate = TestIsolate::New(params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 1);
    }
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context =
          v8::Context::FromSnapshot(isolate, 0).ToLocalChecked();
      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 2);
    }
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context =
          v8::Context::FromSnapshot(isolate, 1).ToLocalChecked();
      v8::Context::Scope context_scope(context);
      ExpectUndefined("this.f");
    }
  }

  isolate->Dispose();
  delete[] blob.data;
}

static void SerializedCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8_num(42));
}

static void SerializedCallbackReplacement(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8_num(1337));
}

static void NamedPropertyGetterForSerialization(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (name->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("x"))
          .FromJust()) {
    info.GetReturnValue().Set(v8_num(2016));
  }
}

static void AccessorForSerialization(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(v8_num(2017));
}

static int serialized_static_field = 314;

class SerializedExtension : public v8::Extension {
 public:
  SerializedExtension()
      : v8::Extension("serialized extension",
                      "native function g();"
                      "function h() { return 13; };"
                      "function i() { return 14; };"
                      "var o = { p: 7 };") {}

  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) {
    CHECK(name->Equals(isolate->GetCurrentContext(), v8_str("g")).FromJust());
    return v8::FunctionTemplate::New(isolate, FunctionCallback);
  }

  static void FunctionCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(v8_num(12));
  }
};

intptr_t original_external_references[] = {
    reinterpret_cast<intptr_t>(SerializedCallback),
    reinterpret_cast<intptr_t>(&serialized_static_field),
    reinterpret_cast<intptr_t>(&NamedPropertyGetterForSerialization),
    reinterpret_cast<intptr_t>(&AccessorForSerialization),
    reinterpret_cast<intptr_t>(&SerializedExtension::FunctionCallback),
    reinterpret_cast<intptr_t>(&serialized_static_field),  // duplicate entry
    0};

intptr_t replaced_external_references[] = {
    reinterpret_cast<intptr_t>(SerializedCallbackReplacement),
    reinterpret_cast<intptr_t>(&serialized_static_field),
    reinterpret_cast<intptr_t>(&NamedPropertyGetterForSerialization),
    reinterpret_cast<intptr_t>(&AccessorForSerialization),
    reinterpret_cast<intptr_t>(&SerializedExtension::FunctionCallback),
    reinterpret_cast<intptr_t>(&serialized_static_field),
    0};

TEST(SnapshotCreatorExternalReferences) {
  DisableAlwaysOpt();
  v8::StartupData blob;
  {
    v8::SnapshotCreator creator(original_external_references);
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      v8::Local<v8::FunctionTemplate> callback =
          v8::FunctionTemplate::New(isolate, SerializedCallback);
      v8::Local<v8::Value> function =
          callback->GetFunction(context).ToLocalChecked();
      CHECK(context->Global()->Set(context, v8_str("f"), function).FromJust());
      ExpectInt32("f()", 42);
      creator.SetDefaultContext(context);
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  // Deserialize with the original external reference.
  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = original_external_references;
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 42);
    }
    isolate->Dispose();
  }

  // Deserialize with the some other external reference.
  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = replaced_external_references;
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 1337);
    }
    isolate->Dispose();
  }
  delete[] blob.data;
}

TEST(SnapshotCreatorUnknownExternalReferences) {
  DisableAlwaysOpt();
  v8::SnapshotCreator creator;
  v8::Isolate* isolate = creator.GetIsolate();
  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::FunctionTemplate> callback =
        v8::FunctionTemplate::New(isolate, SerializedCallback);
    v8::Local<v8::Value> function =
        callback->GetFunction(context).ToLocalChecked();
    CHECK(context->Global()->Set(context, v8_str("f"), function).FromJust());
    ExpectInt32("f()", 42);

    creator.SetDefaultContext(context);
  }
  v8::StartupData blob =
      creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);

  delete[] blob.data;
}

struct InternalFieldData {
  uint32_t data;
};

v8::StartupData SerializeInternalFields(v8::Local<v8::Object> holder, int index,
                                        void* data) {
  CHECK_EQ(reinterpret_cast<void*>(2016), data);
  InternalFieldData* embedder_field = static_cast<InternalFieldData*>(
      holder->GetAlignedPointerFromInternalField(index));
  int size = sizeof(*embedder_field);
  char* payload = new char[size];
  // We simply use memcpy to serialize the content.
  memcpy(payload, embedder_field, size);
  return {payload, size};
}

std::vector<InternalFieldData*> deserialized_data;

void DeserializeInternalFields(v8::Local<v8::Object> holder, int index,
                               v8::StartupData payload, void* data) {
  CHECK_EQ(reinterpret_cast<void*>(2017), data);
  InternalFieldData* embedder_field = new InternalFieldData{0};
  memcpy(embedder_field, payload.data, payload.raw_size);
  holder->SetAlignedPointerInInternalField(index, embedder_field);
  deserialized_data.push_back(embedder_field);
}

TEST(SnapshotCreatorTemplates) {
  DisableAlwaysOpt();
  v8::StartupData blob;

  {
    InternalFieldData* a1 = new InternalFieldData{11};
    InternalFieldData* b0 = new InternalFieldData{20};
    InternalFieldData* c0 = new InternalFieldData{30};

    v8::SnapshotCreator creator(original_external_references);
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::ExtensionConfiguration* no_extension = nullptr;
      v8::Local<v8::ObjectTemplate> global_template =
          v8::ObjectTemplate::New(isolate);
      v8::Local<v8::FunctionTemplate> callback =
          v8::FunctionTemplate::New(isolate, SerializedCallback);
      global_template->Set(v8_str("f"), callback);
      v8::Local<v8::Context> context =
          v8::Context::New(isolate, no_extension, global_template);
      creator.SetDefaultContext(context);
      context = v8::Context::New(isolate, no_extension, global_template);
      v8::Local<v8::ObjectTemplate> object_template =
          v8::ObjectTemplate::New(isolate);
      object_template->SetInternalFieldCount(3);

      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 42);

      v8::Local<v8::Object> a =
          object_template->NewInstance(context).ToLocalChecked();
      v8::Local<v8::Object> b =
          object_template->NewInstance(context).ToLocalChecked();
      v8::Local<v8::Object> c =
          object_template->NewInstance(context).ToLocalChecked();
      v8::Local<v8::External> null_external =
          v8::External::New(isolate, nullptr);
      v8::Local<v8::External> field_external =
          v8::External::New(isolate, &serialized_static_field);
      a->SetInternalField(0, b);
      a->SetAlignedPointerInInternalField(1, a1);
      b->SetAlignedPointerInInternalField(0, b0);
      b->SetInternalField(1, c);
      c->SetAlignedPointerInInternalField(0, c0);
      c->SetInternalField(1, null_external);
      c->SetInternalField(2, field_external);
      CHECK(context->Global()->Set(context, v8_str("a"), a).FromJust());

      CHECK_EQ(0u,
               creator.AddContext(context, v8::SerializeInternalFieldsCallback(
                                               SerializeInternalFields,
                                               reinterpret_cast<void*>(2016))));
      CHECK_EQ(0u, creator.AddTemplate(callback));
      CHECK_EQ(1u, creator.AddTemplate(global_template));
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);

    delete a1;
    delete b0;
    delete c0;
  }

  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = original_external_references;
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      {
        // Create a new context without a new object template.
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context =
            v8::Context::FromSnapshot(
                isolate, 0,
                v8::DeserializeInternalFieldsCallback(
                    DeserializeInternalFields, reinterpret_cast<void*>(2017)))
                .ToLocalChecked();
        v8::Context::Scope context_scope(context);
        ExpectInt32("f()", 42);

        // Retrieve the snapshotted object template.
        v8::Local<v8::ObjectTemplate> obj_template =
            v8::ObjectTemplate::FromSnapshot(isolate, 1).ToLocalChecked();
        CHECK(!obj_template.IsEmpty());
        v8::Local<v8::Object> object =
            obj_template->NewInstance(context).ToLocalChecked();
        CHECK(context->Global()->Set(context, v8_str("o"), object).FromJust());
        ExpectInt32("o.f()", 42);
        // Check that it instantiates to the same prototype.
        ExpectTrue("o.f.prototype === f.prototype");

        // Retrieve the snapshotted function template.
        v8::Local<v8::FunctionTemplate> fun_template =
            v8::FunctionTemplate::FromSnapshot(isolate, 0).ToLocalChecked();
        CHECK(!fun_template.IsEmpty());
        v8::Local<v8::Function> fun =
            fun_template->GetFunction(context).ToLocalChecked();
        CHECK(context->Global()->Set(context, v8_str("g"), fun).FromJust());
        ExpectInt32("g()", 42);
        // Check that it instantiates to the same prototype.
        ExpectTrue("g.prototype === f.prototype");

        // Retrieve embedder fields.
        v8::Local<v8::Object> a = context->Global()
                                      ->Get(context, v8_str("a"))
                                      .ToLocalChecked()
                                      ->ToObject(context)
                                      .ToLocalChecked();
        v8::Local<v8::Object> b =
            a->GetInternalField(0)->ToObject(context).ToLocalChecked();
        InternalFieldData* a1 = reinterpret_cast<InternalFieldData*>(
            a->GetAlignedPointerFromInternalField(1));
        v8::Local<v8::Value> a2 = a->GetInternalField(2);

        InternalFieldData* b0 = reinterpret_cast<InternalFieldData*>(
            b->GetAlignedPointerFromInternalField(0));
        v8::Local<v8::Object> c =
            b->GetInternalField(1)->ToObject(context).ToLocalChecked();
        v8::Local<v8::Value> b2 = b->GetInternalField(2);

        InternalFieldData* c0 = reinterpret_cast<InternalFieldData*>(
            c->GetAlignedPointerFromInternalField(0));
        v8::Local<v8::Value> c1 = c->GetInternalField(1);
        v8::Local<v8::Value> c2 = c->GetInternalField(2);

        CHECK_EQ(11u, a1->data);
        CHECK(a2->IsUndefined());
        CHECK_EQ(20u, b0->data);
        CHECK(b2->IsUndefined());
        CHECK_EQ(30u, c0->data);
        CHECK(c1->IsExternal());
        CHECK_NULL(v8::Local<v8::External>::Cast(c1)->Value());
        CHECK_EQ(static_cast<void*>(&serialized_static_field),
                 v8::Local<v8::External>::Cast(c2)->Value());

        // Accessing out of bound returns empty MaybeHandle.
        CHECK(v8::ObjectTemplate::FromSnapshot(isolate, 2).IsEmpty());
        CHECK(v8::FunctionTemplate::FromSnapshot(isolate, 2).IsEmpty());
        CHECK(v8::Context::FromSnapshot(isolate, 1).IsEmpty());

        for (auto data : deserialized_data) delete data;
        deserialized_data.clear();
      }
    }
    isolate->Dispose();
  }
  delete[] blob.data;
}

TEST(SnapshotCreatorIncludeGlobalProxy) {
  DisableAlwaysOpt();
  v8::StartupData blob;

  {
    v8::SnapshotCreator creator(original_external_references);
    v8::Isolate* isolate = creator.GetIsolate();
    v8::RegisterExtension(new SerializedExtension);
    const char* extension_names[] = {"serialized extension"};
    v8::ExtensionConfiguration extensions(1, extension_names);
    {
      // Set default context. This context implicitly does *not* serialize
      // the global proxy, and upon deserialization one has to be created
      // in the bootstrapper from the global object template.
      // Side effects from extensions are persisted though.
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::ObjectTemplate> global_template =
          v8::ObjectTemplate::New(isolate);
      v8::Local<v8::FunctionTemplate> callback =
          v8::FunctionTemplate::New(isolate, SerializedCallback);
      global_template->Set(v8_str("f"), callback);
      global_template->SetHandler(v8::NamedPropertyHandlerConfiguration(
          NamedPropertyGetterForSerialization));
      v8::Local<v8::Context> context =
          v8::Context::New(isolate, &extensions, global_template);
      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 42);
      ExpectInt32("g()", 12);
      ExpectInt32("h()", 13);
      ExpectInt32("o.p", 7);
      ExpectInt32("x", 2016);
      creator.SetDefaultContext(context);
    }
    {
      // Add additional context. This context implicitly *does* serialize
      // the global proxy, and upon deserialization one has to be created
      // in the bootstrapper from the global object template.
      // Side effects from extensions are persisted.
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::ObjectTemplate> global_template =
          v8::ObjectTemplate::New(isolate);
      v8::Local<v8::FunctionTemplate> callback =
          v8::FunctionTemplate::New(isolate, SerializedCallback);
      global_template->SetInternalFieldCount(3);
      global_template->Set(v8_str("f"), callback);
      global_template->SetHandler(v8::NamedPropertyHandlerConfiguration(
          NamedPropertyGetterForSerialization));
      global_template->SetAccessor(v8_str("y"), AccessorForSerialization);
      v8::Local<v8::Private> priv =
          v8::Private::ForApi(isolate, v8_str("cached"));
      global_template->SetAccessorProperty(
          v8_str("cached"),
          v8::FunctionTemplate::NewWithCache(isolate, SerializedCallback, priv,
                                             v8::Local<v8::Value>()));
      v8::Local<v8::Context> context =
          v8::Context::New(isolate, &extensions, global_template);
      v8::Context::Scope context_scope(context);

      CHECK(context->Global()
                ->SetPrivate(context, priv, v8_str("cached string"))
                .FromJust());
      v8::Local<v8::Private> hidden =
          v8::Private::ForApi(isolate, v8_str("hidden"));
      CHECK(context->Global()
                ->SetPrivate(context, hidden, v8_str("hidden string"))
                .FromJust());

      ExpectInt32("f()", 42);
      ExpectInt32("g()", 12);
      ExpectInt32("h()", 13);
      ExpectInt32("o.p", 7);
      ExpectInt32("x", 2016);
      ExpectInt32("y", 2017);
      CHECK(v8_str("hidden string")
                ->Equals(context, context->Global()
                                      ->GetPrivate(context, hidden)
                                      .ToLocalChecked())
                .FromJust());

      CHECK_EQ(0u,
               creator.AddContext(context, v8::SerializeInternalFieldsCallback(
                                               SerializeInternalFields,
                                               reinterpret_cast<void*>(2016))));
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = original_external_references;
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      // We can introduce new extensions, which could override the already
      // snapshotted extension.
      v8::Extension* extension = new v8::Extension("new extension",
                                                   "function i() { return 24; }"
                                                   "function j() { return 25; }"
                                                   "if (o.p == 7) o.p++;");
      extension->set_auto_enable(true);
      v8::RegisterExtension(extension);
      {
        // Create a new context from default context snapshot. This will
        // create a new global object from a new global object template
        // without the interceptor.
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);
        ExpectInt32("f()", 42);
        ExpectInt32("g()", 12);
        ExpectInt32("h()", 13);
        ExpectInt32("i()", 24);
        ExpectInt32("j()", 25);
        ExpectInt32("o.p", 8);
        v8::TryCatch try_catch(isolate);
        CHECK(CompileRun("x").IsEmpty());
        CHECK(try_catch.HasCaught());
      }
      {
        // Create a new context from first additional context snapshot. This
        // will use the global object from the snapshot, including interceptor.
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context =
            v8::Context::FromSnapshot(
                isolate, 0,
                v8::DeserializeInternalFieldsCallback(
                    DeserializeInternalFields, reinterpret_cast<void*>(2017)))
                .ToLocalChecked();

        {
          v8::Context::Scope context_scope(context);
          ExpectInt32("f()", 42);
          ExpectInt32("g()", 12);
          ExpectInt32("h()", 13);
          ExpectInt32("i()", 24);
          ExpectInt32("j()", 25);
          ExpectInt32("o.p", 8);
          ExpectInt32("x", 2016);
          v8::Local<v8::Private> hidden =
              v8::Private::ForApi(isolate, v8_str("hidden"));
          CHECK(v8_str("hidden string")
                    ->Equals(context, context->Global()
                                          ->GetPrivate(context, hidden)
                                          .ToLocalChecked())
                    .FromJust());
          ExpectString("cached", "cached string");
        }

        v8::Local<v8::Object> global = context->Global();
        CHECK_EQ(3, global->InternalFieldCount());
        context->DetachGlobal();

        // New context, but reuse global proxy.
        v8::ExtensionConfiguration* no_extensions = nullptr;
        v8::Local<v8::Context> context2 =
            v8::Context::FromSnapshot(
                isolate, 0,
                v8::DeserializeInternalFieldsCallback(
                    DeserializeInternalFields, reinterpret_cast<void*>(2017)),
                no_extensions, global)
                .ToLocalChecked();
        {
          v8::Context::Scope context_scope(context2);
          ExpectInt32("f()", 42);
          ExpectInt32("g()", 12);
          ExpectInt32("h()", 13);
          ExpectInt32("i()", 24);
          ExpectInt32("j()", 25);
          ExpectInt32("o.p", 8);
          ExpectInt32("x", 2016);
          v8::Local<v8::Private> hidden =
              v8::Private::ForApi(isolate, v8_str("hidden"));
          CHECK(v8_str("hidden string")
                    ->Equals(context2, context2->Global()
                                           ->GetPrivate(context2, hidden)
                                           .ToLocalChecked())
                    .FromJust());

          // Set cached accessor property again.
          v8::Local<v8::Private> priv =
              v8::Private::ForApi(isolate, v8_str("cached"));
          CHECK(context2->Global()
                    ->SetPrivate(context2, priv, v8_str("cached string 1"))
                    .FromJust());
          ExpectString("cached", "cached string 1");
        }

        CHECK(context2->Global()->Equals(context2, global).FromJust());
      }
    }
    isolate->Dispose();
  }
  delete[] blob.data;
}

UNINITIALIZED_TEST(ReinitializeStringHashSeedNotRehashable) {
  DisableAlwaysOpt();
  i::FLAG_rehash_snapshot = true;
  i::FLAG_hash_seed = 42;
  i::FLAG_allow_natives_syntax = true;
  v8::StartupData blob;
  {
    v8::SnapshotCreator creator;
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      // Create dictionary mode object.
      CompileRun(
          "var a = {};"
          "a.b = 1;"
          "a.c = 2;"
          "delete a.b;");
      ExpectInt32("a.c", 2);
      creator.SetDefaultContext(context);
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  i::FLAG_hash_seed = 1337;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  create_params.snapshot_blob = &blob;
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    // Check that no rehashing has been performed.
    CHECK_EQ(42, reinterpret_cast<i::Isolate*>(isolate)->heap()->HashSeed());
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    CHECK(!context.IsEmpty());
    v8::Context::Scope context_scope(context);
    ExpectInt32("a.c", 2);
  }
  isolate->Dispose();
  delete[] blob.data;
}

TEST(SerializationMemoryStats) {
  FLAG_profile_deserialization = true;
  FLAG_always_opt = false;
  v8::StartupData blob = v8::V8::CreateSnapshotDataBlob();
  delete[] blob.data;
}
