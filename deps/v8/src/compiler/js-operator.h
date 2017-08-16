// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_OPERATOR_H_
#define V8_COMPILER_JS_OPERATOR_H_

#include "src/base/compiler-specific.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/runtime/runtime.h"
#include "src/type-hints.h"

namespace v8 {
namespace internal {

class AllocationSite;
class BoilerplateDescription;
class ConstantElementsPair;
class SharedFunctionInfo;
class FeedbackVector;

namespace compiler {

// Forward declarations.
class Operator;
struct JSOperatorGlobalCache;

// Defines the frequency a given Call/Construct site was executed. For some
// call sites the frequency is not known.
class CallFrequency final {
 public:
  CallFrequency() : value_(std::numeric_limits<float>::quiet_NaN()) {}
  explicit CallFrequency(float value) : value_(value) {
    DCHECK(!std::isnan(value));
  }

  bool IsKnown() const { return !IsUnknown(); }
  bool IsUnknown() const { return std::isnan(value_); }
  float value() const {
    DCHECK(IsKnown());
    return value_;
  }

  bool operator==(CallFrequency const& that) const {
    return bit_cast<uint32_t>(this->value_) == bit_cast<uint32_t>(that.value_);
  }
  bool operator!=(CallFrequency const& that) const { return !(*this == that); }

  friend size_t hash_value(CallFrequency f) {
    return bit_cast<uint32_t>(f.value_);
  }

 private:
  float value_;
};

std::ostream& operator<<(std::ostream&, CallFrequency);

// Defines a pair of {FeedbackVector} and {FeedbackSlot}, which
// is used to access the type feedback for a certain {Node}.
class V8_EXPORT_PRIVATE VectorSlotPair {
 public:
  VectorSlotPair();
  VectorSlotPair(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : vector_(vector), slot_(slot) {}

  bool IsValid() const { return !vector_.is_null() && !slot_.IsInvalid(); }

  Handle<FeedbackVector> vector() const { return vector_; }
  FeedbackSlot slot() const { return slot_; }

  int index() const;

 private:
  const Handle<FeedbackVector> vector_;
  const FeedbackSlot slot_;
};

bool operator==(VectorSlotPair const&, VectorSlotPair const&);
bool operator!=(VectorSlotPair const&, VectorSlotPair const&);

size_t hash_value(VectorSlotPair const&);


// The ConvertReceiverMode is used as parameter by JSConvertReceiver operators.
ConvertReceiverMode ConvertReceiverModeOf(Operator const* op);


// The ToBooleanHints are used as parameter by JSToBoolean operators.
ToBooleanHints ToBooleanHintsOf(Operator const* op);

// Defines the flags for a JavaScript call forwarding parameters. This
// is used as parameter by JSConstructForwardVarargs operators.
class ConstructForwardVarargsParameters final {
 public:
  ConstructForwardVarargsParameters(size_t arity, uint32_t start_index)
      : bit_field_(ArityField::encode(arity) |
                   StartIndexField::encode(start_index)) {}

  size_t arity() const { return ArityField::decode(bit_field_); }
  uint32_t start_index() const { return StartIndexField::decode(bit_field_); }

  bool operator==(ConstructForwardVarargsParameters const& that) const {
    return this->bit_field_ == that.bit_field_;
  }
  bool operator!=(ConstructForwardVarargsParameters const& that) const {
    return !(*this == that);
  }

 private:
  friend size_t hash_value(ConstructForwardVarargsParameters const& p) {
    return p.bit_field_;
  }

  typedef BitField<size_t, 0, 16> ArityField;
  typedef BitField<uint32_t, 16, 16> StartIndexField;

  uint32_t const bit_field_;
};

std::ostream& operator<<(std::ostream&,
                         ConstructForwardVarargsParameters const&);

ConstructForwardVarargsParameters const& ConstructForwardVarargsParametersOf(
    Operator const*) WARN_UNUSED_RESULT;

// Defines the arity and the feedback for a JavaScript constructor call. This is
// used as a parameter by JSConstruct operators.
class ConstructParameters final {
 public:
  ConstructParameters(uint32_t arity, CallFrequency frequency,
                      VectorSlotPair const& feedback)
      : arity_(arity), frequency_(frequency), feedback_(feedback) {}

  uint32_t arity() const { return arity_; }
  CallFrequency frequency() const { return frequency_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  uint32_t const arity_;
  CallFrequency const frequency_;
  VectorSlotPair const feedback_;
};

bool operator==(ConstructParameters const&, ConstructParameters const&);
bool operator!=(ConstructParameters const&, ConstructParameters const&);

size_t hash_value(ConstructParameters const&);

std::ostream& operator<<(std::ostream&, ConstructParameters const&);

ConstructParameters const& ConstructParametersOf(Operator const*);

// Defines the arity for JavaScript calls with a spread as the last
// parameter. This is used as a parameter by JSConstructWithSpread and
// JSCallWithSpread operators.
class SpreadWithArityParameter final {
 public:
  explicit SpreadWithArityParameter(uint32_t arity) : arity_(arity) {}

  uint32_t arity() const { return arity_; }

 private:
  uint32_t const arity_;
};

bool operator==(SpreadWithArityParameter const&,
                SpreadWithArityParameter const&);
bool operator!=(SpreadWithArityParameter const&,
                SpreadWithArityParameter const&);

size_t hash_value(SpreadWithArityParameter const&);

std::ostream& operator<<(std::ostream&, SpreadWithArityParameter const&);

SpreadWithArityParameter const& SpreadWithArityParameterOf(Operator const*);

// Defines the flags for a JavaScript call forwarding parameters. This
// is used as parameter by JSCallForwardVarargs operators.
class CallForwardVarargsParameters final {
 public:
  CallForwardVarargsParameters(size_t arity, uint32_t start_index,
                               TailCallMode tail_call_mode)
      : bit_field_(ArityField::encode(arity) |
                   StartIndexField::encode(start_index) |
                   TailCallModeField::encode(tail_call_mode)) {}

  size_t arity() const { return ArityField::decode(bit_field_); }
  uint32_t start_index() const { return StartIndexField::decode(bit_field_); }
  TailCallMode tail_call_mode() const {
    return TailCallModeField::decode(bit_field_);
  }

  bool operator==(CallForwardVarargsParameters const& that) const {
    return this->bit_field_ == that.bit_field_;
  }
  bool operator!=(CallForwardVarargsParameters const& that) const {
    return !(*this == that);
  }

 private:
  friend size_t hash_value(CallForwardVarargsParameters const& p) {
    return p.bit_field_;
  }

  typedef BitField<size_t, 0, 15> ArityField;
  typedef BitField<uint32_t, 15, 15> StartIndexField;
  typedef BitField<TailCallMode, 30, 1> TailCallModeField;

  uint32_t const bit_field_;
};

std::ostream& operator<<(std::ostream&, CallForwardVarargsParameters const&);

CallForwardVarargsParameters const& CallForwardVarargsParametersOf(
    Operator const*) WARN_UNUSED_RESULT;

// Defines the arity and the call flags for a JavaScript function call. This is
// used as a parameter by JSCall operators.
class CallParameters final {
 public:
  CallParameters(size_t arity, CallFrequency frequency,
                 VectorSlotPair const& feedback, TailCallMode tail_call_mode,
                 ConvertReceiverMode convert_mode)
      : bit_field_(ArityField::encode(arity) |
                   ConvertReceiverModeField::encode(convert_mode) |
                   TailCallModeField::encode(tail_call_mode)),
        frequency_(frequency),
        feedback_(feedback) {}

  size_t arity() const { return ArityField::decode(bit_field_); }
  CallFrequency frequency() const { return frequency_; }
  ConvertReceiverMode convert_mode() const {
    return ConvertReceiverModeField::decode(bit_field_);
  }
  TailCallMode tail_call_mode() const {
    return TailCallModeField::decode(bit_field_);
  }
  VectorSlotPair const& feedback() const { return feedback_; }

  bool operator==(CallParameters const& that) const {
    return this->bit_field_ == that.bit_field_ &&
           this->frequency_ == that.frequency_ &&
           this->feedback_ == that.feedback_;
  }
  bool operator!=(CallParameters const& that) const { return !(*this == that); }

 private:
  friend size_t hash_value(CallParameters const& p) {
    return base::hash_combine(p.bit_field_, p.frequency_, p.feedback_);
  }

  typedef BitField<size_t, 0, 29> ArityField;
  typedef BitField<ConvertReceiverMode, 29, 2> ConvertReceiverModeField;
  typedef BitField<TailCallMode, 31, 1> TailCallModeField;

  uint32_t const bit_field_;
  CallFrequency const frequency_;
  VectorSlotPair const feedback_;
};

size_t hash_value(CallParameters const&);

std::ostream& operator<<(std::ostream&, CallParameters const&);

const CallParameters& CallParametersOf(const Operator* op);


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

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, ContextAccess const&);

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

// Defines the slot count and ScopeType for a new function or eval context. This
// is used as a parameter by the JSCreateFunctionContext operator.
class CreateFunctionContextParameters final {
 public:
  CreateFunctionContextParameters(int slot_count, ScopeType scope_type);

  int slot_count() const { return slot_count_; }
  ScopeType scope_type() const { return scope_type_; }

 private:
  int const slot_count_;
  ScopeType const scope_type_;
};

bool operator==(CreateFunctionContextParameters const& lhs,
                CreateFunctionContextParameters const& rhs);
bool operator!=(CreateFunctionContextParameters const& lhs,
                CreateFunctionContextParameters const& rhs);

size_t hash_value(CreateFunctionContextParameters const& parameters);

std::ostream& operator<<(std::ostream& os,
                         CreateFunctionContextParameters const& parameters);

CreateFunctionContextParameters const& CreateFunctionContextParametersOf(
    Operator const*);

// Defines parameters for JSStoreNamedOwn operator.
class StoreNamedOwnParameters final {
 public:
  StoreNamedOwnParameters(Handle<Name> name, VectorSlotPair const& feedback)
      : name_(name), feedback_(feedback) {}

  Handle<Name> name() const { return name_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  Handle<Name> const name_;
  VectorSlotPair const feedback_;
};

bool operator==(StoreNamedOwnParameters const&, StoreNamedOwnParameters const&);
bool operator!=(StoreNamedOwnParameters const&, StoreNamedOwnParameters const&);

size_t hash_value(StoreNamedOwnParameters const&);

std::ostream& operator<<(std::ostream&, StoreNamedOwnParameters const&);

const StoreNamedOwnParameters& StoreNamedOwnParametersOf(const Operator* op);

// Defines the feedback, i.e., vector and index, for storing a data property in
// an object literal. This is
// used as a parameter by the JSStoreDataPropertyInLiteral operator.
class FeedbackParameter final {
 public:
  explicit FeedbackParameter(VectorSlotPair const& feedback)
      : feedback_(feedback) {}

  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  VectorSlotPair const feedback_;
};

bool operator==(FeedbackParameter const&, FeedbackParameter const&);
bool operator!=(FeedbackParameter const&, FeedbackParameter const&);

size_t hash_value(FeedbackParameter const&);

std::ostream& operator<<(std::ostream&, FeedbackParameter const&);

const FeedbackParameter& FeedbackParameterOf(const Operator* op);

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
                          VectorSlotPair const& feedback,
                          PretenureFlag pretenure)
      : shared_info_(shared_info), feedback_(feedback), pretenure_(pretenure) {}

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  VectorSlotPair const& feedback() const { return feedback_; }
  PretenureFlag pretenure() const { return pretenure_; }

 private:
  const Handle<SharedFunctionInfo> shared_info_;
  VectorSlotPair const feedback_;
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

class GeneratorStoreParameters final {
 public:
  GeneratorStoreParameters(int register_count, SuspendFlags flags)
      : register_count_(register_count), suspend_flags_(flags) {}

  int register_count() const { return register_count_; }
  SuspendFlags suspend_flags() const { return suspend_flags_; }
  SuspendFlags suspend_type() const {
    return suspend_flags_ & SuspendFlags::kSuspendTypeMask;
  }

 private:
  int register_count_;
  SuspendFlags suspend_flags_;
};

bool operator==(GeneratorStoreParameters const&,
                GeneratorStoreParameters const&);
bool operator!=(GeneratorStoreParameters const&,
                GeneratorStoreParameters const&);

size_t hash_value(GeneratorStoreParameters const&);

std::ostream& operator<<(std::ostream&, GeneratorStoreParameters const&);

const GeneratorStoreParameters& GeneratorStoreParametersOf(const Operator* op);

BinaryOperationHint BinaryOperationHintOf(const Operator* op);

CompareOperationHint CompareOperationHintOf(const Operator* op);

// Interface for building JavaScript-level operators, e.g. directly from the
// AST. Most operators have no parameters, thus can be globally shared for all
// graphs.
class V8_EXPORT_PRIVATE JSOperatorBuilder final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  explicit JSOperatorBuilder(Zone* zone);

  const Operator* Equal(CompareOperationHint hint);
  const Operator* StrictEqual(CompareOperationHint hint);
  const Operator* LessThan(CompareOperationHint hint);
  const Operator* GreaterThan(CompareOperationHint hint);
  const Operator* LessThanOrEqual(CompareOperationHint hint);
  const Operator* GreaterThanOrEqual(CompareOperationHint hint);

  const Operator* BitwiseOr();
  const Operator* BitwiseXor();
  const Operator* BitwiseAnd();
  const Operator* ShiftLeft();
  const Operator* ShiftRight();
  const Operator* ShiftRightLogical();
  const Operator* Add(BinaryOperationHint hint);
  const Operator* Subtract();
  const Operator* Multiply();
  const Operator* Divide();
  const Operator* Modulus();

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
                                VectorSlotPair const& feedback,
                                PretenureFlag pretenure);
  const Operator* CreateIterResultObject();
  const Operator* CreateKeyValueArray();
  const Operator* CreateLiteralArray(Handle<ConstantElementsPair> constant,
                                     int literal_flags, int literal_index,
                                     int number_of_elements);
  const Operator* CreateLiteralObject(Handle<BoilerplateDescription> constant,
                                      int literal_flags, int literal_index,
                                      int number_of_properties);
  const Operator* CreateLiteralRegExp(Handle<String> constant_pattern,
                                      int literal_flags, int literal_index);

  const Operator* CallForwardVarargs(size_t arity, uint32_t start_index,
                                     TailCallMode tail_call_mode);
  const Operator* Call(
      size_t arity, CallFrequency frequency = CallFrequency(),
      VectorSlotPair const& feedback = VectorSlotPair(),
      ConvertReceiverMode convert_mode = ConvertReceiverMode::kAny,
      TailCallMode tail_call_mode = TailCallMode::kDisallow);
  const Operator* CallWithSpread(uint32_t arity);
  const Operator* CallRuntime(Runtime::FunctionId id);
  const Operator* CallRuntime(Runtime::FunctionId id, size_t arity);
  const Operator* CallRuntime(const Runtime::Function* function, size_t arity);

  const Operator* ConstructForwardVarargs(size_t arity, uint32_t start_index);
  const Operator* Construct(uint32_t arity,
                            CallFrequency frequency = CallFrequency(),
                            VectorSlotPair const& feedback = VectorSlotPair());
  const Operator* ConstructWithSpread(uint32_t arity);

  const Operator* ConvertReceiver(ConvertReceiverMode convert_mode);

  const Operator* LoadProperty(VectorSlotPair const& feedback);
  const Operator* LoadNamed(Handle<Name> name, VectorSlotPair const& feedback);

  const Operator* StoreProperty(LanguageMode language_mode,
                                VectorSlotPair const& feedback);
  const Operator* StoreNamed(LanguageMode language_mode, Handle<Name> name,
                             VectorSlotPair const& feedback);

  const Operator* StoreNamedOwn(Handle<Name> name,
                                VectorSlotPair const& feedback);
  const Operator* StoreDataPropertyInLiteral(const VectorSlotPair& feedback);

  const Operator* DeleteProperty();

  const Operator* HasProperty();

  const Operator* GetSuperConstructor();

  const Operator* CreateGeneratorObject();

  const Operator* LoadGlobal(const Handle<Name>& name,
                             const VectorSlotPair& feedback,
                             TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  const Operator* StoreGlobal(LanguageMode language_mode,
                              const Handle<Name>& name,
                              const VectorSlotPair& feedback);

  const Operator* LoadContext(size_t depth, size_t index, bool immutable);
  const Operator* StoreContext(size_t depth, size_t index);

  const Operator* LoadModule(int32_t cell_index);
  const Operator* StoreModule(int32_t cell_index);

  const Operator* ClassOf();
  const Operator* TypeOf();
  const Operator* InstanceOf();
  const Operator* OrdinaryHasInstance();

  const Operator* ForInNext();
  const Operator* ForInPrepare();

  const Operator* LoadMessage();
  const Operator* StoreMessage();

  // Used to implement Ignition's SuspendGenerator bytecode.
  const Operator* GeneratorStore(int register_count,
                                 SuspendFlags suspend_flags);

  // Used to implement Ignition's ResumeGenerator bytecode.
  const Operator* GeneratorRestoreContinuation();
  const Operator* GeneratorRestoreRegister(int index);

  const Operator* StackCheck();
  const Operator* Debugger();

  const Operator* CreateFunctionContext(int slot_count, ScopeType scope_type);
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
