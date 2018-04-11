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
  for (int i = 0; i < builtin_count; i++) {
    v->VisitRootPointer(Root::kBuiltins, name(i), &builtins_[i]);
  }
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
    default:
      Builtins::Kind kind = Builtins::KindOf(name);
      if (kind == TFJ || kind == CPP) {
        return Callable(code, BuiltinDescriptor(isolate));
      }
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
bool Builtins::IsBuiltin(Code* code) {
  return Builtins::IsBuiltinId(code->builtin_index());
}

// static
bool Builtins::IsOffHeapBuiltin(Code* code) {
#ifdef V8_EMBEDDED_BUILTINS
  return FLAG_stress_off_heap_code &&
         Builtins::IsBuiltinId(code->builtin_index()) &&
         Builtins::IsOffHeapSafe(code->builtin_index());
#else
  return false;
#endif
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
    case kArrayFindLoopEagerDeoptContinuation:  // https://crbug.com/v8/6786.
    case kArrayFindLoopLazyDeoptContinuation:   // https://crbug.com/v8/6786.
    // https://crbug.com/v8/6786.
    case kArrayFindLoopAfterCallbackLazyDeoptContinuation:
    // https://crbug.com/v8/6786.
    case kArrayFindIndexLoopEagerDeoptContinuation:
    // https://crbug.com/v8/6786.
    case kArrayFindIndexLoopLazyDeoptContinuation:
    // https://crbug.com/v8/6786.
    case kArrayFindIndexLoopAfterCallbackLazyDeoptContinuation:
    case kArrayForEachLoopEagerDeoptContinuation:  // https://crbug.com/v8/6786.
    case kArrayForEachLoopLazyDeoptContinuation:   // https://crbug.com/v8/6786.
    case kArrayMapLoopEagerDeoptContinuation:      // https://crbug.com/v8/6786.
    case kArrayMapLoopLazyDeoptContinuation:       // https://crbug.com/v8/6786.
    case kArrayEveryLoopEagerDeoptContinuation:    // https://crbug.com/v8/6786.
    case kArrayEveryLoopLazyDeoptContinuation:     // https://crbug.com/v8/6786.
    case kArrayFilterLoopEagerDeoptContinuation:   // https://crbug.com/v8/6786.
    case kArrayFilterLoopLazyDeoptContinuation:    // https://crbug.com/v8/6786.
    case kArrayReducePreLoopEagerDeoptContinuation:
    case kArrayReduceLoopEagerDeoptContinuation:   // https://crbug.com/v8/6786.
    case kArrayReduceLoopLazyDeoptContinuation:    // https://crbug.com/v8/6786.
    case kArrayReduceRightPreLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopLazyDeoptContinuation:
    case kArraySomeLoopEagerDeoptContinuation:  // https://crbug.com/v8/6786.
    case kArraySomeLoopLazyDeoptContinuation:   // https://crbug.com/v8/6786.
    case kAsyncGeneratorAwaitCaught:            // https://crbug.com/v8/6786.
    case kAsyncGeneratorAwaitUncaught:          // https://crbug.com/v8/6786.
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
    case kPromiseConstructorLazyDeoptContinuation:  // crbug/v8/6786.
    case kProxyConstructor_ConstructStub:     // https://crbug.com/v8/6787.
    case kNumberConstructor_ConstructStub:    // https://crbug.com/v8/6787.
    case kStringConstructor_ConstructStub:    // https://crbug.com/v8/6787.
    case kTypedArrayConstructor_ConstructStub:  // https://crbug.com/v8/6787.
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
bool Builtins::IsIsolateIndependent(int index) {
  DCHECK(IsBuiltinId(index));
  switch (index) {
#ifdef DEBUG
    case kAbortJS:
    case kAllocateHeapNumber:
    case kArrayEveryLoopEagerDeoptContinuation:
    case kArrayFilterLoopEagerDeoptContinuation:
    case kArrayFindIndexLoopEagerDeoptContinuation:
    case kArrayFindIndexLoopLazyDeoptContinuation:
    case kArrayFindLoopEagerDeoptContinuation:
    case kArrayFindLoopLazyDeoptContinuation:
    case kArrayForEachLoopEagerDeoptContinuation:
    case kArrayForEachLoopLazyDeoptContinuation:
    case kArrayMapLoopEagerDeoptContinuation:
    case kArrayReduceLoopEagerDeoptContinuation:
    case kArrayReduceLoopLazyDeoptContinuation:
    case kArrayReducePreLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopLazyDeoptContinuation:
    case kArrayReduceRightPreLoopEagerDeoptContinuation:
    case kArraySomeLoopEagerDeoptContinuation:
    case kBitwiseNot:
    case kBooleanPrototypeToString:
    case kBooleanPrototypeValueOf:
    case kContinueToCodeStubBuiltin:
    case kContinueToCodeStubBuiltinWithResult:
    case kContinueToJavaScriptBuiltin:
    case kContinueToJavaScriptBuiltinWithResult:
    case kDatePrototypeGetDate:
    case kDatePrototypeGetDay:
    case kDatePrototypeGetFullYear:
    case kDatePrototypeGetHours:
    case kDatePrototypeGetMilliseconds:
    case kDatePrototypeGetMinutes:
    case kDatePrototypeGetMonth:
    case kDatePrototypeGetSeconds:
    case kDatePrototypeGetTime:
    case kDatePrototypeGetTimezoneOffset:
    case kDatePrototypeGetUTCDate:
    case kDatePrototypeGetUTCDay:
    case kDatePrototypeGetUTCFullYear:
    case kDatePrototypeGetUTCHours:
    case kDatePrototypeGetUTCMilliseconds:
    case kDatePrototypeGetUTCMinutes:
    case kDatePrototypeGetUTCMonth:
    case kDatePrototypeGetUTCSeconds:
    case kDatePrototypeToPrimitive:
    case kDatePrototypeValueOf:
    case kDecrement:
    case kDivide:
    case kGlobalIsFinite:
    case kGlobalIsNaN:
    case kIncrement:
    case kKeyedLoadIC_Slow:
    case kKeyedLoadICTrampoline:
    case kKeyedStoreIC_Slow:
    case kKeyedStoreICTrampoline:
    case kLoadField:
    case kLoadGlobalICInsideTypeofTrampoline:
    case kLoadGlobalIC_Slow:
    case kLoadGlobalICTrampoline:
    case kLoadIC_Slow:
    case kLoadICTrampoline:
    case kMapPrototypeEntries:
    case kMapPrototypeGet:
    case kMapPrototypeGetSize:
    case kMapPrototypeHas:
    case kMapPrototypeKeys:
    case kMapPrototypeValues:
    case kMathAcos:
    case kMathAcosh:
    case kMathAsin:
    case kMathAsinh:
    case kMathAtan:
    case kMathAtan2:
    case kMathAtanh:
    case kMathCbrt:
    case kMathCeil:
    case kMathCos:
    case kMathCosh:
    case kMathExp:
    case kMathExpm1:
    case kMathFloor:
    case kMathFround:
    case kMathLog:
    case kMathLog10:
    case kMathLog1p:
    case kMathLog2:
    case kMathMax:
    case kMathMin:
    case kMathRound:
    case kMathSign:
    case kMathSin:
    case kMathSinh:
    case kMathSqrt:
    case kMathTan:
    case kMathTanh:
    case kMathTrunc:
    case kModulus:
    case kMultiply:
    case kNonPrimitiveToPrimitive_Default:
    case kNonPrimitiveToPrimitive_Number:
    case kNonPrimitiveToPrimitive_String:
    case kNumberIsFinite:
    case kNumberIsInteger:
    case kNumberIsNaN:
    case kNumberIsSafeInteger:
    case kNumberPrototypeValueOf:
    case kObjectPrototypeToLocaleString:
    case kObjectPrototypeValueOf:
    case kPromiseCapabilityDefaultReject:
    case kPromiseCapabilityDefaultResolve:
    case kPromiseConstructorLazyDeoptContinuation:
    case kPromiseInternalReject:
    case kPromiseInternalResolve:
    case kPromiseResolveTrampoline:
    case kPromiseThrowerFinally:
    case kPromiseValueThunkFinally:
    case kProxyConstructor:
    case kReflectHas:
    case kRegExpPrototypeDotAllGetter:
    case kRegExpPrototypeGlobalGetter:
    case kRegExpPrototypeIgnoreCaseGetter:
    case kRegExpPrototypeMultilineGetter:
    case kRegExpPrototypeSourceGetter:
    case kRegExpPrototypeStickyGetter:
    case kRegExpPrototypeUnicodeGetter:
    case kReturnReceiver:
    case kSetPrototypeEntries:
    case kSetPrototypeGetSize:
    case kSetPrototypeValues:
    case kStoreGlobalIC_Slow:
    case kStoreGlobalICTrampoline:
    case kStoreICTrampoline:
    case kStringPrototypeBig:
    case kStringPrototypeBlink:
    case kStringPrototypeBold:
    case kStringPrototypeConcat:
    case kStringPrototypeFixed:
    case kStringPrototypeItalics:
    case kStringPrototypeIterator:
    case kStringPrototypeSmall:
    case kStringPrototypeStrike:
    case kStringPrototypeSub:
    case kStringPrototypeSup:
#ifdef V8_INTL_SUPPORT
    case kStringPrototypeToLowerCaseIntl:
#endif
    case kSubtract:
    case kSymbolPrototypeToPrimitive:
    case kSymbolPrototypeToString:
    case kSymbolPrototypeValueOf:
    case kThrowWasmTrapDivByZero:
    case kThrowWasmTrapDivUnrepresentable:
    case kThrowWasmTrapFloatUnrepresentable:
    case kThrowWasmTrapFuncInvalid:
    case kThrowWasmTrapFuncSigMismatch:
    case kThrowWasmTrapMemOutOfBounds:
    case kThrowWasmTrapRemByZero:
    case kThrowWasmTrapUnreachable:
    case kToInteger:
    case kTypedArrayConstructor:
    case kWasmStackGuard:
    case kWeakMapGet:
    case kWeakMapHas:
    case kWeakMapPrototypeDelete:
    case kWeakMapPrototypeSet:
    case kWeakSetHas:
    case kWeakSetPrototypeAdd:
    case kWeakSetPrototypeDelete:
#else
    case kAbortJS:
    case kAdd:
    case kAllocateHeapNumber:
    case kArrayEvery:
    case kArrayEveryLoopContinuation:
    case kArrayEveryLoopEagerDeoptContinuation:
    case kArrayEveryLoopLazyDeoptContinuation:
    case kArrayFilterLoopEagerDeoptContinuation:
    case kArrayFilterLoopLazyDeoptContinuation:
    case kArrayFindIndexLoopAfterCallbackLazyDeoptContinuation:
    case kArrayFindIndexLoopContinuation:
    case kArrayFindIndexLoopEagerDeoptContinuation:
    case kArrayFindIndexLoopLazyDeoptContinuation:
    case kArrayFindLoopAfterCallbackLazyDeoptContinuation:
    case kArrayFindLoopContinuation:
    case kArrayFindLoopEagerDeoptContinuation:
    case kArrayFindLoopLazyDeoptContinuation:
    case kArrayForEach:
    case kArrayForEachLoopContinuation:
    case kArrayForEachLoopEagerDeoptContinuation:
    case kArrayForEachLoopLazyDeoptContinuation:
    case kArrayFrom:
    case kArrayIncludes:
    case kArrayIndexOf:
    case kArrayIsArray:
    case kArrayMapLoopContinuation:
    case kArrayMapLoopEagerDeoptContinuation:
    case kArrayMapLoopLazyDeoptContinuation:
    case kArrayOf:
    case kArrayPrototypeEntries:
    case kArrayPrototypeFind:
    case kArrayPrototypeFindIndex:
    case kArrayPrototypeKeys:
    case kArrayPrototypeSlice:
    case kArrayPrototypeValues:
    case kArrayReduce:
    case kArrayReduceLoopContinuation:
    case kArrayReduceLoopEagerDeoptContinuation:
    case kArrayReduceLoopLazyDeoptContinuation:
    case kArrayReducePreLoopEagerDeoptContinuation:
    case kArrayReduceRight:
    case kArrayReduceRightLoopContinuation:
    case kArrayReduceRightLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopLazyDeoptContinuation:
    case kArrayReduceRightPreLoopEagerDeoptContinuation:
    case kArraySome:
    case kArraySomeLoopContinuation:
    case kArraySomeLoopEagerDeoptContinuation:
    case kArraySomeLoopLazyDeoptContinuation:
    case kAsyncFromSyncIteratorPrototypeNext:
    case kAsyncFromSyncIteratorPrototypeReturn:
    case kAsyncFromSyncIteratorPrototypeThrow:
    case kAsyncFunctionAwaitFulfill:
    case kAsyncFunctionAwaitReject:
    case kAsyncFunctionPromiseCreate:
    case kAsyncFunctionPromiseRelease:
    case kAsyncGeneratorAwaitFulfill:
    case kAsyncGeneratorAwaitReject:
    case kAsyncGeneratorResumeNext:
    case kAsyncGeneratorReturnClosedFulfill:
    case kAsyncGeneratorReturnClosedReject:
    case kAsyncGeneratorReturnFulfill:
    case kAsyncGeneratorYieldFulfill:
    case kAsyncIteratorValueUnwrap:
    case kBitwiseNot:
    case kBooleanPrototypeToString:
    case kBooleanPrototypeValueOf:
    case kCallProxy:
    case kConstructFunction:
    case kConstructProxy:
    case kContinueToCodeStubBuiltin:
    case kContinueToCodeStubBuiltinWithResult:
    case kContinueToJavaScriptBuiltin:
    case kContinueToJavaScriptBuiltinWithResult:
    case kCreateGeneratorObject:
    case kCreateIterResultObject:
    case kCreateRegExpLiteral:
    case kDatePrototypeGetDate:
    case kDatePrototypeGetDay:
    case kDatePrototypeGetFullYear:
    case kDatePrototypeGetHours:
    case kDatePrototypeGetMilliseconds:
    case kDatePrototypeGetMinutes:
    case kDatePrototypeGetMonth:
    case kDatePrototypeGetSeconds:
    case kDatePrototypeGetTime:
    case kDatePrototypeGetTimezoneOffset:
    case kDatePrototypeGetUTCDate:
    case kDatePrototypeGetUTCDay:
    case kDatePrototypeGetUTCFullYear:
    case kDatePrototypeGetUTCHours:
    case kDatePrototypeGetUTCMilliseconds:
    case kDatePrototypeGetUTCMinutes:
    case kDatePrototypeGetUTCMonth:
    case kDatePrototypeGetUTCSeconds:
    case kDatePrototypeToPrimitive:
    case kDatePrototypeValueOf:
    case kDecrement:
    case kDeleteProperty:
    case kDivide:
    case kEqual:
    case kFastConsoleAssert:
    case kFastNewClosure:
    case kFastNewFunctionContextEval:
    case kFastNewFunctionContextFunction:
    case kFastNewObject:
    case kFindOrderedHashMapEntry:
    case kForInEnumerate:
    case kForInFilter:
    case kFunctionPrototypeHasInstance:
    case kGeneratorPrototypeNext:
    case kGeneratorPrototypeReturn:
    case kGeneratorPrototypeThrow:
    case kGetSuperConstructor:
    case kGlobalIsFinite:
    case kGlobalIsNaN:
    case kGreaterThan:
    case kGreaterThanOrEqual:
    case kHasProperty:
    case kIncrement:
    case kInstanceOf:
    case kKeyedLoadIC_Megamorphic:
    case kKeyedLoadIC_PolymorphicName:
    case kKeyedLoadIC_Slow:
    case kKeyedLoadICTrampoline:
    case kKeyedStoreIC_Slow:
    case kKeyedStoreICTrampoline:
    case kLessThan:
    case kLessThanOrEqual:
    case kLoadField:
    case kLoadGlobalIC:
    case kLoadGlobalICInsideTypeof:
    case kLoadGlobalICInsideTypeofTrampoline:
    case kLoadGlobalIC_Slow:
    case kLoadGlobalICTrampoline:
    case kLoadIC:
    case kLoadIC_FunctionPrototype:
    case kLoadIC_Noninlined:
    case kLoadIC_Slow:
    case kLoadIC_StringLength:
    case kLoadIC_StringWrapperLength:
    case kLoadICTrampoline:
    case kLoadIC_Uninitialized:
    case kMapPrototypeEntries:
    case kMapPrototypeForEach:
    case kMapPrototypeGet:
    case kMapPrototypeGetSize:
    case kMapPrototypeHas:
    case kMapPrototypeKeys:
    case kMapPrototypeValues:
    case kMathAcos:
    case kMathAcosh:
    case kMathAsin:
    case kMathAsinh:
    case kMathAtan:
    case kMathAtan2:
    case kMathAtanh:
    case kMathCbrt:
    case kMathCeil:
    case kMathCos:
    case kMathCosh:
    case kMathExp:
    case kMathExpm1:
    case kMathFloor:
    case kMathFround:
    case kMathLog:
    case kMathLog10:
    case kMathLog1p:
    case kMathLog2:
    case kMathMax:
    case kMathMin:
    case kMathRound:
    case kMathSign:
    case kMathSin:
    case kMathSinh:
    case kMathSqrt:
    case kMathTan:
    case kMathTanh:
    case kMathTrunc:
    case kModulus:
    case kMultiply:
    case kNegate:
    case kNewArgumentsElements:
    case kNonNumberToNumber:
    case kNonNumberToNumeric:
    case kNonPrimitiveToPrimitive_Default:
    case kNonPrimitiveToPrimitive_Number:
    case kNonPrimitiveToPrimitive_String:
    case kNumberConstructor:
    case kNumberIsFinite:
    case kNumberIsInteger:
    case kNumberIsNaN:
    case kNumberIsSafeInteger:
    case kNumberParseFloat:
    case kNumberPrototypeValueOf:
    case kNumberToString:
    case kObjectConstructor:
    case kObjectConstructor_ConstructStub:
    case kObjectCreate:
    case kObjectIs:
    case kObjectKeys:
    case kObjectPrototypeHasOwnProperty:
    case kObjectPrototypeIsPrototypeOf:
    case kObjectPrototypeToLocaleString:
    case kObjectPrototypeToString:
    case kObjectPrototypeValueOf:
    case kOrderedHashTableHealIndex:
    case kOrdinaryHasInstance:
    case kOrdinaryToPrimitive_Number:
    case kOrdinaryToPrimitive_String:
    case kPromiseAll:
    case kPromiseCapabilityDefaultReject:
    case kPromiseCapabilityDefaultResolve:
    case kPromiseCatchFinally:
    case kPromiseConstructor:
    case kPromiseConstructorLazyDeoptContinuation:
    case kPromiseFulfillReactionJob:
    case kPromiseInternalConstructor:
    case kPromiseInternalReject:
    case kPromiseInternalResolve:
    case kPromisePrototypeCatch:
    case kPromisePrototypeFinally:
    case kPromiseRace:
    case kPromiseReject:
    case kPromiseRejectReactionJob:
    case kPromiseResolve:
    case kPromiseResolveThenableJob:
    case kPromiseResolveTrampoline:
    case kPromiseThenFinally:
    case kPromiseThrowerFinally:
    case kPromiseValueThunkFinally:
    case kProxyConstructor:
    case kProxyGetProperty:
    case kProxyHasProperty:
    case kProxySetProperty:
    case kRecordWrite:
    case kReflectHas:
    case kRegExpConstructor:
    case kRegExpPrototypeCompile:
    case kRegExpPrototypeDotAllGetter:
    case kRegExpPrototypeFlagsGetter:
    case kRegExpPrototypeGlobalGetter:
    case kRegExpPrototypeIgnoreCaseGetter:
    case kRegExpPrototypeMultilineGetter:
    case kRegExpPrototypeReplace:
    case kRegExpPrototypeSearch:
    case kRegExpPrototypeSourceGetter:
    case kRegExpPrototypeSplit:
    case kRegExpPrototypeStickyGetter:
    case kRegExpPrototypeUnicodeGetter:
    case kResolvePromise:
    case kReturnReceiver:
    case kRunMicrotasks:
    case kSameValue:
    case kSetPrototypeEntries:
    case kSetPrototypeForEach:
    case kSetPrototypeGetSize:
    case kSetPrototypeHas:
    case kSetPrototypeValues:
    case kStoreGlobalIC_Slow:
    case kStoreGlobalICTrampoline:
    case kStoreICTrampoline:
    case kStrictEqual:
    case kStringCodePointAtUTF16:
    case kStringCodePointAtUTF32:
    case kStringConstructor:
    case kStringEqual:
    case kStringGreaterThan:
    case kStringGreaterThanOrEqual:
    case kStringIndexOf:
    case kStringLessThan:
    case kStringLessThanOrEqual:
    case kStringPrototypeAnchor:
    case kStringPrototypeBig:
    case kStringPrototypeBlink:
    case kStringPrototypeBold:
    case kStringPrototypeCharCodeAt:
    case kStringPrototypeCodePointAt:
    case kStringPrototypeConcat:
    case kStringPrototypeFixed:
    case kStringPrototypeFontcolor:
    case kStringPrototypeFontsize:
    case kStringPrototypeIncludes:
    case kStringPrototypeIndexOf:
    case kStringPrototypeItalics:
    case kStringPrototypeIterator:
    case kStringPrototypeLink:
    case kStringPrototypeMatch:
    case kStringPrototypePadEnd:
    case kStringPrototypePadStart:
    case kStringPrototypeRepeat:
    case kStringPrototypeReplace:
    case kStringPrototypeSearch:
    case kStringPrototypeSmall:
    case kStringPrototypeStrike:
    case kStringPrototypeSub:
    case kStringPrototypeSup:
#ifdef V8_INTL_SUPPORT
    case kStringPrototypeToLowerCaseIntl:
    case kStringToLowerCaseIntl:
#endif
    case kStringPrototypeToString:
    case kStringPrototypeValueOf:
    case kStringRepeat:
    case kStringToNumber:
    case kSubtract:
    case kSymbolPrototypeToPrimitive:
    case kSymbolPrototypeToString:
    case kSymbolPrototypeValueOf:
    case kThrowWasmTrapDivByZero:
    case kThrowWasmTrapDivUnrepresentable:
    case kThrowWasmTrapFloatUnrepresentable:
    case kThrowWasmTrapFuncInvalid:
    case kThrowWasmTrapFuncSigMismatch:
    case kThrowWasmTrapMemOutOfBounds:
    case kThrowWasmTrapRemByZero:
    case kThrowWasmTrapUnreachable:
    case kToBoolean:
    case kToBooleanLazyDeoptContinuation:
    case kToInteger:
    case kToInteger_TruncateMinusZero:
    case kToName:
    case kToNumber:
    case kToNumeric:
    case kToString:
    case kTypedArrayConstructor:
    case kTypedArrayConstructor_ConstructStub:
    case kTypedArrayPrototypeByteLength:
    case kTypedArrayPrototypeByteOffset:
    case kTypedArrayPrototypeEntries:
    case kTypedArrayPrototypeEvery:
    case kTypedArrayPrototypeFind:
    case kTypedArrayPrototypeFindIndex:
    case kTypedArrayPrototypeForEach:
    case kTypedArrayPrototypeKeys:
    case kTypedArrayPrototypeLength:
    case kTypedArrayPrototypeReduce:
    case kTypedArrayPrototypeReduceRight:
    case kTypedArrayPrototypeSet:
    case kTypedArrayPrototypeSlice:
    case kTypedArrayPrototypeSome:
    case kTypedArrayPrototypeSubArray:
    case kTypedArrayPrototypeToStringTag:
    case kTypedArrayPrototypeValues:
    case kTypeof:
    case kWasmStackGuard:
    case kWeakMapGet:
    case kWeakMapHas:
    case kWeakMapLookupHashIndex:
    case kWeakMapPrototypeDelete:
    case kWeakMapPrototypeSet:
    case kWeakSetHas:
    case kWeakSetPrototypeAdd:
    case kWeakSetPrototypeDelete:
#endif
      return true;
    default:
      return false;
  }
  UNREACHABLE();
}

// static
bool Builtins::IsOffHeapSafe(int index) {
#ifndef V8_EMBEDDED_BUILTINS
  return false;
#else
  DCHECK(IsBuiltinId(index));
  if (IsTooShortForOffHeapTrampoline(index)) return false;
  switch (index) {
#ifdef DEBUG
    case kAbortJS:
    case kAllocateHeapNumber:
    case kArrayEveryLoopEagerDeoptContinuation:
    case kArrayFilterLoopEagerDeoptContinuation:
    case kArrayFindIndexLoopEagerDeoptContinuation:
    case kArrayFindIndexLoopLazyDeoptContinuation:
    case kArrayFindLoopEagerDeoptContinuation:
    case kArrayFindLoopLazyDeoptContinuation:
    case kArrayForEachLoopEagerDeoptContinuation:
    case kArrayForEachLoopLazyDeoptContinuation:
    case kArrayMapLoopEagerDeoptContinuation:
    case kArrayReduceLoopEagerDeoptContinuation:
    case kArrayReduceLoopLazyDeoptContinuation:
    case kArrayReducePreLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopLazyDeoptContinuation:
    case kArrayReduceRightPreLoopEagerDeoptContinuation:
    case kArraySomeLoopEagerDeoptContinuation:
    case kBitwiseNot:
    case kBooleanPrototypeToString:
    case kBooleanPrototypeValueOf:
    case kContinueToCodeStubBuiltin:
    case kContinueToCodeStubBuiltinWithResult:
    case kContinueToJavaScriptBuiltin:
    case kContinueToJavaScriptBuiltinWithResult:
    case kDatePrototypeGetDate:
    case kDatePrototypeGetDay:
    case kDatePrototypeGetFullYear:
    case kDatePrototypeGetHours:
    case kDatePrototypeGetMilliseconds:
    case kDatePrototypeGetMinutes:
    case kDatePrototypeGetMonth:
    case kDatePrototypeGetSeconds:
    case kDatePrototypeGetTime:
    case kDatePrototypeGetTimezoneOffset:
    case kDatePrototypeGetUTCDate:
    case kDatePrototypeGetUTCDay:
    case kDatePrototypeGetUTCFullYear:
    case kDatePrototypeGetUTCHours:
    case kDatePrototypeGetUTCMilliseconds:
    case kDatePrototypeGetUTCMinutes:
    case kDatePrototypeGetUTCMonth:
    case kDatePrototypeGetUTCSeconds:
    case kDatePrototypeToPrimitive:
    case kDatePrototypeValueOf:
    case kDecrement:
    case kDivide:
    case kGlobalIsFinite:
    case kGlobalIsNaN:
    case kIncrement:
    case kKeyedLoadIC_Slow:
    case kKeyedLoadICTrampoline:
    case kKeyedStoreIC_Slow:
    case kKeyedStoreICTrampoline:
    case kLoadField:
    case kLoadGlobalICInsideTypeofTrampoline:
    case kLoadGlobalIC_Slow:
    case kLoadGlobalICTrampoline:
    case kLoadIC_Slow:
    case kLoadICTrampoline:
    case kMapPrototypeEntries:
    case kMapPrototypeGet:
    case kMapPrototypeGetSize:
    case kMapPrototypeHas:
    case kMapPrototypeKeys:
    case kMapPrototypeValues:
    case kMathAcos:
    case kMathAcosh:
    case kMathAsin:
    case kMathAsinh:
    case kMathAtan:
    case kMathAtan2:
    case kMathAtanh:
    case kMathCbrt:
    case kMathCeil:
    case kMathCos:
    case kMathCosh:
    case kMathExp:
    case kMathExpm1:
    case kMathFloor:
    case kMathFround:
    case kMathLog:
    case kMathLog10:
    case kMathLog1p:
    case kMathLog2:
    case kMathMax:
    case kMathMin:
    case kMathRound:
    case kMathSign:
    case kMathSin:
    case kMathSinh:
    case kMathSqrt:
    case kMathTan:
    case kMathTanh:
    case kMathTrunc:
    case kModulus:
    case kMultiply:
    case kNonPrimitiveToPrimitive_Default:
    case kNonPrimitiveToPrimitive_Number:
    case kNonPrimitiveToPrimitive_String:
    case kNumberIsFinite:
    case kNumberIsInteger:
    case kNumberIsNaN:
    case kNumberIsSafeInteger:
    case kNumberPrototypeValueOf:
    case kObjectPrototypeToLocaleString:
    case kObjectPrototypeValueOf:
    case kPromiseCapabilityDefaultReject:
    case kPromiseCapabilityDefaultResolve:
    case kPromiseConstructorLazyDeoptContinuation:
    case kPromiseInternalReject:
    case kPromiseInternalResolve:
    case kPromiseResolveTrampoline:
    case kPromiseThrowerFinally:
    case kPromiseValueThunkFinally:
    case kProxyConstructor:
    case kReflectHas:
    case kRegExpPrototypeDotAllGetter:
    case kRegExpPrototypeGlobalGetter:
    case kRegExpPrototypeIgnoreCaseGetter:
    case kRegExpPrototypeMultilineGetter:
    case kRegExpPrototypeSourceGetter:
    case kRegExpPrototypeStickyGetter:
    case kRegExpPrototypeUnicodeGetter:
    case kReturnReceiver:
    case kSetPrototypeEntries:
    case kSetPrototypeGetSize:
    case kSetPrototypeValues:
    case kStoreGlobalIC_Slow:
    case kStoreGlobalICTrampoline:
    case kStoreICTrampoline:
    case kStringPrototypeBig:
    case kStringPrototypeBlink:
    case kStringPrototypeBold:
    case kStringPrototypeConcat:
    case kStringPrototypeFixed:
    case kStringPrototypeItalics:
    case kStringPrototypeIterator:
    case kStringPrototypeSmall:
    case kStringPrototypeStrike:
    case kStringPrototypeSub:
    case kStringPrototypeSup:
#ifdef V8_INTL_SUPPORT
    case kStringPrototypeToLowerCaseIntl:
#endif
    case kSubtract:
    case kSymbolPrototypeToPrimitive:
    case kSymbolPrototypeToString:
    case kSymbolPrototypeValueOf:
    case kThrowWasmTrapDivByZero:
    case kThrowWasmTrapDivUnrepresentable:
    case kThrowWasmTrapFloatUnrepresentable:
    case kThrowWasmTrapFuncInvalid:
    case kThrowWasmTrapFuncSigMismatch:
    case kThrowWasmTrapMemOutOfBounds:
    case kThrowWasmTrapRemByZero:
    case kThrowWasmTrapUnreachable:
    case kToInteger:
    case kTypedArrayConstructor:
    case kWasmStackGuard:
    case kWeakMapGet:
    case kWeakMapHas:
    case kWeakMapPrototypeDelete:
    case kWeakMapPrototypeSet:
    case kWeakSetHas:
    case kWeakSetPrototypeAdd:
    case kWeakSetPrototypeDelete:
#else
    case kAbortJS:
    case kAdd:
    case kAllocateHeapNumber:
    case kArrayEvery:
    case kArrayEveryLoopContinuation:
    case kArrayEveryLoopEagerDeoptContinuation:
    case kArrayEveryLoopLazyDeoptContinuation:
    case kArrayFilterLoopEagerDeoptContinuation:
    case kArrayFilterLoopLazyDeoptContinuation:
    case kArrayFindIndexLoopAfterCallbackLazyDeoptContinuation:
    case kArrayFindIndexLoopContinuation:
    case kArrayFindIndexLoopEagerDeoptContinuation:
    case kArrayFindIndexLoopLazyDeoptContinuation:
    case kArrayFindLoopAfterCallbackLazyDeoptContinuation:
    case kArrayFindLoopContinuation:
    case kArrayFindLoopEagerDeoptContinuation:
    case kArrayFindLoopLazyDeoptContinuation:
    case kArrayForEach:
    case kArrayForEachLoopContinuation:
    case kArrayForEachLoopEagerDeoptContinuation:
    case kArrayForEachLoopLazyDeoptContinuation:
    case kArrayFrom:
    case kArrayIncludes:
    case kArrayIndexOf:
    case kArrayIsArray:
    case kArrayMapLoopContinuation:
    case kArrayMapLoopEagerDeoptContinuation:
    case kArrayMapLoopLazyDeoptContinuation:
    case kArrayOf:
    case kArrayPrototypeEntries:
    case kArrayPrototypeFind:
    case kArrayPrototypeFindIndex:
    case kArrayPrototypeKeys:
    case kArrayPrototypeSlice:
    case kArrayPrototypeValues:
    case kArrayReduce:
    case kArrayReduceLoopContinuation:
    case kArrayReduceLoopEagerDeoptContinuation:
    case kArrayReduceLoopLazyDeoptContinuation:
    case kArrayReducePreLoopEagerDeoptContinuation:
    case kArrayReduceRight:
    case kArrayReduceRightLoopContinuation:
    case kArrayReduceRightLoopEagerDeoptContinuation:
    case kArrayReduceRightLoopLazyDeoptContinuation:
    case kArrayReduceRightPreLoopEagerDeoptContinuation:
    case kArraySome:
    case kArraySomeLoopContinuation:
    case kArraySomeLoopEagerDeoptContinuation:
    case kArraySomeLoopLazyDeoptContinuation:
    case kAsyncFromSyncIteratorPrototypeNext:
    case kAsyncFromSyncIteratorPrototypeReturn:
    case kAsyncFromSyncIteratorPrototypeThrow:
    case kAsyncFunctionAwaitFulfill:
    case kAsyncFunctionAwaitReject:
    case kAsyncFunctionPromiseCreate:
    case kAsyncFunctionPromiseRelease:
    case kAsyncGeneratorAwaitFulfill:
    case kAsyncGeneratorAwaitReject:
    case kAsyncGeneratorResumeNext:
    case kAsyncGeneratorReturnClosedFulfill:
    case kAsyncGeneratorReturnClosedReject:
    case kAsyncGeneratorReturnFulfill:
    case kAsyncGeneratorYieldFulfill:
    case kAsyncIteratorValueUnwrap:
    case kBitwiseNot:
    case kBooleanPrototypeToString:
    case kBooleanPrototypeValueOf:
    case kCallProxy:
    case kConstructFunction:
    case kConstructProxy:
    case kContinueToCodeStubBuiltin:
    case kContinueToCodeStubBuiltinWithResult:
    case kContinueToJavaScriptBuiltin:
    case kContinueToJavaScriptBuiltinWithResult:
    case kCreateGeneratorObject:
    case kCreateIterResultObject:
    case kCreateRegExpLiteral:
    case kDatePrototypeGetDate:
    case kDatePrototypeGetDay:
    case kDatePrototypeGetFullYear:
    case kDatePrototypeGetHours:
    case kDatePrototypeGetMilliseconds:
    case kDatePrototypeGetMinutes:
    case kDatePrototypeGetMonth:
    case kDatePrototypeGetSeconds:
    case kDatePrototypeGetTime:
    case kDatePrototypeGetTimezoneOffset:
    case kDatePrototypeGetUTCDate:
    case kDatePrototypeGetUTCDay:
    case kDatePrototypeGetUTCFullYear:
    case kDatePrototypeGetUTCHours:
    case kDatePrototypeGetUTCMilliseconds:
    case kDatePrototypeGetUTCMinutes:
    case kDatePrototypeGetUTCMonth:
    case kDatePrototypeGetUTCSeconds:
    case kDatePrototypeToPrimitive:
    case kDatePrototypeValueOf:
    case kDecrement:
    case kDeleteProperty:
    case kDivide:
    case kEqual:
    case kFastConsoleAssert:
    case kFastNewClosure:
    case kFastNewFunctionContextEval:
    case kFastNewFunctionContextFunction:
    case kFastNewObject:
    case kFindOrderedHashMapEntry:
    case kForInEnumerate:
    case kForInFilter:
    case kFunctionPrototypeHasInstance:
    case kGeneratorPrototypeNext:
    case kGeneratorPrototypeReturn:
    case kGeneratorPrototypeThrow:
    case kGetSuperConstructor:
    case kGlobalIsFinite:
    case kGlobalIsNaN:
    case kGreaterThan:
    case kGreaterThanOrEqual:
    case kHasProperty:
    case kIncrement:
    case kInstanceOf:
    case kKeyedLoadIC_Megamorphic:
    case kKeyedLoadIC_PolymorphicName:
    case kKeyedLoadIC_Slow:
    case kKeyedLoadICTrampoline:
    case kKeyedStoreIC_Slow:
    case kKeyedStoreICTrampoline:
    case kLessThan:
    case kLessThanOrEqual:
    case kLoadField:
    case kLoadGlobalIC:
    case kLoadGlobalICInsideTypeof:
    case kLoadGlobalICInsideTypeofTrampoline:
    case kLoadGlobalIC_Slow:
    case kLoadGlobalICTrampoline:
    case kLoadIC:
    case kLoadIC_FunctionPrototype:
    case kLoadIC_Noninlined:
    case kLoadIC_Slow:
    case kLoadICTrampoline:
    case kLoadIC_Uninitialized:
    case kMapPrototypeEntries:
    case kMapPrototypeForEach:
    case kMapPrototypeGet:
    case kMapPrototypeGetSize:
    case kMapPrototypeHas:
    case kMapPrototypeKeys:
    case kMapPrototypeValues:
    case kMathAcos:
    case kMathAcosh:
    case kMathAsin:
    case kMathAsinh:
    case kMathAtan:
    case kMathAtan2:
    case kMathAtanh:
    case kMathCbrt:
    case kMathCeil:
    case kMathCos:
    case kMathCosh:
    case kMathExp:
    case kMathExpm1:
    case kMathFloor:
    case kMathFround:
    case kMathLog:
    case kMathLog10:
    case kMathLog1p:
    case kMathLog2:
    case kMathMax:
    case kMathMin:
    case kMathRound:
    case kMathSign:
    case kMathSin:
    case kMathSinh:
    case kMathSqrt:
    case kMathTan:
    case kMathTanh:
    case kMathTrunc:
    case kModulus:
    case kMultiply:
    case kNegate:
    case kNewArgumentsElements:
    case kNonNumberToNumber:
    case kNonNumberToNumeric:
    case kNonPrimitiveToPrimitive_Default:
    case kNonPrimitiveToPrimitive_Number:
    case kNonPrimitiveToPrimitive_String:
    case kNumberConstructor:
    case kNumberIsFinite:
    case kNumberIsInteger:
    case kNumberIsNaN:
    case kNumberIsSafeInteger:
    case kNumberParseFloat:
    case kNumberPrototypeValueOf:
    case kNumberToString:
    case kObjectConstructor:
    case kObjectConstructor_ConstructStub:
    case kObjectCreate:
    case kObjectIs:
    case kObjectKeys:
    case kObjectPrototypeHasOwnProperty:
    case kObjectPrototypeIsPrototypeOf:
    case kObjectPrototypeToLocaleString:
    case kObjectPrototypeToString:
    case kObjectPrototypeValueOf:
    case kOrderedHashTableHealIndex:
    case kOrdinaryHasInstance:
    case kOrdinaryToPrimitive_Number:
    case kOrdinaryToPrimitive_String:
    case kPromiseAll:
    case kPromiseCapabilityDefaultReject:
    case kPromiseCapabilityDefaultResolve:
    case kPromiseCatchFinally:
    case kPromiseConstructor:
    case kPromiseConstructorLazyDeoptContinuation:
    case kPromiseFulfillReactionJob:
    case kPromiseInternalConstructor:
    case kPromiseInternalReject:
    case kPromiseInternalResolve:
    case kPromisePrototypeCatch:
    case kPromisePrototypeFinally:
    case kPromiseRace:
    case kPromiseReject:
    case kPromiseRejectReactionJob:
    case kPromiseResolve:
    case kPromiseResolveThenableJob:
    case kPromiseResolveTrampoline:
    case kPromiseThenFinally:
    case kPromiseThrowerFinally:
    case kPromiseValueThunkFinally:
    case kProxyConstructor:
    case kProxyGetProperty:
    case kProxyHasProperty:
    case kProxySetProperty:
    case kRecordWrite:
    case kReflectHas:
    case kRegExpConstructor:
    case kRegExpPrototypeCompile:
    case kRegExpPrototypeDotAllGetter:
    case kRegExpPrototypeFlagsGetter:
    case kRegExpPrototypeGlobalGetter:
    case kRegExpPrototypeIgnoreCaseGetter:
    case kRegExpPrototypeMultilineGetter:
    case kRegExpPrototypeReplace:
    case kRegExpPrototypeSearch:
    case kRegExpPrototypeSourceGetter:
    case kRegExpPrototypeSplit:
    case kRegExpPrototypeStickyGetter:
    case kRegExpPrototypeUnicodeGetter:
    case kResolvePromise:
    case kReturnReceiver:
    case kRunMicrotasks:
    case kSameValue:
    case kSetPrototypeEntries:
    case kSetPrototypeForEach:
    case kSetPrototypeGetSize:
    case kSetPrototypeHas:
    case kSetPrototypeValues:
    case kStoreGlobalIC_Slow:
    case kStoreGlobalICTrampoline:
    case kStoreICTrampoline:
    case kStrictEqual:
    case kStringCodePointAtUTF16:
    case kStringCodePointAtUTF32:
    case kStringConstructor:
    case kStringEqual:
    case kStringGreaterThan:
    case kStringGreaterThanOrEqual:
    case kStringIndexOf:
    case kStringLessThan:
    case kStringLessThanOrEqual:
    case kStringPrototypeAnchor:
    case kStringPrototypeBig:
    case kStringPrototypeBlink:
    case kStringPrototypeBold:
    case kStringPrototypeCharCodeAt:
    case kStringPrototypeCodePointAt:
    case kStringPrototypeConcat:
    case kStringPrototypeFixed:
    case kStringPrototypeFontcolor:
    case kStringPrototypeFontsize:
    case kStringPrototypeIncludes:
    case kStringPrototypeIndexOf:
    case kStringPrototypeItalics:
    case kStringPrototypeIterator:
    case kStringPrototypeLink:
    case kStringPrototypeMatch:
    case kStringPrototypePadEnd:
    case kStringPrototypePadStart:
    case kStringPrototypeRepeat:
    case kStringPrototypeReplace:
    case kStringPrototypeSearch:
    case kStringPrototypeSmall:
    case kStringPrototypeStrike:
    case kStringPrototypeSub:
    case kStringPrototypeSup:
#ifdef V8_INTL_SUPPORT
    case kStringPrototypeToLowerCaseIntl:
    case kStringToLowerCaseIntl:
#endif
    case kStringPrototypeToString:
    case kStringPrototypeValueOf:
    case kStringRepeat:
    case kStringToNumber:
    case kSubtract:
    case kSymbolPrototypeToPrimitive:
    case kSymbolPrototypeToString:
    case kSymbolPrototypeValueOf:
    case kThrowWasmTrapDivByZero:
    case kThrowWasmTrapDivUnrepresentable:
    case kThrowWasmTrapFloatUnrepresentable:
    case kThrowWasmTrapFuncInvalid:
    case kThrowWasmTrapFuncSigMismatch:
    case kThrowWasmTrapMemOutOfBounds:
    case kThrowWasmTrapRemByZero:
    case kThrowWasmTrapUnreachable:
    case kToBoolean:
    case kToBooleanLazyDeoptContinuation:
    case kToInteger:
    case kToInteger_TruncateMinusZero:
    case kToName:
    case kToNumber:
    case kToNumeric:
    case kToString:
    case kTypedArrayConstructor:
    case kTypedArrayConstructor_ConstructStub:
    case kTypedArrayPrototypeByteLength:
    case kTypedArrayPrototypeByteOffset:
    case kTypedArrayPrototypeEntries:
    case kTypedArrayPrototypeEvery:
    case kTypedArrayPrototypeFind:
    case kTypedArrayPrototypeFindIndex:
    case kTypedArrayPrototypeForEach:
    case kTypedArrayPrototypeKeys:
    case kTypedArrayPrototypeLength:
    case kTypedArrayPrototypeReduce:
    case kTypedArrayPrototypeReduceRight:
    case kTypedArrayPrototypeSet:
    case kTypedArrayPrototypeSlice:
    case kTypedArrayPrototypeSome:
    case kTypedArrayPrototypeSubArray:
    case kTypedArrayPrototypeToStringTag:
    case kTypedArrayPrototypeValues:
    case kTypeof:
    case kWasmStackGuard:
    case kWeakMapGet:
    case kWeakMapHas:
    case kWeakMapLookupHashIndex:
    case kWeakMapPrototypeDelete:
    case kWeakMapPrototypeSet:
    case kWeakSetHas:
    case kWeakSetPrototypeAdd:
    case kWeakSetPrototypeDelete:
#endif  // !DEBUG
      return true;
    default:
      return false;
  }
  UNREACHABLE();
#endif  // V8_EMBEDDED_BUILTINS
}

// static
bool Builtins::IsTooShortForOffHeapTrampoline(int index) {
  switch (index) {
    case kLoadIC_StringLength:
    case kLoadIC_StringWrapperLength:
      return true;
    default:
      return false;
  }
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
