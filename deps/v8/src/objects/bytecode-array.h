// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BYTECODE_ARRAY_H_
#define V8_OBJECTS_BYTECODE_ARRAY_H_

#include "src/objects/fixed-array.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

namespace interpreter {
class Register;
}  // namespace interpreter

#include "torque-generated/src/objects/bytecode-array-tq.inc"

// BytecodeArray represents a sequence of interpreter bytecodes.
class BytecodeArray
    : public TorqueGeneratedBytecodeArray<BytecodeArray, FixedArrayBase> {
 public:
  static constexpr int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }

  inline uint8_t get(int index) const;
  inline void set(int index, uint8_t value);

  inline Address GetFirstBytecodeAddress();

  inline int32_t frame_size() const;
  inline void set_frame_size(int32_t frame_size);

  // Note: The register count is derived from frame_size.
  inline int register_count() const;

  // Note: the parameter count includes the implicit 'this' receiver.
  inline int32_t parameter_count() const;
  inline void set_parameter_count(int32_t number_of_parameters);

  inline interpreter::Register incoming_new_target_or_generator_register()
      const;
  inline void set_incoming_new_target_or_generator_register(
      interpreter::Register incoming_new_target_or_generator_register);

  inline bool HasSourcePositionTable() const;
  inline bool DidSourcePositionGenerationFail() const;

  // If source positions have not been collected or an exception has been thrown
  // this will return empty_byte_array.
  DECL_GETTER(SourcePositionTable, Tagged<ByteArray>)

  // Raw accessors to access these fields during code cache deserialization.
  DECL_GETTER(raw_constant_pool, Tagged<Object>)
  DECL_GETTER(raw_handler_table, Tagged<Object>)
  DECL_GETTER(raw_source_position_table, Tagged<Object>)

  // Indicates that an attempt was made to collect source positions, but that it
  // failed most likely due to stack exhaustion. When in this state
  // |SourcePositionTable| will return an empty byte array rather than crashing
  // as it would if no attempt was ever made to collect source positions.
  inline void SetSourcePositionsFailedToCollect();

  inline int BytecodeArraySize() const;

  // Returns the size of bytecode and its metadata. This includes the size of
  // bytecode, constant pool, source position table, and handler table.
  DECL_GETTER(SizeIncludingMetadata, int)

  DECL_PRINTER(BytecodeArray)
  DECL_VERIFIER(BytecodeArray)

  V8_EXPORT_PRIVATE void PrintJson(std::ostream& os);
  V8_EXPORT_PRIVATE void Disassemble(std::ostream& os);

  V8_EXPORT_PRIVATE static void Disassemble(Handle<BytecodeArray> handle,
                                            std::ostream& os);

  void CopyBytecodesTo(Tagged<BytecodeArray> to);

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  // Maximal memory consumption for a single BytecodeArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single BytecodeArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;

  class BodyDescriptor;

 private:
  // Hide accessors inherited from generated class. Use parameter_count instead.
  DECL_INT_ACCESSORS(parameter_size)

  TQ_OBJECT_CONSTRUCTORS(BytecodeArray)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BYTECODE_ARRAY_H_
