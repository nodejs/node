// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_OPERATOR_H_
#define V8_COMPILER_JS_OPERATOR_H_

#include "src/runtime/runtime.h"
#include "src/type-hints.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Operator;
struct JSOperatorGlobalCache;


// Defines a pair of {TypeFeedbackVector} and {TypeFeedbackVectorSlot}, which
// is used to access the type feedback for a certain {Node}.
class VectorSlotPair {
 public:
  VectorSlotPair();
  VectorSlotPair(Handle<TypeFeedbackVector> vector, FeedbackVectorSlot slot)
      : vector_(vector), slot_(slot) {}

  bool IsValid() const { return !vector_.is_null() && !slot_.IsInvalid(); }

  Handle<TypeFeedbackVector> vector() const { return vector_; }
  FeedbackVectorSlot slot() const { return slot_; }

  int index() const;

 private:
  const Handle<TypeFeedbackVector> vector_;
  const FeedbackVectorSlot slot_;
};

bool operator==(VectorSlotPair const&, VectorSlotPair const&);
bool operator!=(VectorSlotPair const&, VectorSlotPair const&);

size_t hash_value(VectorSlotPair const&);


// The ConvertReceiverMode is used as parameter by JSConvertReceiver operators.
ConvertReceiverMode ConvertReceiverModeOf(Operator const* op);


// The ToBooleanHints are used as parameter by JSToBoolean operators.
ToBooleanHints ToBooleanHintsOf(Operator const* op);


// Defines the arity and the feedback for a JavaScript constructor call. This is
// used as a parameter by JSCallConstruct operators.
class CallConstructParameters final {
 public:
  CallConstructParameters(uint32_t arity, float frequency,
                          VectorSlotPair const& feedback)
      : arity_(arity), frequency_(frequency), feedback_(feedback) {}

  uint32_t arity() const { return arity_; }
  float frequency() const { return frequency_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  uint32_t const arity_;
  float const frequency_;
  VectorSlotPair const feedback_;
};

bool operator==(CallConstructParameters const&, CallConstructParameters const&);
bool operator!=(CallConstructParameters const&, CallConstructParameters const&);

size_t hash_value(CallConstructParameters const&);

std::ostream& operator<<(std::ostream&, CallConstructParameters const&);

CallConstructParameters const& CallConstructParametersOf(Operator const*);


// Defines the arity and the call flags for a JavaScript function call. This is
// used as a parameter by JSCallFunction operators.
class CallFunctionParameters final {
 public:
  CallFunctionParameters(size_t arity, float frequency,
                         VectorSlotPair const& feedback,
                         TailCallMode tail_call_mode,
                         ConvertReceiverMode convert_mode)
      : bit_field_(ArityField::encode(arity) |
                   ConvertReceiverModeField::encode(convert_mode) |
                   TailCallModeField::encode(tail_call_mode)),
        frequency_(frequency),
        feedback_(feedback) {}

  size_t arity() const { return ArityField::decode(bit_field_); }
  float frequency() const { return frequency_; }
  ConvertReceiverMode convert_mode() const {
    return ConvertReceiverModeField::decode(bit_field_);
  }
  TailCallMode tail_call_mode() const {
    return TailCallModeField::decode(bit_field_);
  }
  VectorSlotPair const& feedback() const { return feedback_; }

  bool operator==(CallFunctionParameters const& that) const {
    return this->bit_field_ == that.bit_field_ &&
           this->frequency_ == that.frequency_ &&
           this->feedback_ == that.feedback_;
  }
  bool operator!=(CallFunctionParameters const& that) const {
    return !(*this == that);
  }

 private:
  friend size_t hash_value(CallFunctionParameters const& p) {
    return base::hash_combine(p.bit_field_, p.frequency_, p.feedback_);
  }

  typedef BitField<size_t, 0, 29> ArityField;
  typedef BitField<ConvertReceiverMode, 29, 2> ConvertReceiverModeField;
  typedef BitField<TailCallMode, 31, 1> TailCallModeField;

  uint32_t const bit_field_;
  float const frequency_;
  VectorSlotPair const feedback_;
};

size_t hash_value(CallFunctionParameters const&);

std::ostream& operator<<(std::ostream&, CallFunctionParameters const&);

const CallFunctionParameters& CallFunctionParametersOf(const Operator* op);


// Defines the arity and the ID for a runtime function call. This is used as a
// parameter by JSCallRuntime operators.
class CallRuntimeParameters final {
 public:
  CallRuntimeParameters(Runtime::FunctionId id, size_t arity)
      : id_(id), arity_(arity) {}

  Runtime::FunctionId id() const { return id_; }
  size_t arity() const { return arity_; }

 private:
  const Runtime::FunctionId id_;
  const size_t arity_;
};

bool operator==(CallRuntimeParameters const&, CallRuntimeParameters const&);
bool operator!=(CallRuntimeParameters const&, CallRuntimeParameters const&);

size_t hash_value(CallRuntimeParameters const&);

std::ostream& operator<<(std::ostream&, CallRuntimeParameters const&);

const CallRuntimeParameters& CallRuntimeParametersOf(const Operator* op);


// Defines the location of a context slot relative to a specific scope. This is
// used as a parameter by JSLoadContext and JSStoreContext operators and allows
// accessing a context-allocated variable without keeping track of the scope.
class ContextAccess final {
 public:
  ContextAccess(size_t depth, size_t index, bool immutable);

  size_t depth() const { return depth_; }
  size_t index() const { return index_; }
  bool immutable() const { return immutable_; }

 private:
  // For space reasons, we keep this tightly packed, otherwise we could just use
  // a simple int/int/bool POD.
  const bool immutable_;
  const uint16_t depth_;
  const uint32_t index_;
};

bool operator==(ContextAccess const&, ContextAccess const&);
bool operator!=(ContextAccess const&, ContextAccess const&);

size_t hash_value(ContextAccess const&);

std::ostream& operator<<(std::ostream&, ContextAccess const&);

ContextAccess const& ContextAccessOf(Operator const*);

// Defines the name and ScopeInfo for a new catch context. This is used as a
// parameter by the JSCreateCatchContext operator.
class CreateCatchContextParameters final {
 public:
  CreateCatchContextParameters(Handle<String> catch_name,
                               Handle<ScopeInfo> scope_info);

  Handle<String> catch_name() const { return catch_name_; }
  Handle<ScopeInfo> scope_info() const { return scope_info_; }

 private:
  Handle<String> const catch_name_;
  Handle<ScopeInfo> const scope_info_;
};

bool operator==(CreateCatchContextParameters const& lhs,
                CreateCatchContextParameters const& rhs);
bool operator!=(CreateCatchContextParameters const& lhs,
                CreateCatchContextParameters const& rhs);

size_t hash_value(CreateCatchContextParameters const& parameters);

std::ostream& operator<<(std::ostream& os,
                         CreateCatchContextParameters const& parameters);

CreateCatchContextParameters const& CreateCatchContextParametersOf(
    Operator const*);

// Defines the property of an object for a named access. This is
// used as a parameter by the JSLoadNamed and JSStoreNamed operators.
class NamedAccess final {
 public:
  NamedAccess(LanguageMode language_mode, Handle<Name> name,
              VectorSlotPair const& feedback)
      : name_(name), feedback_(feedback), language_mode_(language_mode) {}

  Handle<Name> name() const { return name_; }
  LanguageMode language_mode() const { return language_mode_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  Handle<Name> const name_;
  VectorSlotPair const feedback_;
  LanguageMode const language_mode_;
};

bool operator==(NamedAccess const&, NamedAccess const&);
bool operator!=(NamedAccess const&, NamedAccess const&);

size_t hash_value(NamedAccess const&);

std::ostream& operator<<(std::ostream&, NamedAccess const&);

const NamedAccess& NamedAccessOf(const Operator* op);


// Defines the property being loaded from an object by a named load. This is
// used as a parameter by JSLoadGlobal operator.
class LoadGlobalParameters final {
 public:
  LoadGlobalParameters(const Handle<Name>& name, const VectorSlotPair& feedback,
                       TypeofMode typeof_mode)
      : name_(name), feedback_(feedback), typeof_mode_(typeof_mode) {}

  const Handle<Name>& name() const { return name_; }
  TypeofMode typeof_mode() const { return typeof_mode_; }

  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  const Handle<Name> name_;
  const VectorSlotPair feedback_;
  const TypeofMode typeof_mode_;
};

bool operator==(LoadGlobalParameters const&, LoadGlobalParameters const&);
bool operator!=(LoadGlobalParameters const&, LoadGlobalParameters const&);

size_t hash_value(LoadGlobalParameters const&);

std::ostream& operator<<(std::ostream&, LoadGlobalParameters const&);

const LoadGlobalParameters& LoadGlobalParametersOf(const Operator* op);


// Defines the property being stored to an object by a named store. This is
// used as a parameter by JSStoreGlobal operator.
class StoreGlobalParameters final {
 public:
  StoreGlobalParameters(LanguageMode language_mode,
                        const VectorSlotPair& feedback,
                        const Handle<Name>& name)
      : language_mode_(language_mode), name_(name), feedback_(feedback) {}

  LanguageMode language_mode() const { return language_mode_; }
  const VectorSlotPair& feedback() const { return feedback_; }
  const Handle<Name>& name() const { return name_; }

 private:
  const LanguageMode language_mode_;
  const Handle<Name> name_;
  const VectorSlotPair feedback_;
};

bool operator==(StoreGlobalParameters const&, StoreGlobalParameters const&);
bool operator!=(StoreGlobalParameters const&, StoreGlobalParameters const&);

size_t hash_value(StoreGlobalParameters const&);

std::ostream& operator<<(std::ostream&, StoreGlobalParameters const&);

const StoreGlobalParameters& StoreGlobalParametersOf(const Operator* op);


// Defines the property of an object for a keyed access. This is used
// as a parameter by the JSLoadProperty and JSStoreProperty operators.
class PropertyAccess final {
 public:
  PropertyAccess(LanguageMode language_mode, VectorSlotPair const& feedback)
      : feedback_(feedback), language_mode_(language_mode) {}

  LanguageMode language_mode() const { return language_mode_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  VectorSlotPair const feedback_;
  LanguageMode const language_mode_;
};

bool operator==(PropertyAccess const&, PropertyAccess const&);
bool operator!=(PropertyAccess const&, PropertyAccess const&);

size_t hash_value(PropertyAccess const&);

std::ostream& operator<<(std::ostream&, PropertyAccess const&);

PropertyAccess const& PropertyAccessOf(const Operator* op);


// CreateArgumentsType is used as parameter to JSCreateArguments nodes.
CreateArgumentsType const& CreateArgumentsTypeOf(const Operator* op);


// Defines shared information for the array that should be created. This is
// used as parameter by JSCreateArray operators.
class CreateArrayParameters final {
 public:
  explicit CreateArrayParameters(size_t arity, Handle<AllocationSite> site)
      : arity_(arity), site_(site) {}

  size_t arity() const { return arity_; }
  Handle<AllocationSite> site() const { return site_; }

 private:
  size_t const arity_;
  Handle<AllocationSite> const site_;
};

bool operator==(CreateArrayParameters const&, CreateArrayParameters const&);
bool operator!=(CreateArrayParameters const&, CreateArrayParameters const&);

size_t hash_value(CreateArrayParameters const&);

std::ostream& operator<<(std::ostream&, CreateArrayParameters const&);

const CreateArrayParameters& CreateArrayParametersOf(const Operator* op);


// Defines shared information for the closure that should be created. This is
// used as a parameter by JSCreateClosure operators.
class CreateClosureParameters final {
 public:
  CreateClosureParameters(Handle<SharedFunctionInfo> shared_info,
                          PretenureFlag pretenure)
      : shared_info_(shared_info), pretenure_(pretenure) {}

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  PretenureFlag pretenure() const { return pretenure_; }

 private:
  const Handle<SharedFunctionInfo> shared_info_;
  const PretenureFlag pretenure_;
};

bool operator==(CreateClosureParameters const&, CreateClosureParameters const&);
bool operator!=(CreateClosureParameters const&, CreateClosureParameters const&);

size_t hash_value(CreateClosureParameters const&);

std::ostream& operator<<(std::ostream&, CreateClosureParameters const&);

const CreateClosureParameters& CreateClosureParametersOf(const Operator* op);

// Defines shared information for the literal that should be created. This is
// used as parameter by JSCreateLiteralArray, JSCreateLiteralObject and
// JSCreateLiteralRegExp operators.
class CreateLiteralParameters final {
 public:
  CreateLiteralParameters(Handle<HeapObject> constant, int length, int flags,
                          int index)
      : constant_(constant), length_(length), flags_(flags), index_(index) {}

  Handle<HeapObject> constant() const { return constant_; }
  int length() const { return length_; }
  int flags() const { return flags_; }
  int index() const { return index_; }

 private:
  Handle<HeapObject> const constant_;
  int const length_;
  int const flags_;
  int const index_;
};

bool operator==(CreateLiteralParameters const&, CreateLiteralParameters const&);
bool operator!=(CreateLiteralParameters const&, CreateLiteralParameters const&);

size_t hash_value(CreateLiteralParameters const&);

std::ostream& operator<<(std::ostream&, CreateLiteralParameters const&);

const CreateLiteralParameters& CreateLiteralParametersOf(const Operator* op);

BinaryOperationHint BinaryOperationHintOf(const Operator* op);

CompareOperationHint CompareOperationHintOf(const Operator* op);

// Interface for building JavaScript-level operators, e.g. directly from the
// AST. Most operators have no parameters, thus can be globally shared for all
// graphs.
class JSOperatorBuilder final : public ZoneObject {
 public:
  explicit JSOperatorBuilder(Zone* zone);

  const Operator* Equal(CompareOperationHint hint);
  const Operator* NotEqual(CompareOperationHint hint);
  const Operator* StrictEqual(CompareOperationHint hint);
  const Operator* StrictNotEqual(CompareOperationHint hint);
  const Operator* LessThan(CompareOperationHint hint);
  const Operator* GreaterThan(CompareOperationHint hint);
  const Operator* LessThanOrEqual(CompareOperationHint hint);
  const Operator* GreaterThanOrEqual(CompareOperationHint hint);

  const Operator* BitwiseOr(BinaryOperationHint hint);
  const Operator* BitwiseXor(BinaryOperationHint hint);
  const Operator* BitwiseAnd(BinaryOperationHint hint);
  const Operator* ShiftLeft(BinaryOperationHint hint);
  const Operator* ShiftRight(BinaryOperationHint hint);
  const Operator* ShiftRightLogical(BinaryOperationHint hint);
  const Operator* Add(BinaryOperationHint hint);
  const Operator* Subtract(BinaryOperationHint hint);
  const Operator* Multiply(BinaryOperationHint hint);
  const Operator* Divide(BinaryOperationHint hint);
  const Operator* Modulus(BinaryOperationHint hint);

  const Operator* ToBoolean(ToBooleanHints hints);
  const Operator* ToInteger();
  const Operator* ToLength();
  const Operator* ToName();
  const Operator* ToNumber();
  const Operator* ToObject();
  const Operator* ToString();

  const Operator* Create();
  const Operator* CreateArguments(CreateArgumentsType type);
  const Operator* CreateArray(size_t arity, Handle<AllocationSite> site);
  const Operator* CreateClosure(Handle<SharedFunctionInfo> shared_info,
                                PretenureFlag pretenure);
  const Operator* CreateIterResultObject();
  const Operator* CreateLiteralArray(Handle<FixedArray> constant_elements,
                                     int literal_flags, int literal_index,
                                     int number_of_elements);
  const Operator* CreateLiteralObject(Handle<FixedArray> constant_properties,
                                      int literal_flags, int literal_index,
                                      int number_of_properties);
  const Operator* CreateLiteralRegExp(Handle<String> constant_pattern,
                                      int literal_flags, int literal_index);

  const Operator* CallFunction(
      size_t arity, float frequency = 0.0f,
      VectorSlotPair const& feedback = VectorSlotPair(),
      ConvertReceiverMode convert_mode = ConvertReceiverMode::kAny,
      TailCallMode tail_call_mode = TailCallMode::kDisallow);
  const Operator* CallRuntime(Runtime::FunctionId id);
  const Operator* CallRuntime(Runtime::FunctionId id, size_t arity);
  const Operator* CallRuntime(const Runtime::Function* function, size_t arity);
  const Operator* CallConstruct(uint32_t arity, float frequency,
                                VectorSlotPair const& feedback);

  const Operator* ConvertReceiver(ConvertReceiverMode convert_mode);

  const Operator* LoadProperty(VectorSlotPair const& feedback);
  const Operator* LoadNamed(Handle<Name> name, VectorSlotPair const& feedback);

  const Operator* StoreProperty(LanguageMode language_mode,
                                VectorSlotPair const& feedback);
  const Operator* StoreNamed(LanguageMode language_mode, Handle<Name> name,
                             VectorSlotPair const& feedback);

  const Operator* DeleteProperty(LanguageMode language_mode);

  const Operator* HasProperty();

  const Operator* LoadGlobal(const Handle<Name>& name,
                             const VectorSlotPair& feedback,
                             TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  const Operator* StoreGlobal(LanguageMode language_mode,
                              const Handle<Name>& name,
                              const VectorSlotPair& feedback);

  const Operator* LoadContext(size_t depth, size_t index, bool immutable);
  const Operator* StoreContext(size_t depth, size_t index);

  const Operator* TypeOf();
  const Operator* InstanceOf();
  const Operator* OrdinaryHasInstance();

  const Operator* ForInNext();
  const Operator* ForInPrepare();

  const Operator* LoadMessage();
  const Operator* StoreMessage();

  // Used to implement Ignition's SuspendGenerator bytecode.
  const Operator* GeneratorStore(int register_count);

  // Used to implement Ignition's ResumeGenerator bytecode.
  const Operator* GeneratorRestoreContinuation();
  const Operator* GeneratorRestoreRegister(int index);

  const Operator* StackCheck();

  const Operator* CreateFunctionContext(int slot_count);
  const Operator* CreateCatchContext(const Handle<String>& name,
                                     const Handle<ScopeInfo>& scope_info);
  const Operator* CreateWithContext(const Handle<ScopeInfo>& scope_info);
  const Operator* CreateBlockContext(const Handle<ScopeInfo>& scpope_info);
  const Operator* CreateModuleContext();
  const Operator* CreateScriptContext(const Handle<ScopeInfo>& scpope_info);

 private:
  Zone* zone() const { return zone_; }

  const JSOperatorGlobalCache& cache_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(JSOperatorBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_OPERATOR_H_
