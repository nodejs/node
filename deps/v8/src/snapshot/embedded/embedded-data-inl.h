// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_EMBEDDED_EMBEDDED_DATA_INL_H_
#define V8_SNAPSHOT_EMBEDDED_EMBEDDED_DATA_INL_H_

#include "src/snapshot/embedded/embedded-data.h"

namespace v8 {
namespace internal {

bool EmbeddedData::BuiltinContains(Builtin builtin, Address pc) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  Address start =
      reinterpret_cast<Address>(RawCode() + desc.instruction_offset);
  DCHECK_LT(start, reinterpret_cast<Address>(code_ + code_size_));
  if (pc < start) return false;
  Address end = start + desc.instruction_length;
  return pc < end;
}

Address EmbeddedData::InstructionStartOfBuiltin(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result = RawCode() + desc.instruction_offset;
  DCHECK_LT(result, code_ + code_size_);
  return reinterpret_cast<Address>(result);
}

Address EmbeddedData::InstructionEndOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result =
      RawCode() + desc.instruction_offset + desc.instruction_length;
  DCHECK_LT(result, code_ + code_size_);
  return reinterpret_cast<Address>(result);
}

uint32_t EmbeddedData::InstructionSizeOfBuiltin(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  return desc.instruction_length;
}

Address EmbeddedData::MetadataStartOfBuiltin(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result = RawMetadata() + desc.metadata_offset;
  DCHECK_LE(desc.metadata_offset, data_size_);
  return reinterpret_cast<Address>(result);
}

uint32_t EmbeddedData::MetadataSizeOfBuiltin(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  return desc.metadata_length;
}

Address EmbeddedData::SafepointTableStartOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result = RawMetadata() + desc.metadata_offset;
  DCHECK_LE(desc.handler_table_offset, data_size_);
  return reinterpret_cast<Address>(result);
}

uint32_t EmbeddedData::SafepointTableSizeOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
#if V8_EMBEDDED_CONSTANT_POOL_BOOL
  DCHECK_LE(desc.handler_table_offset, desc.constant_pool_offset);
#else
  DCHECK_LE(desc.handler_table_offset, desc.code_comments_offset_offset);
#endif
  return desc.handler_table_offset - desc.metadata_offset;
}

Address EmbeddedData::HandlerTableStartOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result = RawMetadata() + desc.handler_table_offset;
  DCHECK_LE(desc.handler_table_offset, data_size_);
  return reinterpret_cast<Address>(result);
}

uint32_t EmbeddedData::HandlerTableSizeOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
#if V8_EMBEDDED_CONSTANT_POOL_BOOL
  DCHECK_LE(desc.handler_table_offset, desc.constant_pool_offset);
  return desc.constant_pool_offset - desc.handler_table_offset;
#else
  DCHECK_LE(desc.handler_table_offset, desc.code_comments_offset_offset);
  return desc.code_comments_offset_offset - desc.handler_table_offset;
#endif
}

Address EmbeddedData::ConstantPoolStartOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
#if V8_EMBEDDED_CONSTANT_POOL_BOOL
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result = RawMetadata() + desc.constant_pool_offset;
  DCHECK_LE(desc.constant_pool_offset, data_size_);
  return reinterpret_cast<Address>(result);
#else
  return kNullAddress;
#endif
}

uint32_t EmbeddedData::ConstantPoolSizeOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
#if V8_EMBEDDED_CONSTANT_POOL_BOOL
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  DCHECK_LE(desc.constant_pool_offset, desc.code_comments_offset_offset);
  return desc.code_comments_offset_offset - desc.constant_pool_offset;
#else
  return 0;
#endif
}

Address EmbeddedData::CodeCommentsStartOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result = RawMetadata() + desc.code_comments_offset_offset;
  DCHECK_LE(desc.code_comments_offset_offset, data_size_);
  return reinterpret_cast<Address>(result);
}

uint32_t EmbeddedData::CodeCommentsSizeOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  DCHECK_LE(desc.code_comments_offset_offset,
            desc.unwinding_info_offset_offset);
  return desc.unwinding_info_offset_offset - desc.code_comments_offset_offset;
}

Address EmbeddedData::UnwindingInfoStartOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  const uint8_t* result = RawMetadata() + desc.unwinding_info_offset_offset;
  DCHECK_LE(desc.unwinding_info_offset_offset, data_size_);
  return reinterpret_cast<Address>(result);
}

uint32_t EmbeddedData::UnwindingInfoSizeOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  uint32_t metadata_end_offset = desc.metadata_offset + desc.metadata_length;
  DCHECK_LE(desc.unwinding_info_offset_offset, metadata_end_offset);
  return metadata_end_offset - desc.unwinding_info_offset_offset;
}

uint32_t EmbeddedData::StackSlotsOf(Builtin builtin) const {
  DCHECK(Builtins::IsBuiltinId(builtin));
  const struct LayoutDescription& desc = LayoutDescription(builtin);
  return desc.stack_slots;
}

Address EmbeddedData::InstructionStartOfBytecodeHandlers() const {
  return InstructionStartOfBuiltin(Builtin::kFirstBytecodeHandler);
}

Address EmbeddedData::InstructionEndOfBytecodeHandlers() const {
  static_assert(static_cast<int>(Builtin::kFirstBytecodeHandler) +
                    kNumberOfBytecodeHandlers +
                    2 * kNumberOfWideBytecodeHandlers ==
                Builtins::kBuiltinCount);
  Builtin lastBytecodeHandler = Builtins::FromInt(Builtins::kBuiltinCount - 1);
  return InstructionStartOfBuiltin(lastBytecodeHandler) +
         InstructionSizeOfBuiltin(lastBytecodeHandler);
}

// Padded with kCodeAlignment.
// TODO(v8:11045): Consider removing code alignment.
uint32_t EmbeddedData::PaddedInstructionSizeOfBuiltin(Builtin builtin) const {
  uint32_t size = InstructionSizeOfBuiltin(builtin);
  CHECK_NE(size, 0);
  return PadAndAlignCode(size);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_EMBEDDED_EMBEDDED_DATA_INL_H_
