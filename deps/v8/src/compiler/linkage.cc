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
  if (info->function() != NULL) {
    // If we already have the function literal, use the number of parameters
    // plus the receiver.
    return GetJSCallDescriptor(1 + info->function()->parameter_count(), zone,
                               CallDescriptor::kNoFlags);
  }
  if (!info->closure().is_null()) {
    // If we are compiling a JS function, use a JS call descriptor,
    // plus the receiver.
    SharedFunctionInfo* shared = info->closure()->shared();
    return GetJSCallDescriptor(1 + shared->formal_parameter_count(), zone,
                               CallDescriptor::kNoFlags);
  }
  if (info->code_stub() != NULL) {
    // Use the code stub interface descriptor.
    CallInterfaceDescriptor descriptor =
        info->code_stub()->GetCallInterfaceDescriptor();
    return GetStubCallDescriptor(descriptor, 0, CallDescriptor::kNoFlags, zone);
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


CallDescriptor* Linkage::GetJSCallDescriptor(
    int parameter_count, CallDescriptor::Flags flags) const {
  return GetJSCallDescriptor(parameter_count, zone_, flags);
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Runtime::FunctionId function, int parameter_count,
    Operator::Properties properties) const {
  return GetRuntimeCallDescriptor(function, parameter_count, properties, zone_);
}


CallDescriptor* Linkage::GetStubCallDescriptor(
    const CallInterfaceDescriptor& descriptor, int stack_parameter_count,
    CallDescriptor::Flags flags) const {
  return GetStubCallDescriptor(descriptor, stack_parameter_count, flags, zone_);
}


// static
bool Linkage::NeedsFrameState(Runtime::FunctionId function) {
  if (!FLAG_turbo_deoptimization) {
    return false;
  }
  // TODO(jarin) At the moment, we only add frame state for
  // few chosen runtime functions.
  switch (function) {
    case Runtime::kApply:
    case Runtime::kArrayBufferNeuter:
    case Runtime::kArrayConcat:
    case Runtime::kBasicJSONStringify:
    case Runtime::kCheckExecutionState:
    case Runtime::kCollectStackTrace:
    case Runtime::kCompileLazy:
    case Runtime::kCompileOptimized:
    case Runtime::kCompileString:
    case Runtime::kCreateObjectLiteral:
    case Runtime::kDebugBreak:
    case Runtime::kDataViewSetInt8:
    case Runtime::kDataViewSetUint8:
    case Runtime::kDataViewSetInt16:
    case Runtime::kDataViewSetUint16:
    case Runtime::kDataViewSetInt32:
    case Runtime::kDataViewSetUint32:
    case Runtime::kDataViewSetFloat32:
    case Runtime::kDataViewSetFloat64:
    case Runtime::kDataViewGetInt8:
    case Runtime::kDataViewGetUint8:
    case Runtime::kDataViewGetInt16:
    case Runtime::kDataViewGetUint16:
    case Runtime::kDataViewGetInt32:
    case Runtime::kDataViewGetUint32:
    case Runtime::kDataViewGetFloat32:
    case Runtime::kDataViewGetFloat64:
    case Runtime::kDebugEvaluate:
    case Runtime::kDebugEvaluateGlobal:
    case Runtime::kDebugGetLoadedScripts:
    case Runtime::kDebugGetPropertyDetails:
    case Runtime::kDebugPromiseEvent:
    case Runtime::kDefineAccessorPropertyUnchecked:
    case Runtime::kDefineDataPropertyUnchecked:
    case Runtime::kDeleteProperty:
    case Runtime::kDeoptimizeFunction:
    case Runtime::kFunctionBindArguments:
    case Runtime::kGetDefaultReceiver:
    case Runtime::kGetFrameCount:
    case Runtime::kGetOwnProperty:
    case Runtime::kGetOwnPropertyNames:
    case Runtime::kGetPropertyNamesFast:
    case Runtime::kGetPrototype:
    case Runtime::kInlineArguments:
    case Runtime::kInlineCallFunction:
    case Runtime::kInlineDateField:
    case Runtime::kInlineRegExpExec:
    case Runtime::kInternalSetPrototype:
    case Runtime::kInterrupt:
    case Runtime::kIsPropertyEnumerable:
    case Runtime::kIsSloppyModeFunction:
    case Runtime::kLiveEditGatherCompileInfo:
    case Runtime::kLoadLookupSlot:
    case Runtime::kLoadLookupSlotNoReferenceError:
    case Runtime::kMaterializeRegExpLiteral:
    case Runtime::kNewObject:
    case Runtime::kNewObjectFromBound:
    case Runtime::kNewObjectWithAllocationSite:
    case Runtime::kObjectFreeze:
    case Runtime::kOwnKeys:
    case Runtime::kParseJson:
    case Runtime::kPrepareStep:
    case Runtime::kPreventExtensions:
    case Runtime::kPromiseRejectEvent:
    case Runtime::kPromiseRevokeReject:
    case Runtime::kRegExpCompile:
    case Runtime::kRegExpExecMultiple:
    case Runtime::kResolvePossiblyDirectEval:
    case Runtime::kSetPrototype:
    case Runtime::kSetScriptBreakPoint:
    case Runtime::kSparseJoinWithSeparator:
    case Runtime::kStackGuard:
    case Runtime::kStoreKeyedToSuper_Sloppy:
    case Runtime::kStoreKeyedToSuper_Strict:
    case Runtime::kStoreToSuper_Sloppy:
    case Runtime::kStoreToSuper_Strict:
    case Runtime::kStoreLookupSlot:
    case Runtime::kStringBuilderConcat:
    case Runtime::kStringBuilderJoin:
    case Runtime::kStringMatch:
    case Runtime::kStringReplaceGlobalRegExpWithString:
    case Runtime::kThrowNonMethodError:
    case Runtime::kThrowNotDateError:
    case Runtime::kThrowReferenceError:
    case Runtime::kThrowUnsupportedSuperError:
    case Runtime::kThrow:
    case Runtime::kTypedArraySetFastCases:
    case Runtime::kTypedArrayInitializeFromArrayLike:
#ifdef V8_I18N_SUPPORT
    case Runtime::kGetImplFromInitializedIntlObject:
#endif
      return true;
    default:
      return false;
  }
}


//==============================================================================
// Provide unimplemented methods on unsupported architectures, to at least link.
//==============================================================================
#if !V8_TURBOFAN_BACKEND
CallDescriptor* Linkage::GetJSCallDescriptor(int parameter_count, Zone* zone,
                                             CallDescriptor::Flags flags) {
  UNIMPLEMENTED();
  return NULL;
}


CallDescriptor* Linkage::GetRuntimeCallDescriptor(
    Runtime::FunctionId function, int parameter_count,
    Operator::Properties properties, Zone* zone) {
  UNIMPLEMENTED();
  return NULL;
}


CallDescriptor* Linkage::GetStubCallDescriptor(
    const CallInterfaceDescriptor& descriptor, int stack_parameter_count,
    CallDescriptor::Flags flags, Zone* zone) {
  UNIMPLEMENTED();
  return NULL;
}


CallDescriptor* Linkage::GetSimplifiedCDescriptor(Zone* zone,
                                                  MachineSignature* sig) {
  UNIMPLEMENTED();
  return NULL;
}
#endif  // !V8_TURBOFAN_BACKEND
}
}
}  // namespace v8::internal::compiler
