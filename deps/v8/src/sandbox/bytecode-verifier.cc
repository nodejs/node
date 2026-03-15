// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/bytecode-verifier.h"

#include "src/codegen/handler-table.h"
#include "src/flags/flags.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/runtime/runtime.h"
#include "src/utils/bit-vector.h"

namespace v8::internal {

// static
void BytecodeVerifier::Verify(IsolateForSandbox isolate,
                              Handle<BytecodeArray> bytecode, Zone* zone) {
  if (v8_flags.verify_bytecode_full) {
    VerifyFull(isolate, bytecode, zone);
  } else if (v8_flags.verify_bytecode_light) {
    VerifyLight(isolate, bytecode, zone);
  }

  bytecode->MarkVerified(isolate);
}

// static
void BytecodeVerifier::VerifyLight(IsolateForSandbox isolate,
                                   Handle<BytecodeArray> bytecode, Zone* zone) {
  // VerifyLight is meant to catch the most important issues (in particular,
  // ones that we've seen in the past) and should be lightweight enough to be
  // enabled by default.
  //
  // In particular, the lightweight verification ensures basic control-flow
  // integrity (CFI) by validating that jump targets are valid.

  unsigned bytecode_length = bytecode->length();
  BitVector valid_offsets(bytecode_length, zone);
  BitVector seen_jumps(bytecode_length, zone);

  interpreter::BytecodeArrayIterator iterator(bytecode);
  for (; !iterator.done(); iterator.Advance()) {
    unsigned current_offset = iterator.current_offset();
    valid_offsets.Add(current_offset);

    interpreter::Bytecode current_bytecode = iterator.current_bytecode();

    if (interpreter::Bytecodes::IsJump(current_bytecode)) {
      unsigned target_offset = iterator.GetJumpTargetOffset();
      Check(target_offset < bytecode_length, "Invalid jump offset");
      // We're specifically disallowing a forward jump with offset zero (i.e.
      // to itself here) as that may cause our compilers to become confused.
      Check(!interpreter::Bytecodes::IsForwardJump(current_bytecode) ||
                target_offset > current_offset,
            "Invalid jump offset");
      seen_jumps.Add(target_offset);
    } else if (interpreter::Bytecodes::IsSwitch(current_bytecode)) {
      for (const auto entry : iterator.GetJumpTableTargetOffsets()) {
        unsigned target_offset = entry.target_offset;
        Check(target_offset < bytecode_length, "Invalid switch offset");
        Check(target_offset > current_offset, "Invalid switch offset");
        seen_jumps.Add(target_offset);
      }
    }
  }

  Check(seen_jumps.IsSubsetOf(valid_offsets), "Invalid control-flow");

  HandlerTable table(*bytecode);
  for (int i = 0; i < table.NumberOfRangeEntries(); ++i) {
    unsigned start = table.GetRangeStart(i);
    unsigned end = table.GetRangeEnd(i);
    unsigned handler = table.GetRangeHandler(i);
    Check(end <= bytecode_length && start <= end,
          "Invalid exception handler range");
    Check(handler < bytecode_length && valid_offsets.Contains(handler),
          "Invalid exception handler offset");
  }
}

// static
void BytecodeVerifier::VerifyFull(IsolateForSandbox isolate,
                                  Handle<BytecodeArray> bytecode, Zone* zone) {
  // VerifyFull does full verification and is for now just used during fuzzing
  // (to test the verifier). However, in the future it may also (sometimes)
  // be enabled in production as well.
  //
  // Full verification includes the lightweight verification for CFI but also
  // ensures that registers accesses are valid (i.e. don't read/write
  // out-of-bounds on the stack). In addition, we also need to check a few more
  // invariants, for example that various IDs embedded in the bytecode are
  // valid, but that is mostly for fuzzer cleanliness.

  auto VerifyRegister = [&](interpreter::Register reg, bool is_output) {
    if (reg.is_parameter()) {
      int param_index = reg.ToParameterIndex();
      int parameter_count = bytecode->parameter_count();
      if (!base::IsInHalfOpenRange(param_index, 0, parameter_count)) {
        // Access to these special objects is fine as they are inside the
        // sandbox and so anyway untrusted. However, we must not access trusted
        // objects/values such as the BytecodeArray itself
        // (is_bytecode_array()) or the bytecode offset (is_bytecode_offset()).
        if (!reg.is_current_context() && !reg.is_function_closure()) {
          FailVerification("Parameter index out of bounds");
        } else if (is_output) {
          // However, we should not write to these special registers as they
          // are not "real" parameters. This would be fine if we were only
          // executing the bytecode (these objects are inside the sandbox, so
          // it doesn't matter if they or pointers to them get corrupted), but
          // for example our compilers sometimes assume that writes only go to
          // "normal" parameters or registers and would otherwise crash in
          // potentially unsafe ways. See e.g. https://crbug.com/467843950.
          FailVerification("Invalid write to special register");
        }
      }
    } else {
      Check(base::IsInHalfOpenRange(reg.index(), 0, bytecode->register_count()),
            "Register index out of bounds");
    }
  };

  interpreter::Register incoming_new_target_or_generator =
      bytecode->incoming_new_target_or_generator_register();
  if (incoming_new_target_or_generator.is_valid()) {
    VerifyRegister(incoming_new_target_or_generator, false);
  }

  unsigned constant_pool_length = bytecode->constant_pool()->length();

  interpreter::BytecodeArrayIterator iterator(bytecode);
  interpreter::Bytecode previous_bytecode = interpreter::Bytecode::kIllegal;
  int previous_bytecode_end = 0;
  for (; !iterator.done(); iterator.Advance()) {
    interpreter::Bytecode current_bytecode = iterator.current_bytecode();
    previous_bytecode = current_bytecode;

    Check(current_bytecode <= interpreter::Bytecode::kLast, "Invalid bytecode");
    // We don't really care about this one as it will terminate the process
    // upon execution, but it will terminate it in a way that we detect as
    // a bug, so we have to disallow it here for fuzzer cleanliness.
    Check(current_bytecode != interpreter::Bytecode::kIllegal,
          "Illegal bytecode");

    int current_bytecode_start = iterator.current_offset();
    int current_bytecode_end =
        current_bytecode_start + iterator.current_bytecode_size();
    previous_bytecode_end = current_bytecode_end;

    if (iterator.current_operand_scale() > interpreter::OperandScale::kSingle) {
      bool bytecode_can_be_scaled =
          interpreter::Bytecodes::IsBytecodeWithScalableOperands(
              current_bytecode);
      Check(bytecode_can_be_scaled, "Invalid scaled bytecode");
    }

    int operand_count =
        interpreter::Bytecodes::NumberOfOperands(current_bytecode);
    const interpreter::OperandType* operand_types =
        interpreter::Bytecodes::GetOperandTypes(current_bytecode);
    for (int i = 0; i < operand_count; ++i) {
      interpreter::OperandType operand_type = operand_types[i];
      switch (operand_type) {
        case interpreter::OperandType::kReg:
        case interpreter::OperandType::kRegOut:
        case interpreter::OperandType::kRegInOut:
        case interpreter::OperandType::kRegPair:
        case interpreter::OperandType::kRegOutPair:
        case interpreter::OperandType::kRegOutTriple:
        case interpreter::OperandType::kRegList:
        case interpreter::OperandType::kRegOutList: {
          bool is_output =
              interpreter::Bytecodes::IsRegisterOutputOperandType(operand_type);
          int count = iterator.GetRegisterOperandRange(i);
          if (count > 0) {
            interpreter::Register start = iterator.GetRegisterOperand(i);
            VerifyRegister(start, is_output);
            if (count > 1) {
              interpreter::Register end(start.index() + count - 1);
              VerifyRegister(end, is_output);
              Check(start.is_parameter() == end.is_parameter(),
                    "Register range crosses parameter/local boundary");
            }
          }
          break;
        }
        case interpreter::OperandType::kConstantPoolIndex: {
          unsigned index = iterator.GetConstantPoolIndexOperand(i);
          Check(index < constant_pool_length,
                "Constant pool index out of bounds");
          break;
        }
        case interpreter::OperandType::kRuntimeId: {
          Runtime::FunctionId id = iterator.GetRuntimeIdOperand(i);
          Check(id < Runtime::kNumFunctions, "Invalid runtime function id");
          Check(IsAllowedRuntimeFunction(id), "Disallowed runtime function");
          break;
        }
        case interpreter::OperandType::kAbortReason: {
          // This is mostly just needed for fuzzer cleanliness. Otherwise we'll
          // perform some OOB memory reads in the abort handler.
          int reason = static_cast<int>(iterator.GetAbortReasonOperand(i));
          Check(IsValidAbortReason(reason), "Invalid abort reason");
          break;
        }
        case interpreter::OperandType::kFlag8:
        case interpreter::OperandType::kFlag16:
        case interpreter::OperandType::kEmbeddedFeedback:
        case interpreter::OperandType::kIntrinsicId:
        case interpreter::OperandType::kNativeContextIndex:
        case interpreter::OperandType::kUImm:
        case interpreter::OperandType::kImm:
        case interpreter::OperandType::kFeedbackSlot:
        case interpreter::OperandType::kContextSlot:
        case interpreter::OperandType::kCoverageSlot:
        case interpreter::OperandType::kRegCount:
          break;
        case interpreter::OperandType::kNone:
          UNREACHABLE();
      }
    }

    if (interpreter::Bytecodes::WritesImplicitRegister(current_bytecode)) {
      interpreter::Register reg = iterator.GetStarTargetRegister();
      VerifyRegister(reg, true);
    }
  }

  Check(previous_bytecode_end == bytecode->length(),
        "Final instruction does not end at the end of the bytecode array");

  if (!interpreter::Bytecodes::Returns(previous_bytecode) &&
      !interpreter::Bytecodes::UnconditionallyThrows(previous_bytecode) &&
      !interpreter::Bytecodes::IsUnconditionalJump(previous_bytecode)) {
    FailVerification(
        "Bytecode does not end with a control-flow terminating instruction");
  }

  // Finally perform lightweight verification for CFI. If we ever enable full
  // verification in production then we'd most likely want to avoid iterating
  // over the bytecode twice, so we'd have to embed the CFI checks also here.
  //
  // It would probably be more "natural" to perform the lightweight
  // verification at the start instead of at the end. However, lightweight
  // verification doesn't ensure that the opcodes are valid (only full
  // verification does) and so we would get into weird states if we observe
  // invalid bytecode opcodes. Therefore we do this at the end, once we're
  // certain that the bytecode is otherwise well-structured.
  VerifyLight(isolate, bytecode, zone);
}

// static
bool BytecodeVerifier::IsAllowedRuntimeFunction(Runtime::FunctionId id) {
  // This is currently purely for fuzzer-cleanliness and so we only add
  // functions to this blocklist if we see them cause crashes during fuzzing.
  // It's unclear if we'll ever need this in production, but if we decide to
  // use it, we should do a more thorough audit to determine which functions
  // should be disallowed here.
  switch (id) {
#if V8_ENABLE_WEBASSEMBLY
    case Runtime::kWasmTriggerTierUp:
      return false;
#endif  // V8_ENABLE_WEBASSEMBLY
    default:
      return true;
  }
}

}  // namespace v8::internal
