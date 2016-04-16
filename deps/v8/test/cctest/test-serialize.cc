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

#include "src/ast/scopeinfo.h"
#include "src/bootstrapper.h"
#include "src/compilation-cache.h"
#include "src/debug/debug.h"
#include "src/heap/spaces.h"
#include "src/objects.h"
#include "src/parsing/parser.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/serialize.h"
#include "src/snapshot/snapshot.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/utils-inl.h"

using namespace v8::internal;


bool DefaultSnapshotAvailable() {
  return i::Snapshot::DefaultSnapshotBlob() != NULL;
}


void DisableTurbofan() {
  const char* flag = "--turbo-filter=\"\"";
  FlagList::SetFlagsFromString(flag, StrLength(flag));
}


// TestIsolate is used for testing isolate serialization.
class TestIsolate : public Isolate {
 public:
  static v8::Isolate* NewInitialized(bool enable_serializer) {
    i::Isolate* isolate = new TestIsolate(enable_serializer);
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Isolate::Scope isolate_scope(v8_isolate);
    isolate->Init(NULL);
    return v8_isolate;
  }
  explicit TestIsolate(bool enable_serializer) : Isolate(enable_serializer) {
    set_array_buffer_allocator(CcTest::array_buffer_allocator());
  }
};


void WritePayload(const Vector<const byte>& payload, const char* file_name) {
  FILE* file = v8::base::OS::FOpen(file_name, "wb");
  if (file == NULL) {
    PrintF("Unable to write to snapshot file \"%s\"\n", file_name);
    exit(1);
  }
  size_t written = fwrite(payload.begin(), 1, payload.length(), file);
  if (written != static_cast<size_t>(payload.length())) {
    i::PrintF("Writing snapshot file failed.. Aborting.\n");
    exit(1);
  }
  fclose(file);
}


static bool WriteToFile(Isolate* isolate, const char* snapshot_file) {
  SnapshotByteSink sink;
  StartupSerializer ser(isolate, &sink);
  ser.SerializeStrongReferences();
  ser.SerializeWeakReferencesAndDeferred();
  SnapshotData snapshot_data(ser);
  WritePayload(snapshot_data.RawData(), snapshot_file);
  return true;
}


static void Serialize(v8::Isolate* isolate) {
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
  internal_isolate->heap()->CollectAllAvailableGarbage("serialize");
  WriteToFile(internal_isolate, FLAG_testing_serialization_file);
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


// Test that the whole heap can be serialized.
UNINITIALIZED_TEST(Serialize) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  Serialize(isolate);
}


// Test that heap serialization is non-destructive.
UNINITIALIZED_TEST(SerializeTwice) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  v8::Isolate* isolate = TestIsolate::NewInitialized(true);
  Serialize(isolate);
  Serialize(isolate);
}


//----------------------------------------------------------------------------
// Tests that the heap can be deserialized.

v8::Isolate* InitializeFromFile(const char* snapshot_file) {
  int len;
  byte* str = ReadBytes(snapshot_file, &len);
  if (!str) return NULL;
  v8::Isolate* v8_isolate = NULL;
  {
    SnapshotData snapshot_data(Vector<const byte>(str, len));
    Deserializer deserializer(&snapshot_data);
    Isolate* isolate = new TestIsolate(false);
    v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    v8::Isolate::Scope isolate_scope(v8_isolate);
    isolate->Init(&deserializer);
  }
  DeleteArray(str);
  return v8_isolate;
}


static v8::Isolate* Deserialize() {
  v8::Isolate* isolate = InitializeFromFile(FLAG_testing_serialization_file);
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


UNINITIALIZED_DEPENDENT_TEST(Deserialize, Serialize) {
  // The serialize-deserialize tests only work if the VM is built without
  // serialization.  That doesn't matter.  We don't need to be able to
  // serialize a snapshot in a VM that is booted from a snapshot.
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  v8::Isolate* isolate = Deserialize();
  {
    v8::HandleScope handle_scope(isolate);
    v8::Isolate::Scope isolate_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck(isolate);
  }
  isolate->Dispose();
}


UNINITIALIZED_DEPENDENT_TEST(DeserializeFromSecondSerialization,
                             SerializeTwice) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  v8::Isolate* isolate = Deserialize();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck(isolate);
  }
  isolate->Dispose();
}


UNINITIALIZED_DEPENDENT_TEST(DeserializeAndRunScript2, Serialize) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  v8::Isolate* isolate = Deserialize();
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


UNINITIALIZED_DEPENDENT_TEST(DeserializeFromSecondSerializationAndRunScript2,
                             SerializeTwice) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  v8::Isolate* isolate = Deserialize();
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


UNINITIALIZED_TEST(PartialSerialization) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
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
    // Make sure all builtin scripts are cached.
    {
      HandleScope scope(isolate);
      for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
        isolate->bootstrapper()->SourceLookup<Natives>(i);
      }
    }
    heap->CollectAllGarbage();
    heap->CollectAllGarbage();

    Object* raw_foo;
    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::String> foo = v8_str("foo");
      CHECK(!foo.IsEmpty());
      raw_foo = *(v8::Utils::OpenHandle(*foo));
    }

    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Exit();
    }
    env.Reset();

    SnapshotByteSink startup_sink;
    StartupSerializer startup_serializer(isolate, &startup_sink);
    startup_serializer.SerializeStrongReferences();

    SnapshotByteSink partial_sink;
    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         &partial_sink);
    partial_serializer.Serialize(&raw_foo);

    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(startup_serializer);
    SnapshotData partial_snapshot(partial_serializer);

    WritePayload(partial_snapshot.RawData(), FLAG_testing_serialization_file);
    WritePayload(startup_snapshot.RawData(), startup_name.start());

    startup_name.Dispose();
  }
  v8_isolate->Exit();
  v8_isolate->Dispose();
}


UNINITIALIZED_DEPENDENT_TEST(PartialDeserialization, PartialSerialization) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
  Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
  SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

  v8::Isolate* v8_isolate = InitializeFromFile(startup_name.start());
  CHECK(v8_isolate);
  startup_name.Dispose();
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    const char* file_name = FLAG_testing_serialization_file;

    int snapshot_size = 0;
    byte* snapshot = ReadBytes(file_name, &snapshot_size);

    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    HandleScope handle_scope(isolate);
    Handle<Object> root;
    // Intentionally empty handle. The deserializer should not come across
    // any references to the global proxy in this test.
    Handle<JSGlobalProxy> global_proxy = Handle<JSGlobalProxy>::null();
    {
      SnapshotData snapshot_data(Vector<const byte>(snapshot, snapshot_size));
      Deserializer deserializer(&snapshot_data);
      root = deserializer.DeserializePartial(isolate, global_proxy)
                 .ToHandleChecked();
      CHECK(root->IsString());
    }

    Handle<Object> root2;
    {
      SnapshotData snapshot_data(Vector<const byte>(snapshot, snapshot_size));
      Deserializer deserializer(&snapshot_data);
      root2 = deserializer.DeserializePartial(isolate, global_proxy)
                  .ToHandleChecked();
      CHECK(root2->IsString());
      CHECK(root.is_identical_to(root2));
    }

    DeleteArray(snapshot);
  }
  v8_isolate->Dispose();
}


UNINITIALIZED_TEST(ContextSerialization) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
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
    // Make sure all builtin scripts are cached.
    {
      HandleScope scope(isolate);
      for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
        isolate->bootstrapper()->SourceLookup<Natives>(i);
      }
    }
    // If we don't do this then we end up with a stray root pointing at the
    // context even after we have disposed of env.
    heap->CollectAllGarbage();

    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Exit();
    }

    i::Object* raw_context = *v8::Utils::OpenPersistent(env);

    env.Reset();

    SnapshotByteSink startup_sink;
    StartupSerializer startup_serializer(isolate, &startup_sink);
    startup_serializer.SerializeStrongReferences();

    SnapshotByteSink partial_sink;
    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         &partial_sink);
    partial_serializer.Serialize(&raw_context);
    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(startup_serializer);
    SnapshotData partial_snapshot(partial_serializer);

    WritePayload(partial_snapshot.RawData(), FLAG_testing_serialization_file);
    WritePayload(startup_snapshot.RawData(), startup_name.start());

    startup_name.Dispose();
  }
  v8_isolate->Dispose();
}


UNINITIALIZED_DEPENDENT_TEST(ContextDeserialization, ContextSerialization) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
  int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
  Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
  SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

  v8::Isolate* v8_isolate = InitializeFromFile(startup_name.start());
  CHECK(v8_isolate);
  startup_name.Dispose();
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    const char* file_name = FLAG_testing_serialization_file;

    int snapshot_size = 0;
    byte* snapshot = ReadBytes(file_name, &snapshot_size);

    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    HandleScope handle_scope(isolate);
    Handle<Object> root;
    Handle<JSGlobalProxy> global_proxy =
        isolate->factory()->NewUninitializedJSGlobalProxy();
    {
      SnapshotData snapshot_data(Vector<const byte>(snapshot, snapshot_size));
      Deserializer deserializer(&snapshot_data);
      root = deserializer.DeserializePartial(isolate, global_proxy)
                 .ToHandleChecked();
      CHECK(root->IsContext());
      CHECK(Handle<Context>::cast(root)->global_proxy() == *global_proxy);
    }

    Handle<Object> root2;
    {
      SnapshotData snapshot_data(Vector<const byte>(snapshot, snapshot_size));
      Deserializer deserializer(&snapshot_data);
      root2 = deserializer.DeserializePartial(isolate, global_proxy)
                  .ToHandleChecked();
      CHECK(root2->IsContext());
      CHECK(!root.is_identical_to(root2));
    }
    DeleteArray(snapshot);
  }
  v8_isolate->Dispose();
}


UNINITIALIZED_TEST(CustomContextSerialization) {
  DisableTurbofan();
  if (DefaultSnapshotAvailable()) return;
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
          "var r = Math.sin(0) + Math.cos(0);"
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
    // Make sure all builtin scripts are cached.
    {
      HandleScope scope(isolate);
      for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
        isolate->bootstrapper()->SourceLookup<Natives>(i);
      }
    }
    // If we don't do this then we end up with a stray root pointing at the
    // context even after we have disposed of env.
    isolate->heap()->CollectAllAvailableGarbage("snapshotting");

    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Exit();
    }

    i::Object* raw_context = *v8::Utils::OpenPersistent(env);

    env.Reset();

    SnapshotByteSink startup_sink;
    StartupSerializer startup_serializer(isolate, &startup_sink);
    startup_serializer.SerializeStrongReferences();

    SnapshotByteSink partial_sink;
    PartialSerializer partial_serializer(isolate, &startup_serializer,
                                         &partial_sink);
    partial_serializer.Serialize(&raw_context);
    startup_serializer.SerializeWeakReferencesAndDeferred();

    SnapshotData startup_snapshot(startup_serializer);
    SnapshotData partial_snapshot(partial_serializer);

    WritePayload(partial_snapshot.RawData(), FLAG_testing_serialization_file);
    WritePayload(startup_snapshot.RawData(), startup_name.start());

    startup_name.Dispose();
  }
  v8_isolate->Dispose();
}


UNINITIALIZED_DEPENDENT_TEST(CustomContextDeserialization,
                             CustomContextSerialization) {
  DisableTurbofan();
  FLAG_crankshaft = false;
  if (DefaultSnapshotAvailable()) return;
  int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
  Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
  SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

  v8::Isolate* v8_isolate = InitializeFromFile(startup_name.start());
  CHECK(v8_isolate);
  startup_name.Dispose();
  {
    v8::Isolate::Scope isolate_scope(v8_isolate);

    const char* file_name = FLAG_testing_serialization_file;

    int snapshot_size = 0;
    byte* snapshot = ReadBytes(file_name, &snapshot_size);

    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    HandleScope handle_scope(isolate);
    Handle<Object> root;
    Handle<JSGlobalProxy> global_proxy =
        isolate->factory()->NewUninitializedJSGlobalProxy();
    {
      SnapshotData snapshot_data(Vector<const byte>(snapshot, snapshot_size));
      Deserializer deserializer(&snapshot_data);
      root = deserializer.DeserializePartial(isolate, global_proxy)
                 .ToHandleChecked();
      CHECK(root->IsContext());
      Handle<Context> context = Handle<Context>::cast(root);
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
      CHECK_EQ(1, r);
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
    DeleteArray(snapshot);
  }
  v8_isolate->Dispose();
}


TEST(PerIsolateSnapshotBlobs) {
  DisableTurbofan();
  const char* source1 = "function f() { return 42; }";
  const char* source2 =
      "function f() { return g() * 2; }"
      "function g() { return 43; }"
      "/./.test('a')";

  v8::StartupData data1 = v8::V8::CreateSnapshotDataBlob(source1);
  v8::StartupData data2 = v8::V8::CreateSnapshotDataBlob(source2);

  v8::Isolate::CreateParams params1;
  params1.snapshot_blob = &data1;
  params1.array_buffer_allocator = CcTest::array_buffer_allocator();

  v8::Isolate* isolate1 = v8::Isolate::New(params1);
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

  v8::Isolate::CreateParams params2;
  params2.snapshot_blob = &data2;
  params2.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate2 = v8::Isolate::New(params2);
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


TEST(PerIsolateSnapshotBlobsOutdatedContextWithOverflow) {
  DisableTurbofan();

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

  v8::Isolate* isolate = v8::Isolate::New(params);
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


TEST(PerIsolateSnapshotBlobsWithLocker) {
  DisableTurbofan();
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
  v8::Isolate* isolate1 = v8::Isolate::New(params1);
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


TEST(SnapshotBlobsStackOverflow) {
  DisableTurbofan();
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

  v8::Isolate* isolate = v8::Isolate::New(params);
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


TEST(TestThatAlwaysSucceeds) {
}


TEST(TestThatAlwaysFails) {
  bool ArtificialFailure = false;
  CHECK(ArtificialFailure);
}


DEPENDENT_TEST(DependentTestThatAlwaysFails, TestThatAlwaysSucceeds) {
  bool ArtificialFailure2 = false;
  CHECK(ArtificialFailure2);
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
  return Compiler::CompileScript(
      source, name, 0, 0, v8::ScriptOriginOptions(), Handle<Object>(),
      Handle<Context>(isolate->native_context()), NULL, cached_data, options,
      NOT_NATIVES_CODE, false);
}


TEST(SerializeToplevelOnePlusOne) {
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


TEST(CodeCachePromotedToCompilationCache) {
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

  CHECK(isolate->compilation_cache()
            ->LookupScript(src, src, 0, 0, v8::ScriptOriginOptions(),
                           isolate->native_context(), SLOPPY)
            .ToHandleChecked()
            .is_identical_to(copy));

  delete cache;
}


TEST(SerializeToplevelInternalizedString) {
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


TEST(SerializeToplevelLargeCodeObject) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  Vector<const uint8_t> source =
      ConstructSource(STATIC_CHAR_VECTOR("var j=1; try { if (j) throw 1;"),
                      STATIC_CHAR_VECTOR("for(var i=0;i<1;i++)j++;"),
                      STATIC_CHAR_VECTOR("} catch (e) { j=7; } j"), 10000);
  Handle<String> source_str =
      isolate->factory()->NewStringFromOneByte(source).ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig =
      CompileScript(isolate, source_str, Handle<String>(), &cache,
                    v8::ScriptCompiler::kProduceCodeCache);

  CHECK(isolate->heap()->InSpace(orig->code(), LO_SPACE));

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


TEST(SerializeToplevelLargeStrings) {
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


TEST(SerializeToplevelThreeBigStrings) {
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
                      STATIC_CHAR_VECTOR("\";"), 600000);
  Handle<String> source_b_str =
      f->NewStringFromOneByte(source_b).ToHandleChecked();

  Vector<const uint8_t> source_c =
      ConstructSource(STATIC_CHAR_VECTOR("var c = \""), STATIC_CHAR_VECTOR("c"),
                      STATIC_CHAR_VECTOR("\";"), 500000);
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
  CHECK_EQ(600000 + 700000, result.FromJust());
  result = CompileRun("(b + c).length")
               ->Int32Value(v8::Isolate::GetCurrent()->GetCurrentContext());
  CHECK_EQ(500000 + 600000, result.FromJust());
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


TEST(SerializeToplevelExternalString) {
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


TEST(SerializeToplevelLargeExternalString) {
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


TEST(SerializeToplevelExternalScriptName) {
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


TEST(SerializeToplevelIsolates) {
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


TEST(SerializeToplevelFlagChange) {
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


TEST(SerializeToplevelBitFlip) {
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


TEST(SerializeWithHarmonyScoping) {
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


TEST(SerializeInternalReference) {
#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64
  return;
#endif
  // Disable experimental natives that are loaded after deserialization.
  FLAG_function_context_specialization = false;
  FLAG_always_opt = true;
  const char* flag = "--turbo-filter=foo";
  FlagList::SetFlagsFromString(flag, StrLength(flag));

  const char* source =
      "var foo = (function(stdlib, foreign, heap) {"
      "  function foo(i) {"
      "    i = i|0;"
      "    var j = 0;"
      "    switch (i) {"
      "      case 0:"
      "      case 1: j = 1; break;"
      "      case 2:"
      "      case 3: j = 2; break;"
      "      case 4:"
      "      case 5: j = foo(3) + 1; break;"
      "      default: j = 0; break;"
      "    }"
      "    return j + 10;"
      "  }"
      "  return { foo: foo };"
      "})(this, {}, undefined).foo;"
      "foo(1);";

  v8::StartupData data = v8::V8::CreateSnapshotDataBlob(source);
  CHECK(data.data);

  v8::Isolate::CreateParams params;
  params.snapshot_blob = &data;
  params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(params);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope h_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    delete[] data.data;  // We can dispose of the snapshot blob now.
    v8::Context::Scope c_scope(context);
    v8::Local<v8::Function> foo =
        v8::Local<v8::Function>::Cast(CompileRun("foo"));

    // There are at least 6 internal references.
    int mask = RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
               RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED);
    RelocIterator it(
        Handle<JSFunction>::cast(v8::Utils::OpenHandle(*foo))->code(), mask);
    for (int i = 0; i < 6; ++i) {
      CHECK(!it.done());
      it.next();
    }

    CHECK(Handle<JSFunction>::cast(v8::Utils::OpenHandle(*foo))
              ->code()
              ->is_turbofanned());
    CHECK_EQ(11, CompileRun("foo(0)")
                     ->Int32Value(isolate->GetCurrentContext())
                     .FromJust());
    CHECK_EQ(11, CompileRun("foo(1)")
                     ->Int32Value(isolate->GetCurrentContext())
                     .FromJust());
    CHECK_EQ(12, CompileRun("foo(2)")
                     ->Int32Value(isolate->GetCurrentContext())
                     .FromJust());
    CHECK_EQ(12, CompileRun("foo(3)")
                     ->Int32Value(isolate->GetCurrentContext())
                     .FromJust());
    CHECK_EQ(23, CompileRun("foo(4)")
                     ->Int32Value(isolate->GetCurrentContext())
                     .FromJust());
    CHECK_EQ(23, CompileRun("foo(5)")
                     ->Int32Value(isolate->GetCurrentContext())
                     .FromJust());
    CHECK_EQ(10, CompileRun("foo(6)")
                     ->Int32Value(isolate->GetCurrentContext())
                     .FromJust());
  }
  isolate->Dispose();
}


TEST(Regress503552) {
  // Test that the code serializer can deal with weak cells that form a linked
  // list during incremental marking.

  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  HandleScope scope(isolate);
  Handle<String> source = isolate->factory()->NewStringFromAsciiChecked(
      "function f() {} function g() {}");
  ScriptData* script_data = NULL;
  Handle<SharedFunctionInfo> shared = Compiler::CompileScript(
      source, Handle<String>(), 0, 0, v8::ScriptOriginOptions(),
      Handle<Object>(), Handle<Context>(isolate->native_context()), NULL,
      &script_data, v8::ScriptCompiler::kProduceCodeCache, NOT_NATIVES_CODE,
      false);
  delete script_data;

  SimulateIncrementalMarking(isolate->heap());

  script_data = CodeSerializer::Serialize(isolate, shared, source);
  delete script_data;
}


TEST(SerializationMemoryStats) {
  FLAG_profile_deserialization = true;
  FLAG_always_opt = false;
  v8::StartupData blob = v8::V8::CreateSnapshotDataBlob();
  delete[] blob.data;
}
