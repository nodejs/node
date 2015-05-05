// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_OPERATOR_H_
#define V8_COMPILER_JS_OPERATOR_H_

#include "src/runtime/runtime.h"
#include "src/unique.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Operator;
struct JSOperatorGlobalCache;


// Defines the arity and the call flags for a JavaScript function call. This is
// used as a parameter by JSCallFunction operators.
class CallFunctionParameters FINAL {
 public:
  CallFunctionParameters(size_t arity, CallFunctionFlags flags)
      : arity_(arity), flags_(flags) {}

  size_t arity() const { return arity_; }
  CallFunctionFlags flags() const { return flags_; }

 private:
  const size_t arity_;
  const CallFunctionFlags flags_;
};

bool operator==(CallFunctionParameters const&, CallFunctionParameters const&);
bool operator!=(CallFunctionParameters const&, CallFunctionParameters const&);

size_t hash_value(CallFunctionParameters const&);

std::ostream& operator<<(std::ostream&, CallFunctionParameters const&);

const CallFunctionParameters& CallFunctionParametersOf(const Operator* op);


// Defines the arity and the ID for a runtime function call. This is used as a
// parameter by JSCallRuntime operators.
class CallRuntimeParameters FINAL {
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
class ContextAccess FINAL {
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


class VectorSlotPair {
 public:
  VectorSlotPair(Handle<TypeFeedbackVector> vector, FeedbackVectorICSlot slot)
      : vector_(vector), slot_(slot) {}

  Handle<TypeFeedbackVector> vector() const { return vector_; }
  FeedbackVectorICSlot slot() const { return slot_; }

  int index() const { return vector_->GetIndex(slot_); }

 private:
  const Handle<TypeFeedbackVector> vector_;
  const FeedbackVectorICSlot slot_;
};


bool operator==(VectorSlotPair const& lhs, VectorSlotPair const& rhs);


// Defines the property being loaded from an object by a named load. This is
// used as a parameter by JSLoadNamed operators.
class LoadNamedParameters FINAL {
 public:
  LoadNamedParameters(const Unique<Name>& name, const VectorSlotPair& feedback,
                      ContextualMode contextual_mode)
      : name_(name), contextual_mode_(contextual_mode), feedback_(feedback) {}

  const Unique<Name>& name() const { return name_; }
  ContextualMode contextual_mode() const { return contextual_mode_; }

  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  const Unique<Name> name_;
  const ContextualMode contextual_mode_;
  const VectorSlotPair feedback_;
};

bool operator==(LoadNamedParameters const&, LoadNamedParameters const&);
bool operator!=(LoadNamedParameters const&, LoadNamedParameters const&);

size_t hash_value(LoadNamedParameters const&);

std::ostream& operator<<(std::ostream&, LoadNamedParameters const&);

const LoadNamedParameters& LoadNamedParametersOf(const Operator* op);


// Defines the property being loaded from an object. This is
// used as a parameter by JSLoadProperty operators.
class LoadPropertyParameters FINAL {
 public:
  explicit LoadPropertyParameters(const VectorSlotPair& feedback)
      : feedback_(feedback) {}

  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  const VectorSlotPair feedback_;
};

bool operator==(LoadPropertyParameters const&, LoadPropertyParameters const&);
bool operator!=(LoadPropertyParameters const&, LoadPropertyParameters const&);

size_t hash_value(LoadPropertyParameters const&);

std::ostream& operator<<(std::ostream&, LoadPropertyParameters const&);

const LoadPropertyParameters& LoadPropertyParametersOf(const Operator* op);


// Defines the property being stored to an object by a named store. This is
// used as a parameter by JSStoreNamed operators.
class StoreNamedParameters FINAL {
 public:
  StoreNamedParameters(LanguageMode language_mode, const Unique<Name>& name)
      : language_mode_(language_mode), name_(name) {}

  LanguageMode language_mode() const { return language_mode_; }
  const Unique<Name>& name() const { return name_; }

 private:
  const LanguageMode language_mode_;
  const Unique<Name> name_;
};

bool operator==(StoreNamedParameters const&, StoreNamedParameters const&);
bool operator!=(StoreNamedParameters const&, StoreNamedParameters const&);

size_t hash_value(StoreNamedParameters const&);

std::ostream& operator<<(std::ostream&, StoreNamedParameters const&);

const StoreNamedParameters& StoreNamedParametersOf(const Operator* op);


// Interface for building JavaScript-level operators, e.g. directly from the
// AST. Most operators have no parameters, thus can be globally shared for all
// graphs.
class JSOperatorBuilder FINAL : public ZoneObject {
 public:
  explicit JSOperatorBuilder(Zone* zone);

  const Operator* Equal();
  const Operator* NotEqual();
  const Operator* StrictEqual();
  const Operator* StrictNotEqual();
  const Operator* LessThan();
  const Operator* GreaterThan();
  const Operator* LessThanOrEqual();
  const Operator* GreaterThanOrEqual();
  const Operator* BitwiseOr();
  const Operator* BitwiseXor();
  const Operator* BitwiseAnd();
  const Operator* ShiftLeft();
  const Operator* ShiftRight();
  const Operator* ShiftRightLogical();
  const Operator* Add();
  const Operator* Subtract();
  const Operator* Multiply();
  const Operator* Divide();
  const Operator* Modulus();

  const Operator* UnaryNot();
  const Operator* ToBoolean();
  const Operator* ToNumber();
  const Operator* ToString();
  const Operator* ToName();
  const Operator* ToObject();
  const Operator* Yield();

  const Operator* Create();

  const Operator* CallFunction(size_t arity, CallFunctionFlags flags);
  const Operator* CallRuntime(Runtime::FunctionId id, size_t arity);

  const Operator* CallConstruct(int arguments);

  const Operator* LoadProperty(const VectorSlotPair& feedback);
  const Operator* LoadNamed(const Unique<Name>& name,
                            const VectorSlotPair& feedback,
                            ContextualMode contextual_mode = NOT_CONTEXTUAL);

  const Operator* StoreProperty(LanguageMode language_mode);
  const Operator* StoreNamed(LanguageMode language_mode,
                             const Unique<Name>& name);

  const Operator* DeleteProperty(LanguageMode language_mode);

  const Operator* HasProperty();

  const Operator* LoadContext(size_t depth, size_t index, bool immutable);
  const Operator* StoreContext(size_t depth, size_t index);

  const Operator* TypeOf();
  const Operator* InstanceOf();

  const Operator* StackCheck();

  // TODO(titzer): nail down the static parts of each of these context flavors.
  const Operator* CreateFunctionContext();
  const Operator* CreateCatchContext(const Unique<String>& name);
  const Operator* CreateWithContext();
  const Operator* CreateBlockContext();
  const Operator* CreateModuleContext();
  const Operator* CreateScriptContext();

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
