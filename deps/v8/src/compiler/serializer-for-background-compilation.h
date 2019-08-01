// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_
#define V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_

#include "src/base/optional.h"
#include "src/compiler/access-info.h"
#include "src/utils/utils.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

namespace interpreter {
class BytecodeArrayIterator;
}  // namespace interpreter

class BytecodeArray;
class FeedbackVector;
class LookupIterator;
class NativeContext;
class ScriptContextTable;
class SharedFunctionInfo;
class SourcePositionTableIterator;
class Zone;

namespace compiler {

#define CLEAR_ENVIRONMENT_LIST(V) \
  V(Abort)                        \
  V(CallRuntime)                  \
  V(CallRuntimeForPair)           \
  V(CreateBlockContext)           \
  V(CreateEvalContext)            \
  V(CreateFunctionContext)        \
  V(Debugger)                     \
  V(PopContext)                   \
  V(PushContext)                  \
  V(ResumeGenerator)              \
  V(ReThrow)                      \
  V(StaContextSlot)               \
  V(StaCurrentContextSlot)        \
  V(SuspendGenerator)             \
  V(SwitchOnGeneratorState)       \
  V(Throw)

#define CLEAR_ACCUMULATOR_LIST(V)   \
  V(Add)                            \
  V(AddSmi)                         \
  V(BitwiseAnd)                     \
  V(BitwiseAndSmi)                  \
  V(BitwiseNot)                     \
  V(BitwiseOr)                      \
  V(BitwiseOrSmi)                   \
  V(BitwiseXor)                     \
  V(BitwiseXorSmi)                  \
  V(CloneObject)                    \
  V(CreateArrayFromIterable)        \
  V(CreateArrayLiteral)             \
  V(CreateEmptyArrayLiteral)        \
  V(CreateEmptyObjectLiteral)       \
  V(CreateMappedArguments)          \
  V(CreateObjectLiteral)            \
  V(CreateRestParameter)            \
  V(CreateUnmappedArguments)        \
  V(Dec)                            \
  V(DeletePropertySloppy)           \
  V(DeletePropertyStrict)           \
  V(Div)                            \
  V(DivSmi)                         \
  V(Exp)                            \
  V(ExpSmi)                         \
  V(ForInContinue)                  \
  V(ForInEnumerate)                 \
  V(ForInNext)                      \
  V(ForInStep)                      \
  V(GetTemplateObject)              \
  V(Inc)                            \
  V(LdaContextSlot)                 \
  V(LdaCurrentContextSlot)          \
  V(LdaImmutableContextSlot)        \
  V(LdaImmutableCurrentContextSlot) \
  V(LogicalNot)                     \
  V(Mod)                            \
  V(ModSmi)                         \
  V(Mul)                            \
  V(MulSmi)                         \
  V(Negate)                         \
  V(SetPendingMessage)              \
  V(ShiftLeft)                      \
  V(ShiftLeftSmi)                   \
  V(ShiftRight)                     \
  V(ShiftRightLogical)              \
  V(ShiftRightLogicalSmi)           \
  V(ShiftRightSmi)                  \
  V(Sub)                            \
  V(SubSmi)                         \
  V(TestEqual)                      \
  V(TestEqualStrict)                \
  V(TestGreaterThan)                \
  V(TestGreaterThanOrEqual)         \
  V(TestInstanceOf)                 \
  V(TestLessThan)                   \
  V(TestLessThanOrEqual)            \
  V(TestNull)                       \
  V(TestReferenceEqual)             \
  V(TestTypeOf)                     \
  V(TestUndefined)                  \
  V(TestUndetectable)               \
  V(ToBooleanLogicalNot)            \
  V(ToName)                         \
  V(ToNumber)                       \
  V(ToNumeric)                      \
  V(ToString)                       \
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
  V(LdaNamedPropertyNoFeedback)       \
  V(StackCheck)                       \
  V(StaNamedPropertyNoFeedback)       \
  V(ThrowReferenceErrorIfHole)        \
  V(ThrowSuperAlreadyCalledIfNotHole) \
  V(ThrowSuperNotCalledIfHole)

#define SUPPORTED_BYTECODE_LIST(V)   \
  V(CallAnyReceiver)                 \
  V(CallProperty)                    \
  V(CallProperty0)                   \
  V(CallProperty1)                   \
  V(CallProperty2)                   \
  V(CallUndefinedReceiver)           \
  V(CallUndefinedReceiver0)          \
  V(CallUndefinedReceiver1)          \
  V(CallUndefinedReceiver2)          \
  V(CallWithSpread)                  \
  V(Construct)                       \
  V(ConstructWithSpread)             \
  V(CreateClosure)                   \
  V(ExtraWide)                       \
  V(GetSuperConstructor)             \
  V(Illegal)                         \
  V(LdaConstant)                     \
  V(LdaFalse)                        \
  V(LdaGlobal)                       \
  V(LdaGlobalInsideTypeof)           \
  V(LdaKeyedProperty)                \
  V(LdaLookupGlobalSlot)             \
  V(LdaLookupGlobalSlotInsideTypeof) \
  V(LdaNamedProperty)                \
  V(LdaNull)                         \
  V(Ldar)                            \
  V(LdaSmi)                          \
  V(LdaTheHole)                      \
  V(LdaTrue)                         \
  V(LdaUndefined)                    \
  V(LdaZero)                         \
  V(Mov)                             \
  V(Return)                          \
  V(StaGlobal)                       \
  V(StaInArrayLiteral)               \
  V(StaKeyedProperty)                \
  V(StaNamedOwnProperty)             \
  V(StaNamedProperty)                \
  V(Star)                            \
  V(TestIn)                          \
  V(Wide)                            \
  CLEAR_ENVIRONMENT_LIST(V)          \
  CLEAR_ACCUMULATOR_LIST(V)          \
  CONDITIONAL_JUMPS_LIST(V)          \
  UNCONDITIONAL_JUMPS_LIST(V)        \
  IGNORED_BYTECODE_LIST(V)

class JSHeapBroker;

template <typename T>
struct HandleComparator {
  bool operator()(const Handle<T>& lhs, const Handle<T>& rhs) const {
    return lhs.address() < rhs.address();
  }
};

struct FunctionBlueprint {
  Handle<SharedFunctionInfo> shared;
  Handle<FeedbackVector> feedback_vector;

  bool operator<(const FunctionBlueprint& other) const {
    // A feedback vector is never used for more than one SFI, so it can
    // be used for strict ordering of blueprints.
    DCHECK_IMPLIES(feedback_vector.equals(other.feedback_vector),
                   shared.equals(other.shared));
    return HandleComparator<FeedbackVector>()(feedback_vector,
                                              other.feedback_vector);
  }
};

class CompilationSubject {
 public:
  explicit CompilationSubject(FunctionBlueprint blueprint)
      : blueprint_(blueprint) {}
  CompilationSubject(Handle<JSFunction> closure, Isolate* isolate);

  FunctionBlueprint blueprint() const { return blueprint_; }
  MaybeHandle<JSFunction> closure() const { return closure_; }

 private:
  FunctionBlueprint blueprint_;
  MaybeHandle<JSFunction> closure_;
};

using ConstantsSet = ZoneSet<Handle<Object>, HandleComparator<Object>>;
using MapsSet = ZoneSet<Handle<Map>, HandleComparator<Map>>;
using BlueprintsSet = ZoneSet<FunctionBlueprint>;

class Hints {
 public:
  explicit Hints(Zone* zone);

  const ConstantsSet& constants() const;
  const MapsSet& maps() const;
  const BlueprintsSet& function_blueprints() const;

  void AddConstant(Handle<Object> constant);
  void AddMap(Handle<Map> map);
  void AddFunctionBlueprint(FunctionBlueprint function_blueprint);

  void Add(const Hints& other);

  void Clear();
  bool IsEmpty() const;

 private:
  ConstantsSet constants_;
  MapsSet maps_;
  BlueprintsSet function_blueprints_;
};
using HintsVector = ZoneVector<Hints>;

enum class SerializerForBackgroundCompilationFlag : uint8_t {
  kBailoutOnUninitialized = 1 << 0,
  kCollectSourcePositions = 1 << 1,
  kOsr = 1 << 2,
};
using SerializerForBackgroundCompilationFlags =
    base::Flags<SerializerForBackgroundCompilationFlag>;

// The SerializerForBackgroundCompilation makes sure that the relevant function
// data such as bytecode, SharedFunctionInfo and FeedbackVector, used by later
// optimizations in the compiler, is copied to the heap broker.
class SerializerForBackgroundCompilation {
 public:
  SerializerForBackgroundCompilation(
      JSHeapBroker* broker, CompilationDependencies* dependencies, Zone* zone,
      Handle<JSFunction> closure,
      SerializerForBackgroundCompilationFlags flags);
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

  void ProcessJump(interpreter::BytecodeArrayIterator* iterator);
  void MergeAfterJump(interpreter::BytecodeArrayIterator* iterator);

  void ProcessKeyedPropertyAccess(Hints const& receiver, Hints const& key,
                                  FeedbackSlot slot, AccessMode mode);
  void ProcessNamedPropertyAccess(interpreter::BytecodeArrayIterator* iterator,
                                  AccessMode mode);
  void ProcessNamedPropertyAccess(Hints const& receiver, NameRef const& name,
                                  FeedbackSlot slot, AccessMode mode);

  GlobalAccessFeedback const* ProcessFeedbackForGlobalAccess(FeedbackSlot slot);
  NamedAccessFeedback const* ProcessFeedbackMapsForNamedAccess(
      const MapHandles& maps, AccessMode mode, NameRef const& name);
  ElementAccessFeedback const* ProcessFeedbackMapsForElementAccess(
      const MapHandles& maps, AccessMode mode);
  void ProcessFeedbackForPropertyAccess(FeedbackSlot slot, AccessMode mode,
                                        base::Optional<NameRef> static_name);
  void ProcessMapForNamedPropertyAccess(MapRef const& map, NameRef const& name);

  Hints RunChildSerializer(CompilationSubject function,
                           base::Optional<Hints> new_target,
                           const HintsVector& arguments, bool with_spread);

  JSHeapBroker* broker() const { return broker_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Zone* zone() const { return zone_; }
  Environment* environment() const { return environment_; }
  SerializerForBackgroundCompilationFlags flags() const { return flags_; }

  JSHeapBroker* const broker_;
  CompilationDependencies* const dependencies_;
  Zone* const zone_;
  Environment* const environment_;
  ZoneUnorderedMap<int, Environment*> stashed_environments_;
  SerializerForBackgroundCompilationFlags const flags_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_
