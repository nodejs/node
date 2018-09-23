// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"

#include "src/api-inl.h"
#include "src/assembler-inl.h"
#include "src/builtins/builtins-descriptors.h"
#include "src/callable.h"
#include "src/instruction-stream.h"
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
  { static_cast<Address>(static_cast<uintptr_t>(           \
                              Count) << (kBitsPerByte * (kPointerSize - 1))) }},
#else
#define DECL_TFJ(Name, Count, ...) { #Name, Builtins::TFJ, \
                              { static_cast<Address>(Count) }},
#endif
#define DECL_TFC(Name, ...) { #Name, Builtins::TFC, {} },
#define DECL_TFS(Name, ...) { #Name, Builtins::TFS, {} },
#define DECL_TFH(Name, ...) { #Name, Builtins::TFH, {} },
#define DECL_BCH(Name, ...) { #Name "Handler", Builtins::BCH, {} }, \
                            { #Name "WideHandler", Builtins::BCH, {} }, \
                            { #Name "ExtraWideHandler", Builtins::BCH, {} },
#define DECL_ASM(Name, ...) { #Name, Builtins::ASM, {} },
const BuiltinMetadata builtin_metadata[] = {
  BUILTIN_LIST(DECL_CPP, DECL_API, DECL_TFJ, DECL_TFC, DECL_TFS, DECL_TFH,
               DECL_BCH, DECL_ASM)
};
#undef DECL_CPP
#undef DECL_API
#undef DECL_TFJ
#undef DECL_TFC
#undef DECL_TFS
#undef DECL_TFH
#undef DECL_BCH
#undef DECL_ASM
// clang-format on

}  // namespace

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

const char* Builtins::Lookup(Address pc) {
  // Off-heap pc's can be looked up through binary search.
  if (FLAG_embedded_builtins) {
    Code* maybe_builtin = InstructionStream::TryLookupCode(isolate_, pc);
    if (maybe_builtin != nullptr) return name(maybe_builtin->builtin_index());
  }

  // May be called during initialization (disassembler).
  if (initialized_) {
    for (int i = 0; i < builtin_count; i++) {
      if (isolate_->heap()->builtin(i)->contains(pc)) return name(i);
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
  isolate_->heap()->set_builtin(index, builtin);
}

Code* Builtins::builtin(int index) { return isolate_->heap()->builtin(index); }

Handle<Code> Builtins::builtin_handle(int index) {
  DCHECK(IsBuiltinId(index));
  return Handle<Code>(
      reinterpret_cast<Code**>(isolate_->heap()->builtin_address(index)));
}

// static
int Builtins::GetStackParameterCount(Name name) {
  DCHECK(Builtins::KindOf(name) == TFJ);
  return builtin_metadata[name].kind_specific_data.parameter_count;
}

// static
Callable Builtins::CallableFor(Isolate* isolate, Name name) {
  Handle<Code> code = isolate->builtins()->builtin_handle(name);
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
                 CASE_OTHER, CASE_OTHER, IGNORE_BUILTIN, IGNORE_BUILTIN)
#undef CASE_OTHER
    default:
      Builtins::Kind kind = Builtins::KindOf(name);
      DCHECK_NE(kind, BCH);
      if (kind == TFJ || kind == CPP) {
        return Callable(code, JSTrampolineDescriptor{});
      }
      UNREACHABLE();
  }
  CallInterfaceDescriptor descriptor(key);
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
bool Builtins::IsBuiltin(const Code* code) {
  return Builtins::IsBuiltinId(code->builtin_index());
}

bool Builtins::IsBuiltinHandle(Handle<HeapObject> maybe_code,
                               int* index) const {
  Heap* heap = isolate_->heap();
  Address handle_location = maybe_code.address();
  Address start = heap->builtin_address(0);
  Address end = heap->builtin_address(Builtins::builtin_count);
  if (handle_location >= end) return false;
  if (handle_location < start) return false;
  *index = static_cast<int>(handle_location - start) >> kPointerSizeLog2;
  DCHECK(Builtins::IsBuiltinId(*index));
  return true;
}

// static
bool Builtins::IsIsolateIndependentBuiltin(const Code* code) {
  if (FLAG_embedded_builtins) {
    const int builtin_index = code->builtin_index();
    return Builtins::IsBuiltinId(builtin_index) &&
           Builtins::IsIsolateIndependent(builtin_index);
  } else {
    return false;
  }
}

// static
bool Builtins::IsLazy(int index) {
  DCHECK(IsBuiltinId(index));

  if (FLAG_embedded_builtins) {
    // We don't want to lazy-deserialize off-heap builtins.
    if (Builtins::IsIsolateIndependent(index)) return false;
  }

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
    case kArrayEveryLoopEagerDeoptContinuation:
    case kArrayEveryLoopLazyDeoptContinuation:
    case kArrayFilterLoopEagerDeoptContinuation:
    case kArrayFilterLoopLazyDeoptContinuation:
    case kArrayFindIndexLoopAfterCallbackLazyDeoptContinuation:
    case kArrayFindIndexLoopEagerDeoptContinuation:
    case kArrayFindIndexLoopLazyDeoptContinuation:
    case kArrayFindLoopAfterCallbackLazyDeoptContinuation:
    case kArrayFindLoopEagerDeoptContinuation:
    case kArrayFindLoopLazyDeoptContinuation:
    case kArrayForEachLoopEagerDeoptContinuation:
    case kArrayForEachLoopLazyDeoptContinuation:
    case kArrayMapLoopEagerDeoptContinuation:
    case kArrayMapLoopLazyDeoptContinuation:
    case kArrayReduceLoopEagerDeoptContinuation:
    case kArrayReduceLoopLazyDeoptContinuation:
    case kArrayReducePreLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopLazyDeoptContinuation:
    case kArrayReduceRightPreLoopEagerDeoptContinuation:
    case kArraySomeLoopEagerDeoptContinuation:
    case kArraySomeLoopLazyDeoptContinuation:
    case kAsyncGeneratorAwaitCaught:            // https://crbug.com/v8/6786.
    case kAsyncGeneratorAwaitUncaught:          // https://crbug.com/v8/6786.
    // CEntry variants must be immovable, whereas lazy deserialization allocates
    // movable code.
    case kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit:
    case kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_BuiltinExit:
    case kCEntry_Return1_DontSaveFPRegs_ArgvInRegister_NoBuiltinExit:
    case kCEntry_Return1_SaveFPRegs_ArgvOnStack_NoBuiltinExit:
    case kCEntry_Return1_SaveFPRegs_ArgvOnStack_BuiltinExit:
    case kCEntry_Return2_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit:
    case kCEntry_Return2_DontSaveFPRegs_ArgvOnStack_BuiltinExit:
    case kCEntry_Return2_DontSaveFPRegs_ArgvInRegister_NoBuiltinExit:
    case kCEntry_Return2_SaveFPRegs_ArgvOnStack_NoBuiltinExit:
    case kCEntry_Return2_SaveFPRegs_ArgvOnStack_BuiltinExit:
    case kCompileLazy:
    case kDebugBreakTrampoline:
    case kDeserializeLazy:
    case kFunctionPrototypeHasInstance:  // https://crbug.com/v8/6786.
    case kHandleApiCall:
    case kIllegal:
    case kInstantiateAsmJs:
    case kInterpreterEnterBytecodeAdvance:
    case kInterpreterEnterBytecodeDispatch:
    case kInterpreterEntryTrampoline:
    case kPromiseConstructorLazyDeoptContinuation:
    case kRecordWrite:  // https://crbug.com/chromium/765301.
    case kThrowWasmTrapDivByZero:             // Required by wasm.
    case kThrowWasmTrapDivUnrepresentable:    // Required by wasm.
    case kThrowWasmTrapFloatUnrepresentable:  // Required by wasm.
    case kThrowWasmTrapFuncInvalid:           // Required by wasm.
    case kThrowWasmTrapFuncSigMismatch:       // Required by wasm.
    case kThrowWasmTrapMemOutOfBounds:        // Required by wasm.
    case kThrowWasmTrapRemByZero:             // Required by wasm.
    case kThrowWasmTrapUnreachable:           // Required by wasm.
    case kToBooleanLazyDeoptContinuation:
    case kToNumber:                           // Required by wasm.
    case kGenericConstructorLazyDeoptContinuation:
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
bool Builtins::IsIsolateIndependent(int index) {
  DCHECK(IsBuiltinId(index));
#ifndef V8_TARGET_ARCH_IA32
  switch (index) {
// Bytecode handlers do not yet support being embedded.
#ifdef V8_EMBEDDED_BYTECODE_HANDLERS
#define BYTECODE_BUILTIN(Name, ...) \
  case k##Name##Handler:            \
  case k##Name##WideHandler:        \
  case k##Name##ExtraWideHandler:   \
    return false;
    BUILTIN_LIST_BYTECODE_HANDLERS(BYTECODE_BUILTIN)
#undef BYTECODE_BUILTIN
#endif  // V8_EMBEDDED_BYTECODE_HANDLERS

    // TODO(jgruber): There's currently two blockers for moving
    // InterpreterEntryTrampoline into the binary:
    // 1. InterpreterEnterBytecode calculates a pointer into the middle of
    //    InterpreterEntryTrampoline (see interpreter_entry_return_pc_offset).
    //    When the builtin is embedded, the pointer would need to be calculated
    //    at an offset from the embedded instruction stream (instead of the
    //    trampoline code object).
    // 2. We create distinct copies of the trampoline to make it possible to
    //    attribute ticks in the interpreter to individual JS functions.
    //    See https://crrev.com/c/959081 and InstallBytecodeArray. When the
    //    trampoline is embedded, we need to ensure that CopyCode creates a copy
    //    of the builtin itself (and not just the trampoline).
    case kInterpreterEntryTrampoline:
      return false;
    default:
      return true;
  }
#else   // V8_TARGET_ARCH_IA32
  // TODO(jgruber, v8:6666): Implement support.
  // ia32 is a work-in-progress. This will let us make builtins
  // isolate-independent one-by-one.
  switch (index) {
    case kContinueToCodeStubBuiltin:
    case kContinueToCodeStubBuiltinWithResult:
    case kContinueToJavaScriptBuiltin:
    case kContinueToJavaScriptBuiltinWithResult:
    case kWasmAllocateHeapNumber:
    case kWasmCallJavaScript:
    case kWasmToNumber:
    case kDoubleToI:
      return true;
    default:
      return false;
  }
#endif  // V8_TARGET_ARCH_IA32
  UNREACHABLE();
}

// static
bool Builtins::IsWasmRuntimeStub(int index) {
  DCHECK(IsBuiltinId(index));
  switch (index) {
#define CASE_TRAP(Name) case kThrowWasm##Name:
#define CASE(Name) case k##Name:
    WASM_RUNTIME_STUB_LIST(CASE, CASE_TRAP)
#undef CASE_TRAP
#undef CASE
    return true;
    default:
      return false;
  }
  UNREACHABLE();
}

// static
Handle<Code> Builtins::GenerateOffHeapTrampolineFor(Isolate* isolate,
                                                    Address off_heap_entry) {
  DCHECK(isolate->serializer_enabled());
  DCHECK_NOT_NULL(isolate->embedded_blob());
  DCHECK_NE(0, isolate->embedded_blob_size());

  constexpr size_t buffer_size = 256;  // Enough to fit the single jmp.
  byte buffer[buffer_size];            // NOLINT(runtime/arrays)

  // Generate replacement code that simply tail-calls the off-heap code.
  MacroAssembler masm(isolate, buffer, buffer_size, CodeObjectRequired::kYes);
  DCHECK(!masm.has_frame());
  {
    FrameScope scope(&masm, StackFrame::NONE);
    masm.JumpToInstructionStream(off_heap_entry);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);

  return isolate->factory()->NewCode(desc, Code::BUILTIN, masm.CodeObject());
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
    case BCH: return "BCH";
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
