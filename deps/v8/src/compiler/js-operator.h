// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_OPERATOR_H_
#define V8_COMPILER_JS_OPERATOR_H_

#include "src/base/compiler-specific.h"
#include "src/codegen/tnode.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/globals.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/opcodes.h"
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

namespace wasm {
class ValueType;
}

namespace compiler {

// Forward declarations.
class JSGraph;
class Operator;
struct JSOperatorGlobalCache;

// Macro lists.
#define JS_UNOP_WITH_FEEDBACK(V) \
  JS_BITWISE_UNOP_LIST(V)        \
  JS_ARITH_UNOP_LIST(V)

#define JS_BINOP_WITH_FEEDBACK(V) \
  JS_ARITH_BINOP_LIST(V)          \
  JS_BITWISE_BINOP_LIST(V)        \
  JS_COMPARE_BINOP_LIST(V)        \
  V(JSInstanceOf, InstanceOf)

// Predicates.
class JSOperator final : public AllStatic {
 public:
  static constexpr bool IsUnaryWithFeedback(Operator::Opcode opcode) {
#define CASE(Name, ...)   \
  case IrOpcode::k##Name: \
    return true;
    switch (opcode) {
      JS_UNOP_WITH_FEEDBACK(CASE);
      default:
        return false;
    }
#undef CASE
    return false;
  }

  static constexpr bool IsBinaryWithFeedback(Operator::Opcode opcode) {
#define CASE(Name, ...)   \
  case IrOpcode::k##Name: \
    return true;
    switch (opcode) {
      JS_BINOP_WITH_FEEDBACK(CASE);
      default:
        return false;
    }
#undef CASE
    return false;
  }
};

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

  friend size_t hash_value(CallFrequency const& f) {
    return bit_cast<uint32_t>(f.value_);
  }

  static constexpr float kNoFeedbackCallFrequency = -1;

 private:
  float value_;
};

std::ostream& operator<<(std::ostream&, CallFrequency const&);

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

  using ArityField = base::BitField<size_t, 0, 16>;
  using StartIndexField = base::BitField<uint32_t, 16, 16>;

  uint32_t const bit_field_;
};

std::ostream& operator<<(std::ostream&,
                         ConstructForwardVarargsParameters const&);

ConstructForwardVarargsParameters const& ConstructForwardVarargsParametersOf(
    Operator const*) V8_WARN_UNUSED_RESULT;

// Defines the arity (parameters plus the target and new target) and the
// feedback for a JavaScript constructor call. This is used as a parameter by
// JSConstruct, JSConstructWithArrayLike, and JSConstructWithSpread operators.
class ConstructParameters final {
 public:
  // A separate declaration to get around circular declaration dependencies.
  // Checked to equal JSConstructNode::kExtraInputCount below.
  static constexpr int kExtraConstructInputCount = 3;

  ConstructParameters(uint32_t arity, CallFrequency const& frequency,
                      FeedbackSource const& feedback)
      : arity_(arity), frequency_(frequency), feedback_(feedback) {
    DCHECK_GE(arity, kExtraConstructInputCount);
    DCHECK(is_int32(arity));
  }

  // TODO(jgruber): Consider removing `arity()` and just storing the arity
  // without extra args in ConstructParameters. Every spot that creates
  // ConstructParameters artifically adds the extra args. Every spot that uses
  // ConstructParameters artificially subtracts the extra args.
  // We keep them for now for consistency with other spots
  // that expect `arity()` to include extra args.
  uint32_t arity() const { return arity_; }
  int arity_without_implicit_args() const {
    return static_cast<int>(arity_ - kExtraConstructInputCount);
  }

  CallFrequency const& frequency() const { return frequency_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  uint32_t const arity_;
  CallFrequency const frequency_;
  FeedbackSource const feedback_;
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

  using ArityField = base::BitField<size_t, 0, 15>;
  using StartIndexField = base::BitField<uint32_t, 15, 15>;

  uint32_t const bit_field_;
};

std::ostream& operator<<(std::ostream&, CallForwardVarargsParameters const&);

CallForwardVarargsParameters const& CallForwardVarargsParametersOf(
    Operator const*) V8_WARN_UNUSED_RESULT;

// Defines the arity (parameters plus the target and receiver) and the call
// flags for a JavaScript function call. This is used as a parameter by JSCall,
// JSCallWithArrayLike and JSCallWithSpread operators.
class CallParameters final {
 public:
  // A separate declaration to get around circular declaration dependencies.
  // Checked to equal JSCallNode::kExtraInputCount below.
  static constexpr int kExtraCallInputCount = 3;

  CallParameters(size_t arity, CallFrequency const& frequency,
                 FeedbackSource const& feedback,
                 ConvertReceiverMode convert_mode,
                 SpeculationMode speculation_mode,
                 CallFeedbackRelation feedback_relation)
      : bit_field_(ArityField::encode(arity) |
                   CallFeedbackRelationField::encode(feedback_relation) |
                   SpeculationModeField::encode(speculation_mode) |
                   ConvertReceiverModeField::encode(convert_mode)),
        frequency_(frequency),
        feedback_(feedback) {
    // CallFeedbackRelation is ignored if the feedback slot is invalid.
    DCHECK_IMPLIES(speculation_mode == SpeculationMode::kAllowSpeculation,
                   feedback.IsValid());
    DCHECK_IMPLIES(!feedback.IsValid(),
                   feedback_relation == CallFeedbackRelation::kUnrelated);
    DCHECK_GE(arity, kExtraCallInputCount);
    DCHECK(is_int32(arity));
  }

  // TODO(jgruber): Consider removing `arity()` and just storing the arity
  // without extra args in CallParameters.
  size_t arity() const { return ArityField::decode(bit_field_); }
  int arity_without_implicit_args() const {
    return static_cast<int>(arity() - kExtraCallInputCount);
  }

  CallFrequency const& frequency() const { return frequency_; }
  ConvertReceiverMode convert_mode() const {
    return ConvertReceiverModeField::decode(bit_field_);
  }
  FeedbackSource const& feedback() const { return feedback_; }

  SpeculationMode speculation_mode() const {
    return SpeculationModeField::decode(bit_field_);
  }

  CallFeedbackRelation feedback_relation() const {
    return CallFeedbackRelationField::decode(bit_field_);
  }

  bool operator==(CallParameters const& that) const {
    return this->bit_field_ == that.bit_field_ &&
           this->frequency_ == that.frequency_ &&
           this->feedback_ == that.feedback_;
  }
  bool operator!=(CallParameters const& that) const { return !(*this == that); }

 private:
  friend size_t hash_value(CallParameters const& p) {
    FeedbackSource::Hash feedback_hash;
    return base::hash_combine(p.bit_field_, p.frequency_,
                              feedback_hash(p.feedback_));
  }

  using ArityField = base::BitField<size_t, 0, 27>;
  using CallFeedbackRelationField = base::BitField<CallFeedbackRelation, 27, 1>;
  using SpeculationModeField = base::BitField<SpeculationMode, 28, 1>;
  using ConvertReceiverModeField = base::BitField<ConvertReceiverMode, 29, 2>;

  uint32_t const bit_field_;
  CallFrequency const frequency_;
  FeedbackSource const feedback_;
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
  StoreNamedOwnParameters(Handle<Name> name, FeedbackSource const& feedback)
      : name_(name), feedback_(feedback) {}

  Handle<Name> name() const { return name_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  Handle<Name> const name_;
  FeedbackSource const feedback_;
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
  explicit FeedbackParameter(FeedbackSource const& feedback)
      : feedback_(feedback) {}

  FeedbackSource const& feedback() const { return feedback_; }

 private:
  FeedbackSource const feedback_;
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
              FeedbackSource const& feedback)
      : name_(name), feedback_(feedback), language_mode_(language_mode) {}

  Handle<Name> name() const { return name_; }
  LanguageMode language_mode() const { return language_mode_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  Handle<Name> const name_;
  FeedbackSource const feedback_;
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
  LoadGlobalParameters(const Handle<Name>& name, const FeedbackSource& feedback,
                       TypeofMode typeof_mode)
      : name_(name), feedback_(feedback), typeof_mode_(typeof_mode) {}

  const Handle<Name>& name() const { return name_; }
  TypeofMode typeof_mode() const { return typeof_mode_; }

  const FeedbackSource& feedback() const { return feedback_; }

 private:
  const Handle<Name> name_;
  const FeedbackSource feedback_;
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
                        const FeedbackSource& feedback,
                        const Handle<Name>& name)
      : language_mode_(language_mode), name_(name), feedback_(feedback) {}

  LanguageMode language_mode() const { return language_mode_; }
  FeedbackSource const& feedback() const { return feedback_; }
  Handle<Name> const& name() const { return name_; }

 private:
  LanguageMode const language_mode_;
  Handle<Name> const name_;
  FeedbackSource const feedback_;
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
  PropertyAccess(LanguageMode language_mode, FeedbackSource const& feedback)
      : feedback_(feedback), language_mode_(language_mode) {}

  LanguageMode language_mode() const { return language_mode_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  FeedbackSource const feedback_;
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
                          Handle<Code> code, AllocationType allocation)
      : shared_info_(shared_info), code_(code), allocation_(allocation) {}

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  Handle<Code> code() const { return code_; }
  AllocationType allocation() const { return allocation_; }

 private:
  Handle<SharedFunctionInfo> const shared_info_;
  Handle<Code> const code_;
  AllocationType const allocation_;
};

bool operator==(CreateClosureParameters const&, CreateClosureParameters const&);
bool operator!=(CreateClosureParameters const&, CreateClosureParameters const&);

size_t hash_value(CreateClosureParameters const&);

std::ostream& operator<<(std::ostream&, CreateClosureParameters const&);

const CreateClosureParameters& CreateClosureParametersOf(const Operator* op);

class GetTemplateObjectParameters final {
 public:
  GetTemplateObjectParameters(Handle<TemplateObjectDescription> description,
                              Handle<SharedFunctionInfo> shared,
                              FeedbackSource const& feedback)
      : description_(description), shared_(shared), feedback_(feedback) {}

  Handle<TemplateObjectDescription> description() const { return description_; }
  Handle<SharedFunctionInfo> shared() const { return shared_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  Handle<TemplateObjectDescription> const description_;
  Handle<SharedFunctionInfo> const shared_;
  FeedbackSource const feedback_;
};

bool operator==(GetTemplateObjectParameters const&,
                GetTemplateObjectParameters const&);
bool operator!=(GetTemplateObjectParameters const&,
                GetTemplateObjectParameters const&);

size_t hash_value(GetTemplateObjectParameters const&);

std::ostream& operator<<(std::ostream&, GetTemplateObjectParameters const&);

const GetTemplateObjectParameters& GetTemplateObjectParametersOf(
    const Operator* op);

// Defines shared information for the literal that should be created. This is
// used as parameter by JSCreateLiteralArray, JSCreateLiteralObject and
// JSCreateLiteralRegExp operators.
class CreateLiteralParameters final {
 public:
  CreateLiteralParameters(Handle<HeapObject> constant,
                          FeedbackSource const& feedback, int length, int flags)
      : constant_(constant),
        feedback_(feedback),
        length_(length),
        flags_(flags) {}

  Handle<HeapObject> constant() const { return constant_; }
  FeedbackSource const& feedback() const { return feedback_; }
  int length() const { return length_; }
  int flags() const { return flags_; }

 private:
  Handle<HeapObject> const constant_;
  FeedbackSource const feedback_;
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
  CloneObjectParameters(FeedbackSource const& feedback, int flags)
      : feedback_(feedback), flags_(flags) {}

  FeedbackSource const& feedback() const { return feedback_; }
  int flags() const { return flags_; }

 private:
  FeedbackSource const feedback_;
  int const flags_;
};

bool operator==(CloneObjectParameters const&, CloneObjectParameters const&);
bool operator!=(CloneObjectParameters const&, CloneObjectParameters const&);

size_t hash_value(CloneObjectParameters const&);

std::ostream& operator<<(std::ostream&, CloneObjectParameters const&);

const CloneObjectParameters& CloneObjectParametersOf(const Operator* op);

// Defines the shared information for the iterator symbol thats loaded and
// called. This is used as a parameter by JSGetIterator operator.
class GetIteratorParameters final {
 public:
  GetIteratorParameters(const FeedbackSource& load_feedback,
                        const FeedbackSource& call_feedback)
      : load_feedback_(load_feedback), call_feedback_(call_feedback) {}

  FeedbackSource const& loadFeedback() const { return load_feedback_; }
  FeedbackSource const& callFeedback() const { return call_feedback_; }

 private:
  FeedbackSource const load_feedback_;
  FeedbackSource const call_feedback_;
};

bool operator==(GetIteratorParameters const&, GetIteratorParameters const&);
bool operator!=(GetIteratorParameters const&, GetIteratorParameters const&);

size_t hash_value(GetIteratorParameters const&);

std::ostream& operator<<(std::ostream&, GetIteratorParameters const&);

const GetIteratorParameters& GetIteratorParametersOf(const Operator* op);

enum class ForInMode : uint8_t {
  kUseEnumCacheKeysAndIndices,
  kUseEnumCacheKeys,
  kGeneric
};
size_t hash_value(ForInMode const&);
std::ostream& operator<<(std::ostream&, ForInMode const&);

class ForInParameters final {
 public:
  ForInParameters(const FeedbackSource& feedback, ForInMode mode)
      : feedback_(feedback), mode_(mode) {}

  const FeedbackSource& feedback() const { return feedback_; }
  ForInMode mode() const { return mode_; }

 private:
  const FeedbackSource feedback_;
  const ForInMode mode_;
};

bool operator==(ForInParameters const&, ForInParameters const&);
bool operator!=(ForInParameters const&, ForInParameters const&);
size_t hash_value(ForInParameters const&);
std::ostream& operator<<(std::ostream&, ForInParameters const&);
const ForInParameters& ForInParametersOf(const Operator* op);

#if V8_ENABLE_WEBASSEMBLY
class JSWasmCallParameters {
 public:
  explicit JSWasmCallParameters(const wasm::WasmModule* module,
                                const wasm::FunctionSig* signature,
                                FeedbackSource const& feedback)
      : module_(module), signature_(signature), feedback_(feedback) {
    DCHECK_NOT_NULL(module);
    DCHECK_NOT_NULL(signature);
  }

  const wasm::WasmModule* module() const { return module_; }
  const wasm::FunctionSig* signature() const { return signature_; }
  FeedbackSource const& feedback() const { return feedback_; }
  int input_count() const;
  int arity_without_implicit_args() const;

 private:
  const wasm::WasmModule* const module_;
  const wasm::FunctionSig* const signature_;
  const FeedbackSource feedback_;
};

JSWasmCallParameters const& JSWasmCallParametersOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&,
                                           JSWasmCallParameters const&);
size_t hash_value(JSWasmCallParameters const&);
bool operator==(JSWasmCallParameters const&, JSWasmCallParameters const&);
#endif  // V8_ENABLE_WEBASSEMBLY

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
  JSOperatorBuilder(const JSOperatorBuilder&) = delete;
  JSOperatorBuilder& operator=(const JSOperatorBuilder&) = delete;

  const Operator* Equal(FeedbackSource const& feedback);
  const Operator* StrictEqual(FeedbackSource const& feedback);
  const Operator* LessThan(FeedbackSource const& feedback);
  const Operator* GreaterThan(FeedbackSource const& feedback);
  const Operator* LessThanOrEqual(FeedbackSource const& feedback);
  const Operator* GreaterThanOrEqual(FeedbackSource const& feedback);

  const Operator* BitwiseOr(FeedbackSource const& feedback);
  const Operator* BitwiseXor(FeedbackSource const& feedback);
  const Operator* BitwiseAnd(FeedbackSource const& feedback);
  const Operator* ShiftLeft(FeedbackSource const& feedback);
  const Operator* ShiftRight(FeedbackSource const& feedback);
  const Operator* ShiftRightLogical(FeedbackSource const& feedback);
  const Operator* Add(FeedbackSource const& feedback);
  const Operator* Subtract(FeedbackSource const& feedback);
  const Operator* Multiply(FeedbackSource const& feedback);
  const Operator* Divide(FeedbackSource const& feedback);
  const Operator* Modulus(FeedbackSource const& feedback);
  const Operator* Exponentiate(FeedbackSource const& feedback);

  const Operator* BitwiseNot(FeedbackSource const& feedback);
  const Operator* Decrement(FeedbackSource const& feedback);
  const Operator* Increment(FeedbackSource const& feedback);
  const Operator* Negate(FeedbackSource const& feedback);

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
      Handle<SharedFunctionInfo> shared_info, Handle<Code> code,
      AllocationType allocation = AllocationType::kYoung);
  const Operator* CreateIterResultObject();
  const Operator* CreateStringIterator();
  const Operator* CreateKeyValueArray();
  const Operator* CreateObject();
  const Operator* CreatePromise();
  const Operator* CreateTypedArray();
  const Operator* CreateLiteralArray(
      Handle<ArrayBoilerplateDescription> constant,
      FeedbackSource const& feedback, int literal_flags,
      int number_of_elements);
  const Operator* CreateEmptyLiteralArray(FeedbackSource const& feedback);
  const Operator* CreateArrayFromIterable();
  const Operator* CreateEmptyLiteralObject();
  const Operator* CreateLiteralObject(
      Handle<ObjectBoilerplateDescription> constant,
      FeedbackSource const& feedback, int literal_flags,
      int number_of_properties);
  const Operator* CloneObject(FeedbackSource const& feedback,
                              int literal_flags);
  const Operator* CreateLiteralRegExp(Handle<String> constant_pattern,
                                      FeedbackSource const& feedback,
                                      int literal_flags);

  const Operator* GetTemplateObject(
      Handle<TemplateObjectDescription> description,
      Handle<SharedFunctionInfo> shared, FeedbackSource const& feedback);

  const Operator* CallForwardVarargs(size_t arity, uint32_t start_index);
  const Operator* Call(
      size_t arity, CallFrequency const& frequency = CallFrequency(),
      FeedbackSource const& feedback = FeedbackSource(),
      ConvertReceiverMode convert_mode = ConvertReceiverMode::kAny,
      SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation,
      CallFeedbackRelation feedback_relation =
          CallFeedbackRelation::kUnrelated);
  const Operator* CallWithArrayLike(
      CallFrequency const& frequency,
      const FeedbackSource& feedback = FeedbackSource{},
      SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation,
      CallFeedbackRelation feedback_relation = CallFeedbackRelation::kRelated);
  const Operator* CallWithSpread(
      uint32_t arity, CallFrequency const& frequency = CallFrequency(),
      FeedbackSource const& feedback = FeedbackSource(),
      SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation,
      CallFeedbackRelation feedback_relation = CallFeedbackRelation::kRelated);
  const Operator* CallRuntime(Runtime::FunctionId id);
  const Operator* CallRuntime(Runtime::FunctionId id, size_t arity);
  const Operator* CallRuntime(const Runtime::Function* function, size_t arity);

#if V8_ENABLE_WEBASSEMBLY
  const Operator* CallWasm(const wasm::WasmModule* wasm_module,
                           const wasm::FunctionSig* wasm_signature,
                           FeedbackSource const& feedback);
#endif  // V8_ENABLE_WEBASSEMBLY

  const Operator* ConstructForwardVarargs(size_t arity, uint32_t start_index);
  const Operator* Construct(uint32_t arity,
                            CallFrequency const& frequency = CallFrequency(),
                            FeedbackSource const& feedback = FeedbackSource());
  const Operator* ConstructWithArrayLike(CallFrequency const& frequency,
                                         FeedbackSource const& feedback);
  const Operator* ConstructWithSpread(
      uint32_t arity, CallFrequency const& frequency = CallFrequency(),
      FeedbackSource const& feedback = FeedbackSource());

  const Operator* LoadProperty(FeedbackSource const& feedback);
  const Operator* LoadNamed(Handle<Name> name, FeedbackSource const& feedback);
  const Operator* LoadNamedFromSuper(Handle<Name> name,
                                     FeedbackSource const& feedback);

  const Operator* StoreProperty(LanguageMode language_mode,
                                FeedbackSource const& feedback);
  const Operator* StoreNamed(LanguageMode language_mode, Handle<Name> name,
                             FeedbackSource const& feedback);

  const Operator* StoreNamedOwn(Handle<Name> name,
                                FeedbackSource const& feedback);
  const Operator* StoreDataPropertyInLiteral(const FeedbackSource& feedback);
  const Operator* StoreInArrayLiteral(const FeedbackSource& feedback);

  const Operator* DeleteProperty();

  const Operator* HasProperty(FeedbackSource const& feedback);

  const Operator* GetSuperConstructor();

  const Operator* CreateGeneratorObject();

  const Operator* LoadGlobal(const Handle<Name>& name,
                             const FeedbackSource& feedback,
                             TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  const Operator* StoreGlobal(LanguageMode language_mode,
                              const Handle<Name>& name,
                              const FeedbackSource& feedback);

  const Operator* HasContextExtension(size_t depth);
  const Operator* LoadContext(size_t depth, size_t index, bool immutable);
  const Operator* StoreContext(size_t depth, size_t index);

  const Operator* LoadModule(int32_t cell_index);
  const Operator* StoreModule(int32_t cell_index);

  const Operator* GetImportMeta();

  const Operator* HasInPrototypeChain();
  const Operator* InstanceOf(const FeedbackSource& feedback);
  const Operator* OrdinaryHasInstance();

  const Operator* AsyncFunctionEnter();
  const Operator* AsyncFunctionReject();
  const Operator* AsyncFunctionResolve();

  const Operator* ForInEnumerate();
  const Operator* ForInNext(ForInMode mode, const FeedbackSource& feedback);
  const Operator* ForInPrepare(ForInMode mode, const FeedbackSource& feedback);

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

  const Operator* StackCheck(StackCheckKind kind);
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

  const Operator* GetIterator(FeedbackSource const& load_feedback,
                              FeedbackSource const& call_feedback);

 private:
  Zone* zone() const { return zone_; }

  const JSOperatorGlobalCache& cache_;
  Zone* const zone_;
};

// Node wrappers.

class JSNodeWrapperBase : public NodeWrapper {
 public:
  explicit constexpr JSNodeWrapperBase(Node* node) : NodeWrapper(node) {}

  // Valid iff this node has a context input.
  TNode<Object> context() const {
    // Could be a Context or NoContextConstant.
    return TNode<Object>::UncheckedCast(
        NodeProperties::GetContextInput(node()));
  }

  // Valid iff this node has exactly one effect input.
  Effect effect() const {
    DCHECK_EQ(node()->op()->EffectInputCount(), 1);
    return Effect{NodeProperties::GetEffectInput(node())};
  }

  // Valid iff this node has exactly one control input.
  Control control() const {
    DCHECK_EQ(node()->op()->ControlInputCount(), 1);
    return Control{NodeProperties::GetControlInput(node())};
  }

  // Valid iff this node has a frame state input.
  FrameState frame_state() const {
    return FrameState{NodeProperties::GetFrameStateInput(node())};
  }
};

#define DEFINE_INPUT_ACCESSORS(Name, name, TheIndex, Type) \
  static constexpr int Name##Index() { return TheIndex; }  \
  TNode<Type> name() const {                               \
    return TNode<Type>::UncheckedCast(                     \
        NodeProperties::GetValueInput(node(), TheIndex));  \
  }

class JSUnaryOpNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSUnaryOpNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(JSOperator::IsUnaryWithFeedback(node->opcode()));
  }

#define INPUTS(V)            \
  V(Value, value, 0, Object) \
  V(FeedbackVector, feedback_vector, 1, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

#define V(JSName, ...) using JSName##Node = JSUnaryOpNode;
JS_UNOP_WITH_FEEDBACK(V)
#undef V

class JSBinaryOpNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSBinaryOpNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(JSOperator::IsBinaryWithFeedback(node->opcode()));
  }

  const FeedbackParameter& Parameters() const {
    return FeedbackParameterOf(node()->op());
  }

#define INPUTS(V)            \
  V(Left, left, 0, Object)   \
  V(Right, right, 1, Object) \
  V(FeedbackVector, feedback_vector, 2, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

#define V(JSName, ...) using JSName##Node = JSBinaryOpNode;
JS_BINOP_WITH_FEEDBACK(V)
#undef V

class JSGetIteratorNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSGetIteratorNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSGetIterator);
  }

  const GetIteratorParameters& Parameters() const {
    return GetIteratorParametersOf(node()->op());
  }

#define INPUTS(V)                  \
  V(Receiver, receiver, 0, Object) \
  V(FeedbackVector, feedback_vector, 1, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSCloneObjectNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSCloneObjectNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSCloneObject);
  }

  const CloneObjectParameters& Parameters() const {
    return CloneObjectParametersOf(node()->op());
  }

#define INPUTS(V)              \
  V(Source, source, 0, Object) \
  V(FeedbackVector, feedback_vector, 1, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSGetTemplateObjectNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSGetTemplateObjectNode(Node* node)
      : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSGetTemplateObject);
  }

  const GetTemplateObjectParameters& Parameters() const {
    return GetTemplateObjectParametersOf(node()->op());
  }

#define INPUTS(V) V(FeedbackVector, feedback_vector, 0, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSCreateLiteralOpNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSCreateLiteralOpNode(Node* node)
      : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSCreateLiteralArray ||
                     node->opcode() == IrOpcode::kJSCreateLiteralObject ||
                     node->opcode() == IrOpcode::kJSCreateLiteralRegExp);
  }

  const CreateLiteralParameters& Parameters() const {
    return CreateLiteralParametersOf(node()->op());
  }

#define INPUTS(V) V(FeedbackVector, feedback_vector, 0, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

using JSCreateLiteralArrayNode = JSCreateLiteralOpNode;
using JSCreateLiteralObjectNode = JSCreateLiteralOpNode;
using JSCreateLiteralRegExpNode = JSCreateLiteralOpNode;

class JSHasPropertyNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSHasPropertyNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSHasProperty);
  }

  const PropertyAccess& Parameters() const {
    return PropertyAccessOf(node()->op());
  }

#define INPUTS(V)              \
  V(Object, object, 0, Object) \
  V(Key, key, 1, Object)       \
  V(FeedbackVector, feedback_vector, 2, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSLoadPropertyNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSLoadPropertyNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSLoadProperty);
  }

  const PropertyAccess& Parameters() const {
    return PropertyAccessOf(node()->op());
  }

#define INPUTS(V)              \
  V(Object, object, 0, Object) \
  V(Key, key, 1, Object)       \
  V(FeedbackVector, feedback_vector, 2, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSStorePropertyNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSStorePropertyNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSStoreProperty);
  }

  const PropertyAccess& Parameters() const {
    return PropertyAccessOf(node()->op());
  }

#define INPUTS(V)              \
  V(Object, object, 0, Object) \
  V(Key, key, 1, Object)       \
  V(Value, value, 2, Object)   \
  V(FeedbackVector, feedback_vector, 3, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

namespace js_node_wrapper_utils {
// Avoids template definitions in the .cc file.
TNode<Oddball> UndefinedConstant(JSGraph* jsgraph);
}  // namespace js_node_wrapper_utils

class JSCallOrConstructNode : public JSNodeWrapperBase {
 public:
  explicit constexpr JSCallOrConstructNode(Node* node)
      : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSCall ||
                     node->opcode() == IrOpcode::kJSCallWithArrayLike ||
                     node->opcode() == IrOpcode::kJSCallWithSpread ||
                     node->opcode() == IrOpcode::kJSConstruct ||
                     node->opcode() == IrOpcode::kJSConstructWithArrayLike ||
                     node->opcode() == IrOpcode::kJSConstructWithSpread
#if V8_ENABLE_WEBASSEMBLY
                     || node->opcode() == IrOpcode::kJSWasmCall
#endif  // V8_ENABLE_WEBASSEMBLY
    );  // NOLINT(whitespace/parens)
  }

#define INPUTS(V)              \
  V(Target, target, 0, Object) \
  V(ReceiverOrNewTarget, receiver_or_new_target, 1, Object)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS

  // Besides actual arguments, JSCall nodes (and variants) also take the
  // following. Note that we rely on the fact that all variants (JSCall,
  // JSCallWithArrayLike, JSCallWithSpread, JSConstruct,
  // JSConstructWithArrayLike, JSConstructWithSpread, JSWasmCall) have the same
  // underlying node layout.
  static constexpr int kTargetInputCount = 1;
  static constexpr int kReceiverOrNewTargetInputCount = 1;
  static constexpr int kFeedbackVectorInputCount = 1;
  static constexpr int kExtraInputCount = kTargetInputCount +
                                          kReceiverOrNewTargetInputCount +
                                          kFeedbackVectorInputCount;
  STATIC_ASSERT(kExtraInputCount == CallParameters::kExtraCallInputCount);
  STATIC_ASSERT(kExtraInputCount ==
                ConstructParameters::kExtraConstructInputCount);

  // Just for static asserts for spots that rely on node layout.
  static constexpr bool kFeedbackVectorIsLastInput = true;

  // Some spots rely on the fact that call and construct variants have the same
  // layout.
  static constexpr bool kHaveIdenticalLayouts = true;

  // This is the arity fed into Call/ConstructArguments.
  static constexpr int ArityForArgc(int parameters) {
    return parameters + kExtraInputCount;
  }

  static constexpr int FirstArgumentIndex() {
    return ReceiverOrNewTargetIndex() + 1;
  }
  static constexpr int ArgumentIndex(int i) { return FirstArgumentIndex() + i; }

  TNode<Object> Argument(int i) const {
    DCHECK_LT(i, ArgumentCount());
    return TNode<Object>::UncheckedCast(
        NodeProperties::GetValueInput(node(), ArgumentIndex(i)));
  }
  int LastArgumentIndex() const {
    DCHECK_GT(ArgumentCount(), 0);
    return ArgumentIndex(ArgumentCount() - 1);
  }
  TNode<Object> LastArgument() const {
    DCHECK_GT(ArgumentCount(), 0);
    return Argument(ArgumentCount() - 1);
  }
  TNode<Object> ArgumentOr(int i, Node* default_value) const {
    return i < ArgumentCount() ? Argument(i)
                               : TNode<Object>::UncheckedCast(default_value);
  }
  TNode<Object> ArgumentOrUndefined(int i, JSGraph* jsgraph) const {
    return ArgumentOr(i, js_node_wrapper_utils::UndefinedConstant(jsgraph));
  }
  virtual int ArgumentCount() const = 0;

  static constexpr int FeedbackVectorIndexForArgc(int argc) {
    STATIC_ASSERT(kFeedbackVectorIsLastInput);
    return ArgumentIndex(argc - 1) + 1;
  }
  int FeedbackVectorIndex() const {
    return FeedbackVectorIndexForArgc(ArgumentCount());
  }
  TNode<HeapObject> feedback_vector() const {
    return TNode<HeapObject>::UncheckedCast(
        NodeProperties::GetValueInput(node(), FeedbackVectorIndex()));
  }
};

template <int kOpcode>
class JSCallNodeBase final : public JSCallOrConstructNode {
 public:
  explicit constexpr JSCallNodeBase(Node* node) : JSCallOrConstructNode(node) {
    CONSTEXPR_DCHECK(node->opcode() == kOpcode);
  }

  const CallParameters& Parameters() const {
    return CallParametersOf(node()->op());
  }

#define INPUTS(V)              \
  V(Target, target, 0, Object) \
  V(Receiver, receiver, 1, Object)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS

  static constexpr int kReceiverInputCount = 1;
  STATIC_ASSERT(kReceiverInputCount ==
                JSCallOrConstructNode::kReceiverOrNewTargetInputCount);

  int ArgumentCount() const override {
    // Note: The count reported by this function depends only on the parameter,
    // thus adding/removing inputs will not affect it.
    return Parameters().arity_without_implicit_args();
  }
};

using JSCallNode = JSCallNodeBase<IrOpcode::kJSCall>;
using JSCallWithSpreadNode = JSCallNodeBase<IrOpcode::kJSCallWithSpread>;
using JSCallWithArrayLikeNode = JSCallNodeBase<IrOpcode::kJSCallWithArrayLike>;

#if V8_ENABLE_WEBASSEMBLY
class JSWasmCallNode final : public JSCallOrConstructNode {
 public:
  explicit constexpr JSWasmCallNode(Node* node) : JSCallOrConstructNode(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSWasmCall);
  }

  const JSWasmCallParameters& Parameters() const {
    return OpParameter<JSWasmCallParameters>(node()->op());
  }

#define INPUTS(V)              \
  V(Target, target, 0, Object) \
  V(Receiver, receiver, 1, Object)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS

  static constexpr int kReceiverInputCount = 1;
  STATIC_ASSERT(kReceiverInputCount ==
                JSCallOrConstructNode::kReceiverOrNewTargetInputCount);

  int ArgumentCount() const override {
    // Note: The count reported by this function depends only on the parameter
    // count, thus adding/removing inputs will not affect it.
    return Parameters().arity_without_implicit_args();
  }

  static Type TypeForWasmReturnType(const wasm::ValueType& type);
};
#endif  // V8_ENABLE_WEBASSEMBLY

template <int kOpcode>
class JSConstructNodeBase final : public JSCallOrConstructNode {
 public:
  explicit constexpr JSConstructNodeBase(Node* node)
      : JSCallOrConstructNode(node) {
    CONSTEXPR_DCHECK(node->opcode() == kOpcode);
  }

  const ConstructParameters& Parameters() const {
    return ConstructParametersOf(node()->op());
  }

#define INPUTS(V)              \
  V(Target, target, 0, Object) \
  V(NewTarget, new_target, 1, Object)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS

  static constexpr int kNewTargetInputCount = 1;
  STATIC_ASSERT(kNewTargetInputCount ==
                JSCallOrConstructNode::kReceiverOrNewTargetInputCount);

  int ArgumentCount() const {
    // Note: The count reported by this function depends only on the parameter,
    // thus adding/removing inputs will not affect it.
    return Parameters().arity_without_implicit_args();
  }
};

using JSConstructNode = JSConstructNodeBase<IrOpcode::kJSConstruct>;
using JSConstructWithSpreadNode =
    JSConstructNodeBase<IrOpcode::kJSConstructWithSpread>;
using JSConstructWithArrayLikeNode =
    JSConstructNodeBase<IrOpcode::kJSConstructWithArrayLike>;

class JSLoadNamedNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSLoadNamedNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSLoadNamed);
  }

  const NamedAccess& Parameters() const { return NamedAccessOf(node()->op()); }

#define INPUTS(V)              \
  V(Object, object, 0, Object) \
  V(FeedbackVector, feedback_vector, 1, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSLoadNamedFromSuperNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSLoadNamedFromSuperNode(Node* node)
      : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSLoadNamedFromSuper);
  }

  const NamedAccess& Parameters() const { return NamedAccessOf(node()->op()); }

#define INPUTS(V)                       \
  V(Receiver, receiver, 0, Object)      \
  V(HomeObject, home_object, 1, Object) \
  V(FeedbackVector, feedback_vector, 2, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSStoreNamedNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSStoreNamedNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSStoreNamed);
  }

  const NamedAccess& Parameters() const { return NamedAccessOf(node()->op()); }

#define INPUTS(V)              \
  V(Object, object, 0, Object) \
  V(Value, value, 1, Object)   \
  V(FeedbackVector, feedback_vector, 2, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSStoreNamedOwnNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSStoreNamedOwnNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSStoreNamedOwn);
  }

  const StoreNamedOwnParameters& Parameters() const {
    return StoreNamedOwnParametersOf(node()->op());
  }

#define INPUTS(V)              \
  V(Object, object, 0, Object) \
  V(Value, value, 1, Object)   \
  V(FeedbackVector, feedback_vector, 2, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSStoreGlobalNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSStoreGlobalNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSStoreGlobal);
  }

  const StoreGlobalParameters& Parameters() const {
    return StoreGlobalParametersOf(node()->op());
  }

#define INPUTS(V)            \
  V(Value, value, 0, Object) \
  V(FeedbackVector, feedback_vector, 1, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSLoadGlobalNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSLoadGlobalNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSLoadGlobal);
  }

  const LoadGlobalParameters& Parameters() const {
    return LoadGlobalParametersOf(node()->op());
  }

#define INPUTS(V) V(FeedbackVector, feedback_vector, 0, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSCreateEmptyLiteralArrayNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSCreateEmptyLiteralArrayNode(Node* node)
      : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSCreateEmptyLiteralArray);
  }

  const FeedbackParameter& Parameters() const {
    return FeedbackParameterOf(node()->op());
  }

#define INPUTS(V) V(FeedbackVector, feedback_vector, 0, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSStoreDataPropertyInLiteralNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSStoreDataPropertyInLiteralNode(Node* node)
      : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSStoreDataPropertyInLiteral);
  }

  const FeedbackParameter& Parameters() const {
    return FeedbackParameterOf(node()->op());
  }

#define INPUTS(V)              \
  V(Object, object, 0, Object) \
  V(Name, name, 1, Object)     \
  V(Value, value, 2, Object)   \
  V(Flags, flags, 3, Object)   \
  V(FeedbackVector, feedback_vector, 4, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSStoreInArrayLiteralNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSStoreInArrayLiteralNode(Node* node)
      : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSStoreInArrayLiteral);
  }

  const FeedbackParameter& Parameters() const {
    return FeedbackParameterOf(node()->op());
  }

#define INPUTS(V)            \
  V(Array, array, 0, Object) \
  V(Index, index, 1, Object) \
  V(Value, value, 2, Object) \
  V(FeedbackVector, feedback_vector, 3, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSCreateClosureNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSCreateClosureNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSCreateClosure);
  }

  const CreateClosureParameters& Parameters() const {
    return CreateClosureParametersOf(node()->op());
  }

#define INPUTS(V) V(FeedbackCell, feedback_cell, 0, FeedbackCell)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS

  FeedbackCellRef GetFeedbackCellRefChecked(JSHeapBroker* broker) const;
};

class JSForInPrepareNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSForInPrepareNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSForInPrepare);
  }

  const ForInParameters& Parameters() const {
    return ForInParametersOf(node()->op());
  }

#define INPUTS(V)                      \
  V(Enumerator, enumerator, 0, Object) \
  V(FeedbackVector, feedback_vector, 1, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

class JSForInNextNode final : public JSNodeWrapperBase {
 public:
  explicit constexpr JSForInNextNode(Node* node) : JSNodeWrapperBase(node) {
    CONSTEXPR_DCHECK(node->opcode() == IrOpcode::kJSForInNext);
  }

  const ForInParameters& Parameters() const {
    return ForInParametersOf(node()->op());
  }

#define INPUTS(V)                       \
  V(Receiver, receiver, 0, Object)      \
  V(CacheArray, cache_array, 1, Object) \
  V(CacheType, cache_type, 2, Object)   \
  V(Index, index, 3, Smi)               \
  V(FeedbackVector, feedback_vector, 4, HeapObject)
  INPUTS(DEFINE_INPUT_ACCESSORS)
#undef INPUTS
};

#undef DEFINE_INPUT_ACCESSORS

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_OPERATOR_H_
