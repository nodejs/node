// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BYTECODE_ARRAY_H_
#define V8_OBJECTS_BYTECODE_ARRAY_H_

#include "src/objects/struct.h"
#include "src/objects/trusted-object.h"
#include "src/objects/trusted-pointer.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class BytecodeWrapper;

namespace interpreter {
class Register;
}  // namespace interpreter

// TODO(jgruber): These should no longer be included here; instead, all
// TorqueGeneratedFooAsserts should be emitted into a global .cc file.
#include "torque-generated/src/objects/bytecode-array-tq.inc"

// BytecodeArray represents a sequence of interpreter bytecodes.
V8_OBJECT class BytecodeArray : public ExposedTrustedObject {
 public:
  // The length of this bytecode array, in bytes.
  inline int length() const;
  inline int length(AcquireLoadTag tag) const;
  inline void set_length(int value);
  inline void set_length(int value, ReleaseStoreTag tag);

  // The handler table contains offsets of exception handlers.
  DECL_PROTECTED_POINTER_ACCESSORS(handler_table, TrustedByteArray)

  DECL_PROTECTED_POINTER_ACCESSORS(constant_pool, TrustedFixedArray)

  // The BytecodeWrapper for this BytecodeArray. When the sandbox is enabled,
  // the BytecodeArray lives in trusted space outside of the sandbox, but the
  // wrapper object lives inside the main heap and therefore inside the
  // sandbox. As such, the wrapper object can be used in cases where a
  // BytecodeArray needs to be referenced alongside other tagged pointer
  // references (so for example inside a FixedArray).
  inline Tagged<BytecodeWrapper> wrapper() const;
  inline void set_wrapper(Tagged<BytecodeWrapper> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Source position table. Can contain:
  // * Smi::zero() (initial value, or if an error occurred while explicitly
  // collecting source positions for pre-existing bytecode).
  // * empty_trusted_byte_array (for bytecode generated for functions that will
  // never have source positions, e.g. native functions).
  // * TrustedByteArray (if source positions were collected for the bytecode)
  DECL_RELEASE_ACQUIRE_PROTECTED_POINTER_ACCESSORS(source_position_table,
                                                   TrustedByteArray)

  DECL_INT32_ACCESSORS(frame_size)

  inline int32_t max_frame_size() const;

  static constexpr int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }

  inline uint8_t get(int index) const;
  inline void set(int index, uint8_t value);

  inline Address GetFirstBytecodeAddress();

  // Note: The register count is derived from frame_size.
  inline int register_count() const;

  // Note: the parameter count includes the implicit 'this' receiver.
  inline uint16_t parameter_count() const;
  inline uint16_t parameter_count_without_receiver() const;
  inline void set_parameter_count(uint16_t number_of_parameters);
  inline uint16_t max_arguments() const;
  inline void set_max_arguments(uint16_t max_arguments);

  inline interpreter::Register incoming_new_target_or_generator_register()
      const;
  inline void set_incoming_new_target_or_generator_register(
      interpreter::Register incoming_new_target_or_generator_register);

  inline bool HasSourcePositionTable() const;
  int SourcePosition(int offset) const;
  int SourceStatementPosition(int offset) const;

  // If source positions have not been collected or an exception has been thrown
  // this will return the empty_trusted_byte_array.
  inline Tagged<TrustedByteArray> SourcePositionTable() const;
  inline Tagged<TrustedByteArray> SourcePositionTable(
      PtrComprCageBase cage_base) const;

  // Raw accessors to access these fields during code cache deserialization.
  inline Tagged<Union<Smi, TrustedFixedArray>> raw_constant_pool() const;
  inline Tagged<Union<Smi, TrustedByteArray>> raw_handler_table() const;
  // This accessor can also be used when it's not guaranteed that a source
  // position table exists, for example because it hasn't been collected. In
  // that case, Smi::zero() will be returned.
  inline Tagged<Object> raw_source_position_table(AcquireLoadTag) const;

  // Indicates that an attempt was made to collect source positions, but that it
  // failed, most likely due to stack exhaustion. When in this state
  // |SourcePositionTable| will return an empty byte array.
  inline void SetSourcePositionsFailedToCollect();

  inline int BytecodeArraySize() const;

  // Returns the size of bytecode and its metadata. This includes the size of
  // bytecode, constant pool, source position table, and handler table.
  inline int SizeIncludingMetadata() const;

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

  class BodyDescriptor;

  // Back-compat offset/size constants.
  static const int kLengthOffset;
  static const int kWrapperOffset;
  static const int kSourcePositionTableOffset;
  static const int kHandlerTableOffset;
  static const int kConstantPoolOffset;
  static const int kFrameSizeOffset;
  static const int kParameterSizeOffset;
  static const int kMaxArgumentsOffset;
  static const int kIncomingNewTargetOrGeneratorRegisterOffset;
  static const int kHeaderSize;
  static const int kBytesOffset;

  // Maximal memory consumption for a single BytecodeArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single BytecodeArray.
  static const int kMaxLength;

 private:
  friend class BytecodeVerifier;
  friend class NoOpBytecodeVerifier;

  // Mark this BytecodeArray as successfully verified. Must only be called by
  // the BytecodeVerifier after successful verification.
  // Under the hood, this will "publish" the BytecodeArray, making it
  // accessible to the sandbox. As such, (only) after this step the
  // BytecodeArray can be executed in the interpreter.
  inline void MarkVerified(IsolateForSandbox isolate);

 public:
  TaggedMember<Smi> length_;
  TaggedMember<BytecodeWrapper> wrapper_;
  ProtectedTaggedMember<TrustedByteArray> source_position_table_;
  ProtectedTaggedMember<TrustedByteArray> handler_table_;
  ProtectedTaggedMember<TrustedFixedArray> constant_pool_;
  int32_t frame_size_;
  uint16_t parameter_size_;
  uint16_t max_arguments_;
  int32_t incoming_new_target_or_generator_register_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
  FLEXIBLE_ARRAY_MEMBER(uint8_t, bytes);
} V8_OBJECT_END;

inline constexpr int BytecodeArray::kLengthOffset =
    offsetof(BytecodeArray, length_);
inline constexpr int BytecodeArray::kWrapperOffset =
    offsetof(BytecodeArray, wrapper_);
inline constexpr int BytecodeArray::kSourcePositionTableOffset =
    offsetof(BytecodeArray, source_position_table_);
inline constexpr int BytecodeArray::kHandlerTableOffset =
    offsetof(BytecodeArray, handler_table_);
inline constexpr int BytecodeArray::kConstantPoolOffset =
    offsetof(BytecodeArray, constant_pool_);
inline constexpr int BytecodeArray::kFrameSizeOffset =
    offsetof(BytecodeArray, frame_size_);
inline constexpr int BytecodeArray::kParameterSizeOffset =
    offsetof(BytecodeArray, parameter_size_);
inline constexpr int BytecodeArray::kMaxArgumentsOffset =
    offsetof(BytecodeArray, max_arguments_);
inline constexpr int
    BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset =
        offsetof(BytecodeArray, incoming_new_target_or_generator_register_);
inline constexpr int BytecodeArray::kHeaderSize =
    OFFSET_OF_DATA_START(BytecodeArray);
inline constexpr int BytecodeArray::kBytesOffset =
    OFFSET_OF_DATA_START(BytecodeArray);
inline constexpr int BytecodeArray::kMaxLength =
    BytecodeArray::kMaxSize - BytecodeArray::kHeaderSize;

// A BytecodeWrapper wraps a BytecodeArray but lives inside the sandbox. This
// can be useful for example when a reference to a BytecodeArray needs to be
// stored along other tagged pointers inside an array or similar datastructure.
V8_OBJECT class BytecodeWrapper : public Struct {
 public:
  DECL_TRUSTED_POINTER_ACCESSORS(bytecode, BytecodeArray)

  DECL_PRINTER(BytecodeWrapper)
  DECL_VERIFIER(BytecodeWrapper)

  class BodyDescriptor;

 public:
  TrustedPointerMember<BytecodeArray, kBytecodeArrayIndirectPointerTag>
      bytecode_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BYTECODE_ARRAY_H_
