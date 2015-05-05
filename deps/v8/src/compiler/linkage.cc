// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"
#include "src/compiler.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "src/compiler/pipeline.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {


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
            << "j" << d.JSParameterCount() << "i" << d.InputCount() << "f"
            << d.FrameStateCount();
}


CallDescriptor* Linkage::ComputeIncoming(Zone* zone, CompilationInfo* info) {
  if (info->code_stub() != NULL) {
    // Use the code stub interface descriptor.
    CallInterfaceDescriptor descriptor =
        info->code_stub()->GetCallInterfaceDescriptor();
    return GetStubCallDescriptor(info->isolate(), zone, descriptor, 0,
                                 CallDescriptor::kNoFlags,
                                 Operator::kNoProperties);
  }
  if (info->function() != NULL) {
    // If we already have the function literal, use the number of parameters
    // plus the receiver.
    return GetJSCallDescriptor(zone, info->is_osr(),
                               1 + info->function()->parameter_count(),
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


FrameOffset Linkage::GetFrameOffset(int spill_slot, Frame* frame,
                                    int extra) const {
  if (frame->GetSpillSlotCount() > 0 || incoming_->IsJSFunctionCall() ||
      incoming_->kind() == CallDescriptor::kCallAddress) {
    int offset;
    int register_save_area_size = frame->GetRegisterSaveAreaSize();
    if (spill_slot >= 0) {
      // Local or spill slot. Skip the frame pointer, function, and
      // context in the fixed part of the frame.
      offset =
          -(spill_slot + 1) * kPointerSize - register_save_area_size + extra;
    } else {
      // Incoming parameter. Skip the return address.
      offset = -(spill_slot + 1) * kPointerSize + kFPOnStackSize +
               kPCOnStackSize + extra;
    }
    return FrameOffset::FromFramePointer(offset);
  } else {
    // No frame. Retrieve all parameters relative to stack pointer.
    DCHECK(spill_slot < 0);  // Must be a parameter.
    int register_save_area_size = frame->GetRegisterSaveAreaSize();
    int offset = register_save_area_size - (spill_slot + 1) * kPointerSize +
                 kPCOnStackSize + extra;
    return FrameOffset::FromStackPointer(offset);
  }
}


// static
bool Linkage::NeedsFrameState(Runtime::FunctionId function) {
  if (!FLAG_turbo_deoptimization) {
    return false;
  }

  // Most runtime functions need a FrameState. A few chosen ones that we know
  // not to call into arbitrary JavaScript, not to throw, and not to deoptimize
  // are blacklisted here and can be called without a FrameState.
  switch (function) {
    case Runtime::kDeclareGlobals:                 // TODO(jarin): Is it safe?
    case Runtime::kDefineClassMethod:              // TODO(jarin): Is it safe?
    case Runtime::kDefineGetterPropertyUnchecked:  // TODO(jarin): Is it safe?
    case Runtime::kDefineSetterPropertyUnchecked:  // TODO(jarin): Is it safe?
    case Runtime::kForInCacheArrayLength:
    case Runtime::kForInInit:
    case Runtime::kForInNext:
    case Runtime::kNewArguments:
    case Runtime::kNewClosure:
    case Runtime::kNewFunctionContext:
    case Runtime::kNewRestParamSlow:
    case Runtime::kPushBlockContext:
    case Runtime::kPushCatchContext:
    case Runtime::kReThrow:
    case Runtime::kSetProperty:  // TODO(jarin): Is it safe?
    case Runtime::kStringCompareRT:
    case Runtime::kStringEquals:
    case Runtime::kToFastProperties:  // TODO(jarin): Is it safe?
    case Runtime::kTraceEnter:
    case Runtime::kTraceExit:
    case Runtime::kTypeof:
      return false;
    case Runtime::kInlineArguments:
    case Runtime::kInlineCallFunction:
    case Runtime::kInlineDateField:
    case Runtime::kInlineDeoptimizeNow:
    case Runtime::kInlineGetPrototype:
    case Runtime::kInlineRegExpExec:
      return true;
    default:
      break;
  }

  // Most inlined runtime functions (except the ones listed above) can be called
  // without a FrameState or will be lowered by JSIntrinsicLowering internally.
  const Runtime::Function* const f = Runtime::FunctionForId(function);
  if (f->intrinsic_type == Runtime::IntrinsicType::INLINE) return false;

  return true;
}


//==============================================================================
// Provide unimplemented methods on unsupported architectures, to at least link.
//==============================================================================
#if !V8_TURBOFAN_BACKEND
CallDescriptor* Linkage::GetJSCallDescriptor(Zone* zone, bool is_osr,
                                             int parameter_count,
                                             CallDescriptor::Flags flags) {
  UNIMPLEMENTED();
  return NULL;
}


LinkageLocation Linkage::GetOsrValueLocation(int index) const {
  UNIMPLEMENTED();
  return LinkageLocation(-1);  // Dummy value
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Zone* zone, Runtime::FunctionId function, int parameter_count,
    Operator::Properties properties) {
  UNIMPLEMENTED();
  return NULL;
}


CallDescriptor* Linkage::GetStubCallDescriptor(
    Isolate* isolate, Zone* zone, const CallInterfaceDescriptor& descriptor,
    int stack_parameter_count, CallDescriptor::Flags flags,
    Operator::Properties properties, MachineType return_type) {
  UNIMPLEMENTED();
  return NULL;
}


CallDescriptor* Linkage::GetSimplifiedCDescriptor(Zone* zone,
                                                  const MachineSignature* sig) {
  UNIMPLEMENTED();
  return NULL;
}
#endif  // !V8_TURBOFAN_BACKEND
}
}
}  // namespace v8::internal::compiler
