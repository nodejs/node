// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_OPERATOR_H_
#define V8_COMPILER_JS_OPERATOR_H_

#include "src/base/compiler-specific.h"
#include "src/common/globals.h"
#include "src/compiler/vector-slot-pair.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/type-hints.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class AllocationSite;
class ObjectBoilerplateDescription;
class ArrayBoilerplateDescription;
class FeedbackCell;
class SharedFunctionInfo;

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

  static constexpr float kNoFeedbackCallFrequency = -1;

 private:
  float value_;
};

std::ostream& operator<<(std::ostream&, CallFrequency);

CallFrequency CallFrequencyOf(Operator const* op) V8_WARN_UNUSED_RESULT;

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

  using ArityField = BitField<size_t, 0, 16>;
  using StartIndexField = BitField<uint32_t, 16, 16>;

  uint32_t const bit_field_;
};

std::ostream& operator<<(std::ostream&,
                         ConstructForwardVarargsParameters const&);

ConstructForwardVarargsParameters const& ConstructForwardVarargsParametersOf(
    Operator const*) V8_WARN_UNUSED_RESULT;

// Defines the arity and the feedback for a JavaScript constructor call. This is
// used as a parameter by JSConstruct and JSConstructWithSpread operators.
class ConstructParameters final {
 public:
  ConstructParameters(uint32_t arity, CallFrequency frequency,
                      VectorSlotPair const& feedback)
      : arity_(arity), frequency_(frequency), feedback_(feedback) {}

  uint32_t arity() const { return arity_; }
  CallFrequency const& frequency() const { return frequency_; }
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

// Defines the flags for a JavaScript call forwarding parameters. This
// is used as parameter by JSCallForwardVarargs operators.
class CallForwardVarargsParameters final {
 public:
  CallForwardVarargsParameters(size_t arity, uint32_t start_index)
      : bit_field_(ArityField::encode(arity) |
                   StartIndexField::encode(start_index)) {}

  size_t arity() const { return ArityField::decode(bit_field_); }
  uint32_t start_index() const { return StartIndexField::decode(bit_field_); }

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

  using ArityField = BitField<size_t, 0, 15>;
  using StartIndexField = BitField<uint32_t, 15, 15>;

  uint32_t const bit_field_;
};

std::ostream& operator<<(std::ostream&, CallForwardVarargsParameters const&);

CallForwardVarargsParameters const& CallForwardVarargsParametersOf(
    Operator const*) V8_WARN_UNUSED_RESULT;

// Defines the arity and the call flags for a JavaScript function call. This is
// used as a parameter by JSCall and JSCallWithSpread operators.
class CallParameters final {
 public:
  CallParameters(size_t arity, CallFrequency const& frequency,
                 VectorSlotPair const& feedback,
                 ConvertReceiverMode convert_mode,
                 SpeculationMode speculation_mode)
      : bit_field_(ArityField::encode(arity) |
                   SpeculationModeField::encode(speculation_mode) |
                   ConvertReceiverModeField::encode(convert_mode)),
        frequency_(frequency),
        feedback_(feedback) {}

  size_t arity() const { return ArityField::decode(bit_field_); }
  CallFrequency const& frequency() const { return frequency_; }
  ConvertReceiverMode convert_mode() const {
    return ConvertReceiverModeField::decode(bit_field_);
  }
  VectorSlotPair const& feedback() const { return feedback_; }

  SpeculationMode speculation_mode() const {
    return SpeculationModeField::decode(bit_field_);
  }

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

  using ArityField = BitField<size_t, 0, 28>;
  using SpeculationModeField = BitField<SpeculationMode, 28, 1>;
  using ConvertReceiverModeField = BitField<ConvertReceiverMode, 29, 2>;

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

V8_EXPORT_PRIVATE ContextAccess const& ContextAccessOf(Operator const*);

// Defines the slot count and ScopeType for a new function or eval context. This
// is used as a parameter by the JSCreateFunctionContext operator.
class CreateFunctionContextParameters final {
 public:
  CreateFunctionContextParameters(Handle<ScopeInfo> scope_info, int slot_count,
                                  ScopeType scope_type);

  Handle<ScopeInfo> scope_info() const { return scope_info_; }
  int slot_count() const { return slot_count_; }
  ScopeType scope_type() const { return scope_type_; }

 private:
  Handle<ScopeInfo> scope_info_;
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
// an object literal. This is used as a parameter by JSCreateEmptyLiteralArray
// and JSStoreDataPropertyInLiteral operators.
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

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&,
                                           PropertyAccess const&);

PropertyAccess const& PropertyAccessOf(const Operator* op);


// CreateArgumentsType is used as parameter to JSCreateArguments nodes.
CreateArgumentsType const& CreateArgumentsTypeOf(const Operator* op);


// Defines shared information for the array that should be created. This is
// used as parameter by JSCreateArray operators.
class CreateArrayParameters final {
 public:
  explicit CreateArrayParameters(size_t arity, MaybeHandle<AllocationSite> site)
      : arity_(arity), site_(site) {}

  size_t arity() const { return arity_; }
  MaybeHandle<AllocationSite> site() const { return site_; }

 private:
  size_t const arity_;
  MaybeHandle<AllocationSite> const site_;
};

bool operator==(CreateArrayParameters const&, CreateArrayParameters const&);
bool operator!=(CreateArrayParameters const&, CreateArrayParameters const&);

size_t hash_value(CreateArrayParameters const&);

std::ostream& operator<<(std::ostream&, CreateArrayParameters const&);

const CreateArrayParameters& CreateArrayParametersOf(const Operator* op);

// Defines shared information for the array iterator that should be created.
// This is used as parameter by JSCreateArrayIterator operators.
class CreateArrayIteratorParameters final {
 public:
  explicit CreateArrayIteratorParameters(IterationKind kind) : kind_(kind) {}

  IterationKind kind() const { return kind_; }

 private:
  IterationKind const kind_;
};

bool operator==(CreateArrayIteratorParameters const&,
                CreateArrayIteratorParameters const&);
bool operator!=(CreateArrayIteratorParameters const&,
                CreateArrayIteratorParameters const&);

size_t hash_value(CreateArrayIteratorParameters const&);

std::ostream& operator<<(std::ostream&, CreateArrayIteratorParameters const&);

const CreateArrayIteratorParameters& CreateArrayIteratorParametersOf(
    const Operator* op);

// Defines shared information for the array iterator that should be created.
// This is used as parameter by JSCreateCollectionIterator operators.
class CreateCollectionIteratorParameters final {
 public:
  explicit CreateCollectionIteratorParameters(CollectionKind collection_kind,
                                              IterationKind iteration_kind)
      : collection_kind_(collection_kind), iteration_kind_(iteration_kind) {
    CHECK(!(collection_kind == CollectionKind::kSet &&
            iteration_kind == IterationKind::kKeys));
  }

  CollectionKind collection_kind() const { return collection_kind_; }
  IterationKind iteration_kind() const { return iteration_kind_; }

 private:
  CollectionKind const collection_kind_;
  IterationKind const iteration_kind_;
};

bool operator==(CreateCollectionIteratorParameters const&,
                CreateCollectionIteratorParameters const&);
bool operator!=(CreateCollectionIteratorParameters const&,
                CreateCollectionIteratorParameters const&);

size_t hash_value(CreateCollectionIteratorParameters const&);

std::ostream& operator<<(std::ostream&,
                         CreateCollectionIteratorParameters const&);

const CreateCollectionIteratorParameters& CreateCollectionIteratorParametersOf(
    const Operator* op);

// Defines shared information for the bound function that should be created.
// This is used as parameter by JSCreateBoundFunction operators.
class CreateBoundFunctionParameters final {
 public:
  CreateBoundFunctionParameters(size_t arity, Handle<Map> map)
      : arity_(arity), map_(map) {}

  size_t arity() const { return arity_; }
  Handle<Map> map() const { return map_; }

 private:
  size_t const arity_;
  Handle<Map> const map_;
};

bool operator==(CreateBoundFunctionParameters const&,
                CreateBoundFunctionParameters const&);
bool operator!=(CreateBoundFunctionParameters const&,
                CreateBoundFunctionParameters const&);

size_t hash_value(CreateBoundFunctionParameters const&);

std::ostream& operator<<(std::ostream&, CreateBoundFunctionParameters const&);

const CreateBoundFunctionParameters& CreateBoundFunctionParametersOf(
    const Operator* op);

// Defines shared information for the closure that should be created. This is
// used as a parameter by JSCreateClosure operators.
class CreateClosureParameters final {
 public:
  CreateClosureParameters(Handle<SharedFunctionInfo> shared_info,
                          Handle<FeedbackCell> feedback_cell, Handle<Code> code,
                          AllocationType allocation)
      : shared_info_(shared_info),
        feedback_cell_(feedback_cell),
        code_(code),
        allocation_(allocation) {}

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  Handle<FeedbackCell> feedback_cell() const { return feedback_cell_; }
  Handle<Code> code() const { return code_; }
  AllocationType allocation() const { return allocation_; }

 private:
  Handle<SharedFunctionInfo> const shared_info_;
  Handle<FeedbackCell> const feedback_cell_;
  Handle<Code> const code_;
  AllocationType const allocation_;
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
  CreateLiteralParameters(Handle<HeapObject> constant,
                          VectorSlotPair const& feedback, int length, int flags)
      : constant_(constant),
        feedback_(feedback),
        length_(length),
        flags_(flags) {}

  Handle<HeapObject> constant() const { return constant_; }
  VectorSlotPair const& feedback() const { return feedback_; }
  int length() const { return length_; }
  int flags() const { return flags_; }

 private:
  Handle<HeapObject> const constant_;
  VectorSlotPair const feedback_;
  int const length_;
  int const flags_;
};

bool operator==(CreateLiteralParameters const&, CreateLiteralParameters const&);
bool operator!=(CreateLiteralParameters const&, CreateLiteralParameters const&);

size_t hash_value(CreateLiteralParameters const&);

std::ostream& operator<<(std::ostream&, CreateLiteralParameters const&);

const CreateLiteralParameters& CreateLiteralParametersOf(const Operator* op);

class CloneObjectParameters final {
 public:
  CloneObjectParameters(VectorSlotPair const& feedback, int flags)
      : feedback_(feedback), flags_(flags) {}

  VectorSlotPair const& feedback() const { return feedback_; }
  int flags() const { return flags_; }

 private:
  VectorSlotPair const feedback_;
  int const flags_;
};

bool operator==(CloneObjectParameters const&, CloneObjectParameters const&);
bool operator!=(CloneObjectParameters const&, CloneObjectParameters const&);

size_t hash_value(CloneObjectParameters const&);

std::ostream& operator<<(std::ostream&, CloneObjectParameters const&);

const CloneObjectParameters& CloneObjectParametersOf(const Operator* op);

// Descriptor used by the JSForInPrepare and JSForInNext opcodes.
enum class ForInMode : uint8_t {
  kUseEnumCacheKeysAndIndices,
  kUseEnumCacheKeys,
  kGeneric
};

size_t hash_value(ForInMode);

std::ostream& operator<<(std::ostream&, ForInMode);

ForInMode ForInModeOf(Operator const* op) V8_WARN_UNUSED_RESULT;

BinaryOperationHint BinaryOperationHintOf(const Operator* op);

CompareOperationHint CompareOperationHintOf(const Operator* op);

int RegisterCountOf(Operator const* op) V8_WARN_UNUSED_RESULT;

int GeneratorStoreValueCountOf(const Operator* op) V8_WARN_UNUSED_RESULT;
int RestoreRegisterIndexOf(const Operator* op) V8_WARN_UNUSED_RESULT;

Handle<ScopeInfo> ScopeInfoOf(const Operator* op) V8_WARN_UNUSED_RESULT;

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
  const Operator* Exponentiate();

  const Operator* BitwiseNot();
  const Operator* Decrement();
  const Operator* Increment();
  const Operator* Negate();

  const Operator* ToLength();
  const Operator* ToName();
  const Operator* ToNumber();
  const Operator* ToNumberConvertBigInt();
  const Operator* ToNumeric();
  const Operator* ToObject();
  const Operator* ToString();

  const Operator* Create();
  const Operator* CreateArguments(CreateArgumentsType type);
  const Operator* CreateArray(size_t arity, MaybeHandle<AllocationSite> site);
  const Operator* CreateArrayIterator(IterationKind);
  const Operator* CreateAsyncFunctionObject(int register_count);
  const Operator* CreateCollectionIterator(CollectionKind, IterationKind);
  const Operator* CreateBoundFunction(size_t arity, Handle<Map> map);
  const Operator* CreateClosure(
      Handle<SharedFunctionInfo> shared_info,
      Handle<FeedbackCell> feedback_cell, Handle<Code> code,
      AllocationType allocation = AllocationType::kYoung);
  const Operator* CreateIterResultObject();
  const Operator* CreateStringIterator();
  const Operator* CreateKeyValueArray();
  const Operator* CreateObject();
  const Operator* CreatePromise();
  const Operator* CreateTypedArray();
  const Operator* CreateLiteralArray(
      Handle<ArrayBoilerplateDescription> constant,
      VectorSlotPair const& feedback, int literal_flags,
      int number_of_elements);
  const Operator* CreateEmptyLiteralArray(VectorSlotPair const& feedback);
  const Operator* CreateArrayFromIterable();
  const Operator* CreateEmptyLiteralObject();

  const Operator* CreateLiteralObject(
      Handle<ObjectBoilerplateDescription> constant,
      VectorSlotPair const& feedback, int literal_flags,
      int number_of_properties);
  const Operator* CloneObject(VectorSlotPair const& feedback,
                              int literal_flags);
  const Operator* CreateLiteralRegExp(Handle<String> constant_pattern,
                                      VectorSlotPair const& feedback,
                                      int literal_flags);

  const Operator* CallForwardVarargs(size_t arity, uint32_t start_index);
  const Operator* Call(
      size_t arity, CallFrequency const& frequency = CallFrequency(),
      VectorSlotPair const& feedback = VectorSlotPair(),
      ConvertReceiverMode convert_mode = ConvertReceiverMode::kAny,
      SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation);
  const Operator* CallWithArrayLike(CallFrequency frequency);
  const Operator* CallWithSpread(
      uint32_t arity, CallFrequency const& frequency = CallFrequency(),
      VectorSlotPair const& feedback = VectorSlotPair(),
      SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation);
  const Operator* CallRuntime(Runtime::FunctionId id);
  const Operator* CallRuntime(Runtime::FunctionId id, size_t arity);
  const Operator* CallRuntime(const Runtime::Function* function, size_t arity);

  const Operator* ConstructForwardVarargs(size_t arity, uint32_t start_index);
  const Operator* Construct(uint32_t arity,
                            CallFrequency frequency = CallFrequency(),
                            VectorSlotPair const& feedback = VectorSlotPair());
  const Operator* ConstructWithArrayLike(CallFrequency frequency);
  const Operator* ConstructWithSpread(
      uint32_t arity, CallFrequency frequency = CallFrequency(),
      VectorSlotPair const& feedback = VectorSlotPair());

  const Operator* LoadProperty(VectorSlotPair const& feedback);
  const Operator* LoadNamed(Handle<Name> name, VectorSlotPair const& feedback);

  const Operator* StoreProperty(LanguageMode language_mode,
                                VectorSlotPair const& feedback);
  const Operator* StoreNamed(LanguageMode language_mode, Handle<Name> name,
                             VectorSlotPair const& feedback);

  const Operator* StoreNamedOwn(Handle<Name> name,
                                VectorSlotPair const& feedback);
  const Operator* StoreDataPropertyInLiteral(const VectorSlotPair& feedback);
  const Operator* StoreInArrayLiteral(const VectorSlotPair& feedback);

  const Operator* DeleteProperty();

  const Operator* HasProperty(VectorSlotPair const& feedback);

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

  const Operator* HasInPrototypeChain();
  const Operator* InstanceOf(const VectorSlotPair& feedback);
  const Operator* OrdinaryHasInstance();

  const Operator* AsyncFunctionEnter();
  const Operator* AsyncFunctionReject();
  const Operator* AsyncFunctionResolve();

  const Operator* ForInEnumerate();
  const Operator* ForInNext(ForInMode);
  const Operator* ForInPrepare(ForInMode);

  const Operator* LoadMessage();
  const Operator* StoreMessage();

  // Used to implement Ignition's SuspendGenerator bytecode.
  const Operator* GeneratorStore(int value_count);

  // Used to implement Ignition's SwitchOnGeneratorState bytecode.
  const Operator* GeneratorRestoreContinuation();
  const Operator* GeneratorRestoreContext();

  // Used to implement Ignition's ResumeGenerator bytecode.
  const Operator* GeneratorRestoreRegister(int index);
  const Operator* GeneratorRestoreInputOrDebugPos();

  const Operator* StackCheck();
  const Operator* Debugger();

  const Operator* FulfillPromise();
  const Operator* PerformPromiseThen();
  const Operator* PromiseResolve();
  const Operator* RejectPromise();
  const Operator* ResolvePromise();

  const Operator* CreateFunctionContext(Handle<ScopeInfo> scope_info,
                                        int slot_count, ScopeType scope_type);
  const Operator* CreateCatchContext(const Handle<ScopeInfo>& scope_info);
  const Operator* CreateWithContext(const Handle<ScopeInfo>& scope_info);
  const Operator* CreateBlockContext(const Handle<ScopeInfo>& scpope_info);

  const Operator* ObjectIsArray();
  const Operator* ParseInt();
  const Operator* RegExpTest();

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
