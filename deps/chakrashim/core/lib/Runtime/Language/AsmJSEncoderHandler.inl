//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Default all macro to nothing
#ifndef DEF2
#define DEF2(process, op, func)
#endif
#ifndef DEF3
#define DEF3(process, op, func, y)
#endif
#ifndef DEF2_WMS
#define DEF2_WMS(process, op, func)
#endif
#ifndef DEF3_WMS
#define DEF3_WMS(process, op, func, y)
#endif
#ifndef DEF4_WMS
#define DEF4_WMS(process, op, func, y, t)
#endif
#ifndef EXDEF2
#define EXDEF2(process, op, func)
#endif
#ifndef EXDEF3
#define EXDEF3(process, op, func, y)
#endif
#ifndef EXDEF2_WMS
#define EXDEF2_WMS(process, op, func)
#endif
#ifndef EXDEF3_WMS
#define EXDEF3_WMS(process, op, func, y)
#endif
#ifndef EXDEF4_WMS
#define EXDEF4_WMS(process, op, func, y, t)
#endif

#ifdef NTBUILD
// NT header is missing __cdecl on these API
#define _CRT_HAS_CDECL false
#else
#define _CRT_HAS_CDECL true
#endif

  DEF3    ( CUSTOM     , Nop               , OP_Empty                , Empty         )
EXDEF3    ( CUSTOM     , NopEx             , OP_Empty                , Empty         )
  DEF3    ( CUSTOM     , Label             , OP_Label                , Empty         )
  DEF3    ( CUSTOM     , Ret               , OP_Empty                , Empty         )

// External Calls
  DEF3    ( CUSTOM     , StartCall         , OP_StartCall            , StartCall     )
  DEF3_WMS( CUSTOM     , Call              , OP_Call                 , AsmCall       )
  DEF3_WMS( CUSTOM     , ArgOut_Db         , OP_ArgOut_Db            , Reg1Double1   )
  DEF3_WMS( CUSTOM     , ArgOut_Int        , OP_ArgOut_Int           , Reg1Int1      )
  DEF3_WMS( CUSTOM     , Conv_VTD          , OP_Conv_VTD             , Double1Reg1   )
  DEF3_WMS( CUSTOM     , Conv_VTF          , OP_Conv_VTF             , Float1Reg1    )
  DEF3_WMS( CUSTOM     , Conv_VTI          , OP_Conv_VTI             , Int1Reg1      )
// Internal Calls
  DEF3    ( CUSTOM     , I_StartCall       , OP_I_StartCall          , StartCall     )
  DEF3_WMS( CUSTOM     , I_Call            , OP_I_Call               , AsmCall       )
  DEF3_WMS( CUSTOM     , I_ArgOut_Db       , OP_I_ArgOut_Db          , Reg1Double1   )
  DEF3_WMS( CUSTOM     , I_ArgOut_Flt      , OP_I_ArgOut_Flt         , Reg1Float1    )
  DEF3_WMS( CUSTOM     , I_ArgOut_Int      , OP_I_ArgOut_Int         , Reg1Int1      )
  DEF3_WMS( CUSTOM     , I_Conv_VTD        , OP_I_Conv_VTD           , Double2       )
  DEF3_WMS( CUSTOM     , I_Conv_VTI        , OP_I_Conv_VTI           , Int2          )
  DEF3_WMS( CUSTOM     , I_Conv_VTF        , OP_I_Conv_VTF           , Float2        )

  DEF3    ( CUSTOM     , AsmBr             , OP_Br                   , AsmBr         )
  DEF3_WMS( CUSTOM     , BrTrue_Int        , OP_BrTrue               , BrInt1        )
  DEF3_WMS( CUSTOM     , BrEq_Int          , OP_BrEq                 , BrInt2        )

// Switching
  DEF3_WMS( INT2       , BeginSwitch_Int   , Ld_Int                  , Int2          )
  DEF3    ( CUSTOM     , EndSwitch_Int     , OP_Br                   , AsmBr         )
  DEF3_WMS( CUSTOM     , Case_Int          , OP_BrEq                 , BrInt2        )

  DEF3_WMS( CUSTOM     , Conv_DTI          , Op_Db_To_Int            , Int1Double1   )
  DEF3_WMS( CUSTOM     , Conv_ITD          , Op_Int_To_Db            , Double1Int1   )
  DEF3_WMS( CUSTOM     , Conv_UTD          , Op_UInt_To_Db           , Double1Int1   )
  DEF3_WMS( CUSTOM     , Conv_FTD          , Op_Float_To_Db          , Double1Float1 )
  DEF3_WMS( CUSTOM     , Conv_FTI          , Op_Float_To_Int         , Int1Float1    )

  DEF3_WMS( CUSTOM     , Return_Db         , OP_SetReturnDouble      , Double2       )
  DEF3_WMS( CUSTOM     , Return_Flt        , OP_SetReturnFloat       , Float2        )
  DEF3_WMS( CUSTOM     , Return_Int        , OP_SetReturnInt         , Int2          )


  DEF3_WMS( CUSTOM     , LdUndef           , OP_LdUndef              , AsmReg1       )
  DEF3_WMS( CUSTOM     , LdSlotArr         , OP_LdSlot               , ElementSlot   )
  DEF3_WMS( CUSTOM     , LdSlot            , OP_LdSlot               , ElementSlot   )
  DEF3_WMS( CUSTOM     , LdSlot_Db         , Op_LdSlot_Db            , ElementSlot   )
  DEF3_WMS( CUSTOM     , LdSlot_Int        , Op_LdSlot_Int           , ElementSlot   )
  DEF3_WMS( CUSTOM     , LdSlot_Flt        , Op_LdSlot_Flt           , ElementSlot   )
  DEF3_WMS( CUSTOM     , StSlot_Db         , Op_StSlot_Db            , ElementSlot   )
  DEF3_WMS( CUSTOM     , StSlot_Int        , Op_StSlot_Int           , ElementSlot   )
  DEF3_WMS( CUSTOM     , StSlot_Flt        , Op_StSlot_Flt           , ElementSlot   )
  DEF3_WMS( ELEMENTSLOT, LdArr_Func        , LdArr_Func              , ElementSlot   )


  DEF3_WMS( CUSTOM     , LdArr             , Op_LdArr                ,  AsmTypedArr  )
  DEF3_WMS( CUSTOM     , LdArrConst        , Op_LdArrConst           ,  AsmTypedArr  )
  DEF3_WMS( CUSTOM     , StArr             , Op_StArr                ,  AsmTypedArr  )
  DEF3_WMS( CUSTOM     , StArrConst        , Op_StArrConst           ,  AsmTypedArr  )

  DEF3_WMS( CUSTOM     , Ld_IntConst       , Op_LdConst_Int          , Int1Const1    )
  DEF3_WMS( INT2       , Ld_Int            , Ld_Int                  , Int2          )
  DEF3_WMS( INT2       , Neg_Int           , Neg_Int                 , Int2          )
  DEF3_WMS( INT2       , Not_Int           , Not_Int                 , Int2          )
  DEF3_WMS( INT2       , LogNot_Int        , LogNot_Int              , Int2          )
  DEF3_WMS( INT2       , Conv_ITB          , Int_To_Bool             , Int2          )
  DEF3_WMS( INT3       , Add_Int           , Add_Int                 , Int3          )
  DEF3_WMS( INT3       , Sub_Int           , Sub_Int                 , Int3          )
  DEF3_WMS( INT3       , Mul_Int           , Mul_Int                 , Int3          )
  DEF3_WMS( INT3       , Div_Int           , Div_Int                 , Int3          )
  DEF3_WMS( INT3       , Rem_Int           , Rem_Int                 , Int3          )
  DEF3_WMS( INT3       , And_Int           , And_Int                 , Int3          )
  DEF3_WMS( INT3       , Or_Int            , Or_Int                  , Int3          )
  DEF3_WMS( INT3       , Xor_Int           , Xor_Int                 , Int3          )
  DEF3_WMS( INT3       , Shl_Int           , Shl_Int                 , Int3          )
  DEF3_WMS( INT3       , Shr_Int           , Shr_Int                 , Int3          )
  DEF3_WMS( INT3       , ShrU_Int          , ShrU_Int                , Int3          )

  DEF3_WMS( INT3       , Mul_UInt          , Mul_UInt                , Int3          )
  DEF3_WMS( INT3       , Div_UInt          , Div_UInt                , Int3          )
  DEF3_WMS( INT3       , Rem_UInt          , Rem_UInt                , Int3          )

  DEF3_WMS( DOUBLE2    , Ld_Db             , Ld_Db                   , Double2       )
  DEF3_WMS( FLOAT2     , Ld_Flt            , Ld_Flt                  , Float2        )
  DEF3_WMS( DOUBLE2    , Neg_Db            , Neg_Db                  , Double2       )
  DEF3_WMS( DOUBLE3    , Add_Db            , Add_Db                  , Double3       )
  DEF3_WMS( DOUBLE3    , Sub_Db            , Sub_Db                  , Double3       )
  DEF3_WMS( DOUBLE3    , Mul_Db            , Mul_Db                  , Double3       )
  DEF3_WMS( DOUBLE3    , Div_Db            , Div_Db                  , Double3       )
  DEF3_WMS( DOUBLE3    , Rem_Db            , Rem_Db                  , Double3       )

  //float math
  DEF3_WMS( FLOAT2     , Neg_Flt           , Neg_Flt                 , Float2        )
  DEF3_WMS( FLOAT3     , Add_Flt           , Add_Flt                 , Float3        )
  DEF3_WMS( FLOAT3     , Sub_Flt           , Sub_Flt                 , Float3        )
  DEF3_WMS( FLOAT3     , Mul_Flt           , Mul_Flt                 , Float3        )
  DEF3_WMS (FLOAT3     , Div_Flt           , Div_Flt                 , Float3        )

  DEF3_WMS( INT3       , CmLt_Int          , Lt_Int                  , Int3          )
  DEF3_WMS( INT3       , CmLe_Int          , Le_Int                  , Int3          )
  DEF3_WMS( INT3       , CmGt_Int          , Gt_Int                  , Int3          )
  DEF3_WMS( INT3       , CmGe_Int          , Ge_Int                  , Int3          )
  DEF3_WMS( INT3       , CmEq_Int          , Eq_Int                  , Int3          )
  DEF3_WMS( INT3       , CmNe_Int          , Ne_Int                  , Int3          )

  DEF3_WMS( INT3       , CmLt_UnInt        , Lt_UInt                 , Int3          )
  DEF3_WMS( INT3       , CmLe_UnInt        , Le_UInt                 , Int3          )
  DEF3_WMS( INT3       , CmGt_UnInt        , Gt_UInt                 , Int3          )
  DEF3_WMS( INT3       , CmGe_UnInt        , Ge_UInt                 , Int3          )

  DEF3_WMS( INT1DOUBLE2, CmLt_Db           , CmpLt_Db                , Int1Double2   )
  DEF3_WMS( INT1DOUBLE2, CmLe_Db           , CmpLe_Db                , Int1Double2   )
  DEF3_WMS( INT1DOUBLE2, CmGt_Db           , CmpGt_Db                , Int1Double2   )
  DEF3_WMS( INT1DOUBLE2, CmGe_Db           , CmpGe_Db                , Int1Double2   )
  DEF3_WMS( INT1DOUBLE2, CmEq_Db           , CmpEq_Db                , Int1Double2   )
  DEF3_WMS( INT1DOUBLE2, CmNe_Db           , CmpNe_Db                , Int1Double2   )

  DEF3_WMS( INT1FLOAT2 , CmLt_Flt          , CmpLt_Flt               , Int1Float2    )
  DEF3_WMS( INT1FLOAT2 , CmLe_Flt          , CmpLe_Flt               , Int1Float2    )
  DEF3_WMS( INT1FLOAT2 , CmGt_Flt          , CmpGt_Flt               , Int1Float2    )
  DEF3_WMS( INT1FLOAT2 , CmGe_Flt          , CmpGe_Flt               , Int1Float2    )
  DEF3_WMS( INT1FLOAT2 , CmEq_Flt          , CmpEq_Flt               , Int1Float2    )
  DEF3_WMS( INT1FLOAT2 , CmNe_Flt          , CmpNe_Flt               , Int1Float2    )

  DEF3_WMS( INT2       , Abs_Int           , Abs_Int                 , Int2          )
  DEF3_WMS( INT3       , Min_Int           , Min_Int                 , Int3          )
  DEF3_WMS( INT3       , Max_Int           , Max_Int                 , Int3          )
  DEF3_WMS( INT3       , Imul_Int          , Mul_Int                 , Int3          )
  DEF3_WMS( INT2       , Clz32_Int         , Clz32_Int               , Int2          )

  DEF3_WMS( CALLDOUBLE2, Sin_Db            , Math::Sin               , false         )
  DEF3_WMS( CALLDOUBLE2, Cos_Db            , Math::Cos               , false         )
  DEF3_WMS( CALLDOUBLE2, Tan_Db            , Math::Tan               , false         )
  DEF3_WMS( CALLDOUBLE2, Asin_Db           , Math::Asin              , false         )
  DEF3_WMS( CALLDOUBLE2, Acos_Db           , Math::Acos              , false         )
  DEF3_WMS( CALLDOUBLE2, Atan_Db           , Math::Atan              , false         )
  DEF3_WMS( CALLDOUBLE2, Ceil_Db           , ::ceil                  , true          )
  DEF3_WMS( CALLFLOAT2 , Ceil_Flt          , ::ceilf                 , _CRT_HAS_CDECL)
  DEF3_WMS( CALLDOUBLE2, Floor_Db          , ::floor                 , true          )
  DEF3_WMS( CALLFLOAT2 , Floor_Flt         , ::floorf                , _CRT_HAS_CDECL)
  DEF3_WMS( CALLDOUBLE2, Exp_Db            , Math::Exp               , false         )
  DEF3_WMS( CALLDOUBLE2, Log_Db            , Math::Log               , false         )
  DEF3_WMS( CALLDOUBLE3, Pow_Db            , Math::Pow               , false         )
  DEF3_WMS( CALLDOUBLE2, Sqrt_Db           , ::sqrt                  , true          )
  DEF3_WMS( CALLFLOAT2,  Sqrt_Flt          , ::sqrtf                 , _CRT_HAS_CDECL)
  DEF3_WMS( CALLDOUBLE2, Abs_Db            , Math::Abs               , false         )
  DEF3_WMS( CALLFLOAT2 , Abs_Flt           , ::fabsf                 , _CRT_HAS_CDECL)
  DEF3_WMS( CALLDOUBLE3, Atan2_Db          , Math::Atan2             , false         )
  DEF3_WMS( CALLDOUBLE3, Min_Db            , AsmJsMath::Min<double>  , false         )
  DEF3_WMS( CALLDOUBLE3, Max_Db            , AsmJsMath::Max<double>  , false         )

  DEF3_WMS( CUSTOM     , Fround_Flt        , OP_SetFroundFlt         , Float2        )
  DEF3_WMS( CUSTOM     , Fround_Db         , OP_SetFroundDb          , Float1Double1 )
  DEF3_WMS( CUSTOM     , Fround_Int        , OP_SetFroundInt         , Float1Int1    )
  DEF3_WMS( CUSTOM     , AsmJsLoopBodyStart,OP_AsmJsLoopBody                 , AsmUnsigned1  )

  DEF3_WMS( CUSTOM                              , Simd128_Ld_F4                 , OP_Simd128_LdF4                   , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_Ld_I4                 , OP_Simd128_LdI4                   , Int32x4_2              )
  DEF3_WMS( CUSTOM                              , Simd128_Ld_D2                 , OP_Simd128_LdD2                   , Float64x2_2            )

  DEF3_WMS( CUSTOM                              , Simd128_LdSlot_F4             , OP_Simd128_LdSlotF4               , ElementSlot            )
  DEF3_WMS( CUSTOM                              , Simd128_LdSlot_I4             , OP_Simd128_LdSlotI4               , ElementSlot            )
  DEF3_WMS( CUSTOM                              , Simd128_LdSlot_D2             , OP_Simd128_LdSlotD2               , ElementSlot            )

  DEF3_WMS( CUSTOM                              , Simd128_StSlot_F4             , OP_Simd128_StSlotF4               , ElementSlot            )
  DEF3_WMS( CUSTOM                              , Simd128_StSlot_I4             , OP_Simd128_StSlotI4               , ElementSlot            )
  DEF3_WMS( CUSTOM                              , Simd128_StSlot_D2             , OP_Simd128_StSlotD2               , ElementSlot            )

  DEF3_WMS( CUSTOM                              , Simd128_FloatsToF4            , OP_Simd128_FloatsToF4             , Float32x4_1Float4      )
  DEF3_WMS( CUSTOM                              , Simd128_IntsToI4              , OP_Simd128_IntsToI4               , Int32x4_1Int4          )
  DEF3_WMS( CUSTOM                              , Simd128_DoublesToD2           , OP_Simd128_DoublesToD2            , Float64x2_1Double2     )
  DEF3_WMS( CUSTOM                              , Simd128_Return_F4             , OP_Simd128_ReturnF4               , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_Return_I4             , OP_Simd128_ReturnI4               , Int32x4_2              )
  DEF3_WMS( CUSTOM                              , Simd128_Return_D2             , OP_Simd128_ReturnD2               , Float64x2_2            )

  DEF3_WMS( CUSTOM                              , Simd128_Splat_F4              , OP_Simd128_SplatF4                , Float32x4_1Float1      )
  DEF3_WMS( CUSTOM                              , Simd128_Splat_I4              , OP_Simd128_SplatI4                , Int32x4_1Int1          )
  DEF3_WMS( CUSTOM                              , Simd128_Splat_D2              , OP_Simd128_SplatD2                , Float64x2_1Double1     )

  DEF3_WMS( CUSTOM                              , Simd128_FromFloat64x2_F4      , OP_Simd128_FromFloat64x2F4        , Float32x4_1Float64x2_1 )
  DEF3_WMS( CUSTOM                              , Simd128_FromInt32x4_F4        , OP_Simd128_FromInt32x4F4          , Float32x4_1Int32x4_1   )
  DEF3_WMS( CUSTOM                              , Simd128_FromFloat64x2Bits_F4  , OP_Simd128_FromFloat64x2BitsF4    , Float32x4_1Float64x2_1 )
  DEF3_WMS( CUSTOM                              , Simd128_FromInt32x4Bits_F4    , OP_Simd128_FromInt32x4BitsF4      , Float32x4_1Int32x4_1   )

  DEF3_WMS( CUSTOM                              , Simd128_FromFloat32x4_D2      , OP_Simd128_FromFloat32x4D2        , Float64x2_1Float32x4_1 )
  DEF3_WMS( CUSTOM                              , Simd128_FromInt32x4_D2        , OP_Simd128_FromInt32x4D2          , Float64x2_1Int32x4_1   )
  DEF3_WMS( CUSTOM                              , Simd128_FromFloat32x4Bits_D2  , OP_Simd128_FromFloat32x4BitsD2    , Float64x2_1Float32x4_1 )
  DEF3_WMS( CUSTOM                              , Simd128_FromInt32x4Bits_D2    , OP_Simd128_FromInt32x4BitsD2      , Float64x2_1Int32x4_1   )

  DEF3_WMS( CUSTOM                              , Simd128_FromFloat32x4_I4      , OP_Simd128_FromFloat32x4I4        , Int32x4_1Float32x4_1   )
  DEF3_WMS( CUSTOM                              , Simd128_FromFloat64x2_I4      , OP_Simd128_FromFloat64x2I4        , Int32x4_1Float64x2_1   )
  DEF3_WMS( CUSTOM                              , Simd128_FromFloat32x4Bits_I4  , OP_Simd128_FromFloat32x4BitsI4    , Int32x4_1Float32x4_1   )
  DEF3_WMS( CUSTOM                              , Simd128_FromFloat64x2Bits_I4  , OP_Simd128_FromFloat64x2BitsI4    , Int32x4_1Float64x2_1   )

  DEF3_WMS( CUSTOM                              , Simd128_Abs_F4                , OP_Simd128_AbsF4                  , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_Abs_D2                , OP_Simd128_AbsD2                  , Float64x2_2            )

  DEF3_WMS( CUSTOM                              , Simd128_Neg_F4                , OP_Simd128_NegF4                  , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_Neg_I4                , OP_Simd128_NegI4                  , Int32x4_2              )
  DEF3_WMS( CUSTOM                              , Simd128_Neg_D2                , OP_Simd128_NegD2                  , Float64x2_2            )

  DEF3_WMS( CUSTOM                              , Simd128_Rcp_F4                , OP_Simd128_RcpF4                  , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_Rcp_D2                , OP_Simd128_RcpD2                  , Float64x2_2            )

  DEF3_WMS( CUSTOM                              , Simd128_RcpSqrt_F4            , OP_Simd128_RcpSqrtF4              , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_RcpSqrt_D2            , OP_Simd128_RcpSqrtD2              , Float64x2_2            )

  DEF3_WMS( CUSTOM                              , Simd128_Sqrt_F4               , OP_Simd128_SqrtF4                 , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_Sqrt_D2               , OP_Simd128_SqrtD2                 , Float64x2_2            )

  DEF3_WMS( CUSTOM                              , Simd128_Not_F4                , OP_Simd128_NotF4                  , Float32x4_2            )
  DEF3_WMS( CUSTOM                              , Simd128_Not_I4                , OP_Simd128_NotI4                  , Int32x4_2              )

  DEF3_WMS( CUSTOM                              , Simd128_Add_F4                , OP_Simd128_AddF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Add_I4                , OP_Simd128_AddI4                  , Int32x4_3              )
  DEF3_WMS( CUSTOM                              , Simd128_Add_D2                , OP_Simd128_AddD2                  , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Sub_F4                , OP_Simd128_SubF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Sub_I4                , OP_Simd128_SubI4                  , Int32x4_3              )
  DEF3_WMS( CUSTOM                              , Simd128_Sub_D2                , OP_Simd128_SubD2                  , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Mul_F4                , OP_Simd128_MulF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Mul_I4                , OP_Simd128_MulI4                  , Int32x4_3              )
  DEF3_WMS( CUSTOM                              , Simd128_Mul_D2                , OP_Simd128_MulD2                  , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Div_F4                , OP_Simd128_DivF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Div_D2                , OP_Simd128_DivD2                  , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Min_F4                , OP_Simd128_MinF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Min_D2                , OP_Simd128_MinD2                  , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Max_F4                , OP_Simd128_MaxF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Max_D2                , OP_Simd128_MaxD2                  , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Lt_F4                 , OP_Simd128_LtF4                   , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Lt_I4                 , OP_Simd128_LtI4                   , Int32x4_3              )
  DEF3_WMS( CUSTOM                              , Simd128_Lt_D2                 , OP_Simd128_LtD2                   , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Gt_F4                 , OP_Simd128_GtF4                   , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Gt_I4                 , OP_Simd128_GtI4                   , Int32x4_3              )
  DEF3_WMS( CUSTOM                              , Simd128_Gt_D2                 , OP_Simd128_GtD2                   , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_LtEq_F4               , OP_Simd128_LtEqF4                 , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_LtEq_D2               , OP_Simd128_LtEqD2                 , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_GtEq_F4               , OP_Simd128_GtEqF4                 , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_GtEq_D2               , OP_Simd128_GtEqD2                 , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Eq_F4                 , OP_Simd128_EqF4                   , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Eq_I4                 , OP_Simd128_EqI4                   , Int32x4_3              )
  DEF3_WMS( CUSTOM                              , Simd128_Eq_D2                 , OP_Simd128_EqD2                   , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_Neq_F4                , OP_Simd128_NeqF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Neq_D2                , OP_Simd128_NeqD2                  , Float64x2_3            )

  DEF3_WMS( CUSTOM                              , Simd128_And_F4                , OP_Simd128_AndF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_And_I4                , OP_Simd128_AndI4                  , Int32x4_3              )

  DEF3_WMS( CUSTOM                              , Simd128_Or_F4                 , OP_Simd128_OrF4                   , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Or_I4                 , OP_Simd128_OrI4                   , Int32x4_3              )

  DEF3_WMS( CUSTOM                              , Simd128_Xor_F4                , OP_Simd128_XorF4                  , Float32x4_3            )
  DEF3_WMS( CUSTOM                              , Simd128_Xor_I4                , OP_Simd128_XorI4                  , Int32x4_3              )

  // ToDO: Spec change: Change to BitSelect
  DEF3_WMS( CUSTOM                              , Simd128_Select_F4             , OP_Simd128_SelectF4               , Float32x4_1Int32x4_1Float32x4_2)
  DEF3_WMS( CUSTOM                              , Simd128_Select_I4             , OP_Simd128_SelectI4               , Int32x4_4              )
  DEF3_WMS( CUSTOM                              , Simd128_Select_D2             , OP_Simd128_SelectD2               , Float64x2_1Int32x4_1Float64x2_2)

  DEF3_WMS( CUSTOM                              , Simd128_LdSignMask_F4         , OP_Simd128_LdSignMaskF4           , Int1Float32x4_1      )
  DEF3_WMS( CUSTOM                              , Simd128_LdSignMask_I4         , OP_Simd128_LdSignMaskI4           , Int1Int32x4_1        )
  DEF3_WMS( CUSTOM                              , Simd128_LdSignMask_D2         , OP_Simd128_LdSignMaskD2           , Int1Float64x2_1      )

  DEF3_WMS( CUSTOM                              , Simd128_I_ArgOut_F4           , OP_Simd128_I_ArgOutF4             , Reg1Float32x4_1      )
  DEF3_WMS( CUSTOM                              , Simd128_I_ArgOut_I4           , OP_Simd128_I_ArgOutI4             , Reg1Int32x4_1        )
  DEF3_WMS( CUSTOM                              , Simd128_I_ArgOut_D2           , OP_Simd128_I_ArgOutD2             , Reg1Float64x2_1      )

  //Lane acess
  DEF3_WMS( CUSTOM                              , Simd128_ExtractLane_I4        , OP_Simd128_ExtractLaneI4          , Int1Int32x4_1Int1)
  DEF3_WMS( CUSTOM                              , Simd128_ExtractLane_F4        , OP_Simd128_ExtractLaneF4          , Float1Float32x4_1Int1)

  DEF3_WMS( CUSTOM                              , Simd128_ReplaceLane_I4        , OP_Simd128_ReplaceLaneI4          , Int32x4_2Int2)
  DEF3_WMS( CUSTOM                              , Simd128_ReplaceLane_F4        , OP_Simd128_ReplaceLaneF4          , Float32x4_2Int1Float1)

  DEF3_WMS( CUSTOM                              , Simd128_I_Conv_VTF4           , OP_Simd128_I_Conv_VTF4            , Float32x4_2      )
  DEF3_WMS( CUSTOM                              , Simd128_I_Conv_VTI4           , OP_Simd128_I_Conv_VTI4            , Int32x4_2        )
  DEF3_WMS( CUSTOM                              , Simd128_I_Conv_VTD2           , OP_Simd128_I_Conv_VTD2            , Float64x2_2      )

// help the caller to undefine all the macros
#undef DEF2
#undef DEF3
#undef DEF2_WMS
#undef DEF3_WMS
#undef DEF4_WMS
#undef EXDEF2
#undef EXDEF3
#undef EXDEF2_WMS
#undef EXDEF3_WMS
#undef EXDEF4_WMS
