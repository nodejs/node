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

#include <stdlib.h>
#include <utility>

#include "src/api/api-inl.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/execution.h"
#include "src/handles/global-handles.h"
#include "src/heap/combined-heap.h"
#include "src/heap/factory.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-reducer.h"
#include "src/heap/remembered-set.h"
#include "src/ic/ic.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/elements.h"
#include "src/objects/field-type.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/objects/transitions.h"
#include "src/regexp/regexp.h"
#include "src/snapshot/snapshot.h"
#include "src/utils/ostreams.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"
#include "test/cctest/test-feedback-vector.h"
#include "test/cctest/test-transitions.h"

namespace v8 {
namespace internal {
namespace heap {

// We only start allocation-site tracking with the second instantiation.
static const int kPretenureCreationCount =
    AllocationSite::kPretenureMinimumCreated + 1;

static void CheckMap(Map map, int type, int instance_size) {
  CHECK(map.IsHeapObject());
  DCHECK(IsValidHeapObject(CcTest::heap(), map));
  CHECK_EQ(ReadOnlyRoots(CcTest::heap()).meta_map(), map.map());
  CHECK_EQ(type, map.instance_type());
  CHECK_EQ(instance_size, map.instance_size());
}


TEST(HeapMaps) {
  CcTest::InitializeVM();
  ReadOnlyRoots roots(CcTest::heap());
  CheckMap(roots.meta_map(), MAP_TYPE, Map::kSize);
  CheckMap(roots.heap_number_map(), HEAP_NUMBER_TYPE, HeapNumber::kSize);
  CheckMap(roots.fixed_array_map(), FIXED_ARRAY_TYPE, kVariableSizeSentinel);
  CheckMap(roots.hash_table_map(), HASH_TABLE_TYPE, kVariableSizeSentinel);
  CheckMap(roots.string_map(), STRING_TYPE, kVariableSizeSentinel);
}

static void VerifyStoredPrototypeMap(Isolate* isolate,
                                     int stored_map_context_index,
                                     int stored_ctor_context_index) {
  Handle<Context> context = isolate->native_context();

  Handle<Map> this_map(Map::cast(context->get(stored_map_context_index)),
                       isolate);

  Handle<JSFunction> fun(
      JSFunction::cast(context->get(stored_ctor_context_index)), isolate);
  Handle<JSObject> proto(JSObject::cast(fun->initial_map().prototype()),
                         isolate);
  Handle<Map> that_map(proto->map(), isolate);

  CHECK(proto->HasFastProperties());
  CHECK_EQ(*this_map, *that_map);
}

// Checks that critical maps stored on the context (mostly used for fast-path
// checks) are unchanged after initialization.
TEST(ContextMaps) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  VerifyStoredPrototypeMap(isolate,
                           Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX,
                           Context::STRING_FUNCTION_INDEX);
  VerifyStoredPrototypeMap(isolate, Context::REGEXP_PROTOTYPE_MAP_INDEX,
                           Context::REGEXP_FUNCTION_INDEX);
}

TEST(InitialObjects) {
  LocalContext env;
  HandleScope scope(CcTest::i_isolate());
  Handle<Context> context = v8::Utils::OpenHandle(*env);
  // Initial ArrayIterator prototype.
  CHECK_EQ(
      context->initial_array_iterator_prototype(),
      *v8::Utils::OpenHandle(*CompileRun("[][Symbol.iterator]().__proto__")));
  // Initial Array prototype.
  CHECK_EQ(context->initial_array_prototype(),
           *v8::Utils::OpenHandle(*CompileRun("Array.prototype")));
  // Initial Generator prototype.
  CHECK_EQ(context->initial_generator_prototype(),
           *v8::Utils::OpenHandle(
               *CompileRun("(function*(){}).__proto__.prototype")));
  // Initial Iterator prototype.
  CHECK_EQ(context->initial_iterator_prototype(),
           *v8::Utils::OpenHandle(
               *CompileRun("[][Symbol.iterator]().__proto__.__proto__")));
  // Initial Object prototype.
  CHECK_EQ(context->initial_object_prototype(),
           *v8::Utils::OpenHandle(*CompileRun("Object.prototype")));
}

static void CheckOddball(Isolate* isolate, Object obj, const char* string) {
  CHECK(obj.IsOddball());
  Handle<Object> handle(obj, isolate);
  Object print_string = *Object::ToString(isolate, handle).ToHandleChecked();
  CHECK(String::cast(print_string).IsOneByteEqualTo(CStrVector(string)));
}

static void CheckSmi(Isolate* isolate, int value, const char* string) {
  Handle<Object> handle(Smi::FromInt(value), isolate);
  Object print_string = *Object::ToString(isolate, handle).ToHandleChecked();
  CHECK(String::cast(print_string).IsOneByteEqualTo(CStrVector(string)));
}


static void CheckNumber(Isolate* isolate, double value, const char* string) {
  Handle<Object> number = isolate->factory()->NewNumber(value);
  CHECK(number->IsNumber());
  Handle<Object> print_string =
      Object::ToString(isolate, number).ToHandleChecked();
  CHECK(String::cast(*print_string).IsOneByteEqualTo(CStrVector(string)));
}

void CheckEmbeddedObjectsAreEqual(Handle<Code> lhs, Handle<Code> rhs) {
  int mode_mask = RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT);
  RelocIterator lhs_it(*lhs, mode_mask);
  RelocIterator rhs_it(*rhs, mode_mask);
  while (!lhs_it.done() && !rhs_it.done()) {
    CHECK(lhs_it.rinfo()->target_object() == rhs_it.rinfo()->target_object());

    lhs_it.next();
    rhs_it.next();
  }
  CHECK(lhs_it.done() == rhs_it.done());
}

HEAP_TEST(TestNewSpaceRefsInCopiedCode) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope sc(isolate);

  Handle<HeapNumber> value = factory->NewHeapNumber(1.000123);
  CHECK(Heap::InYoungGeneration(*value));

  i::byte buffer[i::Assembler::kMinimalBufferSize];
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  // Add a new-space reference to the code.
  masm.Push(value);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::STUB).Build();

  Handle<Code> copy;
  {
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
    copy = factory->CopyCode(code);
  }

  CheckEmbeddedObjectsAreEqual(code, copy);
  CcTest::CollectAllAvailableGarbage();
  CheckEmbeddedObjectsAreEqual(code, copy);
}

static void CheckFindCodeObject(Isolate* isolate) {
  // Test FindCodeObject
#define __ assm.

  Assembler assm(AssemblerOptions{});

  __ nop();  // supported on all architectures

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::STUB).Build();
  CHECK(code->IsCode());

  HeapObject obj = HeapObject::cast(*code);
  Address obj_addr = obj.address();

  for (int i = 0; i < obj.Size(); i += kTaggedSize) {
    Object found = isolate->FindCodeObject(obj_addr + i);
    CHECK_EQ(*code, found);
  }

  Handle<Code> copy = Factory::CodeBuilder(isolate, desc, Code::STUB).Build();
  HeapObject obj_copy = HeapObject::cast(*copy);
  Object not_right =
      isolate->FindCodeObject(obj_copy.address() + obj_copy.Size() / 2);
  CHECK(not_right != *code);
}


TEST(HandleNull) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  LocalContext context;
  Handle<Object> n(Object(0), isolate);
  CHECK(!n.is_null());
}


TEST(HeapObjects) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope sc(isolate);
  Handle<Object> value = factory->NewNumber(1.000123);
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(1.000123, value->Number());

  value = factory->NewNumber(1.0);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(1.0, value->Number());

  value = factory->NewNumberFromInt(1024);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(1024.0, value->Number());

  value = factory->NewNumberFromInt(Smi::kMinValue);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(Smi::kMinValue, Handle<Smi>::cast(value)->value());

  value = factory->NewNumberFromInt(Smi::kMaxValue);
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(Smi::kMaxValue, Handle<Smi>::cast(value)->value());

#if !defined(V8_TARGET_ARCH_64_BIT)
  // TODO(lrn): We need a NumberFromIntptr function in order to test this.
  value = factory->NewNumberFromInt(Smi::kMinValue - 1);
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(Smi::kMinValue - 1), value->Number());
#endif

  value = factory->NewNumberFromUint(static_cast<uint32_t>(Smi::kMaxValue) + 1);
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(static_cast<uint32_t>(Smi::kMaxValue) + 1),
           value->Number());

  value = factory->NewNumberFromUint(static_cast<uint32_t>(1) << 31);
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(static_cast<uint32_t>(1) << 31),
           value->Number());

  // nan oddball checks
  CHECK(factory->nan_value()->IsNumber());
  CHECK(std::isnan(factory->nan_value()->Number()));

  Handle<String> s = factory->NewStringFromStaticChars("fisk hest ");
  CHECK(s->IsString());
  CHECK_EQ(10, s->length());

  Handle<String> object_string = Handle<String>::cast(factory->Object_string());
  Handle<JSGlobalObject> global(CcTest::i_isolate()->context().global_object(),
                                isolate);
  CHECK(Just(true) == JSReceiver::HasOwnProperty(global, object_string));

  // Check ToString for oddballs
  ReadOnlyRoots roots(heap);
  CheckOddball(isolate, roots.true_value(), "true");
  CheckOddball(isolate, roots.false_value(), "false");
  CheckOddball(isolate, roots.null_value(), "null");
  CheckOddball(isolate, roots.undefined_value(), "undefined");

  // Check ToString for Smis
  CheckSmi(isolate, 0, "0");
  CheckSmi(isolate, 42, "42");
  CheckSmi(isolate, -42, "-42");

  // Check ToString for Numbers
  CheckNumber(isolate, 1.1, "1.1");

  CheckFindCodeObject(isolate);
}

TEST(Tagging) {
  CcTest::InitializeVM();
  int request = 24;
  CHECK_EQ(request, static_cast<int>(OBJECT_POINTER_ALIGN(request)));
  CHECK(Smi::FromInt(42).IsSmi());
  CHECK(Smi::FromInt(Smi::kMinValue).IsSmi());
  CHECK(Smi::FromInt(Smi::kMaxValue).IsSmi());
}


TEST(GarbageCollection) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);
  // Check GC.
  CcTest::CollectGarbage(NEW_SPACE);

  Handle<JSGlobalObject> global(CcTest::i_isolate()->context().global_object(),
                                isolate);
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<String> prop_namex = factory->InternalizeUtf8String("theSlotx");
  Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  Handle<Smi> twenty_four(Smi::FromInt(24), isolate);

  {
    HandleScope inner_scope(isolate);
    // Allocate a function and keep it in global object's property.
    Handle<JSFunction> function = factory->NewFunctionForTest(name);
    Object::SetProperty(isolate, global, name, function).Check();
    // Allocate an object.  Unrooted after leaving the scope.
    Handle<JSObject> obj = factory->NewJSObject(function);
    Object::SetProperty(isolate, obj, prop_name, twenty_three).Check();
    Object::SetProperty(isolate, obj, prop_namex, twenty_four).Check();

    CHECK_EQ(Smi::FromInt(23),
             *Object::GetProperty(isolate, obj, prop_name).ToHandleChecked());
    CHECK_EQ(Smi::FromInt(24),
             *Object::GetProperty(isolate, obj, prop_namex).ToHandleChecked());
  }

  CcTest::CollectGarbage(NEW_SPACE);

  // Function should be alive.
  CHECK(Just(true) == JSReceiver::HasOwnProperty(global, name));
  // Check function is retained.
  Handle<Object> func_value =
      Object::GetProperty(isolate, global, name).ToHandleChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);

  {
    HandleScope inner_scope(isolate);
    // Allocate another object, make it reachable from global.
    Handle<JSObject> obj = factory->NewJSObject(function);
    Object::SetProperty(isolate, global, obj_name, obj).Check();
    Object::SetProperty(isolate, obj, prop_name, twenty_three).Check();
  }

  // After gc, it should survive.
  CcTest::CollectGarbage(NEW_SPACE);

  CHECK(Just(true) == JSReceiver::HasOwnProperty(global, obj_name));
  Handle<Object> obj =
      Object::GetProperty(isolate, global, obj_name).ToHandleChecked();
  CHECK(obj->IsJSObject());
  CHECK_EQ(Smi::FromInt(23),
           *Object::GetProperty(isolate, obj, prop_name).ToHandleChecked());
}


static void VerifyStringAllocation(Isolate* isolate, const char* string) {
  HandleScope scope(isolate);
  Handle<String> s = isolate->factory()->NewStringFromUtf8(
      CStrVector(string)).ToHandleChecked();
  CHECK_EQ(strlen(string), s->length());
  for (int index = 0; index < s->length(); index++) {
    CHECK_EQ(static_cast<uint16_t>(string[index]), s->Get(index));
  }
}


TEST(String) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());

  VerifyStringAllocation(isolate, "a");
  VerifyStringAllocation(isolate, "ab");
  VerifyStringAllocation(isolate, "abc");
  VerifyStringAllocation(isolate, "abcd");
  VerifyStringAllocation(isolate, "fiskerdrengen er paa havet");
}


TEST(LocalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope scope(CcTest::isolate());
  const char* name = "Kasper the spunky";
  Handle<String> string = factory->NewStringFromAsciiChecked(name);
  CHECK_EQ(strlen(name), string->length());
}


TEST(GlobalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  Handle<Object> h1;
  Handle<Object> h2;
  Handle<Object> h3;
  Handle<Object> h4;

  {
    HandleScope scope(isolate);

    Handle<Object> i = factory->NewStringFromStaticChars("fisk");
    Handle<Object> u = factory->NewNumber(1.12344);

    h1 = global_handles->Create(*i);
    h2 = global_handles->Create(*u);
    h3 = global_handles->Create(*i);
    h4 = global_handles->Create(*u);
  }

  // after gc, it should survive
  CcTest::CollectGarbage(NEW_SPACE);

  CHECK((*h1).IsString());
  CHECK((*h2).IsHeapNumber());
  CHECK((*h3).IsString());
  CHECK((*h4).IsHeapNumber());

  CHECK_EQ(*h3, *h1);
  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h3.location());

  CHECK_EQ(*h4, *h2);
  GlobalHandles::Destroy(h2.location());
  GlobalHandles::Destroy(h4.location());
}


static bool WeakPointerCleared = false;

static void TestWeakGlobalHandleCallback(
    const v8::WeakCallbackInfo<void>& data) {
  std::pair<v8::Persistent<v8::Value>*, int>* p =
      reinterpret_cast<std::pair<v8::Persistent<v8::Value>*, int>*>(
          data.GetParameter());
  if (p->second == 1234) WeakPointerCleared = true;
  p->first->Reset();
}

TEST(WeakGlobalUnmodifiedApiHandlesScavenge) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  LocalContext context;
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  WeakPointerCleared = false;

  Handle<Object> h1;
  Handle<Object> h2;

  {
    HandleScope scope(isolate);

    // Create an Api object that is unmodified.
    Local<v8::Function> function = FunctionTemplate::New(context->GetIsolate())
                                       ->GetFunction(context.local())
                                       .ToLocalChecked();
    Local<v8::Object> i =
        function->NewInstance(context.local()).ToLocalChecked();
    Handle<Object> u = factory->NewNumber(1.12344);

    h1 = global_handles->Create(*u);
    h2 = global_handles->Create(*(reinterpret_cast<internal::Address*>(*i)));
  }

  std::pair<Handle<Object>*, int> handle_and_id(&h2, 1234);
  GlobalHandles::MakeWeak(
      h2.location(), reinterpret_cast<void*>(&handle_and_id),
      &TestWeakGlobalHandleCallback, v8::WeakCallbackType::kParameter);

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK((*h1).IsHeapNumber());
  CHECK(WeakPointerCleared);
  GlobalHandles::Destroy(h1.location());
}

TEST(WeakGlobalHandlesMark) {
  FLAG_stress_incremental_marking = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  WeakPointerCleared = false;

  Handle<Object> h1;
  Handle<Object> h2;

  {
    HandleScope scope(isolate);

    Handle<Object> i = factory->NewStringFromStaticChars("fisk");
    Handle<Object> u = factory->NewNumber(1.12344);

    h1 = global_handles->Create(*i);
    h2 = global_handles->Create(*u);
  }

  // Make sure the objects are promoted.
  CcTest::CollectGarbage(OLD_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(!Heap::InYoungGeneration(*h1) && !Heap::InYoungGeneration(*h2));

  std::pair<Handle<Object>*, int> handle_and_id(&h2, 1234);
  GlobalHandles::MakeWeak(
      h2.location(), reinterpret_cast<void*>(&handle_and_id),
      &TestWeakGlobalHandleCallback, v8::WeakCallbackType::kParameter);

  // Incremental marking potentially marked handles before they turned weak.
  CcTest::CollectAllGarbage();
  CHECK((*h1).IsString());
  CHECK(WeakPointerCleared);
  GlobalHandles::Destroy(h1.location());
}


TEST(DeleteWeakGlobalHandle) {
  FLAG_stress_compaction = false;
  FLAG_stress_incremental_marking = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  WeakPointerCleared = false;
  Handle<Object> h;
  {
    HandleScope scope(isolate);

    Handle<Object> i = factory->NewStringFromStaticChars("fisk");
    h = global_handles->Create(*i);
  }

  std::pair<Handle<Object>*, int> handle_and_id(&h, 1234);
  GlobalHandles::MakeWeak(h.location(), reinterpret_cast<void*>(&handle_and_id),
                          &TestWeakGlobalHandleCallback,
                          v8::WeakCallbackType::kParameter);
  CHECK(!WeakPointerCleared);
  CcTest::CollectGarbage(OLD_SPACE);
  CHECK(WeakPointerCleared);
}

TEST(BytecodeArray) {
  if (FLAG_never_compact) return;
  static const uint8_t kRawBytes[] = {0xC3, 0x7E, 0xA5, 0x5A};
  static const int kRawBytesSize = sizeof(kRawBytes);
  static const int32_t kFrameSize = 32;
  static const int32_t kParameterCount = 2;

  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  heap::SimulateFullSpace(heap->old_space());
  Handle<FixedArray> constant_pool =
      factory->NewFixedArray(5, AllocationType::kOld);
  for (int i = 0; i < 5; i++) {
    Handle<Object> number = factory->NewHeapNumber(i);
    constant_pool->set(i, *number);
  }

  // Allocate and initialize BytecodeArray
  Handle<BytecodeArray> array = factory->NewBytecodeArray(
      kRawBytesSize, kRawBytes, kFrameSize, kParameterCount, constant_pool);

  CHECK(array->IsBytecodeArray());
  CHECK_EQ(array->length(), (int)sizeof(kRawBytes));
  CHECK_EQ(array->frame_size(), kFrameSize);
  CHECK_EQ(array->parameter_count(), kParameterCount);
  CHECK_EQ(array->constant_pool(), *constant_pool);
  CHECK_LE(array->address(), array->GetFirstBytecodeAddress());
  CHECK_GE(array->address() + array->BytecodeArraySize(),
           array->GetFirstBytecodeAddress() + array->length());
  for (int i = 0; i < kRawBytesSize; i++) {
    CHECK_EQ(Memory<uint8_t>(array->GetFirstBytecodeAddress() + i),
             kRawBytes[i]);
    CHECK_EQ(array->get(i), kRawBytes[i]);
  }

  FixedArray old_constant_pool_address = *constant_pool;

  // Perform a full garbage collection and force the constant pool to be on an
  // evacuation candidate.
  Page* evac_page = Page::FromHeapObject(*constant_pool);
  heap::ForceEvacuationCandidate(evac_page);
  CcTest::CollectAllGarbage();

  // BytecodeArray should survive.
  CHECK_EQ(array->length(), kRawBytesSize);
  CHECK_EQ(array->frame_size(), kFrameSize);
  for (int i = 0; i < kRawBytesSize; i++) {
    CHECK_EQ(array->get(i), kRawBytes[i]);
    CHECK_EQ(Memory<uint8_t>(array->GetFirstBytecodeAddress() + i),
             kRawBytes[i]);
  }

  // Constant pool should have been migrated.
  CHECK_EQ(array->constant_pool(), *constant_pool);
  CHECK_NE(array->constant_pool(), old_constant_pool_address);
}

TEST(BytecodeArrayAging) {
  static const uint8_t kRawBytes[] = {0xC3, 0x7E, 0xA5, 0x5A};
  static const int kRawBytesSize = sizeof(kRawBytes);
  static const int32_t kFrameSize = 32;
  static const int32_t kParameterCount = 2;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<BytecodeArray> array =
      factory->NewBytecodeArray(kRawBytesSize, kRawBytes, kFrameSize,
                                kParameterCount, factory->empty_fixed_array());

  CHECK_EQ(BytecodeArray::kFirstBytecodeAge, array->bytecode_age());
  array->MakeOlder();
  CHECK_EQ(BytecodeArray::kQuadragenarianBytecodeAge, array->bytecode_age());
  array->set_bytecode_age(BytecodeArray::kLastBytecodeAge);
  array->MakeOlder();
  CHECK_EQ(BytecodeArray::kLastBytecodeAge, array->bytecode_age());
}

static const char* not_so_random_string_table[] = {
  "abstract",
  "boolean",
  "break",
  "byte",
  "case",
  "catch",
  "char",
  "class",
  "const",
  "continue",
  "debugger",
  "default",
  "delete",
  "do",
  "double",
  "else",
  "enum",
  "export",
  "extends",
  "false",
  "final",
  "finally",
  "float",
  "for",
  "function",
  "goto",
  "if",
  "implements",
  "import",
  "in",
  "instanceof",
  "int",
  "interface",
  "long",
  "native",
  "new",
  "null",
  "package",
  "private",
  "protected",
  "public",
  "return",
  "short",
  "static",
  "super",
  "switch",
  "synchronized",
  "this",
  "throw",
  "throws",
  "transient",
  "true",
  "try",
  "typeof",
  "var",
  "void",
  "volatile",
  "while",
  "with",
  nullptr
};

static void CheckInternalizedStrings(const char** strings) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  for (const char* string = *strings; *strings != nullptr;
       string = *strings++) {
    HandleScope scope(isolate);
    Handle<String> a =
        isolate->factory()->InternalizeUtf8String(CStrVector(string));
    // InternalizeUtf8String may return a failure if a GC is needed.
    CHECK(a->IsInternalizedString());
    Handle<String> b = factory->InternalizeUtf8String(string);
    CHECK_EQ(*b, *a);
    CHECK(b->IsOneByteEqualTo(CStrVector(string)));
    b = isolate->factory()->InternalizeUtf8String(CStrVector(string));
    CHECK_EQ(*b, *a);
    CHECK(b->IsOneByteEqualTo(CStrVector(string)));
  }
}


TEST(StringTable) {
  CcTest::InitializeVM();

  v8::HandleScope sc(CcTest::isolate());
  CheckInternalizedStrings(not_so_random_string_table);
  CheckInternalizedStrings(not_so_random_string_table);
}


TEST(FunctionAllocation) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<JSFunction> function = factory->NewFunctionForTest(name);

  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  Handle<Smi> twenty_four(Smi::FromInt(24), isolate);

  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<JSObject> obj = factory->NewJSObject(function);
  Object::SetProperty(isolate, obj, prop_name, twenty_three).Check();
  CHECK_EQ(Smi::FromInt(23),
           *Object::GetProperty(isolate, obj, prop_name).ToHandleChecked());
  // Check that we can add properties to function objects.
  Object::SetProperty(isolate, function, prop_name, twenty_four).Check();
  CHECK_EQ(
      Smi::FromInt(24),
      *Object::GetProperty(isolate, function, prop_name).ToHandleChecked());
}


TEST(ObjectProperties) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> object_string(
      String::cast(ReadOnlyRoots(CcTest::heap()).Object_string()), isolate);
  Handle<Object> object =
      Object::GetProperty(isolate, CcTest::i_isolate()->global_object(),
                          object_string)
          .ToHandleChecked();
  Handle<JSFunction> constructor = Handle<JSFunction>::cast(object);
  Handle<JSObject> obj = factory->NewJSObject(constructor);
  Handle<String> first = factory->InternalizeUtf8String("first");
  Handle<String> second = factory->InternalizeUtf8String("second");

  Handle<Smi> one(Smi::FromInt(1), isolate);
  Handle<Smi> two(Smi::FromInt(2), isolate);

  // check for empty
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));

  // add first
  Object::SetProperty(isolate, obj, first, one).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));

  // delete first
  CHECK(Just(true) ==
        JSReceiver::DeleteProperty(obj, first, LanguageMode::kSloppy));
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));

  // add first and then second
  Object::SetProperty(isolate, obj, first, one).Check();
  Object::SetProperty(isolate, obj, second, two).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, second));

  // delete first and then second
  CHECK(Just(true) ==
        JSReceiver::DeleteProperty(obj, first, LanguageMode::kSloppy));
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, second));
  CHECK(Just(true) ==
        JSReceiver::DeleteProperty(obj, second, LanguageMode::kSloppy));
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, second));

  // add first and then second
  Object::SetProperty(isolate, obj, first, one).Check();
  Object::SetProperty(isolate, obj, second, two).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, second));

  // delete second and then first
  CHECK(Just(true) ==
        JSReceiver::DeleteProperty(obj, second, LanguageMode::kSloppy));
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(true) ==
        JSReceiver::DeleteProperty(obj, first, LanguageMode::kSloppy));
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, second));

  // check string and internalized string match
  const char* string1 = "fisk";
  Handle<String> s1 = factory->NewStringFromAsciiChecked(string1);
  Object::SetProperty(isolate, obj, s1, one).Check();
  Handle<String> s1_string = factory->InternalizeUtf8String(string1);
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, s1_string));

  // check internalized string and string match
  const char* string2 = "fugl";
  Handle<String> s2_string = factory->InternalizeUtf8String(string2);
  Object::SetProperty(isolate, obj, s2_string, one).Check();
  Handle<String> s2 = factory->NewStringFromAsciiChecked(string2);
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, s2));
}


TEST(JSObjectMaps) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<JSFunction> function = factory->NewFunctionForTest(name);

  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<JSObject> obj = factory->NewJSObject(function);
  Handle<Map> initial_map(function->initial_map(), isolate);

  // Set a propery
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  Object::SetProperty(isolate, obj, prop_name, twenty_three).Check();
  CHECK_EQ(Smi::FromInt(23),
           *Object::GetProperty(isolate, obj, prop_name).ToHandleChecked());

  // Check the map has changed
  CHECK(*initial_map != obj->map());
}


TEST(JSArray) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("Array");
  Handle<Object> fun_obj =
      Object::GetProperty(isolate, CcTest::i_isolate()->global_object(), name)
          .ToHandleChecked();
  Handle<JSFunction> function = Handle<JSFunction>::cast(fun_obj);

  // Allocate the object.
  Handle<Object> element;
  Handle<JSObject> object = factory->NewJSObject(function);
  Handle<JSArray> array = Handle<JSArray>::cast(object);
  // We just initialized the VM, no heap allocation failure yet.
  JSArray::Initialize(array, 0);

  // Set array length to 0.
  JSArray::SetLength(array, 0);
  CHECK_EQ(Smi::kZero, array->length());
  // Must be in fast mode.
  CHECK(array->HasSmiOrObjectElements());

  // array[length] = name.
  Object::SetElement(isolate, array, 0, name, ShouldThrow::kDontThrow).Check();
  CHECK_EQ(Smi::FromInt(1), array->length());
  element = i::Object::GetElement(isolate, array, 0).ToHandleChecked();
  CHECK_EQ(*element, *name);

  // Set array length with larger than smi value.
  JSArray::SetLength(array, static_cast<uint32_t>(Smi::kMaxValue) + 1);

  uint32_t int_length = 0;
  CHECK(array->length().ToArrayIndex(&int_length));
  CHECK_EQ(static_cast<uint32_t>(Smi::kMaxValue) + 1, int_length);
  CHECK(array->HasDictionaryElements());  // Must be in slow mode.

  // array[length] = name.
  Object::SetElement(isolate, array, int_length, name, ShouldThrow::kDontThrow)
      .Check();
  uint32_t new_int_length = 0;
  CHECK(array->length().ToArrayIndex(&new_int_length));
  CHECK_EQ(static_cast<double>(int_length), new_int_length - 1);
  element = Object::GetElement(isolate, array, int_length).ToHandleChecked();
  CHECK_EQ(*element, *name);
  element = Object::GetElement(isolate, array, 0).ToHandleChecked();
  CHECK_EQ(*element, *name);
}


TEST(JSObjectCopy) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> object_string(
      String::cast(ReadOnlyRoots(CcTest::heap()).Object_string()), isolate);
  Handle<Object> object =
      Object::GetProperty(isolate, CcTest::i_isolate()->global_object(),
                          object_string)
          .ToHandleChecked();
  Handle<JSFunction> constructor = Handle<JSFunction>::cast(object);
  Handle<JSObject> obj = factory->NewJSObject(constructor);
  Handle<String> first = factory->InternalizeUtf8String("first");
  Handle<String> second = factory->InternalizeUtf8String("second");

  Handle<Smi> one(Smi::FromInt(1), isolate);
  Handle<Smi> two(Smi::FromInt(2), isolate);

  Object::SetProperty(isolate, obj, first, one).Check();
  Object::SetProperty(isolate, obj, second, two).Check();

  Object::SetElement(isolate, obj, 0, first, ShouldThrow::kDontThrow).Check();
  Object::SetElement(isolate, obj, 1, second, ShouldThrow::kDontThrow).Check();

  // Make the clone.
  Handle<Object> value1, value2;
  Handle<JSObject> clone = factory->CopyJSObject(obj);
  CHECK(!clone.is_identical_to(obj));

  value1 = Object::GetElement(isolate, obj, 0).ToHandleChecked();
  value2 = Object::GetElement(isolate, clone, 0).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
  value1 = Object::GetElement(isolate, obj, 1).ToHandleChecked();
  value2 = Object::GetElement(isolate, clone, 1).ToHandleChecked();
  CHECK_EQ(*value1, *value2);

  value1 = Object::GetProperty(isolate, obj, first).ToHandleChecked();
  value2 = Object::GetProperty(isolate, clone, first).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
  value1 = Object::GetProperty(isolate, obj, second).ToHandleChecked();
  value2 = Object::GetProperty(isolate, clone, second).ToHandleChecked();
  CHECK_EQ(*value1, *value2);

  // Flip the values.
  Object::SetProperty(isolate, clone, first, two).Check();
  Object::SetProperty(isolate, clone, second, one).Check();

  Object::SetElement(isolate, clone, 0, second, ShouldThrow::kDontThrow)
      .Check();
  Object::SetElement(isolate, clone, 1, first, ShouldThrow::kDontThrow).Check();

  value1 = Object::GetElement(isolate, obj, 1).ToHandleChecked();
  value2 = Object::GetElement(isolate, clone, 0).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
  value1 = Object::GetElement(isolate, obj, 0).ToHandleChecked();
  value2 = Object::GetElement(isolate, clone, 1).ToHandleChecked();
  CHECK_EQ(*value1, *value2);

  value1 = Object::GetProperty(isolate, obj, second).ToHandleChecked();
  value2 = Object::GetProperty(isolate, clone, first).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
  value1 = Object::GetProperty(isolate, obj, first).ToHandleChecked();
  value2 = Object::GetProperty(isolate, clone, second).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
}


TEST(StringAllocation) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  const unsigned char chars[] = {0xE5, 0xA4, 0xA7};
  for (int length = 0; length < 100; length++) {
    v8::HandleScope scope(CcTest::isolate());
    char* non_one_byte = NewArray<char>(3 * length + 1);
    char* one_byte = NewArray<char>(length + 1);
    non_one_byte[3 * length] = 0;
    one_byte[length] = 0;
    for (int i = 0; i < length; i++) {
      one_byte[i] = 'a';
      non_one_byte[3 * i] = chars[0];
      non_one_byte[3 * i + 1] = chars[1];
      non_one_byte[3 * i + 2] = chars[2];
    }
    Handle<String> non_one_byte_sym = factory->InternalizeUtf8String(
        Vector<const char>(non_one_byte, 3 * length));
    CHECK_EQ(length, non_one_byte_sym->length());
    Handle<String> one_byte_sym =
        factory->InternalizeString(OneByteVector(one_byte, length));
    CHECK_EQ(length, one_byte_sym->length());
    Handle<String> non_one_byte_str =
        factory->NewStringFromUtf8(Vector<const char>(non_one_byte, 3 * length))
            .ToHandleChecked();
    non_one_byte_str->Hash();
    CHECK_EQ(length, non_one_byte_str->length());
    Handle<String> one_byte_str =
        factory->NewStringFromUtf8(Vector<const char>(one_byte, length))
            .ToHandleChecked();
    one_byte_str->Hash();
    CHECK_EQ(length, one_byte_str->length());
    DeleteArray(non_one_byte);
    DeleteArray(one_byte);
  }
}


static int ObjectsFoundInHeap(Heap* heap, Handle<Object> objs[], int size) {
  // Count the number of objects found in the heap.
  int found_count = 0;
  HeapObjectIterator iterator(heap);
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    for (int i = 0; i < size; i++) {
      if (*objs[i] == obj) {
        found_count++;
      }
    }
  }
  return found_count;
}


TEST(Iteration) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  // Array of objects to scan haep for.
  const int objs_count = 6;
  Handle<Object> objs[objs_count];
  int next_objs_index = 0;

  // Allocate a JS array to OLD_SPACE and NEW_SPACE
  objs[next_objs_index++] = factory->NewJSArray(10);
  objs[next_objs_index++] =
      factory->NewJSArray(10, HOLEY_ELEMENTS, AllocationType::kOld);

  // Allocate a small string to OLD_DATA_SPACE and NEW_SPACE
  objs[next_objs_index++] = factory->NewStringFromStaticChars("abcdefghij");
  objs[next_objs_index++] =
      factory->NewStringFromStaticChars("abcdefghij", AllocationType::kOld);

  // Allocate a large string (for large object space).
  int large_size = kMaxRegularHeapObjectSize + 1;
  char* str = new char[large_size];
  for (int i = 0; i < large_size - 1; ++i) str[i] = 'a';
  str[large_size - 1] = '\0';
  objs[next_objs_index++] =
      factory->NewStringFromAsciiChecked(str, AllocationType::kOld);
  delete[] str;

  // Add a Map object to look for.
  objs[next_objs_index++] =
      Handle<Map>(HeapObject::cast(*objs[0]).map(), isolate);

  CHECK_EQ(objs_count, next_objs_index);
  CHECK_EQ(objs_count, ObjectsFoundInHeap(CcTest::heap(), objs, objs_count));
}

TEST(TestBytecodeFlushing) {
#ifndef V8_LITE_MODE
  FLAG_opt = false;
  FLAG_always_opt = false;
  i::FLAG_optimize_for_size = false;
#endif  // V8_LITE_MODE
  i::FLAG_flush_bytecode = true;
  i::FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  Isolate* i_isolate = CcTest::i_isolate();
  Factory* factory = i_isolate->factory();

  {
    v8::HandleScope scope(isolate);
    v8::Context::New(isolate)->Enter();
    const char* source =
        "function foo() {"
        "  var x = 42;"
        "  var y = 42;"
        "  var z = x + y;"
        "};"
        "foo()";
    Handle<String> foo_name = factory->InternalizeUtf8String("foo");

    // This compile will add the code to the compilation cache.
    {
      v8::HandleScope scope(isolate);
      CompileRun(source);
    }

    // Check function is compiled.
    Handle<Object> func_value =
        Object::GetProperty(i_isolate, i_isolate->global_object(), foo_name)
            .ToHandleChecked();
    CHECK(func_value->IsJSFunction());
    Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
    CHECK(function->shared().is_compiled());

    // The code will survive at least two GCs.
    CcTest::CollectAllGarbage();
    CcTest::CollectAllGarbage();
    CHECK(function->shared().is_compiled());

    // Simulate several GCs that use full marking.
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      CcTest::CollectAllGarbage();
    }

    // foo should no longer be in the compilation cache
    CHECK(!function->shared().is_compiled());
    CHECK(!function->is_compiled());
    // Call foo to get it recompiled.
    CompileRun("foo()");
    CHECK(function->shared().is_compiled());
    CHECK(function->is_compiled());
  }
}

#ifndef V8_LITE_MODE

TEST(TestOptimizeAfterBytecodeFlushingCandidate) {
  FLAG_opt = true;
  FLAG_always_opt = false;
  i::FLAG_optimize_for_size = false;
  i::FLAG_incremental_marking = true;
  i::FLAG_flush_bytecode = true;
  i::FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Handle<Object> func_value =
      Object::GetProperty(isolate, isolate->global_object(), foo_name)
          .ToHandleChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
  CHECK(function->shared().is_compiled());

  // The code will survive at least two GCs.
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();
  CHECK(function->shared().is_compiled());

  // Simulate several GCs that use incremental marking.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    heap::SimulateIncrementalMarking(CcTest::heap());
    CcTest::CollectAllGarbage();
  }
  CHECK(!function->shared().is_compiled());
  CHECK(!function->is_compiled());

  // This compile will compile the function again.
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun("foo();");
  }

  // Simulate several GCs that use incremental marking but make sure
  // the loop breaks once the function is enqueued as a candidate.
  for (int i = 0; i < kAgingThreshold; i++) {
    heap::SimulateIncrementalMarking(CcTest::heap());
    if (function->shared().GetBytecodeArray().IsOld()) break;
    CcTest::CollectAllGarbage();
  }

  // Force optimization while incremental marking is active and while
  // the function is enqueued as a candidate.
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "%PrepareFunctionForOptimization(foo);"
        "%OptimizeFunctionOnNextCall(foo); foo();");
  }

  // Simulate one final GC and make sure the candidate wasn't flushed.
  CcTest::CollectAllGarbage();
  CHECK(function->shared().is_compiled());
  CHECK(function->is_compiled());
}

#endif  // V8_LITE_MODE

TEST(TestUseOfIncrementalBarrierOnCompileLazy) {
  if (!FLAG_incremental_marking) return;
  // Turn off always_opt because it interferes with running the built-in for
  // the last call to g().
  FLAG_always_opt = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function make_closure(x) {"
      "  return function() { return x + 3 };"
      "}"
      "var f = make_closure(5);"
      "%PrepareFunctionForOptimization(f); f();"
      "var g = make_closure(5);");

  // Check f is compiled.
  Handle<String> f_name = factory->InternalizeUtf8String("f");
  Handle<Object> f_value =
      Object::GetProperty(isolate, isolate->global_object(), f_name)
          .ToHandleChecked();
  Handle<JSFunction> f_function = Handle<JSFunction>::cast(f_value);
  CHECK(f_function->is_compiled());

  // Check g is not compiled.
  Handle<String> g_name = factory->InternalizeUtf8String("g");
  Handle<Object> g_value =
      Object::GetProperty(isolate, isolate->global_object(), g_name)
          .ToHandleChecked();
  Handle<JSFunction> g_function = Handle<JSFunction>::cast(g_value);
  CHECK(!g_function->is_compiled());

  heap::SimulateIncrementalMarking(heap);
  CompileRun("%OptimizeFunctionOnNextCall(f); f();");

  // g should now have available an optimized function, unmarked by gc. The
  // CompileLazy built-in will discover it and install it in the closure, and
  // the incremental write barrier should be used.
  CompileRun("g();");
  CHECK(g_function->is_compiled());
}

TEST(CompilationCacheCachingBehavior) {
  // If we do not have the compilation cache turned off, this test is invalid.
  if (!FLAG_compilation_cache) {
    return;
  }
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  CompilationCache* compilation_cache = isolate->compilation_cache();
  LanguageMode language_mode = construct_language_mode(FLAG_use_strict);

  v8::HandleScope scope(CcTest::isolate());
  const char* raw_source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo();";
  Handle<String> source = factory->InternalizeUtf8String(raw_source);
  Handle<Context> native_context = isolate->native_context();

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(raw_source);
  }

  // The script should be in the cache now.
  {
    v8::HandleScope scope(CcTest::isolate());
    MaybeHandle<SharedFunctionInfo> cached_script =
        compilation_cache->LookupScript(source, Handle<Object>(), 0, 0,
                                        v8::ScriptOriginOptions(true, false),
                                        native_context, language_mode);
    CHECK(!cached_script.is_null());
  }

  // Check that the code cache entry survives at least one GC.
  {
    CcTest::CollectAllGarbage();
    v8::HandleScope scope(CcTest::isolate());
    MaybeHandle<SharedFunctionInfo> cached_script =
        compilation_cache->LookupScript(source, Handle<Object>(), 0, 0,
                                        v8::ScriptOriginOptions(true, false),
                                        native_context, language_mode);
    CHECK(!cached_script.is_null());

    // Progress code age until it's old and ready for GC.
    Handle<SharedFunctionInfo> shared = cached_script.ToHandleChecked();
    CHECK(shared->HasBytecodeArray());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      shared->GetBytecodeArray().MakeOlder();
    }
  }

  CcTest::CollectAllGarbage();

  {
    v8::HandleScope scope(CcTest::isolate());
    // Ensure code aging cleared the entry from the cache.
    MaybeHandle<SharedFunctionInfo> cached_script =
        compilation_cache->LookupScript(source, Handle<Object>(), 0, 0,
                                        v8::ScriptOriginOptions(true, false),
                                        native_context, language_mode);
    CHECK(cached_script.is_null());
  }
}


static void OptimizeEmptyFunction(const char* name) {
  HandleScope scope(CcTest::i_isolate());
  EmbeddedVector<char, 256> source;
  SNPrintF(source,
           "function %s() { return 0; }"
           "%%PrepareFunctionForOptimization(%s);"
           "%s(); %s();"
           "%%OptimizeFunctionOnNextCall(%s);"
           "%s();",
           name, name, name, name, name, name);
  CompileRun(source.begin());
}


// Count the number of native contexts in the weak list of native contexts.
int CountNativeContexts() {
  int count = 0;
  Object object = CcTest::heap()->native_contexts_list();
  while (!object.IsUndefined(CcTest::i_isolate())) {
    count++;
    object = Context::cast(object).next_context_link();
  }
  return count;
}

TEST(TestInternalWeakLists) {
  FLAG_always_opt = false;
  FLAG_allow_natives_syntax = true;
  v8::V8::Initialize();

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  FLAG_retain_maps_for_n_gc = 0;

  static const int kNumTestContexts = 10;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::Local<v8::Context> ctx[kNumTestContexts];
  if (!isolate->use_optimizer()) return;

  CHECK_EQ(0, CountNativeContexts());

  // Create a number of global contests which gets linked together.
  for (int i = 0; i < kNumTestContexts; i++) {
    ctx[i] = v8::Context::New(CcTest::isolate());

    // Collect garbage that might have been created by one of the
    // installed extensions.
    isolate->compilation_cache()->Clear();
    CcTest::CollectAllGarbage();

    CHECK_EQ(i + 1, CountNativeContexts());

    ctx[i]->Enter();

    // Create a handle scope so no function objects get stuck in the outer
    // handle scope.
    HandleScope scope(isolate);
    OptimizeEmptyFunction("f1");
    OptimizeEmptyFunction("f2");
    OptimizeEmptyFunction("f3");
    OptimizeEmptyFunction("f4");
    OptimizeEmptyFunction("f5");

    // Remove function f1, and
    CompileRun("f1=null");

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      CcTest::CollectGarbage(NEW_SPACE);
    }

    // Mark compact handles the weak references.
    isolate->compilation_cache()->Clear();
    CcTest::CollectAllGarbage();

    // Get rid of f3 and f5 in the same way.
    CompileRun("f3=null");
    for (int j = 0; j < 10; j++) {
      CcTest::CollectGarbage(NEW_SPACE);
    }
    CcTest::CollectAllGarbage();
    CompileRun("f5=null");
    for (int j = 0; j < 10; j++) {
      CcTest::CollectGarbage(NEW_SPACE);
    }
    CcTest::CollectAllGarbage();

    ctx[i]->Exit();
  }

  // Force compilation cache cleanup.
  CcTest::heap()->NotifyContextDisposed(true);
  CcTest::CollectAllGarbage();

  // Dispose the native contexts one by one.
  for (int i = 0; i < kNumTestContexts; i++) {
    // TODO(dcarney): is there a better way to do this?
    i::Address* unsafe = reinterpret_cast<i::Address*>(*ctx[i]);
    *unsafe = ReadOnlyRoots(CcTest::heap()).undefined_value().ptr();
    ctx[i].Clear();

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      CcTest::CollectGarbage(i::NEW_SPACE);
      CHECK_EQ(kNumTestContexts - i, CountNativeContexts());
    }

    // Mark compact handles the weak references.
    CcTest::CollectAllGarbage();
    CHECK_EQ(kNumTestContexts - i - 1, CountNativeContexts());
  }

  CHECK_EQ(0, CountNativeContexts());
}


TEST(TestSizeOfRegExpCode) {
  if (!FLAG_regexp_optimization) return;

  v8::V8::Initialize();

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  LocalContext context;

  // Adjust source below and this check to match
  // RegExp::kRegExpTooLargeToOptimize.
  CHECK_EQ(i::RegExp::kRegExpTooLargeToOptimize, 20 * KB);

  // Compile a regexp that is much larger if we are using regexp optimizations.
  CompileRun(
      "var reg_exp_source = '(?:a|bc|def|ghij|klmno|pqrstu)';"
      "var half_size_reg_exp;"
      "while (reg_exp_source.length < 20 * 1024) {"
      "  half_size_reg_exp = reg_exp_source;"
      "  reg_exp_source = reg_exp_source + reg_exp_source;"
      "}"
      // Flatten string.
      "reg_exp_source.match(/f/);");

  // Get initial heap size after several full GCs, which will stabilize
  // the heap size and return with sweeping finished completely.
  CcTest::CollectAllAvailableGarbage();
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  int initial_size = static_cast<int>(CcTest::heap()->SizeOfObjects());

  CompileRun("'foo'.match(reg_exp_source);");
  CcTest::CollectAllAvailableGarbage();
  int size_with_regexp = static_cast<int>(CcTest::heap()->SizeOfObjects());

  CompileRun("'foo'.match(half_size_reg_exp);");
  CcTest::CollectAllAvailableGarbage();
  int size_with_optimized_regexp =
      static_cast<int>(CcTest::heap()->SizeOfObjects());

  int size_of_regexp_code = size_with_regexp - initial_size;

  // On some platforms the debug-code flag causes huge amounts of regexp code
  // to be emitted, breaking this test.
  if (!FLAG_debug_code) {
    CHECK_LE(size_of_regexp_code, 1 * MB);
  }

  // Small regexp is half the size, but compiles to more than twice the code
  // due to the optimization steps.
  CHECK_GE(size_with_optimized_regexp,
           size_with_regexp + size_of_regexp_code * 2);
}


HEAP_TEST(TestSizeOfObjects) {
  v8::V8::Initialize();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  MarkCompactCollector* collector = heap->mark_compact_collector();

  // Get initial heap size after several full GCs, which will stabilize
  // the heap size and return with sweeping finished completely.
  CcTest::CollectAllAvailableGarbage();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  int initial_size = static_cast<int>(heap->SizeOfObjects());

  {
    HandleScope scope(isolate);
    // Allocate objects on several different old-space pages so that
    // concurrent sweeper threads will be busy sweeping the old space on
    // subsequent GC runs.
    AlwaysAllocateScope always_allocate(CcTest::i_isolate());
    int filler_size = static_cast<int>(FixedArray::SizeFor(8192));
    for (int i = 1; i <= 100; i++) {
      isolate->factory()->NewFixedArray(8192, AllocationType::kOld);
      CHECK_EQ(initial_size + i * filler_size,
               static_cast<int>(heap->SizeOfObjects()));
    }
  }

  // The heap size should go back to initial size after a full GC, even
  // though sweeping didn't finish yet.
  CcTest::CollectAllGarbage();
  // Normally sweeping would not be complete here, but no guarantees.
  CHECK_EQ(initial_size, static_cast<int>(heap->SizeOfObjects()));
  // Waiting for sweeper threads should not change heap size.
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK_EQ(initial_size, static_cast<int>(heap->SizeOfObjects()));
}


TEST(TestAlignmentCalculations) {
  // Maximum fill amounts are consistent.
  int maximum_double_misalignment = kDoubleSize - kTaggedSize;
  int max_word_fill = Heap::GetMaximumFillToAlign(kWordAligned);
  CHECK_EQ(0, max_word_fill);
  int max_double_fill = Heap::GetMaximumFillToAlign(kDoubleAligned);
  CHECK_EQ(maximum_double_misalignment, max_double_fill);
  int max_double_unaligned_fill = Heap::GetMaximumFillToAlign(kDoubleUnaligned);
  CHECK_EQ(maximum_double_misalignment, max_double_unaligned_fill);

  Address base = kNullAddress;
  int fill = 0;

  // Word alignment never requires fill.
  fill = Heap::GetFillToAlign(base, kWordAligned);
  CHECK_EQ(0, fill);
  fill = Heap::GetFillToAlign(base + kTaggedSize, kWordAligned);
  CHECK_EQ(0, fill);

  // No fill is required when address is double aligned.
  fill = Heap::GetFillToAlign(base, kDoubleAligned);
  CHECK_EQ(0, fill);
  // Fill is required if address is not double aligned.
  fill = Heap::GetFillToAlign(base + kTaggedSize, kDoubleAligned);
  CHECK_EQ(maximum_double_misalignment, fill);
  // kDoubleUnaligned has the opposite fill amounts.
  fill = Heap::GetFillToAlign(base, kDoubleUnaligned);
  CHECK_EQ(maximum_double_misalignment, fill);
  fill = Heap::GetFillToAlign(base + kTaggedSize, kDoubleUnaligned);
  CHECK_EQ(0, fill);
}

static HeapObject NewSpaceAllocateAligned(int size,
                                          AllocationAlignment alignment) {
  Heap* heap = CcTest::heap();
  AllocationResult allocation =
      heap->new_space()->AllocateRawAligned(size, alignment);
  HeapObject obj;
  allocation.To(&obj);
  heap->CreateFillerObjectAt(obj.address(), size, ClearRecordedSlots::kNo);
  return obj;
}

// Get new space allocation into the desired alignment.
static Address AlignNewSpace(AllocationAlignment alignment, int offset) {
  Address* top_addr = CcTest::heap()->new_space()->allocation_top_address();
  int fill = Heap::GetFillToAlign(*top_addr, alignment);
  int allocation = fill + offset;
  if (allocation) {
    NewSpaceAllocateAligned(allocation, kWordAligned);
  }
  return *top_addr;
}


TEST(TestAlignedAllocation) {
  // Double misalignment is 4 on 32-bit platforms or when pointer compression
  // is enabled, 0 on 64-bit ones when pointer compression is disabled.
  const intptr_t double_misalignment = kDoubleSize - kTaggedSize;
  Address* top_addr = CcTest::heap()->new_space()->allocation_top_address();
  Address start;
  HeapObject obj;
  HeapObject filler;
  if (double_misalignment) {
    // Allocate a pointer sized object that must be double aligned at an
    // aligned address.
    start = AlignNewSpace(kDoubleAligned, 0);
    obj = NewSpaceAllocateAligned(kTaggedSize, kDoubleAligned);
    CHECK(IsAligned(obj.address(), kDoubleAlignment));
    // There is no filler.
    CHECK_EQ(kTaggedSize, *top_addr - start);

    // Allocate a second pointer sized object that must be double aligned at an
    // unaligned address.
    start = AlignNewSpace(kDoubleAligned, kTaggedSize);
    obj = NewSpaceAllocateAligned(kTaggedSize, kDoubleAligned);
    CHECK(IsAligned(obj.address(), kDoubleAlignment));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler && filler.IsFiller() && filler.Size() == kTaggedSize);
    CHECK_EQ(kTaggedSize + double_misalignment, *top_addr - start);

    // Similarly for kDoubleUnaligned.
    start = AlignNewSpace(kDoubleUnaligned, 0);
    obj = NewSpaceAllocateAligned(kTaggedSize, kDoubleUnaligned);
    CHECK(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    CHECK_EQ(kTaggedSize, *top_addr - start);
    start = AlignNewSpace(kDoubleUnaligned, kTaggedSize);
    obj = NewSpaceAllocateAligned(kTaggedSize, kDoubleUnaligned);
    CHECK(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler && filler.IsFiller() && filler.Size() == kTaggedSize);
    CHECK_EQ(kTaggedSize + double_misalignment, *top_addr - start);
  }
}

static HeapObject OldSpaceAllocateAligned(int size,
                                          AllocationAlignment alignment) {
  Heap* heap = CcTest::heap();
  AllocationResult allocation =
      heap->old_space()->AllocateRawAligned(size, alignment);
  HeapObject obj;
  allocation.To(&obj);
  heap->CreateFillerObjectAt(obj.address(), size, ClearRecordedSlots::kNo);
  return obj;
}

// Get old space allocation into the desired alignment.
static Address AlignOldSpace(AllocationAlignment alignment, int offset) {
  Address* top_addr = CcTest::heap()->old_space()->allocation_top_address();
  int fill = Heap::GetFillToAlign(*top_addr, alignment);
  int allocation = fill + offset;
  if (allocation) {
    OldSpaceAllocateAligned(allocation, kWordAligned);
  }
  Address top = *top_addr;
  // Now force the remaining allocation onto the free list.
  CcTest::heap()->old_space()->FreeLinearAllocationArea();
  return top;
}


// Test the case where allocation must be done from the free list, so filler
// may precede or follow the object.
TEST(TestAlignedOverAllocation) {
  Heap* heap = CcTest::heap();
  // Test checks for fillers before and behind objects and requires a fresh
  // page and empty free list.
  heap::AbandonCurrentlyFreeMemory(heap->old_space());
  // Allocate a dummy object to properly set up the linear allocation info.
  AllocationResult dummy = heap->old_space()->AllocateRawUnaligned(kTaggedSize);
  CHECK(!dummy.IsRetry());
  heap->CreateFillerObjectAt(dummy.ToObjectChecked().address(), kTaggedSize,
                             ClearRecordedSlots::kNo);

  // Double misalignment is 4 on 32-bit platforms or when pointer compression
  // is enabled, 0 on 64-bit ones when pointer compression is disabled.
  const intptr_t double_misalignment = kDoubleSize - kTaggedSize;
  Address start;
  HeapObject obj;
  HeapObject filler;
  if (double_misalignment) {
    start = AlignOldSpace(kDoubleAligned, 0);
    obj = OldSpaceAllocateAligned(kTaggedSize, kDoubleAligned);
    // The object is aligned.
    CHECK(IsAligned(obj.address(), kDoubleAlignment));
    // Try the opposite alignment case.
    start = AlignOldSpace(kDoubleAligned, kTaggedSize);
    obj = OldSpaceAllocateAligned(kTaggedSize, kDoubleAligned);
    CHECK(IsAligned(obj.address(), kDoubleAlignment));
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler);
    CHECK(filler.IsFiller());
    CHECK_EQ(kTaggedSize, filler.Size());
    CHECK(obj != filler && filler.IsFiller() && filler.Size() == kTaggedSize);

    // Similarly for kDoubleUnaligned.
    start = AlignOldSpace(kDoubleUnaligned, 0);
    obj = OldSpaceAllocateAligned(kTaggedSize, kDoubleUnaligned);
    // The object is aligned.
    CHECK(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    // Try the opposite alignment case.
    start = AlignOldSpace(kDoubleUnaligned, kTaggedSize);
    obj = OldSpaceAllocateAligned(kTaggedSize, kDoubleUnaligned);
    CHECK(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler && filler.IsFiller() && filler.Size() == kTaggedSize);
  }
}

TEST(HeapNumberAlignment) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope sc(isolate);

  const auto required_alignment =
      HeapObject::RequiredAlignment(*factory->heap_number_map());
  const int maximum_misalignment =
      Heap::GetMaximumFillToAlign(required_alignment);

  for (int offset = 0; offset <= maximum_misalignment; offset += kTaggedSize) {
    AlignNewSpace(required_alignment, offset);
    Handle<Object> number_new = factory->NewNumber(1.000123);
    CHECK(number_new->IsHeapNumber());
    CHECK(Heap::InYoungGeneration(*number_new));
    CHECK_EQ(0, Heap::GetFillToAlign(HeapObject::cast(*number_new).address(),
                                     required_alignment));

    AlignOldSpace(required_alignment, offset);
    Handle<Object> number_old =
        factory->NewNumber(1.000321, AllocationType::kOld);
    CHECK(number_old->IsHeapNumber());
    CHECK(heap->InOldSpace(*number_old));
    CHECK_EQ(0, Heap::GetFillToAlign(HeapObject::cast(*number_old).address(),
                                     required_alignment));
  }
}

TEST(TestSizeOfObjectsVsHeapObjectIteratorPrecision) {
  CcTest::InitializeVM();
  HeapObjectIterator iterator(CcTest::heap());
  intptr_t size_of_objects_1 = CcTest::heap()->SizeOfObjects();
  intptr_t size_of_objects_2 = 0;
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (!obj.IsFreeSpace()) {
      size_of_objects_2 += obj.Size();
    }
  }
  // Delta must be within 5% of the larger result.
  // TODO(gc): Tighten this up by distinguishing between byte
  // arrays that are real and those that merely mark free space
  // on the heap.
  if (size_of_objects_1 > size_of_objects_2) {
    intptr_t delta = size_of_objects_1 - size_of_objects_2;
    PrintF("Heap::SizeOfObjects: %" V8PRIdPTR
           ", "
           "Iterator: %" V8PRIdPTR
           ", "
           "delta: %" V8PRIdPTR "\n",
           size_of_objects_1, size_of_objects_2, delta);
    CHECK_GT(size_of_objects_1 / 20, delta);
  } else {
    intptr_t delta = size_of_objects_2 - size_of_objects_1;
    PrintF("Heap::SizeOfObjects: %" V8PRIdPTR
           ", "
           "Iterator: %" V8PRIdPTR
           ", "
           "delta: %" V8PRIdPTR "\n",
           size_of_objects_1, size_of_objects_2, delta);
    CHECK_GT(size_of_objects_2 / 20, delta);
  }
}

TEST(GrowAndShrinkNewSpace) {
  // Avoid shrinking new space in GC epilogue. This can happen if allocation
  // throughput samples have been taken while executing the benchmark.
  FLAG_predictable = true;

  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  NewSpace* new_space = heap->new_space();

  if (heap->MaxSemiSpaceSize() == heap->InitialSemiSpaceSize()) {
    return;
  }

  // Make sure we're in a consistent state to start out.
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();
  new_space->Shrink();

  // Explicitly growing should double the space capacity.
  size_t old_capacity, new_capacity;
  old_capacity = new_space->TotalCapacity();
  new_space->Grow();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(2 * old_capacity, new_capacity);

  old_capacity = new_space->TotalCapacity();
  {
    v8::HandleScope temporary_scope(CcTest::isolate());
    heap::SimulateFullSpace(new_space);
  }
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);

  // Explicitly shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);

  // Let the scavenger empty the new space.
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK_LE(new_space->Size(), old_capacity);

  // Explicitly shrinking should halve the space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, 2 * new_capacity);

  // Consecutive shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_space->Shrink();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);
}

TEST(CollectingAllAvailableGarbageShrinksNewSpace) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  if (heap->MaxSemiSpaceSize() == heap->InitialSemiSpaceSize()) {
    return;
  }

  v8::HandleScope scope(CcTest::isolate());
  NewSpace* new_space = heap->new_space();
  size_t old_capacity, new_capacity;
  old_capacity = new_space->TotalCapacity();
  new_space->Grow();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(2 * old_capacity, new_capacity);
  {
    v8::HandleScope temporary_scope(CcTest::isolate());
    heap::SimulateFullSpace(new_space);
  }
  CcTest::CollectAllAvailableGarbage();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);
}

static int NumberOfGlobalObjects() {
  int count = 0;
  HeapObjectIterator iterator(CcTest::heap());
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (obj.IsJSGlobalObject()) count++;
  }
  return count;
}


// Test that we don't embed maps from foreign contexts into
// optimized code.
TEST(LeakNativeContextViaMap) {
  FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = {x: 42}");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v =
        ctx1->Global()->Get(ctx1, v8_str("v")).ToLocalChecked();
    ctx2->Enter();
    CHECK(ctx2->Global()->Set(ctx2, v8_str("o"), v).FromJust());
    v8::Local<v8::Value> res = CompileRun(
        "function f() { return o.x; }"
        "%PrepareFunctionForOptimization(f);"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value(ctx2).FromJust());
    CHECK(ctx2->Global()
              ->Set(ctx2, v8_str("o"), v8::Int32::New(isolate, 0))
              .FromJust());
    ctx2->Exit();
    v8::Local<v8::Context>::New(isolate, ctx1)->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(1, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


// Test that we don't embed functions from foreign contexts into
// optimized code.
TEST(LeakNativeContextViaFunction) {
  FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = function() { return 42; }");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v =
        ctx1->Global()->Get(ctx1, v8_str("v")).ToLocalChecked();
    ctx2->Enter();
    CHECK(ctx2->Global()->Set(ctx2, v8_str("o"), v).FromJust());
    v8::Local<v8::Value> res = CompileRun(
        "function f(x) { return x(); }"
        "%PrepareFunctionForOptimization(f);"
        "for (var i = 0; i < 10; ++i) f(o);"
        "%OptimizeFunctionOnNextCall(f);"
        "f(o);");
    CHECK_EQ(42, res->Int32Value(ctx2).FromJust());
    CHECK(ctx2->Global()
              ->Set(ctx2, v8_str("o"), v8::Int32::New(isolate, 0))
              .FromJust());
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(1, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(LeakNativeContextViaMapKeyed) {
  FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = [42, 43]");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v =
        ctx1->Global()->Get(ctx1, v8_str("v")).ToLocalChecked();
    ctx2->Enter();
    CHECK(ctx2->Global()->Set(ctx2, v8_str("o"), v).FromJust());
    v8::Local<v8::Value> res = CompileRun(
        "function f() { return o[0]; }"
        "%PrepareFunctionForOptimization(f);"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value(ctx2).FromJust());
    CHECK(ctx2->Global()
              ->Set(ctx2, v8_str("o"), v8::Int32::New(isolate, 0))
              .FromJust());
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(1, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(LeakNativeContextViaMapProto) {
  FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = { y: 42}");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v =
        ctx1->Global()->Get(ctx1, v8_str("v")).ToLocalChecked();
    ctx2->Enter();
    CHECK(ctx2->Global()->Set(ctx2, v8_str("o"), v).FromJust());
    v8::Local<v8::Value> res = CompileRun(
        "function f() {"
        "  var p = {x: 42};"
        "  p.__proto__ = o;"
        "  return p.x;"
        "}"
        "%PrepareFunctionForOptimization(f);"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value(ctx2).FromJust());
    CHECK(ctx2->Global()
              ->Set(ctx2, v8_str("o"), v8::Int32::New(isolate, 0))
              .FromJust());
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(1, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(InstanceOfStubWriteBarrier) {
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  FLAG_allow_natives_syntax = true;
#ifdef VERIFY_HEAP
  FLAG_verify_heap = true;
#endif

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer()) return;
  if (FLAG_force_marking_deque_overflows) return;
  v8::HandleScope outer_scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "function foo () { }"
        "function mkbar () { return new (new Function(\"\")) (); }"
        "function f (x) { return (x instanceof foo); }"
        "function g () { f(mkbar()); }"
        "%PrepareFunctionForOptimization(f);"
        "f(new foo()); f(new foo());"
        "%OptimizeFunctionOnNextCall(f);"
        "f(new foo()); g();");
  }

  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  CcTest::heap()->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                          i::GarbageCollectionReason::kTesting);

  i::Handle<JSFunction> f = i::Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  CHECK(f->IsOptimized());

  IncrementalMarking::MarkingState* marking_state = marking->marking_state();

  const double kStepSizeInMs = 100;
  while (!marking_state->IsBlack(f->code()) && !marking->IsStopped()) {
    // Discard any pending GC requests otherwise we will get GC when we enter
    // code below.
    marking->V8Step(kStepSizeInMs, IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    StepOrigin::kV8);
  }

  CHECK(marking->IsMarking());

  {
    v8::HandleScope scope(CcTest::isolate());
    v8::Local<v8::Object> global = CcTest::global();
    v8::Local<v8::Function> g = v8::Local<v8::Function>::Cast(
        global->Get(ctx, v8_str("g")).ToLocalChecked());
    g->Call(ctx, global, 0, nullptr).ToLocalChecked();
  }

  CcTest::heap()->incremental_marking()->set_should_hurry(true);
  CcTest::CollectGarbage(OLD_SPACE);
}

HEAP_TEST(GCFlags) {
  if (!FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();

  heap->set_current_gc_flags(Heap::kNoGCFlags);
  CHECK_EQ(Heap::kNoGCFlags, heap->current_gc_flags_);

  // Check whether we appropriately reset flags after GC.
  CcTest::heap()->CollectAllGarbage(Heap::kReduceMemoryFootprintMask,
                                    GarbageCollectionReason::kTesting);
  CHECK_EQ(Heap::kNoGCFlags, heap->current_gc_flags_);

  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }

  IncrementalMarking* marking = heap->incremental_marking();
  marking->Stop();
  heap->StartIncrementalMarking(Heap::kReduceMemoryFootprintMask,
                                i::GarbageCollectionReason::kTesting);
  CHECK_NE(0, heap->current_gc_flags_ & Heap::kReduceMemoryFootprintMask);

  CcTest::CollectGarbage(NEW_SPACE);
  // NewSpace scavenges should not overwrite the flags.
  CHECK_NE(0, heap->current_gc_flags_ & Heap::kReduceMemoryFootprintMask);

  CcTest::CollectAllGarbage();
  CHECK_EQ(Heap::kNoGCFlags, heap->current_gc_flags_);
}

HEAP_TEST(Regress845060) {
  // Regression test for crbug.com/845060, where a raw pointer to a string's
  // data was kept across an allocation. If the allocation causes GC and
  // moves the string, such raw pointers become invalid.
  FLAG_allow_natives_syntax = true;
  FLAG_stress_incremental_marking = false;
  FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();

  // Preparation: create a string in new space.
  Local<Value> str = CompileRun("var str = (new Array(10000)).join('x'); str");
  CHECK(Heap::InYoungGeneration(*v8::Utils::OpenHandle(*str)));

  // Idle incremental marking sets the "kReduceMemoryFootprint" flag, which
  // causes from_space to be unmapped after scavenging.
  heap->StartIdleIncrementalMarking(GarbageCollectionReason::kTesting);
  CHECK(heap->ShouldReduceMemory());

  // Run the test (which allocates results) until the original string was
  // promoted to old space. Unmapping of from_space causes accesses to any
  // stale raw pointers to crash.
  CompileRun("while (%InYoungGeneration(str)) { str.split(''); }");
  CHECK(!Heap::InYoungGeneration(*v8::Utils::OpenHandle(*str)));
}

TEST(IdleNotificationFinishMarking) {
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  const int initial_gc_count = CcTest::heap()->gc_count();
  heap::SimulateFullSpace(CcTest::heap()->old_space());
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  CcTest::heap()->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                          i::GarbageCollectionReason::kTesting);

  CHECK_EQ(CcTest::heap()->gc_count(), initial_gc_count);

  const double kStepSizeInMs = 100;
  do {
    marking->V8Step(kStepSizeInMs, IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    StepOrigin::kV8);
  } while (
      !CcTest::heap()->mark_compact_collector()->marking_worklist()->IsEmpty());

  marking->SetWeakClosureWasOverApproximatedForTesting(true);

  // The next idle notification has to finish incremental marking.
  const double kLongIdleTime = 1000.0;
  CcTest::isolate()->IdleNotificationDeadline(
      (v8::base::TimeTicks::HighResolutionNow().ToInternalValue() /
       static_cast<double>(v8::base::Time::kMicrosecondsPerSecond)) +
      kLongIdleTime);
  CHECK_EQ(CcTest::heap()->gc_count(), initial_gc_count + 1);
}


// Test that HAllocateObject will always return an object in new-space.
TEST(OptimizedAllocationAlwaysInNewSpace) {
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  heap::SimulateFullSpace(CcTest::heap()->new_space());
  AlwaysAllocateScope always_allocate(CcTest::i_isolate());
  v8::Local<v8::Value> res = CompileRun(
      "function c(x) {"
      "  this.x = x;"
      "  for (var i = 0; i < 32; i++) {"
      "    this['x' + i] = x;"
      "  }"
      "}"
      "function f(x) { return new c(x); };"
      "%PrepareFunctionForOptimization(f);"
      "f(1); f(2); f(3);"
      "%OptimizeFunctionOnNextCall(f);"
      "f(4);");

  CHECK_EQ(4, res.As<v8::Object>()
                  ->GetRealNamedProperty(ctx, v8_str("x"))
                  .ToLocalChecked()
                  ->Int32Value(ctx)
                  .FromJust());

  i::Handle<JSReceiver> o =
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res));

  CHECK(Heap::InYoungGeneration(*o));
}


TEST(OptimizedPretenuringAllocationFolding) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array();"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = [[{}], [1.1]];"
              "  }"
              "  return elements[number_elements-1]"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  v8::Local<v8::Value> int_array =
      v8::Object::Cast(*res)->Get(ctx, v8_str("0")).ToLocalChecked();
  i::Handle<JSObject> int_array_handle = i::Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(int_array)));
  v8::Local<v8::Value> double_array =
      v8::Object::Cast(*res)->Get(ctx, v8_str("1")).ToLocalChecked();
  i::Handle<JSObject> double_array_handle = i::Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(double_array)));

  i::Handle<JSReceiver> o =
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle->elements()));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle->elements()));
}


TEST(OptimizedPretenuringObjectArrayLiterals) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking) {
    return;
  }
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = [{}, {}, {}];"
              "  }"
              "  return elements[number_elements - 1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  i::Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));

  CHECK(CcTest::heap()->InOldSpace(o->elements()));
  CHECK(CcTest::heap()->InOldSpace(*o));
}

TEST(OptimizedPretenuringNestedInObjectProperties) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking) {
    return;
  }
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space until maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  // Keep the nested literal alive while its root is freed
  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "let number_elements = %d;"
              "let elements = new Array(number_elements);"
              "function f() {"
              "  for (let i = 0; i < number_elements; i++) {"
              "     let l =  {a: {c: 2.2, d: {e: 3.3}}, b: 1.1}; "
              "    elements[i] = l.a;"
              "  }"
              "  return elements[number_elements-1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  i::Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));

  // Nested literal sites are only pretenured if the top level
  // literal is pretenured
  CHECK(Heap::InYoungGeneration(*o));
}

TEST(OptimizedPretenuringMixedInObjectProperties) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }


  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = {a: {c: 2.2, d: {}}, b: 1.1};"
              "  }"
              "  return elements[number_elements - 1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  i::Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));

  CHECK(CcTest::heap()->InOldSpace(*o));
  FieldIndex idx1 = FieldIndex::ForPropertyIndex(o->map(), 0);
  FieldIndex idx2 = FieldIndex::ForPropertyIndex(o->map(), 1);
  CHECK(CcTest::heap()->InOldSpace(o->RawFastPropertyAt(idx1)));
  if (!o->IsUnboxedDoubleField(idx2)) {
    CHECK(CcTest::heap()->InOldSpace(o->RawFastPropertyAt(idx2)));
  } else {
    CHECK_EQ(1.1, o->RawFastDoublePropertyAt(idx2));
  }

  JSObject inner_object = JSObject::cast(o->RawFastPropertyAt(idx1));
  CHECK(CcTest::heap()->InOldSpace(inner_object));
  if (!inner_object.IsUnboxedDoubleField(idx1)) {
    CHECK(CcTest::heap()->InOldSpace(inner_object.RawFastPropertyAt(idx1)));
  } else {
    CHECK_EQ(2.2, inner_object.RawFastDoublePropertyAt(idx1));
  }
  CHECK(CcTest::heap()->InOldSpace(inner_object.RawFastPropertyAt(idx2)));
}


TEST(OptimizedPretenuringDoubleArrayProperties) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space until maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = {a: 1.1, b: 2.2};"
              "  }"
              "  return elements[i - 1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  i::Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));

  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK_EQ(o->property_array(),
           ReadOnlyRoots(CcTest::heap()).empty_property_array());
}


TEST(OptimizedPretenuringdoubleArrayLiterals) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = [1.1, 2.2, 3.3];"
              "  }"
              "  return elements[number_elements - 1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  i::Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));

  CHECK(CcTest::heap()->InOldSpace(o->elements()));
  CHECK(CcTest::heap()->InOldSpace(*o));
}


TEST(OptimizedPretenuringNestedMixedArrayLiterals) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = [[{}, {}, {}], [1.1, 2.2, 3.3]];"
              "  }"
              "  return elements[number_elements - 1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  v8::Local<v8::Value> int_array =
      v8::Object::Cast(*res)->Get(ctx, v8_str("0")).ToLocalChecked();
  i::Handle<JSObject> int_array_handle = i::Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(int_array)));
  v8::Local<v8::Value> double_array =
      v8::Object::Cast(*res)->Get(ctx, v8_str("1")).ToLocalChecked();
  i::Handle<JSObject> double_array_handle = i::Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(double_array)));

  Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle->elements()));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle->elements()));
}


TEST(OptimizedPretenuringNestedObjectLiterals) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = [[{}, {}, {}],[{}, {}, {}]];"
              "  }"
              "  return elements[number_elements - 1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  v8::Local<v8::Value> int_array_1 =
      v8::Object::Cast(*res)->Get(ctx, v8_str("0")).ToLocalChecked();
  Handle<JSObject> int_array_handle_1 = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(int_array_1)));
  v8::Local<v8::Value> int_array_2 =
      v8::Object::Cast(*res)->Get(ctx, v8_str("1")).ToLocalChecked();
  Handle<JSObject> int_array_handle_2 = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(int_array_2)));

  Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle_1));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle_1->elements()));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle_2));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle_2->elements()));
}


TEST(OptimizedPretenuringNestedDoubleLiterals) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = [[1.1, 1.2, 1.3],[2.1, 2.2, 2.3]];"
              "  }"
              "  return elements[number_elements - 1];"
              "};"
              "%%PrepareFunctionForOptimization(f);"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();",
              kPretenureCreationCount);

  v8::Local<v8::Value> res = CompileRun(source.begin());

  v8::Local<v8::Value> double_array_1 =
      v8::Object::Cast(*res)->Get(ctx, v8_str("0")).ToLocalChecked();
  i::Handle<JSObject> double_array_handle_1 = i::Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(double_array_1)));
  v8::Local<v8::Value> double_array_2 =
      v8::Object::Cast(*res)->Get(ctx, v8_str("1")).ToLocalChecked();
  i::Handle<JSObject> double_array_handle_2 = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(double_array_2)));

  i::Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle_1));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle_1->elements()));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle_2));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle_2->elements()));
}


// Test regular array literals allocation.
TEST(OptimizedAllocationArrayLiterals) {
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || FLAG_always_opt) return;
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  v8::Local<v8::Value> res = CompileRun(
      "function f() {"
      "  var numbers = new Array(1, 2, 3);"
      "  numbers[0] = 3.14;"
      "  return numbers;"
      "};"
      "%PrepareFunctionForOptimization(f);"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");
  CHECK_EQ(static_cast<int>(3.14), v8::Object::Cast(*res)
                                       ->Get(ctx, v8_str("0"))
                                       .ToLocalChecked()
                                       ->Int32Value(ctx)
                                       .FromJust());

  i::Handle<JSObject> o = Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));

  CHECK(Heap::InYoungGeneration(o->elements()));
}

static int CountMapTransitions(i::Isolate* isolate, Map map) {
  DisallowHeapAllocation no_gc;
  return TransitionsAccessor(isolate, map, &no_gc).NumberOfTransitions();
}


// Test that map transitions are cleared and maps are collected with
// incremental marking as well.
TEST(Regress1465) {
  if (!FLAG_incremental_marking) return;
  FLAG_stress_compaction = false;
  FLAG_stress_incremental_marking = false;
  FLAG_allow_natives_syntax = true;
  FLAG_trace_incremental_marking = true;
  FLAG_retain_maps_for_n_gc = 0;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  i::Isolate* i_isolate = CcTest::i_isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
  static const int transitions_count = 256;

  CompileRun("function F() {}");
  {
    AlwaysAllocateScope always_allocate(CcTest::i_isolate());
    for (int i = 0; i < transitions_count; i++) {
      EmbeddedVector<char, 64> buffer;
      SNPrintF(buffer, "var o = new F; o.prop%d = %d;", i, i);
      CompileRun(buffer.begin());
    }
    CompileRun("var root = new F;");
  }

  i::Handle<JSReceiver> root =
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(
          CcTest::global()->Get(ctx, v8_str("root")).ToLocalChecked()));

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(i_isolate, root->map());
  CompileRun("%DebugPrint(root);");
  CHECK_EQ(transitions_count, transitions_before);

  heap::SimulateIncrementalMarking(CcTest::heap());
  CcTest::CollectAllGarbage();

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after = CountMapTransitions(i_isolate, root->map());
  CompileRun("%DebugPrint(root);");
  CHECK_EQ(1, transitions_after);
}

static i::Handle<JSObject> GetByName(const char* name) {
  return i::Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(
          CcTest::global()
              ->Get(CcTest::isolate()->GetCurrentContext(), v8_str(name))
              .ToLocalChecked())));
}

#ifdef DEBUG
static void AddTransitions(int transitions_count) {
  AlwaysAllocateScope always_allocate(CcTest::i_isolate());
  for (int i = 0; i < transitions_count; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "var o = new F; o.prop%d = %d;", i, i);
    CompileRun(buffer.begin());
  }
}


static void AddPropertyTo(
    int gc_count, Handle<JSObject> object, const char* property_name) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<String> prop_name = factory->InternalizeUtf8String(property_name);
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  FLAG_gc_interval = gc_count;
  FLAG_gc_global = true;
  FLAG_retain_maps_for_n_gc = 0;
  CcTest::heap()->set_allocation_timeout(gc_count);
  Object::SetProperty(isolate, object, prop_name, twenty_three).Check();
}


TEST(TransitionArrayShrinksDuringAllocToZero) {
  FLAG_stress_compaction = false;
  FLAG_stress_incremental_marking = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  i::Isolate* i_isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 10;
  CompileRun("function F() { }");
  AddTransitions(transitions_count);
  CompileRun("var root = new F;");
  Handle<JSObject> root = GetByName("root");

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(i_isolate, root->map());
  CHECK_EQ(transitions_count, transitions_before);

  // Get rid of o
  CompileRun("o = new F;"
             "root = new F");
  root = GetByName("root");
  AddPropertyTo(2, root, "funny");
  CcTest::CollectGarbage(NEW_SPACE);

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after =
      CountMapTransitions(i_isolate, Map::cast(root->map().GetBackPointer()));
  CHECK_EQ(1, transitions_after);
}


TEST(TransitionArrayShrinksDuringAllocToOne) {
  FLAG_stress_compaction = false;
  FLAG_stress_incremental_marking = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  i::Isolate* i_isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 10;
  CompileRun("function F() {}");
  AddTransitions(transitions_count);
  CompileRun("var root = new F;");
  Handle<JSObject> root = GetByName("root");

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(i_isolate, root->map());
  CHECK_EQ(transitions_count, transitions_before);

  root = GetByName("root");
  AddPropertyTo(2, root, "funny");
  CcTest::CollectGarbage(NEW_SPACE);

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after =
      CountMapTransitions(i_isolate, Map::cast(root->map().GetBackPointer()));
  CHECK_EQ(2, transitions_after);
}


TEST(TransitionArrayShrinksDuringAllocToOnePropertyFound) {
  FLAG_stress_compaction = false;
  FLAG_stress_incremental_marking = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  i::Isolate* i_isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 10;
  CompileRun("function F() {}");
  AddTransitions(transitions_count);
  CompileRun("var root = new F;");
  Handle<JSObject> root = GetByName("root");

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(i_isolate, root->map());
  CHECK_EQ(transitions_count, transitions_before);

  root = GetByName("root");
  AddPropertyTo(0, root, "prop9");
  CcTest::CollectGarbage(OLD_SPACE);

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after =
      CountMapTransitions(i_isolate, Map::cast(root->map().GetBackPointer()));
  CHECK_EQ(1, transitions_after);
}
#endif  // DEBUG


TEST(ReleaseOverReservedPages) {
  if (FLAG_never_compact) return;
  FLAG_trace_gc = true;
  // The optimizer can allocate stuff, messing up the test.
#ifndef V8_LITE_MODE
  FLAG_opt = false;
  FLAG_always_opt = false;
#endif  // V8_LITE_MODE
  // - Parallel compaction increases fragmentation, depending on how existing
  //   memory is distributed. Since this is non-deterministic because of
  //   concurrent sweeping, we disable it for this test.
  // - Concurrent sweeping adds non determinism, depending on when memory is
  //   available for further reuse.
  // - Fast evacuation of pages may result in a different page count in old
  //   space.
  ManualGCScope manual_gc_scope;
  FLAG_page_promotion = false;
  FLAG_parallel_compaction = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  // If there's snapshot available, we don't know whether 20 small arrays will
  // fit on the initial pages.
  if (!isolate->snapshot_available()) return;
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());
  // Ensure that the young generation is empty.
  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  static const int number_of_test_pages = 20;

  // Prepare many pages with low live-bytes count.
  PagedSpace* old_space = heap->old_space();
  const int initial_page_count = old_space->CountTotalPages();
  const int overall_page_count = number_of_test_pages + initial_page_count;
  for (int i = 0; i < number_of_test_pages; i++) {
    AlwaysAllocateScope always_allocate(isolate);
    heap::SimulateFullSpace(old_space);
    factory->NewFixedArray(1, AllocationType::kOld);
  }
  CHECK_EQ(overall_page_count, old_space->CountTotalPages());

  // Triggering one GC will cause a lot of garbage to be discovered but
  // even spread across all allocated pages.
  CcTest::CollectAllGarbage();
  CHECK_GE(overall_page_count, old_space->CountTotalPages());

  // Triggering subsequent GCs should cause at least half of the pages
  // to be released to the OS after at most two cycles.
  CcTest::CollectAllGarbage();
  CHECK_GE(overall_page_count, old_space->CountTotalPages());
  CcTest::CollectAllGarbage();
  CHECK_GE(overall_page_count, old_space->CountTotalPages() * 2);

  // Triggering a last-resort GC should cause all pages to be released to the
  // OS so that other processes can seize the memory.  If we get a failure here
  // where there are 2 pages left instead of 1, then we should increase the
  // size of the first page a little in SizeOfFirstPage in spaces.cc.  The
  // first page should be small in order to reduce memory used when the VM
  // boots, but if the 20 small arrays don't fit on the first page then that's
  // an indication that it is too small.
  CcTest::CollectAllAvailableGarbage();
  CHECK_GE(initial_page_count, old_space->CountTotalPages());
}

static int forced_gc_counter = 0;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  isolate->GetCurrentContext();
  if (feature == v8::Isolate::kForcedGC) {
    forced_gc_counter++;
  }
}


TEST(CountForcedGC) {
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  isolate->SetUseCounterCallback(MockUseCounterCallback);

  forced_gc_counter = 0;
  const char* source = "gc();";
  CompileRun(source);
  CHECK_GT(forced_gc_counter, 0);
}


#ifdef OBJECT_PRINT
TEST(PrintSharedFunctionInfo) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  const char* source = "f = function() { return 987654321; }\n"
                       "g = function() { return 123456789; }\n";
  CompileRun(source);
  i::Handle<JSFunction> g = i::Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("g")).ToLocalChecked())));

  StdoutStream os;
  g->shared().Print(os);
  os << std::endl;
}
#endif  // OBJECT_PRINT


TEST(IncrementalMarkingPreservesMonomorphicCallIC) {
  if (!FLAG_use_ic) return;
  if (!FLAG_incremental_marking) return;
  if (FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> fun1, fun2;
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  {
    CompileRun("function fun() {};");
    fun1 = CcTest::global()->Get(ctx, v8_str("fun")).ToLocalChecked();
  }

  {
    CompileRun("function fun() {};");
    fun2 = CcTest::global()->Get(ctx, v8_str("fun")).ToLocalChecked();
  }

  // Prepare function f that contains type feedback for the two closures.
  CHECK(CcTest::global()->Set(ctx, v8_str("fun1"), fun1).FromJust());
  CHECK(CcTest::global()->Set(ctx, v8_str("fun2"), fun2).FromJust());
  CompileRun(
      "function f(a, b) { a(); b(); } %EnsureFeedbackVectorForFunction(f); "
      "f(fun1, fun2);");

  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  Handle<FeedbackVector> feedback_vector(f->feedback_vector(), f->GetIsolate());
  FeedbackVectorHelper feedback_helper(feedback_vector);

  int expected_slots = 2;
  CHECK_EQ(expected_slots, feedback_helper.slot_count());
  int slot1 = 0;
  int slot2 = 1;
  CHECK(feedback_vector->Get(feedback_helper.slot(slot1))->IsWeak());
  CHECK(feedback_vector->Get(feedback_helper.slot(slot2))->IsWeak());

  heap::SimulateIncrementalMarking(CcTest::heap());
  CcTest::CollectAllGarbage();

  CHECK(feedback_vector->Get(feedback_helper.slot(slot1))->IsWeak());
  CHECK(feedback_vector->Get(feedback_helper.slot(slot2))->IsWeak());
}


static void CheckVectorIC(Handle<JSFunction> f, int slot_index,
                          InlineCacheState desired_state) {
  Handle<FeedbackVector> vector =
      Handle<FeedbackVector>(f->feedback_vector(), f->GetIsolate());
  FeedbackVectorHelper helper(vector);
  FeedbackSlot slot = helper.slot(slot_index);
  FeedbackNexus nexus(vector, slot);
  CHECK(nexus.ic_state() == desired_state);
}

TEST(IncrementalMarkingPreservesMonomorphicConstructor) {
  if (!FLAG_incremental_marking) return;
  if (FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  // Prepare function f that contains a monomorphic IC for object
  // originating from the same native context.
  CompileRun(
      "function fun() { this.x = 1; };"
      "function f(o) { return new o(); }"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(fun); f(fun);");
  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  Handle<FeedbackVector> vector(f->feedback_vector(), f->GetIsolate());
  CHECK(vector->Get(FeedbackSlot(0))->IsWeakOrCleared());

  heap::SimulateIncrementalMarking(CcTest::heap());
  CcTest::CollectAllGarbage();

  CHECK(vector->Get(FeedbackSlot(0))->IsWeakOrCleared());
}

TEST(IncrementalMarkingPreservesMonomorphicIC) {
  if (!FLAG_use_ic) return;
  if (!FLAG_incremental_marking) return;
  if (FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  // Prepare function f that contains a monomorphic IC for object
  // originating from the same native context.
  CompileRun(
      "function fun() { this.x = 1; }; var obj = new fun();"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(o) { return o.x; } f(obj); f(obj);");
  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  CheckVectorIC(f, 0, MONOMORPHIC);

  heap::SimulateIncrementalMarking(CcTest::heap());
  CcTest::CollectAllGarbage();

  CheckVectorIC(f, 0, MONOMORPHIC);
}

TEST(IncrementalMarkingPreservesPolymorphicIC) {
  if (!FLAG_use_ic) return;
  if (!FLAG_incremental_marking) return;
  if (FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1, obj2;
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 2; }; var obj = new fun();");
    obj2 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  // Prepare function f that contains a polymorphic IC for objects
  // originating from two different native contexts.
  CHECK(CcTest::global()->Set(ctx, v8_str("obj1"), obj1).FromJust());
  CHECK(CcTest::global()->Set(ctx, v8_str("obj2"), obj2).FromJust());
  CompileRun(
      "function f(o) { return o.x; }; "
      "%EnsureFeedbackVectorForFunction(f);"
      "f(obj1); f(obj1); f(obj2);");
  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  CheckVectorIC(f, 0, POLYMORPHIC);

  // Fire context dispose notification.
  heap::SimulateIncrementalMarking(CcTest::heap());
  CcTest::CollectAllGarbage();

  CheckVectorIC(f, 0, POLYMORPHIC);
}

TEST(ContextDisposeDoesntClearPolymorphicIC) {
  if (!FLAG_use_ic) return;
  if (!FLAG_incremental_marking) return;
  if (FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1, obj2;
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 2; }; var obj = new fun();");
    obj2 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  // Prepare function f that contains a polymorphic IC for objects
  // originating from two different native contexts.
  CHECK(CcTest::global()->Set(ctx, v8_str("obj1"), obj1).FromJust());
  CHECK(CcTest::global()->Set(ctx, v8_str("obj2"), obj2).FromJust());
  CompileRun(
      "function f(o) { return o.x; }; "
      "%EnsureFeedbackVectorForFunction(f);"
      "f(obj1); f(obj1); f(obj2);");
  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  CheckVectorIC(f, 0, POLYMORPHIC);

  // Fire context dispose notification.
  CcTest::isolate()->ContextDisposedNotification();
  heap::SimulateIncrementalMarking(CcTest::heap());
  CcTest::CollectAllGarbage();

  CheckVectorIC(f, 0, POLYMORPHIC);
}


class SourceResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit SourceResource(const char* data)
    : data_(data), length_(strlen(data)) { }

  void Dispose() override {
    i::DeleteArray(data_);
    data_ = nullptr;
  }

  const char* data() const override { return data_; }

  size_t length() const override { return length_; }

  bool IsDisposed() { return data_ == nullptr; }

 private:
  const char* data_;
  size_t length_;
};


void ReleaseStackTraceDataTest(v8::Isolate* isolate, const char* source,
                               const char* accessor) {
  // Test that the data retained by the Error.stack accessor is released
  // after the first time the accessor is fired.  We use external string
  // to check whether the data is being released since the external string
  // resource's callback is fired when the external string is GC'ed.
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::HandleScope scope(isolate);
  SourceResource* resource = new SourceResource(i::StrDup(source));
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::String> source_string =
        v8::String::NewExternalOneByte(isolate, resource).ToLocalChecked();
    i_isolate->heap()->CollectAllAvailableGarbage(
        i::GarbageCollectionReason::kTesting);
    v8::Script::Compile(ctx, source_string)
        .ToLocalChecked()
        ->Run(ctx)
        .ToLocalChecked();
    CHECK(!resource->IsDisposed());
  }
  // i_isolate->heap()->CollectAllAvailableGarbage();
  CHECK(!resource->IsDisposed());

  CompileRun(accessor);
  i_isolate->heap()->CollectAllAvailableGarbage(
      i::GarbageCollectionReason::kTesting);

  // External source has been released.
  CHECK(resource->IsDisposed());
  delete resource;
}


UNINITIALIZED_TEST(ReleaseStackTraceData) {
  if (FLAG_always_opt) {
    // TODO(ulan): Remove this once the memory leak via code_next_link is fixed.
    // See: https://codereview.chromium.org/181833004/
    return;
  }
#ifndef V8_LITE_MODE
  // ICs retain objects.
  FLAG_use_ic = false;
#endif  // V8_LITE_MODE
  FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    static const char* source1 = "var error = null;            "
    /* Normal Error */           "try {                        "
                                 "  throw new Error();         "
                                 "} catch (e) {                "
                                 "  error = e;                 "
                                 "}                            ";
    static const char* source2 = "var error = null;            "
    /* Stack overflow */         "try {                        "
                                 "  (function f() { f(); })(); "
                                 "} catch (e) {                "
                                 "  error = e;                 "
                                 "}                            ";
    static const char* source3 = "var error = null;            "
    /* Normal Error */           "try {                        "
    /* as prototype */           "  throw new Error();         "
                                 "} catch (e) {                "
                                 "  error = {};                "
                                 "  error.__proto__ = e;       "
                                 "}                            ";
    static const char* source4 = "var error = null;            "
    /* Stack overflow */         "try {                        "
    /* as prototype   */         "  (function f() { f(); })(); "
                                 "} catch (e) {                "
                                 "  error = {};                "
                                 "  error.__proto__ = e;       "
                                 "}                            ";
    static const char* getter = "error.stack";
    static const char* setter = "error.stack = 0";

    ReleaseStackTraceDataTest(isolate, source1, setter);
    ReleaseStackTraceDataTest(isolate, source2, setter);
    // We do not test source3 and source4 with setter, since the setter is
    // supposed to (untypically) write to the receiver, not the holder.  This is
    // to emulate the behavior of a data property.

    ReleaseStackTraceDataTest(isolate, source1, getter);
    ReleaseStackTraceDataTest(isolate, source2, getter);
    ReleaseStackTraceDataTest(isolate, source3, getter);
    ReleaseStackTraceDataTest(isolate, source4, getter);
  }
  isolate->Dispose();
}

// TODO(mmarchini) also write tests for async/await and Promise.all
void DetailedErrorStackTraceTest(const char* src,
                                 std::function<void(Handle<FrameArray>)> test) {
  FLAG_detailed_error_stack_trace = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::TryCatch try_catch(CcTest::isolate());
  CompileRun(src);

  CHECK(try_catch.HasCaught());
  Handle<Object> exception = v8::Utils::OpenHandle(*try_catch.Exception());

  Isolate* isolate = CcTest::i_isolate();
  Handle<Name> key = isolate->factory()->stack_trace_symbol();

  Handle<FixedArray> stack_trace(Handle<FixedArray>::cast(
      Object::GetProperty(isolate, exception, key).ToHandleChecked()));

  test(GetFrameArrayFromStackTrace(isolate, stack_trace));
}

// * Test interpreted function error
TEST(DetailedErrorStackTrace) {
  static const char* source =
      "function func1(arg1) {       "
      "  let err = new Error();     "
      "  throw err;                 "
      "}                            "
      "function func2(arg1, arg2) { "
      "  func1(42);                 "
      "}                            "
      "class Foo {};                "
      "function main(arg1, arg2) {  "
      "  func2(arg1, false);        "
      "}                            "
      "var foo = new Foo();         "
      "main(foo);                   ";

  DetailedErrorStackTraceTest(source, [](Handle<FrameArray> stack_trace) {
    FixedArray foo_parameters = stack_trace->Parameters(0);
    CHECK_EQ(foo_parameters.length(), 1);
    CHECK(foo_parameters.get(0).IsSmi());
    CHECK_EQ(Smi::ToInt(foo_parameters.get(0)), 42);

    FixedArray bar_parameters = stack_trace->Parameters(1);
    CHECK_EQ(bar_parameters.length(), 2);
    CHECK(bar_parameters.get(0).IsJSObject());
    CHECK(bar_parameters.get(1).IsBoolean());
    Handle<Object> foo = Handle<Object>::cast(GetByName("foo"));
    CHECK_EQ(bar_parameters.get(0), *foo);
    CHECK(!bar_parameters.get(1).BooleanValue(CcTest::i_isolate()));

    FixedArray main_parameters = stack_trace->Parameters(2);
    CHECK_EQ(main_parameters.length(), 2);
    CHECK(main_parameters.get(0).IsJSObject());
    CHECK(main_parameters.get(1).IsUndefined());
    CHECK_EQ(main_parameters.get(0), *foo);
  });
}

// * Test optimized function with inline frame error
TEST(DetailedErrorStackTraceInline) {
  FLAG_allow_natives_syntax = true;
  static const char* source =
      "function add(x) {                     "
      " if (x == 42)                         "
      "  throw new Error();                  "
      " return x + x;                        "
      "}                                     "
      "add(0);                               "
      "add(1);                               "
      "function foo(x) {                     "
      " return add(x + 1)                    "
      "}                                     "
      "%PrepareFunctionForOptimization(foo); "
      "foo(40);                              "
      "%OptimizeFunctionOnNextCall(foo);     "
      "foo(41);                              ";

  DetailedErrorStackTraceTest(source, [](Handle<FrameArray> stack_trace) {
    FixedArray parameters_add = stack_trace->Parameters(0);
    CHECK_EQ(parameters_add.length(), 1);
    CHECK(parameters_add.get(0).IsSmi());
    CHECK_EQ(Smi::ToInt(parameters_add.get(0)), 42);

    FixedArray parameters_foo = stack_trace->Parameters(1);
    CHECK_EQ(parameters_foo.length(), 1);
    CHECK(parameters_foo.get(0).IsSmi());
    CHECK_EQ(Smi::ToInt(parameters_foo.get(0)), 41);
  });
}

// * Test builtin exit error
TEST(DetailedErrorStackTraceBuiltinExit) {
  static const char* source =
      "function test(arg1) {           "
      "  (new Number()).toFixed(arg1); "
      "}                               "
      "test(9999);                     ";

  DetailedErrorStackTraceTest(source, [](Handle<FrameArray> stack_trace) {
    FixedArray parameters = stack_trace->Parameters(0);

    CHECK_EQ(parameters.length(), 2);
    CHECK(parameters.get(0).IsSmi());
    CHECK_EQ(Smi::ToInt(parameters.get(0)), 9999);
  });
}

TEST(Regress169928) {
  FLAG_allow_natives_syntax = true;
#ifndef V8_LITE_MODE
  FLAG_opt = false;
#endif  // V8_LITE_MODE
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  LocalContext env;
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (FLAG_gc_global || FLAG_stress_compaction ||
      FLAG_stress_incremental_marking)
    return;

  // Prepare the environment
  CompileRun("function fastliteralcase(literal, value) {"
             "    literal[0] = value;"
             "    return literal;"
             "}"
             "function get_standard_literal() {"
             "    var literal = [1, 2, 3];"
             "    return literal;"
             "}"
             "obj = fastliteralcase(get_standard_literal(), 1);"
             "obj = fastliteralcase(get_standard_literal(), 1.5);"
             "obj = fastliteralcase(get_standard_literal(), 2);");

  // prepare the heap
  v8::Local<v8::String> mote_code_string =
      v8_str("fastliteralcase(mote, 2.5);");

  v8::Local<v8::String> array_name = v8_str("mote");
  CHECK(CcTest::global()
            ->Set(env.local(), array_name, v8::Int32::New(CcTest::isolate(), 0))
            .FromJust());

  // First make sure we flip spaces
  CcTest::CollectGarbage(NEW_SPACE);

  // Allocate the object.
  Handle<FixedArray> array_data =
      factory->NewFixedArray(2, AllocationType::kYoung);
  array_data->set(0, Smi::FromInt(1));
  array_data->set(1, Smi::FromInt(2));

  heap::AllocateAllButNBytes(
      CcTest::heap()->new_space(),
      JSArray::kSize + AllocationMemento::kSize + kTaggedSize);

  Handle<JSArray> array =
      factory->NewJSArrayWithElements(array_data, PACKED_SMI_ELEMENTS);

  CHECK_EQ(Smi::FromInt(2), array->length());
  CHECK(array->HasSmiOrObjectElements());

  // We need filler the size of AllocationMemento object, plus an extra
  // fill pointer value.
  HeapObject obj;
  AllocationResult allocation =
      CcTest::heap()->new_space()->AllocateRawUnaligned(
          AllocationMemento::kSize + kTaggedSize);
  CHECK(allocation.To(&obj));
  Address addr_obj = obj.address();
  CcTest::heap()->CreateFillerObjectAt(addr_obj,
                                       AllocationMemento::kSize + kTaggedSize,
                                       ClearRecordedSlots::kNo);

  // Give the array a name, making sure not to allocate strings.
  v8::Local<v8::Object> array_obj = v8::Utils::ToLocal(array);
  CHECK(CcTest::global()->Set(env.local(), array_name, array_obj).FromJust());

  // This should crash with a protection violation if we are running a build
  // with the bug.
  AlwaysAllocateScope aa_scope(isolate);
  v8::Script::Compile(env.local(), mote_code_string)
      .ToLocalChecked()
      ->Run(env.local())
      .ToLocalChecked();
}

TEST(LargeObjectSlotRecording) {
  if (!FLAG_incremental_marking) return;
  if (FLAG_never_compact) return;
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Create an object on an evacuation candidate.
  heap::SimulateFullSpace(heap->old_space());
  Handle<FixedArray> lit =
      isolate->factory()->NewFixedArray(4, AllocationType::kOld);
  Page* evac_page = Page::FromHeapObject(*lit);
  heap::ForceEvacuationCandidate(evac_page);
  FixedArray old_location = *lit;

  // Allocate a large object.
  int size = Max(1000000, kMaxRegularHeapObjectSize + KB);
  CHECK_LT(kMaxRegularHeapObjectSize, size);
  Handle<FixedArray> lo =
      isolate->factory()->NewFixedArray(size, AllocationType::kOld);
  CHECK(heap->lo_space()->Contains(*lo));

  // Start incremental marking to active write barrier.
  heap::SimulateIncrementalMarking(heap, false);

  // Create references from the large object to the object on the evacuation
  // candidate.
  const int kStep = size / 10;
  for (int i = 0; i < size; i += kStep) {
    lo->set(i, *lit);
    CHECK(lo->get(i) == old_location);
  }

  heap::SimulateIncrementalMarking(heap, true);

  // Move the evaucation candidate object.
  CcTest::CollectAllGarbage();

  // Verify that the pointers in the large object got updated.
  for (int i = 0; i < size; i += kStep) {
    CHECK_EQ(lo->get(i), *lit);
    CHECK(lo->get(i) != old_location);
  }
}

class DummyVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {}
};


TEST(DeferredHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(reinterpret_cast<v8::Isolate*>(isolate));
  HandleScopeData* data = isolate->handle_scope_data();
  Handle<Object> init(ReadOnlyRoots(heap).empty_string(), isolate);
  while (data->next < data->limit) {
    Handle<Object> obj(ReadOnlyRoots(heap).empty_string(), isolate);
  }
  // An entire block of handles has been filled.
  // Next handle would require a new block.
  CHECK(data->next == data->limit);

  DeferredHandleScope deferred(isolate);
  DummyVisitor visitor;
  isolate->handle_scope_implementer()->Iterate(&visitor);
  delete deferred.Detach();
}


TEST(IncrementalMarkingStepMakesBigProgressWithLargeObjects) {
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun("function f(n) {"
             "    var a = new Array(n);"
             "    for (var i = 0; i < n; i += 100) a[i] = i;"
             "};"
             "f(10 * 1024 * 1024);");
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  if (marking->IsStopped()) {
    CcTest::heap()->StartIncrementalMarking(
        i::Heap::kNoGCFlags, i::GarbageCollectionReason::kTesting);
  }
  heap::SimulateIncrementalMarking(CcTest::heap());
  CHECK(marking->IsComplete() ||
        marking->IsReadyToOverApproximateWeakClosure());
}


TEST(DisableInlineAllocation) {
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun(
      "function test() {"
      "  var x = [];"
      "  for (var i = 0; i < 10; i++) {"
      "    x[i] = [ {}, [1,2,3], [1,x,3] ];"
      "  }"
      "}"
      "function run() {"
      "  %PrepareFunctionForOptimization(test);"
      "  %OptimizeFunctionOnNextCall(test);"
      "  test();"
      "  %DeoptimizeFunction(test);"
      "}");

  // Warm-up with inline allocation enabled.
  CompileRun("test(); test(); run();");

  // Run test with inline allocation disabled.
  CcTest::heap()->DisableInlineAllocation();
  CompileRun("run()");

  // Run test with inline allocation re-enabled.
  CcTest::heap()->EnableInlineAllocation();
  CompileRun("run()");
}


static int AllocationSitesCount(Heap* heap) {
  int count = 0;
  for (Object site = heap->allocation_sites_list(); site.IsAllocationSite();) {
    AllocationSite cur = AllocationSite::cast(site);
    CHECK(cur.HasWeakNext());
    site = cur.weak_next();
    count++;
  }
  return count;
}

static int SlimAllocationSiteCount(Heap* heap) {
  int count = 0;
  for (Object weak_list = heap->allocation_sites_list();
       weak_list.IsAllocationSite();) {
    AllocationSite weak_cur = AllocationSite::cast(weak_list);
    for (Object site = weak_cur.nested_site(); site.IsAllocationSite();) {
      AllocationSite cur = AllocationSite::cast(site);
      CHECK(!cur.HasWeakNext());
      site = cur.nested_site();
      count++;
    }
    weak_list = weak_cur.weak_next();
  }
  return count;
}

TEST(EnsureAllocationSiteDependentCodesProcessed) {
  if (FLAG_always_opt || !FLAG_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();
  GlobalHandles* global_handles = isolate->global_handles();

  if (!isolate->use_optimizer()) return;

  // The allocation site at the head of the list is ours.
  Handle<AllocationSite> site;
  {
    LocalContext context;
    v8::HandleScope scope(context->GetIsolate());

    int count = AllocationSitesCount(heap);
    CompileRun(
        "var bar = function() { return (new Array()); };"
        "%PrepareFunctionForOptimization(bar);"
        "var a = bar();"
        "bar();"
        "bar();");

    // One allocation site should have been created.
    int new_count = AllocationSitesCount(heap);
    CHECK_EQ(new_count, (count + 1));
    site = Handle<AllocationSite>::cast(
        global_handles->Create(
            AllocationSite::cast(heap->allocation_sites_list())));

    CompileRun("%OptimizeFunctionOnNextCall(bar); bar();");

    Handle<JSFunction> bar_handle = Handle<JSFunction>::cast(
        v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
            CcTest::global()
                ->Get(context.local(), v8_str("bar"))
                .ToLocalChecked())));

    int dependency_group_count = 0;
    DependentCode dependency = site->dependent_code();
    while (dependency != ReadOnlyRoots(heap).empty_weak_fixed_array()) {
      CHECK(dependency.group() ==
                DependentCode::kAllocationSiteTransitionChangedGroup ||
            dependency.group() ==
                DependentCode::kAllocationSiteTenuringChangedGroup);
      CHECK_EQ(1, dependency.count());
      CHECK(dependency.object_at(0)->IsWeak());
      Code function_bar =
          Code::cast(dependency.object_at(0)->GetHeapObjectAssumeWeak());
      CHECK_EQ(bar_handle->code(), function_bar);
      dependency = dependency.next_link();
      dependency_group_count++;
    }
    // Expect a dependent code object for transitioning and pretenuring.
    CHECK_EQ(2, dependency_group_count);
  }

  // Now make sure that a gc should get rid of the function, even though we
  // still have the allocation site alive.
  for (int i = 0; i < 4; i++) {
    CcTest::CollectAllGarbage();
  }

  // The site still exists because of our global handle, but the code is no
  // longer referred to by dependent_code().
  CHECK(site->dependent_code().object_at(0)->IsCleared());
}

void CheckNumberOfAllocations(Heap* heap, const char* source,
                              int expected_full_alloc,
                              int expected_slim_alloc) {
  int prev_fat_alloc_count = AllocationSitesCount(heap);
  int prev_slim_alloc_count = SlimAllocationSiteCount(heap);

  CompileRun(source);

  int fat_alloc_sites = AllocationSitesCount(heap) - prev_fat_alloc_count;
  int slim_alloc_sites = SlimAllocationSiteCount(heap) - prev_slim_alloc_count;

  CHECK_EQ(expected_full_alloc, fat_alloc_sites);
  CHECK_EQ(expected_slim_alloc, slim_alloc_sites);
}

TEST(AllocationSiteCreation) {
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  i::FLAG_allow_natives_syntax = true;

  // Array literals.
  CheckNumberOfAllocations(heap,
                           "function f1() {"
                           "  return []; "
                           "};"
                           "%EnsureFeedbackVectorForFunction(f1); f1();",
                           1, 0);
  CheckNumberOfAllocations(heap,
                           "function f2() {"
                           "  return [1, 2];"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f2); f2();",
                           1, 0);
  CheckNumberOfAllocations(heap,
                           "function f3() {"
                           "  return [[1], [2]];"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f3); f3();",
                           1, 2);
  CheckNumberOfAllocations(heap,
                           "function f4() { "
                           "return [0, [1, 1.1, 1.2, "
                           "], 1.5, [2.1, 2.2], 3];"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f4); f4();",
                           1, 2);

  // Object literals have lazy AllocationSites
  CheckNumberOfAllocations(heap,
                           "function f5() {"
                           " return {};"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f5); f5();",
                           0, 0);

  // No AllocationSites are created for the empty object literal.
  for (int i = 0; i < 5; i++) {
    CheckNumberOfAllocations(heap, "f5(); ", 0, 0);
  }

  CheckNumberOfAllocations(heap,
                           "function f6() {"
                           "  return {a:1};"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f6); f6();",
                           0, 0);

  CheckNumberOfAllocations(heap, "f6(); ", 1, 0);

  CheckNumberOfAllocations(heap,
                           "function f7() {"
                           "  return {a:1, b:2};"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f7); f7(); ",
                           0, 0);
  CheckNumberOfAllocations(heap, "f7(); ", 1, 0);

  // No Allocation sites are created for object subliterals
  CheckNumberOfAllocations(heap,
                           "function f8() {"
                           "return {a:{}, b:{ a:2, c:{ d:{f:{}}} } }; "
                           "};"
                           "%EnsureFeedbackVectorForFunction(f8); f8();",
                           0, 0);
  CheckNumberOfAllocations(heap, "f8(); ", 1, 0);

  // We currently eagerly create allocation sites if there are sub-arrays.
  // Allocation sites are created only for array subliterals
  CheckNumberOfAllocations(heap,
                           "function f9() {"
                           "return {a:[1, 2, 3], b:{ a:2, c:{ d:{f:[]} } }}; "
                           "};"
                           "%EnsureFeedbackVectorForFunction(f9); f9(); ",
                           1, 2);

  // No new AllocationSites created on the second invocation.
  CheckNumberOfAllocations(heap, "f9(); ", 0, 0);
}

TEST(AllocationSiteCreationForIIFE) {
  // No feedback vectors and hence no allocation sites.
  // TODO(mythria): Once lazy feedback allocation is enabled by default
  // re-evaluate if we need any of these tests.
  if (FLAG_lite_mode || FLAG_lazy_feedback_allocation) return;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  i::FLAG_enable_one_shot_optimization = true;

  // No allocation sites within IIFE/top-level
  CheckNumberOfAllocations(heap,
                           R"(
                            (function f4() {
                              return [ 0, [ 1, 1.1, 1.2,], 1.5, [2.1, 2.2], 3 ];
                            })();
                            )",
                           0, 0);

  CheckNumberOfAllocations(heap,
                           R"(
                            l = [ 1, 2, 3, 4];
                            )",
                           0, 0);

  CheckNumberOfAllocations(heap,
                           R"(
                            a = [];
                            )",
                           0, 0);

  CheckNumberOfAllocations(heap,
                           R"(
                            (function f4() {
                              return [];
                            })();
                            )",
                           0, 0);

  // No allocation sites for literals in an iife/top level code even if it has
  // array subliterals
  CheckNumberOfAllocations(heap,
                           R"(
                            (function f10() {
                              return {a: [1], b: [2]};
                            })();
                            )",
                           0, 0);

  CheckNumberOfAllocations(heap,
                           R"(
                            l = {
                              a: 1,
                              b: {
                                c: [5],
                              }
                            };
                            )",
                           0, 0);

  // Eagerly create allocation sites for literals within a loop of iife or
  // top-level code
  CheckNumberOfAllocations(heap,
                           R"(
                            (function f11() {
                              while(true) {
                                return {a: [1], b: [2]};
                              }
                            })();
                            )",
                           1, 2);

  CheckNumberOfAllocations(heap,
                           R"(
                            for (i = 0; i < 1; ++i) {
                              l = {
                                a: 1,
                                b: {
                                  c: [5],
                                }
                              };
                            }
                            )",
                           1, 1);
}

TEST(CellsInOptimizedCodeAreWeak) {
  if (FLAG_always_opt || !FLAG_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun(
        "bar = (function() {"
        "  function bar() {"
        "    return foo(1);"
        "  };"
        "  %PrepareFunctionForOptimization(bar);"
        "  var foo = function(x) { with (x) { return 1 + x; } };"
        "  %NeverOptimizeFunction(foo);"
        "  bar(foo);"
        "  bar(foo);"
        "  bar(foo);"
        "  %OptimizeFunctionOnNextCall(bar);"
        "  bar(foo);"
        "  return bar;})();");

    Handle<JSFunction> bar = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));
    code = scope.CloseAndEscape(Handle<Code>(bar->code(), isolate));
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    CcTest::CollectAllGarbage();
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}


TEST(ObjectsInOptimizedCodeAreWeak) {
  if (FLAG_always_opt || !FLAG_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun(
        "function bar() {"
        "  return foo(1);"
        "};"
        "%PrepareFunctionForOptimization(bar);"
        "function foo(x) { with (x) { return 1 + x; } };"
        "%NeverOptimizeFunction(foo);"
        "bar();"
        "bar();"
        "bar();"
        "%OptimizeFunctionOnNextCall(bar);"
        "bar();");

    Handle<JSFunction> bar = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));
    code = scope.CloseAndEscape(Handle<Code>(bar->code(), isolate));
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    CcTest::CollectAllGarbage();
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}

TEST(NewSpaceObjectsInOptimizedCode) {
  if (FLAG_always_opt || !FLAG_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(isolate);
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(isolate);

    CompileRun(
        "var foo;"
        "var bar;"
        "(function() {"
        "  function foo_func(x) { with (x) { return 1 + x; } };"
        "  %NeverOptimizeFunction(foo_func);"
        "  function bar_func() {"
        "    return foo(1);"
        "  };"
        "  %PrepareFunctionForOptimization(bar_func);"
        "  bar = bar_func;"
        "  foo = foo_func;"
        "  bar_func();"
        "  bar_func();"
        "  bar_func();"
        "  %OptimizeFunctionOnNextCall(bar_func);"
        "  bar_func();"
        "})();");

    Handle<JSFunction> bar = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));

    Handle<JSFunction> foo = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("foo"))
                                           .ToLocalChecked())));

    CHECK(Heap::InYoungGeneration(*foo));
    CcTest::CollectGarbage(NEW_SPACE);
    CcTest::CollectGarbage(NEW_SPACE);
    CHECK(!Heap::InYoungGeneration(*foo));
#ifdef VERIFY_HEAP
    CcTest::heap()->Verify();
#endif
    CHECK(!bar->code().marked_for_deoptimization());
    code = scope.CloseAndEscape(Handle<Code>(bar->code(), isolate));
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    CcTest::CollectAllGarbage();
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}

TEST(ObjectsInEagerlyDeoptimizedCodeAreWeak) {
  if (FLAG_always_opt || !FLAG_opt) return;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun(
        "function bar() {"
        "  return foo(1);"
        "};"
        "function foo(x) { with (x) { return 1 + x; } };"
        "%NeverOptimizeFunction(foo);"
        "%PrepareFunctionForOptimization(bar);"
        "bar();"
        "bar();"
        "bar();"
        "%OptimizeFunctionOnNextCall(bar);"
        "bar();"
        "%DeoptimizeFunction(bar);");

    Handle<JSFunction> bar = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));
    code = scope.CloseAndEscape(Handle<Code>(bar->code(), isolate));
  }

  CHECK(code->marked_for_deoptimization());

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    CcTest::CollectAllGarbage();
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}

static Handle<JSFunction> OptimizeDummyFunction(v8::Isolate* isolate,
                                                const char* name) {
  EmbeddedVector<char, 256> source;
  SNPrintF(source,
           "function %s() { return 0; }"
           "%%PrepareFunctionForOptimization(%s);"
           "%s(); %s();"
           "%%OptimizeFunctionOnNextCall(%s);"
           "%s();",
           name, name, name, name, name, name);
  CompileRun(source.begin());
  i::Handle<JSFunction> fun = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()
              ->Get(isolate->GetCurrentContext(), v8_str(name))
              .ToLocalChecked())));
  return fun;
}

static int GetCodeChainLength(Code code) {
  int result = 0;
  while (code.next_code_link().IsCode()) {
    result++;
    code = Code::cast(code.next_code_link());
  }
  return result;
}


TEST(NextCodeLinkIsWeak) {
  FLAG_always_opt = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  CcTest::CollectAllAvailableGarbage();
  int code_chain_length_before, code_chain_length_after;
  {
    HandleScope scope(heap->isolate());
    Handle<JSFunction> mortal =
        OptimizeDummyFunction(CcTest::isolate(), "mortal");
    Handle<JSFunction> immortal =
        OptimizeDummyFunction(CcTest::isolate(), "immortal");
    CHECK_EQ(immortal->code().next_code_link(), mortal->code());
    code_chain_length_before = GetCodeChainLength(immortal->code());
    // Keep the immortal code and let the mortal code die.
    code = scope.CloseAndEscape(Handle<Code>(immortal->code(), isolate));
    CompileRun("mortal = null; immortal = null;");
  }
  CcTest::CollectAllAvailableGarbage();
  // Now mortal code should be dead.
  code_chain_length_after = GetCodeChainLength(*code);
  CHECK_EQ(code_chain_length_before - 1, code_chain_length_after);
}

TEST(NextCodeLinkInCodeDataContainerIsCleared) {
  FLAG_always_opt = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<CodeDataContainer> code_data_container;
  {
    HandleScope scope(heap->isolate());
    Handle<JSFunction> mortal1 =
        OptimizeDummyFunction(CcTest::isolate(), "mortal1");
    Handle<JSFunction> mortal2 =
        OptimizeDummyFunction(CcTest::isolate(), "mortal2");
    CHECK_EQ(mortal2->code().next_code_link(), mortal1->code());
    code_data_container = scope.CloseAndEscape(Handle<CodeDataContainer>(
        mortal2->code().code_data_container(), isolate));
    CompileRun("mortal1 = null; mortal2 = null;");
  }
  CcTest::CollectAllAvailableGarbage();
  CHECK(code_data_container->next_code_link().IsUndefined(isolate));
}

static Handle<Code> DummyOptimizedCode(Isolate* isolate) {
  i::byte buffer[i::Assembler::kMinimalBufferSize];
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  CodeDesc desc;
  masm.Push(isolate->factory()->undefined_value());
  masm.Push(isolate->factory()->undefined_value());
  masm.Drop(2);
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, Code::OPTIMIZED_FUNCTION)
          .set_self_reference(masm.CodeObject())
          .Build();
  CHECK(code->IsCode());
  return code;
}


TEST(NextCodeLinkIsWeak2) {
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  CcTest::CollectAllAvailableGarbage();
  Handle<Context> context(Context::cast(heap->native_contexts_list()), isolate);
  Handle<Code> new_head;
  Handle<Object> old_head(context->get(Context::OPTIMIZED_CODE_LIST), isolate);
  {
    HandleScope scope(heap->isolate());
    Handle<Code> immortal = DummyOptimizedCode(isolate);
    Handle<Code> mortal = DummyOptimizedCode(isolate);
    mortal->set_next_code_link(*old_head);
    immortal->set_next_code_link(*mortal);
    context->set(Context::OPTIMIZED_CODE_LIST, *immortal);
    new_head = scope.CloseAndEscape(immortal);
  }
  CcTest::CollectAllAvailableGarbage();
  // Now mortal code should be dead.
  CHECK_EQ(*old_head, new_head->next_code_link());
}


static bool weak_ic_cleared = false;

static void ClearWeakIC(
    const v8::WeakCallbackInfo<v8::Persistent<v8::Object>>& data) {
  printf("clear weak is called\n");
  weak_ic_cleared = true;
  data.GetParameter()->Reset();
}


TEST(WeakFunctionInConstructor) {
  if (FLAG_always_opt) return;
  FLAG_stress_compaction = false;
  FLAG_stress_incremental_marking = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  LocalContext env;
  v8::HandleScope scope(isolate);
  CompileRun(
      "function createObj(obj) {"
      "  return new obj();"
      "}");
  i::Handle<JSFunction> createObj = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()
              ->Get(env.local(), v8_str("createObj"))
              .ToLocalChecked())));

  v8::Persistent<v8::Object> garbage;
  {
    v8::HandleScope scope(isolate);
    const char* source =
        " (function() {"
        "   function hat() { this.x = 5; }"
        "   %EnsureFeedbackVectorForFunction(hat);"
        "   %EnsureFeedbackVectorForFunction(createObj);"
        "   createObj(hat);"
        "   createObj(hat);"
        "   return hat;"
        " })();";
    garbage.Reset(isolate, CompileRun(env.local(), source)
                               .ToLocalChecked()
                               ->ToObject(env.local())
                               .ToLocalChecked());
  }
  weak_ic_cleared = false;
  garbage.SetWeak(&garbage, &ClearWeakIC, v8::WeakCallbackType::kParameter);
  CcTest::CollectAllGarbage();
  CHECK(weak_ic_cleared);

  // We've determined the constructor in createObj has had it's weak cell
  // cleared. Now, verify that one additional call with a new function
  // allows monomorphicity.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(createObj->feedback_vector(), CcTest::i_isolate());
  for (int i = 0; i < 20; i++) {
    MaybeObject slot_value = feedback_vector->Get(FeedbackSlot(0));
    CHECK(slot_value->IsWeakOrCleared());
    if (slot_value->IsCleared()) break;
    CcTest::CollectAllGarbage();
  }

  MaybeObject slot_value = feedback_vector->Get(FeedbackSlot(0));
  CHECK(slot_value->IsCleared());
  CompileRun(
      "function coat() { this.x = 6; }"
      "createObj(coat);");
  slot_value = feedback_vector->Get(FeedbackSlot(0));
  CHECK(slot_value->IsWeak());
}


// Checks that the value returned by execution of the source is weak.
void CheckWeakness(const char* source) {
  FLAG_stress_compaction = false;
  FLAG_stress_incremental_marking = false;
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  LocalContext env;
  v8::HandleScope scope(isolate);
  v8::Persistent<v8::Object> garbage;
  {
    v8::HandleScope scope(isolate);
    garbage.Reset(isolate, CompileRun(env.local(), source)
                               .ToLocalChecked()
                               ->ToObject(env.local())
                               .ToLocalChecked());
  }
  weak_ic_cleared = false;
  garbage.SetWeak(&garbage, &ClearWeakIC, v8::WeakCallbackType::kParameter);
  CcTest::CollectAllGarbage();
  CHECK(weak_ic_cleared);
}


// Each of the following "weak IC" tests creates an IC that embeds a map with
// the prototype pointing to _proto_ and checks that the _proto_ dies on GC.
TEST(WeakMapInMonomorphicLoadIC) {
  CheckWeakness(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
      "%EnsureFeedbackVectorForFunction(loadIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   loadIC(obj);"
      "   loadIC(obj);"
      "   loadIC(obj);"
      "   return proto;"
      " })();");
}


TEST(WeakMapInPolymorphicLoadIC) {
  CheckWeakness(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
      "%EnsureFeedbackVectorForFunction(loadIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   loadIC(obj);"
      "   loadIC(obj);"
      "   loadIC(obj);"
      "   var poly = Object.create(proto);"
      "   poly.x = true;"
      "   loadIC(poly);"
      "   return proto;"
      " })();");
}


TEST(WeakMapInMonomorphicKeyedLoadIC) {
  CheckWeakness(
      "function keyedLoadIC(obj, field) {"
      "  return obj[field];"
      "}"
      "%EnsureFeedbackVectorForFunction(keyedLoadIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   keyedLoadIC(obj, 'name');"
      "   keyedLoadIC(obj, 'name');"
      "   keyedLoadIC(obj, 'name');"
      "   return proto;"
      " })();");
}


TEST(WeakMapInPolymorphicKeyedLoadIC) {
  CheckWeakness(
      "function keyedLoadIC(obj, field) {"
      "  return obj[field];"
      "}"
      "%EnsureFeedbackVectorForFunction(keyedLoadIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   keyedLoadIC(obj, 'name');"
      "   keyedLoadIC(obj, 'name');"
      "   keyedLoadIC(obj, 'name');"
      "   var poly = Object.create(proto);"
      "   poly.x = true;"
      "   keyedLoadIC(poly, 'name');"
      "   return proto;"
      " })();");
}


TEST(WeakMapInMonomorphicStoreIC) {
  CheckWeakness(
      "function storeIC(obj, value) {"
      "  obj.name = value;"
      "}"
      "%EnsureFeedbackVectorForFunction(storeIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   storeIC(obj, 'x');"
      "   storeIC(obj, 'x');"
      "   storeIC(obj, 'x');"
      "   return proto;"
      " })();");
}


TEST(WeakMapInPolymorphicStoreIC) {
  CheckWeakness(
      "function storeIC(obj, value) {"
      "  obj.name = value;"
      "}"
      "%EnsureFeedbackVectorForFunction(storeIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   storeIC(obj, 'x');"
      "   storeIC(obj, 'x');"
      "   storeIC(obj, 'x');"
      "   var poly = Object.create(proto);"
      "   poly.x = true;"
      "   storeIC(poly, 'x');"
      "   return proto;"
      " })();");
}


TEST(WeakMapInMonomorphicKeyedStoreIC) {
  CheckWeakness(
      "function keyedStoreIC(obj, field, value) {"
      "  obj[field] = value;"
      "}"
      "%EnsureFeedbackVectorForFunction(keyedStoreIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   keyedStoreIC(obj, 'x');"
      "   keyedStoreIC(obj, 'x');"
      "   keyedStoreIC(obj, 'x');"
      "   return proto;"
      " })();");
}


TEST(WeakMapInPolymorphicKeyedStoreIC) {
  CheckWeakness(
      "function keyedStoreIC(obj, field, value) {"
      "  obj[field] = value;"
      "}"
      "%EnsureFeedbackVectorForFunction(keyedStoreIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   keyedStoreIC(obj, 'x');"
      "   keyedStoreIC(obj, 'x');"
      "   keyedStoreIC(obj, 'x');"
      "   var poly = Object.create(proto);"
      "   poly.x = true;"
      "   keyedStoreIC(poly, 'x');"
      "   return proto;"
      " })();");
}


TEST(WeakMapInMonomorphicCompareNilIC) {
  FLAG_allow_natives_syntax = true;
  CheckWeakness(
      "function compareNilIC(obj) {"
      "  return obj == null;"
      "}"
      "%EnsureFeedbackVectorForFunction(compareNilIC);"
      " (function() {"
      "   var proto = {'name' : 'weak'};"
      "   var obj = Object.create(proto);"
      "   compareNilIC(obj);"
      "   compareNilIC(obj);"
      "   compareNilIC(obj);"
      "   return proto;"
      " })();");
}


Handle<JSFunction> GetFunctionByName(Isolate* isolate, const char* name) {
  Handle<String> str = isolate->factory()->InternalizeUtf8String(name);
  Handle<Object> obj =
      Object::GetProperty(isolate, isolate->global_object(), str)
          .ToHandleChecked();
  return Handle<JSFunction>::cast(obj);
}

void CheckIC(Handle<JSFunction> function, int slot_index,
             InlineCacheState state) {
  FeedbackVector vector = function->feedback_vector();
  FeedbackSlot slot(slot_index);
  FeedbackNexus nexus(vector, slot);
  CHECK_EQ(nexus.ic_state(), state);
}

TEST(MonomorphicStaysMonomorphicAfterGC) {
  if (!FLAG_use_ic) return;
  if (FLAG_always_opt) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  FLAG_allow_natives_syntax = true;
  CompileRun(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
      "%EnsureFeedbackVectorForFunction(loadIC);"
      "function testIC() {"
      "  var proto = {'name' : 'weak'};"
      "  var obj = Object.create(proto);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  return proto;"
      "};");
  Handle<JSFunction> loadIC = GetFunctionByName(isolate, "loadIC");
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CcTest::CollectAllGarbage();
  CheckIC(loadIC, 0, MONOMORPHIC);
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CheckIC(loadIC, 0, MONOMORPHIC);
}


TEST(PolymorphicStaysPolymorphicAfterGC) {
  if (!FLAG_use_ic) return;
  if (FLAG_always_opt) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  FLAG_allow_natives_syntax = true;
  CompileRun(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
      "%EnsureFeedbackVectorForFunction(loadIC);"
      "function testIC() {"
      "  var proto = {'name' : 'weak'};"
      "  var obj = Object.create(proto);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  var poly = Object.create(proto);"
      "  poly.x = true;"
      "  loadIC(poly);"
      "  return proto;"
      "};");
  Handle<JSFunction> loadIC = GetFunctionByName(isolate, "loadIC");
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CcTest::CollectAllGarbage();
  CheckIC(loadIC, 0, POLYMORPHIC);
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CheckIC(loadIC, 0, POLYMORPHIC);
}

#ifdef DEBUG
TEST(AddInstructionChangesNewSpacePromotion) {
  FLAG_allow_natives_syntax = true;
  FLAG_expose_gc = true;
  FLAG_stress_compaction = true;
  FLAG_gc_interval = 1000;
  CcTest::InitializeVM();
  if (!FLAG_allocation_site_pretenuring) return;
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  LocalContext env;
  CompileRun(
      "function add(a, b) {"
      "  return a + b;"
      "}"
      "add(1, 2);"
      "add(\"a\", \"b\");"
      "var oldSpaceObject;"
      "gc();"
      "function crash(x) {"
      "  var object = {a: null, b: null};"
      "  var result = add(1.5, x | 0);"
      "  object.a = result;"
      "  oldSpaceObject = object;"
      "  return object;"
      "}"
      "%PrepareFunctionForOptimization(crash);"
      "crash(1);"
      "crash(1);"
      "%OptimizeFunctionOnNextCall(crash);"
      "crash(1);");

  v8::Local<v8::Object> global = CcTest::global();
  v8::Local<v8::Function> g = v8::Local<v8::Function>::Cast(
      global->Get(env.local(), v8_str("crash")).ToLocalChecked());
  v8::Local<v8::Value> args1[] = {v8_num(1)};
  heap->DisableInlineAllocation();
  heap->set_allocation_timeout(1);
  g->Call(env.local(), global, 1, args1).ToLocalChecked();
  CcTest::CollectAllGarbage();
}


void OnFatalErrorExpectOOM(const char* location, const char* message) {
  // Exit with 0 if the location matches our expectation.
  exit(strcmp(location, "CALL_AND_RETRY_LAST"));
}


TEST(CEntryStubOOM) {
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CcTest::isolate()->SetFatalErrorHandler(OnFatalErrorExpectOOM);

  v8::Local<v8::Value> result = CompileRun(
      "%SetAllocationTimeout(1, 1);"
      "var a = [];"
      "a.__proto__ = [];"
      "a.unshift(1)");

  CHECK(result->IsNumber());
}

#endif  // DEBUG


static void InterruptCallback357137(v8::Isolate* isolate, void* data) { }


static void RequestInterrupt(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CcTest::isolate()->RequestInterrupt(&InterruptCallback357137, nullptr);
}

HEAP_TEST(Regress538257) {
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  v8::Isolate::CreateParams create_params;
  // Set heap limits.
  create_params.constraints.set_max_young_generation_size_in_bytes(3 * MB);
#ifdef DEBUG
  create_params.constraints.set_max_old_generation_size_in_bytes(20 * MB);
#else
  create_params.constraints.set_max_old_generation_size_in_bytes(6 * MB);
#endif
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  isolate->Enter();
  {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    Heap* heap = i_isolate->heap();
    HandleScope handle_scope(i_isolate);
    PagedSpace* old_space = heap->old_space();
    const int kMaxObjects = 10000;
    const int kFixedArrayLen = 512;
    Handle<FixedArray> objects[kMaxObjects];
    for (int i = 0; (i < kMaxObjects) &&
                    heap->CanExpandOldGeneration(old_space->AreaSize());
         i++) {
      objects[i] = i_isolate->factory()->NewFixedArray(kFixedArrayLen,
                                                       AllocationType::kOld);
      heap::ForceEvacuationCandidate(Page::FromHeapObject(*objects[i]));
    }
    heap::SimulateFullSpace(old_space);
    CcTest::CollectAllGarbage();
    // If we get this far, we've successfully aborted compaction. Any further
    // allocations might trigger OOM.
  }
  isolate->Exit();
  isolate->Dispose();
}


TEST(Regress357137) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope hscope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(
      v8::String::NewFromUtf8(isolate, "interrupt", v8::NewStringType::kNormal)
          .ToLocalChecked(),
      v8::FunctionTemplate::New(isolate, RequestInterrupt));
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  CHECK(!context.IsEmpty());
  v8::Context::Scope cscope(context);

  v8::Local<v8::Value> result = CompileRun(
      "var locals = '';"
      "for (var i = 0; i < 512; i++) locals += 'var v' + i + '= 42;';"
      "eval('function f() {' + locals + 'return function() { return v0; }; }');"
      "interrupt();"  // This triggers a fake stack overflow in f.
      "f()()");
  CHECK_EQ(42.0, result->ToNumber(context).ToLocalChecked()->Value());
}


TEST(Regress507979) {
  const int kFixedArrayLen = 10;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  Handle<FixedArray> o1 = isolate->factory()->NewFixedArray(kFixedArrayLen);
  Handle<FixedArray> o2 = isolate->factory()->NewFixedArray(kFixedArrayLen);
  CHECK(Heap::InYoungGeneration(*o1));
  CHECK(Heap::InYoungGeneration(*o2));

  HeapObjectIterator it(isolate->heap(),
                        i::HeapObjectIterator::kFilterUnreachable);

  // Replace parts of an object placed before a live object with a filler. This
  // way the filler object shares the mark bits with the following live object.
  o1->Shrink(isolate, kFixedArrayLen - 1);

  for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    // Let's not optimize the loop away.
    CHECK_NE(obj.address(), kNullAddress);
  }
}

TEST(Regress388880) {
  if (!FLAG_incremental_marking) return;
  FLAG_stress_incremental_marking = false;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  Handle<Map> map1 = Map::Create(isolate, 1);
  Handle<String> name = factory->NewStringFromStaticChars("foo");
  name = factory->InternalizeString(name);
  Handle<Map> map2 =
      Map::CopyWithField(isolate, map1, name, FieldType::Any(isolate), NONE,
                         PropertyConstness::kMutable, Representation::Tagged(),
                         OMIT_TRANSITION)
          .ToHandleChecked();

  size_t desired_offset = Page::kPageSize - map1->instance_size();

  // Allocate padding objects in old pointer space so, that object allocated
  // afterwards would end at the end of the page.
  heap::SimulateFullSpace(heap->old_space());
  size_t padding_size =
      desired_offset - MemoryChunkLayout::ObjectStartOffsetInDataPage();
  heap::CreatePadding(heap, static_cast<int>(padding_size),
                      AllocationType::kOld);

  Handle<JSObject> o = factory->NewJSObjectFromMap(map1, AllocationType::kOld);
  o->set_raw_properties_or_hash(*factory->empty_fixed_array());

  // Ensure that the object allocated where we need it.
  Page* page = Page::FromHeapObject(*o);
  CHECK_EQ(desired_offset, page->Offset(o->address()));

  // Now we have an object right at the end of the page.

  // Enable incremental marking to trigger actions in Heap::AdjustLiveBytes()
  // that would cause crash.
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  CcTest::heap()->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                          i::GarbageCollectionReason::kTesting);
  CHECK(marking->IsMarking());

  // Now everything is set up for crashing in JSObject::MigrateFastToFast()
  // when it calls heap->AdjustLiveBytes(...).
  JSObject::MigrateToMap(isolate, o, map2);
}


TEST(Regress3631) {
  if (!FLAG_incremental_marking) return;
  FLAG_expose_gc = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  v8::Local<v8::Value> result = CompileRun(
      "var weak_map = new WeakMap();"
      "var future_keys = [];"
      "for (var i = 0; i < 50; i++) {"
      "  var key = {'k' : i + 0.1};"
      "  weak_map.set(key, 1);"
      "  future_keys.push({'x' : i + 0.2});"
      "}"
      "weak_map");
  if (marking->IsStopped()) {
    CcTest::heap()->StartIncrementalMarking(
        i::Heap::kNoGCFlags, i::GarbageCollectionReason::kTesting);
  }
  // Incrementally mark the backing store.
  Handle<JSReceiver> obj =
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(result));
  Handle<JSWeakCollection> weak_map(JSWeakCollection::cast(*obj), isolate);
  SimulateIncrementalMarking(heap);
  // Stash the backing store in a handle.
  Handle<Object> save(weak_map->table(), isolate);
  // The following line will update the backing store.
  CompileRun(
      "for (var i = 0; i < 50; i++) {"
      "  weak_map.set(future_keys[i], i);"
      "}");
  heap->incremental_marking()->set_should_hurry(true);
  CcTest::CollectGarbage(OLD_SPACE);
}


TEST(Regress442710) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);
  Handle<JSGlobalObject> global(CcTest::i_isolate()->context().global_object(),
                                isolate);
  Handle<JSArray> array = factory->NewJSArray(2);

  Handle<String> name = factory->InternalizeUtf8String("testArray");
  Object::SetProperty(isolate, global, name, array).Check();
  CompileRun("testArray[0] = 1; testArray[1] = 2; testArray.shift();");
  CcTest::CollectGarbage(OLD_SPACE);
}


HEAP_TEST(NumberStringCacheSize) {
  // Test that the number-string cache has not been resized in the snapshot.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  if (!isolate->snapshot_available()) return;
  Heap* heap = isolate->heap();
  CHECK_EQ(Heap::kInitialNumberStringCacheSize * 2,
           heap->number_string_cache().length());
}


TEST(Regress3877) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  CompileRun("function cls() { this.x = 10; }");
  Handle<WeakFixedArray> weak_prototype_holder = factory->NewWeakFixedArray(1);
  {
    HandleScope inner_scope(isolate);
    v8::Local<v8::Value> result = CompileRun("cls.prototype");
    Handle<JSReceiver> proto =
        v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(result));
    weak_prototype_holder->Set(0, HeapObjectReference::Weak(*proto));
  }
  CHECK(!weak_prototype_holder->Get(0)->IsCleared());
  CompileRun(
      "var a = { };"
      "a.x = new cls();"
      "cls.prototype = null;");
  for (int i = 0; i < 4; i++) {
    CcTest::CollectAllGarbage();
  }
  // The map of a.x keeps prototype alive
  CHECK(!weak_prototype_holder->Get(0)->IsCleared());
  // Change the map of a.x and make the previous map garbage collectable.
  CompileRun("a.x.__proto__ = {};");
  for (int i = 0; i < 4; i++) {
    CcTest::CollectAllGarbage();
  }
  CHECK(weak_prototype_holder->Get(0)->IsCleared());
}

Handle<WeakFixedArray> AddRetainedMap(Isolate* isolate, Heap* heap) {
  HandleScope inner_scope(isolate);
  Handle<Map> map = Map::Create(isolate, 1);
  v8::Local<v8::Value> result =
      CompileRun("(function () { return {x : 10}; })();");
  Handle<JSReceiver> proto =
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(result));
  Map::SetPrototype(isolate, map, proto);
  heap->AddRetainedMap(map);
  Handle<WeakFixedArray> array = isolate->factory()->NewWeakFixedArray(1);
  array->Set(0, HeapObjectReference::Weak(*map));
  return inner_scope.CloseAndEscape(array);
}


void CheckMapRetainingFor(int n) {
  FLAG_retain_maps_for_n_gc = n;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Handle<WeakFixedArray> array_with_map = AddRetainedMap(isolate, heap);
  CHECK(array_with_map->Get(0)->IsWeak());
  for (int i = 0; i < n; i++) {
    heap::SimulateIncrementalMarking(heap);
    CcTest::CollectGarbage(OLD_SPACE);
  }
  CHECK(array_with_map->Get(0)->IsWeak());
  heap::SimulateIncrementalMarking(heap);
  CcTest::CollectGarbage(OLD_SPACE);
  CHECK(array_with_map->Get(0)->IsCleared());
}


TEST(MapRetaining) {
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CheckMapRetainingFor(FLAG_retain_maps_for_n_gc);
  CheckMapRetainingFor(0);
  CheckMapRetainingFor(1);
  CheckMapRetainingFor(7);
}

TEST(PreprocessStackTrace) {
  // Do not automatically trigger early GC.
  FLAG_gc_interval = -1;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::TryCatch try_catch(CcTest::isolate());
  CompileRun("throw new Error();");
  CHECK(try_catch.HasCaught());
  Isolate* isolate = CcTest::i_isolate();
  Handle<Object> exception = v8::Utils::OpenHandle(*try_catch.Exception());
  Handle<Name> key = isolate->factory()->stack_trace_symbol();
  Handle<Object> stack_trace =
      Object::GetProperty(isolate, exception, key).ToHandleChecked();
  Handle<Object> code =
      Object::GetElement(isolate, stack_trace, 3).ToHandleChecked();
  CHECK(code->IsCode());

  CcTest::CollectAllAvailableGarbage();

  Handle<Object> pos =
      Object::GetElement(isolate, stack_trace, 3).ToHandleChecked();
  CHECK(pos->IsSmi());

  Handle<FrameArray> frame_array = Handle<FrameArray>::cast(stack_trace);
  int array_length = frame_array->FrameCount();
  for (int i = 0; i < array_length; i++) {
    Handle<Object> element =
        Object::GetElement(isolate, stack_trace, i).ToHandleChecked();
    CHECK(!element->IsCode());
  }
}


void AllocateInSpace(Isolate* isolate, size_t bytes, AllocationSpace space) {
  CHECK_LE(FixedArray::kHeaderSize, bytes);
  CHECK(IsAligned(bytes, kTaggedSize));
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  AlwaysAllocateScope always_allocate(isolate);
  int elements =
      static_cast<int>((bytes - FixedArray::kHeaderSize) / kTaggedSize);
  Handle<FixedArray> array = factory->NewFixedArray(
      elements,
      space == NEW_SPACE ? AllocationType::kYoung : AllocationType::kOld);
  CHECK((space == NEW_SPACE) == Heap::InYoungGeneration(*array));
  CHECK_EQ(bytes, static_cast<size_t>(array->Size()));
}


TEST(NewSpaceAllocationCounter) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  size_t counter1 = heap->NewSpaceAllocationCounter();
  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);  // Ensure new space is empty.
  const size_t kSize = 1024;
  AllocateInSpace(isolate, kSize, NEW_SPACE);
  size_t counter2 = heap->NewSpaceAllocationCounter();
  CHECK_EQ(kSize, counter2 - counter1);
  CcTest::CollectGarbage(NEW_SPACE);
  size_t counter3 = heap->NewSpaceAllocationCounter();
  CHECK_EQ(0U, counter3 - counter2);
  // Test counter overflow.
  size_t max_counter = static_cast<size_t>(-1);
  heap->set_new_space_allocation_counter(max_counter - 10 * kSize);
  size_t start = heap->NewSpaceAllocationCounter();
  for (int i = 0; i < 20; i++) {
    AllocateInSpace(isolate, kSize, NEW_SPACE);
    size_t counter = heap->NewSpaceAllocationCounter();
    CHECK_EQ(kSize, counter - start);
    start = counter;
  }
}


TEST(OldSpaceAllocationCounter) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  size_t counter1 = heap->OldGenerationAllocationCounter();
  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  const size_t kSize = 1024;
  AllocateInSpace(isolate, kSize, OLD_SPACE);
  size_t counter2 = heap->OldGenerationAllocationCounter();
  // TODO(ulan): replace all CHECK_LE with CHECK_EQ after v8:4148 is fixed.
  CHECK_LE(kSize, counter2 - counter1);
  CcTest::CollectGarbage(NEW_SPACE);
  size_t counter3 = heap->OldGenerationAllocationCounter();
  CHECK_EQ(0u, counter3 - counter2);
  AllocateInSpace(isolate, kSize, OLD_SPACE);
  CcTest::CollectGarbage(OLD_SPACE);
  size_t counter4 = heap->OldGenerationAllocationCounter();
  CHECK_LE(kSize, counter4 - counter3);
  // Test counter overflow.
  size_t max_counter = static_cast<size_t>(-1);
  heap->set_old_generation_allocation_counter_at_last_gc(max_counter -
                                                         10 * kSize);
  size_t start = heap->OldGenerationAllocationCounter();
  for (int i = 0; i < 20; i++) {
    AllocateInSpace(isolate, kSize, OLD_SPACE);
    size_t counter = heap->OldGenerationAllocationCounter();
    CHECK_LE(kSize, counter - start);
    start = counter;
  }
}


static void CheckLeak(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = CcTest::i_isolate();
  Object message(
      *reinterpret_cast<Address*>(isolate->pending_message_obj_address()));
  CHECK(message.IsTheHole(isolate));
}


TEST(MessageObjectLeak) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(
      v8::String::NewFromUtf8(isolate, "check", v8::NewStringType::kNormal)
          .ToLocalChecked(),
      v8::FunctionTemplate::New(isolate, CheckLeak));
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope cscope(context);

  const char* test =
      "try {"
      "  throw 'message 1';"
      "} catch (e) {"
      "}"
      "check();"
      "L: try {"
      "  throw 'message 2';"
      "} finally {"
      "  break L;"
      "}"
      "check();";
  CompileRun(test);

  const char* flag = "--turbo-filter=*";
  FlagList::SetFlagsFromString(flag, strlen(flag));
  FLAG_always_opt = true;

  CompileRun(test);
}


static void CheckEqualSharedFunctionInfos(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Handle<Object> obj1 = v8::Utils::OpenHandle(*args[0]);
  Handle<Object> obj2 = v8::Utils::OpenHandle(*args[1]);
  Handle<JSFunction> fun1 = Handle<JSFunction>::cast(obj1);
  Handle<JSFunction> fun2 = Handle<JSFunction>::cast(obj2);
  CHECK(fun1->shared() == fun2->shared());
}


static void RemoveCodeAndGC(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<Object> obj = v8::Utils::OpenHandle(*args[0]);
  Handle<JSFunction> fun = Handle<JSFunction>::cast(obj);
  // Bytecode is code too.
  SharedFunctionInfo::DiscardCompiled(isolate, handle(fun->shared(), isolate));
  fun->set_code(*BUILTIN_CODE(isolate, CompileLazy));
  CcTest::CollectAllAvailableGarbage();
}


TEST(CanonicalSharedFunctionInfo) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(isolate, "check", v8::FunctionTemplate::New(
                                    isolate, CheckEqualSharedFunctionInfos));
  global->Set(isolate, "remove",
              v8::FunctionTemplate::New(isolate, RemoveCodeAndGC));
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope cscope(context);
  CompileRun(
      "function f() { return function g() {}; }"
      "var g1 = f();"
      "remove(f);"
      "var g2 = f();"
      "check(g1, g2);");

  CompileRun(
      "function f() { return (function() { return function g() {}; })(); }"
      "var g1 = f();"
      "remove(f);"
      "var g2 = f();"
      "check(g1, g2);");
}


TEST(ScriptIterator) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  LocalContext context;

  CcTest::CollectAllGarbage();

  int script_count = 0;
  {
    HeapObjectIterator it(heap);
    for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
      if (obj.IsScript()) script_count++;
    }
  }

  {
    Script::Iterator iterator(isolate);
    for (Script script = iterator.Next(); !script.is_null();
         script = iterator.Next()) {
      script_count--;
    }
  }

  CHECK_EQ(0, script_count);
}


TEST(SharedFunctionInfoIterator) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  LocalContext context;

  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();

  int sfi_count = 0;
  {
    HeapObjectIterator it(heap);
    for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
      if (!obj.IsSharedFunctionInfo()) continue;
      sfi_count++;
    }
  }

  {
    SharedFunctionInfo::GlobalIterator iterator(isolate);
    while (!iterator.Next().is_null()) sfi_count--;
  }

  CHECK_EQ(0, sfi_count);
}

// This is the same as Factory::NewByteArray, except it doesn't retry on
// allocation failure.
AllocationResult HeapTester::AllocateByteArrayForTest(
    Heap* heap, int length, AllocationType allocation_type) {
  DCHECK(length >= 0 && length <= ByteArray::kMaxLength);
  int size = ByteArray::SizeFor(length);
  HeapObject result;
  {
    AllocationResult allocation = heap->AllocateRaw(size, allocation_type);
    if (!allocation.To(&result)) return allocation;
  }

  result.set_map_after_allocation(ReadOnlyRoots(heap).byte_array_map(),
                                  SKIP_WRITE_BARRIER);
  ByteArray::cast(result).set_length(length);
  ByteArray::cast(result).clear_padding();
  return result;
}

HEAP_TEST(Regress587004) {
  ManualGCScope manual_gc_scope;
#ifdef VERIFY_HEAP
  FLAG_verify_heap = false;
#endif
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  const int N =
      (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize) / kTaggedSize;
  Handle<FixedArray> array = factory->NewFixedArray(N, AllocationType::kOld);
  CHECK(heap->old_space()->Contains(*array));
  Handle<Object> number = factory->NewHeapNumber(1.0);
  CHECK(Heap::InYoungGeneration(*number));
  for (int i = 0; i < N; i++) {
    array->set(i, *number);
  }
  CcTest::CollectGarbage(OLD_SPACE);
  heap::SimulateFullSpace(heap->old_space());
  heap->RightTrimFixedArray(*array, N - 1);
  heap->mark_compact_collector()->EnsureSweepingCompleted();
  ByteArray byte_array;
  const int M = 256;
  // Don't allow old space expansion. The test works without this flag too,
  // but becomes very slow.
  heap->set_force_oom(true);
  while (
      AllocateByteArrayForTest(heap, M, AllocationType::kOld).To(&byte_array)) {
    for (int j = 0; j < M; j++) {
      byte_array.set(j, 0x31);
    }
  }
  // Re-enable old space expansion to avoid OOM crash.
  heap->set_force_oom(false);
  CcTest::CollectGarbage(NEW_SPACE);
}

HEAP_TEST(Regress589413) {
  if (!FLAG_incremental_marking) return;
  FLAG_stress_compaction = true;
  FLAG_manual_evacuation_candidates_selection = true;
  FLAG_parallel_compaction = false;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  // Get the heap in clean state.
  CcTest::CollectGarbage(OLD_SPACE);
  CcTest::CollectGarbage(OLD_SPACE);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  // Fill the new space with byte arrays with elements looking like pointers.
  const int M = 256;
  ByteArray byte_array;
  while (AllocateByteArrayForTest(heap, M, AllocationType::kYoung)
             .To(&byte_array)) {
    for (int j = 0; j < M; j++) {
      byte_array.set(j, 0x31);
    }
    // Add the array in root set.
    handle(byte_array, isolate);
  }
  // Make sure the byte arrays will be promoted on the next GC.
  CcTest::CollectGarbage(NEW_SPACE);
  // This number is close to large free list category threshold.
  const int N = 0x3EEE;
  {
    std::vector<FixedArray> arrays;
    std::set<Page*> pages;
    FixedArray array;
    // Fill all pages with fixed arrays.
    heap->set_force_oom(true);
    while (
        AllocateFixedArrayForTest(heap, N, AllocationType::kOld).To(&array)) {
      arrays.push_back(array);
      pages.insert(Page::FromHeapObject(array));
      // Add the array in root set.
      handle(array, isolate);
    }
    // Expand and full one complete page with fixed arrays.
    heap->set_force_oom(false);
    while (
        AllocateFixedArrayForTest(heap, N, AllocationType::kOld).To(&array)) {
      arrays.push_back(array);
      pages.insert(Page::FromHeapObject(array));
      // Add the array in root set.
      handle(array, isolate);
      // Do not expand anymore.
      heap->set_force_oom(true);
    }
    // Expand and mark the new page as evacuation candidate.
    heap->set_force_oom(false);
    {
      AlwaysAllocateScope always_allocate(isolate);
      Handle<HeapObject> ec_obj =
          factory->NewFixedArray(5000, AllocationType::kOld);
      Page* ec_page = Page::FromHeapObject(*ec_obj);
      heap::ForceEvacuationCandidate(ec_page);
      // Make all arrays point to evacuation candidate so that
      // slots are recorded for them.
      for (size_t j = 0; j < arrays.size(); j++) {
        array = arrays[j];
        for (int i = 0; i < N; i++) {
          array.set(i, *ec_obj);
        }
      }
    }
    heap::SimulateIncrementalMarking(heap);
    for (size_t j = 0; j < arrays.size(); j++) {
      heap->RightTrimFixedArray(arrays[j], N - 1);
    }
  }
  // Force allocation from the free list.
  heap->set_force_oom(true);
  CcTest::CollectGarbage(OLD_SPACE);
}

TEST(Regress598319) {
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  // This test ensures that no white objects can cross the progress bar of large
  // objects during incremental marking. It checks this by using Shift() during
  // incremental marking.
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();

  // The size of the array should be larger than kProgressBarScanningChunk.
  const int kNumberOfObjects = Max(FixedArray::kMaxRegularLength + 1, 128 * KB);

  struct Arr {
    Arr(Isolate* isolate, int number_of_objects) {
      root = isolate->factory()->NewFixedArray(1, AllocationType::kOld);
      {
        // Temporary scope to avoid getting any other objects into the root set.
        v8::HandleScope scope(CcTest::isolate());
        Handle<FixedArray> tmp = isolate->factory()->NewFixedArray(
            number_of_objects, AllocationType::kOld);
        root->set(0, *tmp);
        for (int i = 0; i < get().length(); i++) {
          tmp = isolate->factory()->NewFixedArray(100, AllocationType::kOld);
          get().set(i, *tmp);
        }
      }
    }

    FixedArray get() { return FixedArray::cast(root->get(0)); }

    Handle<FixedArray> root;
  } arr(isolate, kNumberOfObjects);

  CHECK_EQ(arr.get().length(), kNumberOfObjects);
  CHECK(heap->lo_space()->Contains(arr.get()));
  LargePage* page = LargePage::FromHeapObject(arr.get());
  CHECK_NOT_NULL(page);

  // GC to cleanup state
  CcTest::CollectGarbage(OLD_SPACE);
  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }

  CHECK(heap->lo_space()->Contains(arr.get()));
  IncrementalMarking* marking = heap->incremental_marking();
  IncrementalMarking::MarkingState* marking_state = marking->marking_state();
  CHECK(marking_state->IsWhite(arr.get()));
  for (int i = 0; i < arr.get().length(); i++) {
    HeapObject arr_value = HeapObject::cast(arr.get().get(i));
    CHECK(marking_state->IsWhite(arr_value));
  }

  // Start incremental marking.
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());

  // Check that we have not marked the interesting array during root scanning.
  for (int i = 0; i < arr.get().length(); i++) {
    HeapObject arr_value = HeapObject::cast(arr.get().get(i));
    CHECK(marking_state->IsWhite(arr_value));
  }

  // Now we search for a state where we are in incremental marking and have
  // only partially marked the large object.
  const double kSmallStepSizeInMs = 0.1;
  while (!marking->IsComplete()) {
    marking->V8Step(kSmallStepSizeInMs,
                    i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    StepOrigin::kV8);
    if (page->IsFlagSet(Page::HAS_PROGRESS_BAR) && page->ProgressBar() > 0) {
      CHECK_NE(page->ProgressBar(), arr.get().Size());
      {
        // Shift by 1, effectively moving one white object across the progress
        // bar, meaning that we will miss marking it.
        v8::HandleScope scope(CcTest::isolate());
        Handle<JSArray> js_array = isolate->factory()->NewJSArrayWithElements(
            Handle<FixedArray>(arr.get(), isolate));
        js_array->GetElementsAccessor()->Shift(js_array);
      }
      break;
    }
  }

  // Finish marking with bigger steps to speed up test.
  const double kLargeStepSizeInMs = 1000;
  while (!marking->IsComplete()) {
    marking->V8Step(kLargeStepSizeInMs,
                    i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    StepOrigin::kV8);
    if (marking->IsReadyToOverApproximateWeakClosure()) {
      marking->FinalizeIncrementally();
    }
  }
  CHECK(marking->IsComplete());

  // All objects need to be black after marking. If a white object crossed the
  // progress bar, we would fail here.
  for (int i = 0; i < arr.get().length(); i++) {
    HeapObject arr_value = HeapObject::cast(arr.get().get(i));
    CHECK(marking_state->IsBlack(arr_value));
  }
}

Handle<FixedArray> ShrinkArrayAndCheckSize(Heap* heap, int length) {
  // Make sure there is no garbage and the compilation cache is empty.
  for (int i = 0; i < 5; i++) {
    CcTest::CollectAllGarbage();
  }
  heap->mark_compact_collector()->EnsureSweepingCompleted();
  size_t size_before_allocation = heap->SizeOfObjects();
  Handle<FixedArray> array =
      heap->isolate()->factory()->NewFixedArray(length, AllocationType::kOld);
  size_t size_after_allocation = heap->SizeOfObjects();
  CHECK_EQ(size_after_allocation, size_before_allocation + array->Size());
  array->Shrink(heap->isolate(), 1);
  size_t size_after_shrinking = heap->SizeOfObjects();
  // Shrinking does not change the space size immediately.
  CHECK_EQ(size_after_allocation, size_after_shrinking);
  // GC and sweeping updates the size to acccount for shrinking.
  CcTest::CollectAllGarbage();
  heap->mark_compact_collector()->EnsureSweepingCompleted();
  intptr_t size_after_gc = heap->SizeOfObjects();
  CHECK_EQ(size_after_gc, size_before_allocation + array->Size());
  return array;
}

TEST(Regress609761) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  int length = kMaxRegularHeapObjectSize / kTaggedSize + 1;
  Handle<FixedArray> array = ShrinkArrayAndCheckSize(heap, length);
  CHECK(heap->lo_space()->Contains(*array));
}

TEST(LiveBytes) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Handle<FixedArray> array = ShrinkArrayAndCheckSize(heap, 2000);
  CHECK(heap->old_space()->Contains(*array));
}

TEST(Regress615489) {
  if (!FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  CcTest::CollectAllGarbage();

  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  marking->StartBlackAllocationForTesting();
  {
    AlwaysAllocateScope always_allocate(CcTest::i_isolate());
    v8::HandleScope inner(CcTest::isolate());
    isolate->factory()->NewFixedArray(500, AllocationType::kOld)->Size();
  }
  const double kStepSizeInMs = 100;
  while (!marking->IsComplete()) {
    marking->V8Step(kStepSizeInMs, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    StepOrigin::kV8);
    if (marking->IsReadyToOverApproximateWeakClosure()) {
      marking->FinalizeIncrementally();
    }
  }
  CHECK(marking->IsComplete());
  intptr_t size_before = heap->SizeOfObjects();
  CcTest::CollectAllGarbage();
  intptr_t size_after = heap->SizeOfObjects();
  // Live size does not increase after garbage collection.
  CHECK_LE(size_after, size_before);
}

class StaticOneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit StaticOneByteResource(const char* data) : data_(data) {}

  ~StaticOneByteResource() override = default;

  const char* data() const override { return data_; }

  size_t length() const override { return strlen(data_); }

 private:
  const char* data_;
};

TEST(Regress631969) {
  if (!FLAG_incremental_marking) return;
  FLAG_manual_evacuation_candidates_selection = true;
  FLAG_parallel_compaction = false;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  // Get the heap in clean state.
  CcTest::CollectGarbage(OLD_SPACE);
  CcTest::CollectGarbage(OLD_SPACE);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  // Allocate two strings in a fresh page and mark the page as evacuation
  // candidate.
  heap::SimulateFullSpace(heap->old_space());
  Handle<String> s1 =
      factory->NewStringFromStaticChars("123456789", AllocationType::kOld);
  Handle<String> s2 =
      factory->NewStringFromStaticChars("01234", AllocationType::kOld);
  heap::ForceEvacuationCandidate(Page::FromHeapObject(*s1));

  heap::SimulateIncrementalMarking(heap, false);

  // Allocate a cons string and promote it to a fresh page in the old space.
  heap::SimulateFullSpace(heap->old_space());
  Handle<String> s3;
  factory->NewConsString(s1, s2).ToHandle(&s3);
  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);

  // Finish incremental marking.
  const double kStepSizeInMs = 100;
  IncrementalMarking* marking = heap->incremental_marking();
  while (!marking->IsComplete()) {
    marking->V8Step(kStepSizeInMs, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    StepOrigin::kV8);
    if (marking->IsReadyToOverApproximateWeakClosure()) {
      marking->FinalizeIncrementally();
    }
  }

  {
    StaticOneByteResource external_string("12345678901234");
    s3->MakeExternal(&external_string);
    CcTest::CollectGarbage(OLD_SPACE);
    // This avoids the GC from trying to free stack allocated resources.
    i::Handle<i::ExternalOneByteString>::cast(s3)->SetResource(isolate,
                                                               nullptr);
  }
}

TEST(LeftTrimFixedArrayInBlackArea) {
  if (!FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  CcTest::CollectAllGarbage();

  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  marking->StartBlackAllocationForTesting();

  // Ensure that we allocate a new page, set up a bump pointer area, and
  // perform the allocation in a black area.
  heap::SimulateFullSpace(heap->old_space());
  isolate->factory()->NewFixedArray(4, AllocationType::kOld);
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(50, AllocationType::kOld);
  CHECK(heap->old_space()->Contains(*array));
  IncrementalMarking::MarkingState* marking_state = marking->marking_state();
  CHECK(marking_state->IsBlack(*array));

  // Now left trim the allocated black area. A filler has to be installed
  // for the trimmed area and all mark bits of the trimmed area have to be
  // cleared.
  FixedArrayBase trimmed = heap->LeftTrimFixedArray(*array, 10);
  CHECK(marking_state->IsBlack(trimmed));

  heap::GcAndSweep(heap, OLD_SPACE);
}

TEST(ContinuousLeftTrimFixedArrayInBlackArea) {
  if (!FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  CcTest::CollectAllGarbage();

  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  marking->StartBlackAllocationForTesting();

  // Ensure that we allocate a new page, set up a bump pointer area, and
  // perform the allocation in a black area.
  heap::SimulateFullSpace(heap->old_space());
  isolate->factory()->NewFixedArray(10, AllocationType::kOld);

  // Allocate the fixed array that will be trimmed later.
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(100, AllocationType::kOld);
  Address start_address = array->address();
  Address end_address = start_address + array->Size();
  Page* page = Page::FromAddress(start_address);
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      marking->non_atomic_marking_state();
  CHECK(marking_state->IsBlack(*array));
  CHECK(marking_state->bitmap(page)->AllBitsSetInRange(
      page->AddressToMarkbitIndex(start_address),
      page->AddressToMarkbitIndex(end_address)));
  CHECK(heap->old_space()->Contains(*array));

  FixedArrayBase previous = *array;
  FixedArrayBase trimmed;

  // First trim in one word steps.
  for (int i = 0; i < 10; i++) {
    trimmed = heap->LeftTrimFixedArray(previous, 1);
    HeapObject filler = HeapObject::FromAddress(previous.address());
    CHECK(filler.IsFiller());
    CHECK(marking_state->IsBlack(trimmed));
    CHECK(marking_state->IsBlack(previous));
    previous = trimmed;
  }

  // Then trim in two and three word steps.
  for (int i = 2; i <= 3; i++) {
    for (int j = 0; j < 10; j++) {
      trimmed = heap->LeftTrimFixedArray(previous, i);
      HeapObject filler = HeapObject::FromAddress(previous.address());
      CHECK(filler.IsFiller());
      CHECK(marking_state->IsBlack(trimmed));
      CHECK(marking_state->IsBlack(previous));
      previous = trimmed;
    }
  }

  heap::GcAndSweep(heap, OLD_SPACE);
}

TEST(ContinuousRightTrimFixedArrayInBlackArea) {
  if (!FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = CcTest::i_isolate();
  CcTest::CollectAllGarbage();

  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  marking->StartBlackAllocationForTesting();

  // Ensure that we allocate a new page, set up a bump pointer area, and
  // perform the allocation in a black area.
  heap::SimulateFullSpace(heap->old_space());
  isolate->factory()->NewFixedArray(10, AllocationType::kOld);

  // Allocate the fixed array that will be trimmed later.
  Handle<FixedArray> array =
      CcTest::i_isolate()->factory()->NewFixedArray(100, AllocationType::kOld);
  Address start_address = array->address();
  Address end_address = start_address + array->Size();
  Page* page = Page::FromAddress(start_address);
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      marking->non_atomic_marking_state();
  CHECK(marking_state->IsBlack(*array));

  CHECK(marking_state->bitmap(page)->AllBitsSetInRange(
      page->AddressToMarkbitIndex(start_address),
      page->AddressToMarkbitIndex(end_address)));
  CHECK(heap->old_space()->Contains(*array));

  // Trim it once by one word to make checking for white marking color uniform.
  Address previous = end_address - kTaggedSize;
  isolate->heap()->RightTrimFixedArray(*array, 1);

  HeapObject filler = HeapObject::FromAddress(previous);
  CHECK(filler.IsFiller());
  CHECK(marking_state->IsImpossible(filler));

  // Trim 10 times by one, two, and three word.
  for (int i = 1; i <= 3; i++) {
    for (int j = 0; j < 10; j++) {
      previous -= kTaggedSize * i;
      isolate->heap()->RightTrimFixedArray(*array, i);
      HeapObject filler = HeapObject::FromAddress(previous);
      CHECK(filler.IsFiller());
      CHECK(marking_state->IsWhite(filler));
    }
  }

  heap::GcAndSweep(heap, OLD_SPACE);
}

TEST(Regress618958) {
  if (!FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  bool isolate_is_locked = true;
  CcTest::isolate()->AdjustAmountOfExternalAllocatedMemory(100 * MB);
  int mark_sweep_count_before = heap->ms_count();
  heap->MemoryPressureNotification(MemoryPressureLevel::kCritical,
                                   isolate_is_locked);
  int mark_sweep_count_after = heap->ms_count();
  int mark_sweeps_performed = mark_sweep_count_after - mark_sweep_count_before;
  // The memory pressuer handler either performed two GCs or performed one and
  // started incremental marking.
  CHECK(mark_sweeps_performed == 2 ||
        (mark_sweeps_performed == 1 &&
         !heap->incremental_marking()->IsStopped()));
}

TEST(YoungGenerationLargeObjectAllocationScavenge) {
  if (FLAG_minor_mc) return;
  FLAG_young_generation_large_objects = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  if (!isolate->serializer_enabled()) return;

  // TODO(hpayer): Update the test as soon as we have a tenure limit for LO.
  Handle<FixedArray> array_small = isolate->factory()->NewFixedArray(200000);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array_small);
  CHECK_EQ(NEW_LO_SPACE, chunk->owner_identity());
  CHECK(chunk->IsFlagSet(MemoryChunk::LARGE_PAGE));
  CHECK(chunk->IsFlagSet(MemoryChunk::TO_PAGE));

  Handle<Object> number = isolate->factory()->NewHeapNumber(123.456);
  array_small->set(0, *number);

  CcTest::CollectGarbage(NEW_SPACE);

  // After the first young generation GC array_small will be in the old
  // generation large object space.
  chunk = MemoryChunk::FromHeapObject(*array_small);
  CHECK_EQ(LO_SPACE, chunk->owner_identity());
  CHECK(!chunk->InYoungGeneration());

  CcTest::CollectAllAvailableGarbage();
}

TEST(YoungGenerationLargeObjectAllocationMarkCompact) {
  if (FLAG_minor_mc) return;
  FLAG_young_generation_large_objects = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  if (!isolate->serializer_enabled()) return;

  // TODO(hpayer): Update the test as soon as we have a tenure limit for LO.
  Handle<FixedArray> array_small = isolate->factory()->NewFixedArray(200000);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array_small);
  CHECK_EQ(NEW_LO_SPACE, chunk->owner_identity());
  CHECK(chunk->IsFlagSet(MemoryChunk::LARGE_PAGE));
  CHECK(chunk->IsFlagSet(MemoryChunk::TO_PAGE));

  Handle<Object> number = isolate->factory()->NewHeapNumber(123.456);
  array_small->set(0, *number);

  CcTest::CollectGarbage(OLD_SPACE);

  // After the first full GC array_small will be in the old generation
  // large object space.
  chunk = MemoryChunk::FromHeapObject(*array_small);
  CHECK_EQ(LO_SPACE, chunk->owner_identity());
  CHECK(!chunk->InYoungGeneration());

  CcTest::CollectAllAvailableGarbage();
}

TEST(YoungGenerationLargeObjectAllocationReleaseScavenger) {
  if (FLAG_minor_mc) return;
  FLAG_young_generation_large_objects = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  if (!isolate->serializer_enabled()) return;

  {
    HandleScope scope(isolate);
    for (int i = 0; i < 10; i++) {
      Handle<FixedArray> array_small = isolate->factory()->NewFixedArray(20000);
      MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array_small);
      CHECK_EQ(NEW_LO_SPACE, chunk->owner_identity());
      CHECK(chunk->IsFlagSet(MemoryChunk::TO_PAGE));
    }
  }

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(isolate->heap()->new_lo_space()->IsEmpty());
  CHECK_EQ(0, isolate->heap()->new_lo_space()->Size());
  CHECK_EQ(0, isolate->heap()->new_lo_space()->SizeOfObjects());
  CHECK(isolate->heap()->lo_space()->IsEmpty());
  CHECK_EQ(0, isolate->heap()->lo_space()->Size());
  CHECK_EQ(0, isolate->heap()->lo_space()->SizeOfObjects());
}

TEST(UncommitUnusedLargeObjectMemory) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();

  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(200000, AllocationType::kOld);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array);
  CHECK(chunk->owner_identity() == LO_SPACE);

  intptr_t size_before = array->Size();
  size_t committed_memory_before = chunk->CommittedPhysicalMemory();

  array->Shrink(isolate, 1);
  CHECK(array->Size() < size_before);

  CcTest::CollectAllGarbage();
  CHECK(chunk->CommittedPhysicalMemory() < committed_memory_before);
  size_t shrinked_size = RoundUp(
      (array->address() - chunk->address()) + array->Size(), CommitPageSize());
  CHECK_EQ(shrinked_size, chunk->CommittedPhysicalMemory());
}

TEST(RememberedSetRemoveRange) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();

  Handle<FixedArray> array = isolate->factory()->NewFixedArray(
      Page::kPageSize / kTaggedSize, AllocationType::kOld);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array);
  CHECK(chunk->owner_identity() == LO_SPACE);
  Address start = array->address();
  // Maps slot to boolean indicator of whether the slot should be in the set.
  std::map<Address, bool> slots;
  slots[start + 0] = true;
  slots[start + kTaggedSize] = true;
  slots[start + Page::kPageSize - kTaggedSize] = true;
  slots[start + Page::kPageSize] = true;
  slots[start + Page::kPageSize + kTaggedSize] = true;
  slots[chunk->area_end() - kTaggedSize] = true;

  for (auto x : slots) {
    RememberedSet<OLD_TO_NEW>::Insert(chunk, x.first);
  }

  RememberedSet<OLD_TO_NEW>::Iterate(chunk,
                                     [&slots](MaybeObjectSlot slot) {
                                       CHECK(slots[slot.address()]);
                                       return KEEP_SLOT;
                                     },
                                     SlotSet::PREFREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, start, start + kTaggedSize,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  slots[start] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(chunk,
                                     [&slots](MaybeObjectSlot slot) {
                                       CHECK(slots[slot.address()]);
                                       return KEEP_SLOT;
                                     },
                                     SlotSet::PREFREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, start + kTaggedSize,
                                         start + Page::kPageSize,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  slots[start + kTaggedSize] = false;
  slots[start + Page::kPageSize - kTaggedSize] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(chunk,
                                     [&slots](MaybeObjectSlot slot) {
                                       CHECK(slots[slot.address()]);
                                       return KEEP_SLOT;
                                     },
                                     SlotSet::PREFREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, start,
                                         start + Page::kPageSize + kTaggedSize,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  slots[start + Page::kPageSize] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(chunk,
                                     [&slots](MaybeObjectSlot slot) {
                                       CHECK(slots[slot.address()]);
                                       return KEEP_SLOT;
                                     },
                                     SlotSet::PREFREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, chunk->area_end() - kTaggedSize,
                                         chunk->area_end(),
                                         SlotSet::FREE_EMPTY_BUCKETS);
  slots[chunk->area_end() - kTaggedSize] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(chunk,
                                     [&slots](MaybeObjectSlot slot) {
                                       CHECK(slots[slot.address()]);
                                       return KEEP_SLOT;
                                     },
                                     SlotSet::PREFREE_EMPTY_BUCKETS);
}

HEAP_TEST(Regress670675) {
  if (!FLAG_incremental_marking) return;
  FLAG_stress_incremental_marking = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  CcTest::CollectAllGarbage();

  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  i::IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  if (marking->IsStopped()) {
    marking->Start(i::GarbageCollectionReason::kTesting);
  }
  size_t array_length = 128 * KB;
  size_t n = heap->OldGenerationSpaceAvailable() / array_length;
  for (size_t i = 0; i < n + 40; i++) {
    {
      HandleScope inner_scope(isolate);
      isolate->factory()->NewFixedArray(static_cast<int>(array_length),
                                        AllocationType::kOld);
    }
    if (marking->IsStopped()) break;
    double deadline = heap->MonotonicallyIncreasingTimeInMs() + 1;
    marking->AdvanceWithDeadline(
        deadline, IncrementalMarking::GC_VIA_STACK_GUARD, StepOrigin::kV8);
  }
  DCHECK(marking->IsStopped());
}

namespace {
Handle<Code> GenerateDummyImmovableCode(Isolate* isolate) {
  Assembler assm(AssemblerOptions{});

  const int kNumberOfNops = 1 << 10;
  for (int i = 0; i < kNumberOfNops; i++) {
    assm.nop();  // supported on all architectures
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, Code::STUB).set_immovable().Build();
  CHECK(code->IsCode());

  return code;
}
}  // namespace

HEAP_TEST(Regress5831) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  // Used to ensure that the generated code is not collected.
  const int kInitialSize = 32;
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(kInitialSize);

  // Ensure that all immovable code space pages are full and we overflow into
  // LO_SPACE.
  const int kMaxIterations = 1 << 16;
  bool overflowed_into_lospace = false;
  for (int i = 0; i < kMaxIterations; i++) {
    Handle<Code> code = GenerateDummyImmovableCode(isolate);
    array = FixedArray::SetAndGrow(isolate, array, i, code);
    CHECK(heap->code_space()->Contains(code->address()) ||
          heap->code_lo_space()->Contains(*code));
    if (heap->code_lo_space()->Contains(*code)) {
      overflowed_into_lospace = true;
      break;
    }
  }

  CHECK(overflowed_into_lospace);

  // Fake a serializer run.
  isolate->serializer_enabled_ = true;

  // Generate the code.
  Handle<Code> code = GenerateDummyImmovableCode(isolate);
  CHECK_GE(i::kMaxRegularHeapObjectSize, code->Size());
  CHECK(!heap->code_space()->first_page()->Contains(code->address()));

  // Ensure it's not in large object space.
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*code);
  CHECK(chunk->owner_identity() != LO_SPACE);
  CHECK(chunk->NeverEvacuate());
}

TEST(Regress6800) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  const int kRootLength = 1000;
  Handle<FixedArray> root =
      isolate->factory()->NewFixedArray(kRootLength, AllocationType::kOld);
  {
    HandleScope inner_scope(isolate);
    Handle<FixedArray> new_space_array = isolate->factory()->NewFixedArray(1);
    for (int i = 0; i < kRootLength; i++) {
      root->set(i, *new_space_array);
    }
    for (int i = 0; i < kRootLength; i++) {
      root->set(i, ReadOnlyRoots(CcTest::heap()).undefined_value());
    }
  }
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK_EQ(0, RememberedSet<OLD_TO_NEW>::NumberOfPreFreedEmptyBuckets(
                  MemoryChunk::FromHeapObject(*root)));
}

TEST(Regress6800LargeObject) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  const int kRootLength = i::kMaxRegularHeapObjectSize / kTaggedSize;
  Handle<FixedArray> root =
      isolate->factory()->NewFixedArray(kRootLength, AllocationType::kOld);
  CcTest::heap()->lo_space()->Contains(*root);
  {
    HandleScope inner_scope(isolate);
    Handle<FixedArray> new_space_array = isolate->factory()->NewFixedArray(1);
    for (int i = 0; i < kRootLength; i++) {
      root->set(i, *new_space_array);
    }
    for (int i = 0; i < kRootLength; i++) {
      root->set(i, ReadOnlyRoots(CcTest::heap()).undefined_value());
    }
  }
  CcTest::CollectGarbage(OLD_SPACE);
  CHECK_EQ(0, RememberedSet<OLD_TO_NEW>::NumberOfPreFreedEmptyBuckets(
                  MemoryChunk::FromHeapObject(*root)));
}

HEAP_TEST(RegressMissingWriteBarrierInAllocate) {
  if (!FLAG_incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  CcTest::CollectAllGarbage();
  heap::SimulateIncrementalMarking(heap, false);
  Handle<Map> map;
  {
    AlwaysAllocateScope always_allocate(isolate);
    map = isolate->factory()->NewMap(HEAP_NUMBER_TYPE, HeapNumber::kSize);
  }
  heap->incremental_marking()->StartBlackAllocationForTesting();
  Handle<HeapObject> object;
  {
    AlwaysAllocateScope always_allocate(isolate);
    object = handle(isolate->factory()->NewForTest(map, AllocationType::kOld),
                    isolate);
  }
  // The object is black. If Factory::New sets the map without write-barrier,
  // then the map is white and will be freed prematurely.
  heap::SimulateIncrementalMarking(heap, true);
  CcTest::CollectAllGarbage();
  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(object->map().IsMap());
}

HEAP_TEST(MarkCompactEpochCounter) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  unsigned epoch0 = heap->mark_compact_collector()->epoch();
  CcTest::CollectGarbage(OLD_SPACE);
  unsigned epoch1 = heap->mark_compact_collector()->epoch();
  CHECK_EQ(epoch0 + 1, epoch1);
  heap::SimulateIncrementalMarking(heap, true);
  CcTest::CollectGarbage(OLD_SPACE);
  unsigned epoch2 = heap->mark_compact_collector()->epoch();
  CHECK_EQ(epoch1 + 1, epoch2);
  CcTest::CollectGarbage(NEW_SPACE);
  unsigned epoch3 = heap->mark_compact_collector()->epoch();
  CHECK_EQ(epoch2, epoch3);
}

UNINITIALIZED_TEST(ReinitializeStringHashSeed) {
  // Enable rehashing and create an isolate and context.
  i::FLAG_rehash_snapshot = true;
  for (int i = 1; i < 3; i++) {
    i::FLAG_hash_seed = 1337 * i;
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      CHECK_EQ(static_cast<uint64_t>(1337 * i),
               HashSeed(reinterpret_cast<i::Isolate*>(isolate)));
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      CHECK(!context.IsEmpty());
      v8::Context::Scope context_scope(context);
    }
    isolate->Dispose();
    ReadOnlyHeap::ClearSharedHeapForTest();
  }
}

const int kHeapLimit = 100 * MB;
Isolate* oom_isolate = nullptr;

void OOMCallback(const char* location, bool is_heap_oom) {
  Heap* heap = oom_isolate->heap();
  size_t kSlack = heap->new_space()->Capacity();
  CHECK_LE(heap->OldGenerationCapacity(), kHeapLimit + kSlack);
  CHECK_LE(heap->memory_allocator()->Size(), heap->MaxReserved() + kSlack);
  base::OS::ExitProcess(0);
}

UNINITIALIZED_TEST(OutOfMemory) {
  if (FLAG_stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) return;
#endif
  FLAG_max_old_space_size = kHeapLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  oom_isolate = i_isolate;
  isolate->SetOOMErrorHandler(OOMCallback);
  {
    Factory* factory = i_isolate->factory();
    HandleScope handle_scope(i_isolate);
    while (true) {
      factory->NewFixedArray(100);
    }
  }
}

UNINITIALIZED_TEST(OutOfMemoryIneffectiveGC) {
  if (!FLAG_detect_ineffective_gcs_near_heap_limit) return;
  if (FLAG_stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) return;
#endif

  FLAG_max_old_space_size = kHeapLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  oom_isolate = i_isolate;
  isolate->SetOOMErrorHandler(OOMCallback);
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  heap->CollectAllGarbage(Heap::kNoGCFlags, GarbageCollectionReason::kTesting);
  {
    HandleScope scope(i_isolate);
    while (heap->OldGenerationSizeOfObjects() <
           heap->MaxOldGenerationSize() * 0.9) {
      factory->NewFixedArray(100, AllocationType::kOld);
    }
    {
      int initial_ms_count = heap->ms_count();
      int ineffective_ms_start = initial_ms_count;
      while (heap->ms_count() < initial_ms_count + 10) {
        HandleScope inner_scope(i_isolate);
        factory->NewFixedArray(30000, AllocationType::kOld);
        if (heap->tracer()->AverageMarkCompactMutatorUtilization() >= 0.3) {
          ineffective_ms_start = heap->ms_count() + 1;
        }
      }
      int consecutive_ineffective_ms = heap->ms_count() - ineffective_ms_start;
      CHECK_IMPLIES(
          consecutive_ineffective_ms >= 4,
          heap->tracer()->AverageMarkCompactMutatorUtilization() >= 0.3);
    }
  }
  isolate->Dispose();
}

HEAP_TEST(Regress779503) {
  // The following regression test ensures that the Scavenger does not allocate
  // over invalid slots. More specific, the Scavenger should not sweep a page
  // that it currently processes because it might allocate over the currently
  // processed slot.
  const int kArraySize = 2048;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  heap::SealCurrentObjects(heap);
  {
    HandleScope handle_scope(isolate);
    // The byte array filled with kHeapObjectTag ensures that we cannot read
    // from the slot again and interpret it as heap value. Doing so will crash.
    Handle<ByteArray> byte_array = isolate->factory()->NewByteArray(kArraySize);
    CHECK(Heap::InYoungGeneration(*byte_array));
    for (int i = 0; i < kArraySize; i++) {
      byte_array->set(i, kHeapObjectTag);
    }

    {
      HandleScope handle_scope(isolate);
      // The FixedArray in old space serves as space for slots.
      Handle<FixedArray> fixed_array =
          isolate->factory()->NewFixedArray(kArraySize, AllocationType::kOld);
      CHECK(!Heap::InYoungGeneration(*fixed_array));
      for (int i = 0; i < kArraySize; i++) {
        fixed_array->set(i, *byte_array);
      }
    }
    // Delay sweeper tasks to allow the scavenger to sweep the page it is
    // currently scavenging.
    heap->delay_sweeper_tasks_for_testing_ = true;
    CcTest::CollectGarbage(OLD_SPACE);
    CHECK(Heap::InYoungGeneration(*byte_array));
  }
  // Scavenging and sweeping the same page will crash as slots will be
  // overridden.
  CcTest::CollectGarbage(NEW_SPACE);
  heap->delay_sweeper_tasks_for_testing_ = false;
}

struct OutOfMemoryState {
  Heap* heap;
  bool oom_triggered;
  size_t old_generation_capacity_at_oom;
  size_t memory_allocator_size_at_oom;
  size_t new_space_capacity_at_oom;
  size_t new_lo_space_size_at_oom;
  size_t current_heap_limit;
  size_t initial_heap_limit;
};

size_t NearHeapLimitCallback(void* raw_state, size_t current_heap_limit,
                             size_t initial_heap_limit) {
  OutOfMemoryState* state = static_cast<OutOfMemoryState*>(raw_state);
  Heap* heap = state->heap;
  state->oom_triggered = true;
  state->old_generation_capacity_at_oom = heap->OldGenerationCapacity();
  state->memory_allocator_size_at_oom = heap->memory_allocator()->Size();
  state->new_space_capacity_at_oom = heap->new_space()->Capacity();
  state->new_lo_space_size_at_oom = heap->new_lo_space()->Size();
  state->current_heap_limit = current_heap_limit;
  state->initial_heap_limit = initial_heap_limit;
  return initial_heap_limit + 100 * MB;
}

size_t MemoryAllocatorSizeFromHeapCapacity(size_t capacity) {
  // Size to capacity factor.
  double factor =
      Page::kPageSize * 1.0 / MemoryChunkLayout::AllocatableMemoryInDataPage();
  // Some tables (e.g. deoptimization table) are allocated directly with the
  // memory allocator. Allow some slack to account for them.
  size_t slack = 5 * MB;
  return static_cast<size_t>(capacity * factor) + slack;
}

UNINITIALIZED_TEST(OutOfMemorySmallObjects) {
  if (FLAG_stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) return;
#endif
  const size_t kOldGenerationLimit = 300 * MB;
  FLAG_max_old_space_size = kOldGenerationLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  Isolate* isolate =
      reinterpret_cast<Isolate*>(v8::Isolate::New(create_params));
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  OutOfMemoryState state;
  state.heap = heap;
  state.oom_triggered = false;
  heap->AddNearHeapLimitCallback(NearHeapLimitCallback, &state);
  {
    HandleScope handle_scope(isolate);
    while (!state.oom_triggered) {
      factory->NewFixedArray(100);
    }
  }
  CHECK_LE(state.old_generation_capacity_at_oom,
           kOldGenerationLimit + state.new_space_capacity_at_oom);
  CHECK_LE(kOldGenerationLimit, state.old_generation_capacity_at_oom +
                                    state.new_space_capacity_at_oom);
  CHECK_LE(
      state.memory_allocator_size_at_oom,
      MemoryAllocatorSizeFromHeapCapacity(state.old_generation_capacity_at_oom +
                                          2 * state.new_space_capacity_at_oom));
  reinterpret_cast<v8::Isolate*>(isolate)->Dispose();
}

UNINITIALIZED_TEST(OutOfMemoryLargeObjects) {
  if (FLAG_stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) return;
#endif
  const size_t kOldGenerationLimit = 300 * MB;
  FLAG_max_old_space_size = kOldGenerationLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  Isolate* isolate =
      reinterpret_cast<Isolate*>(v8::Isolate::New(create_params));
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  OutOfMemoryState state;
  state.heap = heap;
  state.oom_triggered = false;
  heap->AddNearHeapLimitCallback(NearHeapLimitCallback, &state);
  const int kFixedArrayLength = 1000000;
  {
    HandleScope handle_scope(isolate);
    while (!state.oom_triggered) {
      factory->NewFixedArray(kFixedArrayLength);
    }
  }
  CHECK_LE(state.old_generation_capacity_at_oom, kOldGenerationLimit);
  CHECK_LE(kOldGenerationLimit, state.old_generation_capacity_at_oom +
                                    state.new_space_capacity_at_oom +
                                    state.new_lo_space_size_at_oom +
                                    FixedArray::SizeFor(kFixedArrayLength));
  CHECK_LE(
      state.memory_allocator_size_at_oom,
      MemoryAllocatorSizeFromHeapCapacity(state.old_generation_capacity_at_oom +
                                          2 * state.new_space_capacity_at_oom +
                                          state.new_lo_space_size_at_oom));
  reinterpret_cast<v8::Isolate*>(isolate)->Dispose();
}

UNINITIALIZED_TEST(RestoreHeapLimit) {
  if (FLAG_stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) return;
#endif
  ManualGCScope manual_gc_scope;
  const size_t kOldGenerationLimit = 300 * MB;
  FLAG_max_old_space_size = kOldGenerationLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  Isolate* isolate =
      reinterpret_cast<Isolate*>(v8::Isolate::New(create_params));
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  OutOfMemoryState state;
  state.heap = heap;
  state.oom_triggered = false;
  heap->AddNearHeapLimitCallback(NearHeapLimitCallback, &state);
  heap->AutomaticallyRestoreInitialHeapLimit(0.5);
  const int kFixedArrayLength = 1000000;
  {
    HandleScope handle_scope(isolate);
    while (!state.oom_triggered) {
      factory->NewFixedArray(kFixedArrayLength);
    }
  }
  heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
  state.oom_triggered = false;
  {
    HandleScope handle_scope(isolate);
    while (!state.oom_triggered) {
      factory->NewFixedArray(kFixedArrayLength);
    }
  }
  CHECK_EQ(state.current_heap_limit, state.initial_heap_limit);
  reinterpret_cast<v8::Isolate*>(isolate)->Dispose();
}

void HeapTester::UncommitFromSpace(Heap* heap) {
  heap->UncommitFromSpace();
  heap->memory_allocator()->unmapper()->EnsureUnmappingCompleted();
}

class DeleteNative {
 public:
  static void Deleter(void* arg) {
    delete reinterpret_cast<DeleteNative*>(arg);
  }
};

TEST(Regress8014) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  Heap* heap = isolate->heap();
  {
    HandleScope scope(isolate);
    for (int i = 0; i < 10000; i++) {
      auto handle = Managed<DeleteNative>::FromRawPtr(isolate, 1000000,
                                                      new DeleteNative());
      USE(handle);
    }
  }
  int ms_count = heap->ms_count();
  heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
  // Several GCs can be triggred by the above call.
  // The bad case triggers 10000 GCs.
  CHECK_LE(heap->ms_count(), ms_count + 10);
}

TEST(Regress8617) {
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  heap::SimulateFullSpace(heap->old_space());
  // Step 1. Create a function and ensure that it is in the old space.
  Handle<Object> foo =
      v8::Utils::OpenHandle(*CompileRun("function foo() { return 42; };"
                                        "foo;"));
  if (heap->InYoungGeneration(*foo)) {
    CcTest::CollectGarbage(NEW_SPACE);
    CcTest::CollectGarbage(NEW_SPACE);
  }
  // Step 2. Create an object with a reference to foo in the descriptor array.
  CompileRun(
      "var obj = {};"
      "obj.method = foo;"
      "obj;");
  // Step 3. Make sure that foo moves during Mark-Compact.
  Page* ec_page = Page::FromAddress(foo->ptr());
  heap::ForceEvacuationCandidate(ec_page);
  // Step 4. Start incremental marking.
  heap::SimulateIncrementalMarking(heap, false);
  CHECK(ec_page->IsEvacuationCandidate());
  // Step 5. Install a new descriptor array on the map of the object.
  // This runs the marking barrier for the descriptor array.
  // In the bad case it sets the number of marked descriptors but does not
  // change the color of the descriptor array.
  CompileRun("obj.bar = 10;");
  // Step 6. Promote the descriptor array to old space. During promotion
  // the Scavenger will not record the slot of foo in the descriptor array.
  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  // Step 7. Complete the Mark-Compact.
  CcTest::CollectAllGarbage();
  // Step 8. Use the descriptor for foo, which contains a stale pointer.
  CompileRun("obj.method()");
}

HEAP_TEST(MemoryReducerActivationForSmallHeaps) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  CHECK_EQ(heap->memory_reducer()->state_.action, MemoryReducer::Action::kDone);
  HandleScope scope(isolate);
  const size_t kActivationThreshold = 1 * MB;
  size_t initial_capacity = heap->OldGenerationCapacity();
  while (heap->OldGenerationCapacity() <
         initial_capacity + kActivationThreshold) {
    isolate->factory()->NewFixedArray(1 * KB, AllocationType::kOld);
  }
  CHECK_EQ(heap->memory_reducer()->state_.action, MemoryReducer::Action::kWait);
}

TEST(CodeObjectRegistry) {
  // We turn off compaction to ensure that code is not moving.
  FLAG_never_compact = true;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  Handle<Code> code1;
  HandleScope outer_scope(heap->isolate());
  Address code2_address;
  {
    code1 = DummyOptimizedCode(isolate);
    Handle<Code> code2 = DummyOptimizedCode(isolate);
    code2_address = code2->address();
    // If this check breaks, change the allocation to ensure that both code
    // objects are on the same page.
    CHECK_EQ(MemoryChunk::FromHeapObject(*code1),
             MemoryChunk::FromHeapObject(*code2));
    CHECK(MemoryChunk::FromHeapObject(*code1)->Contains(code1->address()));
    CHECK(MemoryChunk::FromHeapObject(*code2)->Contains(code2->address()));
  }
  CcTest::CollectAllAvailableGarbage();
  CHECK(MemoryChunk::FromHeapObject(*code1)->Contains(code1->address()));
  CHECK(MemoryChunk::FromAddress(code2_address)->Contains(code2_address));
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

#undef __
