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
  return ExternalReference(id).address();
}


template <class T>
static uint32_t Encode(const ExternalReferenceEncoder& encoder, T id) {
  return encoder.Encode(AddressOf(id));
}


static int make_code(TypeCode type, int id) {
  return static_cast<uint32_t>(type) << kReferenceTypeShift | id;
}


TEST(ExternalReferenceEncoder) {
  StatsTable::SetCounterFunction(counter_function);
  Heap::Setup(false);
  ExternalReferenceEncoder encoder;
  CHECK_EQ(make_code(BUILTIN, Builtins::ArrayCode),
           Encode(encoder, Builtins::ArrayCode));
  CHECK_EQ(make_code(v8::internal::RUNTIME_FUNCTION, Runtime::kAbort),
           Encode(encoder, Runtime::kAbort));
  CHECK_EQ(make_code(IC_UTILITY, IC::kLoadCallbackProperty),
           Encode(encoder, IC_Utility(IC::kLoadCallbackProperty)));
  ExternalReference keyed_load_function_prototype =
      ExternalReference(&Counters::keyed_load_function_prototype);
  CHECK_EQ(make_code(STATS_COUNTER, Counters::k_keyed_load_function_prototype),
           encoder.Encode(keyed_load_function_prototype.address()));
  ExternalReference the_hole_value_location =
      ExternalReference::the_hole_value_location();
  CHECK_EQ(make_code(UNCLASSIFIED, 2),
           encoder.Encode(the_hole_value_location.address()));
  ExternalReference stack_limit_address =
      ExternalReference::address_of_stack_limit();
  CHECK_EQ(make_code(UNCLASSIFIED, 4),
           encoder.Encode(stack_limit_address.address()));
  ExternalReference real_stack_limit_address =
      ExternalReference::address_of_real_stack_limit();
  CHECK_EQ(make_code(UNCLASSIFIED, 5),
           encoder.Encode(real_stack_limit_address.address()));
#ifdef ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(make_code(UNCLASSIFIED, 15),
           encoder.Encode(ExternalReference::debug_break().address()));
#endif  // ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(make_code(UNCLASSIFIED, 10),
           encoder.Encode(ExternalReference::new_space_start().address()));
  CHECK_EQ(make_code(UNCLASSIFIED, 3),
           encoder.Encode(ExternalReference::roots_address().address()));
}


TEST(ExternalReferenceDecoder) {
  StatsTable::SetCounterFunction(counter_function);
  Heap::Setup(false);
  ExternalReferenceDecoder decoder;
  CHECK_EQ(AddressOf(Builtins::ArrayCode),
           decoder.Decode(make_code(BUILTIN, Builtins::ArrayCode)));
  CHECK_EQ(AddressOf(Runtime::kAbort),
           decoder.Decode(make_code(v8::internal::RUNTIME_FUNCTION,
                                    Runtime::kAbort)));
  CHECK_EQ(AddressOf(IC_Utility(IC::kLoadCallbackProperty)),
           decoder.Decode(make_code(IC_UTILITY, IC::kLoadCallbackProperty)));
  ExternalReference keyed_load_function =
      ExternalReference(&Counters::keyed_load_function_prototype);
  CHECK_EQ(keyed_load_function.address(),
           decoder.Decode(
               make_code(STATS_COUNTER,
                         Counters::k_keyed_load_function_prototype)));
  CHECK_EQ(ExternalReference::the_hole_value_location().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 2)));
  CHECK_EQ(ExternalReference::address_of_stack_limit().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 4)));
  CHECK_EQ(ExternalReference::address_of_real_stack_limit().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 5)));
#ifdef ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(ExternalReference::debug_break().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 15)));
#endif  // ENABLE_DEBUGGER_SUPPORT
  CHECK_EQ(ExternalReference::new_space_start().address(),
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
      int large_space_used);

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
      int large_space_used) {
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
  fprintf(fp, "large %d\n", large_space_used);
  fclose(fp);
}


static bool WriteToFile(const char* snapshot_file) {
  FileByteSink file(snapshot_file);
  StartupSerializer ser(&file);
  ser.Serialize();
  return true;
}


static void Serialize() {
  // We have to create one context.  One reason for this is so that the builtins
  // can be loaded from v8natives.js and their addresses can be processed.  This
  // will clear the pending fixups array, which would otherwise contain GC roots
  // that would confuse the serialization/deserialization process.
  v8::Persistent<v8::Context> env = v8::Context::New();
  env.Dispose();
  WriteToFile(FLAG_testing_serialization_file);
}


// Test that the whole heap can be serialized.
TEST(Serialize) {
  Serializer::Enable();
  v8::V8::Initialize();
  Serialize();
}


// Test that heap serialization is non-destructive.
TEST(SerializeTwice) {
  Serializer::Enable();
  v8::V8::Initialize();
  Serialize();
  Serialize();
}


//----------------------------------------------------------------------------
// Tests that the heap can be deserialized.

static void Deserialize() {
  CHECK(Snapshot::Initialize(FLAG_testing_serialization_file));
}


static void SanityCheck() {
  v8::HandleScope scope;
#ifdef DEBUG
  Heap::Verify();
#endif
  CHECK(Top::global()->IsJSObject());
  CHECK(Top::global_context()->IsContext());
  CHECK(Heap::symbol_table()->IsSymbolTable());
  CHECK(!Factory::LookupAsciiSymbol("Empty")->IsFailure());
}


DEPENDENT_TEST(Deserialize, Serialize) {
  // The serialize-deserialize tests only work if the VM is built without
  // serialization.  That doesn't matter.  We don't need to be able to
  // serialize a snapshot in a VM that is booted from a snapshot.
  if (!Snapshot::IsEnabled()) {
    v8::HandleScope scope;

    Deserialize();

    v8::Persistent<v8::Context> env = v8::Context::New();
    env->Enter();

    SanityCheck();
  }
}


DEPENDENT_TEST(DeserializeFromSecondSerialization, SerializeTwice) {
  if (!Snapshot::IsEnabled()) {
    v8::HandleScope scope;

    Deserialize();

    v8::Persistent<v8::Context> env = v8::Context::New();
    env->Enter();

    SanityCheck();
  }
}


DEPENDENT_TEST(DeserializeAndRunScript2, Serialize) {
  if (!Snapshot::IsEnabled()) {
    v8::HandleScope scope;

    Deserialize();

    v8::Persistent<v8::Context> env = v8::Context::New();
    env->Enter();

    const char* c_source = "\"1234\".length";
    v8::Local<v8::String> source = v8::String::New(c_source);
    v8::Local<v8::Script> script = v8::Script::Compile(source);
    CHECK_EQ(4, script->Run()->Int32Value());
  }
}


DEPENDENT_TEST(DeserializeFromSecondSerializationAndRunScript2,
               SerializeTwice) {
  if (!Snapshot::IsEnabled()) {
    v8::HandleScope scope;

    Deserialize();

    v8::Persistent<v8::Context> env = v8::Context::New();
    env->Enter();

    const char* c_source = "\"1234\".length";
    v8::Local<v8::String> source = v8::String::New(c_source);
    v8::Local<v8::Script> script = v8::Script::Compile(source);
    CHECK_EQ(4, script->Run()->Int32Value());
  }
}


TEST(PartialSerialization) {
  Serializer::Enable();
  v8::V8::Initialize();

  v8::Persistent<v8::Context> env = v8::Context::New();
  ASSERT(!env.IsEmpty());
  env->Enter();
  // Make sure all builtin scripts are cached.
  { HandleScope scope;
    for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
      Bootstrapper::NativesSourceLookup(i);
    }
  }
  Heap::CollectAllGarbage(true);
  Heap::CollectAllGarbage(true);

  Object* raw_foo;
  {
    v8::HandleScope handle_scope;
    v8::Local<v8::String> foo = v8::String::New("foo");
    ASSERT(!foo.IsEmpty());
    raw_foo = *(v8::Utils::OpenHandle(*foo));
  }

  int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
  Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
  OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

  env->Exit();
  env.Dispose();

  FileByteSink startup_sink(startup_name.start());
  startup_name.Dispose();
  StartupSerializer startup_serializer(&startup_sink);
  startup_serializer.SerializeStrongReferences();

  FileByteSink partial_sink(FLAG_testing_serialization_file);
  PartialSerializer p_ser(&startup_serializer, &partial_sink);
  p_ser.Serialize(&raw_foo);
  startup_serializer.SerializeWeakReferences();
  partial_sink.WriteSpaceUsed(p_ser.CurrentAllocationAddress(NEW_SPACE),
                              p_ser.CurrentAllocationAddress(OLD_POINTER_SPACE),
                              p_ser.CurrentAllocationAddress(OLD_DATA_SPACE),
                              p_ser.CurrentAllocationAddress(CODE_SPACE),
                              p_ser.CurrentAllocationAddress(MAP_SPACE),
                              p_ser.CurrentAllocationAddress(CELL_SPACE),
                              p_ser.CurrentAllocationAddress(LO_SPACE));
}


static void ReserveSpaceForPartialSnapshot(const char* file_name) {
  int file_name_length = StrLength(file_name) + 10;
  Vector<char> name = Vector<char>::New(file_name_length + 1);
  OS::SNPrintF(name, "%s.size", file_name);
  FILE* fp = OS::FOpen(name.start(), "r");
  name.Dispose();
  int new_size, pointer_size, data_size, code_size, map_size, cell_size;
  int large_size;
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
  CHECK_EQ(1, fscanf(fp, "large %d\n", &large_size));
#ifdef _MSC_VER
#undef fscanf
#endif
  fclose(fp);
  Heap::ReserveSpace(new_size,
                     pointer_size,
                     data_size,
                     code_size,
                     map_size,
                     cell_size,
                     large_size);
}


DEPENDENT_TEST(PartialDeserialization, PartialSerialization) {
  if (!Snapshot::IsEnabled()) {
    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

    CHECK(Snapshot::Initialize(startup_name.start()));
    startup_name.Dispose();

    const char* file_name = FLAG_testing_serialization_file;
    ReserveSpaceForPartialSnapshot(file_name);

    int snapshot_size = 0;
    byte* snapshot = ReadBytes(file_name, &snapshot_size);

    Object* root;
    {
      SnapshotByteSource source(snapshot, snapshot_size);
      Deserializer deserializer(&source);
      deserializer.DeserializePartial(&root);
      CHECK(root->IsString());
    }
    v8::HandleScope handle_scope;
    Handle<Object>root_handle(root);

    Object* root2;
    {
      SnapshotByteSource source(snapshot, snapshot_size);
      Deserializer deserializer(&source);
      deserializer.DeserializePartial(&root2);
      CHECK(root2->IsString());
      CHECK(*root_handle == root2);
    }
  }
}


TEST(ContextSerialization) {
  Serializer::Enable();
  v8::V8::Initialize();

  v8::Persistent<v8::Context> env = v8::Context::New();
  ASSERT(!env.IsEmpty());
  env->Enter();
  // Make sure all builtin scripts are cached.
  { HandleScope scope;
    for (int i = 0; i < Natives::GetBuiltinsCount(); i++) {
      Bootstrapper::NativesSourceLookup(i);
    }
  }
  // If we don't do this then we end up with a stray root pointing at the
  // context even after we have disposed of env.
  Heap::CollectAllGarbage(true);

  int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
  Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
  OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

  env->Exit();

  Object* raw_context = *(v8::Utils::OpenHandle(*env));

  env.Dispose();

  FileByteSink startup_sink(startup_name.start());
  startup_name.Dispose();
  StartupSerializer startup_serializer(&startup_sink);
  startup_serializer.SerializeStrongReferences();

  FileByteSink partial_sink(FLAG_testing_serialization_file);
  PartialSerializer p_ser(&startup_serializer, &partial_sink);
  p_ser.Serialize(&raw_context);
  startup_serializer.SerializeWeakReferences();
  partial_sink.WriteSpaceUsed(p_ser.CurrentAllocationAddress(NEW_SPACE),
                              p_ser.CurrentAllocationAddress(OLD_POINTER_SPACE),
                              p_ser.CurrentAllocationAddress(OLD_DATA_SPACE),
                              p_ser.CurrentAllocationAddress(CODE_SPACE),
                              p_ser.CurrentAllocationAddress(MAP_SPACE),
                              p_ser.CurrentAllocationAddress(CELL_SPACE),
                              p_ser.CurrentAllocationAddress(LO_SPACE));
}


DEPENDENT_TEST(ContextDeserialization, ContextSerialization) {
  if (!Snapshot::IsEnabled()) {
    int file_name_length = StrLength(FLAG_testing_serialization_file) + 10;
    Vector<char> startup_name = Vector<char>::New(file_name_length + 1);
    OS::SNPrintF(startup_name, "%s.startup", FLAG_testing_serialization_file);

    CHECK(Snapshot::Initialize(startup_name.start()));
    startup_name.Dispose();

    const char* file_name = FLAG_testing_serialization_file;
    ReserveSpaceForPartialSnapshot(file_name);

    int snapshot_size = 0;
    byte* snapshot = ReadBytes(file_name, &snapshot_size);

    Object* root;
    {
      SnapshotByteSource source(snapshot, snapshot_size);
      Deserializer deserializer(&source);
      deserializer.DeserializePartial(&root);
      CHECK(root->IsContext());
    }
    v8::HandleScope handle_scope;
    Handle<Object>root_handle(root);

    Object* root2;
    {
      SnapshotByteSource source(snapshot, snapshot_size);
      Deserializer deserializer(&source);
      deserializer.DeserializePartial(&root2);
      CHECK(root2->IsContext());
      CHECK(*root_handle != root2);
    }
  }
}


TEST(LinearAllocation) {
  v8::V8::Initialize();
  int new_space_max = 512 * KB;

  for (int size = 1000; size < 5 * MB; size += size >> 1) {
    int new_space_size = (size < new_space_max) ? size : new_space_max;
    Heap::ReserveSpace(
        new_space_size,
        size,              // Old pointer space.
        size,              // Old data space.
        size,              // Code space.
        size,              // Map space.
        size,              // Cell space.
        size);             // Large object space.
    LinearAllocationScope linear_allocation_scope;
    const int kSmallFixedArrayLength = 4;
    const int kSmallFixedArraySize =
        FixedArray::kHeaderSize + kSmallFixedArrayLength * kPointerSize;
    const int kSmallStringLength = 16;
    const int kSmallStringSize =
        (SeqAsciiString::kHeaderSize + kSmallStringLength +
        kObjectAlignmentMask) & ~kObjectAlignmentMask;
    const int kMapSize = Map::kSize;

    Object* new_last = NULL;
    for (int i = 0;
         i + kSmallFixedArraySize <= new_space_size;
         i += kSmallFixedArraySize) {
      Object* obj =
          Heap::AllocateFixedArray(kSmallFixedArrayLength)->ToObjectChecked();
      if (new_last != NULL) {
        CHECK(reinterpret_cast<char*>(obj) ==
              reinterpret_cast<char*>(new_last) + kSmallFixedArraySize);
      }
      new_last = obj;
    }

    Object* pointer_last = NULL;
    for (int i = 0;
         i + kSmallFixedArraySize <= size;
         i += kSmallFixedArraySize) {
      Object* obj = Heap::AllocateFixedArray(kSmallFixedArrayLength,
                                             TENURED)->ToObjectChecked();
      int old_page_fullness = i % Page::kPageSize;
      int page_fullness = (i + kSmallFixedArraySize) % Page::kPageSize;
      if (page_fullness < old_page_fullness ||
          page_fullness > Page::kObjectAreaSize) {
        i = RoundUp(i, Page::kPageSize);
        pointer_last = NULL;
      }
      if (pointer_last != NULL) {
        CHECK(reinterpret_cast<char*>(obj) ==
              reinterpret_cast<char*>(pointer_last) + kSmallFixedArraySize);
      }
      pointer_last = obj;
    }

    Object* data_last = NULL;
    for (int i = 0; i + kSmallStringSize <= size; i += kSmallStringSize) {
      Object* obj = Heap::AllocateRawAsciiString(kSmallStringLength,
                                                 TENURED)->ToObjectChecked();
      int old_page_fullness = i % Page::kPageSize;
      int page_fullness = (i + kSmallStringSize) % Page::kPageSize;
      if (page_fullness < old_page_fullness ||
          page_fullness > Page::kObjectAreaSize) {
        i = RoundUp(i, Page::kPageSize);
        data_last = NULL;
      }
      if (data_last != NULL) {
        CHECK(reinterpret_cast<char*>(obj) ==
              reinterpret_cast<char*>(data_last) + kSmallStringSize);
      }
      data_last = obj;
    }

    Object* map_last = NULL;
    for (int i = 0; i + kMapSize <= size; i += kMapSize) {
      Object* obj = Heap::AllocateMap(JS_OBJECT_TYPE,
                                      42 * kPointerSize)->ToObjectChecked();
      int old_page_fullness = i % Page::kPageSize;
      int page_fullness = (i + kMapSize) % Page::kPageSize;
      if (page_fullness < old_page_fullness ||
          page_fullness > Page::kObjectAreaSize) {
        i = RoundUp(i, Page::kPageSize);
        map_last = NULL;
      }
      if (map_last != NULL) {
        CHECK(reinterpret_cast<char*>(obj) ==
              reinterpret_cast<char*>(map_last) + kMapSize);
      }
      map_last = obj;
    }

    if (size > Page::kObjectAreaSize) {
      // Support for reserving space in large object space is not there yet,
      // but using an always-allocate scope is fine for now.
      AlwaysAllocateScope always;
      int large_object_array_length =
          (size - FixedArray::kHeaderSize) / kPointerSize;
      Object* obj = Heap::AllocateFixedArray(large_object_array_length,
                                             TENURED)->ToObjectChecked();
      CHECK(!obj->IsFailure());
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
