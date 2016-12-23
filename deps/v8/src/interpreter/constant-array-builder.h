// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_
#define V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_

#include "src/identity-map.h"
#include "src/interpreter/bytecodes.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Isolate;

namespace interpreter {

// A helper class for constructing constant arrays for the
// interpreter. Each instance of this class is intended to be used to
// generate exactly one FixedArray of constants via the ToFixedArray
// method.
class ConstantArrayBuilder final BASE_EMBEDDED {
 public:
  // Capacity of the 8-bit operand slice.
  static const size_t k8BitCapacity = 1u << kBitsPerByte;

  // Capacity of the 16-bit operand slice.
  static const size_t k16BitCapacity = (1u << 2 * kBitsPerByte) - k8BitCapacity;

  // Capacity of the 32-bit operand slice.
  static const size_t k32BitCapacity =
      kMaxUInt32 - k16BitCapacity - k8BitCapacity + 1;

  ConstantArrayBuilder(Zone* zone, Handle<Object> the_hole_value);

  // Generate a fixed array of constants based on inserted objects.
  Handle<FixedArray> ToFixedArray(Isolate* isolate);

  // Returns the object in the constant pool array that at index
  // |index|.
  Handle<Object> At(size_t index) const;

  // Returns the number of elements in the array.
  size_t size() const;

  // Insert an object into the constants array if it is not already
  // present. Returns the array index associated with the object.
  size_t Insert(Handle<Object> object);

  // Allocates an empty entry and returns the array index associated with the
  // reservation. Entry can be inserted by calling InsertReservedEntry().
  size_t AllocateEntry();

  // Inserts the given object into an allocated entry.
  void InsertAllocatedEntry(size_t index, Handle<Object> object);

  // Creates a reserved entry in the constant pool and returns
  // the size of the operand that'll be required to hold the entry
  // when committed.
  OperandSize CreateReservedEntry();

  // Commit reserved entry and returns the constant pool index for the
  // SMI value.
  size_t CommitReservedEntry(OperandSize operand_size, Smi* value);

  // Discards constant pool reservation.
  void DiscardReservedEntry(OperandSize operand_size);

 private:
  typedef uint32_t index_t;

  index_t AllocateIndex(Handle<Object> object);
  index_t AllocateReservedEntry(Smi* value);

  struct ConstantArraySlice final : public ZoneObject {
    ConstantArraySlice(Zone* zone, size_t start_index, size_t capacity,
                       OperandSize operand_size);
    void Reserve();
    void Unreserve();
    size_t Allocate(Handle<Object> object);
    Handle<Object> At(size_t index) const;
    void InsertAt(size_t index, Handle<Object> object);
    bool AllElementsAreUnique() const;

    inline size_t available() const { return capacity() - reserved() - size(); }
    inline size_t reserved() const { return reserved_; }
    inline size_t capacity() const { return capacity_; }
    inline size_t size() const { return constants_.size(); }
    inline size_t start_index() const { return start_index_; }
    inline size_t max_index() const { return start_index_ + capacity() - 1; }
    inline OperandSize operand_size() const { return operand_size_; }

   private:
    const size_t start_index_;
    const size_t capacity_;
    size_t reserved_;
    OperandSize operand_size_;
    ZoneVector<Handle<Object>> constants_;

    DISALLOW_COPY_AND_ASSIGN(ConstantArraySlice);
  };

  ConstantArraySlice* IndexToSlice(size_t index) const;
  ConstantArraySlice* OperandSizeToSlice(OperandSize operand_size) const;

  Handle<Object> the_hole_value() const { return the_hole_value_; }

  ConstantArraySlice* idx_slice_[3];
  base::TemplateHashMapImpl<Address, index_t, base::KeyEqualityMatcher<Address>,
                            ZoneAllocationPolicy>
      constants_map_;
  ZoneMap<Smi*, index_t> smi_map_;
  ZoneVector<std::pair<Smi*, index_t>> smi_pairs_;
  Zone* zone_;
  Handle<Object> the_hole_value_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_
