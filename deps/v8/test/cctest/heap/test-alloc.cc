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
#include "src/objects-inl.h"
#include "src/property.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

Handle<Object> HeapTester::TestAllocateAfterFailures() {
  // Similar to what the factory's retrying logic does in the last-resort case,
  // we wrap the allocator function in an AlwaysAllocateScope.  Test that
  // all allocations succeed immediately without any retry.
  CcTest::CollectAllAvailableGarbage();
  AlwaysAllocateScope scope(CcTest::i_isolate());
  Heap* heap = CcTest::heap();
  int size = FixedArray::SizeFor(100);
  // New space.
  HeapObject* obj = heap->AllocateRaw(size, NEW_SPACE).ToObjectChecked();
  // In order to pass heap verification on Isolate teardown, mark the
  // allocated area as a filler.
  heap->CreateFillerObjectAt(obj->address(), size, ClearRecordedSlots::kNo);

  // Old space.
  heap::SimulateFullSpace(heap->old_space());
  obj = heap->AllocateRaw(size, OLD_SPACE).ToObjectChecked();
  heap->CreateFillerObjectAt(obj->address(), size, ClearRecordedSlots::kNo);

  // Large object space.
  static const size_t kLargeObjectSpaceFillerLength =
      3 * (Page::kPageSize / 10);
  static const size_t kLargeObjectSpaceFillerSize =
      FixedArray::SizeFor(kLargeObjectSpaceFillerLength);
  CHECK_GT(kLargeObjectSpaceFillerSize,
           static_cast<size_t>(heap->old_space()->AreaSize()));
  while (heap->OldGenerationSpaceAvailable() > kLargeObjectSpaceFillerSize) {
    obj = heap->AllocateRaw(kLargeObjectSpaceFillerSize, OLD_SPACE)
              .ToObjectChecked();
    heap->CreateFillerObjectAt(obj->address(), size, ClearRecordedSlots::kNo);
  }
  obj = heap->AllocateRaw(kLargeObjectSpaceFillerSize, OLD_SPACE)
            .ToObjectChecked();
  heap->CreateFillerObjectAt(obj->address(), size, ClearRecordedSlots::kNo);

  // Map space.
  heap::SimulateFullSpace(heap->map_space());
  obj = heap->AllocateRaw(Map::kSize, MAP_SPACE).ToObjectChecked();
  heap->CreateFillerObjectAt(obj->address(), Map::kSize,
                             ClearRecordedSlots::kNo);

  // Code space.
  heap::SimulateFullSpace(heap->code_space());
  size = CcTest::i_isolate()->builtins()->builtin(Builtins::kIllegal)->Size();
  obj = heap->AllocateRaw(size, CODE_SPACE).ToObjectChecked();
  heap->CreateFillerObjectAt(obj->address(), size, ClearRecordedSlots::kNo);
  return CcTest::i_isolate()->factory()->true_value();
}


HEAP_TEST(StressHandles) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = v8::Context::New(CcTest::isolate());
  env->Enter();
  Handle<Object> o = TestAllocateAfterFailures();
  CHECK(o->IsTrue(CcTest::i_isolate()));
  env->Exit();
}


void TestGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  info.GetReturnValue().Set(
      v8::Utils::ToLocal(HeapTester::TestAllocateAfterFailures()));
}

void TestSetter(v8::Local<v8::Name> name, v8::Local<v8::Value> value,
                const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> TestAccessorInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name = isolate->factory()->NewStringFromStaticChars("get");
  return Accessors::MakeAccessor(isolate, name, &TestGetter, &TestSetter);
}


TEST(StressJS) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> env = v8::Context::New(CcTest::isolate());
  env->Enter();

  NewFunctionArgs args = NewFunctionArgs::ForBuiltin(
      factory->function_string(), isolate->sloppy_function_map(),
      Builtins::kEmptyFunction);
  Handle<JSFunction> function = factory->NewFunction(args);
  CHECK(!function->shared()->construct_as_builtin());

  // Force the creation of an initial map.
  factory->NewJSObject(function);

  // Patch the map to have an accessor for "get".
  Handle<Map> map(function->initial_map());
  Handle<DescriptorArray> instance_descriptors(map->instance_descriptors());
  CHECK_EQ(0, instance_descriptors->number_of_descriptors());

  PropertyAttributes attrs = NONE;
  Handle<AccessorInfo> foreign = TestAccessorInfo(isolate, attrs);
  Map::EnsureDescriptorSlack(map, 1);

  Descriptor d = Descriptor::AccessorConstant(
      Handle<Name>(Name::cast(foreign->name())), foreign, attrs);
  map->AppendDescriptor(&d);

  // Add the Foo constructor the global object.
  CHECK(env->Global()
            ->Set(env, v8::String::NewFromUtf8(CcTest::isolate(), "Foo",
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked(),
                  v8::Utils::CallableToLocal(function))
            .FromJust());
  // Call the accessor through JavaScript.
  v8::Local<v8::Value> result =
      v8::Script::Compile(
          env, v8::String::NewFromUtf8(CcTest::isolate(), "(new Foo).get",
                                       v8::NewStringType::kNormal)
                   .ToLocalChecked())
          .ToLocalChecked()
          ->Run(env)
          .ToLocalChecked();
  CHECK_EQ(true, result->BooleanValue(env).FromJust());
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

namespace {

// Plain old data class.  Represents a block of allocated memory.
class Block {
 public:
  Block(Address base_arg, int size_arg)
      : base(base_arg), size(size_arg) {}

  Address base;
  int size;
};

}  // namespace

TEST(CodeRange) {
  const size_t code_range_size = 32*MB;
  CcTest::InitializeVM();
  CodeRange code_range(reinterpret_cast<Isolate*>(CcTest::isolate()));
  code_range.SetUp(code_range_size);
  size_t current_allocated = 0;
  size_t total_allocated = 0;
  std::vector<Block> blocks;
  blocks.reserve(1000);

  while (total_allocated < 5 * code_range_size) {
    if (current_allocated < code_range_size / 10) {
      // Allocate a block.
      // Geometrically distributed sizes, greater than
      // kMaxRegularHeapObjectSize (which is greater than code page area).
      // TODO(gc): instead of using 3 use some contant based on code_range_size
      // kMaxRegularHeapObjectSize.
      size_t requested = (kMaxRegularHeapObjectSize << (Pseudorandom() % 3)) +
                         Pseudorandom() % 5000 + 1;
      requested = RoundUp(requested, MemoryAllocator::GetCommitPageSize());
      size_t allocated = 0;

      // The request size has to be at least 2 code guard pages larger than the
      // actual commit size.
      Address base = code_range.AllocateRawMemory(
          requested, requested - (2 * MemoryAllocator::CodePageGuardSize()),
          &allocated);
      CHECK_NOT_NULL(base);
      blocks.emplace_back(base, static_cast<int>(allocated));
      current_allocated += static_cast<int>(allocated);
      total_allocated += static_cast<int>(allocated);
    } else {
      // Free a block.
      size_t index = Pseudorandom() % blocks.size();
      code_range.FreeRawMemory(blocks[index].base, blocks[index].size);
      current_allocated -= blocks[index].size;
      if (index < blocks.size() - 1) {
        blocks[index] = blocks.back();
      }
      blocks.pop_back();
    }
  }
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
