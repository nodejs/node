//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Default all macro to nothing
#ifndef MACRO
#define MACRO( opcode, layout, attr)
#endif

#ifndef MACRO_WMS
#define MACRO_WMS(opcode, layout, attr)
#endif

#ifndef MACRO_EXTEND
#define MACRO_EXTEND(opcode, layout, attr)
#endif

#ifndef MACRO_EXTEND_WMS
#define MACRO_EXTEND_WMS(opcode, layout, attr)
#endif

//          (   OpCodeAsmJs             , LayoutAsmJs  , OpCodeAttrAsmJs )
//          (        |                  ,     |        ,       |         )
//          (        |                  ,     |        ,       |         )
//          (        V                  ,     V        ,       V         )

MACRO       ( EndOfBlock                , Empty        , None            ) // End-of-buffer
// Prefix, order must be maintained to be the same as in OpCodes.h
MACRO       ( ExtendedOpcodePrefix      , Empty        , None            )
MACRO       ( MediumLayoutPrefix        , Empty        , None            )
MACRO       ( ExtendedMediumLayoutPrefix, Empty        , None            )
MACRO       ( LargeLayoutPrefix         , Empty        , None            )
MACRO       ( ExtendedLargeLayoutPrefix , Empty        , None            )

MACRO       ( Nop                       , Empty        , None            ) // No operation (Default value = 0)
MACRO_EXTEND( NopEx                     , Empty        , None            ) // No operation (Default value = 0)
MACRO       ( Label                     , Empty        , None            ) // No operation (Default value = 0)

// These need to exist for the interpreterLoop.inl to compile
MACRO       ( Yield                     , Empty        , None            )
MACRO       ( Leave                     , Empty        , None            )
MACRO       ( LeaveNull                 , Empty        , None            )
MACRO       ( FinallyEnd                , Empty        , None            )
MACRO       ( Break                     , Empty        , None            )
MACRO       ( Ret                       , Empty        , None            )

// External Function calls
MACRO       ( StartCall                 , StartCall    , None            ) // Initialize memory for a call
MACRO_WMS   ( Call                      , AsmCall      , None            ) // Execute call and place return value in register
MACRO_WMS   ( ArgOut_Db                 , Reg1Double1  , None            ) // convert double to var and place it for function call
MACRO_WMS   ( ArgOut_Int                , Reg1Int1     , None            ) // convert int to var and place it for function call
MACRO_WMS   ( Conv_VTD                  , Double1Reg1  , None            ) // convert var to double
MACRO_WMS   ( Conv_VTI                  , Int1Reg1     , None            ) // convert var to int
MACRO_WMS   ( Conv_VTF                  , Float1Reg1   , None            ) // convert var to float
// Internal calls
MACRO       ( I_StartCall               , StartCall    , None         )    // Initialize memory for a call
MACRO_WMS   ( I_Call                    , AsmCall      , None         )    // Execute call and place return value in register
MACRO_WMS   ( I_ArgOut_Db               , Reg1Double1  , None         )    // convert double to var and place it for function call
MACRO_WMS   ( I_ArgOut_Int              , Reg1Int1     , None         )    // convert int to var and place it for function call
MACRO_WMS   ( I_ArgOut_Flt              , Reg1Float1   , None         )    // convert float to var and place it for function call
MACRO_WMS   ( I_Conv_VTD                , Double2      , None         )    // convert var to double
MACRO_WMS   ( I_Conv_VTI                , Int2         , None         )    // convert var to int
MACRO_WMS   ( I_Conv_VTF                , Float2       , None         )    // convert var to float

// loop
MACRO_WMS   ( AsmJsLoopBodyStart        , AsmUnsigned1    , None      )    // Marks the start of a loop body
// Branching
MACRO       ( AsmBr                     , AsmBr        , OpNoFallThrough ) // Unconditional branch
MACRO_WMS   ( BrTrue_Int                , BrInt1       , None            ) // Jumps to offset if int value is not 0
MACRO_WMS   ( BrEq_Int                  , BrInt2       , None            ) // Jumps to offset if both int are equals

// Switching
MACRO_WMS   ( BeginSwitch_Int           , Int2         , None            ) // Start of an integer switch statement, same function as Ld_Int
MACRO       ( EndSwitch_Int             , AsmBr        , OpNoFallThrough ) // End of an integer switch statement, jumps to default case or past end of switch
MACRO_WMS   ( Case_Int                  , BrInt2       , None            ) // Integer branch, same function as BrInt2

// Type conversion
MACRO_WMS   ( Conv_DTI                  , Int1Double1  , None            ) // convert double to int
MACRO_WMS   ( Conv_FTI                  , Int1Float1   , None            ) // convert float to int
MACRO_WMS   ( Conv_ITD                  , Double1Int1  , None            ) // convert int to double
MACRO_WMS   ( Conv_FTD                  , Double1Float1, None            ) // convert float to double
MACRO_WMS   ( Conv_UTD                  , Double1Int1  , None            ) // convert unsigned int to double
MACRO_WMS   ( Return_Db                 , Double2      , None            ) // convert double to var
MACRO_WMS   ( Return_Flt                , Float2       , None            ) // convert float to var
MACRO_WMS   ( Return_Int                , Int2         , None            ) // convert int to var

// Module memory manipulation
MACRO_WMS   ( LdUndef                   , AsmReg1      , None            ) // Load 'undefined' usually in return register
MACRO_WMS   ( LdSlotArr                 , ElementSlot  , None            ) // Loads an array of Var from an array of Var
MACRO_WMS   ( LdSlot                    , ElementSlot  , None            ) // Loads a Var from an array of Var
MACRO_WMS   ( LdSlot_Db                 , ElementSlot  , None            ) // Loads a double from the Module
MACRO_WMS   ( LdSlot_Int                , ElementSlot  , None            ) // Loads an Int from the Module
MACRO_WMS   ( LdSlot_Flt                , ElementSlot  , None            ) // Loads an Float from the Module
MACRO_WMS   ( StSlot_Db                 , ElementSlot  , None            ) // Sets a double in the Module
MACRO_WMS   ( StSlot_Int                , ElementSlot  , None            ) // Sets an Int in the Module
MACRO_WMS   ( StSlot_Flt                , ElementSlot  , None            ) // Sets an Int in the Module
MACRO_WMS   ( LdArr_Func                , ElementSlot  , None            ) // opcode to load func from function tables

// Array Buffer manipulations
MACRO_WMS   ( LdArr                     , AsmTypedArr  , None            )
MACRO_WMS   ( LdArrConst                , AsmTypedArr  , None            )
MACRO_WMS   ( StArr                     , AsmTypedArr  , None            )
MACRO_WMS   ( StArrConst                , AsmTypedArr  , None            )

// Int math
MACRO_WMS   ( Ld_IntConst               , Int1Const1   , None            ) // Sets an int register from a const int
MACRO_WMS   ( Ld_Int                    , Int2         , None            ) // Sets an int from another int register
MACRO_WMS   ( Neg_Int                   , Int2         , None            ) // int unary '-'
MACRO_WMS   ( Not_Int                   , Int2         , None            ) // int unary '~'
MACRO_WMS   ( LogNot_Int                , Int2         , None            ) // int unary '!'
MACRO_WMS   ( Conv_ITB                  , Int2         , None            ) // int unary '!!' transform an int into a bool (0|1)
MACRO_WMS   ( Add_Int                   , Int3         , None            ) // int32 Arithmetic '+'
MACRO_WMS   ( Sub_Int                   , Int3         , None            ) // int32 Arithmetic '-' (subtract)
MACRO_WMS   ( Mul_Int                   , Int3         , None            ) // int32 Arithmetic '*'
MACRO_WMS   ( Div_Int                   , Int3         , None            ) // int32 Arithmetic '/'
MACRO_WMS   ( Rem_Int                   , Int3         , None            ) // int32 Arithmetic '%'
MACRO_WMS   ( And_Int                   , Int3         , None            ) // int32 Bitwise '&'
MACRO_WMS   ( Or_Int                    , Int3         , None            ) // int32 Bitwise '|'
MACRO_WMS   ( Xor_Int                   , Int3         , None            ) // int32 Bitwise '^'
MACRO_WMS   ( Shl_Int                   , Int3         , None            ) // int32 Shift '<<' (signed, truncate)
MACRO_WMS   ( Shr_Int                   , Int3         , None            ) // int32 Shift '>>' (signed, truncate)
MACRO_WMS   ( ShrU_Int                  , Int3         , None            ) // int32 Shift '>>>'(unsigned, truncate)

// Unsigned int math
MACRO_WMS   ( Mul_UInt                  , Int3         , None            ) // unsigned int32 Arithmetic '*'
MACRO_WMS   ( Div_UInt                  , Int3         , None            ) // unsigned int32 Arithmetic '/'
MACRO_WMS   ( Rem_UInt                  , Int3         , None            ) // unsigned int32 Arithmetic '%'

// Double math
MACRO_WMS   ( Ld_Db                     , Double2       , None           ) // Sets a double from another double register
MACRO_WMS   ( Neg_Db                    , Double2       , None           ) // Double Unary '-'
MACRO_WMS   ( Add_Db                    , Double3       , None           ) // Double Arithmetic '+'
MACRO_WMS   ( Sub_Db                    , Double3       , None           ) // Double Arithmetic '-' (subtract)
MACRO_WMS   ( Mul_Db                    , Double3       , None           ) // Double Arithmetic '*'
MACRO_WMS   ( Div_Db                    , Double3       , None           ) // Double Arithmetic '/'
MACRO_WMS   ( Rem_Db                    , Double3       , None           ) // Double Arithmetic '%'

// float math
MACRO_WMS   ( Ld_Flt                    , Float2        , None           ) // Sets a float from another float register
MACRO_WMS   ( Neg_Flt                   , Float2        , None           ) // Float  Unary '-'
MACRO_WMS   ( Add_Flt                   , Float3        , None           ) // Float Arithmetic '+'
MACRO_WMS   ( Sub_Flt                   , Float3        , None           ) // Float Arithmetic '-' (subtract)
MACRO_WMS   ( Mul_Flt                   , Float3        , None           ) // Float Arithmetic '*'
MACRO_WMS   ( Div_Flt                   , Float3        , None           ) // Float Arithmetic '/'
// Int comparisons
MACRO_WMS   ( CmLt_Int                  , Int3         , None            ) // int32 Comparison <
MACRO_WMS   ( CmLe_Int                  , Int3         , None            ) // int32 Comparison <=
MACRO_WMS   ( CmGt_Int                  , Int3         , None            ) // int32 Comparison >
MACRO_WMS   ( CmGe_Int                  , Int3         , None            ) // int32 Comparison >=
MACRO_WMS   ( CmEq_Int                  , Int3         , None            ) // int32 Comparison ==
MACRO_WMS   ( CmNe_Int                  , Int3         , None            ) // int32 Comparison !=

// Unsigned int comparisons
MACRO_WMS   ( CmLt_UnInt                , Int3         , None            ) // unsigned int32 Comparison <
MACRO_WMS   ( CmLe_UnInt                , Int3         , None            ) // unsigned int32 Comparison <=
MACRO_WMS   ( CmGt_UnInt                , Int3         , None            ) // unsigned int32 Comparison >
MACRO_WMS   ( CmGe_UnInt                , Int3         , None            ) // unsigned int32 Comparison >=

// Double comparisons
MACRO_WMS   ( CmLt_Db                   , Int1Double2   , None           ) // double Comparison <
MACRO_WMS   ( CmLe_Db                   , Int1Double2   , None           ) // double Comparison <=
MACRO_WMS   ( CmGt_Db                   , Int1Double2   , None           ) // double Comparison >
MACRO_WMS   ( CmGe_Db                   , Int1Double2   , None           ) // double Comparison >=
MACRO_WMS   ( CmEq_Db                   , Int1Double2   , None           ) // double Comparison ==
MACRO_WMS   ( CmNe_Db                   , Int1Double2   , None           ) // double Comparison !=

// Float comparisons
MACRO_WMS   ( CmLt_Flt                  , Int1Float2    , None           ) // float Comparison <
MACRO_WMS   ( CmLe_Flt                  , Int1Float2    , None           ) // float Comparison <=
MACRO_WMS   ( CmGt_Flt                  , Int1Float2    , None           ) // float Comparison >
MACRO_WMS   ( CmGe_Flt                  , Int1Float2    , None           ) // float Comparison >=
MACRO_WMS   ( CmEq_Flt                  , Int1Float2    , None           ) // float Comparison ==
MACRO_WMS   ( CmNe_Flt                  , Int1Float2    , None           ) // float Comparison !=

// Math builtin functions for ints
MACRO_WMS   ( Abs_Int                   , Int2         , None            )
MACRO_WMS   ( Min_Int                   , Int3         , None            )
MACRO_WMS   ( Max_Int                   , Int3         , None            )
MACRO_WMS   ( Imul_Int                  , Int3         , None            )
MACRO_WMS   ( Clz32_Int                 , Int2         , None            )

// Math builtin functions for doubles & floats
MACRO_WMS   ( Sin_Db                    , Double2       , None           )
MACRO_WMS   ( Cos_Db                    , Double2       , None           )
MACRO_WMS   ( Tan_Db                    , Double2       , None           )
MACRO_WMS   ( Asin_Db                   , Double2       , None           )
MACRO_WMS   ( Acos_Db                   , Double2       , None           )
MACRO_WMS   ( Atan_Db                   , Double2       , None           )
MACRO_WMS   ( Abs_Db                    , Double2       , None           )
MACRO_WMS   ( Ceil_Db                   , Double2       , None           )
MACRO_WMS   ( Ceil_Flt                  , Float2        , None           )
MACRO_WMS   ( Floor_Db                  , Double2       , None           )
MACRO_WMS   ( Floor_Flt                 , Float2        , None           )
MACRO_WMS   ( Exp_Db                    , Double2       , None           )
MACRO_WMS   ( Log_Db                    , Double2       , None           )
MACRO_WMS   ( Pow_Db                    , Double3       , None           )
MACRO_WMS   ( Sqrt_Db                   , Double2       , None           )
MACRO_WMS   ( Sqrt_Flt                  , Float2        , None           )
MACRO_WMS   ( Abs_Flt                   , Float2        , None           )
MACRO_WMS   ( Atan2_Db                  , Double3       , None           )
MACRO_WMS   ( Min_Db                    , Double3       , None           )
MACRO_WMS   ( Max_Db                    , Double3       , None           )

// Fround
MACRO_WMS   ( Fround_Flt                , Float2        , None           )
MACRO_WMS   ( Fround_Db                 , Float1Double1 , None           )
MACRO_WMS   ( Fround_Int                , Float1Int1    , None           )

#define MACRO_SIMD(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO(opcode, asmjsLayout, opCodeAttrAsmJs)
#define MACRO_SIMD_WMS(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_WMS(opcode, asmjsLayout, opCodeAttrAsmJs)
#define MACRO_SIMD_ASMJS_ONLY_WMS(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_WMS(opcode, asmjsLayout, opCodeAttrAsmJs)

#define MACRO_SIMD_EXTEND(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_EXTEND(opcode, asmjsLayout, opCodeAttrAsmJs)
#define MACRO_SIMD_EXTEND_WMS(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_EXTEND_WMS(opcode, asmjsLayout, opCodeAttrAsmJs)
#define MACRO_SIMD_ASMJS_ONLY_EXTEND_WMS(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_EXTEND_WMS(opcode, asmjsLayout, opCodeAttrAsmJs)

#include "OpCodesSimd.h"

// help the caller to undefine all the macros
#undef MACRO
#undef MACRO_WMS
#undef MACRO_EXTEND
#undef MACRO_EXTEND_WMS
#undef MACRO_BACKEND_ONLY
