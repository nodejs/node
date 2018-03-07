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
#include "src/snapshot/builtin-deserializer.h"
#include "src/snapshot/builtin-serializer.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/partial-deserializer.h"
#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/snapshot.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/snapshot/startup-serializer.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"
#include "test/cctest/setup-isolate-for-tests.h"

namespace v8 {
namespace internal {

enum CodeCacheType { kLazy, kEager, kAfterExecute };

void DisableLazyDeserialization() {
  // UNINITIALIZED tests do not set up the isolate sufficiently for lazy
  // deserialization to work.
  FLAG_lazy_deserialization = false;
}

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
    isolate->setup_delegate_ = new SetupIsolateDelegateForTests(true);
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Isolate::Scope isolate_scope(v8_isolate);
    isolate->Init(nullptr);
    return v8_isolate;
  }
  // Wraps v8::Isolate::New, but with a TestIsolate under the hood.
  // Allows flexibility to bootstrap with or without snapshot even when
  // the production Isolate class has one or the other behavior baked in.
  static v8::Isolate* New(const v8::Isolate::CreateParams& params) {
    i::Isolate* isolate = new TestIsolate(false);
    bool create_heap_objects = params.snapshot_blob == nullptr;
    isolate->setup_delegate_ =
        new SetupIsolateDelegateForTests(create_heap_objects);
    return v8::IsolateNewImpl(isolate, params);
  }
  explicit TestIsolate(bool enable_serializer) : Isolate(enable_serializer) {
    set_array_buffer_allocator(CcTest::array_buffer_allocator());
  }
  void SetDeserializeFromSnapshot() {
    setup_delegate_ = new SetupIsolateDelegateForTests(false);
  }
};

static Vector<const byte> WritePayload(const Vector<const byte>& payload) {
  int length = payload.length();
  byte* blob = NewArray<byte>(length);
  memcpy(blob, payload.begin(), length);
  return Vector<const byte>(const_cast<const byte*>(blob), length);
}

// A convenience struct to simplify management of the two blobs required to
// deserialize an isolate.
struct StartupBlobs {
  Vector<const byte> startup;
  Vector<const byte> builtin;

  void Dispose() {
    startup.Dispose();
    builtin.Dispose();
  }
};

static StartupBlobs Serialize(v8::Isolate* isolate) {
  // We have to create one context.  One reason for this is so that the builtins
  // can be loaded from self hosted JS builtins and their addresses can be
  // processed.  This will clear the pending fixups array, which would otherwise
  // contain GC roots that would confuse the serialization/deserialization
  // process.
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

  i::BuiltinSerializer builtin_serializer(internal_isolate, &ser);
  builtin_serializer.SerializeBuiltinsAndHandlers();

  ser.SerializeWeakReferencesAndDeferred();
  SnapshotData startup_snapshot(&ser);
  BuiltinSnapshotData builtin_snapshot(&builtin_serializer);
  return {WritePayload(startup_snapshot.RawData()),
          WritePayload(builtin_snapshot.RawData())};
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

v8::Isolate* InitializeFromBlob(StartupBlobs& blobs) {
  v8::Isolate* v8_isolate = nullptr;
  {
    SnapshotData startup_snapshot(blobs.startup);
    BuiltinSnapshotData builtin_snapshot(blobs.builtin);
    StartupDeserializer deserializer(&startup_snapshot, &builtin_snapshot);
    TestIsolate* isolate = new TestIsolate(false);
    v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Isolate::Scope isolate_scope(v8_isolate);
    isolate->SetDeserializeFromSnapshot();
    isolate->Init(&deserializer);
  }
  return v8_isolate;
}

static v8::Isolate* Deserialize(StartupBlobs& blobs) {
  v8::Isolate* isolate = InitializeFromBlob(blobs);
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
  isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("Empty"));
}

UNINITIALIZED_TEST(StartupSerializerOnce) {
  DisableLazyDeserialization();
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  StartupBlobs blobs = Serialize(isolate);
  isolate->Dispose();
  isolate = Deserialize(blobs);
  {
    v8::HandleScope handle_scope(isolate);
    v8::Isolate::Scope isolate_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck(isolate);
  }
  isolate->Dispose();
  blobs.Dispose();
}

UNINITIALIZED_TEST(StartupSerializerRootMapDependencies) {
  DisableAlwaysOpt();
  v8::SnapshotCreator snapshot_creator;
  v8::Isolate* isolate = snapshot_creator.GetIsolate();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    Isolate* internal_isolate = reinterpret_cast<Isolate*>(isolate);
    // Here is interesting retaining path:
    // - FreeSpaceMap
    // - Map for Map types itself
    // - NullValue
    // - Internalized one byte string
    // - Map for Internalized one byte string
    // - WeakCell
    // - TheHoleValue
    // - HeapNumber
    // HeapNumber objects require kDoubleUnaligned on 32-bit
    // platforms. So, without special measures we're risking to serialize
    // object, requiring alignment before FreeSpaceMap is fully serialized.
    v8::internal::Handle<Map> map(
        internal_isolate->heap()->one_byte_internalized_string_map());
    Map::WeakCellForMap(map);
    // Need to avoid DCHECKs inside SnapshotCreator.
    snapshot_creator.SetDefaultContext(v8::Context::New(isolate));
  }

  v8::StartupData startup_data = snapshot_creator.CreateBlob(
      v8::SnapshotCreator::FunctionCodeHandling::kKeep);

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &startup_data;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();
  isolate = v8::Isolate::New(params);

  {
    v8::HandleScope handle_scope(isolate);
    v8::Isolate::Scope isolate_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck(isolate);
  }
  isolate->Dispose();
  delete[] startup_data.data;
}

UNINITIALIZED_TEST(StartupSerializerTwice) {
  DisableLazyDeserialization();
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  StartupBlobs blobs1 = Serialize(isolate);
  StartupBlobs blobs2 = Serialize(isolate);
  isolate->Dispose();
  blobs1.Dispose();
  isolate = Deserialize(blobs2);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck(isolate);
  }
  isolate->Dispose();
  blobs2.Dispose();
}

UNINITIALIZED_TEST(StartupSerializerOnceRunScript) {
  DisableLazyDeserialization();
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  StartupBlobs blobs = Serialize(isolate);
  isolate->Dispose();
  isolate = Deserialize(blobs);
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
  blobs.Dispose();
}

UNINITIALIZED_TEST(StartupSerializerTwiceRunScript) {
  DisableLazyDeserialization();
  DisableAlwaysOpt();
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  StartupBlobs blobs1 = Serialize(isolate);
  StartupBlobs blobs2 = Serialize(isolate);
  isolate->Dispose();
  blobs1.Dispose();
  isolate = Deserialize(blobs2);
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
  blobs2.Dispose();
}

static void PartiallySerializeContext(Vector<const byte>* startup_blob_out,
                                      Vector<const byte>* builtin_blob_out,
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

    i::Context* raw_context = i::Context::cast(*v8::Utils::OpenPersistent(env));

    env.Reset();

    SnapshotByteSink startup_sink;
    StartupSerializer startup_serializer(
        isolate, v8::SnapshotCreator::FunctionCodeHandling::kClear);
    startup_serializer.SerializeStrongReferences();

    SnapshotByteSink partial_sink;
    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         v8::SerializeInternalFieldsCallback());
    partial_serializer.Serialize(&raw_context, false);

    i::BuiltinSerializer builtin_serializer(isolate, &startup_serializer);
    builtin_serializer.SerializeBuiltinsAndHandlers();

    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(&startup_serializer);
    BuiltinSnapshotData builtin_snapshot(&builtin_serializer);
    SnapshotData partial_snapshot(&partial_serializer);

    *partial_blob_out = WritePayload(partial_snapshot.RawData());
    *builtin_blob_out = WritePayload(builtin_snapshot.RawData());
    *startup_blob_out = WritePayload(startup_snapshot.RawData());
  }
  v8_isolate->Dispose();
}

UNINITIALIZED_TEST(PartialSerializerContext) {
  DisableLazyDeserialization();
  DisableAlwaysOpt();
  Vector<const byte> startup_blob;
  Vector<const byte> builtin_blob;
  Vector<const byte> partial_blob;
  PartiallySerializeContext(&startup_blob, &builtin_blob, &partial_blob);

  StartupBlobs blobs = {startup_blob, builtin_blob};
  v8::Isolate* v8_isolate = InitializeFromBlob(blobs);
  CHECK(v8_isolate);
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
      root = PartialDeserializer::DeserializeContext(
                 isolate, &snapshot_data, false, global_proxy,
                 v8::DeserializeInternalFieldsCallback())
                 .ToHandleChecked();
      CHECK(root->IsContext());
      CHECK(Handle<Context>::cast(root)->global_proxy() == *global_proxy);
    }

    Handle<Object> root2;
    {
      SnapshotData snapshot_data(partial_blob);
      root2 = PartialDeserializer::DeserializeContext(
                  isolate, &snapshot_data, false, global_proxy,
                  v8::DeserializeInternalFieldsCallback())
                  .ToHandleChecked();
      CHECK(root2->IsContext());
      CHECK(!root.is_identical_to(root2));
    }
    partial_blob.Dispose();
  }
  v8_isolate->Dispose();
  blobs.Dispose();
}

static void PartiallySerializeCustomContext(
    Vector<const byte>* startup_blob_out, Vector<const byte>* builtin_blob_out,
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

    i::Context* raw_context = i::Context::cast(*v8::Utils::OpenPersistent(env));

    env.Reset();

    SnapshotByteSink startup_sink;
    StartupSerializer startup_serializer(
        isolate, v8::SnapshotCreator::FunctionCodeHandling::kClear);
    startup_serializer.SerializeStrongReferences();

    SnapshotByteSink partial_sink;
    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         v8::SerializeInternalFieldsCallback());
    partial_serializer.Serialize(&raw_context, false);

    i::BuiltinSerializer builtin_serializer(isolate, &startup_serializer);
    builtin_serializer.SerializeBuiltinsAndHandlers();

    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(&startup_serializer);
    BuiltinSnapshotData builtin_snapshot(&builtin_serializer);
    SnapshotData partial_snapshot(&partial_serializer);

    *partial_blob_out = WritePayload(partial_snapshot.RawData());
    *builtin_blob_out = WritePayload(builtin_snapshot.RawData());
    *startup_blob_out = WritePayload(startup_snapshot.RawData());
  }
  v8_isolate->Dispose();
}

UNINITIALIZED_TEST(PartialSerializerCustomContext) {
  DisableLazyDeserialization();
  DisableAlwaysOpt();
  Vector<const byte> startup_blob;
  Vector<const byte> builtin_blob;
  Vector<const byte> partial_blob;
  PartiallySerializeCustomContext(&startup_blob, &builtin_blob, &partial_blob);

  StartupBlobs blobs = {startup_blob, builtin_blob};
  v8::Isolate* v8_isolate = InitializeFromBlob(blobs);
  CHECK(v8_isolate);
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
      root = PartialDeserializer::DeserializeContext(
                 isolate, &snapshot_data, false, global_proxy,
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
  blobs.Dispose();
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
    v8::Context::Scope c_scope(context);
    v8::Maybe<int32_t> result =
        CompileRun("f()")->Int32Value(isolate1->GetCurrentContext());
    CHECK_EQ(42, result.FromJust());
    CHECK(CompileRun("this.g")->IsUndefined());
  }
  isolate1->Dispose();
  delete[] data1.data;  // We can dispose of the snapshot blob now.
}

struct InternalFieldData {
  uint32_t data;
};

v8::StartupData SerializeInternalFields(v8::Local<v8::Object> holder, int index,
                                        void* data) {
  CHECK_EQ(reinterpret_cast<void*>(2016), data);
  InternalFieldData* embedder_field = static_cast<InternalFieldData*>(
      holder->GetAlignedPointerFromInternalField(index));
  if (embedder_field == nullptr) return {nullptr, 0};
  int size = sizeof(*embedder_field);
  char* payload = new char[size];
  // We simply use memcpy to serialize the content.
  memcpy(payload, embedder_field, size);
  return {payload, size};
}

std::vector<InternalFieldData*> deserialized_data;

void DeserializeInternalFields(v8::Local<v8::Object> holder, int index,
                               v8::StartupData payload, void* data) {
  if (payload.raw_size == 0) {
    holder->SetAlignedPointerInInternalField(index, nullptr);
    return;
  }
  CHECK_EQ(reinterpret_cast<void*>(2017), data);
  InternalFieldData* embedder_field = new InternalFieldData{0};
  memcpy(embedder_field, payload.data, payload.raw_size);
  holder->SetAlignedPointerInInternalField(index, embedder_field);
  deserialized_data.push_back(embedder_field);
}

typedef std::vector<std::tuple<const char*, int32_t>> Int32Expectations;

void TestInt32Expectations(const Int32Expectations& expectations) {
  for (const auto& e : expectations) {
    ExpectInt32(std::get<0>(e), std::get<1>(e));
  }
}

void TypedArrayTestHelper(
    const char* code, const Int32Expectations& expectations,
    const char* code_to_run_after_restore = nullptr,
    const Int32Expectations& after_restore_expectations = Int32Expectations()) {
  DisableAlwaysOpt();
  i::FLAG_allow_natives_syntax = true;
  v8::StartupData blob;
  {
    v8::SnapshotCreator creator;
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);

      CompileRun(code);
      TestInt32Expectations(expectations);
      creator.SetDefaultContext(
          context, v8::SerializeInternalFieldsCallback(
                       SerializeInternalFields, reinterpret_cast<void*>(2016)));
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  v8::Isolate::CreateParams create_params;
  create_params.snapshot_blob = &blob;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = TestIsolate::New(create_params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(
        isolate, nullptr, v8::MaybeLocal<v8::ObjectTemplate>(),
        v8::MaybeLocal<v8::Value>(),
        v8::DeserializeInternalFieldsCallback(DeserializeInternalFields,
                                              reinterpret_cast<void*>(2017)));
    CHECK(deserialized_data.empty());  // We do not expect any embedder data.
    v8::Context::Scope c_scope(context);
    TestInt32Expectations(expectations);
    if (code_to_run_after_restore) {
      CompileRun(code_to_run_after_restore);
    }
    TestInt32Expectations(after_restore_expectations);
  }
  isolate->Dispose();
  delete[] blob.data;  // We can dispose of the snapshot blob now.
}

TEST(CustomSnapshotDataBlobWithOffHeapTypedArray) {
  const char* code =
      "var x = new Uint8Array(128);"
      "x[0] = 12;"
      "var arr = new Array(17);"
      "arr[1] = 24;"
      "var y = new Uint32Array(arr);"
      "var buffer = new ArrayBuffer(128);"
      "var z = new Int16Array(buffer);"
      "z[0] = 48;";
  Int32Expectations expectations = {std::make_tuple("x[0]", 12),
                                    std::make_tuple("y[1]", 24),
                                    std::make_tuple("z[0]", 48)};

  TypedArrayTestHelper(code, expectations);
}

TEST(CustomSnapshotDataBlobSharedArrayBuffer) {
  const char* code =
      "var x = new Int32Array([12, 24, 48, 96]);"
      "var y = new Uint8Array(x.buffer)";
  Int32Expectations expectations = {
    std::make_tuple("x[0]", 12),
    std::make_tuple("x[1]", 24),
#if !V8_TARGET_BIG_ENDIAN
    std::make_tuple("y[0]", 12),
    std::make_tuple("y[1]", 0),
    std::make_tuple("y[2]", 0),
    std::make_tuple("y[3]", 0),
    std::make_tuple("y[4]", 24)
#else
    std::make_tuple("y[3]", 12),
    std::make_tuple("y[2]", 0),
    std::make_tuple("y[1]", 0),
    std::make_tuple("y[0]", 0),
    std::make_tuple("y[7]", 24)
#endif
  };

  TypedArrayTestHelper(code, expectations);
}

TEST(CustomSnapshotDataBlobArrayBufferWithOffset) {
  const char* code =
      "var x = new Int32Array([12, 24, 48, 96]);"
      "var y = new Int32Array(x.buffer, 4, 2)";
  Int32Expectations expectations = {
      std::make_tuple("x[1]", 24), std::make_tuple("x[2]", 48),
      std::make_tuple("y[0]", 24), std::make_tuple("y[1]", 48),
  };

  // Verify that the typed arrays use the same buffer (not independent copies).
  const char* code_to_run_after_restore = "x[2] = 57; y[0] = 42;";
  Int32Expectations after_restore_expectations = {
      std::make_tuple("x[1]", 42), std::make_tuple("y[1]", 57),
  };

  TypedArrayTestHelper(code, expectations, code_to_run_after_restore,
                       after_restore_expectations);
}

TEST(CustomSnapshotDataBlobDataView) {
  const char* code =
      "var x = new Int8Array([1, 2, 3, 4]);"
      "var v = new DataView(x.buffer)";
  Int32Expectations expectations = {std::make_tuple("v.getInt8(0)", 1),
                                    std::make_tuple("v.getInt8(1)", 2),
                                    std::make_tuple("v.getInt16(0)", 258),
                                    std::make_tuple("v.getInt16(1)", 515)};

  TypedArrayTestHelper(code, expectations);
}

TEST(CustomSnapshotDataBlobNeuteredArrayBuffer) {
  const char* code =
      "var x = new Int16Array([12, 24, 48]);"
      "%ArrayBufferNeuter(x.buffer);";
  Int32Expectations expectations = {std::make_tuple("x.buffer.byteLength", 0),
                                    std::make_tuple("x.length", 0)};

  DisableAlwaysOpt();
  i::FLAG_allow_natives_syntax = true;
  v8::StartupData blob;
  {
    v8::SnapshotCreator creator;
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);

      CompileRun(code);
      TestInt32Expectations(expectations);
      creator.SetDefaultContext(
          context, v8::SerializeInternalFieldsCallback(
                       SerializeInternalFields, reinterpret_cast<void*>(2016)));
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  v8::Isolate::CreateParams create_params;
  create_params.snapshot_blob = &blob;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = TestIsolate::New(create_params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(
        isolate, nullptr, v8::MaybeLocal<v8::ObjectTemplate>(),
        v8::MaybeLocal<v8::Value>(),
        v8::DeserializeInternalFieldsCallback(DeserializeInternalFields,
                                              reinterpret_cast<void*>(2017)));
    v8::Context::Scope c_scope(context);
    TestInt32Expectations(expectations);

    v8::Local<v8::Value> x = CompileRun("x");
    CHECK(x->IsTypedArray());
    i::Handle<i::JSTypedArray> array =
        i::Handle<i::JSTypedArray>::cast(v8::Utils::OpenHandle(*x));
    CHECK(array->WasNeutered());
    CHECK_NULL(
        FixedTypedArrayBase::cast(array->elements())->external_pointer());
  }
  isolate->Dispose();
  delete[] blob.data;  // We can dispose of the snapshot blob now.
}

i::Handle<i::JSArrayBuffer> GetBufferFromTypedArray(
    v8::Local<v8::Value> typed_array) {
  CHECK(typed_array->IsTypedArray());

  i::Handle<i::JSArrayBufferView> view = i::Handle<i::JSArrayBufferView>::cast(
      v8::Utils::OpenHandle(*typed_array));

  return i::handle(i::JSArrayBuffer::cast(view->buffer()));
}

TEST(CustomSnapshotDataBlobOnOrOffHeapTypedArray) {
  const char* code =
      "var x = new Uint8Array(8);"
      "x[0] = 12;"
      "x[7] = 24;"
      "var y = new Int16Array([12, 24, 48]);"
      "var z = new Int32Array(64);"
      "z[0] = 96;";
  Int32Expectations expectations = {
      std::make_tuple("x[0]", 12), std::make_tuple("x[7]", 24),
      std::make_tuple("y[2]", 48), std::make_tuple("z[0]", 96)};

  DisableAlwaysOpt();
  i::FLAG_allow_natives_syntax = true;
  v8::StartupData blob;
  {
    v8::SnapshotCreator creator;
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);

      CompileRun(code);
      TestInt32Expectations(expectations);
      creator.SetDefaultContext(
          context, v8::SerializeInternalFieldsCallback(
                       SerializeInternalFields, reinterpret_cast<void*>(2016)));
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  v8::Isolate::CreateParams create_params;
  create_params.snapshot_blob = &blob;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = TestIsolate::New(create_params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(
        isolate, nullptr, v8::MaybeLocal<v8::ObjectTemplate>(),
        v8::MaybeLocal<v8::Value>(),
        v8::DeserializeInternalFieldsCallback(DeserializeInternalFields,
                                              reinterpret_cast<void*>(2017)));
    v8::Context::Scope c_scope(context);
    TestInt32Expectations(expectations);

    i::Handle<i::JSArrayBuffer> buffer =
        GetBufferFromTypedArray(CompileRun("x"));
    // The resulting buffer should be on-heap.
    CHECK_NULL(buffer->backing_store());

    buffer = GetBufferFromTypedArray(CompileRun("y"));
    CHECK_NULL(buffer->backing_store());

    buffer = GetBufferFromTypedArray(CompileRun("z"));
    // The resulting buffer should be off-heap.
    CHECK_NOT_NULL(buffer->backing_store());
  }
  isolate->Dispose();
  delete[] blob.data;  // We can dispose of the snapshot blob now.
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
    v8::Context::Scope c_scope(context);
    v8::Maybe<int32_t> result =
        CompileRun("f()")->Int32Value(isolate2->GetCurrentContext());
    CHECK_EQ(86, result.FromJust());
    result = CompileRun("g()")->Int32Value(isolate2->GetCurrentContext());
    CHECK_EQ(43, result.FromJust());
  }
  isolate2->Dispose();
  delete[] data2.data;  // We can dispose of the snapshot blob now.
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

    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
    v8::Context::Scope c_scope(context);
    v8::Local<v8::Value> result = CompileRun(source2);
    v8::Maybe<bool> compare = v8_str("42")->Equals(
        v8::Isolate::GetCurrent()->GetCurrentContext(), result);
    CHECK(compare.FromJust());
  }
  isolate->Dispose();
  delete[] data.data;  // We can dispose of the snapshot blob now.
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
    v8::Context::Scope c_scope(context);
    v8::Maybe<int32_t> result = CompileRun("f()")->Int32Value(context);
    CHECK_EQ(42, result.FromJust());
  }
  isolate1->Dispose();
  delete[] data1.data;  // We can dispose of the snapshot blob now.
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
  delete[] data.data;  // We can dispose of the snapshot blob now.
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
    v8::Context::Scope c_scope(context);
    // Running the warmup script has effect on whether functions are
    // pre-compiled, but does not pollute the context.
    CHECK(IsCompiled("Math.abs"));
    CHECK(IsCompiled("String.raw"));
    CHECK(CompileRun("Math.random")->IsFunction());
  }
  isolate->Dispose();
  delete[] warm.data;
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
    v8::Context::Scope c_scope(context);
    // Running the warmup script has effect on whether functions are
    // pre-compiled, but does not pollute the context.
    CHECK(IsCompiled("f"));
    CHECK(IsCompiled("Math.abs"));
    CHECK(!IsCompiled("g"));
    CHECK(IsCompiled("String.raw"));
    CHECK(!IsCompiled("Array.prototype.sort"));
    CHECK_EQ(5, CompileRun("a")->Int32Value(context).FromJust());
  }
  isolate->Dispose();
  delete[] warm.data;
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
    v8::Context::Scope c_scope(context);
    CHECK_EQ(7, CompileRun("a[0]()")->Int32Value(context).FromJust());
  }
  isolate->Dispose();
  source.Dispose();
  delete[] data.data;  // We can dispose of the snapshot blob now.
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
  for (HeapObject* obj = iterator.next(); obj != nullptr;
       obj = iterator.next()) {
    if (obj->IsCode() && Code::cast(obj)->kind() == Code::BUILTIN) counter++;
  }
  return counter;
}


static Handle<SharedFunctionInfo> CompileScript(
    Isolate* isolate, Handle<String> source, Handle<String> name,
    ScriptData** cached_data, v8::ScriptCompiler::CompileOptions options) {
  return Compiler::GetSharedFunctionInfoForScript(
             source, name, 0, 0, v8::ScriptOriginOptions(), Handle<Object>(),
             Handle<Context>(isolate->native_context()), nullptr, cached_data,
             options, ScriptCompiler::kNoCacheNoReason, NOT_NATIVES_CODE,
             Handle<FixedArray>())
      .ToHandleChecked();
}

TEST(CodeSerializerOnePlusOne) {
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

  ScriptData* cache = nullptr;

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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();
  CHECK_EQ(2, Handle<Smi>::cast(copy_result)->value());

  CHECK_EQ(builtins_count, CountBuiltins());

  delete cache;
}

TEST(CodeSerializerPromotedToCompilationCache) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();

  v8::HandleScope scope(CcTest::isolate());

  const char* source = "1 + 1";

  Handle<String> src = isolate->factory()
                           ->NewStringFromUtf8(CStrVector(source))
                           .ToHandleChecked();
  ScriptData* cache = nullptr;

  CompileScript(isolate, src, src, &cache,
                v8::ScriptCompiler::kProduceCodeCache);

  DisallowCompilation no_compile_expected(isolate);
  Handle<SharedFunctionInfo> copy = CompileScript(
      isolate, src, src, &cache, v8::ScriptCompiler::kConsumeCodeCache);

  InfoVectorPair pair = isolate->compilation_cache()->LookupScript(
      src, src, 0, 0, v8::ScriptOriginOptions(), isolate->native_context(),
      LanguageMode::kSloppy);

  CHECK(pair.shared() == *copy);

  delete cache;
}

TEST(CodeSerializerInternalizedString) {
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
  ScriptData* cache = nullptr;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, orig_source, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);
  Handle<JSFunction> orig_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          orig, isolate->native_context());
  Handle<Object> orig_result =
      Execution::Call(isolate, orig_fun, global, 0, nullptr).ToHandleChecked();
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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();
  CHECK(orig_result.is_identical_to(copy_result));
  Handle<String> expected =
      isolate->factory()->NewStringFromAsciiChecked("string1");

  CHECK(Handle<String>::cast(copy_result)->Equals(*expected));
  CHECK_EQ(builtins_count, CountBuiltins());

  delete cache;
}

TEST(CodeSerializerLargeCodeObject) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  // The serializer only tests the shared code, which is always the unoptimized
  // code. Don't even bother generating optimized code to avoid timeouts.
  FLAG_always_opt = false;

  Vector<const uint8_t> source = ConstructSource(
      STATIC_CHAR_VECTOR("var j=1; if (j == 0) {"),
      STATIC_CHAR_VECTOR(
          "for (let i of Object.prototype) for (let k = 0; k < 0; ++k);"),
      STATIC_CHAR_VECTOR("} j=7; j"), 1100);
  Handle<String> source_str =
      isolate->factory()->NewStringFromOneByte(source).ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = nullptr;

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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();

  int result_int;
  CHECK(copy_result->ToInt32(&result_int));
  CHECK_EQ(7, result_int);

  delete cache;
  source.Dispose();
}

TEST(CodeSerializerLargeCodeObjectWithIncrementalMarking) {
  if (FLAG_never_compact) return;
  ManualGCScope manual_gc_scope;
  FLAG_always_opt = false;
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
      STATIC_CHAR_VECTOR("} j=7; var s = 'happy_hippo'; j"), 10000);
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
  ScriptData* cache = nullptr;

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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();

  int result_int;
  CHECK(copy_result->ToInt32(&result_int));
  CHECK_EQ(7, result_int);

  delete cache;
  source.Dispose();
}
TEST(CodeSerializerLargeStrings) {
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
  ScriptData* cache = nullptr;

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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();

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
  ScriptData* cache = nullptr;

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

  USE(Execution::Call(isolate, copy_fun, global, 0, nullptr));

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
      : data_(data), length_(length), dispose_count_(0) {}
  virtual const char* data() const { return data_; }
  virtual size_t length() const { return length_; }
  virtual void Dispose() { dispose_count_++; }
  int dispose_count() { return dispose_count_; }

 private:
  const char* data_;
  size_t length_;
  int dispose_count_;
};


class SerializerTwoByteResource : public v8::String::ExternalStringResource {
 public:
  SerializerTwoByteResource(const char* data, size_t length)
      : data_(AsciiToTwoByteString(data)), length_(length), dispose_count_(0) {}
  ~SerializerTwoByteResource() { DeleteArray<const uint16_t>(data_); }

  virtual const uint16_t* data() const { return data_; }
  virtual size_t length() const { return length_; }
  virtual void Dispose() { dispose_count_++; }
  int dispose_count() { return dispose_count_; }

 private:
  const uint16_t* data_;
  size_t length_;
  int dispose_count_;
};

TEST(CodeSerializerExternalString) {
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
  ScriptData* cache = nullptr;

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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();

  CHECK_EQ(15.0, copy_result->Number());

  delete cache;
}

TEST(CodeSerializerLargeExternalString) {
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
  ScriptData* cache = nullptr;

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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();

  CHECK_EQ(42.0, copy_result->Number());

  delete cache;
  string.Dispose();
}

TEST(CodeSerializerExternalScriptName) {
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
  ScriptData* cache = nullptr;

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
      Execution::Call(isolate, copy_fun, global, 0, nullptr).ToHandleChecked();

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

v8::ScriptCompiler::CachedData* ProduceCache(
    const char* source, CodeCacheType cacheType = CodeCacheType::kLazy) {
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
    v8::ScriptCompiler::CompileOptions options;
    switch (cacheType) {
      case CodeCacheType::kLazy:
        options = v8::ScriptCompiler::kProduceCodeCache;
        break;
      case CodeCacheType::kEager:
        options = v8::ScriptCompiler::kProduceFullCodeCache;
        break;
      case CodeCacheType::kAfterExecute:
        options = v8::ScriptCompiler::kNoCompileOptions;
        break;
      default:
        UNREACHABLE();
    }
    v8::Local<v8::UnboundScript> script =
        v8::ScriptCompiler::CompileUnboundScript(isolate1, &source, options)
            .ToLocalChecked();

    v8::Local<v8::Value> result = script->BindToCurrentContext()
                                      ->Run(isolate1->GetCurrentContext())
                                      .ToLocalChecked();
    v8::Local<v8::String> result_string =
        result->ToString(isolate1->GetCurrentContext()).ToLocalChecked();
    CHECK(result_string->Equals(isolate1->GetCurrentContext(), v8_str("abcdef"))
              .FromJust());

    if (cacheType == CodeCacheType::kAfterExecute) {
      cache = ScriptCompiler::CreateCodeCache(script, source_str);
    } else {
      const ScriptCompiler::CachedData* data = source.GetCachedData();
      CHECK(data);
      uint8_t* buffer = NewArray<uint8_t>(data->length);
      MemCopy(buffer, data->data, data->length);
      cache = new v8::ScriptCompiler::CachedData(
          buffer, data->length, v8::ScriptCompiler::CachedData::BufferOwned);
    }
    CHECK(cache);
  }
  isolate1->Dispose();
  return cache;
}

void CheckDeserializedFlag(v8::Local<v8::UnboundScript> script) {
  i::Handle<i::SharedFunctionInfo> sfi = v8::Utils::OpenHandle(*script);
  i::Handle<i::Script> i_script(Script::cast(sfi->script()));
  i::SharedFunctionInfo::ScriptIterator iterator(i_script);
  while (SharedFunctionInfo* next = iterator.Next()) {
    CHECK_EQ(next->is_compiled(), next->deserialized());
  }
}

TEST(CodeSerializerIsolates) {
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
    CheckDeserializedFlag(script);
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

TEST(CodeSerializerIsolatesEager) {
  const char* source =
      "function f() {"
      "  return function g() {"
      "    return 'abc';"
      "  }"
      "}"
      "f()() + 'def'";
  v8::ScriptCompiler::CachedData* cache =
      ProduceCache(source, CodeCacheType::kEager);

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
    CheckDeserializedFlag(script);
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

TEST(CodeSerializerAfterExecute) {
  // We test that no compilations happen when running this code. Forcing
  // to always optimize breaks this test.
  bool prev_opt_value = FLAG_opt;
  bool prev_always_opt_value = FLAG_always_opt;
  FLAG_always_opt = false;
  FLAG_opt = false;
  const char* source = "function f() { return 'abc'; }; f() + 'def'";
  v8::ScriptCompiler::CachedData* cache =
      ProduceCache(source, CodeCacheType::kAfterExecute);

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
    v8::Local<v8::UnboundScript> script;
    {
      DisallowCompilation no_compile_expected(
          reinterpret_cast<Isolate*>(isolate2));
      script = v8::ScriptCompiler::CompileUnboundScript(
                   isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache)
                   .ToLocalChecked();
    }
    CHECK(!cache->rejected);
    CheckDeserializedFlag(script);

    Handle<SharedFunctionInfo> sfi = v8::Utils::OpenHandle(*script);
    CHECK(sfi->HasBytecodeArray());
    BytecodeArray* bytecode = sfi->bytecode_array();
    CHECK_EQ(bytecode->interrupt_budget(),
             interpreter::Interpreter::kInterruptBudget);
    CHECK_EQ(bytecode->osr_loop_nesting_level(), 0);

    {
      DisallowCompilation no_compile_expected(
          reinterpret_cast<Isolate*>(isolate2));
      v8::Local<v8::Value> result = script->BindToCurrentContext()
                                        ->Run(isolate2->GetCurrentContext())
                                        .ToLocalChecked();
      v8::Local<v8::String> result_string =
          result->ToString(isolate2->GetCurrentContext()).ToLocalChecked();
      CHECK(
          result_string->Equals(isolate2->GetCurrentContext(), v8_str("abcdef"))
              .FromJust());
    }
  }
  isolate2->Dispose();

  // Restore the flags.
  FLAG_always_opt = prev_always_opt_value;
  FLAG_opt = prev_opt_value;
}

TEST(CodeSerializerFlagChange) {
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
    CheckDeserializedFlag(script);
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

TEST(Regress503552) {
  if (!FLAG_incremental_marking) return;
  // Test that the code serializer can deal with weak cells that form a linked
  // list during incremental marking.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  HandleScope scope(isolate);
  Handle<String> source = isolate->factory()->NewStringFromAsciiChecked(
      "function f() {} function g() {}");
  ScriptData* script_data = nullptr;
  Handle<SharedFunctionInfo> shared =
      Compiler::GetSharedFunctionInfoForScript(
          source, MaybeHandle<String>(), 0, 0, v8::ScriptOriginOptions(),
          MaybeHandle<Object>(), Handle<Context>(isolate->native_context()),
          nullptr, &script_data, v8::ScriptCompiler::kProduceCodeCache,
          ScriptCompiler::kNoCacheNoReason, NOT_NATIVES_CODE,
          MaybeHandle<FixedArray>())
          .ToHandleChecked();
  delete script_data;

  heap::SimulateIncrementalMarking(isolate->heap());

  script_data = CodeSerializer::Serialize(isolate, shared, source);
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

static SerializerOneByteResource serializable_one_byte_resource("one_byte", 8);
static SerializerTwoByteResource serializable_two_byte_resource("two_byte", 8);

intptr_t original_external_references[] = {
    reinterpret_cast<intptr_t>(SerializedCallback),
    reinterpret_cast<intptr_t>(&serialized_static_field),
    reinterpret_cast<intptr_t>(&NamedPropertyGetterForSerialization),
    reinterpret_cast<intptr_t>(&AccessorForSerialization),
    reinterpret_cast<intptr_t>(&SerializedExtension::FunctionCallback),
    reinterpret_cast<intptr_t>(&serialized_static_field),  // duplicate entry
    reinterpret_cast<intptr_t>(&serializable_one_byte_resource),
    reinterpret_cast<intptr_t>(&serializable_two_byte_resource),
    0};

intptr_t replaced_external_references[] = {
    reinterpret_cast<intptr_t>(SerializedCallbackReplacement),
    reinterpret_cast<intptr_t>(&serialized_static_field),
    reinterpret_cast<intptr_t>(&NamedPropertyGetterForSerialization),
    reinterpret_cast<intptr_t>(&AccessorForSerialization),
    reinterpret_cast<intptr_t>(&SerializedExtension::FunctionCallback),
    reinterpret_cast<intptr_t>(&serialized_static_field),
    reinterpret_cast<intptr_t>(&serializable_one_byte_resource),
    reinterpret_cast<intptr_t>(&serializable_two_byte_resource),
    0};

intptr_t short_external_references[] = {
    reinterpret_cast<intptr_t>(SerializedCallbackReplacement), 0};

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

      CHECK(context->Global()
                ->Set(context, v8_str("one_byte"),
                      v8::String::NewExternalOneByte(
                          isolate, &serializable_one_byte_resource)
                          .ToLocalChecked())
                .FromJust());
      CHECK(context->Global()
                ->Set(context, v8_str("two_byte"),
                      v8::String::NewExternalTwoByte(
                          isolate, &serializable_two_byte_resource)
                          .ToLocalChecked())
                .FromJust());

      ExpectInt32("f()", 42);
      ExpectString("one_byte", "one_byte");
      ExpectString("two_byte", "two_byte");
      creator.SetDefaultContext(context);
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  CHECK_EQ(1, serializable_one_byte_resource.dispose_count());
  CHECK_EQ(1, serializable_two_byte_resource.dispose_count());

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
      ExpectString("one_byte", "one_byte");
      ExpectString("two_byte", "two_byte");
      CHECK(CompileRun("one_byte").As<v8::String>()->IsExternalOneByte());
      CHECK(CompileRun("two_byte").As<v8::String>()->IsExternal());
    }
    isolate->Dispose();
  }

  CHECK_EQ(2, serializable_one_byte_resource.dispose_count());
  CHECK_EQ(2, serializable_two_byte_resource.dispose_count());

  // Deserialize with some other external reference.
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

  CHECK_EQ(3, serializable_one_byte_resource.dispose_count());
  CHECK_EQ(3, serializable_two_byte_resource.dispose_count());

  delete[] blob.data;
}

TEST(SnapshotCreatorShortExternalReferences) {
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

  // Deserialize with an incomplete list of external references.
  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = short_external_references;
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

v8::StartupData CreateSnapshotWithDefaultAndCustom() {
  v8::SnapshotCreator creator(original_external_references);
  v8::Isolate* isolate = creator.GetIsolate();
  {
    v8::HandleScope handle_scope(isolate);
    {
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      CompileRun("function f() { return 41; }");
      creator.SetDefaultContext(context);
      ExpectInt32("f()", 41);
    }
    {
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      v8::Local<v8::FunctionTemplate> function_template =
          v8::FunctionTemplate::New(isolate, SerializedCallback);
      v8::Local<v8::Value> function =
          function_template->GetFunction(context).ToLocalChecked();
      CHECK(context->Global()->Set(context, v8_str("f"), function).FromJust());
      v8::Local<v8::ObjectTemplate> object_template =
          v8::ObjectTemplate::New(isolate);
      object_template->SetAccessor(v8_str("x"), AccessorForSerialization);
      v8::Local<v8::Object> object =
          object_template->NewInstance(context).ToLocalChecked();
      CHECK(context->Global()->Set(context, v8_str("o"), object).FromJust());
      ExpectInt32("f()", 42);
      ExpectInt32("o.x", 2017);
      creator.AddContext(context);
    }
  }
  return creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
}

TEST(SnapshotCreatorNoExternalReferencesDefault) {
  DisableAlwaysOpt();
  v8::StartupData blob = CreateSnapshotWithDefaultAndCustom();

  // Deserialize with an incomplete list of external references.
  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = nullptr;
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 41);
    }
    isolate->Dispose();
  }
  delete[] blob.data;
}

TEST(SnapshotCreatorNoExternalReferencesCustomFail1) {
  DisableAlwaysOpt();
  v8::StartupData blob = CreateSnapshotWithDefaultAndCustom();

  // Deserialize with an incomplete list of external references.
  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = nullptr;
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context =
          v8::Context::FromSnapshot(isolate, 0).ToLocalChecked();
      v8::Context::Scope context_scope(context);
      ExpectInt32("f()", 42);
    }
    isolate->Dispose();
  }
  delete[] blob.data;
}

TEST(SnapshotCreatorNoExternalReferencesCustomFail2) {
  DisableAlwaysOpt();
  v8::StartupData blob = CreateSnapshotWithDefaultAndCustom();

  // Deserialize with an incomplete list of external references.
  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    params.external_references = nullptr;
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context =
          v8::Context::FromSnapshot(isolate, 0).ToLocalChecked();
      v8::Context::Scope context_scope(context);
      ExpectInt32("o.x", 2017);
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

TEST(SnapshotCreatorAddData) {
  DisableAlwaysOpt();
  v8::StartupData blob;

  {
    v8::SnapshotCreator creator;
    v8::Isolate* isolate = creator.GetIsolate();
    v8::Eternal<v8::Value> eternal_number;
    v8::Persistent<v8::Value> persistent_number_1;
    v8::Persistent<v8::Value> persistent_number_2;
    v8::Persistent<v8::Context> persistent_context;
    {
      v8::HandleScope handle_scope(isolate);

      eternal_number.Set(isolate, v8_num(2017));
      persistent_number_1.Reset(isolate, v8_num(2018));
      persistent_number_2.Reset(isolate, v8_num(2019));

      v8::Local<v8::Context> context = v8::Context::New(isolate);
      CHECK_EQ(0u, creator.AddData(context, persistent_number_2.Get(isolate)));
      creator.SetDefaultContext(context);
      context = v8::Context::New(isolate);
      persistent_context.Reset(isolate, context);

      v8::Context::Scope context_scope(context);

      v8::Local<v8::Object> object = CompileRun("({ p: 12 })").As<v8::Object>();

      v8::Local<v8::ObjectTemplate> object_template =
          v8::ObjectTemplate::New(isolate);
      object_template->SetInternalFieldCount(3);

      v8::Local<v8::Private> private_symbol =
          v8::Private::ForApi(isolate, v8_str("private_symbol"));

      v8::Local<v8::Signature> signature =
        v8::Signature::New(isolate, v8::FunctionTemplate::New(isolate));

      v8::Local<v8::AccessorSignature> accessor_signature =
           v8::AccessorSignature::New(isolate,
                                      v8::FunctionTemplate::New(isolate));

      CHECK_EQ(0u, creator.AddData(context, object));
      CHECK_EQ(1u, creator.AddData(context, v8_str("context-dependent")));
      CHECK_EQ(2u, creator.AddData(context, persistent_number_1.Get(isolate)));
      CHECK_EQ(3u, creator.AddData(context, object_template));
      CHECK_EQ(4u, creator.AddData(context, persistent_context.Get(isolate)));
      creator.AddContext(context);

      CHECK_EQ(0u, creator.AddData(v8_str("context-independent")));
      CHECK_EQ(1u, creator.AddData(eternal_number.Get(isolate)));
      CHECK_EQ(2u, creator.AddData(object_template));
      CHECK_EQ(3u, creator.AddData(v8::FunctionTemplate::New(isolate)));
      CHECK_EQ(4u, creator.AddData(private_symbol));
      CHECK_EQ(5u, creator.AddData(signature));
      CHECK_EQ(6u, creator.AddData(accessor_signature));
    }

    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context =
          v8::Context::FromSnapshot(isolate, 0).ToLocalChecked();

      // Check serialized data on the context.
      v8::Local<v8::Object> object =
          context->GetDataFromSnapshotOnce<v8::Object>(0).ToLocalChecked();
      CHECK(context->GetDataFromSnapshotOnce<v8::Object>(0).IsEmpty());
      CHECK_EQ(12, object->Get(context, v8_str("p"))
                       .ToLocalChecked()
                       ->Int32Value(context)
                       .FromJust());

      v8::Local<v8::String> string =
          context->GetDataFromSnapshotOnce<v8::String>(1).ToLocalChecked();
      CHECK(context->GetDataFromSnapshotOnce<v8::String>(1).IsEmpty());
      CHECK(string->Equals(context, v8_str("context-dependent")).FromJust());

      v8::Local<v8::Number> number =
          context->GetDataFromSnapshotOnce<v8::Number>(2).ToLocalChecked();
      CHECK(context->GetDataFromSnapshotOnce<v8::Number>(2).IsEmpty());
      CHECK_EQ(2018, number->Int32Value(context).FromJust());

      v8::Local<v8::ObjectTemplate> templ =
          context->GetDataFromSnapshotOnce<v8::ObjectTemplate>(3)
              .ToLocalChecked();
      CHECK(context->GetDataFromSnapshotOnce<v8::ObjectTemplate>(3).IsEmpty());
      CHECK_EQ(3, templ->InternalFieldCount());

      v8::Local<v8::Context> serialized_context =
          context->GetDataFromSnapshotOnce<v8::Context>(4).ToLocalChecked();
      CHECK(context->GetDataFromSnapshotOnce<v8::Context>(4).IsEmpty());
      CHECK_EQ(*v8::Utils::OpenHandle(*serialized_context),
               *v8::Utils::OpenHandle(*context));

      CHECK(context->GetDataFromSnapshotOnce<v8::Value>(5).IsEmpty());

      // Check serialized data on the isolate.
      string = isolate->GetDataFromSnapshotOnce<v8::String>(0).ToLocalChecked();
      CHECK(context->GetDataFromSnapshotOnce<v8::String>(0).IsEmpty());
      CHECK(string->Equals(context, v8_str("context-independent")).FromJust());

      number = isolate->GetDataFromSnapshotOnce<v8::Number>(1).ToLocalChecked();
      CHECK(isolate->GetDataFromSnapshotOnce<v8::Number>(1).IsEmpty());
      CHECK_EQ(2017, number->Int32Value(context).FromJust());

      templ = isolate->GetDataFromSnapshotOnce<v8::ObjectTemplate>(2)
                  .ToLocalChecked();
      CHECK(isolate->GetDataFromSnapshotOnce<v8::ObjectTemplate>(2).IsEmpty());
      CHECK_EQ(3, templ->InternalFieldCount());

      isolate->GetDataFromSnapshotOnce<v8::FunctionTemplate>(3)
          .ToLocalChecked();
      CHECK(
          isolate->GetDataFromSnapshotOnce<v8::FunctionTemplate>(3).IsEmpty());

      isolate->GetDataFromSnapshotOnce<v8::Private>(4).ToLocalChecked();
      CHECK(
          isolate->GetDataFromSnapshotOnce<v8::Private>(4).IsEmpty());

      isolate->GetDataFromSnapshotOnce<v8::Signature>(5).ToLocalChecked();
      CHECK(isolate->GetDataFromSnapshotOnce<v8::Signature>(5).IsEmpty());

      isolate->GetDataFromSnapshotOnce<v8::AccessorSignature>(6)
          .ToLocalChecked();
      CHECK(
          isolate->GetDataFromSnapshotOnce<v8::AccessorSignature>(6).IsEmpty());

      CHECK(isolate->GetDataFromSnapshotOnce<v8::Value>(7).IsEmpty());
    }
    isolate->Dispose();
  }
  {
    SnapshotCreator creator(nullptr, &blob);
    v8::Isolate* isolate = creator.GetIsolate();
    {
      // Adding data to a snapshot replaces the list of existing data.
      v8::HandleScope hscope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      creator.SetDefaultContext(context);
      context = v8::Context::FromSnapshot(isolate, 0).ToLocalChecked();
      v8::Local<v8::String> string =
          context->GetDataFromSnapshotOnce<v8::String>(1).ToLocalChecked();
      CHECK(context->GetDataFromSnapshotOnce<v8::String>(1).IsEmpty());
      CHECK(string->Equals(context, v8_str("context-dependent")).FromJust());
      v8::Local<v8::Number> number =
          isolate->GetDataFromSnapshotOnce<v8::Number>(1).ToLocalChecked();
      CHECK(isolate->GetDataFromSnapshotOnce<v8::Number>(1).IsEmpty());
      CHECK_EQ(2017, number->Int32Value(context).FromJust());

      CHECK_EQ(0u, creator.AddData(context, v8_num(2016)));
      CHECK_EQ(0u, creator.AddContext(context));
      CHECK_EQ(0u, creator.AddData(v8_str("stuff")));
    }
    delete[] blob.data;
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }
  {
    v8::Isolate::CreateParams params;
    params.snapshot_blob = &blob;
    params.array_buffer_allocator = CcTest::array_buffer_allocator();
    // Test-appropriate equivalent of v8::Isolate::New.
    v8::Isolate* isolate = TestIsolate::New(params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);

      // Context where we did not re-add data no longer has data.
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      CHECK(context->GetDataFromSnapshotOnce<v8::Object>(0).IsEmpty());

      // Context where we re-added data has completely new ones.
      context = v8::Context::FromSnapshot(isolate, 0).ToLocalChecked();
      v8::Local<v8::Value> value =
          context->GetDataFromSnapshotOnce<v8::Value>(0).ToLocalChecked();
      CHECK_EQ(2016, value->Int32Value(context).FromJust());
      CHECK(context->GetDataFromSnapshotOnce<v8::Value>(1).IsEmpty());

      // Ditto for the isolate.
      v8::Local<v8::String> string =
          isolate->GetDataFromSnapshotOnce<v8::String>(0).ToLocalChecked();
      CHECK(string->Equals(context, v8_str("stuff")).FromJust());
      CHECK(context->GetDataFromSnapshotOnce<v8::String>(1).IsEmpty());
    }
    isolate->Dispose();
  }
  delete[] blob.data;
}

TEST(SnapshotCreatorUnknownHandles) {
  DisableAlwaysOpt();
  v8::StartupData blob;

  {
    v8::SnapshotCreator creator;
    v8::Isolate* isolate = creator.GetIsolate();
    v8::Eternal<v8::Value> eternal_number;
    v8::Persistent<v8::Value> persistent_number;
    {
      v8::HandleScope handle_scope(isolate);

      eternal_number.Set(isolate, v8_num(2017));
      persistent_number.Reset(isolate, v8_num(2018));

      v8::Local<v8::Context> context = v8::Context::New(isolate);
      creator.SetDefaultContext(context);
    }

    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
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

UNINITIALIZED_TEST(ReinitializeHashSeedNotRehashable) {
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
      // Create an object with an ordered hash table.
      CompileRun(
          "var m = new Map();"
          "m.set('a', 1);"
          "m.set('b', 2);");
      ExpectInt32("m.get('b')", 2);
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
    ExpectInt32("m.get('b')", 2);
  }
  isolate->Dispose();
  delete[] blob.data;
}

UNINITIALIZED_TEST(ReinitializeHashSeedRehashable) {
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
          "var a = new Array(10000);"
          "%NormalizeElements(a);"
          "a[133] = 1;"
          "a[177] = 2;"
          "a[971] = 3;"
          "a[7997] = 4;"
          "a[2111] = 5;"
          "var o = {};"
          "%OptimizeObjectForAddingMultipleProperties(o, 3);"
          "o.a = 1;"
          "o.b = 2;"
          "o.c = 3;"
          "var p = { foo: 1 };"  // Test rehashing of transition arrays.
          "p = JSON.parse('{\"foo\": {\"x\": 1}}');");
      i::Handle<i::Object> i_a = v8::Utils::OpenHandle(*CompileRun("a"));
      i::Handle<i::Object> i_o = v8::Utils::OpenHandle(*CompileRun("o"));
      CHECK(i_a->IsJSArray());
      CHECK(i_a->IsJSObject());
      CHECK(!i::Handle<i::JSArray>::cast(i_a)->HasFastElements());
      CHECK(!i::Handle<i::JSObject>::cast(i_o)->HasFastProperties());
      ExpectInt32("a[2111]", 5);
      ExpectInt32("o.c", 3);
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
    // Check that rehashing has been performed.
    CHECK_EQ(1337, reinterpret_cast<i::Isolate*>(isolate)->heap()->HashSeed());
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    CHECK(!context.IsEmpty());
    v8::Context::Scope context_scope(context);
    i::Handle<i::Object> i_a = v8::Utils::OpenHandle(*CompileRun("a"));
    i::Handle<i::Object> i_o = v8::Utils::OpenHandle(*CompileRun("o"));
    CHECK(i_a->IsJSArray());
    CHECK(i_a->IsJSObject());
    CHECK(!i::Handle<i::JSArray>::cast(i_a)->HasFastElements());
    CHECK(!i::Handle<i::JSObject>::cast(i_o)->HasFastProperties());
    ExpectInt32("a[2111]", 5);
    ExpectInt32("o.c", 3);
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

TEST(BuiltinsHaveBuiltinIdForLazyDeserialization) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);

  CHECK(Builtins::IsLazy(Builtins::kRegExpPrototypeExec));
  CHECK_EQ(Builtins::kRegExpPrototypeExec,
           isolate->regexp_exec_function()
               ->shared()
               ->lazy_deserialization_builtin_id());
  CHECK(Builtins::IsLazy(Builtins::kAsyncIteratorValueUnwrap));
  CHECK_EQ(Builtins::kAsyncIteratorValueUnwrap,
           isolate->async_iterator_value_unwrap_shared_fun()
               ->lazy_deserialization_builtin_id());

  CHECK(!Builtins::IsLazy(Builtins::kIllegal));
  CHECK(!isolate->opaque_reference_function()
             ->shared()
             ->HasLazyDeserializationBuiltinId());
}

}  // namespace internal
}  // namespace v8
