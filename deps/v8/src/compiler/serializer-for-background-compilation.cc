// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/serializer-for-background-compilation.h"

#include <sstream>

#include "src/base/optional.h"
#include "src/compiler/access-info.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/serializer-hints.h"
#include "src/compiler/zone-stats.h"
#include "src/handles/handles-inl.h"
#include "src/ic/call-optimization.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects/code.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

#define KILL_ENVIRONMENT_LIST(V) \
  V(Abort)                       \
  V(ReThrow)                     \
  V(Throw)

#define CLEAR_ACCUMULATOR_LIST(V) \
  V(CallRuntime)                  \
  V(CloneObject)                  \
  V(CreateArrayFromIterable)      \
  V(CreateEmptyObjectLiteral)     \
  V(CreateMappedArguments)        \
  V(CreateRestParameter)          \
  V(CreateUnmappedArguments)      \
  V(DeletePropertySloppy)         \
  V(DeletePropertyStrict)         \
  V(ForInContinue)                \
  V(ForInEnumerate)               \
  V(ForInStep)                    \
  V(LogicalNot)                   \
  V(SetPendingMessage)            \
  V(TestNull)                     \
  V(TestReferenceEqual)           \
  V(TestTypeOf)                   \
  V(TestUndefined)                \
  V(TestUndetectable)             \
  V(ToBooleanLogicalNot)          \
  V(ToName)                       \
  V(ToString)                     \
  V(TypeOf)

#define UNCONDITIONAL_JUMPS_LIST(V) \
  V(Jump)                           \
  V(JumpConstant)                   \
  V(JumpLoop)

#define CONDITIONAL_JUMPS_LIST(V) \
  V(JumpIfFalse)                  \
  V(JumpIfFalseConstant)          \
  V(JumpIfJSReceiver)             \
  V(JumpIfJSReceiverConstant)     \
  V(JumpIfNotNull)                \
  V(JumpIfNotNullConstant)        \
  V(JumpIfNotUndefined)           \
  V(JumpIfNotUndefinedConstant)   \
  V(JumpIfNull)                   \
  V(JumpIfNullConstant)           \
  V(JumpIfToBooleanFalse)         \
  V(JumpIfToBooleanFalseConstant) \
  V(JumpIfToBooleanTrue)          \
  V(JumpIfToBooleanTrueConstant)  \
  V(JumpIfTrue)                   \
  V(JumpIfTrueConstant)           \
  V(JumpIfUndefined)              \
  V(JumpIfUndefinedConstant)      \
  V(JumpIfUndefinedOrNull)        \
  V(JumpIfUndefinedOrNullConstant)

#define IGNORED_BYTECODE_LIST(V)      \
  V(IncBlockCounter)                  \
  V(StackCheck)                       \
  V(ThrowSuperAlreadyCalledIfNotHole) \
  V(ThrowSuperNotCalledIfHole)

#define UNREACHABLE_BYTECODE_LIST(V) \
  V(ExtraWide)                       \
  V(Illegal)                         \
  V(Wide)

#define BINARY_OP_LIST(V) \
  V(Add)                  \
  V(AddSmi)               \
  V(BitwiseAnd)           \
  V(BitwiseAndSmi)        \
  V(BitwiseOr)            \
  V(BitwiseOrSmi)         \
  V(BitwiseXor)           \
  V(BitwiseXorSmi)        \
  V(Div)                  \
  V(DivSmi)               \
  V(Exp)                  \
  V(ExpSmi)               \
  V(Mod)                  \
  V(ModSmi)               \
  V(Mul)                  \
  V(MulSmi)               \
  V(ShiftLeft)            \
  V(ShiftLeftSmi)         \
  V(ShiftRight)           \
  V(ShiftRightSmi)        \
  V(ShiftRightLogical)    \
  V(ShiftRightLogicalSmi) \
  V(Sub)                  \
  V(SubSmi)

#define UNARY_OP_LIST(V) \
  V(BitwiseNot)          \
  V(Dec)                 \
  V(Inc)                 \
  V(Negate)

#define COMPARE_OP_LIST(V)  \
  V(TestEqual)              \
  V(TestEqualStrict)        \
  V(TestGreaterThan)        \
  V(TestGreaterThanOrEqual) \
  V(TestLessThan)           \
  V(TestLessThanOrEqual)

#define SUPPORTED_BYTECODE_LIST(V)    \
  V(CallAnyReceiver)                  \
  V(CallJSRuntime)                    \
  V(CallNoFeedback)                   \
  V(CallProperty)                     \
  V(CallProperty0)                    \
  V(CallProperty1)                    \
  V(CallProperty2)                    \
  V(CallUndefinedReceiver)            \
  V(CallUndefinedReceiver0)           \
  V(CallUndefinedReceiver1)           \
  V(CallUndefinedReceiver2)           \
  V(CallWithSpread)                   \
  V(Construct)                        \
  V(ConstructWithSpread)              \
  V(CreateArrayLiteral)               \
  V(CreateBlockContext)               \
  V(CreateCatchContext)               \
  V(CreateClosure)                    \
  V(CreateEmptyArrayLiteral)          \
  V(CreateEvalContext)                \
  V(CreateFunctionContext)            \
  V(CreateObjectLiteral)              \
  V(CreateRegExpLiteral)              \
  V(CreateWithContext)                \
  V(ForInNext)                        \
  V(ForInPrepare)                     \
  V(GetIterator)                      \
  V(GetSuperConstructor)              \
  V(GetTemplateObject)                \
  V(InvokeIntrinsic)                  \
  V(LdaConstant)                      \
  V(LdaContextSlot)                   \
  V(LdaCurrentContextSlot)            \
  V(LdaImmutableContextSlot)          \
  V(LdaImmutableCurrentContextSlot)   \
  V(LdaModuleVariable)                \
  V(LdaFalse)                         \
  V(LdaGlobal)                        \
  V(LdaGlobalInsideTypeof)            \
  V(LdaKeyedProperty)                 \
  V(LdaLookupContextSlot)             \
  V(LdaLookupContextSlotInsideTypeof) \
  V(LdaLookupGlobalSlot)              \
  V(LdaLookupGlobalSlotInsideTypeof)  \
  V(LdaLookupSlot)                    \
  V(LdaLookupSlotInsideTypeof)        \
  V(LdaNamedProperty)                 \
  V(LdaNamedPropertyNoFeedback)       \
  V(LdaNull)                          \
  V(Ldar)                             \
  V(LdaSmi)                           \
  V(LdaTheHole)                       \
  V(LdaTrue)                          \
  V(LdaUndefined)                     \
  V(LdaZero)                          \
  V(Mov)                              \
  V(PopContext)                       \
  V(PushContext)                      \
  V(Return)                           \
  V(StaContextSlot)                   \
  V(StaCurrentContextSlot)            \
  V(StaDataPropertyInLiteral)         \
  V(StaGlobal)                        \
  V(StaInArrayLiteral)                \
  V(StaKeyedProperty)                 \
  V(StaLookupSlot)                    \
  V(StaModuleVariable)                \
  V(StaNamedOwnProperty)              \
  V(StaNamedProperty)                 \
  V(StaNamedPropertyNoFeedback)       \
  V(Star)                             \
  V(SwitchOnGeneratorState)           \
  V(SwitchOnSmiNoFeedback)            \
  V(TestIn)                           \
  V(TestInstanceOf)                   \
  V(ThrowReferenceErrorIfHole)        \
  V(ToNumber)                         \
  V(ToNumeric)                        \
  BINARY_OP_LIST(V)                   \
  COMPARE_OP_LIST(V)                  \
  CLEAR_ACCUMULATOR_LIST(V)           \
  CONDITIONAL_JUMPS_LIST(V)           \
  IGNORED_BYTECODE_LIST(V)            \
  KILL_ENVIRONMENT_LIST(V)            \
  UNARY_OP_LIST(V)                    \
  UNCONDITIONAL_JUMPS_LIST(V)         \
  UNREACHABLE_BYTECODE_LIST(V)

struct HintsImpl : public ZoneObject {
  explicit HintsImpl(Zone* zone) : zone_(zone) {}

  ConstantsSet constants_;
  MapsSet maps_;
  VirtualClosuresSet virtual_closures_;
  VirtualContextsSet virtual_contexts_;
  VirtualBoundFunctionsSet virtual_bound_functions_;

  Zone* const zone_;
};

void Hints::EnsureAllocated(Zone* zone, bool check_zone_equality) {
  if (IsAllocated()) {
    if (check_zone_equality) CHECK_EQ(zone, impl_->zone_);
    // ... else {zone} lives no longer than {impl_->zone_} but we have no way of
    // checking that.
  } else {
    impl_ = new (zone) HintsImpl(zone);
  }
  DCHECK(IsAllocated());
}

struct VirtualBoundFunction {
  Hints const bound_target;
  HintsVector const bound_arguments;

  VirtualBoundFunction(Hints const& target, const HintsVector& arguments)
      : bound_target(target), bound_arguments(arguments) {}

  bool operator==(const VirtualBoundFunction& other) const {
    if (bound_arguments.size() != other.bound_arguments.size()) return false;
    if (bound_target != other.bound_target) return false;

    for (size_t i = 0; i < bound_arguments.size(); ++i) {
      if (bound_arguments[i] != other.bound_arguments[i]) return false;
    }
    return true;
  }
};

// A VirtualClosure is a SharedFunctionInfo and a FeedbackVector, plus
// Hints about the context in which a closure will be created from them.
class VirtualClosure {
 public:
  VirtualClosure(Handle<JSFunction> function, Isolate* isolate, Zone* zone);

  VirtualClosure(Handle<SharedFunctionInfo> shared,
                 Handle<FeedbackVector> feedback_vector,
                 Hints const& context_hints);

  Handle<SharedFunctionInfo> shared() const { return shared_; }
  Handle<FeedbackVector> feedback_vector() const { return feedback_vector_; }
  Hints const& context_hints() const { return context_hints_; }

  bool operator==(const VirtualClosure& other) const {
    // A feedback vector is never used for more than one SFI.  There might,
    // however, be two virtual closures with the same SFI and vector, but
    // different context hints. crbug.com/1024282 has a link to a document
    // describing why the context_hints_ might be different in that case.
    DCHECK_IMPLIES(feedback_vector_.equals(other.feedback_vector_),
                   shared_.equals(other.shared_));
    return feedback_vector_.equals(other.feedback_vector_) &&
           context_hints_ == other.context_hints_;
  }

 private:
  Handle<SharedFunctionInfo> const shared_;
  Handle<FeedbackVector> const feedback_vector_;
  Hints const context_hints_;
};

// A CompilationSubject is a VirtualClosure, optionally with a matching
// concrete closure.
class CompilationSubject {
 public:
  explicit CompilationSubject(VirtualClosure virtual_closure)
      : virtual_closure_(virtual_closure), closure_() {}

  // The zone parameter is to correctly initialize the virtual closure,
  // which contains zone-allocated context information.
  CompilationSubject(Handle<JSFunction> closure, Isolate* isolate, Zone* zone);

  const VirtualClosure& virtual_closure() const { return virtual_closure_; }
  MaybeHandle<JSFunction> closure() const { return closure_; }

 private:
  VirtualClosure const virtual_closure_;
  MaybeHandle<JSFunction> const closure_;
};

// A Callee is either a JSFunction (which may not have a feedback vector), or a
// VirtualClosure. Note that this is different from CompilationSubject, which
// always has a VirtualClosure.
class Callee {
 public:
  explicit Callee(Handle<JSFunction> jsfunction)
      : jsfunction_(jsfunction), virtual_closure_() {}
  explicit Callee(VirtualClosure const& virtual_closure)
      : jsfunction_(), virtual_closure_(virtual_closure) {}

  Handle<SharedFunctionInfo> shared(Isolate* isolate) const {
    return virtual_closure_.has_value()
               ? virtual_closure_->shared()
               : handle(jsfunction_.ToHandleChecked()->shared(), isolate);
  }

  bool HasFeedbackVector() const {
    Handle<JSFunction> function;
    return virtual_closure_.has_value() ||
           jsfunction_.ToHandleChecked()->has_feedback_vector();
  }

  CompilationSubject ToCompilationSubject(Isolate* isolate, Zone* zone) const {
    CHECK(HasFeedbackVector());
    return virtual_closure_.has_value()
               ? CompilationSubject(*virtual_closure_)
               : CompilationSubject(jsfunction_.ToHandleChecked(), isolate,
                                    zone);
  }

 private:
  MaybeHandle<JSFunction> const jsfunction_;
  base::Optional<VirtualClosure> const virtual_closure_;
};

// If a list of arguments (hints) is shorter than the function's parameter
// count, this enum expresses what we know about the missing arguments.
enum MissingArgumentsPolicy {
  kMissingArgumentsAreUndefined,  // ... as in the JS undefined value
  kMissingArgumentsAreUnknown,
};

// The SerializerForBackgroundCompilation makes sure that the relevant function
// data such as bytecode, SharedFunctionInfo and FeedbackVector, used by later
// optimizations in the compiler, is copied to the heap broker.
class SerializerForBackgroundCompilation {
 public:
  SerializerForBackgroundCompilation(
      ZoneStats* zone_stats, JSHeapBroker* broker,
      CompilationDependencies* dependencies, Handle<JSFunction> closure,
      SerializerForBackgroundCompilationFlags flags, BailoutId osr_offset);
  Hints Run();  // NOTE: Returns empty for an
                // already-serialized function.

  class Environment;

 private:
  SerializerForBackgroundCompilation(
      ZoneStats* zone_stats, JSHeapBroker* broker,
      CompilationDependencies* dependencies, CompilationSubject function,
      base::Optional<Hints> new_target, const HintsVector& arguments,
      MissingArgumentsPolicy padding,
      SerializerForBackgroundCompilationFlags flags, int nesting_level);

  bool BailoutOnUninitialized(ProcessedFeedback const& feedback);

  void TraverseBytecode();

#define DECLARE_VISIT_BYTECODE(name, ...) \
  void Visit##name(interpreter::BytecodeArrayIterator* iterator);
  SUPPORTED_BYTECODE_LIST(DECLARE_VISIT_BYTECODE)
#undef DECLARE_VISIT_BYTECODE

  Hints& register_hints(interpreter::Register reg);

  // Return a vector containing the hints for the given register range (in
  // order). Also prepare these hints for feedback backpropagation by allocating
  // any that aren't yet allocated.
  HintsVector PrepareArgumentsHints(interpreter::Register first, size_t count);

  // Like above except that the hints have to be given directly.
  template <typename... MoreHints>
  HintsVector PrepareArgumentsHints(Hints* hints, MoreHints... more);

  void ProcessCalleeForCallOrConstruct(Callee const& callee,
                                       base::Optional<Hints> new_target,
                                       const HintsVector& arguments,
                                       SpeculationMode speculation_mode,
                                       MissingArgumentsPolicy padding,
                                       Hints* result_hints);
  void ProcessCalleeForCallOrConstruct(Handle<Object> callee,
                                       base::Optional<Hints> new_target,
                                       const HintsVector& arguments,
                                       SpeculationMode speculation_mode,
                                       MissingArgumentsPolicy padding,
                                       Hints* result_hints);
  void ProcessCallOrConstruct(Hints callee, base::Optional<Hints> new_target,
                              HintsVector* arguments, FeedbackSlot slot,
                              MissingArgumentsPolicy padding);
  void ProcessCallOrConstructRecursive(Hints const& callee,
                                       base::Optional<Hints> new_target,
                                       const HintsVector& arguments,
                                       SpeculationMode speculation_mode,
                                       MissingArgumentsPolicy padding,
                                       Hints* result_hints);
  void ProcessNewTargetForConstruct(Hints const& new_target,
                                    Hints* result_hints);
  void ProcessCallVarArgs(
      ConvertReceiverMode receiver_mode, Hints const& callee,
      interpreter::Register first_reg, int reg_count, FeedbackSlot slot,
      MissingArgumentsPolicy padding = kMissingArgumentsAreUndefined);
  void ProcessApiCall(Handle<SharedFunctionInfo> target,
                      const HintsVector& arguments);
  void ProcessReceiverMapForApiCall(FunctionTemplateInfoRef target,
                                    Handle<Map> receiver);
  void ProcessBuiltinCall(Handle<SharedFunctionInfo> target,
                          base::Optional<Hints> new_target,
                          const HintsVector& arguments,
                          SpeculationMode speculation_mode,
                          MissingArgumentsPolicy padding, Hints* result_hints);

  void ProcessJump(interpreter::BytecodeArrayIterator* iterator);

  void ProcessKeyedPropertyAccess(Hints* receiver, Hints const& key,
                                  FeedbackSlot slot, AccessMode access_mode,
                                  bool honor_bailout_on_uninitialized);
  void ProcessNamedPropertyAccess(Hints* receiver, NameRef const& name,
                                  FeedbackSlot slot, AccessMode access_mode);
  void ProcessNamedAccess(Hints* receiver, NamedAccessFeedback const& feedback,
                          AccessMode access_mode, Hints* result_hints);
  void ProcessElementAccess(Hints const& receiver, Hints const& key,
                            ElementAccessFeedback const& feedback,
                            AccessMode access_mode);

  void ProcessModuleVariableAccess(
      interpreter::BytecodeArrayIterator* iterator);

  void ProcessHintsForObjectCreate(Hints const& prototype);
  void ProcessMapHintsForPromises(Hints const& receiver_hints);
  void ProcessHintsForPromiseResolve(Hints const& resolution_hints);
  void ProcessHintsForHasInPrototypeChain(Hints const& instance_hints);
  void ProcessHintsForRegExpTest(Hints const& regexp_hints);
  PropertyAccessInfo ProcessMapForRegExpTest(MapRef map);
  void ProcessHintsForFunctionBind(Hints const& receiver_hints);
  void ProcessHintsForObjectGetPrototype(Hints const& object_hints);
  void ProcessConstantForOrdinaryHasInstance(HeapObjectRef const& constructor,
                                             bool* walk_prototypes);
  void ProcessConstantForInstanceOf(ObjectRef const& constant,
                                    bool* walk_prototypes);
  void ProcessHintsForOrdinaryHasInstance(Hints const& constructor_hints,
                                          Hints const& instance_hints);

  void ProcessGlobalAccess(FeedbackSlot slot, bool is_load);

  void ProcessCompareOperation(FeedbackSlot slot);
  void ProcessForIn(FeedbackSlot slot);
  void ProcessUnaryOrBinaryOperation(FeedbackSlot slot,
                                     bool honor_bailout_on_uninitialized);

  PropertyAccessInfo ProcessMapForNamedPropertyAccess(
      Hints* receiver, MapRef receiver_map, NameRef const& name,
      AccessMode access_mode, base::Optional<JSObjectRef> concrete_receiver,
      Hints* result_hints);

  void ProcessCreateContext(interpreter::BytecodeArrayIterator* iterator,
                            int scopeinfo_operand_index);

  enum ContextProcessingMode {
    kIgnoreSlot,
    kSerializeSlot,
  };

  void ProcessContextAccess(Hints const& context_hints, int slot, int depth,
                            ContextProcessingMode mode,
                            Hints* result_hints = nullptr);
  void ProcessImmutableLoad(ContextRef const& context, int slot,
                            ContextProcessingMode mode,
                            Hints* new_accumulator_hints);
  void ProcessLdaLookupGlobalSlot(interpreter::BytecodeArrayIterator* iterator);
  void ProcessLdaLookupContextSlot(
      interpreter::BytecodeArrayIterator* iterator);

  // Performs extension lookups for [0, depth) like
  // BytecodeGraphBuilder::CheckContextExtensions().
  void ProcessCheckContextExtensions(int depth);

  Hints RunChildSerializer(CompilationSubject function,
                           base::Optional<Hints> new_target,
                           const HintsVector& arguments,
                           MissingArgumentsPolicy padding);

  // When (forward-)branching bytecodes are encountered, e.g. a conditional
  // jump, we call ContributeToJumpTargetEnvironment to "remember" the current
  // environment, associated with the jump target offset. When serialization
  // eventually reaches that offset, we call IncorporateJumpTargetEnvironment to
  // merge that environment back into whatever is the current environment then.
  // Note: Since there may be multiple jumps to the same target,
  // ContributeToJumpTargetEnvironment may actually do a merge as well.
  void ContributeToJumpTargetEnvironment(int target_offset);
  void IncorporateJumpTargetEnvironment(int target_offset);

  VirtualClosure function() const { return function_; }

  Hints& return_value_hints() { return return_value_hints_; }

  Handle<FeedbackVector> feedback_vector() const;
  Handle<BytecodeArray> bytecode_array() const;
  BytecodeAnalysis const& GetBytecodeAnalysis(
      SerializationPolicy policy = SerializationPolicy::kAssumeSerialized);

  JSHeapBroker* broker() const { return broker_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Zone* zone() { return zone_scope_.zone(); }
  Environment* environment() const { return environment_; }
  SerializerForBackgroundCompilationFlags flags() const { return flags_; }
  BailoutId osr_offset() const { return osr_offset_; }

  JSHeapBroker* const broker_;
  CompilationDependencies* const dependencies_;
  ZoneStats::Scope zone_scope_;
  SerializerForBackgroundCompilationFlags const flags_;
  // Instead of storing the virtual_closure here, we could extract it from the
  // {closure_hints_} but that would be cumbersome.
  VirtualClosure const function_;
  BailoutId const osr_offset_;
  ZoneUnorderedMap<int, Environment*> jump_target_environments_;
  Environment* const environment_;
  HintsVector const arguments_;
  Hints return_value_hints_;
  Hints closure_hints_;

  int nesting_level_ = 0;
};

void RunSerializerForBackgroundCompilation(
    ZoneStats* zone_stats, JSHeapBroker* broker,
    CompilationDependencies* dependencies, Handle<JSFunction> closure,
    SerializerForBackgroundCompilationFlags flags, BailoutId osr_offset) {
  SerializerForBackgroundCompilation serializer(
      zone_stats, broker, dependencies, closure, flags, osr_offset);
  serializer.Run();
}

using BytecodeArrayIterator = interpreter::BytecodeArrayIterator;

VirtualClosure::VirtualClosure(Handle<SharedFunctionInfo> shared,
                               Handle<FeedbackVector> feedback_vector,
                               Hints const& context_hints)
    : shared_(shared),
      feedback_vector_(feedback_vector),
      context_hints_(context_hints) {
  // The checked invariant rules out recursion and thus avoids complexity.
  CHECK(context_hints_.virtual_closures().IsEmpty());
}

VirtualClosure::VirtualClosure(Handle<JSFunction> function, Isolate* isolate,
                               Zone* zone)
    : shared_(handle(function->shared(), isolate)),
      feedback_vector_(function->feedback_vector(), isolate),
      context_hints_(
          Hints::SingleConstant(handle(function->context(), isolate), zone)) {
  // The checked invariant rules out recursion and thus avoids complexity.
  CHECK(context_hints_.virtual_closures().IsEmpty());
}

CompilationSubject::CompilationSubject(Handle<JSFunction> closure,
                                       Isolate* isolate, Zone* zone)
    : virtual_closure_(closure, isolate, zone), closure_(closure) {
  CHECK(closure->has_feedback_vector());
}

Hints Hints::Copy(Zone* zone) const {
  if (!IsAllocated()) return *this;
  Hints result;
  result.EnsureAllocated(zone);
  result.impl_->constants_ = impl_->constants_;
  result.impl_->maps_ = impl_->maps_;
  result.impl_->virtual_contexts_ = impl_->virtual_contexts_;
  result.impl_->virtual_closures_ = impl_->virtual_closures_;
  result.impl_->virtual_bound_functions_ = impl_->virtual_bound_functions_;
  return result;
}

bool Hints::operator==(Hints const& other) const {
  if (impl_ == other.impl_) return true;
  if (IsEmpty() && other.IsEmpty()) return true;
  return IsAllocated() && other.IsAllocated() &&
         constants() == other.constants() &&
         virtual_closures() == other.virtual_closures() &&
         maps() == other.maps() &&
         virtual_contexts() == other.virtual_contexts() &&
         virtual_bound_functions() == other.virtual_bound_functions();
}

bool Hints::operator!=(Hints const& other) const { return !(*this == other); }

#ifdef ENABLE_SLOW_DCHECKS
bool Hints::Includes(Hints const& other) const {
  if (impl_ == other.impl_ || other.IsEmpty()) return true;
  return IsAllocated() && constants().Includes(other.constants()) &&
         virtual_closures().Includes(other.virtual_closures()) &&
         maps().Includes(other.maps());
}
#endif

Hints Hints::SingleConstant(Handle<Object> constant, Zone* zone) {
  Hints result;
  result.AddConstant(constant, zone, nullptr);
  return result;
}

Hints Hints::SingleMap(Handle<Map> map, Zone* zone) {
  Hints result;
  result.AddMap(map, zone, nullptr);
  return result;
}

ConstantsSet Hints::constants() const {
  return IsAllocated() ? impl_->constants_ : ConstantsSet();
}

MapsSet Hints::maps() const { return IsAllocated() ? impl_->maps_ : MapsSet(); }

VirtualClosuresSet Hints::virtual_closures() const {
  return IsAllocated() ? impl_->virtual_closures_ : VirtualClosuresSet();
}

VirtualContextsSet Hints::virtual_contexts() const {
  return IsAllocated() ? impl_->virtual_contexts_ : VirtualContextsSet();
}

VirtualBoundFunctionsSet Hints::virtual_bound_functions() const {
  return IsAllocated() ? impl_->virtual_bound_functions_
                       : VirtualBoundFunctionsSet();
}

void Hints::AddVirtualContext(VirtualContext const& virtual_context, Zone* zone,
                              JSHeapBroker* broker) {
  EnsureAllocated(zone);
  if (impl_->virtual_contexts_.Size() >= kMaxHintsSize) {
    TRACE_BROKER_MISSING(broker,
                         "opportunity - limit for virtual contexts reached.");
    return;
  }
  impl_->virtual_contexts_.Add(virtual_context, impl_->zone_);
}

void Hints::AddConstant(Handle<Object> constant, Zone* zone,
                        JSHeapBroker* broker) {
  EnsureAllocated(zone);
  if (impl_->constants_.Size() >= kMaxHintsSize) {
    TRACE_BROKER_MISSING(broker, "opportunity - limit for constants reached.");
    return;
  }
  impl_->constants_.Add(constant, impl_->zone_);
}

void Hints::AddMap(Handle<Map> map, Zone* zone, JSHeapBroker* broker,
                   bool check_zone_equality) {
  EnsureAllocated(zone, check_zone_equality);
  if (impl_->maps_.Size() >= kMaxHintsSize) {
    TRACE_BROKER_MISSING(broker, "opportunity - limit for maps reached.");
    return;
  }
  impl_->maps_.Add(map, impl_->zone_);
}

void Hints::AddVirtualClosure(VirtualClosure const& virtual_closure, Zone* zone,
                              JSHeapBroker* broker) {
  EnsureAllocated(zone);
  if (impl_->virtual_closures_.Size() >= kMaxHintsSize) {
    TRACE_BROKER_MISSING(broker,
                         "opportunity - limit for virtual closures reached.");
    return;
  }
  impl_->virtual_closures_.Add(virtual_closure, impl_->zone_);
}

void Hints::AddVirtualBoundFunction(VirtualBoundFunction const& bound_function,
                                    Zone* zone, JSHeapBroker* broker) {
  EnsureAllocated(zone);
  if (impl_->virtual_bound_functions_.Size() >= kMaxHintsSize) {
    TRACE_BROKER_MISSING(
        broker, "opportunity - limit for virtual bound functions reached.");
    return;
  }
  // TODO(mslekova): Consider filtering the hints in the added bound function,
  // for example: a) Remove any non-JS(Bound)Function constants, b) Truncate the
  // argument vector the formal parameter count.
  impl_->virtual_bound_functions_.Add(bound_function, impl_->zone_);
}

void Hints::Add(Hints const& other, Zone* zone, JSHeapBroker* broker) {
  if (impl_ == other.impl_ || other.IsEmpty()) return;
  EnsureAllocated(zone);
  if (!Union(other)) {
    TRACE_BROKER_MISSING(broker, "opportunity - hints limit reached.");
  }
}

Hints Hints::CopyToParentZone(Zone* zone, JSHeapBroker* broker) const {
  if (!IsAllocated()) return *this;

  Hints result;

  for (auto const& x : constants()) result.AddConstant(x, zone, broker);
  for (auto const& x : maps()) result.AddMap(x, zone, broker);
  for (auto const& x : virtual_contexts())
    result.AddVirtualContext(x, zone, broker);

  // Adding hints from a child serializer run means copying data out from
  // a zone that's being destroyed. VirtualClosures and VirtualBoundFunction
  // have zone allocated data, so we've got to make a deep copy to eliminate
  // traces of the dying zone.
  for (auto const& x : virtual_closures()) {
    VirtualClosure new_virtual_closure(
        x.shared(), x.feedback_vector(),
        x.context_hints().CopyToParentZone(zone, broker));
    result.AddVirtualClosure(new_virtual_closure, zone, broker);
  }
  for (auto const& x : virtual_bound_functions()) {
    HintsVector new_arguments_hints(zone);
    for (auto hint : x.bound_arguments) {
      new_arguments_hints.push_back(hint.CopyToParentZone(zone, broker));
    }
    VirtualBoundFunction new_bound_function(
        x.bound_target.CopyToParentZone(zone, broker), new_arguments_hints);
    result.AddVirtualBoundFunction(new_bound_function, zone, broker);
  }

  return result;
}

bool Hints::IsEmpty() const {
  if (!IsAllocated()) return true;
  return constants().IsEmpty() && maps().IsEmpty() &&
         virtual_closures().IsEmpty() && virtual_contexts().IsEmpty() &&
         virtual_bound_functions().IsEmpty();
}

std::ostream& operator<<(std::ostream& out,
                         const VirtualContext& virtual_context) {
  out << "Distance " << virtual_context.distance << " from "
      << Brief(*virtual_context.context) << std::endl;
  return out;
}

std::ostream& operator<<(std::ostream& out, const Hints& hints);

std::ostream& operator<<(std::ostream& out,
                         const VirtualClosure& virtual_closure) {
  out << Brief(*virtual_closure.shared()) << std::endl;
  out << Brief(*virtual_closure.feedback_vector()) << std::endl;
  !virtual_closure.context_hints().IsEmpty() &&
      out << virtual_closure.context_hints() << "):" << std::endl;
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const VirtualBoundFunction& virtual_bound_function) {
  out << std::endl << "    Target: " << virtual_bound_function.bound_target;
  out << "    Arguments:" << std::endl;
  for (auto hint : virtual_bound_function.bound_arguments) {
    out << "    " << hint;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const Hints& hints) {
  out << "(impl_ = " << hints.impl_ << ")\n";
  for (Handle<Object> constant : hints.constants()) {
    out << "  constant " << Brief(*constant) << std::endl;
  }
  for (Handle<Map> map : hints.maps()) {
    out << "  map " << Brief(*map) << std::endl;
  }
  for (VirtualClosure const& virtual_closure : hints.virtual_closures()) {
    out << "  virtual closure " << virtual_closure << std::endl;
  }
  for (VirtualContext const& virtual_context : hints.virtual_contexts()) {
    out << "  virtual context " << virtual_context << std::endl;
  }
  for (VirtualBoundFunction const& virtual_bound_function :
       hints.virtual_bound_functions()) {
    out << "  virtual bound function " << virtual_bound_function << std::endl;
  }
  return out;
}

void Hints::Reset(Hints* other, Zone* zone) {
  other->EnsureShareable(zone);
  *this = *other;
  DCHECK(IsAllocated());
}

class SerializerForBackgroundCompilation::Environment : public ZoneObject {
 public:
  Environment(Zone* zone, CompilationSubject function);
  Environment(Zone* zone, Isolate* isolate, CompilationSubject function,
              base::Optional<Hints> new_target, const HintsVector& arguments,
              MissingArgumentsPolicy padding);

  bool IsDead() const { return !alive_; }

  void Kill() {
    DCHECK(!IsDead());
    alive_ = false;
    DCHECK(IsDead());
  }

  void Resurrect() {
    DCHECK(IsDead());
    alive_ = true;
    DCHECK(!IsDead());
  }

  // Merge {other} into {this} environment (leaving {other} unmodified).
  void Merge(Environment* other, Zone* zone, JSHeapBroker* broker);

  Hints const& current_context_hints() const { return current_context_hints_; }
  Hints const& accumulator_hints() const { return accumulator_hints_; }

  Hints& current_context_hints() { return current_context_hints_; }
  Hints& accumulator_hints() { return accumulator_hints_; }
  Hints& register_hints(interpreter::Register reg);

 private:
  friend std::ostream& operator<<(std::ostream& out, const Environment& env);

  Hints current_context_hints_;
  Hints accumulator_hints_;

  HintsVector parameters_hints_;  // First parameter is the receiver.
  HintsVector locals_hints_;

  bool alive_ = true;
};

SerializerForBackgroundCompilation::Environment::Environment(
    Zone* zone, CompilationSubject function)
    : parameters_hints_(function.virtual_closure()
                            .shared()
                            ->GetBytecodeArray()
                            .parameter_count(),
                        Hints(), zone),
      locals_hints_(function.virtual_closure()
                        .shared()
                        ->GetBytecodeArray()
                        .register_count(),
                    Hints(), zone) {
  // Consume the virtual_closure's context hint information.
  current_context_hints_ = function.virtual_closure().context_hints();
}

SerializerForBackgroundCompilation::Environment::Environment(
    Zone* zone, Isolate* isolate, CompilationSubject function,
    base::Optional<Hints> new_target, const HintsVector& arguments,
    MissingArgumentsPolicy padding)
    : Environment(zone, function) {
  // Set the hints for the actually passed arguments, at most up to
  // the parameter_count.
  for (size_t i = 0; i < std::min(arguments.size(), parameters_hints_.size());
       ++i) {
    parameters_hints_[i] = arguments[i];
  }

  if (padding == kMissingArgumentsAreUndefined) {
    Hints const undefined_hint =
        Hints::SingleConstant(isolate->factory()->undefined_value(), zone);
    for (size_t i = arguments.size(); i < parameters_hints_.size(); ++i) {
      parameters_hints_[i] = undefined_hint;
    }
  } else {
    DCHECK_EQ(padding, kMissingArgumentsAreUnknown);
  }

  // Set hints for new_target.
  interpreter::Register new_target_reg =
      function.virtual_closure()
          .shared()
          ->GetBytecodeArray()
          .incoming_new_target_or_generator_register();
  if (new_target_reg.is_valid()) {
    Hints& hints = register_hints(new_target_reg);
    CHECK(hints.IsEmpty());
    if (new_target.has_value()) hints = *new_target;
  }
}

Hints& SerializerForBackgroundCompilation::register_hints(
    interpreter::Register reg) {
  if (reg.is_function_closure()) return closure_hints_;
  return environment()->register_hints(reg);
}

Hints& SerializerForBackgroundCompilation::Environment::register_hints(
    interpreter::Register reg) {
  if (reg.is_current_context()) return current_context_hints_;
  if (reg.is_parameter()) {
    return parameters_hints_[reg.ToParameterIndex(
        static_cast<int>(parameters_hints_.size()))];
  }
  DCHECK(!reg.is_function_closure());
  CHECK_LT(reg.index(), locals_hints_.size());
  return locals_hints_[reg.index()];
}

void SerializerForBackgroundCompilation::Environment::Merge(
    Environment* other, Zone* zone, JSHeapBroker* broker) {
  // {other} is guaranteed to have the same layout because it comes from an
  // earlier bytecode in the same function.
  DCHECK_EQ(parameters_hints_.size(), other->parameters_hints_.size());
  DCHECK_EQ(locals_hints_.size(), other->locals_hints_.size());

  if (IsDead()) {
    parameters_hints_ = other->parameters_hints_;
    locals_hints_ = other->locals_hints_;
    current_context_hints_ = other->current_context_hints_;
    accumulator_hints_ = other->accumulator_hints_;
    Resurrect();
  } else {
    for (size_t i = 0; i < parameters_hints_.size(); ++i) {
      parameters_hints_[i].Merge(other->parameters_hints_[i], zone, broker);
    }
    for (size_t i = 0; i < locals_hints_.size(); ++i) {
      locals_hints_[i].Merge(other->locals_hints_[i], zone, broker);
    }
    current_context_hints_.Merge(other->current_context_hints_, zone, broker);
    accumulator_hints_.Merge(other->accumulator_hints_, zone, broker);
  }

  CHECK(!IsDead());
}

bool Hints::Union(Hints const& other) {
  CHECK(IsAllocated());
  if (impl_->constants_.Size() + other.constants().Size() > kMaxHintsSize ||
      impl_->maps_.Size() + other.maps().Size() > kMaxHintsSize ||
      impl_->virtual_closures_.Size() + other.virtual_closures().Size() >
          kMaxHintsSize ||
      impl_->virtual_contexts_.Size() + other.virtual_contexts().Size() >
          kMaxHintsSize ||
      impl_->virtual_bound_functions_.Size() +
              other.virtual_bound_functions().Size() >
          kMaxHintsSize) {
    return false;
  }
  Zone* zone = impl_->zone_;
  impl_->constants_.Union(other.constants(), zone);
  impl_->maps_.Union(other.maps(), zone);
  impl_->virtual_closures_.Union(other.virtual_closures(), zone);
  impl_->virtual_contexts_.Union(other.virtual_contexts(), zone);
  impl_->virtual_bound_functions_.Union(other.virtual_bound_functions(), zone);
  return true;
}

void Hints::Merge(Hints const& other, Zone* zone, JSHeapBroker* broker) {
  if (impl_ == other.impl_) {
    return;
  }
  if (!IsAllocated()) {
    *this = other.Copy(zone);
    DCHECK(IsAllocated());
    return;
  }
  *this = this->Copy(zone);
  if (!Union(other)) {
    TRACE_BROKER_MISSING(broker, "opportunity - hints limit reached.");
  }
  DCHECK(IsAllocated());
}

std::ostream& operator<<(
    std::ostream& out,
    const SerializerForBackgroundCompilation::Environment& env) {
  std::ostringstream output_stream;

  if (env.IsDead()) {
    output_stream << "dead\n";
  } else {
    output_stream << "alive\n";
    for (size_t i = 0; i < env.parameters_hints_.size(); ++i) {
      Hints const& hints = env.parameters_hints_[i];
      if (!hints.IsEmpty()) {
        if (i == 0) {
          output_stream << "Hints for <this>: ";
        } else {
          output_stream << "Hints for a" << i - 1 << ": ";
        }
        output_stream << hints;
      }
    }
    for (size_t i = 0; i < env.locals_hints_.size(); ++i) {
      Hints const& hints = env.locals_hints_[i];
      if (!hints.IsEmpty()) {
        output_stream << "Hints for r" << i << ": " << hints;
      }
    }
  }

  if (!env.current_context_hints().IsEmpty()) {
    output_stream << "Hints for <context>: " << env.current_context_hints();
  }
  if (!env.accumulator_hints().IsEmpty()) {
    output_stream << "Hints for <accumulator>: " << env.accumulator_hints();
  }

  out << output_stream.str();
  return out;
}

SerializerForBackgroundCompilation::SerializerForBackgroundCompilation(
    ZoneStats* zone_stats, JSHeapBroker* broker,
    CompilationDependencies* dependencies, Handle<JSFunction> closure,
    SerializerForBackgroundCompilationFlags flags, BailoutId osr_offset)
    : broker_(broker),
      dependencies_(dependencies),
      zone_scope_(zone_stats, ZONE_NAME),
      flags_(flags),
      function_(closure, broker->isolate(), zone()),
      osr_offset_(osr_offset),
      jump_target_environments_(zone()),
      environment_(new (zone()) Environment(
          zone(), CompilationSubject(closure, broker_->isolate(), zone()))),
      arguments_(zone()) {
  closure_hints_.AddConstant(closure, zone(), broker_);
  JSFunctionRef(broker, closure).Serialize();

  TRACE_BROKER(broker_, "Hints for <closure>: " << closure_hints_);
  TRACE_BROKER(broker_, "Initial environment:\n" << *environment_);
}

SerializerForBackgroundCompilation::SerializerForBackgroundCompilation(
    ZoneStats* zone_stats, JSHeapBroker* broker,
    CompilationDependencies* dependencies, CompilationSubject function,
    base::Optional<Hints> new_target, const HintsVector& arguments,
    MissingArgumentsPolicy padding,
    SerializerForBackgroundCompilationFlags flags, int nesting_level)
    : broker_(broker),
      dependencies_(dependencies),
      zone_scope_(zone_stats, ZONE_NAME),
      flags_(flags),
      function_(function.virtual_closure()),
      osr_offset_(BailoutId::None()),
      jump_target_environments_(zone()),
      environment_(new (zone())
                       Environment(zone(), broker_->isolate(), function,
                                   new_target, arguments, padding)),
      arguments_(arguments),
      nesting_level_(nesting_level) {
  Handle<JSFunction> closure;
  if (function.closure().ToHandle(&closure)) {
    closure_hints_.AddConstant(closure, zone(), broker);
    JSFunctionRef(broker, closure).Serialize();
  } else {
    closure_hints_.AddVirtualClosure(function.virtual_closure(), zone(),
                                     broker);
  }

  TRACE_BROKER(broker_, "Hints for <closure>: " << closure_hints_);
  TRACE_BROKER(broker_, "Initial environment:\n" << *environment_);
}

bool SerializerForBackgroundCompilation::BailoutOnUninitialized(
    ProcessedFeedback const& feedback) {
  DCHECK(!environment()->IsDead());
  if (!(flags() &
        SerializerForBackgroundCompilationFlag::kBailoutOnUninitialized)) {
    return false;
  }
  if (!osr_offset().IsNone()) {
    // Exclude OSR from this optimization because we might end up skipping the
    // OSR entry point. TODO(neis): Support OSR?
    return false;
  }
  if (feedback.IsInsufficient()) {
    environment()->Kill();
    return true;
  }
  return false;
}

Hints SerializerForBackgroundCompilation::Run() {
  TraceScope tracer(broker(), this, "SerializerForBackgroundCompilation::Run");
  if (nesting_level_ >= FLAG_max_serializer_nesting) {
    TRACE_BROKER_MISSING(
        broker(),
        "opportunity - Reached max nesting level for "
        "SerializerForBackgroundCompilation::Run, bailing out.\n");
    return Hints();
  }

  TRACE_BROKER_MEMORY(broker(), "[serializer start] Broker zone usage: "
                                    << broker()->zone()->allocation_size());
  SharedFunctionInfoRef shared(broker(), function().shared());
  FeedbackVectorRef feedback_vector_ref(broker(), feedback_vector());
  if (!broker()->ShouldBeSerializedForCompilation(shared, feedback_vector_ref,
                                                  arguments_)) {
    TRACE_BROKER(broker(),
                 "opportunity - Already ran serializer for SharedFunctionInfo "
                     << Brief(*shared.object()) << ", bailing out.\n");
    return Hints();
  }

  {
    HintsVector arguments_copy_in_broker_zone(broker()->zone());
    for (auto const& hints : arguments_) {
      arguments_copy_in_broker_zone.push_back(
          hints.CopyToParentZone(broker()->zone(), broker()));
    }
    broker()->SetSerializedForCompilation(shared, feedback_vector_ref,
                                          arguments_copy_in_broker_zone);
  }

  // We eagerly call the {EnsureSourcePositionsAvailable} for all serialized
  // SFIs while still on the main thread. Source positions will later be used
  // by JSInliner::ReduceJSCall.
  if (flags() &
      SerializerForBackgroundCompilationFlag::kCollectSourcePositions) {
    SharedFunctionInfo::EnsureSourcePositionsAvailable(broker()->isolate(),
                                                       shared.object());
  }

  feedback_vector_ref.Serialize();
  TraverseBytecode();

  if (return_value_hints().IsEmpty()) {
    TRACE_BROKER(broker(), "Return value hints: none");
  } else {
    TRACE_BROKER(broker(), "Return value hints: " << return_value_hints());
  }
  TRACE_BROKER_MEMORY(broker(), "[serializer end] Broker zone usage: "
                                    << broker()->zone()->allocation_size());
  return return_value_hints();
}

class HandlerRangeMatcher {
 public:
  HandlerRangeMatcher(BytecodeArrayIterator const& bytecode_iterator,
                      Handle<BytecodeArray> bytecode_array)
      : bytecode_iterator_(bytecode_iterator) {
    HandlerTable table(*bytecode_array);
    for (int i = 0, n = table.NumberOfRangeEntries(); i < n; ++i) {
      ranges_.insert({table.GetRangeStart(i), table.GetRangeEnd(i),
                      table.GetRangeHandler(i)});
    }
    ranges_iterator_ = ranges_.cbegin();
  }

  using OffsetReporter = std::function<void(int handler_offset)>;

  void HandlerOffsetForCurrentPosition(const OffsetReporter& offset_reporter) {
    CHECK(!bytecode_iterator_.done());
    const int current_offset = bytecode_iterator_.current_offset();

    // Remove outdated try ranges from the stack.
    while (!stack_.empty()) {
      const int end = stack_.top().end;
      if (end < current_offset) {
        stack_.pop();
      } else {
        break;
      }
    }

    // Advance the iterator and maintain the stack.
    while (ranges_iterator_ != ranges_.cend() &&
           ranges_iterator_->start <= current_offset) {
      if (ranges_iterator_->end >= current_offset) {
        stack_.push(*ranges_iterator_);
        if (ranges_iterator_->start == current_offset) {
          offset_reporter(ranges_iterator_->handler);
        }
      }
      ranges_iterator_++;
    }

    if (!stack_.empty() && stack_.top().start < current_offset) {
      offset_reporter(stack_.top().handler);
    }
  }

 private:
  BytecodeArrayIterator const& bytecode_iterator_;

  struct Range {
    int start;
    int end;
    int handler;
    friend bool operator<(const Range& a, const Range& b) {
      if (a.start < b.start) return true;
      if (a.start == b.start) {
        if (a.end < b.end) return true;
        CHECK_GT(a.end, b.end);
      }
      return false;
    }
  };
  std::set<Range> ranges_;
  std::set<Range>::const_iterator ranges_iterator_;
  std::stack<Range> stack_;
};

Handle<FeedbackVector> SerializerForBackgroundCompilation::feedback_vector()
    const {
  return function().feedback_vector();
}

Handle<BytecodeArray> SerializerForBackgroundCompilation::bytecode_array()
    const {
  return handle(function().shared()->GetBytecodeArray(), broker()->isolate());
}

BytecodeAnalysis const& SerializerForBackgroundCompilation::GetBytecodeAnalysis(
    SerializationPolicy policy) {
  return broker()->GetBytecodeAnalysis(
      bytecode_array(), osr_offset(),
      flags() &
          SerializerForBackgroundCompilationFlag::kAnalyzeEnvironmentLiveness,
      policy);
}

void SerializerForBackgroundCompilation::TraverseBytecode() {
  BytecodeAnalysis const& bytecode_analysis =
      GetBytecodeAnalysis(SerializationPolicy::kSerializeIfNeeded);
  BytecodeArrayRef(broker(), bytecode_array()).SerializeForCompilation();

  BytecodeArrayIterator iterator(bytecode_array());
  HandlerRangeMatcher try_start_matcher(iterator, bytecode_array());

  bool has_one_shot_bytecode = false;
  for (; !iterator.done(); iterator.Advance()) {
    has_one_shot_bytecode =
        has_one_shot_bytecode ||
        interpreter::Bytecodes::IsOneShotBytecode(iterator.current_bytecode());

    int const current_offset = iterator.current_offset();

    // TODO(mvstanton): we might want to ignore the current environment if we
    // are at the start of a catch handler.
    IncorporateJumpTargetEnvironment(current_offset);

    TRACE_BROKER(broker(),
                 "Handling bytecode: " << current_offset << "  "
                                       << iterator.current_bytecode());
    TRACE_BROKER(broker(), "Current environment: " << *environment());

    if (environment()->IsDead()) {
      continue;  // Skip this bytecode since TF won't generate code for it.
    }

    auto save_handler_environments = [&](int handler_offset) {
      auto it = jump_target_environments_.find(handler_offset);
      if (it == jump_target_environments_.end()) {
        ContributeToJumpTargetEnvironment(handler_offset);
        TRACE_BROKER(broker(),
                     "Handler offset for current pos: " << handler_offset);
      }
    };
    try_start_matcher.HandlerOffsetForCurrentPosition(
        save_handler_environments);

    if (bytecode_analysis.IsLoopHeader(current_offset)) {
      // Graph builder might insert jumps to resume targets in the loop body.
      LoopInfo const& loop_info =
          bytecode_analysis.GetLoopInfoFor(current_offset);
      for (const auto& target : loop_info.resume_jump_targets()) {
        ContributeToJumpTargetEnvironment(target.target_offset());
      }
    }

    switch (iterator.current_bytecode()) {
#define DEFINE_BYTECODE_CASE(name)     \
  case interpreter::Bytecode::k##name: \
    Visit##name(&iterator);            \
    break;
      SUPPORTED_BYTECODE_LIST(DEFINE_BYTECODE_CASE)
#undef DEFINE_BYTECODE_CASE
      default:
        break;
    }
  }

  if (has_one_shot_bytecode) {
    broker()->isolate()->CountUsage(
        v8::Isolate::UseCounterFeature::kOptimizedFunctionWithOneShotBytecode);
  }
}

void SerializerForBackgroundCompilation::VisitGetIterator(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot load_slot = iterator->GetSlotOperand(1);
  FeedbackSlot call_slot = iterator->GetSlotOperand(2);

  Handle<Name> name = broker()->isolate()->factory()->iterator_symbol();
  ProcessNamedPropertyAccess(receiver, NameRef(broker(), name), load_slot,
                             AccessMode::kLoad);
  if (environment()->IsDead()) return;

  Hints callee;
  HintsVector args = PrepareArgumentsHints(receiver);

  ProcessCallOrConstruct(callee, base::nullopt, &args, call_slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitGetSuperConstructor(
    BytecodeArrayIterator* iterator) {
  interpreter::Register dst = iterator->GetRegisterOperand(0);
  Hints result_hints;
  for (auto constant : environment()->accumulator_hints().constants()) {
    // For JSNativeContextSpecialization::ReduceJSGetSuperConstructor.
    if (!constant->IsJSFunction()) continue;
    MapRef map(broker(),
               handle(HeapObject::cast(*constant).map(), broker()->isolate()));
    map.SerializePrototype();
    ObjectRef proto = map.prototype();
    if (proto.IsHeapObject() && proto.AsHeapObject().map().is_constructor()) {
      result_hints.AddConstant(proto.object(), zone(), broker());
    }
  }
  register_hints(dst) = result_hints;
}

void SerializerForBackgroundCompilation::VisitGetTemplateObject(
    BytecodeArrayIterator* iterator) {
  TemplateObjectDescriptionRef description(
      broker(), iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  FeedbackSource source(feedback_vector(), slot);
  SharedFunctionInfoRef shared(broker(), function().shared());
  JSArrayRef template_object = shared.GetTemplateObject(
      description, source, SerializationPolicy::kSerializeIfNeeded);
  environment()->accumulator_hints() =
      Hints::SingleConstant(template_object.object(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaTrue(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints() = Hints::SingleConstant(
      broker()->isolate()->factory()->true_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaFalse(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints() = Hints::SingleConstant(
      broker()->isolate()->factory()->false_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaTheHole(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints() = Hints::SingleConstant(
      broker()->isolate()->factory()->the_hole_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaUndefined(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints() = Hints::SingleConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaNull(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints() = Hints::SingleConstant(
      broker()->isolate()->factory()->null_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaZero(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints() = Hints::SingleConstant(
      handle(Smi::FromInt(0), broker()->isolate()), zone());
}

void SerializerForBackgroundCompilation::VisitLdaSmi(
    BytecodeArrayIterator* iterator) {
  Handle<Smi> smi(Smi::FromInt(iterator->GetImmediateOperand(0)),
                  broker()->isolate());
  environment()->accumulator_hints() = Hints::SingleConstant(smi, zone());
}

void SerializerForBackgroundCompilation::VisitInvokeIntrinsic(
    BytecodeArrayIterator* iterator) {
  Runtime::FunctionId functionId = iterator->GetIntrinsicIdOperand(0);
  // For JSNativeContextSpecialization::ReduceJSAsyncFunctionResolve and
  // JSNativeContextSpecialization::ReduceJSResolvePromise.
  switch (functionId) {
    case Runtime::kInlineAsyncFunctionResolve: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncFunctionResolve));
      interpreter::Register first_reg = iterator->GetRegisterOperand(1);
      size_t reg_count = iterator->GetRegisterCountOperand(2);
      CHECK_EQ(reg_count, 3);
      HintsVector args = PrepareArgumentsHints(first_reg, reg_count);
      Hints const& resolution_hints = args[1];  // The resolution object.
      ProcessHintsForPromiseResolve(resolution_hints);
      return;
    }
    case Runtime::kInlineAsyncGeneratorReject:
    case Runtime::kAsyncGeneratorReject: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncGeneratorReject));
      break;
    }
    case Runtime::kInlineAsyncGeneratorResolve:
    case Runtime::kAsyncGeneratorResolve: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncGeneratorResolve));
      break;
    }
    case Runtime::kInlineAsyncGeneratorYield:
    case Runtime::kAsyncGeneratorYield: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncGeneratorYield));
      break;
    }
    case Runtime::kInlineAsyncGeneratorAwaitUncaught:
    case Runtime::kAsyncGeneratorAwaitUncaught: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncGeneratorAwaitUncaught));
      break;
    }
    case Runtime::kInlineAsyncGeneratorAwaitCaught:
    case Runtime::kAsyncGeneratorAwaitCaught: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncGeneratorAwaitCaught));
      break;
    }
    case Runtime::kInlineAsyncFunctionAwaitUncaught:
    case Runtime::kAsyncFunctionAwaitUncaught: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncFunctionAwaitUncaught));
      break;
    }
    case Runtime::kInlineAsyncFunctionAwaitCaught:
    case Runtime::kAsyncFunctionAwaitCaught: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncFunctionAwaitCaught));
      break;
    }
    case Runtime::kInlineAsyncFunctionReject:
    case Runtime::kAsyncFunctionReject: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncFunctionReject));
      break;
    }
    case Runtime::kAsyncFunctionResolve: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kAsyncFunctionResolve));
      break;
    }
    case Runtime::kInlineCopyDataProperties:
    case Runtime::kCopyDataProperties: {
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kCopyDataProperties));
      break;
    }
    default: {
      break;
    }
  }
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitLdaConstant(
    BytecodeArrayIterator* iterator) {
  ObjectRef object(
      broker(), iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  environment()->accumulator_hints() =
      Hints::SingleConstant(object.object(), zone());
}

void SerializerForBackgroundCompilation::VisitPushContext(
    BytecodeArrayIterator* iterator) {
  register_hints(iterator->GetRegisterOperand(0))
      .Reset(&environment()->current_context_hints(), zone());
  environment()->current_context_hints().Reset(
      &environment()->accumulator_hints(), zone());
}

void SerializerForBackgroundCompilation::VisitPopContext(
    BytecodeArrayIterator* iterator) {
  environment()->current_context_hints().Reset(
      &register_hints(iterator->GetRegisterOperand(0)), zone());
}

void SerializerForBackgroundCompilation::ProcessImmutableLoad(
    ContextRef const& context_ref, int slot, ContextProcessingMode mode,
    Hints* result_hints) {
  DCHECK_EQ(mode, kSerializeSlot);
  base::Optional<ObjectRef> slot_value =
      context_ref.get(slot, SerializationPolicy::kSerializeIfNeeded);

  // If requested, record the object as a hint for the result value.
  if (result_hints != nullptr && slot_value.has_value()) {
    result_hints->AddConstant(slot_value.value().object(), zone(), broker());
  }
}

void SerializerForBackgroundCompilation::ProcessContextAccess(
    Hints const& context_hints, int slot, int depth, ContextProcessingMode mode,
    Hints* result_hints) {
  // This function is for JSContextSpecialization::ReduceJSLoadContext and
  // ReduceJSStoreContext. Those reductions attempt to eliminate as many
  // loads as possible by making use of constant Context objects. In the
  // case of an immutable load, ReduceJSLoadContext even attempts to load
  // the value at {slot}, replacing the load with a constant.
  for (auto x : context_hints.constants()) {
    if (x->IsContext()) {
      // Walk this context to the given depth and serialize the slot found.
      ContextRef context_ref(broker(), x);
      size_t remaining_depth = depth;
      context_ref = context_ref.previous(
          &remaining_depth, SerializationPolicy::kSerializeIfNeeded);
      if (remaining_depth == 0 && mode != kIgnoreSlot) {
        ProcessImmutableLoad(context_ref, slot, mode, result_hints);
      }
    }
  }
  for (auto x : context_hints.virtual_contexts()) {
    if (x.distance <= static_cast<unsigned int>(depth)) {
      ContextRef context_ref(broker(), x.context);
      size_t remaining_depth = depth - x.distance;
      context_ref = context_ref.previous(
          &remaining_depth, SerializationPolicy::kSerializeIfNeeded);
      if (remaining_depth == 0 && mode != kIgnoreSlot) {
        ProcessImmutableLoad(context_ref, slot, mode, result_hints);
      }
    }
  }
}

void SerializerForBackgroundCompilation::VisitLdaContextSlot(
    BytecodeArrayIterator* iterator) {
  Hints const& context_hints = register_hints(iterator->GetRegisterOperand(0));
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints() = new_accumulator_hints;
}

void SerializerForBackgroundCompilation::VisitLdaCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints const& context_hints = environment()->current_context_hints();
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints() = new_accumulator_hints;
}

void SerializerForBackgroundCompilation::VisitLdaImmutableContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints const& context_hints = register_hints(iterator->GetRegisterOperand(0));
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kSerializeSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints() = new_accumulator_hints;
}

void SerializerForBackgroundCompilation::VisitLdaImmutableCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints const& context_hints = environment()->current_context_hints();
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kSerializeSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints() = new_accumulator_hints;
}

void SerializerForBackgroundCompilation::ProcessModuleVariableAccess(
    BytecodeArrayIterator* iterator) {
  const int slot = Context::EXTENSION_INDEX;
  const int depth = iterator->GetUnsignedImmediateOperand(1);
  Hints const& context_hints = environment()->current_context_hints();

  Hints result_hints;
  ProcessContextAccess(context_hints, slot, depth, kSerializeSlot,
                       &result_hints);
  for (Handle<Object> constant : result_hints.constants()) {
    ObjectRef object(broker(), constant);
    // For JSTypedLowering::BuildGetModuleCell.
    if (object.IsSourceTextModule()) object.AsSourceTextModule().Serialize();
  }
}

void SerializerForBackgroundCompilation::VisitLdaModuleVariable(
    BytecodeArrayIterator* iterator) {
  ProcessModuleVariableAccess(iterator);
}

void SerializerForBackgroundCompilation::VisitStaModuleVariable(
    BytecodeArrayIterator* iterator) {
  ProcessModuleVariableAccess(iterator);
}

void SerializerForBackgroundCompilation::VisitStaLookupSlot(
    BytecodeArrayIterator* iterator) {
  ObjectRef(broker(),
            iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitStaContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints const& hints = register_hints(iterator->GetRegisterOperand(0));
  ProcessContextAccess(hints, slot, depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitStaCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints const& context_hints = environment()->current_context_hints();
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitLdar(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Reset(
      &register_hints(iterator->GetRegisterOperand(0)), zone());
}

void SerializerForBackgroundCompilation::VisitStar(
    BytecodeArrayIterator* iterator) {
  interpreter::Register reg = iterator->GetRegisterOperand(0);
  register_hints(reg).Reset(&environment()->accumulator_hints(), zone());
}

void SerializerForBackgroundCompilation::VisitMov(
    BytecodeArrayIterator* iterator) {
  interpreter::Register src = iterator->GetRegisterOperand(0);
  interpreter::Register dst = iterator->GetRegisterOperand(1);
  register_hints(dst).Reset(&register_hints(src), zone());
}

void SerializerForBackgroundCompilation::VisitCreateRegExpLiteral(
    BytecodeArrayIterator* iterator) {
  Handle<String> constant_pattern = Handle<String>::cast(
      iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  StringRef description(broker(), constant_pattern);
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  FeedbackSource source(feedback_vector(), slot);
  broker()->ProcessFeedbackForRegExpLiteral(source);
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitCreateArrayLiteral(
    BytecodeArrayIterator* iterator) {
  Handle<ArrayBoilerplateDescription> array_boilerplate_description =
      Handle<ArrayBoilerplateDescription>::cast(
          iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  ArrayBoilerplateDescriptionRef description(broker(),
                                             array_boilerplate_description);
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  FeedbackSource source(feedback_vector(), slot);
  broker()->ProcessFeedbackForArrayOrObjectLiteral(source);
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitCreateEmptyArrayLiteral(
    BytecodeArrayIterator* iterator) {
  FeedbackSlot slot = iterator->GetSlotOperand(0);
  FeedbackSource source(feedback_vector(), slot);
  broker()->ProcessFeedbackForArrayOrObjectLiteral(source);
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitCreateObjectLiteral(
    BytecodeArrayIterator* iterator) {
  Handle<ObjectBoilerplateDescription> constant_properties =
      Handle<ObjectBoilerplateDescription>::cast(
          iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  ObjectBoilerplateDescriptionRef description(broker(), constant_properties);
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  FeedbackSource source(feedback_vector(), slot);
  broker()->ProcessFeedbackForArrayOrObjectLiteral(source);
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitCreateFunctionContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext(iterator, 0);
}

void SerializerForBackgroundCompilation::VisitCreateBlockContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext(iterator, 0);
}

void SerializerForBackgroundCompilation::VisitCreateEvalContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext(iterator, 0);
}

void SerializerForBackgroundCompilation::VisitCreateWithContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext(iterator, 1);
}

void SerializerForBackgroundCompilation::VisitCreateCatchContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext(iterator, 1);
}

void SerializerForBackgroundCompilation::VisitForInNext(
    BytecodeArrayIterator* iterator) {
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessForIn(slot);
}

void SerializerForBackgroundCompilation::VisitForInPrepare(
    BytecodeArrayIterator* iterator) {
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessForIn(slot);
}

void SerializerForBackgroundCompilation::ProcessCreateContext(
    interpreter::BytecodeArrayIterator* iterator, int scopeinfo_operand_index) {
  Handle<ScopeInfo> scope_info =
      Handle<ScopeInfo>::cast(iterator->GetConstantForIndexOperand(
          scopeinfo_operand_index, broker()->isolate()));
  ScopeInfoRef scope_info_ref(broker(), scope_info);
  scope_info_ref.SerializeScopeInfoChain();

  Hints const& current_context_hints = environment()->current_context_hints();
  Hints result_hints;

  // For each constant context, we must create a virtual context from
  // it of distance one.
  for (auto x : current_context_hints.constants()) {
    if (x->IsContext()) {
      Handle<Context> as_context(Handle<Context>::cast(x));
      result_hints.AddVirtualContext(VirtualContext(1, as_context), zone(),
                                     broker());
    }
  }

  // For each virtual context, we must create a virtual context from
  // it of distance {existing distance} + 1.
  for (auto x : current_context_hints.virtual_contexts()) {
    result_hints.AddVirtualContext(VirtualContext(x.distance + 1, x.context),
                                   zone(), broker());
  }

  environment()->accumulator_hints() = result_hints;
}

void SerializerForBackgroundCompilation::VisitCreateClosure(
    BytecodeArrayIterator* iterator) {
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>::cast(
      iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  Handle<FeedbackCell> feedback_cell =
      feedback_vector()->GetClosureFeedbackCell(iterator->GetIndexOperand(1));
  FeedbackCellRef feedback_cell_ref(broker(), feedback_cell);
  Handle<Object> cell_value(feedback_cell->value(), broker()->isolate());
  ObjectRef cell_value_ref(broker(), cell_value);

  Hints result_hints;
  if (cell_value->IsFeedbackVector()) {
    VirtualClosure virtual_closure(shared,
                                   Handle<FeedbackVector>::cast(cell_value),
                                   environment()->current_context_hints());
    result_hints.AddVirtualClosure(virtual_closure, zone(), broker());
  }
  environment()->accumulator_hints() = result_hints;
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessCallVarArgs(ConvertReceiverMode::kNullOrUndefined, callee, first_reg,
                     reg_count, slot);
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver0(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);

  Hints const receiver = Hints::SingleConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
  HintsVector parameters({receiver}, zone());

  ProcessCallOrConstruct(callee, base::nullopt, &parameters, slot,
                         kMissingArgumentsAreUndefined);
}

namespace {
void PrepareArgumentsHintsInternal(Zone* zone, HintsVector* args) {}

template <typename... MoreHints>
void PrepareArgumentsHintsInternal(Zone* zone, HintsVector* args, Hints* hints,
                                   MoreHints... more) {
  hints->EnsureShareable(zone);
  args->push_back(*hints);
  PrepareArgumentsHintsInternal(zone, args, more...);
}
}  // namespace

template <typename... MoreHints>
HintsVector SerializerForBackgroundCompilation::PrepareArgumentsHints(
    Hints* hints, MoreHints... more) {
  HintsVector args(zone());
  PrepareArgumentsHintsInternal(zone(), &args, hints, more...);
  return args;
}

HintsVector SerializerForBackgroundCompilation::PrepareArgumentsHints(
    interpreter::Register first, size_t count) {
  HintsVector result(zone());
  const int reg_base = first.index();
  for (int i = 0; i < static_cast<int>(count); ++i) {
    Hints& hints = register_hints(interpreter::Register(reg_base + i));
    hints.EnsureShareable(zone());
    result.push_back(hints);
  }
  return result;
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver1(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  Hints* arg0 = &register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);

  Hints receiver = Hints::SingleConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
  HintsVector args = PrepareArgumentsHints(&receiver, arg0);

  ProcessCallOrConstruct(callee, base::nullopt, &args, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver2(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  Hints* arg0 = &register_hints(iterator->GetRegisterOperand(1));
  Hints* arg1 = &register_hints(iterator->GetRegisterOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  Hints receiver = Hints::SingleConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
  HintsVector args = PrepareArgumentsHints(&receiver, arg0, arg1);

  ProcessCallOrConstruct(callee, base::nullopt, &args, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallAnyReceiver(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessCallVarArgs(ConvertReceiverMode::kAny, callee, first_reg, reg_count,
                     slot);
}

void SerializerForBackgroundCompilation::VisitCallNoFeedback(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  ProcessCallVarArgs(ConvertReceiverMode::kAny, callee, first_reg, reg_count,
                     FeedbackSlot::Invalid());
}

void SerializerForBackgroundCompilation::VisitCallProperty(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessCallVarArgs(ConvertReceiverMode::kNotNullOrUndefined, callee,
                     first_reg, reg_count, slot);
}

void SerializerForBackgroundCompilation::VisitCallProperty0(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);

  HintsVector args = PrepareArgumentsHints(receiver);

  ProcessCallOrConstruct(callee, base::nullopt, &args, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallProperty1(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(1));
  Hints* arg0 = &register_hints(iterator->GetRegisterOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  HintsVector args = PrepareArgumentsHints(receiver, arg0);

  ProcessCallOrConstruct(callee, base::nullopt, &args, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallProperty2(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(1));
  Hints* arg0 = &register_hints(iterator->GetRegisterOperand(2));
  Hints* arg1 = &register_hints(iterator->GetRegisterOperand(3));
  FeedbackSlot slot = iterator->GetSlotOperand(4);

  HintsVector args = PrepareArgumentsHints(receiver, arg0, arg1);

  ProcessCallOrConstruct(callee, base::nullopt, &args, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallWithSpread(
    BytecodeArrayIterator* iterator) {
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessCallVarArgs(ConvertReceiverMode::kAny, callee, first_reg, reg_count,
                     slot, kMissingArgumentsAreUnknown);
}

void SerializerForBackgroundCompilation::VisitCallJSRuntime(
    BytecodeArrayIterator* iterator) {
  const int runtime_index = iterator->GetNativeContextIndexOperand(0);
  ObjectRef constant =
      broker()
          ->target_native_context()
          .get(runtime_index, SerializationPolicy::kSerializeIfNeeded)
          .value();
  Hints const callee = Hints::SingleConstant(constant.object(), zone());
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  ProcessCallVarArgs(ConvertReceiverMode::kNullOrUndefined, callee, first_reg,
                     reg_count, FeedbackSlot::Invalid());
}

Hints SerializerForBackgroundCompilation::RunChildSerializer(
    CompilationSubject function, base::Optional<Hints> new_target,
    const HintsVector& arguments, MissingArgumentsPolicy padding) {
  SerializerForBackgroundCompilation child_serializer(
      zone_scope_.zone_stats(), broker(), dependencies(), function, new_target,
      arguments, padding, flags(), nesting_level_ + 1);
  Hints result = child_serializer.Run();
  // The Hints returned by the call to Run are allocated in the zone
  // created by the child serializer. Adding those hints to a hints
  // object created in our zone will preserve the information.
  return result.CopyToParentZone(zone(), broker());
}

void SerializerForBackgroundCompilation::ProcessCalleeForCallOrConstruct(
    Callee const& callee, base::Optional<Hints> new_target,
    const HintsVector& arguments, SpeculationMode speculation_mode,
    MissingArgumentsPolicy padding, Hints* result_hints) {
  Handle<SharedFunctionInfo> shared = callee.shared(broker()->isolate());
  if (shared->IsApiFunction()) {
    ProcessApiCall(shared, arguments);
    DCHECK_NE(shared->GetInlineability(), SharedFunctionInfo::kIsInlineable);
  } else if (shared->HasBuiltinId()) {
    ProcessBuiltinCall(shared, new_target, arguments, speculation_mode, padding,
                       result_hints);
    DCHECK_NE(shared->GetInlineability(), SharedFunctionInfo::kIsInlineable);
  } else if ((flags() &
              SerializerForBackgroundCompilationFlag::kEnableTurboInlining) &&
             shared->GetInlineability() == SharedFunctionInfo::kIsInlineable &&
             callee.HasFeedbackVector()) {
    CompilationSubject subject =
        callee.ToCompilationSubject(broker()->isolate(), zone());
    result_hints->Add(
        RunChildSerializer(subject, new_target, arguments, padding), zone(),
        broker());
  }
}

namespace {
// Returns the innermost bound target and inserts all bound arguments and
// {original_arguments} into {expanded_arguments} in the appropriate order.
JSReceiverRef UnrollBoundFunction(JSBoundFunctionRef const& bound_function,
                                  JSHeapBroker* broker,
                                  const HintsVector& original_arguments,
                                  HintsVector* expanded_arguments, Zone* zone) {
  DCHECK(expanded_arguments->empty());

  JSReceiverRef target = bound_function.AsJSReceiver();
  HintsVector reversed_bound_arguments(zone);
  for (; target.IsJSBoundFunction();
       target = target.AsJSBoundFunction().bound_target_function()) {
    for (int i = target.AsJSBoundFunction().bound_arguments().length() - 1;
         i >= 0; --i) {
      Hints const arg = Hints::SingleConstant(
          target.AsJSBoundFunction().bound_arguments().get(i).object(), zone);
      reversed_bound_arguments.push_back(arg);
    }
    Hints const arg = Hints::SingleConstant(
        target.AsJSBoundFunction().bound_this().object(), zone);
    reversed_bound_arguments.push_back(arg);
  }

  expanded_arguments->insert(expanded_arguments->end(),
                             reversed_bound_arguments.rbegin(),
                             reversed_bound_arguments.rend());
  expanded_arguments->insert(expanded_arguments->end(),
                             original_arguments.begin(),
                             original_arguments.end());

  return target;
}
}  // namespace

void SerializerForBackgroundCompilation::ProcessCalleeForCallOrConstruct(
    Handle<Object> callee, base::Optional<Hints> new_target,
    const HintsVector& arguments, SpeculationMode speculation_mode,
    MissingArgumentsPolicy padding, Hints* result_hints) {
  const HintsVector* actual_arguments = &arguments;
  HintsVector expanded_arguments(zone());
  if (callee->IsJSBoundFunction()) {
    JSBoundFunctionRef bound_function(broker(),
                                      Handle<JSBoundFunction>::cast(callee));
    bound_function.Serialize();
    callee = UnrollBoundFunction(bound_function, broker(), arguments,
                                 &expanded_arguments, zone())
                 .object();
    actual_arguments = &expanded_arguments;
  }
  if (!callee->IsJSFunction()) return;

  JSFunctionRef function(broker(), Handle<JSFunction>::cast(callee));
  function.Serialize();
  Callee new_callee(function.object());
  ProcessCalleeForCallOrConstruct(new_callee, new_target, *actual_arguments,
                                  speculation_mode, padding, result_hints);
}

void SerializerForBackgroundCompilation::ProcessCallOrConstruct(
    Hints callee, base::Optional<Hints> new_target, HintsVector* arguments,
    FeedbackSlot slot, MissingArgumentsPolicy padding) {
  SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation;

  if (!slot.IsInvalid()) {
    FeedbackSource source(feedback_vector(), slot);
    ProcessedFeedback const& feedback =
        broker()->ProcessFeedbackForCall(source);
    if (BailoutOnUninitialized(feedback)) return;

    if (!feedback.IsInsufficient()) {
      // Incorporate feedback into hints copy to simplify processing.
      // TODO(neis): Modify the original hints instead?
      speculation_mode = feedback.AsCall().speculation_mode();
      // Incorporate target feedback into hints copy to simplify processing.
      base::Optional<HeapObjectRef> target = feedback.AsCall().target();
      if (target.has_value() && target->map().is_callable()) {
        callee = callee.Copy(zone());
        // TODO(mvstanton): if the map isn't callable then we have an allocation
        // site, and it may make sense to add the Array JSFunction constant.
        if (new_target.has_value()) {
          // Construct; feedback is new_target, which often is also the callee.
          new_target = new_target->Copy(zone());
          new_target->AddConstant(target->object(), zone(), broker());
          callee.AddConstant(target->object(), zone(), broker());
        } else {
          // Call; target is callee.
          callee.AddConstant(target->object(), zone(), broker());
        }
      }
    }
  }

  Hints result_hints_from_new_target;
  if (new_target.has_value()) {
    ProcessNewTargetForConstruct(*new_target, &result_hints_from_new_target);
    // These hints are a good guess at the resulting object, so they are useful
    // for both the accumulator and the constructor call's receiver. The latter
    // is still missing completely in {arguments} so add it now.
    arguments->insert(arguments->begin(), result_hints_from_new_target);
  }

  // For JSNativeContextSpecialization::InferReceiverRootMap
  Hints new_accumulator_hints = result_hints_from_new_target.Copy(zone());

  ProcessCallOrConstructRecursive(callee, new_target, *arguments,
                                  speculation_mode, padding,
                                  &new_accumulator_hints);
  environment()->accumulator_hints() = new_accumulator_hints;
}

void SerializerForBackgroundCompilation::ProcessCallOrConstructRecursive(
    Hints const& callee, base::Optional<Hints> new_target,
    const HintsVector& arguments, SpeculationMode speculation_mode,
    MissingArgumentsPolicy padding, Hints* result_hints) {
  // For JSCallReducer::ReduceJSCall and JSCallReducer::ReduceJSConstruct.
  for (auto constant : callee.constants()) {
    ProcessCalleeForCallOrConstruct(constant, new_target, arguments,
                                    speculation_mode, padding, result_hints);
  }

  // For JSCallReducer::ReduceJSCall and JSCallReducer::ReduceJSConstruct.
  for (auto hint : callee.virtual_closures()) {
    ProcessCalleeForCallOrConstruct(Callee(hint), new_target, arguments,
                                    speculation_mode, padding, result_hints);
  }

  for (auto hint : callee.virtual_bound_functions()) {
    HintsVector new_arguments = hint.bound_arguments;
    new_arguments.insert(new_arguments.end(), arguments.begin(),
                         arguments.end());
    ProcessCallOrConstructRecursive(hint.bound_target, new_target,
                                    new_arguments, speculation_mode, padding,
                                    result_hints);
  }
}

void SerializerForBackgroundCompilation::ProcessNewTargetForConstruct(
    Hints const& new_target_hints, Hints* result_hints) {
  for (Handle<Object> target : new_target_hints.constants()) {
    if (target->IsJSBoundFunction()) {
      // Unroll the bound function.
      while (target->IsJSBoundFunction()) {
        target = handle(
            Handle<JSBoundFunction>::cast(target)->bound_target_function(),
            broker()->isolate());
      }
    }
    if (target->IsJSFunction()) {
      Handle<JSFunction> new_target(Handle<JSFunction>::cast(target));
      if (new_target->has_prototype_slot(broker()->isolate()) &&
          new_target->has_initial_map()) {
        result_hints->AddMap(
            handle(new_target->initial_map(), broker()->isolate()), zone(),
            broker());
      }
    }
  }

  for (auto const& virtual_bound_function :
       new_target_hints.virtual_bound_functions()) {
    ProcessNewTargetForConstruct(virtual_bound_function.bound_target,
                                 result_hints);
  }
}

void SerializerForBackgroundCompilation::ProcessCallVarArgs(
    ConvertReceiverMode receiver_mode, Hints const& callee,
    interpreter::Register first_reg, int reg_count, FeedbackSlot slot,
    MissingArgumentsPolicy padding) {
  HintsVector args = PrepareArgumentsHints(first_reg, reg_count);
  // The receiver is either given in the first register or it is implicitly
  // the {undefined} value.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    args.insert(args.begin(),
                Hints::SingleConstant(
                    broker()->isolate()->factory()->undefined_value(), zone()));
  }
  ProcessCallOrConstruct(callee, base::nullopt, &args, slot, padding);
}

void SerializerForBackgroundCompilation::ProcessApiCall(
    Handle<SharedFunctionInfo> target, const HintsVector& arguments) {
  ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                          Builtins::kCallFunctionTemplate_CheckAccess));
  ObjectRef(broker(),
            broker()->isolate()->builtins()->builtin_handle(
                Builtins::kCallFunctionTemplate_CheckCompatibleReceiver));
  ObjectRef(
      broker(),
      broker()->isolate()->builtins()->builtin_handle(
          Builtins::kCallFunctionTemplate_CheckAccessAndCompatibleReceiver));

  FunctionTemplateInfoRef target_template_info(
      broker(), handle(target->function_data(), broker()->isolate()));
  if (!target_template_info.has_call_code()) return;
  target_template_info.SerializeCallCode();

  SharedFunctionInfoRef target_ref(broker(), target);
  target_ref.SerializeFunctionTemplateInfo();

  if (target_template_info.accept_any_receiver() &&
      target_template_info.is_signature_undefined()) {
    return;
  }

  if (arguments.empty()) return;
  Hints const& receiver_hints = arguments[0];
  for (auto hint : receiver_hints.constants()) {
    if (hint->IsUndefined()) {
      // The receiver is the global proxy.
      Handle<JSGlobalProxy> global_proxy =
          broker()->target_native_context().global_proxy_object().object();
      ProcessReceiverMapForApiCall(
          target_template_info,
          handle(global_proxy->map(), broker()->isolate()));
      continue;
    }

    if (!hint->IsJSReceiver()) continue;
    Handle<JSReceiver> receiver(Handle<JSReceiver>::cast(hint));

    ProcessReceiverMapForApiCall(target_template_info,
                                 handle(receiver->map(), broker()->isolate()));
  }

  for (auto receiver_map : receiver_hints.maps()) {
    ProcessReceiverMapForApiCall(target_template_info, receiver_map);
  }
}

void SerializerForBackgroundCompilation::ProcessReceiverMapForApiCall(
    FunctionTemplateInfoRef target, Handle<Map> receiver) {
  if (!receiver->is_access_check_needed()) {
    MapRef receiver_map(broker(), receiver);
    TRACE_BROKER(broker(), "Serializing holder for target: " << target);
    target.LookupHolderOfExpectedType(receiver_map,
                                      SerializationPolicy::kSerializeIfNeeded);
  }
}

void SerializerForBackgroundCompilation::ProcessHintsForObjectCreate(
    Hints const& prototype) {
  for (Handle<Object> constant_handle : prototype.constants()) {
    ObjectRef constant(broker(), constant_handle);
    if (constant.IsJSObject()) constant.AsJSObject().SerializeObjectCreateMap();
  }
}

void SerializerForBackgroundCompilation::ProcessBuiltinCall(
    Handle<SharedFunctionInfo> target, base::Optional<Hints> new_target,
    const HintsVector& arguments, SpeculationMode speculation_mode,
    MissingArgumentsPolicy padding, Hints* result_hints) {
  DCHECK(target->HasBuiltinId());
  const int builtin_id = target->builtin_id();
  const char* name = Builtins::name(builtin_id);
  TRACE_BROKER(broker(), "Serializing for call to builtin " << name);
  switch (builtin_id) {
    case Builtins::kObjectCreate: {
      if (arguments.size() >= 2) {
        ProcessHintsForObjectCreate(arguments[1]);
      } else {
        ProcessHintsForObjectCreate(Hints::SingleConstant(
            broker()->isolate()->factory()->undefined_value(), zone()));
      }
      break;
    }
    case Builtins::kPromisePrototypeCatch: {
      // For JSCallReducer::ReducePromisePrototypeCatch.
      if (speculation_mode != SpeculationMode::kDisallowSpeculation) {
        if (arguments.size() >= 1) {
          ProcessMapHintsForPromises(arguments[0]);
        }
      }
      break;
    }
    case Builtins::kPromisePrototypeFinally: {
      // For JSCallReducer::ReducePromisePrototypeFinally.
      if (speculation_mode != SpeculationMode::kDisallowSpeculation) {
        if (arguments.size() >= 1) {
          ProcessMapHintsForPromises(arguments[0]);
        }
      }
      break;
    }
    case Builtins::kPromisePrototypeThen: {
      // For JSCallReducer::ReducePromisePrototypeThen.
      if (speculation_mode != SpeculationMode::kDisallowSpeculation) {
        if (arguments.size() >= 1) {
          ProcessMapHintsForPromises(arguments[0]);
        }
      }
      break;
    }
    case Builtins::kPromiseResolveTrampoline:
      // For JSCallReducer::ReducePromiseInternalResolve and
      // JSNativeContextSpecialization::ReduceJSResolvePromise.
      if (arguments.size() >= 1) {
        Hints const resolution_hints =
            arguments.size() >= 2
                ? arguments[1]
                : Hints::SingleConstant(
                      broker()->isolate()->factory()->undefined_value(),
                      zone());
        ProcessHintsForPromiseResolve(resolution_hints);
      }
      break;
    case Builtins::kRegExpPrototypeTest:
    case Builtins::kRegExpPrototypeTestFast:
      // For JSCallReducer::ReduceRegExpPrototypeTest.
      if (arguments.size() >= 1 &&
          speculation_mode != SpeculationMode::kDisallowSpeculation) {
        Hints const& regexp_hints = arguments[0];
        ProcessHintsForRegExpTest(regexp_hints);
      }
      break;
    case Builtins::kArrayEvery:
    case Builtins::kArrayFilter:
    case Builtins::kArrayForEach:
    case Builtins::kArrayPrototypeFind:
    case Builtins::kArrayPrototypeFindIndex:
    case Builtins::kArrayMap:
    case Builtins::kArraySome:
      if (arguments.size() >= 2 &&
          speculation_mode != SpeculationMode::kDisallowSpeculation) {
        Hints const& callback = arguments[1];
        // "Call(callbackfn, T, « kValue, k, O »)"
        HintsVector new_arguments(zone());
        new_arguments.push_back(
            arguments.size() < 3
                ? Hints::SingleConstant(
                      broker()->isolate()->factory()->undefined_value(), zone())
                : arguments[2]);                // T
        new_arguments.push_back(Hints());       // kValue
        new_arguments.push_back(Hints());       // k
        new_arguments.push_back(arguments[0]);  // O
        for (auto constant : callback.constants()) {
          ProcessCalleeForCallOrConstruct(
              constant, base::nullopt, new_arguments, speculation_mode,
              kMissingArgumentsAreUndefined, result_hints);
        }
        for (auto virtual_closure : callback.virtual_closures()) {
          ProcessCalleeForCallOrConstruct(
              Callee(virtual_closure), base::nullopt, new_arguments,
              speculation_mode, kMissingArgumentsAreUndefined, result_hints);
        }
      }
      break;
    case Builtins::kArrayReduce:
    case Builtins::kArrayReduceRight:
      if (arguments.size() >= 2 &&
          speculation_mode != SpeculationMode::kDisallowSpeculation) {
        Hints const& callback = arguments[1];
        // "Call(callbackfn, undefined, « accumulator, kValue, k, O »)"
        HintsVector new_arguments(zone());
        new_arguments.push_back(Hints::SingleConstant(
            broker()->isolate()->factory()->undefined_value(), zone()));
        new_arguments.push_back(Hints());       // accumulator
        new_arguments.push_back(Hints());       // kValue
        new_arguments.push_back(Hints());       // k
        new_arguments.push_back(arguments[0]);  // O
        for (auto constant : callback.constants()) {
          ProcessCalleeForCallOrConstruct(
              constant, base::nullopt, new_arguments, speculation_mode,
              kMissingArgumentsAreUndefined, result_hints);
        }
        for (auto virtual_closure : callback.virtual_closures()) {
          ProcessCalleeForCallOrConstruct(
              Callee(virtual_closure), base::nullopt, new_arguments,
              speculation_mode, kMissingArgumentsAreUndefined, result_hints);
        }
      }
      break;
    case Builtins::kFunctionPrototypeApply:
      if (arguments.size() >= 1) {
        // Drop hints for all arguments except the user-given receiver.
        Hints const new_receiver =
            arguments.size() >= 2
                ? arguments[1]
                : Hints::SingleConstant(
                      broker()->isolate()->factory()->undefined_value(),
                      zone());
        HintsVector new_arguments({new_receiver}, zone());
        for (auto constant : arguments[0].constants()) {
          ProcessCalleeForCallOrConstruct(
              constant, base::nullopt, new_arguments, speculation_mode,
              kMissingArgumentsAreUnknown, result_hints);
        }
        for (auto const& virtual_closure : arguments[0].virtual_closures()) {
          ProcessCalleeForCallOrConstruct(
              Callee(virtual_closure), base::nullopt, new_arguments,
              speculation_mode, kMissingArgumentsAreUnknown, result_hints);
        }
      }
      break;
    case Builtins::kPromiseConstructor:
      if (arguments.size() >= 1) {
        // "Call(executor, undefined, « resolvingFunctions.[[Resolve]],
        //                              resolvingFunctions.[[Reject]] »)"
        HintsVector new_arguments(
            {Hints::SingleConstant(
                broker()->isolate()->factory()->undefined_value(), zone())},
            zone());
        for (auto constant : arguments[0].constants()) {
          ProcessCalleeForCallOrConstruct(
              constant, base::nullopt, new_arguments,
              SpeculationMode::kDisallowSpeculation,
              kMissingArgumentsAreUnknown, result_hints);
        }
        for (auto const& virtual_closure : arguments[0].virtual_closures()) {
          ProcessCalleeForCallOrConstruct(
              Callee(virtual_closure), base::nullopt, new_arguments,
              SpeculationMode::kDisallowSpeculation,
              kMissingArgumentsAreUnknown, result_hints);
        }
      }
      break;
    case Builtins::kFunctionPrototypeCall:
      if (arguments.size() >= 1) {
        HintsVector new_arguments(arguments.begin() + 1, arguments.end(),
                                  zone());
        for (auto constant : arguments[0].constants()) {
          ProcessCalleeForCallOrConstruct(constant, base::nullopt,
                                          new_arguments, speculation_mode,
                                          padding, result_hints);
        }
        for (auto const& virtual_closure : arguments[0].virtual_closures()) {
          ProcessCalleeForCallOrConstruct(
              Callee(virtual_closure), base::nullopt, new_arguments,
              speculation_mode, padding, result_hints);
        }
      }
      break;
    case Builtins::kReflectApply:
    case Builtins::kReflectConstruct:
      if (arguments.size() >= 2) {
        for (auto constant : arguments[1].constants()) {
          if (constant->IsJSFunction()) {
            JSFunctionRef(broker(), constant).Serialize();
          }
        }
      }
      break;
    case Builtins::kObjectPrototypeIsPrototypeOf:
      if (arguments.size() >= 2) {
        ProcessHintsForHasInPrototypeChain(arguments[1]);
      }
      break;
    case Builtins::kFunctionPrototypeHasInstance:
      // For JSCallReducer::ReduceFunctionPrototypeHasInstance.
      if (arguments.size() >= 2) {
        ProcessHintsForOrdinaryHasInstance(arguments[0], arguments[1]);
      }
      break;
    case Builtins::kFastFunctionPrototypeBind:
      if (arguments.size() >= 1 &&
          speculation_mode != SpeculationMode::kDisallowSpeculation) {
        Hints const& bound_target = arguments[0];
        ProcessHintsForFunctionBind(bound_target);
        HintsVector new_arguments(arguments.begin() + 1, arguments.end(),
                                  zone());
        result_hints->AddVirtualBoundFunction(
            VirtualBoundFunction(bound_target, new_arguments), zone(),
            broker());
      }
      break;
    case Builtins::kObjectGetPrototypeOf:
    case Builtins::kReflectGetPrototypeOf:
      if (arguments.size() >= 2) {
        ProcessHintsForObjectGetPrototype(arguments[1]);
      } else {
        Hints const undefined_hint = Hints::SingleConstant(
            broker()->isolate()->factory()->undefined_value(), zone());
        ProcessHintsForObjectGetPrototype(undefined_hint);
      }
      break;
    case Builtins::kObjectPrototypeGetProto:
      if (arguments.size() >= 1) {
        ProcessHintsForObjectGetPrototype(arguments[0]);
      }
      break;
    case Builtins::kMapIteratorPrototypeNext:
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kOrderedHashTableHealIndex));
      ObjectRef(broker(),
                broker()->isolate()->factory()->empty_ordered_hash_map());
      break;
    case Builtins::kSetIteratorPrototypeNext:
      ObjectRef(broker(), broker()->isolate()->builtins()->builtin_handle(
                              Builtins::kOrderedHashTableHealIndex));
      ObjectRef(broker(),
                broker()->isolate()->factory()->empty_ordered_hash_set());
      break;
    default:
      break;
  }
}

void SerializerForBackgroundCompilation::ProcessHintsForOrdinaryHasInstance(
    Hints const& constructor_hints, Hints const& instance_hints) {
  bool walk_prototypes = false;
  for (Handle<Object> constructor : constructor_hints.constants()) {
    // For JSNativeContextSpecialization::ReduceJSOrdinaryHasInstance.
    if (constructor->IsHeapObject()) {
      ProcessConstantForOrdinaryHasInstance(
          HeapObjectRef(broker(), constructor), &walk_prototypes);
    }
  }
  // For JSNativeContextSpecialization::ReduceJSHasInPrototypeChain.
  if (walk_prototypes) ProcessHintsForHasInPrototypeChain(instance_hints);
}

void SerializerForBackgroundCompilation::ProcessHintsForHasInPrototypeChain(
    Hints const& instance_hints) {
  auto processMap = [&](Handle<Map> map_handle) {
    MapRef map(broker(), map_handle);
    while (map.IsJSObjectMap()) {
      map.SerializePrototype();
      map = map.prototype().map();
    }
  };

  for (auto hint : instance_hints.constants()) {
    if (!hint->IsHeapObject()) continue;
    Handle<HeapObject> object(Handle<HeapObject>::cast(hint));
    processMap(handle(object->map(), broker()->isolate()));
  }
  for (auto map_hint : instance_hints.maps()) {
    processMap(map_hint);
  }
}

void SerializerForBackgroundCompilation::ProcessHintsForPromiseResolve(
    Hints const& resolution_hints) {
  auto processMap = [&](Handle<Map> map) {
    broker()->GetPropertyAccessInfo(
        MapRef(broker(), map),
        NameRef(broker(), broker()->isolate()->factory()->then_string()),
        AccessMode::kLoad, dependencies(),
        SerializationPolicy::kSerializeIfNeeded);
  };

  for (auto hint : resolution_hints.constants()) {
    if (!hint->IsHeapObject()) continue;
    Handle<HeapObject> resolution(Handle<HeapObject>::cast(hint));
    processMap(handle(resolution->map(), broker()->isolate()));
  }
  for (auto map_hint : resolution_hints.maps()) {
    processMap(map_hint);
  }
}

void SerializerForBackgroundCompilation::ProcessMapHintsForPromises(
    Hints const& receiver_hints) {
  // We need to serialize the prototypes on each receiver map.
  for (auto constant : receiver_hints.constants()) {
    if (!constant->IsJSPromise()) continue;
    Handle<Map> map(Handle<HeapObject>::cast(constant)->map(),
                    broker()->isolate());
    MapRef(broker(), map).SerializePrototype();
  }
  for (auto map : receiver_hints.maps()) {
    if (!map->IsJSPromiseMap()) continue;
    MapRef(broker(), map).SerializePrototype();
  }
}

PropertyAccessInfo SerializerForBackgroundCompilation::ProcessMapForRegExpTest(
    MapRef map) {
  PropertyAccessInfo ai_exec = broker()->GetPropertyAccessInfo(
      map, NameRef(broker(), broker()->isolate()->factory()->exec_string()),
      AccessMode::kLoad, dependencies(),
      SerializationPolicy::kSerializeIfNeeded);

  Handle<JSObject> holder;
  if (ai_exec.IsDataConstant() && ai_exec.holder().ToHandle(&holder)) {
    // The property is on the prototype chain.
    JSObjectRef holder_ref(broker(), holder);
    holder_ref.GetOwnDataProperty(ai_exec.field_representation(),
                                  ai_exec.field_index(),
                                  SerializationPolicy::kSerializeIfNeeded);
  }
  return ai_exec;
}

void SerializerForBackgroundCompilation::ProcessHintsForRegExpTest(
    Hints const& regexp_hints) {
  for (auto hint : regexp_hints.constants()) {
    if (!hint->IsJSRegExp()) continue;
    Handle<JSRegExp> regexp(Handle<JSRegExp>::cast(hint));
    Handle<Map> regexp_map(regexp->map(), broker()->isolate());
    PropertyAccessInfo ai_exec =
        ProcessMapForRegExpTest(MapRef(broker(), regexp_map));
    Handle<JSObject> holder;
    if (ai_exec.IsDataConstant() && !ai_exec.holder().ToHandle(&holder)) {
      // The property is on the object itself.
      JSObjectRef holder_ref(broker(), regexp);
      holder_ref.GetOwnDataProperty(ai_exec.field_representation(),
                                    ai_exec.field_index(),
                                    SerializationPolicy::kSerializeIfNeeded);
    }
  }

  for (auto map : regexp_hints.maps()) {
    if (!map->IsJSRegExpMap()) continue;
    ProcessMapForRegExpTest(MapRef(broker(), map));
  }
}

namespace {
void ProcessMapForFunctionBind(MapRef map) {
  map.SerializePrototype();
  int min_nof_descriptors = i::Max(JSFunction::kLengthDescriptorIndex,
                                   JSFunction::kNameDescriptorIndex) +
                            1;
  if (map.NumberOfOwnDescriptors() >= min_nof_descriptors) {
    map.SerializeOwnDescriptor(
        InternalIndex(JSFunction::kLengthDescriptorIndex));
    map.SerializeOwnDescriptor(InternalIndex(JSFunction::kNameDescriptorIndex));
  }
}
}  // namespace

void SerializerForBackgroundCompilation::ProcessHintsForFunctionBind(
    Hints const& receiver_hints) {
  for (auto constant : receiver_hints.constants()) {
    if (!constant->IsJSFunction()) continue;
    JSFunctionRef function(broker(), constant);
    function.Serialize();
    ProcessMapForFunctionBind(function.map());
  }

  for (auto map : receiver_hints.maps()) {
    if (!map->IsJSFunctionMap()) continue;
    MapRef map_ref(broker(), map);
    ProcessMapForFunctionBind(map_ref);
  }
}

void SerializerForBackgroundCompilation::ProcessHintsForObjectGetPrototype(
    Hints const& object_hints) {
  for (auto constant : object_hints.constants()) {
    if (!constant->IsHeapObject()) continue;
    HeapObjectRef object(broker(), constant);
    object.map().SerializePrototype();
  }

  for (auto map : object_hints.maps()) {
    MapRef map_ref(broker(), map);
    map_ref.SerializePrototype();
  }
}

void SerializerForBackgroundCompilation::ContributeToJumpTargetEnvironment(
    int target_offset) {
  auto it = jump_target_environments_.find(target_offset);
  if (it == jump_target_environments_.end()) {
    jump_target_environments_[target_offset] =
        new (zone()) Environment(*environment());
  } else {
    it->second->Merge(environment(), zone(), broker());
  }
}

void SerializerForBackgroundCompilation::IncorporateJumpTargetEnvironment(
    int target_offset) {
  auto it = jump_target_environments_.find(target_offset);
  if (it != jump_target_environments_.end()) {
    environment()->Merge(it->second, zone(), broker());
    jump_target_environments_.erase(it);
  }
}

void SerializerForBackgroundCompilation::ProcessJump(
    interpreter::BytecodeArrayIterator* iterator) {
  int jump_target = iterator->GetJumpTargetOffset();
  if (iterator->current_offset() < jump_target) {
    ContributeToJumpTargetEnvironment(jump_target);
  }
}

void SerializerForBackgroundCompilation::VisitReturn(
    BytecodeArrayIterator* iterator) {
  return_value_hints().Add(environment()->accumulator_hints(), zone(),
                           broker());
  environment()->Kill();
}

void SerializerForBackgroundCompilation::VisitSwitchOnSmiNoFeedback(
    interpreter::BytecodeArrayIterator* iterator) {
  interpreter::JumpTableTargetOffsets targets =
      iterator->GetJumpTableTargetOffsets();
  for (const auto& target : targets) {
    ContributeToJumpTargetEnvironment(target.target_offset);
  }
}

void SerializerForBackgroundCompilation::VisitSwitchOnGeneratorState(
    interpreter::BytecodeArrayIterator* iterator) {
  for (const auto& target : GetBytecodeAnalysis().resume_jump_targets()) {
    ContributeToJumpTargetEnvironment(target.target_offset());
  }
}

void SerializerForBackgroundCompilation::VisitConstruct(
    BytecodeArrayIterator* iterator) {
  Hints& new_target = environment()->accumulator_hints();
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  size_t reg_count = iterator->GetRegisterCountOperand(2);
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  HintsVector args = PrepareArgumentsHints(first_reg, reg_count);

  ProcessCallOrConstruct(callee, new_target, &args, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitConstructWithSpread(
    BytecodeArrayIterator* iterator) {
  Hints const& new_target = environment()->accumulator_hints();
  Hints const& callee = register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  size_t reg_count = iterator->GetRegisterCountOperand(2);
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  DCHECK_GT(reg_count, 0);
  reg_count--;  // Pop the spread element.
  HintsVector args = PrepareArgumentsHints(first_reg, reg_count);

  ProcessCallOrConstruct(callee, new_target, &args, slot,
                         kMissingArgumentsAreUnknown);
}

void SerializerForBackgroundCompilation::ProcessGlobalAccess(FeedbackSlot slot,
                                                             bool is_load) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForGlobalAccess(source);

  if (is_load) {
    Hints result_hints;
    if (feedback.kind() == ProcessedFeedback::kGlobalAccess) {
      // We may be able to contribute to accumulator constant hints.
      base::Optional<ObjectRef> value =
          feedback.AsGlobalAccess().GetConstantHint();
      if (value.has_value()) {
        result_hints.AddConstant(value->object(), zone(), broker());
      }
    } else {
      DCHECK(feedback.IsInsufficient());
    }
    environment()->accumulator_hints() = result_hints;
  }
}

void SerializerForBackgroundCompilation::VisitLdaGlobal(
    BytecodeArrayIterator* iterator) {
  NameRef(broker(),
          iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessGlobalAccess(slot, true);
}

void SerializerForBackgroundCompilation::VisitLdaGlobalInsideTypeof(
    BytecodeArrayIterator* iterator) {
  VisitLdaGlobal(iterator);
}

void SerializerForBackgroundCompilation::VisitLdaLookupSlot(
    BytecodeArrayIterator* iterator) {
  ObjectRef(broker(),
            iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitLdaLookupSlotInsideTypeof(
    BytecodeArrayIterator* iterator) {
  ObjectRef(broker(),
            iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::ProcessCheckContextExtensions(
    int depth) {
  // for BytecodeGraphBuilder::CheckContextExtensions.
  Hints const& context_hints = environment()->current_context_hints();
  for (int i = 0; i < depth; i++) {
    ProcessContextAccess(context_hints, Context::EXTENSION_INDEX, i,
                         kSerializeSlot);
  }
  SharedFunctionInfoRef shared(broker(), function().shared());
  shared.SerializeScopeInfoChain();
}

void SerializerForBackgroundCompilation::ProcessLdaLookupGlobalSlot(
    BytecodeArrayIterator* iterator) {
  ProcessCheckContextExtensions(iterator->GetUnsignedImmediateOperand(2));
  // TODO(neis): BytecodeGraphBilder may insert a JSLoadGlobal.
  VisitLdaGlobal(iterator);
}

void SerializerForBackgroundCompilation::VisitLdaLookupGlobalSlot(
    BytecodeArrayIterator* iterator) {
  ProcessLdaLookupGlobalSlot(iterator);
}

void SerializerForBackgroundCompilation::VisitLdaLookupGlobalSlotInsideTypeof(
    BytecodeArrayIterator* iterator) {
  ProcessLdaLookupGlobalSlot(iterator);
}

void SerializerForBackgroundCompilation::VisitStaGlobal(
    BytecodeArrayIterator* iterator) {
  NameRef(broker(),
          iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessGlobalAccess(slot, false);
}

void SerializerForBackgroundCompilation::ProcessLdaLookupContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot_index = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  NameRef(broker(),
          iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  ProcessCheckContextExtensions(depth);
  environment()->accumulator_hints() = Hints();
  ProcessContextAccess(environment()->current_context_hints(), slot_index,
                       depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitLdaLookupContextSlot(
    BytecodeArrayIterator* iterator) {
  ProcessLdaLookupContextSlot(iterator);
}

void SerializerForBackgroundCompilation::VisitLdaLookupContextSlotInsideTypeof(
    BytecodeArrayIterator* iterator) {
  ProcessLdaLookupContextSlot(iterator);
}

// TODO(neis): Avoid duplicating this.
namespace {
template <class MapContainer>
MapHandles GetRelevantReceiverMaps(Isolate* isolate, MapContainer const& maps) {
  MapHandles result;
  for (Handle<Map> map : maps) {
    if (Map::TryUpdate(isolate, map).ToHandle(&map) &&
        !map->is_abandoned_prototype_map()) {
      DCHECK(!map->is_deprecated());
      result.push_back(map);
    }
  }
  return result;
}
}  // namespace

void SerializerForBackgroundCompilation::ProcessCompareOperation(
    FeedbackSlot slot) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(function().feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForCompareOperation(source);
  if (BailoutOnUninitialized(feedback)) return;
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::ProcessForIn(FeedbackSlot slot) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback = broker()->ProcessFeedbackForForIn(source);
  if (BailoutOnUninitialized(feedback)) return;
  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::ProcessUnaryOrBinaryOperation(
    FeedbackSlot slot, bool honor_bailout_on_uninitialized) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  // Internally V8 uses binary op feedback also for unary ops.
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForBinaryOperation(source);
  if (honor_bailout_on_uninitialized && BailoutOnUninitialized(feedback)) {
    return;
  }
  environment()->accumulator_hints() = Hints();
}

PropertyAccessInfo
SerializerForBackgroundCompilation::ProcessMapForNamedPropertyAccess(
    Hints* receiver, MapRef receiver_map, NameRef const& name,
    AccessMode access_mode, base::Optional<JSObjectRef> concrete_receiver,
    Hints* result_hints) {
  // For JSNativeContextSpecialization::InferReceiverRootMap
  receiver_map.SerializeRootMap();

  // For JSNativeContextSpecialization::ReduceNamedAccess.
  JSGlobalProxyRef global_proxy =
      broker()->target_native_context().global_proxy_object();
  JSGlobalObjectRef global_object =
      broker()->target_native_context().global_object();
  if (receiver_map.equals(global_proxy.map())) {
    base::Optional<PropertyCellRef> cell = global_object.GetPropertyCell(
        name, SerializationPolicy::kSerializeIfNeeded);
    if (access_mode == AccessMode::kLoad && cell.has_value()) {
      result_hints->AddConstant(cell->value().object(), zone(), broker());
    }
  }

  PropertyAccessInfo access_info = broker()->GetPropertyAccessInfo(
      receiver_map, name, access_mode, dependencies(),
      SerializationPolicy::kSerializeIfNeeded);

  // For JSNativeContextSpecialization::InlinePropertySetterCall
  // and InlinePropertyGetterCall.
  if (access_info.IsAccessorConstant() && !access_info.constant().is_null()) {
    if (access_info.constant()->IsJSFunction()) {
      JSFunctionRef function(broker(), access_info.constant());

      // For JSCallReducer and JSInlining(Heuristic).
      HintsVector arguments({Hints::SingleMap(receiver_map.object(), zone())},
                            zone());
      // In the case of a setter any added result hints won't make sense, but
      // they will be ignored anyways by Process*PropertyAccess due to the
      // access mode not being kLoad.
      ProcessCalleeForCallOrConstruct(
          function.object(), base::nullopt, arguments,
          SpeculationMode::kDisallowSpeculation, kMissingArgumentsAreUndefined,
          result_hints);

      // For JSCallReducer::ReduceCallApiFunction.
      Handle<SharedFunctionInfo> sfi = function.shared().object();
      if (sfi->IsApiFunction()) {
        FunctionTemplateInfoRef fti_ref(
            broker(), handle(sfi->get_api_func_data(), broker()->isolate()));
        if (fti_ref.has_call_code()) {
          fti_ref.SerializeCallCode();
          ProcessReceiverMapForApiCall(fti_ref, receiver_map.object());
        }
      }
    } else if (access_info.constant()->IsJSBoundFunction()) {
      JSBoundFunctionRef function(broker(), access_info.constant());

      // For JSCallReducer::ReduceJSCall.
      function.Serialize();
    } else {
      FunctionTemplateInfoRef fti(broker(), access_info.constant());
      if (fti.has_call_code()) fti.SerializeCallCode();
    }
  } else if (access_info.IsModuleExport()) {
    // For JSNativeContextSpecialization::BuildPropertyLoad
    DCHECK(!access_info.constant().is_null());
    CellRef(broker(), access_info.constant());
  }

  switch (access_mode) {
    case AccessMode::kLoad:
      // For PropertyAccessBuilder::TryBuildLoadConstantDataField
      if (access_info.IsDataConstant()) {
        base::Optional<JSObjectRef> holder;
        Handle<JSObject> prototype;
        if (access_info.holder().ToHandle(&prototype)) {
          holder = JSObjectRef(broker(), prototype);
        } else {
          CHECK_IMPLIES(concrete_receiver.has_value(),
                        concrete_receiver->map().equals(receiver_map));
          holder = concrete_receiver;
        }

        if (holder.has_value()) {
          base::Optional<ObjectRef> constant(holder->GetOwnDataProperty(
              access_info.field_representation(), access_info.field_index(),
              SerializationPolicy::kSerializeIfNeeded));
          if (constant.has_value()) {
            result_hints->AddConstant(constant->object(), zone(), broker());
          }
        }
      }
      break;
    case AccessMode::kStore:
    case AccessMode::kStoreInLiteral:
      // For MapInference (StoreField case).
      if (access_info.IsDataField() || access_info.IsDataConstant()) {
        Handle<Map> transition_map;
        if (access_info.transition_map().ToHandle(&transition_map)) {
          MapRef map_ref(broker(), transition_map);
          TRACE_BROKER(broker(), "Propagating transition map "
                                     << map_ref << " to receiver hints.");
          receiver->AddMap(transition_map, zone(), broker_, false);
        }
      }
      break;
    case AccessMode::kHas:
      break;
  }

  return access_info;
}

void SerializerForBackgroundCompilation::VisitLdaKeyedProperty(
    BytecodeArrayIterator* iterator) {
  Hints const& key = environment()->accumulator_hints();
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kLoad, true);
}

void SerializerForBackgroundCompilation::ProcessKeyedPropertyAccess(
    Hints* receiver, Hints const& key, FeedbackSlot slot,
    AccessMode access_mode, bool honor_bailout_on_uninitialized) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForPropertyAccess(source, access_mode,
                                                 base::nullopt);
  if (honor_bailout_on_uninitialized && BailoutOnUninitialized(feedback)) {
    return;
  }

  Hints new_accumulator_hints;
  switch (feedback.kind()) {
    case ProcessedFeedback::kElementAccess:
      ProcessElementAccess(*receiver, key, feedback.AsElementAccess(),
                           access_mode);
      break;
    case ProcessedFeedback::kNamedAccess:
      ProcessNamedAccess(receiver, feedback.AsNamedAccess(), access_mode,
                         &new_accumulator_hints);
      break;
    case ProcessedFeedback::kInsufficient:
      break;
    default:
      UNREACHABLE();
  }

  if (access_mode == AccessMode::kLoad) {
    environment()->accumulator_hints() = new_accumulator_hints;
  }
}

void SerializerForBackgroundCompilation::ProcessNamedPropertyAccess(
    Hints* receiver, NameRef const& name, FeedbackSlot slot,
    AccessMode access_mode) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForPropertyAccess(source, access_mode, name);
  if (BailoutOnUninitialized(feedback)) return;

  Hints new_accumulator_hints;
  switch (feedback.kind()) {
    case ProcessedFeedback::kNamedAccess:
      DCHECK(name.equals(feedback.AsNamedAccess().name()));
      ProcessNamedAccess(receiver, feedback.AsNamedAccess(), access_mode,
                         &new_accumulator_hints);
      break;
    case ProcessedFeedback::kInsufficient:
      break;
    default:
      UNREACHABLE();
  }

  if (access_mode == AccessMode::kLoad) {
    environment()->accumulator_hints() = new_accumulator_hints;
  }
}

void SerializerForBackgroundCompilation::ProcessNamedAccess(
    Hints* receiver, NamedAccessFeedback const& feedback,
    AccessMode access_mode, Hints* result_hints) {
  for (Handle<Map> map : feedback.maps()) {
    MapRef map_ref(broker(), map);
    TRACE_BROKER(broker(), "Propagating feedback map "
                               << map_ref << " to receiver hints.");
    receiver->AddMap(map, zone(), broker_, false);
  }

  for (Handle<Map> map :
       GetRelevantReceiverMaps(broker()->isolate(), receiver->maps())) {
    MapRef map_ref(broker(), map);
    ProcessMapForNamedPropertyAccess(receiver, map_ref, feedback.name(),
                                     access_mode, base::nullopt, result_hints);
  }

  for (Handle<Object> hint : receiver->constants()) {
    ObjectRef object(broker(), hint);
    if (access_mode == AccessMode::kLoad && object.IsJSObject()) {
      MapRef map_ref = object.AsJSObject().map();
      ProcessMapForNamedPropertyAccess(receiver, map_ref, feedback.name(),
                                       access_mode, object.AsJSObject(),
                                       result_hints);
    }
    // For JSNativeContextSpecialization::ReduceJSLoadNamed.
    if (access_mode == AccessMode::kLoad && object.IsJSFunction() &&
        feedback.name().equals(ObjectRef(
            broker(), broker()->isolate()->factory()->prototype_string()))) {
      JSFunctionRef function = object.AsJSFunction();
      function.Serialize();
      if (result_hints != nullptr && function.has_prototype()) {
        result_hints->AddConstant(function.prototype().object(), zone(),
                                  broker());
      }
    }
    // TODO(neis): Also record accumulator hint for string.length and maybe
    // more?
  }
}

void SerializerForBackgroundCompilation::ProcessElementAccess(
    Hints const& receiver, Hints const& key,
    ElementAccessFeedback const& feedback, AccessMode access_mode) {
  for (auto const& group : feedback.transition_groups()) {
    for (Handle<Map> map_handle : group) {
      MapRef map(broker(), map_handle);
      switch (access_mode) {
        case AccessMode::kHas:
        case AccessMode::kLoad:
          map.SerializeForElementLoad();
          break;
        case AccessMode::kStore:
          map.SerializeForElementStore();
          break;
        case AccessMode::kStoreInLiteral:
          // This operation is fairly local and simple, nothing to serialize.
          break;
      }
    }
  }

  for (Handle<Object> hint : receiver.constants()) {
    ObjectRef receiver_ref(broker(), hint);

    // For JSNativeContextSpecialization::InferReceiverRootMap
    if (receiver_ref.IsHeapObject()) {
      receiver_ref.AsHeapObject().map().SerializeRootMap();
    }

    // For JSNativeContextSpecialization::ReduceElementAccess.
    if (receiver_ref.IsJSTypedArray()) {
      receiver_ref.AsJSTypedArray().Serialize();
    }

    // For JSNativeContextSpecialization::ReduceElementLoadFromHeapConstant.
    if (access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) {
      for (Handle<Object> hint : key.constants()) {
        ObjectRef key_ref(broker(), hint);
        // TODO(neis): Do this for integer-HeapNumbers too?
        if (key_ref.IsSmi() && key_ref.AsSmi() >= 0) {
          base::Optional<ObjectRef> element =
              receiver_ref.GetOwnConstantElement(
                  key_ref.AsSmi(), SerializationPolicy::kSerializeIfNeeded);
          if (!element.has_value() && receiver_ref.IsJSArray()) {
            // We didn't find a constant element, but if the receiver is a
            // cow-array we can exploit the fact that any future write to the
            // element will replace the whole elements storage.
            receiver_ref.AsJSArray().GetOwnCowElement(
                key_ref.AsSmi(), SerializationPolicy::kSerializeIfNeeded);
          }
        }
      }
    }
  }

  // For JSNativeContextSpecialization::InferReceiverRootMap
  for (Handle<Map> map : receiver.maps()) {
    MapRef map_ref(broker(), map);
    map_ref.SerializeRootMap();
  }
}

void SerializerForBackgroundCompilation::VisitLdaNamedProperty(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  NameRef name(broker(),
               iterator->GetConstantForIndexOperand(1, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessNamedPropertyAccess(receiver, name, slot, AccessMode::kLoad);
}

// TODO(neis): Do feedback-independent serialization also for *NoFeedback
// bytecodes.
void SerializerForBackgroundCompilation::VisitLdaNamedPropertyNoFeedback(
    BytecodeArrayIterator* iterator) {
  NameRef(broker(),
          iterator->GetConstantForIndexOperand(1, broker()->isolate()));
}

void SerializerForBackgroundCompilation::VisitStaNamedProperty(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  NameRef name(broker(),
               iterator->GetConstantForIndexOperand(1, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessNamedPropertyAccess(receiver, name, slot, AccessMode::kStore);
}

void SerializerForBackgroundCompilation::VisitStaNamedPropertyNoFeedback(
    BytecodeArrayIterator* iterator) {
  NameRef(broker(),
          iterator->GetConstantForIndexOperand(1, broker()->isolate()));
}

void SerializerForBackgroundCompilation::VisitStaNamedOwnProperty(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  NameRef name(broker(),
               iterator->GetConstantForIndexOperand(1, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessNamedPropertyAccess(receiver, name, slot, AccessMode::kStoreInLiteral);
}

void SerializerForBackgroundCompilation::VisitTestIn(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &environment()->accumulator_hints();
  Hints const& key = register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kHas, false);
}

// For JSNativeContextSpecialization::ReduceJSOrdinaryHasInstance.
void SerializerForBackgroundCompilation::ProcessConstantForOrdinaryHasInstance(
    HeapObjectRef const& constructor, bool* walk_prototypes) {
  if (constructor.IsJSBoundFunction()) {
    constructor.AsJSBoundFunction().Serialize();
    ProcessConstantForInstanceOf(
        constructor.AsJSBoundFunction().bound_target_function(),
        walk_prototypes);
  } else if (constructor.IsJSFunction()) {
    constructor.AsJSFunction().Serialize();
    *walk_prototypes =
        *walk_prototypes ||
        (constructor.map().has_prototype_slot() &&
         constructor.AsJSFunction().has_prototype() &&
         !constructor.AsJSFunction().PrototypeRequiresRuntimeLookup());
  }
}

void SerializerForBackgroundCompilation::ProcessConstantForInstanceOf(
    ObjectRef const& constructor, bool* walk_prototypes) {
  if (!constructor.IsHeapObject()) return;
  HeapObjectRef constructor_heap_object = constructor.AsHeapObject();

  PropertyAccessInfo access_info = broker()->GetPropertyAccessInfo(
      constructor_heap_object.map(),
      NameRef(broker(), broker()->isolate()->factory()->has_instance_symbol()),
      AccessMode::kLoad, dependencies(),
      SerializationPolicy::kSerializeIfNeeded);

  if (access_info.IsNotFound()) {
    ProcessConstantForOrdinaryHasInstance(constructor_heap_object,
                                          walk_prototypes);
  } else if (access_info.IsDataConstant()) {
    Handle<JSObject> holder;
    bool found_on_proto = access_info.holder().ToHandle(&holder);
    JSObjectRef holder_ref = found_on_proto ? JSObjectRef(broker(), holder)
                                            : constructor.AsJSObject();
    base::Optional<ObjectRef> constant = holder_ref.GetOwnDataProperty(
        access_info.field_representation(), access_info.field_index(),
        SerializationPolicy::kSerializeIfNeeded);
    CHECK(constant.has_value());
    if (constant->IsJSFunction()) {
      JSFunctionRef function = constant->AsJSFunction();
      function.Serialize();
      if (function.shared().HasBuiltinId() &&
          function.shared().builtin_id() ==
              Builtins::kFunctionPrototypeHasInstance) {
        // For JSCallReducer::ReduceFunctionPrototypeHasInstance.
        ProcessConstantForOrdinaryHasInstance(constructor_heap_object,
                                              walk_prototypes);
      }
    }
  }
}

void SerializerForBackgroundCompilation::VisitTestInstanceOf(
    BytecodeArrayIterator* iterator) {
  Hints const& lhs = register_hints(iterator->GetRegisterOperand(0));
  Hints rhs = environment()->accumulator_hints();
  FeedbackSlot slot = iterator->GetSlotOperand(1);

  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForInstanceOf(source);

  // Incorporate feedback (about rhs) into hints copy to simplify processing.
  // TODO(neis): Propagate into original hints?
  if (!feedback.IsInsufficient()) {
    InstanceOfFeedback const& rhs_feedback = feedback.AsInstanceOf();
    if (rhs_feedback.value().has_value()) {
      rhs = rhs.Copy(zone());
      Handle<JSObject> constructor = rhs_feedback.value()->object();
      rhs.AddConstant(constructor, zone(), broker());
    }
  }

  bool walk_prototypes = false;
  for (Handle<Object> constant : rhs.constants()) {
    ProcessConstantForInstanceOf(ObjectRef(broker(), constant),
                                 &walk_prototypes);
  }
  if (walk_prototypes) ProcessHintsForHasInPrototypeChain(lhs);

  environment()->accumulator_hints() = Hints();
}

void SerializerForBackgroundCompilation::VisitToNumeric(
    BytecodeArrayIterator* iterator) {
  FeedbackSlot slot = iterator->GetSlotOperand(0);
  ProcessUnaryOrBinaryOperation(slot, false);
}

void SerializerForBackgroundCompilation::VisitToNumber(
    BytecodeArrayIterator* iterator) {
  FeedbackSlot slot = iterator->GetSlotOperand(0);
  ProcessUnaryOrBinaryOperation(slot, false);
}

void SerializerForBackgroundCompilation::VisitThrowReferenceErrorIfHole(
    BytecodeArrayIterator* iterator) {
  ObjectRef(broker(),
            iterator->GetConstantForIndexOperand(0, broker()->isolate()));
}

void SerializerForBackgroundCompilation::VisitStaKeyedProperty(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  Hints const& key = register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStore, true);
}

void SerializerForBackgroundCompilation::VisitStaInArrayLiteral(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  Hints const& key = register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStoreInLiteral,
                             true);
}

void SerializerForBackgroundCompilation::VisitStaDataPropertyInLiteral(
    BytecodeArrayIterator* iterator) {
  Hints* receiver = &register_hints(iterator->GetRegisterOperand(0));
  Hints const& key = register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStoreInLiteral,
                             false);
}

#define DEFINE_CLEAR_ACCUMULATOR(name, ...)             \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    environment()->accumulator_hints() = Hints();       \
  }
CLEAR_ACCUMULATOR_LIST(DEFINE_CLEAR_ACCUMULATOR)
#undef DEFINE_CLEAR_ACCUMULATOR

#define DEFINE_CONDITIONAL_JUMP(name, ...)              \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    ProcessJump(iterator);                              \
  }
CONDITIONAL_JUMPS_LIST(DEFINE_CONDITIONAL_JUMP)
#undef DEFINE_CONDITIONAL_JUMP

#define DEFINE_UNCONDITIONAL_JUMP(name, ...)            \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    ProcessJump(iterator);                              \
    environment()->Kill();                              \
  }
UNCONDITIONAL_JUMPS_LIST(DEFINE_UNCONDITIONAL_JUMP)
#undef DEFINE_UNCONDITIONAL_JUMP

#define DEFINE_IGNORE(name, ...)                        \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {}
IGNORED_BYTECODE_LIST(DEFINE_IGNORE)
#undef DEFINE_IGNORE

#define DEFINE_UNREACHABLE(name, ...)                   \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    UNREACHABLE();                                      \
  }
UNREACHABLE_BYTECODE_LIST(DEFINE_UNREACHABLE)
#undef DEFINE_UNREACHABLE

#define DEFINE_KILL(name, ...)                          \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    environment()->Kill();                              \
  }
KILL_ENVIRONMENT_LIST(DEFINE_KILL)
#undef DEFINE_KILL

#define DEFINE_BINARY_OP(name, ...)                     \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    FeedbackSlot slot = iterator->GetSlotOperand(1);    \
    ProcessUnaryOrBinaryOperation(slot, true);          \
  }
BINARY_OP_LIST(DEFINE_BINARY_OP)
#undef DEFINE_BINARY_OP

#define DEFINE_COMPARE_OP(name, ...)                    \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    FeedbackSlot slot = iterator->GetSlotOperand(1);    \
    ProcessCompareOperation(slot);                      \
  }
COMPARE_OP_LIST(DEFINE_COMPARE_OP)
#undef DEFINE_COMPARE_OP

#define DEFINE_UNARY_OP(name, ...)                      \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    FeedbackSlot slot = iterator->GetSlotOperand(0);    \
    ProcessUnaryOrBinaryOperation(slot, true);          \
  }
UNARY_OP_LIST(DEFINE_UNARY_OP)
#undef DEFINE_UNARY_OP

#undef BINARY_OP_LIST
#undef CLEAR_ACCUMULATOR_LIST
#undef COMPARE_OP_LIST
#undef CONDITIONAL_JUMPS_LIST
#undef IGNORED_BYTECODE_LIST
#undef KILL_ENVIRONMENT_LIST
#undef SUPPORTED_BYTECODE_LIST
#undef UNARY_OP_LIST
#undef UNCONDITIONAL_JUMPS_LIST
#undef UNREACHABLE_BYTECODE_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
