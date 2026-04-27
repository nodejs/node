// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DEFINITIONS_H_
#define V8_BUILTINS_BUILTINS_DEFINITIONS_H_

#include "builtins-generated/bytecodes-builtins-list.h"
#include "src/common/globals.h"

// include generated header
#include "torque-generated/builtin-definitions.h"

namespace v8 {
namespace internal {

#if V8_ENABLE_GEARBOX
#define WITH_GEARBOX(KIND, NAME, ...) \
  KIND(NAME##_Generic, __VA_ARGS__)   \
  KIND(NAME##_ISX, __VA_ARGS__)       \
  KIND(NAME, __VA_ARGS__)

constexpr int kGearboxISXBuiltinIdOffset = -1;
constexpr int kGearboxGenericBuiltinIdOffset = -2;
#else
#define WITH_GEARBOX(KIND, NAME, ...) KIND(NAME, __VA_ARGS__)
#endif  // V8_ENABLE_GEARBOX

// CPP: Builtin in C++. Entered via BUILTIN_EXIT frame.
//      Args: name, formal parameter count
// TFJ: Builtin in Turbofan, with JS linkage (callable as Javascript function).
//      Args: name, formal parameter count, explicit argument names...
// TFS: Builtin in Turbofan, with CodeStub linkage.
//      Args: name, needs context, explicit argument names...
// TFC: Builtin in Turbofan, with CodeStub linkage and custom descriptor.
//      Args: name, interface descriptor
// TFH: Handlers in Turbofan, with CodeStub linkage.
//      Args: name, interface descriptor
// BCH: Bytecode Handlers, with bytecode dispatch linkage.
//      Args: name, OperandScale, Bytecode
// ASM: Builtin in platform-dependent assembly.
//      Args: name, interface descriptor

// Versions of the above builtins, but defined using Turboshaft Assembler.
// TFJ_TSA: Builtin in Turboshaft, with JS linkage (callable as Javascript
//          function).
//          Args: name, formal parameter count, explicit argument names...
// TFC_TSA: Builtin in Turboshaft, with CodeStub linkage and custom descriptor.
//          Args: name, interface descriptor
// BCH_TSA: Bytecode Handlers in Turboshaft, with bytecode dispatch linkage.
//          Args: name, OperandScale, Bytecode

// Builtins are additionally split into tiers, where the tier determines the
// distance of the builtins table from the root register within IsolateData.
//
//  - Tier 0 (T0) are guaranteed to be close to the root register and can thus
//    be accessed efficiently root-relative calls (so not, e.g., calls from
//    generated code when short-builtin-calls is on).
//  - T1 builtins have no distance guarantees.
//
// Note, this mechanism works only if the set of T0 builtins is kept as small
// as possible. Please, resist the temptation to add your builtin here unless
// there's a very good reason.
#define BUILTIN_LIST_BASE_TIER0(CPP, TFJ, TFC, TFS, TFH, ASM)               \
  /* Deoptimization entries. */                                             \
  ASM(DeoptimizationEntry_Eager, DeoptimizationEntry)                       \
  ASM(DeoptimizationEntry_Lazy, DeoptimizationEntry)                        \
  ASM(DeoptimizationEntry_LazyAfterFastCall, DeoptimizationEntry)           \
                                                                            \
  /* GC write barrier. */                                                   \
  TFC(RecordWriteSaveFP, WriteBarrier)                                      \
  TFC(RecordWriteIgnoreFP, WriteBarrier)                                    \
  TFC(EphemeronKeyBarrierSaveFP, WriteBarrier)                              \
  TFC(EphemeronKeyBarrierIgnoreFP, WriteBarrier)                            \
                                                                            \
  /* TODO(ishell): dummy builtin added here just to keep the Tier0 table */ \
  /* size unmodified to avoid unexpected performance implications. */       \
  /* It should be removed. */

#define LOAD_IC_IN_OBJECT_FIELD_WITH_INDEX_HANDLER_LIST(V, GENERATE_MACRO) \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 0)                         \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 1)                         \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 2)                         \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 3)                         \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 4)                         \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 5)                         \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 6)                         \
  GENERATE_MACRO(V, InObject, NonDouble, Field, 7)

#define LOAD_IC_OUT_OF_OBJECT_FIELD_WITH_INDEX_HANDLER_LIST(V, GENERATE_MACRO) \
  GENERATE_MACRO(V, OutOfObject, NonDouble, Field, 0)                          \
  GENERATE_MACRO(V, OutOfObject, NonDouble, Field, 1)                          \
  GENERATE_MACRO(V, OutOfObject, NonDouble, Field, 2)                          \
  GENERATE_MACRO(V, OutOfObject, NonDouble, Field, 3)

#define LOAD_IC_HANDLER_LIST(V, GENERATE_MACRO)                                \
  GENERATE_MACRO(V, /*Location*/, /*Representation*/, Uninitialized,           \
                 /*Index*/)                                                    \
  GENERATE_MACRO(V, InObject, NonDouble, Field, /*Index*/)                     \
  LOAD_IC_IN_OBJECT_FIELD_WITH_INDEX_HANDLER_LIST(V, GENERATE_MACRO)           \
  GENERATE_MACRO(V, OutOfObject, NonDouble, Field, /*Index*/)                  \
  LOAD_IC_OUT_OF_OBJECT_FIELD_WITH_INDEX_HANDLER_LIST(V, GENERATE_MACRO)       \
  GENERATE_MACRO(V, /*Location*/, Double, Field, /*Index*/)                    \
  GENERATE_MACRO(V, /*Location*/, /*Representation*/, ConstantFromPrototype,   \
                 /*Index*/)                                                    \
  GENERATE_MACRO(V, /*Location*/, /*Representation*/, StringLength, /*Index*/) \
  GENERATE_MACRO(V, /*Location*/, /*Representation*/, Generic, /*Index*/)

#define GENERATE_BUILTIN_LOAD_IC_DEFINITION(V, Location, Representation, Kind, \
                                            Index)                             \
  V(LoadIC##Location##Representation##Kind##Index##Baseline, LoadBaseline)

#define BUILTIN_LOAD_IC_HANDLER_LIST(V) \
  LOAD_IC_HANDLER_LIST(V, GENERATE_BUILTIN_LOAD_IC_DEFINITION)

#ifdef V8_ENABLE_SPARKPLUG_PLUS
#define TYPED_COMPARE_OPERATION_HANDLER_HELPER(V, OPERATION, TYPE) \
  V(OPERATION##_##TYPE##_Baseline, Compare_WithEmbeddedFeedbackOffset)

#define TYPED_STRICTEQUAL_HANDLER_HELPER(V, TYPE) \
  TYPED_COMPARE_OPERATION_HANDLER_HELPER(V, StrictEqual, TYPE)

#define TYPED_EQUAL_HANDLER_HELPER(V, TYPE) \
  TYPED_COMPARE_OPERATION_HANDLER_HELPER(V, Equal, TYPE)

#define GENERATE_BUILTIN_TYPED_STRICTEQUAL_HANDLER(V)     \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, Any)                \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, Symbol)             \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, Number)             \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, Receiver)           \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, String)             \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, InternalizedString) \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, SignedSmall)        \
  TYPED_STRICTEQUAL_HANDLER_HELPER(V, None)

#define GENERATE_BUILTIN_TYPED_EQUAL_HANDLER(V)     \
  TYPED_EQUAL_HANDLER_HELPER(V, Any)                \
  TYPED_EQUAL_HANDLER_HELPER(V, Number)             \
  TYPED_EQUAL_HANDLER_HELPER(V, String)             \
  TYPED_EQUAL_HANDLER_HELPER(V, InternalizedString) \
  TYPED_EQUAL_HANDLER_HELPER(V, Receiver)           \
  TYPED_EQUAL_HANDLER_HELPER(V, SignedSmall)        \
  TYPED_EQUAL_HANDLER_HELPER(V, None)
#endif

#define GENERATE_BUILTIN_TYPED_RELATIONAL_COMPARE_HANDLER(V, OP) \
  TYPED_COMPARE_OPERATION_HANDLER_HELPER(V, OP, Number)          \
  TYPED_COMPARE_OPERATION_HANDLER_HELPER(V, OP, SignedSmall)     \
  TYPED_COMPARE_OPERATION_HANDLER_HELPER(V, OP, None)

/* Tiering related builtins
 *
 * These builtins are used for tiering. Some special conventions apply. They,
 * - can be passed to the JSDispatchTable::SetTieringRequest to be executed
 *   instead of the actual JSFunction's code.
 * - need to uninstall themselves using JSDispatchTable::ResetTieringRequest.
 * - need to tail call the actual JSFunction's code.
 *
 * Also, there are lifecycle considerations since the tiering requests are
 * mutually exclusive.
 *
 * For RCS the optimizing builtins should include the work `Optimize` in their
 * name (see tools/callstats_groups.py).
 *
 * */
#define BUILTIN_LIST_BASE_TIERING_TURBOFAN(TFC) \
  TFC(StartTurbofanOptimizeJob, JSTrampoline)   \
  TFC(OptimizeTurbofanEager, JSTrampoline)

#define BUILTIN_LIST_BASE_TIERING_MAGLEV(TFC) \
  TFC(StartMaglevOptimizeJob, JSTrampoline)   \
  TFC(OptimizeMaglevEager, JSTrampoline)

#define BUILTIN_LIST_BASE_TIERING(TFC)             \
  BUILTIN_LIST_BASE_TIERING_MAGLEV(TFC)            \
  BUILTIN_LIST_BASE_TIERING_TURBOFAN(TFC)          \
  TFC(FunctionLogNextExecution, JSTrampoline)      \
  TFC(MarkReoptimizeLazyDeoptimized, JSTrampoline) \
  TFC(MarkLazyDeoptimized, JSTrampoline)

#define BUILTIN_LIST_BASE_TIER1(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH,     \
                                ASM)                                           \
  /* GC write barriers */                                                      \
  TFC(IndirectPointerBarrierSaveFP, IndirectPointerWriteBarrier)               \
  TFC(IndirectPointerBarrierIgnoreFP, IndirectPointerWriteBarrier)             \
                                                                               \
  /* Adaptors for CPP builtins with various formal parameter counts. */        \
  /* We split these versions for simplicity (not all architectures have */     \
  /* enough registers for extra CEntry arguments) and speculatively for */     \
  /* performance reasons. */                                                   \
  TFC(AdaptorWithBuiltinExitFrame0, CppBuiltinAdaptor)                         \
  TFC(AdaptorWithBuiltinExitFrame1, CppBuiltinAdaptor)                         \
  TFC(AdaptorWithBuiltinExitFrame2, CppBuiltinAdaptor)                         \
  TFC(AdaptorWithBuiltinExitFrame3, CppBuiltinAdaptor)                         \
  TFC(AdaptorWithBuiltinExitFrame4, CppBuiltinAdaptor)                         \
  TFC(AdaptorWithBuiltinExitFrame5, CppBuiltinAdaptor)                         \
                                                                               \
  /* TSAN support for stores in generated code. */                             \
  IF_TSAN(TFC, TSANRelaxedStore8IgnoreFP, TSANStore)                           \
  IF_TSAN(TFC, TSANRelaxedStore8SaveFP, TSANStore)                             \
  IF_TSAN(TFC, TSANRelaxedStore16IgnoreFP, TSANStore)                          \
  IF_TSAN(TFC, TSANRelaxedStore16SaveFP, TSANStore)                            \
  IF_TSAN(TFC, TSANRelaxedStore32IgnoreFP, TSANStore)                          \
  IF_TSAN(TFC, TSANRelaxedStore32SaveFP, TSANStore)                            \
  IF_TSAN(TFC, TSANRelaxedStore64IgnoreFP, TSANStore)                          \
  IF_TSAN(TFC, TSANRelaxedStore64SaveFP, TSANStore)                            \
  IF_TSAN(TFC, TSANSeqCstStore8IgnoreFP, TSANStore)                            \
  IF_TSAN(TFC, TSANSeqCstStore8SaveFP, TSANStore)                              \
  IF_TSAN(TFC, TSANSeqCstStore16IgnoreFP, TSANStore)                           \
  IF_TSAN(TFC, TSANSeqCstStore16SaveFP, TSANStore)                             \
  IF_TSAN(TFC, TSANSeqCstStore32IgnoreFP, TSANStore)                           \
  IF_TSAN(TFC, TSANSeqCstStore32SaveFP, TSANStore)                             \
  IF_TSAN(TFC, TSANSeqCstStore64IgnoreFP, TSANStore)                           \
  IF_TSAN(TFC, TSANSeqCstStore64SaveFP, TSANStore)                             \
  IF_TSAN(TFC, TSANReleaseStore8IgnoreFP, TSANStore)                           \
  IF_TSAN(TFC, TSANReleaseStore8SaveFP, TSANStore)                             \
  IF_TSAN(TFC, TSANReleaseStore16IgnoreFP, TSANStore)                          \
  IF_TSAN(TFC, TSANReleaseStore16SaveFP, TSANStore)                            \
  IF_TSAN(TFC, TSANReleaseStore32IgnoreFP, TSANStore)                          \
  IF_TSAN(TFC, TSANReleaseStore32SaveFP, TSANStore)                            \
  IF_TSAN(TFC, TSANReleaseStore64IgnoreFP, TSANStore)                          \
  IF_TSAN(TFC, TSANReleaseStore64SaveFP, TSANStore)                            \
                                                                               \
  /* TSAN support for loads in generated code. */                              \
  IF_TSAN(TFC, TSANRelaxedLoad32IgnoreFP, TSANLoad)                            \
  IF_TSAN(TFC, TSANRelaxedLoad32SaveFP, TSANLoad)                              \
  IF_TSAN(TFC, TSANRelaxedLoad64IgnoreFP, TSANLoad)                            \
  IF_TSAN(TFC, TSANRelaxedLoad64SaveFP, TSANLoad)                              \
                                                                               \
  /* Calls */                                                                  \
  /* ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList) */              \
  ASM(CallFunction_ReceiverIsNullOrUndefined, CallTrampoline)                  \
  ASM(CallFunction_ReceiverIsNotNullOrUndefined, CallTrampoline)               \
  ASM(CallFunction_ReceiverIsAny, CallTrampoline)                              \
  /* ES6 section 9.4.1.1 [[Call]] ( thisArgument, argumentsList) */            \
  ASM(CallBoundFunction, CallTrampoline)                                       \
  TFC(CallWrappedFunction, CallTrampoline)                                     \
  /* ES6 section 7.3.12 Call(F, V, [argumentsList]) */                         \
  ASM(Call_ReceiverIsNullOrUndefined, CallTrampoline)                          \
  ASM(Call_ReceiverIsNotNullOrUndefined, CallTrampoline)                       \
  ASM(Call_ReceiverIsAny, CallTrampoline)                                      \
  TFC(Call_ReceiverIsNullOrUndefined_Baseline_Compact,                         \
      CallTrampoline_Baseline_Compact)                                         \
  TFC(Call_ReceiverIsNullOrUndefined_Baseline, CallTrampoline_Baseline)        \
  TFC(Call_ReceiverIsNotNullOrUndefined_Baseline_Compact,                      \
      CallTrampoline_Baseline_Compact)                                         \
  TFC(Call_ReceiverIsNotNullOrUndefined_Baseline, CallTrampoline_Baseline)     \
  TFC(Call_ReceiverIsAny_Baseline_Compact, CallTrampoline_Baseline_Compact)    \
  TFC(Call_ReceiverIsAny_Baseline, CallTrampoline_Baseline)                    \
  TFC(Call_ReceiverIsNullOrUndefined_WithFeedback,                             \
      CallTrampoline_WithFeedback)                                             \
  TFC(Call_ReceiverIsNotNullOrUndefined_WithFeedback,                          \
      CallTrampoline_WithFeedback)                                             \
  TFC(Call_ReceiverIsAny_WithFeedback, CallTrampoline_WithFeedback)            \
                                                                               \
  /* ES6 section 9.5.12[[Call]] ( thisArgument, argumentsList ) */             \
  TFC(CallProxy, CallTrampoline)                                               \
  ASM(CallVarargs, CallVarargs)                                                \
  TFC(CallWithSpread, CallWithSpread)                                          \
  TFC(CallWithSpread_Baseline, CallWithSpread_Baseline)                        \
  TFC(CallWithSpread_WithFeedback, CallWithSpread_WithFeedback)                \
  TFC(CallWithArrayLike, CallWithArrayLike)                                    \
  TFC(CallWithArrayLike_WithFeedback, CallWithArrayLike_WithFeedback)          \
  ASM(CallForwardVarargs, CallForwardVarargs)                                  \
  ASM(CallFunctionForwardVarargs, CallForwardVarargs)                          \
  /* Call an API callback via a {FunctionTemplateInfo}, doing appropriate */   \
  /* access and compatible receiver checks. */                                 \
  TFC(CallFunctionTemplate_Generic, CallFunctionTemplateGeneric)               \
  TFC(CallFunctionTemplate_CheckAccess, CallFunctionTemplate)                  \
  TFC(CallFunctionTemplate_CheckCompatibleReceiver, CallFunctionTemplate)      \
  TFC(CallFunctionTemplate_CheckAccessAndCompatibleReceiver,                   \
      CallFunctionTemplate)                                                    \
                                                                               \
  /* Construct */                                                              \
  /* ES6 section 9.2.2 [[Construct]] ( argumentsList, newTarget) */            \
  ASM(ConstructFunction, JSTrampoline)                                         \
  /* ES6 section 9.4.1.2 [[Construct]] (argumentsList, newTarget) */           \
  ASM(ConstructBoundFunction, JSTrampoline)                                    \
  ASM(ConstructedNonConstructable, JSTrampoline)                               \
  /* ES6 section 7.3.13 Construct (F, [argumentsList], [newTarget]) */         \
  ASM(Construct, ConstructStub)                                                \
  ASM(ConstructVarargs, ConstructVarargs)                                      \
  TFC(ConstructWithSpread, ConstructWithSpread)                                \
  TFC(ConstructWithSpread_Baseline, ConstructWithSpread_Baseline)              \
  TFC(ConstructWithSpread_WithFeedback, ConstructWithSpread_WithFeedback)      \
  TFC(ConstructWithArrayLike, ConstructWithArrayLike)                          \
  ASM(ConstructForwardVarargs, ConstructForwardVarargs)                        \
  ASM(ConstructForwardAllArgs, ConstructForwardAllArgs)                        \
  TFC(ConstructForwardAllArgs_Baseline, ConstructForwardAllArgs_Baseline)      \
  TFC(ConstructForwardAllArgs_WithFeedback,                                    \
      ConstructForwardAllArgs_WithFeedback)                                    \
  ASM(ConstructFunctionForwardVarargs, ConstructForwardVarargs)                \
  TFC(Construct_Baseline, Construct_Baseline)                                  \
  TFC(Construct_WithFeedback, Construct_WithFeedback)                          \
  ASM(JSConstructStubGeneric, ConstructStub)                                   \
  ASM(JSBuiltinsConstructStub, ConstructStub)                                  \
  TFC(FastNewObject, FastNewObject)                                            \
  TFS(FastNewClosure, NeedsContext::kYes, kSharedFunctionInfo, kFeedbackCell)  \
  /* ES6 section 9.5.14 [[Construct]] ( argumentsList, newTarget) */           \
  TFC(ConstructProxy, JSTrampoline)                                            \
                                                                               \
  /* Apply and entries */                                                      \
  ASM(JSEntry, JSEntry)                                                        \
  ASM(JSConstructEntry, JSEntry)                                               \
  ASM(JSRunMicrotasksEntry, RunMicrotasksEntry)                                \
  /* Call a JSValue. */                                                        \
  ASM(JSEntryTrampoline, JSEntry)                                              \
  /* Construct a JSValue. */                                                   \
  ASM(JSConstructEntryTrampoline, JSEntry)                                     \
  ASM(ResumeGeneratorTrampoline, ResumeGenerator)                              \
                                                                               \
  /* String helpers */                                                         \
  IF_TSA(TFC_TSA, TFC, StringFromCodePointAt, StringAtAsString)                \
  TFC(StringEqual, StringEqual)                                                \
  IF_WASM(TFC, WasmJSStringEqual, StringEqual)                                 \
  TFC(StringGreaterThan, CompareNoContext)                                     \
  TFC(StringGreaterThanOrEqual, CompareNoContext)                              \
  TFC(StringLessThan, CompareNoContext)                                        \
  TFC(StringLessThanOrEqual, CompareNoContext)                                 \
  TFC(StringCompare, CompareNoContext)                                         \
  IF_WASM(TFC, WasmStringCompare, CompareNoContext)                            \
  TFC(StringSubstring, StringSubstring)                                        \
                                                                               \
  /* OrderedHashTable helpers */                                               \
  TFS(OrderedHashTableHealIndex, NeedsContext::kYes, kTable, kIndex)           \
                                                                               \
  /* Interpreter */                                                            \
  /* InterpreterEntryTrampoline dispatches to the interpreter to run a */      \
  /* JSFunction in the form of bytecodes */                                    \
  ASM(InterpreterEntryTrampoline, JSTrampoline)                                \
  ASM(InterpreterEntryTrampolineForProfiling, JSTrampoline)                    \
  ASM(InterpreterForwardAllArgsThenConstruct, ConstructForwardAllArgs)         \
  ASM(InterpreterPushArgsThenCall, InterpreterPushArgsThenCall)                \
  ASM(InterpreterPushUndefinedAndArgsThenCall, InterpreterPushArgsThenCall)    \
  ASM(InterpreterPushArgsThenCallWithFinalSpread, InterpreterPushArgsThenCall) \
  ASM(InterpreterPushArgsThenConstruct, InterpreterPushArgsThenConstruct)      \
  ASM(InterpreterPushArgsThenFastConstructFunction,                            \
      InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterPushArgsThenConstructArrayFunction,                           \
      InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterPushArgsThenConstructWithFinalSpread,                         \
      InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterEnterAtBytecode, Void)                                        \
  ASM(InterpreterEnterAtNextBytecode, Void)                                    \
  ASM(InterpreterOnStackReplacement, OnStackReplacement)                       \
                                                                               \
  /* Baseline Compiler */                                                      \
  ASM(BaselineOutOfLinePrologue, BaselineOutOfLinePrologue)                    \
  ASM(BaselineOutOfLinePrologueDeopt, Void)                                    \
  ASM(BaselineOnStackReplacement, OnStackReplacement)                          \
  ASM(BaselineLeaveFrame, BaselineLeaveFrame)                                  \
  ASM(InterpreterOnStackReplacement_ToBaseline, Void)                          \
                                                                               \
  /* Maglev Compiler */                                                        \
  ASM(MaglevFunctionEntryStackCheck_WithoutNewTarget, Void)                    \
  ASM(MaglevFunctionEntryStackCheck_WithNewTarget, Void)                       \
  ASM(MaglevOptimizeCodeOrTailCallOptimizedCodeSlot,                           \
      MaglevOptimizeCodeOrTailCallOptimizedCodeSlot)                           \
                                                                               \
  /* Code life-cycle */                                                        \
  TFC(CompileLazy, JSTrampoline)                                               \
  TFC(InstantiateAsmJs, JSTrampoline)                                          \
  ASM(NotifyDeoptimized, Void)                                                 \
                                                                               \
  BUILTIN_LIST_BASE_TIERING(TFC)                                               \
                                                                               \
  /* Trampolines called when returning from a deoptimization that expects   */ \
  /* to continue in a JavaScript builtin to finish the functionality of a   */ \
  /* an TF-inlined version of builtin that has side-effects.                */ \
  /*                                                                        */ \
  /* The trampolines work as follows:                                       */ \
  /*   1. Trampoline restores input register values that                    */ \
  /*      the builtin expects from a BuiltinContinuationFrame.              */ \
  /*   2. Trampoline tears down BuiltinContinuationFrame.                   */ \
  /*   3. Trampoline jumps to the builtin's address.                        */ \
  /*   4. Builtin executes as if invoked by the frame above it.             */ \
  /*   5. When the builtin returns, execution resumes normally in the       */ \
  /*      calling frame, processing any return result from the JavaScript   */ \
  /*      builtin as if it had called the builtin directly.                 */ \
  /*                                                                        */ \
  /* There are two variants of the stub that differ in their handling of a  */ \
  /* value returned by the next frame deeper on the stack. For LAZY deopts, */ \
  /* the return value (e.g. rax on x64) is explicitly passed as an extra    */ \
  /* stack parameter to the JavaScript builtin by the "WithResult"          */ \
  /* trampoline variant. The plain variant is used in EAGER deopt contexts  */ \
  /* and has no such special handling. */                                      \
  ASM(ContinueToCodeStubBuiltin, ContinueToBuiltin)                            \
  ASM(ContinueToCodeStubBuiltinWithResult, ContinueToBuiltin)                  \
  ASM(ContinueToJavaScriptBuiltin, ContinueToBuiltin)                          \
  ASM(ContinueToJavaScriptBuiltinWithResult, ContinueToBuiltin)                \
                                                                               \
  /* API callback handling */                                                  \
  ASM(CallApiCallbackGeneric, CallApiCallbackGeneric)                          \
  ASM(CallApiCallbackOptimizedNoProfiling, CallApiCallbackOptimized)           \
  ASM(CallApiCallbackOptimized, CallApiCallbackOptimized)                      \
  ASM(CallApiGetter, CallApiGetter)                                            \
  ASM(CallNamedInterceptorGetter, CallApiGetter)                               \
  ASM(CallNamedInterceptorSetter, CallApiSetter)                               \
  TFC(HandleApiCallOrConstruct, JSTrampoline)                                  \
  CPP(HandleApiConstruct, kDontAdaptArgumentsSentinel)                         \
  CPP(HandleApiCallAsFunctionDelegate, kDontAdaptArgumentsSentinel)            \
  CPP(HandleApiCallAsConstructorDelegate, kDontAdaptArgumentsSentinel)         \
                                                                               \
  /* Adapters for Turbofan into runtime */                                     \
  TFC(AllocateInYoungGeneration, Allocate)                                     \
  TFC(AllocateInOldGeneration, Allocate)                                       \
  IF_WASM(TFC, WasmAllocateInYoungGeneration, Allocate)                        \
  IF_WASM(TFC, WasmAllocateInOldGeneration, Allocate)                          \
  IF_WASM(TFC, WasmAllocateInSharedHeap, WasmAllocateShared)                   \
                                                                               \
  TFC(NewHeapNumber, NewHeapNumber)                                            \
                                                                               \
  /* TurboFan support builtins */                                              \
  TFS(CopyFastSmiOrObjectElements, NeedsContext::kNo, kObject)                 \
  TFC(GrowFastDoubleElements, GrowArrayElements)                               \
  TFC(GrowFastSmiOrObjectElements, GrowArrayElements)                          \
                                                                               \
  /* Debugger */                                                               \
  TFJ(DebugBreakTrampoline, kDontAdaptArgumentsSentinel)                       \
  ASM(RestartFrameTrampoline, RestartFrameTrampoline)                          \
                                                                               \
  /* Type conversions */                                                       \
  TFC(ToNumber, TypeConversion)                                                \
  TFC(ToBigInt, TypeConversion)                                                \
  TFC(ToNumber_Baseline, TypeConversion_Baseline)                              \
  TFC(ToNumeric_Baseline, TypeConversion_Baseline)                             \
  TFC(PlainPrimitiveToNumber, TypeConversionNoContext)                         \
  TFC(ToNumberConvertBigInt, TypeConversion)                                   \
  TFC(ToBigIntConvertNumber, TypeConversion)                                   \
  TFC(Typeof, Typeof)                                                          \
  TFC(Typeof_Baseline, UnaryOp_Baseline)                                       \
  TFC(BigIntToI64, BigIntToI64)                                                \
  TFC(BigIntToI32Pair, BigIntToI32Pair)                                        \
  TFC(I64ToBigInt, I64ToBigInt)                                                \
  TFC(I32PairToBigInt, I32PairToBigInt)                                        \
                                                                               \
  /* Type conversions continuations */                                         \
  TFC(ToBooleanLazyDeoptContinuation, SingleParameterOnStack)                  \
  TFC(MathCeilContinuation, SingleParameterOnStack)                            \
  TFC(MathFloorContinuation, SingleParameterOnStack)                           \
  TFC(MathRoundContinuation, SingleParameterOnStack)                           \
  TFC(MathClz32Continuation, SingleParameterOnStack)                           \
                                                                               \
  /* Handlers */                                                               \
  TFH(KeyedLoadIC_PolymorphicName, LoadWithVector)                             \
  TFH(KeyedStoreIC_Megamorphic, StoreWithVector)                               \
  TFH(DefineKeyedOwnIC_Megamorphic, StoreNoFeedback)                           \
  TFH(LoadGlobalIC_NoFeedback, LoadGlobalNoFeedback)                           \
  TFH(LoadIC_FunctionPrototype, LoadWithVector)                                \
  TFH(LoadIC_StringLength, LoadWithVector)                                     \
  TFH(LoadIC_StringWrapperLength, LoadWithVector)                              \
  TFH(LoadIC_NoFeedback, LoadNoFeedback)                                       \
  TFH(StoreGlobalIC_Slow, StoreWithVector)                                     \
  TFH(StoreIC_NoFeedback, StoreNoFeedback)                                     \
  TFH(DefineNamedOwnIC_NoFeedback, StoreNoFeedback)                            \
  TFH(KeyedLoadIC_SloppyArguments, LoadWithVector)                             \
  TFH(LoadIndexedInterceptorIC, LoadWithVector)                                \
  TFH(KeyedStoreIC_SloppyArguments_InBounds, StoreWithVector)                  \
  TFH(KeyedStoreIC_SloppyArguments_NoTransitionGrowAndHandleCOW,               \
      StoreWithVector)                                                         \
  TFH(KeyedStoreIC_SloppyArguments_NoTransitionIgnoreTypedArrayOOB,            \
      StoreWithVector)                                                         \
  TFH(KeyedStoreIC_SloppyArguments_NoTransitionHandleCOW, StoreWithVector)     \
  WITH_GEARBOX(TFH, StoreFastElementIC_InBounds, StoreWithVector)              \
  TFH(StoreFastElementIC_NoTransitionGrowAndHandleCOW, StoreWithVector)        \
  TFH(StoreFastElementIC_NoTransitionIgnoreTypedArrayOOB, StoreWithVector)     \
  TFH(StoreFastElementIC_NoTransitionHandleCOW, StoreWithVector)               \
  TFH(ElementsTransitionAndStore_InBounds, StoreTransition)                    \
  TFH(ElementsTransitionAndStore_NoTransitionGrowAndHandleCOW,                 \
      StoreTransition)                                                         \
  TFH(ElementsTransitionAndStore_NoTransitionIgnoreTypedArrayOOB,              \
      StoreTransition)                                                         \
  TFH(ElementsTransitionAndStore_NoTransitionHandleCOW, StoreTransition)       \
  TFH(KeyedHasIC_PolymorphicName, LoadWithVector)                              \
  TFH(KeyedHasIC_SloppyArguments, LoadWithVector)                              \
  TFH(HasIndexedInterceptorIC, LoadWithVector)                                 \
                                                                               \
  /* Microtask helpers */                                                      \
  TFS(EnqueueMicrotask, NeedsContext::kYes, kMicrotask)                        \
  TFJ(GlobalQueueMicrotask, kJSArgcReceiverSlots + 1, kReceiver, kCallback)    \
  ASM(RunMicrotasksTrampoline, RunMicrotasksEntry)                             \
  TFC(RunMicrotasks, RunMicrotasks)                                            \
                                                                               \
  /* Object property helpers */                                                \
  TFS(HasProperty, NeedsContext::kYes, kObject, kKey)                          \
  TFS(DeleteProperty, NeedsContext::kYes, kObject, kKey, kLanguageMode)        \
  TFS(CopyDataProperties, NeedsContext::kYes, kTarget, kSource)                \
  TFS(SetDataProperties, NeedsContext::kYes, kTarget, kSource)                 \
  TFC(CopyDataPropertiesWithExcludedPropertiesOnStack,                         \
      CopyDataPropertiesWithExcludedPropertiesOnStack)                         \
  TFC(CopyDataPropertiesWithExcludedProperties,                                \
      CopyDataPropertiesWithExcludedProperties)                                \
                                                                               \
  /* Abort */                                                                  \
  TFC(Abort, Abort)                                                            \
  TFC(AbortCSADcheck, Abort)                                                   \
                                                                               \
  /* Built-in functions for Javascript */                                      \
  /* Special internal builtins */                                              \
  CPP(EmptyFunction, kDontAdaptArgumentsSentinel)                              \
  CPP(EmptyFunction1, JSParameterCount(1))                                     \
  CPP(Illegal, kDontAdaptArgumentsSentinel)                                    \
  CPP(IllegalInvocationThrower, kDontAdaptArgumentsSentinel)                   \
  CPP(StrictPoisonPillThrower, JSParameterCount(0))                            \
  CPP(UnsupportedThrower, kDontAdaptArgumentsSentinel)                         \
  TFJ(ReturnReceiver, kJSArgcReceiverSlots, kReceiver)                         \
                                                                               \
  /* AbstractModuleSource */                                                   \
  CPP(AbstractModuleSourceToStringTag, JSParameterCount(0))                    \
                                                                               \
  /* Array */                                                                  \
  TFC(ArrayConstructor, JSTrampoline)                                          \
  TFC(ArrayConstructorImpl, ArrayConstructor)                                  \
  TFC(ArrayNoArgumentConstructor_PackedSmi_DontOverride,                       \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_HoleySmi_DontOverride,                        \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_PackedSmi_DisableAllocationSites,             \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_HoleySmi_DisableAllocationSites,              \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_Packed_DisableAllocationSites,                \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_Holey_DisableAllocationSites,                 \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_PackedDouble_DisableAllocationSites,          \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_HoleyDouble_DisableAllocationSites,           \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArraySingleArgumentConstructor_PackedSmi_DontOverride,                   \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_HoleySmi_DontOverride,                    \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_PackedSmi_DisableAllocationSites,         \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_HoleySmi_DisableAllocationSites,          \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_Packed_DisableAllocationSites,            \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_Holey_DisableAllocationSites,             \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_PackedDouble_DisableAllocationSites,      \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_HoleyDouble_DisableAllocationSites,       \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArrayNArgumentsConstructor, ArrayNArgumentsConstructor)                  \
  CPP(ArrayConcat, kDontAdaptArgumentsSentinel)                                \
  CPP(ArrayPrototypeFill, kDontAdaptArgumentsSentinel)                         \
  TFS(ArrayIncludesSmi, NeedsContext::kYes, kElements, kSearchElement,         \
      kLength, kFromIndex)                                                     \
  TFS(ArrayIncludesSmiOrObject, NeedsContext::kYes, kElements, kSearchElement, \
      kLength, kFromIndex)                                                     \
  TFS(ArrayIncludesPackedDoubles, NeedsContext::kYes, kElements,               \
      kSearchElement, kLength, kFromIndex)                                     \
  TFS(ArrayIncludesHoleyDoubles, NeedsContext::kYes, kElements,                \
      kSearchElement, kLength, kFromIndex)                                     \
  TFJ(ArrayIncludes, kDontAdaptArgumentsSentinel)                              \
  TFS(ArrayIndexOfSmi, NeedsContext::kYes, kElements, kSearchElement, kLength, \
      kFromIndex)                                                              \
  TFS(ArrayIndexOfSmiOrObject, NeedsContext::kYes, kElements, kSearchElement,  \
      kLength, kFromIndex)                                                     \
  TFS(ArrayIndexOfPackedDoubles, NeedsContext::kYes, kElements,                \
      kSearchElement, kLength, kFromIndex)                                     \
  TFS(ArrayIndexOfHoleyDoubles, NeedsContext::kYes, kElements, kSearchElement, \
      kLength, kFromIndex)                                                     \
  TFJ(ArrayIndexOf, kDontAdaptArgumentsSentinel)                               \
  CPP(ArrayPop, kDontAdaptArgumentsSentinel)                                   \
  TFJ(ArrayPrototypePop, kDontAdaptArgumentsSentinel)                          \
  CPP(ArrayPush, kDontAdaptArgumentsSentinel)                                  \
  TFJ(ArrayPrototypePush, kDontAdaptArgumentsSentinel)                         \
  CPP(ArrayShift, kDontAdaptArgumentsSentinel)                                 \
  CPP(ArrayUnshift, kDontAdaptArgumentsSentinel)                               \
  /* Support for Array.from and other array-copying idioms */                  \
  TFS(CloneFastJSArray, NeedsContext::kYes, kSource)                           \
  TFS(CloneFastJSArrayFillingHoles, NeedsContext::kYes, kSource)               \
  TFS(ExtractFastJSArray, NeedsContext::kYes, kSource, kBegin, kCount)         \
  TFS(CreateArrayFromSlowBoilerplate, NeedsContext::kYes, kFeedbackVector,     \
      kSlot, kBoilerplateDescriptor, kFlags)                                   \
  TFS(CreateObjectFromSlowBoilerplate, NeedsContext::kYes, kFeedbackVector,    \
      kSlot, kBoilerplateDescriptor, kFlags)                                   \
  TFC(CreateArrayFromSlowBoilerplateHelper, CreateFromSlowBoilerplateHelper)   \
  TFC(CreateObjectFromSlowBoilerplateHelper, CreateFromSlowBoilerplateHelper)  \
  TFJ(ArrayPrototypeEntries, kJSArgcReceiverSlots, kReceiver)                  \
  TFJ(ArrayPrototypeKeys, kJSArgcReceiverSlots, kReceiver)                     \
  TFJ(ArrayPrototypeValues, kJSArgcReceiverSlots, kReceiver)                   \
  TFJ(ArrayIteratorPrototypeNext, kJSArgcReceiverSlots, kReceiver)             \
                                                                               \
  /* ArrayBuffer */                                                            \
  CPP(ArrayBufferConstructor, JSParameterCount(1))                             \
  CPP(ArrayBufferConstructor_DoNotInitialize, kDontAdaptArgumentsSentinel)     \
  CPP(ArrayBufferPrototypeSlice, JSParameterCount(2))                          \
  CPP(ArrayBufferPrototypeResize, JSParameterCount(1))                         \
  CPP(ArrayBufferPrototypeTransfer, kDontAdaptArgumentsSentinel)               \
  CPP(ArrayBufferPrototypeTransferToFixedLength, kDontAdaptArgumentsSentinel)  \
  CPP(ArrayBufferPrototypeTransferToImmutable, kDontAdaptArgumentsSentinel)    \
  CPP(ArrayBufferPrototypeSliceToImmutable, JSParameterCount(2))               \
                                                                               \
  /* AsyncFunction */                                                          \
  TFS(AsyncFunctionEnter, NeedsContext::kYes, kClosure, kReceiver)             \
  TFS(AsyncFunctionReject, NeedsContext::kYes, kAsyncFunctionObject, kReason)  \
  TFS(AsyncFunctionResolve, NeedsContext::kYes, kAsyncFunctionObject, kValue)  \
  TFC(AsyncFunctionLazyDeoptContinuation, AsyncFunctionStackParameter)         \
  TFS(AsyncFunctionAwait, NeedsContext::kYes, kAsyncFunctionObject, kValue)    \
  TFJ(AsyncFunctionAwaitRejectClosure, kJSArgcReceiverSlots + 1, kReceiver,    \
      kSentError)                                                              \
  TFJ(AsyncFunctionAwaitResolveClosure, kJSArgcReceiverSlots + 1, kReceiver,   \
      kSentValue)                                                              \
                                                                               \
  /* BigInt */                                                                 \
  CPP(BigIntConstructor, kDontAdaptArgumentsSentinel)                          \
  CPP(BigIntAsUintN, kDontAdaptArgumentsSentinel)                              \
  CPP(BigIntAsIntN, kDontAdaptArgumentsSentinel)                               \
  CPP(BigIntPrototypeToLocaleString, kDontAdaptArgumentsSentinel)              \
  CPP(BigIntPrototypeToString, kDontAdaptArgumentsSentinel)                    \
  CPP(BigIntPrototypeValueOf, kDontAdaptArgumentsSentinel)                     \
                                                                               \
  /* CallSite */                                                               \
  CPP(CallSitePrototypeGetColumnNumber, JSParameterCount(0))                   \
  CPP(CallSitePrototypeGetEnclosingColumnNumber, JSParameterCount(0))          \
  CPP(CallSitePrototypeGetEnclosingLineNumber, JSParameterCount(0))            \
  CPP(CallSitePrototypeGetEvalOrigin, JSParameterCount(0))                     \
  CPP(CallSitePrototypeGetFileName, JSParameterCount(0))                       \
  CPP(CallSitePrototypeGetFunction, JSParameterCount(0))                       \
  CPP(CallSitePrototypeGetFunctionName, JSParameterCount(0))                   \
  CPP(CallSitePrototypeGetLineNumber, JSParameterCount(0))                     \
  CPP(CallSitePrototypeGetMethodName, JSParameterCount(0))                     \
  CPP(CallSitePrototypeGetPosition, JSParameterCount(0))                       \
  CPP(CallSitePrototypeGetPromiseIndex, JSParameterCount(0))                   \
  CPP(CallSitePrototypeGetScriptHash, JSParameterCount(0))                     \
  CPP(CallSitePrototypeGetScriptNameOrSourceURL, JSParameterCount(0))          \
  CPP(CallSitePrototypeGetThis, JSParameterCount(0))                           \
  CPP(CallSitePrototypeGetTypeName, JSParameterCount(0))                       \
  CPP(CallSitePrototypeIsAsync, JSParameterCount(0))                           \
  CPP(CallSitePrototypeIsConstructor, JSParameterCount(0))                     \
  CPP(CallSitePrototypeIsEval, JSParameterCount(0))                            \
  CPP(CallSitePrototypeIsNative, JSParameterCount(0))                          \
  CPP(CallSitePrototypeIsPromiseAll, JSParameterCount(0))                      \
  CPP(CallSitePrototypeIsToplevel, JSParameterCount(0))                        \
  CPP(CallSitePrototypeToString, JSParameterCount(0))                          \
                                                                               \
  /* Console */                                                                \
  CPP(ConsoleDebug, kDontAdaptArgumentsSentinel)                               \
  CPP(ConsoleError, kDontAdaptArgumentsSentinel)                               \
  CPP(ConsoleInfo, kDontAdaptArgumentsSentinel)                                \
  CPP(ConsoleLog, kDontAdaptArgumentsSentinel)                                 \
  CPP(ConsoleWarn, kDontAdaptArgumentsSentinel)                                \
  CPP(ConsoleDir, kDontAdaptArgumentsSentinel)                                 \
  CPP(ConsoleDirXml, kDontAdaptArgumentsSentinel)                              \
  CPP(ConsoleTable, kDontAdaptArgumentsSentinel)                               \
  CPP(ConsoleTrace, kDontAdaptArgumentsSentinel)                               \
  CPP(ConsoleGroup, kDontAdaptArgumentsSentinel)                               \
  CPP(ConsoleGroupCollapsed, kDontAdaptArgumentsSentinel)                      \
  CPP(ConsoleGroupEnd, kDontAdaptArgumentsSentinel)                            \
  CPP(ConsoleClear, kDontAdaptArgumentsSentinel)                               \
  CPP(ConsoleCount, kDontAdaptArgumentsSentinel)                               \
  CPP(ConsoleCountReset, kDontAdaptArgumentsSentinel)                          \
  CPP(ConsoleAssert, kDontAdaptArgumentsSentinel)                              \
  CPP(ConsoleProfile, kDontAdaptArgumentsSentinel)                             \
  CPP(ConsoleProfileEnd, kDontAdaptArgumentsSentinel)                          \
  CPP(ConsoleTime, kDontAdaptArgumentsSentinel)                                \
  CPP(ConsoleTimeLog, kDontAdaptArgumentsSentinel)                             \
  CPP(ConsoleTimeEnd, kDontAdaptArgumentsSentinel)                             \
  CPP(ConsoleTimeStamp, kDontAdaptArgumentsSentinel)                           \
  CPP(ConsoleContext, kDontAdaptArgumentsSentinel)                             \
                                                                               \
  /* DataView */                                                               \
  CPP(DataViewConstructor, kDontAdaptArgumentsSentinel)                        \
                                                                               \
  /* Date */                                                                   \
  CPP(DateConstructor, kDontAdaptArgumentsSentinel)                            \
  TFJ(DatePrototypeGetDate, kJSArgcReceiverSlots, kReceiver)                   \
  TFJ(DatePrototypeGetDay, kJSArgcReceiverSlots, kReceiver)                    \
  TFJ(DatePrototypeGetFullYear, kJSArgcReceiverSlots, kReceiver)               \
  TFJ(DatePrototypeGetHours, kJSArgcReceiverSlots, kReceiver)                  \
  TFJ(DatePrototypeGetMilliseconds, kJSArgcReceiverSlots, kReceiver)           \
  TFJ(DatePrototypeGetMinutes, kJSArgcReceiverSlots, kReceiver)                \
  TFJ(DatePrototypeGetMonth, kJSArgcReceiverSlots, kReceiver)                  \
  TFJ(DatePrototypeGetSeconds, kJSArgcReceiverSlots, kReceiver)                \
  TFJ(DatePrototypeGetTime, kJSArgcReceiverSlots, kReceiver)                   \
  TFJ(DatePrototypeGetTimezoneOffset, kJSArgcReceiverSlots, kReceiver)         \
  TFJ(DatePrototypeGetUTCDate, kJSArgcReceiverSlots, kReceiver)                \
  TFJ(DatePrototypeGetUTCDay, kJSArgcReceiverSlots, kReceiver)                 \
  TFJ(DatePrototypeGetUTCFullYear, kJSArgcReceiverSlots, kReceiver)            \
  TFJ(DatePrototypeGetUTCHours, kJSArgcReceiverSlots, kReceiver)               \
  TFJ(DatePrototypeGetUTCMilliseconds, kJSArgcReceiverSlots, kReceiver)        \
  TFJ(DatePrototypeGetUTCMinutes, kJSArgcReceiverSlots, kReceiver)             \
  TFJ(DatePrototypeGetUTCMonth, kJSArgcReceiverSlots, kReceiver)               \
  TFJ(DatePrototypeGetUTCSeconds, kJSArgcReceiverSlots, kReceiver)             \
  TFJ(DatePrototypeValueOf, kJSArgcReceiverSlots, kReceiver)                   \
  TFJ(DatePrototypeToPrimitive, kJSArgcReceiverSlots + 1, kReceiver, kHint)    \
  CPP(DatePrototypeGetYear, JSParameterCount(0))                               \
  CPP(DatePrototypeSetYear, kDontAdaptArgumentsSentinel)                       \
  CPP(DateNow, kDontAdaptArgumentsSentinel)                                    \
  CPP(DateParse, kDontAdaptArgumentsSentinel)                                  \
  CPP(DatePrototypeSetDate, kDontAdaptArgumentsSentinel)                       \
  CPP(DatePrototypeSetFullYear, kDontAdaptArgumentsSentinel)                   \
  CPP(DatePrototypeSetHours, kDontAdaptArgumentsSentinel)                      \
  CPP(DatePrototypeSetMilliseconds, kDontAdaptArgumentsSentinel)               \
  CPP(DatePrototypeSetMinutes, kDontAdaptArgumentsSentinel)                    \
  CPP(DatePrototypeSetMonth, kDontAdaptArgumentsSentinel)                      \
  CPP(DatePrototypeSetSeconds, kDontAdaptArgumentsSentinel)                    \
  CPP(DatePrototypeSetTime, kDontAdaptArgumentsSentinel)                       \
  CPP(DatePrototypeSetUTCDate, kDontAdaptArgumentsSentinel)                    \
  CPP(DatePrototypeSetUTCFullYear, kDontAdaptArgumentsSentinel)                \
  CPP(DatePrototypeSetUTCHours, kDontAdaptArgumentsSentinel)                   \
  CPP(DatePrototypeSetUTCMilliseconds, kDontAdaptArgumentsSentinel)            \
  CPP(DatePrototypeSetUTCMinutes, kDontAdaptArgumentsSentinel)                 \
  CPP(DatePrototypeSetUTCMonth, kDontAdaptArgumentsSentinel)                   \
  CPP(DatePrototypeSetUTCSeconds, kDontAdaptArgumentsSentinel)                 \
  CPP(DatePrototypeToDateString, kDontAdaptArgumentsSentinel)                  \
  CPP(DatePrototypeToISOString, kDontAdaptArgumentsSentinel)                   \
  CPP(DatePrototypeToUTCString, kDontAdaptArgumentsSentinel)                   \
  CPP(DatePrototypeToString, kDontAdaptArgumentsSentinel)                      \
  CPP(DatePrototypeToTimeString, kDontAdaptArgumentsSentinel)                  \
  CPP(DatePrototypeToJson, kDontAdaptArgumentsSentinel)                        \
  CPP(DateUTC, kDontAdaptArgumentsSentinel)                                    \
                                                                               \
  /* DisposabeStack*/                                                          \
  CPP(DisposableStackConstructor, kDontAdaptArgumentsSentinel)                 \
  CPP(DisposableStackPrototypeUse, JSParameterCount(1))                        \
  CPP(DisposableStackPrototypeDispose, JSParameterCount(0))                    \
  CPP(DisposableStackPrototypeGetDisposed, JSParameterCount(0))                \
  CPP(DisposableStackPrototypeAdopt, JSParameterCount(2))                      \
  CPP(DisposableStackPrototypeDefer, JSParameterCount(1))                      \
  CPP(DisposableStackPrototypeMove, JSParameterCount(0))                       \
                                                                               \
  /* Async DisposabeStack*/                                                    \
  CPP(AsyncDisposableStackOnFulfilled, JSParameterCount(0))                    \
  CPP(AsyncDisposableStackOnRejected, JSParameterCount(0))                     \
  CPP(AsyncDisposeFromSyncDispose, JSParameterCount(0))                        \
  CPP(AsyncDisposableStackConstructor, kDontAdaptArgumentsSentinel)            \
  CPP(AsyncDisposableStackPrototypeUse, JSParameterCount(1))                   \
  CPP(AsyncDisposableStackPrototypeDisposeAsync, JSParameterCount(0))          \
  CPP(AsyncDisposableStackPrototypeGetDisposed, JSParameterCount(0))           \
  CPP(AsyncDisposableStackPrototypeAdopt, JSParameterCount(2))                 \
  CPP(AsyncDisposableStackPrototypeDefer, JSParameterCount(1))                 \
  CPP(AsyncDisposableStackPrototypeMove, JSParameterCount(0))                  \
                                                                               \
  /* Error */                                                                  \
  CPP(ErrorConstructor, kDontAdaptArgumentsSentinel)                           \
  CPP(ErrorCaptureStackTrace, kDontAdaptArgumentsSentinel)                     \
  CPP(ErrorPrototypeToString, JSParameterCount(0))                             \
  CPP(ErrorIsError, JSParameterCount(1))                                       \
                                                                               \
  /* Function */                                                               \
  CPP(FunctionConstructor, kDontAdaptArgumentsSentinel)                        \
  ASM(FunctionPrototypeApply, JSTrampoline)                                    \
  CPP(FunctionPrototypeBind, kDontAdaptArgumentsSentinel)                      \
  IF_WASM(CPP, WebAssemblyFunctionPrototypeBind, kDontAdaptArgumentsSentinel)  \
  IF_WASM(TFJ, WasmConstructorWrapper, kDontAdaptArgumentsSentinel)            \
  IF_WASM(TFJ, WasmMethodWrapper, kDontAdaptArgumentsSentinel)                 \
  ASM(FunctionPrototypeCall, JSTrampoline)                                     \
  CPP(FunctionPrototypeToString, kDontAdaptArgumentsSentinel)                  \
  IF_FUNCTION_ARGUMENTS_CALLER_ARE_ON_PROTOTYPE(                               \
      CPP, FunctionPrototypeLegacyArgumentsGetter, JSParameterCount(0))        \
  IF_FUNCTION_ARGUMENTS_CALLER_ARE_ON_PROTOTYPE(                               \
      CPP, FunctionPrototypeLegacyArgumentsSetter, JSParameterCount(1))        \
  IF_FUNCTION_ARGUMENTS_CALLER_ARE_ON_PROTOTYPE(                               \
      CPP, FunctionPrototypeLegacyCallerGetter, JSParameterCount(0))           \
  IF_FUNCTION_ARGUMENTS_CALLER_ARE_ON_PROTOTYPE(                               \
      CPP, FunctionPrototypeLegacyCallerSetter, JSParameterCount(1))           \
                                                                               \
  /* Belongs to Objects but is a dependency of GeneratorPrototypeResume */     \
  TFS(CreateIterResultObject, NeedsContext::kYes, kValue, kDone)               \
                                                                               \
  /* Generator and Async */                                                    \
  TFS(CreateGeneratorObject, NeedsContext::kYes, kClosure, kReceiver)          \
  CPP(GeneratorFunctionConstructor, kDontAdaptArgumentsSentinel)               \
  TFJ(GeneratorPrototypeNext, kDontAdaptArgumentsSentinel)                     \
  TFJ(GeneratorPrototypeReturn, kDontAdaptArgumentsSentinel)                   \
  TFJ(GeneratorPrototypeThrow, kDontAdaptArgumentsSentinel)                    \
  CPP(AsyncFunctionConstructor, kDontAdaptArgumentsSentinel)                   \
  TFC(SuspendGeneratorBaseline, SuspendGeneratorBaseline)                      \
  TFC(ResumeGeneratorBaseline, ResumeGeneratorBaseline)                        \
  TFC(GeneratorNextLazyDeoptContinuation, GeneratorNextLazyDeoptContinuation)  \
                                                                               \
  /* Iterator Protocol */                                                      \
  TFC(GetIteratorWithFeedbackLazyDeoptContinuation, GetIteratorStackParameter) \
  TFC(CallIteratorWithFeedbackLazyDeoptContinuation, SingleParameterOnStack)   \
  TFC(ForOfNextResultDeoptContinuation, ForOfNextResultDeoptContinuation)      \
  TFC(ForOfNextLoadDoneLazyDeoptContinuation,                                  \
      ForOfNextLoadDoneLazyDeoptContinuation)                                  \
  TFC(ForOfNextLoadValueEagerDeoptContinuation,                                \
      ForOfNextLoadValueEagerDeoptContinuation)                                \
  TFC(ForOfNextLoadValueLazyDeoptContinuation,                                 \
      ForOfNextLoadValueLazyDeoptContinuation)                                 \
                                                                               \
  /* Global object */                                                          \
  CPP(GlobalDecodeURI, kDontAdaptArgumentsSentinel)                            \
  CPP(GlobalDecodeURIComponent, kDontAdaptArgumentsSentinel)                   \
  CPP(GlobalEncodeURI, kDontAdaptArgumentsSentinel)                            \
  CPP(GlobalEncodeURIComponent, kDontAdaptArgumentsSentinel)                   \
  CPP(GlobalEscape, kDontAdaptArgumentsSentinel)                               \
  CPP(GlobalUnescape, kDontAdaptArgumentsSentinel)                             \
  CPP(GlobalEval, kDontAdaptArgumentsSentinel)                                 \
  TFJ(GlobalIsFinite, kJSArgcReceiverSlots + 1, kReceiver, kNumber)            \
  TFJ(GlobalIsNaN, kJSArgcReceiverSlots + 1, kReceiver, kNumber)               \
                                                                               \
  /* JSON */                                                                   \
  CPP(JsonParse, kDontAdaptArgumentsSentinel)                                  \
  CPP(JsonStringify, JSParameterCount(3))                                      \
  CPP(JsonRawJson, JSParameterCount(1))                                        \
  CPP(JsonIsRawJson, JSParameterCount(1))                                      \
                                                                               \
  /* ICs */                                                                    \
  TFH(LoadIC, LoadWithVector)                                                  \
  TFH(LoadIC_Megamorphic, LoadWithVector)                                      \
  TFH(LoadIC_Noninlined, LoadWithVector)                                       \
  TFH(LoadICTrampoline, Load)                                                  \
  BUILTIN_LOAD_IC_HANDLER_LIST(TFH)                                            \
  TFH(LoadICTrampoline_Megamorphic, Load)                                      \
  TFH(LoadSuperIC, LoadWithReceiverAndVector)                                  \
  TFH(LoadSuperICBaseline, LoadWithReceiverBaseline)                           \
  TFH(KeyedLoadIC, KeyedLoadWithVector)                                        \
  TFH(EnumeratedKeyedLoadIC, EnumeratedKeyedLoad)                              \
  TFH(KeyedLoadIC_Megamorphic, KeyedLoadWithVector)                            \
  TFH(KeyedLoadICTrampoline, KeyedLoad)                                        \
  TFH(KeyedLoadICBaseline, KeyedLoadBaseline)                                  \
  TFH(EnumeratedKeyedLoadICBaseline, EnumeratedKeyedLoadBaseline)              \
  TFH(KeyedLoadICTrampoline_Megamorphic, KeyedLoad)                            \
  TFH(StoreGlobalIC, StoreGlobalWithVector)                                    \
  TFH(StoreGlobalICTrampoline, StoreGlobal)                                    \
  TFH(StoreGlobalICBaseline, StoreGlobalBaseline)                              \
  TFH(StoreIC, StoreWithVector)                                                \
  TFH(StoreIC_Megamorphic, StoreWithVector)                                    \
  TFH(StoreICTrampoline, Store)                                                \
  TFH(StoreICTrampoline_Megamorphic, Store)                                    \
  TFH(StoreICBaseline, StoreBaseline)                                          \
  TFH(DefineNamedOwnIC, StoreWithVector)                                       \
  TFH(DefineNamedOwnICTrampoline, Store)                                       \
  TFH(DefineNamedOwnICBaseline, StoreBaseline)                                 \
  TFH(KeyedStoreIC, StoreWithVector)                                           \
  TFH(KeyedStoreICTrampoline, Store)                                           \
  TFH(KeyedStoreICTrampoline_Megamorphic, Store)                               \
  TFH(KeyedStoreICBaseline, StoreBaseline)                                     \
  TFH(DefineKeyedOwnIC, DefineKeyedOwnWithVector)                              \
  TFH(DefineKeyedOwnICTrampoline, DefineKeyedOwn)                              \
  TFH(DefineKeyedOwnICBaseline, DefineKeyedOwnBaseline)                        \
  TFH(StoreInArrayLiteralIC, StoreWithVector)                                  \
  TFH(StoreInArrayLiteralICBaseline, StoreBaseline)                            \
  TFH(LookupContextNoCellTrampoline, LookupTrampoline)                         \
  TFH(LookupContextTrampoline, LookupTrampoline)                               \
  TFH(LookupContextNoCellBaseline, LookupBaseline)                             \
  TFH(LookupScriptContextBaseline, LookupBaseline)                             \
  TFH(LookupContextNoCellInsideTypeofTrampoline, LookupTrampoline)             \
  TFH(LookupContextInsideTypeofTrampoline, LookupTrampoline)                   \
  TFH(LookupContextNoCellInsideTypeofBaseline, LookupBaseline)                 \
  TFH(LookupContextInsideTypeofBaseline, LookupBaseline)                       \
  TFH(LoadGlobalIC, LoadGlobalWithVector)                                      \
  TFH(LoadGlobalICInsideTypeof, LoadGlobalWithVector)                          \
  TFH(LoadGlobalICTrampoline, LoadGlobal)                                      \
  TFH(LoadGlobalICBaseline, LoadGlobalBaseline)                                \
  TFH(LoadGlobalICInsideTypeofTrampoline, LoadGlobal)                          \
  TFH(LoadGlobalICInsideTypeofBaseline, LoadGlobalBaseline)                    \
  TFH(LookupGlobalIC, LookupWithVector)                                        \
  TFH(LookupGlobalICTrampoline, LookupTrampoline)                              \
  TFH(LookupGlobalICBaseline, LookupBaseline)                                  \
  TFH(LookupGlobalICInsideTypeof, LookupWithVector)                            \
  TFH(LookupGlobalICInsideTypeofTrampoline, LookupTrampoline)                  \
  TFH(LookupGlobalICInsideTypeofBaseline, LookupBaseline)                      \
  TFH(CloneObjectIC, CloneObjectWithVector)                                    \
  TFH(CloneObjectICBaseline, CloneObjectBaseline)                              \
  TFH(CloneObjectIC_Slow, CloneObjectWithVector)                               \
  TFH(KeyedHasIC, KeyedHasICWithVector)                                        \
  TFH(KeyedHasICBaseline, KeyedHasICBaseline)                                  \
  TFH(KeyedHasIC_Megamorphic, KeyedHasICWithVector)                            \
  TFH(AddLhsIsStringConstantInternalizeWithVector,                             \
      AddStringConstantInternalizeWithVector)                                  \
  TFH(AddLhsIsStringConstantInternalizeTrampoline,                             \
      AddStringConstantInternalizeTrampoline)                                  \
  TFH(AddRhsIsStringConstantInternalizeWithVector,                             \
      AddStringConstantInternalizeWithVector)                                  \
  TFH(AddRhsIsStringConstantInternalizeTrampoline,                             \
      AddStringConstantInternalizeTrampoline)                                  \
                                                                               \
  /* IterableToList */                                                         \
  TFS(IterableToList, NeedsContext::kYes, kIterable, kIteratorFn)              \
  TFS(IterableToFixedArray, NeedsContext::kYes, kIterable, kIteratorFn)        \
  TFS(IterableToListWithSymbolLookup, NeedsContext::kYes, kIterable)           \
  TFS(IterableToFixedArrayWithSymbolLookupSlow, NeedsContext::kYes, kIterable) \
  TFS(IterableToListMayPreserveHoles, NeedsContext::kYes, kIterable,           \
      kIteratorFn)                                                             \
  TFS(IterableToListConvertHoles, NeedsContext::kYes, kIterable, kIteratorFn)  \
  IF_WASM(TFS, IterableToFixedArrayForWasm, NeedsContext::kYes, kIterable,     \
          kExpectedLength)                                                     \
                                                                               \
  TFS(StringListFromIterable, NeedsContext::kYes, kIterable)                   \
                                                                               \
  /* Map */                                                                    \
  TFS(FindOrderedHashMapEntry, NeedsContext::kYes, kTable, kKey)               \
  TFJ(MapConstructor, kDontAdaptArgumentsSentinel)                             \
  TFJ(MapPrototypeSet, kJSArgcReceiverSlots + 2, kReceiver, kKey, kValue)      \
  TFJ(MapPrototypeDelete, kJSArgcReceiverSlots + 1, kReceiver, kKey)           \
  TFJ(MapPrototypeGet, kJSArgcReceiverSlots + 1, kReceiver, kKey)              \
  TFJ(MapPrototypeHas, kJSArgcReceiverSlots + 1, kReceiver, kKey)              \
  CPP(MapPrototypeClear, JSParameterCount(0))                                  \
  TFJ(MapPrototypeGetOrInsert, kJSArgcReceiverSlots + 2, kReceiver, kKey,      \
      kValue)                                                                  \
  TFJ(MapPrototypeGetOrInsertComputed, kJSArgcReceiverSlots + 2, kReceiver,    \
      kKey, kCallbackfn)                                                       \
  TFJ(MapPrototypeEntries, kJSArgcReceiverSlots, kReceiver)                    \
  TFJ(MapPrototypeGetSize, kJSArgcReceiverSlots, kReceiver)                    \
  TFJ(MapPrototypeForEach, kDontAdaptArgumentsSentinel)                        \
  TFJ(MapPrototypeKeys, kJSArgcReceiverSlots, kReceiver)                       \
  TFJ(MapPrototypeValues, kJSArgcReceiverSlots, kReceiver)                     \
  TFJ(MapIteratorPrototypeNext, kJSArgcReceiverSlots, kReceiver)               \
  TFS(MapIteratorToList, NeedsContext::kYes, kSource)                          \
                                                                               \
  CPP(NumberPrototypeToExponential, kDontAdaptArgumentsSentinel)               \
  CPP(NumberPrototypeToFixed, kDontAdaptArgumentsSentinel)                     \
  CPP(NumberPrototypeToLocaleString, kDontAdaptArgumentsSentinel)              \
  CPP(NumberPrototypeToPrecision, kDontAdaptArgumentsSentinel)                 \
  CPP(MathSumPrecise, JSParameterCount(1))                                     \
  TFC(SameValue, CompareNoContext)                                             \
  TFC(SameValueNumbersOnly, CompareNoContext)                                  \
                                                                               \
  /* Binary ops with feedback collection */                                    \
  TFC(Add_Baseline, BinaryOp_Baseline)                                         \
  TFC(AddSmi_Baseline, BinarySmiOp_Baseline)                                   \
  TFC(Subtract_Baseline, BinaryOp_Baseline)                                    \
  TFC(SubtractSmi_Baseline, BinarySmiOp_Baseline)                              \
  TFC(Multiply_Baseline, BinaryOp_Baseline)                                    \
  TFC(MultiplySmi_Baseline, BinarySmiOp_Baseline)                              \
  TFC(Divide_Baseline, BinaryOp_Baseline)                                      \
  TFC(DivideSmi_Baseline, BinarySmiOp_Baseline)                                \
  TFC(Modulus_Baseline, BinaryOp_Baseline)                                     \
  TFC(ModulusSmi_Baseline, BinarySmiOp_Baseline)                               \
  TFC(Exponentiate_Baseline, BinaryOp_Baseline)                                \
  TFC(ExponentiateSmi_Baseline, BinarySmiOp_Baseline)                          \
  TFC(BitwiseAnd_Baseline, BinaryOp_Baseline)                                  \
  TFC(BitwiseAndSmi_Baseline, BinarySmiOp_Baseline)                            \
  TFC(BitwiseOr_Baseline, BinaryOp_Baseline)                                   \
  TFC(BitwiseOrSmi_Baseline, BinarySmiOp_Baseline)                             \
  TFC(BitwiseXor_Baseline, BinaryOp_Baseline)                                  \
  TFC(BitwiseXorSmi_Baseline, BinarySmiOp_Baseline)                            \
  TFC(ShiftLeft_Baseline, BinaryOp_Baseline)                                   \
  TFC(ShiftLeftSmi_Baseline, BinarySmiOp_Baseline)                             \
  TFC(ShiftRight_Baseline, BinaryOp_Baseline)                                  \
  TFC(ShiftRightSmi_Baseline, BinarySmiOp_Baseline)                            \
  TFC(ShiftRightLogical_Baseline, BinaryOp_Baseline)                           \
  TFC(ShiftRightLogicalSmi_Baseline, BinarySmiOp_Baseline)                     \
                                                                               \
  IF_TSA(TFC_TSA, TFC, Add_WithFeedback, BinaryOp_WithFeedback)                \
  TFC(Subtract_WithFeedback, BinaryOp_WithFeedback)                            \
  TFC(Multiply_WithFeedback, BinaryOp_WithFeedback)                            \
  TFC(Divide_WithFeedback, BinaryOp_WithFeedback)                              \
  TFC(Modulus_WithFeedback, BinaryOp_WithFeedback)                             \
  TFC(Exponentiate_WithFeedback, BinaryOp_WithFeedback)                        \
  TFC(BitwiseAnd_WithFeedback, BinaryOp_WithFeedback)                          \
  TFC(BitwiseOr_WithFeedback, BinaryOp_WithFeedback)                           \
  TFC(BitwiseXor_WithFeedback, BinaryOp_WithFeedback)                          \
  TFC(ShiftLeft_WithFeedback, BinaryOp_WithFeedback)                           \
  TFC(ShiftRight_WithFeedback, BinaryOp_WithFeedback)                          \
  TFC(ShiftRightLogical_WithFeedback, BinaryOp_WithFeedback)                   \
                                                                               \
  /* Like Add_WithFeedback, but lhs is a known constant and the result is */   \
  /* used as a property key and thus should be internalized early.        */   \
  TFC(Add_LhsIsStringConstant_Internalize_WithFeedback, BinaryOp_WithFeedback) \
  TFC(Add_LhsIsStringConstant_Internalize_Baseline, BinaryOp_Baseline)         \
  TFC(Add_RhsIsStringConstant_Internalize_WithFeedback, BinaryOp_WithFeedback) \
  TFC(Add_RhsIsStringConstant_Internalize_Baseline, BinaryOp_Baseline)         \
                                                                               \
  /* Typed Comparison baseline stubs */                                        \
  TFC(Equal_Generic_Baseline, Compare_WithEmbeddedFeedbackOffset)              \
  IF_SPARKPLUG_PLUS(GENERATE_BUILTIN_TYPED_EQUAL_HANDLER, TFC)                 \
  IF_SPARKPLUG_PLUS(TFC, EqualAndTryPatchCode, CompareAndTryPatchCode)         \
                                                                               \
  TFC(StrictEqual_Generic_Baseline, Compare_WithEmbeddedFeedbackOffset)        \
  IF_SPARKPLUG_PLUS(GENERATE_BUILTIN_TYPED_STRICTEQUAL_HANDLER, TFC)           \
  IF_SPARKPLUG_PLUS(TFC, StrictEqualAndTryPatchCode, CompareAndTryPatchCode)   \
                                                                               \
  TFC(LessThan_Generic_Baseline, Compare_WithEmbeddedFeedbackOffset)           \
  TFC(GreaterThan_Generic_Baseline, Compare_WithEmbeddedFeedbackOffset)        \
  TFC(LessThanOrEqual_Generic_Baseline, Compare_WithEmbeddedFeedbackOffset)    \
  TFC(GreaterThanOrEqual_Generic_Baseline, Compare_WithEmbeddedFeedbackOffset) \
  IF_SPARKPLUG_PLUS(GENERATE_BUILTIN_TYPED_RELATIONAL_COMPARE_HANDLER, TFC,    \
                    LessThan)                                                  \
  IF_SPARKPLUG_PLUS(TFC, LessThanAndTryPatchCode, CompareAndTryPatchCode)      \
  IF_SPARKPLUG_PLUS(GENERATE_BUILTIN_TYPED_RELATIONAL_COMPARE_HANDLER, TFC,    \
                    GreaterThan)                                               \
  IF_SPARKPLUG_PLUS(TFC, GreaterThanAndTryPatchCode, CompareAndTryPatchCode)   \
  IF_SPARKPLUG_PLUS(GENERATE_BUILTIN_TYPED_RELATIONAL_COMPARE_HANDLER, TFC,    \
                    LessThanOrEqual)                                           \
  IF_SPARKPLUG_PLUS(TFC, LessThanOrEqualAndTryPatchCode,                       \
                    CompareAndTryPatchCode)                                    \
  IF_SPARKPLUG_PLUS(GENERATE_BUILTIN_TYPED_RELATIONAL_COMPARE_HANDLER, TFC,    \
                    GreaterThanOrEqual)                                        \
  IF_SPARKPLUG_PLUS(TFC, GreaterThanOrEqualAndTryPatchCode,                    \
                    CompareAndTryPatchCode)                                    \
                                                                               \
  TFC(Equal_WithEmbeddedFeedback, Compare_WithEmbeddedFeedback)                \
  TFC(StrictEqual_WithEmbeddedFeedback, Compare_WithEmbeddedFeedback)          \
  TFC(LessThan_WithEmbeddedFeedback, Compare_WithEmbeddedFeedback)             \
  TFC(GreaterThan_WithEmbeddedFeedback, Compare_WithEmbeddedFeedback)          \
  TFC(LessThanOrEqual_WithEmbeddedFeedback, Compare_WithEmbeddedFeedback)      \
  TFC(GreaterThanOrEqual_WithEmbeddedFeedback, Compare_WithEmbeddedFeedback)   \
                                                                               \
  /* Unary ops with feedback collection */                                     \
  TFC(BitwiseNot_Baseline, UnaryOp_Baseline)                                   \
  TFC(Decrement_Baseline, UnaryOp_Baseline)                                    \
  TFC(Increment_Baseline, UnaryOp_Baseline)                                    \
  TFC(Negate_Baseline, UnaryOp_Baseline)                                       \
  IF_TSA(TFC_TSA, TFC, BitwiseNot_WithFeedback, UnaryOp_WithFeedback)          \
  TFC(Decrement_WithFeedback, UnaryOp_WithFeedback)                            \
  TFC(Increment_WithFeedback, UnaryOp_WithFeedback)                            \
  TFC(Negate_WithFeedback, UnaryOp_WithFeedback)                               \
                                                                               \
  /* Object */                                                                 \
  TFJ(ObjectAssign, kDontAdaptArgumentsSentinel)                               \
  TFJ(ObjectCreate, kDontAdaptArgumentsSentinel)                               \
  CPP(ObjectDefineGetter, JSParameterCount(2))                                 \
  CPP(ObjectDefineProperties, JSParameterCount(2))                             \
  CPP(ObjectDefineProperty, JSParameterCount(3))                               \
  CPP(ObjectDefineSetter, JSParameterCount(2))                                 \
  TFJ(ObjectEntries, kJSArgcReceiverSlots + 1, kReceiver, kObject)             \
  CPP(ObjectFreeze, kDontAdaptArgumentsSentinel)                               \
  TFJ(ObjectGetOwnPropertyDescriptor, kDontAdaptArgumentsSentinel)             \
  CPP(ObjectGetOwnPropertyDescriptors, kDontAdaptArgumentsSentinel)            \
  TFJ(ObjectGetOwnPropertyNames, kJSArgcReceiverSlots + 1, kReceiver, kObject) \
  CPP(ObjectGetOwnPropertySymbols, kDontAdaptArgumentsSentinel)                \
  TFJ(ObjectHasOwn, kJSArgcReceiverSlots + 2, kReceiver, kObject, kKey)        \
  TFJ(ObjectIs, kJSArgcReceiverSlots + 2, kReceiver, kLeft, kRight)            \
  CPP(ObjectIsFrozen, kDontAdaptArgumentsSentinel)                             \
  CPP(ObjectIsSealed, kDontAdaptArgumentsSentinel)                             \
  TFJ(ObjectKeys, kJSArgcReceiverSlots + 1, kReceiver, kObject)                \
  CPP(ObjectLookupGetter, JSParameterCount(1))                                 \
  CPP(ObjectLookupSetter, JSParameterCount(1))                                 \
  TFJ(ObjectPrototypeHasOwnProperty, kJSArgcReceiverSlots + 1, kReceiver,      \
      kKey)                                                                    \
  TFJ(ObjectPrototypeIsPrototypeOf, kJSArgcReceiverSlots + 1, kReceiver,       \
      kValue)                                                                  \
  CPP(ObjectPrototypePropertyIsEnumerable, kDontAdaptArgumentsSentinel)        \
  CPP(ObjectPrototypeGetProto, JSParameterCount(0))                            \
  CPP(ObjectPrototypeSetProto, JSParameterCount(1))                            \
  CPP(ObjectSeal, kDontAdaptArgumentsSentinel)                                 \
  TFS(ObjectToString, NeedsContext::kYes, kReceiver)                           \
  TFJ(ObjectValues, kJSArgcReceiverSlots + 1, kReceiver, kObject)              \
                                                                               \
  /* instanceof */                                                             \
  TFC(OrdinaryHasInstance, Compare)                                            \
  TFC(InstanceOf, Compare)                                                     \
  TFC(InstanceOf_WithFeedback, Compare_WithFeedback)                           \
  TFC(InstanceOf_Baseline, Compare_Baseline)                                   \
                                                                               \
  /* for-in */                                                                 \
  TFS(ForInEnumerate, NeedsContext::kYes, kReceiver)                           \
  TFC(ForInPrepare, ForInPrepare)                                              \
  TFS(ForInFilter, NeedsContext::kYes, kKey, kObject)                          \
                                                                               \
  /* Reflect */                                                                \
  ASM(ReflectApply, JSTrampoline)                                              \
  ASM(ReflectConstruct, JSTrampoline)                                          \
  CPP(ReflectDefineProperty, JSParameterCount(3))                              \
  CPP(ReflectOwnKeys, JSParameterCount(1))                                     \
  CPP(ReflectSet, kDontAdaptArgumentsSentinel)                                 \
                                                                               \
  /* RegExp */                                                                 \
  CPP(RegExpCapture1Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture2Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture3Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture4Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture5Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture6Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture7Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture8Getter, JSParameterCount(0))                               \
  CPP(RegExpCapture9Getter, JSParameterCount(0))                               \
  TFJ(RegExpConstructor, kJSArgcReceiverSlots + 2, kReceiver, kPattern,        \
      kFlags)                                                                  \
  CPP(RegExpInputGetter, JSParameterCount(0))                                  \
  CPP(RegExpInputSetter, JSParameterCount(1))                                  \
  CPP(RegExpLastMatchGetter, JSParameterCount(0))                              \
  CPP(RegExpLastParenGetter, JSParameterCount(0))                              \
  CPP(RegExpLeftContextGetter, JSParameterCount(0))                            \
  TFJ(RegExpPrototypeCompile, kJSArgcReceiverSlots + 2, kReceiver, kPattern,   \
      kFlags)                                                                  \
  CPP(RegExpPrototypeToString, kDontAdaptArgumentsSentinel)                    \
  CPP(RegExpRightContextGetter, JSParameterCount(0))                           \
  CPP(RegExpEscape, JSParameterCount(1))                                       \
                                                                               \
  /* RegExp helpers */                                                         \
  TFS(RegExpExecAtom, NeedsContext::kYes, kRegExp, kString, kLastIndex,        \
      kMatchInfo)                                                              \
  ASM(RegExpInterpreterTrampoline, RegExpTrampoline)                           \
  ASM(RegExpExperimentalTrampoline, RegExpTrampoline)                          \
                                                                               \
  /* Set */                                                                    \
  TFS(FindOrderedHashSetEntry, NeedsContext::kYes, kTable, kKey)               \
  TFJ(SetConstructor, kDontAdaptArgumentsSentinel)                             \
  TFJ(SetPrototypeHas, kJSArgcReceiverSlots + 1, kReceiver, kKey)              \
  TFJ(SetPrototypeAdd, kJSArgcReceiverSlots + 1, kReceiver, kKey)              \
  TFJ(SetPrototypeDelete, kJSArgcReceiverSlots + 1, kReceiver, kKey)           \
  CPP(SetPrototypeClear, JSParameterCount(0))                                  \
  TFJ(SetPrototypeEntries, kJSArgcReceiverSlots, kReceiver)                    \
  TFJ(SetPrototypeGetSize, kJSArgcReceiverSlots, kReceiver)                    \
  TFJ(SetPrototypeForEach, kDontAdaptArgumentsSentinel)                        \
  TFJ(SetPrototypeValues, kJSArgcReceiverSlots, kReceiver)                     \
  TFJ(SetIteratorPrototypeNext, kJSArgcReceiverSlots, kReceiver)               \
  TFS(SetOrSetIteratorToList, NeedsContext::kYes, kSource)                     \
                                                                               \
  /* ShadowRealm */                                                            \
  CPP(ShadowRealmConstructor, kDontAdaptArgumentsSentinel)                     \
  TFS(ShadowRealmGetWrappedValue, NeedsContext::kYes, kCreationContext,        \
      kTargetContext, kValue)                                                  \
  CPP(ShadowRealmPrototypeEvaluate, JSParameterCount(1))                       \
  TFJ(ShadowRealmPrototypeImportValue, kJSArgcReceiverSlots + 2, kReceiver,    \
      kSpecifier, kExportName)                                                 \
  TFJ(ShadowRealmImportValueFulfilled, kJSArgcReceiverSlots + 1, kReceiver,    \
      kExports)                                                                \
  TFJ(ShadowRealmImportValueRejected, kJSArgcReceiverSlots + 1, kReceiver,     \
      kException)                                                              \
                                                                               \
  /* SharedArrayBuffer */                                                      \
  CPP(SharedArrayBufferPrototypeGetByteLength, kDontAdaptArgumentsSentinel)    \
  CPP(SharedArrayBufferPrototypeSlice, JSParameterCount(2))                    \
  CPP(SharedArrayBufferPrototypeGrow, JSParameterCount(1))                     \
                                                                               \
  TFJ(AtomicsLoad, kJSArgcReceiverSlots + 2, kReceiver, kArrayOrSharedObject,  \
      kIndexOrFieldName)                                                       \
  TFJ(AtomicsStore, kJSArgcReceiverSlots + 3, kReceiver, kArrayOrSharedObject, \
      kIndexOrFieldName, kValue)                                               \
  TFJ(AtomicsExchange, kJSArgcReceiverSlots + 3, kReceiver,                    \
      kArrayOrSharedObject, kIndexOrFieldName, kValue)                         \
  TFJ(AtomicsCompareExchange, kJSArgcReceiverSlots + 4, kReceiver,             \
      kArrayOrSharedObject, kIndexOrFieldName, kOldValue, kNewValue)           \
  TFJ(AtomicsAdd, kJSArgcReceiverSlots + 3, kReceiver, kArray, kIndex, kValue) \
  TFJ(AtomicsSub, kJSArgcReceiverSlots + 3, kReceiver, kArray, kIndex, kValue) \
  TFJ(AtomicsAnd, kJSArgcReceiverSlots + 3, kReceiver, kArray, kIndex, kValue) \
  TFJ(AtomicsOr, kJSArgcReceiverSlots + 3, kReceiver, kArray, kIndex, kValue)  \
  TFJ(AtomicsXor, kJSArgcReceiverSlots + 3, kReceiver, kArray, kIndex, kValue) \
  CPP(AtomicsNotify, JSParameterCount(3))                                      \
  CPP(AtomicsIsLockFree, JSParameterCount(1))                                  \
  CPP(AtomicsWait, JSParameterCount(4))                                        \
  CPP(AtomicsWaitAsync, JSParameterCount(4))                                   \
  CPP(AtomicsPause, kDontAdaptArgumentsSentinel)                               \
                                                                               \
  /* String */                                                                 \
  CPP(StringFromCodePoint, kDontAdaptArgumentsSentinel)                        \
  IF_TSA(TFJ_TSA, TFJ, StringFromCharCode, kDontAdaptArgumentsSentinel)        \
  CPP(StringPrototypeLastIndexOf, kDontAdaptArgumentsSentinel)                 \
  TFJ(StringPrototypeMatchAll, kJSArgcReceiverSlots + 1, kReceiver, kRegexp)   \
  TFJ(StringPrototypeReplace, kJSArgcReceiverSlots + 2, kReceiver, kSearch,    \
      kReplace)                                                                \
  TFJ(StringPrototypeSplit, kDontAdaptArgumentsSentinel)                       \
  CPP(StringRaw, kDontAdaptArgumentsSentinel)                                  \
  /*SELECT_TSA_LEVEL(IGNORE_BUILTIN, TFC_TSA, IGNORE_BUILTIN, ToString,        \
   * ToString)*/                                                               \
                                                                               \
  /* Symbol */                                                                 \
  CPP(SymbolConstructor, kDontAdaptArgumentsSentinel)                          \
  CPP(SymbolFor, kDontAdaptArgumentsSentinel)                                  \
  CPP(SymbolKeyFor, kDontAdaptArgumentsSentinel)                               \
                                                                               \
  /* TypedArray */                                                             \
  TFJ(TypedArrayBaseConstructor, kJSArgcReceiverSlots, kReceiver)              \
  TFJ(TypedArrayConstructor, kDontAdaptArgumentsSentinel)                      \
  CPP(TypedArrayPrototypeBuffer, kDontAdaptArgumentsSentinel)                  \
  TFJ(TypedArrayPrototypeByteLength, kJSArgcReceiverSlots, kReceiver)          \
  TFJ(TypedArrayPrototypeByteOffset, kJSArgcReceiverSlots, kReceiver)          \
  TFJ(TypedArrayPrototypeLength, kJSArgcReceiverSlots, kReceiver)              \
  CPP(TypedArrayPrototypeCopyWithin, kDontAdaptArgumentsSentinel)              \
  CPP(TypedArrayPrototypeFill, kDontAdaptArgumentsSentinel)                    \
  CPP(TypedArrayPrototypeIncludes, kDontAdaptArgumentsSentinel)                \
  CPP(TypedArrayPrototypeIndexOf, kDontAdaptArgumentsSentinel)                 \
  CPP(TypedArrayPrototypeLastIndexOf, kDontAdaptArgumentsSentinel)             \
  CPP(TypedArrayPrototypeReverse, kDontAdaptArgumentsSentinel)                 \
  TFJ(TypedArrayPrototypeToStringTag, kJSArgcReceiverSlots, kReceiver)         \
  /* ES6 %TypedArray%.prototype.map */                                         \
  TFJ(TypedArrayPrototypeMap, kDontAdaptArgumentsSentinel)                     \
  CPP(Uint8ArrayFromBase64, kDontAdaptArgumentsSentinel)                       \
  CPP(Uint8ArrayPrototypeSetFromBase64, kDontAdaptArgumentsSentinel)           \
  CPP(Uint8ArrayFromHex, kDontAdaptArgumentsSentinel)                          \
  CPP(Uint8ArrayPrototypeSetFromHex, kDontAdaptArgumentsSentinel)              \
  CPP(Uint8ArrayPrototypeToBase64, kDontAdaptArgumentsSentinel)                \
  CPP(Uint8ArrayPrototypeToHex, kDontAdaptArgumentsSentinel)                   \
                                                                               \
  /* Wasm */                                                                   \
  IF_WASM_DRUMBRAKE(ASM, WasmInterpreterEntry, WasmDummy)                      \
  IF_WASM_DRUMBRAKE(ASM, GenericJSToWasmInterpreterWrapper, JSTrampoline)      \
  IF_WASM_DRUMBRAKE(ASM, WasmInterpreterCWasmEntry, WasmDummy)                 \
  IF_WASM_DRUMBRAKE(ASM, GenericWasmToJSInterpreterWrapper, WasmDummy)         \
                                                                               \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem32S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem32U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_F32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_F64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem32S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem32U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem32S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem32U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_F32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_F64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8S_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8U_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32S_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32U_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64LoadMem_s, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8S_LocalSet_s, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8U_LocalSet_s, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16S_LocalSet_s,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16U_LocalSet_s,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8S_LocalSet_s, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8U_LocalSet_s, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16S_LocalSet_s,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16U_LocalSet_s,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32S_LocalSet_s,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32U_LocalSet_s,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem_LocalSet_s, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem_LocalSet_s, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32LoadMem_LocalSet_s, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64LoadMem_LocalSet_s, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32StoreMem8_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32StoreMem16_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem8_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem16_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem32_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F32StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F64StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32StoreMem8_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32StoreMem16_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem8_s, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem16_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem32_s, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64StoreMem_s, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadStoreMem_s, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadStoreMem_s, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F32LoadStoreMem_s, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F64LoadStoreMem_s, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadStoreMem_s, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadStoreMem_s, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32LoadStoreMem_s, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64LoadStoreMem_s, WasmDummy)       \
                                                                               \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem32S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem32U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_I64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_F32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2r_F64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem32S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem32U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem32S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem32U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_I64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_F32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2r_F64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8S_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8U_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32S_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32U_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64LoadMem_l, WasmDummy)            \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8S_LocalSet_l, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem8U_LocalSet_l, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16S_LocalSet_l,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem16U_LocalSet_l,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8S_LocalSet_l, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem8U_LocalSet_l, WasmDummy) \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16S_LocalSet_l,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem16U_LocalSet_l,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32S_LocalSet_l,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem32U_LocalSet_l,           \
                                  WasmDummy)                                   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadMem_LocalSet_l, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadMem_LocalSet_l, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32LoadMem_LocalSet_l, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64LoadMem_LocalSet_l, WasmDummy)   \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32StoreMem8_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32StoreMem16_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem8_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem16_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem32_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F32StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F64StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32StoreMem8_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32StoreMem16_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem8_l, WasmDummy)          \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem16_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem32_l, WasmDummy)         \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64StoreMem_l, WasmDummy)           \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I32LoadStoreMem_l, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_I64LoadStoreMem_l, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F32LoadStoreMem_l, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, r2s_F64LoadStoreMem_l, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I32LoadStoreMem_l, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_I64LoadStoreMem_l, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F32LoadStoreMem_l, WasmDummy)       \
  IF_WASM_DRUMBRAKE_INSTR_HANDLER(ASM, s2s_F64LoadStoreMem_l, WasmDummy)       \
                                                                               \
  IF_WASM(ASM, JSToWasmWrapperAsm, WasmJSToWasmWrapper)                        \
  IF_WASM(ASM, WasmReturnPromiseOnSuspendAsm, WasmJSToWasmWrapper)             \
  IF_WASM(ASM, JSToWasmStressSwitchStacksAsm, WasmJSToWasmWrapper)             \
  IF_WASM(ASM, WasmToJsWrapperAsm, WasmDummy)                                  \
  IF_WASM(TFC, WasmToJsWrapperCSA, WasmToJSWrapper)                            \
  IF_WASM(TFC, WasmToJsWrapperInvalidSig, WasmToJSWrapper)                     \
  IF_WASM(ASM, WasmSuspend, WasmSuspend)                                       \
  IF_WASM(ASM, WasmResume, JSTrampoline)                                       \
  IF_WASM(ASM, WasmFXResume, WasmFXResume)                                     \
  IF_WASM(ASM, WasmFXResumeThrow, WasmFXResumeThrow)                           \
  IF_WASM(ASM, WasmFXResumeThrowRef, WasmFXResumeThrowRef)                     \
  IF_WASM(ASM, WasmFXSuspend, WasmFXSuspend)                                   \
  IF_WASM(ASM, WasmFXSwitch, WasmFXSwitch)                                     \
  IF_WASM(ASM, WasmFXReturn, WasmFXReturn)                                     \
  IF_WASM(ASM, WasmReject, JSTrampoline)                                       \
  IF_WASM(ASM, WasmTrapHandlerLandingPad, WasmDummy)                           \
  IF_WASM(ASM, WasmCompileLazy, WasmDummy)                                     \
  IF_WASM(ASM, WasmLiftoffFrameSetup, WasmDummy)                               \
  IF_WASM(ASM, WasmDebugBreak, WasmDummy)                                      \
  IF_WASM(ASM, WasmOnStackReplace, WasmDummy)                                  \
  IF_WASM(ASM, WasmHandleStackOverflow, WasmHandleStackOverflow)               \
  IF_WASM(TFC, WasmFloat32ToNumber, WasmFloat32ToNumber)                       \
  IF_WASM(TFC, WasmFloat64ToNumber, WasmFloat64ToTagged)                       \
  IF_WASM(TFC, WasmFloat64ToString, WasmFloat64ToTagged)                       \
  IF_WASM(TFC, JSToWasmLazyDeoptContinuation, SingleParameterOnStack)          \
                                                                               \
  /* WeakMap */                                                                \
  TFJ(WeakMapConstructor, kDontAdaptArgumentsSentinel)                         \
  TFS(WeakMapLookupHashIndex, NeedsContext::kYes, kTable, kKey)                \
  TFJ(WeakMapPrototypeGet, kJSArgcReceiverSlots + 1, kReceiver, kKey)          \
  TFJ(WeakMapPrototypeHas, kJSArgcReceiverSlots + 1, kReceiver, kKey)          \
  TFJ(WeakMapPrototypeSet, kJSArgcReceiverSlots + 2, kReceiver, kKey, kValue)  \
  TFJ(WeakMapPrototypeGetOrInsert, kJSArgcReceiverSlots + 2, kReceiver, kKey,  \
      kValue)                                                                  \
  TFJ(WeakMapPrototypeGetOrInsertComputed, kJSArgcReceiverSlots + 2,           \
      kReceiver, kKey, kCallbackfn)                                            \
  TFJ(WeakMapPrototypeDelete, kJSArgcReceiverSlots + 1, kReceiver, kKey)       \
                                                                               \
  /* WeakSet */                                                                \
  TFJ(WeakSetConstructor, kDontAdaptArgumentsSentinel)                         \
  TFJ(WeakSetPrototypeHas, kJSArgcReceiverSlots + 1, kReceiver, kKey)          \
  TFJ(WeakSetPrototypeAdd, kJSArgcReceiverSlots + 1, kReceiver, kValue)        \
  TFJ(WeakSetPrototypeDelete, kJSArgcReceiverSlots + 1, kReceiver, kValue)     \
                                                                               \
  /* WeakSet / WeakMap Helpers */                                              \
  TFS(WeakCollectionDelete, NeedsContext::kYes, kCollection, kKey)             \
  TFS(WeakCollectionSet, NeedsContext::kYes, kCollection, kKey, kValue)        \
                                                                               \
  /* JS Structs and friends */                                                 \
  CPP(SharedSpaceJSObjectHasInstance, kDontAdaptArgumentsSentinel)             \
  CPP(SharedStructTypeConstructor, kDontAdaptArgumentsSentinel)                \
  CPP(SharedStructTypeIsSharedStruct, JSParameterCount(1))                     \
  CPP(SharedStructConstructor, JSParameterCount(0))                            \
  CPP(SharedArrayConstructor, JSParameterCount(0))                             \
  CPP(SharedArrayIsSharedArray, JSParameterCount(1))                           \
  CPP(AtomicsMutexConstructor, JSParameterCount(0))                            \
  CPP(AtomicsMutexIsMutex, JSParameterCount(1))                                \
  CPP(AtomicsMutexLock, JSParameterCount(2))                                   \
  CPP(AtomicsMutexLockWithTimeout, JSParameterCount(3))                        \
  CPP(AtomicsMutexTryLock, JSParameterCount(2))                                \
  CPP(AtomicsConditionConstructor, JSParameterCount(0))                        \
  CPP(AtomicsConditionIsCondition, JSParameterCount(1))                        \
  CPP(AtomicsConditionWait, kDontAdaptArgumentsSentinel)                       \
  CPP(AtomicsConditionNotify, kDontAdaptArgumentsSentinel)                     \
                                                                               \
  /* AsyncGenerator */                                                         \
                                                                               \
  TFS(AsyncGeneratorResolve, NeedsContext::kYes, kGenerator, kValue, kDone)    \
  TFS(AsyncGeneratorReject, NeedsContext::kYes, kGenerator, kValue)            \
  TFS(AsyncGeneratorYieldWithAwait, NeedsContext::kYes, kGenerator, kValue)    \
  TFS(AsyncGeneratorReturn, NeedsContext::kYes, kGenerator, kValue)            \
  TFS(AsyncGeneratorResumeNext, NeedsContext::kYes, kGenerator)                \
                                                                               \
  /* AsyncGeneratorFunction( p1, p2, ... pn, body ) */                         \
  CPP(AsyncGeneratorFunctionConstructor, kDontAdaptArgumentsSentinel)          \
  /* AsyncGenerator.prototype.next ( value ) */                                \
  TFJ(AsyncGeneratorPrototypeNext, kDontAdaptArgumentsSentinel)                \
  /* AsyncGenerator.prototype.return ( value ) */                              \
  TFJ(AsyncGeneratorPrototypeReturn, kDontAdaptArgumentsSentinel)              \
  /* AsyncGenerator.prototype.throw ( exception ) */                           \
  TFJ(AsyncGeneratorPrototypeThrow, kDontAdaptArgumentsSentinel)               \
                                                                               \
  /* specific to Async Generators. Internal / Not exposed to JS code. */       \
  TFS(AsyncGeneratorAwait, NeedsContext::kYes, kAsyncGeneratorObject, kValue)  \
  TFJ(AsyncGeneratorAwaitResolveClosure, kJSArgcReceiverSlots + 1, kReceiver,  \
      kValue)                                                                  \
  TFJ(AsyncGeneratorAwaitRejectClosure, kJSArgcReceiverSlots + 1, kReceiver,   \
      kValue)                                                                  \
  TFJ(AsyncGeneratorYieldWithAwaitResolveClosure, kJSArgcReceiverSlots + 1,    \
      kReceiver, kValue)                                                       \
  TFJ(AsyncGeneratorReturnClosedResolveClosure, kJSArgcReceiverSlots + 1,      \
      kReceiver, kValue)                                                       \
  TFJ(AsyncGeneratorReturnClosedRejectClosure, kJSArgcReceiverSlots + 1,       \
      kReceiver, kValue)                                                       \
  TFJ(AsyncGeneratorReturnResolveClosure, kJSArgcReceiverSlots + 1, kReceiver, \
      kValue)                                                                  \
                                                                               \
  /* Async-from-Sync Iterator */                                               \
                                                                               \
  /* %AsyncFromSyncIteratorPrototype% */                                       \
  TFJ(AsyncFromSyncIteratorPrototypeNext, kDontAdaptArgumentsSentinel)         \
  TFJ(AsyncFromSyncIteratorPrototypeThrow, kDontAdaptArgumentsSentinel)        \
  TFJ(AsyncFromSyncIteratorPrototypeReturn, kDontAdaptArgumentsSentinel)       \
  TFJ(AsyncFromSyncIteratorCloseSyncAndRethrow, kJSArgcReceiverSlots + 1,      \
      kReceiver, kError)                                                       \
  TFJ(AsyncIteratorValueUnwrap, kJSArgcReceiverSlots + 1, kReceiver, kValue)   \
                                                                               \
  /* CEntry */                                                                 \
  ASM(CEntry_Return1_ArgvInRegister_NoBuiltinExit, InterpreterCEntry1)         \
  ASM(CEntry_Return1_ArgvOnStack_BuiltinExit, CEntryForCPPBuiltin)             \
  ASM(CEntry_Return1_ArgvOnStack_NoBuiltinExit, CEntryDummy)                   \
  ASM(CEntry_Return2_ArgvInRegister_NoBuiltinExit, InterpreterCEntry2)         \
  ASM(CEntry_Return2_ArgvOnStack_NoBuiltinExit, CEntryDummy)                   \
  ASM(WasmCEntry, CEntryDummy)                                                 \
  ASM(DirectCEntry, CEntryDummy)                                               \
                                                                               \
  /* String helpers */                                                         \
  TFS(StringAdd_CheckNone, NeedsContext::kYes, kLeft, kRight)                  \
  IF_WASM(TFS, WasmStringAdd_CheckNone, NeedsContext::kYes, kLeft, kRight)     \
  TFS(SubString, NeedsContext::kYes, kString, kFrom, kTo)                      \
                                                                               \
  /* Miscellaneous */                                                          \
  ASM(DoubleToI, Void)                                                         \
  TFC(GetProperty, GetProperty)                                                \
  TFS(GetPropertyWithReceiver, NeedsContext::kYes, kObject, kKey, kReceiver,   \
      kOnNonExistent)                                                          \
  TFS(SetProperty, NeedsContext::kYes, kReceiver, kKey, kValue)                \
  TFS(CreateDataProperty, NeedsContext::kYes, kReceiver, kKey, kValue)         \
  TFS(GetOwnPropertyDescriptor, NeedsContext::kYes, kReceiver, kKey)           \
  TFC(FindNonDefaultConstructorOrConstruct,                                    \
      FindNonDefaultConstructorOrConstruct)                                    \
  TFS(OrdinaryGetOwnPropertyDescriptor, NeedsContext::kYes, kReceiver, kKey)   \
  TFS(CheckMaglevType, NeedsContext::kNo, kObject, kType,                      \
      kAllowWideningSmiToInt32)                                                \
  IF_SHADOW_STACK(ASM, AdaptShadowStackForDeopt, Void)                         \
                                                                               \
  /* Trace */                                                                  \
  CPP(IsTraceCategoryEnabled, JSParameterCount(1))                             \
  CPP(Trace, JSParameterCount(5))                                              \
                                                                               \
  /* Weak refs */                                                              \
  CPP(FinalizationRegistryUnregister, kDontAdaptArgumentsSentinel)             \
                                                                               \
  /* Async modules */                                                          \
  TFJ(AsyncModuleEvaluate, kDontAdaptArgumentsSentinel)                        \
                                                                               \
  /* CallAsyncModule* are spec anonymyous functions */                         \
  CPP(CallAsyncModuleFulfilled, JSParameterCount(0))                           \
  CPP(CallAsyncModuleRejected, JSParameterCount(0))

#define BUILTIN_LIST_BASE(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH, ASM) \
  BUILTIN_LIST_BASE_TIER0(CPP, TFJ, TFC, TFS, TFH, ASM)                   \
  BUILTIN_LIST_BASE_TIER1(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH, ASM)

#ifdef V8_TEMPORAL_SUPPORT
#define BUILTIN_LIST_TEMPORAL(CPP, TFJ)                                        \
                                                                               \
  /* Temporal */                                                               \
  CPP(TemporalNowInstant, kDontAdaptArgumentsSentinel)                         \
  CPP(TemporalNowTimeZoneId, kDontAdaptArgumentsSentinel)                      \
  CPP(TemporalNowPlainDateTimeISO, kDontAdaptArgumentsSentinel)                \
  CPP(TemporalNowZonedDateTimeISO, kDontAdaptArgumentsSentinel)                \
  CPP(TemporalNowPlainDateISO, kDontAdaptArgumentsSentinel)                    \
  CPP(TemporalNowPlainTimeISO, kDontAdaptArgumentsSentinel)                    \
                                                                               \
  /* Temporal.PlaneDate */                                                     \
  CPP(TemporalPlainDateConstructor, kDontAdaptArgumentsSentinel)               \
  CPP(TemporalPlainDateFrom, kDontAdaptArgumentsSentinel)                      \
  CPP(TemporalPlainDateCompare, kDontAdaptArgumentsSentinel)                   \
  CPP(TemporalPlainDatePrototypeCalendarId, JSParameterCount(0))               \
  CPP(TemporalPlainDatePrototypeEra, JSParameterCount(0))                      \
  CPP(TemporalPlainDatePrototypeEraYear, JSParameterCount(0))                  \
  CPP(TemporalPlainDatePrototypeYear, JSParameterCount(0))                     \
  CPP(TemporalPlainDatePrototypeMonth, JSParameterCount(0))                    \
  CPP(TemporalPlainDatePrototypeMonthCode, JSParameterCount(0))                \
  CPP(TemporalPlainDatePrototypeDay, JSParameterCount(0))                      \
  CPP(TemporalPlainDatePrototypeDayOfWeek, JSParameterCount(0))                \
  CPP(TemporalPlainDatePrototypeDayOfYear, JSParameterCount(0))                \
  CPP(TemporalPlainDatePrototypeWeekOfYear, JSParameterCount(0))               \
  CPP(TemporalPlainDatePrototypeYearOfWeek, JSParameterCount(0))               \
  CPP(TemporalPlainDatePrototypeDaysInWeek, JSParameterCount(0))               \
  CPP(TemporalPlainDatePrototypeDaysInMonth, JSParameterCount(0))              \
  CPP(TemporalPlainDatePrototypeDaysInYear, JSParameterCount(0))               \
  CPP(TemporalPlainDatePrototypeMonthsInYear, JSParameterCount(0))             \
  CPP(TemporalPlainDatePrototypeInLeapYear, JSParameterCount(0))               \
  CPP(TemporalPlainDatePrototypeToPlainYearMonth, kDontAdaptArgumentsSentinel) \
  CPP(TemporalPlainDatePrototypeToPlainMonthDay, kDontAdaptArgumentsSentinel)  \
  CPP(TemporalPlainDatePrototypeAdd, kDontAdaptArgumentsSentinel)              \
  CPP(TemporalPlainDatePrototypeSubtract, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalPlainDatePrototypeWith, kDontAdaptArgumentsSentinel)             \
  CPP(TemporalPlainDatePrototypeWithCalendar, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalPlainDatePrototypeUntil, kDontAdaptArgumentsSentinel)            \
  CPP(TemporalPlainDatePrototypeSince, kDontAdaptArgumentsSentinel)            \
  CPP(TemporalPlainDatePrototypeEquals, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalPlainDatePrototypeToPlainDateTime, kDontAdaptArgumentsSentinel)  \
  CPP(TemporalPlainDatePrototypeToZonedDateTime, kDontAdaptArgumentsSentinel)  \
  CPP(TemporalPlainDatePrototypeToString, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalPlainDatePrototypeToLocaleString, kDontAdaptArgumentsSentinel)   \
  CPP(TemporalPlainDatePrototypeToJSON, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalPlainDatePrototypeValueOf, kDontAdaptArgumentsSentinel)          \
                                                                               \
  /* Temporal.PlaneTime */                                                     \
  CPP(TemporalPlainTimeConstructor, kDontAdaptArgumentsSentinel)               \
  CPP(TemporalPlainTimeFrom, kDontAdaptArgumentsSentinel)                      \
  CPP(TemporalPlainTimeCompare, kDontAdaptArgumentsSentinel)                   \
  CPP(TemporalPlainTimePrototypeHour, JSParameterCount(0))                     \
  CPP(TemporalPlainTimePrototypeMinute, JSParameterCount(0))                   \
  CPP(TemporalPlainTimePrototypeSecond, JSParameterCount(0))                   \
  CPP(TemporalPlainTimePrototypeMillisecond, JSParameterCount(0))              \
  CPP(TemporalPlainTimePrototypeMicrosecond, JSParameterCount(0))              \
  CPP(TemporalPlainTimePrototypeNanosecond, JSParameterCount(0))               \
  CPP(TemporalPlainTimePrototypeAdd, kDontAdaptArgumentsSentinel)              \
  CPP(TemporalPlainTimePrototypeSubtract, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalPlainTimePrototypeWith, kDontAdaptArgumentsSentinel)             \
  CPP(TemporalPlainTimePrototypeUntil, kDontAdaptArgumentsSentinel)            \
  CPP(TemporalPlainTimePrototypeSince, kDontAdaptArgumentsSentinel)            \
  CPP(TemporalPlainTimePrototypeRound, kDontAdaptArgumentsSentinel)            \
  CPP(TemporalPlainTimePrototypeEquals, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalPlainTimePrototypeToString, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalPlainTimePrototypeToLocaleString, kDontAdaptArgumentsSentinel)   \
  CPP(TemporalPlainTimePrototypeToJSON, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalPlainTimePrototypeValueOf, kDontAdaptArgumentsSentinel)          \
                                                                               \
  /* Temporal.PlainDateTime */                                                 \
  CPP(TemporalPlainDateTimeConstructor, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalPlainDateTimeFrom, kDontAdaptArgumentsSentinel)                  \
  CPP(TemporalPlainDateTimeCompare, kDontAdaptArgumentsSentinel)               \
  CPP(TemporalPlainDateTimePrototypeCalendarId, JSParameterCount(0))           \
  CPP(TemporalPlainDateTimePrototypeEra, JSParameterCount(0))                  \
  CPP(TemporalPlainDateTimePrototypeEraYear, JSParameterCount(0))              \
  CPP(TemporalPlainDateTimePrototypeYear, JSParameterCount(0))                 \
  CPP(TemporalPlainDateTimePrototypeMonth, JSParameterCount(0))                \
  CPP(TemporalPlainDateTimePrototypeMonthCode, JSParameterCount(0))            \
  CPP(TemporalPlainDateTimePrototypeDay, JSParameterCount(0))                  \
  CPP(TemporalPlainDateTimePrototypeHour, JSParameterCount(0))                 \
  CPP(TemporalPlainDateTimePrototypeMinute, JSParameterCount(0))               \
  CPP(TemporalPlainDateTimePrototypeSecond, JSParameterCount(0))               \
  CPP(TemporalPlainDateTimePrototypeMillisecond, JSParameterCount(0))          \
  CPP(TemporalPlainDateTimePrototypeMicrosecond, JSParameterCount(0))          \
  CPP(TemporalPlainDateTimePrototypeNanosecond, JSParameterCount(0))           \
  CPP(TemporalPlainDateTimePrototypeDayOfWeek, JSParameterCount(0))            \
  CPP(TemporalPlainDateTimePrototypeDayOfYear, JSParameterCount(0))            \
  CPP(TemporalPlainDateTimePrototypeWeekOfYear, JSParameterCount(0))           \
  CPP(TemporalPlainDateTimePrototypeYearOfWeek, JSParameterCount(0))           \
  CPP(TemporalPlainDateTimePrototypeDaysInWeek, JSParameterCount(0))           \
  CPP(TemporalPlainDateTimePrototypeDaysInMonth, JSParameterCount(0))          \
  CPP(TemporalPlainDateTimePrototypeDaysInYear, JSParameterCount(0))           \
  CPP(TemporalPlainDateTimePrototypeMonthsInYear, JSParameterCount(0))         \
  CPP(TemporalPlainDateTimePrototypeInLeapYear, JSParameterCount(0))           \
  CPP(TemporalPlainDateTimePrototypeWith, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalPlainDateTimePrototypeWithPlainTime,                             \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalPlainDateTimePrototypeWithCalendar, kDontAdaptArgumentsSentinel) \
  CPP(TemporalPlainDateTimePrototypeAdd, kDontAdaptArgumentsSentinel)          \
  CPP(TemporalPlainDateTimePrototypeSubtract, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalPlainDateTimePrototypeUntil, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalPlainDateTimePrototypeSince, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalPlainDateTimePrototypeRound, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalPlainDateTimePrototypeEquals, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalPlainDateTimePrototypeToString, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalPlainDateTimePrototypeToJSON, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalPlainDateTimePrototypeToLocaleString,                            \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalPlainDateTimePrototypeValueOf, kDontAdaptArgumentsSentinel)      \
  CPP(TemporalPlainDateTimePrototypeToZonedDateTime,                           \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalPlainDateTimePrototypeToPlainDate, kDontAdaptArgumentsSentinel)  \
  CPP(TemporalPlainDateTimePrototypeToPlainTime, kDontAdaptArgumentsSentinel)  \
                                                                               \
  /* Temporal.ZonedDateTime */                                                 \
  CPP(TemporalZonedDateTimeConstructor, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalZonedDateTimeFrom, kDontAdaptArgumentsSentinel)                  \
  CPP(TemporalZonedDateTimeCompare, kDontAdaptArgumentsSentinel)               \
  CPP(TemporalZonedDateTimePrototypeTimeZoneId, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeCalendarId, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeEra, JSParameterCount(0))                  \
  CPP(TemporalZonedDateTimePrototypeEraYear, JSParameterCount(0))              \
  CPP(TemporalZonedDateTimePrototypeYear, JSParameterCount(0))                 \
  CPP(TemporalZonedDateTimePrototypeMonth, JSParameterCount(0))                \
  CPP(TemporalZonedDateTimePrototypeMonthCode, JSParameterCount(0))            \
  CPP(TemporalZonedDateTimePrototypeDay, JSParameterCount(0))                  \
  CPP(TemporalZonedDateTimePrototypeHour, JSParameterCount(0))                 \
  CPP(TemporalZonedDateTimePrototypeMinute, JSParameterCount(0))               \
  CPP(TemporalZonedDateTimePrototypeSecond, JSParameterCount(0))               \
  CPP(TemporalZonedDateTimePrototypeMillisecond, JSParameterCount(0))          \
  CPP(TemporalZonedDateTimePrototypeMicrosecond, JSParameterCount(0))          \
  CPP(TemporalZonedDateTimePrototypeNanosecond, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeEpochMilliseconds, JSParameterCount(0))    \
  CPP(TemporalZonedDateTimePrototypeEpochNanoseconds, JSParameterCount(0))     \
  CPP(TemporalZonedDateTimePrototypeDayOfWeek, JSParameterCount(0))            \
  CPP(TemporalZonedDateTimePrototypeDayOfYear, JSParameterCount(0))            \
  CPP(TemporalZonedDateTimePrototypeWeekOfYear, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeYearOfWeek, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeHoursInDay, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeDaysInWeek, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeDaysInMonth, JSParameterCount(0))          \
  CPP(TemporalZonedDateTimePrototypeDaysInYear, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeMonthsInYear, JSParameterCount(0))         \
  CPP(TemporalZonedDateTimePrototypeInLeapYear, JSParameterCount(0))           \
  CPP(TemporalZonedDateTimePrototypeOffsetNanoseconds, JSParameterCount(0))    \
  CPP(TemporalZonedDateTimePrototypeOffset, JSParameterCount(0))               \
  CPP(TemporalZonedDateTimePrototypeWith, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalZonedDateTimePrototypeWithPlainTime,                             \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalZonedDateTimePrototypeWithTimeZone, kDontAdaptArgumentsSentinel) \
  CPP(TemporalZonedDateTimePrototypeWithCalendar, kDontAdaptArgumentsSentinel) \
  CPP(TemporalZonedDateTimePrototypeAdd, kDontAdaptArgumentsSentinel)          \
  CPP(TemporalZonedDateTimePrototypeSubtract, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalZonedDateTimePrototypeUntil, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalZonedDateTimePrototypeSince, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalZonedDateTimePrototypeRound, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalZonedDateTimePrototypeEquals, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalZonedDateTimePrototypeToString, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalZonedDateTimePrototypeToJSON, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalZonedDateTimePrototypeToLocaleString,                            \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalZonedDateTimePrototypeValueOf, kDontAdaptArgumentsSentinel)      \
  CPP(TemporalZonedDateTimePrototypeStartOfDay, kDontAdaptArgumentsSentinel)   \
  CPP(TemporalZonedDateTimePrototypeGetTimeZoneTransition,                     \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalZonedDateTimePrototypeToInstant, kDontAdaptArgumentsSentinel)    \
  CPP(TemporalZonedDateTimePrototypeToPlainDate, kDontAdaptArgumentsSentinel)  \
  CPP(TemporalZonedDateTimePrototypeToPlainTime, kDontAdaptArgumentsSentinel)  \
  CPP(TemporalZonedDateTimePrototypeToPlainDateTime,                           \
      kDontAdaptArgumentsSentinel)                                             \
                                                                               \
  /* Temporal.Duration */                                                      \
  CPP(TemporalDurationConstructor, kDontAdaptArgumentsSentinel)                \
  CPP(TemporalDurationFrom, kDontAdaptArgumentsSentinel)                       \
  CPP(TemporalDurationCompare, kDontAdaptArgumentsSentinel)                    \
  CPP(TemporalDurationPrototypeYears, JSParameterCount(0))                     \
  CPP(TemporalDurationPrototypeMonths, JSParameterCount(0))                    \
  CPP(TemporalDurationPrototypeWeeks, JSParameterCount(0))                     \
  CPP(TemporalDurationPrototypeDays, JSParameterCount(0))                      \
  CPP(TemporalDurationPrototypeHours, JSParameterCount(0))                     \
  CPP(TemporalDurationPrototypeMinutes, JSParameterCount(0))                   \
  CPP(TemporalDurationPrototypeSeconds, JSParameterCount(0))                   \
  CPP(TemporalDurationPrototypeMilliseconds, JSParameterCount(0))              \
  CPP(TemporalDurationPrototypeMicroseconds, JSParameterCount(0))              \
  CPP(TemporalDurationPrototypeNanoseconds, JSParameterCount(0))               \
  CPP(TemporalDurationPrototypeSign, JSParameterCount(0))                      \
  CPP(TemporalDurationPrototypeBlank, JSParameterCount(0))                     \
  CPP(TemporalDurationPrototypeWith, kDontAdaptArgumentsSentinel)              \
  CPP(TemporalDurationPrototypeNegated, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalDurationPrototypeAbs, kDontAdaptArgumentsSentinel)               \
  CPP(TemporalDurationPrototypeAdd, kDontAdaptArgumentsSentinel)               \
  CPP(TemporalDurationPrototypeSubtract, kDontAdaptArgumentsSentinel)          \
  CPP(TemporalDurationPrototypeRound, kDontAdaptArgumentsSentinel)             \
  CPP(TemporalDurationPrototypeTotal, kDontAdaptArgumentsSentinel)             \
  CPP(TemporalDurationPrototypeToString, kDontAdaptArgumentsSentinel)          \
  CPP(TemporalDurationPrototypeToJSON, kDontAdaptArgumentsSentinel)            \
  CPP(TemporalDurationPrototypeToLocaleString, kDontAdaptArgumentsSentinel)    \
  CPP(TemporalDurationPrototypeValueOf, kDontAdaptArgumentsSentinel)           \
                                                                               \
  /* Temporal.Instant */                                                       \
  CPP(TemporalInstantConstructor, kDontAdaptArgumentsSentinel)                 \
  CPP(TemporalInstantFrom, kDontAdaptArgumentsSentinel)                        \
  CPP(TemporalInstantFromEpochMilliseconds, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalInstantFromEpochNanoseconds, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalInstantCompare, kDontAdaptArgumentsSentinel)                     \
  CPP(TemporalInstantPrototypeEpochMilliseconds, JSParameterCount(0))          \
  CPP(TemporalInstantPrototypeEpochNanoseconds, JSParameterCount(0))           \
  CPP(TemporalInstantPrototypeAdd, kDontAdaptArgumentsSentinel)                \
  CPP(TemporalInstantPrototypeSubtract, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalInstantPrototypeUntil, kDontAdaptArgumentsSentinel)              \
  CPP(TemporalInstantPrototypeSince, kDontAdaptArgumentsSentinel)              \
  CPP(TemporalInstantPrototypeRound, kDontAdaptArgumentsSentinel)              \
  CPP(TemporalInstantPrototypeEquals, kDontAdaptArgumentsSentinel)             \
  CPP(TemporalInstantPrototypeToString, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalInstantPrototypeToJSON, kDontAdaptArgumentsSentinel)             \
  CPP(TemporalInstantPrototypeToLocaleString, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalInstantPrototypeValueOf, kDontAdaptArgumentsSentinel)            \
  CPP(TemporalInstantPrototypeToZonedDateTimeISO, kDontAdaptArgumentsSentinel) \
                                                                               \
  /* Temporal.PlainYearMonth */                                                \
  CPP(TemporalPlainYearMonthConstructor, kDontAdaptArgumentsSentinel)          \
  CPP(TemporalPlainYearMonthFrom, kDontAdaptArgumentsSentinel)                 \
  CPP(TemporalPlainYearMonthCompare, kDontAdaptArgumentsSentinel)              \
  CPP(TemporalPlainYearMonthPrototypeCalendarId, JSParameterCount(0))          \
  CPP(TemporalPlainYearMonthPrototypeEra, JSParameterCount(0))                 \
  CPP(TemporalPlainYearMonthPrototypeEraYear, JSParameterCount(0))             \
  CPP(TemporalPlainYearMonthPrototypeYear, JSParameterCount(0))                \
  CPP(TemporalPlainYearMonthPrototypeMonth, JSParameterCount(0))               \
  CPP(TemporalPlainYearMonthPrototypeMonthCode, JSParameterCount(0))           \
  CPP(TemporalPlainYearMonthPrototypeDaysInYear, JSParameterCount(0))          \
  CPP(TemporalPlainYearMonthPrototypeDaysInMonth, JSParameterCount(0))         \
  CPP(TemporalPlainYearMonthPrototypeMonthsInYear, JSParameterCount(0))        \
  CPP(TemporalPlainYearMonthPrototypeInLeapYear, JSParameterCount(0))          \
  CPP(TemporalPlainYearMonthPrototypeWith, kDontAdaptArgumentsSentinel)        \
  CPP(TemporalPlainYearMonthPrototypeAdd, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalPlainYearMonthPrototypeSubtract, kDontAdaptArgumentsSentinel)    \
  CPP(TemporalPlainYearMonthPrototypeUntil, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalPlainYearMonthPrototypeSince, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalPlainYearMonthPrototypeEquals, kDontAdaptArgumentsSentinel)      \
  CPP(TemporalPlainYearMonthPrototypeToString, kDontAdaptArgumentsSentinel)    \
  CPP(TemporalPlainYearMonthPrototypeToJSON, kDontAdaptArgumentsSentinel)      \
  CPP(TemporalPlainYearMonthPrototypeToLocaleString,                           \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalPlainYearMonthPrototypeValueOf, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalPlainYearMonthPrototypeToPlainDate, kDontAdaptArgumentsSentinel) \
                                                                               \
  /* Temporal.PlainMonthDay */                                                 \
  CPP(TemporalPlainMonthDayConstructor, kDontAdaptArgumentsSentinel)           \
  CPP(TemporalPlainMonthDayFrom, kDontAdaptArgumentsSentinel)                  \
  /* There are no compare for PlainMonthDay */                                 \
  CPP(TemporalPlainMonthDayPrototypeCalendarId, JSParameterCount(0))           \
  CPP(TemporalPlainMonthDayPrototypeMonthCode, JSParameterCount(0))            \
  CPP(TemporalPlainMonthDayPrototypeDay, JSParameterCount(0))                  \
  CPP(TemporalPlainMonthDayPrototypeWith, kDontAdaptArgumentsSentinel)         \
  CPP(TemporalPlainMonthDayPrototypeEquals, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalPlainMonthDayPrototypeToString, kDontAdaptArgumentsSentinel)     \
  CPP(TemporalPlainMonthDayPrototypeToJSON, kDontAdaptArgumentsSentinel)       \
  CPP(TemporalPlainMonthDayPrototypeToLocaleString,                            \
      kDontAdaptArgumentsSentinel)                                             \
  CPP(TemporalPlainMonthDayPrototypeValueOf, kDontAdaptArgumentsSentinel)      \
  CPP(TemporalPlainMonthDayPrototypeToPlainDate, kDontAdaptArgumentsSentinel)  \
                                                                               \
  CPP(DatePrototypeToTemporalInstant, kDontAdaptArgumentsSentinel)
#else  // V8_TEMPORAL_SUPPORT
#define BUILTIN_LIST_TEMPORAL(CPP, TFJ)
#endif  // V8_TEMPORAL_SUPPORT

#ifdef V8_INTL_SUPPORT
#define BUILTIN_LIST_INTL(CPP, TFJ, TFS)                                       \
  CPP(CollatorConstructor, kDontAdaptArgumentsSentinel)                        \
  CPP(CollatorInternalCompare, JSParameterCount(2))                            \
  CPP(CollatorPrototypeCompare, kDontAdaptArgumentsSentinel)                   \
  CPP(CollatorSupportedLocalesOf, kDontAdaptArgumentsSentinel)                 \
  CPP(CollatorPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)           \
  CPP(DatePrototypeToLocaleDateString, kDontAdaptArgumentsSentinel)            \
  CPP(DatePrototypeToLocaleString, kDontAdaptArgumentsSentinel)                \
  CPP(DatePrototypeToLocaleTimeString, kDontAdaptArgumentsSentinel)            \
  CPP(DateTimeFormatConstructor, kDontAdaptArgumentsSentinel)                  \
  CPP(DateTimeFormatInternalFormat, JSParameterCount(1))                       \
  CPP(DateTimeFormatPrototypeFormat, kDontAdaptArgumentsSentinel)              \
  CPP(DateTimeFormatPrototypeFormatRange, kDontAdaptArgumentsSentinel)         \
  CPP(DateTimeFormatPrototypeFormatRangeToParts, kDontAdaptArgumentsSentinel)  \
  CPP(DateTimeFormatPrototypeFormatToParts, kDontAdaptArgumentsSentinel)       \
  CPP(DateTimeFormatPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)     \
  CPP(DateTimeFormatSupportedLocalesOf, kDontAdaptArgumentsSentinel)           \
  CPP(DisplayNamesConstructor, kDontAdaptArgumentsSentinel)                    \
  CPP(DisplayNamesPrototypeOf, kDontAdaptArgumentsSentinel)                    \
  CPP(DisplayNamesPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)       \
  CPP(DisplayNamesSupportedLocalesOf, kDontAdaptArgumentsSentinel)             \
  CPP(DurationFormatConstructor, kDontAdaptArgumentsSentinel)                  \
  CPP(DurationFormatPrototypeFormat, kDontAdaptArgumentsSentinel)              \
  CPP(DurationFormatPrototypeFormatToParts, kDontAdaptArgumentsSentinel)       \
  CPP(DurationFormatPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)     \
  CPP(DurationFormatSupportedLocalesOf, kDontAdaptArgumentsSentinel)           \
  CPP(IntlGetCanonicalLocales, kDontAdaptArgumentsSentinel)                    \
  CPP(IntlSupportedValuesOf, kDontAdaptArgumentsSentinel)                      \
  CPP(ListFormatConstructor, kDontAdaptArgumentsSentinel)                      \
  TFJ(ListFormatPrototypeFormat, kDontAdaptArgumentsSentinel)                  \
  TFJ(ListFormatPrototypeFormatToParts, kDontAdaptArgumentsSentinel)           \
  CPP(ListFormatPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)         \
  CPP(ListFormatSupportedLocalesOf, kDontAdaptArgumentsSentinel)               \
  CPP(LocaleConstructor, kDontAdaptArgumentsSentinel)                          \
  CPP(LocalePrototypeBaseName, JSParameterCount(0))                            \
  CPP(LocalePrototypeCalendar, JSParameterCount(0))                            \
  CPP(LocalePrototypeCaseFirst, JSParameterCount(0))                           \
  CPP(LocalePrototypeCollation, JSParameterCount(0))                           \
  CPP(LocalePrototypeFirstDayOfWeek, JSParameterCount(0))                      \
  CPP(LocalePrototypeGetCalendars, kDontAdaptArgumentsSentinel)                \
  CPP(LocalePrototypeGetCollations, kDontAdaptArgumentsSentinel)               \
  CPP(LocalePrototypeGetHourCycles, kDontAdaptArgumentsSentinel)               \
  CPP(LocalePrototypeGetNumberingSystems, kDontAdaptArgumentsSentinel)         \
  CPP(LocalePrototypeGetTimeZones, kDontAdaptArgumentsSentinel)                \
  CPP(LocalePrototypeGetTextInfo, kDontAdaptArgumentsSentinel)                 \
  CPP(LocalePrototypeGetWeekInfo, kDontAdaptArgumentsSentinel)                 \
  CPP(LocalePrototypeHourCycle, JSParameterCount(0))                           \
  CPP(LocalePrototypeLanguage, JSParameterCount(0))                            \
  CPP(LocalePrototypeMaximize, kDontAdaptArgumentsSentinel)                    \
  CPP(LocalePrototypeMinimize, kDontAdaptArgumentsSentinel)                    \
  CPP(LocalePrototypeNumeric, JSParameterCount(0))                             \
  CPP(LocalePrototypeNumberingSystem, JSParameterCount(0))                     \
  CPP(LocalePrototypeRegion, JSParameterCount(0))                              \
  CPP(LocalePrototypeScript, JSParameterCount(0))                              \
  CPP(LocalePrototypeToString, kDontAdaptArgumentsSentinel)                    \
  CPP(LocalePrototypeVariants, JSParameterCount(0))                            \
  CPP(NumberFormatConstructor, kDontAdaptArgumentsSentinel)                    \
  CPP(NumberFormatInternalFormatNumber, JSParameterCount(1))                   \
  CPP(NumberFormatPrototypeFormatNumber, kDontAdaptArgumentsSentinel)          \
  CPP(NumberFormatPrototypeFormatRange, kDontAdaptArgumentsSentinel)           \
  CPP(NumberFormatPrototypeFormatRangeToParts, kDontAdaptArgumentsSentinel)    \
  CPP(NumberFormatPrototypeFormatToParts, kDontAdaptArgumentsSentinel)         \
  CPP(NumberFormatPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)       \
  CPP(NumberFormatSupportedLocalesOf, kDontAdaptArgumentsSentinel)             \
  CPP(PluralRulesConstructor, kDontAdaptArgumentsSentinel)                     \
  CPP(PluralRulesPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)        \
  CPP(PluralRulesPrototypeSelect, kDontAdaptArgumentsSentinel)                 \
  CPP(PluralRulesPrototypeSelectRange, kDontAdaptArgumentsSentinel)            \
  CPP(PluralRulesSupportedLocalesOf, kDontAdaptArgumentsSentinel)              \
  CPP(RelativeTimeFormatConstructor, kDontAdaptArgumentsSentinel)              \
  CPP(RelativeTimeFormatPrototypeFormat, kDontAdaptArgumentsSentinel)          \
  CPP(RelativeTimeFormatPrototypeFormatToParts, kDontAdaptArgumentsSentinel)   \
  CPP(RelativeTimeFormatPrototypeResolvedOptions, kDontAdaptArgumentsSentinel) \
  CPP(RelativeTimeFormatSupportedLocalesOf, kDontAdaptArgumentsSentinel)       \
  CPP(SegmenterConstructor, kDontAdaptArgumentsSentinel)                       \
  CPP(SegmenterPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)          \
  CPP(SegmenterPrototypeSegment, kDontAdaptArgumentsSentinel)                  \
  CPP(SegmenterSupportedLocalesOf, kDontAdaptArgumentsSentinel)                \
  CPP(SegmentIteratorPrototypeNext, kDontAdaptArgumentsSentinel)               \
  CPP(SegmentsPrototypeContaining, kDontAdaptArgumentsSentinel)                \
  CPP(SegmentsPrototypeIterator, JSParameterCount(0))                          \
  CPP(StringPrototypeLocaleCompareIntl, kDontAdaptArgumentsSentinel)           \
  CPP(StringPrototypeNormalizeIntl, kDontAdaptArgumentsSentinel)               \
  TFJ(StringPrototypeToLocaleLowerCase, kDontAdaptArgumentsSentinel)           \
  CPP(StringPrototypeToLocaleUpperCase, kDontAdaptArgumentsSentinel)           \
  TFJ(StringPrototypeToLowerCaseIntl, kJSArgcReceiverSlots, kReceiver)         \
  CPP(StringPrototypeToUpperCaseIntl, kDontAdaptArgumentsSentinel)             \
  TFS(StringToLowerCaseIntl, NeedsContext::kYes, kString)                      \
  IF_WASM(TFS, WasmStringToLowerCaseIntl, NeedsContext::kYes, kString)         \
                                                                               \
  CPP(V8BreakIteratorConstructor, kDontAdaptArgumentsSentinel)                 \
  CPP(V8BreakIteratorInternalAdoptText, JSParameterCount(1))                   \
  CPP(V8BreakIteratorInternalBreakType, JSParameterCount(0))                   \
  CPP(V8BreakIteratorInternalCurrent, JSParameterCount(0))                     \
  CPP(V8BreakIteratorInternalFirst, JSParameterCount(0))                       \
  CPP(V8BreakIteratorInternalNext, JSParameterCount(0))                        \
  CPP(V8BreakIteratorPrototypeAdoptText, kDontAdaptArgumentsSentinel)          \
  CPP(V8BreakIteratorPrototypeBreakType, kDontAdaptArgumentsSentinel)          \
  CPP(V8BreakIteratorPrototypeCurrent, kDontAdaptArgumentsSentinel)            \
  CPP(V8BreakIteratorPrototypeFirst, kDontAdaptArgumentsSentinel)              \
  CPP(V8BreakIteratorPrototypeNext, kDontAdaptArgumentsSentinel)               \
  CPP(V8BreakIteratorPrototypeResolvedOptions, kDontAdaptArgumentsSentinel)    \
  CPP(V8BreakIteratorSupportedLocalesOf, kDontAdaptArgumentsSentinel)
#else
#define BUILTIN_LIST_INTL(CPP, TFJ, TFS)                             \
  /* non-locale specific fallback version */                         \
  CPP(StringPrototypeLocaleCompare, JSParameterCount(1))             \
  /* no-op fallback version */                                       \
  CPP(StringPrototypeNormalize, kDontAdaptArgumentsSentinel)         \
  /* same as toLowercase; fallback version */                        \
  CPP(StringPrototypeToLocaleLowerCase, kDontAdaptArgumentsSentinel) \
  /* same as toUppercase; fallback version */                        \
  CPP(StringPrototypeToLocaleUpperCase, kDontAdaptArgumentsSentinel) \
  /* (obsolete) Unibrow version */                                   \
  CPP(StringPrototypeToLowerCase, kDontAdaptArgumentsSentinel)       \
  /* (obsolete) Unibrow version */                                   \
  CPP(StringPrototypeToUpperCase, kDontAdaptArgumentsSentinel)
#endif  // V8_INTL_SUPPORT

#ifdef V8_DUMPLING
#define BUILTIN_LIST_DUMPLING(ASM) ASM(DumpFrame, Void)
#else
#define BUILTIN_LIST_DUMPLING(ASM)
#endif

#define BUILTIN_LIST(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH, BCH_TSA, BCH, \
                     ASM)                                                     \
  BUILTIN_LIST_BASE(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH, ASM)           \
  BUILTIN_LIST_FROM_TORQUE(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH, ASM)    \
  BUILTIN_LIST_INTL(CPP, TFJ, TFS)                                            \
  BUILTIN_LIST_TEMPORAL(CPP, TFJ)                                             \
  BUILTIN_LIST_DUMPLING(ASM)                                                  \
  BUILTIN_LIST_BYTECODE_HANDLERS(BCH_TSA, BCH)

// See the comment on top of BUILTIN_LIST_BASE_TIER0 for an explanation of
// tiers.
#define BUILTIN_LIST_TIER0(CPP, TFJ, TFC, TFS, TFH, BCH, ASM) \
  BUILTIN_LIST_BASE_TIER0(CPP, TFJ, TFC, TFS, TFH, ASM)

#define BUILTIN_LIST_TIER1(CPP, TFJ_TSA, TFJ, TFC, TFS, TFH, BCH_TSA, BCH, \
                           ASM)                                            \
  BUILTIN_LIST_BASE_TIER1(CPP, TFJ_TSA, TFJ, TFC, TFS, TFH, ASM)           \
  BUILTIN_LIST_FROM_TORQUE(CPP, TFJ_TSA, TFJ, TFC_TSA, TFC, TFS, TFH, ASM) \
  BUILTIN_LIST_INTL(CPP, TFJ, TFS)                                         \
  BUILTIN_LIST_TEMPORAL(CPP, TFJ)                                          \
  BUILTIN_LIST_DUMPLING(ASM)                                               \
  BUILTIN_LIST_BYTECODE_HANDLERS(BCH_TSA, BCH)

// The exception thrown in the following builtins are caught
// internally and result in a promise rejection.
#define BUILTIN_PROMISE_REJECTION_PREDICTION_LIST(V) \
  V(AsyncFromSyncIteratorPrototypeNext)              \
  V(AsyncFromSyncIteratorPrototypeReturn)            \
  V(AsyncFromSyncIteratorPrototypeThrow)             \
  V(AsyncFunctionAwait)                              \
  V(AsyncGeneratorResolve)                           \
  V(AsyncGeneratorAwait)                             \
  V(PromiseAll)                                      \
  V(PromiseAny)                                      \
  V(PromiseConstructor)                              \
  V(PromiseConstructorLazyDeoptContinuation)         \
  V(PromiseFulfillReactionJob)                       \
  V(PromiseRejectReactionJob)                        \
  V(PromiseRace)                                     \
  V(PromiseTry)                                      \
  V(ResolvePromise)                                  \
  V(AsyncDisposeFromSyncDispose)

#define IGNORE_BUILTIN(...)

#define BUILTIN_LIST_C(V)                                                      \
  BUILTIN_LIST(V, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFJ_TSA(V)                                                \
  BUILTIN_LIST(IGNORE_BUILTIN, V, IGNORE_BUILTIN, IGNORE_BUILTIN,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFJ(V)                                                    \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, V, IGNORE_BUILTIN,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFC_TSA(V)                                                \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, V,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFC(V)                                                    \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               V, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFS(V)                                                    \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, V, IGNORE_BUILTIN, IGNORE_BUILTIN,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFH(V)                                                    \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, V, IGNORE_BUILTIN,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_BCH_TSA(V)                                                \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, V,              \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_BCH(V)                                                    \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               V, IGNORE_BUILTIN)

#define BUILTIN_LIST_A(V)                                                      \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, V)

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DEFINITIONS_H_
