// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LINKAGE_H_
#define V8_COMPILER_LINKAGE_H_

#include "src/base/compiler-specific.h"
#include "src/base/flags.h"
#include "src/compiler/frame.h"
#include "src/compiler/operator.h"
#include "src/globals.h"
#include "src/interface-descriptors.h"
#include "src/machine-type.h"
#include "src/reglist.h"
#include "src/runtime/runtime.h"
#include "src/signature.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;
class OptimizedCompilationInfo;

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

  static LinkageLocation ForAnyRegister(
      MachineType type = MachineType::None()) {
    return LinkageLocation(REGISTER, ANY_REGISTER, type);
  }

  static LinkageLocation ForRegister(int32_t reg,
                                     MachineType type = MachineType::None()) {
    DCHECK_LE(0, reg);
    return LinkageLocation(REGISTER, reg, type);
  }

  static LinkageLocation ForCallerFrameSlot(int32_t slot, MachineType type) {
    DCHECK_GT(0, slot);
    return LinkageLocation(STACK_SLOT, slot, type);
  }

  static LinkageLocation ForCalleeFrameSlot(int32_t slot, MachineType type) {
    // TODO(titzer): bailout instead of crashing here.
    DCHECK(slot >= 0 && slot < LinkageLocation::MAX_STACK_SLOT);
    return LinkageLocation(STACK_SLOT, slot, type);
  }

  static LinkageLocation ForSavedCallerReturnAddress() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kCallerPCOffset) /
                                  kPointerSize,
                              MachineType::Pointer());
  }

  static LinkageLocation ForSavedCallerFramePtr() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kCallerFPOffset) /
                                  kPointerSize,
                              MachineType::Pointer());
  }

  static LinkageLocation ForSavedCallerConstantPool() {
    DCHECK(V8_EMBEDDED_CONSTANT_POOL);
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kConstantPoolOffset) /
                                  kPointerSize,
                              MachineType::AnyTagged());
  }

  static LinkageLocation ForSavedCallerFunction() {
    return ForCalleeFrameSlot((StandardFrameConstants::kCallerPCOffset -
                               StandardFrameConstants::kFunctionOffset) /
                                  kPointerSize,
                              MachineType::AnyTagged());
  }

  static LinkageLocation ConvertToTailCallerLocation(
      LinkageLocation caller_location, int stack_param_delta) {
    if (!caller_location.IsRegister()) {
      return LinkageLocation(STACK_SLOT,
                             caller_location.GetLocation() + stack_param_delta,
                             caller_location.GetType());
    }
    return caller_location;
  }

  MachineType GetType() const { return machine_type_; }

  int GetSize() const {
    return 1 << ElementSizeLog2Of(GetType().representation());
  }

  int GetSizeInPointers() const {
    // Round up
    return (GetSize() + kPointerSize - 1) / kPointerSize;
  }

  int32_t GetLocation() const {
    // We can't use LocationField::decode here because it doesn't work for
    // negative values!
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

 private:
  enum LocationType { REGISTER, STACK_SLOT };

  class TypeField : public BitField<LocationType, 0, 1> {};
  class LocationField : public BitField<int32_t, TypeField::kNext, 31> {};

  static constexpr int32_t ANY_REGISTER = -1;
  static constexpr int32_t MAX_STACK_SLOT = 32767;

  LinkageLocation(LocationType type, int32_t location,
                  MachineType machine_type) {
    bit_field_ = TypeField::encode(type) |
                 ((location << LocationField::kShift) & LocationField::kMask);
    machine_type_ = machine_type;
  }

  int32_t bit_field_;
  MachineType machine_type_;
};

typedef Signature<LinkageLocation> LocationSignature;

// Describes a call to various parts of the compiler. Every call has the notion
// of a "target", which is the first input to the call.
class V8_EXPORT_PRIVATE CallDescriptor final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  // Describes the kind of this call, which determines the target.
  enum Kind {
    kCallCodeObject,   // target is a Code object
    kCallJSFunction,   // target is a JSFunction object
    kCallAddress,      // target is a machine pointer
    kCallWasmFunction  // target is a wasm function
  };

  enum Flag {
    kNoFlags = 0u,
    kNeedsFrameState = 1u << 0,
    kHasExceptionHandler = 1u << 1,
    kCanUseRoots = 1u << 2,
    // Causes the code generator to initialize the root register.
    kInitializeRootRegister = 1u << 3,
    // Does not ever try to allocate space on our heap.
    kNoAllocate = 1u << 4,
    // Push argument count as part of function prologue.
    kPushArgumentCount = 1u << 5,
    // Use retpoline for this call if indirect.
    kRetpoline = 1u << 6,
    // Use the kJavaScriptCallCodeStartRegister (fixed) register for the
    // indirect target address when calling.
    kFixedTargetRegister = 1u << 7
  };
  typedef base::Flags<Flag> Flags;

  CallDescriptor(Kind kind, MachineType target_type, LinkageLocation target_loc,
                 LocationSignature* location_sig, size_t stack_param_count,
                 Operator::Properties properties,
                 RegList callee_saved_registers,
                 RegList callee_saved_fp_registers, Flags flags,
                 const char* debug_name = "",
                 const RegList allocatable_registers = 0,
                 size_t stack_return_count = 0)
      : kind_(kind),
        target_type_(target_type),
        target_loc_(target_loc),
        location_sig_(location_sig),
        stack_param_count_(stack_param_count),
        stack_return_count_(stack_return_count),
        properties_(properties),
        callee_saved_registers_(callee_saved_registers),
        callee_saved_fp_registers_(callee_saved_fp_registers),
        allocatable_registers_(allocatable_registers),
        flags_(flags),
        debug_name_(debug_name) {}

  // Returns the kind of this call.
  Kind kind() const { return kind_; }

  // Returns {true} if this descriptor is a call to a C function.
  bool IsCFunctionCall() const { return kind_ == kCallAddress; }

  // Returns {true} if this descriptor is a call to a JSFunction.
  bool IsJSFunctionCall() const { return kind_ == kCallJSFunction; }

  // Returns {true} if this descriptor is a call to a WebAssembly function.
  bool IsWasmFunctionCall() const { return kind_ == kCallWasmFunction; }

  bool RequiresFrameAsIncoming() const {
    return IsCFunctionCall() || IsJSFunctionCall() || IsWasmFunctionCall();
  }

  // The number of return values from this call.
  size_t ReturnCount() const { return location_sig_->return_count(); }

  // The number of C parameters to this call.
  size_t ParameterCount() const { return location_sig_->parameter_count(); }

  // The number of stack parameters to the call.
  size_t StackParameterCount() const { return stack_param_count_; }

  // The number of stack return values from the call.
  size_t StackReturnCount() const { return stack_return_count_; }

  // The number of parameters to the JS function call.
  size_t JSParameterCount() const {
    DCHECK(IsJSFunctionCall());
    return stack_param_count_;
  }

  // The total number of inputs to this call, which includes the target,
  // receiver, context, etc.
  // TODO(titzer): this should input the framestate input too.
  size_t InputCount() const { return 1 + location_sig_->parameter_count(); }

  size_t FrameStateCount() const { return NeedsFrameState() ? 1 : 0; }

  Flags flags() const { return flags_; }

  bool NeedsFrameState() const { return flags() & kNeedsFrameState; }
  bool PushArgumentCount() const { return flags() & kPushArgumentCount; }
  bool InitializeRootRegister() const {
    return flags() & kInitializeRootRegister;
  }

  LinkageLocation GetReturnLocation(size_t index) const {
    return location_sig_->GetReturn(index);
  }

  LinkageLocation GetInputLocation(size_t index) const {
    if (index == 0) return target_loc_;
    return location_sig_->GetParam(index - 1);
  }

  MachineSignature* GetMachineSignature(Zone* zone) const;

  MachineType GetReturnType(size_t index) const {
    return location_sig_->GetReturn(index).GetType();
  }

  MachineType GetInputType(size_t index) const {
    if (index == 0) return target_type_;
    return location_sig_->GetParam(index - 1).GetType();
  }

  MachineType GetParameterType(size_t index) const {
    return location_sig_->GetParam(index).GetType();
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

  // Returns the first stack slot that is not used by the stack parameters.
  int GetFirstUnusedStackSlot() const;

  int GetStackParameterDelta(const CallDescriptor* tail_caller) const;

  bool CanTailCall(const Node* call) const;

  int CalculateFixedFrameSize() const;

  RegList AllocatableRegisters() const { return allocatable_registers_; }

  bool HasRestrictedAllocatableRegisters() const {
    return allocatable_registers_ != 0;
  }

  void set_save_fp_mode(SaveFPRegsMode mode) { save_fp_mode_ = mode; }

  SaveFPRegsMode get_save_fp_mode() const { return save_fp_mode_; }

 private:
  friend class Linkage;
  SaveFPRegsMode save_fp_mode_ = kSaveFPRegs;

  const Kind kind_;
  const MachineType target_type_;
  const LinkageLocation target_loc_;
  const LocationSignature* const location_sig_;
  const size_t stack_param_count_;
  const size_t stack_return_count_;
  const Operator::Properties properties_;
  const RegList callee_saved_registers_;
  const RegList callee_saved_fp_registers_;
  // Non-zero value means restricting the set of allocatable registers for
  // register allocator to use.
  const RegList allocatable_registers_;
  const Flags flags_;
  const char* const debug_name_;

  DISALLOW_COPY_AND_ASSIGN(CallDescriptor);
};

DEFINE_OPERATORS_FOR_FLAGS(CallDescriptor::Flags)

std::ostream& operator<<(std::ostream& os, const CallDescriptor& d);
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const CallDescriptor::Kind& k);

// Defines the linkage for a compilation, including the calling conventions
// for incoming parameters and return value(s) as well as the outgoing calling
// convention for any kind of call. Linkage is generally architecture-specific.
//
// Can be used to translate {arg_index} (i.e. index of the call node input) as
// well as {param_index} (i.e. as stored in parameter nodes) into an operator
// representing the architecture-specific location. The following call node
// layouts are supported (where {n} is the number of value inputs):
//
//                        #0          #1     #2     [...]             #n
// Call[CodeStub]         code,       arg 1, arg 2, [...],            context
// Call[JSFunction]       function,   rcvr,  arg 1, [...], new, #arg, context
// Call[Runtime]          CEntry,     arg 1, arg 2, [...], fun, #arg, context
// Call[BytecodeDispatch] address,    arg 1, arg 2, [...]
class V8_EXPORT_PRIVATE Linkage : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  explicit Linkage(CallDescriptor* incoming) : incoming_(incoming) {}

  static CallDescriptor* ComputeIncoming(Zone* zone,
                                         OptimizedCompilationInfo* info);

  // The call descriptor for this compilation unit describes the locations
  // of incoming parameters and the outgoing return value(s).
  CallDescriptor* GetIncomingDescriptor() const { return incoming_; }
  static CallDescriptor* GetJSCallDescriptor(Zone* zone, bool is_osr,
                                             int parameter_count,
                                             CallDescriptor::Flags flags);

  static CallDescriptor* GetRuntimeCallDescriptor(
      Zone* zone, Runtime::FunctionId function, int js_parameter_count,
      Operator::Properties properties, CallDescriptor::Flags flags);

  static CallDescriptor* GetCEntryStubCallDescriptor(
      Zone* zone, int return_count, int js_parameter_count,
      const char* debug_name, Operator::Properties properties,
      CallDescriptor::Flags flags);

  static CallDescriptor* GetStubCallDescriptor(
      Zone* zone, const CallInterfaceDescriptor& descriptor,
      int stack_parameter_count, CallDescriptor::Flags flags,
      Operator::Properties properties = Operator::kNoProperties,
      StubCallMode stub_mode = StubCallMode::kCallOnHeapBuiltin);

  static CallDescriptor* GetBytecodeDispatchCallDescriptor(
      Zone* zone, const CallInterfaceDescriptor& descriptor,
      int stack_parameter_count);

  // Creates a call descriptor for simplified C calls that is appropriate
  // for the host platform. This simplified calling convention only supports
  // integers and pointers of one word size each, i.e. no floating point,
  // structs, pointers to members, etc.
  static CallDescriptor* GetSimplifiedCDescriptor(
      Zone* zone, const MachineSignature* sig,
      bool set_initialize_root_flag = false);

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

  static bool NeedsFrameStateInput(Runtime::FunctionId function);

  // Get the location where an incoming OSR value is stored.
  LinkageLocation GetOsrValueLocation(int index) const;

  // A special {Parameter} index for Stub Calls that represents context.
  static int GetStubCallContextParamIndex(int parameter_count) {
    return parameter_count + 0;  // Parameter (arity + 0) is special.
  }

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

  // A special {OsrValue} index to indicate the accumulator register.
  static const int kOsrAccumulatorRegisterIndex = -1;

 private:
  CallDescriptor* const incoming_;

  DISALLOW_COPY_AND_ASSIGN(Linkage);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LINKAGE_H_
