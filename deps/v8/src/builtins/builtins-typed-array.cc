// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/logging/counters.h"
#include "src/objects/elements.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 22.2 TypedArray Objects

// ES6 section 22.2.3.1 get %TypedArray%.prototype.buffer
BUILTIN(TypedArrayPrototypeBuffer) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSTypedArray, typed_array,
                 "get %TypedArray%.prototype.buffer");
  return *typed_array->GetBuffer();
}

namespace {

int64_t CapRelativeIndex(Handle<Object> num, int64_t minimum, int64_t maximum) {
  if (V8_LIKELY(num->IsSmi())) {
    int64_t relative = Smi::ToInt(*num);
    return relative < 0 ? std::max<int64_t>(relative + maximum, minimum)
                        : std::min<int64_t>(relative, maximum);
  } else {
    DCHECK(num->IsHeapNumber());
    double relative = HeapNumber::cast(*num).value();
    DCHECK(!std::isnan(relative));
    return static_cast<int64_t>(
        relative < 0 ? std::max<double>(relative + maximum, minimum)
                     : std::min<double>(relative, maximum));
  }
}

}  // namespace

BUILTIN(TypedArrayPrototypeCopyWithin) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.copyWithin";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  int64_t len = array->length();
  int64_t to = 0;
  int64_t from = 0;
  int64_t final = len;

  if (V8_LIKELY(args.length() > 1)) {
    Handle<Object> num;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num, Object::ToInteger(isolate, args.at<Object>(1)));
    to = CapRelativeIndex(num, 0, len);

    if (args.length() > 2) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, num, Object::ToInteger(isolate, args.at<Object>(2)));
      from = CapRelativeIndex(num, 0, len);

      Handle<Object> end = args.atOrUndefined(isolate, 3);
      if (!end->IsUndefined(isolate)) {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, num,
                                           Object::ToInteger(isolate, end));
        final = CapRelativeIndex(num, 0, len);
      }
    }
  }

  int64_t count = std::min<int64_t>(final - from, len - to);
  if (count <= 0) return *array;

  // TypedArray buffer may have been transferred/detached during parameter
  // processing above. Return early in this case, to prevent potential UAF error
  // TODO(caitp): throw here, as though the full algorithm were performed (the
  // throw would have come from ecma262/#sec-integerindexedelementget)
  // (see )
  if (V8_UNLIKELY(array->WasDetached())) return *array;

  // Ensure processed indexes are within array bounds
  DCHECK_GE(from, 0);
  DCHECK_LT(from, len);
  DCHECK_GE(to, 0);
  DCHECK_LT(to, len);
  DCHECK_GE(len - count, 0);

  size_t element_size = array->element_size();
  to = to * element_size;
  from = from * element_size;
  count = count * element_size;

  uint8_t* data = static_cast<uint8_t*>(array->DataPtr());
  std::memmove(data + to, data + from, count);

  return *array;
}

BUILTIN(TypedArrayPrototypeFill) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.fill";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));
  ElementsKind kind = array->GetElementsKind();

  Handle<Object> obj_value = args.atOrUndefined(isolate, 1);
  if (kind == BIGINT64_ELEMENTS || kind == BIGUINT64_ELEMENTS) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, obj_value,
                                       BigInt::FromObject(isolate, obj_value));
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, obj_value,
                                       Object::ToNumber(isolate, obj_value));
  }

  int64_t len = array->length();
  int64_t start = 0;
  int64_t end = len;

  if (args.length() > 2) {
    Handle<Object> num = args.atOrUndefined(isolate, 2);
    if (!num->IsUndefined(isolate)) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, num, Object::ToInteger(isolate, num));
      start = CapRelativeIndex(num, 0, len);

      num = args.atOrUndefined(isolate, 3);
      if (!num->IsUndefined(isolate)) {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, num, Object::ToInteger(isolate, num));
        end = CapRelativeIndex(num, 0, len);
      }
    }
  }

  int64_t count = end - start;
  if (count <= 0) return *array;

  if (V8_UNLIKELY(array->WasDetached())) return *array;

  // Ensure processed indexes are within array bounds
  DCHECK_GE(start, 0);
  DCHECK_LT(start, len);
  DCHECK_GE(end, 0);
  DCHECK_LE(end, len);
  DCHECK_LE(count, len);

  return ElementsAccessor::ForKind(kind)->Fill(array, obj_value, start, end);
}

BUILTIN(TypedArrayPrototypeIncludes) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.includes";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  if (args.length() < 2) return ReadOnlyRoots(isolate).false_value();

  int64_t len = array->length();
  if (len == 0) return ReadOnlyRoots(isolate).false_value();

  int64_t index = 0;
  if (args.length() > 2) {
    Handle<Object> num;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num, Object::ToInteger(isolate, args.at<Object>(2)));
    index = CapRelativeIndex(num, 0, len);
  }

  // TODO(cwhan.tunz): throw. See the above comment in CopyWithin.
  if (V8_UNLIKELY(array->WasDetached()))
    return ReadOnlyRoots(isolate).false_value();

  Handle<Object> search_element = args.atOrUndefined(isolate, 1);
  ElementsAccessor* elements = array->GetElementsAccessor();
  Maybe<bool> result =
      elements->IncludesValue(isolate, array, search_element, index, len);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

BUILTIN(TypedArrayPrototypeIndexOf) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.indexOf";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  int64_t len = array->length();
  if (len == 0) return Smi::FromInt(-1);

  int64_t index = 0;
  if (args.length() > 2) {
    Handle<Object> num;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num, Object::ToInteger(isolate, args.at<Object>(2)));
    index = CapRelativeIndex(num, 0, len);
  }

  // TODO(cwhan.tunz): throw. See the above comment in CopyWithin.
  if (V8_UNLIKELY(array->WasDetached())) return Smi::FromInt(-1);

  Handle<Object> search_element = args.atOrUndefined(isolate, 1);
  ElementsAccessor* elements = array->GetElementsAccessor();
  Maybe<int64_t> result =
      elements->IndexOfValue(isolate, array, search_element, index, len);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->NewNumberFromInt64(result.FromJust());
}

BUILTIN(TypedArrayPrototypeLastIndexOf) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.lastIndexOf";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  int64_t len = array->length();
  if (len == 0) return Smi::FromInt(-1);

  int64_t index = len - 1;
  if (args.length() > 2) {
    Handle<Object> num;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num, Object::ToInteger(isolate, args.at<Object>(2)));
    // Set a negative value (-1) for returning -1 if num is negative and
    // len + num is still negative. Upper bound is len - 1.
    index = std::min<int64_t>(CapRelativeIndex(num, -1, len), len - 1);
  }

  if (index < 0) return Smi::FromInt(-1);

  // TODO(cwhan.tunz): throw. See the above comment in CopyWithin.
  if (V8_UNLIKELY(array->WasDetached())) return Smi::FromInt(-1);

  Handle<Object> search_element = args.atOrUndefined(isolate, 1);
  ElementsAccessor* elements = array->GetElementsAccessor();
  Maybe<int64_t> result =
      elements->LastIndexOfValue(array, search_element, index);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->NewNumberFromInt64(result.FromJust());
}

BUILTIN(TypedArrayPrototypeReverse) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.reverse";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  ElementsAccessor* elements = array->GetElementsAccessor();
  elements->Reverse(*array);
  return *array;
}

}  // namespace internal
}  // namespace v8
