// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "objects.h"
#include "elements.h"
#include "utils.h"

namespace v8 {
namespace internal {


ElementsAccessor** ElementsAccessor::elements_accessors_;


bool HasKey(FixedArray* array, Object* key) {
  int len0 = array->length();
  for (int i = 0; i < len0; i++) {
    Object* element = array->get(i);
    if (element->IsSmi() && element == key) return true;
    if (element->IsString() &&
        key->IsString() && String::cast(element)->Equals(String::cast(key))) {
      return true;
    }
  }
  return false;
}


// Base class for element handler implementations. Contains the
// the common logic for objects with different ElementsKinds.
// Subclasses must specialize method for which the element
// implementation differs from the base class implementation.
//
// This class is intended to be used in the following way:
//
//   class SomeElementsAccessor :
//       public ElementsAccessorBase<SomeElementsAccessor,
//                                   BackingStoreClass> {
//     ...
//   }
//
// This is an example of the Curiously Recurring Template Pattern (see
// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern).  We use
// CRTP to guarantee aggressive compile time optimizations (i.e.  inlining and
// specialization of SomeElementsAccessor methods).
template <typename ElementsAccessorSubclass, typename BackingStoreClass>
class ElementsAccessorBase : public ElementsAccessor {
 protected:
  ElementsAccessorBase() { }
  virtual MaybeObject* Get(FixedArrayBase* backing_store,
                           uint32_t key,
                           JSObject* obj,
                           Object* receiver) {
    return ElementsAccessorSubclass::GetImpl(
        BackingStoreClass::cast(backing_store), key, obj, receiver);
  }

  static MaybeObject* GetImpl(BackingStoreClass* backing_store,
                              uint32_t key,
                              JSObject* obj,
                              Object* receiver) {
    return (key < ElementsAccessorSubclass::GetCapacityImpl(backing_store))
           ? backing_store->get(key)
           : backing_store->GetHeap()->the_hole_value();
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) = 0;

  virtual MaybeObject* AddElementsToFixedArray(FixedArrayBase* from,
                                               FixedArray* to,
                                               JSObject* holder,
                                               Object* receiver) {
    int len0 = to->length();
#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      for (int i = 0; i < len0; i++) {
        ASSERT(!to->get(i)->IsTheHole());
      }
    }
#endif
    BackingStoreClass* backing_store = BackingStoreClass::cast(from);
    uint32_t len1 = ElementsAccessorSubclass::GetCapacityImpl(backing_store);

    // Optimize if 'other' is empty.
    // We cannot optimize if 'this' is empty, as other may have holes.
    if (len1 == 0) return to;

    // Compute how many elements are not in other.
    int extra = 0;
    for (uint32_t y = 0; y < len1; y++) {
      if (ElementsAccessorSubclass::HasElementAtIndexImpl(
          backing_store, y, holder, receiver)) {
        uint32_t key =
            ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, y);
        MaybeObject* maybe_value =
            ElementsAccessorSubclass::GetImpl(backing_store, key,
                                              holder, receiver);
        Object* value;
        if (!maybe_value->ToObject(&value)) return maybe_value;
        ASSERT(!value->IsTheHole());
        if (!HasKey(to, value)) {
          extra++;
        }
      }
    }

    if (extra == 0) return to;

    // Allocate the result
    FixedArray* result;
    MaybeObject* maybe_obj =
        backing_store->GetHeap()->AllocateFixedArray(len0 + extra);
    if (!maybe_obj->To<FixedArray>(&result)) return maybe_obj;

    // Fill in the content
    {
      AssertNoAllocation no_gc;
      WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
      for (int i = 0; i < len0; i++) {
        Object* e = to->get(i);
        ASSERT(e->IsString() || e->IsNumber());
        result->set(i, e, mode);
      }
    }
    // Fill in the extra values.
    int index = 0;
    for (uint32_t y = 0; y < len1; y++) {
      if (ElementsAccessorSubclass::HasElementAtIndexImpl(
          backing_store, y, holder, receiver)) {
        uint32_t key =
            ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, y);
        MaybeObject* maybe_value =
            ElementsAccessorSubclass::GetImpl(backing_store, key,
                                              holder, receiver);
        Object* value;
        if (!maybe_value->ToObject(&value)) return maybe_value;
        if (!value->IsTheHole() && !HasKey(to, value)) {
          result->set(len0 + index, value);
          index++;
        }
      }
    }
    ASSERT(extra == index);
    return result;
  }

 protected:
  static uint32_t GetCapacityImpl(BackingStoreClass* backing_store) {
    return backing_store->length();
  }

  virtual uint32_t GetCapacity(FixedArrayBase* backing_store) {
    return ElementsAccessorSubclass::GetCapacityImpl(
        BackingStoreClass::cast(backing_store));
  }

  static bool HasElementAtIndexImpl(BackingStoreClass* backing_store,
                                    uint32_t index,
                                    JSObject* holder,
                                    Object* receiver) {
    uint32_t key =
        ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, index);
    MaybeObject* element =
        ElementsAccessorSubclass::GetImpl(backing_store, key, holder, receiver);
    return !element->IsTheHole();
  }

  virtual bool HasElementAtIndex(FixedArrayBase* backing_store,
                                 uint32_t index,
                                 JSObject* holder,
                                 Object* receiver) {
    return ElementsAccessorSubclass::HasElementAtIndexImpl(
        BackingStoreClass::cast(backing_store), index, holder, receiver);
  }

  static uint32_t GetKeyForIndexImpl(BackingStoreClass* backing_store,
                                     uint32_t index) {
    return index;
  }

  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                              uint32_t index) {
    return ElementsAccessorSubclass::GetKeyForIndexImpl(
        BackingStoreClass::cast(backing_store), index);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsAccessorBase);
};


class FastElementsAccessor
    : public ElementsAccessorBase<FastElementsAccessor, FixedArray> {
 public:
  static MaybeObject* DeleteCommon(JSObject* obj,
                                   uint32_t key) {
    ASSERT(obj->HasFastElements() || obj->HasFastArgumentsElements());
    Heap* heap = obj->GetHeap();
    FixedArray* backing_store = FixedArray::cast(obj->elements());
    if (backing_store->map() == heap->non_strict_arguments_elements_map()) {
      backing_store = FixedArray::cast(backing_store->get(1));
    } else {
      Object* writable;
      MaybeObject* maybe = obj->EnsureWritableFastElements();
      if (!maybe->ToObject(&writable)) return maybe;
      backing_store = FixedArray::cast(writable);
    }
    uint32_t length = static_cast<uint32_t>(
        obj->IsJSArray()
        ? Smi::cast(JSArray::cast(obj)->length())->value()
        : backing_store->length());
    if (key < length) {
      backing_store->set_the_hole(key);
      // If an old space backing store is larger than a certain size and
      // has too few used values, normalize it.
      // To avoid doing the check on every delete we require at least
      // one adjacent hole to the value being deleted.
      Object* hole = heap->the_hole_value();
      const int kMinLengthForSparsenessCheck = 64;
      if (backing_store->length() >= kMinLengthForSparsenessCheck &&
          !heap->InNewSpace(backing_store) &&
          ((key > 0 && backing_store->get(key - 1) == hole) ||
           (key + 1 < length && backing_store->get(key + 1) == hole))) {
        int num_used = 0;
        for (int i = 0; i < backing_store->length(); ++i) {
          if (backing_store->get(i) != hole) ++num_used;
          // Bail out early if more than 1/4 is used.
          if (4 * num_used > backing_store->length()) break;
        }
        if (4 * num_used <= backing_store->length()) {
          MaybeObject* result = obj->NormalizeElements();
          if (result->IsFailure()) return result;
        }
      }
    }
    return heap->true_value();
  }

 protected:
  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    return DeleteCommon(obj, key);
  }
};


class FastDoubleElementsAccessor
    : public ElementsAccessorBase<FastDoubleElementsAccessor,
                                  FixedDoubleArray> {
 protected:
  friend class ElementsAccessorBase<FastDoubleElementsAccessor,
                                    FixedDoubleArray>;

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    int length = obj->IsJSArray()
        ? Smi::cast(JSArray::cast(obj)->length())->value()
        : FixedDoubleArray::cast(obj->elements())->length();
    if (key < static_cast<uint32_t>(length)) {
      FixedDoubleArray::cast(obj->elements())->set_the_hole(key);
    }
    return obj->GetHeap()->true_value();
  }

  static bool HasElementAtIndexImpl(FixedDoubleArray* backing_store,
                                    uint32_t index,
                                    JSObject* holder,
                                    Object* receiver) {
    return !backing_store->is_the_hole(index);
  }
};


// Super class for all external element arrays.
template<typename ExternalElementsAccessorSubclass,
         typename ExternalArray>
class ExternalElementsAccessor
    : public ElementsAccessorBase<ExternalElementsAccessorSubclass,
                                  ExternalArray> {
 protected:
  friend class ElementsAccessorBase<ExternalElementsAccessorSubclass,
                                    ExternalArray>;

  static MaybeObject* GetImpl(ExternalArray* backing_store,
                              uint32_t key,
                              JSObject* obj,
                              Object* receiver) {
    return
        key < ExternalElementsAccessorSubclass::GetCapacityImpl(backing_store)
        ? backing_store->get(key)
        : backing_store->GetHeap()->undefined_value();
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    // External arrays always ignore deletes.
    return obj->GetHeap()->true_value();
  }
};


class ExternalByteElementsAccessor
    : public ExternalElementsAccessor<ExternalByteElementsAccessor,
                                      ExternalByteArray> {
};


class ExternalUnsignedByteElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedByteElementsAccessor,
                                      ExternalUnsignedByteArray> {
};


class ExternalShortElementsAccessor
    : public ExternalElementsAccessor<ExternalShortElementsAccessor,
                                      ExternalShortArray> {
};


class ExternalUnsignedShortElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedShortElementsAccessor,
                                      ExternalUnsignedShortArray> {
};


class ExternalIntElementsAccessor
    : public ExternalElementsAccessor<ExternalIntElementsAccessor,
                                      ExternalIntArray> {
};


class ExternalUnsignedIntElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedIntElementsAccessor,
                                      ExternalUnsignedIntArray> {
};


class ExternalFloatElementsAccessor
    : public ExternalElementsAccessor<ExternalFloatElementsAccessor,
                                      ExternalFloatArray> {
};


class ExternalDoubleElementsAccessor
    : public ExternalElementsAccessor<ExternalDoubleElementsAccessor,
                                      ExternalDoubleArray> {
};


class PixelElementsAccessor
    : public ExternalElementsAccessor<PixelElementsAccessor,
                                      ExternalPixelArray> {
};


class DictionaryElementsAccessor
    : public ElementsAccessorBase<DictionaryElementsAccessor,
                                  SeededNumberDictionary> {
 public:
  static MaybeObject* DeleteCommon(JSObject* obj,
                                   uint32_t key,
                                   JSReceiver::DeleteMode mode) {
    Isolate* isolate = obj->GetIsolate();
    Heap* heap = isolate->heap();
    FixedArray* backing_store = FixedArray::cast(obj->elements());
    bool is_arguments =
        (obj->GetElementsKind() == NON_STRICT_ARGUMENTS_ELEMENTS);
    if (is_arguments) {
      backing_store = FixedArray::cast(backing_store->get(1));
    }
    SeededNumberDictionary* dictionary =
        SeededNumberDictionary::cast(backing_store);
    int entry = dictionary->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* result = dictionary->DeleteProperty(entry, mode);
      if (result == heap->true_value()) {
        MaybeObject* maybe_elements = dictionary->Shrink(key);
        FixedArray* new_elements = NULL;
        if (!maybe_elements->To(&new_elements)) {
          return maybe_elements;
        }
        if (is_arguments) {
          FixedArray::cast(obj->elements())->set(1, new_elements);
        } else {
          obj->set_elements(new_elements);
        }
      }
      if (mode == JSObject::STRICT_DELETION &&
          result == heap->false_value()) {
        // In strict mode, attempting to delete a non-configurable property
        // throws an exception.
        HandleScope scope(isolate);
        Handle<Object> holder(obj);
        Handle<Object> name = isolate->factory()->NewNumberFromUint(key);
        Handle<Object> args[2] = { name, holder };
        Handle<Object> error =
            isolate->factory()->NewTypeError("strict_delete_property",
                                             HandleVector(args, 2));
        return isolate->Throw(*error);
      }
    }
    return heap->true_value();
  }

 protected:
  friend class ElementsAccessorBase<DictionaryElementsAccessor,
                                    SeededNumberDictionary>;

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    return DeleteCommon(obj, key, mode);
  }

  static MaybeObject* GetImpl(SeededNumberDictionary* backing_store,
                              uint32_t key,
                              JSObject* obj,
                              Object* receiver) {
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* element = backing_store->ValueAt(entry);
      PropertyDetails details = backing_store->DetailsAt(entry);
      if (details.type() == CALLBACKS) {
        return obj->GetElementWithCallback(receiver,
                                           element,
                                           key,
                                           obj);
      } else {
        return element;
      }
    }
    return obj->GetHeap()->the_hole_value();
  }

  static uint32_t GetKeyForIndexImpl(SeededNumberDictionary* dict,
                                     uint32_t index) {
    Object* key = dict->KeyAt(index);
    return Smi::cast(key)->value();
  }
};


class NonStrictArgumentsElementsAccessor
    : public ElementsAccessorBase<NonStrictArgumentsElementsAccessor,
                                  FixedArray> {
 protected:
  friend class ElementsAccessorBase<NonStrictArgumentsElementsAccessor,
                                    FixedArray>;

  static MaybeObject* GetImpl(FixedArray* parameter_map,
                              uint32_t key,
                              JSObject* obj,
                              Object* receiver) {
    Object* probe = GetParameterMapArg(parameter_map, key);
    if (!probe->IsTheHole()) {
      Context* context = Context::cast(parameter_map->get(0));
      int context_index = Smi::cast(probe)->value();
      ASSERT(!context->get(context_index)->IsTheHole());
      return context->get(context_index);
    } else {
      // Object is not mapped, defer to the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      return ElementsAccessor::ForArray(arguments)->Get(arguments,
                                                        key,
                                                        obj,
                                                        receiver);
    }
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key
                              ,
                              JSReceiver::DeleteMode mode) {
    FixedArray* parameter_map = FixedArray::cast(obj->elements());
    Object* probe = GetParameterMapArg(parameter_map, key);
    if (!probe->IsTheHole()) {
      // TODO(kmillikin): We could check if this was the last aliased
      // parameter, and revert to normal elements in that case.  That
      // would enable GC of the context.
      parameter_map->set_the_hole(key + 2);
    } else {
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        return DictionaryElementsAccessor::DeleteCommon(obj, key, mode);
      } else {
        return FastElementsAccessor::DeleteCommon(obj, key);
      }
    }
    return obj->GetHeap()->true_value();
  }

  static uint32_t GetCapacityImpl(FixedArray* parameter_map) {
    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return Max(static_cast<uint32_t>(parameter_map->length() - 2),
               ForArray(arguments)->GetCapacity(arguments));
  }

  static uint32_t GetKeyForIndexImpl(FixedArray* dict,
                                     uint32_t index) {
    return index;
  }

  static bool HasElementAtIndexImpl(FixedArray* parameter_map,
                                    uint32_t index,
                                    JSObject* holder,
                                    Object* receiver) {
    Object* probe = GetParameterMapArg(parameter_map, index);
    if (!probe->IsTheHole()) {
      return true;
    } else {
      FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
      ElementsAccessor* accessor = ElementsAccessor::ForArray(arguments);
      return !accessor->Get(arguments, index, holder, receiver)->IsTheHole();
    }
  }

 private:
  static Object* GetParameterMapArg(FixedArray* parameter_map,
                                    uint32_t key) {
    uint32_t length = parameter_map->length();
    return key < (length - 2 )
        ? parameter_map->get(key + 2)
        : parameter_map->GetHeap()->the_hole_value();
  }
};


ElementsAccessor* ElementsAccessor::ForArray(FixedArrayBase* array) {
  switch (array->map()->instance_type()) {
    case FIXED_ARRAY_TYPE:
      if (array->IsDictionary()) {
        return elements_accessors_[DICTIONARY_ELEMENTS];
      } else {
        return elements_accessors_[FAST_ELEMENTS];
      }
    case EXTERNAL_BYTE_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_BYTE_ELEMENTS];
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_UNSIGNED_BYTE_ELEMENTS];
    case EXTERNAL_SHORT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_SHORT_ELEMENTS];
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_UNSIGNED_SHORT_ELEMENTS];
    case EXTERNAL_INT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_INT_ELEMENTS];
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_UNSIGNED_INT_ELEMENTS];
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_FLOAT_ELEMENTS];
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_DOUBLE_ELEMENTS];
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_PIXEL_ELEMENTS];
    default:
      UNREACHABLE();
      return NULL;
  }
}


void ElementsAccessor::InitializeOncePerProcess() {
  static struct ConcreteElementsAccessors {
    FastElementsAccessor fast_elements_handler;
    FastDoubleElementsAccessor fast_double_elements_handler;
    DictionaryElementsAccessor dictionary_elements_handler;
    NonStrictArgumentsElementsAccessor non_strict_arguments_elements_handler;
    ExternalByteElementsAccessor byte_elements_handler;
    ExternalUnsignedByteElementsAccessor unsigned_byte_elements_handler;
    ExternalShortElementsAccessor short_elements_handler;
    ExternalUnsignedShortElementsAccessor unsigned_short_elements_handler;
    ExternalIntElementsAccessor int_elements_handler;
    ExternalUnsignedIntElementsAccessor unsigned_int_elements_handler;
    ExternalFloatElementsAccessor float_elements_handler;
    ExternalDoubleElementsAccessor double_elements_handler;
    PixelElementsAccessor pixel_elements_handler;
  } element_accessors;

  static ElementsAccessor* accessor_array[] = {
    &element_accessors.fast_elements_handler,
    &element_accessors.fast_double_elements_handler,
    &element_accessors.dictionary_elements_handler,
    &element_accessors.non_strict_arguments_elements_handler,
    &element_accessors.byte_elements_handler,
    &element_accessors.unsigned_byte_elements_handler,
    &element_accessors.short_elements_handler,
    &element_accessors.unsigned_short_elements_handler,
    &element_accessors.int_elements_handler,
    &element_accessors.unsigned_int_elements_handler,
    &element_accessors.float_elements_handler,
    &element_accessors.double_elements_handler,
    &element_accessors.pixel_elements_handler
  };

  elements_accessors_ = accessor_array;
}


} }  // namespace v8::internal
