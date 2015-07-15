// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_OPERATOR_H_
#define V8_COMPILER_COMMON_OPERATOR_H_

#include "src/compiler/frame-states.h"
#include "src/compiler/machine-type.h"
#include "src/unique.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ExternalReference;


namespace compiler {

// Forward declarations.
class CallDescriptor;
struct CommonOperatorGlobalCache;
class Operator;


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


// Prediction whether throw-site is surrounded by any local catch-scope.
enum class IfExceptionHint { kLocallyUncaught, kLocallyCaught };

size_t hash_value(IfExceptionHint hint);

std::ostream& operator<<(std::ostream&, IfExceptionHint);


class SelectParameters final {
 public:
  explicit SelectParameters(MachineType type,
                            BranchHint hint = BranchHint::kNone)
      : type_(type), hint_(hint) {}

  MachineType type() const { return type_; }
  BranchHint hint() const { return hint_; }

 private:
  const MachineType type_;
  const BranchHint hint_;
};

bool operator==(SelectParameters const&, SelectParameters const&);
bool operator!=(SelectParameters const&, SelectParameters const&);

size_t hash_value(SelectParameters const& p);

std::ostream& operator<<(std::ostream&, SelectParameters const& p);

SelectParameters const& SelectParametersOf(const Operator* const);


size_t ProjectionIndexOf(const Operator* const);


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
  const Operator* IfException(IfExceptionHint hint);
  const Operator* Switch(size_t control_output_count);
  const Operator* IfValue(int32_t value);
  const Operator* IfDefault();
  const Operator* Throw();
  const Operator* Deoptimize();
  const Operator* Return();
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
  const Operator* HeapConstant(const Unique<HeapObject>&);

  const Operator* Select(MachineType, BranchHint = BranchHint::kNone);
  const Operator* Phi(MachineType type, int value_input_count);
  const Operator* EffectPhi(int effect_input_count);
  const Operator* EffectSet(int arguments);
  const Operator* ValueEffect(int arguments);
  const Operator* Finish(int arguments);
  const Operator* StateValues(int arguments);
  const Operator* TypedStateValues(const ZoneVector<MachineType>* types);
  const Operator* FrameState(BailoutId bailout_id,
                             OutputFrameStateCombine state_combine,
                             const FrameStateFunctionInfo* function_info);
  const Operator* Call(const CallDescriptor* descriptor);
  const Operator* TailCall(const CallDescriptor* descriptor);
  const Operator* Projection(size_t index);

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
