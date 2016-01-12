//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifndef TEMP_DISABLE_ASMJS
// See  Lib\Runtime\Language\InterpreterProcessOpCodeAsmJs.h  for Handler Process
//         (   HandlerProcess , OpCodeAsmJs ,        HandlerFunction       , LayoutAsmJs , Type  )
//         (        |         ,     |       ,              |               ,      |      ,   |   )
//         (        |         ,     |       ,              |               ,      |      ,   |   )
//         (        V         ,     V       ,              V               ,      V      ,   V   )

  DEF2    (NOPASMJS          , Nop          , Empty                                              )
  DEF2    (NOPASMJS          , Label        , Empty                                              )
EXDEF2    (NOPASMJS          , NopEx        , Empty                                              )
  DEF2    (BR_ASM            , AsmBr        , OP_Br                                              )

  DEF2_WMS(FALLTHROUGH_ASM   , LdSlotArr    , /* Common case with LdSlot */                      )
  DEF3_WMS(GET_ELEM_SLOT_ASM , LdSlot       , OP_LdAsmJsSlot               , ElementSlot         )
  DEF2_WMS(FUNCtoA1Mem       , LdUndef      , JavascriptOperators::OP_LdUndef                    )

// Function Calls
  DEF2(FALLTHROUGH_ASM, I_StartCall, /* Common case with StartCall */                            )
  DEF3    ( CUSTOM_ASMJS     , StartCall    , OP_AsmStartCall              , StartCall           )

  DEF3_WMS(CUSTOM_ASMJS      , I_Call       , OP_I_AsmCall                 , AsmCall             )
  DEF3_WMS( CUSTOM_ASMJS     , Call         , OP_AsmCall                   , AsmCall             )
  DEF2_WMS(D1toR1Out         , I_ArgOut_Db  , OP_I_SetOutAsmDb                                   )
  DEF2_WMS( D1toR1Out        , ArgOut_Db    , OP_SetOutAsmDb                                     ) // convert double to var and set it as outparam
  DEF2_WMS(I1toR1Out         , I_ArgOut_Int , OP_I_SetOutAsmInt                                  )
  DEF2_WMS( I1toR1Out        , ArgOut_Int   , OP_SetOutAsmInt                                    ) // convert int to var and set it as outparam
  DEF2_WMS( F1toR1Out        , I_ArgOut_Flt , OP_I_SetOutAsmFlt                                  ) // convert float to var and set it as outparam
  DEF2_WMS(D1toD1Mem         , I_Conv_VTD   , (double)                                           )
  DEF2_WMS( R1toD1Mem        , Conv_VTD     , JavascriptConversion::ToNumber                     ) // convert var to double
  DEF2_WMS(F1toF1Mem         , I_Conv_VTF   , (float)                                            )
  DEF2_WMS(R1toF1Mem         , Conv_VTF     , JavascriptConversion::ToNumber                     ) // convert var to float
  DEF2_WMS(I1toI1Mem         , I_Conv_VTI   , (int)                                              )
  DEF2_WMS( R1toI1Mem        , Conv_VTI     , JavascriptMath::ToInt32                            ) // convert var to int

  DEF3_WMS( CUSTOM_ASMJS     , LdArr_Func   , OP_LdArrFunc                 , ElementSlot         )
  DEF4_WMS( TEMPLATE_ASMJS   , LdSlot_Db    , OP_LdSlotPrimitive           , ElementSlot, double )
  DEF4_WMS( TEMPLATE_ASMJS   , LdSlot_Int   , OP_LdSlotPrimitive           , ElementSlot, int    )
  DEF4_WMS( TEMPLATE_ASMJS   , LdSlot_Flt   , OP_LdSlotPrimitive           , ElementSlot, float  )
  DEF4_WMS( TEMPLATE_ASMJS   , StSlot_Db    , OP_StSlotPrimitive           , ElementSlot, double )
  DEF4_WMS( TEMPLATE_ASMJS   , StSlot_Int   , OP_StSlotPrimitive           , ElementSlot, int    )
  DEF4_WMS( TEMPLATE_ASMJS   , StSlot_Flt   , OP_StSlotPrimitive           , ElementSlot, float  )
  DEF3_WMS( CUSTOM_ASMJS     , LdArr        , OP_LdArrGeneric              , AsmTypedArr         )
  DEF3_WMS( CUSTOM_ASMJS     , LdArrConst   , OP_LdArrConstIndex           , AsmTypedArr         )
  DEF3_WMS( CUSTOM_ASMJS     , StArr        , OP_StArrGeneric              , AsmTypedArr         )
  DEF3_WMS( CUSTOM_ASMJS     , StArrConst   , OP_StArrConstIndex           , AsmTypedArr         )

  DEF2_WMS( C1toI1           , Ld_IntConst  , None                                               )
  DEF2_WMS( BR_ASM_MemStack  , BrTrue_Int   , None                                               ) // Jumps to location if int reg is true
  DEF2_WMS( BR_ASM_Mem       , BrEq_Int     , AsmJsMath::CmpEq<int>                              ) // Jumps to location if both int reg are equal
  DEF2_WMS( D1toI1Scr        , Conv_DTI     , JavascriptConversion::ToInt32                      ) // convert double to int
  DEF2_WMS( F1toI1Scr        , Conv_FTI     , JavascriptConversion::ToInt32                      ) // convert float to int
  DEF2_WMS( I1toD1Mem        , Conv_ITD     , (double)                                           ) // convert int to double
  DEF2_WMS( U1toD1Mem        , Conv_UTD     , (double)                                           ) // convert unsigned int to double
  DEF2_WMS( F1toD1Mem        , Conv_FTD     , (double)                                           ) // convert unsigned float to double
  DEF2_WMS( I1toI1Mem        , Ld_Int       , (int)                                              )
  DEF2_WMS( D1toD1Mem        , Ld_Db        , (double)                                           )
  DEF2_WMS( F1toF1Mem        , Ld_Flt       , (float)                                            )
  DEF2_WMS( D1toD1Mem        , Return_Db    , (double)                                           ) // convert double to var
  DEF2_WMS( F1toF1Mem        , Return_Flt   , (float)                                            ) // convert float to var
  DEF2_WMS( I1toI1Mem        , Return_Int   , (int)                                              ) // convert int to var

  DEF2_WMS( I1toI1Mem        , BeginSwitch_Int, (int)                                            )
  DEF2    ( BR_ASM           , EndSwitch_Int, OP_Br                                              )
  DEF2_WMS( BR_ASM_Mem       , Case_Int     , AsmJsMath::CmpEq<int>                              )

  DEF2_WMS( I1toI1Mem        , Neg_Int      , AsmJsMath::Neg<int>                                ) // int unary '-'
  DEF2_WMS( I1toI1Mem        , Not_Int      , AsmJsMath::Not                                     ) // int unary '~'
  DEF2_WMS( I1toI1Mem        , LogNot_Int   , AsmJsMath::LogNot                                  ) // int unary '!'
  DEF2_WMS( I1toI1Mem        , Conv_ITB     , AsmJsMath::ToBool                                  ) // convert an int to a bool (0|1)
  DEF2_WMS( I2toI1Mem        , Add_Int      , AsmJsMath::Add<int>                                )
  DEF2_WMS( I2toI1Mem        , Sub_Int      , AsmJsMath::Sub<int>                                )
  DEF2_WMS( I2toI1Mem        , Mul_Int      , AsmJsMath::Mul<int>                                )
  DEF2_WMS( I2toI1Mem        , Div_Int      , AsmJsMath::Div<int>                                )
  DEF2_WMS( I2toI1Mem        , Rem_Int      , AsmJsMath::Rem<int>                                )
  DEF2_WMS( I2toI1Mem        , And_Int      , AsmJsMath::And                                     )
  DEF2_WMS( I2toI1Mem        , Or_Int       , AsmJsMath::Or                                      )
  DEF2_WMS( I2toI1Mem        , Xor_Int      , AsmJsMath::Xor                                     )
  DEF2_WMS( I2toI1Mem        , Shl_Int      , AsmJsMath::Shl                                     )
  DEF2_WMS( I2toI1Mem        , Shr_Int      , AsmJsMath::Shr                                     )
  DEF2_WMS( I2toI1Mem        , ShrU_Int     , AsmJsMath::ShrU                                    )

  DEF2_WMS( I2toI1MemDConv   , Mul_UInt     , AsmJsMath::Mul<double>                             )
  DEF2_WMS( I2toI1MemDConv   , Div_UInt     , AsmJsMath::Div<double>                             )
  DEF2_WMS( I2toI1MemDConv   , Rem_UInt     , AsmJsMath::Rem<uint>                               )

  DEF2_WMS( D1toD1Mem        , Neg_Db       , AsmJsMath::Neg<double>                             ) // double unary '-'
  DEF2_WMS( D2toD1Mem        , Add_Db       , AsmJsMath::Add<double>                             )
  DEF2_WMS( D2toD1Mem        , Sub_Db       , AsmJsMath::Sub<double>                             )
  DEF2_WMS( D2toD1Mem        , Mul_Db       , AsmJsMath::Mul<double>                             )
  DEF2_WMS( D2toD1Mem        , Div_Db       , AsmJsMath::Div<double>                             )
  DEF2_WMS( D2toD1Mem        , Rem_Db       , AsmJsMath::Rem<double>                             )

  DEF2_WMS( F1toF1Mem        , Neg_Flt      , AsmJsMath::Neg<float>                              ) // float unary '-'
  DEF2_WMS( F2toF1Mem        , Add_Flt      , AsmJsMath::Add<float>                              )
  DEF2_WMS( F2toF1Mem        , Sub_Flt      , AsmJsMath::Sub<float>                              )
  DEF2_WMS( F2toF1Mem        , Mul_Flt      , AsmJsMath::Mul<float>                              )
  DEF2_WMS( F2toF1Mem        , Div_Flt      , AsmJsMath::Div<float>                              )

  DEF2_WMS( I2toI1Mem        , CmLt_Int     , AsmJsMath::CmpLt<int>                              )
  DEF2_WMS( I2toI1Mem        , CmLe_Int     , AsmJsMath::CmpLe<int>                              )
  DEF2_WMS( I2toI1Mem        , CmGt_Int     , AsmJsMath::CmpGt<int>                              )
  DEF2_WMS( I2toI1Mem        , CmGe_Int     , AsmJsMath::CmpGe<int>                              )
  DEF2_WMS( I2toI1Mem        , CmEq_Int     , AsmJsMath::CmpEq<int>                              )
  DEF2_WMS( I2toI1Mem        , CmNe_Int     , AsmJsMath::CmpNe<int>                              )
  DEF2_WMS( I2toI1Mem        , CmLt_UnInt   , AsmJsMath::CmpLt<unsigned int>                     )
  DEF2_WMS( I2toI1Mem        , CmLe_UnInt   , AsmJsMath::CmpLe<unsigned int>                     )
  DEF2_WMS( I2toI1Mem        , CmGt_UnInt   , AsmJsMath::CmpGt<unsigned int>                     )
  DEF2_WMS( I2toI1Mem        , CmGe_UnInt   , AsmJsMath::CmpGe<unsigned int>                     )
  DEF2_WMS( I1toI1Mem        , Abs_Int      , ::abs                                              )
  DEF2_WMS( I2toI1Mem        , Min_Int      , min                                                )
  DEF2_WMS( I2toI1Mem        , Max_Int      , max                                                )
  DEF2_WMS( I2toI1Mem        , Imul_Int     , AsmJsMath::Mul<int>                                )
  DEF2_WMS( I1toI1Mem        , Clz32_Int    , AsmJsMath::Clz32                                   )


  DEF2_WMS( D2toI1Mem        , CmLt_Db      , AsmJsMath::CmpLt<double>                           )
  DEF2_WMS( D2toI1Mem        , CmLe_Db      , AsmJsMath::CmpLe<double>                           )
  DEF2_WMS( D2toI1Mem        , CmGt_Db      , AsmJsMath::CmpGt<double>                           )
  DEF2_WMS( D2toI1Mem        , CmGe_Db      , AsmJsMath::CmpGe<double>                           )
  DEF2_WMS( D2toI1Mem        , CmEq_Db      , AsmJsMath::CmpEq<double>                           )
  DEF2_WMS( D2toI1Mem        , CmNe_Db      , AsmJsMath::CmpNe<double>                           )

  DEF2_WMS( F2toI1Mem        , CmLt_Flt     , AsmJsMath::CmpLt<float>                            )
  DEF2_WMS( F2toI1Mem        , CmLe_Flt     , AsmJsMath::CmpLe<float>                            )
  DEF2_WMS( F2toI1Mem        , CmGt_Flt     , AsmJsMath::CmpGt<float>                            )
  DEF2_WMS( F2toI1Mem        , CmGe_Flt     , AsmJsMath::CmpGe<float>                            )
  DEF2_WMS( F2toI1Mem        , CmEq_Flt     , AsmJsMath::CmpEq<float>                            )
  DEF2_WMS( F2toI1Mem        , CmNe_Flt     , AsmJsMath::CmpNe<float>                            )

  DEF2_WMS( D1toD1Mem        , Sin_Db       , Math::Sin                                          )
  DEF2_WMS( D1toD1Mem        , Cos_Db       , Math::Cos                                          )
  DEF2_WMS( D1toD1Mem        , Tan_Db       , Math::Tan                                          )
  DEF2_WMS( D1toD1Mem        , Asin_Db      , Math::Asin                                         )
  DEF2_WMS( D1toD1Mem        , Acos_Db      , Math::Acos                                         )
  DEF2_WMS( D1toD1Mem        , Atan_Db      , Math::Atan                                         )
  DEF2_WMS( D1toD1Mem        , Ceil_Db      , ::ceil                                             )
  DEF2_WMS( F1toF1Mem        , Ceil_Flt     , ::ceilf                                            )
  DEF2_WMS( D1toD1Mem        , Floor_Db     , ::floor                                            )
  DEF2_WMS( F1toF1Mem        , Floor_Flt    , ::floorf                                           )
  DEF2_WMS( D1toD1Mem        , Exp_Db       , Math::Exp                                          )
  DEF2_WMS( D1toD1Mem        , Log_Db       , Math::Log                                          )
  DEF2_WMS( D2toD1Mem        , Pow_Db       , Math::Pow                                          )
  DEF2_WMS( D1toD1Mem        , Sqrt_Db      , ::sqrt                                             )
  DEF2_WMS( F1toF1Mem        , Sqrt_Flt     , ::sqrtf                                            )
  DEF2_WMS( D1toD1Mem        , Abs_Db       , Math::Abs                                          )
  DEF2_WMS( F1toF1Mem        , Abs_Flt      , ::fabsf                                            )
  DEF2_WMS( D2toD1Mem        , Atan2_Db     , Math::Atan2                                        )
  DEF2_WMS( D2toD1Mem        , Min_Db       , AsmJsMath::Min<double>                             )
  DEF2_WMS( D2toD1Mem        , Max_Db       , AsmJsMath::Max<double>                             )


  DEF2_WMS( F1toF1Mem        , Fround_Flt   , (float)                                            )
  DEF2_WMS( D1toF1Mem        , Fround_Db    , (float)                                            )
  DEF2_WMS( I1toF1Mem        , Fround_Int   , (float)                                            )

  DEF2_WMS( IP_TARG_ASM      , AsmJsLoopBodyStart, OP_ProfiledLoopBodyStart                      )

  //unary ops
  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_Ld_F4        , (AsmJsSIMDValue)                                   )
  DEF2_WMS( SIMD_I4_1toI4_1  , Simd128_Ld_I4        , (AsmJsSIMDValue)                                   )
  EXDEF2_WMS( SIMD_D2_1toD2_1  , Simd128_Ld_D2        , (AsmJsSIMDValue)                                   )

  DEF2_WMS( SIMD_F4toF4_1    , Simd128_FloatsToF4   , SIMDFloat32x4Operation::OpFloat32x4                )
  DEF2_WMS( SIMD_I4toI4_1    , Simd128_IntsToI4     , SIMDInt32x4Operation::OpInt32x4                    )
  DEF2_WMS( SIMD_D2toD2_1    , Simd128_DoublesToD2  , SIMDFloat64x2Operation::OpFloat64x2                )

  DEF4_WMS( TEMPLATE_ASMJS   , Simd128_LdSlot_F4    , OP_LdSlotPrimitive          , ElementSlot, AsmJsSIMDValue)
  DEF4_WMS( TEMPLATE_ASMJS   , Simd128_LdSlot_I4    , OP_LdSlotPrimitive          , ElementSlot, AsmJsSIMDValue)
  EXDEF4_WMS( TEMPLATE_ASMJS   , Simd128_LdSlot_D2    , OP_LdSlotPrimitive          , ElementSlot, AsmJsSIMDValue)

  DEF4_WMS(TEMPLATE_ASMJS    , Simd128_StSlot_F4    , OP_StSlotPrimitive          , ElementSlot, AsmJsSIMDValue)
  DEF4_WMS(TEMPLATE_ASMJS    , Simd128_StSlot_I4    , OP_StSlotPrimitive          , ElementSlot, AsmJsSIMDValue)
  EXDEF4_WMS(TEMPLATE_ASMJS    , Simd128_StSlot_D2    , OP_StSlotPrimitive          , ElementSlot, AsmJsSIMDValue)

  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_Return_F4    , (AsmJsSIMDValue)                                   )
  DEF2_WMS( SIMD_I4_1toI4_1  , Simd128_Return_I4    , (AsmJsSIMDValue)                                   )
  DEF2_WMS( SIMD_D2_1toD2_1  , Simd128_Return_D2    , (AsmJsSIMDValue)                                   )

  DEF2_WMS( SIMD_F1toF4_1    , Simd128_Splat_F4     ,Js::SIMDFloat32x4Operation::OpSplat                )
  DEF2_WMS( SIMD_I1toI4_1    , Simd128_Splat_I4     ,Js::SIMDInt32x4Operation::OpSplat                  )
  DEF2_WMS( SIMD_D1toD2_1    , Simd128_Splat_D2     ,Js::SIMDFloat64x2Operation::OpSplat                )

  DEF2_WMS( SIMD_D2_1toF4_1  , Simd128_FromFloat64x2_F4    ,SIMDFloat32x4Operation::OpFromFloat64x2      )
  DEF2_WMS( SIMD_D2_1toF4_1  , Simd128_FromFloat64x2Bits_F4,SIMDFloat32x4Operation::OpFromFloat64x2Bits  )
  DEF2_WMS( SIMD_I4_1toF4_1  , Simd128_FromInt32x4_F4      ,SIMDFloat32x4Operation::OpFromInt32x4        )
  DEF2_WMS( SIMD_I4_1toF4_1  , Simd128_FromInt32x4Bits_F4  ,SIMDFloat32x4Operation::OpFromInt32x4Bits    )

  DEF2_WMS( SIMD_D2_1toI4_1  , Simd128_FromFloat64x2_I4    ,SIMDInt32x4Operation::OpFromFloat64x2        )
  DEF2_WMS( SIMD_D2_1toI4_1  , Simd128_FromFloat64x2Bits_I4,SIMDInt32x4Operation::OpFromFloat64x2Bits    )
  DEF2_WMS( SIMD_F4_1toI4_1  , Simd128_FromFloat32x4_I4    ,SIMDInt32x4Operation::OpFromFloat32x4        )
  DEF2_WMS( SIMD_F4_1toI4_1  , Simd128_FromFloat32x4Bits_I4,SIMDInt32x4Operation::OpFromFloat32x4Bits    )

  DEF2_WMS( SIMD_F4_1toD2_1  , Simd128_FromFloat32x4_D2    ,SIMDFloat64x2Operation::OpFromFloat32x4      )
  DEF2_WMS( SIMD_F4_1toD2_1  , Simd128_FromFloat32x4Bits_D2,SIMDFloat64x2Operation::OpFromFloat32x4Bits  )
  DEF2_WMS( SIMD_I4_1toD2_1  , Simd128_FromInt32x4_D2      ,SIMDFloat64x2Operation::OpFromInt32x4        )
  DEF2_WMS( SIMD_I4_1toD2_1  , Simd128_FromInt32x4Bits_D2  ,SIMDFloat64x2Operation::OpFromInt32x4Bits    )

  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_Abs_F4              ,SIMDFloat32x4Operation::OpAbs                )
  DEF2_WMS( SIMD_D2_1toD2_1  , Simd128_Abs_D2              ,SIMDFloat64x2Operation::OpAbs                )

  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_Neg_F4              ,SIMDFloat32x4Operation::OpNeg                )
  DEF2_WMS( SIMD_I4_1toI4_1  , Simd128_Neg_I4              ,SIMDInt32x4Operation::OpNeg                  )
  DEF2_WMS( SIMD_D2_1toD2_1  , Simd128_Neg_D2              ,SIMDFloat64x2Operation::OpNeg                )

  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_Rcp_F4              ,SIMDFloat32x4Operation::OpReciprocal         )
  DEF2_WMS( SIMD_D2_1toD2_1  , Simd128_Rcp_D2              ,SIMDFloat64x2Operation::OpReciprocal         )

  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_RcpSqrt_F4          ,SIMDFloat32x4Operation::OpReciprocalSqrt     )
  DEF2_WMS( SIMD_D2_1toD2_1  , Simd128_RcpSqrt_D2          ,SIMDFloat64x2Operation::OpReciprocalSqrt     )

  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_Sqrt_F4             ,SIMDFloat32x4Operation::OpSqrt               )
  DEF2_WMS( SIMD_D2_1toD2_1  , Simd128_Sqrt_D2             ,SIMDFloat64x2Operation::OpSqrt               )

  DEF2_WMS( SIMD_F4_1toF4_1  , Simd128_Not_F4              , Js::SIMDFloat32x4Operation::OpNot           )
  DEF2_WMS( SIMD_I4_1toI4_1  , Simd128_Not_I4              , Js::SIMDInt32x4Operation::OpNot             )

  // binary ops
  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Add_F4              , Js::SIMDFloat32x4Operation::OpAdd           )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Add_I4              , Js::SIMDInt32x4Operation::OpAdd             )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Add_D2              , Js::SIMDFloat64x2Operation::OpAdd           )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Sub_F4              , Js::SIMDFloat32x4Operation::OpSub           )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Sub_I4              , Js::SIMDInt32x4Operation::OpSub             )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Sub_D2              , Js::SIMDFloat64x2Operation::OpSub           )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Mul_F4              , Js::SIMDFloat32x4Operation::OpMul           )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Mul_I4              , Js::SIMDInt32x4Operation::OpMul             )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Mul_D2              , Js::SIMDFloat64x2Operation::OpMul           )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Div_F4              , Js::SIMDFloat32x4Operation::OpDiv           )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Div_D2              , Js::SIMDFloat64x2Operation::OpDiv           )

  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Min_D2              , Js::SIMDFloat64x2Operation::OpMin           )
  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Min_F4              , Js::SIMDFloat32x4Operation::OpMin           )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Max_F4              , Js::SIMDFloat32x4Operation::OpMax           )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Max_D2              , Js::SIMDFloat64x2Operation::OpMax           )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Lt_F4               , Js::SIMDFloat32x4Operation::OpLessThan      )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Lt_I4               , Js::SIMDInt32x4Operation::OpLessThan        )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Lt_D2               , Js::SIMDFloat64x2Operation::OpLessThan      )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_LtEq_F4             , Js::SIMDFloat32x4Operation::OpLessThanOrEqual)
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_LtEq_D2             , Js::SIMDFloat64x2Operation::OpLessThanOrEqual)

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Eq_F4               , Js::SIMDFloat32x4Operation::OpEqual         )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Eq_I4               , Js::SIMDInt32x4Operation::OpEqual           )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Eq_D2               , Js::SIMDFloat64x2Operation::OpEqual         )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Neq_F4              , Js::SIMDFloat32x4Operation::OpNotEqual      )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Neq_D2              , Js::SIMDFloat64x2Operation::OpNotEqual      )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_GtEq_F4             , Js::SIMDFloat32x4Operation::OpGreaterThanOrEqual)
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_GtEq_D2             , Js::SIMDFloat64x2Operation::OpGreaterThanOrEqual)

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Gt_F4               , Js::SIMDFloat32x4Operation::OpGreaterThan   )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Gt_I4               , Js::SIMDInt32x4Operation::OpGreaterThan     )
  DEF2_WMS( SIMD_D2_2toD2_1  , Simd128_Gt_D2               , Js::SIMDFloat64x2Operation::OpGreaterThan   )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_And_F4              , Js::SIMDFloat32x4Operation::OpAnd           )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_And_I4              , Js::SIMDInt32x4Operation::OpAnd             )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Or_F4               , Js::SIMDFloat32x4Operation::OpOr            )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Or_I4               , Js::SIMDInt32x4Operation::OpOr              )

  DEF2_WMS( SIMD_F4_2toF4_1  , Simd128_Xor_F4              , Js::SIMDFloat32x4Operation::OpXor           )
  DEF2_WMS( SIMD_I4_2toI4_1  , Simd128_Xor_I4              , Js::SIMDInt32x4Operation::OpXor             )

  // ternary ops
  DEF2_WMS( SIMD_F4_3toF4_1      , Simd128_Clamp_F4        , Js::SIMDFloat32x4Operation::OpClamp         )
  DEF2_WMS( SIMD_D2_3toD2_1      , Simd128_Clamp_D2        , Js::SIMDFloat64x2Operation::OpClamp         )
  DEF2_WMS( SIMD_I4_1F4_2toF4_1  , Simd128_Select_F4       , Js::SIMDFloat32x4Operation::OpSelect        )
  DEF2_WMS( SIMD_I4_3toI4_1      , Simd128_Select_I4       , Js::SIMDInt32x4Operation::OpSelect          )
  DEF2_WMS( SIMD_I4_1D2_2toD2_1  , Simd128_Select_D2       , Js::SIMDFloat64x2Operation::OpSelect        )

  DEF2_WMS( SIMD_F4_1toI1        , Simd128_LdSignMask_F4   , Js::SIMDFloat32x4Operation::OpGetSignMask   )
  DEF2_WMS( SIMD_I4_1toI1        , Simd128_LdSignMask_I4   , Js::SIMDInt32x4Operation::OpGetSignMask     )
  DEF2_WMS( SIMD_D2_1toI1        , Simd128_LdSignMask_D2   , Js::SIMDFloat64x2Operation::OpGetSignMask   )

  // args out, copy value to outParams
  DEF2_WMS   ( SIMD_F4_1toR1Mem  , Simd128_I_ArgOut_F4     , OP_I_SetOutAsmSimd                          )                               // ArgOut Float64x2
  DEF2_WMS   ( SIMD_I4_1toR1Mem  , Simd128_I_ArgOut_I4     , OP_I_SetOutAsmSimd                          )                               // ArgOut Float64x2
  DEF2_WMS   ( SIMD_D2_1toR1Mem  , Simd128_I_ArgOut_D2     , OP_I_SetOutAsmSimd                          )                               // ArgOut Float64x2

  DEF2_WMS   ( SIMD_F4_1toF4_1   , Simd128_I_Conv_VTF4     , (AsmJsSIMDValue)                            )
  DEF2_WMS   ( SIMD_I4_1toI4_1   , Simd128_I_Conv_VTI4     , (AsmJsSIMDValue)                            )
  DEF2_WMS   ( SIMD_D2_1toD2_1   , Simd128_I_Conv_VTD2     , (AsmJsSIMDValue)                            )


  DEF2_WMS   ( SIMD_F4_1I4toF4_1   , Simd128_Swizzle_F4      , SIMD128InnerShuffle                       )
  DEF2_WMS   ( SIMD_F4_2I4toF4_1   , Simd128_Shuffle_F4      , SIMD128InnerShuffle                       )

  DEF2_WMS   ( SIMD_I4_1I4toI4_1   , Simd128_Swizzle_I4      , SIMD128InnerShuffle                       )
  DEF2_WMS   ( SIMD_I4_2I4toI4_1   , Simd128_Shuffle_I4      , SIMD128InnerShuffle                       )

  // Extended opcodes

  EXDEF2_WMS (  SIMD_D2_1I2toD2_1  , Simd128_Swizzle_D2      , SIMD128InnerShuffle                        )
  EXDEF2_WMS (  SIMD_D2_2I2toD2_1  , Simd128_Shuffle_D2      , SIMD128InnerShuffle                        )

  //Lane Access
  EXDEF2_WMS   ( SIMD_I4_1I1toI1   , Simd128_ExtractLane_I4  , SIMD128InnerExtractLaneI4                  )
  EXDEF2_WMS   ( SIMD_I4_1I2toI4_1 , Simd128_ReplaceLane_I4  , SIMD128InnerReplaceLaneI4                  )
  EXDEF2_WMS   ( SIMD_F4_1I1toF1   , Simd128_ExtractLane_F4  , SIMD128InnerExtractLaneF4                  )
  EXDEF2_WMS   ( SIMD_F4_1I1F1toF4_1, Simd128_ReplaceLane_F4 , SIMD128InnerReplaceLaneF4                  )

  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_LdArr_F4        , OP_SimdLdArrGeneric , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_LdArr_I4        , OP_SimdLdArrGeneric , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_LdArr_D2        , OP_SimdLdArrGeneric , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_StArr_F4        , OP_SimdStArrGeneric , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_StArr_I4        , OP_SimdStArrGeneric , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_StArr_D2        , OP_SimdStArrGeneric , AsmSimdTypedArr       )

  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_LdArrConst_F4        , OP_SimdLdArrConstIndex  , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_LdArrConst_I4        , OP_SimdLdArrConstIndex  , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_LdArrConst_D2        , OP_SimdLdArrConstIndex  , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_StArrConst_F4        , OP_SimdStArrConstIndex  , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_StArrConst_I4        , OP_SimdStArrConstIndex  , AsmSimdTypedArr       )
  EXDEF3_WMS   ( CUSTOM_ASMJS      , Simd128_StArrConst_D2        , OP_SimdStArrConstIndex  , AsmSimdTypedArr       )
#endif
