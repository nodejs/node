// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/serializer-for-background-compilation.h"

#include <sstream>

#include "src/base/optional.h"
#include "src/compiler/access-info.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/functional-list.h"
#include "src/compiler/js-heap-broker.h"
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

#define CLEAR_ENVIRONMENT_LIST(V) \
  V(CallRuntimeForPair)           \
  V(Debugger)                     \
  V(ResumeGenerator)              \
  V(SuspendGenerator)

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
  CLEAR_ENVIRONMENT_LIST(V)           \
  CONDITIONAL_JUMPS_LIST(V)           \
  IGNORED_BYTECODE_LIST(V)            \
  KILL_ENVIRONMENT_LIST(V)            \
  UNARY_OP_LIST(V)                    \
  UNCONDITIONAL_JUMPS_LIST(V)         \
  UNREACHABLE_BYTECODE_LIST(V)

template <typename T, typename EqualTo>
class FunctionalSet {
 public:
  void Add(T const& elem, Zone* zone) {
    for (auto const& l : data_) {
      if (equal_to(l, elem)) return;
    }
    data_.PushFront(elem, zone);
  }

  bool Includes(FunctionalSet<T, EqualTo> const& other) const {
    return std::all_of(other.begin(), other.end(), [&](T const& other_elem) {
      return std::any_of(this->begin(), this->end(), [&](T const& this_elem) {
        return equal_to(this_elem, other_elem);
      });
    });
  }

  bool IsEmpty() const { return data_.begin() == data_.end(); }

  void Clear() { data_.Clear(); }

  using iterator = typename FunctionalList<T>::iterator;

  iterator begin() const { return data_.begin(); }
  iterator end() const { return data_.end(); }

 private:
  static EqualTo equal_to;
  FunctionalList<T> data_;
};

template <typename T, typename EqualTo>
EqualTo FunctionalSet<T, EqualTo>::equal_to;

struct VirtualContext {
  unsigned int distance;
  Handle<Context> context;

  VirtualContext(unsigned int distance_in, Handle<Context> context_in)
      : distance(distance_in), context(context_in) {
    CHECK_GT(distance, 0);
  }
  bool operator==(const VirtualContext& other) const {
    return context.equals(other.context) && distance == other.distance;
  }
};

class FunctionBlueprint;
using ConstantsSet = FunctionalSet<Handle<Object>, Handle<Object>::equal_to>;
using VirtualContextsSet =
    FunctionalSet<VirtualContext, std::equal_to<VirtualContext>>;
using MapsSet = FunctionalSet<Handle<Map>, Handle<Map>::equal_to>;
using BlueprintsSet =
    FunctionalSet<FunctionBlueprint, std::equal_to<FunctionBlueprint>>;

class Hints {
 public:
  Hints() = default;

  static Hints SingleConstant(Handle<Object> constant, Zone* zone);

  const ConstantsSet& constants() const;
  const MapsSet& maps() const;
  const BlueprintsSet& function_blueprints() const;
  const VirtualContextsSet& virtual_contexts() const;

  void AddConstant(Handle<Object> constant, Zone* zone);
  void AddMap(Handle<Map> map, Zone* zone);
  void AddFunctionBlueprint(FunctionBlueprint function_blueprint, Zone* zone);
  void AddVirtualContext(VirtualContext virtual_context, Zone* zone);

  void Add(const Hints& other, Zone* zone);
  void AddFromChildSerializer(const Hints& other, Zone* zone);

  void Clear();
  bool IsEmpty() const;

#ifdef ENABLE_SLOW_DCHECKS
  bool Includes(Hints const& other) const;
  bool Equals(Hints const& other) const;
#endif

 private:
  VirtualContextsSet virtual_contexts_;
  ConstantsSet constants_;
  MapsSet maps_;
  BlueprintsSet function_blueprints_;
};

using HintsVector = ZoneVector<Hints>;

// A FunctionBlueprint is a SharedFunctionInfo and a FeedbackVector, plus
// Hints about the context in which a closure will be created from them.
class FunctionBlueprint {
 public:
  FunctionBlueprint(Handle<JSFunction> function, Isolate* isolate, Zone* zone);

  FunctionBlueprint(Handle<SharedFunctionInfo> shared,
                    Handle<FeedbackVector> feedback_vector,
                    const Hints& context_hints);

  Handle<SharedFunctionInfo> shared() const { return shared_; }
  Handle<FeedbackVector> feedback_vector() const { return feedback_vector_; }
  const Hints& context_hints() const { return context_hints_; }

  bool operator==(const FunctionBlueprint& other) const {
    // A feedback vector is never used for more than one SFI. Moreover, we can
    // never have two blueprints with identical feedback vector (and SFI) but
    // different hints, because:
    // (1) A blueprint originates either (i) from the data associated with a
    //     CreateClosure bytecode, in which case two different CreateClosure
    //     bytecodes never have the same feedback vector, or (ii) from a
    //     JSFunction, in which case the hints are determined by the closure.
    // (2) We never extend a blueprint's hints after construction.
    //
    // It is therefore sufficient to look at the feedback vector in order to
    // decide equality.
    DCHECK_IMPLIES(feedback_vector_.equals(other.feedback_vector_),
                   shared_.equals(other.shared_));
    SLOW_DCHECK(!feedback_vector_.equals(other.feedback_vector_) ||
                context_hints_.Equals(other.context_hints_));
    return feedback_vector_.equals(other.feedback_vector_);
  }

 private:
  Handle<SharedFunctionInfo> shared_;
  Handle<FeedbackVector> feedback_vector_;
  Hints context_hints_;
};

// A CompilationSubject is a FunctionBlueprint, optionally with a matching
// closure.
class CompilationSubject {
 public:
  explicit CompilationSubject(FunctionBlueprint blueprint)
      : blueprint_(blueprint) {}

  // The zone parameter is to correctly initialize the blueprint,
  // which contains zone-allocated context information.
  CompilationSubject(Handle<JSFunction> closure, Isolate* isolate, Zone* zone);

  const FunctionBlueprint& blueprint() const { return blueprint_; }
  MaybeHandle<JSFunction> closure() const { return closure_; }

 private:
  FunctionBlueprint blueprint_;
  MaybeHandle<JSFunction> closure_;
};

// A Callee is either a JSFunction (which may not have a feedback vector), or a
// FunctionBlueprint. Note that this is different from CompilationSubject, which
// always has a FunctionBlueprint.
class Callee {
 public:
  explicit Callee(Handle<JSFunction> jsfunction)
      : jsfunction_(jsfunction), blueprint_() {}
  explicit Callee(FunctionBlueprint const& blueprint)
      : jsfunction_(), blueprint_(blueprint) {}

  Handle<SharedFunctionInfo> shared(Isolate* isolate) const {
    return blueprint_.has_value()
               ? blueprint_->shared()
               : handle(jsfunction_.ToHandleChecked()->shared(), isolate);
  }

  bool HasFeedbackVector() const {
    Handle<JSFunction> function;
    return blueprint_.has_value() ||
           jsfunction_.ToHandleChecked()->has_feedback_vector();
  }

  CompilationSubject ToCompilationSubject(Isolate* isolate, Zone* zone) const {
    CHECK(HasFeedbackVector());
    return blueprint_.has_value()
               ? CompilationSubject(*blueprint_)
               : CompilationSubject(jsfunction_.ToHandleChecked(), isolate,
                                    zone);
  }

 private:
  MaybeHandle<JSFunction> const jsfunction_;
  base::Optional<FunctionBlueprint> const blueprint_;
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
  Hints Run();  // NOTE: Returns empty for an already-serialized function.

  class Environment;

 private:
  SerializerForBackgroundCompilation(
      ZoneStats* zone_stats, JSHeapBroker* broker,
      CompilationDependencies* dependencies, CompilationSubject function,
      base::Optional<Hints> new_target, const HintsVector& arguments,
      MissingArgumentsPolicy padding,
      SerializerForBackgroundCompilationFlags flags);

  bool BailoutOnUninitialized(ProcessedFeedback const& feedback);

  void TraverseBytecode();

#define DECLARE_VISIT_BYTECODE(name, ...) \
  void Visit##name(interpreter::BytecodeArrayIterator* iterator);
  SUPPORTED_BYTECODE_LIST(DECLARE_VISIT_BYTECODE)
#undef DECLARE_VISIT_BYTECODE

  void ProcessSFIForCallOrConstruct(Callee const& callee,
                                    base::Optional<Hints> new_target,
                                    const HintsVector& arguments,
                                    SpeculationMode speculation_mode,
                                    MissingArgumentsPolicy padding);
  void ProcessCalleeForCallOrConstruct(Handle<Object> callee,
                                       base::Optional<Hints> new_target,
                                       const HintsVector& arguments,
                                       SpeculationMode speculation_mode,
                                       MissingArgumentsPolicy padding);
  void ProcessCallOrConstruct(Hints callee, base::Optional<Hints> new_target,
                              const HintsVector& arguments, FeedbackSlot slot,
                              MissingArgumentsPolicy padding);
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
                          MissingArgumentsPolicy padding);

  void ProcessJump(interpreter::BytecodeArrayIterator* iterator);

  void ProcessKeyedPropertyAccess(Hints const& receiver, Hints const& key,
                                  FeedbackSlot slot, AccessMode access_mode,
                                  bool honor_bailout_on_uninitialized);
  void ProcessNamedPropertyAccess(Hints const& receiver, NameRef const& name,
                                  FeedbackSlot slot, AccessMode access_mode);
  void ProcessNamedAccess(Hints receiver, NamedAccessFeedback const& feedback,
                          AccessMode access_mode, Hints* new_accumulator_hints);
  void ProcessElementAccess(Hints receiver, Hints key,
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
      MapRef receiver_map, NameRef const& name, AccessMode access_mode,
      base::Optional<JSObjectRef> receiver, Hints* new_accumulator_hints);

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
  Environment* const environment_;
  ZoneUnorderedMap<int, Environment*> jump_target_environments_;
  SerializerForBackgroundCompilationFlags const flags_;
  BailoutId const osr_offset_;
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

FunctionBlueprint::FunctionBlueprint(Handle<SharedFunctionInfo> shared,
                                     Handle<FeedbackVector> feedback_vector,
                                     const Hints& context_hints)
    : shared_(shared),
      feedback_vector_(feedback_vector),
      context_hints_(context_hints) {
  // The checked invariant rules out recursion and thus avoids complexity.
  CHECK(context_hints_.function_blueprints().IsEmpty());
}

FunctionBlueprint::FunctionBlueprint(Handle<JSFunction> function,
                                     Isolate* isolate, Zone* zone)
    : shared_(handle(function->shared(), isolate)),
      feedback_vector_(function->feedback_vector(), isolate),
      context_hints_() {
  context_hints_.AddConstant(handle(function->context(), isolate), zone);
  // The checked invariant rules out recursion and thus avoids complexity.
  CHECK(context_hints_.function_blueprints().IsEmpty());
}

CompilationSubject::CompilationSubject(Handle<JSFunction> closure,
                                       Isolate* isolate, Zone* zone)
    : blueprint_(closure, isolate, zone), closure_(closure) {
  CHECK(closure->has_feedback_vector());
}

#ifdef ENABLE_SLOW_DCHECKS
bool Hints::Includes(Hints const& other) const {
  return constants().Includes(other.constants()) &&
         function_blueprints().Includes(other.function_blueprints()) &&
         maps().Includes(other.maps());
}
bool Hints::Equals(Hints const& other) const {
  return this->Includes(other) && other.Includes(*this);
}
#endif

Hints Hints::SingleConstant(Handle<Object> constant, Zone* zone) {
  Hints result;
  result.AddConstant(constant, zone);
  return result;
}

const ConstantsSet& Hints::constants() const { return constants_; }

const MapsSet& Hints::maps() const { return maps_; }

const BlueprintsSet& Hints::function_blueprints() const {
  return function_blueprints_;
}

const VirtualContextsSet& Hints::virtual_contexts() const {
  return virtual_contexts_;
}

void Hints::AddVirtualContext(VirtualContext virtual_context, Zone* zone) {
  virtual_contexts_.Add(virtual_context, zone);
}

void Hints::AddConstant(Handle<Object> constant, Zone* zone) {
  constants_.Add(constant, zone);
}

void Hints::AddMap(Handle<Map> map, Zone* zone) { maps_.Add(map, zone); }

void Hints::AddFunctionBlueprint(FunctionBlueprint function_blueprint,
                                 Zone* zone) {
  function_blueprints_.Add(function_blueprint, zone);
}

void Hints::Add(const Hints& other, Zone* zone) {
  for (auto x : other.constants()) AddConstant(x, zone);
  for (auto x : other.maps()) AddMap(x, zone);
  for (auto x : other.function_blueprints()) AddFunctionBlueprint(x, zone);
  for (auto x : other.virtual_contexts()) AddVirtualContext(x, zone);
}

void Hints::AddFromChildSerializer(const Hints& other, Zone* zone) {
  for (auto x : other.constants()) AddConstant(x, zone);
  for (auto x : other.maps()) AddMap(x, zone);
  for (auto x : other.virtual_contexts()) AddVirtualContext(x, zone);

  // Adding hints from a child serializer run means copying data out from
  // a zone that's being destroyed. FunctionBlueprints have zone allocated
  // data, so we've got to make a deep copy to eliminate traces of the
  // dying zone.
  for (auto x : other.function_blueprints()) {
    Hints new_blueprint_hints;
    new_blueprint_hints.AddFromChildSerializer(x.context_hints(), zone);
    FunctionBlueprint new_blueprint(x.shared(), x.feedback_vector(),
                                    new_blueprint_hints);
    AddFunctionBlueprint(new_blueprint, zone);
  }
}

bool Hints::IsEmpty() const {
  return constants().IsEmpty() && maps().IsEmpty() &&
         function_blueprints().IsEmpty() && virtual_contexts().IsEmpty();
}

std::ostream& operator<<(std::ostream& out,
                         const VirtualContext& virtual_context) {
  out << "Distance " << virtual_context.distance << " from "
      << Brief(*virtual_context.context) << std::endl;
  return out;
}

std::ostream& operator<<(std::ostream& out, const Hints& hints);

std::ostream& operator<<(std::ostream& out,
                         const FunctionBlueprint& blueprint) {
  out << Brief(*blueprint.shared()) << std::endl;
  out << Brief(*blueprint.feedback_vector()) << std::endl;
  !blueprint.context_hints().IsEmpty() && out << blueprint.context_hints()
                                              << "):" << std::endl;
  return out;
}

std::ostream& operator<<(std::ostream& out, const Hints& hints) {
  for (Handle<Object> constant : hints.constants()) {
    out << "  constant " << Brief(*constant) << std::endl;
  }
  for (Handle<Map> map : hints.maps()) {
    out << "  map " << Brief(*map) << std::endl;
  }
  for (FunctionBlueprint const& blueprint : hints.function_blueprints()) {
    out << "  blueprint " << blueprint << std::endl;
  }
  for (VirtualContext const& virtual_context : hints.virtual_contexts()) {
    out << "  virtual context " << virtual_context << std::endl;
  }
  return out;
}

void Hints::Clear() {
  virtual_contexts_.Clear();
  constants_.Clear();
  maps_.Clear();
  function_blueprints_.Clear();
  DCHECK(IsEmpty());
}

class SerializerForBackgroundCompilation::Environment : public ZoneObject {
 public:
  Environment(Zone* zone, CompilationSubject function);
  Environment(Zone* zone, Isolate* isolate, CompilationSubject function,
              base::Optional<Hints> new_target, const HintsVector& arguments,
              MissingArgumentsPolicy padding);

  bool IsDead() const { return ephemeral_hints_.empty(); }

  void Kill() {
    DCHECK(!IsDead());
    ephemeral_hints_.clear();
    DCHECK(IsDead());
  }

  void Revive() {
    DCHECK(IsDead());
    ephemeral_hints_.resize(ephemeral_hints_size(), Hints());
    DCHECK(!IsDead());
  }

  // Merge {other} into {this} environment (leaving {other} unmodified).
  void Merge(Environment* other);

  FunctionBlueprint function() const { return function_; }

  Hints const& closure_hints() const { return closure_hints_; }
  Hints const& current_context_hints() const { return current_context_hints_; }
  Hints& current_context_hints() { return current_context_hints_; }
  Hints const& return_value_hints() const { return return_value_hints_; }
  Hints& return_value_hints() { return return_value_hints_; }

  Hints& accumulator_hints() {
    CHECK_LT(accumulator_index(), ephemeral_hints_.size());
    return ephemeral_hints_[accumulator_index()];
  }

  Hints& register_hints(interpreter::Register reg) {
    if (reg.is_function_closure()) return closure_hints_;
    if (reg.is_current_context()) return current_context_hints_;
    int local_index = RegisterToLocalIndex(reg);
    CHECK_LT(local_index, ephemeral_hints_.size());
    return ephemeral_hints_[local_index];
  }

  // Clears all hints except those for the context, return value, and the
  // closure.
  void ClearEphemeralHints() {
    for (auto& hints : ephemeral_hints_) hints.Clear();
  }

  // Appends the hints for the given register range to {dst} (in order).
  void ExportRegisterHints(interpreter::Register first, size_t count,
                           HintsVector* dst);

 private:
  friend std::ostream& operator<<(std::ostream& out, const Environment& env);

  int RegisterToLocalIndex(interpreter::Register reg) const;

  int parameter_count() const { return parameter_count_; }
  int register_count() const { return register_count_; }

  Zone* const zone_;
  // Instead of storing the blueprint here, we could extract it from the
  // (closure) hints but that would be cumbersome.
  FunctionBlueprint const function_;
  int const parameter_count_;
  int const register_count_;

  Hints closure_hints_;
  Hints current_context_hints_;
  Hints return_value_hints_;

  // ephemeral_hints_ contains hints for the contents of the registers,
  // the accumulator and the parameters. The layout is as follows:
  // [ parameters | registers | accumulator ]
  // The first parameter is the receiver.
  HintsVector ephemeral_hints_;
  int accumulator_index() const { return parameter_count() + register_count(); }
  int ephemeral_hints_size() const { return accumulator_index() + 1; }
};

SerializerForBackgroundCompilation::Environment::Environment(
    Zone* zone, CompilationSubject function)
    : zone_(zone),
      function_(function.blueprint()),
      parameter_count_(
          function_.shared()->GetBytecodeArray().parameter_count()),
      register_count_(function_.shared()->GetBytecodeArray().register_count()),
      closure_hints_(),
      current_context_hints_(),
      return_value_hints_(),
      ephemeral_hints_(ephemeral_hints_size(), Hints(), zone) {
  Handle<JSFunction> closure;
  if (function.closure().ToHandle(&closure)) {
    closure_hints_.AddConstant(closure, zone);
  } else {
    closure_hints_.AddFunctionBlueprint(function.blueprint(), zone);
  }

  // Consume blueprint context hint information.
  current_context_hints().Add(function.blueprint().context_hints(), zone);
}

SerializerForBackgroundCompilation::Environment::Environment(
    Zone* zone, Isolate* isolate, CompilationSubject function,
    base::Optional<Hints> new_target, const HintsVector& arguments,
    MissingArgumentsPolicy padding)
    : Environment(zone, function) {
  // Copy the hints for the actually passed arguments, at most up to
  // the parameter_count.
  size_t param_count = static_cast<size_t>(parameter_count());
  for (size_t i = 0; i < std::min(arguments.size(), param_count); ++i) {
    ephemeral_hints_[i] = arguments[i];
  }

  if (padding == kMissingArgumentsAreUndefined) {
    Hints undefined_hint =
        Hints::SingleConstant(isolate->factory()->undefined_value(), zone);
    for (size_t i = arguments.size(); i < param_count; ++i) {
      ephemeral_hints_[i] = undefined_hint;
    }
  } else {
    DCHECK_EQ(padding, kMissingArgumentsAreUnknown);
  }

  interpreter::Register new_target_reg =
      function_.shared()
          ->GetBytecodeArray()
          .incoming_new_target_or_generator_register();
  if (new_target_reg.is_valid()) {
    DCHECK(register_hints(new_target_reg).IsEmpty());
    if (new_target.has_value()) {
      register_hints(new_target_reg).Add(*new_target, zone);
    }
  }
}

void SerializerForBackgroundCompilation::Environment::Merge(
    Environment* other) {
  // {other} is guaranteed to have the same layout because it comes from an
  // earlier bytecode in the same function.
  CHECK_EQ(parameter_count(), other->parameter_count());
  CHECK_EQ(register_count(), other->register_count());

  SLOW_DCHECK(closure_hints_.Equals(other->closure_hints_));

  if (IsDead()) {
    ephemeral_hints_ = other->ephemeral_hints_;
    SLOW_DCHECK(return_value_hints_.Includes(other->return_value_hints_));
    CHECK(!IsDead());
    return;
  }

  CHECK_EQ(ephemeral_hints_.size(), other->ephemeral_hints_.size());
  for (size_t i = 0; i < ephemeral_hints_.size(); ++i) {
    ephemeral_hints_[i].Add(other->ephemeral_hints_[i], zone_);
  }

  return_value_hints_.Add(other->return_value_hints_, zone_);
}

std::ostream& operator<<(
    std::ostream& out,
    const SerializerForBackgroundCompilation::Environment& env) {
  std::ostringstream output_stream;
  output_stream << "Function ";
  env.function_.shared()->Name().Print(output_stream);

  if (env.IsDead()) {
    output_stream << "dead\n";
  } else {
    output_stream << "alive\n";
    for (int i = 0; i < static_cast<int>(env.ephemeral_hints_.size()); ++i) {
      Hints const& hints = env.ephemeral_hints_[i];
      if (!hints.IsEmpty()) {
        if (i < env.parameter_count()) {
          output_stream << "Hints for a" << i << ":\n";
        } else if (i < env.parameter_count() + env.register_count()) {
          int local_register = i - env.parameter_count();
          output_stream << "Hints for r" << local_register << ":\n";
        } else if (i == env.accumulator_index()) {
          output_stream << "Hints for <accumulator>:\n";
        } else {
          UNREACHABLE();
        }
        output_stream << hints;
      }
    }
  }

  if (!env.closure_hints().IsEmpty()) {
    output_stream << "Hints for <closure>:\n" << env.closure_hints();
  }
  if (!env.current_context_hints().IsEmpty()) {
    output_stream << "Hints for <context>:\n" << env.current_context_hints();
  }
  if (!env.return_value_hints().IsEmpty()) {
    output_stream << "Hints for {return value}:\n" << env.return_value_hints();
  }

  out << output_stream.str();
  return out;
}

int SerializerForBackgroundCompilation::Environment::RegisterToLocalIndex(
    interpreter::Register reg) const {
  if (reg.is_parameter()) {
    return reg.ToParameterIndex(parameter_count());
  } else {
    DCHECK(!reg.is_function_closure());
    return parameter_count() + reg.index();
  }
}

SerializerForBackgroundCompilation::SerializerForBackgroundCompilation(
    ZoneStats* zone_stats, JSHeapBroker* broker,
    CompilationDependencies* dependencies, Handle<JSFunction> closure,
    SerializerForBackgroundCompilationFlags flags, BailoutId osr_offset)
    : broker_(broker),
      dependencies_(dependencies),
      zone_scope_(zone_stats, ZONE_NAME),
      environment_(new (zone()) Environment(
          zone(), CompilationSubject(closure, broker_->isolate(), zone()))),
      jump_target_environments_(zone()),
      flags_(flags),
      osr_offset_(osr_offset) {
  JSFunctionRef(broker, closure).Serialize();
}

SerializerForBackgroundCompilation::SerializerForBackgroundCompilation(
    ZoneStats* zone_stats, JSHeapBroker* broker,
    CompilationDependencies* dependencies, CompilationSubject function,
    base::Optional<Hints> new_target, const HintsVector& arguments,
    MissingArgumentsPolicy padding,
    SerializerForBackgroundCompilationFlags flags)
    : broker_(broker),
      dependencies_(dependencies),
      zone_scope_(zone_stats, ZONE_NAME),
      environment_(new (zone())
                       Environment(zone(), broker_->isolate(), function,
                                   new_target, arguments, padding)),
      jump_target_environments_(zone()),
      flags_(flags),
      osr_offset_(BailoutId::None()) {
  TraceScope tracer(
      broker_, this,
      "SerializerForBackgroundCompilation::SerializerForBackgroundCompilation");
  TRACE_BROKER(broker_, "Initial environment:\n" << *environment_);
  Handle<JSFunction> closure;
  if (function.closure().ToHandle(&closure)) {
    JSFunctionRef(broker, closure).Serialize();
  }
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
  TRACE_BROKER_MEMORY(broker(), "[serializer start] Broker zone usage: "
                                    << broker()->zone()->allocation_size());
  SharedFunctionInfoRef shared(broker(), environment()->function().shared());
  FeedbackVectorRef feedback_vector_ref(broker(), feedback_vector());
  if (shared.IsSerializedForCompilation(feedback_vector_ref)) {
    TRACE_BROKER(broker(), "Already ran serializer for SharedFunctionInfo "
                               << Brief(*shared.object())
                               << ", bailing out.\n");
    return Hints();
  }
  shared.SetSerializedForCompilation(feedback_vector_ref);

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

  TRACE_BROKER_MEMORY(broker(), "[serializer end] Broker zone usage: "
                                    << broker()->zone()->allocation_size());
  return environment()->return_value_hints();
}

class ExceptionHandlerMatcher {
 public:
  explicit ExceptionHandlerMatcher(
      BytecodeArrayIterator const& bytecode_iterator,
      Handle<BytecodeArray> bytecode_array)
      : bytecode_iterator_(bytecode_iterator) {
    HandlerTable table(*bytecode_array);
    for (int i = 0, n = table.NumberOfRangeEntries(); i < n; ++i) {
      handlers_.insert(table.GetRangeHandler(i));
    }
    handlers_iterator_ = handlers_.cbegin();
  }

  bool CurrentBytecodeIsExceptionHandlerStart() {
    CHECK(!bytecode_iterator_.done());
    while (handlers_iterator_ != handlers_.cend() &&
           *handlers_iterator_ < bytecode_iterator_.current_offset()) {
      handlers_iterator_++;
    }
    return handlers_iterator_ != handlers_.cend() &&
           *handlers_iterator_ == bytecode_iterator_.current_offset();
  }

 private:
  BytecodeArrayIterator const& bytecode_iterator_;
  std::set<int> handlers_;
  std::set<int>::const_iterator handlers_iterator_;
};

Handle<FeedbackVector> SerializerForBackgroundCompilation::feedback_vector()
    const {
  return environment()->function().feedback_vector();
}

Handle<BytecodeArray> SerializerForBackgroundCompilation::bytecode_array()
    const {
  return handle(environment()->function().shared()->GetBytecodeArray(),
                broker()->isolate());
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
  ExceptionHandlerMatcher handler_matcher(iterator, bytecode_array());

  bool has_one_shot_bytecode = false;
  for (; !iterator.done(); iterator.Advance()) {
    has_one_shot_bytecode =
        has_one_shot_bytecode ||
        interpreter::Bytecodes::IsOneShotBytecode(iterator.current_bytecode());

    int const current_offset = iterator.current_offset();
    IncorporateJumpTargetEnvironment(current_offset);

    TRACE_BROKER(broker(),
                 "Handling bytecode: " << current_offset << "  "
                                       << iterator.current_bytecode());
    TRACE_BROKER(broker(), "Current environment: " << *environment());

    if (environment()->IsDead()) {
      if (handler_matcher.CurrentBytecodeIsExceptionHandlerStart()) {
        environment()->Revive();
      } else {
        continue;  // Skip this bytecode since TF won't generate code for it.
      }
    }

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
      default: {
        environment()->ClearEphemeralHints();
        break;
      }
    }
  }

  if (has_one_shot_bytecode) {
    broker()->isolate()->CountUsage(
        v8::Isolate::UseCounterFeature::kOptimizedFunctionWithOneShotBytecode);
  }
}

void SerializerForBackgroundCompilation::VisitGetIterator(
    BytecodeArrayIterator* iterator) {
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Handle<Name> name = broker()->isolate()->factory()->iterator_symbol();
  FeedbackSlot load_slot = iterator->GetSlotOperand(1);
  ProcessNamedPropertyAccess(receiver, NameRef(broker(), name), load_slot,
                             AccessMode::kLoad);
  if (environment()->IsDead()) return;

  const Hints& callee = Hints();
  FeedbackSlot call_slot = iterator->GetSlotOperand(2);
  HintsVector parameters({receiver}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, call_slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitGetSuperConstructor(
    BytecodeArrayIterator* iterator) {
  interpreter::Register dst = iterator->GetRegisterOperand(0);
  environment()->register_hints(dst).Clear();

  for (auto constant : environment()->accumulator_hints().constants()) {
    // For JSNativeContextSpecialization::ReduceJSGetSuperConstructor.
    if (!constant->IsJSFunction()) continue;
    MapRef map(broker(),
               handle(HeapObject::cast(*constant).map(), broker()->isolate()));
    map.SerializePrototype();
    ObjectRef proto = map.prototype();
    if (proto.IsHeapObject() && proto.AsHeapObject().map().is_constructor()) {
      environment()->register_hints(dst).AddConstant(proto.object(), zone());
    }
  }
}

void SerializerForBackgroundCompilation::VisitGetTemplateObject(
    BytecodeArrayIterator* iterator) {
  TemplateObjectDescriptionRef description(
      broker(), iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  FeedbackSource source(feedback_vector(), slot);
  SharedFunctionInfoRef shared(broker(), environment()->function().shared());
  JSArrayRef template_object = shared.GetTemplateObject(
      description, source, SerializationPolicy::kSerializeIfNeeded);
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(template_object.object(),
                                                 zone());
}

void SerializerForBackgroundCompilation::VisitLdaTrue(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->true_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaFalse(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->false_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaTheHole(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->the_hole_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaUndefined(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaNull(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->null_value(), zone());
}

void SerializerForBackgroundCompilation::VisitLdaZero(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      handle(Smi::FromInt(0), broker()->isolate()), zone());
}

void SerializerForBackgroundCompilation::VisitLdaSmi(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      handle(Smi::FromInt(iterator->GetImmediateOperand(0)),
             broker()->isolate()),
      zone());
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
      HintsVector arguments(zone());
      environment()->ExportRegisterHints(first_reg, reg_count, &arguments);
      Hints const& resolution_hints = arguments[1];  // The resolution object.
      ProcessHintsForPromiseResolve(resolution_hints);
      environment()->accumulator_hints().Clear();
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
  environment()->ClearEphemeralHints();
}

void SerializerForBackgroundCompilation::VisitLdaConstant(
    BytecodeArrayIterator* iterator) {
  ObjectRef object(
      broker(), iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(object.object(), zone());
}

void SerializerForBackgroundCompilation::VisitPushContext(
    BytecodeArrayIterator* iterator) {
  // Transfer current context hints to the destination register hints.
  Hints& current_context_hints = environment()->current_context_hints();
  Hints& saved_context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  saved_context_hints.Clear();
  saved_context_hints.Add(current_context_hints, zone());

  // New context is in the accumulator. Put those hints into the current context
  // register hints.
  current_context_hints.Clear();
  current_context_hints.Add(environment()->accumulator_hints(), zone());
}

void SerializerForBackgroundCompilation::VisitPopContext(
    BytecodeArrayIterator* iterator) {
  // Replace current context hints with hints given in the argument register.
  Hints& new_context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  environment()->current_context_hints().Clear();
  environment()->current_context_hints().Add(new_context_hints, zone());
}

void SerializerForBackgroundCompilation::ProcessImmutableLoad(
    ContextRef const& context_ref, int slot, ContextProcessingMode mode,
    Hints* result_hints) {
  DCHECK_EQ(mode, kSerializeSlot);
  base::Optional<ObjectRef> slot_value =
      context_ref.get(slot, SerializationPolicy::kSerializeIfNeeded);

  // If requested, record the object as a hint for the result value.
  if (result_hints != nullptr && slot_value.has_value()) {
    result_hints->AddConstant(slot_value.value().object(), zone());
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
  Hints const& context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().Add(new_accumulator_hints, zone());
}

void SerializerForBackgroundCompilation::VisitLdaCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints const& context_hints = environment()->current_context_hints();
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().Add(new_accumulator_hints, zone());
}

void SerializerForBackgroundCompilation::VisitLdaImmutableContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints const& context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kSerializeSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().Add(new_accumulator_hints, zone());
}

void SerializerForBackgroundCompilation::VisitLdaImmutableCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints const& context_hints = environment()->current_context_hints();
  Hints new_accumulator_hints;
  ProcessContextAccess(context_hints, slot, depth, kSerializeSlot,
                       &new_accumulator_hints);
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().Add(new_accumulator_hints, zone());
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
  environment()->accumulator_hints().Clear();
}

void SerializerForBackgroundCompilation::VisitStaContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints const& register_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  ProcessContextAccess(register_hints, slot, depth, kIgnoreSlot);
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
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().Add(
      environment()->register_hints(iterator->GetRegisterOperand(0)), zone());
}

void SerializerForBackgroundCompilation::VisitStar(
    BytecodeArrayIterator* iterator) {
  interpreter::Register reg = iterator->GetRegisterOperand(0);
  environment()->register_hints(reg).Clear();
  environment()->register_hints(reg).Add(environment()->accumulator_hints(),
                                         zone());
}

void SerializerForBackgroundCompilation::VisitMov(
    BytecodeArrayIterator* iterator) {
  interpreter::Register src = iterator->GetRegisterOperand(0);
  interpreter::Register dst = iterator->GetRegisterOperand(1);
  environment()->register_hints(dst).Clear();
  environment()->register_hints(dst).Add(environment()->register_hints(src),
                                         zone());
}

void SerializerForBackgroundCompilation::VisitCreateRegExpLiteral(
    BytecodeArrayIterator* iterator) {
  Handle<String> constant_pattern = Handle<String>::cast(
      iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  StringRef description(broker(), constant_pattern);
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  FeedbackSource source(feedback_vector(), slot);
  broker()->ProcessFeedbackForRegExpLiteral(source);
  environment()->accumulator_hints().Clear();
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
  environment()->accumulator_hints().Clear();
}

void SerializerForBackgroundCompilation::VisitCreateEmptyArrayLiteral(
    BytecodeArrayIterator* iterator) {
  FeedbackSlot slot = iterator->GetSlotOperand(0);
  FeedbackSource source(feedback_vector(), slot);
  broker()->ProcessFeedbackForArrayOrObjectLiteral(source);
  environment()->accumulator_hints().Clear();
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
  environment()->accumulator_hints().Clear();
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

  Hints const& current_context_hints = environment()->current_context_hints();
  Hints& accumulator_hints = environment()->accumulator_hints();
  accumulator_hints.Clear();

  // For each constant context, we must create a virtual context from
  // it of distance one.
  for (auto x : current_context_hints.constants()) {
    if (x->IsContext()) {
      Handle<Context> as_context(Handle<Context>::cast(x));
      accumulator_hints.AddVirtualContext(VirtualContext(1, as_context),
                                          zone());
    }
  }

  // For each virtual context, we must create a virtual context from
  // it of distance {existing distance} + 1.
  for (auto x : current_context_hints.virtual_contexts()) {
    accumulator_hints.AddVirtualContext(
        VirtualContext(x.distance + 1, x.context), zone());
  }
}

void SerializerForBackgroundCompilation::VisitCreateClosure(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();

  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>::cast(
      iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  Handle<FeedbackCell> feedback_cell =
      feedback_vector()->GetClosureFeedbackCell(iterator->GetIndexOperand(1));
  FeedbackCellRef feedback_cell_ref(broker(), feedback_cell);
  Handle<Object> cell_value(feedback_cell->value(), broker()->isolate());
  ObjectRef cell_value_ref(broker(), cell_value);

  if (cell_value->IsFeedbackVector()) {
    FunctionBlueprint blueprint(shared,
                                Handle<FeedbackVector>::cast(cell_value),
                                environment()->current_context_hints());
    environment()->accumulator_hints().AddFunctionBlueprint(blueprint, zone());
  }
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessCallVarArgs(ConvertReceiverMode::kNullOrUndefined, callee, first_reg,
                     reg_count, slot);
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver0(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);

  Hints receiver = Hints::SingleConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
  HintsVector parameters({receiver}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver1(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const Hints& arg0 =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);

  Hints receiver = Hints::SingleConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
  HintsVector parameters({receiver, arg0}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver2(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const Hints& arg0 =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  const Hints& arg1 =
      environment()->register_hints(iterator->GetRegisterOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  Hints receiver = Hints::SingleConstant(
      broker()->isolate()->factory()->undefined_value(), zone());
  HintsVector parameters({receiver, arg0, arg1}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallAnyReceiver(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessCallVarArgs(ConvertReceiverMode::kAny, callee, first_reg, reg_count,
                     slot);
}

void SerializerForBackgroundCompilation::VisitCallNoFeedback(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  ProcessCallVarArgs(ConvertReceiverMode::kAny, callee, first_reg, reg_count,
                     FeedbackSlot::Invalid());
}

void SerializerForBackgroundCompilation::VisitCallProperty(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessCallVarArgs(ConvertReceiverMode::kNotNullOrUndefined, callee,
                     first_reg, reg_count, slot);
}

void SerializerForBackgroundCompilation::VisitCallProperty0(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const Hints& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);

  HintsVector parameters({receiver}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallProperty1(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const Hints& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  const Hints& arg0 =
      environment()->register_hints(iterator->GetRegisterOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  HintsVector parameters({receiver, arg0}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallProperty2(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const Hints& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  const Hints& arg0 =
      environment()->register_hints(iterator->GetRegisterOperand(2));
  const Hints& arg1 =
      environment()->register_hints(iterator->GetRegisterOperand(3));
  FeedbackSlot slot = iterator->GetSlotOperand(4);

  HintsVector parameters({receiver, arg0, arg1}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitCallWithSpread(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
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
  Hints callee = Hints::SingleConstant(constant.object(), zone());
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
      arguments, padding, flags());
  // The Hints returned by the call to Run are allocated in the zone
  // created by the child serializer. Adding those hints to a hints
  // object created in our zone will preserve the information.
  Hints hints;
  hints.AddFromChildSerializer(child_serializer.Run(), zone());
  return hints;
}

void SerializerForBackgroundCompilation::ProcessSFIForCallOrConstruct(
    Callee const& callee, base::Optional<Hints> new_target,
    const HintsVector& arguments, SpeculationMode speculation_mode,
    MissingArgumentsPolicy padding) {
  Handle<SharedFunctionInfo> shared = callee.shared(broker()->isolate());
  if (shared->IsApiFunction()) {
    ProcessApiCall(shared, arguments);
    DCHECK(!shared->IsInlineable());
  } else if (shared->HasBuiltinId()) {
    ProcessBuiltinCall(shared, new_target, arguments, speculation_mode,
                       padding);
    DCHECK(!shared->IsInlineable());
  } else if (shared->IsInlineable() && callee.HasFeedbackVector()) {
    CompilationSubject subject =
        callee.ToCompilationSubject(broker()->isolate(), zone());
    environment()->accumulator_hints().Add(
        RunChildSerializer(subject, new_target, arguments, padding), zone());
  }
}

namespace {
// Returns the innermost bound target and inserts all bound arguments and
// {original_arguments} into {expanded_arguments} in the appropriate order.
JSReceiverRef UnrollBoundFunction(JSBoundFunctionRef const& bound_function,
                                  JSHeapBroker* broker,
                                  const HintsVector& original_arguments,
                                  HintsVector* expanded_arguments) {
  DCHECK(expanded_arguments->empty());

  JSReceiverRef target = bound_function.AsJSReceiver();
  HintsVector reversed_bound_arguments(broker->zone());
  for (; target.IsJSBoundFunction();
       target = target.AsJSBoundFunction().bound_target_function()) {
    for (int i = target.AsJSBoundFunction().bound_arguments().length() - 1;
         i >= 0; --i) {
      Hints arg = Hints::SingleConstant(
          target.AsJSBoundFunction().bound_arguments().get(i).object(),
          broker->zone());
      reversed_bound_arguments.push_back(arg);
    }
    Hints arg = Hints::SingleConstant(
        target.AsJSBoundFunction().bound_this().object(), broker->zone());
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
    MissingArgumentsPolicy padding) {
  const HintsVector* actual_arguments = &arguments;
  HintsVector expanded_arguments(zone());
  if (callee->IsJSBoundFunction()) {
    JSBoundFunctionRef bound_function(broker(),
                                      Handle<JSBoundFunction>::cast(callee));
    bound_function.Serialize();
    callee = UnrollBoundFunction(bound_function, broker(), arguments,
                                 &expanded_arguments)
                 .object();
    actual_arguments = &expanded_arguments;
  }
  if (!callee->IsJSFunction()) return;

  JSFunctionRef function(broker(), Handle<JSFunction>::cast(callee));
  function.Serialize();
  Callee new_callee(function.object());
  ProcessSFIForCallOrConstruct(new_callee, new_target, *actual_arguments,
                               speculation_mode, padding);
}

void SerializerForBackgroundCompilation::ProcessCallOrConstruct(
    Hints callee, base::Optional<Hints> new_target,
    const HintsVector& arguments, FeedbackSlot slot,
    MissingArgumentsPolicy padding) {
  SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation;
  if (!slot.IsInvalid()) {
    FeedbackSource source(feedback_vector(), slot);
    ProcessedFeedback const& feedback =
        broker()->ProcessFeedbackForCall(source);
    if (BailoutOnUninitialized(feedback)) return;

    // Incorporate feedback into hints copy to simplify processing.
    if (!feedback.IsInsufficient()) {
      speculation_mode = feedback.AsCall().speculation_mode();
      base::Optional<HeapObjectRef> target = feedback.AsCall().target();
      if (target.has_value() && target->map().is_callable()) {
        // TODO(mvstanton): if the map isn't callable then we have an allocation
        // site, and it may make sense to add the Array JSFunction constant.
        if (new_target.has_value()) {
          // Construct; feedback is new_target, which often is also the callee.
          new_target->AddConstant(target->object(), zone());
          callee.AddConstant(target->object(), zone());
        } else {
          // Call; target is callee.
          callee.AddConstant(target->object(), zone());
        }
      }
    }
  }

  environment()->accumulator_hints().Clear();

  // For JSCallReducer::ReduceJSCall and JSCallReducer::ReduceJSConstruct.
  for (auto constant : callee.constants()) {
    ProcessCalleeForCallOrConstruct(constant, new_target, arguments,
                                    speculation_mode, padding);
  }

  // For JSCallReducer::ReduceJSCall and JSCallReducer::ReduceJSConstruct.
  for (auto hint : callee.function_blueprints()) {
    ProcessSFIForCallOrConstruct(Callee(hint), new_target, arguments,
                                 speculation_mode, padding);
  }
}

void SerializerForBackgroundCompilation::ProcessCallVarArgs(
    ConvertReceiverMode receiver_mode, Hints const& callee,
    interpreter::Register first_reg, int reg_count, FeedbackSlot slot,
    MissingArgumentsPolicy padding) {
  HintsVector arguments(zone());
  // The receiver is either given in the first register or it is implicitly
  // the {undefined} value.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    arguments.push_back(Hints::SingleConstant(
        broker()->isolate()->factory()->undefined_value(), zone()));
  }
  environment()->ExportRegisterHints(first_reg, reg_count, &arguments);

  ProcessCallOrConstruct(callee, base::nullopt, arguments, slot, padding);
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
    TRACE_BROKER(broker(), "Serializing holder for target:" << target);
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
    MissingArgumentsPolicy padding) {
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
        CHECK_GE(arguments.size(), 1);
        ProcessMapHintsForPromises(arguments[0]);
      }
      break;
    }
    case Builtins::kPromisePrototypeFinally: {
      // For JSCallReducer::ReducePromisePrototypeFinally.
      if (speculation_mode != SpeculationMode::kDisallowSpeculation) {
        CHECK_GE(arguments.size(), 1);
        ProcessMapHintsForPromises(arguments[0]);
      }
      break;
    }
    case Builtins::kPromisePrototypeThen: {
      // For JSCallReducer::ReducePromisePrototypeThen.
      if (speculation_mode != SpeculationMode::kDisallowSpeculation) {
        CHECK_GE(arguments.size(), 1);
        ProcessMapHintsForPromises(arguments[0]);
      }
      break;
    }
    case Builtins::kPromiseResolveTrampoline:
      // For JSCallReducer::ReducePromiseInternalResolve and
      // JSNativeContextSpecialization::ReduceJSResolvePromise.
      if (arguments.size() >= 1) {
        Hints const& resolution_hints =
            arguments.size() >= 2
                ? arguments[1]
                : Hints::SingleConstant(
                      broker()->isolate()->factory()->undefined_value(),
                      zone());
        ProcessHintsForPromiseResolve(resolution_hints);
      }
      break;
    case Builtins::kPromiseInternalResolve:
      // For JSCallReducer::ReducePromiseInternalResolve and
      // JSNativeContextSpecialization::ReduceJSResolvePromise.
      if (arguments.size() >= 2) {
        Hints const& resolution_hints =
            arguments.size() >= 3
                ? arguments[2]
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
          ProcessCalleeForCallOrConstruct(constant, base::nullopt,
                                          new_arguments,
                                          SpeculationMode::kDisallowSpeculation,
                                          kMissingArgumentsAreUndefined);
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
          ProcessCalleeForCallOrConstruct(constant, base::nullopt,
                                          new_arguments,
                                          SpeculationMode::kDisallowSpeculation,
                                          kMissingArgumentsAreUndefined);
        }
      }
      break;
    // TODO(neis): At least for Array* we should look at blueprints too.
    // TODO(neis): Might need something like a FunctionBlueprint but for
    // creating bound functions rather than creating closures.
    case Builtins::kFunctionPrototypeApply:
      if (arguments.size() >= 1) {
        // Drop hints for all arguments except the user-given receiver.
        Hints new_receiver =
            arguments.size() >= 2
                ? arguments[1]
                : Hints::SingleConstant(
                      broker()->isolate()->factory()->undefined_value(),
                      zone());
        HintsVector new_arguments({new_receiver}, zone());
        for (auto constant : arguments[0].constants()) {
          ProcessCalleeForCallOrConstruct(constant, base::nullopt,
                                          new_arguments,
                                          SpeculationMode::kDisallowSpeculation,
                                          kMissingArgumentsAreUnknown);
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
          ProcessCalleeForCallOrConstruct(constant, base::nullopt,
                                          new_arguments,
                                          SpeculationMode::kDisallowSpeculation,
                                          kMissingArgumentsAreUnknown);
        }
      }
      break;
    case Builtins::kFunctionPrototypeCall:
      if (arguments.size() >= 1) {
        HintsVector new_arguments(arguments.begin() + 1, arguments.end(),
                                  zone());
        for (auto constant : arguments[0].constants()) {
          ProcessCalleeForCallOrConstruct(
              constant, base::nullopt, new_arguments,
              SpeculationMode::kDisallowSpeculation, padding);
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
        ProcessHintsForFunctionBind(arguments[0]);
      }
      break;
    case Builtins::kObjectGetPrototypeOf:
    case Builtins::kReflectGetPrototypeOf:
      if (arguments.size() >= 2) {
        ProcessHintsForObjectGetPrototype(arguments[1]);
      } else {
        Hints undefined_hint = Hints::SingleConstant(
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
    if (!hint->IsJSReceiver()) continue;
    Handle<JSReceiver> receiver(Handle<JSReceiver>::cast(hint));
    processMap(handle(receiver->map(), broker()->isolate()));
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
    it->second->Merge(environment());
  }
}

void SerializerForBackgroundCompilation::IncorporateJumpTargetEnvironment(
    int target_offset) {
  auto it = jump_target_environments_.find(target_offset);
  if (it != jump_target_environments_.end()) {
    environment()->Merge(it->second);
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
  environment()->return_value_hints().Add(environment()->accumulator_hints(),
                                          zone());
  environment()->ClearEphemeralHints();
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

void SerializerForBackgroundCompilation::Environment::ExportRegisterHints(
    interpreter::Register first, size_t count, HintsVector* dst) {
  const int reg_base = first.index();
  for (int i = 0; i < static_cast<int>(count); ++i) {
    dst->push_back(register_hints(interpreter::Register(reg_base + i)));
  }
}

void SerializerForBackgroundCompilation::VisitConstruct(
    BytecodeArrayIterator* iterator) {
  Hints const& new_target = environment()->accumulator_hints();
  Hints const& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  size_t reg_count = iterator->GetRegisterCountOperand(2);
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  HintsVector arguments(zone());
  environment()->ExportRegisterHints(first_reg, reg_count, &arguments);

  ProcessCallOrConstruct(callee, new_target, arguments, slot,
                         kMissingArgumentsAreUndefined);
}

void SerializerForBackgroundCompilation::VisitConstructWithSpread(
    BytecodeArrayIterator* iterator) {
  Hints const& new_target = environment()->accumulator_hints();
  Hints const& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  size_t reg_count = iterator->GetRegisterCountOperand(2);
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  HintsVector arguments(zone());
  environment()->ExportRegisterHints(first_reg, reg_count, &arguments);
  DCHECK(!arguments.empty());
  arguments.pop_back();  // Remove the spread element.
  ProcessCallOrConstruct(callee, new_target, arguments, slot,
                         kMissingArgumentsAreUnknown);
}

void SerializerForBackgroundCompilation::ProcessGlobalAccess(FeedbackSlot slot,
                                                             bool is_load) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForGlobalAccess(source);

  if (is_load) {
    environment()->accumulator_hints().Clear();
    if (feedback.kind() == ProcessedFeedback::kGlobalAccess) {
      // We may be able to contribute to accumulator constant hints.
      base::Optional<ObjectRef> value =
          feedback.AsGlobalAccess().GetConstantHint();
      if (value.has_value()) {
        environment()->accumulator_hints().AddConstant(value->object(), zone());
      }
    } else {
      DCHECK(feedback.IsInsufficient());
    }
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
  environment()->accumulator_hints().Clear();
}

void SerializerForBackgroundCompilation::VisitLdaLookupSlotInsideTypeof(
    BytecodeArrayIterator* iterator) {
  ObjectRef(broker(),
            iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  environment()->accumulator_hints().Clear();
}

void SerializerForBackgroundCompilation::ProcessCheckContextExtensions(
    int depth) {
  // for BytecodeGraphBuilder::CheckContextExtensions.
  Hints& context_hints = environment()->current_context_hints();
  for (int i = 0; i < depth; i++) {
    ProcessContextAccess(context_hints, Context::EXTENSION_INDEX, i,
                         kSerializeSlot);
  }
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
  environment()->accumulator_hints().Clear();
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
  FeedbackSource source(environment()->function().feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForCompareOperation(source);
  if (BailoutOnUninitialized(feedback)) return;
  environment()->accumulator_hints().Clear();
}

void SerializerForBackgroundCompilation::ProcessForIn(FeedbackSlot slot) {
  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback = broker()->ProcessFeedbackForForIn(source);
  if (BailoutOnUninitialized(feedback)) return;
  environment()->accumulator_hints().Clear();
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
  environment()->accumulator_hints().Clear();
}

PropertyAccessInfo
SerializerForBackgroundCompilation::ProcessMapForNamedPropertyAccess(
    MapRef receiver_map, NameRef const& name, AccessMode access_mode,
    base::Optional<JSObjectRef> receiver, Hints* new_accumulator_hints) {
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
      new_accumulator_hints->AddConstant(cell->value().object(), zone());
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

      // For JSCallReducer::ReduceJSCall.
      function.Serialize();

      // For JSCallReducer::ReduceCallApiFunction.
      Handle<SharedFunctionInfo> sfi = function.shared().object();
      if (sfi->IsApiFunction()) {
        FunctionTemplateInfoRef fti_ref(
            broker(), handle(sfi->get_api_func_data(), broker()->isolate()));
        if (fti_ref.has_call_code()) fti_ref.SerializeCallCode();
        ProcessReceiverMapForApiCall(fti_ref, receiver_map.object());
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

  // For PropertyAccessBuilder::TryBuildLoadConstantDataField
  if (access_mode == AccessMode::kLoad) {
    if (access_info.IsDataConstant()) {
      base::Optional<JSObjectRef> holder;
      Handle<JSObject> prototype;
      if (access_info.holder().ToHandle(&prototype)) {
        holder = JSObjectRef(broker(), prototype);
      } else {
        CHECK_IMPLIES(receiver.has_value(),
                      receiver->map().equals(receiver_map));
        holder = receiver;
      }

      if (holder.has_value()) {
        base::Optional<ObjectRef> constant(holder->GetOwnDataProperty(
            access_info.field_representation(), access_info.field_index(),
            SerializationPolicy::kSerializeIfNeeded));
        if (constant.has_value()) {
          new_accumulator_hints->AddConstant(constant->object(), zone());
        }
      }
    }
  }

  return access_info;
}

void SerializerForBackgroundCompilation::VisitLdaKeyedProperty(
    BytecodeArrayIterator* iterator) {
  Hints const& key = environment()->accumulator_hints();
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kLoad, true);
}

void SerializerForBackgroundCompilation::ProcessKeyedPropertyAccess(
    Hints const& receiver, Hints const& key, FeedbackSlot slot,
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
      ProcessElementAccess(receiver, key, feedback.AsElementAccess(),
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
    environment()->accumulator_hints().Clear();
    environment()->accumulator_hints().Add(new_accumulator_hints, zone());
  } else {
    DCHECK(new_accumulator_hints.IsEmpty());
  }
}

void SerializerForBackgroundCompilation::ProcessNamedPropertyAccess(
    Hints const& receiver, NameRef const& name, FeedbackSlot slot,
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
      // TODO(neis): Propagate feedback maps to receiver hints.
      break;
    case ProcessedFeedback::kInsufficient:
      break;
    default:
      UNREACHABLE();
  }

  if (access_mode == AccessMode::kLoad) {
    environment()->accumulator_hints().Clear();
    environment()->accumulator_hints().Add(new_accumulator_hints, zone());
  } else {
    DCHECK(new_accumulator_hints.IsEmpty());
  }
}

void SerializerForBackgroundCompilation::ProcessNamedAccess(
    Hints receiver, NamedAccessFeedback const& feedback, AccessMode access_mode,
    Hints* new_accumulator_hints) {
  for (Handle<Map> map : feedback.maps()) {
    MapRef map_ref(broker(), map);
    ProcessMapForNamedPropertyAccess(map_ref, feedback.name(), access_mode,
                                     base::nullopt, new_accumulator_hints);
  }

  for (Handle<Map> map :
       GetRelevantReceiverMaps(broker()->isolate(), receiver.maps())) {
    MapRef map_ref(broker(), map);
    ProcessMapForNamedPropertyAccess(map_ref, feedback.name(), access_mode,
                                     base::nullopt, new_accumulator_hints);
  }

  for (Handle<Object> hint : receiver.constants()) {
    ObjectRef object(broker(), hint);
    if (access_mode == AccessMode::kLoad && object.IsJSObject()) {
      MapRef map_ref = object.AsJSObject().map();
      ProcessMapForNamedPropertyAccess(map_ref, feedback.name(), access_mode,
                                       object.AsJSObject(),
                                       new_accumulator_hints);
    }
    // For JSNativeContextSpecialization::ReduceJSLoadNamed.
    if (access_mode == AccessMode::kLoad && object.IsJSFunction() &&
        feedback.name().equals(ObjectRef(
            broker(), broker()->isolate()->factory()->prototype_string()))) {
      JSFunctionRef function = object.AsJSFunction();
      function.Serialize();
      if (new_accumulator_hints != nullptr && function.has_prototype()) {
        new_accumulator_hints->AddConstant(function.prototype().object(),
                                           zone());
      }
    }
    // TODO(neis): Also record accumulator hint for string.length and maybe
    // more?
  }
}

void SerializerForBackgroundCompilation::ProcessElementAccess(
    Hints receiver, Hints key, ElementAccessFeedback const& feedback,
    AccessMode access_mode) {
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
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
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
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
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
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  NameRef name(broker(),
               iterator->GetConstantForIndexOperand(1, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessNamedPropertyAccess(receiver, name, slot, AccessMode::kStoreInLiteral);
}

void SerializerForBackgroundCompilation::VisitTestIn(
    BytecodeArrayIterator* iterator) {
  Hints const& receiver = environment()->accumulator_hints();
  Hints const& key =
      environment()->register_hints(iterator->GetRegisterOperand(0));
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
  Hints const& lhs =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Hints rhs = environment()->accumulator_hints();
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  Hints new_accumulator_hints;

  if (slot.IsInvalid() || feedback_vector().is_null()) return;
  FeedbackSource source(feedback_vector(), slot);
  ProcessedFeedback const& feedback =
      broker()->ProcessFeedbackForInstanceOf(source);

  // Incorporate feedback (about rhs) into hints copy to simplify processing.
  if (!feedback.IsInsufficient()) {
    InstanceOfFeedback const& rhs_feedback = feedback.AsInstanceOf();
    if (rhs_feedback.value().has_value()) {
      Handle<JSObject> constructor = rhs_feedback.value()->object();
      rhs.AddConstant(constructor, zone());
    }
  }

  bool walk_prototypes = false;
  for (Handle<Object> constant : rhs.constants()) {
    ProcessConstantForInstanceOf(ObjectRef(broker(), constant),
                                 &walk_prototypes);
  }
  if (walk_prototypes) ProcessHintsForHasInPrototypeChain(lhs);

  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().Add(new_accumulator_hints, zone());
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
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Hints const& key =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStore, true);
}

void SerializerForBackgroundCompilation::VisitStaInArrayLiteral(
    BytecodeArrayIterator* iterator) {
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Hints const& key =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStoreInLiteral,
                             true);
}

void SerializerForBackgroundCompilation::VisitStaDataPropertyInLiteral(
    BytecodeArrayIterator* iterator) {
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Hints const& key =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStoreInLiteral,
                             false);
}

#define DEFINE_CLEAR_ENVIRONMENT(name, ...)             \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    environment()->ClearEphemeralHints();               \
  }
CLEAR_ENVIRONMENT_LIST(DEFINE_CLEAR_ENVIRONMENT)
#undef DEFINE_CLEAR_ENVIRONMENT

#define DEFINE_CLEAR_ACCUMULATOR(name, ...)             \
  void SerializerForBackgroundCompilation::Visit##name( \
      BytecodeArrayIterator* iterator) {                \
    environment()->accumulator_hints().Clear();         \
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
    environment()->ClearEphemeralHints();               \
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
#undef CLEAR_ENVIRONMENT_LIST
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
