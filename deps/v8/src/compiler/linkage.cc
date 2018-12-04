// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/linkage.h"

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/frame.h"
#include "src/compiler/node.h"
#include "src/compiler/osr.h"
#include "src/compiler/pipeline.h"
#include "src/optimized-compilation-info.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

inline LinkageLocation regloc(Register reg, MachineType type) {
  return LinkageLocation::ForRegister(reg.code(), type);
}

}  // namespace


std::ostream& operator<<(std::ostream& os, const CallDescriptor::Kind& k) {
  switch (k) {
    case CallDescriptor::kCallCodeObject:
      os << "Code";
      break;
    case CallDescriptor::kCallJSFunction:
      os << "JS";
      break;
    case CallDescriptor::kCallAddress:
      os << "Addr";
      break;
    case CallDescriptor::kCallWasmFunction:
      os << "Wasm";
      break;
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, const CallDescriptor& d) {
  // TODO(svenpanne) Output properties etc. and be less cryptic.
  return os << d.kind() << ":" << d.debug_name() << ":r" << d.ReturnCount()
            << "s" << d.StackParameterCount() << "i" << d.InputCount() << "f"
            << d.FrameStateCount();
}

MachineSignature* CallDescriptor::GetMachineSignature(Zone* zone) const {
  size_t param_count = ParameterCount();
  size_t return_count = ReturnCount();
  MachineType* types = zone->NewArray<MachineType>(param_count + return_count);
  int current = 0;
  for (size_t i = 0; i < return_count; ++i) {
    types[current++] = GetReturnType(i);
  }
  for (size_t i = 0; i < param_count; ++i) {
    types[current++] = GetParameterType(i);
  }
  return new (zone) MachineSignature(return_count, param_count, types);
}

bool CallDescriptor::HasSameReturnLocationsAs(
    const CallDescriptor* other) const {
  if (ReturnCount() != other->ReturnCount()) return false;
  for (size_t i = 0; i < ReturnCount(); ++i) {
    if (GetReturnLocation(i) != other->GetReturnLocation(i)) return false;
  }
  return true;
}

int CallDescriptor::GetFirstUnusedStackSlot() const {
  int slots_above_sp = 0;
  for (size_t i = 0; i < InputCount(); ++i) {
    LinkageLocation operand = GetInputLocation(i);
    if (!operand.IsRegister()) {
      int new_candidate =
          -operand.GetLocation() + operand.GetSizeInPointers() - 1;
      if (new_candidate > slots_above_sp) {
        slots_above_sp = new_candidate;
      }
    }
  }
  return slots_above_sp;
}

int CallDescriptor::GetStackParameterDelta(
    CallDescriptor const* tail_caller) const {
  int callee_slots_above_sp = GetFirstUnusedStackSlot();
  int tail_caller_slots_above_sp = tail_caller->GetFirstUnusedStackSlot();
  int stack_param_delta = callee_slots_above_sp - tail_caller_slots_above_sp;
  if (kPadArguments) {
    // Adjust stack delta when it is odd.
    if (stack_param_delta % 2 != 0) {
      if (callee_slots_above_sp % 2 != 0) {
        // The delta is odd due to the callee - we will need to add one slot
        // of padding.
        ++stack_param_delta;
      } else {
        // The delta is odd because of the caller. We already have one slot of
        // padding that we can reuse for arguments, so we will need one fewer
        // slot.
        --stack_param_delta;
      }
    }
  }
  return stack_param_delta;
}

bool CallDescriptor::CanTailCall(const Node* node) const {
  return HasSameReturnLocationsAs(CallDescriptorOf(node->op()));
}

int CallDescriptor::CalculateFixedFrameSize() const {
  switch (kind_) {
    case kCallJSFunction:
      return PushArgumentCount()
                 ? OptimizedBuiltinFrameConstants::kFixedSlotCount
                 : StandardFrameConstants::kFixedSlotCount;
    case kCallAddress:
      return CommonFrameConstants::kFixedSlotCountAboveFp +
             CommonFrameConstants::kCPSlotCount;
    case kCallCodeObject:
      return TypedFrameConstants::kFixedSlotCount;
    case kCallWasmFunction:
      return WasmCompiledFrameConstants::kFixedSlotCount;
  }
  UNREACHABLE();
}

CallDescriptor* Linkage::ComputeIncoming(Zone* zone,
                                         OptimizedCompilationInfo* info) {
  DCHECK(!info->IsStub());
  if (!info->closure().is_null()) {
    // If we are compiling a JS function, use a JS call descriptor,
    // plus the receiver.
    SharedFunctionInfo* shared = info->closure()->shared();
    return GetJSCallDescriptor(zone, info->is_osr(),
                               1 + shared->internal_formal_parameter_count(),
                               CallDescriptor::kCanUseRoots);
  }
  return nullptr;  // TODO(titzer): ?
}


// static
bool Linkage::NeedsFrameStateInput(Runtime::FunctionId function) {
  switch (function) {
    // Most runtime functions need a FrameState. A few chosen ones that we know
    // not to call into arbitrary JavaScript, not to throw, and not to lazily
    // deoptimize are whitelisted here and can be called without a FrameState.
    case Runtime::kAbort:
    case Runtime::kAllocateInTargetSpace:
    case Runtime::kCreateIterResultObject:
    case Runtime::kIncBlockCounter:
    case Runtime::kIsFunction:
    case Runtime::kNewClosure:
    case Runtime::kNewClosure_Tenured:
    case Runtime::kNewFunctionContext:
    case Runtime::kPushBlockContext:
    case Runtime::kPushCatchContext:
    case Runtime::kReThrow:
    case Runtime::kStringEqual:
    case Runtime::kStringLessThan:
    case Runtime::kStringLessThanOrEqual:
    case Runtime::kStringGreaterThan:
    case Runtime::kStringGreaterThanOrEqual:
    case Runtime::kToFastProperties:  // TODO(conradw): Is it safe?
    case Runtime::kTraceEnter:
    case Runtime::kTraceExit:
      return false;

    // Some inline intrinsics are also safe to call without a FrameState.
    case Runtime::kInlineCreateIterResultObject:
    case Runtime::kInlineGeneratorClose:
    case Runtime::kInlineGeneratorGetResumeMode:
    case Runtime::kInlineCreateJSGeneratorObject:
    case Runtime::kInlineIsArray:
    case Runtime::kInlineIsJSReceiver:
    case Runtime::kInlineIsRegExp:
    case Runtime::kInlineIsSmi:
    case Runtime::kInlineIsTypedArray:
      return false;

    default:
      break;
  }

  // For safety, default to needing a FrameState unless whitelisted.
  return true;
}


bool CallDescriptor::UsesOnlyRegisters() const {
  for (size_t i = 0; i < InputCount(); ++i) {
    if (!GetInputLocation(i).IsRegister()) return false;
  }
  for (size_t i = 0; i < ReturnCount(); ++i) {
    if (!GetReturnLocation(i).IsRegister()) return false;
  }
  return true;
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Zone* zone, Runtime::FunctionId function_id, int js_parameter_count,
    Operator::Properties properties, CallDescriptor::Flags flags) {
  const Runtime::Function* function = Runtime::FunctionForId(function_id);
  const int return_count = function->result_size;
  const char* debug_name = function->name;

  if (!Linkage::NeedsFrameStateInput(function_id)) {
    flags = static_cast<CallDescriptor::Flags>(
        flags & ~CallDescriptor::kNeedsFrameState);
  }

  return GetCEntryStubCallDescriptor(zone, return_count, js_parameter_count,
                                     debug_name, properties, flags);
}

CallDescriptor* Linkage::GetCEntryStubCallDescriptor(
    Zone* zone, int return_count, int js_parameter_count,
    const char* debug_name, Operator::Properties properties,
    CallDescriptor::Flags flags) {
  const size_t function_count = 1;
  const size_t num_args_count = 1;
  const size_t context_count = 1;
  const size_t parameter_count = function_count +
                                 static_cast<size_t>(js_parameter_count) +
                                 num_args_count + context_count;

  LocationSignature::Builder locations(zone, static_cast<size_t>(return_count),
                                       static_cast<size_t>(parameter_count));

  // Add returns.
  if (locations.return_count_ > 0) {
    locations.AddReturn(regloc(kReturnRegister0, MachineType::AnyTagged()));
  }
  if (locations.return_count_ > 1) {
    locations.AddReturn(regloc(kReturnRegister1, MachineType::AnyTagged()));
  }
  if (locations.return_count_ > 2) {
    locations.AddReturn(regloc(kReturnRegister2, MachineType::AnyTagged()));
  }

  // All parameters to the runtime call go on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    locations.AddParam(LinkageLocation::ForCallerFrameSlot(
        i - js_parameter_count, MachineType::AnyTagged()));
  }
  // Add runtime function itself.
  locations.AddParam(
      regloc(kRuntimeCallFunctionRegister, MachineType::Pointer()));

  // Add runtime call argument count.
  locations.AddParam(
      regloc(kRuntimeCallArgCountRegister, MachineType::Int32()));

  // Add context.
  locations.AddParam(regloc(kContextRegister, MachineType::AnyTagged()));

  // The target for runtime calls is a code object.
  MachineType target_type = MachineType::AnyTagged();
  LinkageLocation target_loc =
      LinkageLocation::ForAnyRegister(MachineType::AnyTagged());
  return new (zone) CallDescriptor(     // --
      CallDescriptor::kCallCodeObject,  // kind
      target_type,                      // target MachineType
      target_loc,                       // target location
      locations.Build(),                // location_sig
      js_parameter_count,               // stack_parameter_count
      properties,                       // properties
      kNoCalleeSaved,                   // callee-saved
      kNoCalleeSaved,                   // callee-saved fp
      flags,                            // flags
      debug_name);                      // debug name
}

CallDescriptor* Linkage::GetJSCallDescriptor(Zone* zone, bool is_osr,
                                             int js_parameter_count,
                                             CallDescriptor::Flags flags) {
  const size_t return_count = 1;
  const size_t context_count = 1;
  const size_t new_target_count = 1;
  const size_t num_args_count = 1;
  const size_t parameter_count =
      js_parameter_count + new_target_count + num_args_count + context_count;

  LocationSignature::Builder locations(zone, return_count, parameter_count);

  // All JS calls have exactly one return value.
  locations.AddReturn(regloc(kReturnRegister0, MachineType::AnyTagged()));

  // All parameters to JS calls go on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    int spill_slot_index = i - js_parameter_count;
    locations.AddParam(LinkageLocation::ForCallerFrameSlot(
        spill_slot_index, MachineType::AnyTagged()));
  }

  // Add JavaScript call new target value.
  locations.AddParam(
      regloc(kJavaScriptCallNewTargetRegister, MachineType::AnyTagged()));

  // Add JavaScript call argument count.
  locations.AddParam(
      regloc(kJavaScriptCallArgCountRegister, MachineType::Int32()));

  // Add context.
  locations.AddParam(regloc(kContextRegister, MachineType::AnyTagged()));

  // The target for JS function calls is the JSFunction object.
  MachineType target_type = MachineType::AnyTagged();
  // When entering into an OSR function from unoptimized code the JSFunction
  // is not in a register, but it is on the stack in the marker spill slot.
  LinkageLocation target_loc =
      is_osr ? LinkageLocation::ForSavedCallerFunction()
             : regloc(kJSFunctionRegister, MachineType::AnyTagged());
  return new (zone) CallDescriptor(     // --
      CallDescriptor::kCallJSFunction,  // kind
      target_type,                      // target MachineType
      target_loc,                       // target location
      locations.Build(),                // location_sig
      js_parameter_count,               // stack_parameter_count
      Operator::kNoProperties,          // properties
      kNoCalleeSaved,                   // callee-saved
      kNoCalleeSaved,                   // callee-saved fp
      flags,                            // flags
      "js-call");
}

// TODO(turbofan): cache call descriptors for code stub calls.
// TODO(jgruber): Clean up stack parameter count handling. The descriptor
// already knows the formal stack parameter count and ideally only additional
// stack parameters should be passed into this method. All call-sites should
// be audited for correctness (e.g. many used to assume a stack parameter count
// of 0).
CallDescriptor* Linkage::GetStubCallDescriptor(
    Zone* zone, const CallInterfaceDescriptor& descriptor,
    int stack_parameter_count, CallDescriptor::Flags flags,
    Operator::Properties properties, StubCallMode stub_mode) {
  const int register_parameter_count = descriptor.GetRegisterParameterCount();
  const int js_parameter_count =
      register_parameter_count + stack_parameter_count;
  const int context_count = descriptor.HasContextParameter() ? 1 : 0;
  const size_t parameter_count =
      static_cast<size_t>(js_parameter_count + context_count);

  DCHECK_GE(stack_parameter_count, descriptor.GetStackParameterCount());

  size_t return_count = descriptor.GetReturnCount();
  LocationSignature::Builder locations(zone, return_count, parameter_count);

  // Add returns.
  if (locations.return_count_ > 0) {
    locations.AddReturn(regloc(kReturnRegister0, descriptor.GetReturnType(0)));
  }
  if (locations.return_count_ > 1) {
    locations.AddReturn(regloc(kReturnRegister1, descriptor.GetReturnType(1)));
  }
  if (locations.return_count_ > 2) {
    locations.AddReturn(regloc(kReturnRegister2, descriptor.GetReturnType(2)));
  }

  // Add parameters in registers and on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    if (i < register_parameter_count) {
      // The first parameters go in registers.
      Register reg = descriptor.GetRegisterParameter(i);
      MachineType type = descriptor.GetParameterType(i);
      locations.AddParam(regloc(reg, type));
    } else {
      // The rest of the parameters go on the stack.
      int stack_slot = i - register_parameter_count - stack_parameter_count;
      locations.AddParam(LinkageLocation::ForCallerFrameSlot(
          stack_slot, MachineType::AnyTagged()));
    }
  }
  // Add context.
  if (context_count) {
    locations.AddParam(regloc(kContextRegister, MachineType::AnyTagged()));
  }

  // The target for stub calls depends on the requested mode.
  CallDescriptor::Kind kind = stub_mode == StubCallMode::kCallWasmRuntimeStub
                                  ? CallDescriptor::kCallWasmFunction
                                  : CallDescriptor::kCallCodeObject;
  MachineType target_type = stub_mode == StubCallMode::kCallWasmRuntimeStub
                                ? MachineType::Pointer()
                                : MachineType::AnyTagged();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister(target_type);
  return new (zone) CallDescriptor(          // --
      kind,                                  // kind
      target_type,                           // target MachineType
      target_loc,                            // target location
      locations.Build(),                     // location_sig
      stack_parameter_count,                 // stack_parameter_count
      properties,                            // properties
      kNoCalleeSaved,                        // callee-saved registers
      kNoCalleeSaved,                        // callee-saved fp
      CallDescriptor::kCanUseRoots | flags,  // flags
      descriptor.DebugName(),                // debug name
      descriptor.allocatable_registers());
}

// static
CallDescriptor* Linkage::GetBytecodeDispatchCallDescriptor(
    Zone* zone, const CallInterfaceDescriptor& descriptor,
    int stack_parameter_count) {
  const int register_parameter_count = descriptor.GetRegisterParameterCount();
  const int parameter_count = register_parameter_count + stack_parameter_count;

  DCHECK_EQ(descriptor.GetReturnCount(), 1);
  LocationSignature::Builder locations(zone, 1, parameter_count);

  locations.AddReturn(regloc(kReturnRegister0, descriptor.GetReturnType(0)));

  // Add parameters in registers and on the stack.
  for (int i = 0; i < parameter_count; i++) {
    if (i < register_parameter_count) {
      // The first parameters go in registers.
      Register reg = descriptor.GetRegisterParameter(i);
      MachineType type = descriptor.GetParameterType(i);
      locations.AddParam(regloc(reg, type));
    } else {
      // The rest of the parameters go on the stack.
      int stack_slot = i - register_parameter_count - stack_parameter_count;
      locations.AddParam(LinkageLocation::ForCallerFrameSlot(
          stack_slot, MachineType::AnyTagged()));
    }
  }

  // The target for interpreter dispatches is a code entry address.
  MachineType target_type = MachineType::Pointer();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister(target_type);
  const CallDescriptor::Flags kFlags =
      CallDescriptor::kCanUseRoots | CallDescriptor::kFixedTargetRegister;
  return new (zone) CallDescriptor(  // --
      CallDescriptor::kCallAddress,  // kind
      target_type,                   // target MachineType
      target_loc,                    // target location
      locations.Build(),             // location_sig
      stack_parameter_count,         // stack_parameter_count
      Operator::kNoProperties,       // properties
      kNoCalleeSaved,                // callee-saved registers
      kNoCalleeSaved,                // callee-saved fp
      kFlags,                        // flags
      descriptor.DebugName());
}

LinkageLocation Linkage::GetOsrValueLocation(int index) const {
  CHECK(incoming_->IsJSFunctionCall());
  int parameter_count = static_cast<int>(incoming_->JSParameterCount() - 1);
  int first_stack_slot = OsrHelper::FirstStackSlotIndex(parameter_count);

  if (index == kOsrContextSpillSlotIndex) {
    // Context. Use the parameter location of the context spill slot.
    // Parameter (arity + 2) is special for the context of the function frame.
    // >> context_index = target + receiver + params + new_target + #args
    int context_index = 1 + 1 + parameter_count + 1 + 1;
    return incoming_->GetInputLocation(context_index);
  } else if (index >= first_stack_slot) {
    // Local variable stored in this (callee) stack.
    int spill_index =
        index - first_stack_slot + StandardFrameConstants::kFixedSlotCount;
    return LinkageLocation::ForCalleeFrameSlot(spill_index,
                                               MachineType::AnyTagged());
  } else {
    // Parameter. Use the assigned location from the incoming call descriptor.
    int parameter_index = 1 + index;  // skip index 0, which is the target.
    return incoming_->GetInputLocation(parameter_index);
  }
}

namespace {
inline bool IsTaggedReg(const LinkageLocation& loc, Register reg) {
  return loc.IsRegister() && loc.AsRegister() == reg.code() &&
         loc.GetType().representation() ==
             MachineRepresentation::kTaggedPointer;
}
}  // namespace

bool Linkage::ParameterHasSecondaryLocation(int index) const {
  // TODO(titzer): this should be configurable, not call-type specific.
  if (incoming_->IsJSFunctionCall()) {
    LinkageLocation loc = GetParameterLocation(index);
    return IsTaggedReg(loc, kJSFunctionRegister) ||
           IsTaggedReg(loc, kContextRegister);
  }
  if (incoming_->IsWasmFunctionCall()) {
    LinkageLocation loc = GetParameterLocation(index);
    return IsTaggedReg(loc, kWasmInstanceRegister);
  }
  return false;
}

LinkageLocation Linkage::GetParameterSecondaryLocation(int index) const {
  // TODO(titzer): these constants are necessary due to offset/slot# mismatch
  static const int kJSContextSlot = 2 + StandardFrameConstants::kCPSlotCount;
  static const int kJSFunctionSlot = 3 + StandardFrameConstants::kCPSlotCount;
  static const int kWasmInstanceSlot = 3 + StandardFrameConstants::kCPSlotCount;

  DCHECK(ParameterHasSecondaryLocation(index));
  LinkageLocation loc = GetParameterLocation(index);

  // TODO(titzer): this should be configurable, not call-type specific.
  if (incoming_->IsJSFunctionCall()) {
    if (IsTaggedReg(loc, kJSFunctionRegister)) {
      return LinkageLocation::ForCalleeFrameSlot(kJSFunctionSlot,
                                                 MachineType::AnyTagged());
    } else {
      DCHECK(IsTaggedReg(loc, kContextRegister));
      return LinkageLocation::ForCalleeFrameSlot(kJSContextSlot,
                                                 MachineType::AnyTagged());
    }
  }
  if (incoming_->IsWasmFunctionCall()) {
    DCHECK(IsTaggedReg(loc, kWasmInstanceRegister));
    return LinkageLocation::ForCalleeFrameSlot(kWasmInstanceSlot,
                                               MachineType::AnyTagged());
  }
  UNREACHABLE();
  return LinkageLocation::ForCalleeFrameSlot(0, MachineType::AnyTagged());
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
