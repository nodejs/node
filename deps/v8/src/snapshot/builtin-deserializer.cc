// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/builtin-deserializer.h"

#include "src/assembler-inl.h"
#include "src/interpreter/interpreter.h"
#include "src/objects-inl.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

using interpreter::Bytecodes;
using interpreter::Interpreter;

// Tracks the code object currently being deserialized (required for
// allocation).
class DeserializingCodeObjectScope {
 public:
  DeserializingCodeObjectScope(BuiltinDeserializer* builtin_deserializer,
                               int code_object_id)
      : builtin_deserializer_(builtin_deserializer) {
    DCHECK_EQ(BuiltinDeserializer::kNoCodeObjectId,
              builtin_deserializer->current_code_object_id_);
    builtin_deserializer->current_code_object_id_ = code_object_id;
  }

  ~DeserializingCodeObjectScope() {
    builtin_deserializer_->current_code_object_id_ =
        BuiltinDeserializer::kNoCodeObjectId;
  }

 private:
  BuiltinDeserializer* builtin_deserializer_;

  DISALLOW_COPY_AND_ASSIGN(DeserializingCodeObjectScope)
};

BuiltinDeserializer::BuiltinDeserializer(Isolate* isolate,
                                         const BuiltinSnapshotData* data)
    : Deserializer(data, false) {
  code_offsets_ = data->BuiltinOffsets();
  DCHECK_EQ(BSU::kNumberOfCodeObjects, code_offsets_.length());
  DCHECK(std::is_sorted(code_offsets_.begin(), code_offsets_.end()));

  Initialize(isolate);
}

void BuiltinDeserializer::DeserializeEagerBuiltinsAndHandlers() {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  DCHECK_EQ(0, source()->position());

  // Deserialize builtins.

  Builtins* builtins = isolate()->builtins();
  for (int i = 0; i < BSU::kNumberOfBuiltins; i++) {
    if (IsLazyDeserializationEnabled() && Builtins::IsLazy(i)) {
      // Do nothing. These builtins have been replaced by DeserializeLazy in
      // InitializeFromReservations.
      DCHECK_EQ(builtins->builtin(Builtins::kDeserializeLazy),
                builtins->builtin(i));
    } else {
      builtins->set_builtin(i, DeserializeBuiltinRaw(i));
    }
  }

#ifdef DEBUG
  for (int i = 0; i < BSU::kNumberOfBuiltins; i++) {
    Object* o = builtins->builtin(i);
    DCHECK(o->IsCode() && Code::cast(o)->is_builtin());
  }
#endif

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_builtin_code) {
    // We can't print builtins during deserialization because they may refer
    // to not yet deserialized builtins.
    for (int i = 0; i < BSU::kNumberOfBuiltins; i++) {
      if (!IsLazyDeserializationEnabled() || !Builtins::IsLazy(i)) {
        Code* code = builtins->builtin(i);
        const char* name = Builtins::name(i);
        code->PrintBuiltinCode(isolate(), name);
      }
    }
  }
#endif

  // Deserialize bytecode handlers.

  Interpreter* interpreter = isolate()->interpreter();
  DCHECK(!isolate()->interpreter()->IsDispatchTableInitialized());

  BSU::ForEachBytecode([=](Bytecode bytecode, OperandScale operand_scale) {
    // Bytecodes without a dedicated handler are patched up in a second pass.
    if (!Bytecodes::BytecodeHasHandler(bytecode, operand_scale)) return;

    // If lazy-deserialization is enabled and the current bytecode is lazy,
    // we write the generic LazyDeserialization handler into the dispatch table
    // and deserialize later upon first use.
    Code* code = (FLAG_lazy_handler_deserialization &&
                  IsLazyDeserializationEnabled() && Bytecodes::IsLazy(bytecode))
                     ? GetDeserializeLazyHandler(operand_scale)
                     : DeserializeHandlerRaw(bytecode, operand_scale);

    interpreter->SetBytecodeHandler(bytecode, operand_scale, code);
  });

  // Patch up holes in the dispatch table.

  Code* illegal_handler = interpreter->GetBytecodeHandler(
      Bytecode::kIllegal, OperandScale::kSingle);

  BSU::ForEachBytecode([=](Bytecode bytecode, OperandScale operand_scale) {
    if (Bytecodes::BytecodeHasHandler(bytecode, operand_scale)) return;
    interpreter->SetBytecodeHandler(bytecode, operand_scale, illegal_handler);
  });

  DCHECK(isolate()->interpreter()->IsDispatchTableInitialized());
}

Code* BuiltinDeserializer::DeserializeBuiltin(int builtin_id) {
  allocator()->ReserveAndInitializeBuiltinsTableForBuiltin(builtin_id);
  DisallowHeapAllocation no_gc;
  Code* code = DeserializeBuiltinRaw(builtin_id);

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_builtin_code) {
    const char* name = Builtins::name(builtin_id);
    code->PrintBuiltinCode(isolate(), name);
  }
#endif  // ENABLE_DISASSEMBLER

  return code;
}

Code* BuiltinDeserializer::DeserializeHandler(Bytecode bytecode,
                                              OperandScale operand_scale) {
  allocator()->ReserveForHandler(bytecode, operand_scale);
  DisallowHeapAllocation no_gc;
  return DeserializeHandlerRaw(bytecode, operand_scale);
}

Code* BuiltinDeserializer::DeserializeBuiltinRaw(int builtin_id) {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  DCHECK(Builtins::IsBuiltinId(builtin_id));

  DeserializingCodeObjectScope scope(this, builtin_id);

  const int initial_position = source()->position();
  source()->set_position(code_offsets_[builtin_id]);

  Object* o = ReadDataSingle();
  DCHECK(o->IsCode() && Code::cast(o)->is_builtin());

  // Rewind.
  source()->set_position(initial_position);

  // Flush the instruction cache.
  Code* code = Code::cast(o);
  Assembler::FlushICache(code->raw_instruction_start(),
                         code->raw_instruction_size());

  PROFILE(isolate(), CodeCreateEvent(CodeEventListener::BUILTIN_TAG,
                                     AbstractCode::cast(code),
                                     Builtins::name(builtin_id)));
  LOG_CODE_EVENT(isolate(),
                 CodeLinePosInfoRecordEvent(
                     code->raw_instruction_start(),
                     ByteArray::cast(code->source_position_table())));
  return code;
}

Code* BuiltinDeserializer::DeserializeHandlerRaw(Bytecode bytecode,
                                                 OperandScale operand_scale) {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  DCHECK(Bytecodes::BytecodeHasHandler(bytecode, operand_scale));

  const int code_object_id = BSU::BytecodeToIndex(bytecode, operand_scale);
  DeserializingCodeObjectScope scope(this, code_object_id);

  const int initial_position = source()->position();
  source()->set_position(code_offsets_[code_object_id]);

  Object* o = ReadDataSingle();
  DCHECK(o->IsCode() && Code::cast(o)->kind() == Code::BYTECODE_HANDLER);

  // Rewind.
  source()->set_position(initial_position);

  // Flush the instruction cache.
  Code* code = Code::cast(o);
  Assembler::FlushICache(code->raw_instruction_start(),
                         code->raw_instruction_size());

  std::string name = Bytecodes::ToString(bytecode, operand_scale);
  PROFILE(isolate(), CodeCreateEvent(CodeEventListener::HANDLER_TAG,
                                     AbstractCode::cast(code), name.c_str()));
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_builtin_code) {
    code->PrintBuiltinCode(isolate(), name.c_str());
  }
#endif  // ENABLE_DISASSEMBLER

  return code;
}

uint32_t BuiltinDeserializer::ExtractCodeObjectSize(int code_object_id) {
  DCHECK_LT(code_object_id, BSU::kNumberOfCodeObjects);

  const int initial_position = source()->position();

  // Grab the size of the code object.
  source()->set_position(code_offsets_[code_object_id]);
  byte data = source()->Get();

  USE(data);
  DCHECK_EQ(kNewObject | kPlain | kStartOfObject | CODE_SPACE, data);
  const uint32_t result = source()->GetInt() << kObjectAlignmentBits;

  // Rewind.
  source()->set_position(initial_position);

  return result;
}

Code* BuiltinDeserializer::GetDeserializeLazyHandler(
    interpreter::OperandScale operand_scale) const {
  STATIC_ASSERT(interpreter::BytecodeOperands::kOperandScaleCount == 3);
  switch (operand_scale) {
    case OperandScale::kSingle:
      return Code::cast(isolate()->heap()->deserialize_lazy_handler());
    case OperandScale::kDouble:
      return Code::cast(isolate()->heap()->deserialize_lazy_handler_wide());
    case OperandScale::kQuadruple:
      return Code::cast(
          isolate()->heap()->deserialize_lazy_handler_extra_wide());
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
