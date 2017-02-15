// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_OPERATOR_H_
#define V8_COMPILER_COMMON_OPERATOR_H_

#include "src/assembler.h"
#include "src/compiler/frame-states.h"
#include "src/deoptimize-reason.h"
#include "src/machine-type.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CallDescriptor;
struct CommonOperatorGlobalCache;
class Operator;
class Type;

// Prediction hint for branches.
enum class BranchHint : uint8_t { kNone, kTrue, kFalse };

inline BranchHint NegateBranchHint(BranchHint hint) {
  switch (hint) {
    case BranchHint::kNone:
      return hint;
    case BranchHint::kTrue:
      return BranchHint::kFalse;
    case BranchHint::kFalse:
      return BranchHint::kTrue;
  }
  UNREACHABLE();
  return hint;
}

inline size_t hash_value(BranchHint hint) { return static_cast<size_t>(hint); }

std::ostream& operator<<(std::ostream&, BranchHint);

BranchHint BranchHintOf(const Operator* const);

// Deoptimize reason for Deoptimize, DeoptimizeIf and DeoptimizeUnless.
DeoptimizeReason DeoptimizeReasonOf(Operator const* const);

// Deoptimize bailout kind.
enum class DeoptimizeKind : uint8_t { kEager, kSoft };

size_t hash_value(DeoptimizeKind kind);

std::ostream& operator<<(std::ostream&, DeoptimizeKind);

// Parameters for the {Deoptimize} operator.
class DeoptimizeParameters final {
 public:
  DeoptimizeParameters(DeoptimizeKind kind, DeoptimizeReason reason)
      : kind_(kind), reason_(reason) {}

  DeoptimizeKind kind() const { return kind_; }
  DeoptimizeReason reason() const { return reason_; }

 private:
  DeoptimizeKind const kind_;
  DeoptimizeReason const reason_;
};

bool operator==(DeoptimizeParameters, DeoptimizeParameters);
bool operator!=(DeoptimizeParameters, DeoptimizeParameters);

size_t hast_value(DeoptimizeParameters p);

std::ostream& operator<<(std::ostream&, DeoptimizeParameters p);

DeoptimizeParameters const& DeoptimizeParametersOf(Operator const* const);


class SelectParameters final {
 public:
  explicit SelectParameters(MachineRepresentation representation,
                            BranchHint hint = BranchHint::kNone)
      : representation_(representation), hint_(hint) {}

  MachineRepresentation representation() const { return representation_; }
  BranchHint hint() const { return hint_; }

 private:
  const MachineRepresentation representation_;
  const BranchHint hint_;
};

bool operator==(SelectParameters const&, SelectParameters const&);
bool operator!=(SelectParameters const&, SelectParameters const&);

size_t hash_value(SelectParameters const& p);

std::ostream& operator<<(std::ostream&, SelectParameters const& p);

SelectParameters const& SelectParametersOf(const Operator* const);

CallDescriptor const* CallDescriptorOf(const Operator* const);

size_t ProjectionIndexOf(const Operator* const);

MachineRepresentation PhiRepresentationOf(const Operator* const);


// The {IrOpcode::kParameter} opcode represents an incoming parameter to the
// function. This class bundles the index and a debug name for such operators.
class ParameterInfo final {
 public:
  ParameterInfo(int index, const char* debug_name)
      : index_(index), debug_name_(debug_name) {}

  int index() const { return index_; }
  const char* debug_name() const { return debug_name_; }

 private:
  int index_;
  const char* debug_name_;
};

std::ostream& operator<<(std::ostream&, ParameterInfo const&);

int ParameterIndexOf(const Operator* const);
const ParameterInfo& ParameterInfoOf(const Operator* const);

class RelocatablePtrConstantInfo final {
 public:
  enum Type { kInt32, kInt64 };

  RelocatablePtrConstantInfo(int32_t value, RelocInfo::Mode rmode)
      : value_(value), rmode_(rmode), type_(kInt32) {}
  RelocatablePtrConstantInfo(int64_t value, RelocInfo::Mode rmode)
      : value_(value), rmode_(rmode), type_(kInt64) {}

  intptr_t value() const { return value_; }
  RelocInfo::Mode rmode() const { return rmode_; }
  Type type() const { return type_; }

 private:
  intptr_t value_;
  RelocInfo::Mode rmode_;
  Type type_;
};

bool operator==(RelocatablePtrConstantInfo const& lhs,
                RelocatablePtrConstantInfo const& rhs);
bool operator!=(RelocatablePtrConstantInfo const& lhs,
                RelocatablePtrConstantInfo const& rhs);

std::ostream& operator<<(std::ostream&, RelocatablePtrConstantInfo const&);

size_t hash_value(RelocatablePtrConstantInfo const& p);

// Used to mark a region (as identified by BeginRegion/FinishRegion) as either
// JavaScript-observable or not (i.e. allocations are not JavaScript observable
// themselves, but transitioning stores are).
enum class RegionObservability : uint8_t { kObservable, kNotObservable };

size_t hash_value(RegionObservability);

std::ostream& operator<<(std::ostream&, RegionObservability);

RegionObservability RegionObservabilityOf(Operator const*) WARN_UNUSED_RESULT;

std::ostream& operator<<(std::ostream& os,
                         const ZoneVector<MachineType>* types);

Type* TypeGuardTypeOf(Operator const*) WARN_UNUSED_RESULT;

// Interface for building common operators that can be used at any level of IR,
// including JavaScript, mid-level, and low-level.
class CommonOperatorBuilder final : public ZoneObject {
 public:
  explicit CommonOperatorBuilder(Zone* zone);

  const Operator* Dead();
  const Operator* End(size_t control_input_count);
  const Operator* Branch(BranchHint = BranchHint::kNone);
  const Operator* IfTrue();
  const Operator* IfFalse();
  const Operator* IfSuccess();
  const Operator* IfException();
  const Operator* Switch(size_t control_output_count);
  const Operator* IfValue(int32_t value);
  const Operator* IfDefault();
  const Operator* Throw();
  const Operator* Deoptimize(DeoptimizeKind kind, DeoptimizeReason reason);
  const Operator* DeoptimizeIf(DeoptimizeReason reason);
  const Operator* DeoptimizeUnless(DeoptimizeReason reason);
  const Operator* Return(int value_input_count = 1);
  const Operator* Terminate();

  const Operator* Start(int value_output_count);
  const Operator* Loop(int control_input_count);
  const Operator* Merge(int control_input_count);
  const Operator* Parameter(int index, const char* debug_name = nullptr);

  const Operator* OsrNormalEntry();
  const Operator* OsrLoopEntry();
  const Operator* OsrValue(int index);

  const Operator* Int32Constant(int32_t);
  const Operator* Int64Constant(int64_t);
  const Operator* Float32Constant(volatile float);
  const Operator* Float64Constant(volatile double);
  const Operator* ExternalConstant(const ExternalReference&);
  const Operator* NumberConstant(volatile double);
  const Operator* HeapConstant(const Handle<HeapObject>&);

  const Operator* RelocatableInt32Constant(int32_t value,
                                           RelocInfo::Mode rmode);
  const Operator* RelocatableInt64Constant(int64_t value,
                                           RelocInfo::Mode rmode);

  const Operator* Select(MachineRepresentation, BranchHint = BranchHint::kNone);
  const Operator* Phi(MachineRepresentation representation,
                      int value_input_count);
  const Operator* EffectPhi(int effect_input_count);
  const Operator* InductionVariablePhi(int value_input_count);
  const Operator* LoopExit();
  const Operator* LoopExitValue();
  const Operator* LoopExitEffect();
  const Operator* Checkpoint();
  const Operator* BeginRegion(RegionObservability);
  const Operator* FinishRegion();
  const Operator* StateValues(int arguments);
  const Operator* ObjectState(int pointer_slots, int id);
  const Operator* TypedStateValues(const ZoneVector<MachineType>* types);
  const Operator* FrameState(BailoutId bailout_id,
                             OutputFrameStateCombine state_combine,
                             const FrameStateFunctionInfo* function_info);
  const Operator* Call(const CallDescriptor* descriptor);
  const Operator* TailCall(const CallDescriptor* descriptor);
  const Operator* Projection(size_t index);
  const Operator* Retain();
  const Operator* TypeGuard(Type* type);

  // Constructs a new merge or phi operator with the same opcode as {op}, but
  // with {size} inputs.
  const Operator* ResizeMergeOrPhi(const Operator* op, int size);

  // Constructs function info for frame state construction.
  const FrameStateFunctionInfo* CreateFrameStateFunctionInfo(
      FrameStateType type, int parameter_count, int local_count,
      Handle<SharedFunctionInfo> shared_info);

 private:
  Zone* zone() const { return zone_; }

  const CommonOperatorGlobalCache& cache_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(CommonOperatorBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMMON_OPERATOR_H_
