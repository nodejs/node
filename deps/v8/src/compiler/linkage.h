// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LINKAGE_H_
#define V8_COMPILER_LINKAGE_H_

#include "src/v8.h"

#include "src/code-stubs.h"
#include "src/compiler/frame.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

// Describes the location for a parameter or a return value to a call.
// TODO(titzer): replace with Radium locations when they are ready.
class LinkageLocation {
 public:
  LinkageLocation(MachineType rep, int location)
      : rep_(rep), location_(location) {}

  inline MachineType representation() const { return rep_; }

  static const int16_t ANY_REGISTER = 32767;

 private:
  friend class CallDescriptor;
  friend class OperandGenerator;
  MachineType rep_;
  int16_t location_;  // >= 0 implies register, otherwise stack slot.
};


class CallDescriptor : public ZoneObject {
 public:
  // Describes whether the first parameter is a code object, a JSFunction,
  // or an address--all of which require different machine sequences to call.
  enum Kind { kCallCodeObject, kCallJSFunction, kCallAddress };

  enum DeoptimizationSupport { kCanDeoptimize, kCannotDeoptimize };

  CallDescriptor(Kind kind, int8_t return_count, int16_t parameter_count,
                 int16_t input_count, LinkageLocation* locations,
                 Operator::Property properties, RegList callee_saved_registers,
                 DeoptimizationSupport deoptimization_support,
                 const char* debug_name = "")
      : kind_(kind),
        return_count_(return_count),
        parameter_count_(parameter_count),
        input_count_(input_count),
        locations_(locations),
        properties_(properties),
        callee_saved_registers_(callee_saved_registers),
        deoptimization_support_(deoptimization_support),
        debug_name_(debug_name) {}
  // Returns the kind of this call.
  Kind kind() const { return kind_; }

  // Returns {true} if this descriptor is a call to a JSFunction.
  bool IsJSFunctionCall() const { return kind_ == kCallJSFunction; }

  // The number of return values from this call, usually 0 or 1.
  int ReturnCount() const { return return_count_; }

  // The number of JavaScript parameters to this call, including receiver,
  // but not the context.
  int ParameterCount() const { return parameter_count_; }

  int InputCount() const { return input_count_; }

  bool CanLazilyDeoptimize() const {
    return deoptimization_support_ == kCanDeoptimize;
  }

  LinkageLocation GetReturnLocation(int index) {
    DCHECK(index < return_count_);
    return locations_[0 + index];  // return locations start at 0.
  }

  LinkageLocation GetInputLocation(int index) {
    DCHECK(index < input_count_ + 1);  // input_count + 1 is the context.
    return locations_[return_count_ + index];  // inputs start after returns.
  }

  // Operator properties describe how this call can be optimized, if at all.
  Operator::Property properties() const { return properties_; }

  // Get the callee-saved registers, if any, across this call.
  RegList CalleeSavedRegisters() { return callee_saved_registers_; }

  const char* debug_name() const { return debug_name_; }

 private:
  friend class Linkage;

  Kind kind_;
  int8_t return_count_;
  int16_t parameter_count_;
  int16_t input_count_;
  LinkageLocation* locations_;
  Operator::Property properties_;
  RegList callee_saved_registers_;
  DeoptimizationSupport deoptimization_support_;
  const char* debug_name_;
};

OStream& operator<<(OStream& os, const CallDescriptor& d);
OStream& operator<<(OStream& os, const CallDescriptor::Kind& k);

// Defines the linkage for a compilation, including the calling conventions
// for incoming parameters and return value(s) as well as the outgoing calling
// convention for any kind of call. Linkage is generally architecture-specific.
//
// Can be used to translate {arg_index} (i.e. index of the call node input) as
// well as {param_index} (i.e. as stored in parameter nodes) into an operator
// representing the architecture-specific location. The following call node
// layouts are supported (where {n} is the number value inputs):
//
//                  #0          #1     #2     #3     [...]             #n
// Call[CodeStub]   code,       arg 1, arg 2, arg 3, [...],            context
// Call[JSFunction] function,   rcvr,  arg 1, arg 2, [...],            context
// Call[Runtime]    CEntryStub, arg 1, arg 2, arg 3, [...], fun, #arg, context
class Linkage : public ZoneObject {
 public:
  explicit Linkage(CompilationInfo* info);
  explicit Linkage(CompilationInfo* info, CallDescriptor* incoming)
      : info_(info), incoming_(incoming) {}

  // The call descriptor for this compilation unit describes the locations
  // of incoming parameters and the outgoing return value(s).
  CallDescriptor* GetIncomingDescriptor() { return incoming_; }
  CallDescriptor* GetJSCallDescriptor(int parameter_count);
  static CallDescriptor* GetJSCallDescriptor(int parameter_count, Zone* zone);
  CallDescriptor* GetRuntimeCallDescriptor(
      Runtime::FunctionId function, int parameter_count,
      Operator::Property properties,
      CallDescriptor::DeoptimizationSupport can_deoptimize =
          CallDescriptor::kCannotDeoptimize);
  static CallDescriptor* GetRuntimeCallDescriptor(
      Runtime::FunctionId function, int parameter_count,
      Operator::Property properties,
      CallDescriptor::DeoptimizationSupport can_deoptimize, Zone* zone);

  CallDescriptor* GetStubCallDescriptor(
      CodeStubInterfaceDescriptor* descriptor, int stack_parameter_count = 0,
      CallDescriptor::DeoptimizationSupport can_deoptimize =
          CallDescriptor::kCannotDeoptimize);
  static CallDescriptor* GetStubCallDescriptor(
      CodeStubInterfaceDescriptor* descriptor, int stack_parameter_count,
      CallDescriptor::DeoptimizationSupport can_deoptimize, Zone* zone);

  // Creates a call descriptor for simplified C calls that is appropriate
  // for the host platform. This simplified calling convention only supports
  // integers and pointers of one word size each, i.e. no floating point,
  // structs, pointers to members, etc.
  static CallDescriptor* GetSimplifiedCDescriptor(
      Zone* zone, int num_params, MachineType return_type,
      const MachineType* param_types);

  // Get the location of an (incoming) parameter to this function.
  LinkageLocation GetParameterLocation(int index) {
    return incoming_->GetInputLocation(index + 1);
  }

  // Get the location where this function should place its return value.
  LinkageLocation GetReturnLocation() {
    return incoming_->GetReturnLocation(0);
  }

  // Get the frame offset for a given spill slot. The location depends on the
  // calling convention and the specific frame layout, and may thus be
  // architecture-specific. Negative spill slots indicate arguments on the
  // caller's frame. The {extra} parameter indicates an additional offset from
  // the frame offset, e.g. to index into part of a double slot.
  FrameOffset GetFrameOffset(int spill_slot, Frame* frame, int extra = 0);

  CompilationInfo* info() const { return info_; }

 private:
  CompilationInfo* info_;
  CallDescriptor* incoming_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_LINKAGE_H_
