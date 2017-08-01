// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/builtins/builtins-descriptors.h"
#include "src/callable.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Object* Builtin_##Name(int argc, Object** args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)

Builtins::Builtins() : initialized_(false) {
  memset(builtins_, 0, sizeof(builtins_[0]) * builtin_count);
}

Builtins::~Builtins() {}

void Builtins::TearDown() { initialized_ = false; }

void Builtins::IterateBuiltins(RootVisitor* v) {
  v->VisitRootPointers(Root::kBuiltins, &builtins_[0],
                       &builtins_[0] + builtin_count);
}

const char* Builtins::Lookup(byte* pc) {
  // may be called during initialization (disassembler!)
  if (initialized_) {
    for (int i = 0; i < builtin_count; i++) {
      Code* entry = Code::cast(builtins_[i]);
      if (entry->contains(pc)) return name(i);
    }
  }
  return NULL;
}

Handle<Code> Builtins::NewFunctionContext(ScopeType scope_type) {
  switch (scope_type) {
    case ScopeType::EVAL_SCOPE:
      return FastNewFunctionContextEval();
    case ScopeType::FUNCTION_SCOPE:
      return FastNewFunctionContextFunction();
    default:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

Handle<Code> Builtins::NewCloneShallowArray(
    AllocationSiteMode allocation_mode) {
  switch (allocation_mode) {
    case TRACK_ALLOCATION_SITE:
      return FastCloneShallowArrayTrack();
    case DONT_TRACK_ALLOCATION_SITE:
      return FastCloneShallowArrayDontTrack();
    default:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

Handle<Code> Builtins::NonPrimitiveToPrimitive(ToPrimitiveHint hint) {
  switch (hint) {
    case ToPrimitiveHint::kDefault:
      return NonPrimitiveToPrimitive_Default();
    case ToPrimitiveHint::kNumber:
      return NonPrimitiveToPrimitive_Number();
    case ToPrimitiveHint::kString:
      return NonPrimitiveToPrimitive_String();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

Handle<Code> Builtins::OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint) {
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      return OrdinaryToPrimitive_Number();
    case OrdinaryToPrimitiveHint::kString:
      return OrdinaryToPrimitive_String();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

// static
int Builtins::GetBuiltinParameterCount(Name name) {
  switch (name) {
#define TFJ_CASE(Name, ParamCount, ...) \
  case k##Name: {                       \
    return ParamCount;                  \
  }
    BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, TFJ_CASE, IGNORE_BUILTIN,
                 IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)
#undef TFJ_CASE
    default:
      UNREACHABLE();
      return 0;
  }
}

// static
Callable Builtins::CallableFor(Isolate* isolate, Name name) {
  Handle<Code> code(
      reinterpret_cast<Code**>(isolate->builtins()->builtin_address(name)));
  CallDescriptors::Key key;
  switch (name) {
// This macro is deliberately crafted so as to emit very little code,
// in order to keep binary size of this function under control.
#define CASE(Name, ...)                                \
  case k##Name: {                                      \
    key = Builtin_##Name##_InterfaceDescriptor::key(); \
    break;                                             \
  }
    BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, CASE, CASE,
                 CASE, IGNORE_BUILTIN, IGNORE_BUILTIN)
#undef CASE
    case kConsoleAssert: {
      return Callable(code, BuiltinDescriptor(isolate));
    }
    default:
      UNREACHABLE();
      return Callable(Handle<Code>::null(), VoidDescriptor(isolate));
  }
  CallInterfaceDescriptor descriptor(isolate, key);
  return Callable(code, descriptor);
}

// static
const char* Builtins::name(int index) {
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return #Name;
    BUILTIN_LIST_ALL(CASE)
#undef CASE
    default:
      UNREACHABLE();
      break;
  }
  return "";
}

// static
Address Builtins::CppEntryOf(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return FUNCTION_ADDR(Builtin_##Name);
    BUILTIN_LIST_C(CASE)
#undef CASE
    default:
      return nullptr;
  }
  UNREACHABLE();
}

// static
bool Builtins::IsCpp(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return true;
#define BUILTIN_LIST_CPP(V)                                       \
  BUILTIN_LIST(V, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)
    BUILTIN_LIST_CPP(CASE)
#undef BUILTIN_LIST_CPP
#undef CASE
    default:
      return false;
  }
  UNREACHABLE();
}

// static
bool Builtins::IsApi(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return true;
#define BUILTIN_LIST_API(V)                                       \
  BUILTIN_LIST(IGNORE_BUILTIN, V, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)
    BUILTIN_LIST_API(CASE);
#undef BUILTIN_LIST_API
#undef CASE
    default:
      return false;
  }
  UNREACHABLE();
}

// static
bool Builtins::HasCppImplementation(int index) {
  DCHECK(0 <= index && index < builtin_count);
  switch (index) {
#define CASE(Name, ...) \
  case k##Name:         \
    return true;
    BUILTIN_LIST_C(CASE)
#undef CASE
    default:
      return false;
  }
  UNREACHABLE();
}

#define DEFINE_BUILTIN_ACCESSOR(Name, ...)                                    \
  Handle<Code> Builtins::Name() {                                             \
    Code** code_address = reinterpret_cast<Code**>(builtin_address(k##Name)); \
    return Handle<Code>(code_address);                                        \
  }
BUILTIN_LIST_ALL(DEFINE_BUILTIN_ACCESSOR)
#undef DEFINE_BUILTIN_ACCESSOR

Handle<Code> Builtins::JSConstructStubGeneric() {
  return FLAG_harmony_restrict_constructor_return
             ? JSConstructStubGenericRestrictedReturn()
             : JSConstructStubGenericUnrestrictedReturn();
}

// static
bool Builtins::AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                    Handle<JSObject> target_global_proxy) {
  if (FLAG_allow_unsafe_function_constructor) return true;
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  Handle<Context> responsible_context =
      impl->MicrotaskContextIsLastEnteredContext() ? impl->MicrotaskContext()
                                                   : impl->LastEnteredContext();
  // TODO(jochen): Remove this.
  if (responsible_context.is_null()) {
    return true;
  }
  if (*responsible_context == target->context()) return true;
  return isolate->MayAccess(responsible_context, target_global_proxy);
}

}  // namespace internal
}  // namespace v8
