// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "cctest.h"

using namespace v8::internal;


class HandleArray : public Malloced {
 public:
  static const unsigned kArraySize = 200;
  explicit HandleArray() {}
  ~HandleArray() { Reset(v8::Isolate::GetCurrent()); }
  void Reset(v8::Isolate* isolate) {
    for (unsigned i = 0; i < kArraySize; i++) {
      if (handles_[i].IsEmpty()) continue;
      handles_[i].Dispose(isolate);
      handles_[i].Clear();
    }
  }
  v8::Persistent<v8::Value> handles_[kArraySize];
 private:
  DISALLOW_COPY_AND_ASSIGN(HandleArray);
};


// An aligned character array of size 1024.
class AlignedArray : public Malloced {
 public:
  static const unsigned kArraySize = 1024/sizeof(uint64_t);
  AlignedArray() { Reset(); }

  void Reset() {
    for (unsigned i = 0; i < kArraySize; i++) {
      data_[i] = 0;
    }
  }

  template<typename T>
  T As() { return reinterpret_cast<T>(data_); }

 private:
  uint64_t data_[kArraySize];
  DISALLOW_COPY_AND_ASSIGN(AlignedArray);
};


class DescriptorTestHelper {
 public:
  DescriptorTestHelper() :
      isolate_(NULL), array_(new AlignedArray), handle_array_(new HandleArray) {
    v8::V8::Initialize();
    isolate_ = v8::Isolate::GetCurrent();
  }
  v8::Isolate* isolate_;
  // Data objects.
  SmartPointer<AlignedArray> array_;
  SmartPointer<HandleArray> handle_array_;
 private:
  DISALLOW_COPY_AND_ASSIGN(DescriptorTestHelper);
};


static v8::Local<v8::ObjectTemplate> CreateConstructor(
    v8::Handle<v8::Context> context,
    const char* class_name,
    int internal_field,
    const char* descriptor_name = NULL,
    v8::Handle<v8::DeclaredAccessorDescriptor> descriptor =
        v8::Handle<v8::DeclaredAccessorDescriptor>()) {
  v8::Local<v8::FunctionTemplate> constructor = v8::FunctionTemplate::New();
  v8::Local<v8::ObjectTemplate> obj_template = constructor->InstanceTemplate();
  // Setup object template.
  if (descriptor_name != NULL && !descriptor.IsEmpty()) {
    bool added_accessor =
        obj_template->SetAccessor(v8_str(descriptor_name), descriptor);
    CHECK(added_accessor);
  }
  obj_template->SetInternalFieldCount((internal_field+1)*2 + 7);
  context->Global()->Set(v8_str(class_name), constructor->GetFunction());
  return obj_template;
}


static void VerifyRead(v8::Handle<v8::DeclaredAccessorDescriptor> descriptor,
                       int internal_field,
                       void* internal_object,
                       v8::Handle<v8::Value> expected_value) {
  LocalContext local_context;
  v8::HandleScope scope(local_context->GetIsolate());
  v8::Handle<v8::Context> context = local_context.local();
  CreateConstructor(context, "Accessible", internal_field, "x", descriptor);
  // Setup object.
  CompileRun("var accessible = new Accessible();");
  v8::Local<v8::Object> obj(
      v8::Object::Cast(*context->Global()->Get(v8_str("accessible"))));
  obj->SetAlignedPointerInInternalField(internal_field, internal_object);
  bool added_accessor;
  added_accessor = obj->SetAccessor(v8_str("y"), descriptor);
  CHECK(added_accessor);
  added_accessor = obj->SetAccessor(v8_str("13"), descriptor);
  CHECK(added_accessor);
  // Test access from template getter.
  v8::Local<v8::Value> value;
  value = CompileRun("accessible.x;");
  CHECK_EQ(expected_value, value);
  value = CompileRun("accessible['x'];");
  CHECK_EQ(expected_value, value);
  // Test access from object getter.
  value = CompileRun("accessible.y;");
  CHECK_EQ(expected_value, value);
  value = CompileRun("accessible['y'];");
  CHECK_EQ(expected_value, value);
  value = CompileRun("accessible[13];");
  CHECK_EQ(expected_value, value);
  value = CompileRun("accessible['13'];");
  CHECK_EQ(expected_value, value);
}


static v8::Handle<v8::Value> Convert(int32_t value, v8::Isolate* isolate) {
  return v8::Integer::New(value, isolate);
}


static v8::Handle<v8::Value> Convert(float value, v8::Isolate*) {
  return v8::Number::New(value);
}


static v8::Handle<v8::Value> Convert(double value, v8::Isolate*) {
  return v8::Number::New(value);
}


typedef v8::ObjectOperationDescriptor OOD;

template<typename T>
static void TestPrimitiveValue(
    T value,
    v8::DeclaredAccessorDescriptorDataType data_type,
    DescriptorTestHelper* helper) {
  v8::HandleScope handle_scope(helper->isolate_);
  int index = 17;
  int internal_field = 6;
  v8::Handle<v8::DeclaredAccessorDescriptor> descriptor =
      OOD::NewInternalFieldDereference(helper->isolate_, internal_field)
      ->NewRawShift(helper->isolate_, static_cast<uint16_t>(index*sizeof(T)))
      ->NewPrimitiveValue(helper->isolate_, data_type, 0);
  v8::Handle<v8::Value> expected = Convert(value, helper->isolate_);
  helper->array_->Reset();
  helper->array_->As<T*>()[index] = value;
  VerifyRead(descriptor, internal_field, *helper->array_, expected);
}


TEST(PrimitiveValueRead) {
  DescriptorTestHelper helper;
  TestPrimitiveValue<int32_t>(203, v8::kDescriptorInt32Type, &helper);
  TestPrimitiveValue<float>(23.7f, v8::kDescriptorFloatType, &helper);
  TestPrimitiveValue<double>(23.7, v8::kDescriptorDoubleType, &helper);
}


template<typename T>
static void TestBitmaskCompare(T bitmask,
                               T compare_value,
                               DescriptorTestHelper* helper) {
  v8::HandleScope handle_scope(helper->isolate_);
  int index = 13;
  int internal_field = 4;
  v8::Handle<v8::RawOperationDescriptor> raw_descriptor =
      OOD::NewInternalFieldDereference(helper->isolate_, internal_field)
      ->NewRawShift(helper->isolate_, static_cast<uint16_t>(index*sizeof(T)));
  v8::Handle<v8::DeclaredAccessorDescriptor> descriptor;
  switch (sizeof(T)) {
    case 1:
      descriptor = raw_descriptor->NewBitmaskCompare8(
            helper->isolate_,
            static_cast<uint8_t>(bitmask),
            static_cast<uint8_t>(compare_value));
      break;
    case 2:
      descriptor = raw_descriptor->NewBitmaskCompare16(
          helper->isolate_,
          static_cast<uint16_t>(bitmask),
          static_cast<uint16_t>(compare_value));
      break;
    case 4:
      descriptor = raw_descriptor->NewBitmaskCompare32(
          helper->isolate_,
          static_cast<uint32_t>(bitmask),
          static_cast<uint32_t>(compare_value));
      break;
    default:
      CHECK(false);
      break;
  }
  AlignedArray* array = *helper->array_;
  array->Reset();
  VerifyRead(descriptor, internal_field, array, v8::False(helper->isolate_));
  array->As<T*>()[index] = compare_value;
  VerifyRead(descriptor, internal_field, array, v8::True(helper->isolate_));
  helper->array_->As<T*>()[index] = compare_value & bitmask;
  VerifyRead(descriptor, internal_field, array, v8::True(helper->isolate_));
}


TEST(BitmaskCompareRead) {
  DescriptorTestHelper helper;
  TestBitmaskCompare<uint8_t>(0xf3, 0xa8, &helper);
  TestBitmaskCompare<uint16_t>(0xfefe, 0x7d42, &helper);
  TestBitmaskCompare<uint32_t>(0xfefeab18, 0x1234fdec, &helper);
}


TEST(PointerCompareRead) {
  DescriptorTestHelper helper;
  v8::HandleScope handle_scope(helper.isolate_);
  int index = 35;
  int internal_field = 3;
  void* ptr = helper.isolate_;
  v8::Handle<v8::DeclaredAccessorDescriptor> descriptor =
      OOD::NewInternalFieldDereference(helper.isolate_, internal_field)
      ->NewRawShift(helper.isolate_, static_cast<uint16_t>(index*sizeof(ptr)))
      ->NewPointerCompare(helper.isolate_, ptr);
  AlignedArray* array = *helper.array_;
  VerifyRead(descriptor, internal_field, array, v8::False(helper.isolate_));
  array->As<uintptr_t*>()[index] = reinterpret_cast<uintptr_t>(ptr);
  VerifyRead(descriptor, internal_field, array, v8::True(helper.isolate_));
}


TEST(PointerDereferenceRead) {
  DescriptorTestHelper helper;
  v8::HandleScope handle_scope(helper.isolate_);
  int first_index = 13;
  int internal_field = 7;
  int second_index = 11;
  int pointed_to_index = 75;
  uint16_t expected = 0x1425;
  v8::Handle<v8::DeclaredAccessorDescriptor> descriptor =
      OOD::NewInternalFieldDereference(helper.isolate_, internal_field)
      ->NewRawShift(helper.isolate_, first_index*kPointerSize)
      ->NewRawDereference(helper.isolate_)
      ->NewRawShift(helper.isolate_,
                    static_cast<uint16_t>(second_index*sizeof(int16_t)))
      ->NewPrimitiveValue(helper.isolate_, v8::kDescriptorInt16Type, 0);
  AlignedArray* array = *helper.array_;
  array->As<uintptr_t**>()[first_index] =
      &array->As<uintptr_t*>()[pointed_to_index];
  VerifyRead(descriptor, internal_field, array, v8::Integer::New(0));
  second_index += pointed_to_index*sizeof(uintptr_t)/sizeof(uint16_t);
  array->As<uint16_t*>()[second_index] = expected;
  VerifyRead(descriptor, internal_field, array, v8::Integer::New(expected));
}


TEST(HandleDereferenceRead) {
  DescriptorTestHelper helper;
  v8::HandleScope handle_scope(helper.isolate_);
  int index = 13;
  int internal_field = 0;
  v8::Handle<v8::DeclaredAccessorDescriptor> descriptor =
      OOD::NewInternalFieldDereference(helper.isolate_, internal_field)
      ->NewRawShift(helper.isolate_, index*kPointerSize)
      ->NewHandleDereference(helper.isolate_);
  HandleArray* array = *helper.handle_array_;
  v8::Handle<v8::String> expected = v8_str("whatever");
  array->handles_[index] = v8::Persistent<v8::Value>::New(helper.isolate_,
                                                          expected);
  VerifyRead(descriptor, internal_field, array, expected);
}
