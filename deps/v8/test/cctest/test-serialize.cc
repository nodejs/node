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

#include "sys/stat.h"

#include "v8.h"

#include "debug.h"
#include "ic-inl.h"
#include "runtime.h"
#include "serialize.h"
#include "scopeinfo.h"
#include "snapshot.h"
#include "cctest.h"
#include "spaces.h"
#include "objects.h"
#include "natives.h"
#include "bootstrapper.h"

using namespace v8::internal;

static const unsigned kCounters = 256;
static int local_counters[kCounters];
static const char* local_counter_names[kCounters];


static unsigned CounterHash(const char* s) {
  unsigned hash = 0;
  while (*++s) {
    hash |= hash << 5;
    hash += *s;
  }
  return hash;
}


// Callback receiver to track counters in test.
static int* counter_function(const char* name) {
  unsigned hash = CounterHash(name) % kCounters;
  unsigned original_hash = hash;
  USE(original_hash);
  while (true) {
    if (local_counter_names[hash] == name) {
      return &local_counters[hash];
    }
    if (local_counter_names[hash] == 0) {
      local_counter_names[hash] = name;
      return &local_counters[hash];
    }
    if (strcmp(local_counter_names[hash], name) == 0) {
      return &local_counters[hash];
    }
    hash = (hash + 1) % kCounters;
    ASSERT(hash != original_hash);  // Hash table has been filled up.
  }
}


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
  isolate->stats_table()->SetCounterFunction(counter_function);
  v8::V8::Initialize();

  ExternalReferenceEncoder encoder(isolate);
  CHECK_EQ(make_code(BUILTIN, Builtins::kArrayCode),
           Encode(encoder, Builtins::kArrayCode));
  CHECK_EQ(make_code(v8::internal::RUNTIME_FUNCTION, Runtime::kAbort),
           Encode(encoder, Runtime::kAbort));
  ExternalReference total_compile_size =
      ExternalReference(isolate->counters()->total_compile_size());
  CHECK_EQ(make_code(STATS_COUNTER, Counters::k_total_compile_size),
           encoder.Encode(total_compile_size.address()));
  ExternalReference stack_limit_address =
      ExternalReference::address_of_stack_limit(isolate);
  CHECK_EQ(make_code(UNCLASSIFIED, 4),
           encoder.Encode(stack_limit_address.address()));
  ExternalReference real_stack_limit_address =
      ExternalReference::address_of_real_stack_limit(isolate);
  CHECK_EQ(make_code(UNCLASSIFIED, 5),
           encoder.Encode(real_stack_limit_address.address()));
#ifdef ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(make_code(UNCLASSIFIED, 16),
           encoder.Encode(ExternalReference::debug_break(isolate).address()));
#endif  // ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(make_code(UNCLASSIFIED, 10),
           encoder.Encode(
               ExternalReference::new_space_start(isolate).address()));
  CHECK_EQ(make_code(UNCLASSIFIED, 3),
           encoder.Encode(
               ExternalReference::roots_array_start(isolate).address()));
  CHECK_EQ(make_code(UNCLASSIFIED, 52),
           encoder.Encode(ExternalReference::cpu_features().address()));
}


TEST(ExternalReferenceDecoder) {
  Isolate* isolate = CcTest::i_isolate();
  isolate->stats_table()->SetCounterFunction(counter_function);
  v8::V8::Initialize();

  ExternalReferenceDecoder decoder(isolate);
  CHECK_EQ(AddressOf(Builtins::kArrayCode),
           decoder.Decode(make_code(BUILTIN, Builtins::kArrayCode)));
  CHECK_EQ(AddressOf(Runtime::kAbort),
           decoder.Decode(make_code(v8::internal::RUNTIME_FUNCTION,
                                    Runtime::kAbort)));
  ExternalReference total_compile_size =
      ExternalReference(isolate->counters()->total_compile_size());
  CHECK_EQ(total_compile_size.address(),
           decoder.Decode(
               make_code(STATS_COUNTER,
                         Counters::k_total_compile_size)));
  CHECK_EQ(ExternalReference::address_of_stack_limit(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 4)));
  CHECK_EQ(ExternalReference::address_of_real_stack_limit(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 5)));
#ifdef ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(ExternalReference::debug_break(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 16)));
#endif  // ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(ExternalReference::new_space_start(isolate).address(),
           decoder.Decode(make_code(UNCLASSIFIED, 10)));
}


class FileByteSink : public SnapshotByteSink {
 public:
  explicit FileByteSink(const char* snapshot_file) {
    fp_ = OS::FOpen(snapshot_file, "wb");
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
  virtual void Put(int byte, const char* description) {
    if (fp_ != NULL) {
      fputc(byte, fp_);
    }
  }
  virtual int Position() {
    return ftell(fp_);
  }
  void WriteSpaceUsed(
      int new_space_used,
      int pointer_space_used,
      int data_space_used,
      int code_space_used,
      int map_space_used,
      int cell_space_used,
      int property_cell_space_used);

 private:
  FILE* fp_;
  const char* file_name_;
};


void FileByteSink::WriteSpaceUsed(
      int new_space_used,
      int pointer_space_used,
      int data_space_used,
      int code_space_used,
      int map_space_used,
      int cell_space_used,
      int property_cell_space_used) {
  int file_name_length = StrLength(file_name_) + 10;
  Vector<char> name = Vector<char>::New(file_name_length + 1);
  OS::SNPrintF(name, "%s.size", file_name_);
  FILE* fp = OS::FOpen(name.start(), "w");
  name.Dispose();
  fprintf(fp, "new %d\n", new_space_used);
  fprintf(fp, "pointer %d\n", pointer_space_used);
  fprintf(fp, "data %d\n", data_space_used);
  fprintf(fp, "code %d\n", code_space_used);
  fprintf(fp, "map %d\n", map_space_used);
  fprintf(fp, "cell %d\n", cell_space_used);
  fprintf(fp, "property cell %d\n", property_cell_space_used);
  fclose(fp);
}


static bool WriteToFile(Isolate* isolate, const char* snapshot_file) {
  FileByteSink file(snapshot_file);
  StartupSerializer ser(isolate, &file);
  ser.Serialize();

  file.WriteSpaceUsed(
      ser.CurrentAllocationAddress(NEW_SPACE),
      ser.CurrentAllocationAddress(OLD_POINTER_SPACE),
      ser.CurrentAllocationAddress(OLD_DATA_SPACE),
      ser.CurrentAllocationAddress(CODE_SPACE),
      ser.CurrentAllocationAddress(MAP_SPACE),
      ser.CurrentAllocationAddress(CELL_SPACE),
      ser.CurrentAllocationAddress(PROPERTY_CELL_SPACE));

  return true;
}


static void Serialize() {
  // We have to create one context.  One reason for this is so that the builtins
  // can be loaded from v8natives.js and their addresses can be processed.  This
  // will clear the pending fixups array, which would otherwise contain GC roots
  // that would confuse the serialization/deserialization process.
  v8::Isolate* isolate = CcTest::isolate();
  {
    v8::HandleScope scope(isolate);
    v8::Context::New(isolate);
  }

  Isolate* internal_isolate = CcTest::i_isolate();
  internal_isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags, "serialize");
  WriteToFile(internal_isolate, FLAG_testing_serialization_file);
}


// Test that the whole heap can be serialized.
TEST(Serialize) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    Serializer::Enable(CcTest::i_isolate());
    v8::V8::Initialize();
    Serialize();
  }
}


// Test that heap serialization is non-destructive.
TEST(SerializeTwice) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    Serializer::Enable(CcTest::i_isolate());
    v8::V8::Initialize();
    Serialize();
    Serialize();
  }
}


//----------------------------------------------------------------------------
// Tests that the heap can be deserialized.

static void Deserialize() {
  CHECK(Snapshot::Initialize(FLAG_testing_serialization_file));
}


static void SanityCheck() {
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
#ifdef VERIFY_HEAP
  CcTest::heap()->Verify();
#endif
  CHECK(isolate->global_object()->IsJSObject());
  CHECK(isolate->native_context()->IsContext());
  CHECK(CcTest::heap()->string_table()->IsStringTable());
  CHECK(!isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("Empty"))->IsFailure());
}


DEPENDENT_TEST(Deserialize, Serialize) {
  // The serialize-deserialize tests only work if the VM is built without
  // serialization.  That doesn't matter.  We don't need to be able to
  // serialize a snapshot in a VM that is booted from a snapshot.
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate* isolate = CcTest::isolate();
    v8::HandleScope scope(isolate);
    Deserialize();

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck();
  }
}


DEPENDENT_TEST(DeserializeFromSecondSerialization, SerializeTwice) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate* isolate = CcTest::isolate();
    v8::HandleScope scope(isolate);
    Deserialize();

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    SanityCheck();
  }
}


DEPENDENT_TEST(DeserializeAndRunScript2, Serialize) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate* isolate = CcTest::isolate();
    v8::HandleScope scope(isolate);
    Deserialize();

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    const char* c_source = "\"1234\".length";
    v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, c_source);
    v8::Local<v8::Script> script = v8::Script::Compile(source);
    CHECK_EQ(4, script->Run()->Int32Value());
  }
}


DEPENDENT_TEST(DeserializeFromSecondSerializationAndRunScript2,
               SerializeTwice) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    v8::Isolate* isolate = CcTest::isolate();
    v8::HandleScope scope(isolate);
    Deserialize();

    v8::Local<v8::Context> env = v8::Context::New(isolate);
    env->Enter();

    const char* c_source = "\"1234\".length";
    v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, c_source);
    v8::Local<v8::Script> script = v8::Script::Compile(source);
    CHECK_EQ(4, script->Run()->Int32Value());
  }
}


TEST(PartialSerialization) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    Isolate* isolate = CcTest::i_isolate();
    Serializer::Enable(isolate);
    v8::V8::Initialize();
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    Heap* heap = isolate->heap();

    v8::Persistent<v8::Context> env;
    {
      HandleScope scope(isolate);
      env.Reset(v8_isolate, v8::Context::New(v8_isolate));
    }
    ASSERT(!env.IsEmpty());
    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Enter();
    }
    // Make sure all builtin scripts are cached.
    { HandleScope scope(isolate);
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
      ASSERT(!foo.IsEmpty());
      raw_foo = *(v8::Utils::OpenHandle(*foo));
    }

    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

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
        p_ser.CurrentAllocationAddress(PROPERTY_CELL_SPACE));

    startup_sink.WriteSpaceUsed(
        startup_serializer.CurrentAllocationAddress(NEW_SPACE),
        startup_serializer.CurrentAllocationAddress(OLD_POINTER_SPACE),
        startup_serializer.CurrentAllocationAddress(OLD_DATA_SPACE),
        startup_serializer.CurrentAllocationAddress(CODE_SPACE),
        startup_serializer.CurrentAllocationAddress(MAP_SPACE),
        startup_serializer.CurrentAllocationAddress(CELL_SPACE),
        startup_serializer.CurrentAllocationAddress(PROPERTY_CELL_SPACE));
    startup_name.Dispose();
  }
}


static void ReserveSpaceForSnapshot(Deserializer* deserializer,
                                    const char* file_name) {
  int file_name_length = StrLength(file_name) + 10;
  Vector<char> name = Vector<char>::New(file_name_length + 1);
  OS::SNPrintF(name, "%s.size", file_name);
  FILE* fp = OS::FOpen(name.start(), "r");
  name.Dispose();
  int new_size, pointer_size, data_size, code_size, map_size, cell_size,
      property_cell_size;
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
}


DEPENDENT_TEST(PartialDeserialization, PartialSerialization) {
  if (!Snapshot::IsEnabled()) {
    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

    CHECK(Snapshot::Initialize(startup_name.start()));
    startup_name.Dispose();

    const char* file_name = FLAG_testing_serialization_file;

    int snapshot_size = 0;
    byte* snapshot = ReadBytes(file_name, &snapshot_size);

    Isolate* isolate = CcTest::i_isolate();
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
}


TEST(ContextSerialization) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    Isolate* isolate = CcTest::i_isolate();
    Serializer::Enable(isolate);
    v8::V8::Initialize();
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    Heap* heap = isolate->heap();

    v8::Persistent<v8::Context> env;
    {
      HandleScope scope(isolate);
      env.Reset(v8_isolate, v8::Context::New(v8_isolate));
    }
    ASSERT(!env.IsEmpty());
    {
      v8::HandleScope handle_scope(v8_isolate);
      v8::Local<v8::Context>::New(v8_isolate, env)->Enter();
    }
    // Make sure all builtin scripts are cached.
    { HandleScope scope(isolate);
      for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
        isolate->bootstrapper()->NativesSourceLookup(i);
      }
    }
    // If we don't do this then we end up with a stray root pointing at the
    // context even after we have disposed of env.
    heap->CollectAllGarbage(Heap::kNoGCFlags);

    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

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
        p_ser.CurrentAllocationAddress(PROPERTY_CELL_SPACE));

    startup_sink.WriteSpaceUsed(
        startup_serializer.CurrentAllocationAddress(NEW_SPACE),
        startup_serializer.CurrentAllocationAddress(OLD_POINTER_SPACE),
        startup_serializer.CurrentAllocationAddress(OLD_DATA_SPACE),
        startup_serializer.CurrentAllocationAddress(CODE_SPACE),
        startup_serializer.CurrentAllocationAddress(MAP_SPACE),
        startup_serializer.CurrentAllocationAddress(CELL_SPACE),
        startup_serializer.CurrentAllocationAddress(PROPERTY_CELL_SPACE));
    startup_name.Dispose();
  }
}


DEPENDENT_TEST(ContextDeserialization, ContextSerialization) {
  if (!Snapshot::HaveASnapshotToStartFrom()) {
    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

    CHECK(Snapshot::Initialize(startup_name.start()));
    startup_name.Dispose();

    const char* file_name = FLAG_testing_serialization_file;

    int snapshot_size = 0;
    byte* snapshot = ReadBytes(file_name, &snapshot_size);

    Isolate* isolate = CcTest::i_isolate();
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
