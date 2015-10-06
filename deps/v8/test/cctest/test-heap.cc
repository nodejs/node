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

#include "src/v8.h"

#include "src/compilation-cache.h"
#include "src/context-measure.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"
#include "src/snapshot/snapshot.h"
#include "test/cctest/cctest.h"

using v8::Just;

namespace v8 {
namespace internal {

// Tests that should have access to private methods of {v8::internal::Heap}.
// Those tests need to be defined using HEAP_TEST(Name) { ... }.
#define HEAP_TEST_METHODS(V) \
  V(GCFlags)


#define HEAP_TEST(Name)                                                      \
  CcTest register_test_##Name(HeapTester::Test##Name, __FILE__, #Name, NULL, \
                              true, true);                                   \
  void HeapTester::Test##Name()


class HeapTester {
 public:
#define DECLARE_STATIC(Name) static void Test##Name();

  HEAP_TEST_METHODS(DECLARE_STATIC)
#undef HEAP_TEST_METHODS
};


static void CheckMap(Map* map, int type, int instance_size) {
  CHECK(map->IsHeapObject());
#ifdef DEBUG
  CHECK(CcTest::heap()->Contains(map));
#endif
  CHECK_EQ(CcTest::heap()->meta_map(), map->map());
  CHECK_EQ(type, map->instance_type());
  CHECK_EQ(instance_size, map->instance_size());
}


TEST(HeapMaps) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CheckMap(heap->meta_map(), MAP_TYPE, Map::kSize);
  CheckMap(heap->heap_number_map(), HEAP_NUMBER_TYPE, HeapNumber::kSize);
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type) \
  CheckMap(heap->type##_map(), SIMD128_VALUE_TYPE, Type::kSize);
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
  CheckMap(heap->fixed_array_map(), FIXED_ARRAY_TYPE, kVariableSizeSentinel);
  CheckMap(heap->string_map(), STRING_TYPE, kVariableSizeSentinel);
}


static void CheckOddball(Isolate* isolate, Object* obj, const char* string) {
  CHECK(obj->IsOddball());
  Handle<Object> handle(obj, isolate);
  Object* print_string =
      *Execution::ToString(isolate, handle).ToHandleChecked();
  CHECK(String::cast(print_string)->IsUtf8EqualTo(CStrVector(string)));
}


static void CheckSmi(Isolate* isolate, int value, const char* string) {
  Handle<Object> handle(Smi::FromInt(value), isolate);
  Object* print_string =
      *Execution::ToString(isolate, handle).ToHandleChecked();
  CHECK(String::cast(print_string)->IsUtf8EqualTo(CStrVector(string)));
}


static void CheckNumber(Isolate* isolate, double value, const char* string) {
  Handle<Object> number = isolate->factory()->NewNumber(value);
  CHECK(number->IsNumber());
  Handle<Object> print_string =
      Execution::ToString(isolate, number).ToHandleChecked();
  CHECK(String::cast(*print_string)->IsUtf8EqualTo(CStrVector(string)));
}


static void CheckFindCodeObject(Isolate* isolate) {
  // Test FindCodeObject
#define __ assm.

  Assembler assm(isolate, NULL, 0);

  __ nop();  // supported on all architectures

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  CHECK(code->IsCode());

  HeapObject* obj = HeapObject::cast(*code);
  Address obj_addr = obj->address();

  for (int i = 0; i < obj->Size(); i += kPointerSize) {
    Object* found = isolate->FindCodeObject(obj_addr + i);
    CHECK_EQ(*code, found);
  }

  Handle<Code> copy = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  HeapObject* obj_copy = HeapObject::cast(*copy);
  Object* not_right = isolate->FindCodeObject(obj_copy->address() +
                                              obj_copy->Size() / 2);
  CHECK(not_right != *code);
}


TEST(HandleNull) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  LocalContext context;
  Handle<Object> n(static_cast<Object*>(nullptr), isolate);
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
  Handle<GlobalObject> global(CcTest::i_isolate()->context()->global_object());
  CHECK(Just(true) == JSReceiver::HasOwnProperty(global, object_string));

  // Check ToString for oddballs
  CheckOddball(isolate, heap->true_value(), "true");
  CheckOddball(isolate, heap->false_value(), "false");
  CheckOddball(isolate, heap->null_value(), "null");
  CheckOddball(isolate, heap->undefined_value(), "undefined");

  // Check ToString for Smis
  CheckSmi(isolate, 0, "0");
  CheckSmi(isolate, 42, "42");
  CheckSmi(isolate, -42, "-42");

  // Check ToString for Numbers
  CheckNumber(isolate, 1.1, "1.1");

  CheckFindCodeObject(isolate);
}


template <typename T, typename LANE_TYPE, int LANES>
static void CheckSimdValue(T* value, LANE_TYPE lane_values[LANES],
                           LANE_TYPE other_value) {
  // Check against lane_values, and check that all lanes can be set to
  // other_value without disturbing the other lanes.
  for (int i = 0; i < LANES; i++) {
    CHECK_EQ(lane_values[i], value->get_lane(i));
  }
  for (int i = 0; i < LANES; i++) {
    value->set_lane(i, other_value);  // change the value
    for (int j = 0; j < LANES; j++) {
      if (i != j)
        CHECK_EQ(lane_values[j], value->get_lane(j));
      else
        CHECK_EQ(other_value, value->get_lane(j));
    }
    value->set_lane(i, lane_values[i]);  // restore the lane
  }
  CHECK(value->BooleanValue());  // SIMD values are 'true'.
}


TEST(SimdObjects) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);

  // Float32x4
  {
    float lanes[4] = {1, 2, 3, 4};
    float quiet_NaN = std::numeric_limits<float>::quiet_NaN();
    float signaling_NaN = std::numeric_limits<float>::signaling_NaN();

    Handle<Float32x4> value = factory->NewFloat32x4(lanes);
    CHECK(value->IsFloat32x4());
    CheckSimdValue<Float32x4, float, 4>(*value, lanes, 3.14f);

    // Check special lane values.
    value->set_lane(1, -0.0);
    CHECK_EQ(-0.0, value->get_lane(1));
    CHECK(std::signbit(value->get_lane(1)));  // Sign bit should be preserved.
    value->set_lane(2, quiet_NaN);
    CHECK(std::isnan(value->get_lane(2)));
    value->set_lane(3, signaling_NaN);
    CHECK(std::isnan(value->get_lane(3)));

#ifdef OBJECT_PRINT
    // Check value printing.
    {
      value = factory->NewFloat32x4(lanes);
      std::ostringstream os;
      value->Float32x4Print(os);
      CHECK_EQ("1, 2, 3, 4", os.str());
    }
    {
      float special_lanes[4] = {0, -0.0, quiet_NaN, signaling_NaN};
      value = factory->NewFloat32x4(special_lanes);
      std::ostringstream os;
      value->Float32x4Print(os);
      // Value printing doesn't preserve signed zeroes.
      CHECK_EQ("0, 0, NaN, NaN", os.str());
    }
#endif  // OBJECT_PRINT
  }
  // Int32x4
  {
    int32_t lanes[4] = {-1, 0, 1, 2};

    Handle<Int32x4> value = factory->NewInt32x4(lanes);
    CHECK(value->IsInt32x4());
    CheckSimdValue<Int32x4, int32_t, 4>(*value, lanes, 3);

#ifdef OBJECT_PRINT
    std::ostringstream os;
    value->Int32x4Print(os);
    CHECK_EQ("-1, 0, 1, 2", os.str());
#endif  // OBJECT_PRINT
  }
  // Bool32x4
  {
    bool lanes[4] = {true, true, true, false};

    Handle<Bool32x4> value = factory->NewBool32x4(lanes);
    CHECK(value->IsBool32x4());
    CheckSimdValue<Bool32x4, bool, 4>(*value, lanes, false);

#ifdef OBJECT_PRINT
    std::ostringstream os;
    value->Bool32x4Print(os);
    CHECK_EQ("true, true, true, false", os.str());
#endif  // OBJECT_PRINT
  }
  // Int16x8
  {
    int16_t lanes[8] = {-1, 0, 1, 2, 3, 4, 5, -32768};

    Handle<Int16x8> value = factory->NewInt16x8(lanes);
    CHECK(value->IsInt16x8());
    CheckSimdValue<Int16x8, int16_t, 8>(*value, lanes, 32767);

#ifdef OBJECT_PRINT
    std::ostringstream os;
    value->Int16x8Print(os);
    CHECK_EQ("-1, 0, 1, 2, 3, 4, 5, -32768", os.str());
#endif  // OBJECT_PRINT
  }
  // Bool16x8
  {
    bool lanes[8] = {true, true, true, true, true, true, true, false};

    Handle<Bool16x8> value = factory->NewBool16x8(lanes);
    CHECK(value->IsBool16x8());
    CheckSimdValue<Bool16x8, bool, 8>(*value, lanes, false);

#ifdef OBJECT_PRINT
    std::ostringstream os;
    value->Bool16x8Print(os);
    CHECK_EQ("true, true, true, true, true, true, true, false", os.str());
#endif  // OBJECT_PRINT
  }
  // Int8x16
  {
    int8_t lanes[16] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, -128};

    Handle<Int8x16> value = factory->NewInt8x16(lanes);
    CHECK(value->IsInt8x16());
    CheckSimdValue<Int8x16, int8_t, 16>(*value, lanes, 127);

#ifdef OBJECT_PRINT
    std::ostringstream os;
    value->Int8x16Print(os);
    CHECK_EQ("-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, -128",
             os.str());
#endif  // OBJECT_PRINT
  }
  // Bool8x16
  {
    bool lanes[16] = {true, true, true, true, true, true, true, false,
                      true, true, true, true, true, true, true, false};

    Handle<Bool8x16> value = factory->NewBool8x16(lanes);
    CHECK(value->IsBool8x16());
    CheckSimdValue<Bool8x16, bool, 16>(*value, lanes, false);

#ifdef OBJECT_PRINT
    std::ostringstream os;
    value->Bool8x16Print(os);
    CHECK_EQ(
        "true, true, true, true, true, true, true, false, true, true, true, "
        "true, true, true, true, false",
        os.str());
#endif  // OBJECT_PRINT
  }
}


TEST(Tagging) {
  CcTest::InitializeVM();
  int request = 24;
  CHECK_EQ(request, static_cast<int>(OBJECT_POINTER_ALIGN(request)));
  CHECK(Smi::FromInt(42)->IsSmi());
  CHECK(Smi::FromInt(Smi::kMinValue)->IsSmi());
  CHECK(Smi::FromInt(Smi::kMaxValue)->IsSmi());
}


TEST(GarbageCollection) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);
  // Check GC.
  heap->CollectGarbage(NEW_SPACE);

  Handle<GlobalObject> global(CcTest::i_isolate()->context()->global_object());
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<String> prop_namex = factory->InternalizeUtf8String("theSlotx");
  Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  Handle<Smi> twenty_four(Smi::FromInt(24), isolate);

  {
    HandleScope inner_scope(isolate);
    // Allocate a function and keep it in global object's property.
    Handle<JSFunction> function = factory->NewFunction(name);
    JSReceiver::SetProperty(global, name, function, SLOPPY).Check();
    // Allocate an object.  Unrooted after leaving the scope.
    Handle<JSObject> obj = factory->NewJSObject(function);
    JSReceiver::SetProperty(obj, prop_name, twenty_three, SLOPPY).Check();
    JSReceiver::SetProperty(obj, prop_namex, twenty_four, SLOPPY).Check();

    CHECK_EQ(Smi::FromInt(23),
             *Object::GetProperty(obj, prop_name).ToHandleChecked());
    CHECK_EQ(Smi::FromInt(24),
             *Object::GetProperty(obj, prop_namex).ToHandleChecked());
  }

  heap->CollectGarbage(NEW_SPACE);

  // Function should be alive.
  CHECK(Just(true) == JSReceiver::HasOwnProperty(global, name));
  // Check function is retained.
  Handle<Object> func_value =
      Object::GetProperty(global, name).ToHandleChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);

  {
    HandleScope inner_scope(isolate);
    // Allocate another object, make it reachable from global.
    Handle<JSObject> obj = factory->NewJSObject(function);
    JSReceiver::SetProperty(global, obj_name, obj, SLOPPY).Check();
    JSReceiver::SetProperty(obj, prop_name, twenty_three, SLOPPY).Check();
  }

  // After gc, it should survive.
  heap->CollectGarbage(NEW_SPACE);

  CHECK(Just(true) == JSReceiver::HasOwnProperty(global, obj_name));
  Handle<Object> obj =
      Object::GetProperty(global, obj_name).ToHandleChecked();
  CHECK(obj->IsJSObject());
  CHECK_EQ(Smi::FromInt(23),
           *Object::GetProperty(obj, prop_name).ToHandleChecked());
}


static void VerifyStringAllocation(Isolate* isolate, const char* string) {
  HandleScope scope(isolate);
  Handle<String> s = isolate->factory()->NewStringFromUtf8(
      CStrVector(string)).ToHandleChecked();
  CHECK_EQ(StrLength(string), s->length());
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
  CHECK_EQ(StrLength(name), string->length());
}


TEST(GlobalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
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
  heap->CollectGarbage(NEW_SPACE);

  CHECK((*h1)->IsString());
  CHECK((*h2)->IsHeapNumber());
  CHECK((*h3)->IsString());
  CHECK((*h4)->IsHeapNumber());

  CHECK_EQ(*h3, *h1);
  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h3.location());

  CHECK_EQ(*h4, *h2);
  GlobalHandles::Destroy(h2.location());
  GlobalHandles::Destroy(h4.location());
}


static bool WeakPointerCleared = false;

static void TestWeakGlobalHandleCallback(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  std::pair<v8::Persistent<v8::Value>*, int>* p =
      reinterpret_cast<std::pair<v8::Persistent<v8::Value>*, int>*>(
          data.GetParameter());
  if (p->second == 1234) WeakPointerCleared = true;
  p->first->Reset();
}


TEST(WeakGlobalHandlesScavenge) {
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
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

  std::pair<Handle<Object>*, int> handle_and_id(&h2, 1234);
  GlobalHandles::MakeWeak(h2.location(),
                          reinterpret_cast<void*>(&handle_and_id),
                          &TestWeakGlobalHandleCallback);

  // Scavenge treats weak pointers as normal roots.
  heap->CollectGarbage(NEW_SPACE);

  CHECK((*h1)->IsString());
  CHECK((*h2)->IsHeapNumber());

  CHECK(!WeakPointerCleared);
  CHECK(!global_handles->IsNearDeath(h2.location()));
  CHECK(!global_handles->IsNearDeath(h1.location()));

  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h2.location());
}


TEST(WeakGlobalHandlesMark) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
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
  heap->CollectGarbage(OLD_SPACE);
  heap->CollectGarbage(NEW_SPACE);
  CHECK(!heap->InNewSpace(*h1) && !heap->InNewSpace(*h2));

  std::pair<Handle<Object>*, int> handle_and_id(&h2, 1234);
  GlobalHandles::MakeWeak(h2.location(),
                          reinterpret_cast<void*>(&handle_and_id),
                          &TestWeakGlobalHandleCallback);
  CHECK(!GlobalHandles::IsNearDeath(h1.location()));
  CHECK(!GlobalHandles::IsNearDeath(h2.location()));

  // Incremental marking potentially marked handles before they turned weak.
  heap->CollectAllGarbage();

  CHECK((*h1)->IsString());

  CHECK(WeakPointerCleared);
  CHECK(!GlobalHandles::IsNearDeath(h1.location()));

  GlobalHandles::Destroy(h1.location());
}


TEST(DeleteWeakGlobalHandle) {
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
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
  GlobalHandles::MakeWeak(h.location(),
                          reinterpret_cast<void*>(&handle_and_id),
                          &TestWeakGlobalHandleCallback);

  // Scanvenge does not recognize weak reference.
  heap->CollectGarbage(NEW_SPACE);

  CHECK(!WeakPointerCleared);

  // Mark-compact treats weak reference properly.
  heap->CollectGarbage(OLD_SPACE);

  CHECK(WeakPointerCleared);
}


TEST(BytecodeArray) {
  static const uint8_t kRawBytes[] = {0xc3, 0x7e, 0xa5, 0x5a};
  static const int kRawBytesSize = sizeof(kRawBytes);
  static const int kFrameSize = 32;

  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  // Allocate and initialize BytecodeArray
  Handle<BytecodeArray> array =
      factory->NewBytecodeArray(kRawBytesSize, kRawBytes, kFrameSize);

  CHECK(array->IsBytecodeArray());
  CHECK_EQ(array->length(), (int)sizeof(kRawBytes));
  CHECK_EQ(array->frame_size(), kFrameSize);
  CHECK_LE(array->address(), array->GetFirstBytecodeAddress());
  CHECK_GE(array->address() + array->BytecodeArraySize(),
           array->GetFirstBytecodeAddress() + array->length());
  for (int i = 0; i < kRawBytesSize; i++) {
    CHECK_EQ(array->GetFirstBytecodeAddress()[i], kRawBytes[i]);
    CHECK_EQ(array->get(i), kRawBytes[i]);
  }

  // Full garbage collection
  heap->CollectAllGarbage();

  // BytecodeArray should survive
  CHECK_EQ(array->length(), kRawBytesSize);
  CHECK_EQ(array->frame_size(), kFrameSize);

  for (int i = 0; i < kRawBytesSize; i++) {
    CHECK_EQ(array->get(i), kRawBytes[i]);
    CHECK_EQ(array->GetFirstBytecodeAddress()[i], kRawBytes[i]);
  }
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
  0
};


static void CheckInternalizedStrings(const char** strings) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  for (const char* string = *strings; *strings != 0; string = *strings++) {
    HandleScope scope(isolate);
    Handle<String> a =
        isolate->factory()->InternalizeUtf8String(CStrVector(string));
    // InternalizeUtf8String may return a failure if a GC is needed.
    CHECK(a->IsInternalizedString());
    Handle<String> b = factory->InternalizeUtf8String(string);
    CHECK_EQ(*b, *a);
    CHECK(b->IsUtf8EqualTo(CStrVector(string)));
    b = isolate->factory()->InternalizeUtf8String(CStrVector(string));
    CHECK_EQ(*b, *a);
    CHECK(b->IsUtf8EqualTo(CStrVector(string)));
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
  Handle<JSFunction> function = factory->NewFunction(name);

  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  Handle<Smi> twenty_four(Smi::FromInt(24), isolate);

  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<JSObject> obj = factory->NewJSObject(function);
  JSReceiver::SetProperty(obj, prop_name, twenty_three, SLOPPY).Check();
  CHECK_EQ(Smi::FromInt(23),
           *Object::GetProperty(obj, prop_name).ToHandleChecked());
  // Check that we can add properties to function objects.
  JSReceiver::SetProperty(function, prop_name, twenty_four, SLOPPY).Check();
  CHECK_EQ(Smi::FromInt(24),
           *Object::GetProperty(function, prop_name).ToHandleChecked());
}


TEST(ObjectProperties) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> object_string(String::cast(CcTest::heap()->Object_string()));
  Handle<Object> object = Object::GetProperty(
      CcTest::i_isolate()->global_object(), object_string).ToHandleChecked();
  Handle<JSFunction> constructor = Handle<JSFunction>::cast(object);
  Handle<JSObject> obj = factory->NewJSObject(constructor);
  Handle<String> first = factory->InternalizeUtf8String("first");
  Handle<String> second = factory->InternalizeUtf8String("second");

  Handle<Smi> one(Smi::FromInt(1), isolate);
  Handle<Smi> two(Smi::FromInt(2), isolate);

  // check for empty
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));

  // add first
  JSReceiver::SetProperty(obj, first, one, SLOPPY).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));

  // delete first
  JSReceiver::DeleteProperty(obj, first, SLOPPY).Check();
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));

  // add first and then second
  JSReceiver::SetProperty(obj, first, one, SLOPPY).Check();
  JSReceiver::SetProperty(obj, second, two, SLOPPY).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, second));

  // delete first and then second
  JSReceiver::DeleteProperty(obj, first, SLOPPY).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, second));
  JSReceiver::DeleteProperty(obj, second, SLOPPY).Check();
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, second));

  // add first and then second
  JSReceiver::SetProperty(obj, first, one, SLOPPY).Check();
  JSReceiver::SetProperty(obj, second, two, SLOPPY).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, second));

  // delete second and then first
  JSReceiver::DeleteProperty(obj, second, SLOPPY).Check();
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, first));
  JSReceiver::DeleteProperty(obj, first, SLOPPY).Check();
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, first));
  CHECK(Just(false) == JSReceiver::HasOwnProperty(obj, second));

  // check string and internalized string match
  const char* string1 = "fisk";
  Handle<String> s1 = factory->NewStringFromAsciiChecked(string1);
  JSReceiver::SetProperty(obj, s1, one, SLOPPY).Check();
  Handle<String> s1_string = factory->InternalizeUtf8String(string1);
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, s1_string));

  // check internalized string and string match
  const char* string2 = "fugl";
  Handle<String> s2_string = factory->InternalizeUtf8String(string2);
  JSReceiver::SetProperty(obj, s2_string, one, SLOPPY).Check();
  Handle<String> s2 = factory->NewStringFromAsciiChecked(string2);
  CHECK(Just(true) == JSReceiver::HasOwnProperty(obj, s2));
}


TEST(JSObjectMaps) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<JSFunction> function = factory->NewFunction(name);

  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<JSObject> obj = factory->NewJSObject(function);
  Handle<Map> initial_map(function->initial_map());

  // Set a propery
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  JSReceiver::SetProperty(obj, prop_name, twenty_three, SLOPPY).Check();
  CHECK_EQ(Smi::FromInt(23),
           *Object::GetProperty(obj, prop_name).ToHandleChecked());

  // Check the map has changed
  CHECK(*initial_map != obj->map());
}


TEST(JSArray) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("Array");
  Handle<Object> fun_obj = Object::GetProperty(
      CcTest::i_isolate()->global_object(), name).ToHandleChecked();
  Handle<JSFunction> function = Handle<JSFunction>::cast(fun_obj);

  // Allocate the object.
  Handle<Object> element;
  Handle<JSObject> object = factory->NewJSObject(function);
  Handle<JSArray> array = Handle<JSArray>::cast(object);
  // We just initialized the VM, no heap allocation failure yet.
  JSArray::Initialize(array, 0);

  // Set array length to 0.
  JSArray::SetLength(array, 0);
  CHECK_EQ(Smi::FromInt(0), array->length());
  // Must be in fast mode.
  CHECK(array->HasFastSmiOrObjectElements());

  // array[length] = name.
  JSReceiver::SetElement(isolate, array, 0, name, SLOPPY).Check();
  CHECK_EQ(Smi::FromInt(1), array->length());
  element = i::Object::GetElement(isolate, array, 0).ToHandleChecked();
  CHECK_EQ(*element, *name);

  // Set array length with larger than smi value.
  JSArray::SetLength(array, static_cast<uint32_t>(Smi::kMaxValue) + 1);

  uint32_t int_length = 0;
  CHECK(array->length()->ToArrayIndex(&int_length));
  CHECK_EQ(static_cast<uint32_t>(Smi::kMaxValue) + 1, int_length);
  CHECK(array->HasDictionaryElements());  // Must be in slow mode.

  // array[length] = name.
  JSReceiver::SetElement(isolate, array, int_length, name, SLOPPY).Check();
  uint32_t new_int_length = 0;
  CHECK(array->length()->ToArrayIndex(&new_int_length));
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
  Handle<String> object_string(String::cast(CcTest::heap()->Object_string()));
  Handle<Object> object = Object::GetProperty(
      CcTest::i_isolate()->global_object(), object_string).ToHandleChecked();
  Handle<JSFunction> constructor = Handle<JSFunction>::cast(object);
  Handle<JSObject> obj = factory->NewJSObject(constructor);
  Handle<String> first = factory->InternalizeUtf8String("first");
  Handle<String> second = factory->InternalizeUtf8String("second");

  Handle<Smi> one(Smi::FromInt(1), isolate);
  Handle<Smi> two(Smi::FromInt(2), isolate);

  JSReceiver::SetProperty(obj, first, one, SLOPPY).Check();
  JSReceiver::SetProperty(obj, second, two, SLOPPY).Check();

  JSReceiver::SetElement(isolate, obj, 0, first, SLOPPY).Check();
  JSReceiver::SetElement(isolate, obj, 1, second, SLOPPY).Check();

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

  value1 = Object::GetProperty(obj, first).ToHandleChecked();
  value2 = Object::GetProperty(clone, first).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
  value1 = Object::GetProperty(obj, second).ToHandleChecked();
  value2 = Object::GetProperty(clone, second).ToHandleChecked();
  CHECK_EQ(*value1, *value2);

  // Flip the values.
  JSReceiver::SetProperty(clone, first, two, SLOPPY).Check();
  JSReceiver::SetProperty(clone, second, one, SLOPPY).Check();

  JSReceiver::SetElement(isolate, clone, 0, second, SLOPPY).Check();
  JSReceiver::SetElement(isolate, clone, 1, first, SLOPPY).Check();

  value1 = Object::GetElement(isolate, obj, 1).ToHandleChecked();
  value2 = Object::GetElement(isolate, clone, 0).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
  value1 = Object::GetElement(isolate, obj, 0).ToHandleChecked();
  value2 = Object::GetElement(isolate, clone, 1).ToHandleChecked();
  CHECK_EQ(*value1, *value2);

  value1 = Object::GetProperty(obj, second).ToHandleChecked();
  value2 = Object::GetProperty(clone, first).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
  value1 = Object::GetProperty(obj, first).ToHandleChecked();
  value2 = Object::GetProperty(clone, second).ToHandleChecked();
  CHECK_EQ(*value1, *value2);
}


TEST(StringAllocation) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  const unsigned char chars[] = { 0xe5, 0xa4, 0xa7 };
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
        factory->InternalizeOneByteString(OneByteVector(one_byte, length));
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
  HeapIterator iterator(heap);
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
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
      factory->NewJSArray(10, FAST_HOLEY_ELEMENTS, Strength::WEAK, TENURED);

  // Allocate a small string to OLD_DATA_SPACE and NEW_SPACE
  objs[next_objs_index++] = factory->NewStringFromStaticChars("abcdefghij");
  objs[next_objs_index++] =
      factory->NewStringFromStaticChars("abcdefghij", TENURED);

  // Allocate a large string (for large object space).
  int large_size = Page::kMaxRegularHeapObjectSize + 1;
  char* str = new char[large_size];
  for (int i = 0; i < large_size - 1; ++i) str[i] = 'a';
  str[large_size - 1] = '\0';
  objs[next_objs_index++] = factory->NewStringFromAsciiChecked(str, TENURED);
  delete[] str;

  // Add a Map object to look for.
  objs[next_objs_index++] = Handle<Map>(HeapObject::cast(*objs[0])->map());

  CHECK_EQ(objs_count, next_objs_index);
  CHECK_EQ(objs_count, ObjectsFoundInHeap(CcTest::heap(), objs, objs_count));
}


static int LenFromSize(int size) {
  return (size - FixedArray::kHeaderSize) / kPointerSize;
}


TEST(Regression39128) {
  // Test case for crbug.com/39128.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  TestHeap* heap = CcTest::test_heap();

  // Increase the chance of 'bump-the-pointer' allocation in old space.
  heap->CollectAllGarbage();

  v8::HandleScope scope(CcTest::isolate());

  // The plan: create JSObject which references objects in new space.
  // Then clone this object (forcing it to go into old space) and check
  // that region dirty marks are updated correctly.

  // Step 1: prepare a map for the object.  We add 1 inobject property to it.
  // Create a map with single inobject property.
  Handle<Map> my_map = Map::Create(CcTest::i_isolate(), 1);
  int n_properties = my_map->GetInObjectProperties();
  CHECK_GT(n_properties, 0);

  int object_size = my_map->instance_size();

  // Step 2: allocate a lot of objects so to almost fill new space: we need
  // just enough room to allocate JSObject and thus fill the newspace.

  int allocation_amount = Min(FixedArray::kMaxSize,
                              Page::kMaxRegularHeapObjectSize + kPointerSize);
  int allocation_len = LenFromSize(allocation_amount);
  NewSpace* new_space = heap->new_space();
  Address* top_addr = new_space->allocation_top_address();
  Address* limit_addr = new_space->allocation_limit_address();
  while ((*limit_addr - *top_addr) > allocation_amount) {
    CHECK(!heap->always_allocate());
    Object* array = heap->AllocateFixedArray(allocation_len).ToObjectChecked();
    CHECK(new_space->Contains(array));
  }

  // Step 3: now allocate fixed array and JSObject to fill the whole new space.
  int to_fill = static_cast<int>(*limit_addr - *top_addr - object_size);
  int fixed_array_len = LenFromSize(to_fill);
  CHECK(fixed_array_len < FixedArray::kMaxLength);

  CHECK(!heap->always_allocate());
  Object* array = heap->AllocateFixedArray(fixed_array_len).ToObjectChecked();
  CHECK(new_space->Contains(array));

  Object* object = heap->AllocateJSObjectFromMap(*my_map).ToObjectChecked();
  CHECK(new_space->Contains(object));
  JSObject* jsobject = JSObject::cast(object);
  CHECK_EQ(0, FixedArray::cast(jsobject->elements())->length());
  CHECK_EQ(0, jsobject->properties()->length());
  // Create a reference to object in new space in jsobject.
  FieldIndex index = FieldIndex::ForInObjectOffset(
      JSObject::kHeaderSize - kPointerSize);
  jsobject->FastPropertyAtPut(index, array);

  CHECK_EQ(0, static_cast<int>(*limit_addr - *top_addr));

  // Step 4: clone jsobject, but force always allocate first to create a clone
  // in old pointer space.
  Address old_space_top = heap->old_space()->top();
  AlwaysAllocateScope aa_scope(isolate);
  Object* clone_obj = heap->CopyJSObject(jsobject).ToObjectChecked();
  JSObject* clone = JSObject::cast(clone_obj);
  if (clone->address() != old_space_top) {
    // Alas, got allocated from free list, we cannot do checks.
    return;
  }
  CHECK(heap->old_space()->Contains(clone->address()));
}


UNINITIALIZED_TEST(TestCodeFlushing) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
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
    Handle<Object> func_value = Object::GetProperty(i_isolate->global_object(),
                                                    foo_name).ToHandleChecked();
    CHECK(func_value->IsJSFunction());
    Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
    CHECK(function->shared()->is_compiled());

    // The code will survive at least two GCs.
    i_isolate->heap()->CollectAllGarbage();
    i_isolate->heap()->CollectAllGarbage();
    CHECK(function->shared()->is_compiled());

    // Simulate several GCs that use full marking.
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      i_isolate->heap()->CollectAllGarbage();
    }

    // foo should no longer be in the compilation cache
    CHECK(!function->shared()->is_compiled() || function->IsOptimized());
    CHECK(!function->is_compiled() || function->IsOptimized());
    // Call foo to get it recompiled.
    CompileRun("foo()");
    CHECK(function->shared()->is_compiled());
    CHECK(function->is_compiled());
  }
  isolate->Exit();
  isolate->Dispose();
}


TEST(TestCodeFlushingPreAged) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "function foo() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // Compile foo, but don't run it.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Handle<Object> func_value =
      Object::GetProperty(isolate->global_object(), foo_name).ToHandleChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
  CHECK(function->shared()->is_compiled());

  // The code has been run so will survive at least one GC.
  CcTest::heap()->CollectAllGarbage();
  CHECK(function->shared()->is_compiled());

  // The code was only run once, so it should be pre-aged and collected on the
  // next GC.
  CcTest::heap()->CollectAllGarbage();
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());

  // Execute the function again twice, and ensure it is reset to the young age.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("foo();"
               "foo();");
  }

  // The code will survive at least two GC now that it is young again.
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CHECK(function->shared()->is_compiled());

  // Simulate several GCs that use full marking.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    CcTest::heap()->CollectAllGarbage();
  }

  // foo should no longer be in the compilation cache
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());
  CHECK(!function->is_compiled() || function->IsOptimized());
  // Call foo to get it recompiled.
  CompileRun("foo()");
  CHECK(function->shared()->is_compiled());
  CHECK(function->is_compiled());
}


TEST(TestCodeFlushingIncremental) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "function foo() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Handle<Object> func_value =
      Object::GetProperty(isolate->global_object(), foo_name).ToHandleChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
  CHECK(function->shared()->is_compiled());

  // The code will survive at least two GCs.
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CHECK(function->shared()->is_compiled());

  // Simulate several GCs that use incremental marking.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    SimulateIncrementalMarking(CcTest::heap());
    CcTest::heap()->CollectAllGarbage();
  }
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());
  CHECK(!function->is_compiled() || function->IsOptimized());

  // This compile will compile the function again.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("foo();");
  }

  // Simulate several GCs that use incremental marking but make sure
  // the loop breaks once the function is enqueued as a candidate.
  for (int i = 0; i < kAgingThreshold; i++) {
    SimulateIncrementalMarking(CcTest::heap());
    if (!function->next_function_link()->IsUndefined()) break;
    CcTest::heap()->CollectAllGarbage();
  }

  // Force optimization while incremental marking is active and while
  // the function is enqueued as a candidate.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("%OptimizeFunctionOnNextCall(foo); foo();");
  }

  // Simulate one final GC to make sure the candidate queue is sane.
  CcTest::heap()->CollectAllGarbage();
  CHECK(function->shared()->is_compiled() || !function->IsOptimized());
  CHECK(function->is_compiled() || !function->IsOptimized());
}


TEST(TestCodeFlushingIncrementalScavenge) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "var foo = function() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo();"
                       "var bar = function() {"
                       "  var x = 23;"
                       "};"
                       "bar();";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");
  Handle<String> bar_name = factory->InternalizeUtf8String("bar");

  // Perfrom one initial GC to enable code flushing.
  CcTest::heap()->CollectAllGarbage();

  // This compile will add the code to the compilation cache.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check functions are compiled.
  Handle<Object> func_value =
      Object::GetProperty(isolate->global_object(), foo_name).ToHandleChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
  CHECK(function->shared()->is_compiled());
  Handle<Object> func_value2 =
      Object::GetProperty(isolate->global_object(), bar_name).ToHandleChecked();
  CHECK(func_value2->IsJSFunction());
  Handle<JSFunction> function2 = Handle<JSFunction>::cast(func_value2);
  CHECK(function2->shared()->is_compiled());

  // Clear references to functions so that one of them can die.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("foo = 0; bar = 0;");
  }

  // Bump the code age so that flushing is triggered while the function
  // object is still located in new-space.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    function->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    function2->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
  }

  // Simulate incremental marking so that the functions are enqueued as
  // code flushing candidates. Then kill one of the functions. Finally
  // perform a scavenge while incremental marking is still running.
  SimulateIncrementalMarking(CcTest::heap());
  *function2.location() = NULL;
  CcTest::heap()->CollectGarbage(NEW_SPACE, "test scavenge while marking");

  // Simulate one final GC to make sure the candidate queue is sane.
  CcTest::heap()->CollectAllGarbage();
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());
  CHECK(!function->is_compiled() || function->IsOptimized());
}


TEST(TestCodeFlushingIncrementalAbort) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "function foo() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Handle<Object> func_value =
      Object::GetProperty(isolate->global_object(), foo_name).ToHandleChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function = Handle<JSFunction>::cast(func_value);
  CHECK(function->shared()->is_compiled());

  // The code will survive at least two GCs.
  heap->CollectAllGarbage();
  heap->CollectAllGarbage();
  CHECK(function->shared()->is_compiled());

  // Bump the code age so that flushing is triggered.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    function->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
  }

  // Simulate incremental marking so that the function is enqueued as
  // code flushing candidate.
  SimulateIncrementalMarking(heap);

  // Enable the debugger and add a breakpoint while incremental marking
  // is running so that incremental marking aborts and code flushing is
  // disabled.
  int position = 0;
  Handle<Object> breakpoint_object(Smi::FromInt(0), isolate);
  EnableDebugger();
  isolate->debug()->SetBreakPoint(function, breakpoint_object, &position);
  isolate->debug()->ClearAllBreakPoints();
  DisableDebugger();

  // Force optimization now that code flushing is disabled.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("%OptimizeFunctionOnNextCall(foo); foo();");
  }

  // Simulate one final GC to make sure the candidate queue is sane.
  heap->CollectAllGarbage();
  CHECK(function->shared()->is_compiled() || !function->IsOptimized());
  CHECK(function->is_compiled() || !function->IsOptimized());
}


TEST(CompilationCacheCachingBehavior) {
  // If we do not flush code, or have the compilation cache turned off, this
  // test is invalid.
  if (!FLAG_flush_code || !FLAG_compilation_cache) {
    return;
  }
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  CompilationCache* compilation_cache = isolate->compilation_cache();
  LanguageMode language_mode =
      construct_language_mode(FLAG_use_strict, FLAG_use_strong);

  v8::HandleScope scope(CcTest::isolate());
  const char* raw_source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo()";
  Handle<String> source = factory->InternalizeUtf8String(raw_source);
  Handle<Context> native_context = isolate->native_context();

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(raw_source);
  }

  // On first compilation, only a hash is inserted in the code cache. We can't
  // find that value.
  MaybeHandle<SharedFunctionInfo> info = compilation_cache->LookupScript(
      source, Handle<Object>(), 0, 0,
      v8::ScriptOriginOptions(false, true, false), native_context,
      language_mode);
  CHECK(info.is_null());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(raw_source);
  }

  // On second compilation, the hash is replaced by a real cache entry mapping
  // the source to the shared function info containing the code.
  info = compilation_cache->LookupScript(
      source, Handle<Object>(), 0, 0,
      v8::ScriptOriginOptions(false, true, false), native_context,
      language_mode);
  CHECK(!info.is_null());

  heap->CollectAllGarbage();

  // On second compilation, the hash is replaced by a real cache entry mapping
  // the source to the shared function info containing the code.
  info = compilation_cache->LookupScript(
      source, Handle<Object>(), 0, 0,
      v8::ScriptOriginOptions(false, true, false), native_context,
      language_mode);
  CHECK(!info.is_null());

  while (!info.ToHandleChecked()->code()->IsOld()) {
    info.ToHandleChecked()->code()->MakeOlder(NO_MARKING_PARITY);
  }

  heap->CollectAllGarbage();
  // Ensure code aging cleared the entry from the cache.
  info = compilation_cache->LookupScript(
      source, Handle<Object>(), 0, 0,
      v8::ScriptOriginOptions(false, true, false), native_context,
      language_mode);
  CHECK(info.is_null());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(raw_source);
  }

  // On first compilation, only a hash is inserted in the code cache. We can't
  // find that value.
  info = compilation_cache->LookupScript(
      source, Handle<Object>(), 0, 0,
      v8::ScriptOriginOptions(false, true, false), native_context,
      language_mode);
  CHECK(info.is_null());

  for (int i = 0; i < CompilationCacheTable::kHashGenerations; i++) {
    compilation_cache->MarkCompactPrologue();
  }

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(raw_source);
  }

  // If we aged the cache before caching the script, ensure that we didn't cache
  // on next compilation.
  info = compilation_cache->LookupScript(
      source, Handle<Object>(), 0, 0,
      v8::ScriptOriginOptions(false, true, false), native_context,
      language_mode);
  CHECK(info.is_null());
}


static void OptimizeEmptyFunction(const char* name) {
  HandleScope scope(CcTest::i_isolate());
  EmbeddedVector<char, 256> source;
  SNPrintF(source,
           "function %s() { return 0; }"
           "%s(); %s();"
           "%%OptimizeFunctionOnNextCall(%s);"
           "%s();",
           name, name, name, name, name);
  CompileRun(source.start());
}


// Count the number of native contexts in the weak list of native contexts.
int CountNativeContexts() {
  int count = 0;
  Object* object = CcTest::heap()->native_contexts_list();
  while (!object->IsUndefined()) {
    count++;
    object = Context::cast(object)->get(Context::NEXT_CONTEXT_LINK);
  }
  // Subtract one to compensate for the code stub context that is always present
  return count - 1;
}


// Count the number of user functions in the weak list of optimized
// functions attached to a native context.
static int CountOptimizedUserFunctions(v8::Handle<v8::Context> context) {
  int count = 0;
  Handle<Context> icontext = v8::Utils::OpenHandle(*context);
  Object* object = icontext->get(Context::OPTIMIZED_FUNCTIONS_LIST);
  while (object->IsJSFunction() && !JSFunction::cast(object)->IsBuiltin()) {
    count++;
    object = JSFunction::cast(object)->next_function_link();
  }
  return count;
}


TEST(TestInternalWeakLists) {
  FLAG_always_opt = false;
  FLAG_allow_natives_syntax = true;
  v8::V8::Initialize();

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (FLAG_gc_global || FLAG_stress_compaction) return;
  FLAG_retain_maps_for_n_gc = 0;

  static const int kNumTestContexts = 10;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  v8::Handle<v8::Context> ctx[kNumTestContexts];
  if (!isolate->use_crankshaft()) return;

  CHECK_EQ(0, CountNativeContexts());

  // Create a number of global contests which gets linked together.
  for (int i = 0; i < kNumTestContexts; i++) {
    ctx[i] = v8::Context::New(CcTest::isolate());

    // Collect garbage that might have been created by one of the
    // installed extensions.
    isolate->compilation_cache()->Clear();
    heap->CollectAllGarbage();

    CHECK_EQ(i + 1, CountNativeContexts());

    ctx[i]->Enter();

    // Create a handle scope so no function objects get stuck in the outer
    // handle scope.
    HandleScope scope(isolate);
    CHECK_EQ(0, CountOptimizedUserFunctions(ctx[i]));
    OptimizeEmptyFunction("f1");
    CHECK_EQ(1, CountOptimizedUserFunctions(ctx[i]));
    OptimizeEmptyFunction("f2");
    CHECK_EQ(2, CountOptimizedUserFunctions(ctx[i]));
    OptimizeEmptyFunction("f3");
    CHECK_EQ(3, CountOptimizedUserFunctions(ctx[i]));
    OptimizeEmptyFunction("f4");
    CHECK_EQ(4, CountOptimizedUserFunctions(ctx[i]));
    OptimizeEmptyFunction("f5");
    CHECK_EQ(5, CountOptimizedUserFunctions(ctx[i]));

    // Remove function f1, and
    CompileRun("f1=null");

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->CollectGarbage(NEW_SPACE);
      CHECK_EQ(5, CountOptimizedUserFunctions(ctx[i]));
    }

    // Mark compact handles the weak references.
    isolate->compilation_cache()->Clear();
    heap->CollectAllGarbage();
    CHECK_EQ(4, CountOptimizedUserFunctions(ctx[i]));

    // Get rid of f3 and f5 in the same way.
    CompileRun("f3=null");
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->CollectGarbage(NEW_SPACE);
      CHECK_EQ(4, CountOptimizedUserFunctions(ctx[i]));
    }
    CcTest::heap()->CollectAllGarbage();
    CHECK_EQ(3, CountOptimizedUserFunctions(ctx[i]));
    CompileRun("f5=null");
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->CollectGarbage(NEW_SPACE);
      CHECK_EQ(3, CountOptimizedUserFunctions(ctx[i]));
    }
    CcTest::heap()->CollectAllGarbage();
    CHECK_EQ(2, CountOptimizedUserFunctions(ctx[i]));

    ctx[i]->Exit();
  }

  // Force compilation cache cleanup.
  CcTest::heap()->NotifyContextDisposed(true);
  CcTest::heap()->CollectAllGarbage();

  // Dispose the native contexts one by one.
  for (int i = 0; i < kNumTestContexts; i++) {
    // TODO(dcarney): is there a better way to do this?
    i::Object** unsafe = reinterpret_cast<i::Object**>(*ctx[i]);
    *unsafe = CcTest::heap()->undefined_value();
    ctx[i].Clear();

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->CollectGarbage(i::NEW_SPACE);
      CHECK_EQ(kNumTestContexts - i, CountNativeContexts());
    }

    // Mark compact handles the weak references.
    CcTest::heap()->CollectAllGarbage();
    CHECK_EQ(kNumTestContexts - i - 1, CountNativeContexts());
  }

  CHECK_EQ(0, CountNativeContexts());
}


// Count the number of native contexts in the weak list of native contexts
// causing a GC after the specified number of elements.
static int CountNativeContextsWithGC(Isolate* isolate, int n) {
  Heap* heap = isolate->heap();
  int count = 0;
  Handle<Object> object(heap->native_contexts_list(), isolate);
  while (!object->IsUndefined()) {
    count++;
    if (count == n) heap->CollectAllGarbage();
    object =
        Handle<Object>(Context::cast(*object)->get(Context::NEXT_CONTEXT_LINK),
                       isolate);
  }
  // Subtract one to compensate for the code stub context that is always present
  return count - 1;
}


// Count the number of user functions in the weak list of optimized
// functions attached to a native context causing a GC after the
// specified number of elements.
static int CountOptimizedUserFunctionsWithGC(v8::Handle<v8::Context> context,
                                             int n) {
  int count = 0;
  Handle<Context> icontext = v8::Utils::OpenHandle(*context);
  Isolate* isolate = icontext->GetIsolate();
  Handle<Object> object(icontext->get(Context::OPTIMIZED_FUNCTIONS_LIST),
                        isolate);
  while (object->IsJSFunction() &&
         !Handle<JSFunction>::cast(object)->IsBuiltin()) {
    count++;
    if (count == n) isolate->heap()->CollectAllGarbage();
    object = Handle<Object>(
        Object::cast(JSFunction::cast(*object)->next_function_link()),
        isolate);
  }
  return count;
}


TEST(TestInternalWeakListsTraverseWithGC) {
  FLAG_always_opt = false;
  FLAG_allow_natives_syntax = true;
  v8::V8::Initialize();

  static const int kNumTestContexts = 10;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::Handle<v8::Context> ctx[kNumTestContexts];
  if (!isolate->use_crankshaft()) return;

  CHECK_EQ(0, CountNativeContexts());

  // Create an number of contexts and check the length of the weak list both
  // with and without GCs while iterating the list.
  for (int i = 0; i < kNumTestContexts; i++) {
    ctx[i] = v8::Context::New(CcTest::isolate());
    CHECK_EQ(i + 1, CountNativeContexts());
    CHECK_EQ(i + 1, CountNativeContextsWithGC(isolate, i / 2 + 1));
  }

  ctx[0]->Enter();

  // Compile a number of functions the length of the weak list of optimized
  // functions both with and without GCs while iterating the list.
  CHECK_EQ(0, CountOptimizedUserFunctions(ctx[0]));
  OptimizeEmptyFunction("f1");
  CHECK_EQ(1, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(1, CountOptimizedUserFunctionsWithGC(ctx[0], 1));
  OptimizeEmptyFunction("f2");
  CHECK_EQ(2, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(2, CountOptimizedUserFunctionsWithGC(ctx[0], 1));
  OptimizeEmptyFunction("f3");
  CHECK_EQ(3, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(3, CountOptimizedUserFunctionsWithGC(ctx[0], 1));
  OptimizeEmptyFunction("f4");
  CHECK_EQ(4, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(4, CountOptimizedUserFunctionsWithGC(ctx[0], 2));
  OptimizeEmptyFunction("f5");
  CHECK_EQ(5, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(5, CountOptimizedUserFunctionsWithGC(ctx[0], 4));

  ctx[0]->Exit();
}


TEST(TestSizeOfRegExpCode) {
  if (!FLAG_regexp_optimization) return;

  v8::V8::Initialize();

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  LocalContext context;

  // Adjust source below and this check to match
  // RegExpImple::kRegExpTooLargeToOptimize.
  DCHECK_EQ(i::RegExpImpl::kRegExpTooLargeToOptimize, 20 * KB);

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
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  int initial_size = static_cast<int>(CcTest::heap()->SizeOfObjects());

  CompileRun("'foo'.match(reg_exp_source);");
  CcTest::heap()->CollectAllGarbage();
  int size_with_regexp = static_cast<int>(CcTest::heap()->SizeOfObjects());

  CompileRun("'foo'.match(half_size_reg_exp);");
  CcTest::heap()->CollectAllGarbage();
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


TEST(TestSizeOfObjects) {
  v8::V8::Initialize();

  // Get initial heap size after several full GCs, which will stabilize
  // the heap size and return with sweeping finished completely.
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage();
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  int initial_size = static_cast<int>(CcTest::heap()->SizeOfObjects());

  {
    // Allocate objects on several different old-space pages so that
    // concurrent sweeper threads will be busy sweeping the old space on
    // subsequent GC runs.
    AlwaysAllocateScope always_allocate(CcTest::i_isolate());
    int filler_size = static_cast<int>(FixedArray::SizeFor(8192));
    for (int i = 1; i <= 100; i++) {
      CcTest::test_heap()->AllocateFixedArray(8192, TENURED).ToObjectChecked();
      CHECK_EQ(initial_size + i * filler_size,
               static_cast<int>(CcTest::heap()->SizeOfObjects()));
    }
  }

  // The heap size should go back to initial size after a full GC, even
  // though sweeping didn't finish yet.
  CcTest::heap()->CollectAllGarbage();

  // Normally sweeping would not be complete here, but no guarantees.

  CHECK_EQ(initial_size, static_cast<int>(CcTest::heap()->SizeOfObjects()));

  // Waiting for sweeper threads should not change heap size.
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK_EQ(initial_size, static_cast<int>(CcTest::heap()->SizeOfObjects()));
}


TEST(TestAlignmentCalculations) {
  // Maximum fill amounts are consistent.
  int maximum_double_misalignment = kDoubleSize - kPointerSize;
  int maximum_simd128_misalignment = kSimd128Size - kPointerSize;
  int max_word_fill = Heap::GetMaximumFillToAlign(kWordAligned);
  CHECK_EQ(0, max_word_fill);
  int max_double_fill = Heap::GetMaximumFillToAlign(kDoubleAligned);
  CHECK_EQ(maximum_double_misalignment, max_double_fill);
  int max_double_unaligned_fill = Heap::GetMaximumFillToAlign(kDoubleUnaligned);
  CHECK_EQ(maximum_double_misalignment, max_double_unaligned_fill);
  int max_simd128_unaligned_fill =
      Heap::GetMaximumFillToAlign(kSimd128Unaligned);
  CHECK_EQ(maximum_simd128_misalignment, max_simd128_unaligned_fill);

  Address base = static_cast<Address>(NULL);
  int fill = 0;

  // Word alignment never requires fill.
  fill = Heap::GetFillToAlign(base, kWordAligned);
  CHECK_EQ(0, fill);
  fill = Heap::GetFillToAlign(base + kPointerSize, kWordAligned);
  CHECK_EQ(0, fill);

  // No fill is required when address is double aligned.
  fill = Heap::GetFillToAlign(base, kDoubleAligned);
  CHECK_EQ(0, fill);
  // Fill is required if address is not double aligned.
  fill = Heap::GetFillToAlign(base + kPointerSize, kDoubleAligned);
  CHECK_EQ(maximum_double_misalignment, fill);
  // kDoubleUnaligned has the opposite fill amounts.
  fill = Heap::GetFillToAlign(base, kDoubleUnaligned);
  CHECK_EQ(maximum_double_misalignment, fill);
  fill = Heap::GetFillToAlign(base + kPointerSize, kDoubleUnaligned);
  CHECK_EQ(0, fill);

  // 128 bit SIMD types have 2 or 4 possible alignments, depending on platform.
  fill = Heap::GetFillToAlign(base, kSimd128Unaligned);
  CHECK_EQ((3 * kPointerSize) & kSimd128AlignmentMask, fill);
  fill = Heap::GetFillToAlign(base + kPointerSize, kSimd128Unaligned);
  CHECK_EQ((2 * kPointerSize) & kSimd128AlignmentMask, fill);
  fill = Heap::GetFillToAlign(base + 2 * kPointerSize, kSimd128Unaligned);
  CHECK_EQ(kPointerSize, fill);
  fill = Heap::GetFillToAlign(base + 3 * kPointerSize, kSimd128Unaligned);
  CHECK_EQ(0, fill);
}


static HeapObject* NewSpaceAllocateAligned(int size,
                                           AllocationAlignment alignment) {
  Heap* heap = CcTest::heap();
  AllocationResult allocation =
      heap->new_space()->AllocateRawAligned(size, alignment);
  HeapObject* obj = NULL;
  allocation.To(&obj);
  heap->CreateFillerObjectAt(obj->address(), size);
  return obj;
}


// Get new space allocation into the desired alignment.
static Address AlignNewSpace(AllocationAlignment alignment, int offset) {
  Address* top_addr = CcTest::heap()->new_space()->allocation_top_address();
  int fill = Heap::GetFillToAlign(*top_addr, alignment);
  if (fill) {
    NewSpaceAllocateAligned(fill + offset, kWordAligned);
  }
  return *top_addr;
}


TEST(TestAlignedAllocation) {
  // Double misalignment is 4 on 32-bit platforms, 0 on 64-bit ones.
  const intptr_t double_misalignment = kDoubleSize - kPointerSize;
  Address* top_addr = CcTest::heap()->new_space()->allocation_top_address();
  Address start;
  HeapObject* obj;
  HeapObject* filler;
  if (double_misalignment) {
    // Allocate a pointer sized object that must be double aligned at an
    // aligned address.
    start = AlignNewSpace(kDoubleAligned, 0);
    obj = NewSpaceAllocateAligned(kPointerSize, kDoubleAligned);
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment));
    // There is no filler.
    CHECK_EQ(kPointerSize, *top_addr - start);

    // Allocate a second pointer sized object that must be double aligned at an
    // unaligned address.
    start = AlignNewSpace(kDoubleAligned, kPointerSize);
    obj = NewSpaceAllocateAligned(kPointerSize, kDoubleAligned);
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler && filler->IsFiller() &&
          filler->Size() == kPointerSize);
    CHECK_EQ(kPointerSize + double_misalignment, *top_addr - start);

    // Similarly for kDoubleUnaligned.
    start = AlignNewSpace(kDoubleUnaligned, 0);
    obj = NewSpaceAllocateAligned(kPointerSize, kDoubleUnaligned);
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment, kPointerSize));
    CHECK_EQ(kPointerSize, *top_addr - start);
    start = AlignNewSpace(kDoubleUnaligned, kPointerSize);
    obj = NewSpaceAllocateAligned(kPointerSize, kDoubleUnaligned);
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment, kPointerSize));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler && filler->IsFiller() &&
          filler->Size() == kPointerSize);
    CHECK_EQ(kPointerSize + double_misalignment, *top_addr - start);
  }

  // Now test SIMD alignment. There are 2 or 4 possible alignments, depending
  // on platform.
  start = AlignNewSpace(kSimd128Unaligned, 0);
  obj = NewSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
  CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
  // There is no filler.
  CHECK_EQ(kPointerSize, *top_addr - start);
  start = AlignNewSpace(kSimd128Unaligned, kPointerSize);
  obj = NewSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
  CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
  // There is a filler object before the object.
  filler = HeapObject::FromAddress(start);
  CHECK(obj != filler && filler->IsFiller() &&
        filler->Size() == kSimd128Size - kPointerSize);
  CHECK_EQ(kPointerSize + kSimd128Size - kPointerSize, *top_addr - start);

  if (double_misalignment) {
    // Test the 2 other alignments possible on 32 bit platforms.
    start = AlignNewSpace(kSimd128Unaligned, 2 * kPointerSize);
    obj = NewSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
    CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler && filler->IsFiller() &&
          filler->Size() == 2 * kPointerSize);
    CHECK_EQ(kPointerSize + 2 * kPointerSize, *top_addr - start);
    start = AlignNewSpace(kSimd128Unaligned, 3 * kPointerSize);
    obj = NewSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
    CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    CHECK(obj != filler && filler->IsFiller() &&
          filler->Size() == kPointerSize);
    CHECK_EQ(kPointerSize + kPointerSize, *top_addr - start);
  }
}


static HeapObject* OldSpaceAllocateAligned(int size,
                                           AllocationAlignment alignment) {
  Heap* heap = CcTest::heap();
  AllocationResult allocation =
      heap->old_space()->AllocateRawAligned(size, alignment);
  HeapObject* obj = NULL;
  allocation.To(&obj);
  heap->CreateFillerObjectAt(obj->address(), size);
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
  CcTest::heap()->old_space()->EmptyAllocationInfo();
  return top;
}


// Test the case where allocation must be done from the free list, so filler
// may precede or follow the object.
TEST(TestAlignedOverAllocation) {
  // Double misalignment is 4 on 32-bit platforms, 0 on 64-bit ones.
  const intptr_t double_misalignment = kDoubleSize - kPointerSize;
  Address start;
  HeapObject* obj;
  HeapObject* filler1;
  HeapObject* filler2;
  if (double_misalignment) {
    start = AlignOldSpace(kDoubleAligned, 0);
    obj = OldSpaceAllocateAligned(kPointerSize, kDoubleAligned);
    // The object is aligned, and a filler object is created after.
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment));
    filler1 = HeapObject::FromAddress(start + kPointerSize);
    CHECK(obj != filler1 && filler1->IsFiller() &&
          filler1->Size() == kPointerSize);
    // Try the opposite alignment case.
    start = AlignOldSpace(kDoubleAligned, kPointerSize);
    obj = OldSpaceAllocateAligned(kPointerSize, kDoubleAligned);
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment));
    filler1 = HeapObject::FromAddress(start);
    CHECK(obj != filler1);
    CHECK(filler1->IsFiller());
    CHECK(filler1->Size() == kPointerSize);
    CHECK(obj != filler1 && filler1->IsFiller() &&
          filler1->Size() == kPointerSize);

    // Similarly for kDoubleUnaligned.
    start = AlignOldSpace(kDoubleUnaligned, 0);
    obj = OldSpaceAllocateAligned(kPointerSize, kDoubleUnaligned);
    // The object is aligned, and a filler object is created after.
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment, kPointerSize));
    filler1 = HeapObject::FromAddress(start + kPointerSize);
    CHECK(obj != filler1 && filler1->IsFiller() &&
          filler1->Size() == kPointerSize);
    // Try the opposite alignment case.
    start = AlignOldSpace(kDoubleUnaligned, kPointerSize);
    obj = OldSpaceAllocateAligned(kPointerSize, kDoubleUnaligned);
    CHECK(IsAddressAligned(obj->address(), kDoubleAlignment, kPointerSize));
    filler1 = HeapObject::FromAddress(start);
    CHECK(obj != filler1 && filler1->IsFiller() &&
          filler1->Size() == kPointerSize);
  }

  // Now test SIMD alignment. There are 2 or 4 possible alignments, depending
  // on platform.
  start = AlignOldSpace(kSimd128Unaligned, 0);
  obj = OldSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
  CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
  // There is a filler object after the object.
  filler1 = HeapObject::FromAddress(start + kPointerSize);
  CHECK(obj != filler1 && filler1->IsFiller() &&
        filler1->Size() == kSimd128Size - kPointerSize);
  start = AlignOldSpace(kSimd128Unaligned, kPointerSize);
  obj = OldSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
  CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
  // There is a filler object before the object.
  filler1 = HeapObject::FromAddress(start);
  CHECK(obj != filler1 && filler1->IsFiller() &&
        filler1->Size() == kSimd128Size - kPointerSize);

  if (double_misalignment) {
    // Test the 2 other alignments possible on 32 bit platforms.
    start = AlignOldSpace(kSimd128Unaligned, 2 * kPointerSize);
    obj = OldSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
    CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
    // There are filler objects before and after the object.
    filler1 = HeapObject::FromAddress(start);
    CHECK(obj != filler1 && filler1->IsFiller() &&
          filler1->Size() == 2 * kPointerSize);
    filler2 = HeapObject::FromAddress(start + 3 * kPointerSize);
    CHECK(obj != filler2 && filler2->IsFiller() &&
          filler2->Size() == kPointerSize);
    start = AlignOldSpace(kSimd128Unaligned, 3 * kPointerSize);
    obj = OldSpaceAllocateAligned(kPointerSize, kSimd128Unaligned);
    CHECK(IsAddressAligned(obj->address(), kSimd128Alignment, kPointerSize));
    // There are filler objects before and after the object.
    filler1 = HeapObject::FromAddress(start);
    CHECK(obj != filler1 && filler1->IsFiller() &&
          filler1->Size() == kPointerSize);
    filler2 = HeapObject::FromAddress(start + 2 * kPointerSize);
    CHECK(obj != filler2 && filler2->IsFiller() &&
          filler2->Size() == 2 * kPointerSize);
  }
}


TEST(TestSizeOfObjectsVsHeapIteratorPrecision) {
  CcTest::InitializeVM();
  HeapIterator iterator(CcTest::heap());
  intptr_t size_of_objects_1 = CcTest::heap()->SizeOfObjects();
  intptr_t size_of_objects_2 = 0;
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next()) {
    if (!obj->IsFreeSpace()) {
      size_of_objects_2 += obj->Size();
    }
  }
  // Delta must be within 5% of the larger result.
  // TODO(gc): Tighten this up by distinguishing between byte
  // arrays that are real and those that merely mark free space
  // on the heap.
  if (size_of_objects_1 > size_of_objects_2) {
    intptr_t delta = size_of_objects_1 - size_of_objects_2;
    PrintF("Heap::SizeOfObjects: %" V8_PTR_PREFIX "d, "
           "Iterator: %" V8_PTR_PREFIX "d, "
           "delta: %" V8_PTR_PREFIX "d\n",
           size_of_objects_1, size_of_objects_2, delta);
    CHECK_GT(size_of_objects_1 / 20, delta);
  } else {
    intptr_t delta = size_of_objects_2 - size_of_objects_1;
    PrintF("Heap::SizeOfObjects: %" V8_PTR_PREFIX "d, "
           "Iterator: %" V8_PTR_PREFIX "d, "
           "delta: %" V8_PTR_PREFIX "d\n",
           size_of_objects_1, size_of_objects_2, delta);
    CHECK_GT(size_of_objects_2 / 20, delta);
  }
}


static void FillUpNewSpace(NewSpace* new_space) {
  // Fill up new space to the point that it is completely full. Make sure
  // that the scavenger does not undo the filling.
  Heap* heap = new_space->heap();
  Isolate* isolate = heap->isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  AlwaysAllocateScope always_allocate(isolate);
  intptr_t available = new_space->Capacity() - new_space->Size();
  intptr_t number_of_fillers = (available / FixedArray::SizeFor(32)) - 1;
  for (intptr_t i = 0; i < number_of_fillers; i++) {
    CHECK(heap->InNewSpace(*factory->NewFixedArray(32, NOT_TENURED)));
  }
}


TEST(GrowAndShrinkNewSpace) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  NewSpace* new_space = heap->new_space();

  if (heap->ReservedSemiSpaceSize() == heap->InitialSemiSpaceSize() ||
      heap->MaxSemiSpaceSize() == heap->InitialSemiSpaceSize()) {
    // The max size cannot exceed the reserved size, since semispaces must be
    // always within the reserved space.  We can't test new space growing and
    // shrinking if the reserved size is the same as the minimum (initial) size.
    return;
  }

  // Explicitly growing should double the space capacity.
  intptr_t old_capacity, new_capacity;
  old_capacity = new_space->TotalCapacity();
  new_space->Grow();
  new_capacity = new_space->TotalCapacity();
  CHECK(2 * old_capacity == new_capacity);

  old_capacity = new_space->TotalCapacity();
  FillUpNewSpace(new_space);
  new_capacity = new_space->TotalCapacity();
  CHECK(old_capacity == new_capacity);

  // Explicitly shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  CHECK(old_capacity == new_capacity);

  // Let the scavenger empty the new space.
  heap->CollectGarbage(NEW_SPACE);
  CHECK_LE(new_space->Size(), old_capacity);

  // Explicitly shrinking should halve the space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  CHECK(old_capacity == 2 * new_capacity);

  // Consecutive shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_space->Shrink();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  CHECK(old_capacity == new_capacity);
}


TEST(CollectingAllAvailableGarbageShrinksNewSpace) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  if (heap->ReservedSemiSpaceSize() == heap->InitialSemiSpaceSize() ||
      heap->MaxSemiSpaceSize() == heap->InitialSemiSpaceSize()) {
    // The max size cannot exceed the reserved size, since semispaces must be
    // always within the reserved space.  We can't test new space growing and
    // shrinking if the reserved size is the same as the minimum (initial) size.
    return;
  }

  v8::HandleScope scope(CcTest::isolate());
  NewSpace* new_space = heap->new_space();
  intptr_t old_capacity, new_capacity;
  old_capacity = new_space->TotalCapacity();
  new_space->Grow();
  new_capacity = new_space->TotalCapacity();
  CHECK(2 * old_capacity == new_capacity);
  FillUpNewSpace(new_space);
  heap->CollectAllAvailableGarbage();
  new_capacity = new_space->TotalCapacity();
  CHECK(old_capacity == new_capacity);
}


static int NumberOfGlobalObjects() {
  int count = 0;
  HeapIterator iterator(CcTest::heap());
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsGlobalObject()) count++;
  }
  // Subtract two to compensate for the two global objects (not global
  // JSObjects, of which there would only be one) that are part of the code stub
  // context, which is always present.
  return count - 2;
}


// Test that we don't embed maps from foreign contexts into
// optimized code.
TEST(LeakNativeContextViaMap) {
  i::FLAG_allow_natives_syntax = true;
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

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = {x: 42}");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f() { return o.x; }"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    v8::Local<v8::Context>::New(isolate, ctx1)->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


// Test that we don't embed functions from foreign contexts into
// optimized code.
TEST(LeakNativeContextViaFunction) {
  i::FLAG_allow_natives_syntax = true;
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

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = function() { return 42; }");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f(x) { return x(); }"
        "for (var i = 0; i < 10; ++i) f(o);"
        "%OptimizeFunctionOnNextCall(f);"
        "f(o);");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(LeakNativeContextViaMapKeyed) {
  i::FLAG_allow_natives_syntax = true;
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

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = [42, 43]");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f() { return o[0]; }"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(LeakNativeContextViaMapProto) {
  i::FLAG_allow_natives_syntax = true;
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

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = { y: 42}");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f() {"
        "  var p = {x: 42};"
        "  p.__proto__ = o;"
        "  return p.x;"
        "}"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(InstanceOfStubWriteBarrier) {
  i::FLAG_allow_natives_syntax = true;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  if (i::FLAG_force_marking_deque_overflows) return;
  v8::HandleScope outer_scope(CcTest::isolate());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "function foo () { }"
        "function mkbar () { return new (new Function(\"\")) (); }"
        "function f (x) { return (x instanceof foo); }"
        "function g () { f(mkbar()); }"
        "f(new foo()); f(new foo());"
        "%OptimizeFunctionOnNextCall(f);"
        "f(new foo()); g();");
  }

  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start(Heap::kNoGCFlags);

  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));

  CHECK(f->IsOptimized());

  while (!Marking::IsBlack(Marking::MarkBitFrom(f->code())) &&
         !marking->IsStopped()) {
    // Discard any pending GC requests otherwise we will get GC when we enter
    // code below.
    marking->Step(MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
  }

  CHECK(marking->IsMarking());

  {
    v8::HandleScope scope(CcTest::isolate());
    v8::Handle<v8::Object> global = CcTest::global();
    v8::Handle<v8::Function> g =
        v8::Handle<v8::Function>::Cast(global->Get(v8_str("g")));
    g->Call(global, 0, NULL);
  }

  CcTest::heap()->incremental_marking()->set_should_hurry(true);
  CcTest::heap()->CollectGarbage(OLD_SPACE);
}


static int NumberOfProtoTransitions(Map* map) {
  return TransitionArray::NumberOfPrototypeTransitions(
      TransitionArray::GetPrototypeTransitions(map));
}


TEST(PrototypeTransitionClearing) {
  if (FLAG_never_compact) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun("var base = {};");
  Handle<JSObject> baseObject =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("base"))));
  int initialTransitions = NumberOfProtoTransitions(baseObject->map());

  CompileRun(
      "var live = [];"
      "for (var i = 0; i < 10; i++) {"
      "  var object = {};"
      "  var prototype = {};"
      "  object.__proto__ = prototype;"
      "  if (i >= 3) live.push(object, prototype);"
      "}");

  // Verify that only dead prototype transitions are cleared.
  CHECK_EQ(initialTransitions + 10,
           NumberOfProtoTransitions(baseObject->map()));
  CcTest::heap()->CollectAllGarbage();
  const int transitions = 10 - 3;
  CHECK_EQ(initialTransitions + transitions,
           NumberOfProtoTransitions(baseObject->map()));

  // Verify that prototype transitions array was compacted.
  FixedArray* trans =
      TransitionArray::GetPrototypeTransitions(baseObject->map());
  for (int i = initialTransitions; i < initialTransitions + transitions; i++) {
    int j = TransitionArray::kProtoTransitionHeaderSize + i;
    CHECK(trans->get(j)->IsWeakCell());
    CHECK(WeakCell::cast(trans->get(j))->value()->IsMap());
  }

  // Make sure next prototype is placed on an old-space evacuation candidate.
  Handle<JSObject> prototype;
  PagedSpace* space = CcTest::heap()->old_space();
  {
    AlwaysAllocateScope always_allocate(isolate);
    SimulateFullSpace(space);
    prototype = factory->NewJSArray(32 * KB, FAST_HOLEY_ELEMENTS,
                                    Strength::WEAK, TENURED);
  }

  // Add a prototype on an evacuation candidate and verify that transition
  // clearing correctly records slots in prototype transition array.
  i::FLAG_always_compact = true;
  Handle<Map> map(baseObject->map());
  CHECK(!space->LastPage()->Contains(
      TransitionArray::GetPrototypeTransitions(*map)->address()));
  CHECK(space->LastPage()->Contains(prototype->address()));
}


TEST(ResetSharedFunctionInfoCountersDuringIncrementalMarking) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  v8::HandleScope outer_scope(CcTest::isolate());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "function f () {"
        "  var s = 0;"
        "  for (var i = 0; i < 100; i++)  s += i;"
        "  return s;"
        "}"
        "f(); f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
  }
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));
  CHECK(f->IsOptimized());

  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start(Heap::kNoGCFlags);
  // The following calls will increment CcTest::heap()->global_ic_age().
  CcTest::isolate()->ContextDisposedNotification();
  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();
  CHECK_EQ(CcTest::heap()->global_ic_age(), f->shared()->ic_age());
  CHECK_EQ(0, f->shared()->opt_count());
  CHECK_EQ(0, f->shared()->code()->profiler_ticks());
}


TEST(ResetSharedFunctionInfoCountersDuringMarkSweep) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  v8::HandleScope outer_scope(CcTest::isolate());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "function f () {"
        "  var s = 0;"
        "  for (var i = 0; i < 100; i++)  s += i;"
        "  return s;"
        "}"
        "f(); f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
  }
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));
  CHECK(f->IsOptimized());

  CcTest::heap()->incremental_marking()->Stop();

  // The following two calls will increment CcTest::heap()->global_ic_age().
  CcTest::isolate()->ContextDisposedNotification();
  CcTest::heap()->CollectAllGarbage();

  CHECK_EQ(CcTest::heap()->global_ic_age(), f->shared()->ic_age());
  CHECK_EQ(0, f->shared()->opt_count());
  CHECK_EQ(0, f->shared()->code()->profiler_ticks());
}


HEAP_TEST(GCFlags) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();

  heap->set_current_gc_flags(Heap::kNoGCFlags);
  CHECK_EQ(Heap::kNoGCFlags, heap->current_gc_flags());

  // Set the flags to check whether we appropriately resets them after the GC.
  heap->set_current_gc_flags(Heap::kAbortIncrementalMarkingMask);
  heap->CollectAllGarbage(Heap::kReduceMemoryFootprintMask);
  CHECK_EQ(Heap::kNoGCFlags, heap->current_gc_flags());

  MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }

  IncrementalMarking* marking = heap->incremental_marking();
  marking->Stop();
  marking->Start(Heap::kReduceMemoryFootprintMask);
  CHECK_NE(0, heap->current_gc_flags() & Heap::kReduceMemoryFootprintMask);

  heap->CollectGarbage(NEW_SPACE);
  // NewSpace scavenges should not overwrite the flags.
  CHECK_NE(0, heap->current_gc_flags() & Heap::kReduceMemoryFootprintMask);

  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK_EQ(Heap::kNoGCFlags, heap->current_gc_flags());
}


TEST(IdleNotificationFinishMarking) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  SimulateFullSpace(CcTest::heap()->old_space());
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start(Heap::kNoGCFlags);

  CHECK_EQ(CcTest::heap()->gc_count(), 0);

  // TODO(hpayer): We cannot write proper unit test right now for heap.
  // The ideal test would call kMaxIdleMarkingDelayCounter to test the
  // marking delay counter.

  // Perform a huge incremental marking step but don't complete marking.
  intptr_t bytes_processed = 0;
  do {
    bytes_processed =
        marking->Step(1 * MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                      IncrementalMarking::FORCE_MARKING,
                      IncrementalMarking::DO_NOT_FORCE_COMPLETION);
    CHECK(!marking->IsIdleMarkingDelayCounterLimitReached());
  } while (bytes_processed);

  // The next invocations of incremental marking are not going to complete
  // marking
  // since the completion threshold is not reached
  for (size_t i = 0; i < IncrementalMarking::kMaxIdleMarkingDelayCounter - 2;
       i++) {
    marking->Step(1 * MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                  IncrementalMarking::FORCE_MARKING,
                  IncrementalMarking::DO_NOT_FORCE_COMPLETION);
    CHECK(!marking->IsIdleMarkingDelayCounterLimitReached());
  }

  marking->SetWeakClosureWasOverApproximatedForTesting(true);

  // The next idle notification has to finish incremental marking.
  const double kLongIdleTime = 1000.0;
  CcTest::isolate()->IdleNotificationDeadline(
      (v8::base::TimeTicks::HighResolutionNow().ToInternalValue() /
       static_cast<double>(v8::base::Time::kMicrosecondsPerSecond)) +
      kLongIdleTime);
  CHECK_EQ(CcTest::heap()->gc_count(), 1);
}


// Test that HAllocateObject will always return an object in new-space.
TEST(OptimizedAllocationAlwaysInNewSpace) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  SimulateFullSpace(CcTest::heap()->new_space());
  AlwaysAllocateScope always_allocate(CcTest::i_isolate());
  v8::Local<v8::Value> res = CompileRun(
      "function c(x) {"
      "  this.x = x;"
      "  for (var i = 0; i < 32; i++) {"
      "    this['x' + i] = x;"
      "  }"
      "}"
      "function f(x) { return new c(x); };"
      "f(1); f(2); f(3);"
      "%OptimizeFunctionOnNextCall(f);"
      "f(4);");
  CHECK_EQ(
      4, res.As<v8::Object>()->GetRealNamedProperty(v8_str("x"))->Int32Value());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InNewSpace(*o));
}


TEST(OptimizedPretenuringAllocationFolding) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array();"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [[{}], [1.1]];"
      "  }"
      "  return elements[number_elements-1]"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated);

  v8::Local<v8::Value> res = CompileRun(source.start());

  v8::Local<v8::Value> int_array = v8::Object::Cast(*res)->Get(v8_str("0"));
  Handle<JSObject> int_array_handle =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(int_array));
  v8::Local<v8::Value> double_array = v8::Object::Cast(*res)->Get(v8_str("1"));
  Handle<JSObject> double_array_handle =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(double_array));

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle->elements()));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle->elements()));
}


TEST(OptimizedPretenuringObjectArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [{}, {}, {}];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated);

  v8::Local<v8::Value> res = CompileRun(source.start());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldSpace(o->elements()));
  CHECK(CcTest::heap()->InOldSpace(*o));
}


TEST(OptimizedPretenuringMixedInObjectProperties) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }


  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = {a: {c: 2.2, d: {}}, b: 1.1};"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated);

  v8::Local<v8::Value> res = CompileRun(source.start());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldSpace(*o));
  FieldIndex idx1 = FieldIndex::ForPropertyIndex(o->map(), 0);
  FieldIndex idx2 = FieldIndex::ForPropertyIndex(o->map(), 1);
  CHECK(CcTest::heap()->InOldSpace(o->RawFastPropertyAt(idx1)));
  if (!o->IsUnboxedDoubleField(idx2)) {
    CHECK(CcTest::heap()->InOldSpace(o->RawFastPropertyAt(idx2)));
  } else {
    CHECK_EQ(1.1, o->RawFastDoublePropertyAt(idx2));
  }

  JSObject* inner_object =
      reinterpret_cast<JSObject*>(o->RawFastPropertyAt(idx1));
  CHECK(CcTest::heap()->InOldSpace(inner_object));
  if (!inner_object->IsUnboxedDoubleField(idx1)) {
    CHECK(CcTest::heap()->InOldSpace(inner_object->RawFastPropertyAt(idx1)));
  } else {
    CHECK_EQ(2.2, inner_object->RawFastDoublePropertyAt(idx1));
  }
  CHECK(CcTest::heap()->InOldSpace(inner_object->RawFastPropertyAt(idx2)));
}


TEST(OptimizedPretenuringDoubleArrayProperties) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = {a: 1.1, b: 2.2};"
      "  }"
      "  return elements[i - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated);

  v8::Local<v8::Value> res = CompileRun(source.start());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(o->properties()));
}


TEST(OptimizedPretenuringdoubleArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [1.1, 2.2, 3.3];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated);

  v8::Local<v8::Value> res = CompileRun(source.start());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldSpace(o->elements()));
  CHECK(CcTest::heap()->InOldSpace(*o));
}


TEST(OptimizedPretenuringNestedMixedArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = 100;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [[{}, {}, {}], [1.1, 2.2, 3.3]];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();");

  v8::Local<v8::Value> res = CompileRun(source.start());

  v8::Local<v8::Value> int_array = v8::Object::Cast(*res)->Get(v8_str("0"));
  Handle<JSObject> int_array_handle =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(int_array));
  v8::Local<v8::Value> double_array = v8::Object::Cast(*res)->Get(v8_str("1"));
  Handle<JSObject> double_array_handle =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(double_array));

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle->elements()));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle->elements()));
}


TEST(OptimizedPretenuringNestedObjectLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [[{}, {}, {}],[{}, {}, {}]];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated);

  v8::Local<v8::Value> res = CompileRun(source.start());

  v8::Local<v8::Value> int_array_1 = v8::Object::Cast(*res)->Get(v8_str("0"));
  Handle<JSObject> int_array_handle_1 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(int_array_1));
  v8::Local<v8::Value> int_array_2 = v8::Object::Cast(*res)->Get(v8_str("1"));
  Handle<JSObject> int_array_handle_2 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(int_array_2));

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle_1));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle_1->elements()));
  CHECK(CcTest::heap()->InOldSpace(*int_array_handle_2));
  CHECK(CcTest::heap()->InOldSpace(int_array_handle_2->elements()));
}


TEST(OptimizedPretenuringNestedDoubleLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [[1.1, 1.2, 1.3],[2.1, 2.2, 2.3]];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated);

  v8::Local<v8::Value> res = CompileRun(source.start());

  v8::Local<v8::Value> double_array_1 =
      v8::Object::Cast(*res)->Get(v8_str("0"));
  Handle<JSObject> double_array_handle_1 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(double_array_1));
  v8::Local<v8::Value> double_array_2 =
      v8::Object::Cast(*res)->Get(v8_str("1"));
  Handle<JSObject> double_array_handle_2 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(double_array_2));

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldSpace(*o));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle_1));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle_1->elements()));
  CHECK(CcTest::heap()->InOldSpace(*double_array_handle_2));
  CHECK(CcTest::heap()->InOldSpace(double_array_handle_2->elements()));
}


// Make sure pretenuring feedback is gathered for constructed objects as well
// as for literals.
TEST(OptimizedPretenuringConstructorCalls) {
  if (!i::FLAG_pretenuring_call_new) {
    // FLAG_pretenuring_call_new needs to be synced with the snapshot.
    return;
  }
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  // Call new is doing slack tracking for the first
  // JSFunction::kGenerousAllocationCount allocations, and we can't find
  // mementos during that time.
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function foo() {"
      "  this.a = 3;"
      "  this.b = {};"
      "}"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = new foo();"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated +
      JSFunction::kGenerousAllocationCount);

  v8::Local<v8::Value> res = CompileRun(source.start());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldSpace(*o));
}


TEST(OptimizedPretenuringCallNew) {
  if (!i::FLAG_pretenuring_call_new) {
    // FLAG_pretenuring_call_new needs to be synced with the snapshot.
    return;
  }
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  // Grow new space unitl maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  // Call new is doing slack tracking for the first
  // JSFunction::kGenerousAllocationCount allocations, and we can't find
  // mementos during that time.
  i::SNPrintF(
      source,
      "var number_elements = %d;"
      "var elements = new Array(number_elements);"
      "function g() { this.a = 0; }"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = new g();"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); gc();"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      AllocationSite::kPretenureMinimumCreated +
      JSFunction::kGenerousAllocationCount);

  v8::Local<v8::Value> res = CompileRun(source.start());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldSpace(*o));
}


// Test regular array literals allocation.
TEST(OptimizedAllocationArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "function f() {"
      "  var numbers = new Array(1, 2, 3);"
      "  numbers[0] = 3.14;"
      "  return numbers;"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");
  CHECK_EQ(static_cast<int>(3.14),
           v8::Object::Cast(*res)->Get(v8_str("0"))->Int32Value());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InNewSpace(o->elements()));
}


static int CountMapTransitions(Map* map) {
  return TransitionArray::NumberOfTransitions(map->raw_transitions());
}


// Test that map transitions are cleared and maps are collected with
// incremental marking as well.
TEST(Regress1465) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_trace_incremental_marking = true;
  i::FLAG_retain_maps_for_n_gc = 0;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 256;

  CompileRun("function F() {}");
  {
    AlwaysAllocateScope always_allocate(CcTest::i_isolate());
    for (int i = 0; i < transitions_count; i++) {
      EmbeddedVector<char, 64> buffer;
      SNPrintF(buffer, "var o = new F; o.prop%d = %d;", i, i);
      CompileRun(buffer.start());
    }
    CompileRun("var root = new F;");
  }

  Handle<JSObject> root =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("root"))));

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(root->map());
  CompileRun("%DebugPrint(root);");
  CHECK_EQ(transitions_count, transitions_before);

  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after = CountMapTransitions(root->map());
  CompileRun("%DebugPrint(root);");
  CHECK_EQ(1, transitions_after);
}


#ifdef DEBUG
static void AddTransitions(int transitions_count) {
  AlwaysAllocateScope always_allocate(CcTest::i_isolate());
  for (int i = 0; i < transitions_count; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "var o = new F; o.prop%d = %d;", i, i);
    CompileRun(buffer.start());
  }
}


static Handle<JSObject> GetByName(const char* name) {
  return v8::Utils::OpenHandle(
      *v8::Handle<v8::Object>::Cast(
          CcTest::global()->Get(v8_str(name))));
}


static void AddPropertyTo(
    int gc_count, Handle<JSObject> object, const char* property_name) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<String> prop_name = factory->InternalizeUtf8String(property_name);
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  i::FLAG_gc_interval = gc_count;
  i::FLAG_gc_global = true;
  i::FLAG_retain_maps_for_n_gc = 0;
  CcTest::heap()->set_allocation_timeout(gc_count);
  JSReceiver::SetProperty(object, prop_name, twenty_three, SLOPPY).Check();
}


TEST(TransitionArrayShrinksDuringAllocToZero) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 10;
  CompileRun("function F() { }");
  AddTransitions(transitions_count);
  CompileRun("var root = new F;");
  Handle<JSObject> root = GetByName("root");

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(root->map());
  CHECK_EQ(transitions_count, transitions_before);

  // Get rid of o
  CompileRun("o = new F;"
             "root = new F");
  root = GetByName("root");
  AddPropertyTo(2, root, "funny");
  CcTest::heap()->CollectGarbage(NEW_SPACE);

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after = CountMapTransitions(
      Map::cast(root->map()->GetBackPointer()));
  CHECK_EQ(1, transitions_after);
}


TEST(TransitionArrayShrinksDuringAllocToOne) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 10;
  CompileRun("function F() {}");
  AddTransitions(transitions_count);
  CompileRun("var root = new F;");
  Handle<JSObject> root = GetByName("root");

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(root->map());
  CHECK_EQ(transitions_count, transitions_before);

  root = GetByName("root");
  AddPropertyTo(2, root, "funny");
  CcTest::heap()->CollectGarbage(NEW_SPACE);

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after = CountMapTransitions(
      Map::cast(root->map()->GetBackPointer()));
  CHECK_EQ(2, transitions_after);
}


TEST(TransitionArrayShrinksDuringAllocToOnePropertyFound) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 10;
  CompileRun("function F() {}");
  AddTransitions(transitions_count);
  CompileRun("var root = new F;");
  Handle<JSObject> root = GetByName("root");

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(root->map());
  CHECK_EQ(transitions_count, transitions_before);

  root = GetByName("root");
  AddPropertyTo(0, root, "prop9");
  CcTest::i_isolate()->heap()->CollectGarbage(OLD_SPACE);

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after = CountMapTransitions(
      Map::cast(root->map()->GetBackPointer()));
  CHECK_EQ(1, transitions_after);
}


TEST(TransitionArraySimpleToFull) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 1;
  CompileRun("function F() {}");
  AddTransitions(transitions_count);
  CompileRun("var root = new F;");
  Handle<JSObject> root = GetByName("root");

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(root->map());
  CHECK_EQ(transitions_count, transitions_before);

  CompileRun("o = new F;"
             "root = new F");
  root = GetByName("root");
  DCHECK(TransitionArray::IsSimpleTransition(root->map()->raw_transitions()));
  AddPropertyTo(2, root, "happy");

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after = CountMapTransitions(
      Map::cast(root->map()->GetBackPointer()));
  CHECK_EQ(1, transitions_after);
}
#endif  // DEBUG


TEST(Regress2143a) {
  i::FLAG_incremental_marking = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Prepare a map transition from the root object together with a yet
  // untransitioned root object.
  CompileRun("var root = new Object;"
             "root.foo = 0;"
             "root = new Object;");

  SimulateIncrementalMarking(CcTest::heap());

  // Compile a StoreIC that performs the prepared map transition. This
  // will restart incremental marking and should make sure the root is
  // marked grey again.
  CompileRun("function f(o) {"
             "  o.foo = 0;"
             "}"
             "f(new Object);"
             "f(root);");

  // This bug only triggers with aggressive IC clearing.
  CcTest::heap()->AgeInlineCaches();

  // Explicitly request GC to perform final marking step and sweeping.
  CcTest::heap()->CollectAllGarbage();

  Handle<JSObject> root =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("root"))));

  // The root object should be in a sane state.
  CHECK(root->IsJSObject());
  CHECK(root->map()->IsMap());
}


TEST(Regress2143b) {
  i::FLAG_incremental_marking = true;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Prepare a map transition from the root object together with a yet
  // untransitioned root object.
  CompileRun("var root = new Object;"
             "root.foo = 0;"
             "root = new Object;");

  SimulateIncrementalMarking(CcTest::heap());

  // Compile an optimized LStoreNamedField that performs the prepared
  // map transition. This will restart incremental marking and should
  // make sure the root is marked grey again.
  CompileRun("function f(o) {"
             "  o.foo = 0;"
             "}"
             "f(new Object);"
             "f(new Object);"
             "%OptimizeFunctionOnNextCall(f);"
             "f(root);"
             "%DeoptimizeFunction(f);");

  // This bug only triggers with aggressive IC clearing.
  CcTest::heap()->AgeInlineCaches();

  // Explicitly request GC to perform final marking step and sweeping.
  CcTest::heap()->CollectAllGarbage();

  Handle<JSObject> root =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("root"))));

  // The root object should be in a sane state.
  CHECK(root->IsJSObject());
  CHECK(root->map()->IsMap());
}


TEST(ReleaseOverReservedPages) {
  if (FLAG_never_compact) return;
  i::FLAG_trace_gc = true;
  // The optimizer can allocate stuff, messing up the test.
  i::FLAG_crankshaft = false;
  i::FLAG_always_opt = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());
  static const int number_of_test_pages = 20;

  // Prepare many pages with low live-bytes count.
  PagedSpace* old_space = heap->old_space();
  CHECK_EQ(1, old_space->CountTotalPages());
  for (int i = 0; i < number_of_test_pages; i++) {
    AlwaysAllocateScope always_allocate(isolate);
    SimulateFullSpace(old_space);
    factory->NewFixedArray(1, TENURED);
  }
  CHECK_EQ(number_of_test_pages + 1, old_space->CountTotalPages());

  // Triggering one GC will cause a lot of garbage to be discovered but
  // even spread across all allocated pages.
  heap->CollectAllGarbage(Heap::kFinalizeIncrementalMarkingMask,
                          "triggered for preparation");
  CHECK_GE(number_of_test_pages + 1, old_space->CountTotalPages());

  // Triggering subsequent GCs should cause at least half of the pages
  // to be released to the OS after at most two cycles.
  heap->CollectAllGarbage(Heap::kFinalizeIncrementalMarkingMask,
                          "triggered by test 1");
  CHECK_GE(number_of_test_pages + 1, old_space->CountTotalPages());
  heap->CollectAllGarbage(Heap::kFinalizeIncrementalMarkingMask,
                          "triggered by test 2");
  CHECK_GE(number_of_test_pages + 1, old_space->CountTotalPages() * 2);

  // Triggering a last-resort GC should cause all pages to be released to the
  // OS so that other processes can seize the memory.  If we get a failure here
  // where there are 2 pages left instead of 1, then we should increase the
  // size of the first page a little in SizeOfFirstPage in spaces.cc.  The
  // first page should be small in order to reduce memory used when the VM
  // boots, but if the 20 small arrays don't fit on the first page then that's
  // an indication that it is too small.
  heap->CollectAllAvailableGarbage("triggered really hard");
  CHECK_EQ(1, old_space->CountTotalPages());
}

static int forced_gc_counter = 0;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  isolate->GetCallingContext();
  if (feature == v8::Isolate::kForcedGC) {
    forced_gc_counter++;
  }
}


TEST(CountForcedGC) {
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  isolate->SetUseCounterCallback(MockUseCounterCallback);

  forced_gc_counter = 0;
  const char* source = "gc();";
  CompileRun(source);
  CHECK_GT(forced_gc_counter, 0);
}


TEST(Regress2237) {
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  Handle<String> slice(CcTest::heap()->empty_string());

  {
    // Generate a parent that lives in new-space.
    v8::HandleScope inner_scope(CcTest::isolate());
    const char* c = "This text is long enough to trigger sliced strings.";
    Handle<String> s = factory->NewStringFromAsciiChecked(c);
    CHECK(s->IsSeqOneByteString());
    CHECK(CcTest::heap()->InNewSpace(*s));

    // Generate a sliced string that is based on the above parent and
    // lives in old-space.
    SimulateFullSpace(CcTest::heap()->new_space());
    AlwaysAllocateScope always_allocate(isolate);
    Handle<String> t = factory->NewProperSubString(s, 5, 35);
    CHECK(t->IsSlicedString());
    CHECK(!CcTest::heap()->InNewSpace(*t));
    *slice.location() = *t.location();
  }

  CHECK(SlicedString::cast(*slice)->parent()->IsSeqOneByteString());
  CcTest::heap()->CollectAllGarbage();
  CHECK(SlicedString::cast(*slice)->parent()->IsSeqOneByteString());
}


#ifdef OBJECT_PRINT
TEST(PrintSharedFunctionInfo) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "f = function() { return 987654321; }\n"
                       "g = function() { return 123456789; }\n";
  CompileRun(source);
  Handle<JSFunction> g =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("g"))));

  OFStream os(stdout);
  g->shared()->Print(os);
  os << std::endl;
}
#endif  // OBJECT_PRINT


TEST(IncrementalMarkingPreservesMonomorphicCallIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> fun1, fun2;

  {
    LocalContext env;
    CompileRun("function fun() {};");
    fun1 = env->Global()->Get(v8_str("fun"));
  }

  {
    LocalContext env;
    CompileRun("function fun() {};");
    fun2 = env->Global()->Get(v8_str("fun"));
  }

  // Prepare function f that contains type feedback for closures
  // originating from two different native contexts.
  CcTest::global()->Set(v8_str("fun1"), fun1);
  CcTest::global()->Set(v8_str("fun2"), fun2);
  CompileRun("function f(a, b) { a(); b(); } f(fun1, fun2);");

  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));

  Handle<TypeFeedbackVector> feedback_vector(f->shared()->feedback_vector());

  int expected_slots = 2;
  CHECK_EQ(expected_slots, feedback_vector->ICSlots());
  int slot1 = 0;
  int slot2 = 1;
  CHECK(feedback_vector->Get(FeedbackVectorICSlot(slot1))->IsWeakCell());
  CHECK(feedback_vector->Get(FeedbackVectorICSlot(slot2))->IsWeakCell());

  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  CHECK(!WeakCell::cast(feedback_vector->Get(FeedbackVectorICSlot(slot1)))
             ->cleared());
  CHECK(!WeakCell::cast(feedback_vector->Get(FeedbackVectorICSlot(slot2)))
             ->cleared());
}


static Code* FindFirstIC(Code* code, Code::Kind kind) {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::CONSTRUCT_CALL) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Code* target = Code::GetCodeFromTargetAddress(info->target_address());
    if (target->is_inline_cache_stub() && target->kind() == kind) {
      return target;
    }
  }
  return NULL;
}


static void CheckVectorIC(Handle<JSFunction> f, int ic_slot_index,
                          InlineCacheState desired_state) {
  Handle<TypeFeedbackVector> vector =
      Handle<TypeFeedbackVector>(f->shared()->feedback_vector());
  FeedbackVectorICSlot slot(ic_slot_index);
  if (vector->GetKind(slot) == Code::LOAD_IC) {
    LoadICNexus nexus(vector, slot);
    CHECK(nexus.StateFromFeedback() == desired_state);
  } else {
    CHECK(vector->GetKind(slot) == Code::KEYED_LOAD_IC);
    KeyedLoadICNexus nexus(vector, slot);
    CHECK(nexus.StateFromFeedback() == desired_state);
  }
}


static void CheckVectorICCleared(Handle<JSFunction> f, int ic_slot_index) {
  Handle<TypeFeedbackVector> vector =
      Handle<TypeFeedbackVector>(f->shared()->feedback_vector());
  FeedbackVectorICSlot slot(ic_slot_index);
  LoadICNexus nexus(vector, slot);
  CHECK(IC::IsCleared(&nexus));
}


TEST(ICInBuiltInIsClearedAppropriately) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  Handle<JSFunction> apply;
  {
    LocalContext env;
    v8::Local<v8::Value> res = CompileRun("Function.apply");
    Handle<JSObject> maybe_apply =
        v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
    apply = Handle<JSFunction>::cast(maybe_apply);
    TypeFeedbackVector* vector = apply->shared()->feedback_vector();
    CHECK(vector->ICSlots() == 1);
    CheckVectorIC(apply, 0, UNINITIALIZED);
    CompileRun(
        "function b(a1, a2, a3) { return a1 + a2 + a3; }"
        "function fun(bar) { bar.apply({}, [1, 2, 3]); };"
        "fun(b); fun(b)");
    CheckVectorIC(apply, 0, MONOMORPHIC);
  }

  // Fire context dispose notification.
  CcTest::isolate()->ContextDisposedNotification();
  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  // The IC in apply has been cleared, ready to learn again.
  CheckVectorIC(apply, 0, PREMONOMORPHIC);
}


TEST(IncrementalMarkingPreservesMonomorphicConstructor) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Prepare function f that contains a monomorphic IC for object
  // originating from the same native context.
  CompileRun(
      "function fun() { this.x = 1; };"
      "function f(o) { return new o(); } f(fun); f(fun);");
  Handle<JSFunction> f = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CcTest::global()->Get(v8_str("f"))));


  Handle<TypeFeedbackVector> vector(f->shared()->feedback_vector());
  CHECK(vector->Get(FeedbackVectorSlot(0))->IsWeakCell());

  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  CHECK(vector->Get(FeedbackVectorSlot(0))->IsWeakCell());
}


TEST(IncrementalMarkingClearsMonomorphicConstructor) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> fun1;

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; };");
    fun1 = env->Global()->Get(v8_str("fun"));
  }

  // Prepare function f that contains a monomorphic constructor for object
  // originating from a different native context.
  CcTest::global()->Set(v8_str("fun1"), fun1);
  CompileRun(
      "function fun() { this.x = 1; };"
      "function f(o) { return new o(); } f(fun1); f(fun1);");
  Handle<JSFunction> f = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CcTest::global()->Get(v8_str("f"))));


  Handle<TypeFeedbackVector> vector(f->shared()->feedback_vector());
  CHECK(vector->Get(FeedbackVectorSlot(0))->IsWeakCell());

  // Fire context dispose notification.
  CcTest::isolate()->ContextDisposedNotification();
  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  CHECK_EQ(*TypeFeedbackVector::UninitializedSentinel(isolate),
           vector->Get(FeedbackVectorSlot(0)));
}


TEST(IncrementalMarkingPreservesMonomorphicIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Prepare function f that contains a monomorphic IC for object
  // originating from the same native context.
  CompileRun("function fun() { this.x = 1; }; var obj = new fun();"
             "function f(o) { return o.x; } f(obj); f(obj);");
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));

  Code* ic_before = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CheckVectorIC(f, 0, MONOMORPHIC);
  CHECK(ic_before->ic_state() == DEFAULT);

  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  Code* ic_after = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CheckVectorIC(f, 0, MONOMORPHIC);
  CHECK(ic_after->ic_state() == DEFAULT);
}


TEST(IncrementalMarkingClearsMonomorphicIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1;

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(v8_str("obj"));
  }

  // Prepare function f that contains a monomorphic IC for object
  // originating from a different native context.
  CcTest::global()->Set(v8_str("obj1"), obj1);
  CompileRun("function f(o) { return o.x; } f(obj1); f(obj1);");
  Handle<JSFunction> f = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CcTest::global()->Get(v8_str("f"))));

  Code* ic_before = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CheckVectorIC(f, 0, MONOMORPHIC);
  CHECK(ic_before->ic_state() == DEFAULT);

  // Fire context dispose notification.
  CcTest::isolate()->ContextDisposedNotification();
  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  Code* ic_after = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CheckVectorICCleared(f, 0);
  CHECK(ic_after->ic_state() == DEFAULT);
}


TEST(IncrementalMarkingPreservesPolymorphicIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1, obj2;

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(v8_str("obj"));
  }

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 2; }; var obj = new fun();");
    obj2 = env->Global()->Get(v8_str("obj"));
  }

  // Prepare function f that contains a polymorphic IC for objects
  // originating from two different native contexts.
  CcTest::global()->Set(v8_str("obj1"), obj1);
  CcTest::global()->Set(v8_str("obj2"), obj2);
  CompileRun("function f(o) { return o.x; } f(obj1); f(obj1); f(obj2);");
  Handle<JSFunction> f = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CcTest::global()->Get(v8_str("f"))));

  Code* ic_before = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CheckVectorIC(f, 0, POLYMORPHIC);
  CHECK(ic_before->ic_state() == DEFAULT);

  // Fire context dispose notification.
  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  Code* ic_after = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CheckVectorIC(f, 0, POLYMORPHIC);
  CHECK(ic_after->ic_state() == DEFAULT);
}


TEST(IncrementalMarkingClearsPolymorphicIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1, obj2;

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(v8_str("obj"));
  }

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 2; }; var obj = new fun();");
    obj2 = env->Global()->Get(v8_str("obj"));
  }

  // Prepare function f that contains a polymorphic IC for objects
  // originating from two different native contexts.
  CcTest::global()->Set(v8_str("obj1"), obj1);
  CcTest::global()->Set(v8_str("obj2"), obj2);
  CompileRun("function f(o) { return o.x; } f(obj1); f(obj1); f(obj2);");
  Handle<JSFunction> f = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CcTest::global()->Get(v8_str("f"))));

  Code* ic_before = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CheckVectorIC(f, 0, POLYMORPHIC);
  CHECK(ic_before->ic_state() == DEFAULT);

  // Fire context dispose notification.
  CcTest::isolate()->ContextDisposedNotification();
  SimulateIncrementalMarking(CcTest::heap());
  CcTest::heap()->CollectAllGarbage();

  CheckVectorICCleared(f, 0);
  CHECK(ic_before->ic_state() == DEFAULT);
}


class SourceResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit SourceResource(const char* data)
    : data_(data), length_(strlen(data)) { }

  virtual void Dispose() {
    i::DeleteArray(data_);
    data_ = NULL;
  }

  const char* data() const { return data_; }

  size_t length() const { return length_; }

  bool IsDisposed() { return data_ == NULL; }

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
    v8::Handle<v8::String> source_string =
        v8::String::NewExternal(isolate, resource);
    i_isolate->heap()->CollectAllAvailableGarbage();
    v8::Script::Compile(source_string)->Run();
    CHECK(!resource->IsDisposed());
  }
  // i_isolate->heap()->CollectAllAvailableGarbage();
  CHECK(!resource->IsDisposed());

  CompileRun(accessor);
  i_isolate->heap()->CollectAllAvailableGarbage();

  // External source has been released.
  CHECK(resource->IsDisposed());
  delete resource;
}


UNINITIALIZED_TEST(ReleaseStackTraceData) {
  if (i::FLAG_always_opt) {
    // TODO(ulan): Remove this once the memory leak via code_next_link is fixed.
    // See: https://codereview.chromium.org/181833004/
    return;
  }
  FLAG_use_ic = false;  // ICs retain objects.
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


TEST(Regress159140) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage();

  // Prepare several closures that are all eligible for code flushing
  // because all reachable ones are not optimized. Make sure that the
  // optimized code object is directly reachable through a handle so
  // that it is marked black during incremental marking.
  Handle<Code> code;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function h(x) {}"
               "function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "var g = mkClosure();"
               "f(1); f(2);"
               "g(1); g(2);"
               "h(1); h(2);"
               "%OptimizeFunctionOnNextCall(f); f(3);"
               "%OptimizeFunctionOnNextCall(h); h(3);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    CompileRun("f = null;");

    Handle<JSFunction> g =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("g"))));
    CHECK(g->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      g->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    code = inner_scope.CloseAndEscape(Handle<Code>(f->code()));
  }

  // Simulate incremental marking so that the functions are enqueued as
  // code flushing candidates. Then optimize one function. Finally
  // finish the GC to complete code flushing.
  SimulateIncrementalMarking(heap);
  CompileRun("%OptimizeFunctionOnNextCall(g); g(3);");
  heap->CollectAllGarbage();

  // Unoptimized code is missing and the deoptimizer will go ballistic.
  CompileRun("g('bozo');");
}


TEST(Regress165495) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage();

  // Prepare an optimized closure that the optimized code map will get
  // populated. Then age the unoptimized code to trigger code flushing
  // but make sure the optimized code is unreachable.
  {
    HandleScope inner_scope(isolate);
    CompileRun("function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "f(1); f(2);"
               "%OptimizeFunctionOnNextCall(f); f(3);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    CompileRun("f = null;");
  }

  // Simulate incremental marking so that unoptimized code is flushed
  // even though it still is cached in the optimized code map.
  SimulateIncrementalMarking(heap);
  heap->CollectAllGarbage();

  // Make a new closure that will get code installed from the code map.
  // Unoptimized code is missing and the deoptimizer will go ballistic.
  CompileRun("var g = mkClosure(); g('bozo');");
}


TEST(Regress169209) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage();

  // Prepare a shared function info eligible for code flushing for which
  // the unoptimized code will be replaced during optimization.
  Handle<SharedFunctionInfo> shared1;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function f() { return 'foobar'; }"
               "function g(x) { if (x) f(); }"
               "f();"
               "g(false);"
               "g(false);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    shared1 = inner_scope.CloseAndEscape(handle(f->shared(), isolate));
  }

  // Prepare a shared function info eligible for code flushing that will
  // represent the dangling tail of the candidate list.
  Handle<SharedFunctionInfo> shared2;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function flushMe() { return 0; }"
               "flushMe(1);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("flushMe"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    shared2 = inner_scope.CloseAndEscape(handle(f->shared(), isolate));
  }

  // Simulate incremental marking and collect code flushing candidates.
  SimulateIncrementalMarking(heap);
  CHECK(shared1->code()->gc_metadata() != NULL);

  // Optimize function and make sure the unoptimized code is replaced.
#ifdef DEBUG
  FLAG_stop_at = "f";
#endif
  CompileRun("%OptimizeFunctionOnNextCall(g);"
             "g(false);");

  // Finish garbage collection cycle.
  heap->CollectAllGarbage();
  CHECK(shared1->code()->gc_metadata() == NULL);
}


TEST(Regress169928) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_crankshaft = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (FLAG_gc_global || FLAG_stress_compaction) return;

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
  CcTest::global()->Set(array_name, v8::Int32::New(CcTest::isolate(), 0));

  // First make sure we flip spaces
  CcTest::heap()->CollectGarbage(NEW_SPACE);

  // Allocate the object.
  Handle<FixedArray> array_data = factory->NewFixedArray(2, NOT_TENURED);
  array_data->set(0, Smi::FromInt(1));
  array_data->set(1, Smi::FromInt(2));

  AllocateAllButNBytes(CcTest::heap()->new_space(),
                       JSArray::kSize + AllocationMemento::kSize +
                       kPointerSize);

  Handle<JSArray> array =
      factory->NewJSArrayWithElements(array_data, FAST_SMI_ELEMENTS);

  CHECK_EQ(Smi::FromInt(2), array->length());
  CHECK(array->HasFastSmiOrObjectElements());

  // We need filler the size of AllocationMemento object, plus an extra
  // fill pointer value.
  HeapObject* obj = NULL;
  AllocationResult allocation =
      CcTest::heap()->new_space()->AllocateRawUnaligned(
          AllocationMemento::kSize + kPointerSize);
  CHECK(allocation.To(&obj));
  Address addr_obj = obj->address();
  CcTest::heap()->CreateFillerObjectAt(
      addr_obj, AllocationMemento::kSize + kPointerSize);

  // Give the array a name, making sure not to allocate strings.
  v8::Handle<v8::Object> array_obj = v8::Utils::ToLocal(array);
  CcTest::global()->Set(array_name, array_obj);

  // This should crash with a protection violation if we are running a build
  // with the bug.
  AlwaysAllocateScope aa_scope(isolate);
  v8::Script::Compile(mote_code_string)->Run();
}


TEST(Regress168801) {
  if (i::FLAG_never_compact) return;
  i::FLAG_always_compact = true;
  i::FLAG_cache_optimized_code = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage();

  // Ensure the code ends up on an evacuation candidate.
  SimulateFullSpace(heap->code_space());

  // Prepare an unoptimized function that is eligible for code flushing.
  Handle<JSFunction> function;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "f(1); f(2);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    function = inner_scope.CloseAndEscape(handle(*f, isolate));
  }

  // Simulate incremental marking so that unoptimized function is enqueued as a
  // candidate for code flushing. The shared function info however will not be
  // explicitly enqueued.
  SimulateIncrementalMarking(heap);

  // Now optimize the function so that it is taken off the candidate list.
  {
    HandleScope inner_scope(isolate);
    CompileRun("%OptimizeFunctionOnNextCall(f); f(3);");
  }

  // This cycle will bust the heap and subsequent cycles will go ballistic.
  heap->CollectAllGarbage();
  heap->CollectAllGarbage();
}


TEST(Regress173458) {
  if (i::FLAG_never_compact) return;
  i::FLAG_always_compact = true;
  i::FLAG_cache_optimized_code = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage();

  // Ensure the code ends up on an evacuation candidate.
  SimulateFullSpace(heap->code_space());

  // Prepare an unoptimized function that is eligible for code flushing.
  Handle<JSFunction> function;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "f(1); f(2);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    function = inner_scope.CloseAndEscape(handle(*f, isolate));
  }

  // Simulate incremental marking so that unoptimized function is enqueued as a
  // candidate for code flushing. The shared function info however will not be
  // explicitly enqueued.
  SimulateIncrementalMarking(heap);

  // Now enable the debugger which in turn will disable code flushing.
  CHECK(isolate->debug()->Load());

  // This cycle will bust the heap and subsequent cycles will go ballistic.
  heap->CollectAllGarbage();
  heap->CollectAllGarbage();
}


#ifdef DEBUG
TEST(Regress513507) {
  i::FLAG_flush_optimized_code_cache = false;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_gc_global = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Prepare function whose optimized code map we can use.
  Handle<SharedFunctionInfo> shared;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function f() { return 1 }"
               "f(); %OptimizeFunctionOnNextCall(f); f();");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    shared = inner_scope.CloseAndEscape(handle(f->shared(), isolate));
    CompileRun("f = null");
  }

  // Prepare optimized code that we can use.
  Handle<Code> code;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function g() { return 2 }"
               "g(); %OptimizeFunctionOnNextCall(g); g();");

    Handle<JSFunction> g =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("g"))));
    code = inner_scope.CloseAndEscape(handle(g->code(), isolate));
    if (!code->is_optimized_code()) return;
  }

  Handle<FixedArray> lit = isolate->factory()->empty_fixed_array();
  Handle<Context> context(isolate->context());

  // Add the new code several times to the optimized code map and also set an
  // allocation timeout so that expanding the code map will trigger a GC.
  heap->set_allocation_timeout(5);
  FLAG_gc_interval = 1000;
  for (int i = 0; i < 10; ++i) {
    BailoutId id = BailoutId(i);
    SharedFunctionInfo::AddToOptimizedCodeMap(shared, context, code, lit, id);
  }
}
#endif  // DEBUG


TEST(Regress514122) {
  i::FLAG_flush_optimized_code_cache = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perfrom one initial GC to enable code flushing.
  CcTest::heap()->CollectAllGarbage();

  // Prepare function whose optimized code map we can use.
  Handle<SharedFunctionInfo> shared;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function f() { return 1 }"
               "f(); %OptimizeFunctionOnNextCall(f); f();");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    shared = inner_scope.CloseAndEscape(handle(f->shared(), isolate));
    CompileRun("f = null");
  }

  // Prepare optimized code that we can use.
  Handle<Code> code;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function g() { return 2 }"
               "g(); %OptimizeFunctionOnNextCall(g); g();");

    Handle<JSFunction> g =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("g"))));
    code = inner_scope.CloseAndEscape(handle(g->code(), isolate));
    if (!code->is_optimized_code()) return;
  }

  Handle<FixedArray> lit = isolate->factory()->empty_fixed_array();
  Handle<Context> context(isolate->context());

  // Add the code several times to the optimized code map.
  for (int i = 0; i < 3; ++i) {
    HandleScope inner_scope(isolate);
    BailoutId id = BailoutId(i);
    SharedFunctionInfo::AddToOptimizedCodeMap(shared, context, code, lit, id);
  }
  shared->optimized_code_map()->Print();

  // Add the code with a literals array to be evacuated.
  Page* evac_page;
  {
    HandleScope inner_scope(isolate);
    AlwaysAllocateScope always_allocate(isolate);
    // Make sure literal is placed on an old-space evacuation candidate.
    SimulateFullSpace(heap->old_space());
    Handle<FixedArray> lit = isolate->factory()->NewFixedArray(23, TENURED);
    evac_page = Page::FromAddress(lit->address());
    BailoutId id = BailoutId(100);
    SharedFunctionInfo::AddToOptimizedCodeMap(shared, context, code, lit, id);
  }

  // Heap is ready, force {lit_page} to become an evacuation candidate and
  // simulate incremental marking to enqueue optimized code map.
  FLAG_manual_evacuation_candidates_selection = true;
  evac_page->SetFlag(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING);
  SimulateIncrementalMarking(heap);

  // No matter whether reachable or not, {boomer} is doomed.
  Handle<Object> boomer(shared->optimized_code_map(), isolate);

  // Add the code several times to the optimized code map. This will leave old
  // copies of the optimized code map unreachable but still marked.
  for (int i = 3; i < 6; ++i) {
    HandleScope inner_scope(isolate);
    BailoutId id = BailoutId(i);
    SharedFunctionInfo::AddToOptimizedCodeMap(shared, context, code, lit, id);
  }

  // Trigger a GC to flush out the bug.
  heap->CollectGarbage(i::OLD_SPACE, "fire in the hole");
  boomer->Print();
}


class DummyVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) { }
};


TEST(DeferredHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(reinterpret_cast<v8::Isolate*>(isolate));
  HandleScopeData* data = isolate->handle_scope_data();
  Handle<Object> init(heap->empty_string(), isolate);
  while (data->next < data->limit) {
    Handle<Object> obj(heap->empty_string(), isolate);
  }
  // An entire block of handles has been filled.
  // Next handle would require a new block.
  DCHECK(data->next == data->limit);

  DeferredHandleScope deferred(isolate);
  DummyVisitor visitor;
  isolate->handle_scope_implementer()->Iterate(&visitor);
  delete deferred.Detach();
}


TEST(IncrementalMarkingStepMakesBigProgressWithLargeObjects) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun("function f(n) {"
             "    var a = new Array(n);"
             "    for (var i = 0; i < n; i += 100) a[i] = i;"
             "};"
             "f(10 * 1024 * 1024);");
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  if (marking->IsStopped()) marking->Start(Heap::kNoGCFlags);
  // This big step should be sufficient to mark the whole array.
  marking->Step(100 * MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
  DCHECK(marking->IsComplete() ||
         marking->IsReadyToOverApproximateWeakClosure());
}


TEST(DisableInlineAllocation) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun("function test() {"
             "  var x = [];"
             "  for (var i = 0; i < 10; i++) {"
             "    x[i] = [ {}, [1,2,3], [1,x,3] ];"
             "  }"
             "}"
             "function run() {"
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
  for (Object* site = heap->allocation_sites_list();
       !(site->IsUndefined());
       site = AllocationSite::cast(site)->weak_next()) {
    count++;
  }
  return count;
}


TEST(EnsureAllocationSiteDependentCodesProcessed) {
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();
  GlobalHandles* global_handles = isolate->global_handles();

  if (!isolate->use_crankshaft()) return;

  // The allocation site at the head of the list is ours.
  Handle<AllocationSite> site;
  {
    LocalContext context;
    v8::HandleScope scope(context->GetIsolate());

    int count = AllocationSitesCount(heap);
    CompileRun("var bar = function() { return (new Array()); };"
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

    DependentCode::GroupStartIndexes starts(site->dependent_code());
    CHECK_GE(starts.number_of_entries(), 1);
    int index = starts.at(DependentCode::kAllocationSiteTransitionChangedGroup);
    CHECK(site->dependent_code()->object_at(index)->IsWeakCell());
    Code* function_bar = Code::cast(
        WeakCell::cast(site->dependent_code()->object_at(index))->value());
    Handle<JSFunction> bar_handle =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("bar"))));
    CHECK_EQ(bar_handle->code(), function_bar);
  }

  // Now make sure that a gc should get rid of the function, even though we
  // still have the allocation site alive.
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage();
  }

  // The site still exists because of our global handle, but the code is no
  // longer referred to by dependent_code().
  DependentCode::GroupStartIndexes starts(site->dependent_code());
  int index = starts.at(DependentCode::kAllocationSiteTransitionChangedGroup);
  CHECK(site->dependent_code()->object_at(index)->IsWeakCell() &&
        WeakCell::cast(site->dependent_code()->object_at(index))->cleared());
}


TEST(CellsInOptimizedCodeAreWeak) {
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  i::FLAG_weak_embedded_objects_in_optimized_code = true;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_crankshaft()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun("bar = (function() {"
               "  function bar() {"
               "    return foo(1);"
               "  };"
               "  var foo = function(x) { with (x) { return 1 + x; } };"
               "  bar(foo);"
               "  bar(foo);"
               "  bar(foo);"
               "  %OptimizeFunctionOnNextCall(bar);"
               "  bar(foo);"
               "  return bar;})();");

    Handle<JSFunction> bar =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("bar"))));
    code = scope.CloseAndEscape(Handle<Code>(bar->code()));
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage();
  }

  DCHECK(code->marked_for_deoptimization());
}


TEST(ObjectsInOptimizedCodeAreWeak) {
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  i::FLAG_weak_embedded_objects_in_optimized_code = true;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_crankshaft()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun("function bar() {"
               "  return foo(1);"
               "};"
               "function foo(x) { with (x) { return 1 + x; } };"
               "bar();"
               "bar();"
               "bar();"
               "%OptimizeFunctionOnNextCall(bar);"
               "bar();");

    Handle<JSFunction> bar =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("bar"))));
    code = scope.CloseAndEscape(Handle<Code>(bar->code()));
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage();
  }

  DCHECK(code->marked_for_deoptimization());
}


TEST(NoWeakHashTableLeakWithIncrementalMarking) {
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  if (!i::FLAG_incremental_marking) return;
  i::FLAG_weak_embedded_objects_in_optimized_code = true;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_compilation_cache = false;
  i::FLAG_retain_maps_for_n_gc = 0;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();

  // Do not run for no-snap builds.
  if (!i::Snapshot::HaveASnapshotToStartFrom(isolate)) return;

  v8::internal::Heap* heap = CcTest::heap();

  // Get a clean slate regarding optimized functions on the heap.
  i::Deoptimizer::DeoptimizeAll(isolate);
  heap->CollectAllGarbage();

  if (!isolate->use_crankshaft()) return;
  HandleScope outer_scope(heap->isolate());
  for (int i = 0; i < 3; i++) {
    SimulateIncrementalMarking(heap);
    {
      LocalContext context;
      HandleScope scope(heap->isolate());
      EmbeddedVector<char, 256> source;
      SNPrintF(source,
               "function bar%d() {"
               "  return foo%d(1);"
               "};"
               "function foo%d(x) { with (x) { return 1 + x; } };"
               "bar%d();"
               "bar%d();"
               "bar%d();"
               "%%OptimizeFunctionOnNextCall(bar%d);"
               "bar%d();",
               i, i, i, i, i, i, i, i);
      CompileRun(source.start());
    }
    heap->CollectAllGarbage();
  }
  int elements = 0;
  if (heap->weak_object_to_code_table()->IsHashTable()) {
    WeakHashTable* t = WeakHashTable::cast(heap->weak_object_to_code_table());
    elements = t->NumberOfElements();
  }
  CHECK_EQ(0, elements);
}


static Handle<JSFunction> OptimizeDummyFunction(const char* name) {
  EmbeddedVector<char, 256> source;
  SNPrintF(source,
          "function %s() { return 0; }"
          "%s(); %s();"
          "%%OptimizeFunctionOnNextCall(%s);"
          "%s();", name, name, name, name, name);
  CompileRun(source.start());
  Handle<JSFunction> fun =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str(name))));
  return fun;
}


static int GetCodeChainLength(Code* code) {
  int result = 0;
  while (code->next_code_link()->IsCode()) {
    result++;
    code = Code::cast(code->next_code_link());
  }
  return result;
}


TEST(NextCodeLinkIsWeak) {
  i::FLAG_always_opt = false;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_crankshaft()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  heap->CollectAllAvailableGarbage();
  int code_chain_length_before, code_chain_length_after;
  {
    HandleScope scope(heap->isolate());
    Handle<JSFunction> mortal = OptimizeDummyFunction("mortal");
    Handle<JSFunction> immortal = OptimizeDummyFunction("immortal");
    CHECK_EQ(immortal->code()->next_code_link(), mortal->code());
    code_chain_length_before = GetCodeChainLength(immortal->code());
    // Keep the immortal code and let the mortal code die.
    code = scope.CloseAndEscape(Handle<Code>(immortal->code()));
    CompileRun("mortal = null; immortal = null;");
  }
  heap->CollectAllAvailableGarbage();
  // Now mortal code should be dead.
  code_chain_length_after = GetCodeChainLength(*code);
  CHECK_EQ(code_chain_length_before - 1, code_chain_length_after);
}


static Handle<Code> DummyOptimizedCode(Isolate* isolate) {
  i::byte buffer[i::Assembler::kMinimalBufferSize];
  MacroAssembler masm(isolate, buffer, sizeof(buffer));
  CodeDesc desc;
  masm.Push(isolate->factory()->undefined_value());
  masm.Drop(1);
  masm.GetCode(&desc);
  Handle<Object> undefined(isolate->heap()->undefined_value(), isolate);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::OPTIMIZED_FUNCTION), undefined);
  CHECK(code->IsCode());
  return code;
}


TEST(NextCodeLinkIsWeak2) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_crankshaft()) return;
  HandleScope outer_scope(heap->isolate());
  heap->CollectAllAvailableGarbage();
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
  heap->CollectAllAvailableGarbage();
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
  if (i::FLAG_always_opt) return;
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  CompileRun(
      "function createObj(obj) {"
      "  return new obj();"
      "}");
  Handle<JSFunction> createObj =
      v8::Utils::OpenHandle(*v8::Handle<v8::Function>::Cast(
                                CcTest::global()->Get(v8_str("createObj"))));

  v8::Persistent<v8::Object> garbage;
  {
    v8::HandleScope scope(isolate);
    const char* source =
        " (function() {"
        "   function hat() { this.x = 5; }"
        "   createObj(hat);"
        "   createObj(hat);"
        "   return hat;"
        " })();";
    garbage.Reset(isolate, CompileRun(source)->ToObject(isolate));
  }
  weak_ic_cleared = false;
  garbage.SetWeak(&garbage, &ClearWeakIC, v8::WeakCallbackType::kParameter);
  Heap* heap = CcTest::i_isolate()->heap();
  heap->CollectAllGarbage();
  CHECK(weak_ic_cleared);

  // We've determined the constructor in createObj has had it's weak cell
  // cleared. Now, verify that one additional call with a new function
  // allows monomorphicity.
  Handle<TypeFeedbackVector> feedback_vector = Handle<TypeFeedbackVector>(
      createObj->shared()->feedback_vector(), CcTest::i_isolate());
  for (int i = 0; i < 20; i++) {
    Object* slot_value = feedback_vector->Get(FeedbackVectorSlot(0));
    CHECK(slot_value->IsWeakCell());
    if (WeakCell::cast(slot_value)->cleared()) break;
    heap->CollectAllGarbage();
  }

  Object* slot_value = feedback_vector->Get(FeedbackVectorSlot(0));
  CHECK(slot_value->IsWeakCell() && WeakCell::cast(slot_value)->cleared());
  CompileRun(
      "function coat() { this.x = 6; }"
      "createObj(coat);");
  slot_value = feedback_vector->Get(FeedbackVectorSlot(0));
  CHECK(slot_value->IsWeakCell() && !WeakCell::cast(slot_value)->cleared());
}


// Checks that the value returned by execution of the source is weak.
void CheckWeakness(const char* source) {
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Persistent<v8::Object> garbage;
  {
    v8::HandleScope scope(isolate);
    garbage.Reset(isolate, CompileRun(source)->ToObject(isolate));
  }
  weak_ic_cleared = false;
  garbage.SetWeak(&garbage, &ClearWeakIC, v8::WeakCallbackType::kParameter);
  Heap* heap = CcTest::i_isolate()->heap();
  heap->CollectAllGarbage();
  CHECK(weak_ic_cleared);
}


// Each of the following "weak IC" tests creates an IC that embeds a map with
// the prototype pointing to _proto_ and checks that the _proto_ dies on GC.
TEST(WeakMapInMonomorphicLoadIC) {
  CheckWeakness("function loadIC(obj) {"
                "  return obj.name;"
                "}"
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
  CheckWeakness("function keyedLoadIC(obj, field) {"
                "  return obj[field];"
                "}"
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
  CheckWeakness("function storeIC(obj, value) {"
                "  obj.name = value;"
                "}"
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
  CheckWeakness("function keyedStoreIC(obj, field, value) {"
                "  obj[field] = value;"
                "}"
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
  CheckWeakness("function compareNilIC(obj) {"
                "  return obj == null;"
                "}"
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
      Object::GetProperty(isolate->global_object(), str).ToHandleChecked();
  return Handle<JSFunction>::cast(obj);
}


void CheckIC(Code* code, Code::Kind kind, SharedFunctionInfo* shared,
             int ic_slot, InlineCacheState state) {
  if (kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC ||
      kind == Code::CALL_IC) {
    TypeFeedbackVector* vector = shared->feedback_vector();
    FeedbackVectorICSlot slot(ic_slot);
    if (kind == Code::LOAD_IC) {
      LoadICNexus nexus(vector, slot);
      CHECK_EQ(nexus.StateFromFeedback(), state);
    } else if (kind == Code::KEYED_LOAD_IC) {
      KeyedLoadICNexus nexus(vector, slot);
      CHECK_EQ(nexus.StateFromFeedback(), state);
    } else if (kind == Code::CALL_IC) {
      CallICNexus nexus(vector, slot);
      CHECK_EQ(nexus.StateFromFeedback(), state);
    }
  } else {
    Code* ic = FindFirstIC(code, kind);
    CHECK(ic->is_inline_cache_stub());
    CHECK(ic->ic_state() == state);
  }
}


TEST(MonomorphicStaysMonomorphicAfterGC) {
  if (FLAG_always_opt) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
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
  heap->CollectAllGarbage();
  CheckIC(loadIC->code(), Code::LOAD_IC, loadIC->shared(), 0, MONOMORPHIC);
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CheckIC(loadIC->code(), Code::LOAD_IC, loadIC->shared(), 0, MONOMORPHIC);
}


TEST(PolymorphicStaysPolymorphicAfterGC) {
  if (FLAG_always_opt) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
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
  heap->CollectAllGarbage();
  CheckIC(loadIC->code(), Code::LOAD_IC, loadIC->shared(), 0, POLYMORPHIC);
  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CheckIC(loadIC->code(), Code::LOAD_IC, loadIC->shared(), 0, POLYMORPHIC);
}


TEST(WeakCell) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();
  v8::internal::Factory* factory = isolate->factory();

  HandleScope outer_scope(isolate);
  Handle<WeakCell> weak_cell1;
  {
    HandleScope inner_scope(isolate);
    Handle<HeapObject> value = factory->NewFixedArray(1, NOT_TENURED);
    weak_cell1 = inner_scope.CloseAndEscape(factory->NewWeakCell(value));
  }

  Handle<FixedArray> survivor = factory->NewFixedArray(1, NOT_TENURED);
  Handle<WeakCell> weak_cell2;
  {
    HandleScope inner_scope(isolate);
    weak_cell2 = inner_scope.CloseAndEscape(factory->NewWeakCell(survivor));
  }
  CHECK(weak_cell1->value()->IsFixedArray());
  CHECK_EQ(*survivor, weak_cell2->value());
  heap->CollectGarbage(NEW_SPACE);
  CHECK(weak_cell1->value()->IsFixedArray());
  CHECK_EQ(*survivor, weak_cell2->value());
  heap->CollectGarbage(NEW_SPACE);
  CHECK(weak_cell1->value()->IsFixedArray());
  CHECK_EQ(*survivor, weak_cell2->value());
  heap->CollectAllAvailableGarbage();
  CHECK(weak_cell1->cleared());
  CHECK_EQ(*survivor, weak_cell2->value());
}


TEST(WeakCellsWithIncrementalMarking) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();
  v8::internal::Factory* factory = isolate->factory();

  const int N = 16;
  HandleScope outer_scope(isolate);
  Handle<FixedArray> survivor = factory->NewFixedArray(1, NOT_TENURED);
  Handle<WeakCell> weak_cells[N];

  for (int i = 0; i < N; i++) {
    HandleScope inner_scope(isolate);
    Handle<HeapObject> value =
        i == 0 ? survivor : factory->NewFixedArray(1, NOT_TENURED);
    Handle<WeakCell> weak_cell = factory->NewWeakCell(value);
    CHECK(weak_cell->value()->IsFixedArray());
    IncrementalMarking* marking = heap->incremental_marking();
    if (marking->IsStopped()) marking->Start(Heap::kNoGCFlags);
    marking->Step(128, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
    heap->CollectGarbage(NEW_SPACE);
    CHECK(weak_cell->value()->IsFixedArray());
    weak_cells[i] = inner_scope.CloseAndEscape(weak_cell);
  }
  heap->CollectAllGarbage();
  CHECK_EQ(*survivor, weak_cells[0]->value());
  for (int i = 1; i < N; i++) {
    CHECK(weak_cells[i]->cleared());
  }
}


#ifdef DEBUG
TEST(AddInstructionChangesNewSpacePromotion) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;
  i::FLAG_stress_compaction = true;
  i::FLAG_gc_interval = 1000;
  CcTest::InitializeVM();
  if (!i::FLAG_allocation_site_pretenuring) return;
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

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
      "crash(1);"
      "crash(1);"
      "%OptimizeFunctionOnNextCall(crash);"
      "crash(1);");

  v8::Handle<v8::Object> global = CcTest::global();
    v8::Handle<v8::Function> g =
        v8::Handle<v8::Function>::Cast(global->Get(v8_str("crash")));
  v8::Handle<v8::Value> args1[] = { v8_num(1) };
  heap->DisableInlineAllocation();
  heap->set_allocation_timeout(1);
  g->Call(global, 1, args1);
  heap->CollectAllGarbage();
}


void OnFatalErrorExpectOOM(const char* location, const char* message) {
  // Exit with 0 if the location matches our expectation.
  exit(strcmp(location, "CALL_AND_RETRY_LAST"));
}


TEST(CEntryStubOOM) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::V8::SetFatalErrorHandler(OnFatalErrorExpectOOM);

  v8::Handle<v8::Value> result = CompileRun(
      "%SetFlags('--gc-interval=1');"
      "var a = [];"
      "a.__proto__ = [];"
      "a.unshift(1)");

  CHECK(result->IsNumber());
}

#endif  // DEBUG


static void InterruptCallback357137(v8::Isolate* isolate, void* data) { }


static void RequestInterrupt(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CcTest::isolate()->RequestInterrupt(&InterruptCallback357137, NULL);
}


TEST(Regress357137) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope hscope(isolate);
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(v8::String::NewFromUtf8(isolate, "interrupt"),
              v8::FunctionTemplate::New(isolate, RequestInterrupt));
  v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
  DCHECK(!context.IsEmpty());
  v8::Context::Scope cscope(context);

  v8::Local<v8::Value> result = CompileRun(
      "var locals = '';"
      "for (var i = 0; i < 512; i++) locals += 'var v' + i + '= 42;';"
      "eval('function f() {' + locals + 'return function() { return v0; }; }');"
      "interrupt();"  // This triggers a fake stack overflow in f.
      "f()()");
  CHECK_EQ(42.0, result->ToNumber(isolate)->Value());
}


TEST(Regress507979) {
  const int kFixedArrayLen = 10;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope handle_scope(isolate);

  Handle<FixedArray> o1 = isolate->factory()->NewFixedArray(kFixedArrayLen);
  Handle<FixedArray> o2 = isolate->factory()->NewFixedArray(kFixedArrayLen);
  CHECK(heap->InNewSpace(o1->address()));
  CHECK(heap->InNewSpace(o2->address()));

  HeapIterator it(heap, i::HeapIterator::kFilterUnreachable);

  // Replace parts of an object placed before a live object with a filler. This
  // way the filler object shares the mark bits with the following live object.
  o1->Shrink(kFixedArrayLen - 1);

  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next()) {
    // Let's not optimize the loop away.
    CHECK(obj->address() != nullptr);
  }
}


TEST(ArrayShiftSweeping) {
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  v8::Local<v8::Value> result = CompileRun(
      "var array = new Array(40000);"
      "var tmp = new Array(100000);"
      "array[0] = 10;"
      "gc();"
      "gc();"
      "array.shift();"
      "array;");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(result));
  CHECK(heap->InOldSpace(o->elements()));
  CHECK(heap->InOldSpace(*o));
  Page* page = Page::FromAddress(o->elements()->address());
  CHECK(page->parallel_sweeping() <= MemoryChunk::SWEEPING_FINALIZE ||
        Marking::IsBlack(Marking::MarkBitFrom(o->elements())));
}


UNINITIALIZED_TEST(PromotionQueue) {
  i::FLAG_expose_gc = true;
  i::FLAG_max_semi_space_size = 2 * (Page::kPageSize / MB);
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    Heap* heap = i_isolate->heap();
    NewSpace* new_space = heap->new_space();

    // In this test we will try to overwrite the promotion queue which is at the
    // end of to-space. To actually make that possible, we need at least two
    // semi-space pages and take advantage of fragmentation.
    // (1) Grow semi-space to two pages.
    // (2) Create a few small long living objects and call the scavenger to
    // move them to the other semi-space.
    // (3) Create a huge object, i.e., remainder of first semi-space page and
    // create another huge object which should be of maximum allocatable memory
    // size of the second semi-space page.
    // (4) Call the scavenger again.
    // What will happen is: the scavenger will promote the objects created in
    // (2) and will create promotion queue entries at the end of the second
    // semi-space page during the next scavenge when it promotes the objects to
    // the old generation. The first allocation of (3) will fill up the first
    // semi-space page. The second allocation in (3) will not fit into the
    // first semi-space page, but it will overwrite the promotion queue which
    // are in the second semi-space page. If the right guards are in place, the
    // promotion queue will be evacuated in that case.

    // Grow the semi-space to two pages to make semi-space copy overwrite the
    // promotion queue, which will be at the end of the second page.
    intptr_t old_capacity = new_space->TotalCapacity();

    // If we are in a low memory config, we can't grow to two pages and we can't
    // run this test. This also means the issue we are testing cannot arise, as
    // there is no fragmentation.
    if (new_space->IsAtMaximumCapacity()) return;

    new_space->Grow();
    CHECK(new_space->IsAtMaximumCapacity());
    CHECK(2 * old_capacity == new_space->TotalCapacity());

    // Call the scavenger two times to get an empty new space
    heap->CollectGarbage(NEW_SPACE);
    heap->CollectGarbage(NEW_SPACE);

    // First create a few objects which will survive a scavenge, and will get
    // promoted to the old generation later on. These objects will create
    // promotion queue entries at the end of the second semi-space page.
    const int number_handles = 12;
    Handle<FixedArray> handles[number_handles];
    for (int i = 0; i < number_handles; i++) {
      handles[i] = i_isolate->factory()->NewFixedArray(1, NOT_TENURED);
    }
    heap->CollectGarbage(NEW_SPACE);

    // Create the first huge object which will exactly fit the first semi-space
    // page.
    int new_linear_size =
        static_cast<int>(*heap->new_space()->allocation_limit_address() -
                         *heap->new_space()->allocation_top_address());
    int length = new_linear_size / kPointerSize - FixedArray::kHeaderSize;
    Handle<FixedArray> first =
        i_isolate->factory()->NewFixedArray(length, NOT_TENURED);
    CHECK(heap->InNewSpace(*first));

    // Create the second huge object of maximum allocatable second semi-space
    // page size.
    new_linear_size =
        static_cast<int>(*heap->new_space()->allocation_limit_address() -
                         *heap->new_space()->allocation_top_address());
    length = Page::kMaxRegularHeapObjectSize / kPointerSize -
             FixedArray::kHeaderSize;
    Handle<FixedArray> second =
        i_isolate->factory()->NewFixedArray(length, NOT_TENURED);
    CHECK(heap->InNewSpace(*second));

    // This scavenge will corrupt memory if the promotion queue is not
    // evacuated.
    heap->CollectGarbage(NEW_SPACE);
  }
  isolate->Dispose();
}


TEST(Regress388880) {
  i::FLAG_expose_gc = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  Handle<Map> map1 = Map::Create(isolate, 1);
  Handle<Map> map2 =
      Map::CopyWithField(map1, factory->NewStringFromStaticChars("foo"),
                         HeapType::Any(isolate), NONE, Representation::Tagged(),
                         OMIT_TRANSITION).ToHandleChecked();

  int desired_offset = Page::kPageSize - map1->instance_size();

  // Allocate fixed array in old pointer space so, that object allocated
  // afterwards would end at the end of the page.
  {
    SimulateFullSpace(heap->old_space());
    int padding_size = desired_offset - Page::kObjectStartOffset;
    int padding_array_length =
        (padding_size - FixedArray::kHeaderSize) / kPointerSize;

    Handle<FixedArray> temp2 =
        factory->NewFixedArray(padding_array_length, TENURED);
    Page* page = Page::FromAddress(temp2->address());
    CHECK_EQ(Page::kObjectStartOffset, page->Offset(temp2->address()));
  }

  Handle<JSObject> o = factory->NewJSObjectFromMap(map1, TENURED);
  o->set_properties(*factory->empty_fixed_array());

  // Ensure that the object allocated where we need it.
  Page* page = Page::FromAddress(o->address());
  CHECK_EQ(desired_offset, page->Offset(o->address()));

  // Now we have an object right at the end of the page.

  // Enable incremental marking to trigger actions in Heap::AdjustLiveBytes()
  // that would cause crash.
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start(Heap::kNoGCFlags);
  CHECK(marking->IsMarking());

  // Now everything is set up for crashing in JSObject::MigrateFastToFast()
  // when it calls heap->AdjustLiveBytes(...).
  JSObject::MigrateToMap(o, map2);
}


TEST(Regress3631) {
  i::FLAG_expose_gc = true;
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
    marking->Start(Heap::kNoGCFlags);
  }
  // Incrementally mark the backing store.
  Handle<JSObject> obj =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(result));
  Handle<JSWeakCollection> weak_map(reinterpret_cast<JSWeakCollection*>(*obj));
  while (!Marking::IsBlack(
             Marking::MarkBitFrom(HeapObject::cast(weak_map->table()))) &&
         !marking->IsStopped()) {
    marking->Step(MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
  }
  // Stash the backing store in a handle.
  Handle<Object> save(weak_map->table(), isolate);
  // The following line will update the backing store.
  CompileRun(
      "for (var i = 0; i < 50; i++) {"
      "  weak_map.set(future_keys[i], i);"
      "}");
  heap->incremental_marking()->set_should_hurry(true);
  heap->CollectGarbage(OLD_SPACE);
}


TEST(Regress442710) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);
  Handle<GlobalObject> global(CcTest::i_isolate()->context()->global_object());
  Handle<JSArray> array = factory->NewJSArray(2);

  Handle<String> name = factory->InternalizeUtf8String("testArray");
  JSReceiver::SetProperty(global, name, array, SLOPPY).Check();
  CompileRun("testArray[0] = 1; testArray[1] = 2; testArray.shift();");
  heap->CollectGarbage(OLD_SPACE);
}


TEST(NumberStringCacheSize) {
  // Test that the number-string cache has not been resized in the snapshot.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  if (!isolate->snapshot_available()) return;
  Heap* heap = isolate->heap();
  CHECK_EQ(TestHeap::kInitialNumberStringCacheSize * 2,
           heap->number_string_cache()->length());
}


TEST(Regress3877) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  CompileRun("function cls() { this.x = 10; }");
  Handle<WeakCell> weak_prototype;
  {
    HandleScope inner_scope(isolate);
    v8::Local<v8::Value> result = CompileRun("cls.prototype");
    Handle<JSObject> proto =
        v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(result));
    weak_prototype = inner_scope.CloseAndEscape(factory->NewWeakCell(proto));
  }
  CHECK(!weak_prototype->cleared());
  CompileRun(
      "var a = { };"
      "a.x = new cls();"
      "cls.prototype = null;");
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage();
  }
  // The map of a.x keeps prototype alive
  CHECK(!weak_prototype->cleared());
  // Change the map of a.x and make the previous map garbage collectable.
  CompileRun("a.x.__proto__ = {};");
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage();
  }
  CHECK(weak_prototype->cleared());
}


Handle<WeakCell> AddRetainedMap(Isolate* isolate, Heap* heap) {
    HandleScope inner_scope(isolate);
    Handle<Map> map = Map::Create(isolate, 1);
    v8::Local<v8::Value> result =
        CompileRun("(function () { return {x : 10}; })();");
    Handle<JSObject> proto =
        v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(result));
    Map::SetPrototype(map, proto);
    heap->AddRetainedMap(map);
    return inner_scope.CloseAndEscape(Map::WeakCellForMap(map));
}


void CheckMapRetainingFor(int n) {
  FLAG_retain_maps_for_n_gc = n;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Handle<WeakCell> weak_cell = AddRetainedMap(isolate, heap);
  CHECK(!weak_cell->cleared());
  for (int i = 0; i < n; i++) {
    heap->CollectGarbage(OLD_SPACE);
  }
  CHECK(!weak_cell->cleared());
  heap->CollectGarbage(OLD_SPACE);
  CHECK(weak_cell->cleared());
}


TEST(MapRetaining) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CheckMapRetainingFor(FLAG_retain_maps_for_n_gc);
  CheckMapRetainingFor(0);
  CheckMapRetainingFor(1);
  CheckMapRetainingFor(7);
}


TEST(RegressArrayListGC) {
  FLAG_retain_maps_for_n_gc = 1;
  FLAG_incremental_marking = 0;
  FLAG_gc_global = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  AddRetainedMap(isolate, heap);
  Handle<Map> map = Map::Create(isolate, 1);
  heap->CollectGarbage(OLD_SPACE);
  // Force GC in old space on next addition of retained map.
  Map::WeakCellForMap(map);
  SimulateFullSpace(CcTest::heap()->new_space());
  for (int i = 0; i < 10; i++) {
    heap->AddRetainedMap(map);
  }
  heap->CollectGarbage(OLD_SPACE);
}


#ifdef DEBUG
TEST(PathTracer) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> result = CompileRun("'abc'");
  Handle<Object> o = v8::Utils::OpenHandle(*result);
  CcTest::i_isolate()->heap()->TracePathToObject(*o);
}
#endif  // DEBUG


TEST(WritableVsImmortalRoots) {
  for (int i = 0; i < Heap::kStrongRootListLength; ++i) {
    Heap::RootListIndex root_index = static_cast<Heap::RootListIndex>(i);
    bool writable = Heap::RootCanBeWrittenAfterInitialization(root_index);
    bool immortal = Heap::RootIsImmortalImmovable(root_index);
    // A root value can be writable, immortal, or neither, but not both.
    CHECK(!immortal || !writable);
  }
}


static void TestRightTrimFixedTypedArray(i::ExternalArrayType type,
                                         int initial_length,
                                         int elements_to_trim) {
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  Handle<FixedTypedArrayBase> array =
      factory->NewFixedTypedArray(initial_length, type, true);
  int old_size = array->size();
  heap->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(*array,
                                                         elements_to_trim);

  // Check that free space filler is at the right place and did not smash the
  // array header.
  CHECK(array->IsFixedArrayBase());
  CHECK_EQ(initial_length - elements_to_trim, array->length());
  int new_size = array->size();
  if (new_size != old_size) {
    // Free space filler should be created in this case.
    Address next_obj_address = array->address() + array->size();
    CHECK(HeapObject::FromAddress(next_obj_address)->IsFiller());
  }
  heap->CollectAllAvailableGarbage();
}


TEST(Regress472513) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // The combination of type/initial_length/elements_to_trim triggered
  // typed array header smashing with free space filler (crbug/472513).

  // 64-bit cases.
  TestRightTrimFixedTypedArray(i::kExternalUint8Array, 32, 6);
  TestRightTrimFixedTypedArray(i::kExternalUint8Array, 32 - 7, 6);
  TestRightTrimFixedTypedArray(i::kExternalUint16Array, 16, 6);
  TestRightTrimFixedTypedArray(i::kExternalUint16Array, 16 - 3, 6);
  TestRightTrimFixedTypedArray(i::kExternalUint32Array, 8, 6);
  TestRightTrimFixedTypedArray(i::kExternalUint32Array, 8 - 1, 6);

  // 32-bit cases.
  TestRightTrimFixedTypedArray(i::kExternalUint8Array, 16, 3);
  TestRightTrimFixedTypedArray(i::kExternalUint8Array, 16 - 3, 3);
  TestRightTrimFixedTypedArray(i::kExternalUint16Array, 8, 3);
  TestRightTrimFixedTypedArray(i::kExternalUint16Array, 8 - 1, 3);
  TestRightTrimFixedTypedArray(i::kExternalUint32Array, 4, 3);
}


TEST(WeakFixedArray) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  Handle<HeapNumber> number = CcTest::i_isolate()->factory()->NewHeapNumber(1);
  Handle<WeakFixedArray> array = WeakFixedArray::Add(Handle<Object>(), number);
  array->Remove(number);
  array->Compact<WeakFixedArray::NullCallback>();
  WeakFixedArray::Add(array, number);
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
      JSObject::GetProperty(exception, key).ToHandleChecked();
  Handle<Object> code =
      Object::GetElement(isolate, stack_trace, 3).ToHandleChecked();
  CHECK(code->IsCode());

  isolate->heap()->CollectAllAvailableGarbage("stack trace preprocessing");

  Handle<Object> pos =
      Object::GetElement(isolate, stack_trace, 3).ToHandleChecked();
  CHECK(pos->IsSmi());

  Handle<JSArray> stack_trace_array = Handle<JSArray>::cast(stack_trace);
  int array_length = Smi::cast(stack_trace_array->length())->value();
  for (int i = 0; i < array_length; i++) {
    Handle<Object> element =
        Object::GetElement(isolate, stack_trace, i).ToHandleChecked();
    CHECK(!element->IsCode());
  }
}


static bool utils_has_been_collected = false;

static void UtilsHasBeenCollected(
    const v8::WeakCallbackInfo<v8::Persistent<v8::Object>>& data) {
  utils_has_been_collected = true;
  data.GetParameter()->Reset();
}


TEST(BootstrappingExports) {
  FLAG_expose_natives_as = "natives";
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();

  if (Snapshot::HaveASnapshotToStartFrom(CcTest::i_isolate())) return;

  utils_has_been_collected = false;

  v8::Persistent<v8::Object> utils;

  {
    v8::HandleScope scope(isolate);
    v8::Handle<v8::Object> natives =
        CcTest::global()->Get(v8_str("natives"))->ToObject(isolate);
    utils.Reset(isolate, natives->Get(v8_str("utils"))->ToObject(isolate));
    natives->Delete(v8_str("utils"));
  }

  utils.SetWeak(&utils, UtilsHasBeenCollected,
                v8::WeakCallbackType::kParameter);

  CcTest::heap()->CollectAllAvailableGarbage("fire weak callbacks");

  CHECK(utils_has_been_collected);
}


TEST(Regress1878) {
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Function> constructor =
      v8::Utils::ToLocal(CcTest::i_isolate()->internal_array_function());
  CcTest::global()->Set(v8_str("InternalArray"), constructor);

  v8::TryCatch try_catch(isolate);

  CompileRun(
      "var a = Array();"
      "for (var i = 0; i < 1000; i++) {"
      "  var ai = new InternalArray(10000);"
      "  if (%HaveSameMap(ai, a)) throw Error();"
      "  if (!%HasFastObjectElements(ai)) throw Error();"
      "}"
      "for (var i = 0; i < 1000; i++) {"
      "  var ai = new InternalArray(10000);"
      "  if (%HaveSameMap(ai, a)) throw Error();"
      "  if (!%HasFastObjectElements(ai)) throw Error();"
      "}");

  CHECK(!try_catch.HasCaught());
}


void AllocateInSpace(Isolate* isolate, size_t bytes, AllocationSpace space) {
  CHECK(bytes >= FixedArray::kHeaderSize);
  CHECK(bytes % kPointerSize == 0);
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  AlwaysAllocateScope always_allocate(isolate);
  int elements =
      static_cast<int>((bytes - FixedArray::kHeaderSize) / kPointerSize);
  Handle<FixedArray> array = factory->NewFixedArray(
      elements, space == NEW_SPACE ? NOT_TENURED : TENURED);
  CHECK((space == NEW_SPACE) == isolate->heap()->InNewSpace(*array));
  CHECK_EQ(bytes, static_cast<size_t>(array->Size()));
}


TEST(NewSpaceAllocationCounter) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  size_t counter1 = heap->NewSpaceAllocationCounter();
  heap->CollectGarbage(NEW_SPACE);
  const size_t kSize = 1024;
  AllocateInSpace(isolate, kSize, NEW_SPACE);
  size_t counter2 = heap->NewSpaceAllocationCounter();
  CHECK_EQ(kSize, counter2 - counter1);
  heap->CollectGarbage(NEW_SPACE);
  size_t counter3 = heap->NewSpaceAllocationCounter();
  CHECK_EQ(0U, counter3 - counter2);
  // Test counter overflow.
  size_t max_counter = -1;
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
  heap->CollectGarbage(NEW_SPACE);
  heap->CollectGarbage(NEW_SPACE);
  const size_t kSize = 1024;
  AllocateInSpace(isolate, kSize, OLD_SPACE);
  size_t counter2 = heap->OldGenerationAllocationCounter();
  // TODO(ulan): replace all CHECK_LE with CHECK_EQ after v8:4148 is fixed.
  CHECK_LE(kSize, counter2 - counter1);
  heap->CollectGarbage(NEW_SPACE);
  size_t counter3 = heap->OldGenerationAllocationCounter();
  CHECK_EQ(0u, counter3 - counter2);
  AllocateInSpace(isolate, kSize, OLD_SPACE);
  heap->CollectGarbage(OLD_SPACE);
  size_t counter4 = heap->OldGenerationAllocationCounter();
  CHECK_LE(kSize, counter4 - counter3);
  // Test counter overflow.
  size_t max_counter = -1;
  heap->set_old_generation_allocation_counter(max_counter - 10 * kSize);
  size_t start = heap->OldGenerationAllocationCounter();
  for (int i = 0; i < 20; i++) {
    AllocateInSpace(isolate, kSize, OLD_SPACE);
    size_t counter = heap->OldGenerationAllocationCounter();
    CHECK_LE(kSize, counter - start);
    start = counter;
  }
}


TEST(NewSpaceAllocationThroughput) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  GCTracer* tracer = heap->tracer();
  int time1 = 100;
  size_t counter1 = 1000;
  tracer->SampleAllocation(time1, counter1, 0);
  int time2 = 200;
  size_t counter2 = 2000;
  tracer->SampleAllocation(time2, counter2, 0);
  size_t throughput =
      tracer->NewSpaceAllocationThroughputInBytesPerMillisecond();
  CHECK_EQ((counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  tracer->SampleAllocation(time3, counter3, 0);
  throughput = tracer->NewSpaceAllocationThroughputInBytesPerMillisecond();
  CHECK_EQ((counter3 - counter1) / (time3 - time1), throughput);
}


TEST(NewSpaceAllocationThroughput2) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  GCTracer* tracer = heap->tracer();
  int time1 = 100;
  size_t counter1 = 1000;
  tracer->SampleAllocation(time1, counter1, 0);
  int time2 = 200;
  size_t counter2 = 2000;
  tracer->SampleAllocation(time2, counter2, 0);
  size_t throughput =
      tracer->NewSpaceAllocationThroughputInBytesPerMillisecond(100);
  CHECK_EQ((counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  tracer->SampleAllocation(time3, counter3, 0);
  throughput = tracer->NewSpaceAllocationThroughputInBytesPerMillisecond(100);
  CHECK_EQ((counter3 - counter1) / (time3 - time1), throughput);
}


static void CheckLeak(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = CcTest::i_isolate();
  Object* message =
      *reinterpret_cast<Object**>(isolate->pending_message_obj_address());
  CHECK(message->IsTheHole());
}


TEST(MessageObjectLeak) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(v8::String::NewFromUtf8(isolate, "check"),
              v8::FunctionTemplate::New(isolate, CheckLeak));
  v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
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
  FlagList::SetFlagsFromString(flag, StrLength(flag));
  FLAG_always_opt = true;
  FLAG_turbo_try_catch = true;
  FLAG_turbo_try_finally = true;

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
  fun->ReplaceCode(*isolate->builtins()->CompileLazy());
  fun->shared()->ReplaceCode(*isolate->builtins()->CompileLazy());
  isolate->heap()->CollectAllAvailableGarbage("remove code and gc");
}


TEST(CanonicalSharedFunctionInfo) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(isolate, "check", v8::FunctionTemplate::New(
                                    isolate, CheckEqualSharedFunctionInfos));
  global->Set(isolate, "remove",
              v8::FunctionTemplate::New(isolate, RemoveCodeAndGC));
  v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);
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


TEST(OldGenerationAllocationThroughput) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  GCTracer* tracer = heap->tracer();
  int time1 = 100;
  size_t counter1 = 1000;
  tracer->SampleAllocation(time1, 0, counter1);
  int time2 = 200;
  size_t counter2 = 2000;
  tracer->SampleAllocation(time2, 0, counter2);
  size_t throughput =
      tracer->OldGenerationAllocationThroughputInBytesPerMillisecond(100);
  CHECK_EQ((counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  tracer->SampleAllocation(time3, 0, counter3);
  throughput =
      tracer->OldGenerationAllocationThroughputInBytesPerMillisecond(100);
  CHECK_EQ((counter3 - counter1) / (time3 - time1), throughput);
}


TEST(AllocationThroughput) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  GCTracer* tracer = heap->tracer();
  int time1 = 100;
  size_t counter1 = 1000;
  tracer->SampleAllocation(time1, counter1, counter1);
  int time2 = 200;
  size_t counter2 = 2000;
  tracer->SampleAllocation(time2, counter2, counter2);
  size_t throughput = tracer->AllocationThroughputInBytesPerMillisecond(100);
  CHECK_EQ(2 * (counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  tracer->SampleAllocation(time3, counter3, counter3);
  throughput = tracer->AllocationThroughputInBytesPerMillisecond(100);
  CHECK_EQ(2 * (counter3 - counter1) / (time3 - time1), throughput);
}


TEST(SlotsBufferObjectSlotsRemoval) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  SlotsBuffer* buffer = new SlotsBuffer(NULL);
  void* fake_object[1];

  Handle<FixedArray> array = factory->NewFixedArray(2, TENURED);
  CHECK(heap->old_space()->Contains(*array));
  array->set(0, reinterpret_cast<Object*>(fake_object), SKIP_WRITE_BARRIER);

  // Firstly, let's test the regular slots buffer entry.
  buffer->Add(HeapObject::RawField(*array, FixedArray::kHeaderSize));
  CHECK(reinterpret_cast<void*>(buffer->Get(0)) ==
        HeapObject::RawField(*array, FixedArray::kHeaderSize));
  SlotsBuffer::RemoveObjectSlots(CcTest::i_isolate()->heap(), buffer,
                                 array->address(),
                                 array->address() + array->Size());
  CHECK(reinterpret_cast<void*>(buffer->Get(0)) ==
        HeapObject::RawField(heap->empty_fixed_array(),
                             FixedArrayBase::kLengthOffset));

  // Secondly, let's test the typed slots buffer entry.
  SlotsBuffer::AddTo(NULL, &buffer, SlotsBuffer::EMBEDDED_OBJECT_SLOT,
                     array->address() + FixedArray::kHeaderSize,
                     SlotsBuffer::FAIL_ON_OVERFLOW);
  CHECK(reinterpret_cast<void*>(buffer->Get(1)) ==
        reinterpret_cast<Object**>(SlotsBuffer::EMBEDDED_OBJECT_SLOT));
  CHECK(reinterpret_cast<void*>(buffer->Get(2)) ==
        HeapObject::RawField(*array, FixedArray::kHeaderSize));
  SlotsBuffer::RemoveObjectSlots(CcTest::i_isolate()->heap(), buffer,
                                 array->address(),
                                 array->address() + array->Size());
  CHECK(reinterpret_cast<void*>(buffer->Get(1)) ==
        HeapObject::RawField(heap->empty_fixed_array(),
                             FixedArrayBase::kLengthOffset));
  CHECK(reinterpret_cast<void*>(buffer->Get(2)) ==
        HeapObject::RawField(heap->empty_fixed_array(),
                             FixedArrayBase::kLengthOffset));
  delete buffer;
}


TEST(ContextMeasure) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  LocalContext context;

  int size_upper_limit = 0;
  int count_upper_limit = 0;
  HeapIterator it(CcTest::heap());
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next()) {
    size_upper_limit += obj->Size();
    count_upper_limit++;
  }

  ContextMeasure measure(*isolate->native_context());

  PrintF("Context size        : %d bytes\n", measure.Size());
  PrintF("Context object count: %d\n", measure.Count());

  CHECK_LE(1000, measure.Count());
  CHECK_LE(50000, measure.Size());

  CHECK_LE(measure.Count(), count_upper_limit);
  CHECK_LE(measure.Size(), size_upper_limit);
}
}  // namespace internal
}  // namespace v8
