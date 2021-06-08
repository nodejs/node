// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_TRANSLATION_ARRAY_H_
#define V8_DEOPTIMIZER_TRANSLATION_ARRAY_H_

#include "src/codegen/register-arch.h"
#include "src/deoptimizer/translation-opcode.h"
#include "src/objects/fixed-array.h"
#include "src/zone/zone-containers.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/value-type.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

class Factory;

// The TranslationArray is the on-heap representation of translations created
// during code generation in a (zone-allocated) TranslationArrayBuilder. The
// translation array specifies how to transform an optimized frame back into
// one or more unoptimized frames.
// TODO(jgruber): Consider a real type instead of this type alias.
using TranslationArray = ByteArray;

class TranslationArrayIterator {
 public:
  TranslationArrayIterator(TranslationArray buffer, int index);

  int32_t Next();

  bool HasNext() const;

  void Skip(int n) {
    for (int i = 0; i < n; i++) Next();
  }

 private:
  std::vector<int32_t> uncompressed_contents_;
  TranslationArray buffer_;
  int index_;
};

class TranslationArrayBuilder {
 public:
  explicit TranslationArrayBuilder(Zone* zone)
      : contents_(zone), contents_for_compression_(zone), zone_(zone) {}

  Handle<TranslationArray> ToTranslationArray(Factory* factory);

  int BeginTranslation(int frame_count, int jsframe_count,
                       int update_feedback_count) {
    int start_index = Size();
    auto opcode = TranslationOpcode::BEGIN;
    Add(opcode);
    Add(frame_count);
    Add(jsframe_count);
    Add(update_feedback_count);
    DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 3);
    return start_index;
  }

  void BeginInterpretedFrame(BytecodeOffset bytecode_offset, int literal_id,
                             unsigned height, int return_value_offset,
                             int return_value_count);
  void BeginArgumentsAdaptorFrame(int literal_id, unsigned height);
  void BeginConstructStubFrame(BytecodeOffset bailout_id, int literal_id,
                               unsigned height);
  void BeginBuiltinContinuationFrame(BytecodeOffset bailout_id, int literal_id,
                                     unsigned height);
#if V8_ENABLE_WEBASSEMBLY
  void BeginJSToWasmBuiltinContinuationFrame(
      BytecodeOffset bailout_id, int literal_id, unsigned height,
      base::Optional<wasm::ValueKind> return_kind);
#endif  // V8_ENABLE_WEBASSEMBLY
  void BeginJavaScriptBuiltinContinuationFrame(BytecodeOffset bailout_id,
                                               int literal_id, unsigned height);
  void BeginJavaScriptBuiltinContinuationWithCatchFrame(
      BytecodeOffset bailout_id, int literal_id, unsigned height);
  void ArgumentsElements(CreateArgumentsType type);
  void ArgumentsLength();
  void BeginCapturedObject(int length);
  void AddUpdateFeedback(int vector_literal, int slot);
  void DuplicateObject(int object_index);
  void StoreRegister(Register reg);
  void StoreInt32Register(Register reg);
  void StoreInt64Register(Register reg);
  void StoreUint32Register(Register reg);
  void StoreBoolRegister(Register reg);
  void StoreFloatRegister(FloatRegister reg);
  void StoreDoubleRegister(DoubleRegister reg);
  void StoreStackSlot(int index);
  void StoreInt32StackSlot(int index);
  void StoreInt64StackSlot(int index);
  void StoreUint32StackSlot(int index);
  void StoreBoolStackSlot(int index);
  void StoreFloatStackSlot(int index);
  void StoreDoubleStackSlot(int index);
  void StoreLiteral(int literal_id);
  void StoreJSFrameFunction();

 private:
  void Add(int32_t value);
  void Add(TranslationOpcode opcode) { Add(static_cast<int32_t>(opcode)); }

  int Size() const {
    return V8_UNLIKELY(FLAG_turbo_compress_translation_arrays)
               ? static_cast<int>(contents_for_compression_.size())
               : static_cast<int>(contents_.size());
  }
  int SizeInBytes() const {
    return V8_UNLIKELY(FLAG_turbo_compress_translation_arrays)
               ? Size() * kInt32Size
               : Size();
  }

  Zone* zone() const { return zone_; }

  ZoneVector<uint8_t> contents_;
  ZoneVector<int32_t> contents_for_compression_;
  Zone* const zone_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_TRANSLATION_ARRAY_H_
