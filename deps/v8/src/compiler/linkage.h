// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LINKAGE_H_
#define V8_COMPILER_LINKAGE_H_

#include "src/base/flags.h"
#include "src/compiler/frame.h"
#include "src/compiler/operator.h"
#include "src/frames.h"
#include "src/machine-type.h"
#include "src/runtime/runtime.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;
class CompilationInfo;

namespace compiler {

const RegList kNoCalleeSaved = 0;

class Node;
class OsrHelper;

// Describes the location for a parameter or a return value to a call.
class LinkageLocation {
 public:
  bool operator==(const LinkageLocation& other) const {
    return bit_field_ == other.bit_field_;
  }

  bool operator!=(const LinkageLocation& other) const {
    return !(*this == other);
  }

  static LinkageLocation ForAnyRegister() {
    return LinkageLocation(REGISTER, ANY_REGISTER);
  }

  static LinkageLocation ForRegister(int32_t reg) {
    DCHECK(reg >= 0);
    return LinkageLocation(REGISTER, reg);
  }

  static LinkageLocation ForCallerFrameSlot(int32_t slot) {
    DCHECK(slot < 0);
    return LinkageLocation(STACK_SLOT, slot);
  }

  static LinkageLocation ForCalleeFrameSlot(int32_t slot) {
    // TODO(titzer): bailout instead of crashing here.
    DCHECK(slot >= 0 && slot < LinkageLocation::MAX_STACK_SLOT);
    return LinkageLocation(STACK_SLOT, slot);
  }

  static LinkageLocation ForSavedCallerReturnAddress() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kCallerPCOffset) /
                              kPointerSize);
  }

  static LinkageLocation ForSavedCallerFramePtr() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kCallerFPOffset) /
                              kPointerSize);
  }

  static LinkageLocation ForSavedCallerConstantPool() {
    DCHECK(V8_EMBEDDED_CONSTANT_POOL);
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kConstantPoolOffset) /
                              kPointerSize);
  }

  static LinkageLocation ConvertToTailCallerLocation(
      LinkageLocation caller_location, int stack_param_delta) {
    if (!caller_location.IsRegister()) {
      return LinkageLocation(STACK_SLOT,
                             caller_location.GetLocation() - stack_param_delta);
    }
    return caller_location;
  }

 private:
  friend class CallDescriptor;
  friend class OperandGenerator;

  enum LocationType { REGISTER, STACK_SLOT };

  class TypeField : public BitField<LocationType, 0, 1> {};
  class LocationField : public BitField<int32_t, TypeField::kNext, 31> {};

  static const int32_t ANY_REGISTER = -1;
  static const int32_t MAX_STACK_SLOT = 32767;

  LinkageLocation(LocationType type, int32_t location) {
    bit_field_ = TypeField::encode(type) |
                 ((location << LocationField::kShift) & LocationField::kMask);
  }

  int32_t GetLocation() const {
    return static_cast<int32_t>(bit_field_ & LocationField::kMask) >>
           LocationField::kShift;
  }

  bool IsRegister() const { return TypeField::decode(bit_field_) == REGISTER; }
  bool IsAnyRegister() const {
    return IsRegister() && GetLocation() == ANY_REGISTER;
  }
  bool IsCallerFrameSlot() const { return !IsRegister() && GetLocation() < 0; }
  bool IsCalleeFrameSlot() const { return !IsRegister() && GetLocation() >= 0; }

  int32_t AsRegister() const {
    DCHECK(IsRegister());
    return GetLocation();
  }
  int32_t AsCallerFrameSlot() const {
    DCHECK(IsCallerFrameSlot());
    return GetLocation();
  }
  int32_t AsCalleeFrameSlot() const {
    DCHECK(IsCalleeFrameSlot());
    return GetLocation();
  }

  int32_t bit_field_;
};

typedef Signature<LinkageLocation> LocationSignature;

// Describes a call to various parts of the compiler. Every call has the notion
// of a "target", which is the first input to the call.
class CallDescriptor final : public ZoneObject {
 public:
  // Describes the kind of this call, which determines the target.
  enum Kind {
    kCallCodeObject,  // target is a Code object
    kCallJSFunction,  // target is a JSFunction object
    kCallAddress,     // target is a machine pointer
    kLazyBailout      // the call is no-op, only used for lazy bailout
  };

  enum Flag {
    kNoFlags = 0u,
    kNeedsFrameState = 1u << 0,
    kPatchableCallSite = 1u << 1,
    kNeedsNopAfterCall = 1u << 2,
    kHasExceptionHandler = 1u << 3,
    kHasLocalCatchHandler = 1u << 4,
    kSupportsTailCalls = 1u << 5,
    kCanUseRoots = 1u << 6,
    // Indicates that the native stack should be used for a code object. This
    // information is important for native calls on arm64.
    kUseNativeStack = 1u << 7,
    kPatchableCallSiteWithNop = kPatchableCallSite | kNeedsNopAfterCall
  };
  typedef base::Flags<Flag> Flags;

  CallDescriptor(Kind kind, MachineType target_type, LinkageLocation target_loc,
                 const MachineSignature* machine_sig,
                 LocationSignature* location_sig, size_t stack_param_count,
                 Operator::Properties properties,
                 RegList callee_saved_registers,
                 RegList callee_saved_fp_registers, Flags flags,
                 const char* debug_name = "")
      : kind_(kind),
        target_type_(target_type),
        target_loc_(target_loc),
        machine_sig_(machine_sig),
        location_sig_(location_sig),
        stack_param_count_(stack_param_count),
        properties_(properties),
        callee_saved_registers_(callee_saved_registers),
        callee_saved_fp_registers_(callee_saved_fp_registers),
        flags_(flags),
        debug_name_(debug_name) {
    DCHECK(machine_sig->return_count() == location_sig->return_count());
    DCHECK(machine_sig->parameter_count() == location_sig->parameter_count());
  }

  // Returns the kind of this call.
  Kind kind() const { return kind_; }

  // Returns {true} if this descriptor is a call to a C function.
  bool IsCFunctionCall() const { return kind_ == kCallAddress; }

  // Returns {true} if this descriptor is a call to a JSFunction.
  bool IsJSFunctionCall() const { return kind_ == kCallJSFunction; }

  bool RequiresFrameAsIncoming() const {
    return IsCFunctionCall() || IsJSFunctionCall();
  }

  // The number of return values from this call.
  size_t ReturnCount() const { return machine_sig_->return_count(); }

  // The number of C parameters to this call.
  size_t CParameterCount() const { return machine_sig_->parameter_count(); }

  // The number of stack parameters to the call.
  size_t StackParameterCount() const { return stack_param_count_; }

  // The number of parameters to the JS function call.
  size_t JSParameterCount() const {
    DCHECK(IsJSFunctionCall());
    return stack_param_count_;
  }

  // The total number of inputs to this call, which includes the target,
  // receiver, context, etc.
  // TODO(titzer): this should input the framestate input too.
  size_t InputCount() const { return 1 + machine_sig_->parameter_count(); }

  size_t FrameStateCount() const { return NeedsFrameState() ? 1 : 0; }

  Flags flags() const { return flags_; }

  bool NeedsFrameState() const { return flags() & kNeedsFrameState; }
  bool SupportsTailCalls() const { return flags() & kSupportsTailCalls; }
  bool UseNativeStack() const { return flags() & kUseNativeStack; }

  LinkageLocation GetReturnLocation(size_t index) const {
    return location_sig_->GetReturn(index);
  }

  LinkageLocation GetInputLocation(size_t index) const {
    if (index == 0) return target_loc_;
    return location_sig_->GetParam(index - 1);
  }

  const MachineSignature* GetMachineSignature() const { return machine_sig_; }

  MachineType GetReturnType(size_t index) const {
    return machine_sig_->GetReturn(index);
  }

  MachineType GetInputType(size_t index) const {
    if (index == 0) return target_type_;
    return machine_sig_->GetParam(index - 1);
  }

  // Operator properties describe how this call can be optimized, if at all.
  Operator::Properties properties() const { return properties_; }

  // Get the callee-saved registers, if any, across this call.
  RegList CalleeSavedRegisters() const { return callee_saved_registers_; }

  // Get the callee-saved FP registers, if any, across this call.
  RegList CalleeSavedFPRegisters() const { return callee_saved_fp_registers_; }

  const char* debug_name() const { return debug_name_; }

  bool UsesOnlyRegisters() const;

  bool HasSameReturnLocationsAs(const CallDescriptor* other) const;

  bool CanTailCall(const Node* call, int* stack_param_delta) const;

 private:
  friend class Linkage;

  const Kind kind_;
  const MachineType target_type_;
  const LinkageLocation target_loc_;
  const MachineSignature* const machine_sig_;
  const LocationSignature* const location_sig_;
  const size_t stack_param_count_;
  const Operator::Properties properties_;
  const RegList callee_saved_registers_;
  const RegList callee_saved_fp_registers_;
  const Flags flags_;
  const char* const debug_name_;

  DISALLOW_COPY_AND_ASSIGN(CallDescriptor);
};

DEFINE_OPERATORS_FOR_FLAGS(CallDescriptor::Flags)

std::ostream& operator<<(std::ostream& os, const CallDescriptor& d);
std::ostream& operator<<(std::ostream& os, const CallDescriptor::Kind& k);

// Defines the linkage for a compilation, including the calling conventions
// for incoming parameters and return value(s) as well as the outgoing calling
// convention for any kind of call. Linkage is generally architecture-specific.
//
// Can be used to translate {arg_index} (i.e. index of the call node input) as
// well as {param_index} (i.e. as stored in parameter nodes) into an operator
// representing the architecture-specific location. The following call node
// layouts are supported (where {n} is the number of value inputs):
//
//                  #0          #1     #2     #3     [...]             #n
// Call[CodeStub]   code,       arg 1, arg 2, arg 3, [...],            context
// Call[JSFunction] function,   rcvr,  arg 1, arg 2, [...], new, #arg, context
// Call[Runtime]    CEntryStub, arg 1, arg 2, arg 3, [...], fun, #arg, context
class Linkage : public ZoneObject {
 public:
  explicit Linkage(CallDescriptor* incoming) : incoming_(incoming) {}

  static CallDescriptor* ComputeIncoming(Zone* zone, CompilationInfo* info);

  // The call descriptor for this compilation unit describes the locations
  // of incoming parameters and the outgoing return value(s).
  CallDescriptor* GetIncomingDescriptor() const { return incoming_; }
  static CallDescriptor* GetJSCallDescriptor(Zone* zone, bool is_osr,
                                             int parameter_count,
                                             CallDescriptor::Flags flags);

  static CallDescriptor* GetRuntimeCallDescriptor(
      Zone* zone, Runtime::FunctionId function, int parameter_count,
      Operator::Properties properties, CallDescriptor::Flags flags);

  static CallDescriptor* GetLazyBailoutDescriptor(Zone* zone);

  static CallDescriptor* GetStubCallDescriptor(
      Isolate* isolate, Zone* zone, const CallInterfaceDescriptor& descriptor,
      int stack_parameter_count, CallDescriptor::Flags flags,
      Operator::Properties properties = Operator::kNoProperties,
      MachineType return_type = MachineType::AnyTagged(),
      size_t return_count = 1);

  // Creates a call descriptor for simplified C calls that is appropriate
  // for the host platform. This simplified calling convention only supports
  // integers and pointers of one word size each, i.e. no floating point,
  // structs, pointers to members, etc.
  static CallDescriptor* GetSimplifiedCDescriptor(Zone* zone,
                                                  const MachineSignature* sig);

  // Creates a call descriptor for interpreter handler code stubs. These are not
  // intended to be called directly but are instead dispatched to by the
  // interpreter.
  static CallDescriptor* GetInterpreterDispatchDescriptor(Zone* zone);

  // Get the location of an (incoming) parameter to this function.
  LinkageLocation GetParameterLocation(int index) const {
    return incoming_->GetInputLocation(index + 1);  // + 1 to skip target.
  }

  // Get the machine type of an (incoming) parameter to this function.
  MachineType GetParameterType(int index) const {
    return incoming_->GetInputType(index + 1);  // + 1 to skip target.
  }

  // Get the location where this function should place its return value.
  LinkageLocation GetReturnLocation(size_t index = 0) const {
    return incoming_->GetReturnLocation(index);
  }

  // Get the machine type of this function's return value.
  MachineType GetReturnType(size_t index = 0) const {
    return incoming_->GetReturnType(index);
  }

  bool ParameterHasSecondaryLocation(int index) const;
  LinkageLocation GetParameterSecondaryLocation(int index) const;

  static int FrameStateInputCount(Runtime::FunctionId function);

  // Get the location where an incoming OSR value is stored.
  LinkageLocation GetOsrValueLocation(int index) const;

  // A special {Parameter} index for JSCalls that represents the new target.
  static int GetJSCallNewTargetParamIndex(int parameter_count) {
    return parameter_count + 0;  // Parameter (arity + 0) is special.
  }

  // A special {Parameter} index for JSCalls that represents the argument count.
  static int GetJSCallArgCountParamIndex(int parameter_count) {
    return parameter_count + 1;  // Parameter (arity + 1) is special.
  }

  // A special {Parameter} index for JSCalls that represents the context.
  static int GetJSCallContextParamIndex(int parameter_count) {
    return parameter_count + 2;  // Parameter (arity + 2) is special.
  }

  // A special {Parameter} index for JSCalls that represents the closure.
  static const int kJSCallClosureParamIndex = -1;

  // A special {OsrValue} index to indicate the context spill slot.
  static const int kOsrContextSpillSlotIndex = -1;

  // Special parameter indices used to pass fixed register data through
  // interpreter dispatches.
  static const int kInterpreterAccumulatorParameter = 0;
  static const int kInterpreterRegisterFileParameter = 1;
  static const int kInterpreterBytecodeOffsetParameter = 2;
  static const int kInterpreterBytecodeArrayParameter = 3;
  static const int kInterpreterDispatchTableParameter = 4;
  static const int kInterpreterContextParameter = 5;

 private:
  CallDescriptor* const incoming_;

  DISALLOW_COPY_AND_ASSIGN(Linkage);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LINKAGE_H_
