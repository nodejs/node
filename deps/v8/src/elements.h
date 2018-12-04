// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ELEMENTS_H_
#define V8_ELEMENTS_H_

#include "src/elements-kind.h"
#include "src/keys.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class JSTypedArray;

// Abstract base class for handles that can operate on objects with differing
// ElementsKinds.
class ElementsAccessor {
 public:
  explicit ElementsAccessor(const char* name) : name_(name) { }
  virtual ~ElementsAccessor() = default;

  const char* name() const { return name_; }

  // Returns a shared ElementsAccessor for the specified ElementsKind.
  static ElementsAccessor* ForKind(ElementsKind elements_kind) {
    DCHECK_LT(static_cast<int>(elements_kind), kElementsKindCount);
    return elements_accessors_[elements_kind];
  }

  // Checks the elements of an object for consistency, asserting when a problem
  // is found.
  virtual void Validate(JSObject* obj) = 0;

  // Returns true if a holder contains an element with the specified index
  // without iterating up the prototype chain.  The caller can optionally pass
  // in the backing store to use for the check, which must be compatible with
  // the ElementsKind of the ElementsAccessor. If backing_store is nullptr, the
  // holder->elements() is used as the backing store. If a |filter| is
  // specified the PropertyAttributes of the element at the given index
  // are compared to the given |filter|. If they match/overlap the given
  // index is ignored. Note that only Dictionary elements have custom
  // PropertyAttributes associated, hence the |filter| argument is ignored for
  // all but DICTIONARY_ELEMENTS and SLOW_SLOPPY_ARGUMENTS_ELEMENTS.
  virtual bool HasElement(JSObject* holder, uint32_t index,
                          FixedArrayBase* backing_store,
                          PropertyFilter filter = ALL_PROPERTIES) = 0;

  inline bool HasElement(JSObject* holder, uint32_t index,
                         PropertyFilter filter = ALL_PROPERTIES);

  // Note: this is currently not implemented for string wrapper and
  // typed array elements.
  virtual bool HasEntry(JSObject* holder, uint32_t entry) = 0;

  // TODO(cbruni): HasEntry and Get should not be exposed publicly with the
  // entry parameter.
  virtual Handle<Object> Get(Handle<JSObject> holder, uint32_t entry) = 0;

  virtual bool HasAccessors(JSObject* holder) = 0;
  virtual uint32_t NumberOfElements(JSObject* holder) = 0;

  // Modifies the length data property as specified for JSArrays and resizes the
  // underlying backing store accordingly. The method honors the semantics of
  // changing array sizes as defined in EcmaScript 5.1 15.4.5.2, i.e. array that
  // have non-deletable elements can only be shrunk to the size of highest
  // element that is non-deletable.
  virtual void SetLength(Handle<JSArray> holder, uint32_t new_length) = 0;


  // If kCopyToEnd is specified as the copy_size to CopyElements, it copies all
  // of elements from source after source_start to the destination array.
  static const int kCopyToEnd = -1;
  // If kCopyToEndAndInitializeToHole is specified as the copy_size to
  // CopyElements, it copies all of elements from source after source_start to
  // destination array, padding any remaining uninitialized elements in the
  // destination array with the hole.
  static const int kCopyToEndAndInitializeToHole = -2;

  // Copy all indices that have elements from |object| into the given
  // KeyAccumulator. For Dictionary-based element-kinds we filter out elements
  // whose PropertyAttribute match |filter|.
  virtual void CollectElementIndices(Handle<JSObject> object,
                                     Handle<FixedArrayBase> backing_store,
                                     KeyAccumulator* keys) = 0;

  inline void CollectElementIndices(Handle<JSObject> object,
                                    KeyAccumulator* keys);

  virtual Maybe<bool> CollectValuesOrEntries(
      Isolate* isolate, Handle<JSObject> object,
      Handle<FixedArray> values_or_entries, bool get_entries, int* nof_items,
      PropertyFilter filter = ALL_PROPERTIES) = 0;

  virtual MaybeHandle<FixedArray> PrependElementIndices(
      Handle<JSObject> object, Handle<FixedArrayBase> backing_store,
      Handle<FixedArray> keys, GetKeysConversion convert,
      PropertyFilter filter = ALL_PROPERTIES) = 0;

  inline MaybeHandle<FixedArray> PrependElementIndices(
      Handle<JSObject> object, Handle<FixedArray> keys,
      GetKeysConversion convert, PropertyFilter filter = ALL_PROPERTIES);

  virtual void AddElementsToKeyAccumulator(Handle<JSObject> receiver,
                                           KeyAccumulator* accumulator,
                                           AddKeyConversion convert) = 0;

  virtual void TransitionElementsKind(Handle<JSObject> object,
                                      Handle<Map> map) = 0;
  virtual void GrowCapacityAndConvert(Handle<JSObject> object,
                                      uint32_t capacity) = 0;
  // Unlike GrowCapacityAndConvert do not attempt to convert the backing store
  // and simply return false in this case.
  virtual bool GrowCapacity(Handle<JSObject> object, uint32_t index) = 0;

  static void InitializeOncePerProcess();
  static void TearDown();

  virtual void Set(Handle<JSObject> holder, uint32_t entry, Object* value) = 0;

  virtual void Add(Handle<JSObject> object, uint32_t index,
                   Handle<Object> value, PropertyAttributes attributes,
                   uint32_t new_capacity) = 0;

  static Handle<JSArray> Concat(Isolate* isolate, Arguments* args,
                                uint32_t concat_size, uint32_t result_length);

  virtual uint32_t Push(Handle<JSArray> receiver, Arguments* args,
                        uint32_t push_size) = 0;

  virtual uint32_t Unshift(Handle<JSArray> receiver, Arguments* args,
                           uint32_t unshift_size) = 0;

  virtual Handle<JSObject> Slice(Handle<JSObject> receiver, uint32_t start,
                                 uint32_t end) = 0;

  virtual Handle<Object> Pop(Handle<JSArray> receiver) = 0;

  virtual Handle<Object> Shift(Handle<JSArray> receiver) = 0;

  virtual Handle<NumberDictionary> Normalize(Handle<JSObject> object) = 0;

  virtual uint32_t GetCapacity(JSObject* holder,
                               FixedArrayBase* backing_store) = 0;

  virtual Object* Fill(Handle<JSObject> receiver, Handle<Object> obj_value,
                       uint32_t start, uint32_t end) = 0;

  // Check an Object's own elements for an element (using SameValueZero
  // semantics)
  virtual Maybe<bool> IncludesValue(Isolate* isolate, Handle<JSObject> receiver,
                                    Handle<Object> value, uint32_t start,
                                    uint32_t length) = 0;

  // Check an Object's own elements for the index of an element (using SameValue
  // semantics)
  virtual Maybe<int64_t> IndexOfValue(Isolate* isolate,
                                      Handle<JSObject> receiver,
                                      Handle<Object> value, uint32_t start,
                                      uint32_t length) = 0;

  virtual Maybe<int64_t> LastIndexOfValue(Handle<JSObject> receiver,
                                          Handle<Object> value,
                                          uint32_t start) = 0;

  virtual void Reverse(JSObject* receiver) = 0;

  virtual void CopyElements(Isolate* isolate, Handle<FixedArrayBase> source,
                            ElementsKind source_kind,
                            Handle<FixedArrayBase> destination, int size) = 0;

  virtual Object* CopyElements(Handle<Object> source,
                               Handle<JSObject> destination, size_t length,
                               uint32_t offset = 0) = 0;

  virtual Handle<FixedArray> CreateListFromArrayLike(Isolate* isolate,
                                                     Handle<JSObject> object,
                                                     uint32_t length) = 0;

  virtual void CopyTypedArrayElementsSlice(JSTypedArray* source,
                                           JSTypedArray* destination,
                                           size_t start, size_t end) = 0;

 protected:
  friend class LookupIterator;

  // Element handlers distinguish between entries and indices when they
  // manipulate elements. Entries refer to elements in terms of their location
  // in the underlying storage's backing store representation, and are between 0
  // and GetCapacity. Indices refer to elements in terms of the value that would
  // be specified in JavaScript to access the element. In most implementations,
  // indices are equivalent to entries. In the NumberDictionary
  // ElementsAccessor, entries are mapped to an index using the KeyAt method on
  // the NumberDictionary.
  virtual uint32_t GetEntryForIndex(Isolate* isolate, JSObject* holder,
                                    FixedArrayBase* backing_store,
                                    uint32_t index) = 0;

  virtual PropertyDetails GetDetails(JSObject* holder, uint32_t entry) = 0;
  virtual void Reconfigure(Handle<JSObject> object,
                           Handle<FixedArrayBase> backing_store, uint32_t entry,
                           Handle<Object> value,
                           PropertyAttributes attributes) = 0;

  // Deletes an element in an object.
  virtual void Delete(Handle<JSObject> holder, uint32_t entry) = 0;

  // NOTE: this method violates the handlified function signature convention:
  // raw pointer parameter |source_holder| in the function that allocates.
  // This is done intentionally to avoid ArrayConcat() builtin performance
  // degradation.
  virtual void CopyElements(JSObject* source_holder, uint32_t source_start,
                            ElementsKind source_kind,
                            Handle<FixedArrayBase> destination,
                            uint32_t destination_start, int copy_size) = 0;

 private:
  static ElementsAccessor** elements_accessors_;
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(ElementsAccessor);
};

void CheckArrayAbuse(Handle<JSObject> obj, const char* op, uint32_t index,
                     bool allow_appending = false);

V8_WARN_UNUSED_RESULT MaybeHandle<Object> ArrayConstructInitializeElements(
    Handle<JSArray> array, Arguments* args);

// Called directly from CSA.
void CopyFastNumberJSArrayElementsToTypedArray(Context* context,
                                               JSArray* source,
                                               JSTypedArray* destination,
                                               uintptr_t length,
                                               uintptr_t offset);
void CopyTypedArrayElementsToTypedArray(JSTypedArray* source,
                                        JSTypedArray* destination,
                                        uintptr_t length, uintptr_t offset);
void CopyTypedArrayElementsSlice(JSTypedArray* source,
                                 JSTypedArray* destination, uintptr_t start,
                                 uintptr_t end);

}  // namespace internal
}  // namespace v8

#endif  // V8_ELEMENTS_H_
