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

#include "v8.h"
#include "accessors.h"
#include "top.h"

#include "cctest.h"


using namespace v8::internal;


static MaybeObject* AllocateAfterFailures() {
  static int attempts = 0;
  if (++attempts < 3) return Failure::RetryAfterGC();

  // New space.
  NewSpace* new_space = Heap::new_space();
  static const int kNewSpaceFillerSize = ByteArray::SizeFor(0);
  while (new_space->Available() > kNewSpaceFillerSize) {
    int available_before = static_cast<int>(new_space->Available());
    CHECK(!Heap::AllocateByteArray(0)->IsFailure());
    if (available_before == new_space->Available()) {
      // It seems that we are avoiding new space allocations when
      // allocation is forced, so no need to fill up new space
      // in order to make the test harder.
      break;
    }
  }
  CHECK(!Heap::AllocateByteArray(100)->IsFailure());
  CHECK(!Heap::AllocateFixedArray(100, NOT_TENURED)->IsFailure());

  // Make sure we can allocate through optimized allocation functions
  // for specific kinds.
  CHECK(!Heap::AllocateFixedArray(100)->IsFailure());
  CHECK(!Heap::AllocateHeapNumber(0.42)->IsFailure());
  CHECK(!Heap::AllocateArgumentsObject(Smi::FromInt(87), 10)->IsFailure());
  Object* object =
      Heap::AllocateJSObject(*Top::object_function())->ToObjectChecked();
  CHECK(!Heap::CopyJSObject(JSObject::cast(object))->IsFailure());

  // Old data space.
  OldSpace* old_data_space = Heap::old_data_space();
  static const int kOldDataSpaceFillerSize = ByteArray::SizeFor(0);
  while (old_data_space->Available() > kOldDataSpaceFillerSize) {
    CHECK(!Heap::AllocateByteArray(0, TENURED)->IsFailure());
  }
  CHECK(!Heap::AllocateRawAsciiString(100, TENURED)->IsFailure());

  // Large object space.
  while (!Heap::OldGenerationAllocationLimitReached()) {
    CHECK(!Heap::AllocateFixedArray(10000, TENURED)->IsFailure());
  }
  CHECK(!Heap::AllocateFixedArray(10000, TENURED)->IsFailure());

  // Map space.
  MapSpace* map_space = Heap::map_space();
  static const int kMapSpaceFillerSize = Map::kSize;
  InstanceType instance_type = JS_OBJECT_TYPE;
  int instance_size = JSObject::kHeaderSize;
  while (map_space->Available() > kMapSpaceFillerSize) {
    CHECK(!Heap::AllocateMap(instance_type, instance_size)->IsFailure());
  }
  CHECK(!Heap::AllocateMap(instance_type, instance_size)->IsFailure());

  // Test that we can allocate in old pointer space and code space.
  CHECK(!Heap::AllocateFixedArray(100, TENURED)->IsFailure());
  CHECK(!Heap::CopyCode(Builtins::builtin(Builtins::Illegal))->IsFailure());

  // Return success.
  return Smi::FromInt(42);
}


static Handle<Object> Test() {
  CALL_HEAP_FUNCTION(AllocateAfterFailures(), Object);
}


TEST(StressHandles) {
  v8::Persistent<v8::Context> env = v8::Context::New();
  v8::HandleScope scope;
  env->Enter();
  Handle<Object> o = Test();
  CHECK(o->IsSmi() && Smi::cast(*o)->value() == 42);
  env->Exit();
}


static MaybeObject* TestAccessorGet(Object* object, void*) {
  return AllocateAfterFailures();
}


const AccessorDescriptor kDescriptor = {
  TestAccessorGet,
  0,
  0
};


TEST(StressJS) {
  v8::Persistent<v8::Context> env = v8::Context::New();
  v8::HandleScope scope;
  env->Enter();
  Handle<JSFunction> function =
      Factory::NewFunction(Factory::function_symbol(), Factory::null_value());
  // Force the creation of an initial map and set the code to
  // something empty.
  Factory::NewJSObject(function);
  function->set_code(Builtins::builtin(Builtins::EmptyFunction));
  // Patch the map to have an accessor for "get".
  Handle<Map> map(function->initial_map());
  Handle<DescriptorArray> instance_descriptors(map->instance_descriptors());
  Handle<Proxy> proxy = Factory::NewProxy(&kDescriptor);
  instance_descriptors = Factory::CopyAppendProxyDescriptor(
      instance_descriptors,
      Factory::NewStringFromAscii(Vector<const char>("get", 3)),
      proxy,
      static_cast<PropertyAttributes>(0));
  map->set_instance_descriptors(*instance_descriptors);
  // Add the Foo constructor the global object.
  env->Global()->Set(v8::String::New("Foo"), v8::Utils::ToLocal(function));
  // Call the accessor through JavaScript.
  v8::Handle<v8::Value> result =
      v8::Script::Compile(v8::String::New("(new Foo).get"))->Run();
  CHECK_EQ(42, result->Int32Value());
  env->Exit();
}


// CodeRange test.
// Tests memory management in a CodeRange by allocating and freeing blocks,
// using a pseudorandom generator to choose block sizes geometrically
// distributed between 2 * Page::kPageSize and 2^5 + 1 * Page::kPageSize.
// Ensure that the freed chunks are collected and reused by allocating (in
// total) more than the size of the CodeRange.

// This pseudorandom generator does not need to be particularly good.
// Use the lower half of the V8::Random() generator.
unsigned int Pseudorandom() {
  static uint32_t lo = 2345;
  lo = 18273 * (lo & 0xFFFF) + (lo >> 16);  // Provably not 0.
  return lo & 0xFFFF;
}


// Plain old data class.  Represents a block of allocated memory.
class Block {
 public:
  Block(void* base_arg, int size_arg)
      : base(base_arg), size(size_arg) {}

  void *base;
  int size;
};


TEST(CodeRange) {
  const int code_range_size = 16*MB;
  CodeRange::Setup(code_range_size);
  int current_allocated = 0;
  int total_allocated = 0;
  List<Block> blocks(1000);

  while (total_allocated < 5 * code_range_size) {
    if (current_allocated < code_range_size / 10) {
      // Allocate a block.
      // Geometrically distributed sizes, greater than Page::kPageSize.
      size_t requested = (Page::kPageSize << (Pseudorandom() % 6)) +
           Pseudorandom() % 5000 + 1;
      size_t allocated = 0;
      void* base = CodeRange::AllocateRawMemory(requested, &allocated);
      blocks.Add(Block(base, static_cast<int>(allocated)));
      current_allocated += static_cast<int>(allocated);
      total_allocated += static_cast<int>(allocated);
    } else {
      // Free a block.
      int index = Pseudorandom() % blocks.length();
      CodeRange::FreeRawMemory(blocks[index].base, blocks[index].size);
      current_allocated -= blocks[index].size;
      if (index < blocks.length() - 1) {
        blocks[index] = blocks.RemoveLast();
      } else {
        blocks.RemoveLast();
      }
    }
  }

  CodeRange::TearDown();
}
