// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "v8chakra.h"
#include <math.h>

namespace v8 {

using jsrt::ContextShim;

static bool IsOfType(const Value* ref, JsValueType type) {
  JsValueType valueType;
  if (JsGetValueType(const_cast<Value*>(ref), &valueType) != JsNoError) {
    return false;
  }
  return valueType == type;
}

static bool IsOfType(const Value* ref, ContextShim::GlobalType index) {
  return jsrt::InstanceOf(const_cast<Value*>(ref),
                          ContextShim::GetCurrent()->GetGlobalType(index));
}

bool Value::IsUndefined() const {
  return IsOfType(this, JsValueType::JsUndefined);
}

bool Value::IsNull() const {
  return IsOfType(this, JsValueType::JsNull);
}

bool Value::IsTrue() const {
  bool isTrue;
  if (JsEquals(jsrt::GetTrue(), (JsValueRef)this, &isTrue) != JsNoError) {
    return false;
  }

  return isTrue;
}

bool Value::IsFalse() const {
  bool isFalse;
  if (JsEquals(jsrt::GetFalse(), (JsValueRef)this, &isFalse) != JsNoError) {
    return false;
  }

  return isFalse;
}

bool Value::IsString() const {
  return IsOfType(this, JsValueType::JsString);
}

bool Value::IsFunction() const {
  return IsOfType(this, JsValueType::JsFunction);
}

bool Value::IsArray() const {
  return IsOfType(this, JsValueType::JsArray);
}

bool Value::IsObject() const {
  JsValueType type;
  if (JsGetValueType((JsValueRef)this, &type) != JsNoError) {
    return false;
  }

  return type >= JsValueType::JsObject && type != JsSymbol;
}

bool Value::IsExternal() const {
  return External::IsExternal(this);
}

bool Value::IsArrayBuffer() const {
  return IsOfType(this, JsValueType::JsArrayBuffer);
}

bool Value::IsTypedArray() const {
  return IsOfType(this, JsValueType::JsTypedArray);
}

#define DEFINE_TYPEDARRAY_CHECK(ArrayType) \
  bool Value::Is##ArrayType##Array() const { \
  JsTypedArrayType typedArrayType; \
  return JsGetTypedArrayInfo(const_cast<Value*>(this), \
                             &typedArrayType, \
                             nullptr, nullptr, nullptr) == JsNoError && \
  typedArrayType == JsTypedArrayType::JsArrayType##ArrayType; \
}

DEFINE_TYPEDARRAY_CHECK(Uint8)
DEFINE_TYPEDARRAY_CHECK(Uint8Clamped)
DEFINE_TYPEDARRAY_CHECK(Int8)
DEFINE_TYPEDARRAY_CHECK(Uint16)
DEFINE_TYPEDARRAY_CHECK(Int16)
DEFINE_TYPEDARRAY_CHECK(Uint32)
DEFINE_TYPEDARRAY_CHECK(Int32)
DEFINE_TYPEDARRAY_CHECK(Float32)
DEFINE_TYPEDARRAY_CHECK(Float64)

bool Value::IsDataView() const {
  return IsOfType(this, JsValueType::JsDataView);
}

bool Value::IsBoolean() const {
  return IsOfType(this, JsValueType::JsBoolean);
}

bool Value::IsNumber() const {
  return IsOfType(this, JsValueType::JsNumber);
}

bool Value::IsInt32() const {
  if (!IsNumber()) {
    return false;
  }

  double value = NumberValue();

  // check that the value is smaller than int 32 bit maximum
  if (value > INT_MAX || value < INT_MIN) {
    return false;
  }

  return trunc(value) == value;
}

bool Value::IsUint32() const {
  if (!IsNumber()) {
    return false;
  }

  double value = NumberValue();
  // check that the value is smaller than 32 bit maximum
  if (value < 0 || value > UINT_MAX) {
    return false;
  }

  return trunc(value) == value;
}

bool Value::IsDate() const {
  return IsOfType(this, ContextShim::GlobalType::Date);
}

bool Value::IsBooleanObject() const {
  return IsOfType(this, ContextShim::GlobalType::Boolean);
}

bool Value::IsNumberObject() const {
  return IsOfType(this, ContextShim::GlobalType::Number);
}

bool Value::IsStringObject() const {
  return IsOfType(this, ContextShim::GlobalType::String);
}
bool Value::IsMap() const
{
    return IsOfType(this, ContextShim::GlobalType::Map);
}

bool Value::IsSet() const
{
    return IsOfType(this, ContextShim::GlobalType::Set);
}

bool Value::IsNativeError() const {
  return IsOfType(this, ContextShim::GlobalType::Error)
    || IsOfType(this, ContextShim::GlobalType::EvalError)
    || IsOfType(this, ContextShim::GlobalType::RangeError)
    || IsOfType(this, ContextShim::GlobalType::ReferenceError)
    || IsOfType(this, ContextShim::GlobalType::SyntaxError)
    || IsOfType(this, ContextShim::GlobalType::TypeError)
    || IsOfType(this, ContextShim::GlobalType::URIError);
}

bool Value::IsRegExp() const {
  return IsOfType(this, ContextShim::GlobalType::RegExp);
}

bool Value::IsMapIterator() const {
  JsValueRef resultRef = JS_INVALID_REFERENCE;
  JsErrorCode errorCode = jsrt::IsValueMapIterator(
    const_cast<Value*>(this), &resultRef);
  if (errorCode != JsNoError) {
    return false;
  }
  return Local<Value>(resultRef)->BooleanValue();
}

bool Value::IsSetIterator() const {
  JsValueRef resultRef = JS_INVALID_REFERENCE;
  JsErrorCode errorCode = jsrt::IsValueSetIterator(
    const_cast<Value*>(this), &resultRef);
  if (errorCode != JsNoError) {
    return false;
  }
  return Local<Value>(resultRef)->BooleanValue();
}

bool Value::IsPromise() const {
  return IsOfType(this, ContextShim::GlobalType::Promise);
}

MaybeLocal<Boolean> Value::ToBoolean(Local<Context> context) const {
  JsValueRef value;
  if (JsConvertValueToBoolean((JsValueRef)this, &value) != JsNoError) {
    return Local<Boolean>();
  }

  return Local<Boolean>::New(value);
}

MaybeLocal<Number> Value::ToNumber(Local<Context> context) const {
  JsValueRef value;
  if (JsConvertValueToNumber((JsValueRef)this, &value) != JsNoError) {
    return Local<Number>();
  }

  return Local<Number>::New(value);
}

MaybeLocal<String> Value::ToString(Local<Context> context) const {
  JsValueRef value;
  if (JsConvertValueToString((JsValueRef)this, &value) != JsNoError) {
    return Local<String>();
  }

  return Local<String>::New(value);
}

MaybeLocal<String> Value::ToDetailString(Local<Context> context) const {
  return ToString(context);
}

MaybeLocal<Object> Value::ToObject(Local<Context> context) const {
  JsValueRef value;
  if (JsConvertValueToObject((JsValueRef)this, &value) != JsNoError) {
    return Local<Object>();
  }

  return Local<Object>::New(value);
}

MaybeLocal<Integer> Value::ToInteger(Local<Context> context) const {
  Maybe<double> maybe = NumberValue(context);
  if (maybe.IsNothing()) {
    return Local<Integer>();
  }
  double doubleValue = maybe.FromJust();

  double value = isfinite(doubleValue) ? trunc(doubleValue) :
                  (isnan(doubleValue) ? 0 : doubleValue);
  int intValue = static_cast<int>(value);
  if (value == intValue) {
    return Integer::New(nullptr, intValue);
  }

  Value* result = value == doubleValue ?
                    const_cast<Value*>(this) : *Number::New(nullptr, value);
  return Local<Integer>(reinterpret_cast<Integer*>(result));
}

MaybeLocal<Uint32> Value::ToUint32(Local<Context> context) const {
  Local<Integer> jsValue = Integer::NewFromUnsigned(nullptr,
                                                    this->Uint32Value());
  return jsValue.As<Uint32>();
}

MaybeLocal<Int32> Value::ToInt32(Local<Context> context) const {
  Local<Integer> jsValue = Integer::New(nullptr, this->Int32Value());
  return jsValue.As<Int32>();
}

Local<Boolean> Value::ToBoolean(Isolate* isolate) const {
  return FromMaybe(ToBoolean(Local<Context>()));
}

Local<Number> Value::ToNumber(Isolate* isolate) const {
  return FromMaybe(ToNumber(Local<Context>()));
}

Local<String> Value::ToString(Isolate* isolate) const {
  return FromMaybe(ToString(Local<Context>()));
}

Local<String> Value::ToDetailString(Isolate* isolate) const {
  return FromMaybe(ToDetailString(Local<Context>()));
}

Local<Object> Value::ToObject(Isolate* isolate) const {
  return FromMaybe(ToObject(Local<Context>()));
}

Local<Integer> Value::ToInteger(Isolate* isolate) const {
  return FromMaybe(ToInteger(Local<Context>()));
}

Local<Uint32> Value::ToUint32(Isolate* isolate) const {
  return FromMaybe(ToUint32(Local<Context>()));
}

Local<Int32> Value::ToInt32(Isolate* isolate) const {
  return FromMaybe(ToInt32(Local<Context>()));
}

MaybeLocal<Uint32> Value::ToArrayIndex(Local<Context> context) const {
  if (IsNumber()) {
    return ToUint32(context);
  }

  MaybeLocal<String> maybeString = ToString(context);
  bool isUint32;
  uint32_t uint32Value;
  if (maybeString.IsEmpty() ||
      jsrt::TryParseUInt32(*FromMaybe(maybeString),
                           &isUint32, &uint32Value) != JsNoError) {
    return Local<Uint32>();
  }

  return Integer::NewFromUnsigned(nullptr, uint32Value).As<Uint32>();
}

Local<Uint32> Value::ToArrayIndex() const {
  return FromMaybe(ToArrayIndex(Local<Context>()));
}

Maybe<bool> Value::BooleanValue(Local<Context> context) const {
  bool value;
  if (jsrt::ValueToNative</*LIKELY*/true>(JsConvertValueToBoolean,
                                          JsBooleanToBool,
                                          (JsValueRef)this,
                                          &value) != JsNoError) {
    return Nothing<bool>();
  }
  return Just(value);
}

Maybe<double> Value::NumberValue(Local<Context> context) const {
  double value;
  if (jsrt::ValueToDoubleLikely((JsValueRef)this, &value) != JsNoError) {
    return Nothing<double>();
  }
  return Just(value);
}

Maybe<int64_t> Value::IntegerValue(Local<Context> context) const {
  Maybe<double> maybe = NumberValue(context);
  return maybe.IsNothing() ?
    Nothing<int64_t>() : Just(static_cast<int64_t>(maybe.FromJust()));
}

Maybe<uint32_t> Value::Uint32Value(Local<Context> context) const {
  Maybe<int32_t> maybe = Int32Value(context);
  return maybe.IsNothing() ?
    Nothing<uint32_t>() : Just(static_cast<uint32_t>(maybe.FromJust()));
}

Maybe<int32_t> Value::Int32Value(Local<Context> context) const {
  int intValue;
  if (jsrt::ValueToIntLikely((JsValueRef)this, &intValue) != JsNoError) {
    return Nothing<int32_t>();
  }
  return Just(intValue);
}

bool Value::BooleanValue() const {
  return FromMaybe(BooleanValue(Local<Context>()));
}

double Value::NumberValue() const {
  return FromMaybe(NumberValue(Local<Context>()));
}

int64_t Value::IntegerValue() const {
  return FromMaybe(IntegerValue(Local<Context>()));
}

uint32_t Value::Uint32Value() const {
  return FromMaybe(Uint32Value(Local<Context>()));
}

int32_t Value::Int32Value() const {
  return FromMaybe(Int32Value(Local<Context>()));
}

Maybe<bool> Value::Equals(Local<Context> context, Handle<Value> that) const {
  bool equals;
  if (JsEquals((JsValueRef)this, *that, &equals) != JsNoError) {
    return Nothing<bool>();
  }

  return Just(equals);
}

bool Value::Equals(Handle<Value> that) const {
  return FromMaybe(Equals(Local<Context>(), that));
}

bool Value::StrictEquals(Handle<Value> that) const {
  bool strictEquals;
  if (JsStrictEquals((JsValueRef)this, *that, &strictEquals) != JsNoError) {
    return false;
  }

  return strictEquals;
}

}  // namespace v8
