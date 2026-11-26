// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <span>
#include <vector>

#include "src/api/api-inl.h"
#include "src/base/strings.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/object-conversions-inl.h"
#include "test/unittests/api/api-test-utils.h"
#include "test/unittests/heap/heap-utils.h"

namespace v8 {
namespace api_typed_array_unittest {

using ApiTypedArrayTest = ::v8::TestWithIsolate;

using ::v8::Array;
using ::v8::Context;
using ::v8::Local;
using ::v8::Value;

namespace {

void CheckElementValue(i::Isolate* isolate, int expected,
                       i::DirectHandle<i::JSAny> obj, int offset) {
  i::Tagged<i::Object> element =
      *i::Object::GetElement(isolate, obj, offset).ToHandleChecked();
  CHECK_EQ(expected, i::Smi::ToInt(element));
}

template <class ElementType>
void ObjectWithExternalArrayTestHelper(v8::Isolate* v8_isolate,
                                       Local<Context> context,
                                       v8::Local<v8::TypedArray> obj,
                                       int element_count,
                                       i::ExternalArrayType array_type,
                                       int64_t low, int64_t high) {
  i::DirectHandle<i::JSTypedArray> jsobj = v8::Utils::OpenDirectHandle(*obj);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  obj->Set(context, v8_str("field"), v8::Int32::New(v8_isolate, 1503))
      .FromJust();
  CHECK(context->Global()->Set(context, v8_str("ext_array"), obj).FromJust());
  v8::Local<v8::Value> result = CompileRun("ext_array.field");
  CHECK_EQ(1503, result->Int32Value(context).FromJust());
  result = CompileRun("ext_array[1]");
  CHECK_EQ(1, result->Int32Value(context).FromJust());

  // Check assigned smis
  result = CompileRun(
      "for (var i = 0; i < 8; i++) {"
      "  ext_array[i] = i;"
      "}"
      "var sum = 0;"
      "for (var i = 0; i < 8; i++) {"
      "  sum += ext_array[i];"
      "}"
      "sum;");

  CHECK_EQ(28, result->Int32Value(context).FromJust());
  // Check pass through of assigned smis
  result = CompileRun(
      "var sum = 0;"
      "for (var i = 0; i < 8; i++) {"
      "  sum += ext_array[i] = ext_array[i] = -i;"
      "}"
      "sum;");
  CHECK_EQ(-28, result->Int32Value(context).FromJust());

  // Check assigned smis in reverse order
  result = CompileRun(
      "for (var i = 8; --i >= 0; ) {"
      "  ext_array[i] = i;"
      "}"
      "var sum = 0;"
      "for (var i = 0; i < 8; i++) {"
      "  sum += ext_array[i];"
      "}"
      "sum;");
  CHECK_EQ(28, result->Int32Value(context).FromJust());

  // Check pass through of assigned HeapNumbers
  result = CompileRun(
      "var sum = 0;"
      "for (var i = 0; i < 16; i+=2) {"
      "  sum += ext_array[i] = ext_array[i] = (-i * 0.5);"
      "}"
      "sum;");
  CHECK_EQ(-28, result->Int32Value(context).FromJust());

  // Check assigned HeapNumbers
  result = CompileRun(
      "for (var i = 0; i < 16; i+=2) {"
      "  ext_array[i] = (i * 0.5);"
      "}"
      "var sum = 0;"
      "for (var i = 0; i < 16; i+=2) {"
      "  sum += ext_array[i];"
      "}"
      "sum;");
  CHECK_EQ(28, result->Int32Value(context).FromJust());

  // Check assigned HeapNumbers in reverse order
  result = CompileRun(
      "for (var i = 14; i >= 0; i-=2) {"
      "  ext_array[i] = (i * 0.5);"
      "}"
      "var sum = 0;"
      "for (var i = 0; i < 16; i+=2) {"
      "  sum += ext_array[i];"
      "}"
      "sum;");
  CHECK_EQ(28, result->Int32Value(context).FromJust());

  std::array<char, 1024> test_buf;

  // Check legal boundary conditions.
  // The repeated loads and stores ensure the ICs are exercised.
  const char* boundary_program =
      "var res = 0;"
      "for (var i = 0; i < 16; i++) {"
      "  ext_array[i] = %lld;"
      "  if (i > 8) {"
      "    res = ext_array[i];"
      "  }"
      "}"
      "res;";
  v8::base::SNPrintF(test_buf, boundary_program, low);
  result = CompileRun(test_buf.data());
  CHECK_EQ(low, result->IntegerValue(context).FromJust());

  v8::base::SNPrintF(test_buf, boundary_program, high);
  result = CompileRun(test_buf.data());
  CHECK_EQ(high, result->IntegerValue(context).FromJust());

  // Check misprediction of type in IC.
  result = CompileRun(
      "var tmp_array = ext_array;"
      "var sum = 0;"
      "for (var i = 0; i < 8; i++) {"
      "  tmp_array[i] = i;"
      "  sum += tmp_array[i];"
      "  if (i == 4) {"
      "    tmp_array = {};"
      "  }"
      "}"
      "sum;");
  // Force GC to trigger verification.
  i::InvokeMajorGC(isolate);
  CHECK_EQ(28, result->Int32Value(context).FromJust());

  // Make sure out-of-range loads do not throw.
  v8::base::SNPrintF(test_buf,
                     "var caught_exception = false;"
                     "try {"
                     "  ext_array[%d];"
                     "} catch (e) {"
                     "  caught_exception = true;"
                     "}"
                     "caught_exception;",
                     element_count);
  result = CompileRun(test_buf.data());
  CHECK(!result->BooleanValue(v8_isolate));

  // Make sure out-of-range stores do not throw.
  v8::base::SNPrintF(test_buf,
                     "var caught_exception = false;"
                     "try {"
                     "  ext_array[%d] = 1;"
                     "} catch (e) {"
                     "  caught_exception = true;"
                     "}"
                     "caught_exception;",
                     element_count);
  result = CompileRun(test_buf.data());
  CHECK(!result->BooleanValue(v8_isolate));

  // Check other boundary conditions, values and operations.
  result = CompileRun(
      "for (var i = 0; i < 8; i++) {"
      "  ext_array[7] = undefined;"
      "}"
      "ext_array[7];");
  CHECK_EQ(0, result->Int32Value(context).FromJust());
  if (array_type == i::kExternalFloat64Array ||
      array_type == i::kExternalFloat32Array) {
    CHECK(std::isnan(i::Object::NumberValue(Cast<i::Number>(
        *i::Object::GetElement(isolate, jsobj, 7).ToHandleChecked()))));
  } else {
    CheckElementValue(isolate, 0, jsobj, 7);
  }

  result = CompileRun(
      "for (var i = 0; i < 8; i++) {"
      "  ext_array[6] = '2.3';"
      "}"
      "ext_array[6];");
  CHECK_EQ(2, result->Int32Value(context).FromJust());
  CHECK_EQ(2,
           static_cast<int>(i::Object::NumberValue(Cast<i::Number>(
               *i::Object::GetElement(isolate, jsobj, 6).ToHandleChecked()))));

  if (array_type != i::kExternalFloat32Array &&
      array_type != i::kExternalFloat64Array) {
    // Though the specification doesn't state it, be explicit about
    // converting NaNs and +/-Infinity to zero.
    result = CompileRun(
        "for (var i = 0; i < 8; i++) {"
        "  ext_array[i] = 5;"
        "}"
        "for (var i = 0; i < 8; i++) {"
        "  ext_array[i] = NaN;"
        "}"
        "ext_array[5];");
    CHECK_EQ(0, result->Int32Value(context).FromJust());
    CheckElementValue(isolate, 0, jsobj, 5);

    result = CompileRun(
        "for (var i = 0; i < 8; i++) {"
        "  ext_array[i] = 5;"
        "}"
        "for (var i = 0; i < 8; i++) {"
        "  ext_array[i] = Infinity;"
        "}"
        "ext_array[5];");
    int expected_value =
        (array_type == i::kExternalUint8ClampedArray) ? 255 : 0;
    CHECK_EQ(expected_value, result->Int32Value(context).FromJust());
    CheckElementValue(isolate, expected_value, jsobj, 5);

    result = CompileRun(
        "for (var i = 0; i < 8; i++) {"
        "  ext_array[i] = 5;"
        "}"
        "for (var i = 0; i < 8; i++) {"
        "  ext_array[i] = -Infinity;"
        "}"
        "ext_array[5];");
    CHECK_EQ(0, result->Int32Value(context).FromJust());
    CheckElementValue(isolate, 0, jsobj, 5);

    // Check truncation behavior of integral arrays.
    const char* unsigned_data =
        "var source_data = [0.6, 10.6];"
        "var expected_results = [0, 10];";
    const char* signed_data =
        "var source_data = [0.6, 10.6, -0.6, -10.6];"
        "var expected_results = [0, 10, 0, -10];";
    const char* pixel_data =
        "var source_data = [0.6, 10.6];"
        "var expected_results = [1, 11];";
    bool is_unsigned = (array_type == i::kExternalUint8Array ||
                        array_type == i::kExternalUint16Array ||
                        array_type == i::kExternalUint32Array);
    bool is_pixel_data = array_type == i::kExternalUint8ClampedArray;

    v8::base::SNPrintF(
        test_buf,
        "%s"
        "var all_passed = true;"
        "for (var i = 0; i < source_data.length; i++) {"
        "  for (var j = 0; j < 8; j++) {"
        "    ext_array[j] = source_data[i];"
        "  }"
        "  all_passed = all_passed &&"
        "               (ext_array[5] == expected_results[i]);"
        "}"
        "all_passed;",
        (is_unsigned ? unsigned_data
                     : (is_pixel_data ? pixel_data : signed_data)));
    result = CompileRun(test_buf.data());
    CHECK(result->BooleanValue(v8_isolate));
  }

  {
    ElementType* data_ptr = static_cast<ElementType*>(jsobj->DataPtr());
    for (int i = 0; i < element_count; i++) {
      data_ptr[i] = static_cast<ElementType>(i);
    }
  }

  bool old_natives_flag_sentry = i::v8_flags.allow_natives_syntax;
  i::v8_flags.allow_natives_syntax.Set(true);

  // Test complex assignments
  result = CompileRun(
      "function ee_op_test_complex_func(sum) {"
      " for (var i = 0; i < 40; ++i) {"
      "   sum += (ext_array[i] += 1);"
      "   sum += (ext_array[i] -= 1);"
      " } "
      " return sum;"
      "};"
      "%PrepareFunctionForOptimization(ee_op_test_complex_func);"
      "sum=0;"
      "sum=ee_op_test_complex_func(sum);"
      "sum=ee_op_test_complex_func(sum);"
      "%OptimizeFunctionOnNextCall(ee_op_test_complex_func);"
      "sum=ee_op_test_complex_func(sum);"
      "sum;");
  CHECK_EQ(4800, result->Int32Value(context).FromJust());

  // Test count operations
  result = CompileRun(
      "function ee_op_test_count_func(sum) {"
      " for (var i = 0; i < 40; ++i) {"
      "   sum += (++ext_array[i]);"
      "   sum += (--ext_array[i]);"
      " } "
      " return sum;"
      "};"
      "%PrepareFunctionForOptimization(ee_op_test_count_func);"
      "sum=0;"
      "sum=ee_op_test_count_func(sum);"
      "sum=ee_op_test_count_func(sum);"
      "%OptimizeFunctionOnNextCall(ee_op_test_count_func);"
      "sum=ee_op_test_count_func(sum);"
      "sum;");
  CHECK_EQ(4800, result->Int32Value(context).FromJust());

  i::v8_flags.allow_natives_syntax.Set(old_natives_flag_sentry);

  result = CompileRun(
      "ext_array[3] = 33;"
      "delete ext_array[3];"
      "ext_array[3];");
  CHECK_EQ(33, result->Int32Value(context).FromJust());

  result = CompileRun(
      "ext_array[0] = 10; ext_array[1] = 11;"
      "ext_array[2] = 12; ext_array[3] = 13;"
      "try { ext_array.__defineGetter__('2', function() { return 120; }); }"
      "catch (e) { }"
      "ext_array[2];");
  CHECK_EQ(12, result->Int32Value(context).FromJust());

  result = CompileRun(
      "var js_array = new Array(40);"
      "js_array[0] = 77;"
      "js_array;");
  CHECK_EQ(77, v8::Object::Cast(*result)
                   ->Get(context, v8_str("0"))
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());

  result = CompileRun(
      "ext_array[1] = 23;"
      "ext_array.__proto__ = [];"
      "js_array.__proto__ = ext_array;"
      "js_array.concat(ext_array);");
  CHECK_EQ(77, v8::Object::Cast(*result)
                   ->Get(context, v8_str("0"))
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
  CHECK_EQ(23, v8::Object::Cast(*result)
                   ->Get(context, v8_str("1"))
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());

  result = CompileRun("ext_array[1] = 23;");
  CHECK_EQ(23, result->Int32Value(context).FromJust());
}

template <typename ElementType, typename TypedArray, class ArrayBufferType>
void TypedArrayTestHelper(i::ExternalArrayType array_type, int64_t low,
                          int64_t high) {
  const int kElementCount = 50;

  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope handle_scope(isolate);

  Local<ArrayBufferType> ab =
      ArrayBufferType::New(isolate, (kElementCount + 2) * sizeof(ElementType));
  Local<TypedArray> ta =
      TypedArray::New(ab, 2 * sizeof(ElementType), kElementCount);
  CheckInternalFieldsAreZero<v8::ArrayBufferView>(ta);
  CHECK_EQ(kElementCount, static_cast<int>(ta->Length()));
  CHECK_EQ(2 * sizeof(ElementType), ta->ByteOffset());
  CHECK_EQ(kElementCount * sizeof(ElementType), ta->ByteLength());
  CHECK(ab->Equals(env.local(), ta->GetBuffer()).FromJust());
  CHECK_EQ((std::is_same_v<ArrayBufferType, v8::SharedArrayBuffer>),
           ta->GetBuffer()->IsShared());

  ElementType* data =
      reinterpret_cast<ElementType*>(ab->GetBackingStore()->Data()) + 2;
  for (int i = 0; i < kElementCount; i++) {
    data[i] = static_cast<ElementType>(i);
  }

  ObjectWithExternalArrayTestHelper<ElementType>(
      env.isolate(), env.local(), ta, kElementCount, array_type, low, high);

  // TODO(v8:11111): Use API functions for testing these, once they're exposed
  // via the API.
  i::DirectHandle<i::JSTypedArray> i_ta = v8::Utils::OpenDirectHandle(*ta);
  CHECK(!i_ta->is_length_tracking());
  CHECK(!i_ta->is_backed_by_rab());
}

}  // namespace

THREADED_TEST(ApiTypedArrayTest, Uint8Array) {
  TypedArrayTestHelper<uint8_t, v8::Uint8Array, v8::ArrayBuffer>(
      i::kExternalUint8Array, 0, 0xFF);
}

THREADED_TEST(ApiTypedArrayTest, Int8Array) {
  TypedArrayTestHelper<int8_t, v8::Int8Array, v8::ArrayBuffer>(
      i::kExternalInt8Array, -0x80, 0x7F);
}

THREADED_TEST(ApiTypedArrayTest, Uint16Array) {
  TypedArrayTestHelper<uint16_t, v8::Uint16Array, v8::ArrayBuffer>(
      i::kExternalUint16Array, 0, 0xFFFF);
}

THREADED_TEST(ApiTypedArrayTest, Int16Array) {
  TypedArrayTestHelper<int16_t, v8::Int16Array, v8::ArrayBuffer>(
      i::kExternalInt16Array, -0x8000, 0x7FFF);
}

THREADED_TEST(ApiTypedArrayTest, Uint32Array) {
  TypedArrayTestHelper<uint32_t, v8::Uint32Array, v8::ArrayBuffer>(
      i::kExternalUint32Array, 0, UINT_MAX);
}

THREADED_TEST(ApiTypedArrayTest, Int32Array) {
  TypedArrayTestHelper<int32_t, v8::Int32Array, v8::ArrayBuffer>(
      i::kExternalInt32Array, INT_MIN, INT_MAX);
}

THREADED_TEST(ApiTypedArrayTest, Float32Array) {
  TypedArrayTestHelper<float, v8::Float32Array, v8::ArrayBuffer>(
      i::kExternalFloat32Array, -500, 500);
}

THREADED_TEST(ApiTypedArrayTest, Float64Array) {
  TypedArrayTestHelper<double, v8::Float64Array, v8::ArrayBuffer>(
      i::kExternalFloat64Array, -500, 500);
}

THREADED_TEST(ApiTypedArrayTest, Uint8ClampedArray) {
  TypedArrayTestHelper<uint8_t, v8::Uint8ClampedArray, v8::ArrayBuffer>(
      i::kExternalUint8ClampedArray, 0, 0xFF);
}

THREADED_TEST(ApiTypedArrayTest, DataView) {
  const int kSize = 50;

  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope handle_scope(isolate);

  Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 2 + kSize);
  Local<v8::DataView> dv = v8::DataView::New(ab, 2, kSize);
  CheckInternalFieldsAreZero<v8::ArrayBufferView>(dv);
  CHECK_EQ(2u, dv->ByteOffset());
  CHECK_EQ(kSize, static_cast<int>(dv->ByteLength()));
  CHECK(ab->Equals(env.local(), dv->GetBuffer()).FromJust());

  // TODO(v8:11111): Use API functions for testing these, once they're exposed
  // via the API.
  i::DirectHandle<i::JSDataViewOrRabGsabDataView> i_dv =
      v8::Utils::OpenDirectHandle(*dv);
  CHECK(!i_dv->is_length_tracking());
  CHECK(!i_dv->is_backed_by_rab());
}

THREADED_TEST(ApiTypedArrayTest, SharedUint8Array) {
  TypedArrayTestHelper<uint8_t, v8::Uint8Array, v8::SharedArrayBuffer>(
      i::kExternalUint8Array, 0, 0xFF);
}

THREADED_TEST(ApiTypedArrayTest, SharedInt8Array) {
  TypedArrayTestHelper<int8_t, v8::Int8Array, v8::SharedArrayBuffer>(
      i::kExternalInt8Array, -0x80, 0x7F);
}

THREADED_TEST(ApiTypedArrayTest, SharedUint16Array) {
  TypedArrayTestHelper<uint16_t, v8::Uint16Array, v8::SharedArrayBuffer>(
      i::kExternalUint16Array, 0, 0xFFFF);
}

THREADED_TEST(ApiTypedArrayTest, SharedInt16Array) {
  TypedArrayTestHelper<int16_t, v8::Int16Array, v8::SharedArrayBuffer>(
      i::kExternalInt16Array, -0x8000, 0x7FFF);
}

THREADED_TEST(ApiTypedArrayTest, SharedUint32Array) {
  TypedArrayTestHelper<uint32_t, v8::Uint32Array, v8::SharedArrayBuffer>(
      i::kExternalUint32Array, 0, UINT_MAX);
}

THREADED_TEST(ApiTypedArrayTest, SharedInt32Array) {
  TypedArrayTestHelper<int32_t, v8::Int32Array, v8::SharedArrayBuffer>(
      i::kExternalInt32Array, INT_MIN, INT_MAX);
}

THREADED_TEST(ApiTypedArrayTest, SharedFloat32Array) {
  TypedArrayTestHelper<float, v8::Float32Array, v8::SharedArrayBuffer>(
      i::kExternalFloat32Array, -500, 500);
}

THREADED_TEST(ApiTypedArrayTest, SharedFloat64Array) {
  TypedArrayTestHelper<double, v8::Float64Array, v8::SharedArrayBuffer>(
      i::kExternalFloat64Array, -500, 500);
}

THREADED_TEST(ApiTypedArrayTest, SharedUint8ClampedArray) {
  TypedArrayTestHelper<uint8_t, v8::Uint8ClampedArray, v8::SharedArrayBuffer>(
      i::kExternalUint8ClampedArray, 0, 0xFF);
}

THREADED_TEST(ApiTypedArrayTest, SharedDataView) {
  const int kSize = 50;

  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope handle_scope(isolate);

  Local<v8::SharedArrayBuffer> ab =
      v8::SharedArrayBuffer::New(isolate, 2 + kSize);
  Local<v8::DataView> dv = v8::DataView::New(ab, 2, kSize);
  CheckInternalFieldsAreZero<v8::ArrayBufferView>(dv);
  CHECK_EQ(2u, dv->ByteOffset());
  CHECK_EQ(kSize, static_cast<int>(dv->ByteLength()));
  CHECK(ab->Equals(env.local(), dv->GetBuffer()).FromJust());

  // TODO(v8:11111): Use API functions for testing these, once they're exposed
  // via the API.
  i::DirectHandle<i::JSDataViewOrRabGsabDataView> i_dv =
      v8::Utils::OpenDirectHandle(*dv);
  CHECK(!i_dv->is_length_tracking());
  CHECK(!i_dv->is_backed_by_rab());
}

#define IS_ARRAY_BUFFER_VIEW_TEST(View)                                     \
  THREADED_TEST(ApiTypedArrayTest, Is##View) {                              \
    LocalContext env;                                                       \
    v8::Isolate* isolate = env.isolate();                                   \
    v8::HandleScope handle_scope(isolate);                                  \
                                                                            \
    Local<Value> result = CompileRun(                                       \
        "var ab = new ArrayBuffer(128);"                                    \
        "new " #View "(ab)");                                               \
    CHECK(result->IsArrayBufferView());                                     \
    CHECK(result->Is##View());                                              \
    CheckInternalFieldsAreZero<v8::ArrayBufferView>(result.As<v8::View>()); \
  }

IS_ARRAY_BUFFER_VIEW_TEST(Uint8Array)
IS_ARRAY_BUFFER_VIEW_TEST(Int8Array)
IS_ARRAY_BUFFER_VIEW_TEST(Uint16Array)
IS_ARRAY_BUFFER_VIEW_TEST(Int16Array)
IS_ARRAY_BUFFER_VIEW_TEST(Uint32Array)
IS_ARRAY_BUFFER_VIEW_TEST(Int32Array)
IS_ARRAY_BUFFER_VIEW_TEST(Float32Array)
IS_ARRAY_BUFFER_VIEW_TEST(Float64Array)
IS_ARRAY_BUFFER_VIEW_TEST(Uint8ClampedArray)
IS_ARRAY_BUFFER_VIEW_TEST(DataView)

#undef IS_ARRAY_BUFFER_VIEW_TEST

TEST_F(ApiTypedArrayTest, InternalFieldsOnTypedArray) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.local();
  Context::Scope context_scope(context);
  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, 1);
  v8::Local<v8::Uint8Array> array = v8::Uint8Array::New(buffer, 0, 1);
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    CHECK_EQ(static_cast<void*>(nullptr),
             array->GetAlignedPointerFromInternalField(
                 i, v8::kEmbedderDataTypeTagDefault));
  }
}

TEST_F(ApiTypedArrayTest, InternalFieldsOnDataView) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.local();
  Context::Scope context_scope(context);
  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, 1);
  v8::Local<v8::DataView> array = v8::DataView::New(buffer, 0, 1);
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    CHECK_EQ(static_cast<void*>(nullptr),
             array->GetAlignedPointerFromInternalField(
                 i, v8::kEmbedderDataTypeTagDefault));
  }
}

TEST_F(ApiTypedArrayTest,
       DetachedArrayBufferViewsPretendOffsetAndLengthAreZero) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.local();
  Context::Scope context_scope(context);

  v8::Local<v8::ArrayBuffer> buffer = v8::Local<v8::ArrayBuffer>::Cast(
      CompileRun("let ab = new ArrayBuffer(100, {maxByteLength: 200}); ab"));

  v8::Local<v8::Uint16Array> fixed_length_ta =
      v8::Uint16Array::New(buffer, 2, 1);
  // Length-tracking TypedArrays cannot (yet) be constructed via the API.
  v8::Local<v8::Uint16Array> length_tracking_ta =
      v8::Local<v8::Uint16Array>::Cast(CompileRun("new Uint16Array(ab, 2)"));

  // RAB/GSAB-backed DataViews cannot (yet) be constructed via the API.
  v8::Local<v8::DataView> fixed_length_dv =
      v8::Local<v8::DataView>::Cast(CompileRun("new DataView(ab, 1, 1)"));
  v8::Local<v8::DataView> length_tracking_dv =
      v8::Local<v8::DataView>::Cast(CompileRun("new DataView(ab, 1)"));

  CHECK_EQ(2, fixed_length_ta->ByteOffset());
  CHECK_EQ(2, length_tracking_ta->ByteOffset());
  CHECK_EQ(1, fixed_length_dv->ByteOffset());
  CHECK_EQ(1, length_tracking_dv->ByteOffset());

  CHECK_EQ(2, fixed_length_ta->ByteLength());
  CHECK_EQ(98, length_tracking_ta->ByteLength());
  CHECK_EQ(1, fixed_length_dv->ByteLength());
  CHECK_EQ(99, length_tracking_dv->ByteLength());

  CHECK_EQ(1, fixed_length_ta->Length());
  CHECK_EQ(49, length_tracking_ta->Length());

  buffer->Detach({}).Check();

  CHECK_EQ(0, fixed_length_ta->ByteOffset());
  CHECK_EQ(0, length_tracking_ta->ByteOffset());
  CHECK_EQ(0, fixed_length_dv->ByteOffset());
  CHECK_EQ(0, length_tracking_dv->ByteOffset());

  CHECK_EQ(0, fixed_length_ta->ByteLength());
  CHECK_EQ(0, length_tracking_ta->ByteLength());
  CHECK_EQ(0, fixed_length_dv->ByteLength());
  CHECK_EQ(0, length_tracking_dv->ByteLength());

  CHECK_EQ(0, fixed_length_ta->Length());
  CHECK_EQ(0, length_tracking_ta->Length());
}

TEST_F(ApiTypedArrayTest,
       OutOfBoundsArrayBufferViewsPretendOffsetAndLengthAreZero) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.local();
  Context::Scope context_scope(context);

  v8::Local<v8::ArrayBuffer> buffer = v8::Local<v8::ArrayBuffer>::Cast(
      CompileRun("let ab = new ArrayBuffer(100, {maxByteLength: 200}); ab"));

  v8::Local<v8::Uint16Array> fixed_length_ta =
      v8::Uint16Array::New(buffer, 2, 1);
  // Length-tracking TypedArrays cannot (yet) be constructed via the API.
  v8::Local<v8::Uint16Array> length_tracking_ta =
      v8::Local<v8::Uint16Array>::Cast(CompileRun("new Uint16Array(ab, 2)"));

  // RAB/GSAB-backed DataViews cannot (yet) be constructed via the API.
  v8::Local<v8::DataView> fixed_length_dv =
      v8::Local<v8::DataView>::Cast(CompileRun("new DataView(ab, 1, 1)"));
  v8::Local<v8::DataView> length_tracking_dv =
      v8::Local<v8::DataView>::Cast(CompileRun("new DataView(ab, 1)"));

  CHECK_EQ(2, fixed_length_ta->ByteOffset());
  CHECK_EQ(2, length_tracking_ta->ByteOffset());
  CHECK_EQ(1, fixed_length_dv->ByteOffset());
  CHECK_EQ(1, length_tracking_dv->ByteOffset());

  CHECK_EQ(2, fixed_length_ta->ByteLength());
  CHECK_EQ(98, length_tracking_ta->ByteLength());
  CHECK_EQ(1, fixed_length_dv->ByteLength());
  CHECK_EQ(99, length_tracking_dv->ByteLength());

  CHECK_EQ(1, fixed_length_ta->Length());
  CHECK_EQ(49, length_tracking_ta->Length());

  CompileRun("ab.resize(0)");

  CHECK_EQ(0, fixed_length_ta->ByteOffset());
  CHECK_EQ(0, length_tracking_ta->ByteOffset());
  CHECK_EQ(0, fixed_length_dv->ByteOffset());
  CHECK_EQ(0, length_tracking_dv->ByteOffset());

  CHECK_EQ(0, fixed_length_ta->ByteLength());
  CHECK_EQ(0, length_tracking_ta->ByteLength());
  CHECK_EQ(0, fixed_length_dv->ByteLength());
  CHECK_EQ(0, length_tracking_dv->ByteLength());

  CHECK_EQ(0, fixed_length_ta->Length());
  CHECK_EQ(0, length_tracking_ta->Length());
}

namespace {
void TestOnHeapHasBuffer(const char* array_name, size_t elem_size) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope handle_scope(isolate);

  std::array<char, 128> source;
  // Test on-heap sizes.
  for (size_t size = 0; size <= i::JSTypedArray::kMaxSizeInHeap;
       size += elem_size) {
    size_t length = size / elem_size;
    v8::base::SNPrintF(source, "new %sArray(%zu)", array_name, length);
    auto typed_array =
        v8::Local<v8::TypedArray>::Cast(CompileRun(source.data()));

    CHECK_EQ(length, typed_array->Length());

    // Should not (yet) have a buffer.
    CHECK(!typed_array->HasBuffer());

    // Get the buffer and check its length.
    i::DirectHandle<i::JSTypedArray> i_typed_array =
        v8::Utils::OpenDirectHandle(*typed_array);
    auto i_array_buffer1 = i_typed_array->GetBuffer(env.i_isolate());
    CHECK_EQ(size, i_array_buffer1->byte_length());
    CHECK(typed_array->HasBuffer());

    // Should have the same buffer each time.
    auto i_array_buffer2 = i_typed_array->GetBuffer(env.i_isolate());
    CHECK(i_array_buffer1.is_identical_to(i_array_buffer2));
  }
}

void TestOffHeapHasBuffer(const char* array_name, size_t elem_size) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope handle_scope(isolate);

  std::array<char, 128> source;
  // Test off-heap sizes.
  size_t size = i::JSTypedArray::kMaxSizeInHeap;
  for (int i = 0; i < 3; i++) {
    size_t length = 1 + (size / elem_size);
    v8::base::SNPrintF(source, "new %sArray(%zu)", array_name, length);
    auto typed_array =
        v8::Local<v8::TypedArray>::Cast(CompileRun(source.data()));
    CHECK_EQ(length, typed_array->Length());

    // Should already have a buffer.
    CHECK(typed_array->HasBuffer());

    // Get the buffer and check its length.
    i::DirectHandle<i::JSTypedArray> i_typed_array =
        v8::Utils::OpenDirectHandle(*typed_array);
    auto i_array_buffer1 = i_typed_array->GetBuffer(env.i_isolate());
    CHECK_EQ(length * elem_size, i_array_buffer1->byte_length());

    size *= 2;
  }
}

}  // namespace

#define TEST_HAS_BUFFER(array_name, elem_size)                      \
  TEST_F(ApiTypedArrayTest, OnHeap_##array_name##Array_HasBuffer) { \
    TestOnHeapHasBuffer(#array_name, elem_size);                    \
  }                                                                 \
  TEST_F(ApiTypedArrayTest, OffHeap_##array_name##_HasBuffer) {     \
    TestOffHeapHasBuffer(#array_name, elem_size);                   \
  }

TEST_HAS_BUFFER(Uint8, 1)
TEST_HAS_BUFFER(Int8, 1)
TEST_HAS_BUFFER(Uint16, 2)
TEST_HAS_BUFFER(Int16, 2)
TEST_HAS_BUFFER(Uint32, 4)
TEST_HAS_BUFFER(Int32, 4)
TEST_HAS_BUFFER(Float32, 4)
TEST_HAS_BUFFER(Float64, 8)

#undef TEST_HAS_BUFFER

TEST_F(ApiTypedArrayTest, Uint8Array_MaybeNew_Empty) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Uint8Array> arr =
      v8::Uint8Array::MaybeNew(isolate, std::span<const uint8_t>{})
          .ToLocalChecked();
  EXPECT_EQ(0u, arr->Length());
  EXPECT_EQ(0u, arr->ByteLength());
  EXPECT_EQ(0u, arr->ByteOffset());
  EXPECT_EQ(i::JSTypedArray::kMaxSizeInHeap == 0, arr->HasBuffer());

  v8::Local<v8::ArrayBufferBase> buffer = arr->GetBuffer();
  EXPECT_EQ(0u, buffer->ByteLength());
  EXPECT_TRUE(arr->HasBuffer());
}

TEST_F(ApiTypedArrayTest, Uint8Array_MaybeNew_OnHeapAndOffHeapSizes) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::Local<v8::Context> context = env.local();
  v8::HandleScope handle_scope(isolate);

  std::vector<size_t> sizes;
  if (i::JSTypedArray::kMaxSizeInHeap > 0) {
    sizes.push_back(1);
    if (i::JSTypedArray::kMaxSizeInHeap > 16) {
      sizes.push_back(16);
    }
    if (i::JSTypedArray::kMaxSizeInHeap > 2) {
      sizes.push_back(i::JSTypedArray::kMaxSizeInHeap - 1);
    }
    sizes.push_back(i::JSTypedArray::kMaxSizeInHeap);
  }
  sizes.push_back(i::JSTypedArray::kMaxSizeInHeap + 1);
  sizes.push_back(128);
  sizes.push_back(4096);

  for (size_t size : sizes) {
    std::vector<uint8_t> input(size);
    for (size_t i = 0; i < size; ++i) {
      input[i] = static_cast<uint8_t>((i * 37 + 13) & 0xff);
    }

    v8::Local<v8::Uint8Array> arr =
        v8::Uint8Array::MaybeNew(isolate, input).ToLocalChecked();
    EXPECT_EQ(size, arr->Length());
    EXPECT_EQ(size, arr->ByteLength());
    EXPECT_EQ(0u, arr->ByteOffset());
    EXPECT_EQ(i::JSTypedArray::kMaxSizeInHeap == 0 ||
                  size > i::JSTypedArray::kMaxSizeInHeap,
              arr->HasBuffer());

    std::vector<uint8_t> output(size);
    EXPECT_EQ(size, arr->CopyContents(output.data(), output.size()));
    EXPECT_EQ(input, output);

    CHECK(context->Global()->Set(context, v8_str("u8"), arr).FromJust());
    EXPECT_TRUE(CompileRun("u8 instanceof Uint8Array")->BooleanValue(isolate));
    EXPECT_EQ(static_cast<int32_t>(input[0]),
              CompileRun("u8[0]")->Int32Value(context).FromJust());
    EXPECT_EQ(static_cast<int32_t>(input[size - 1]),
              CompileRun("u8[u8.length - 1]")->Int32Value(context).FromJust());

    // Mutating the source span must not affect the Uint8Array.
    const uint8_t original_first = input[0];
    input[0] ^= 0xff;
    EXPECT_EQ(static_cast<int32_t>(original_first),
              CompileRun("u8[0]")->Int32Value(context).FromJust());

    // Accessing .buffer materializes an ArrayBuffer aliasing the same bytes.
    v8::Local<v8::Value> buffer_len = CompileRun("u8.buffer.byteLength");
    EXPECT_EQ(static_cast<int32_t>(size),
              buffer_len->Int32Value(context).FromJust());
    EXPECT_TRUE(arr->HasBuffer());
    CompileRun("new Uint8Array(u8.buffer)[0] = 0x5a;");
    EXPECT_EQ(0x5a, CompileRun("u8[0]")->Int32Value(context).FromJust());
  }
}

TEST_F(ApiTypedArrayTest, Uint8Array_MaybeNew_SurvivesGC) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::Local<v8::Context> context = env.local();
  v8::HandleScope handle_scope(isolate);

  std::array<uint8_t, 32> on_heap_input;
  for (size_t i = 0; i < on_heap_input.size(); ++i) {
    on_heap_input[i] = static_cast<uint8_t>(i + 1);
  }
  std::array<uint8_t, 128> off_heap_input;
  for (size_t i = 0; i < off_heap_input.size(); ++i) {
    off_heap_input[i] = static_cast<uint8_t>(255 - i);
  }

  v8::Local<v8::Uint8Array> on_heap =
      v8::Uint8Array::MaybeNew(isolate, on_heap_input).ToLocalChecked();
  v8::Local<v8::Uint8Array> off_heap =
      v8::Uint8Array::MaybeNew(isolate, off_heap_input).ToLocalChecked();

  CHECK(context->Global()->Set(context, v8_str("on_heap"), on_heap).FromJust());
  CHECK(
      context->Global()->Set(context, v8_str("off_heap"), off_heap).FromJust());

  i::InvokeMinorGC(env.i_isolate());
  i::InvokeMajorGC(env.i_isolate());

  std::array<uint8_t, 32> on_heap_output;
  EXPECT_EQ(on_heap_input.size(),
            on_heap->CopyContents(on_heap_output.data(), 32));
  EXPECT_EQ(on_heap_input, on_heap_output);

  std::array<uint8_t, 128> off_heap_output;
  EXPECT_EQ(
      off_heap_input.size(),
      off_heap->CopyContents(off_heap_output.data(), off_heap_output.size()));
  EXPECT_EQ(off_heap_input, off_heap_output);

  EXPECT_EQ(
      32,
      CompileRun("on_heap.buffer.byteLength")->Int32Value(context).FromJust());
  EXPECT_EQ(
      128,
      CompileRun("off_heap.buffer.byteLength")->Int32Value(context).FromJust());
}

class RestrictedAllocator final : public v8::ArrayBuffer::Allocator {
 public:
  explicit RestrictedAllocator(size_t limit) : limit_(limit) {}
  ~RestrictedAllocator() override { delete underlying_allocator_; }

  void* Allocate(size_t length) override {
    return underlying_allocator_->Allocate(length);
  }

  void* AllocateUninitialized(size_t length) override {
    return underlying_allocator_->AllocateUninitialized(length);
  }

  void Free(void* data, size_t length) override {
    underlying_allocator_->Free(data, length);
  }

  size_t MaxAllocationSize() const override { return limit_; }

 private:
  size_t limit_;
  v8::ArrayBuffer::Allocator* underlying_allocator_ =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
};

TEST_F(ApiTypedArrayTest, Uint8Array_MaybeNew_AllocationFailure) {
  constexpr size_t kLimit = i::JSTypedArray::kMaxSizeInHeap + 64;
  RestrictedAllocator allocator(kLimit);
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = &allocator;

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  isolate->Enter();
  {
    v8::HandleScope handle_scope(isolate);
    LocalContext context(isolate);

    std::vector<uint8_t> data(kLimit + 1, 0xab);
    EXPECT_FALSE(v8::Uint8Array::MaybeNew(
                     isolate, std::span<const uint8_t>(data).first(kLimit))
                     .IsEmpty());
    EXPECT_TRUE(v8::Uint8Array::MaybeNew(isolate, data).IsEmpty());
  }
  isolate->Exit();
  isolate->Dispose();
}

}  // namespace api_typed_array_unittest
}  // namespace v8
