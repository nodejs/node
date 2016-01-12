//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifndef TEMP_DISABLE_ASMJS
#define PROCESS_FALLTHROUGH_ASM(name, func) \
    case OpCodeAsmJs::name:
#define PROCESS_FALLTHROUGH_ASM_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name:

#define PROCESS_READ_LAYOUT_ASMJS(name, layout, suffix) \
    CompileAssert(OpCodeInfoAsmJs<OpCodeAsmJs::name>::Layout == OpLayoutTypeAsmJs::layout); \
    const unaligned OpLayout##layout##suffix * playout = m_reader.layout##suffix(ip); \
    Assert((playout != nullptr) == (Js::OpLayoutTypeAsmJs::##layout != Js::OpLayoutTypeAsmJs::Empty)); // Make sure playout is used

#define PROCESS_NOPASMJS_COMMON(name, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, layout, suffix); \
        break; \
    }

#define PROCESS_NOPASMJS(name, func) PROCESS_NOPASMJS_COMMON(name, func,)

#define PROCESS_BR_ASM(name, func) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, AsmBr,); \
        ip = func(playout); \
        break; \
    }


#define PROCESS_GET_ELEM_SLOT_ASM_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, layout, suffix); \
        SetNonVarReg(playout->Value, \
                func(GetNonVarReg(playout->Instance), playout)); \
        break; \
    }

#define PROCESS_GET_ELEM_SLOT_ASM(name, func, layout) PROCESS_GET_ELEM_SLOT_ASM_COMMON(name, func, layout,)


#define PROCESS_FUNCtoA1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, AsmReg1, suffix); \
        SetReg(playout->R0, \
                func(GetScriptContext())); \
        break; \
    }

#define PROCESS_FUNCtoA1Mem(name, func) PROCESS_FUNCtoA1Mem_COMMON(name, func,)


#define PROCESS_CUSTOM_ASMJS_COMMON(name, func, layout, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, layout, suffix); \
        func(playout); \
        break; \
    }

#define PROCESS_CUSTOM_ASMJS(name, func, layout) PROCESS_CUSTOM_ASMJS_COMMON(name, func, layout,)



#define PROCESS_I2toI1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
        { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int3, suffix); \
        SetRegRawInt(playout->I0, \
                func(GetRegRawInt(playout->I1), GetRegRawInt(playout->I2))); \
        break; \
        }

#define PROCESS_I2toI1Mem(name, func) PROCESS_I2toI1Mem_COMMON(name, func,)


#define PROCESS_I2toI1MemDConv_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
        { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int3, suffix); \
        SetRegRawInt(playout->I0, \
                JavascriptConversion::ToInt32(\
                func((unsigned int)GetRegRawInt(playout->I1), (unsigned int)GetRegRawInt(playout->I2)))); \
        break; \
        }

#define PROCESS_I2toI1MemDConv(name, func) PROCESS_I2toI1MemDConv_COMMON(name, func,)

#define PROCESS_F2toF1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                { \
                PROCESS_READ_LAYOUT_ASMJS(name, Float3, suffix); \
                SetRegRawFloat(playout->F0, \
                func(GetRegRawFloat(playout->F1), GetRegRawFloat(playout->F2))); \
                break; \
                }

#define PROCESS_D2toD1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Double3, suffix); \
        SetRegRawDouble(playout->D0, \
                func(GetRegRawDouble(playout->D1), GetRegRawDouble(playout->D2))); \
        break; \
                }

#define PROCESS_D2toD1Mem(name, func) PROCESS_D2toD1Mem_COMMON(name, func,)
#define PROCESS_F2toF1Mem(name, func) PROCESS_F2toF1Mem_COMMON(name, func,)

#define PROCESS_I1toI1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int2, suffix); \
        SetRegRawInt(playout->I0, \
                func(GetRegRawInt(playout->I1))); \
        break; \
    }
#define PROCESS_I1toI1Mem(name, func) PROCESS_I1toI1Mem_COMMON(name, func,)


#define PROCESS_D1toD1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Double2, suffix); \
        SetRegRawDouble(playout->D0, \
                GetRegRawDouble(playout->D1)); \
        break; \
                                                                }

#define PROCESS_D1toD1(name, func) PROCESS_D1toD1_COMMON(name, func,)

#define PROCESS_D1toD1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, Double2, suffix); \
        SetRegRawDouble(playout->D0, \
                func(GetRegRawDouble(playout->D1))); \
        break; \
    }

#define PROCESS_F1toF1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float2, suffix); \
    SetRegRawFloat(playout->F0, \
    func(GetRegRawFloat(playout->F1))); \
    break; \
    }

#define PROCESS_D1toF1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float1Double1, suffix); \
    SetRegRawFloat(playout->F0, \
            func(GetRegRawDouble(playout->D1))); \
    break; \
    }

#define PROCESS_I1toF1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float1Int1, suffix); \
    SetRegRawFloat(playout->F0, \
            func(GetRegRawInt(playout->I1))); \
    break; \
    }

#define PROCESS_D1toD1Mem(name, func) PROCESS_D1toD1Mem_COMMON(name, func,)
#define PROCESS_F1toF1Mem(name, func) PROCESS_F1toF1Mem_COMMON(name, func,)
#define PROCESS_D1toF1Mem(name, func) PROCESS_D1toF1Mem_COMMON(name, func,)
#define PROCESS_I1toF1Mem(name, func) PROCESS_I1toF1Mem_COMMON(name, func,)

#define PROCESS_IP_TARG_ASM_IMPL(name, func, layoutSize) \
    case OpCodeAsmJs::name: \
    { \
    Assert(!switchProfileMode); \
    ip = func<layoutSize, INTERPRETERPROFILE>(ip); \
if (switchProfileMode) \
        { \
        m_reader.SetIP(ip); \
        return nullptr; \
        } \
        break; \
    }
#define PROCESS_IP_TARG_ASM_COMMON(name, func, suffix) PROCESS_IP_TARG_ASM##suffix(name, func)

#define PROCESS_IP_TARG_ASM_Large(name, func) PROCESS_IP_TARG_ASM_IMPL(name, func, Js::LargeLayout)
#define PROCESS_IP_TARG_ASM_Medium(name, func) PROCESS_IP_TARG_ASM_IMPL(name, func, Js::MediumLayout)
#define PROCESS_IP_TARG_ASM_Small(name, func) PROCESS_IP_TARG_ASM_IMPL(name, func, Js::SmallLayout)

#define PROCESS_D1toI1Scr_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int1Double1, suffix); \
        SetRegRawInt(playout->I0, \
                func(GetRegRawDouble(playout->D1))); \
        break; \
                                                                }
#define PROCESS_D1toI1Scr(name, func) PROCESS_D1toI1Scr_COMMON(name, func,)

#define PROCESS_F1toI1Scr_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
       PROCESS_READ_LAYOUT_ASMJS(name, Int1Float1, suffix); \
       SetRegRawInt(playout->I0, \
                func(GetRegRawFloat(playout->F1))); \
       break; \
                                                                }
#define PROCESS_F1toI1Scr(name, func) PROCESS_F1toI1Scr_COMMON(name, func,)

#define PROCESS_I1toD1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Double1Int1, suffix); \
        SetRegRawDouble(playout->D0, \
                func(GetRegRawInt(playout->I1))); \
        break; \
                                                                }
#define PROCESS_I1toD1Mem(name, func) PROCESS_I1toD1Mem_COMMON(name, func,)

#define PROCESS_U1toD1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Double1Int1, suffix); \
        SetRegRawDouble(playout->D0, \
                func((unsigned int)GetRegRawInt(playout->I1)) ); \
        break; \
                                                                }
#define PROCESS_F1toD1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Double1Float1, suffix); \
        SetRegRawDouble(playout->D0, \
                func((float)GetRegRawFloat(playout->F1))); \
        break; \
                                                                }

#define PROCESS_U1toD1Mem(name, func) PROCESS_U1toD1Mem_COMMON(name, func,)
#define PROCESS_F1toD1Mem(name, func) PROCESS_F1toD1Mem_COMMON(name, func,)


#define PROCESS_R1toD1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Double1Reg1, suffix); \
        SetRegRawDouble(playout->D0, \
                func(GetReg(playout->R1),scriptContext)); \
        break; \
                                }

#define PROCESS_R1toD1Mem(name, func) PROCESS_R1toD1Mem_COMMON(name, func,)

#define PROCESS_R1toF1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Float1Reg1, suffix); \
        SetRegRawFloat(playout->F0, \
        (float)func(GetReg(playout->R1), scriptContext)); \
        break; \
                                }

#define PROCESS_R1toF1Mem(name, func) PROCESS_R1toF1Mem_COMMON(name, func,)


#define PROCESS_R1toI1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int1Reg1, suffix); \
        SetRegRawInt(playout->I0, \
                func(GetReg(playout->R1),scriptContext)); \
        break; \
                                                                }

#define PROCESS_R1toI1Mem(name, func) PROCESS_R1toI1Mem_COMMON(name, func,)

#define PROCESS_D1toR1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Reg1Double1, suffix); \
        SetRegRaw(playout->R0, \
            func(GetRegRawDouble(playout->D1),scriptContext)); \
        break; \
                                }

#define PROCESS_F1toR1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Reg1Float1, suffix); \
        SetRegRawFloat(playout->R0, \
            GetRegRawFloat(playout->F1)); \
        break; \
                                }

#define PROCESS_D1toR1Mem(name, func) PROCESS_D1toR1Mem_COMMON(name, func,)
#define PROCESS_F1toR1Mem(name, func) PROCESS_F1toR1Mem_COMMON(name, func,)


#define PROCESS_C1toI1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int1Const1, suffix); \
        SetRegRawInt( playout->I0, \
                func( playout->C1 )); \
        break; \
                                                                }

#define PROCESS_C1toI1Mem(name, func) PROCESS_C1toI1Mem_COMMON(name, func,)

#define PROCESS_C1toI1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int1Const1, suffix); \
        SetRegRawInt( playout->I0, playout->C1 ); \
        break; \
                                                                }

#define PROCESS_C1toI1(name, func) PROCESS_C1toI1_COMMON(name, func,)

#define PROCESS_I1toR1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Reg1Int1, suffix); \
        SetRegRawInt(playout->R0, \
              GetRegRawInt(playout->I1)); \
        break; \
                                }

#define PROCESS_I1toR1Mem(name, func) PROCESS_I1toR1Mem_COMMON(name, func,)

#define PROCESS_I1toR1Out_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Reg1Int1, suffix); \
            func(playout->R0, GetRegRawInt(playout->I1)); \
        break; \
                                                                }
#define PROCESS_F1toR1Out_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Reg1Float1, suffix); \
            func(playout->R0, GetRegRawFloat(playout->F1)); \
        break; \
                                                                }

#define PROCESS_D1toR1Out_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Reg1Double1, suffix); \
            func(playout->R0, GetRegRawDouble(playout->D1)); \
        break; \
                                                                }

#define PROCESS_D2toI1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int1Double2, suffix); \
        SetRegRawInt(playout->I0, \
                func(GetRegRawDouble(playout->D1),GetRegRawDouble(playout->D2))); \
        break; \
                                                                }

#define PROCESS_F2toI1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
                                                                { \
        PROCESS_READ_LAYOUT_ASMJS(name, Int1Float2, suffix); \
        SetRegRawInt(playout->I0, \
              func(GetRegRawFloat(playout->F1), GetRegRawFloat(playout->F2))); \
        break; \
                                                                }

#define PROCESS_D2toI1Mem(name, func) PROCESS_D2toI1Mem_COMMON(name, func,)
#define PROCESS_F2toI1Mem(name, func) PROCESS_F2toI1Mem_COMMON(name, func,)

#define PROCESS_BR_ASM_Mem_COMMON(name, func,suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, BrInt2, suffix); \
        if (func(GetRegRawInt(playout->I1), GetRegRawInt(playout->I2))) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }
#define PROCESS_BR_ASM_Mem(name, func) PROCESS_BR_ASM_Mem_COMMON(name, func,)

#define PROCESS_BR_ASM_MemStack_COMMON(name, func,suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, BrInt1, suffix); \
        if (GetRegRawInt(playout->I1)) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }
#define PROCESS_BR_ASM_MemStack(name, func) PROCESS_BR_ASM_MemStack_COMMON(name, func,)

#define PROCESS_TEMPLATE_ASMJS_COMMON(name, func, layout, suffix, type) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, layout, suffix); \
        func<OpLayout##layout##suffix,type>(playout); \
        break; \
    }


// initializers
#define PROCESS_SIMD_F4_1toR1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
        PROCESS_READ_LAYOUT_ASMJS(name, Reg1Float32x4_1, suffix); \
        func(playout->R0, GetRegRawSimd(playout->F4_1)); \
        break; \
    }
#define PROCESS_SIMD_F4_1toR1Mem(name, func, suffix) PROCESS_F4_1toR1Mem_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_1toR1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Reg1Int32x4_1, suffix); \
    func(playout->R0, GetRegRawSimd(playout->I4_1)); \
    break; \
    }
#define PROCESS_SIMD_I4_1toR1Mem(name, func, suffix) PROCESS_I4_1toR1Mem_COMMON(name, func, suffix)

#define PROCESS_SIMD_D2_1toR1Mem_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Reg1Float64x2_1, suffix); \
    func(playout->R0, GetRegRawSimd(playout->D2_1)); \
    break; \
    }
#define PROCESS_SIMD_D2_1toR1Mem(name, func, suffix) PROCESS_D2_1toR1Mem_COMMON(name, func, suffix)


#define PROCESS_SIMD_I1toI4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_1Int1, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawInt(playout->I1))); \
    break; \
    }
#define PROCESS_SIMD_I1toI4_1(name, func, suffix) PROCESS_SIMD_I1toI4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_F1toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_1Float1, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawFloat(playout->F1))); \
    break; \
    }
#define PROCESS_SIMD_F1toF4_1(name, func, suffix) PROCESS_SIMD_F1toF4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_D1toD2_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_1Double1, suffix); \
    SetRegRawSimd(playout->D2_0, func(GetRegRawDouble(playout->D1))); \
    break; \
    }
#define PROCESS_SIMD_D1toD2_1(name, func, suffix) PROCESS_SIMD_D1toD2_1_COMMON(name, func, suffix)

//// Value transfer
#define PROCESS_SIMD_F4toF4_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_1Float4, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawFloat(playout->F1), GetRegRawFloat(playout->F2), GetRegRawFloat(playout->F3), GetRegRawFloat(playout->F4))); \
    break; \
    }
#define PROCESS_SIMD_F4toF4_1(name, func, suffix) PROCESS_SIMD_F4toF4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4toI4_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_1Int4, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawInt(playout->I1), GetRegRawInt(playout->I2), GetRegRawInt(playout->I3), GetRegRawInt(playout->I4))); \
    break; \
    }
#define PROCESS_SIMD_I4toI4_1(name, func, suffix) PROCESS_SIMD_I4toI4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_D2toD2_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_1Double2, suffix); \
    SetRegRawSimd(playout->D2_0, func(GetRegRawDouble(playout->D1), GetRegRawDouble(playout->D2))); \
    break; \
    }
#define PROCESS_SIMD_D2toD2_1(name, func, suffix) PROCESS_SIMD_D2toD2_1_COMMON(name, func, suffix)

//// Conversions
#define PROCESS_SIMD_D2_1toF4_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_1Float64x2_1, suffix); \
    SetRegRawSimd(playout->F4_0, \
    func(GetRegRawSimd(playout->D2_1))); \
    break; \
    }
#define PROCESS_SIMD_D2_1toF4_1(name, func, suffix) PROCESS_SIMD_D2_1toF4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_1toF4_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_1Int32x4_1, suffix); \
    SetRegRawSimd(playout->F4_0, \
    func(GetRegRawSimd(playout->I4_1))); \
    break; \
    }
#define PROCESS_SIMD_I4_1toF4_1_1(name, func, suffix) PROCESS_SIMD_I4_1toF4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_D2_1toI4_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_1Float64x2_1, suffix); \
    SetRegRawSimd(playout->I4_0, \
    func(GetRegRawSimd(playout->D2_1))); \
    break; \
    }
#define PROCESS_SIMD_D2_1toI4_1(name, func, suffix) PROCESS_SIMD_D2_1toI4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_F4_1toI4_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_1Float32x4_1, suffix); \
    SetRegRawSimd(playout->I4_0, \
    func(GetRegRawSimd(playout->F4_1))); \
    break; \
    }
#define PROCESS_SIMD_F4_1toI4_1(name, func, suffix) PROCESS_SIMD_F4_1toI4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_F4_1toD2_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_1Float32x4_1, suffix); \
    SetRegRawSimd(playout->D2_0, \
    func(GetRegRawSimd(playout->F4_1))); \
    break; \
    }
#define PROCESS_SIMD_F4_1toD2_1(name, func, suffix) PROCESS_SIMD_F4_1toD2_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_1toD2_1_COMMON(name, func, suffix)\
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_1Int32x4_1, suffix); \
    SetRegRawSimd(playout->D2_0, \
    func(GetRegRawSimd(playout->I4_1))); \
    break; \
    }
#define PROCESS_SIMD_I4_1toD2_1(name, func, suffix) PROCESS_SIMD_I4_1toD2_1_COMMON(name, func, suffix)

// unary ops
#define PROCESS_SIMD_F4_1toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_2, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawSimd(playout->F4_1))); \
    break; \
    }
#define PROCESS_SIMD_F4_1toF4_1  (name, func,) PROCESS_F4_1toF4_1_COMMON(name, func,)

#define PROCESS_SIMD_I4_1toI4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_2, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawSimd(playout->I4_1))); \
    break; \
    }
#define PROCESS_SIMD_I4_1toI4_1  (name, func,) PROCESS_I4_1toI4_1_COMMON(name, func,)


#define PROCESS_SIMD_D2_1toD2_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_2, suffix); \
    SetRegRawSimd(playout->D2_0, func(GetRegRawSimd(playout->D2_1))); \
    break; \
    }
#define PROCESS_SIMD_D2_1toD2_1  (name, func,) PROCESS_D2_1toD2_1_COMMON(name, func,)


// binary ops
#define PROCESS_SIMD_F4_2toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_3, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawSimd(playout->F4_1), GetRegRawSimd(playout->F4_2))); \
    break; \
    }
#define PROCESS_SIMD_F4_2toF4_1(name, func, suffix) PROCESS_SIMD_F4_2toF4_1COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_2toI4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_3, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawSimd(playout->I4_1), GetRegRawSimd(playout->I4_2))); \
    break; \
    }
#define PROCESS_SIMD_I4_2toI4_1(name, func, suffix) PROCESS_SIMD_I4_2toI4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_D2_2toD2_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_3, suffix); \
    SetRegRawSimd(playout->D2_0, func(GetRegRawSimd(playout->D2_1), GetRegRawSimd(playout->D2_2))); \
    break; \
    }
#define PROCESS_SIMD_D2_2toD2_1(name, func, suffix) PROCESS_SIMD_D2_2toD2_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_SetLane_F4_COMMON(name, field, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_2Float1, suffix); \
    AsmJsSIMDValue val = GetRegRawSimd(playout->F4_1); \
    val.f32[field] = GetRegRawFloat(playout->F2); \
    SetRegRawSimd(playout->F4_0, val); \
    break; \
    }
#define PROCESS_SIMD_SetLane_F4(name, field, suffix) PROCESS_SIMD_SetLane_F4_COMMON(name, field, suffix)

#define PROCESS_SIMD_SetLane_I4_COMMON(name, field, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_2Int1, suffix); \
    AsmJsSIMDValue val = GetRegRawSimd(playout->I4_1); \
    val.i32[field] = GetRegRawInt(playout->I2); \
    SetRegRawSimd(playout->I4_0, val); \
    break; \
    }
#define PROCESS_SIMD_SetLane_I4(name, field, suffix) PROCESS_SIMD_SetLane_I4_COMMON(name, field, suffix)

#define PROCESS_SIMD_SetLane_D2_COMMON(name, field, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_2Double1, suffix); \
    AsmJsSIMDValue val = GetRegRawSimd(playout->D2_1); \
    val.f64[field] = GetRegRawDouble(playout->D2); \
    SetRegRawSimd(playout->D2_0, val); \
    break; \
    }
#define PROCESS_SIMD_SetLane_D2(name, field, suffix) PROCESS_SIMD_SetLane_D2_COMMON(name, field, suffix)

// ternary
#define PROCESS_SIMD_F4_3toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_4, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawSimd(playout->F4_1), GetRegRawSimd(playout->F4_2), GetRegRawSimd(playout->F4_3))); \
    break; \
    }
#define PROCESS_SIMD_F4_3toF4_1(name, func, suffix) PROCESS_SIMD_F4_3toF4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_3toI4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_4, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawSimd(playout->I4_1), GetRegRawSimd(playout->I4_2), GetRegRawSimd(playout->I4_3))); \
    break; \
    }
#define PROCESS_SIMD_I4_3toI4_1(name, func, suffix) PROCESS_SIMD_I4_3toI4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_D2_3toD2_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_4, suffix); \
    SetRegRawSimd(playout->D2_0, func(GetRegRawSimd(playout->D2_1), GetRegRawSimd(playout->D2_2), GetRegRawSimd(playout->D2_3))); \
    break; \
    }
#define PROCESS_SIMD_D2_3toD2_1(name, func, suffix) PROCESS_SIMD_D2_3toD2_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_1F4_2toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_1Int32x4_1Float32x4_2, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawSimd(playout->I4_1), GetRegRawSimd(playout->F4_2), GetRegRawSimd(playout->F4_3))); \
    break; \
    }
#define PROCESS_SIMD_I4_1F4_2toF4_1(name, func, suffix) PROCESS_SIMD_I4_1F4_2toF4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_1D2_2toD2_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_1Int32x4_1Float64x2_2, suffix); \
    SetRegRawSimd(playout->D2_0, func(GetRegRawSimd(playout->I4_1), GetRegRawSimd(playout->D2_2), GetRegRawSimd(playout->D2_3))); \
    break; \
    }
#define PROCESS_SIMD_I4_1D2_2toD2_1(name, func, suffix) PROCESS_SIMD_I4_1D2_2toD2_1_COMMON(name, func, suffix)

// Extract Lane / Replace Lane
#define PROCESS_SIMD_I4_1I1toI1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int1Int32x4_1Int1, suffix); \
    SetRegRawInt(playout->I0, func(GetRegRawSimd(playout->I4_1), GetRegRawInt(playout->I2))); \
    break; \
    }
#define PROCESS_SIMD_I4_1I1toI1(name, func, suffix) PROCESS_SIMD_I4_1I1toI1_COMMON(name, func, suffix)
#define PROCESS_SIMD_I4_1I2toI4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_2Int2, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawSimd(playout->I4_1), GetRegRawInt(playout->I2), GetRegRawInt(playout->I3))); \
    break; \
    }
#define PROCESS_SIMD_I4_1I2toI4_1(name, func, suffix) PROCESS_SIMD_I4_1I2toI4_1_COMMON(name, func, suffix)

#define PROCESS_SIMD_F4_1I1toF1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float1Float32x4_1Int1, suffix); \
    SetRegRawFloat(playout->F0, func(GetRegRawSimd(playout->F4_1), GetRegRawInt(playout->I2))); \
    break; \
    }
#define PROCESS_SIMD_F4_1I1toF1(name, func, suffix) PROCESS_SIMD_F4_1I1toF1_COMMON(name, func, suffix)
#define PROCESS_SIMD_F4_1I1F1toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_2Int1Float1, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawSimd(playout->F4_1), GetRegRawInt(playout->I2), GetRegRawFloat(playout->F3))); \
    break; \
    }
#define PROCESS_SIMD_I4_1I2toI4_1(name, func, suffix) PROCESS_SIMD_I4_1I2toI4_1_COMMON(name, func, suffix)

// signmask
#define PROCESS_SIMD_F4_1toI1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int1Float32x4_1, suffix); \
    SetRegRawInt(playout->I0, func(GetRegRawSimd(playout->F4_1))); \
    break; \
    }
#define PROCESS_SIMD_F4_1toI1(name, func, suffix) PROCESS_SIMD_F4_1toI1_COMMON(name, func, suffix)

#define PROCESS_SIMD_I4_1toI1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int1Int32x4_1, suffix); \
    SetRegRawInt(playout->I0, func(GetRegRawSimd(playout->I4_1))); \
    break; \
    }
#define PROCESS_SIMD_I4_1toI1(name, func, suffix) PROCESS_SIMD_I4_1toI1_COMMON(name, func, suffix)

#define PROCESS_SIMD_D2_1toI1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int1Float64x2_1, suffix); \
    SetRegRawInt(playout->I0, func(GetRegRawSimd(playout->D2_1))); \
    break; \
    }
#define PROCESS_SIMD_D2_1toI1(name, func, suffix) PROCESS_SIMD_D2_1toI1_COMMON(name, func, suffix)


// f4swizzle
#define PROCESS_SIMD_F4_1I4toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_2Int4, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawSimd(playout->F4_1), GetRegRawSimd(playout->F4_1), GetRegRawInt(playout->I2), GetRegRawInt(playout->I3), GetRegRawInt(playout->I4), GetRegRawInt(playout->I5))); \
    break; \
    }
#define PROCESS_SIMD_F4_1I4toF4_1(name, func, suffix) PROCESS_SIMD_F4_1I4toF4_1_COMMON(name, func, suffix)

// f4shuffle
#define PROCESS_SIMD_F4_2I4toF4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float32x4_3Int4, suffix); \
    SetRegRawSimd(playout->F4_0, func(GetRegRawSimd(playout->F4_1), GetRegRawSimd(playout->F4_2), GetRegRawInt(playout->I3), GetRegRawInt(playout->I4), GetRegRawInt(playout->I5), GetRegRawInt(playout->I6))); \
    break; \
    }
#define PROCESS_SIMD_F4_1I4toF4_1(name, func, suffix) PROCESS_SIMD_F4_1I4toF4_1_COMMON(name, func, suffix)

// i4swizzle
#define PROCESS_SIMD_I4_1I4toI4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_2Int4, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawSimd(playout->I4_1), GetRegRawSimd(playout->I4_1), GetRegRawInt(playout->I2), GetRegRawInt(playout->I3), GetRegRawInt(playout->I4), GetRegRawInt(playout->I5))); \
    break; \
    }
#define PROCESS_SIMD_I4_1I4toI4_1(name, func, suffix) PROCESS_SIMD_I4_1I4toI4_1_COMMON(name, func, suffix)

// i4shuffle
#define PROCESS_SIMD_I4_2I4toI4_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Int32x4_3Int4, suffix); \
    SetRegRawSimd(playout->I4_0, func(GetRegRawSimd(playout->I4_1), GetRegRawSimd(playout->I4_2), GetRegRawInt(playout->I3), GetRegRawInt(playout->I4), GetRegRawInt(playout->I5), GetRegRawInt(playout->I6))); \
    break; \
    }
#define PROCESS_SIMD_I4_1I4toI4_1(name, func, suffix) PROCESS_SIMD_I4_1I4toI4_1_COMMON(name, func, suffix)

// d2swizzle
#define PROCESS_SIMD_D2_1I2toD2_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_2Int2, suffix); \
    SetRegRawSimd(playout->D2_0, func<2>(GetRegRawSimd(playout->D2_1), GetRegRawSimd(playout->D2_1), GetRegRawInt(playout->I2), GetRegRawInt(playout->I3), 0, 0)); \
    break; \
    }
#define PROCESS_SIMD_D2_1I2toD2_1(name, func, suffix) PROCESS_SIMD_D2_1I2toD2_1_COMMON(name, func, suffix)

// d2shuffle
#define PROCESS_SIMD_D2_2I2toD2_1_COMMON(name, func, suffix) \
    case OpCodeAsmJs::name: \
    { \
    PROCESS_READ_LAYOUT_ASMJS(name, Float64x2_3Int2, suffix); \
    SetRegRawSimd(playout->D2_0, func<2>(GetRegRawSimd(playout->D2_1), GetRegRawSimd(playout->D2_2), GetRegRawInt(playout->I3), GetRegRawInt(playout->I4), 0, 0)); \
    break; \
    }
#define PROCESS_SIMD_D2_2I4toD2_1(name, func, suffix) PROCESS_SIMD_D2_2I2toD2_1COMMON(name, func, suffix)

#endif