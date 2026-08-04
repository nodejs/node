// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BYTECODE_ARRAY_INL_H_
#define V8_OBJECTS_BYTECODE_ARRAY_INL_H_

#include "src/objects/bytecode-array.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/ptr-compr-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/interpreter/bytecode-register.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/trusted-pointer-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

int BytecodeArray::length() const { return length_.load().value(); }
int BytecodeArray::length(AcquireLoadTag tag) const {
  return length_.Acquire_Load().value();
}
void BytecodeArray::set_length(int value) {
  length_.store_no_write_barrier(Smi::FromInt(value));
}
void BytecodeArray::set_length(int value, ReleaseStoreTag tag) {
  length_.Release_Store_no_write_barrier(Smi::FromInt(value));
}

Tagged<TrustedByteArray> BytecodeArray::handler_table() const {
  DCHECK(has_handler_table());
  return handler_table_.load();
}
void BytecodeArray::set_handler_table(Tagged<TrustedByteArray> value,
                                      WriteBarrierMode mode) {
  handler_table_.store(this, value, mode);
}
bool BytecodeArray::has_handler_table() const {
  return !handler_table_.load().is_null();
}
void BytecodeArray::clear_handler_table() {
  handler_table_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<TrustedFixedArray> BytecodeArray::constant_pool() const {
  DCHECK(has_constant_pool());
  return constant_pool_.load();
}
void BytecodeArray::set_constant_pool(Tagged<TrustedFixedArray> value,
                                      WriteBarrierMode mode) {
  constant_pool_.store(this, value, mode);
}
bool BytecodeArray::has_constant_pool() const {
  return !constant_pool_.load().is_null();
}
void BytecodeArray::clear_constant_pool() {
  constant_pool_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<BytecodeWrapper> BytecodeArray::wrapper() const {
  return wrapper_.load();
}
void BytecodeArray::set_wrapper(Tagged<BytecodeWrapper> value,
                                WriteBarrierMode mode) {
  wrapper_.store(this, value, mode);
}

Tagged<TrustedByteArray> BytecodeArray::source_position_table(
    AcquireLoadTag) const {
  DCHECK(has_source_position_table(kAcquireLoad));
  return source_position_table_.Acquire_Load();
}
void BytecodeArray::set_source_position_table(Tagged<TrustedByteArray> value,
                                              ReleaseStoreTag,
                                              WriteBarrierMode mode) {
  source_position_table_.Release_Store(this, value, mode);
}
bool BytecodeArray::has_source_position_table(AcquireLoadTag) const {
  return !source_position_table_.Acquire_Load().is_null();
}
void BytecodeArray::clear_source_position_table(ReleaseStoreTag) {
  source_position_table_.Release_Store(this, {}, SKIP_WRITE_BARRIER);
}

uint8_t BytecodeArray::get(int index) const {
  DCHECK(index >= 0 && index < length());
  return bytes()[index];
}

void BytecodeArray::set(int index, uint8_t value) {
  DCHECK(index >= 0 && index < length());
  bytes()[index] = value;
}

void BytecodeArray::set_frame_size(int32_t frame_size) {
  DCHECK_GE(frame_size, 0);
  DCHECK(IsAligned(frame_size, kSystemPointerSize));
  frame_size_ = frame_size;
}

int32_t BytecodeArray::frame_size() const { return frame_size_; }

int BytecodeArray::register_count() const {
  return static_cast<int>(frame_size()) / kSystemPointerSize;
}

uint16_t BytecodeArray::parameter_count() const { return parameter_size_; }

uint16_t BytecodeArray::parameter_count_without_receiver() const {
  return parameter_count() - 1;
}

void BytecodeArray::set_parameter_count(uint16_t number_of_parameters) {
  parameter_size_ = number_of_parameters;
}

uint16_t BytecodeArray::max_arguments() const { return max_arguments_; }

void BytecodeArray::set_max_arguments(uint16_t max_arguments) {
  max_arguments_ = max_arguments;
}

int32_t BytecodeArray::max_frame_size() const {
  return frame_size() + (max_arguments() << kSystemPointerSizeLog2);
}

interpreter::Register BytecodeArray::incoming_new_target_or_generator_register()
    const {
  int32_t register_operand = incoming_new_target_or_generator_register_;
  if (register_operand == 0) {
    return interpreter::Register::invalid_value();
  } else {
    return interpreter::Register::FromOperand(register_operand);
  }
}

void BytecodeArray::set_incoming_new_target_or_generator_register(
    interpreter::Register incoming_new_target_or_generator_register) {
  if (!incoming_new_target_or_generator_register.is_valid()) {
    incoming_new_target_or_generator_register_ = 0;
  } else {
    DCHECK(incoming_new_target_or_generator_register.index() <
           register_count());
    DCHECK_NE(0, incoming_new_target_or_generator_register.ToOperand());
    incoming_new_target_or_generator_register_ =
        incoming_new_target_or_generator_register.ToOperand();
  }
}

void BytecodeArray::clear_padding() {
  int data_size = kHeaderSize + length();
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

Address BytecodeArray::GetFirstBytecodeAddress() {
  return reinterpret_cast<Address>(bytes());
}

bool BytecodeArray::HasSourcePositionTable() const {
  return has_source_position_table(kAcquireLoad);
}

Tagged<TrustedByteArray> BytecodeArray::SourcePositionTable() const {
  // WARNING: This function may be called from a background thread, hence
  // changes to how it accesses the heap can easily lead to bugs.
  Tagged<Object> maybe_table = raw_source_position_table(kAcquireLoad);
  if (maybe_table != Smi::zero())
    return TrustedCast<TrustedByteArray>(maybe_table);
  return Isolate::Current()->heap()->empty_trusted_byte_array();
}
Tagged<TrustedByteArray> BytecodeArray::SourcePositionTable(
    PtrComprCageBase cage_base) const {
  return SourcePositionTable();
}

void BytecodeArray::SetSourcePositionsFailedToCollect() {
  // Store Smi::zero() into the protected source_position_table_ slot
  // without a write barrier. ProtectedTaggedMember<TrustedByteArray> cannot
  // hold a Smi directly, so we fall back to a raw TaggedField store using
  // TrustedSpaceCompressionScheme (the scheme of the surrounding trusted
  // object). The encoded Smi value is scheme-independent, but the scheme
  // matters for non-Smi values, so any future caller must keep using
  // TrustedSpaceCompressionScheme here.
  TaggedField<Object, 0, TrustedSpaceCompressionScheme>::Release_Store(
      Tagged(this), kSourcePositionTableOffset, Smi::zero());
}

Tagged<Union<Smi, TrustedFixedArray>> BytecodeArray::raw_constant_pool() const {
  Tagged<Object> value = constant_pool_.load();
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || IsTrustedFixedArray(value));
  return TrustedCast<Union<Smi, TrustedFixedArray>>(value);
}

Tagged<Union<Smi, TrustedByteArray>> BytecodeArray::raw_handler_table() const {
  Tagged<Object> value = handler_table_.load();
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || IsTrustedByteArray(value));
  return TrustedCast<Union<Smi, TrustedByteArray>>(value);
}

Tagged<Object> BytecodeArray::raw_source_position_table(AcquireLoadTag) const {
  Tagged<Object> value = source_position_table_.Acquire_Load();
  // This field might be 0 during deserialization or if source positions have
  // not been (successfully) collected.
  DCHECK(value == Smi::zero() || IsTrustedByteArray(value));
  return value;
}

int BytecodeArray::BytecodeArraySize() const { return SizeFor(this->length()); }

int BytecodeArray::SizeIncludingMetadata() const {
  int size = BytecodeArraySize();
  auto maybe_constant_pool = raw_constant_pool();
  if (Tagged<TrustedFixedArray> array; TryCast(maybe_constant_pool, &array)) {
    size += array->Size();
  }
  auto maybe_handler_table = raw_handler_table();
  if (Tagged<TrustedByteArray> bytes; TryCast(maybe_handler_table, &bytes)) {
    size += bytes->AllocatedSize();
  }
  Tagged<Object> maybe_table = raw_source_position_table(kAcquireLoad);
  if (IsByteArray(maybe_table)) {
    size += Cast<ByteArray>(maybe_table)->AllocatedSize();
  }
  return size;
}

void BytecodeArray::MarkVerified(IsolateForSandbox isolate) {
#ifdef V8_ENABLE_SANDBOX
  // Only once bytecode has been verified do we "publish" it, thereby making it
  // accessible from within the sandbox via the trusted pointer table.
  Publish(isolate);
#endif

  // Now we also register the BytecodeArray with its in-sandbox wrapper. It
  // would also be possible to this earlier, when allocating the BytecodeArray,
  // but then we're in a slightly inconsistent state as many routines don't
  // expect to see in-sandbox references to unpublished objects.
  wrapper()->set_bytecode(Tagged(this));
}

Tagged<BytecodeArray> BytecodeWrapper::bytecode(
    IsolateForSandbox isolate) const {
  return bytecode_.load(isolate);
}
Tagged<BytecodeArray> BytecodeWrapper::bytecode(IsolateForSandbox isolate,
                                                AcquireLoadTag tag) const {
  return bytecode_.Acquire_Load(isolate);
}
void BytecodeWrapper::set_bytecode(Tagged<BytecodeArray> value,
                                   WriteBarrierMode mode) {
  bytecode_.store(this, value, mode);
}
void BytecodeWrapper::set_bytecode(Tagged<BytecodeArray> value, ReleaseStoreTag,
                                   WriteBarrierMode mode) {
  bytecode_.Release_Store(this, value, mode);
}
bool BytecodeWrapper::has_bytecode() const { return !bytecode_.is_empty(); }
void BytecodeWrapper::clear_bytecode() { bytecode_.clear(this); }

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BYTECODE_ARRAY_INL_H_
