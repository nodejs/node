// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-operator.h"

#include <limits>

#include "src/base/lazy-instance.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/feedback-vector.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

std::ostream& operator<<(std::ostream& os, CallFrequency f) {
  if (f.IsUnknown()) return os << "unknown";
  return os << f.value();
}

VectorSlotPair::VectorSlotPair() {}


int VectorSlotPair::index() const {
  return vector_.is_null() ? -1 : FeedbackVector::GetIndex(slot_);
}


bool operator==(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return lhs.slot() == rhs.slot() &&
         lhs.vector().location() == rhs.vector().location();
}


bool operator!=(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(VectorSlotPair const& p) {
  return base::hash_combine(p.slot(), p.vector().location());
}


ConvertReceiverMode ConvertReceiverModeOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSConvertReceiver, op->opcode());
  return OpParameter<ConvertReceiverMode>(op);
}


ToBooleanHints ToBooleanHintsOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSToBoolean, op->opcode());
  return OpParameter<ToBooleanHints>(op);
}

std::ostream& operator<<(std::ostream& os,
                         ConstructForwardVarargsParameters const& p) {
  return os << p.arity() << ", " << p.start_index();
}

ConstructForwardVarargsParameters const& ConstructForwardVarargsParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSConstructForwardVarargs, op->opcode());
  return OpParameter<ConstructForwardVarargsParameters>(op);
}

bool operator==(ConstructParameters const& lhs,
                ConstructParameters const& rhs) {
  return lhs.arity() == rhs.arity() && lhs.frequency() == rhs.frequency() &&
         lhs.feedback() == rhs.feedback();
}

bool operator!=(ConstructParameters const& lhs,
                ConstructParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(ConstructParameters const& p) {
  return base::hash_combine(p.arity(), p.frequency(), p.feedback());
}

std::ostream& operator<<(std::ostream& os, ConstructParameters const& p) {
  return os << p.arity() << ", " << p.frequency();
}

ConstructParameters const& ConstructParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSConstruct, op->opcode());
  return OpParameter<ConstructParameters>(op);
}

bool operator==(SpreadWithArityParameter const& lhs,
                SpreadWithArityParameter const& rhs) {
  return lhs.arity() == rhs.arity();
}

bool operator!=(SpreadWithArityParameter const& lhs,
                SpreadWithArityParameter const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(SpreadWithArityParameter const& p) {
  return base::hash_combine(p.arity());
}

std::ostream& operator<<(std::ostream& os, SpreadWithArityParameter const& p) {
  return os << p.arity();
}

SpreadWithArityParameter const& SpreadWithArityParameterOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kJSConstructWithSpread ||
         op->opcode() == IrOpcode::kJSCallWithSpread);
  return OpParameter<SpreadWithArityParameter>(op);
}

std::ostream& operator<<(std::ostream& os, CallParameters const& p) {
  os << p.arity() << ", " << p.frequency() << ", " << p.convert_mode() << ", "
     << p.tail_call_mode();
  return os;
}

const CallParameters& CallParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCall, op->opcode());
  return OpParameter<CallParameters>(op);
}

std::ostream& operator<<(std::ostream& os,
                         CallForwardVarargsParameters const& p) {
  return os << p.arity() << ", " << p.start_index() << ", "
            << p.tail_call_mode();
}

CallForwardVarargsParameters const& CallForwardVarargsParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSCallForwardVarargs, op->opcode());
  return OpParameter<CallForwardVarargsParameters>(op);
}


bool operator==(CallRuntimeParameters const& lhs,
                CallRuntimeParameters const& rhs) {
  return lhs.id() == rhs.id() && lhs.arity() == rhs.arity();
}


bool operator!=(CallRuntimeParameters const& lhs,
                CallRuntimeParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CallRuntimeParameters const& p) {
  return base::hash_combine(p.id(), p.arity());
}


std::ostream& operator<<(std::ostream& os, CallRuntimeParameters const& p) {
  return os << p.id() << ", " << p.arity();
}


const CallRuntimeParameters& CallRuntimeParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCallRuntime, op->opcode());
  return OpParameter<CallRuntimeParameters>(op);
}


ContextAccess::ContextAccess(size_t depth, size_t index, bool immutable)
    : immutable_(immutable),
      depth_(static_cast<uint16_t>(depth)),
      index_(static_cast<uint32_t>(index)) {
  DCHECK(depth <= std::numeric_limits<uint16_t>::max());
  DCHECK(index <= std::numeric_limits<uint32_t>::max());
}


bool operator==(ContextAccess const& lhs, ContextAccess const& rhs) {
  return lhs.depth() == rhs.depth() && lhs.index() == rhs.index() &&
         lhs.immutable() == rhs.immutable();
}


bool operator!=(ContextAccess const& lhs, ContextAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(ContextAccess const& access) {
  return base::hash_combine(access.depth(), access.index(), access.immutable());
}


std::ostream& operator<<(std::ostream& os, ContextAccess const& access) {
  return os << access.depth() << ", " << access.index() << ", "
            << access.immutable();
}


ContextAccess const& ContextAccessOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadContext ||
         op->opcode() == IrOpcode::kJSStoreContext);
  return OpParameter<ContextAccess>(op);
}

CreateCatchContextParameters::CreateCatchContextParameters(
    Handle<String> catch_name, Handle<ScopeInfo> scope_info)
    : catch_name_(catch_name), scope_info_(scope_info) {}

bool operator==(CreateCatchContextParameters const& lhs,
                CreateCatchContextParameters const& rhs) {
  return lhs.catch_name().location() == rhs.catch_name().location() &&
         lhs.scope_info().location() == rhs.scope_info().location();
}

bool operator!=(CreateCatchContextParameters const& lhs,
                CreateCatchContextParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(CreateCatchContextParameters const& parameters) {
  return base::hash_combine(parameters.catch_name().location(),
                            parameters.scope_info().location());
}

std::ostream& operator<<(std::ostream& os,
                         CreateCatchContextParameters const& parameters) {
  return os << Brief(*parameters.catch_name()) << ", "
            << Brief(*parameters.scope_info());
}

CreateCatchContextParameters const& CreateCatchContextParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSCreateCatchContext, op->opcode());
  return OpParameter<CreateCatchContextParameters>(op);
}

CreateFunctionContextParameters::CreateFunctionContextParameters(
    int slot_count, ScopeType scope_type)
    : slot_count_(slot_count), scope_type_(scope_type) {}

bool operator==(CreateFunctionContextParameters const& lhs,
                CreateFunctionContextParameters const& rhs) {
  return lhs.slot_count() == rhs.slot_count() &&
         lhs.scope_type() == rhs.scope_type();
}

bool operator!=(CreateFunctionContextParameters const& lhs,
                CreateFunctionContextParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(CreateFunctionContextParameters const& parameters) {
  return base::hash_combine(parameters.slot_count(),
                            static_cast<int>(parameters.scope_type()));
}

std::ostream& operator<<(std::ostream& os,
                         CreateFunctionContextParameters const& parameters) {
  return os << parameters.slot_count() << ", " << parameters.scope_type();
}

CreateFunctionContextParameters const& CreateFunctionContextParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSCreateFunctionContext, op->opcode());
  return OpParameter<CreateFunctionContextParameters>(op);
}

bool operator==(StoreNamedOwnParameters const& lhs,
                StoreNamedOwnParameters const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback();
}

bool operator!=(StoreNamedOwnParameters const& lhs,
                StoreNamedOwnParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(StoreNamedOwnParameters const& p) {
  return base::hash_combine(p.name().location(), p.feedback());
}

std::ostream& operator<<(std::ostream& os, StoreNamedOwnParameters const& p) {
  return os << Brief(*p.name());
}

StoreNamedOwnParameters const& StoreNamedOwnParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSStoreNamedOwn, op->opcode());
  return OpParameter<StoreNamedOwnParameters>(op);
}

bool operator==(FeedbackParameter const& lhs, FeedbackParameter const& rhs) {
  return lhs.feedback() == rhs.feedback();
}

bool operator!=(FeedbackParameter const& lhs, FeedbackParameter const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(FeedbackParameter const& p) {
  return base::hash_combine(p.feedback());
}

std::ostream& operator<<(std::ostream& os, FeedbackParameter const& p) {
  return os;
}

FeedbackParameter const& FeedbackParameterOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSStoreDataPropertyInLiteral);
  return OpParameter<FeedbackParameter>(op);
}

bool operator==(NamedAccess const& lhs, NamedAccess const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.language_mode() == rhs.language_mode() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(NamedAccess const& lhs, NamedAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(NamedAccess const& p) {
  return base::hash_combine(p.name().location(), p.language_mode(),
                            p.feedback());
}


std::ostream& operator<<(std::ostream& os, NamedAccess const& p) {
  return os << Brief(*p.name()) << ", " << p.language_mode();
}


NamedAccess const& NamedAccessOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadNamed ||
         op->opcode() == IrOpcode::kJSStoreNamed);
  return OpParameter<NamedAccess>(op);
}


std::ostream& operator<<(std::ostream& os, PropertyAccess const& p) {
  return os << p.language_mode();
}


bool operator==(PropertyAccess const& lhs, PropertyAccess const& rhs) {
  return lhs.language_mode() == rhs.language_mode() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(PropertyAccess const& lhs, PropertyAccess const& rhs) {
  return !(lhs == rhs);
}


PropertyAccess const& PropertyAccessOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadProperty ||
         op->opcode() == IrOpcode::kJSStoreProperty);
  return OpParameter<PropertyAccess>(op);
}


size_t hash_value(PropertyAccess const& p) {
  return base::hash_combine(p.language_mode(), p.feedback());
}


bool operator==(LoadGlobalParameters const& lhs,
                LoadGlobalParameters const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback() &&
         lhs.typeof_mode() == rhs.typeof_mode();
}


bool operator!=(LoadGlobalParameters const& lhs,
                LoadGlobalParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(LoadGlobalParameters const& p) {
  return base::hash_combine(p.name().location(), p.typeof_mode());
}


std::ostream& operator<<(std::ostream& os, LoadGlobalParameters const& p) {
  return os << Brief(*p.name()) << ", " << p.typeof_mode();
}


const LoadGlobalParameters& LoadGlobalParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSLoadGlobal, op->opcode());
  return OpParameter<LoadGlobalParameters>(op);
}


bool operator==(StoreGlobalParameters const& lhs,
                StoreGlobalParameters const& rhs) {
  return lhs.language_mode() == rhs.language_mode() &&
         lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(StoreGlobalParameters const& lhs,
                StoreGlobalParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(StoreGlobalParameters const& p) {
  return base::hash_combine(p.language_mode(), p.name().location(),
                            p.feedback());
}


std::ostream& operator<<(std::ostream& os, StoreGlobalParameters const& p) {
  return os << p.language_mode() << ", " << Brief(*p.name());
}


const StoreGlobalParameters& StoreGlobalParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSStoreGlobal, op->opcode());
  return OpParameter<StoreGlobalParameters>(op);
}


CreateArgumentsType const& CreateArgumentsTypeOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateArguments, op->opcode());
  return OpParameter<CreateArgumentsType>(op);
}


bool operator==(CreateArrayParameters const& lhs,
                CreateArrayParameters const& rhs) {
  return lhs.arity() == rhs.arity() &&
         lhs.site().location() == rhs.site().location();
}


bool operator!=(CreateArrayParameters const& lhs,
                CreateArrayParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateArrayParameters const& p) {
  return base::hash_combine(p.arity(), p.site().location());
}


std::ostream& operator<<(std::ostream& os, CreateArrayParameters const& p) {
  os << p.arity();
  if (!p.site().is_null()) os << ", " << Brief(*p.site());
  return os;
}


const CreateArrayParameters& CreateArrayParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, op->opcode());
  return OpParameter<CreateArrayParameters>(op);
}


bool operator==(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return lhs.pretenure() == rhs.pretenure() &&
         lhs.feedback() == rhs.feedback() &&
         lhs.shared_info().location() == rhs.shared_info().location();
}


bool operator!=(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateClosureParameters const& p) {
  return base::hash_combine(p.pretenure(), p.shared_info().location(),
                            p.feedback());
}


std::ostream& operator<<(std::ostream& os, CreateClosureParameters const& p) {
  return os << p.pretenure() << ", " << Brief(*p.shared_info());
}


const CreateClosureParameters& CreateClosureParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateClosure, op->opcode());
  return OpParameter<CreateClosureParameters>(op);
}


bool operator==(CreateLiteralParameters const& lhs,
                CreateLiteralParameters const& rhs) {
  return lhs.constant().location() == rhs.constant().location() &&
         lhs.length() == rhs.length() && lhs.flags() == rhs.flags() &&
         lhs.index() == rhs.index();
}


bool operator!=(CreateLiteralParameters const& lhs,
                CreateLiteralParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateLiteralParameters const& p) {
  return base::hash_combine(p.constant().location(), p.length(), p.flags(),
                            p.index());
}


std::ostream& operator<<(std::ostream& os, CreateLiteralParameters const& p) {
  return os << Brief(*p.constant()) << ", " << p.length() << ", " << p.flags()
            << ", " << p.index();
}


const CreateLiteralParameters& CreateLiteralParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSCreateLiteralArray ||
         op->opcode() == IrOpcode::kJSCreateLiteralObject ||
         op->opcode() == IrOpcode::kJSCreateLiteralRegExp);
  return OpParameter<CreateLiteralParameters>(op);
}

bool operator==(GeneratorStoreParameters const& lhs,
                GeneratorStoreParameters const& rhs) {
  return lhs.register_count() == rhs.register_count() &&
         lhs.suspend_type() == rhs.suspend_type();
}
bool operator!=(GeneratorStoreParameters const& lhs,
                GeneratorStoreParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(GeneratorStoreParameters const& p) {
  return base::hash_combine(p.register_count(),
                            static_cast<int>(p.suspend_type()));
}

std::ostream& operator<<(std::ostream& os, GeneratorStoreParameters const& p) {
  const char* suspend_type = SuspendTypeFor(p.suspend_type());
  return os << p.register_count() << " (" << suspend_type << ")";
}

const GeneratorStoreParameters& GeneratorStoreParametersOf(const Operator* op) {
  DCHECK_EQ(op->opcode(), IrOpcode::kJSGeneratorStore);
  return OpParameter<GeneratorStoreParameters>(op);
}

BinaryOperationHint BinaryOperationHintOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSAdd, op->opcode());
  return OpParameter<BinaryOperationHint>(op);
}

CompareOperationHint CompareOperationHintOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSEqual ||
         op->opcode() == IrOpcode::kJSStrictEqual ||
         op->opcode() == IrOpcode::kJSLessThan ||
         op->opcode() == IrOpcode::kJSGreaterThan ||
         op->opcode() == IrOpcode::kJSLessThanOrEqual ||
         op->opcode() == IrOpcode::kJSGreaterThanOrEqual);
  return OpParameter<CompareOperationHint>(op);
}

#define CACHED_OP_LIST(V)                                       \
  V(BitwiseOr, Operator::kNoProperties, 2, 1)                   \
  V(BitwiseXor, Operator::kNoProperties, 2, 1)                  \
  V(BitwiseAnd, Operator::kNoProperties, 2, 1)                  \
  V(ShiftLeft, Operator::kNoProperties, 2, 1)                   \
  V(ShiftRight, Operator::kNoProperties, 2, 1)                  \
  V(ShiftRightLogical, Operator::kNoProperties, 2, 1)           \
  V(Subtract, Operator::kNoProperties, 2, 1)                    \
  V(Multiply, Operator::kNoProperties, 2, 1)                    \
  V(Divide, Operator::kNoProperties, 2, 1)                      \
  V(Modulus, Operator::kNoProperties, 2, 1)                     \
  V(ToInteger, Operator::kNoProperties, 1, 1)                   \
  V(ToLength, Operator::kNoProperties, 1, 1)                    \
  V(ToName, Operator::kNoProperties, 1, 1)                      \
  V(ToNumber, Operator::kNoProperties, 1, 1)                    \
  V(ToObject, Operator::kFoldable, 1, 1)                        \
  V(ToString, Operator::kNoProperties, 1, 1)                    \
  V(Create, Operator::kNoProperties, 2, 1)                      \
  V(CreateIterResultObject, Operator::kEliminatable, 2, 1)      \
  V(CreateKeyValueArray, Operator::kEliminatable, 2, 1)         \
  V(HasProperty, Operator::kNoProperties, 2, 1)                 \
  V(ClassOf, Operator::kPure, 1, 1)                             \
  V(TypeOf, Operator::kPure, 1, 1)                              \
  V(InstanceOf, Operator::kNoProperties, 2, 1)                  \
  V(OrdinaryHasInstance, Operator::kNoProperties, 2, 1)         \
  V(ForInNext, Operator::kNoProperties, 4, 1)                   \
  V(ForInPrepare, Operator::kNoProperties, 1, 3)                \
  V(LoadMessage, Operator::kNoThrow | Operator::kNoWrite, 0, 1) \
  V(StoreMessage, Operator::kNoRead | Operator::kNoThrow, 1, 0) \
  V(GeneratorRestoreContinuation, Operator::kNoThrow, 1, 1)     \
  V(StackCheck, Operator::kNoWrite, 0, 0)                       \
  V(Debugger, Operator::kNoProperties, 0, 0)                    \
  V(GetSuperConstructor, Operator::kNoWrite, 1, 1)

#define BINARY_OP_LIST(V) V(Add)

#define COMPARE_OP_LIST(V)                    \
  V(Equal, Operator::kNoProperties)           \
  V(StrictEqual, Operator::kPure)             \
  V(LessThan, Operator::kNoProperties)        \
  V(GreaterThan, Operator::kNoProperties)     \
  V(LessThanOrEqual, Operator::kNoProperties) \
  V(GreaterThanOrEqual, Operator::kNoProperties)

struct JSOperatorGlobalCache final {
#define CACHED_OP(Name, properties, value_input_count, value_output_count) \
  struct Name##Operator final : public Operator {                          \
    Name##Operator()                                                       \
        : Operator(IrOpcode::kJS##Name, properties, "JS" #Name,            \
                   value_input_count, Operator::ZeroIfPure(properties),    \
                   Operator::ZeroIfEliminatable(properties),               \
                   value_output_count, Operator::ZeroIfPure(properties),   \
                   Operator::ZeroIfNoThrow(properties)) {}                 \
  };                                                                       \
  Name##Operator k##Name##Operator;
  CACHED_OP_LIST(CACHED_OP)
#undef CACHED_OP

#define BINARY_OP(Name)                                                       \
  template <BinaryOperationHint kHint>                                        \
  struct Name##Operator final : public Operator1<BinaryOperationHint> {       \
    Name##Operator()                                                          \
        : Operator1<BinaryOperationHint>(IrOpcode::kJS##Name,                 \
                                         Operator::kNoProperties, "JS" #Name, \
                                         2, 1, 1, 1, 1, 2, kHint) {}          \
  };                                                                          \
  Name##Operator<BinaryOperationHint::kNone> k##Name##NoneOperator;           \
  Name##Operator<BinaryOperationHint::kSignedSmall>                           \
      k##Name##SignedSmallOperator;                                           \
  Name##Operator<BinaryOperationHint::kSigned32> k##Name##Signed32Operator;   \
  Name##Operator<BinaryOperationHint::kNumberOrOddball>                       \
      k##Name##NumberOrOddballOperator;                                       \
  Name##Operator<BinaryOperationHint::kString> k##Name##StringOperator;       \
  Name##Operator<BinaryOperationHint::kAny> k##Name##AnyOperator;
  BINARY_OP_LIST(BINARY_OP)
#undef BINARY_OP

#define COMPARE_OP(Name, properties)                                         \
  template <CompareOperationHint kHint>                                      \
  struct Name##Operator final : public Operator1<CompareOperationHint> {     \
    Name##Operator()                                                         \
        : Operator1<CompareOperationHint>(                                   \
              IrOpcode::kJS##Name, properties, "JS" #Name, 2, 1, 1, 1, 1,    \
              Operator::ZeroIfNoThrow(properties), kHint) {}                 \
  };                                                                         \
  Name##Operator<CompareOperationHint::kNone> k##Name##NoneOperator;         \
  Name##Operator<CompareOperationHint::kSignedSmall>                         \
      k##Name##SignedSmallOperator;                                          \
  Name##Operator<CompareOperationHint::kNumber> k##Name##NumberOperator;     \
  Name##Operator<CompareOperationHint::kNumberOrOddball>                     \
      k##Name##NumberOrOddballOperator;                                      \
  Name##Operator<CompareOperationHint::kInternalizedString>                  \
      k##Name##InternalizedStringOperator;                                   \
  Name##Operator<CompareOperationHint::kString> k##Name##StringOperator;     \
  Name##Operator<CompareOperationHint::kReceiver> k##Name##ReceiverOperator; \
  Name##Operator<CompareOperationHint::kAny> k##Name##AnyOperator;
  COMPARE_OP_LIST(COMPARE_OP)
#undef COMPARE_OP
};

static base::LazyInstance<JSOperatorGlobalCache>::type kCache =
    LAZY_INSTANCE_INITIALIZER;

JSOperatorBuilder::JSOperatorBuilder(Zone* zone)
    : cache_(kCache.Get()), zone_(zone) {}

#define CACHED_OP(Name, properties, value_input_count, value_output_count) \
  const Operator* JSOperatorBuilder::Name() {                              \
    return &cache_.k##Name##Operator;                                      \
  }
CACHED_OP_LIST(CACHED_OP)
#undef CACHED_OP

#define BINARY_OP(Name)                                               \
  const Operator* JSOperatorBuilder::Name(BinaryOperationHint hint) { \
    switch (hint) {                                                   \
      case BinaryOperationHint::kNone:                                \
        return &cache_.k##Name##NoneOperator;                         \
      case BinaryOperationHint::kSignedSmall:                         \
        return &cache_.k##Name##SignedSmallOperator;                  \
      case BinaryOperationHint::kSigned32:                            \
        return &cache_.k##Name##Signed32Operator;                     \
      case BinaryOperationHint::kNumberOrOddball:                     \
        return &cache_.k##Name##NumberOrOddballOperator;              \
      case BinaryOperationHint::kString:                              \
        return &cache_.k##Name##StringOperator;                       \
      case BinaryOperationHint::kAny:                                 \
        return &cache_.k##Name##AnyOperator;                          \
    }                                                                 \
    UNREACHABLE();                                                    \
    return nullptr;                                                   \
  }
BINARY_OP_LIST(BINARY_OP)
#undef BINARY_OP

#define COMPARE_OP(Name, ...)                                          \
  const Operator* JSOperatorBuilder::Name(CompareOperationHint hint) { \
    switch (hint) {                                                    \
      case CompareOperationHint::kNone:                                \
        return &cache_.k##Name##NoneOperator;                          \
      case CompareOperationHint::kSignedSmall:                         \
        return &cache_.k##Name##SignedSmallOperator;                   \
      case CompareOperationHint::kNumber:                              \
        return &cache_.k##Name##NumberOperator;                        \
      case CompareOperationHint::kNumberOrOddball:                     \
        return &cache_.k##Name##NumberOrOddballOperator;               \
      case CompareOperationHint::kInternalizedString:                  \
        return &cache_.k##Name##InternalizedStringOperator;            \
      case CompareOperationHint::kString:                              \
        return &cache_.k##Name##StringOperator;                        \
      case CompareOperationHint::kReceiver:                            \
        return &cache_.k##Name##ReceiverOperator;                      \
      case CompareOperationHint::kAny:                                 \
        return &cache_.k##Name##AnyOperator;                           \
    }                                                                  \
    UNREACHABLE();                                                     \
    return nullptr;                                                    \
  }
COMPARE_OP_LIST(COMPARE_OP)
#undef COMPARE_OP

const Operator* JSOperatorBuilder::StoreDataPropertyInLiteral(
    const VectorSlotPair& feedback) {
  FeedbackParameter parameters(feedback);
  return new (zone()) Operator1<FeedbackParameter>(  // --
      IrOpcode::kJSStoreDataPropertyInLiteral,
      Operator::kNoThrow,              // opcode
      "JSStoreDataPropertyInLiteral",  // name
      4, 1, 1, 0, 1, 0,                // counts
      parameters);                     // parameter
}

const Operator* JSOperatorBuilder::ToBoolean(ToBooleanHints hints) {
  // TODO(turbofan): Cache most important versions of this operator.
  return new (zone()) Operator1<ToBooleanHints>(  //--
      IrOpcode::kJSToBoolean, Operator::kPure,    // opcode
      "JSToBoolean",                              // name
      1, 0, 0, 1, 0, 0,                           // inputs/outputs
      hints);                                     // parameter
}

const Operator* JSOperatorBuilder::CallForwardVarargs(
    size_t arity, uint32_t start_index, TailCallMode tail_call_mode) {
  CallForwardVarargsParameters parameters(arity, start_index, tail_call_mode);
  return new (zone()) Operator1<CallForwardVarargsParameters>(   // --
      IrOpcode::kJSCallForwardVarargs, Operator::kNoProperties,  // opcode
      "JSCallForwardVarargs",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                         // counts
      parameters);                                               // parameter
}

const Operator* JSOperatorBuilder::Call(size_t arity, CallFrequency frequency,
                                        VectorSlotPair const& feedback,
                                        ConvertReceiverMode convert_mode,
                                        TailCallMode tail_call_mode) {
  CallParameters parameters(arity, frequency, feedback, tail_call_mode,
                            convert_mode);
  return new (zone()) Operator1<CallParameters>(   // --
      IrOpcode::kJSCall, Operator::kNoProperties,  // opcode
      "JSCall",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,           // inputs/outputs
      parameters);                                 // parameter
}

const Operator* JSOperatorBuilder::CallWithSpread(uint32_t arity) {
  SpreadWithArityParameter parameters(arity);
  return new (zone()) Operator1<SpreadWithArityParameter>(   // --
      IrOpcode::kJSCallWithSpread, Operator::kNoProperties,  // opcode
      "JSCallWithSpread",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                     // counts
      parameters);                                           // parameter
}

const Operator* JSOperatorBuilder::CallRuntime(Runtime::FunctionId id) {
  const Runtime::Function* f = Runtime::FunctionForId(id);
  return CallRuntime(f, f->nargs);
}


const Operator* JSOperatorBuilder::CallRuntime(Runtime::FunctionId id,
                                               size_t arity) {
  const Runtime::Function* f = Runtime::FunctionForId(id);
  return CallRuntime(f, arity);
}


const Operator* JSOperatorBuilder::CallRuntime(const Runtime::Function* f,
                                               size_t arity) {
  CallRuntimeParameters parameters(f->function_id, arity);
  DCHECK(f->nargs == -1 || f->nargs == static_cast<int>(parameters.arity()));
  return new (zone()) Operator1<CallRuntimeParameters>(   // --
      IrOpcode::kJSCallRuntime, Operator::kNoProperties,  // opcode
      "JSCallRuntime",                                    // name
      parameters.arity(), 1, 1, f->result_size, 1, 2,     // inputs/outputs
      parameters);                                        // parameter
}

const Operator* JSOperatorBuilder::ConstructForwardVarargs(
    size_t arity, uint32_t start_index) {
  ConstructForwardVarargsParameters parameters(arity, start_index);
  return new (zone()) Operator1<ConstructForwardVarargsParameters>(   // --
      IrOpcode::kJSConstructForwardVarargs, Operator::kNoProperties,  // opcode
      "JSConstructForwardVarargs",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                              // counts
      parameters);  // parameter
}

const Operator* JSOperatorBuilder::Construct(uint32_t arity,
                                             CallFrequency frequency,
                                             VectorSlotPair const& feedback) {
  ConstructParameters parameters(arity, frequency, feedback);
  return new (zone()) Operator1<ConstructParameters>(   // --
      IrOpcode::kJSConstruct, Operator::kNoProperties,  // opcode
      "JSConstruct",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                // counts
      parameters);                                      // parameter
}

const Operator* JSOperatorBuilder::ConstructWithSpread(uint32_t arity) {
  SpreadWithArityParameter parameters(arity);
  return new (zone()) Operator1<SpreadWithArityParameter>(        // --
      IrOpcode::kJSConstructWithSpread, Operator::kNoProperties,  // opcode
      "JSConstructWithSpread",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                          // counts
      parameters);                                                // parameter
}

const Operator* JSOperatorBuilder::ConvertReceiver(
    ConvertReceiverMode convert_mode) {
  return new (zone()) Operator1<ConvertReceiverMode>(         // --
      IrOpcode::kJSConvertReceiver, Operator::kEliminatable,  // opcode
      "JSConvertReceiver",                                    // name
      1, 1, 1, 1, 1, 0,                                       // counts
      convert_mode);                                          // parameter
}

const Operator* JSOperatorBuilder::LoadNamed(Handle<Name> name,
                                             const VectorSlotPair& feedback) {
  NamedAccess access(SLOPPY, name, feedback);
  return new (zone()) Operator1<NamedAccess>(           // --
      IrOpcode::kJSLoadNamed, Operator::kNoProperties,  // opcode
      "JSLoadNamed",                                    // name
      1, 1, 1, 1, 1, 2,                                 // counts
      access);                                          // parameter
}

const Operator* JSOperatorBuilder::LoadProperty(
    VectorSlotPair const& feedback) {
  PropertyAccess access(SLOPPY, feedback);
  return new (zone()) Operator1<PropertyAccess>(           // --
      IrOpcode::kJSLoadProperty, Operator::kNoProperties,  // opcode
      "JSLoadProperty",                                    // name
      2, 1, 1, 1, 1, 2,                                    // counts
      access);                                             // parameter
}

const Operator* JSOperatorBuilder::GeneratorStore(int register_count,
                                                  SuspendFlags suspend_flags) {
  GeneratorStoreParameters parameters(register_count, suspend_flags);
  return new (zone()) Operator1<GeneratorStoreParameters>(  // --
      IrOpcode::kJSGeneratorStore, Operator::kNoThrow,      // opcode
      "JSGeneratorStore",                                   // name
      3 + register_count, 1, 1, 0, 1, 0,                    // counts
      parameters);                                          // parameter
}

const Operator* JSOperatorBuilder::GeneratorRestoreRegister(int index) {
  return new (zone()) Operator1<int>(                             // --
      IrOpcode::kJSGeneratorRestoreRegister, Operator::kNoThrow,  // opcode
      "JSGeneratorRestoreRegister",                               // name
      1, 1, 1, 1, 1, 0,                                           // counts
      index);                                                     // parameter
}

const Operator* JSOperatorBuilder::StoreNamed(LanguageMode language_mode,
                                              Handle<Name> name,
                                              VectorSlotPair const& feedback) {
  NamedAccess access(language_mode, name, feedback);
  return new (zone()) Operator1<NamedAccess>(            // --
      IrOpcode::kJSStoreNamed, Operator::kNoProperties,  // opcode
      "JSStoreNamed",                                    // name
      2, 1, 1, 0, 1, 2,                                  // counts
      access);                                           // parameter
}


const Operator* JSOperatorBuilder::StoreProperty(
    LanguageMode language_mode, VectorSlotPair const& feedback) {
  PropertyAccess access(language_mode, feedback);
  return new (zone()) Operator1<PropertyAccess>(            // --
      IrOpcode::kJSStoreProperty, Operator::kNoProperties,  // opcode
      "JSStoreProperty",                                    // name
      3, 1, 1, 0, 1, 2,                                     // counts
      access);                                              // parameter
}

const Operator* JSOperatorBuilder::StoreNamedOwn(
    Handle<Name> name, VectorSlotPair const& feedback) {
  StoreNamedOwnParameters parameters(name, feedback);
  return new (zone()) Operator1<StoreNamedOwnParameters>(   // --
      IrOpcode::kJSStoreNamedOwn, Operator::kNoProperties,  // opcode
      "JSStoreNamedOwn",                                    // name
      2, 1, 1, 0, 1, 2,                                     // counts
      parameters);                                          // parameter
}

const Operator* JSOperatorBuilder::DeleteProperty() {
  return new (zone()) Operator(                              // --
      IrOpcode::kJSDeleteProperty, Operator::kNoProperties,  // opcode
      "JSDeleteProperty",                                    // name
      3, 1, 1, 1, 1, 2);                                     // counts
}

const Operator* JSOperatorBuilder::CreateGeneratorObject() {
  return new (zone()) Operator(                                     // --
      IrOpcode::kJSCreateGeneratorObject, Operator::kEliminatable,  // opcode
      "JSCreateGeneratorObject",                                    // name
      2, 1, 1, 1, 1, 0);                                            // counts
}

const Operator* JSOperatorBuilder::LoadGlobal(const Handle<Name>& name,
                                              const VectorSlotPair& feedback,
                                              TypeofMode typeof_mode) {
  LoadGlobalParameters parameters(name, feedback, typeof_mode);
  return new (zone()) Operator1<LoadGlobalParameters>(   // --
      IrOpcode::kJSLoadGlobal, Operator::kNoProperties,  // opcode
      "JSLoadGlobal",                                    // name
      0, 1, 1, 1, 1, 2,                                  // counts
      parameters);                                       // parameter
}


const Operator* JSOperatorBuilder::StoreGlobal(LanguageMode language_mode,
                                               const Handle<Name>& name,
                                               const VectorSlotPair& feedback) {
  StoreGlobalParameters parameters(language_mode, feedback, name);
  return new (zone()) Operator1<StoreGlobalParameters>(   // --
      IrOpcode::kJSStoreGlobal, Operator::kNoProperties,  // opcode
      "JSStoreGlobal",                                    // name
      1, 1, 1, 0, 1, 2,                                   // counts
      parameters);                                        // parameter
}


const Operator* JSOperatorBuilder::LoadContext(size_t depth, size_t index,
                                               bool immutable) {
  ContextAccess access(depth, index, immutable);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSLoadContext,                  // opcode
      Operator::kNoWrite | Operator::kNoThrow,   // flags
      "JSLoadContext",                           // name
      0, 1, 0, 1, 1, 0,                          // counts
      access);                                   // parameter
}


const Operator* JSOperatorBuilder::StoreContext(size_t depth, size_t index) {
  ContextAccess access(depth, index, false);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSStoreContext,                 // opcode
      Operator::kNoRead | Operator::kNoThrow,    // flags
      "JSStoreContext",                          // name
      1, 1, 1, 0, 1, 0,                          // counts
      access);                                   // parameter
}

const Operator* JSOperatorBuilder::LoadModule(int32_t cell_index) {
  return new (zone()) Operator1<int32_t>(       // --
      IrOpcode::kJSLoadModule,                  // opcode
      Operator::kNoWrite | Operator::kNoThrow,  // flags
      "JSLoadModule",                           // name
      1, 1, 1, 1, 1, 0,                         // counts
      cell_index);                              // parameter
}

const Operator* JSOperatorBuilder::StoreModule(int32_t cell_index) {
  return new (zone()) Operator1<int32_t>(      // --
      IrOpcode::kJSStoreModule,                // opcode
      Operator::kNoRead | Operator::kNoThrow,  // flags
      "JSStoreModule",                         // name
      2, 1, 1, 0, 1, 0,                        // counts
      cell_index);                             // parameter
}

const Operator* JSOperatorBuilder::CreateArguments(CreateArgumentsType type) {
  return new (zone()) Operator1<CreateArgumentsType>(         // --
      IrOpcode::kJSCreateArguments, Operator::kEliminatable,  // opcode
      "JSCreateArguments",                                    // name
      1, 1, 0, 1, 1, 0,                                       // counts
      type);                                                  // parameter
}


const Operator* JSOperatorBuilder::CreateArray(size_t arity,
                                               Handle<AllocationSite> site) {
  // constructor, new_target, arg1, ..., argN
  int const value_input_count = static_cast<int>(arity) + 2;
  CreateArrayParameters parameters(arity, site);
  return new (zone()) Operator1<CreateArrayParameters>(   // --
      IrOpcode::kJSCreateArray, Operator::kNoProperties,  // opcode
      "JSCreateArray",                                    // name
      value_input_count, 1, 1, 1, 1, 2,                   // counts
      parameters);                                        // parameter
}

const Operator* JSOperatorBuilder::CreateClosure(
    Handle<SharedFunctionInfo> shared_info, VectorSlotPair const& feedback,
    PretenureFlag pretenure) {
  CreateClosureParameters parameters(shared_info, feedback, pretenure);
  return new (zone()) Operator1<CreateClosureParameters>(  // --
      IrOpcode::kJSCreateClosure, Operator::kNoThrow,      // opcode
      "JSCreateClosure",                                   // name
      0, 1, 1, 1, 1, 0,                                    // counts
      parameters);                                         // parameter
}

const Operator* JSOperatorBuilder::CreateLiteralArray(
    Handle<ConstantElementsPair> constant_elements, int literal_flags,
    int literal_index, int number_of_elements) {
  CreateLiteralParameters parameters(constant_elements, number_of_elements,
                                     literal_flags, literal_index);
  return new (zone()) Operator1<CreateLiteralParameters>(        // --
      IrOpcode::kJSCreateLiteralArray, Operator::kNoProperties,  // opcode
      "JSCreateLiteralArray",                                    // name
      1, 1, 1, 1, 1, 2,                                          // counts
      parameters);                                               // parameter
}

const Operator* JSOperatorBuilder::CreateLiteralObject(
    Handle<BoilerplateDescription> constant_properties, int literal_flags,
    int literal_index, int number_of_properties) {
  CreateLiteralParameters parameters(constant_properties, number_of_properties,
                                     literal_flags, literal_index);
  return new (zone()) Operator1<CreateLiteralParameters>(         // --
      IrOpcode::kJSCreateLiteralObject, Operator::kNoProperties,  // opcode
      "JSCreateLiteralObject",                                    // name
      1, 1, 1, 1, 1, 2,                                           // counts
      parameters);                                                // parameter
}


const Operator* JSOperatorBuilder::CreateLiteralRegExp(
    Handle<String> constant_pattern, int literal_flags, int literal_index) {
  CreateLiteralParameters parameters(constant_pattern, -1, literal_flags,
                                     literal_index);
  return new (zone()) Operator1<CreateLiteralParameters>(         // --
      IrOpcode::kJSCreateLiteralRegExp, Operator::kNoProperties,  // opcode
      "JSCreateLiteralRegExp",                                    // name
      1, 1, 1, 1, 1, 2,                                           // counts
      parameters);                                                // parameter
}

const Operator* JSOperatorBuilder::CreateFunctionContext(int slot_count,
                                                         ScopeType scope_type) {
  CreateFunctionContextParameters parameters(slot_count, scope_type);
  return new (zone()) Operator1<CreateFunctionContextParameters>(   // --
      IrOpcode::kJSCreateFunctionContext, Operator::kNoProperties,  // opcode
      "JSCreateFunctionContext",                                    // name
      1, 1, 1, 1, 1, 2,                                             // counts
      parameters);                                                  // parameter
}

const Operator* JSOperatorBuilder::CreateCatchContext(
    const Handle<String>& name, const Handle<ScopeInfo>& scope_info) {
  CreateCatchContextParameters parameters(name, scope_info);
  return new (zone()) Operator1<CreateCatchContextParameters>(
      IrOpcode::kJSCreateCatchContext, Operator::kNoProperties,  // opcode
      "JSCreateCatchContext",                                    // name
      2, 1, 1, 1, 1, 2,                                          // counts
      parameters);                                               // parameter
}

const Operator* JSOperatorBuilder::CreateWithContext(
    const Handle<ScopeInfo>& scope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>>(
      IrOpcode::kJSCreateWithContext, Operator::kNoProperties,  // opcode
      "JSCreateWithContext",                                    // name
      2, 1, 1, 1, 1, 2,                                         // counts
      scope_info);                                              // parameter
}

const Operator* JSOperatorBuilder::CreateBlockContext(
    const Handle<ScopeInfo>& scope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>>(              // --
      IrOpcode::kJSCreateBlockContext, Operator::kNoProperties,  // opcode
      "JSCreateBlockContext",                                    // name
      1, 1, 1, 1, 1, 2,                                          // counts
      scope_info);                                               // parameter
}

const Operator* JSOperatorBuilder::CreateScriptContext(
    const Handle<ScopeInfo>& scope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>>(               // --
      IrOpcode::kJSCreateScriptContext, Operator::kNoProperties,  // opcode
      "JSCreateScriptContext",                                    // name
      1, 1, 1, 1, 1, 2,                                           // counts
      scope_info);                                                // parameter
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
