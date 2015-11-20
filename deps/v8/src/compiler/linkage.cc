// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"
#include "src/compiler.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/frame.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "src/compiler/osr.h"
#include "src/compiler/pipeline.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
LinkageLocation regloc(Register reg) {
  return LinkageLocation::ForRegister(Register::ToAllocationIndex(reg));
}


MachineType reptyp(Representation representation) {
  switch (representation.kind()) {
    case Representation::kInteger8:
      return kMachInt8;
    case Representation::kUInteger8:
      return kMachUint8;
    case Representation::kInteger16:
      return kMachInt16;
    case Representation::kUInteger16:
      return kMachUint16;
    case Representation::kInteger32:
      return kMachInt32;
    case Representation::kSmi:
    case Representation::kTagged:
    case Representation::kHeapObject:
      return kMachAnyTagged;
    case Representation::kDouble:
      return kMachFloat64;
    case Representation::kExternal:
      return kMachPtr;
    case Representation::kNone:
    case Representation::kNumRepresentations:
      break;
  }
  UNREACHABLE();
  return kMachNone;
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
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, const CallDescriptor& d) {
  // TODO(svenpanne) Output properties etc. and be less cryptic.
  return os << d.kind() << ":" << d.debug_name() << ":r" << d.ReturnCount()
            << "s" << d.StackParameterCount() << "i" << d.InputCount() << "f"
            << d.FrameStateCount() << "t" << d.SupportsTailCalls();
}


bool CallDescriptor::HasSameReturnLocationsAs(
    const CallDescriptor* other) const {
  if (ReturnCount() != other->ReturnCount()) return false;
  for (size_t i = 0; i < ReturnCount(); ++i) {
    if (GetReturnLocation(i) != other->GetReturnLocation(i)) return false;
  }
  return true;
}


bool CallDescriptor::CanTailCall(const Node* node) const {
  // Determine the number of stack parameters passed in
  size_t stack_params = 0;
  for (size_t i = 0; i < InputCount(); ++i) {
    if (!GetInputLocation(i).IsRegister()) {
      ++stack_params;
    }
  }
  // Ensure the input linkage contains the stack parameters in the right order
  size_t current_stack_param = 0;
  for (size_t i = 0; i < InputCount(); ++i) {
    if (!GetInputLocation(i).IsRegister()) {
      if (GetInputLocation(i) != LinkageLocation::ForCallerFrameSlot(
                                     static_cast<int>(current_stack_param) -
                                     static_cast<int>(stack_params))) {
        return false;
      }
      ++current_stack_param;
    }
  }
  // Tail calling is currently allowed if return locations match and all
  // parameters are either in registers or on the stack but match exactly in
  // number and content.
  CallDescriptor const* other = OpParameter<CallDescriptor const*>(node);
  if (!HasSameReturnLocationsAs(other)) return false;
  size_t current_input = 0;
  size_t other_input = 0;
  while (true) {
    if (other_input >= other->InputCount()) {
      while (current_input < InputCount()) {
        if (!GetInputLocation(current_input).IsRegister()) {
          return false;
        }
        ++current_input;
      }
      return true;
    }
    if (current_input >= InputCount()) {
      while (other_input < other->InputCount()) {
        if (!other->GetInputLocation(other_input).IsRegister()) {
          return false;
        }
        ++other_input;
      }
      return true;
    }
    if (GetInputLocation(current_input).IsRegister()) {
      ++current_input;
      continue;
    }
    if (other->GetInputLocation(other_input).IsRegister()) {
      ++other_input;
      continue;
    }
    if (GetInputLocation(current_input) !=
        other->GetInputLocation(other_input)) {
      return false;
    }
    Node* input = node->InputAt(static_cast<int>(other_input));
    if (input->opcode() != IrOpcode::kParameter) {
      return false;
    }
    // Make sure that the parameter input passed through to the tail call
    // corresponds to the correct stack slot.
    size_t param_index = ParameterIndexOf(input->op());
    if (param_index != current_input - 1) {
      return false;
    }
    ++current_input;
    ++other_input;
  }
  UNREACHABLE();
  return false;
}


CallDescriptor* Linkage::ComputeIncoming(Zone* zone, CompilationInfo* info) {
  if (info->code_stub() != NULL) {
    // Use the code stub interface descriptor.
    CodeStub* stub = info->code_stub();
    CallInterfaceDescriptor descriptor = stub->GetCallInterfaceDescriptor();
    return GetStubCallDescriptor(
        info->isolate(), zone, descriptor, stub->GetStackParameterCount(),
        CallDescriptor::kNoFlags, Operator::kNoProperties);
  }
  if (info->has_literal()) {
    // If we already have the function literal, use the number of parameters
    // plus the receiver.
    return GetJSCallDescriptor(zone, info->is_osr(),
                               1 + info->literal()->parameter_count(),
                               CallDescriptor::kNoFlags);
  }
  if (!info->closure().is_null()) {
    // If we are compiling a JS function, use a JS call descriptor,
    // plus the receiver.
    SharedFunctionInfo* shared = info->closure()->shared();
    return GetJSCallDescriptor(zone, info->is_osr(),
                               1 + shared->internal_formal_parameter_count(),
                               CallDescriptor::kNoFlags);
  }
  return NULL;  // TODO(titzer): ?
}


FrameOffset Linkage::GetFrameOffset(int spill_slot, Frame* frame) const {
  bool has_frame = frame->GetSpillSlotCount() > 0 ||
                   incoming_->IsJSFunctionCall() ||
                   incoming_->kind() == CallDescriptor::kCallAddress;
  const int offset =
      (StandardFrameConstants::kFixedSlotCountAboveFp - spill_slot - 1) *
      kPointerSize;
  if (has_frame) {
    return FrameOffset::FromFramePointer(offset);
  } else {
    // No frame. Retrieve all parameters relative to stack pointer.
    DCHECK(spill_slot < 0);  // Must be a parameter.
    int offsetSpToFp =
        kPointerSize * (StandardFrameConstants::kFixedSlotCountAboveFp -
                        frame->GetTotalFrameSlotCount());
    return FrameOffset::FromStackPointer(offset - offsetSpToFp);
  }
}


// static
int Linkage::FrameStateInputCount(Runtime::FunctionId function) {
  // Most runtime functions need a FrameState. A few chosen ones that we know
  // not to call into arbitrary JavaScript, not to throw, and not to deoptimize
  // are blacklisted here and can be called without a FrameState.
  switch (function) {
    case Runtime::kAllocateInTargetSpace:
    case Runtime::kDateField:
    case Runtime::kDefineClassMethod:              // TODO(jarin): Is it safe?
    case Runtime::kDefineGetterPropertyUnchecked:  // TODO(jarin): Is it safe?
    case Runtime::kDefineSetterPropertyUnchecked:  // TODO(jarin): Is it safe?
    case Runtime::kFinalizeClassDefinition:        // TODO(conradw): Is it safe?
    case Runtime::kForInDone:
    case Runtime::kForInStep:
    case Runtime::kGetOriginalConstructor:
    case Runtime::kNewArguments:
    case Runtime::kNewClosure:
    case Runtime::kNewFunctionContext:
    case Runtime::kNewRestParamSlow:
    case Runtime::kPushBlockContext:
    case Runtime::kPushCatchContext:
    case Runtime::kReThrow:
    case Runtime::kStringCompare:
    case Runtime::kStringEquals:
    case Runtime::kToFastProperties:  // TODO(jarin): Is it safe?
    case Runtime::kTraceEnter:
    case Runtime::kTraceExit:
      return 0;
    case Runtime::kInlineArguments:
    case Runtime::kInlineCallFunction:
    case Runtime::kInlineDefaultConstructorCallSuper:
    case Runtime::kInlineGetCallerJSFunction:
    case Runtime::kInlineGetPrototype:
    case Runtime::kInlineRegExpExec:
    case Runtime::kInlineSubString:
    case Runtime::kInlineToObject:
      return 1;
    case Runtime::kInlineDeoptimizeNow:
    case Runtime::kInlineThrowNotDateError:
      return 2;
    default:
      break;
  }

  // Most inlined runtime functions (except the ones listed above) can be called
  // without a FrameState or will be lowered by JSIntrinsicLowering internally.
  const Runtime::Function* const f = Runtime::FunctionForId(function);
  if (f->intrinsic_type == Runtime::IntrinsicType::INLINE) return 0;

  return 1;
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
    Operator::Properties properties) {
  const size_t function_count = 1;
  const size_t num_args_count = 1;
  const size_t context_count = 1;
  const size_t parameter_count = function_count +
                                 static_cast<size_t>(js_parameter_count) +
                                 num_args_count + context_count;

  const Runtime::Function* function = Runtime::FunctionForId(function_id);
  const size_t return_count = static_cast<size_t>(function->result_size);

  LocationSignature::Builder locations(zone, return_count, parameter_count);
  MachineSignature::Builder types(zone, return_count, parameter_count);

  // Add returns.
  if (locations.return_count_ > 0) {
    locations.AddReturn(regloc(kReturnRegister0));
  }
  if (locations.return_count_ > 1) {
    locations.AddReturn(regloc(kReturnRegister1));
  }
  for (size_t i = 0; i < return_count; i++) {
    types.AddReturn(kMachAnyTagged);
  }

  // All parameters to the runtime call go on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    locations.AddParam(
        LinkageLocation::ForCallerFrameSlot(i - js_parameter_count));
    types.AddParam(kMachAnyTagged);
  }
  // Add runtime function itself.
  locations.AddParam(regloc(kRuntimeCallFunctionRegister));
  types.AddParam(kMachAnyTagged);

  // Add runtime call argument count.
  locations.AddParam(regloc(kRuntimeCallArgCountRegister));
  types.AddParam(kMachPtr);

  // Add context.
  locations.AddParam(regloc(kContextRegister));
  types.AddParam(kMachAnyTagged);

  CallDescriptor::Flags flags = Linkage::FrameStateInputCount(function_id) > 0
                                    ? CallDescriptor::kNeedsFrameState
                                    : CallDescriptor::kNoFlags;

  // The target for runtime calls is a code object.
  MachineType target_type = kMachAnyTagged;
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister();
  return new (zone) CallDescriptor(     // --
      CallDescriptor::kCallCodeObject,  // kind
      target_type,                      // target MachineType
      target_loc,                       // target location
      types.Build(),                    // machine_sig
      locations.Build(),                // location_sig
      js_parameter_count,               // stack_parameter_count
      properties,                       // properties
      kNoCalleeSaved,                   // callee-saved
      kNoCalleeSaved,                   // callee-saved fp
      flags,                            // flags
      function->name);                  // debug name
}


CallDescriptor* Linkage::GetJSCallDescriptor(Zone* zone, bool is_osr,
                                             int js_parameter_count,
                                             CallDescriptor::Flags flags) {
  const size_t return_count = 1;
  const size_t context_count = 1;
  const size_t parameter_count = js_parameter_count + context_count;

  LocationSignature::Builder locations(zone, return_count, parameter_count);
  MachineSignature::Builder types(zone, return_count, parameter_count);

  // All JS calls have exactly one return value.
  locations.AddReturn(regloc(kReturnRegister0));
  types.AddReturn(kMachAnyTagged);

  // All parameters to JS calls go on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    int spill_slot_index = i - js_parameter_count;
    locations.AddParam(LinkageLocation::ForCallerFrameSlot(spill_slot_index));
    types.AddParam(kMachAnyTagged);
  }
  // Add context.
  locations.AddParam(regloc(kContextRegister));
  types.AddParam(kMachAnyTagged);

  // The target for JS function calls is the JSFunction object.
  MachineType target_type = kMachAnyTagged;
  // TODO(titzer): When entering into an OSR function from unoptimized code,
  // the JSFunction is not in a register, but it is on the stack in an
  // unaddressable spill slot. We hack this in the OSR prologue. Fix.
  LinkageLocation target_loc = regloc(kJSFunctionRegister);
  return new (zone) CallDescriptor(     // --
      CallDescriptor::kCallJSFunction,  // kind
      target_type,                      // target MachineType
      target_loc,                       // target location
      types.Build(),                    // machine_sig
      locations.Build(),                // location_sig
      js_parameter_count,               // stack_parameter_count
      Operator::kNoProperties,          // properties
      kNoCalleeSaved,                   // callee-saved
      kNoCalleeSaved,                   // callee-saved fp
      CallDescriptor::kCanUseRoots |    // flags
          flags,                        // flags
      "js-call");
}


CallDescriptor* Linkage::GetInterpreterDispatchDescriptor(Zone* zone) {
  MachineSignature::Builder types(zone, 0, 5);
  LocationSignature::Builder locations(zone, 0, 5);

  // Add registers for fixed parameters passed via interpreter dispatch.
  STATIC_ASSERT(0 == Linkage::kInterpreterAccumulatorParameter);
  types.AddParam(kMachAnyTagged);
  locations.AddParam(regloc(kInterpreterAccumulatorRegister));

  STATIC_ASSERT(1 == Linkage::kInterpreterRegisterFileParameter);
  types.AddParam(kMachPtr);
  locations.AddParam(regloc(kInterpreterRegisterFileRegister));

  STATIC_ASSERT(2 == Linkage::kInterpreterBytecodeOffsetParameter);
  types.AddParam(kMachIntPtr);
  locations.AddParam(regloc(kInterpreterBytecodeOffsetRegister));

  STATIC_ASSERT(3 == Linkage::kInterpreterBytecodeArrayParameter);
  types.AddParam(kMachAnyTagged);
  locations.AddParam(regloc(kInterpreterBytecodeArrayRegister));

  STATIC_ASSERT(4 == Linkage::kInterpreterDispatchTableParameter);
  types.AddParam(kMachPtr);
  locations.AddParam(regloc(kInterpreterDispatchTableRegister));

  LinkageLocation target_loc = LinkageLocation::ForAnyRegister();
  return new (zone) CallDescriptor(         // --
      CallDescriptor::kCallCodeObject,      // kind
      kMachNone,                            // target MachineType
      target_loc,                           // target location
      types.Build(),                        // machine_sig
      locations.Build(),                    // location_sig
      0,                                    // stack_parameter_count
      Operator::kNoProperties,              // properties
      kNoCalleeSaved,                       // callee-saved registers
      kNoCalleeSaved,                       // callee-saved fp regs
      CallDescriptor::kSupportsTailCalls |  // flags
          CallDescriptor::kCanUseRoots,     // flags
      "interpreter-dispatch");
}


// TODO(all): Add support for return representations/locations to
// CallInterfaceDescriptor.
// TODO(turbofan): cache call descriptors for code stub calls.
CallDescriptor* Linkage::GetStubCallDescriptor(
    Isolate* isolate, Zone* zone, const CallInterfaceDescriptor& descriptor,
    int stack_parameter_count, CallDescriptor::Flags flags,
    Operator::Properties properties, MachineType return_type) {
  const int register_parameter_count = descriptor.GetRegisterParameterCount();
  const int js_parameter_count =
      register_parameter_count + stack_parameter_count;
  const int context_count = 1;
  const size_t return_count = 1;
  const size_t parameter_count =
      static_cast<size_t>(js_parameter_count + context_count);

  LocationSignature::Builder locations(zone, return_count, parameter_count);
  MachineSignature::Builder types(zone, return_count, parameter_count);

  // Add return location.
  locations.AddReturn(regloc(kReturnRegister0));
  types.AddReturn(return_type);

  // Add parameters in registers and on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    if (i < register_parameter_count) {
      // The first parameters go in registers.
      Register reg = descriptor.GetRegisterParameter(i);
      Representation rep =
          RepresentationFromType(descriptor.GetParameterType(i));
      locations.AddParam(regloc(reg));
      types.AddParam(reptyp(rep));
    } else {
      // The rest of the parameters go on the stack.
      int stack_slot = i - register_parameter_count - stack_parameter_count;
      locations.AddParam(LinkageLocation::ForCallerFrameSlot(stack_slot));
      types.AddParam(kMachAnyTagged);
    }
  }
  // Add context.
  locations.AddParam(regloc(kContextRegister));
  types.AddParam(kMachAnyTagged);

  // The target for stub calls is a code object.
  MachineType target_type = kMachAnyTagged;
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister();
  return new (zone) CallDescriptor(     // --
      CallDescriptor::kCallCodeObject,  // kind
      target_type,                      // target MachineType
      target_loc,                       // target location
      types.Build(),                    // machine_sig
      locations.Build(),                // location_sig
      stack_parameter_count,            // stack_parameter_count
      properties,                       // properties
      kNoCalleeSaved,                   // callee-saved registers
      kNoCalleeSaved,                   // callee-saved fp
      flags,                            // flags
      descriptor.DebugName(isolate));
}


LinkageLocation Linkage::GetOsrValueLocation(int index) const {
  CHECK(incoming_->IsJSFunctionCall());
  int parameter_count = static_cast<int>(incoming_->JSParameterCount() - 1);
  int first_stack_slot = OsrHelper::FirstStackSlotIndex(parameter_count);

  if (index == kOsrContextSpillSlotIndex) {
    // Context. Use the parameter location of the context spill slot.
    // Parameter (arity + 1) is special for the context of the function frame.
    int context_index = 1 + 1 + parameter_count;  // target + receiver + params
    return incoming_->GetInputLocation(context_index);
  } else if (index >= first_stack_slot) {
    // Local variable stored in this (callee) stack.
    int spill_index =
        index - first_stack_slot + StandardFrameConstants::kFixedSlotCount;
    return LinkageLocation::ForCalleeFrameSlot(spill_index);
  } else {
    // Parameter. Use the assigned location from the incoming call descriptor.
    int parameter_index = 1 + index;  // skip index 0, which is the target.
    return incoming_->GetInputLocation(parameter_index);
  }
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
