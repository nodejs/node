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

#include "src/bootstrapper.h"
#include "src/compilation-cache.h"
#include "src/debug.h"
#include "src/heap/spaces.h"
#include "src/natives.h"
#include "src/objects.h"
#include "src/runtime/runtime.h"
#include "src/scopeinfo.h"
#include "src/serialize.h"
#include "src/snapshot.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


template <class T>
static Address AddressOf(T id) {
  return ExternalReference(id, CcTest::i_isolate()).address();
}


template <class T>
static uint32_t Encode(const ExternalReferenceEncoder& encoder, T id) {
  return encoder.Encode(AddressOf(id));
}


static int make_code(TypeCode type, int id) {
  return static_cast<uint32_t>(type) << kReferenceTypeShift | id;
}


TEST(ExternalReferenceEncoder) {
  Isolate* isolate = CcTest::i_isolate();
  v8::V8::Initialize();

  ExternalReferenceEncoder encoder(isolate);
  CHECK_EQ(make_code(BUILTIN, Builtins::kArrayCode),
           Encode(encoder, Builtins::kArrayCode));
  CHECK_EQ(make_code(v8::internal::RUNTIME_FUNCTION, Runtime::kAbort),
           Encode(encoder, Runtime::kAbort));
  ExternalReference stack_limit_address =
      ExternalReference::address_of_stack_limit(isolate);
  CHECK_EQ(make_code(UNCLASSIFIED, 2),
           encoder.Encode(stack_limit_address.address()));
  ExternalReference real_stack_limit_address =
      ExternalReference::address_of_real_stack_limit(isolate);
  CHECK_EQ(make_code(UNCLASSIFIED, 3),
           encoder.Encode(real_stack_limit_address.address()));
  CHECK_EQ(make_code(UNCLASSIFIED, 8),
           encoder.Encode(ExternalReference::debug_break(isolate).address()));
  CHECK_EQ(
      make_code(UNCLASSIFIED, 4),
      encoder.Encode(ExternalReference::new_space_start(isolate).address()));
  CHECK_EQ(
      make_code(UNCLASSIFIED, 1),
      encoder.Encode(ExternalReference::roots_array_start(isolate).address()));
  CHECK_EQ(make_code(UNCLASSIFIED, 34),
           encoder.Encode(ExternalReference::cpu_features().address()));
}


TEST(ExternalReferenceDecoder) {
  Isolate* isolate = CcTest::i_isolate();
  v8::V8::Initialize();

  ExternalReferenceDecoder decoder(isolate);
  CHECK_EQ(AddressOf(Builtins::kArrayCode),
           decoder.Decode(make_code(BUILTIN, Builtins::kArrayCode)));
  CHECK_EQ(AddressOf(Runtime::kAbort),
           decoder.Decode(make_code(v8::internal::RUNTIME_FUNCTION,
                                    Runtime::kAbort)));
  CHECK_EQ(ExternalReference::address_of_stack_limit(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 2)));
  CHECK_EQ(ExternalReference::address_of_real_stack_limit(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 3)));
  CHECK_EQ(ExternalReference::debug_break(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 8)));
  CHECK_EQ(ExternalReference::new_space_start(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 4)));
}


class FileByteSink : public SnapshotByteSink {
 public:
  explicit FileByteSink(const char* snapshot_file) {
    fp_ = v8::base::OS::FOpen(snapshot_file, "wb");
    file_name_ = snapshot_file;
    if (fp_ == NULL) {
      PrintF("Unable to write to snapshot file \"%s\"\n", snapshot_file);
      exit(1);
    }
  }
  virtual ~FileByteSink() {
    if (fp_ != NULL) {
      fclose(fp_);
    }
  }
  virtual void Put(byte b, const char* description) {
    if (fp_ != NULL) {
      fputc(b, fp_);
    }
  }
  virtual int Position() {
    return ftell(fp_);
  }
  void WriteSpaceUsed(int new_space_used, int pointer_space_used,
                      int data_space_used, int code_space_used,
                      int map_space_used, int cell_space_used,
                      int property_cell_space_used, int lo_space_used);

 private:
  FILE* fp_;
  const char* file_name_;
};


void FileByteSink::WriteSpaceUsed(int new_space_used, int pointer_space_used,
                                  int data_space_used, int code_space_used,
                                  int map_space_used, int cell_space_used,
                                  int property_cell_space_used,
                                  int lo_space_used) {
  int file_name_length = StrLength(file_name_) + 10;
  Vector<char> name = Vector<char>::New(file_name_length + 1);
  SNPrintF(name, "%s.size", file_name_);
  FILE* fp = v8::base::OS::FOpen(name.start(), "w");
  name.Dispose();
  fprintf(fp, "new %d\n", new_space_used);
  fprintf(fp, "pointer %d\n", pointer_space_used);
  fprintf(fp, "data %d\n", data_space_used);
  fprintf(fp, "code %d\n", code_space_used);
  fprintf(fp, "map %d\n", map_space_used);
  fprintf(fp, "cell %d\n", cell_space_used);
  fprintf(fp, "property cell %d\n", property_cell_space_used);
  fprintf(fp, "lo %d\n", lo_space_used);
  fclose(fp);
}


static bool WriteToFile(Isolate* isolate, const char* snapshot_file) {
  FileByteSink file(snapshot_file);
  StartupSerializer ser(isolate, &file);
  ser.Serialize();

  file.WriteSpaceUsed(ser.CurrentAllocationAddress(NEW_SPACE),
                      ser.CurrentAllocationAddress(OLD_POINTER_SPACE),
                      ser.CurrentAllocationAddress(OLD_DATA_SPACE),
                      ser.CurrentAllocationAddress(CODE_SPACE),
                      ser.CurrentAllocationAddress(MAP_SPACE),
                      ser.CurrentAllocationAddress(CELL_SPACE),
                      ser.CurrentAllocationAddress(PROPERTY_CELL_SPACE),
                      ser.CurrentAllocationAddress(LO_SPACE));

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
  internal_isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags, "serialize");
  WriteToFile(internal_isolate, FLAG_testing_serialization_file);
}


// Test that the whole heap can be serialized.
UNINITIALIZED_TEST(Serialize) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate::CreateParams params;
    params.enable_serializer = true;
    v8::Isolate* isolate = v8::Isolate::New(params);
    Serialize(isolate);
  }
}


// Test that heap serialization is non-destructive.
UNINITIALIZED_TEST(SerializeTwice) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate::CreateParams params;
    params.enable_serializer = true;
    v8::Isolate* isolate = v8::Isolate::New(params);
    Serialize(isolate);
    Serialize(isolate);
  }
}


//----------------------------------------------------------------------------
// Tests that the heap can be deserialized.


static void ReserveSpaceForSnapshot(Deserializer* deserializer,
                                    const char* file_name) {
  int file_name_length = StrLength(file_name) + 10;
  Vector<char> name = Vector<char>::New(file_name_length + 1);
  SNPrintF(name, "%s.size", file_name);
  FILE* fp = v8::base::OS::FOpen(name.start(), "r");
  name.Dispose();
  int new_size, pointer_size, data_size, code_size, map_size, cell_size,
      property_cell_size, lo_size;
#ifdef _MSC_VER
  // Avoid warning about unsafe fscanf from MSVC.
  // Please note that this is only fine if %c and %s are not being used.
#define fscanf fscanf_s
#endif
  CHECK_EQ(1, fscanf(fp, "new %d\n", &new_size));
  CHECK_EQ(1, fscanf(fp, "pointer %d\n", &pointer_size));
  CHECK_EQ(1, fscanf(fp, "data %d\n", &data_size));
  CHECK_EQ(1, fscanf(fp, "code %d\n", &code_size));
  CHECK_EQ(1, fscanf(fp, "map %d\n", &map_size));
  CHECK_EQ(1, fscanf(fp, "cell %d\n", &cell_size));
  CHECK_EQ(1, fscanf(fp, "property cell %d\n", &property_cell_size));
  CHECK_EQ(1, fscanf(fp, "lo %d\n", &lo_size));
#ifdef _MSC_VER
#undef fscanf
#endif
  fclose(fp);
  deserializer->set_reservation(NEW_SPACE, new_size);
  deserializer->set_reservation(OLD_POINTER_SPACE, pointer_size);
  deserializer->set_reservation(OLD_DATA_SPACE, data_size);
  deserializer->set_reservation(CODE_SPACE, code_size);
  deserializer->set_reservation(MAP_SPACE, map_size);
  deserializer->set_reservation(CELL_SPACE, cell_size);
  deserializer->set_reservation(PROPERTY_CELL_SPACE, property_cell_size);
  deserializer->set_reservation(LO_SPACE, lo_size);
}


v8::Isolate* InitializeFromFile(const char* snapshot_file) {
  int len;
  byte* str = ReadBytes(snapshot_file, &len);
  if (!str) return NULL;
  v8::Isolate* v8_isolate = NULL;
  {
    SnapshotByteSource source(str, len);
    Deserializer deserializer(&source);
    ReserveSpaceForSnapshot(&deserializer, snapshot_file);
    Isolate* isolate = Isolate::NewForTesting();
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
  if (!Snapshot::HaveASnapshotToStartFrom()) {
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
}


UNINITIALIZED_DEPENDENT_TEST(DeserializeFromSecondSerialization,
                             SerializeTwice) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
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
}


UNINITIALIZED_DEPENDENT_TEST(DeserializeAndRunScript2, Serialize) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate* isolate = Deserialize();
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);


      v8::Local<v8::Context> env = v8::Context::New(isolate);
      env->Enter();

      const char* c_source = "\"1234\".length";
      v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, c_source);
      v8::Local<v8::Script> script = v8::Script::Compile(source);
      CHECK_EQ(4, script->Run()->Int32Value());
    }
    isolate->Dispose();
  }
}


UNINITIALIZED_DEPENDENT_TEST(DeserializeFromSecondSerializationAndRunScript2,
                             SerializeTwice) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate* isolate = Deserialize();
    {
      v8::Isolate::Scope isolate_scope(isolate);
      v8::HandleScope handle_scope(isolate);

      v8::Local<v8::Context> env = v8::Context::New(isolate);
      env->Enter();

      const char* c_source = "\"1234\".length";
      v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, c_source);
      v8::Local<v8::Script> script = v8::Script::Compile(source);
      CHECK_EQ(4, script->Run()->Int32Value());
    }
    isolate->Dispose();
  }
}


UNINITIALIZED_TEST(PartialSerialization) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate::CreateParams params;
    params.enable_serializer = true;
    v8::Isolate* v8_isolate = v8::Isolate::New(params);
    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    v8_isolate->Enter();
    {
      Heap* heap = isolate->heap();

      v8::Persistent<v8::Context> env;
      {
        HandleScope scope(isolate);
        env.Reset(v8_isolate, v8::Context::New(v8_isolate));
      }
      DCHECK(!env.IsEmpty());
      {
        v8::HandleScope handle_scope(v8_isolate);
        v8::Local<v8::Context>::New(v8_isolate, env)->Enter();
      }
      // Make sure all builtin scripts are cached.
      {
        HandleScope scope(isolate);
        for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
          isolate->bootstrapper()->NativesSourceLookup(i);
        }
      }
      heap->CollectAllGarbage(Heap::kNoGCFlags);
      heap->CollectAllGarbage(Heap::kNoGCFlags);

      Object* raw_foo;
      {
        v8::HandleScope handle_scope(v8_isolate);
        v8::Local<v8::String> foo = v8::String::NewFromUtf8(v8_isolate, "foo");
        DCHECK(!foo.IsEmpty());
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

      FileByteSink startup_sink(startup_name.start());
      StartupSerializer startup_serializer(isolate, &startup_sink);
      startup_serializer.SerializeStrongReferences();

      FileByteSink partial_sink(FLAG_testing_serialization_file);
      PartialSerializer p_ser(isolate, &startup_serializer, &partial_sink);
      p_ser.Serialize(&raw_foo);
      startup_serializer.SerializeWeakReferences();

      partial_sink.WriteSpaceUsed(
          p_ser.CurrentAllocationAddress(NEW_SPACE),
          p_ser.CurrentAllocationAddress(OLD_POINTER_SPACE),
          p_ser.CurrentAllocationAddress(OLD_DATA_SPACE),
          p_ser.CurrentAllocationAddress(CODE_SPACE),
          p_ser.CurrentAllocationAddress(MAP_SPACE),
          p_ser.CurrentAllocationAddress(CELL_SPACE),
          p_ser.CurrentAllocationAddress(PROPERTY_CELL_SPACE),
          p_ser.CurrentAllocationAddress(LO_SPACE));

      startup_sink.WriteSpaceUsed(
          startup_serializer.CurrentAllocationAddress(NEW_SPACE),
          startup_serializer.CurrentAllocationAddress(OLD_POINTER_SPACE),
          startup_serializer.CurrentAllocationAddress(OLD_DATA_SPACE),
          startup_serializer.CurrentAllocationAddress(CODE_SPACE),
          startup_serializer.CurrentAllocationAddress(MAP_SPACE),
          startup_serializer.CurrentAllocationAddress(CELL_SPACE),
          startup_serializer.CurrentAllocationAddress(PROPERTY_CELL_SPACE),
          startup_serializer.CurrentAllocationAddress(LO_SPACE));
      startup_name.Dispose();
    }
    v8_isolate->Exit();
    v8_isolate->Dispose();
  }
}


UNINITIALIZED_DEPENDENT_TEST(PartialDeserialization, PartialSerialization) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
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
      Object* root;
      {
        SnapshotByteSource source(snapshot, snapshot_size);
        Deserializer deserializer(&source);
        ReserveSpaceForSnapshot(&deserializer, file_name);
        deserializer.DeserializePartial(isolate, &root);
        CHECK(root->IsString());
      }
      HandleScope handle_scope(isolate);
      Handle<Object> root_handle(root, isolate);


      Object* root2;
      {
        SnapshotByteSource source(snapshot, snapshot_size);
        Deserializer deserializer(&source);
        ReserveSpaceForSnapshot(&deserializer, file_name);
        deserializer.DeserializePartial(isolate, &root2);
        CHECK(root2->IsString());
        CHECK(*root_handle == root2);
      }
    }
    v8_isolate->Dispose();
  }
}


UNINITIALIZED_TEST(ContextSerialization) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate::CreateParams params;
    params.enable_serializer = true;
    v8::Isolate* v8_isolate = v8::Isolate::New(params);
    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    Heap* heap = isolate->heap();
    {
      v8::Isolate::Scope isolate_scope(v8_isolate);

      v8::Persistent<v8::Context> env;
      {
        HandleScope scope(isolate);
        env.Reset(v8_isolate, v8::Context::New(v8_isolate));
      }
      DCHECK(!env.IsEmpty());
      {
        v8::HandleScope handle_scope(v8_isolate);
        v8::Local<v8::Context>::New(v8_isolate, env)->Enter();
      }
      // Make sure all builtin scripts are cached.
      {
        HandleScope scope(isolate);
        for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
          isolate->bootstrapper()->NativesSourceLookup(i);
        }
      }
      // If we don't do this then we end up with a stray root pointing at the
      // context even after we have disposed of env.
      heap->CollectAllGarbage(Heap::kNoGCFlags);

      int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
      Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
      SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

      {
        v8::HandleScope handle_scope(v8_isolate);
        v8::Local<v8::Context>::New(v8_isolate, env)->Exit();
      }

      i::Object* raw_context = *v8::Utils::OpenPersistent(env);

      env.Reset();

      FileByteSink startup_sink(startup_name.start());
      StartupSerializer startup_serializer(isolate, &startup_sink);
      startup_serializer.SerializeStrongReferences();

      FileByteSink partial_sink(FLAG_testing_serialization_file);
      PartialSerializer p_ser(isolate, &startup_serializer, &partial_sink);
      p_ser.Serialize(&raw_context);
      startup_serializer.SerializeWeakReferences();

      partial_sink.WriteSpaceUsed(
          p_ser.CurrentAllocationAddress(NEW_SPACE),
          p_ser.CurrentAllocationAddress(OLD_POINTER_SPACE),
          p_ser.CurrentAllocationAddress(OLD_DATA_SPACE),
          p_ser.CurrentAllocationAddress(CODE_SPACE),
          p_ser.CurrentAllocationAddress(MAP_SPACE),
          p_ser.CurrentAllocationAddress(CELL_SPACE),
          p_ser.CurrentAllocationAddress(PROPERTY_CELL_SPACE),
          p_ser.CurrentAllocationAddress(LO_SPACE));

      startup_sink.WriteSpaceUsed(
          startup_serializer.CurrentAllocationAddress(NEW_SPACE),
          startup_serializer.CurrentAllocationAddress(OLD_POINTER_SPACE),
          startup_serializer.CurrentAllocationAddress(OLD_DATA_SPACE),
          startup_serializer.CurrentAllocationAddress(CODE_SPACE),
          startup_serializer.CurrentAllocationAddress(MAP_SPACE),
          startup_serializer.CurrentAllocationAddress(CELL_SPACE),
          startup_serializer.CurrentAllocationAddress(PROPERTY_CELL_SPACE),
          startup_serializer.CurrentAllocationAddress(LO_SPACE));
      startup_name.Dispose();
    }
    v8_isolate->Dispose();
  }
}


UNINITIALIZED_DEPENDENT_TEST(ContextDeserialization, ContextSerialization) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
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
      Object* root;
      {
        SnapshotByteSource source(snapshot, snapshot_size);
        Deserializer deserializer(&source);
        ReserveSpaceForSnapshot(&deserializer, file_name);
        deserializer.DeserializePartial(isolate, &root);
        CHECK(root->IsContext());
      }
      HandleScope handle_scope(isolate);
      Handle<Object> root_handle(root, isolate);


      Object* root2;
      {
        SnapshotByteSource source(snapshot, snapshot_size);
        Deserializer deserializer(&source);
        ReserveSpaceForSnapshot(&deserializer, file_name);
        deserializer.DeserializePartial(isolate, &root2);
        CHECK(root2->IsContext());
        CHECK(*root_handle != root2);
      }
    }
    v8_isolate->Dispose();
  }
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

  Handle<SharedFunctionInfo> orig = Compiler::CompileScript(
      orig_source, Handle<String>(), 0, 0, false,
      Handle<Context>(isolate->native_context()), NULL, &cache,
      v8::ScriptCompiler::kProduceCodeCache, NOT_NATIVES_CODE);

  int builtins_count = CountBuiltins();

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = Compiler::CompileScript(
        copy_source, Handle<String>(), 0, 0, false,
        Handle<Context>(isolate->native_context()), NULL, &cache,
        v8::ScriptCompiler::kConsumeCodeCache, NOT_NATIVES_CODE);
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

  Handle<SharedFunctionInfo> orig = Compiler::CompileScript(
      orig_source, Handle<String>(), 0, 0, false,
      Handle<Context>(isolate->native_context()), NULL, &cache,
      v8::ScriptCompiler::kProduceCodeCache, NOT_NATIVES_CODE);
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
    copy = Compiler::CompileScript(
        copy_source, Handle<String>(), 0, 0, false,
        Handle<Context>(isolate->native_context()), NULL, &cache,
        v8::ScriptCompiler::kConsumeCodeCache, NOT_NATIVES_CODE);
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

  Handle<SharedFunctionInfo> orig = Compiler::CompileScript(
      source_str, Handle<String>(), 0, 0, false,
      Handle<Context>(isolate->native_context()), NULL, &cache,
      v8::ScriptCompiler::kProduceCodeCache, NOT_NATIVES_CODE);

  CHECK(isolate->heap()->InSpace(orig->code(), LO_SPACE));

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = Compiler::CompileScript(
        source_str, Handle<String>(), 0, 0, false,
        Handle<Context>(isolate->native_context()), NULL, &cache,
        v8::ScriptCompiler::kConsumeCodeCache, NOT_NATIVES_CODE);
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


TEST(SerializeToplevelLargeString) {
  FLAG_serialize_toplevel = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  isolate->compilation_cache()->Disable();  // Disable same-isolate code cache.

  v8::HandleScope scope(CcTest::isolate());

  Vector<const uint8_t> source = ConstructSource(
      STATIC_CHAR_VECTOR("var s = \""), STATIC_CHAR_VECTOR("abcdef"),
      STATIC_CHAR_VECTOR("\"; s"), 1000000);
  Handle<String> source_str =
      isolate->factory()->NewStringFromOneByte(source).ToHandleChecked();

  Handle<JSObject> global(isolate->context()->global_object());
  ScriptData* cache = NULL;

  Handle<SharedFunctionInfo> orig = Compiler::CompileScript(
      source_str, Handle<String>(), 0, 0, false,
      Handle<Context>(isolate->native_context()), NULL, &cache,
      v8::ScriptCompiler::kProduceCodeCache, NOT_NATIVES_CODE);

  Handle<SharedFunctionInfo> copy;
  {
    DisallowCompilation no_compile_expected(isolate);
    copy = Compiler::CompileScript(
        source_str, Handle<String>(), 0, 0, false,
        Handle<Context>(isolate->native_context()), NULL, &cache,
        v8::ScriptCompiler::kConsumeCodeCache, NOT_NATIVES_CODE);
  }
  CHECK_NE(*orig, *copy);

  Handle<JSFunction> copy_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          copy, isolate->native_context());

  Handle<Object> copy_result =
      Execution::Call(isolate, copy_fun, global, 0, NULL).ToHandleChecked();

  CHECK_EQ(6 * 1000000, Handle<String>::cast(copy_result)->length());
  CHECK(isolate->heap()->InSpace(HeapObject::cast(*copy_result), LO_SPACE));

  delete cache;
  source.Dispose();
}


TEST(SerializeToplevelIsolates) {
  FLAG_serialize_toplevel = true;

  const char* source = "function f() { return 'abc'; }; f() + 'def'";
  v8::ScriptCompiler::CachedData* cache;

  v8::Isolate* isolate1 = v8::Isolate::New();
  {
    v8::Isolate::Scope iscope(isolate1);
    v8::HandleScope scope(isolate1);
    v8::Local<v8::Context> context = v8::Context::New(isolate1);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source_str = v8_str(source);
    v8::ScriptOrigin origin(v8_str("test"));
    v8::ScriptCompiler::Source source(source_str, origin);
    v8::Local<v8::UnboundScript> script = v8::ScriptCompiler::CompileUnbound(
        isolate1, &source, v8::ScriptCompiler::kProduceCodeCache);
    const v8::ScriptCompiler::CachedData* data = source.GetCachedData();
    // Persist cached data.
    uint8_t* buffer = NewArray<uint8_t>(data->length);
    MemCopy(buffer, data->data, data->length);
    cache = new v8::ScriptCompiler::CachedData(
        buffer, data->length, v8::ScriptCompiler::CachedData::BufferOwned);

    v8::Local<v8::Value> result = script->BindToCurrentContext()->Run();
    CHECK(result->ToString()->Equals(v8_str("abcdef")));
  }
  isolate1->Dispose();

  v8::Isolate* isolate2 = v8::Isolate::New();
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
      script = v8::ScriptCompiler::CompileUnbound(
          isolate2, &source, v8::ScriptCompiler::kConsumeCodeCache);
    }
    v8::Local<v8::Value> result = script->BindToCurrentContext()->Run();
    CHECK(result->ToString()->Equals(v8_str("abcdef")));
  }
  isolate2->Dispose();
}
