//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
/*
 * These are javascript library functions that might be inlined
 * by the JIT.
 *
 * Notes:
 * - the argc is the number of args to pass to InlineXXX call, e.g. 2 for Math.pow and 2 for String.CharAt.
 * - TODO: consider having dst/src1/src2 in separate columns rather than bitmask, this seems to be better for design but we won't be able to see 'all float' by single check.
 * - TODO: enable string inlines when string type spec is available
 *
 *               target         name                argc  flags                                               EntryInfo
 */
LIBRARY_FUNCTION(Math,          Abs,                1,    BIF_TypeSpecSrcAndDstToFloatOrInt                 , Math::EntryInfo::Abs)
LIBRARY_FUNCTION(Math,          Acos,               1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Acos)
LIBRARY_FUNCTION(Math,          Asin,               1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Asin)
LIBRARY_FUNCTION(Math,          Atan,               1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Atan)
LIBRARY_FUNCTION(Math,          Atan2,              2,    BIF_TypeSpecAllToFloat                            , Math::EntryInfo::Atan2)
LIBRARY_FUNCTION(Math,          Ceil,               1,    BIF_TypeSpecDstToInt | BIF_TypeSpecSrc1ToFloat    , Math::EntryInfo::Ceil)
LIBRARY_FUNCTION(String,        CodePointAt,        2,    BIF_TypeSpecSrc2ToInt | BIF_UseSrc0               , JavascriptString::EntryInfo::CodePointAt)
LIBRARY_FUNCTION(String,        CharAt,             2,    BIF_UseSrc0                                       , JavascriptString::EntryInfo::CharAt  )
LIBRARY_FUNCTION(String,        CharCodeAt,         2,    BIF_UseSrc0                                       , JavascriptString::EntryInfo::CharCodeAt )
LIBRARY_FUNCTION(String,        Concat,             15,   BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptString::EntryInfo::Concat )
LIBRARY_FUNCTION(String,        FromCharCode,       1,    BIF_None                                          , JavascriptString::EntryInfo::FromCharCode)
LIBRARY_FUNCTION(String,        FromCodePoint,      1,    BIF_None                                          , JavascriptString::EntryInfo::FromCodePoint)
LIBRARY_FUNCTION(String,        IndexOf,            3,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptString::EntryInfo::IndexOf)
LIBRARY_FUNCTION(String,        LastIndexOf,        3,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptString::EntryInfo::LastIndexOf)
LIBRARY_FUNCTION(String,        Link,               2,    BIF_UseSrc0                                       , JavascriptString::EntryInfo::Link)
LIBRARY_FUNCTION(String,        LocaleCompare,      2,    BIF_UseSrc0                                       , JavascriptString::EntryInfo::LocaleCompare)
LIBRARY_FUNCTION(String,        Match,              2,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::Match)
LIBRARY_FUNCTION(String,        Replace,            3,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::Replace)
LIBRARY_FUNCTION(String,        Search,             2,    BIF_UseSrc0                                       , JavascriptString::EntryInfo::Search)
LIBRARY_FUNCTION(String,        Slice,              3,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptString::EntryInfo::Slice )
LIBRARY_FUNCTION(String,        Split,              3,    BIF_UseSrc0 | BIF_VariableArgsNumber | BIF_IgnoreDst , JavascriptString::EntryInfo::Split)
LIBRARY_FUNCTION(String,        Substr,             3,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptString::EntryInfo::Substr)
LIBRARY_FUNCTION(String,        Substring,          3,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptString::EntryInfo::Substring)
LIBRARY_FUNCTION(String,        ToLocaleLowerCase,  1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::ToLocaleLowerCase)
LIBRARY_FUNCTION(String,        ToLocaleUpperCase,  1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::ToLocaleUpperCase)
LIBRARY_FUNCTION(String,        ToLowerCase,        1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::ToLowerCase)
LIBRARY_FUNCTION(String,        ToUpperCase,        1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::ToUpperCase)
LIBRARY_FUNCTION(String,        Trim,               1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::Trim)
LIBRARY_FUNCTION(String,        TrimLeft,           1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::TrimLeft)
LIBRARY_FUNCTION(String,        TrimRight,          1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptString::EntryInfo::TrimRight)
LIBRARY_FUNCTION(Math,          Cos,                1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Cos)
LIBRARY_FUNCTION(Math,          Exp,                1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Exp)
LIBRARY_FUNCTION(Math,          Floor,              1,    BIF_TypeSpecDstToInt | BIF_TypeSpecSrc1ToFloat    , Math::EntryInfo::Floor)
LIBRARY_FUNCTION(Math,          Log,                1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Log)
LIBRARY_FUNCTION(Math,          Max,                2,    BIF_TypeSpecSrcAndDstToFloatOrInt                 , Math::EntryInfo::Max)
LIBRARY_FUNCTION(Math,          Min,                2,    BIF_TypeSpecSrcAndDstToFloatOrInt                 , Math::EntryInfo::Min)
LIBRARY_FUNCTION(Math,          Pow,                2,    BIF_TypeSpecAllToFloat                            , Math::EntryInfo::Pow)
LIBRARY_FUNCTION(Math,          Imul,               2,    BIF_TypeSpecAllToInt                              , Math::EntryInfo::Imul)
LIBRARY_FUNCTION(Math,          Clz32,              1,    BIF_TypeSpecAllToInt                              , Math::EntryInfo::Clz32)
LIBRARY_FUNCTION(Array,         Push,               2,    BIF_UseSrc0 | BIF_IgnoreDst | BIF_TypeSpecSrc1ToFloatOrInt, JavascriptArray::EntryInfo::Push)
LIBRARY_FUNCTION(Array,         Pop,                1,    BIF_UseSrc0 | BIF_TypeSpecDstToFloatOrInt         , JavascriptArray::EntryInfo::Pop)
LIBRARY_FUNCTION(Math,          Random,             0,    BIF_TypeSpecDstToFloat                            , Math::EntryInfo::Random)
LIBRARY_FUNCTION(Math,          Round,              1,    BIF_TypeSpecDstToInt | BIF_TypeSpecSrc1ToFloat    , Math::EntryInfo::Round)
LIBRARY_FUNCTION(Math,          Sin,                1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Sin)
LIBRARY_FUNCTION(Math,          Sqrt,               1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Sqrt)
LIBRARY_FUNCTION(Math,          Tan,                1,    BIF_TypeSpecUnaryToFloat                          , Math::EntryInfo::Tan)
LIBRARY_FUNCTION(Array,         Concat,             15,   BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptArray::EntryInfo::Concat)
LIBRARY_FUNCTION(Array,         IndexOf,            2,    BIF_UseSrc0                                       , JavascriptArray::EntryInfo::IndexOf)
LIBRARY_FUNCTION(Array,         Includes,           2,    BIF_UseSrc0                                       , JavascriptArray::EntryInfo::Includes)
LIBRARY_FUNCTION(Array,         IsArray,            1,    BIF_VariableArgsNumber                            , JavascriptArray::EntryInfo::IsArray)
LIBRARY_FUNCTION(Array,         Join,               2,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptArray::EntryInfo::Join)
LIBRARY_FUNCTION(Array,         LastIndexOf,        3,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptArray::EntryInfo::LastIndexOf)
LIBRARY_FUNCTION(Array,         Reverse,            1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptArray::EntryInfo::Reverse)
LIBRARY_FUNCTION(Array,         Shift,              1,    BIF_UseSrc0 | BIF_IgnoreDst                       , JavascriptArray::EntryInfo::Shift)
LIBRARY_FUNCTION(Array,         Slice,              3,    BIF_UseSrc0 | BIF_VariableArgsNumber              , JavascriptArray::EntryInfo::Slice)
LIBRARY_FUNCTION(Array,         Splice,             15,   BIF_UseSrc0 | BIF_VariableArgsNumber | BIF_IgnoreDst  , JavascriptArray::EntryInfo::Splice)
LIBRARY_FUNCTION(Array,         Unshift,            15,   BIF_UseSrc0 | BIF_VariableArgsNumber | BIF_IgnoreDst  , JavascriptArray::EntryInfo::Unshift)
LIBRARY_FUNCTION(Function,      Apply,              3,    BIF_UseSrc0 | BIF_IgnoreDst                           , JavascriptFunction::EntryInfo::Apply)
LIBRARY_FUNCTION(Function,      Call,               15,   BIF_UseSrc0 | BIF_IgnoreDst | BIF_VariableArgsNumber  , JavascriptFunction::EntryInfo::Call)
LIBRARY_FUNCTION(GlobalObject,  ParseInt,           1,    BIF_IgnoreDst                                         , GlobalObject::EntryInfo::ParseInt)
LIBRARY_FUNCTION(RegExp,        Exec,               2,    BIF_UseSrc0 | BIF_IgnoreDst                           , JavascriptRegExp::EntryInfo::Exec)
LIBRARY_FUNCTION(Math,          Fround,             1,    BIF_TypeSpecUnaryToFloat                              , Math::EntryInfo::Fround)

// Note: 1st column is currently used only for debug tracing.

// SIMD_JS
#if ENABLE_NATIVE_CODEGEN
LIBRARY_FUNCTION(SIMD_Float32x4,    Float32x4,         4, BIF_IgnoreDst                                                 , SIMDFloat32x4Lib::EntryInfo::Float32x4)
LIBRARY_FUNCTION(SIMD_Float32x4,    Add,               2, BIF_IgnoreDst                                                 , SIMDFloat32x4Lib::EntryInfo::Add)

LIBRARY_FUNCTION(SIMD_Int32x4,      Int32x4,           4, BIF_IgnoreDst                                                 , SIMDInt32x4Lib::EntryInfo::Int32x4)
LIBRARY_FUNCTION(SIMD_Int32x4,      Add,               2, BIF_IgnoreDst                                                 , SIMDInt32x4Lib::EntryInfo::Add)
#endif