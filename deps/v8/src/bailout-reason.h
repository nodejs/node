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
  V(kArgumentsObjectValueInATestContext,                                       \
    "Arguments object value in a test context")                                \
  V(kArrayIndexConstantValueTooBig, "Array index constant value too big")      \
  V(kAssignmentToLetVariableBeforeInitialization,                              \
    "Assignment to let variable before initialization")                        \
  V(kAssignmentToLOOKUPVariable, "Assignment to LOOKUP variable")              \
  V(kAssignmentToParameterFunctionUsesArgumentsObject,                         \
    "Assignment to parameter, function uses arguments object")                 \
  V(kAssignmentToParameterInArgumentsObject,                                   \
    "Assignment to parameter in arguments object")                             \
  V(kBadValueContextForArgumentsObjectValue,                                   \
    "Bad value context for arguments object value")                            \
  V(kBadValueContextForArgumentsValue,                                         \
    "Bad value context for arguments value")                                   \
  V(kBailedOutDueToDependencyChange, "Bailed out due to dependency change")    \
  V(kBailoutWasNotPrepared, "Bailout was not prepared")                        \
  V(kBothRegistersWereSmisInSelectNonSmi,                                      \
    "Both registers were smis in SelectNonSmi")                                \
  V(kClassConstructorFunction, "Class constructor function")                   \
  V(kClassLiteral, "Class literal")                                            \
  V(kCodeGenerationFailed, "Code generation failed")                           \
  V(kCodeObjectNotProperlyPatched, "Code object not properly patched")         \
  V(kCompoundAssignmentToLookupSlot, "Compound assignment to lookup slot")     \
  V(kComputedPropertyName, "Computed property name")                           \
  V(kContextAllocatedArguments, "Context-allocated arguments")                 \
  V(kCopyBuffersOverlap, "Copy buffers overlap")                               \
  V(kCouldNotGenerateZero, "Could not generate +0.0")                          \
  V(kCouldNotGenerateNegativeZero, "Could not generate -0.0")                  \
  V(kDebuggerStatement, "DebuggerStatement")                                   \
  V(kDeclarationInCatchContext, "Declaration in catch context")                \
  V(kDeclarationInWithContext, "Declaration in with context")                  \
  V(kDefaultNaNModeNotSet, "Default NaN mode not set")                         \
  V(kDeleteWithGlobalVariable, "Delete with global variable")                  \
  V(kDeleteWithNonGlobalVariable, "Delete with non-global variable")           \
  V(kDestinationOfCopyNotAligned, "Destination of copy not aligned")           \
  V(kDontDeleteCellsCannotContainTheHole,                                      \
    "DontDelete cells can't contain the hole")                                 \
  V(kDoExpressionUnmodelable,                                                  \
    "Encountered a do-expression with unmodelable control statements")         \
  V(kDoPushArgumentNotImplementedForDoubleType,                                \
    "DoPushArgument not implemented for double type")                          \
  V(kDynamicImport, "Dynamic module import")                                   \
  V(kEliminatedBoundsCheckFailed, "Eliminated bounds check failed")            \
  V(kEmitLoadRegisterUnsupportedDoubleImmediate,                               \
    "EmitLoadRegister: Unsupported double immediate")                          \
  V(kCyclicObjectStateDetectedInEscapeAnalysis,                                \
    "Cyclic object state detected by escape analysis")                         \
  V(kEval, "eval")                                                             \
  V(kExpectedAllocationSite, "Expected allocation site")                       \
  V(kExpectedBooleanValue, "Expected boolean value")                           \
  V(kExpectedFixedDoubleArrayMap,                                              \
    "Expected a fixed double array map in fast shallow clone array literal")   \
  V(kExpectedFunctionObject, "Expected function object in register")           \
  V(kExpectedHeapNumber, "Expected HeapNumber")                                \
  V(kExpectedJSReceiver, "Expected object to have receiver type")              \
  V(kExpectedNativeContext, "Expected native context")                         \
  V(kExpectedNonIdenticalObjects, "Expected non-identical objects")            \
  V(kExpectedNonNullContext, "Expected non-null context")                      \
  V(kExpectedPositiveZero, "Expected +0.0")                                    \
  V(kExpectedNewSpaceObject, "Expected new space object")                      \
  V(kExpectedUndefinedOrCell, "Expected undefined or cell in register")        \
  V(kExternalStringExpectedButNotFound,                                        \
    "External string expected, but not found")                                 \
  V(kForInStatementWithNonLocalEachVariable,                                   \
    "ForInStatement with non-local each variable")                             \
  V(kForOfStatement, "ForOfStatement")                                         \
  V(kFunctionBeingDebugged, "Function is being debugged")                      \
  V(kFunctionCallsEval, "Function calls eval")                                 \
  V(kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,                      \
    "The function_data field should be a BytecodeArray on interpreter entry")  \
  V(kGeneratedCodeIsTooLarge, "Generated code is too large")                   \
  V(kGenerator, "Generator")                                                   \
  V(kGetIterator, "GetIterator")                                               \
  V(kGlobalFunctionsMustHaveInitialMap,                                        \
    "Global functions must have initial map")                                  \
  V(kGraphBuildingFailed, "Optimized graph construction failed")               \
  V(kHeapNumberMapRegisterClobbered, "HeapNumberMap register clobbered")       \
  V(kHydrogenFilter, "Optimization disabled by filter")                        \
  V(kIndexIsNegative, "Index is negative")                                     \
  V(kIndexIsTooLarge, "Index is too large")                                    \
  V(kInliningBailedOut, "Inlining bailed out")                                 \
  V(kInputGPRIsExpectedToHaveUpper32Cleared,                                   \
    "Input GPR is expected to have upper32 cleared")                           \
  V(kInputStringTooLong, "Input string too long")                              \
  V(kInteger32ToSmiFieldWritingToNonSmiLocation,                               \
    "Integer32ToSmiField writing to non-smi location")                         \
  V(kInvalidBytecode, "Invalid bytecode")                                      \
  V(kInvalidElementsKindForInternalArrayOrInternalPackedArray,                 \
    "Invalid ElementsKind for InternalArray or InternalPackedArray")           \
  V(kInvalidFrameForFastNewRestArgumentsStub,                                  \
    "Invalid frame for FastNewRestArgumentsStub")                              \
  V(kInvalidFrameForFastNewSloppyArgumentsStub,                                \
    "Invalid frame for FastNewSloppyArgumentsStub")                            \
  V(kInvalidFrameForFastNewStrictArgumentsStub,                                \
    "Invalid frame for FastNewStrictArgumentsStub")                            \
  V(kInvalidFullCodegenState, "invalid full-codegen state")                    \
  V(kInvalidHandleScopeLevel, "Invalid HandleScope level")                     \
  V(kInvalidJumpTableIndex, "Invalid jump table index")                        \
  V(kInvalidLeftHandSideInAssignment, "Invalid left-hand side in assignment")  \
  V(kInvalidLhsInCompoundAssignment, "Invalid lhs in compound assignment")     \
  V(kInvalidLhsInCountOperation, "Invalid lhs in count operation")             \
  V(kInvalidMinLength, "Invalid min_length")                                   \
  V(kInvalidRegisterFileInGenerator, "invalid register file in generator")     \
  V(kLiveEdit, "LiveEdit")                                                     \
  V(kLookupVariableInCountOperation, "Lookup variable in count operation")     \
  V(kMapBecameDeprecated, "Map became deprecated")                             \
  V(kMapBecameUnstable, "Map became unstable")                                 \
  V(kMissingBytecodeArray, "Missing bytecode array from function")             \
  V(kNativeFunctionLiteral, "Native function literal")                         \
  V(kNeedSmiLiteral, "Need a Smi literal here")                                \
  V(kNoCasesLeft, "No cases left")                                             \
  V(kNonInitializerAssignmentToConst, "Non-initializer assignment to const")   \
  V(kNonSmiIndex, "Non-smi index")                                             \
  V(kNonSmiKeyInArrayLiteral, "Non-smi key in array literal")                  \
  V(kNonSmiValue, "Non-smi value")                                             \
  V(kNonObject, "Non-object value")                                            \
  V(kNotEnoughVirtualRegistersForValues,                                       \
    "Not enough virtual registers for values")                                 \
  V(kNotEnoughSpillSlotsForOsr, "Not enough spill slots for OSR")              \
  V(kNotEnoughVirtualRegistersRegalloc,                                        \
    "Not enough virtual registers (regalloc)")                                 \
  V(kObjectLiteralWithComplexProperty, "Object literal with complex property") \
  V(kOffsetOutOfRange, "Offset out of range")                                  \
  V(kOperandIsASmiAndNotABoundFunction,                                        \
    "Operand is a smi and not a bound function")                               \
  V(kOperandIsASmiAndNotAFunction, "Operand is a smi and not a function")      \
  V(kOperandIsASmiAndNotAGeneratorObject,                                      \
    "Operand is a smi and not a generator object")                             \
  V(kOperandIsASmiAndNotAName, "Operand is a smi and not a name")              \
  V(kOperandIsASmiAndNotAReceiver, "Operand is a smi and not a receiver")      \
  V(kOperandIsASmiAndNotAString, "Operand is a smi and not a string")          \
  V(kOperandIsASmi, "Operand is a smi")                                        \
  V(kOperandIsNotABoundFunction, "Operand is not a bound function")            \
  V(kOperandIsNotAFunction, "Operand is not a function")                       \
  V(kOperandIsNotAGeneratorObject, "Operand is not a generator object")        \
  V(kOperandIsNotAReceiver, "Operand is not a receiver")                       \
  V(kOperandIsNotASmi, "Operand is not a smi")                                 \
  V(kOperandIsNotAString, "Operand is not a string")                           \
  V(kOperandIsNotSmi, "Operand is not smi")                                    \
  V(kObjectTagged, "The object is tagged")                                     \
  V(kObjectNotTagged, "The object is not tagged")                              \
  V(kOptimizationDisabled, "Optimization disabled")                            \
  V(kOptimizationDisabledForTest, "Optimization disabled for test")            \
  V(kDeoptimizedTooManyTimes, "Deoptimized too many times")                    \
  V(kOutOfVirtualRegistersWhileTryingToAllocateTempRegister,                   \
    "Out of virtual registers while trying to allocate temp register")         \
  V(kParseScopeError, "Parse/scope error")                                     \
  V(kPossibleDirectCallToEval, "Possible direct call to eval")                 \
  V(kReceivedInvalidReturnAddress, "Received invalid return address")          \
  V(kReferenceToAVariableWhichRequiresDynamicLookup,                           \
    "Reference to a variable which requires dynamic lookup")                   \
  V(kReferenceToGlobalLexicalVariable, "Reference to global lexical variable") \
  V(kReferenceToModuleVariable, "Reference to module-allocated variable")      \
  V(kReferenceToUninitializedVariable, "Reference to uninitialized variable")  \
  V(kRegisterDidNotMatchExpectedRoot, "Register did not match expected root")  \
  V(kRegisterWasClobbered, "Register was clobbered")                           \
  V(kRememberedSetPointerInNewSpace, "Remembered set pointer is in new space") \
  V(kRestParameter, "Rest parameters")                                         \
  V(kReturnAddressNotFoundInFrame, "Return address not found in frame")        \
  V(kSloppyFunctionExpectsJSReceiverReceiver,                                  \
    "Sloppy function expects JSReceiver as receiver.")                         \
  V(kSmiAdditionOverflow, "Smi addition overflow")                             \
  V(kSmiSubtractionOverflow, "Smi subtraction overflow")                       \
  V(kSpreadCall, "Call with spread argument")                                  \
  V(kStackAccessBelowStackPointer, "Stack access below stack pointer")         \
  V(kStackFrameTypesMustMatch, "Stack frame types must match")                 \
  V(kSuperReference, "Super reference")                                        \
  V(kTailCall, "Tail call")                                                    \
  V(kTheCurrentStackPointerIsBelowCsp,                                         \
    "The current stack pointer is below csp")                                  \
  V(kTheStackWasCorruptedByMacroAssemblerCall,                                 \
    "The stack was corrupted by MacroAssembler::Call()")                       \
  V(kTooManyParametersLocals, "Too many parameters/locals")                    \
  V(kTooManyParameters, "Too many parameters")                                 \
  V(kTooManySpillSlotsNeededForOSR, "Too many spill slots needed for OSR")     \
  V(kToOperand32UnsupportedImmediate, "ToOperand32 unsupported immediate.")    \
  V(kToOperandIsDoubleRegisterUnimplemented,                                   \
    "ToOperand IsDoubleRegister unimplemented")                                \
  V(kToOperandUnsupportedDoubleImmediate,                                      \
    "ToOperand Unsupported double immediate")                                  \
  V(kTryCatchStatement, "TryCatchStatement")                                   \
  V(kTryFinallyStatement, "TryFinallyStatement")                               \
  V(kUnalignedAllocationInNewSpace, "Unaligned allocation in new space")       \
  V(kUnalignedCellInWriteBarrier, "Unaligned cell in write barrier")           \
  V(kUnexpectedAllocationTop, "Unexpected allocation top")                     \
  V(kUnexpectedColorFound, "Unexpected color bit pattern found")               \
  V(kUnexpectedElementsKindInArrayConstructor,                                 \
    "Unexpected ElementsKind in array constructor")                            \
  V(kUnexpectedFallthroughFromCharCodeAtSlowCase,                              \
    "Unexpected fallthrough from CharCodeAt slow case")                        \
  V(kUnexpectedFallThroughFromStringComparison,                                \
    "Unexpected fall-through from string comparison")                          \
  V(kUnexpectedFallthroughToCharCodeAtSlowCase,                                \
    "Unexpected fallthrough to CharCodeAt slow case")                          \
  V(kUnexpectedFPUStackDepthAfterInstruction,                                  \
    "Unexpected FPU stack depth after instruction")                            \
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
  V(kUnexpectedSmi, "Unexpected smi value")                                    \
  V(kUnexpectedStackDepth, "Unexpected operand stack depth in full-codegen")   \
  V(kUnexpectedStackPointer, "The stack pointer is not the expected value")    \
  V(kUnexpectedStringType, "Unexpected string type")                           \
  V(kUnexpectedTestTypeofLiteralFlag,                                          \
    "Unexpected literal flag for TestTypeof bytecode")                         \
  V(kUnexpectedValue, "Unexpected value")                                      \
  V(kUnsupportedDoubleImmediate, "Unsupported double immediate")               \
  V(kUnsupportedLetCompoundAssignment, "Unsupported let compound assignment")  \
  V(kUnsupportedLookupSlotInDeclaration,                                       \
    "Unsupported lookup slot in declaration")                                  \
  V(kUnsupportedModuleOperation, "Unsupported module operation")               \
  V(kUnsupportedNonPrimitiveCompare, "Unsupported non-primitive compare")      \
  V(kUnsupportedPhiUseOfArguments, "Unsupported phi use of arguments")         \
  V(kUnsupportedPhiUseOfConstVariable,                                         \
    "Unsupported phi use of const or let variable")                            \
  V(kUnexpectedReturnFromFrameDropper,                                         \
    "Unexpectedly returned from dropping frames")                              \
  V(kUnexpectedReturnFromThrow, "Unexpectedly returned from a throw")          \
  V(kUnsupportedSwitchStatement, "Unsupported switch statement")               \
  V(kUnsupportedTaggedImmediate, "Unsupported tagged immediate")               \
  V(kUnstableConstantTypeHeapObject, "Unstable constant-type heap object")     \
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
