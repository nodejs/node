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
  V(kAlignmentMarkerExpected, "Alignment marker expected")                     \
  V(kAllocationIsNotDoubleAligned, "Allocation is not double aligned")         \
  V(kAPICallReturnedInvalidObject, "API call returned invalid object")         \
  V(kArgumentsObjectValueInATestContext,                                       \
    "Arguments object value in a test context")                                \
  V(kArrayBoilerplateCreationFailed, "Array boilerplate creation failed")      \
  V(kArrayIndexConstantValueTooBig, "Array index constant value too big")      \
  V(kAssignmentToArguments, "Assignment to arguments")                         \
  V(kAssignmentToLetVariableBeforeInitialization,                              \
    "Assignment to let variable before initialization")                        \
  V(kAssignmentToLOOKUPVariable, "Assignment to LOOKUP variable")              \
  V(kAssignmentToParameterFunctionUsesArgumentsObject,                         \
    "Assignment to parameter, function uses arguments object")                 \
  V(kAssignmentToParameterInArgumentsObject,                                   \
    "Assignment to parameter in arguments object")                             \
  V(kAttemptToUseUndefinedCache, "Attempt to use undefined cache")             \
  V(kBadValueContextForArgumentsObjectValue,                                   \
    "Bad value context for arguments object value")                            \
  V(kBadValueContextForArgumentsValue,                                         \
    "Bad value context for arguments value")                                   \
  V(kBailedOutDueToDependencyChange, "Bailed out due to dependency change")    \
  V(kBailoutWasNotPrepared, "Bailout was not prepared")                        \
  V(kBinaryStubGenerateFloatingPointCode,                                      \
    "BinaryStub_GenerateFloatingPointCode")                                    \
  V(kBothRegistersWereSmisInSelectNonSmi,                                      \
    "Both registers were smis in SelectNonSmi")                                \
  V(kBuiltinFunctionCannotBeOptimized, "Builtin function cannot be optimized") \
  V(kCallToAJavaScriptRuntimeFunction,                                         \
    "Call to a JavaScript runtime function")                                   \
  V(kCannotTranslatePositionInChangedArea,                                     \
    "Cannot translate position in changed area")                               \
  V(kClassLiteral, "Class literal")                                            \
  V(kCodeGenerationFailed, "Code generation failed")                           \
  V(kCodeObjectNotProperlyPatched, "Code object not properly patched")         \
  V(kCompoundAssignmentToLookupSlot, "Compound assignment to lookup slot")     \
  V(kComputedPropertyName, "Computed property name")                           \
  V(kContextAllocatedArguments, "Context-allocated arguments")                 \
  V(kCopyBuffersOverlap, "Copy buffers overlap")                               \
  V(kCouldNotGenerateZero, "Could not generate +0.0")                          \
  V(kCouldNotGenerateNegativeZero, "Could not generate -0.0")                  \
  V(kDebuggerHasBreakPoints, "Debugger has break points")                      \
  V(kDebuggerStatement, "DebuggerStatement")                                   \
  V(kDeclarationInCatchContext, "Declaration in catch context")                \
  V(kDeclarationInWithContext, "Declaration in with context")                  \
  V(kDefaultNaNModeNotSet, "Default NaN mode not set")                         \
  V(kDeleteWithGlobalVariable, "Delete with global variable")                  \
  V(kDeleteWithNonGlobalVariable, "Delete with non-global variable")           \
  V(kDestinationOfCopyNotAligned, "Destination of copy not aligned")           \
  V(kDontDeleteCellsCannotContainTheHole,                                      \
    "DontDelete cells can't contain the hole")                                 \
  V(kDoPushArgumentNotImplementedForDoubleType,                                \
    "DoPushArgument not implemented for double type")                          \
  V(kEliminatedBoundsCheckFailed, "Eliminated bounds check failed")            \
  V(kEmitLoadRegisterUnsupportedDoubleImmediate,                               \
    "EmitLoadRegister: Unsupported double immediate")                          \
  V(kEval, "eval")                                                             \
  V(kExpected0AsASmiSentinel, "Expected 0 as a Smi sentinel")                  \
  V(kExpectedAlignmentMarker, "Expected alignment marker")                     \
  V(kExpectedAllocationSite, "Expected allocation site")                       \
  V(kExpectedFunctionObject, "Expected function object in register")           \
  V(kExpectedHeapNumber, "Expected HeapNumber")                                \
  V(kExpectedNativeContext, "Expected native context")                         \
  V(kExpectedNonIdenticalObjects, "Expected non-identical objects")            \
  V(kExpectedNonNullContext, "Expected non-null context")                      \
  V(kExpectedPositiveZero, "Expected +0.0")                                    \
  V(kExpectedAllocationSiteInCell, "Expected AllocationSite in property cell") \
  V(kExpectedFixedArrayInFeedbackVector,                                       \
    "Expected fixed array in feedback vector")                                 \
  V(kExpectedFixedArrayInRegisterA2, "Expected fixed array in register a2")    \
  V(kExpectedFixedArrayInRegisterEbx, "Expected fixed array in register ebx")  \
  V(kExpectedFixedArrayInRegisterR2, "Expected fixed array in register r2")    \
  V(kExpectedFixedArrayInRegisterRbx, "Expected fixed array in register rbx")  \
  V(kExpectedNewSpaceObject, "Expected new space object")                      \
  V(kExpectedSmiOrHeapNumber, "Expected smi or HeapNumber")                    \
  V(kExpectedUndefinedOrCell, "Expected undefined or cell in register")        \
  V(kExpectingAlignmentForCopyBytes, "Expecting alignment for CopyBytes")      \
  V(kExportDeclaration, "Export declaration")                                  \
  V(kExternalStringExpectedButNotFound,                                        \
    "External string expected, but not found")                                 \
  V(kFailedBailedOutLastTime, "Failed/bailed out last time")                   \
  V(kForInStatementOptimizationIsDisabled,                                     \
    "ForInStatement optimization is disabled")                                 \
  V(kForInStatementWithNonLocalEachVariable,                                   \
    "ForInStatement with non-local each variable")                             \
  V(kForOfStatement, "ForOfStatement")                                         \
  V(kFrameIsExpectedToBeAligned, "Frame is expected to be aligned")            \
  V(kFunctionCallsEval, "Function calls eval")                                 \
  V(kFunctionIsAGenerator, "Function is a generator")                          \
  V(kFunctionWithIllegalRedeclaration, "Function with illegal redeclaration")  \
  V(kGeneratedCodeIsTooLarge, "Generated code is too large")                   \
  V(kGeneratorFailedToResume, "Generator failed to resume")                    \
  V(kGenerator, "Generator")                                                   \
  V(kGlobalFunctionsMustHaveInitialMap,                                        \
    "Global functions must have initial map")                                  \
  V(kHeapNumberMapRegisterClobbered, "HeapNumberMap register clobbered")       \
  V(kHydrogenFilter, "Optimization disabled by filter")                        \
  V(kImportDeclaration, "Import declaration")                                  \
  V(kImproperObjectOnPrototypeChainForStore,                                   \
    "Improper object on prototype chain for store")                            \
  V(kIndexIsNegative, "Index is negative")                                     \
  V(kIndexIsTooLarge, "Index is too large")                                    \
  V(kInlinedRuntimeFunctionFastOneByteArrayJoin,                               \
    "Inlined runtime function: FastOneByteArrayJoin")                          \
  V(kInlinedRuntimeFunctionGetFromCache,                                       \
    "Inlined runtime function: GetFromCache")                                  \
  V(kInliningBailedOut, "Inlining bailed out")                                 \
  V(kInputGPRIsExpectedToHaveUpper32Cleared,                                   \
    "Input GPR is expected to have upper32 cleared")                           \
  V(kInputStringTooLong, "Input string too long")                              \
  V(kInstanceofStubUnexpectedCallSiteCacheCheck,                               \
    "InstanceofStub unexpected call site cache (check)")                       \
  V(kInstanceofStubUnexpectedCallSiteCacheCmp1,                                \
    "InstanceofStub unexpected call site cache (cmp 1)")                       \
  V(kInstanceofStubUnexpectedCallSiteCacheCmp2,                                \
    "InstanceofStub unexpected call site cache (cmp 2)")                       \
  V(kInstanceofStubUnexpectedCallSiteCacheMov,                                 \
    "InstanceofStub unexpected call site cache (mov)")                         \
  V(kInteger32ToSmiFieldWritingToNonSmiLocation,                               \
    "Integer32ToSmiField writing to non-smi location")                         \
  V(kInvalidCaptureReferenced, "Invalid capture referenced")                   \
  V(kInvalidElementsKindForInternalArrayOrInternalPackedArray,                 \
    "Invalid ElementsKind for InternalArray or InternalPackedArray")           \
  V(kInvalidFullCodegenState, "invalid full-codegen state")                    \
  V(kInvalidHandleScopeLevel, "Invalid HandleScope level")                     \
  V(kInvalidLeftHandSideInAssignment, "Invalid left-hand side in assignment")  \
  V(kInvalidLhsInCompoundAssignment, "Invalid lhs in compound assignment")     \
  V(kInvalidLhsInCountOperation, "Invalid lhs in count operation")             \
  V(kInvalidMinLength, "Invalid min_length")                                   \
  V(kJSGlobalObjectNativeContextShouldBeANativeContext,                        \
    "JSGlobalObject::native_context should be a native context")               \
  V(kJSGlobalProxyContextShouldNotBeNull,                                      \
    "JSGlobalProxy::context() should not be null")                             \
  V(kJSObjectWithFastElementsMapHasSlowElements,                               \
    "JSObject with fast elements map has slow elements")                       \
  V(kLetBindingReInitialization, "Let binding re-initialization")              \
  V(kLhsHasBeenClobbered, "lhs has been clobbered")                            \
  V(kLiveBytesCountOverflowChunkSize, "Live Bytes Count overflow chunk size")  \
  V(kLiveEdit, "LiveEdit")                                                     \
  V(kLookupVariableInCountOperation, "Lookup variable in count operation")     \
  V(kMapBecameDeprecated, "Map became deprecated")                             \
  V(kMapBecameUnstable, "Map became unstable")                                 \
  V(kMapIsNoLongerInEax, "Map is no longer in eax")                            \
  V(kNativeFunctionLiteral, "Native function literal")                         \
  V(kNeedSmiLiteral, "Need a Smi literal here")                                \
  V(kNoCasesLeft, "No cases left")                                             \
  V(kNoEmptyArraysHereInEmitFastOneByteArrayJoin,                              \
    "No empty arrays here in EmitFastOneByteArrayJoin")                        \
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
  V(kObjectFoundInSmiOnlyArray, "Object found in smi-only array")              \
  V(kObjectLiteralWithComplexProperty, "Object literal with complex property") \
  V(kOddballInStringTableIsNotUndefinedOrTheHole,                              \
    "Oddball in string table is not undefined or the hole")                    \
  V(kOffsetOutOfRange, "Offset out of range")                                  \
  V(kOperandIsASmiAndNotAName, "Operand is a smi and not a name")              \
  V(kOperandIsASmiAndNotAString, "Operand is a smi and not a string")          \
  V(kOperandIsASmi, "Operand is a smi")                                        \
  V(kOperandIsNotAName, "Operand is not a name")                               \
  V(kOperandIsNotANumber, "Operand is not a number")                           \
  V(kOperandIsNotASmi, "Operand is not a smi")                                 \
  V(kOperandIsNotAString, "Operand is not a string")                           \
  V(kOperandIsNotSmi, "Operand is not smi")                                    \
  V(kOperandNotANumber, "Operand not a number")                                \
  V(kObjectTagged, "The object is tagged")                                     \
  V(kObjectNotTagged, "The object is not tagged")                              \
  V(kOptimizationDisabled, "Optimization is disabled")                         \
  V(kOptimizedTooManyTimes, "Optimized too many times")                        \
  V(kOutOfVirtualRegistersWhileTryingToAllocateTempRegister,                   \
    "Out of virtual registers while trying to allocate temp register")         \
  V(kParseScopeError, "Parse/scope error")                                     \
  V(kPossibleDirectCallToEval, "Possible direct call to eval")                 \
  V(kPreconditionsWereNotMet, "Preconditions were not met")                    \
  V(kPropertyAllocationCountFailed, "Property allocation count failed")        \
  V(kReceivedInvalidReturnAddress, "Received invalid return address")          \
  V(kReferenceToAVariableWhichRequiresDynamicLookup,                           \
    "Reference to a variable which requires dynamic lookup")                   \
  V(kReferenceToGlobalLexicalVariable, "Reference to global lexical variable") \
  V(kReferenceToUninitializedVariable, "Reference to uninitialized variable")  \
  V(kRegisterDidNotMatchExpectedRoot, "Register did not match expected root")  \
  V(kRegisterWasClobbered, "Register was clobbered")                           \
  V(kRememberedSetPointerInNewSpace, "Remembered set pointer is in new space") \
  V(kRestParameter, "Rest parameters")                                         \
  V(kReturnAddressNotFoundInFrame, "Return address not found in frame")        \
  V(kRhsHasBeenClobbered, "Rhs has been clobbered")                            \
  V(kScopedBlock, "ScopedBlock")                                               \
  V(kScriptContext, "Allocation of script context")                            \
  V(kSmiAdditionOverflow, "Smi addition overflow")                             \
  V(kSmiSubtractionOverflow, "Smi subtraction overflow")                       \
  V(kStackAccessBelowStackPointer, "Stack access below stack pointer")         \
  V(kStackFrameTypesMustMatch, "Stack frame types must match")                 \
  V(kSuperReference, "Super reference")                                        \
  V(kTheCurrentStackPointerIsBelowCsp,                                         \
    "The current stack pointer is below csp")                                  \
  V(kTheInstructionShouldBeALis, "The instruction should be a lis")            \
  V(kTheInstructionShouldBeALui, "The instruction should be a lui")            \
  V(kTheInstructionShouldBeAnOri, "The instruction should be an ori")          \
  V(kTheInstructionShouldBeAnOris, "The instruction should be an oris")        \
  V(kTheInstructionShouldBeALi, "The instruction should be a li")              \
  V(kTheInstructionShouldBeASldi, "The instruction should be a sldi")          \
  V(kTheInstructionToPatchShouldBeALoadFromConstantPool,                       \
    "The instruction to patch should be a load from the constant pool")        \
  V(kTheInstructionToPatchShouldBeAnLdrLiteral,                                \
    "The instruction to patch should be a ldr literal")                        \
  V(kTheInstructionToPatchShouldBeALis,                                        \
    "The instruction to patch should be a lis")                                \
  V(kTheInstructionToPatchShouldBeALui,                                        \
    "The instruction to patch should be a lui")                                \
  V(kTheInstructionToPatchShouldBeAnOri,                                       \
    "The instruction to patch should be an ori")                               \
  V(kTheSourceAndDestinationAreTheSame,                                        \
    "The source and destination are the same")                                 \
  V(kTheStackPointerIsNotAligned, "The stack pointer is not aligned.")         \
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
  V(kUnableToEncodeValueAsSmi, "Unable to encode value as smi")                \
  V(kUnalignedAllocationInNewSpace, "Unaligned allocation in new space")       \
  V(kUnalignedCellInWriteBarrier, "Unaligned cell in write barrier")           \
  V(kUndefinedValueNotLoaded, "Undefined value not loaded")                    \
  V(kUndoAllocationOfNonAllocatedMemory,                                       \
    "Undo allocation of non allocated memory")                                 \
  V(kUnexpectedAllocationTop, "Unexpected allocation top")                     \
  V(kUnexpectedColorFound, "Unexpected color bit pattern found")               \
  V(kUnexpectedElementsKindInArrayConstructor,                                 \
    "Unexpected ElementsKind in array constructor")                            \
  V(kUnexpectedFallthroughFromCharCodeAtSlowCase,                              \
    "Unexpected fallthrough from CharCodeAt slow case")                        \
  V(kUnexpectedFallthroughFromCharFromCodeSlowCase,                            \
    "Unexpected fallthrough from CharFromCode slow case")                      \
  V(kUnexpectedFallThroughFromStringComparison,                                \
    "Unexpected fall-through from string comparison")                          \
  V(kUnexpectedFallThroughInBinaryStubGenerateFloatingPointCode,               \
    "Unexpected fall-through in BinaryStub_GenerateFloatingPointCode")         \
  V(kUnexpectedFallthroughToCharCodeAtSlowCase,                                \
    "Unexpected fallthrough to CharCodeAt slow case")                          \
  V(kUnexpectedFallthroughToCharFromCodeSlowCase,                              \
    "Unexpected fallthrough to CharFromCode slow case")                        \
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
  V(kUnexpectedNumberOfPreAllocatedPropertyFields,                             \
    "Unexpected number of pre-allocated property fields")                      \
  V(kUnexpectedFPCRMode, "Unexpected FPCR mode.")                              \
  V(kUnexpectedSmi, "Unexpected smi value")                                    \
  V(kUnexpectedStringFunction, "Unexpected String function")                   \
  V(kUnexpectedStringType, "Unexpected string type")                           \
  V(kUnexpectedStringWrapperInstanceSize,                                      \
    "Unexpected string wrapper instance size")                                 \
  V(kUnexpectedTypeForRegExpDataFixedArrayExpected,                            \
    "Unexpected type for RegExp data, FixedArray expected")                    \
  V(kUnexpectedValue, "Unexpected value")                                      \
  V(kUnexpectedUnusedPropertiesOfStringWrapper,                                \
    "Unexpected unused properties of string wrapper")                          \
  V(kUnimplemented, "unimplemented")                                           \
  V(kUnsupportedConstCompoundAssignment,                                       \
    "Unsupported const compound assignment")                                   \
  V(kUnsupportedCountOperationWithConst,                                       \
    "Unsupported count operation with const")                                  \
  V(kUnsupportedDoubleImmediate, "Unsupported double immediate")               \
  V(kUnsupportedLetCompoundAssignment, "Unsupported let compound assignment")  \
  V(kUnsupportedLookupSlotInDeclaration,                                       \
    "Unsupported lookup slot in declaration")                                  \
  V(kUnsupportedNonPrimitiveCompare, "Unsupported non-primitive compare")      \
  V(kUnsupportedPhiUseOfArguments, "Unsupported phi use of arguments")         \
  V(kUnsupportedPhiUseOfConstVariable,                                         \
    "Unsupported phi use of const variable")                                   \
  V(kUnsupportedTaggedImmediate, "Unsupported tagged immediate")               \
  V(kVariableResolvedToWithContext, "Variable resolved to with context")       \
  V(kWeShouldNotHaveAnEmptyLexicalContext,                                     \
    "We should not have an empty lexical context")                             \
  V(kWithStatement, "WithStatement")                                           \
  V(kWrongFunctionContext, "Wrong context passed to function")                 \
  V(kWrongAddressOrValuePassedToRecordWrite,                                   \
    "Wrong address or value passed to RecordWrite")                            \
  V(kShouldNotDirectlyEnterOsrFunction,                                        \
    "Should not directly enter OSR-compiled function")                         \
  V(kYield, "Yield")


#define ERROR_MESSAGES_CONSTANTS(C, T) C,
enum BailoutReason {
  ERROR_MESSAGES_LIST(ERROR_MESSAGES_CONSTANTS) kLastErrorMessage
};
#undef ERROR_MESSAGES_CONSTANTS


const char* GetBailoutReason(BailoutReason reason);

}  // namespace internal
}  // namespace v8

#endif  // V8_BAILOUT_REASON_H_
