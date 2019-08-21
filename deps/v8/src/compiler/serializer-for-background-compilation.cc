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
#include "src/compiler/vector-slot-pair.h"
#include "src/handles/handles-inl.h"
#include "src/ic/call-optimization.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects/code.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"
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
  V(Add)                          \
  V(AddSmi)                       \
  V(BitwiseAnd)                   \
  V(BitwiseAndSmi)                \
  V(BitwiseNot)                   \
  V(BitwiseOr)                    \
  V(BitwiseOrSmi)                 \
  V(BitwiseXor)                   \
  V(BitwiseXorSmi)                \
  V(CallRuntime)                  \
  V(CloneObject)                  \
  V(CreateArrayFromIterable)      \
  V(CreateArrayLiteral)           \
  V(CreateEmptyArrayLiteral)      \
  V(CreateEmptyObjectLiteral)     \
  V(CreateMappedArguments)        \
  V(CreateObjectLiteral)          \
  V(CreateRegExpLiteral)          \
  V(CreateRestParameter)          \
  V(CreateUnmappedArguments)      \
  V(Dec)                          \
  V(DeletePropertySloppy)         \
  V(DeletePropertyStrict)         \
  V(Div)                          \
  V(DivSmi)                       \
  V(Exp)                          \
  V(ExpSmi)                       \
  V(ForInContinue)                \
  V(ForInEnumerate)               \
  V(ForInNext)                    \
  V(ForInStep)                    \
  V(Inc)                          \
  V(LdaLookupSlot)                \
  V(LdaLookupSlotInsideTypeof)    \
  V(LogicalNot)                   \
  V(Mod)                          \
  V(ModSmi)                       \
  V(Mul)                          \
  V(MulSmi)                       \
  V(Negate)                       \
  V(SetPendingMessage)            \
  V(ShiftLeft)                    \
  V(ShiftLeftSmi)                 \
  V(ShiftRight)                   \
  V(ShiftRightLogical)            \
  V(ShiftRightLogicalSmi)         \
  V(ShiftRightSmi)                \
  V(StaLookupSlot)                \
  V(Sub)                          \
  V(SubSmi)                       \
  V(TestEqual)                    \
  V(TestEqualStrict)              \
  V(TestGreaterThan)              \
  V(TestGreaterThanOrEqual)       \
  V(TestInstanceOf)               \
  V(TestLessThan)                 \
  V(TestLessThanOrEqual)          \
  V(TestNull)                     \
  V(TestReferenceEqual)           \
  V(TestTypeOf)                   \
  V(TestUndefined)                \
  V(TestUndetectable)             \
  V(ToBooleanLogicalNot)          \
  V(ToName)                       \
  V(ToNumber)                     \
  V(ToNumeric)                    \
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
  V(JumpIfUndefinedConstant)

#define IGNORED_BYTECODE_LIST(V)      \
  V(CallNoFeedback)                   \
  V(IncBlockCounter)                  \
  V(LdaNamedPropertyNoFeedback)       \
  V(StackCheck)                       \
  V(StaNamedPropertyNoFeedback)       \
  V(ThrowReferenceErrorIfHole)        \
  V(ThrowSuperAlreadyCalledIfNotHole) \
  V(ThrowSuperNotCalledIfHole)

#define UNREACHABLE_BYTECODE_LIST(V) \
  V(ExtraWide)                       \
  V(Illegal)                         \
  V(Wide)

#define SUPPORTED_BYTECODE_LIST(V)    \
  V(CallAnyReceiver)                  \
  V(CallJSRuntime)                    \
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
  V(CreateBlockContext)               \
  V(CreateCatchContext)               \
  V(CreateClosure)                    \
  V(CreateEvalContext)                \
  V(CreateFunctionContext)            \
  V(CreateWithContext)                \
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
  V(LdaNamedProperty)                 \
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
  V(StaGlobal)                        \
  V(StaInArrayLiteral)                \
  V(StaKeyedProperty)                 \
  V(StaModuleVariable)                \
  V(StaNamedOwnProperty)              \
  V(StaNamedProperty)                 \
  V(Star)                             \
  V(SwitchOnGeneratorState)           \
  V(SwitchOnSmiNoFeedback)            \
  V(TestIn)                           \
  CLEAR_ACCUMULATOR_LIST(V)           \
  CLEAR_ENVIRONMENT_LIST(V)           \
  CONDITIONAL_JUMPS_LIST(V)           \
  IGNORED_BYTECODE_LIST(V)            \
  KILL_ENVIRONMENT_LIST(V)            \
  UNCONDITIONAL_JUMPS_LIST(V)         \
  UNREACHABLE_BYTECODE_LIST(V)

template <typename T>
struct HandleComparator {
  bool operator()(const Handle<T>& lhs, const Handle<T>& rhs) const {
    return lhs.address() < rhs.address();
  }
};

struct VirtualContext {
  unsigned int distance;
  Handle<Context> context;

  VirtualContext(unsigned int distance_in, Handle<Context> context_in)
      : distance(distance_in), context(context_in) {
    CHECK_GT(distance, 0);
  }
  bool operator<(const VirtualContext& other) const {
    return HandleComparator<Context>()(context, other.context) &&
           distance < other.distance;
  }
};

class FunctionBlueprint;
using ConstantsSet = ZoneSet<Handle<Object>, HandleComparator<Object>>;
using VirtualContextsSet = ZoneSet<VirtualContext>;
using MapsSet = ZoneSet<Handle<Map>, HandleComparator<Map>>;
using BlueprintsSet = ZoneSet<FunctionBlueprint>;

class Hints {
 public:
  explicit Hints(Zone* zone);

  const ConstantsSet& constants() const;
  const MapsSet& maps() const;
  const BlueprintsSet& function_blueprints() const;
  const VirtualContextsSet& virtual_contexts() const;

  void AddConstant(Handle<Object> constant);
  void AddMap(Handle<Map> map);
  void AddFunctionBlueprint(FunctionBlueprint function_blueprint);
  void AddVirtualContext(VirtualContext virtual_context);

  void Add(const Hints& other);

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

class FunctionBlueprint {
 public:
  FunctionBlueprint(Handle<JSFunction> function, Isolate* isolate, Zone* zone);

  FunctionBlueprint(Handle<SharedFunctionInfo> shared,
                    Handle<FeedbackVector> feedback_vector,
                    const Hints& context_hints);

  Handle<SharedFunctionInfo> shared() const { return shared_; }
  Handle<FeedbackVector> feedback_vector() const { return feedback_vector_; }
  const Hints& context_hints() const { return context_hints_; }

  bool operator<(const FunctionBlueprint& other) const {
    // A feedback vector is never used for more than one SFI, so it can
    // be used for strict ordering of blueprints.
    DCHECK_IMPLIES(feedback_vector_.equals(other.feedback_vector_),
                   shared_.equals(other.shared_));
    return HandleComparator<FeedbackVector>()(feedback_vector_,
                                              other.feedback_vector_);
  }

 private:
  Handle<SharedFunctionInfo> shared_;
  Handle<FeedbackVector> feedback_vector_;
  Hints context_hints_;
};

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

// The SerializerForBackgroundCompilation makes sure that the relevant function
// data such as bytecode, SharedFunctionInfo and FeedbackVector, used by later
// optimizations in the compiler, is copied to the heap broker.
class SerializerForBackgroundCompilation {
 public:
  SerializerForBackgroundCompilation(
      JSHeapBroker* broker, CompilationDependencies* dependencies, Zone* zone,
      Handle<JSFunction> closure, SerializerForBackgroundCompilationFlags flags,
      BailoutId osr_offset);
  Hints Run();  // NOTE: Returns empty for an already-serialized function.

  class Environment;

 private:
  SerializerForBackgroundCompilation(
      JSHeapBroker* broker, CompilationDependencies* dependencies, Zone* zone,
      CompilationSubject function, base::Optional<Hints> new_target,
      const HintsVector& arguments,
      SerializerForBackgroundCompilationFlags flags);

  bool BailoutOnUninitialized(FeedbackSlot slot);

  void TraverseBytecode();

#define DECLARE_VISIT_BYTECODE(name, ...) \
  void Visit##name(interpreter::BytecodeArrayIterator* iterator);
  SUPPORTED_BYTECODE_LIST(DECLARE_VISIT_BYTECODE)
#undef DECLARE_VISIT_BYTECODE

  void ProcessCallOrConstruct(Hints callee, base::Optional<Hints> new_target,
                              const HintsVector& arguments, FeedbackSlot slot,
                              bool with_spread = false);
  void ProcessCallVarArgs(interpreter::BytecodeArrayIterator* iterator,
                          ConvertReceiverMode receiver_mode,
                          bool with_spread = false);
  void ProcessApiCall(Handle<SharedFunctionInfo> target,
                      const HintsVector& arguments);
  void ProcessReceiverMapForApiCall(
      FunctionTemplateInfoRef& target,  // NOLINT(runtime/references)
      Handle<Map> receiver);
  void ProcessBuiltinCall(Handle<SharedFunctionInfo> target,
                          const HintsVector& arguments);

  void ProcessJump(interpreter::BytecodeArrayIterator* iterator);

  void ProcessKeyedPropertyAccess(Hints const& receiver, Hints const& key,
                                  FeedbackSlot slot, AccessMode mode);
  void ProcessNamedPropertyAccess(interpreter::BytecodeArrayIterator* iterator,
                                  AccessMode mode);
  void ProcessNamedPropertyAccess(Hints const& receiver, NameRef const& name,
                                  FeedbackSlot slot, AccessMode mode);
  void ProcessMapHintsForPromises(Hints const& receiver_hints);
  void ProcessHintsForPromiseResolve(Hints const& resolution_hints);
  void ProcessHintsForRegExpTest(Hints const& regexp_hints);
  PropertyAccessInfo ProcessMapForRegExpTest(MapRef map);
  void ProcessHintsForFunctionCall(Hints const& target_hints);

  GlobalAccessFeedback const* ProcessFeedbackForGlobalAccess(FeedbackSlot slot);
  NamedAccessFeedback const* ProcessFeedbackMapsForNamedAccess(
      const MapHandles& maps, AccessMode mode, NameRef const& name);
  ElementAccessFeedback const* ProcessFeedbackMapsForElementAccess(
      const MapHandles& maps, AccessMode mode,
      KeyedAccessMode const& keyed_mode);
  void ProcessFeedbackForPropertyAccess(FeedbackSlot slot, AccessMode mode,
                                        base::Optional<NameRef> static_name);
  void ProcessMapForNamedPropertyAccess(MapRef const& map, NameRef const& name);

  void ProcessCreateContext();
  enum ContextProcessingMode {
    kIgnoreSlot,
    kSerializeSlot,
    kSerializeSlotAndAddToAccumulator
  };

  void ProcessContextAccess(const Hints& context_hints, int slot, int depth,
                            ContextProcessingMode mode);
  void ProcessImmutableLoad(ContextRef& context,  // NOLINT(runtime/references)
                            int slot, ContextProcessingMode mode);
  void ProcessLdaLookupGlobalSlot(interpreter::BytecodeArrayIterator* iterator);
  void ProcessLdaLookupContextSlot(
      interpreter::BytecodeArrayIterator* iterator);

  // Performs extension lookups for [0, depth) like
  // BytecodeGraphBuilder::CheckContextExtensions().
  void ProcessCheckContextExtensions(int depth);

  Hints RunChildSerializer(CompilationSubject function,
                           base::Optional<Hints> new_target,
                           const HintsVector& arguments, bool with_spread);

  // When (forward-)branching bytecodes are encountered, e.g. a conditional
  // jump, we call ContributeToJumpTargetEnvironment to "remember" the current
  // environment, associated with the jump target offset. When serialization
  // eventually reaches that offset, we call IncorporateJumpTargetEnvironment to
  // merge that environment back into whatever is the current environment then.
  // Note: Since there may be multiple jumps to the same target,
  // ContributeToJumpTargetEnvironment may actually do a merge as well.
  void ContributeToJumpTargetEnvironment(int target_offset);
  void IncorporateJumpTargetEnvironment(int target_offset);

  Handle<BytecodeArray> bytecode_array() const;
  BytecodeAnalysis const& GetBytecodeAnalysis(bool serialize);

  JSHeapBroker* broker() const { return broker_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Zone* zone() const { return zone_; }
  Environment* environment() const { return environment_; }
  SerializerForBackgroundCompilationFlags flags() const { return flags_; }
  BailoutId osr_offset() const { return osr_offset_; }

  JSHeapBroker* const broker_;
  CompilationDependencies* const dependencies_;
  Zone* const zone_;
  Environment* const environment_;
  ZoneUnorderedMap<int, Environment*> jump_target_environments_;
  SerializerForBackgroundCompilationFlags const flags_;
  BailoutId const osr_offset_;
};

void RunSerializerForBackgroundCompilation(
    JSHeapBroker* broker, CompilationDependencies* dependencies, Zone* zone,
    Handle<JSFunction> closure, SerializerForBackgroundCompilationFlags flags,
    BailoutId osr_offset) {
  SerializerForBackgroundCompilation serializer(broker, dependencies, zone,
                                                closure, flags, osr_offset);
  serializer.Run();
}

using BytecodeArrayIterator = interpreter::BytecodeArrayIterator;

FunctionBlueprint::FunctionBlueprint(Handle<SharedFunctionInfo> shared,
                                     Handle<FeedbackVector> feedback_vector,
                                     const Hints& context_hints)
    : shared_(shared),
      feedback_vector_(feedback_vector),
      context_hints_(context_hints) {}

FunctionBlueprint::FunctionBlueprint(Handle<JSFunction> function,
                                     Isolate* isolate, Zone* zone)
    : shared_(handle(function->shared(), isolate)),
      feedback_vector_(handle(function->feedback_vector(), isolate)),
      context_hints_(zone) {
  context_hints_.AddConstant(handle(function->context(), isolate));
}

CompilationSubject::CompilationSubject(Handle<JSFunction> closure,
                                       Isolate* isolate, Zone* zone)
    : blueprint_(closure, isolate, zone), closure_(closure) {
  CHECK(closure->has_feedback_vector());
}

Hints::Hints(Zone* zone)
    : virtual_contexts_(zone),
      constants_(zone),
      maps_(zone),
      function_blueprints_(zone) {}

#ifdef ENABLE_SLOW_DCHECKS
namespace {
template <typename K, typename Compare>
bool SetIncludes(ZoneSet<K, Compare> const& lhs,
                 ZoneSet<K, Compare> const& rhs) {
  return std::all_of(rhs.cbegin(), rhs.cend(),
                     [&](K const& x) { return lhs.find(x) != lhs.cend(); });
}
}  // namespace
bool Hints::Includes(Hints const& other) const {
  return SetIncludes(constants(), other.constants()) &&
         SetIncludes(function_blueprints(), other.function_blueprints()) &&
         SetIncludes(maps(), other.maps());
}
bool Hints::Equals(Hints const& other) const {
  return this->Includes(other) && other.Includes(*this);
}
#endif

const ConstantsSet& Hints::constants() const { return constants_; }

const MapsSet& Hints::maps() const { return maps_; }

const BlueprintsSet& Hints::function_blueprints() const {
  return function_blueprints_;
}

const VirtualContextsSet& Hints::virtual_contexts() const {
  return virtual_contexts_;
}

void Hints::AddVirtualContext(VirtualContext virtual_context) {
  virtual_contexts_.insert(virtual_context);
}

void Hints::AddConstant(Handle<Object> constant) {
  constants_.insert(constant);
}

void Hints::AddMap(Handle<Map> map) { maps_.insert(map); }

void Hints::AddFunctionBlueprint(FunctionBlueprint function_blueprint) {
  function_blueprints_.insert(function_blueprint);
}

void Hints::Add(const Hints& other) {
  for (auto x : other.constants()) AddConstant(x);
  for (auto x : other.maps()) AddMap(x);
  for (auto x : other.function_blueprints()) AddFunctionBlueprint(x);
  for (auto x : other.virtual_contexts()) AddVirtualContext(x);
}

bool Hints::IsEmpty() const {
  return constants().empty() && maps().empty() &&
         function_blueprints().empty() && virtual_contexts().empty();
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
  virtual_contexts_.clear();
  constants_.clear();
  maps_.clear();
  function_blueprints_.clear();
  DCHECK(IsEmpty());
}

class SerializerForBackgroundCompilation::Environment : public ZoneObject {
 public:
  Environment(Zone* zone, CompilationSubject function);
  Environment(Zone* zone, Isolate* isolate, CompilationSubject function,
              base::Optional<Hints> new_target, const HintsVector& arguments);

  bool IsDead() const { return ephemeral_hints_.empty(); }

  void Kill() {
    DCHECK(!IsDead());
    ephemeral_hints_.clear();
    DCHECK(IsDead());
  }

  void Revive() {
    DCHECK(IsDead());
    ephemeral_hints_.resize(ephemeral_hints_size(), Hints(zone()));
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
                           HintsVector& dst);  // NOLINT(runtime/references)

 private:
  friend std::ostream& operator<<(std::ostream& out, const Environment& env);

  int RegisterToLocalIndex(interpreter::Register reg) const;

  Zone* zone() const { return zone_; }
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
      closure_hints_(zone),
      current_context_hints_(zone),
      return_value_hints_(zone),
      ephemeral_hints_(ephemeral_hints_size(), Hints(zone), zone) {
  Handle<JSFunction> closure;
  if (function.closure().ToHandle(&closure)) {
    closure_hints_.AddConstant(closure);
  } else {
    closure_hints_.AddFunctionBlueprint(function.blueprint());
  }

  // Consume blueprint context hint information.
  current_context_hints().Add(function.blueprint().context_hints());
}

SerializerForBackgroundCompilation::Environment::Environment(
    Zone* zone, Isolate* isolate, CompilationSubject function,
    base::Optional<Hints> new_target, const HintsVector& arguments)
    : Environment(zone, function) {
  // Copy the hints for the actually passed arguments, at most up to
  // the parameter_count.
  size_t param_count = static_cast<size_t>(parameter_count());
  for (size_t i = 0; i < std::min(arguments.size(), param_count); ++i) {
    ephemeral_hints_[i] = arguments[i];
  }

  // Pad the rest with "undefined".
  Hints undefined_hint(zone);
  undefined_hint.AddConstant(isolate->factory()->undefined_value());
  for (size_t i = arguments.size(); i < param_count; ++i) {
    ephemeral_hints_[i] = undefined_hint;
  }

  interpreter::Register new_target_reg =
      function_.shared()
          ->GetBytecodeArray()
          .incoming_new_target_or_generator_register();
  if (new_target_reg.is_valid()) {
    DCHECK(register_hints(new_target_reg).IsEmpty());
    if (new_target.has_value()) {
      register_hints(new_target_reg).Add(*new_target);
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
    ephemeral_hints_[i].Add(other->ephemeral_hints_[i]);
  }

  return_value_hints_.Add(other->return_value_hints_);
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
    JSHeapBroker* broker, CompilationDependencies* dependencies, Zone* zone,
    Handle<JSFunction> closure, SerializerForBackgroundCompilationFlags flags,
    BailoutId osr_offset)
    : broker_(broker),
      dependencies_(dependencies),
      zone_(zone),
      environment_(new (zone) Environment(
          zone, CompilationSubject(closure, broker_->isolate(), zone))),
      jump_target_environments_(zone),
      flags_(flags),
      osr_offset_(osr_offset) {
  JSFunctionRef(broker, closure).Serialize();
}

SerializerForBackgroundCompilation::SerializerForBackgroundCompilation(
    JSHeapBroker* broker, CompilationDependencies* dependencies, Zone* zone,
    CompilationSubject function, base::Optional<Hints> new_target,
    const HintsVector& arguments, SerializerForBackgroundCompilationFlags flags)
    : broker_(broker),
      dependencies_(dependencies),
      zone_(zone),
      environment_(new (zone) Environment(zone, broker_->isolate(), function,
                                          new_target, arguments)),
      jump_target_environments_(zone),
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
    FeedbackSlot slot) {
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
  FeedbackNexus nexus(environment()->function().feedback_vector(), slot);
  if (!slot.IsInvalid() && nexus.IsUninitialized()) {
    FeedbackSource source(nexus);
    if (broker()->HasFeedback(source)) {
      DCHECK_EQ(broker()->GetFeedback(source)->kind(),
                ProcessedFeedback::kInsufficient);
    } else {
      broker()->SetFeedback(source,
                            new (broker()->zone()) InsufficientFeedback());
    }
    environment()->Kill();
    return true;
  }
  return false;
}

Hints SerializerForBackgroundCompilation::Run() {
  TraceScope tracer(broker(), this, "SerializerForBackgroundCompilation::Run");
  SharedFunctionInfoRef shared(broker(), environment()->function().shared());
  FeedbackVectorRef feedback_vector(
      broker(), environment()->function().feedback_vector());
  if (shared.IsSerializedForCompilation(feedback_vector)) {
    TRACE_BROKER(broker(), "Already ran serializer for SharedFunctionInfo "
                               << Brief(*shared.object())
                               << ", bailing out.\n");
    return Hints(zone());
  }
  shared.SetSerializedForCompilation(feedback_vector);

  // We eagerly call the {EnsureSourcePositionsAvailable} for all serialized
  // SFIs while still on the main thread. Source positions will later be used
  // by JSInliner::ReduceJSCall.
  if (flags() &
      SerializerForBackgroundCompilationFlag::kCollectSourcePositions) {
    SharedFunctionInfo::EnsureSourcePositionsAvailable(broker()->isolate(),
                                                       shared.object());
  }

  feedback_vector.SerializeSlots();
  TraverseBytecode();
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

Handle<BytecodeArray> SerializerForBackgroundCompilation::bytecode_array()
    const {
  return handle(environment()->function().shared()->GetBytecodeArray(),
                broker()->isolate());
}

BytecodeAnalysis const& SerializerForBackgroundCompilation::GetBytecodeAnalysis(
    bool serialize) {
  return broker()->GetBytecodeAnalysis(
      bytecode_array(), osr_offset(),
      flags() &
          SerializerForBackgroundCompilationFlag::kAnalyzeEnvironmentLiveness,
      serialize);
}

void SerializerForBackgroundCompilation::TraverseBytecode() {
  BytecodeAnalysis const& bytecode_analysis = GetBytecodeAnalysis(true);
  BytecodeArrayRef(broker(), bytecode_array()).SerializeForCompilation();

  BytecodeArrayIterator iterator(bytecode_array());
  ExceptionHandlerMatcher handler_matcher(iterator, bytecode_array());

  for (; !iterator.done(); iterator.Advance()) {
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
      environment()->register_hints(dst).AddConstant(proto.object());
    }
  }
}

void SerializerForBackgroundCompilation::VisitGetTemplateObject(
    BytecodeArrayIterator* iterator) {
  ObjectRef description(
      broker(), iterator->GetConstantForIndexOperand(0, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  FeedbackVectorRef feedback_vector(
      broker(), environment()->function().feedback_vector());
  SharedFunctionInfoRef shared(broker(), environment()->function().shared());
  JSArrayRef template_object =
      shared.GetTemplateObject(description, feedback_vector, slot, true);
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(template_object.object());
}

void SerializerForBackgroundCompilation::VisitLdaTrue(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->true_value());
}

void SerializerForBackgroundCompilation::VisitLdaFalse(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->false_value());
}

void SerializerForBackgroundCompilation::VisitLdaTheHole(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->the_hole_value());
}

void SerializerForBackgroundCompilation::VisitLdaUndefined(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->undefined_value());
}

void SerializerForBackgroundCompilation::VisitLdaNull(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      broker()->isolate()->factory()->null_value());
}

void SerializerForBackgroundCompilation::VisitLdaZero(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      handle(Smi::FromInt(0), broker()->isolate()));
}

void SerializerForBackgroundCompilation::VisitLdaSmi(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(handle(
      Smi::FromInt(iterator->GetImmediateOperand(0)), broker()->isolate()));
}

void SerializerForBackgroundCompilation::VisitInvokeIntrinsic(
    BytecodeArrayIterator* iterator) {
  Runtime::FunctionId functionId = iterator->GetIntrinsicIdOperand(0);
  // For JSNativeContextSpecialization::ReduceJSAsyncFunctionResolve and
  // JSNativeContextSpecialization::ReduceJSResolvePromise.
  if (functionId == Runtime::kInlineAsyncFunctionResolve) {
    interpreter::Register first_reg = iterator->GetRegisterOperand(1);
    size_t reg_count = iterator->GetRegisterCountOperand(2);
    CHECK_EQ(reg_count, 3);
    HintsVector arguments(zone());
    environment()->ExportRegisterHints(first_reg, reg_count, arguments);
    Hints const& resolution_hints = arguments[1];  // The resolution object.
    ProcessHintsForPromiseResolve(resolution_hints);
    environment()->accumulator_hints().Clear();
    return;
  }
  environment()->ClearEphemeralHints();
}

void SerializerForBackgroundCompilation::VisitLdaConstant(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().AddConstant(
      iterator->GetConstantForIndexOperand(0, broker()->isolate()));
}

void SerializerForBackgroundCompilation::VisitPushContext(
    BytecodeArrayIterator* iterator) {
  // Transfer current context hints to the destination register hints.
  Hints& current_context_hints = environment()->current_context_hints();
  Hints& saved_context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  saved_context_hints.Clear();
  saved_context_hints.Add(current_context_hints);

  // New Context is in the accumulator. Put those hints into the current context
  // register hints.
  current_context_hints.Clear();
  current_context_hints.Add(environment()->accumulator_hints());
}

void SerializerForBackgroundCompilation::VisitPopContext(
    BytecodeArrayIterator* iterator) {
  // Replace current context hints with hints given in the argument register.
  Hints& new_context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  environment()->current_context_hints().Clear();
  environment()->current_context_hints().Add(new_context_hints);
}

void SerializerForBackgroundCompilation::ProcessImmutableLoad(
    ContextRef& context_ref, int slot, ContextProcessingMode mode) {
  DCHECK(mode == kSerializeSlot || mode == kSerializeSlotAndAddToAccumulator);
  base::Optional<ObjectRef> slot_value = context_ref.get(slot, true);

  // Also, put the object into the constant hints for the accumulator.
  if (mode == kSerializeSlotAndAddToAccumulator && slot_value.has_value()) {
    environment()->accumulator_hints().AddConstant(slot_value.value().object());
  }
}

void SerializerForBackgroundCompilation::ProcessContextAccess(
    const Hints& context_hints, int slot, int depth,
    ContextProcessingMode mode) {
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
      context_ref = context_ref.previous(&remaining_depth, true);
      if (remaining_depth == 0 && mode != kIgnoreSlot) {
        ProcessImmutableLoad(context_ref, slot, mode);
      }
    }
  }
  for (auto x : context_hints.virtual_contexts()) {
    if (x.distance <= static_cast<unsigned int>(depth)) {
      ContextRef context_ref(broker(), x.context);
      size_t remaining_depth = depth - x.distance;
      context_ref = context_ref.previous(&remaining_depth, true);
      if (remaining_depth == 0 && mode != kIgnoreSlot) {
        ProcessImmutableLoad(context_ref, slot, mode);
      }
    }
  }
}

void SerializerForBackgroundCompilation::VisitLdaContextSlot(
    BytecodeArrayIterator* iterator) {
  Hints& context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  environment()->accumulator_hints().Clear();
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitLdaCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints& context_hints = environment()->current_context_hints();
  environment()->accumulator_hints().Clear();
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitLdaImmutableContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints& context_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  environment()->accumulator_hints().Clear();
  ProcessContextAccess(context_hints, slot, depth,
                       kSerializeSlotAndAddToAccumulator);
}

void SerializerForBackgroundCompilation::VisitLdaImmutableCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints& context_hints = environment()->current_context_hints();
  environment()->accumulator_hints().Clear();
  ProcessContextAccess(context_hints, slot, depth,
                       kSerializeSlotAndAddToAccumulator);
}

void SerializerForBackgroundCompilation::VisitLdaModuleVariable(
    BytecodeArrayIterator* iterator) {
  const int depth = iterator->GetUnsignedImmediateOperand(1);

  // TODO(mvstanton): If we have a constant module, should we serialize the
  // cell as well? Then we could put the value in the accumulator.
  environment()->accumulator_hints().Clear();
  ProcessContextAccess(environment()->current_context_hints(),
                       Context::EXTENSION_INDEX, depth, kSerializeSlot);
}

void SerializerForBackgroundCompilation::VisitStaModuleVariable(
    BytecodeArrayIterator* iterator) {
  const int depth = iterator->GetUnsignedImmediateOperand(1);
  ProcessContextAccess(environment()->current_context_hints(),
                       Context::EXTENSION_INDEX, depth, kSerializeSlot);
}

void SerializerForBackgroundCompilation::VisitStaContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  Hints& register_hints =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  ProcessContextAccess(register_hints, slot, depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitStaCurrentContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot = iterator->GetIndexOperand(0);
  const int depth = 0;
  Hints& context_hints = environment()->current_context_hints();
  ProcessContextAccess(context_hints, slot, depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitLdar(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();
  environment()->accumulator_hints().Add(
      environment()->register_hints(iterator->GetRegisterOperand(0)));
}

void SerializerForBackgroundCompilation::VisitStar(
    BytecodeArrayIterator* iterator) {
  interpreter::Register reg = iterator->GetRegisterOperand(0);
  environment()->register_hints(reg).Clear();
  environment()->register_hints(reg).Add(environment()->accumulator_hints());
}

void SerializerForBackgroundCompilation::VisitMov(
    BytecodeArrayIterator* iterator) {
  interpreter::Register src = iterator->GetRegisterOperand(0);
  interpreter::Register dst = iterator->GetRegisterOperand(1);
  environment()->register_hints(dst).Clear();
  environment()->register_hints(dst).Add(environment()->register_hints(src));
}

void SerializerForBackgroundCompilation::VisitCreateFunctionContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext();
}

void SerializerForBackgroundCompilation::VisitCreateBlockContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext();
}

void SerializerForBackgroundCompilation::VisitCreateEvalContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext();
}

void SerializerForBackgroundCompilation::VisitCreateWithContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext();
}

void SerializerForBackgroundCompilation::VisitCreateCatchContext(
    BytecodeArrayIterator* iterator) {
  ProcessCreateContext();
}

void SerializerForBackgroundCompilation::ProcessCreateContext() {
  Hints& accumulator_hints = environment()->accumulator_hints();
  accumulator_hints.Clear();
  Hints& current_context_hints = environment()->current_context_hints();

  // For each constant context, we must create a virtual context from
  // it of distance one.
  for (auto x : current_context_hints.constants()) {
    if (x->IsContext()) {
      Handle<Context> as_context(Handle<Context>::cast(x));
      accumulator_hints.AddVirtualContext(VirtualContext(1, as_context));
    }
  }

  // For each virtual context, we must create a virtual context from
  // it of distance {existing distance} + 1.
  for (auto x : current_context_hints.virtual_contexts()) {
    accumulator_hints.AddVirtualContext(
        VirtualContext(x.distance + 1, x.context));
  }
}

void SerializerForBackgroundCompilation::VisitCreateClosure(
    BytecodeArrayIterator* iterator) {
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>::cast(
      iterator->GetConstantForIndexOperand(0, broker()->isolate()));

  Handle<FeedbackCell> feedback_cell =
      environment()->function().feedback_vector()->GetClosureFeedbackCell(
          iterator->GetIndexOperand(1));
  FeedbackCellRef feedback_cell_ref(broker(), feedback_cell);
  Handle<Object> cell_value(feedback_cell->value(), broker()->isolate());
  ObjectRef cell_value_ref(broker(), cell_value);

  environment()->accumulator_hints().Clear();
  if (cell_value->IsFeedbackVector()) {
    // Gather the context hints from the current context register hint
    // structure.
    FunctionBlueprint blueprint(shared,
                                Handle<FeedbackVector>::cast(cell_value),
                                environment()->current_context_hints());

    environment()->accumulator_hints().AddFunctionBlueprint(blueprint);
  }
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver(
    BytecodeArrayIterator* iterator) {
  ProcessCallVarArgs(iterator, ConvertReceiverMode::kNullOrUndefined);
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver0(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);

  Hints receiver(zone());
  receiver.AddConstant(broker()->isolate()->factory()->undefined_value());

  HintsVector parameters({receiver}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot);
}

void SerializerForBackgroundCompilation::VisitCallUndefinedReceiver1(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const Hints& arg0 =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);

  Hints receiver(zone());
  receiver.AddConstant(broker()->isolate()->factory()->undefined_value());

  HintsVector parameters({receiver, arg0}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot);
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

  Hints receiver(zone());
  receiver.AddConstant(broker()->isolate()->factory()->undefined_value());

  HintsVector parameters({receiver, arg0, arg1}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot);
}

void SerializerForBackgroundCompilation::VisitCallAnyReceiver(
    BytecodeArrayIterator* iterator) {
  ProcessCallVarArgs(iterator, ConvertReceiverMode::kAny);
}

void SerializerForBackgroundCompilation::VisitCallProperty(
    BytecodeArrayIterator* iterator) {
  ProcessCallVarArgs(iterator, ConvertReceiverMode::kNullOrUndefined);
}

void SerializerForBackgroundCompilation::VisitCallProperty0(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  const Hints& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);

  HintsVector parameters({receiver}, zone());
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot);
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
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot);
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
  ProcessCallOrConstruct(callee, base::nullopt, parameters, slot);
}

void SerializerForBackgroundCompilation::VisitCallWithSpread(
    BytecodeArrayIterator* iterator) {
  ProcessCallVarArgs(iterator, ConvertReceiverMode::kAny, true);
}

void SerializerForBackgroundCompilation::VisitCallJSRuntime(
    BytecodeArrayIterator* iterator) {
  environment()->accumulator_hints().Clear();

  // BytecodeGraphBuilder::VisitCallJSRuntime needs the {runtime_index}
  // slot in the native context to be serialized.
  const int runtime_index = iterator->GetNativeContextIndexOperand(0);
  broker()->native_context().get(runtime_index, true);
}

Hints SerializerForBackgroundCompilation::RunChildSerializer(
    CompilationSubject function, base::Optional<Hints> new_target,
    const HintsVector& arguments, bool with_spread) {
  if (with_spread) {
    DCHECK_LT(0, arguments.size());
    // Pad the missing arguments in case we were called with spread operator.
    // Drop the last actually passed argument, which contains the spread.
    // We don't know what the spread element produces. Therefore we pretend
    // that the function is called with the maximal number of parameters and
    // that we have no information about the parameters that were not
    // explicitly provided.
    HintsVector padded = arguments;
    padded.pop_back();  // Remove the spread element.
    // Fill the rest with empty hints.
    padded.resize(
        function.blueprint().shared()->GetBytecodeArray().parameter_count(),
        Hints(zone()));
    return RunChildSerializer(function, new_target, padded, false);
  }

  SerializerForBackgroundCompilation child_serializer(
      broker(), dependencies(), zone(), function, new_target, arguments,
      flags());
  return child_serializer.Run();
}

namespace {
base::Optional<HeapObjectRef> GetHeapObjectFeedback(
    JSHeapBroker* broker, Handle<FeedbackVector> feedback_vector,
    FeedbackSlot slot) {
  if (slot.IsInvalid()) return base::nullopt;
  FeedbackNexus nexus(feedback_vector, slot);
  VectorSlotPair feedback(feedback_vector, slot, nexus.ic_state());
  DCHECK(feedback.IsValid());
  if (nexus.IsUninitialized()) return base::nullopt;
  HeapObject object;
  if (!nexus.GetFeedback()->GetHeapObject(&object)) return base::nullopt;
  return HeapObjectRef(broker, handle(object, broker->isolate()));
}
}  // namespace

void SerializerForBackgroundCompilation::ProcessCallOrConstruct(
    Hints callee, base::Optional<Hints> new_target,
    const HintsVector& arguments, FeedbackSlot slot, bool with_spread) {
  // TODO(neis): Make this part of ProcessFeedback*?
  if (BailoutOnUninitialized(slot)) return;

  // Incorporate feedback into hints.
  base::Optional<HeapObjectRef> feedback = GetHeapObjectFeedback(
      broker(), environment()->function().feedback_vector(), slot);
  if (feedback.has_value() && feedback->map().is_callable()) {
    if (new_target.has_value()) {
      // Construct; feedback is new_target, which often is also the callee.
      new_target->AddConstant(feedback->object());
      callee.AddConstant(feedback->object());
    } else {
      // Call; feedback is callee.
      callee.AddConstant(feedback->object());
    }
  }

  environment()->accumulator_hints().Clear();

  for (auto hint : callee.constants()) {
    if (!hint->IsJSFunction()) continue;

    Handle<JSFunction> function = Handle<JSFunction>::cast(hint);
    JSFunctionRef(broker(), function).Serialize();

    Handle<SharedFunctionInfo> shared(function->shared(), broker()->isolate());

    if (shared->IsApiFunction()) {
      ProcessApiCall(shared, arguments);
      DCHECK(!shared->IsInlineable());
    } else if (shared->HasBuiltinId()) {
      ProcessBuiltinCall(shared, arguments);
      DCHECK(!shared->IsInlineable());
    }

    if (!shared->IsInlineable() || !function->has_feedback_vector()) continue;

    environment()->accumulator_hints().Add(RunChildSerializer(
        CompilationSubject(function, broker()->isolate(), zone()), new_target,
        arguments, with_spread));
  }

  for (auto hint : callee.function_blueprints()) {
    Handle<SharedFunctionInfo> shared = hint.shared();

    if (shared->IsApiFunction()) {
      ProcessApiCall(shared, arguments);
      DCHECK(!shared->IsInlineable());
    } else if (shared->HasBuiltinId()) {
      ProcessBuiltinCall(shared, arguments);
      DCHECK(!shared->IsInlineable());
    }

    if (!shared->IsInlineable()) continue;
    environment()->accumulator_hints().Add(RunChildSerializer(
        CompilationSubject(hint), new_target, arguments, with_spread));
  }
}

void SerializerForBackgroundCompilation::ProcessCallVarArgs(
    BytecodeArrayIterator* iterator, ConvertReceiverMode receiver_mode,
    bool with_spread) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  int reg_count = static_cast<int>(iterator->GetRegisterCountOperand(2));
  FeedbackSlot slot = iterator->GetSlotOperand(3);

  HintsVector arguments(zone());
  // The receiver is either given in the first register or it is implicitly
  // the {undefined} value.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    Hints receiver(zone());
    receiver.AddConstant(broker()->isolate()->factory()->undefined_value());
    arguments.push_back(receiver);
  }
  environment()->ExportRegisterHints(first_reg, reg_count, arguments);

  ProcessCallOrConstruct(callee, base::nullopt, arguments, slot);
}

void SerializerForBackgroundCompilation::ProcessApiCall(
    Handle<SharedFunctionInfo> target, const HintsVector& arguments) {
  FunctionTemplateInfoRef target_template_info(
      broker(), handle(target->function_data(), broker()->isolate()));
  if (!target_template_info.has_call_code()) return;

  target_template_info.SerializeCallCode();

  SharedFunctionInfoRef target_ref(broker(), target);
  target_ref.SerializeFunctionTemplateInfo();

  if (target_template_info.accept_any_receiver() &&
      target_template_info.is_signature_undefined())
    return;

  CHECK_GE(arguments.size(), 1);
  Hints const& receiver_hints = arguments[0];
  for (auto hint : receiver_hints.constants()) {
    if (hint->IsUndefined()) {
      // The receiver is the global proxy.
      Handle<JSGlobalProxy> global_proxy =
          broker()->native_context().global_proxy_object().object();
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
    FunctionTemplateInfoRef& target, Handle<Map> receiver) {
  if (receiver->is_access_check_needed()) {
    return;
  }

  MapRef receiver_map(broker(), receiver);
  TRACE_BROKER(broker(), "Serializing holder for target:" << target);

  target.LookupHolderOfExpectedType(receiver_map, true);
}

void SerializerForBackgroundCompilation::ProcessBuiltinCall(
    Handle<SharedFunctionInfo> target, const HintsVector& arguments) {
  DCHECK(target->HasBuiltinId());
  const int builtin_id = target->builtin_id();
  const char* name = Builtins::name(builtin_id);
  TRACE_BROKER(broker(), "Serializing for call to builtin " << name);
  switch (builtin_id) {
    case Builtins::kPromisePrototypeCatch: {
      // For JSCallReducer::ReducePromisePrototypeCatch.
      CHECK_GE(arguments.size(), 1);
      ProcessMapHintsForPromises(arguments[0]);
      break;
    }
    case Builtins::kPromisePrototypeFinally: {
      // For JSCallReducer::ReducePromisePrototypeFinally.
      CHECK_GE(arguments.size(), 1);
      ProcessMapHintsForPromises(arguments[0]);
      break;
    }
    case Builtins::kPromisePrototypeThen: {
      // For JSCallReducer::ReducePromisePrototypeThen.
      CHECK_GE(arguments.size(), 1);
      ProcessMapHintsForPromises(arguments[0]);
      break;
    }
    case Builtins::kPromiseResolveTrampoline:
      // For JSCallReducer::ReducePromiseInternalResolve and
      // JSNativeContextSpecialization::ReduceJSResolvePromise.
      if (arguments.size() >= 2) {
        Hints const& resolution_hints = arguments[1];
        ProcessHintsForPromiseResolve(resolution_hints);
      }
      break;
    case Builtins::kPromiseInternalResolve:
      // For JSCallReducer::ReducePromiseInternalResolve and
      // JSNativeContextSpecialization::ReduceJSResolvePromise.
      if (arguments.size() >= 3) {
        Hints const& resolution_hints = arguments[2];
        ProcessHintsForPromiseResolve(resolution_hints);
      }
      break;
    case Builtins::kRegExpPrototypeTest: {
      // For JSCallReducer::ReduceRegExpPrototypeTest.
      if (arguments.size() >= 1) {
        Hints const& regexp_hints = arguments[0];
        ProcessHintsForRegExpTest(regexp_hints);
      }
      break;
    }
    case Builtins::kFunctionPrototypeCall:
      if (arguments.size() >= 1) {
        Hints const& target_hints = arguments[0];
        ProcessHintsForFunctionCall(target_hints);
      }
      break;
    default:
      break;
  }
}

void SerializerForBackgroundCompilation::ProcessHintsForPromiseResolve(
    Hints const& resolution_hints) {
  auto processMap = [&](Handle<Map> map) {
    broker()->CreateAccessInfoForLoadingThen(MapRef(broker(), map),
                                             dependencies());
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
  PropertyAccessInfo ai_exec =
      broker()->CreateAccessInfoForLoadingExec(map, dependencies());

  Handle<JSObject> holder;
  if (ai_exec.IsDataConstant() && ai_exec.holder().ToHandle(&holder)) {
    // The property is on the prototype chain.
    JSObjectRef holder_ref(broker(), holder);
    holder_ref.GetOwnProperty(ai_exec.field_representation(),
                              ai_exec.field_index(), true);
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
      holder_ref.GetOwnProperty(ai_exec.field_representation(),
                                ai_exec.field_index(), true);
    }
  }

  for (auto map : regexp_hints.maps()) {
    if (!map->IsJSRegExpMap()) continue;
    ProcessMapForRegExpTest(MapRef(broker(), map));
  }
}

void SerializerForBackgroundCompilation::ProcessHintsForFunctionCall(
    Hints const& target_hints) {
  for (auto constant : target_hints.constants()) {
    if (!constant->IsJSFunction()) continue;
    JSFunctionRef func(broker(), constant);
    func.Serialize();
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
  environment()->return_value_hints().Add(environment()->accumulator_hints());
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
  for (const auto& target : GetBytecodeAnalysis(false).resume_jump_targets()) {
    ContributeToJumpTargetEnvironment(target.target_offset());
  }
}

void SerializerForBackgroundCompilation::Environment::ExportRegisterHints(
    interpreter::Register first, size_t count, HintsVector& dst) {
  const int reg_base = first.index();
  for (int i = 0; i < static_cast<int>(count); ++i) {
    dst.push_back(register_hints(interpreter::Register(reg_base + i)));
  }
}

void SerializerForBackgroundCompilation::VisitConstruct(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  size_t reg_count = iterator->GetRegisterCountOperand(2);
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  const Hints& new_target = environment()->accumulator_hints();

  HintsVector arguments(zone());
  environment()->ExportRegisterHints(first_reg, reg_count, arguments);

  ProcessCallOrConstruct(callee, new_target, arguments, slot);
}

void SerializerForBackgroundCompilation::VisitConstructWithSpread(
    BytecodeArrayIterator* iterator) {
  const Hints& callee =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  interpreter::Register first_reg = iterator->GetRegisterOperand(1);
  size_t reg_count = iterator->GetRegisterCountOperand(2);
  FeedbackSlot slot = iterator->GetSlotOperand(3);
  const Hints& new_target = environment()->accumulator_hints();

  HintsVector arguments(zone());
  environment()->ExportRegisterHints(first_reg, reg_count, arguments);

  ProcessCallOrConstruct(callee, new_target, arguments, slot, true);
}

GlobalAccessFeedback const*
SerializerForBackgroundCompilation::ProcessFeedbackForGlobalAccess(
    FeedbackSlot slot) {
  if (slot.IsInvalid()) return nullptr;
  if (environment()->function().feedback_vector().is_null()) return nullptr;
  FeedbackSource source(environment()->function().feedback_vector(), slot);

  if (broker()->HasFeedback(source)) {
    return broker()->GetGlobalAccessFeedback(source);
  }

  const GlobalAccessFeedback* feedback =
      broker()->ProcessFeedbackForGlobalAccess(source);
  broker()->SetFeedback(source, feedback);
  return feedback;
}

void SerializerForBackgroundCompilation::VisitLdaGlobal(
    BytecodeArrayIterator* iterator) {
  FeedbackSlot slot = iterator->GetSlotOperand(1);

  environment()->accumulator_hints().Clear();
  GlobalAccessFeedback const* feedback = ProcessFeedbackForGlobalAccess(slot);
  if (feedback != nullptr) {
    // We may be able to contribute to accumulator constant hints.
    base::Optional<ObjectRef> value = feedback->GetConstantHint();
    if (value.has_value()) {
      environment()->accumulator_hints().AddConstant(value->object());
    }
  }
}

void SerializerForBackgroundCompilation::VisitLdaGlobalInsideTypeof(
    BytecodeArrayIterator* iterator) {
  VisitLdaGlobal(iterator);
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
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessFeedbackForGlobalAccess(slot);
}

void SerializerForBackgroundCompilation::ProcessLdaLookupContextSlot(
    BytecodeArrayIterator* iterator) {
  const int slot_index = iterator->GetIndexOperand(1);
  const int depth = iterator->GetUnsignedImmediateOperand(2);
  ProcessCheckContextExtensions(depth);
  Hints& context_hints = environment()->current_context_hints();
  environment()->accumulator_hints().Clear();
  ProcessContextAccess(context_hints, slot_index, depth, kIgnoreSlot);
}

void SerializerForBackgroundCompilation::VisitLdaLookupContextSlot(
    BytecodeArrayIterator* iterator) {
  ProcessLdaLookupContextSlot(iterator);
}

void SerializerForBackgroundCompilation::VisitLdaLookupContextSlotInsideTypeof(
    BytecodeArrayIterator* iterator) {
  ProcessLdaLookupContextSlot(iterator);
}

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

ElementAccessFeedback const*
SerializerForBackgroundCompilation::ProcessFeedbackMapsForElementAccess(
    const MapHandles& maps, AccessMode mode,
    KeyedAccessMode const& keyed_mode) {
  ElementAccessFeedback const* result =
      broker()->ProcessFeedbackMapsForElementAccess(maps, keyed_mode);
  for (ElementAccessFeedback::MapIterator it = result->all_maps(broker());
       !it.done(); it.advance()) {
    switch (mode) {
      case AccessMode::kHas:
      case AccessMode::kLoad:
        it.current().SerializeForElementLoad();
        break;
      case AccessMode::kStore:
        it.current().SerializeForElementStore();
        break;
      case AccessMode::kStoreInLiteral:
        // This operation is fairly local and simple, nothing to serialize.
        break;
    }
  }
  return result;
}

NamedAccessFeedback const*
SerializerForBackgroundCompilation::ProcessFeedbackMapsForNamedAccess(
    const MapHandles& maps, AccessMode mode, NameRef const& name) {
  ZoneVector<PropertyAccessInfo> access_infos(broker()->zone());
  for (Handle<Map> map : maps) {
    MapRef map_ref(broker(), map);
    ProcessMapForNamedPropertyAccess(map_ref, name);
    AccessInfoFactory access_info_factory(broker(), dependencies(),
                                          broker()->zone());
    PropertyAccessInfo info(access_info_factory.ComputePropertyAccessInfo(
        map, name.object(), mode));
    access_infos.push_back(info);

    // TODO(turbofan): We want to take receiver hints into account as well,
    // not only the feedback maps.
    // For JSNativeContextSpecialization::InlinePropertySetterCall
    // and InlinePropertyGetterCall.
    if (info.IsAccessorConstant() && !info.constant().is_null()) {
      if (info.constant()->IsJSFunction()) {
        // For JSCallReducer::ReduceCallApiFunction.
        Handle<SharedFunctionInfo> sfi(
            handle(Handle<JSFunction>::cast(info.constant())->shared(),
                   broker()->isolate()));
        if (sfi->IsApiFunction()) {
          FunctionTemplateInfoRef fti_ref(
              broker(), handle(sfi->get_api_func_data(), broker()->isolate()));
          if (fti_ref.has_call_code()) fti_ref.SerializeCallCode();
          ProcessReceiverMapForApiCall(fti_ref, map);
        }
      } else {
        FunctionTemplateInfoRef fti_ref(
            broker(), Handle<FunctionTemplateInfo>::cast(info.constant()));
        if (fti_ref.has_call_code()) fti_ref.SerializeCallCode();
      }
    }
  }

  DCHECK(!access_infos.empty());
  return new (broker()->zone()) NamedAccessFeedback(name, access_infos);
}

void SerializerForBackgroundCompilation::ProcessFeedbackForPropertyAccess(
    FeedbackSlot slot, AccessMode mode, base::Optional<NameRef> static_name) {
  if (slot.IsInvalid()) return;
  if (environment()->function().feedback_vector().is_null()) return;

  FeedbackNexus nexus(environment()->function().feedback_vector(), slot);
  FeedbackSource source(nexus);
  if (broker()->HasFeedback(source)) return;

  if (nexus.ic_state() == UNINITIALIZED) {
    broker()->SetFeedback(source,
                          new (broker()->zone()) InsufficientFeedback());
    return;
  }

  MapHandles maps;
  if (nexus.ExtractMaps(&maps) == 0) {  // Megamorphic.
    broker()->SetFeedback(source, nullptr);
    return;
  }

  maps = GetRelevantReceiverMaps(broker()->isolate(), maps);
  if (maps.empty()) {
    broker()->SetFeedback(source,
                          new (broker()->zone()) InsufficientFeedback());
    return;
  }

  ProcessedFeedback const* processed = nullptr;
  base::Optional<NameRef> name =
      static_name.has_value() ? static_name : broker()->GetNameFeedback(nexus);
  if (name.has_value()) {
    processed = ProcessFeedbackMapsForNamedAccess(maps, mode, *name);
  } else if (nexus.GetKeyType() == ELEMENT) {
    DCHECK_NE(nexus.ic_state(), MEGAMORPHIC);
    processed = ProcessFeedbackMapsForElementAccess(
        maps, mode, KeyedAccessMode::FromNexus(nexus));
  }
  broker()->SetFeedback(source, processed);
}

void SerializerForBackgroundCompilation::ProcessKeyedPropertyAccess(
    Hints const& receiver, Hints const& key, FeedbackSlot slot,
    AccessMode mode) {
  if (BailoutOnUninitialized(slot)) return;
  ProcessFeedbackForPropertyAccess(slot, mode, base::nullopt);

  for (Handle<Object> hint : receiver.constants()) {
    ObjectRef receiver_ref(broker(), hint);

    // For JSNativeContextSpecialization::ReduceElementAccess.
    if (receiver_ref.IsJSTypedArray()) {
      receiver_ref.AsJSTypedArray().Serialize();
    }

    // For JSNativeContextSpecialization::ReduceKeyedLoadFromHeapConstant.
    if (mode == AccessMode::kLoad || mode == AccessMode::kHas) {
      for (Handle<Object> hint : key.constants()) {
        ObjectRef key_ref(broker(), hint);
        // TODO(neis): Do this for integer-HeapNumbers too?
        if (key_ref.IsSmi() && key_ref.AsSmi() >= 0) {
          base::Optional<ObjectRef> element =
              receiver_ref.GetOwnConstantElement(key_ref.AsSmi(), true);
          if (!element.has_value() && receiver_ref.IsJSArray()) {
            // We didn't find a constant element, but if the receiver is a
            // cow-array we can exploit the fact that any future write to the
            // element will replace the whole elements storage.
            receiver_ref.AsJSArray().GetOwnCowElement(key_ref.AsSmi(), true);
          }
        }
      }
    }
  }

  environment()->accumulator_hints().Clear();
}

void SerializerForBackgroundCompilation::ProcessMapForNamedPropertyAccess(
    MapRef const& map, NameRef const& name) {
  // For JSNativeContextSpecialization::ReduceNamedAccess.
  if (map.IsMapOfCurrentGlobalProxy()) {
    broker()->native_context().global_proxy_object().GetPropertyCell(name,
                                                                     true);
  }
}

void SerializerForBackgroundCompilation::VisitLdaKeyedProperty(
    BytecodeArrayIterator* iterator) {
  Hints const& key = environment()->accumulator_hints();
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kLoad);
}

void SerializerForBackgroundCompilation::ProcessNamedPropertyAccess(
    Hints const& receiver, NameRef const& name, FeedbackSlot slot,
    AccessMode mode) {
  if (BailoutOnUninitialized(slot)) return;
  ProcessFeedbackForPropertyAccess(slot, mode, name);

  for (Handle<Map> map :
       GetRelevantReceiverMaps(broker()->isolate(), receiver.maps())) {
    ProcessMapForNamedPropertyAccess(MapRef(broker(), map), name);
  }

  JSGlobalProxyRef global_proxy =
      broker()->native_context().global_proxy_object();

  for (Handle<Object> hint : receiver.constants()) {
    ObjectRef object(broker(), hint);
    // For JSNativeContextSpecialization::ReduceNamedAccessFromNexus.
    if (object.equals(global_proxy)) {
      global_proxy.GetPropertyCell(name, true);
    }
    // For JSNativeContextSpecialization::ReduceJSLoadNamed.
    if (mode == AccessMode::kLoad && object.IsJSFunction() &&
        name.equals(ObjectRef(
            broker(), broker()->isolate()->factory()->prototype_string()))) {
      object.AsJSFunction().Serialize();
    }
  }

  environment()->accumulator_hints().Clear();
}

void SerializerForBackgroundCompilation::ProcessNamedPropertyAccess(
    BytecodeArrayIterator* iterator, AccessMode mode) {
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Handle<Name> name = Handle<Name>::cast(
      iterator->GetConstantForIndexOperand(1, broker()->isolate()));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessNamedPropertyAccess(receiver, NameRef(broker(), name), slot, mode);
}

void SerializerForBackgroundCompilation::VisitLdaNamedProperty(
    BytecodeArrayIterator* iterator) {
  ProcessNamedPropertyAccess(iterator, AccessMode::kLoad);
}

void SerializerForBackgroundCompilation::VisitStaNamedProperty(
    BytecodeArrayIterator* iterator) {
  ProcessNamedPropertyAccess(iterator, AccessMode::kStore);
}

void SerializerForBackgroundCompilation::VisitStaNamedOwnProperty(
    BytecodeArrayIterator* iterator) {
  ProcessNamedPropertyAccess(iterator, AccessMode::kStoreInLiteral);
}

void SerializerForBackgroundCompilation::VisitTestIn(
    BytecodeArrayIterator* iterator) {
  Hints const& receiver = environment()->accumulator_hints();
  Hints const& key =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  FeedbackSlot slot = iterator->GetSlotOperand(1);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kHas);
}

void SerializerForBackgroundCompilation::VisitStaKeyedProperty(
    BytecodeArrayIterator* iterator) {
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Hints const& key =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStore);
}

void SerializerForBackgroundCompilation::VisitStaInArrayLiteral(
    BytecodeArrayIterator* iterator) {
  Hints const& receiver =
      environment()->register_hints(iterator->GetRegisterOperand(0));
  Hints const& key =
      environment()->register_hints(iterator->GetRegisterOperand(1));
  FeedbackSlot slot = iterator->GetSlotOperand(2);
  ProcessKeyedPropertyAccess(receiver, key, slot, AccessMode::kStoreInLiteral);
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

#undef CLEAR_ENVIRONMENT_LIST
#undef KILL_ENVIRONMENT_LIST
#undef CLEAR_ACCUMULATOR_LIST
#undef UNCONDITIONAL_JUMPS_LIST
#undef CONDITIONAL_JUMPS_LIST
#undef IGNORED_BYTECODE_LIST
#undef UNREACHABLE_BYTECODE_LIST
#undef SUPPORTED_BYTECODE_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
