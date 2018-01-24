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
#undef FORWARD_DECLARE

namespace {

// TODO(jgruber): Pack in CallDescriptors::Key.
struct BuiltinMetadata {
  const char* name;
  Builtins::Kind kind;
  union {
    Address cpp_entry;       // For CPP and API builtins.
    int8_t parameter_count;  // For TFJ builtins.
  } kind_specific_data;
};

// clang-format off
#define DECL_CPP(Name, ...) { #Name, Builtins::CPP, \
                              { FUNCTION_ADDR(Builtin_##Name) }},
#define DECL_API(Name, ...) { #Name, Builtins::API, \
                              { FUNCTION_ADDR(Builtin_##Name) }},
#ifdef V8_TARGET_BIG_ENDIAN
#define DECL_TFJ(Name, Count, ...) { #Name, Builtins::TFJ, \
  { reinterpret_cast<Address>(static_cast<uintptr_t>(      \
                              Count) << (kBitsPerByte * (kPointerSize - 1))) }},
#else
#define DECL_TFJ(Name, Count, ...) { #Name, Builtins::TFJ, \
                              { reinterpret_cast<Address>(Count) }},
#endif
#define DECL_TFC(Name, ...) { #Name, Builtins::TFC, {} },
#define DECL_TFS(Name, ...) { #Name, Builtins::TFS, {} },
#define DECL_TFH(Name, ...) { #Name, Builtins::TFH, {} },
#define DECL_ASM(Name, ...) { #Name, Builtins::ASM, {} },
const BuiltinMetadata builtin_metadata[] = {
  BUILTIN_LIST(DECL_CPP, DECL_API, DECL_TFJ, DECL_TFC, DECL_TFS, DECL_TFH,
               DECL_ASM)
};
#undef DECL_CPP
#undef DECL_API
#undef DECL_TFJ
#undef DECL_TFC
#undef DECL_TFS
#undef DECL_TFH
#undef DECL_ASM
// clang-format on

}  // namespace

Builtins::Builtins() : initialized_(false) {
  memset(builtins_, 0, sizeof(builtins_[0]) * builtin_count);
}

Builtins::~Builtins() {}

BailoutId Builtins::GetContinuationBailoutId(Name name) {
  DCHECK(Builtins::KindOf(name) == TFJ || Builtins::KindOf(name) == TFC);
  return BailoutId(BailoutId::kFirstBuiltinContinuationId + name);
}

Builtins::Name Builtins::GetBuiltinFromBailoutId(BailoutId id) {
  int builtin_index = id.ToInt() - BailoutId::kFirstBuiltinContinuationId;
  DCHECK(Builtins::KindOf(builtin_index) == TFJ ||
         Builtins::KindOf(builtin_index) == TFC);
  return static_cast<Name>(builtin_index);
}

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
  return nullptr;
}

Handle<Code> Builtins::NewFunctionContext(ScopeType scope_type) {
  switch (scope_type) {
    case ScopeType::EVAL_SCOPE:
      return builtin_handle(kFastNewFunctionContextEval);
    case ScopeType::FUNCTION_SCOPE:
      return builtin_handle(kFastNewFunctionContextFunction);
    default:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

Handle<Code> Builtins::NonPrimitiveToPrimitive(ToPrimitiveHint hint) {
  switch (hint) {
    case ToPrimitiveHint::kDefault:
      return builtin_handle(kNonPrimitiveToPrimitive_Default);
    case ToPrimitiveHint::kNumber:
      return builtin_handle(kNonPrimitiveToPrimitive_Number);
    case ToPrimitiveHint::kString:
      return builtin_handle(kNonPrimitiveToPrimitive_String);
  }
  UNREACHABLE();
}

Handle<Code> Builtins::OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint) {
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      return builtin_handle(kOrdinaryToPrimitive_Number);
    case OrdinaryToPrimitiveHint::kString:
      return builtin_handle(kOrdinaryToPrimitive_String);
  }
  UNREACHABLE();
}

void Builtins::set_builtin(int index, HeapObject* builtin) {
  DCHECK(Builtins::IsBuiltinId(index));
  DCHECK(Internals::HasHeapObjectTag(builtin));
  // The given builtin may be completely uninitialized thus we cannot check its
  // type here.
  builtins_[index] = builtin;
}

Handle<Code> Builtins::builtin_handle(int index) {
  DCHECK(IsBuiltinId(index));
  return Handle<Code>(reinterpret_cast<Code**>(builtin_address(index)));
}

// static
int Builtins::GetStackParameterCount(Name name) {
  DCHECK(Builtins::KindOf(name) == TFJ);
  return builtin_metadata[name].kind_specific_data.parameter_count;
}

// static
Callable Builtins::CallableFor(Isolate* isolate, Name name) {
  Handle<Code> code(
      reinterpret_cast<Code**>(isolate->builtins()->builtin_address(name)));
  CallDescriptors::Key key;
  switch (name) {
// This macro is deliberately crafted so as to emit very little code,
// in order to keep binary size of this function under control.
#define CASE_OTHER(Name, ...)                          \
  case k##Name: {                                      \
    key = Builtin_##Name##_InterfaceDescriptor::key(); \
    break;                                             \
  }
    BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, CASE_OTHER,
                 CASE_OTHER, CASE_OTHER, IGNORE_BUILTIN)
#undef CASE_OTHER
    case kArrayFilterLoopEagerDeoptContinuation:
    case kArrayFilterLoopLazyDeoptContinuation:
    case kArrayForEach:
    case kArrayForEachLoopEagerDeoptContinuation:
    case kArrayForEachLoopLazyDeoptContinuation:
    case kArrayMapLoopEagerDeoptContinuation:
    case kArrayMapLoopLazyDeoptContinuation:
    case kConsoleAssert:
      return Callable(code, BuiltinDescriptor(isolate));
    default:
      UNREACHABLE();
  }
  CallInterfaceDescriptor descriptor(isolate, key);
  return Callable(code, descriptor);
}

// static
const char* Builtins::name(int index) {
  DCHECK(IsBuiltinId(index));
  return builtin_metadata[index].name;
}

// static
Address Builtins::CppEntryOf(int index) {
  DCHECK(Builtins::HasCppImplementation(index));
  return builtin_metadata[index].kind_specific_data.cpp_entry;
}

// static
bool Builtins::IsLazy(int index) {
  DCHECK(IsBuiltinId(index));
  // There are a couple of reasons that builtins can require eager-loading,
  // i.e. deserialization at isolate creation instead of on-demand. For
  // instance:
  // * DeserializeLazy implements lazy loading.
  // * Immovability requirement. This can only conveniently be guaranteed at
  //   isolate creation (at runtime, we'd have to allocate in LO space).
  // * To avoid conflicts in SharedFunctionInfo::function_data (Illegal,
  //   HandleApiCall, interpreter entry trampolines).
  // * Frequent use makes lazy loading unnecessary (CompileLazy).
  // TODO(wasm): Remove wasm builtins once immovability is no longer required.
  switch (index) {
    case kAbort:  // Required by wasm.
    case kArrayForEachLoopEagerDeoptContinuation:  // https://crbug.com/v8/6786.
    case kArrayForEachLoopLazyDeoptContinuation:   // https://crbug.com/v8/6786.
    case kArrayMapLoopEagerDeoptContinuation:      // https://crbug.com/v8/6786.
    case kArrayMapLoopLazyDeoptContinuation:       // https://crbug.com/v8/6786.
    case kArrayFilterLoopEagerDeoptContinuation:   // https://crbug.com/v8/6786.
    case kArrayFilterLoopLazyDeoptContinuation:    // https://crbug.com/v8/6786.
    case kCheckOptimizationMarker:
    case kCompileLazy:
    case kDeserializeLazy:
    case kFunctionPrototypeHasInstance:  // https://crbug.com/v8/6786.
    case kHandleApiCall:
    case kIllegal:
    case kInterpreterEnterBytecodeAdvance:
    case kInterpreterEnterBytecodeDispatch:
    case kInterpreterEntryTrampoline:
    case kObjectConstructor_ConstructStub:    // https://crbug.com/v8/6787.
    case kProxyConstructor_ConstructStub:     // https://crbug.com/v8/6787.
    case kNumberConstructor_ConstructStub:    // https://crbug.com/v8/6787.
    case kStringConstructor_ConstructStub:    // https://crbug.com/v8/6787.
    case kProxyConstructor:                   // https://crbug.com/v8/6787.
    case kRecordWrite:  // https://crbug.com/chromium/765301.
    case kThrowWasmTrapDivByZero:             // Required by wasm.
    case kThrowWasmTrapDivUnrepresentable:    // Required by wasm.
    case kThrowWasmTrapFloatUnrepresentable:  // Required by wasm.
    case kThrowWasmTrapFuncInvalid:           // Required by wasm.
    case kThrowWasmTrapFuncSigMismatch:       // Required by wasm.
    case kThrowWasmTrapMemOutOfBounds:        // Required by wasm.
    case kThrowWasmTrapRemByZero:             // Required by wasm.
    case kThrowWasmTrapUnreachable:           // Required by wasm.
    case kToNumber:                           // Required by wasm.
    case kWasmCompileLazy:                    // Required by wasm.
    case kWasmStackGuard:                     // Required by wasm.
      return false;
    default:
      // TODO(6624): Extend to other kinds.
      return KindOf(index) == TFJ;
  }
  UNREACHABLE();
}

// static
Builtins::Kind Builtins::KindOf(int index) {
  DCHECK(IsBuiltinId(index));
  return builtin_metadata[index].kind;
}

// static
const char* Builtins::KindNameOf(int index) {
  Kind kind = Builtins::KindOf(index);
  // clang-format off
  switch (kind) {
    case CPP: return "CPP";
    case API: return "API";
    case TFJ: return "TFJ";
    case TFC: return "TFC";
    case TFS: return "TFS";
    case TFH: return "TFH";
    case ASM: return "ASM";
  }
  // clang-format on
  UNREACHABLE();
}

// static
bool Builtins::IsCpp(int index) { return Builtins::KindOf(index) == CPP; }

// static
bool Builtins::HasCppImplementation(int index) {
  Kind kind = Builtins::KindOf(index);
  return (kind == CPP || kind == API);
}

Handle<Code> Builtins::JSConstructStubGeneric() {
  return FLAG_harmony_restrict_constructor_return
             ? builtin_handle(kJSConstructStubGenericRestrictedReturn)
             : builtin_handle(kJSConstructStubGenericUnrestrictedReturn);
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
