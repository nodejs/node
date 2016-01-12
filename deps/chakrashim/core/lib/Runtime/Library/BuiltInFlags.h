//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // TODO: consider putting this into #if ENABLE_NATIVE_CODEGEN, but make sure all code using types below compiles when ENABLE_NATIVE_CODEGEN is not defined.

    const int BIAS_ArgSize = 4; // 4 bits.

    // The type to specialize built-in arg to.
    // The reason to introduce a new enum is that the types can be mixed, e.g. a built-in dst can be type spec'd to either int or float, based on src.
    // These values must be powers of 2, as some built-ins support 'mixed' specializations -- float/int.
    // Currently only used by inline.cpp.
    enum BuiltInArgSpecizationType
    {
        BIAST_None = 0,
        BIAST_Float = 1,
        BIAST_Int = 2,
        BIAST_All = (1 << BIAS_ArgSize) - 1,
    };

    // Specifies shift value (number of bits) for BuiltInArgSpecizationType to get dst/src1/src2/etc type spec flag.
    enum BuiltInArgShift
    {
        BIAS_Dst = 0,
        BIAS_Src1 = BIAS_Dst + BIAS_ArgSize,
        BIAS_Src2 = BIAS_Src1 + BIAS_ArgSize,
        BIAS_Other = BIAS_Src2 + BIAS_ArgSize,
    };

    // Some extra options about built-ins, used in LibraryFunction.h.
    enum BuiltInFlags
    {
        BIF_None =                0x0,

        BIF_TypeSpecDstToFloat  = BIAST_Float << BIAS_Dst,      // Dst area
        BIF_TypeSpecDstToInt    = BIAST_Int << BIAS_Dst,

        BIF_TypeSpecSrc1ToFloat = BIAST_Float << BIAS_Src1,    // Src1 area
        BIF_TypeSpecSrc1ToInt   = BIAST_Int << BIAS_Src1,

        BIF_TypeSpecSrc2ToFloat = BIAST_Float << BIAS_Src2,    // Src2 area
        BIF_TypeSpecSrc2ToInt   = BIAST_Int << BIAS_Src2,

        BIF_UseSrc0 =             0x1 << BIAS_Other, // Whether to use src0 (target) in the actual instr, such as InlineMathSin x. Normally for Math.* function src0 is not used.
        BIF_VariableArgsNumber =  0x2 << BIAS_Other, // Whether function can take variable number of arguments, such as Array.unshift()
        BIF_IgnoreDst =           0x4 << BIAS_Other, // Whether actual instr should contain dst or not.
        BIF_TypeSpecAllToFloat              = BIF_TypeSpecDstToFloat | BIF_TypeSpecSrc1ToFloat | BIF_TypeSpecSrc2ToFloat,
        BIF_TypeSpecAllToInt                = BIF_TypeSpecDstToInt   | BIF_TypeSpecSrc1ToInt   | BIF_TypeSpecSrc2ToInt,
        BIF_TypeSpecUnaryToFloat            = BIF_TypeSpecDstToFloat | BIF_TypeSpecSrc1ToFloat,
        BIF_TypeSpecSrcAndDstToFloatOrInt   = BIF_TypeSpecDstToInt   | BIF_TypeSpecDstToFloat  | BIF_TypeSpecSrc1ToInt | BIF_TypeSpecSrc1ToFloat,
        BIF_TypeSpecSrc1ToFloatOrInt        = BIF_TypeSpecSrc1ToInt  | BIF_TypeSpecSrc1ToFloat,
        BIF_TypeSpecDstToFloatOrInt         = BIF_TypeSpecDstToFloat | BIF_TypeSpecDstToInt,
        BIF_Args                            = (BIAST_All << BIAS_Dst) | (BIAST_All << BIAS_Src1) | (BIAST_All << BIAS_Src2),
    };
} // namespace Js
