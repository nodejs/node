// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/counters.h"
#include "src/elements.h"
#include "src/objects-inl.h"

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
  int64_t relative;
  if (V8_LIKELY(num->IsSmi())) {
    relative = Smi::cast(*num)->value();
  } else {
    DCHECK(num->IsHeapNumber());
    double fp = HeapNumber::cast(*num)->value();
    if (V8_UNLIKELY(!std::isfinite(fp))) {
      // +Infinity / -Infinity
      DCHECK(!std::isnan(fp));
      return fp < 0 ? minimum : maximum;
    }
    relative = static_cast<int64_t>(fp);
  }
  return relative < 0 ? std::max<int64_t>(relative + maximum, minimum)
                      : std::min<int64_t>(relative, maximum);
}

// ES7 section 22.2.4.6 TypedArrayCreate ( constructor, argumentList )
MaybeHandle<JSTypedArray> TypedArrayCreate(Isolate* isolate,
                                           Handle<JSFunction> default_ctor,
                                           int argc, Handle<Object>* argv,
                                           const char* method_name) {
  // 1. Let newTypedArray be ? Construct(constructor, argumentList).
  Handle<Object> new_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, new_obj, Execution::New(default_ctor, argc, argv), JSTypedArray);

  // 2. Perform ? ValidateTypedArray(newTypedArray).
  Handle<JSTypedArray> new_array;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, new_array, JSTypedArray::Validate(isolate, new_obj, method_name),
      JSTypedArray);

  // 3. If argumentList is a List of a single Number, then
  // If newTypedArray.[[ArrayLength]] < size, throw a TypeError exception.
  DCHECK_IMPLIES(argc == 1, argv[0]->IsSmi());
  if (argc == 1 && new_array->length_value() < argv[0]->Number()) {
    const MessageTemplate::Template message =
        MessageTemplate::kTypedArrayTooShort;
    THROW_NEW_ERROR(isolate, NewTypeError(message), JSTypedArray);
  }

  // 4. Return newTypedArray.
  return new_array;
}

// ES7 section 22.2.4.7 TypedArraySpeciesCreate ( exemplar, argumentList )
MaybeHandle<JSTypedArray> TypedArraySpeciesCreate(Isolate* isolate,
                                                  Handle<JSTypedArray> exemplar,
                                                  int argc,
                                                  Handle<Object>* argv,
                                                  const char* method_name) {
  // 1. Assert: exemplar is an Object that has a [[TypedArrayName]] internal
  // slot.
  DCHECK(exemplar->IsJSTypedArray());

  // 2. Let defaultConstructor be the intrinsic object listed in column one of
  // Table 51 for exemplar.[[TypedArrayName]].
  Handle<JSFunction> default_ctor = isolate->uint8_array_fun();
  switch (exemplar->type()) {
#define TYPED_ARRAY_CTOR(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array: {                        \
    default_ctor = isolate->type##_array_fun();         \
    break;                                              \
  }

    TYPED_ARRAYS(TYPED_ARRAY_CTOR)
#undef TYPED_ARRAY_CTOR
    default:
      UNREACHABLE();
  }

  // 3. Let constructor be ? SpeciesConstructor(exemplar, defaultConstructor).
  Handle<Object> ctor;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ctor,
      Object::SpeciesConstructor(isolate, exemplar, default_ctor),
      JSTypedArray);

  // 4. Return ? TypedArrayCreate(constructor, argumentList).
  return TypedArrayCreate(isolate, Handle<JSFunction>::cast(ctor), argc, argv,
                          method_name);
}

MaybeHandle<JSTypedArray> TypedArraySpeciesCreateByLength(
    Isolate* isolate, Handle<JSTypedArray> exemplar, const char* method_name,
    int64_t length) {
  const int argc = 1;
  ScopedVector<Handle<Object>> argv(argc);
  argv[0] = isolate->factory()->NewNumberFromInt64(length);
  return TypedArraySpeciesCreate(isolate, exemplar, argc, argv.start(),
                                 method_name);
}

}  // namespace

BUILTIN(TypedArrayPrototypeCopyWithin) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.copyWithin";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  int64_t len = array->length_value();
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
  if (V8_UNLIKELY(array->WasNeutered())) return *array;

  // Ensure processed indexes are within array bounds
  DCHECK_GE(from, 0);
  DCHECK_LT(from, len);
  DCHECK_GE(to, 0);
  DCHECK_LT(to, len);
  DCHECK_GE(len - count, 0);

  Handle<FixedTypedArrayBase> elements(
      FixedTypedArrayBase::cast(array->elements()));
  size_t element_size = array->element_size();
  to = to * element_size;
  from = from * element_size;
  count = count * element_size;

  uint8_t* data = static_cast<uint8_t*>(elements->DataPtr());
  std::memmove(data + to, data + from, count);

  return *array;
}

BUILTIN(TypedArrayPrototypeFill) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.fill";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  Handle<Object> obj_value = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, obj_value, Object::ToNumber(obj_value));

  int64_t len = array->length_value();
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

  if (V8_UNLIKELY(array->WasNeutered())) return *array;

  // Ensure processed indexes are within array bounds
  DCHECK_GE(start, 0);
  DCHECK_LT(start, len);
  DCHECK_GE(end, 0);
  DCHECK_LE(end, len);
  DCHECK_LE(count, len);

  return array->GetElementsAccessor()->Fill(isolate, array, obj_value,
                                            static_cast<uint32_t>(start),
                                            static_cast<uint32_t>(end));
}

BUILTIN(TypedArrayPrototypeIncludes) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.includes";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  if (args.length() < 2) return isolate->heap()->false_value();

  int64_t len = array->length_value();
  if (len == 0) return isolate->heap()->false_value();

  int64_t index = 0;
  if (args.length() > 2) {
    Handle<Object> num;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num, Object::ToInteger(isolate, args.at<Object>(2)));
    index = CapRelativeIndex(num, 0, len);
  }

  // TODO(cwhan.tunz): throw. See the above comment in CopyWithin.
  if (V8_UNLIKELY(array->WasNeutered())) return isolate->heap()->false_value();

  Handle<Object> search_element = args.atOrUndefined(isolate, 1);
  ElementsAccessor* elements = array->GetElementsAccessor();
  Maybe<bool> result = elements->IncludesValue(isolate, array, search_element,
                                               static_cast<uint32_t>(index),
                                               static_cast<uint32_t>(len));
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

BUILTIN(TypedArrayPrototypeIndexOf) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.indexOf";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  int64_t len = array->length_value();
  if (len == 0) return Smi::FromInt(-1);

  int64_t index = 0;
  if (args.length() > 2) {
    Handle<Object> num;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num, Object::ToInteger(isolate, args.at<Object>(2)));
    index = CapRelativeIndex(num, 0, len);
  }

  // TODO(cwhan.tunz): throw. See the above comment in CopyWithin.
  if (V8_UNLIKELY(array->WasNeutered())) return Smi::FromInt(-1);

  Handle<Object> search_element = args.atOrUndefined(isolate, 1);
  ElementsAccessor* elements = array->GetElementsAccessor();
  Maybe<int64_t> result = elements->IndexOfValue(isolate, array, search_element,
                                                 static_cast<uint32_t>(index),
                                                 static_cast<uint32_t>(len));
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->NewNumberFromInt64(result.FromJust());
}

BUILTIN(TypedArrayPrototypeLastIndexOf) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.lastIndexOf";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  int64_t len = array->length_value();
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
  if (V8_UNLIKELY(array->WasNeutered())) return Smi::FromInt(-1);

  Handle<Object> search_element = args.atOrUndefined(isolate, 1);
  ElementsAccessor* elements = array->GetElementsAccessor();
  Maybe<int64_t> result = elements->LastIndexOfValue(
      isolate, array, search_element, static_cast<uint32_t>(index));
  MAYBE_RETURN(result, isolate->heap()->exception());
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

BUILTIN(TypedArrayPrototypeSlice) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.slice";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  int64_t len = array->length_value();
  int64_t start = 0;
  int64_t end = len;
  {
    Handle<Object> num = args.atOrUndefined(isolate, 1);
    if (!num->IsUndefined(isolate)) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, num,
                                         Object::ToInteger(isolate, num));
      start = CapRelativeIndex(num, 0, len);

      num = args.atOrUndefined(isolate, 2);
      if (!num->IsUndefined(isolate)) {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, num,
                                           Object::ToInteger(isolate, num));
        end = CapRelativeIndex(num, 0, len);
      }
    }
  }

  int64_t count = std::max<int64_t>(end - start, 0);

  Handle<JSTypedArray> result_array;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result_array,
      TypedArraySpeciesCreateByLength(isolate, array, method, count));

  // TODO(cwhan.tunz): neutering check of the result_array should be done in
  // TypedArraySpeciesCreate, but currently ValidateTypedArray does not throw
  // for neutered buffer, so this is a temporary neutering check for the result
  // array
  if (V8_UNLIKELY(result_array->WasNeutered())) return *result_array;

  // TODO(cwhan.tunz): should throw.
  if (V8_UNLIKELY(array->WasNeutered())) return *result_array;

  if (count == 0) return *result_array;

  ElementsAccessor* accessor = array->GetElementsAccessor();
  return *accessor->Slice(array, static_cast<uint32_t>(start),
                          static_cast<uint32_t>(end), result_array);
}

}  // namespace internal
}  // namespace v8
