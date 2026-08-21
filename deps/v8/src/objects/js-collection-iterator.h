// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLECTION_ITERATOR_H_
#define V8_OBJECTS_JS_COLLECTION_ITERATOR_H_

#include "src/common/globals.h"
#include "src/objects/js-objects.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

V8_OBJECT class JSCollectionIterator : public JSObject {
 public:
  inline Tagged<Object> table() const;
  inline void set_table(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> index() const;
  inline void set_index(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  void JSCollectionIteratorPrint(std::ostream& os, const char* name);
  V8_EXPORT_PRIVATE void JSCollectionIteratorVerify(Isolate* isolate);

  // Back-compat offset/size constants.
  static const int kTableOffset;
  static const int kIndexOffset;
  static const int kHeaderSize;

 public:
  TaggedMember<Object> table_;
  TaggedMember<Object> index_;
} V8_OBJECT_END;

inline constexpr int JSCollectionIterator::kTableOffset =
    offsetof(JSCollectionIterator, table_);
inline constexpr int JSCollectionIterator::kIndexOffset =
    offsetof(JSCollectionIterator, index_);
inline constexpr int JSCollectionIterator::kHeaderSize =
    sizeof(JSCollectionIterator);

// OrderedHashTableIterator is an iterator that iterates over the keys and
// values of an OrderedHashTable.
//
// The iterator has a reference to the underlying OrderedHashTable data,
// [table], as well as the current [index] the iterator is at.
//
// When the OrderedHashTable is rehashed it adds a reference from the old table
// to the new table as well as storing enough data about the changes so that the
// iterator [index] can be adjusted accordingly.
//
// When the [Next] result from the iterator is requested, the iterator checks if
// there is a newer table that it needs to transition to.
template <class Derived, class TableType>
class OrderedHashTableIterator : public JSCollectionIterator {
 public:
  // Whether the iterator has more elements. This needs to be called before
  // calling |CurrentKey| and/or |CurrentValue|.
  bool HasMore();

  // Move the index forward one.
  void MoveNext() { set_index(Smi::FromInt(Smi::ToInt(index()) + 1)); }

  // Returns the current key of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Tagged<Object> CurrentKey();

 private:
  // Transitions the iterator to the non obsolete backing store. This is a NOP
  // if the [table] is not obsolete.
  void Transition();
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLECTION_ITERATOR_H_
