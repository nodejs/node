// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_
#define V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_

#include "src/identity-map.h"
#include "src/interpreter/bytecodes.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

class Factory;
class Isolate;

namespace interpreter {

// A helper class for constructing constant arrays for the interpreter.
class ConstantArrayBuilder final : public ZoneObject {
 public:
  // Capacity of the 8-bit operand slice.
  static const size_t kLowCapacity = 1u << kBitsPerByte;

  // Capacity of the combined 8-bit and 16-bit operand slices.
  static const size_t kMaxCapacity = 1u << (2 * kBitsPerByte);

  // Capacity of the 16-bit operand slice.
  static const size_t kHighCapacity = kMaxCapacity - kLowCapacity;

  ConstantArrayBuilder(Isolate* isolate, Zone* zone);

  // Generate a fixed array of constants based on inserted objects.
  Handle<FixedArray> ToFixedArray(Factory* factory) const;

  // Returns the object in the constant pool array that at index
  // |index|.
  Handle<Object> At(size_t index) const;

  // Returns the number of elements in the array.
  size_t size() const;

  // Insert an object into the constants array if it is not already
  // present. Returns the array index associated with the object.
  size_t Insert(Handle<Object> object);

  // Creates a reserved entry in the constant pool and returns
  // the size of the operand that'll be required to hold the entry
  // when committed.
  OperandSize CreateReservedEntry();

  // Commit reserved entry and returns the constant pool index for the
  // object.
  size_t CommitReservedEntry(OperandSize operand_size, Handle<Object> object);

  // Discards constant pool reservation.
  void DiscardReservedEntry(OperandSize operand_size);

 private:
  typedef uint16_t index_t;

  index_t AllocateEntry(Handle<Object> object);

  struct ConstantArraySlice final {
    ConstantArraySlice(Zone* zone, size_t start_index, size_t capacity);
    void Reserve();
    void Unreserve();
    size_t Allocate(Handle<Object> object);
    Handle<Object> At(size_t index) const;

    inline size_t available() const { return capacity() - reserved() - size(); }
    inline size_t reserved() const { return reserved_; }
    inline size_t capacity() const { return capacity_; }
    inline size_t size() const { return constants_.size(); }
    inline size_t start_index() const { return start_index_; }

   private:
    const size_t start_index_;
    const size_t capacity_;
    size_t reserved_;
    ZoneVector<Handle<Object>> constants_;

    DISALLOW_COPY_AND_ASSIGN(ConstantArraySlice);
  };

  Isolate* isolate_;
  ConstantArraySlice idx8_slice_;
  ConstantArraySlice idx16_slice_;
  IdentityMap<index_t> constants_map_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_
