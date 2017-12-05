// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/conversions.h"
#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

BUILTIN(BigIntConstructor) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);

  // TODO(jkummerow): Implement properly.

  // Dummy implementation only takes Smi args.
  if (!value->IsSmi()) return isolate->heap()->undefined_value();
  int num = Smi::ToInt(*value);
  return *isolate->factory()->NewBigIntFromInt(num);
}

BUILTIN(BigIntConstructor_ConstructStub) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  DCHECK(*target == target->native_context()->bigint_function());
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));

  // TODO(jkummerow): Implement.
  USE(value);
  USE(result);

  UNIMPLEMENTED();
}

BUILTIN(BigIntParseInt) {
  HandleScope scope(isolate);
  Handle<Object> string = args.atOrUndefined(isolate, 1);
  Handle<Object> radix = args.atOrUndefined(isolate, 2);

  // Convert {string} to a String and flatten it.
  // Fast path: avoid back-and-forth conversion for Smi inputs.
  if (string->IsSmi() && radix->IsUndefined(isolate)) {
    int num = Smi::ToInt(*string);
    return *isolate->factory()->NewBigIntFromInt(num);
  }
  Handle<String> subject;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, subject,
                                     Object::ToString(isolate, string));
  subject = String::Flatten(subject);

  // Convert {radix} to Int32.
  if (!radix->IsNumber()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, radix, Object::ToNumber(radix));
  }
  int radix32 = DoubleToInt32(radix->Number());
  if (radix32 != 0 && (radix32 < 2 || radix32 > 36)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewSyntaxError(MessageTemplate::kToRadixFormatRange));
  }
  RETURN_RESULT_OR_FAILURE(isolate, StringToBigInt(isolate, subject, radix32));
}

BUILTIN(BigIntAsUintN) {
  HandleScope scope(isolate);
  Handle<Object> bits_obj = args.atOrUndefined(isolate, 1);
  Handle<Object> bigint_obj = args.atOrUndefined(isolate, 2);

  // TODO(jkummerow): Implement.
  USE(bits_obj);
  USE(bigint_obj);

  UNIMPLEMENTED();
}

BUILTIN(BigIntAsIntN) {
  HandleScope scope(isolate);
  Handle<Object> bits_obj = args.atOrUndefined(isolate, 1);
  Handle<Object> bigint_obj = args.atOrUndefined(isolate, 2);

  // TODO(jkummerow): Implement.
  USE(bits_obj);
  USE(bigint_obj);

  UNIMPLEMENTED();
}

BUILTIN(BigIntPrototypeToLocaleString) {
  HandleScope scope(isolate);

  // TODO(jkummerow): Implement.

  UNIMPLEMENTED();
}

namespace {

MaybeHandle<BigInt> ThisBigIntValue(Isolate* isolate, Handle<Object> value,
                                    const char* caller) {
  // 1. If Type(value) is BigInt, return value.
  if (value->IsBigInt()) return Handle<BigInt>::cast(value);
  // 2. If Type(value) is Object and value has a [[BigIntData]] internal slot:
  if (value->IsJSValue()) {
    // 2a. Assert: value.[[BigIntData]] is a BigInt value.
    // 2b. Return value.[[BigIntData]].
    Object* data = JSValue::cast(*value)->value();
    if (data->IsBigInt()) return handle(BigInt::cast(data), isolate);
  }
  // 3. Throw a TypeError exception.
  THROW_NEW_ERROR(
      isolate,
      NewTypeError(MessageTemplate::kNotGeneric,
                   isolate->factory()->NewStringFromAsciiChecked(caller),
                   isolate->factory()->NewStringFromStaticChars("BigInt")),
      BigInt);
}

}  // namespace

BUILTIN(BigIntPrototypeToString) {
  HandleScope scope(isolate);
  // 1. Let x be ? thisBigIntValue(this value).
  Handle<BigInt> x;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, x,
      ThisBigIntValue(isolate, args.receiver(), "BigInt.prototype.toString"));
  // 2. If radix is not present, let radixNumber be 10.
  // 3. Else if radix is undefined, let radixNumber be 10.
  Handle<Object> radix = args.atOrUndefined(isolate, 1);
  int radix_number;
  if (radix->IsUndefined(isolate)) {
    radix_number = 10;
  } else {
    // 4. Else, let radixNumber be ? ToInteger(radix).
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, radix,
                                       Object::ToInteger(isolate, radix));
    radix_number = static_cast<int>(radix->Number());
  }
  // 5. If radixNumber < 2 or radixNumber > 36, throw a RangeError exception.
  if (radix_number < 2 || radix_number > 36) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kToRadixFormatRange));
  }
  // Return the String representation of this Number value using the radix
  // specified by radixNumber.
  RETURN_RESULT_OR_FAILURE(isolate, BigInt::ToString(x, radix_number));
}

BUILTIN(BigIntPrototypeValueOf) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      ThisBigIntValue(isolate, args.receiver(), "BigInt.prototype.valueOf"));
}

}  // namespace internal
}  // namespace v8
