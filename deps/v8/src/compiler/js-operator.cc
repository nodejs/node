// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-operator.h"

#include <limits>

#include "src/base/lazy-instance.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"

namespace v8 {
namespace internal {
namespace compiler {

std::ostream& operator<<(std::ostream& os, CallFunctionParameters const& p) {
  return os << p.arity() << ", " << p.flags() << ", " << p.language_mode();
}


const CallFunctionParameters& CallFunctionParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, op->opcode());
  return OpParameter<CallFunctionParameters>(op);
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


bool operator==(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return lhs.slot().ToInt() == rhs.slot().ToInt() &&
         lhs.vector().is_identical_to(rhs.vector());
}


size_t hash_value(VectorSlotPair const& p) {
  // TODO(mvstanton): include the vector in the hash.
  base::hash<int> h;
  return h(p.slot().ToInt());
}


bool operator==(LoadNamedParameters const& lhs,
                LoadNamedParameters const& rhs) {
  return lhs.name() == rhs.name() &&
         lhs.contextual_mode() == rhs.contextual_mode() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(LoadNamedParameters const& lhs,
                LoadNamedParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(LoadNamedParameters const& p) {
  return base::hash_combine(p.name(), p.contextual_mode(), p.feedback());
}


std::ostream& operator<<(std::ostream& os, LoadNamedParameters const& p) {
  return os << Brief(*p.name().handle()) << ", " << p.contextual_mode();
}


std::ostream& operator<<(std::ostream& os, LoadPropertyParameters const& p) {
  // Nothing special to print.
  return os;
}


bool operator==(LoadPropertyParameters const& lhs,
                LoadPropertyParameters const& rhs) {
  return lhs.feedback() == rhs.feedback();
}


bool operator!=(LoadPropertyParameters const& lhs,
                LoadPropertyParameters const& rhs) {
  return !(lhs == rhs);
}


const LoadPropertyParameters& LoadPropertyParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSLoadProperty, op->opcode());
  return OpParameter<LoadPropertyParameters>(op);
}


size_t hash_value(LoadPropertyParameters const& p) {
  return hash_value(p.feedback());
}


const LoadNamedParameters& LoadNamedParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, op->opcode());
  return OpParameter<LoadNamedParameters>(op);
}


bool operator==(StoreNamedParameters const& lhs,
                StoreNamedParameters const& rhs) {
  return lhs.language_mode() == rhs.language_mode() && lhs.name() == rhs.name();
}


bool operator!=(StoreNamedParameters const& lhs,
                StoreNamedParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(StoreNamedParameters const& p) {
  return base::hash_combine(p.language_mode(), p.name());
}


std::ostream& operator<<(std::ostream& os, StoreNamedParameters const& p) {
  return os << p.language_mode() << ", " << Brief(*p.name().handle());
}


const StoreNamedParameters& StoreNamedParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSStoreNamed, op->opcode());
  return OpParameter<StoreNamedParameters>(op);
}


bool operator==(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return lhs.pretenure() == rhs.pretenure() &&
         lhs.shared_info().is_identical_to(rhs.shared_info());
}


bool operator!=(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateClosureParameters const& p) {
  // TODO(mstarzinger): Include hash of the SharedFunctionInfo here.
  base::hash<PretenureFlag> h;
  return h(p.pretenure());
}


std::ostream& operator<<(std::ostream& os, CreateClosureParameters const& p) {
  return os << p.pretenure() << ", " << Brief(*p.shared_info());
}


const CreateClosureParameters& CreateClosureParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateClosure, op->opcode());
  return OpParameter<CreateClosureParameters>(op);
}


#define CACHED_OP_LIST(V)                                 \
  V(Equal, Operator::kNoProperties, 2, 1)                 \
  V(NotEqual, Operator::kNoProperties, 2, 1)              \
  V(StrictEqual, Operator::kPure, 2, 1)                   \
  V(StrictNotEqual, Operator::kPure, 2, 1)                \
  V(UnaryNot, Operator::kPure, 1, 1)                      \
  V(ToBoolean, Operator::kPure, 1, 1)                     \
  V(ToNumber, Operator::kNoProperties, 1, 1)              \
  V(ToString, Operator::kNoProperties, 1, 1)              \
  V(ToName, Operator::kNoProperties, 1, 1)                \
  V(ToObject, Operator::kNoProperties, 1, 1)              \
  V(Yield, Operator::kNoProperties, 1, 1)                 \
  V(Create, Operator::kEliminatable, 0, 1)                \
  V(HasProperty, Operator::kNoProperties, 2, 1)           \
  V(TypeOf, Operator::kPure, 1, 1)                        \
  V(InstanceOf, Operator::kNoProperties, 2, 1)            \
  V(StackCheck, Operator::kNoProperties, 0, 0)            \
  V(CreateFunctionContext, Operator::kNoProperties, 1, 1) \
  V(CreateWithContext, Operator::kNoProperties, 2, 1)     \
  V(CreateBlockContext, Operator::kNoProperties, 2, 1)    \
  V(CreateModuleContext, Operator::kNoProperties, 2, 1)   \
  V(CreateScriptContext, Operator::kNoProperties, 2, 1)


#define CACHED_OP_LIST_WITH_LANGUAGE_MODE(V)           \
  V(LessThan, Operator::kNoProperties, 2, 1)           \
  V(GreaterThan, Operator::kNoProperties, 2, 1)        \
  V(LessThanOrEqual, Operator::kNoProperties, 2, 1)    \
  V(GreaterThanOrEqual, Operator::kNoProperties, 2, 1) \
  V(BitwiseOr, Operator::kNoProperties, 2, 1)          \
  V(BitwiseXor, Operator::kNoProperties, 2, 1)         \
  V(BitwiseAnd, Operator::kNoProperties, 2, 1)         \
  V(ShiftLeft, Operator::kNoProperties, 2, 1)          \
  V(ShiftRight, Operator::kNoProperties, 2, 1)         \
  V(ShiftRightLogical, Operator::kNoProperties, 2, 1)  \
  V(Add, Operator::kNoProperties, 2, 1)                \
  V(Subtract, Operator::kNoProperties, 2, 1)           \
  V(Multiply, Operator::kNoProperties, 2, 1)           \
  V(Divide, Operator::kNoProperties, 2, 1)             \
  V(Modulus, Operator::kNoProperties, 2, 1)            \
  V(StoreProperty, Operator::kNoProperties, 3, 0)


struct JSOperatorGlobalCache final {
#define CACHED(Name, properties, value_input_count, value_output_count)  \
  struct Name##Operator final : public Operator {                        \
    Name##Operator()                                                     \
        : Operator(IrOpcode::kJS##Name, properties, "JS" #Name,          \
                   value_input_count, Operator::ZeroIfPure(properties),  \
                   Operator::ZeroIfEliminatable(properties),             \
                   value_output_count, Operator::ZeroIfPure(properties), \
                   Operator::ZeroIfNoThrow(properties)) {}               \
  };                                                                     \
  Name##Operator k##Name##Operator;
  CACHED_OP_LIST(CACHED)
#undef CACHED


#define CACHED_WITH_LANGUAGE_MODE(Name, properties, value_input_count,        \
                                  value_output_count)                         \
  template <LanguageMode kLanguageMode>                                       \
  struct Name##Operator final : public Operator1<LanguageMode> {              \
    Name##Operator()                                                          \
        : Operator1<LanguageMode>(                                            \
              IrOpcode::kJS##Name, properties, "JS" #Name, value_input_count, \
              Operator::ZeroIfPure(properties),                               \
              Operator::ZeroIfEliminatable(properties), value_output_count,   \
              Operator::ZeroIfPure(properties),                               \
              Operator::ZeroIfNoThrow(properties), kLanguageMode) {}          \
  };                                                                          \
  Name##Operator<SLOPPY> k##Name##SloppyOperator;                             \
  Name##Operator<STRICT> k##Name##StrictOperator;                             \
  Name##Operator<STRONG> k##Name##StrongOperator;
  CACHED_OP_LIST_WITH_LANGUAGE_MODE(CACHED_WITH_LANGUAGE_MODE)
#undef CACHED_WITH_LANGUAGE_MODE
};


static base::LazyInstance<JSOperatorGlobalCache>::type kCache =
    LAZY_INSTANCE_INITIALIZER;


JSOperatorBuilder::JSOperatorBuilder(Zone* zone)
    : cache_(kCache.Get()), zone_(zone) {}


#define CACHED(Name, properties, value_input_count, value_output_count) \
  const Operator* JSOperatorBuilder::Name() {                           \
    return &cache_.k##Name##Operator;                                   \
  }
CACHED_OP_LIST(CACHED)
#undef CACHED


#define CACHED_WITH_LANGUAGE_MODE(Name, properties, value_input_count,  \
                                  value_output_count)                   \
  const Operator* JSOperatorBuilder::Name(LanguageMode language_mode) { \
    switch (language_mode) {                                            \
      case SLOPPY:                                                      \
        return &cache_.k##Name##SloppyOperator;                         \
      case STRICT:                                                      \
        return &cache_.k##Name##StrictOperator;                         \
      case STRONG:                                                      \
        return &cache_.k##Name##StrongOperator;                         \
      case STRONG_BIT:                                                  \
        break; /* %*!%^$#@ */                                           \
    }                                                                   \
    UNREACHABLE();                                                      \
    return nullptr;                                                     \
  }
CACHED_OP_LIST_WITH_LANGUAGE_MODE(CACHED_WITH_LANGUAGE_MODE)
#undef CACHED_WITH_LANGUAGE_MODE


const Operator* JSOperatorBuilder::CallFunction(size_t arity,
                                                CallFunctionFlags flags,
                                                LanguageMode language_mode) {
  CallFunctionParameters parameters(arity, flags, language_mode);
  return new (zone()) Operator1<CallFunctionParameters>(   // --
      IrOpcode::kJSCallFunction, Operator::kNoProperties,  // opcode
      "JSCallFunction",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                   // inputs/outputs
      parameters);                                         // parameter
}


const Operator* JSOperatorBuilder::CallRuntime(Runtime::FunctionId id,
                                               size_t arity) {
  CallRuntimeParameters parameters(id, arity);
  const Runtime::Function* f = Runtime::FunctionForId(parameters.id());
  DCHECK(f->nargs == -1 || f->nargs == static_cast<int>(parameters.arity()));
  return new (zone()) Operator1<CallRuntimeParameters>(   // --
      IrOpcode::kJSCallRuntime, Operator::kNoProperties,  // opcode
      "JSCallRuntime",                                    // name
      parameters.arity(), 1, 1, f->result_size, 1, 2,     // inputs/outputs
      parameters);                                        // parameter
}


const Operator* JSOperatorBuilder::CallConstruct(int arguments) {
  return new (zone()) Operator1<int>(                       // --
      IrOpcode::kJSCallConstruct, Operator::kNoProperties,  // opcode
      "JSCallConstruct",                                    // name
      arguments, 1, 1, 1, 1, 2,                             // counts
      arguments);                                           // parameter
}


const Operator* JSOperatorBuilder::LoadNamed(const Unique<Name>& name,
                                             const VectorSlotPair& feedback,
                                             ContextualMode contextual_mode,
                                             PropertyICMode load_ic) {
  LoadNamedParameters parameters(name, feedback, contextual_mode, load_ic);
  return new (zone()) Operator1<LoadNamedParameters>(   // --
      IrOpcode::kJSLoadNamed, Operator::kNoProperties,  // opcode
      "JSLoadNamed",                                    // name
      1, 1, 1, 1, 1, 2,                                 // counts
      parameters);                                      // parameter
}


const Operator* JSOperatorBuilder::LoadProperty(
    const VectorSlotPair& feedback) {
  LoadPropertyParameters parameters(feedback);
  return new (zone()) Operator1<LoadPropertyParameters>(   // --
      IrOpcode::kJSLoadProperty, Operator::kNoProperties,  // opcode
      "JSLoadProperty",                                    // name
      2, 1, 1, 1, 1, 2,                                    // counts
      parameters);                                         // parameter
}


const Operator* JSOperatorBuilder::StoreNamed(LanguageMode language_mode,
                                              const Unique<Name>& name,
                                              PropertyICMode store_ic) {
  StoreNamedParameters parameters(language_mode, name, store_ic);
  return new (zone()) Operator1<StoreNamedParameters>(   // --
      IrOpcode::kJSStoreNamed, Operator::kNoProperties,  // opcode
      "JSStoreNamed",                                    // name
      2, 1, 1, 0, 1, 2,                                  // counts
      parameters);                                       // parameter
}


const Operator* JSOperatorBuilder::DeleteProperty(LanguageMode language_mode) {
  return new (zone()) Operator1<LanguageMode>(               // --
      IrOpcode::kJSDeleteProperty, Operator::kNoProperties,  // opcode
      "JSDeleteProperty",                                    // name
      2, 1, 1, 1, 1, 2,                                      // counts
      language_mode);                                        // parameter
}


const Operator* JSOperatorBuilder::LoadContext(size_t depth, size_t index,
                                               bool immutable) {
  ContextAccess access(depth, index, immutable);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSLoadContext,                  // opcode
      Operator::kNoWrite | Operator::kNoThrow,   // flags
      "JSLoadContext",                           // name
      1, 1, 0, 1, 1, 0,                          // counts
      access);                                   // parameter
}


const Operator* JSOperatorBuilder::StoreContext(size_t depth, size_t index) {
  ContextAccess access(depth, index, false);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSStoreContext,                 // opcode
      Operator::kNoRead | Operator::kNoThrow,    // flags
      "JSStoreContext",                          // name
      2, 1, 1, 0, 1, 0,                          // counts
      access);                                   // parameter
}


const Operator* JSOperatorBuilder::CreateClosure(
    Handle<SharedFunctionInfo> shared_info, PretenureFlag pretenure) {
  CreateClosureParameters parameters(shared_info, pretenure);
  return new (zone()) Operator1<CreateClosureParameters>(  // --
      IrOpcode::kJSCreateClosure, Operator::kNoThrow,      // opcode
      "JSCreateClosure",                                   // name
      1, 1, 1, 1, 1, 0,                                    // counts
      parameters);                                         // parameter
}


const Operator* JSOperatorBuilder::CreateLiteralArray(int literal_flags) {
  return new (zone()) Operator1<int>(                            // --
      IrOpcode::kJSCreateLiteralArray, Operator::kNoProperties,  // opcode
      "JSCreateLiteralArray",                                    // name
      3, 1, 1, 1, 1, 2,                                          // counts
      literal_flags);                                            // parameter
}


const Operator* JSOperatorBuilder::CreateLiteralObject(int literal_flags) {
  return new (zone()) Operator1<int>(                             // --
      IrOpcode::kJSCreateLiteralObject, Operator::kNoProperties,  // opcode
      "JSCreateLiteralObject",                                    // name
      3, 1, 1, 1, 1, 2,                                           // counts
      literal_flags);                                             // parameter
}


const Operator* JSOperatorBuilder::CreateCatchContext(
    const Unique<String>& name) {
  return new (zone()) Operator1<Unique<String>>(                 // --
      IrOpcode::kJSCreateCatchContext, Operator::kNoProperties,  // opcode
      "JSCreateCatchContext",                                    // name
      2, 1, 1, 1, 1, 2,                                          // counts
      name);                                                     // parameter
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
