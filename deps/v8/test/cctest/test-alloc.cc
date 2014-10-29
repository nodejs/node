// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/accessors.h"
#include "src/api.h"


using namespace v8::internal;


static AllocationResult AllocateAfterFailures() {
  static int attempts = 0;

  if (++attempts < 3) return AllocationResult::Retry();
  TestHeap* heap = CcTest::test_heap();

  // New space.
  SimulateFullSpace(heap->new_space());
  heap->AllocateByteArray(100).ToObjectChecked();
  heap->AllocateFixedArray(100, NOT_TENURED).ToObjectChecked();

  // Make sure we can allocate through optimized allocation functions
  // for specific kinds.
  heap->AllocateFixedArray(100).ToObjectChecked();
  heap->AllocateHeapNumber(0.42).ToObjectChecked();
  Object* object = heap->AllocateJSObject(
      *CcTest::i_isolate()->object_function()).ToObjectChecked();
  heap->CopyJSObject(JSObject::cast(object)).ToObjectChecked();

  // Old data space.
  SimulateFullSpace(heap->old_data_space());
  heap->AllocateByteArray(100, TENURED).ToObjectChecked();

  // Old pointer space.
  SimulateFullSpace(heap->old_pointer_space());
  heap->AllocateFixedArray(10000, TENURED).ToObjectChecked();

  // Large object space.
  static const int kLargeObjectSpaceFillerLength = 300000;
  static const int kLargeObjectSpaceFillerSize = FixedArray::SizeFor(
      kLargeObjectSpaceFillerLength);
  DCHECK(kLargeObjectSpaceFillerSize > heap->old_pointer_space()->AreaSize());
  while (heap->OldGenerationSpaceAvailable() > kLargeObjectSpaceFillerSize) {
    heap->AllocateFixedArray(
        kLargeObjectSpaceFillerLength, TENURED).ToObjectChecked();
  }
  heap->AllocateFixedArray(
      kLargeObjectSpaceFillerLength, TENURED).ToObjectChecked();

  // Map space.
  SimulateFullSpace(heap->map_space());
  int instance_size = JSObject::kHeaderSize;
  heap->AllocateMap(JS_OBJECT_TYPE, instance_size).ToObjectChecked();

  // Test that we can allocate in old pointer space and code space.
  SimulateFullSpace(heap->code_space());
  heap->AllocateFixedArray(100, TENURED).ToObjectChecked();
  heap->CopyCode(CcTest::i_isolate()->builtins()->builtin(
      Builtins::kIllegal)).ToObjectChecked();

  // Return success.
  return Smi::FromInt(42);
}


static Handle<Object> Test() {
  CALL_HEAP_FUNCTION(CcTest::i_isolate(), AllocateAfterFailures(), Object);
}


TEST(StressHandles) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Handle<v8::Context> env = v8::Context::New(CcTest::isolate());
  env->Enter();
  Handle<Object> o = Test();
  CHECK(o->IsSmi() && Smi::cast(*o)->value() == 42);
  env->Exit();
}


void TestGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  info.GetReturnValue().Set(v8::Utils::ToLocal(Test()));
}


void TestSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> TestAccessorInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name = isolate->factory()->NewStringFromStaticChars("get");
  return Accessors::MakeAccessor(isolate, name, &TestGetter, &TestSetter,
                                 attributes);
}


TEST(StressJS) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  v8::Handle<v8::Context> env = v8::Context::New(CcTest::isolate());
  env->Enter();
  Handle<JSFunction> function = factory->NewFunction(
      factory->function_string());
  // Force the creation of an initial map and set the code to
  // something empty.
  factory->NewJSObject(function);
  function->ReplaceCode(CcTest::i_isolate()->builtins()->builtin(
      Builtins::kEmptyFunction));
  // Patch the map to have an accessor for "get".
  Handle<Map> map(function->initial_map());
  Handle<DescriptorArray> instance_descriptors(map->instance_descriptors());
  DCHECK(instance_descriptors->IsEmpty());

  PropertyAttributes attrs = static_cast<PropertyAttributes>(0);
  Handle<AccessorInfo> foreign = TestAccessorInfo(isolate, attrs);
  Map::EnsureDescriptorSlack(map, 1);

  CallbacksDescriptor d(Handle<Name>(Name::cast(foreign->name())),
                        foreign, attrs);
  map->AppendDescriptor(&d);

  // Add the Foo constructor the global object.
  env->Global()->Set(v8::String::NewFromUtf8(CcTest::isolate(), "Foo"),
                     v8::Utils::ToLocal(function));
  // Call the accessor through JavaScript.
  v8::Handle<v8::Value> result = v8::Script::Compile(
      v8::String::NewFromUtf8(CcTest::isolate(), "(new Foo).get"))->Run();
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
  Block(Address base_arg, int size_arg)
      : base(base_arg), size(size_arg) {}

  Address base;
  int size;
};


TEST(CodeRange) {
  const size_t code_range_size = 32*MB;
  CcTest::InitializeVM();
  CodeRange code_range(reinterpret_cast<Isolate*>(CcTest::isolate()));
  code_range.SetUp(code_range_size);
  size_t current_allocated = 0;
  size_t total_allocated = 0;
  List< ::Block> blocks(1000);

  while (total_allocated < 5 * code_range_size) {
    if (current_allocated < code_range_size / 10) {
      // Allocate a block.
      // Geometrically distributed sizes, greater than
      // Page::kMaxRegularHeapObjectSize (which is greater than code page area).
      // TODO(gc): instead of using 3 use some contant based on code_range_size
      // kMaxHeapObjectSize.
      size_t requested =
          (Page::kMaxRegularHeapObjectSize << (Pseudorandom() % 3)) +
          Pseudorandom() % 5000 + 1;
      size_t allocated = 0;
      Address base = code_range.AllocateRawMemory(requested,
                                                  requested,
                                                  &allocated);
      CHECK(base != NULL);
      blocks.Add(::Block(base, static_cast<int>(allocated)));
      current_allocated += static_cast<int>(allocated);
      total_allocated += static_cast<int>(allocated);
    } else {
      // Free a block.
      int index = Pseudorandom() % blocks.length();
      code_range.FreeRawMemory(blocks[index].base, blocks[index].size);
      current_allocated -= blocks[index].size;
      if (index < blocks.length() - 1) {
        blocks[index] = blocks.RemoveLast();
      } else {
        blocks.RemoveLast();
      }
    }
  }

  code_range.TearDown();
}
