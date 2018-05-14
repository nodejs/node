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
bool Builtins::IsBuiltin(const Code* code) {
  return Builtins::IsBuiltinId(code->builtin_index());
}

// static
bool Builtins::IsEmbeddedBuiltin(const Code* code) {
#ifdef V8_EMBEDDED_BUILTINS
  return Builtins::IsBuiltinId(code->builtin_index()) &&
         Builtins::IsIsolateIndependent(code->builtin_index());
#else
  return false;
#endif
}

// static
bool Builtins::IsLazy(int index) {
  DCHECK(IsBuiltinId(index));

#ifdef V8_EMBEDDED_BUILTINS
  // We don't want to lazy-deserialize off-heap builtins.
  if (Builtins::IsIsolateIndependent(index)) return false;
#endif

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
    case kTypedArrayConstructorLazyDeoptContinuation:
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
    case kContinueToCodeStubBuiltin:
    case kContinueToCodeStubBuiltinWithResult:
    case kContinueToJavaScriptBuiltin:
    case kContinueToJavaScriptBuiltinWithResult:
    case kKeyedLoadIC_Slow:
    case kKeyedStoreIC_Slow:
    case kLoadGlobalIC_Slow:
    case kLoadIC_Slow:
    case kStoreGlobalIC_Slow:
    case kWasmStackGuard:
    case kThrowWasmTrapUnreachable:
    case kThrowWasmTrapMemOutOfBounds:
    case kThrowWasmTrapDivByZero:
    case kThrowWasmTrapDivUnrepresentable:
    case kThrowWasmTrapRemByZero:
    case kThrowWasmTrapFloatUnrepresentable:
    case kThrowWasmTrapFuncInvalid:
    case kThrowWasmTrapFuncSigMismatch:
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
    case kAsyncFunctionAwaitCaught:
    case kAsyncFunctionPromiseCreate:
    case kAsyncFunctionPromiseRelease:
    case kAsyncGeneratorResumeNext:
    case kAsyncGeneratorReturnClosedRejectClosure:
    case kAsyncGeneratorReturn:
    case kAsyncIteratorValueUnwrap:
    case kBitwiseNot:
    case kBooleanPrototypeToString:
    case kBooleanPrototypeValueOf:
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
    case kMathCeil:
    case kMathFloor:
    case kMathFround:
    case kMathMax:
    case kMathMin:
    case kMathRound:
    case kMathSign:
    case kMathSqrt:
    case kMathTrunc:
    case kMultiply:
    case kNegate:
    case kNewArgumentsElements:
    case kNonNumberToNumber:
    case kNonNumberToNumeric:
    case kNonPrimitiveToPrimitive_Default:
    case kNonPrimitiveToPrimitive_Number:
    case kNonPrimitiveToPrimitive_String:
    case kNumberIsFinite:
    case kNumberIsInteger:
    case kNumberIsNaN:
    case kNumberIsSafeInteger:
    case kNumberParseFloat:
    case kNumberPrototypeValueOf:
    case kNumberToString:
    case kObjectConstructor:
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
    case kProxyGetProperty:
    case kProxyHasProperty:
    case kProxySetProperty:
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
}

#ifdef V8_EMBEDDED_BUILTINS
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
#endif  // V8_EMBEDDED_BUILTINS

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
