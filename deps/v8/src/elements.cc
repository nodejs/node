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
 public:
  ElementsAccessorBase() { }
  virtual MaybeObject* GetWithReceiver(JSObject* obj,
                                       Object* receiver,
                                       uint32_t index) {
    if (index < ElementsAccessorSubclass::GetLength(obj)) {
      BackingStoreClass* backing_store =
          ElementsAccessorSubclass::GetBackingStore(obj);
      return backing_store->get(index);
    }
    return obj->GetHeap()->the_hole_value();
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t index,
                              JSReceiver::DeleteMode mode) = 0;

  virtual MaybeObject* AddJSArrayKeysToFixedArray(JSArray* other,
                                                  FixedArray* keys) {
    int len0 = keys->length();
#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      for (int i = 0; i < len0; i++) {
        ASSERT(keys->get(i)->IsString() || keys->get(i)->IsNumber());
      }
    }
#endif
    int len1 = ElementsAccessorSubclass::GetCapacity(other);

    // Optimize if 'other' is empty.
    // We cannot optimize if 'this' is empty, as other may have holes
    // or non keys.
    if (len1 == 0) return keys;

    // Compute how many elements are not in other.
    int extra = 0;
    for (int y = 0; y < len1; y++) {
      Object* value;
      MaybeObject* maybe_value =
          ElementsAccessorSubclass::GetElementAtCapacityIndex(other, y);
      if (!maybe_value->ToObject(&value)) return maybe_value;
      if (!value->IsTheHole() && !HasKey(keys, value)) extra++;
    }

    if (extra == 0) return keys;

    // Allocate the result
    FixedArray* result;
    MaybeObject* maybe_obj =
        other->GetHeap()->AllocateFixedArray(len0 + extra);
    if (!maybe_obj->To<FixedArray>(&result)) return maybe_obj;

    // Fill in the content
    {
      AssertNoAllocation no_gc;
      WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
      for (int i = 0; i < len0; i++) {
        Object* e = keys->get(i);
        ASSERT(e->IsString() || e->IsNumber());
        result->set(i, e, mode);
      }
    }
    // Fill in the extra keys.
    int index = 0;
    for (int y = 0; y < len1; y++) {
      MaybeObject* maybe_value =
          ElementsAccessorSubclass::GetElementAtCapacityIndex(other, y);
      Object* value;
      if (!maybe_value->ToObject(&value)) return maybe_value;
      if (!value->IsTheHole() && !HasKey(keys, value)) {
        ASSERT(value->IsString() || value->IsNumber());
        result->set(len0 + index, value);
        index++;
      }
    }
    ASSERT(extra == index);
    return result;
  }

  static uint32_t GetCapacity(JSObject* obj) {
    return ElementsAccessorSubclass::GetBackingStore(obj)->length();
  }

  static MaybeObject* GetElementAtCapacityIndex(JSObject* obj, int index) {
    BackingStoreClass* backing_store =
        ElementsAccessorSubclass::GetBackingStore(obj);
    return backing_store->get(index);
  }

 protected:
  static BackingStoreClass* GetBackingStore(JSObject* obj) {
    return BackingStoreClass::cast(obj->elements());
  }

  static uint32_t GetLength(JSObject* obj) {
    return ElementsAccessorSubclass::GetBackingStore(obj)->length();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsAccessorBase);
};


class FastElementsAccessor
    : public ElementsAccessorBase<FastElementsAccessor, FixedArray> {
 public:
  static MaybeObject* DeleteCommon(JSObject* obj,
                                   uint32_t index) {
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
    if (index < length) {
      backing_store->set_the_hole(index);
      // If an old space backing store is larger than a certain size and
      // has too few used values, normalize it.
      // To avoid doing the check on every delete we require at least
      // one adjacent hole to the value being deleted.
      Object* hole = heap->the_hole_value();
      const int kMinLengthForSparsenessCheck = 64;
      if (backing_store->length() >= kMinLengthForSparsenessCheck &&
          !heap->InNewSpace(backing_store) &&
          ((index > 0 && backing_store->get(index - 1) == hole) ||
           (index + 1 < length && backing_store->get(index + 1) == hole))) {
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

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t index,
                              JSReceiver::DeleteMode mode) {
    return DeleteCommon(obj, index);
  }
};


class FastDoubleElementsAccessor
    : public ElementsAccessorBase<FastDoubleElementsAccessor,
                                  FixedDoubleArray> {
  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t index,
                              JSReceiver::DeleteMode mode) {
    int length = obj->IsJSArray()
        ? Smi::cast(JSArray::cast(obj)->length())->value()
        : FixedDoubleArray::cast(obj->elements())->length();
    if (index < static_cast<uint32_t>(length)) {
      FixedDoubleArray::cast(obj->elements())->set_the_hole(index);
    }
    return obj->GetHeap()->true_value();
  }
};


// Super class for all external element arrays.
template<typename ExternalElementsAccessorSubclass,
         typename ExternalArray>
class ExternalElementsAccessor
    : public ElementsAccessorBase<ExternalElementsAccessorSubclass,
                                  ExternalArray> {
 public:
  virtual MaybeObject* GetWithReceiver(JSObject* obj,
                                       Object* receiver,
                                       uint32_t index) {
    if (index < ExternalElementsAccessorSubclass::GetLength(obj)) {
      ExternalArray* backing_store =
          ExternalElementsAccessorSubclass::GetBackingStore(obj);
      return backing_store->get(index);
    } else {
      return obj->GetHeap()->undefined_value();
    }
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t index,
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
                                  NumberDictionary> {
 public:
  static MaybeObject* GetNumberDictionaryElement(
      JSObject* obj,
      Object* receiver,
      NumberDictionary* backing_store,
      uint32_t index) {
    int entry = backing_store->FindEntry(index);
    if (entry != NumberDictionary::kNotFound) {
      Object* element = backing_store->ValueAt(entry);
      PropertyDetails details = backing_store->DetailsAt(entry);
      if (details.type() == CALLBACKS) {
        return obj->GetElementWithCallback(receiver,
                                           element,
                                           index,
                                           obj);
      } else {
        return element;
      }
    }
    return obj->GetHeap()->the_hole_value();
  }


  static MaybeObject* DeleteCommon(JSObject* obj,
                                   uint32_t index,
                                   JSReceiver::DeleteMode mode) {
    Isolate* isolate = obj->GetIsolate();
    Heap* heap = isolate->heap();
    FixedArray* backing_store = FixedArray::cast(obj->elements());
    bool is_arguments =
        (obj->GetElementsKind() == JSObject::NON_STRICT_ARGUMENTS_ELEMENTS);
    if (is_arguments) {
      backing_store = FixedArray::cast(backing_store->get(1));
    }
    NumberDictionary* dictionary = NumberDictionary::cast(backing_store);
    int entry = dictionary->FindEntry(index);
    if (entry != NumberDictionary::kNotFound) {
      Object* result = dictionary->DeleteProperty(entry, mode);
      if (result == heap->true_value()) {
        MaybeObject* maybe_elements = dictionary->Shrink(index);
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
        Handle<Object> name = isolate->factory()->NewNumberFromUint(index);
        Handle<Object> args[2] = { name, holder };
        Handle<Object> error =
            isolate->factory()->NewTypeError("strict_delete_property",
                                             HandleVector(args, 2));
        return isolate->Throw(*error);
      }
    }
    return heap->true_value();
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t index,
                              JSReceiver::DeleteMode mode) {
    return DeleteCommon(obj, index, mode);
  }

  virtual MaybeObject* GetWithReceiver(JSObject* obj,
                                       Object* receiver,
                                       uint32_t index) {
    return GetNumberDictionaryElement(obj,
                                      receiver,
                                      obj->element_dictionary(),
                                      index);
  }

  static uint32_t GetCapacity(JSObject* obj) {
    return obj->element_dictionary()->Capacity();
  }

  static MaybeObject* GetElementAtCapacityIndex(JSObject* obj, int index) {
    NumberDictionary* dict = obj->element_dictionary();
    if (dict->IsKey(dict->KeyAt(index))) {
      return dict->ValueAt(index);
    } else {
      return obj->GetHeap()->the_hole_value();
    }
  }
};


class NonStrictArgumentsElementsAccessor
    : public ElementsAccessorBase<NonStrictArgumentsElementsAccessor,
                                  FixedArray> {
 public:
  virtual MaybeObject* GetWithReceiver(JSObject* obj,
                                       Object* receiver,
                                       uint32_t index) {
    FixedArray* parameter_map = GetBackingStore(obj);
    uint32_t length = parameter_map->length();
    Object* probe =
        (index < length - 2) ? parameter_map->get(index + 2) : NULL;
    if (probe != NULL && !probe->IsTheHole()) {
      Context* context = Context::cast(parameter_map->get(0));
      int context_index = Smi::cast(probe)->value();
      ASSERT(!context->get(context_index)->IsTheHole());
      return context->get(context_index);
    } else {
      // Object is not mapped, defer to the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        return DictionaryElementsAccessor::GetNumberDictionaryElement(
            obj,
            receiver,
            NumberDictionary::cast(arguments),
            index);
      } else if (index < static_cast<uint32_t>(arguments->length())) {
        return arguments->get(index);
      }
    }
    return obj->GetHeap()->the_hole_value();
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t index,
                              JSReceiver::DeleteMode mode) {
    FixedArray* parameter_map = FixedArray::cast(obj->elements());
    uint32_t length = parameter_map->length();
    Object* probe =
        index < (length - 2) ? parameter_map->get(index + 2) : NULL;
    if (probe != NULL && !probe->IsTheHole()) {
      // TODO(kmillikin): We could check if this was the last aliased
      // parameter, and revert to normal elements in that case.  That
      // would enable GC of the context.
      parameter_map->set_the_hole(index + 2);
    } else {
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        return DictionaryElementsAccessor::DeleteCommon(obj, index, mode);
      } else {
        return FastElementsAccessor::DeleteCommon(obj, index);
      }
    }
    return obj->GetHeap()->true_value();
  }

  static uint32_t GetCapacity(JSObject* obj) {
    // TODO(danno): Return max of parameter map length or backing store
    // capacity.
    return 0;
  }

  static MaybeObject* GetElementAtCapacityIndex(JSObject* obj, int index) {
    // TODO(danno): Return either value from parameter map of backing
    // store value at index.
    return obj->GetHeap()->the_hole_value();
  }
};


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
