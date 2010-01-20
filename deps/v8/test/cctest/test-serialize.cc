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


static int register_code(int reg) {
  return Debug::k_register_address << kDebugIdShift | reg;
}


TEST(ExternalReferenceEncoder) {
  StatsTable::SetCounterFunction(counter_function);
  Heap::Setup(false);
  ExternalReferenceEncoder encoder;
  CHECK_EQ(make_code(BUILTIN, Builtins::ArrayCode),
           Encode(encoder, Builtins::ArrayCode));
  CHECK_EQ(make_code(RUNTIME_FUNCTION, Runtime::kAbort),
           Encode(encoder, Runtime::kAbort));
  CHECK_EQ(make_code(IC_UTILITY, IC::kLoadCallbackProperty),
           Encode(encoder, IC_Utility(IC::kLoadCallbackProperty)));
  CHECK_EQ(make_code(DEBUG_ADDRESS, register_code(3)),
           Encode(encoder, Debug_Address(Debug::k_register_address, 3)));
  ExternalReference keyed_load_function_prototype =
      ExternalReference(&Counters::keyed_load_function_prototype);
  CHECK_EQ(make_code(STATS_COUNTER, Counters::k_keyed_load_function_prototype),
           encoder.Encode(keyed_load_function_prototype.address()));
  ExternalReference passed_function =
      ExternalReference::builtin_passed_function();
  CHECK_EQ(make_code(UNCLASSIFIED, 1),
           encoder.Encode(passed_function.address()));
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
  CHECK_EQ(make_code(UNCLASSIFIED, 11),
           encoder.Encode(ExternalReference::debug_break().address()));
  CHECK_EQ(make_code(UNCLASSIFIED, 7),
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
           decoder.Decode(make_code(RUNTIME_FUNCTION, Runtime::kAbort)));
  CHECK_EQ(AddressOf(IC_Utility(IC::kLoadCallbackProperty)),
           decoder.Decode(make_code(IC_UTILITY, IC::kLoadCallbackProperty)));
  CHECK_EQ(AddressOf(Debug_Address(Debug::k_register_address, 3)),
           decoder.Decode(make_code(DEBUG_ADDRESS, register_code(3))));
  ExternalReference keyed_load_function =
      ExternalReference(&Counters::keyed_load_function_prototype);
  CHECK_EQ(keyed_load_function.address(),
           decoder.Decode(
               make_code(STATS_COUNTER,
                         Counters::k_keyed_load_function_prototype)));
  CHECK_EQ(ExternalReference::builtin_passed_function().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 1)));
  CHECK_EQ(ExternalReference::the_hole_value_location().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 2)));
  CHECK_EQ(ExternalReference::address_of_stack_limit().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 4)));
  CHECK_EQ(ExternalReference::address_of_real_stack_limit().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 5)));
  CHECK_EQ(ExternalReference::debug_break().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 11)));
  CHECK_EQ(ExternalReference::new_space_start().address(),
           decoder.Decode(make_code(UNCLASSIFIED, 7)));
}


static void Serialize() {
  // We have to create one context.  One reason for this is so that the builtins
  // can be loaded from v8natives.js and their addresses can be processed.  This
  // will clear the pending fixups array, which would otherwise contain GC roots
  // that would confuse the serialization/deserialization process.
  v8::Persistent<v8::Context> env = v8::Context::New();
  env.Dispose();
  Snapshot::WriteToFile(FLAG_testing_serialization_file);
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
  CHECK(Top::special_function_table()->IsFixedArray());
  CHECK(Heap::symbol_table()->IsSymbolTable());
  CHECK(!Factory::LookupAsciiSymbol("Empty")->IsFailure());
}


DEPENDENT_TEST(Deserialize, Serialize) {
  v8::HandleScope scope;

  Deserialize();

  v8::Persistent<v8::Context> env = v8::Context::New();
  env->Enter();

  SanityCheck();
}


DEPENDENT_TEST(DeserializeFromSecondSerialization, SerializeTwice) {
  v8::HandleScope scope;

  Deserialize();

  v8::Persistent<v8::Context> env = v8::Context::New();
  env->Enter();

  SanityCheck();
}


DEPENDENT_TEST(DeserializeAndRunScript2, Serialize) {
  v8::HandleScope scope;

  Deserialize();

  v8::Persistent<v8::Context> env = v8::Context::New();
  env->Enter();

  const char* c_source = "\"1234\".length";
  v8::Local<v8::String> source = v8::String::New(c_source);
  v8::Local<v8::Script> script = v8::Script::Compile(source);
  CHECK_EQ(4, script->Run()->Int32Value());
}


DEPENDENT_TEST(DeserializeFromSecondSerializationAndRunScript2,
               SerializeTwice) {
  v8::HandleScope scope;

  Deserialize();

  v8::Persistent<v8::Context> env = v8::Context::New();
  env->Enter();

  const char* c_source = "\"1234\".length";
  v8::Local<v8::String> source = v8::String::New(c_source);
  v8::Local<v8::Script> script = v8::Script::Compile(source);
  CHECK_EQ(4, script->Run()->Int32Value());
}


class FileByteSink : public SnapshotByteSink {
 public:
  explicit FileByteSink(const char* snapshot_file) {
    fp_ = OS::FOpen(snapshot_file, "wb");
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

 private:
  FILE* fp_;
};


TEST(PartialSerialization) {
  Serializer::Enable();
  v8::V8::Initialize();
  v8::Persistent<v8::Context> env = v8::Context::New();
  env->Enter();

  v8::HandleScope handle_scope;
  v8::Local<v8::String> foo = v8::String::New("foo");

  FileByteSink file(FLAG_testing_serialization_file);
  Serializer ser(&file);
  i::Handle<i::String> internal_foo = v8::Utils::OpenHandle(*foo);
  Object* raw_foo = *internal_foo;
  ser.SerializePartial(&raw_foo);
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
        SeqAsciiString::kHeaderSize + kSmallStringLength;
    const int kMapSize = Map::kSize;

    Object* new_last = NULL;
    for (int i = 0;
         i + kSmallFixedArraySize <= new_space_size;
         i += kSmallFixedArraySize) {
      Object* obj = Heap::AllocateFixedArray(kSmallFixedArrayLength);
      if (new_last != NULL) {
        CHECK_EQ(reinterpret_cast<char*>(obj),
                 reinterpret_cast<char*>(new_last) + kSmallFixedArraySize);
      }
      new_last = obj;
    }

    Object* pointer_last = NULL;
    for (int i = 0;
         i + kSmallFixedArraySize <= size;
         i += kSmallFixedArraySize) {
      Object* obj = Heap::AllocateFixedArray(kSmallFixedArrayLength, TENURED);
      int old_page_fullness = i % Page::kPageSize;
      int page_fullness = (i + kSmallFixedArraySize) % Page::kPageSize;
      if (page_fullness < old_page_fullness ||
          page_fullness > Page::kObjectAreaSize) {
        i = RoundUp(i, Page::kPageSize);
        pointer_last = NULL;
      }
      if (pointer_last != NULL) {
        CHECK_EQ(reinterpret_cast<char*>(obj),
                 reinterpret_cast<char*>(pointer_last) + kSmallFixedArraySize);
      }
      pointer_last = obj;
    }

    Object* data_last = NULL;
    for (int i = 0; i + kSmallStringSize <= size; i += kSmallStringSize) {
      Object* obj = Heap::AllocateRawAsciiString(kSmallStringLength, TENURED);
      int old_page_fullness = i % Page::kPageSize;
      int page_fullness = (i + kSmallStringSize) % Page::kPageSize;
      if (page_fullness < old_page_fullness ||
          page_fullness > Page::kObjectAreaSize) {
        i = RoundUp(i, Page::kPageSize);
        data_last = NULL;
      }
      if (data_last != NULL) {
        CHECK_EQ(reinterpret_cast<char*>(obj),
                 reinterpret_cast<char*>(data_last) + kSmallStringSize);
      }
      data_last = obj;
    }

    Object* map_last = NULL;
    for (int i = 0; i + kMapSize <= size; i += kMapSize) {
      Object* obj = Heap::AllocateMap(JS_OBJECT_TYPE, 42 * kPointerSize);
      int old_page_fullness = i % Page::kPageSize;
      int page_fullness = (i + kMapSize) % Page::kPageSize;
      if (page_fullness < old_page_fullness ||
          page_fullness > Page::kObjectAreaSize) {
        i = RoundUp(i, Page::kPageSize);
        map_last = NULL;
      }
      if (map_last != NULL) {
        CHECK_EQ(reinterpret_cast<char*>(obj),
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
                                             TENURED);
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
