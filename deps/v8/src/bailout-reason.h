// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BAILOUT_REASON_H_
#define V8_BAILOUT_REASON_H_

namespace v8 {
namespace internal {

// TODO(svenpanne) introduce an AbortReason and partition this list
#define ERROR_MESSAGES_LIST(V)                                                 \
  V(kNoReason, "no reason")                                                    \
                                                                               \
  V(k32BitValueInRegisterIsNotZeroExtended,                                    \
    "32 bit value in register is not zero-extended")                           \
  V(kAllocatingNonEmptyPackedArray, "Allocating non-empty packed array")       \
  V(kAllocationIsNotDoubleAligned, "Allocation is not double aligned")         \
  V(kAPICallReturnedInvalidObject, "API call returned invalid object")         \
  V(kBailedOutDueToDependencyChange, "Bailed out due to dependency change")    \
  V(kClassConstructorFunction, "Class constructor function")                   \
  V(kClassLiteral, "Class literal")                                            \
  V(kCodeGenerationFailed, "Code generation failed")                           \
  V(kCodeObjectNotProperlyPatched, "Code object not properly patched")         \
  V(kComputedPropertyName, "Computed property name")                           \
  V(kContextAllocatedArguments, "Context-allocated arguments")                 \
  V(kDebuggerStatement, "DebuggerStatement")                                   \
  V(kDeclarationInCatchContext, "Declaration in catch context")                \
  V(kDeclarationInWithContext, "Declaration in with context")                  \
  V(kDynamicImport, "Dynamic module import")                                   \
  V(kCyclicObjectStateDetectedInEscapeAnalysis,                                \
    "Cyclic object state detected by escape analysis")                         \
  V(kEval, "eval")                                                             \
  V(kExpectedAllocationSite, "Expected allocation site")                       \
  V(kExpectedBooleanValue, "Expected boolean value")                           \
  V(kExpectedFeedbackVector, "Expected feedback vector")                       \
  V(kExpectedHeapNumber, "Expected HeapNumber")                                \
  V(kExpectedNonIdenticalObjects, "Expected non-identical objects")            \
  V(kExpectedOptimizationSentinel,                                             \
    "Expected optimized code cell or optimization sentinel")                   \
  V(kExpectedNewSpaceObject, "Expected new space object")                      \
  V(kExpectedUndefinedOrCell, "Expected undefined or cell in register")        \
  V(kForOfStatement, "ForOfStatement")                                         \
  V(kFunctionBeingDebugged, "Function is being debugged")                      \
  V(kFunctionCallsEval, "Function calls eval")                                 \
  V(kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,                      \
    "The function_data field should be a BytecodeArray on interpreter entry")  \
  V(kGenerator, "Generator")                                                   \
  V(kGetIterator, "GetIterator")                                               \
  V(kGraphBuildingFailed, "Optimized graph construction failed")               \
  V(kHeapNumberMapRegisterClobbered, "HeapNumberMap register clobbered")       \
  V(kIndexIsNegative, "Index is negative")                                     \
  V(kIndexIsTooLarge, "Index is too large")                                    \
  V(kInputGPRIsExpectedToHaveUpper32Cleared,                                   \
    "Input GPR is expected to have upper32 cleared")                           \
  V(kInputStringTooLong, "Input string too long")                              \
  V(kInvalidBytecode, "Invalid bytecode")                                      \
  V(kInvalidElementsKindForInternalArrayOrInternalPackedArray,                 \
    "Invalid ElementsKind for InternalArray or InternalPackedArray")           \
  V(kInvalidFullCodegenState, "invalid full-codegen state")                    \
  V(kInvalidHandleScopeLevel, "Invalid HandleScope level")                     \
  V(kInvalidJumpTableIndex, "Invalid jump table index")                        \
  V(kInvalidRegisterFileInGenerator, "invalid register file in generator")     \
  V(kLiveEdit, "LiveEdit")                                                     \
  V(kMissingBytecodeArray, "Missing bytecode array from function")             \
  V(kNativeFunctionLiteral, "Native function literal")                         \
  V(kNoCasesLeft, "No cases left")                                             \
  V(kNonObject, "Non-object value")                                            \
  V(kNotEnoughVirtualRegistersRegalloc,                                        \
    "Not enough virtual registers (regalloc)")                                 \
  V(kOffsetOutOfRange, "Offset out of range")                                  \
  V(kOperandIsASmiAndNotABoundFunction,                                        \
    "Operand is a smi and not a bound function")                               \
  V(kOperandIsASmiAndNotAFixedArray, "Operand is a smi and not a fixed array") \
  V(kOperandIsASmiAndNotAFunction, "Operand is a smi and not a function")      \
  V(kOperandIsASmiAndNotAGeneratorObject,                                      \
    "Operand is a smi and not a generator object")                             \
  V(kOperandIsASmi, "Operand is a smi")                                        \
  V(kOperandIsNotABoundFunction, "Operand is not a bound function")            \
  V(kOperandIsNotAFixedArray, "Operand is not a fixed array")                  \
  V(kOperandIsNotAFunction, "Operand is not a function")                       \
  V(kOperandIsNotAGeneratorObject, "Operand is not a generator object")        \
  V(kOperandIsNotASmi, "Operand is not a smi")                                 \
  V(kOperandIsNotSmi, "Operand is not smi")                                    \
  V(kObjectTagged, "The object is tagged")                                     \
  V(kObjectNotTagged, "The object is not tagged")                              \
  V(kOptimizationDisabled, "Optimization disabled")                            \
  V(kOptimizationDisabledForTest, "Optimization disabled for test")            \
  V(kReceivedInvalidReturnAddress, "Received invalid return address")          \
  V(kReferenceToAVariableWhichRequiresDynamicLookup,                           \
    "Reference to a variable which requires dynamic lookup")                   \
  V(kReferenceToModuleVariable, "Reference to module-allocated variable")      \
  V(kRegisterDidNotMatchExpectedRoot, "Register did not match expected root")  \
  V(kRegisterWasClobbered, "Register was clobbered")                           \
  V(kRememberedSetPointerInNewSpace, "Remembered set pointer is in new space") \
  V(kRestParameter, "Rest parameters")                                         \
  V(kReturnAddressNotFoundInFrame, "Return address not found in frame")        \
  V(kSpreadCall, "Call with spread argument")                                  \
  V(kStackAccessBelowStackPointer, "Stack access below stack pointer")         \
  V(kStackFrameTypesMustMatch, "Stack frame types must match")                 \
  V(kSuperReference, "Super reference")                                        \
  V(kTailCall, "Tail call")                                                    \
  V(kTheCurrentStackPointerIsBelowCsp,                                         \
    "The current stack pointer is below csp")                                  \
  V(kTheStackWasCorruptedByMacroAssemblerCall,                                 \
    "The stack was corrupted by MacroAssembler::Call()")                       \
  V(kTooManyParameters, "Too many parameters")                                 \
  V(kTryCatchStatement, "TryCatchStatement")                                   \
  V(kTryFinallyStatement, "TryFinallyStatement")                               \
  V(kUnalignedAllocationInNewSpace, "Unaligned allocation in new space")       \
  V(kUnalignedCellInWriteBarrier, "Unaligned cell in write barrier")           \
  V(kUnexpectedColorFound, "Unexpected color bit pattern found")               \
  V(kUnexpectedElementsKindInArrayConstructor,                                 \
    "Unexpected ElementsKind in array constructor")                            \
  V(kUnexpectedFallthroughFromCharCodeAtSlowCase,                              \
    "Unexpected fallthrough from CharCodeAt slow case")                        \
  V(kUnexpectedFallThroughFromStringComparison,                                \
    "Unexpected fall-through from string comparison")                          \
  V(kUnexpectedFallthroughToCharCodeAtSlowCase,                                \
    "Unexpected fallthrough to CharCodeAt slow case")                          \
  V(kUnexpectedInitialMapForArrayFunction1,                                    \
    "Unexpected initial map for Array function (1)")                           \
  V(kUnexpectedInitialMapForArrayFunction2,                                    \
    "Unexpected initial map for Array function (2)")                           \
  V(kUnexpectedInitialMapForArrayFunction,                                     \
    "Unexpected initial map for Array function")                               \
  V(kUnexpectedInitialMapForInternalArrayFunction,                             \
    "Unexpected initial map for InternalArray function")                       \
  V(kUnexpectedLevelAfterReturnFromApiCall,                                    \
    "Unexpected level after return from api call")                             \
  V(kUnexpectedNegativeValue, "Unexpected negative value")                     \
  V(kUnexpectedFunctionIDForInvokeIntrinsic,                                   \
    "Unexpected runtime function id for the InvokeIntrinsic bytecode")         \
  V(kUnexpectedFPCRMode, "Unexpected FPCR mode.")                              \
  V(kUnexpectedStackDepth, "Unexpected operand stack depth in full-codegen")   \
  V(kUnexpectedStackPointer, "The stack pointer is not the expected value")    \
  V(kUnexpectedStringType, "Unexpected string type")                           \
  V(kUnexpectedValue, "Unexpected value")                                      \
  V(kUnsupportedModuleOperation, "Unsupported module operation")               \
  V(kUnsupportedNonPrimitiveCompare, "Unsupported non-primitive compare")      \
  V(kUnexpectedReturnFromFrameDropper,                                         \
    "Unexpectedly returned from dropping frames")                              \
  V(kUnexpectedReturnFromThrow, "Unexpectedly returned from a throw")          \
  V(kVariableResolvedToWithContext, "Variable resolved to with context")       \
  V(kWithStatement, "WithStatement")                                           \
  V(kWrongFunctionContext, "Wrong context passed to function")                 \
  V(kWrongAddressOrValuePassedToRecordWrite,                                   \
    "Wrong address or value passed to RecordWrite")                            \
  V(kWrongArgumentCountForInvokeIntrinsic,                                     \
    "Wrong number of arguments for intrinsic")                                 \
  V(kShouldNotDirectlyEnterOsrFunction,                                        \
    "Should not directly enter OSR-compiled function")                         \
  V(kUnexpectedReturnFromWasmTrap,                                             \
    "Should not return after throwing a wasm trap")

#define ERROR_MESSAGES_CONSTANTS(C, T) C,
enum BailoutReason {
  ERROR_MESSAGES_LIST(ERROR_MESSAGES_CONSTANTS) kLastErrorMessage
};
#undef ERROR_MESSAGES_CONSTANTS

const char* GetBailoutReason(BailoutReason reason);

}  // namespace internal
}  // namespace v8

#endif  // V8_BAILOUT_REASON_H_
