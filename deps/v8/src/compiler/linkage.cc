// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/scopes.h"
#include "src/code-stubs.h"
#include "src/compiler.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/frame.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "src/compiler/osr.h"
#include "src/compiler/pipeline.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
LinkageLocation regloc(Register reg) {
  return LinkageLocation::ForRegister(reg.code());
}


MachineType reptyp(Representation representation) {
  switch (representation.kind()) {
    case Representation::kInteger8:
      return MachineType::Int8();
    case Representation::kUInteger8:
      return MachineType::Uint8();
    case Representation::kInteger16:
      return MachineType::Int16();
    case Representation::kUInteger16:
      return MachineType::Uint16();
    case Representation::kInteger32:
      return MachineType::Int32();
    case Representation::kSmi:
    case Representation::kTagged:
    case Representation::kHeapObject:
      return MachineType::AnyTagged();
    case Representation::kDouble:
      return MachineType::Float64();
    case Representation::kExternal:
      return MachineType::Pointer();
    case Representation::kNone:
    case Representation::kNumRepresentations:
      break;
  }
  UNREACHABLE();
  return MachineType::None();
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


bool CallDescriptor::CanTailCall(const Node* node,
                                 int* stack_param_delta) const {
  CallDescriptor const* other = OpParameter<CallDescriptor const*>(node);
  size_t current_input = 0;
  size_t other_input = 0;
  *stack_param_delta = 0;
  bool more_other = true;
  bool more_this = true;
  while (more_other || more_this) {
    if (other_input < other->InputCount()) {
      if (!other->GetInputLocation(other_input).IsRegister()) {
        (*stack_param_delta)--;
      }
    } else {
      more_other = false;
    }
    if (current_input < InputCount()) {
      if (!GetInputLocation(current_input).IsRegister()) {
        (*stack_param_delta)++;
      }
    } else {
      more_this = false;
    }
    ++current_input;
    ++other_input;
  }
  return HasSameReturnLocationsAs(OpParameter<CallDescriptor const*>(node));
}


CallDescriptor* Linkage::ComputeIncoming(Zone* zone, CompilationInfo* info) {
  DCHECK(!info->IsStub());
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
  return nullptr;  // TODO(titzer): ?
}


// static
int Linkage::FrameStateInputCount(Runtime::FunctionId function) {
  // Most runtime functions need a FrameState. A few chosen ones that we know
  // not to call into arbitrary JavaScript, not to throw, and not to deoptimize
  // are blacklisted here and can be called without a FrameState.
  switch (function) {
    case Runtime::kAllocateInTargetSpace:
    case Runtime::kCreateIterResultObject:
    case Runtime::kDefineDataPropertyInLiteral:
    case Runtime::kDefineGetterPropertyUnchecked:  // TODO(jarin): Is it safe?
    case Runtime::kDefineSetterPropertyUnchecked:  // TODO(jarin): Is it safe?
    case Runtime::kFinalizeClassDefinition:        // TODO(conradw): Is it safe?
    case Runtime::kForInDone:
    case Runtime::kForInStep:
    case Runtime::kGetSuperConstructor:
    case Runtime::kIsFunction:
    case Runtime::kNewClosure:
    case Runtime::kNewClosure_Tenured:
    case Runtime::kNewFunctionContext:
    case Runtime::kPushBlockContext:
    case Runtime::kPushCatchContext:
    case Runtime::kReThrow:
    case Runtime::kStringCompare:
    case Runtime::kStringEquals:
    case Runtime::kToFastProperties:  // TODO(jarin): Is it safe?
    case Runtime::kTraceEnter:
    case Runtime::kTraceExit:
      return 0;
    case Runtime::kInlineGetPrototype:
    case Runtime::kInlineRegExpConstructResult:
    case Runtime::kInlineRegExpExec:
    case Runtime::kInlineSubString:
    case Runtime::kInlineToInteger:
    case Runtime::kInlineToLength:
    case Runtime::kInlineToName:
    case Runtime::kInlineToNumber:
    case Runtime::kInlineToObject:
    case Runtime::kInlineToPrimitive_Number:
    case Runtime::kInlineToPrimitive_String:
    case Runtime::kInlineToPrimitive:
    case Runtime::kInlineToString:
      return 1;
    case Runtime::kInlineCall:
    case Runtime::kInlineTailCall:
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
    Operator::Properties properties, CallDescriptor::Flags flags) {
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
  if (locations.return_count_ > 2) {
    locations.AddReturn(regloc(kReturnRegister2));
  }
  for (size_t i = 0; i < return_count; i++) {
    types.AddReturn(MachineType::AnyTagged());
  }

  // All parameters to the runtime call go on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    locations.AddParam(
        LinkageLocation::ForCallerFrameSlot(i - js_parameter_count));
    types.AddParam(MachineType::AnyTagged());
  }
  // Add runtime function itself.
  locations.AddParam(regloc(kRuntimeCallFunctionRegister));
  types.AddParam(MachineType::AnyTagged());

  // Add runtime call argument count.
  locations.AddParam(regloc(kRuntimeCallArgCountRegister));
  types.AddParam(MachineType::Pointer());

  // Add context.
  locations.AddParam(regloc(kContextRegister));
  types.AddParam(MachineType::AnyTagged());

  if (Linkage::FrameStateInputCount(function_id) == 0) {
    flags = static_cast<CallDescriptor::Flags>(
        flags & ~CallDescriptor::kNeedsFrameState);
  }

  // The target for runtime calls is a code object.
  MachineType target_type = MachineType::AnyTagged();
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
  const size_t new_target_count = 1;
  const size_t num_args_count = 1;
  const size_t parameter_count =
      js_parameter_count + new_target_count + num_args_count + context_count;

  LocationSignature::Builder locations(zone, return_count, parameter_count);
  MachineSignature::Builder types(zone, return_count, parameter_count);

  // All JS calls have exactly one return value.
  locations.AddReturn(regloc(kReturnRegister0));
  types.AddReturn(MachineType::AnyTagged());

  // All parameters to JS calls go on the stack.
  for (int i = 0; i < js_parameter_count; i++) {
    int spill_slot_index = i - js_parameter_count;
    locations.AddParam(LinkageLocation::ForCallerFrameSlot(spill_slot_index));
    types.AddParam(MachineType::AnyTagged());
  }

  // Add JavaScript call new target value.
  locations.AddParam(regloc(kJavaScriptCallNewTargetRegister));
  types.AddParam(MachineType::AnyTagged());

  // Add JavaScript call argument count.
  locations.AddParam(regloc(kJavaScriptCallArgCountRegister));
  types.AddParam(MachineType::Int32());

  // Add context.
  locations.AddParam(regloc(kContextRegister));
  types.AddParam(MachineType::AnyTagged());

  // The target for JS function calls is the JSFunction object.
  MachineType target_type = MachineType::AnyTagged();
  // When entering into an OSR function from unoptimized code the JSFunction
  // is not in a register, but it is on the stack in the marker spill slot.
  LinkageLocation target_loc = is_osr ? LinkageLocation::ForSavedCallerMarker()
                                      : regloc(kJSFunctionRegister);
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

// TODO(all): Add support for return representations/locations to
// CallInterfaceDescriptor.
// TODO(turbofan): cache call descriptors for code stub calls.
CallDescriptor* Linkage::GetStubCallDescriptor(
    Isolate* isolate, Zone* zone, const CallInterfaceDescriptor& descriptor,
    int stack_parameter_count, CallDescriptor::Flags flags,
    Operator::Properties properties, MachineType return_type,
    size_t return_count) {
  const int register_parameter_count = descriptor.GetRegisterParameterCount();
  const int js_parameter_count =
      register_parameter_count + stack_parameter_count;
  const int context_count = 1;
  const size_t parameter_count =
      static_cast<size_t>(js_parameter_count + context_count);

  LocationSignature::Builder locations(zone, return_count, parameter_count);
  MachineSignature::Builder types(zone, return_count, parameter_count);

  // Add returns.
  if (locations.return_count_ > 0) {
    locations.AddReturn(regloc(kReturnRegister0));
  }
  if (locations.return_count_ > 1) {
    locations.AddReturn(regloc(kReturnRegister1));
  }
  if (locations.return_count_ > 2) {
    locations.AddReturn(regloc(kReturnRegister2));
  }
  for (size_t i = 0; i < return_count; i++) {
    types.AddReturn(return_type);
  }

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
      types.AddParam(MachineType::AnyTagged());
    }
  }
  // Add context.
  locations.AddParam(regloc(kContextRegister));
  types.AddParam(MachineType::AnyTagged());

  // The target for stub calls is a code object.
  MachineType target_type = MachineType::AnyTagged();
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
    // Parameter (arity + 2) is special for the context of the function frame.
    // >> context_index = target + receiver + params + new_target + #args
    int context_index = 1 + 1 + parameter_count + 1 + 1;
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


bool Linkage::ParameterHasSecondaryLocation(int index) const {
  if (incoming_->kind() != CallDescriptor::kCallJSFunction) return false;
  LinkageLocation loc = GetParameterLocation(index);
  return (loc == regloc(kJSFunctionRegister) ||
          loc == regloc(kContextRegister));
}

LinkageLocation Linkage::GetParameterSecondaryLocation(int index) const {
  DCHECK(ParameterHasSecondaryLocation(index));
  LinkageLocation loc = GetParameterLocation(index);

  if (loc == regloc(kJSFunctionRegister)) {
    return LinkageLocation::ForCalleeFrameSlot(Frame::kJSFunctionSlot);
  } else {
    DCHECK(loc == regloc(kContextRegister));
    return LinkageLocation::ForCalleeFrameSlot(Frame::kContextSlot);
  }
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
