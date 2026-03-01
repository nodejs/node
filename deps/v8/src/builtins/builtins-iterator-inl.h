// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ITERATOR_INL_H_
#define V8_BUILTINS_BUILTINS_ITERATOR_INL_H_

#include "src/builtins/builtins-iterator.h"

#include "src/base/float16.h"
#include "src/base/memory.h"
#include "src/base/numerics/safe_conversions.h"
#include "src/execution/execution.h"
#include "src/execution/protectors-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/ordered-hash-table-inl.h"

namespace v8 {
namespace internal {

// Helper function to perform IteratorStep in C++.
// Returns the result object (JSIteratorResult), or undefined if done.
// https://tc39.es/ecma262/#sec-iteratorstep
inline MaybeDirectHandle<JSReceiver> IteratorStep(
    Isolate* isolate, DirectHandle<JSReceiver> iterator,
    DirectHandle<Object> next_method, DirectHandle<Map> iterator_result_map) {
  // While the iteration with natively supported iterators is very efficient,
  // the following call to the generic next method has unfortunately about 30%
  // overhead over the CSA version in IteratorAssembler.
  DirectHandle<Object> result;
  // 1. Let result be ? IteratorNext(iteratorRecord).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, next_method, iterator,
                      base::Vector<const DirectHandle<Object>>()));

  if (!IsJSReceiver(*result)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kIteratorResultNotAnObject, result));
  }

  DirectHandle<JSReceiver> result_obj = Cast<JSReceiver>(result);
  Tagged<Object> done;

  // 2. Let done be Completion(IteratorComplete(result)).
  if (result_obj->map() == *iterator_result_map) {
    DirectHandle<JSIteratorResult> iter_result =
        Cast<JSIteratorResult>(result_obj);
    done = iter_result->done();
  } else {
    DirectHandle<Object> done_handle;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, done_handle,
        Object::GetProperty(isolate, result_obj,
                            isolate->factory()->done_string()));
    done = *done_handle;
  }

  // 5. If done is true, then
  //    a. Set iteratorRecord.[[Done]] to true.
  //    b. Return done.
  if (Object::BooleanValue(done, isolate)) {
    return MaybeDirectHandle<JSReceiver>();
  }

  // 6. Return result.
  return result_obj;
}

// https://tc39.es/ecma262/#sec-iteratorclose
inline void IteratorClose(Isolate* isolate, DirectHandle<JSReceiver> iterator) {
  DirectHandle<Object> completion_exception;
  bool has_completion_exception = isolate->has_exception();
  if (has_completion_exception) {
    completion_exception = handle(isolate->exception(), isolate);
    isolate->clear_exception();
  }

  // 3. Let innerResult be GetMethod(iterator, "return").
  DirectHandle<Object> return_method;
  DirectHandle<Object> inner_exception;
  bool has_inner_exception = false;

  if (!Object::GetProperty(isolate, iterator,
                           isolate->factory()->return_string())
           .ToHandle(&return_method)) {
    has_inner_exception = true;
    inner_exception = handle(isolate->exception(), isolate);
    isolate->clear_exception();
  } else if (!IsNullOrUndefined(*return_method, isolate)) {
    // 4. If innerResult.[[Type]] is normal, then
    //   a. Let return be innerResult.[[Value]].
    //   b. If return is undefined, return Completion(completion).
    //   c. Set innerResult to Call(return, iterator).
    DirectHandle<Object> inner_result;
    if (!Execution::Call(isolate, return_method, iterator,
                         base::Vector<const DirectHandle<Object>>())
             .ToHandle(&inner_result)) {
      has_inner_exception = true;
      inner_exception = handle(isolate->exception(), isolate);
      isolate->clear_exception();
    } else {
      // 7. If innerResult.[[Value]] is not an Object, throw a TypeError
      // exception.
      if (!has_completion_exception && !IsJSReceiver(*inner_result)) {
        has_inner_exception = true;
        inner_exception = isolate->factory()->NewTypeError(
            MessageTemplate::kCalledOnNonObject,
            isolate->factory()->return_string());
      }
    }
  }

  // 5. If completion is a throw completion, return ? completion.
  if (has_completion_exception) {
    isolate->set_exception(*completion_exception);
  } else if (has_inner_exception) {
    // 6. If innerResult is a throw completion, return ? innerResult.
    isolate->set_exception(*inner_exception);
  }
}

template <typename IntVisitor, typename DoubleVisitor, typename GenericVisitor>
MaybeDirectHandle<Object> IterableForEach(
    Isolate* isolate, DirectHandle<Object> items, IntVisitor int_visitor,
    DoubleVisitor double_visitor, GenericVisitor generic_visitor,
    uint64_t* max_count_out, std::optional<uint64_t> max_count) {
  auto dispatch = [&](DirectHandle<Object> obj) -> bool {
    if (IsSmi(*obj)) {
      return double_visitor(static_cast<double>(Smi::ToInt(*obj)));
    } else if (IsHeapNumber(*obj)) {
      return double_visitor(Object::NumberValue(Cast<HeapNumber>(*obj)));
    } else {
      return generic_visitor(obj);
    }
  };

  // TODO(olivf): Support a smaller limit. Needs additional counting in the case
  // of holey arrays and deleted set elements.
  DCHECK_IMPLIES(max_count, std::numeric_limits<uint32_t>::max() <= *max_count);

  // Fast path for JSArray.
  // This corresponds to an optimized version of
  // CreateListIteratorRecord/IteratorNext for arrays where we know the length
  // and elements layout.
  if (IsJSArray(*items) &&
      Protectors::IsArrayIteratorLookupChainIntact(isolate)) {
    DirectHandle<JSArray> array = Cast<JSArray>(items);
    ElementsKind kind = array->GetElementsKind();
    // TODO(olivf): Move this as a helper into ElementsAccessor.
    // Holey arrays are only supported if there are no properties on the lookup
    // chain. This is approximated by checking for the array prototype and
    // ensuring it has not elements.
    if (IsFastElementsKind(kind) && (!IsHoleyElementsKind(kind) ||
                                     (Protectors::IsNoElementsIntact(isolate) &&
                                      array->HasArrayPrototype(isolate)))) {
      DirectHandle<FixedArrayBase> elements(array->elements(), isolate);
      uint32_t len;
      if (Object::ToUint32(array->length(), &len)) {
        DCHECK_IMPLIES(max_count, len < *max_count);
        if (max_count_out) {
          *max_count_out = len;
        }
        if (len == 0) {
          return isolate->root_handle(RootIndex::kUndefinedValue);
        }
        const MaybeDirectHandle<Object> abort;
        if (kind == PACKED_SMI_ELEMENTS) {
          DirectHandle<FixedArray> smi_elements = Cast<FixedArray>(elements);
          for (uint32_t i = 0; i < len; ++i) {
            Tagged<Object> obj = smi_elements->get(i);
            if (!int_visitor(Smi::ToInt(obj))) return abort;
          }
        } else if (kind == HOLEY_SMI_ELEMENTS) {
          DirectHandle<FixedArray> smi_elements = Cast<FixedArray>(elements);
          for (uint32_t i = 0; i < len; ++i) {
            Tagged<Object> obj = smi_elements->get(i);
            if (IsTheHole(obj, isolate)) {
              if (!generic_visitor(
                      isolate->root_handle(RootIndex::kUndefinedValue))) {
                return abort;
              }
            } else {
              if (!int_visitor(Smi::ToInt(obj))) return abort;
            }
          }
        } else if (kind == PACKED_DOUBLE_ELEMENTS) {
          DirectHandle<FixedDoubleArray> double_elements =
              Cast<FixedDoubleArray>(elements);
          for (uint32_t i = 0; i < len; ++i) {
            if (!double_visitor(double_elements->get_scalar(i))) return abort;
          }
        } else if (kind == HOLEY_DOUBLE_ELEMENTS) {
          DirectHandle<FixedDoubleArray> double_elements =
              Cast<FixedDoubleArray>(elements);
          for (uint32_t i = 0; i < len; ++i) {
            if (double_elements->is_the_hole(i) ||
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
                double_elements->is_undefined(i)
#else
                false
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
            ) {
              if (!generic_visitor(
                      isolate->root_handle(RootIndex::kUndefinedValue))) {
                return abort;
              }
            } else {
              if (!double_visitor(double_elements->get_scalar(i))) return abort;
            }
          }
        } else {
          DirectHandle<FixedArray> fast_elements = Cast<FixedArray>(elements);
          DirectHandle<Object> obj_handle(Smi::zero(), isolate);
          for (uint32_t i = 0; i < len; ++i) {
            Tagged<Object> obj = fast_elements->get(i);
            if (IsTheHole(obj, isolate)) {
              if (!generic_visitor(
                      isolate->root_handle(RootIndex::kUndefinedValue))) {
                return abort;
              }
            } else {
              obj_handle.SetValue(obj);
              if (!dispatch(obj_handle)) return abort;
            }
          }
        }
        return isolate->root_handle(RootIndex::kUndefinedValue);
      }
    }
  }

  // Slow path: Iterator protocol.
  DirectHandle<JSReceiver> receiver;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, receiver,
                             Object::ToObject(isolate, items));

  // https://tc39.es/ecma262/#sec-getiterator
  DirectHandle<Object> iterator_fn;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, iterator_fn,
      Object::GetMethod(isolate, receiver,
                        isolate->factory()->iterator_symbol()));

  // Fast path for JSTypedArray.
  if (IsJSTypedArray(*receiver) &&
      Protectors::IsArrayIteratorLookupChainIntact(isolate) &&
      IsJSFunction(*iterator_fn)) {
    if (DirectHandle<JSFunction> func = Cast<JSFunction>(iterator_fn);
        isolate->builtins()
            ->code(Builtin::kTypedArrayPrototypeValues)
            ->SafeEquals(func->code(isolate))) {
      DirectHandle<JSTypedArray> typed_array = Cast<JSTypedArray>(receiver);
      ElementsKind kind = typed_array->GetElementsKind();
      bool out_of_bounds = false;
      size_t len = typed_array->GetLengthOrOutOfBounds(out_of_bounds);
      if (out_of_bounds || typed_array->WasDetached()) {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kTypedArrayDetachedErrorOperation,
            isolate->factory()->NewStringFromAsciiChecked("iteration")));
        return MaybeDirectHandle<Object>();
      }
      if (max_count) {
        if (len > *max_count) len = *max_count;
      }
      if (max_count_out) *max_count_out = len;

      switch (kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype)                              \
  case TYPE##_ELEMENTS:                                                        \
  case RAB_GSAB_##TYPE##_ELEMENTS: {                                           \
    /* Skip BigInt's here as they often need to be handled differently. */     \
    if constexpr (!std::is_same_v<int64_t, ctype> &&                           \
                  !std::is_same_v<uint64_t, ctype>) {                          \
      CHECK(!IsBigIntTypedArrayElementsKind(kind));                            \
      for (size_t i = 0; i < len; ++i) {                                       \
        if (typed_array->WasDetached()) {                                      \
          isolate->Throw(*isolate->factory()->NewTypeError(                    \
              MessageTemplate::kTypedArrayDetachedErrorOperation,              \
              isolate->factory()->NewStringFromAsciiChecked("iteration")));    \
          return MaybeDirectHandle<Object>();                                  \
        }                                                                      \
        ctype val = base::ReadUnalignedValue<ctype>(                           \
            reinterpret_cast<Address>(typed_array->DataPtr()) +                \
            i * sizeof(ctype));                                                \
        if constexpr (ElementsKind::TYPE##_ELEMENTS == FLOAT16_ELEMENTS) {     \
          if (!double_visitor(fp16_ieee_to_fp32_value(val))) {                 \
            return MaybeDirectHandle<Object>();                                \
          }                                                                    \
        } else if constexpr (std::is_integral_v<ctype> &&                      \
                             base::kIsTypeInRangeForNumericType<int, ctype>) { \
          auto cast = [](auto v) { return base::strict_cast<int>(v); };        \
          if (!int_visitor(cast(val))) {                                       \
            return MaybeDirectHandle<Object>();                                \
          }                                                                    \
        } else {                                                               \
          static_assert(base::kIsTypeInRangeForNumericType<double, ctype>);    \
          auto cast = [](auto v) { return base::strict_cast<double>(v); };     \
          if (!double_visitor(cast(val))) {                                    \
            return MaybeDirectHandle<Object>();                                \
          }                                                                    \
        }                                                                      \
      }                                                                        \
      return isolate->root_handle(RootIndex::kUndefinedValue);                 \
    }                                                                          \
    break;                                                                     \
  }
        TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
        default:
          break;
      }
    }
  }
  if (IsUndefined(*iterator_fn, isolate)) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kNotIterable, receiver));
  }

  DirectHandle<Object> iterator_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, iterator_obj,
      Execution::Call(isolate, iterator_fn, receiver,
                      base::Vector<const DirectHandle<Object>>()));

  if (!IsJSReceiver(*iterator_obj)) {
    THROW_NEW_ERROR(
        isolate, NewTypeError(MessageTemplate::kNotAnIterator, iterator_obj));
  }
  DirectHandle<JSReceiver> iterator = Cast<JSReceiver>(iterator_obj);

  // Medium fast path for JSArrayIterator.
  if (IsJSArrayIterator(*iterator) &&
      Protectors::IsArrayIteratorLookupChainIntact(isolate) &&
      Protectors::IsNoElementsIntact(isolate)) {
    DirectHandle<JSArrayIterator> array_iterator =
        Cast<JSArrayIterator>(iterator);
    if (array_iterator->kind() == IterationKind::kValues) {
      DirectHandle<Object> iterated_object(array_iterator->iterated_object(),
                                           isolate);
      if (IsJSArray(*iterated_object)) {
        DirectHandle<JSArray> array = Cast<JSArray>(iterated_object);
        ElementsKind kind = array->GetElementsKind();
        // Holey arrays are only supported if there are no properties on the
        // lookup chain.
        if (IsFastElementsKind(kind) &&
            (!IsHoleyElementsKind(kind) ||
             (Protectors::IsNoElementsIntact(isolate) &&
              array->HasArrayPrototype(isolate)))) {
          DirectHandle<FixedArrayBase> elements =
              handle(array->elements(), isolate);
          uint32_t len;
          uint32_t current_index;
          if (Object::ToUint32(array->length(), &len) &&
              Object::ToUint32(array_iterator->next_index(), &current_index)) {
            DCHECK_IMPLIES(max_count, len < *max_count);
            if (max_count_out) *max_count_out = len;
            if (len == 0) {
              return isolate->root_handle(RootIndex::kUndefinedValue);
            }
            DirectHandle<Object> obj_handle(Smi::zero(), isolate);
            for (; current_index < len; ++current_index) {
              if (kind == PACKED_SMI_ELEMENTS) {
                DirectHandle<FixedArray> smi_elements =
                    Cast<FixedArray>(elements);
                Tagged<Object> obj = smi_elements->get(current_index);
                if (!int_visitor(Smi::ToInt(obj))) break;
              } else if (kind == HOLEY_SMI_ELEMENTS) {
                DirectHandle<FixedArray> smi_elements =
                    Cast<FixedArray>(elements);
                Tagged<Object> obj = smi_elements->get(current_index);
                if (IsTheHole(obj, isolate)) {
                  if (!generic_visitor(
                          isolate->root_handle(RootIndex::kUndefinedValue))) {
                    break;
                  }
                } else {
                  if (!int_visitor(Smi::ToInt(obj))) break;
                }
              } else if (kind == PACKED_DOUBLE_ELEMENTS) {
                DirectHandle<FixedDoubleArray> double_elements =
                    Cast<FixedDoubleArray>(elements);
                if (!double_visitor(
                        double_elements->get_scalar(current_index))) {
                  break;
                }
              } else if (kind == HOLEY_DOUBLE_ELEMENTS) {
                DirectHandle<FixedDoubleArray> double_elements =
                    Cast<FixedDoubleArray>(elements);
                if (double_elements->is_the_hole(current_index) ||
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
                    double_elements->is_undefined(current_index)
#else
                    false
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
                ) {
                  if (!generic_visitor(
                          isolate->root_handle(RootIndex::kUndefinedValue))) {
                    break;
                  }
                } else {
                  if (!double_visitor(
                          double_elements->get_scalar(current_index))) {
                    break;
                  }
                }
              } else {
                DirectHandle<FixedArray> fast_elements =
                    Cast<FixedArray>(elements);
                Tagged<Object> obj = fast_elements->get(current_index);
                if (IsTheHole(obj, isolate)) {
                  if (!generic_visitor(
                          isolate->root_handle(RootIndex::kUndefinedValue))) {
                    break;
                  }
                } else {
                  obj_handle.SetValue(obj);
                  if (!dispatch(obj_handle)) break;
                }
              }
            }
            if (current_index < len) {
              auto num =
                  isolate->factory()->NewNumberFromUint(current_index + 1);
              array_iterator->set_next_index(*num);
              IteratorClose(isolate, iterator);
              return MaybeDirectHandle<Object>();
            }
            auto num = isolate->factory()->NewNumberFromUint(len);
            array_iterator->set_next_index(*num);
            return isolate->root_handle(RootIndex::kUndefinedValue);
          }
        }
      }
    }
  }

  // Medium fast path for JSSetIterator.
  if (IsJSSetIterator(*iterator) &&
      Protectors::IsSetIteratorLookupChainIntact(isolate)) {
    DirectHandle<JSSetIterator> set_iterator = Cast<JSSetIterator>(iterator);
    DirectHandle<Object> table_obj(set_iterator->table(), isolate);
    if (IsOrderedHashSet(*table_obj)) {
      DirectHandle<OrderedHashSet> table = Cast<OrderedHashSet>(table_obj);
      int capacity = table->UsedCapacity();
      DCHECK_IMPLIES(max_count,
                     static_cast<unsigned int>(capacity) < *max_count);
      if (max_count_out) *max_count_out = capacity;
      int current_index = Smi::ToInt(set_iterator->index());
      DirectHandle<Object> key_handle(Smi::zero(), isolate);
      for (; current_index < capacity; ++current_index) {
        InternalIndex entry(current_index);
        Tagged<Object> key = table->KeyAt(entry);
        if (!IsTheHole(key, isolate)) {
          key_handle.SetValue(key);
          if (!dispatch(key_handle)) break;
        }
      }
      if (current_index < capacity) {
        auto num = isolate->factory()->NewNumberFromUint(current_index + 1);
        set_iterator->set_index(*num);
        IteratorClose(isolate, iterator);
        return MaybeDirectHandle<Object>();
      }
      auto num = isolate->factory()->NewNumberFromUint(capacity);
      set_iterator->set_index(*num);
      return isolate->root_handle(RootIndex::kUndefinedValue);
    }
  }

  DirectHandle<Object> next_method;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, next_method,
      Object::GetProperty(isolate, iterator,
                          isolate->factory()->next_string()));

  uint64_t count = 0;

  DirectHandle<Map> iterator_result_map(
      isolate->native_context()->iterator_result_map(), isolate);

  // Create a new handle so we can reuse it.
  DirectHandle<Object> next_value_handle(Smi::zero(), isolate);

  while (true) {
    HandleScope loop_scope(isolate);
    MaybeDirectHandle<JSReceiver> result =
        IteratorStep(isolate, iterator, next_method, iterator_result_map);
    if (result.is_null()) {
      if (isolate->has_exception()) return MaybeDirectHandle<Object>();
      break;  // Done
    }
    DirectHandle<JSReceiver> result_obj = result.ToHandleChecked();

    if (result_obj->map() == *iterator_result_map) {
      DirectHandle<JSIteratorResult> iter_result =
          Cast<JSIteratorResult>(result_obj);
      next_value_handle.SetValue(iter_result->value());
    } else {
      // https://tc39.es/ecma262/#sec-iteratorvalue
      DirectHandle<Object> value_handle;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value_handle,
          Object::GetProperty(isolate, result_obj,
                              isolate->factory()->value_string()));
      next_value_handle.SetValue(*value_handle);
    }

    if (max_count.has_value()) {
      if (++count > *max_count) {
        if (max_count_out) *max_count_out = count;
        isolate->Throw(*isolate->factory()->NewRangeError(
            MessageTemplate::kStackOverflow));
        IteratorClose(isolate, iterator);
        return MaybeDirectHandle<Object>();
      }
    }

    if (!dispatch(next_value_handle)) {
      // 7.4.13 IfAbruptCloseIterator (value, iteratorRecord)
      // The dispatch failed (visitor threw). We must close the iterator.
      IteratorClose(isolate, iterator);
      return MaybeDirectHandle<Object>();
    }
  }
  if (max_count_out) *max_count_out = count;

  return isolate->root_handle(RootIndex::kUndefinedValue);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ITERATOR_INL_H_
