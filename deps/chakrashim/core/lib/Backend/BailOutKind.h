//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#if !defined(BAIL_OUT_KIND) || !defined(BAIL_OUT_KIND_LAST) || !defined(BAIL_OUT_KIND_VALUE) || !defined(BAIL_OUT_KIND_VALUE_LAST)
    #error BAIL_OUT_KIND, BAIL_OUT_KIND_LAST, BAIL_OUT_KIND_VALUE, and BAIL_OUT_KIND_VALUE_LAST must be defined before including this file.
#endif
               /* kind */                           /* allowed bits */
BAIL_OUT_KIND(BailOutInvalid,                       IR::BailOutOnResultConditions | IR::BailOutForArrayBits | IR::BailOutForDebuggerBits | IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutIntOnly,                       IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutNumberOnly,                    IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutPrimitiveButString,            IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutOnImplicitCalls,               IR::BailOutForArrayBits)
BAIL_OUT_KIND(BailOutOnImplicitCallsPreOp,          (IR::BailOutOnResultConditions | IR::BailOutForArrayBits | IR::BailOutMarkTempObject) & ~IR::BailOutOnArrayAccessHelperCall )
BAIL_OUT_KIND(BailOutOnLossyToInt32ImplicitCalls,   IR::BailOutMarkTempObject) // separate from BailOutOnImplicitCalls so that the bailout can disable LossyIntTypeSpec, but otherwise equivalent in functionality
BAIL_OUT_KIND(BailOutOnMemOpError,                  IR::BailOutForArrayBits)
BAIL_OUT_KIND(BailOutOnInlineFunction,              0)
BAIL_OUT_KIND(BailOutOnNoProfile,                   0)
BAIL_OUT_KIND(BailOutOnPolymorphicInlineFunction,   0)
BAIL_OUT_KIND(BailOutOnFailedPolymorphicInlineTypeCheck,   0)
BAIL_OUT_KIND(BailOutShared,                        0)
BAIL_OUT_KIND(BailOutExpectingObject,               0)
BAIL_OUT_KIND(BailOutOnNotArray,                    IR::BailOutOnMissingValue)
BAIL_OUT_KIND(BailOutOnNotNativeArray,              IR::BailOutOnMissingValue)
BAIL_OUT_KIND(BailOutConventionalTypedArrayAccessOnly, IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutOnIrregularLength,             IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutCheckThis,                     0)
BAIL_OUT_KIND(BailOutOnTaggedValue,                 0)
BAIL_OUT_KIND(BailOutFailedTypeCheck,               IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutFailedEquivalentTypeCheck,     IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutInjected,                      0)
BAIL_OUT_KIND(BailOutExpectingInteger,              0)
BAIL_OUT_KIND(BailOutExpectingString,               0)
BAIL_OUT_KIND(BailOutFailedInlineTypeCheck,         IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutFailedFixedFieldTypeCheck,     IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutFailedFixedFieldCheck,         0)
BAIL_OUT_KIND(BailOutFailedEquivalentFixedFieldTypeCheck,     IR::BailOutMarkTempObject)
BAIL_OUT_KIND(BailOutOnFloor,                       0)
BAIL_OUT_KIND(BailOnModByPowerOf2,                  0)
BAIL_OUT_KIND(BailOnIntMin,                         0)
BAIL_OUT_KIND(BailOnDivResultNotInt,                IR::BailOutOnDivByZero | IR::BailOutOnDivOfMinInt | IR::BailOutOnNegativeZero)
BAIL_OUT_KIND(BailOnSimpleJitToFullJitLoopBody,     0)
BAIL_OUT_KIND(BailOutFailedCtorGuardCheck,          0)
BAIL_OUT_KIND(BailOutOnFailedHoistedBoundCheck,     0)
BAIL_OUT_KIND(LazyBailOut,                          0)
BAIL_OUT_KIND(BailOutOnFailedHoistedLoopCountBasedBoundCheck, 0)
BAIL_OUT_KIND(BailOutForGeneratorYield,             0)
BAIL_OUT_KIND(BailOutOnException,                   0)

// SIMD_JS
BAIL_OUT_KIND(BailOutSimd128F4Only,                 0)
BAIL_OUT_KIND(BailOutSimd128I4Only,                 0)
BAIL_OUT_KIND(BailOutSimd128D2Only,                 0)
BAIL_OUT_KIND(BailOutNoSimdTypeSpec,                0)

BAIL_OUT_KIND(BailOutKindEnd,                       0)

// One bailout instruction can have multiple of the following reasons for bailout combined with any of the above. These tell
// what additional checks must be done to determine whether to bail out.
BAIL_OUT_KIND(BailOutKindBitsStart, 0) // fake bail out kind to indicate start index for kinds below

#define BAIL_OUT_KIND_BIT_START 10      // We can have 2^10 or 1024 bailout condition above
// ======================
// Result condition bits
// ======================
#define BAIL_OUT_KIND_RESULT_CONDITONS_BIT_START BAIL_OUT_KIND_BIT_START
BAIL_OUT_KIND_VALUE(BailOutOnOverflow, 1 << (BAIL_OUT_KIND_RESULT_CONDITONS_BIT_START + 0))
BAIL_OUT_KIND_VALUE(BailOutOnMulOverflow, 1 << (BAIL_OUT_KIND_RESULT_CONDITONS_BIT_START + 1))
BAIL_OUT_KIND_VALUE(BailOutOnNegativeZero, 1 << (BAIL_OUT_KIND_RESULT_CONDITONS_BIT_START + 2))
BAIL_OUT_KIND_VALUE(BailOutOnResultConditions, BailOutOnOverflow | BailOutOnMulOverflow | BailOutOnNegativeZero)
// ================
// Array bits
// ================
#define BAIL_OUT_KIND_ARRAY_BIT_START BAIL_OUT_KIND_RESULT_CONDITONS_BIT_START + 3
BAIL_OUT_KIND_VALUE(BailOutOnMissingValue, 1 << (BAIL_OUT_KIND_ARRAY_BIT_START + 0))
BAIL_OUT_KIND_VALUE(BailOutConventionalNativeArrayAccessOnly, 1 << (BAIL_OUT_KIND_ARRAY_BIT_START + 1))
BAIL_OUT_KIND_VALUE(BailOutConvertedNativeArray, 1 << (BAIL_OUT_KIND_ARRAY_BIT_START + 2))
BAIL_OUT_KIND_VALUE(BailOutOnArrayAccessHelperCall, 1 << (BAIL_OUT_KIND_ARRAY_BIT_START + 3))
BAIL_OUT_KIND_VALUE(BailOutOnInvalidatedArrayHeadSegment, 1 << (BAIL_OUT_KIND_ARRAY_BIT_START + 4))
BAIL_OUT_KIND_VALUE(BailOutOnInvalidatedArrayLength, 1 << (BAIL_OUT_KIND_ARRAY_BIT_START + 5))
BAIL_OUT_KIND_VALUE(
    BailOutForArrayBits,
    (
        BailOutOnMissingValue |
        BailOutConventionalNativeArrayAccessOnly |
        BailOutConvertedNativeArray |
        BailOutOnArrayAccessHelperCall |
        BailOutOnInvalidatedArrayHeadSegment |
        BailOutOnInvalidatedArrayLength
    ))
// ================
// Debug bits
// ================
#define BAIL_OUT_KIND_DEBUG_BIT_START BAIL_OUT_KIND_ARRAY_BIT_START + 6
// Forced bailout by ThreadContext::m_forceInterpreter, e.g. for async break when we enter a function.
BAIL_OUT_KIND_VALUE(BailOutForceByFlag, 1 << (BAIL_OUT_KIND_DEBUG_BIT_START + 0))

// When a function has a breakpoint, we need to bail out when we enter/return back to it.
BAIL_OUT_KIND_VALUE(BailOutBreakPointInFunction, 1 << (BAIL_OUT_KIND_DEBUG_BIT_START + 1))

// Used for stepping/on return from func. Bails out when current frame addr is greater than DebuggingFlags.m_stepEffectiveFrameBase.
BAIL_OUT_KIND_VALUE(BailOutStackFrameBase, 1 << (BAIL_OUT_KIND_DEBUG_BIT_START + 2))

// When we return to a frame in which a value of a local was changed.
BAIL_OUT_KIND_VALUE(BailOutLocalValueChanged, 1 << (BAIL_OUT_KIND_DEBUG_BIT_START + 3))

// Unconditional bailout, e.g. for the 'debugger' statement.
BAIL_OUT_KIND_VALUE(BailOutExplicit, 1 << (BAIL_OUT_KIND_DEBUG_BIT_START + 4))
BAIL_OUT_KIND_VALUE(BailOutStep, 1 << (BAIL_OUT_KIND_DEBUG_BIT_START + 5))
BAIL_OUT_KIND_VALUE(BailOutIgnoreException, 1 << (BAIL_OUT_KIND_DEBUG_BIT_START + 6))

BAIL_OUT_KIND_VALUE(BailOutForDebuggerBits, BailOutForceByFlag | BailOutBreakPointInFunction | BailOutStackFrameBase |
    BailOutLocalValueChanged | BailOutExplicit | BailOutStep | BailOutIgnoreException)

// ======================
// Div Src Condition Bits
// ======================
#define BAIL_OUT_KIND_DIV_SRC_CONDITIONS_BIT_START BAIL_OUT_KIND_DEBUG_BIT_START + 7
BAIL_OUT_KIND_VALUE(BailOutOnDivByZero, 1 << (BAIL_OUT_KIND_DIV_SRC_CONDITIONS_BIT_START + 0))
BAIL_OUT_KIND_VALUE(BailOutOnDivOfMinInt, 1 << (BAIL_OUT_KIND_DIV_SRC_CONDITIONS_BIT_START + 1))
BAIL_OUT_KIND_VALUE(BailOutOnDivSrcConditions, BailOutOnDivByZero | BailOutOnDivOfMinInt)

#define BAIL_OUT_KIND_MISC_BIT_START BAIL_OUT_KIND_DIV_SRC_CONDITIONS_BIT_START + 2
BAIL_OUT_KIND_VALUE(BailOutMarkTempObject, 1 << (BAIL_OUT_KIND_MISC_BIT_START + 0))


BAIL_OUT_KIND_VALUE_LAST(BailOutKindBits, BailOutMarkTempObject | BailOutOnDivSrcConditions | BailOutOnResultConditions | BailOutForArrayBits | BailOutForDebuggerBits)

// Help caller undefine the macros
#undef BAIL_OUT_KIND_LAST
#undef BAIL_OUT_KIND
#undef BAIL_OUT_KIND_VALUE_LAST
#undef BAIL_OUT_KIND_VALUE
