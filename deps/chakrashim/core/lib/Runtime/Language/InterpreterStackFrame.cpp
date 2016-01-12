//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "EHBailoutData.h"
#include "Library\JavascriptRegularExpression.h"
#if DBG_DUMP
#include "ByteCode\OpCodeUtilAsmJs.h"
#endif

#include "Language\InterpreterStackFrame.h"
#include "Library\JavascriptGeneratorFunction.h"


///----------------------------------------------------------------------------
///
/// macros PROCESS_INtoOUT
///
/// This set of macros defines standard patterns for processing OpCodes in
/// RcInterpreter::Run().  Each macro is named for "in" - "out":
/// - A: Var
/// - I: Integer
/// - R: Double
/// - X: Nothing
///
/// Examples:
/// - "A2toA1" reads two registers, each storing an Var, and writes a single
///   register with a new Var.
/// - "A1I1toA2" reads two registers, first an Var and second an Int32, then
///   writes two Var registers.
///
/// Although these could use lookup tables to standard OpLayout types, this
/// additional indirection would slow the main interpreter loop further by
/// preventing the main 'switch' statement from using the OpCode to become a
/// direct local-function jump.
///----------------------------------------------------------------------------

#define PROCESS_FALLTHROUGH(name, func) \
    case OpCode::name:
#define PROCESS_FALLTHROUGH_COMMON(name, func, suffix) \
    case OpCode::name:

#define PROCESS_READ_LAYOUT(name, layout, suffix) \
    CompileAssert(OpCodeInfo<OpCode::name>::Layout == OpLayoutType::layout); \
    const unaligned OpLayout##layout##suffix * playout = m_reader.layout##suffix(ip); \
    Assert((playout != nullptr) == (Js::OpLayoutType::##layout != Js::OpLayoutType::Empty)); // Make sure playout is used


#define PROCESS_NOP_COMMON(name, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        break; \
    }

#define PROCESS_NOP(name, layout) PROCESS_NOP_COMMON(name, layout,)

#define PROCESS_CUSTOM_COMMON(name, func, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        func(playout); \
        break; \
    }

#define PROCESS_CUSTOM(name, func, layout) PROCESS_CUSTOM_COMMON(name, func, layout,)

#define PROCESS_CUSTOM_L_COMMON(name, func, layout, regslot, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        func(playout); \
        break; \
    }

#define PROCESS_CUSTOM_L(name, func, layout, regslot) PROCESS_CUSTOM_L_COMMON(name, func, layout, regslot,)

#define PROCESS_CUSTOM_L_Arg_COMMON(name, func, suffix) PROCESS_CUSTOM_L_COMMON(name, func, Arg, Arg, suffix)
#define PROCESS_CUSTOM_L_Arg2_COMMON(name, func, layout, suffix) PROCESS_CUSTOM_L_COMMON(name, func, layout, Arg, suffix)
#define PROCESS_CUSTOM_L_Arg(name, func) PROCESS_CUSTOM_L_COMMON(name, func, Arg, Arg,)

#define PROCESS_CUSTOM_ArgNoSrc_COMMON(name, func, suffix) PROCESS_CUSTOM_COMMON(name, func, ArgNoSrc, suffix)
#define PROCESS_CUSTOM_ArgNoSrc(name, func) PROCESS_CUSTOM_COMMON(name, func, ArgNoSrc,)

#define PROCESS_CUSTOM_L_R0_COMMON(name, func, layout, suffix) PROCESS_CUSTOM_L_COMMON(name, func, layout, R0, suffix)
#define PROCESS_CUSTOM_L_R0(name, func, layout) PROCESS_CUSTOM_L_COMMON(name, func, layout, R0,)

#define PROCESS_CUSTOM_L_Value_COMMON(name, func, layout, suffix) PROCESS_CUSTOM_L_COMMON(name, func, layout, Value, suffix)
#define PROCESS_CUSTOM_L_Value(name, func, layout) PROCESS_CUSTOM_L_COMMON(name, func, layout, Value,)

#define PROCESS_TRY(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Br,); \
        func(playout); \
        ip = m_reader.GetIP(); \
        break; \
    }

#define PROCESS_EMPTY(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Empty, ); \
        func(); \
        ip = m_reader.GetIP(); \
        break; \
    }

#define PROCESS_TRYBR2_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrReg2, suffix); \
        func((const byte*)(playout + 1), playout->RelativeJumpOffset, playout->R1, playout->R2); \
        ip = m_reader.GetIP(); \
        break; \
    }

#define PROCESS_CALL_COMMON(name, func, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        func(playout); \
        break; \
    }

#define PROCESS_CALL(name, func, layout) PROCESS_CALL_COMMON(name, func, layout,)

#define PROCESS_CALL_FLAGS_COMMON(name, func, layout, flags, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        func(playout, flags); \
        break; \
    }

#define PROCESS_CALL_FLAGS(name, func, layout, regslot) PROCESS_CALL_FLAGS_COMMON(name, func, layout, regslot,)

#define PROCESS_CALL_FLAGS_None_COMMON(name, func, layout, suffix) PROCESS_CALL_FLAGS_COMMON(name, func, layout, CallFlags_None, suffix)
#define PROCESS_CALL_FLAGS_None(name, func, layout) PROCESS_CALL_FLAGS_COMMON(name, func, layout, CallFlags_None,)

#define PROCESS_CALL_FLAGS_Value_COMMON(name, func, layout, suffix) PROCESS_CALL_FLAGS_COMMON(name, func, layout, CallFlags_Value, suffix)
#define PROCESS_CALL_FLAGS_Value(name, func, layout) PROCESS_CALL_FLAGS_COMMON(name, func, layout, CallFlags_Value,)

#define PROCESS_CALL_FLAGS_CallEval_COMMON(name, func, layout, suffix) PROCESS_CALL_FLAGS_COMMON(name, func, layout, CallFlags_ExtraArg, suffix)
#define PROCESS_CALL_FLAGS_CallEval(name, func, layout) PROCESS_CALL_FLAGS_COMMON(name, func, layout, CallFlags_ExtraArg,)

#define PROCESS_A1toXX_ALLOW_STACK_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1, suffix); \
        func(GetRegAllowStackVar(playout->R0)); \
        break; \
    }

#define PROCESS_A1toXX_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1, suffix); \
        func(GetReg(playout->R0)); \
        break; \
    }

#define PROCESS_A1toXX(name, func) PROCESS_A1toXX_COMMON(name, func,)

#define PROCESS_A1toXXMem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1, suffix); \
        func(GetReg(playout->R0), GetScriptContext()); \
        break; \
    }

#define PROCESS_A1toXXMem(name, func) PROCESS_A1toXXMem_COMMON(name, func,)

#define PROCESS_A1toXXMemNonVar_COMMON(name, func, type, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1, suffix); \
        func((type)GetNonVarReg(playout->R0), GetScriptContext()); \
        break; \
    }

#define PROCESS_A1toXXMemNonVar(name, func, type) PROCESS_A1toXXMemNonVar_COMMON(name, func, type,)

#define PROCESS_XXtoA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1, suffix); \
        SetReg(playout->R0, \
                func()); \
        break; \
    }

#define PROCESS_XXtoA1(name, func) PROCESS_XXtoA1_COMMON(name, func,)

#define PROCESS_XXtoA1NonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1, suffix); \
        SetNonVarReg(playout->R0, \
                func()); \
        break; \
    }

#define PROCESS_XXtoA1NonVar(name, func) PROCESS_XXtoA1NonVar_COMMON(name, func,)

#define PROCESS_XXtoA1Mem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1, suffix); \
        SetReg(playout->R0, \
                func(GetScriptContext())); \
        break; \
    }

#define PROCESS_XXtoA1Mem(name, func) PROCESS_XXtoA1Mem_COMMON(name, func,)

#define PROCESS_A1toA1_ALLOW_STACK_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        SetRegAllowStackVar(playout->R0, \
                func(GetRegAllowStackVar(playout->R1))); \
        break; \
    }

#define PROCESS_A1toA1_ALLOW_STACK(name, func) PROCESS_A1toA1_ALLOW_STACK_COMMON(name, func,)

#define PROCESS_A1toA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1))); \
        break; \
    }

#define PROCESS_A1toA1(name, func) PROCESS_A1toA1_COMMON(name, func,)


#define PROCESS_A1toA1Profiled_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ProfiledReg2, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1), playout->profileId)); \
        break; \
    }

#define PROCESS_A1toA1Profiled(name, func) PROCESS_A1toA1Profiled_COMMON(name, func,)

#define PROCESS_A1toA1CallNoArg_COMMON(name, func, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        SetReg(playout->R0, \
                func(playout)); \
        break; \
    }

#define PROCESS_A1toA1CallNoArg(name, func, layout) PROCESS_A1toA1CallNoArg_COMMON(name, func, layout,)

#define PROCESS_A1toA1Mem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1),GetScriptContext())); \
        break; \
    }

#define PROCESS_A1toA1Mem(name, func) PROCESS_A1toA1Mem_COMMON(name, func,)

#define PROCESS_A1toA1NonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        SetNonVarReg(playout->R0, \
                func(GetNonVarReg(playout->R1))); \
        break; \
    }

#define PROCESS_A1toA1NonVar(name, func) PROCESS_A1toA1NonVar_COMMON(name, func,)

#define PROCESS_A1toA1MemNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        SetNonVarReg(playout->R0, \
                func(GetNonVarReg(playout->R1),GetScriptContext())); \
        break; \
    }

#define PROCESS_A1toA1MemNonVar(name, func) PROCESS_A1toA1MemNonVar_COMMON(name, func,)

#define PROCESS_INNERtoA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        SetReg(playout->R0, InnerScopeFromIndex(playout->C1)); \
        break; \
    }

#define PROCESS_INNERtoA1(name, fun) PROCESS_INNERtoA1_COMMON(name, func,)

#define PROCESS_U1toINNERMemNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Unsigned1, suffix); \
        SetInnerScopeFromIndex(playout->C1, func(GetScriptContext())); \
        break; \
    }

#define PROCESS_U1toINNERMemNonVar(name, func) PROCESS_U1toINNERMemNonVar_COMMON(name, func,)

#define PROCESS_XXINNERtoA1MemNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        SetNonVarReg(playout->R0, \
                func(InnerScopeFromIndex(playout->C1), GetScriptContext())); \
        break; \
    }

#define PROCESS_XXINNERtoA1MemNonVar(name, func) PROCESS_XXINNERtoA1MemNonVar_COMMON(name, func,)

#define PROCESS_A1INNERtoA1MemNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2Int1, suffix); \
        SetNonVarReg(playout->R0, \
                func(InnerScopeFromIndex(playout->C1), GetNonVarReg(playout->R1), GetScriptContext())); \
        break; \
    }

#define PROCESS_A1LOCALtoA1MemNonVar(name, func) PROCESS_A1LOCALtoA1MemNonVar_COMMON(name, func,)

#define PROCESS_LOCALI1toA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        SetReg(playout->R0, \
                func(this->localClosure, playout->C1)); \
        break; \
    }

#define PROCESS_LOCALI1toA1(name, func) PROCESS_LOCALI1toA1_COMMON(name, func,)

#define PROCESS_A1I1toA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2Int1, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1), playout->C1)); \
        break; \
    }

#define PROCESS_A1I1toA1(name, func) PROCESS_A1I1toA1_COMMON(name, func,)

#define PROCESS_A1I1toA1Mem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2Int1, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1), playout->C1, GetScriptContext())); \
        break; \
    }

#define PROCESS_A1I1toA1Mem(name, func) PROCESS_A1I1toA1Mem_COMMON(name, func,)

#define PROCESS_RegextoA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        SetReg(playout->R0, \
                func(this->m_functionBody->GetLiteralRegex(playout->C1), GetScriptContext())); \
        break; \
    }

#define PROCESS_RegextoA1(name, func) PROCESS_RegextoA1_COMMON(name, func,)

#define PROCESS_A2toXX_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        func(GetReg(playout->R0), GetReg(playout->R1)); \
        break; \
    }

#define PROCESS_A2toXX(name, func) PROCESS_A2toXX_COMMON(name, func,)

#define PROCESS_A2toXXMemNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        func(GetNonVarReg(playout->R0), GetNonVarReg(playout->R1), GetScriptContext()); \
        break; \
    }

#define PROCESS_A2toXXMemNonVar(name, func) PROCESS_A2toXXMemNonVar_COMMON(name, func,)

#define PROCESS_A1NonVarToA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2, suffix); \
        SetReg(playout->R0, \
            func(GetNonVarReg(playout->R1))); \
        break; \
    }


#define PROCESS_A2NonVarToA1Reg_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3, suffix); \
        SetReg(playout->R0, \
            func(GetNonVarReg(playout->R1), playout->R2)); \
        break; \
    }

#define PROCESS_A2toA1Mem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1), GetReg(playout->R2),GetScriptContext())); \
        break; \
    }

#define PROCESS_A2toA1Mem(name, func) PROCESS_A2toA1Mem_COMMON(name, func,)

#define PROCESS_A2toA1MemProfiled_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ProfiledReg3, suffix); \
        SetReg(playout->R0, \
        func(GetReg(playout->R1), GetReg(playout->R2),GetScriptContext(), playout->profileId)); \
        break; \
    }

#define PROCESS_A2toA1MemProfiled(name, func) PROCESS_A2toA1MemProfiled_COMMON(name, func,)

#define PROCESS_A2toA1NonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3, suffix); \
        SetNonVarReg(playout->R0, \
                func(GetNonVarReg(playout->R1), GetNonVarReg(playout->R2))); \
        break; \
    }

#define PROCESS_A2toA1NonVar(name, func) PROCESS_A2toA1NonVar_COMMON(name, func,)

#define PROCESS_A2toA1MemNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3, suffix); \
        SetNonVarReg(playout->R0, \
                func(GetNonVarReg(playout->R1), GetNonVarReg(playout->R2),GetScriptContext())); \
        break; \
    }

#define PROCESS_A2toA1MemNonVar(name, func) PROCESS_A2toA1MemNonVar_COMMON(name, func,)

#define PROCESS_CMMem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3, suffix); \
        SetReg(playout->R0, \
            func(GetReg(playout->R1), GetReg(playout->R2), GetScriptContext()) ? JavascriptBoolean::OP_LdTrue(GetScriptContext()) : \
                    JavascriptBoolean::OP_LdFalse(GetScriptContext())); \
        break; \
    }

#define PROCESS_CMMem(name, func) PROCESS_CMMem_COMMON(name, func,)

#define PROCESS_ELEM_RtU_to_XX_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementRootU, suffix); \
        func(playout->PropertyIdIndex); \
        break; \
    }

#define PROCESS_ELEM_RtU_to_XX(name, func) PROCESS_ELEM_RtU_to_XX_COMMON(name, func,)

#define PROCESS_ELEM_C2_to_XX_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementScopedC, suffix); \
        func(GetEnvForEvalCode(), playout->PropertyIdIndex, GetReg(playout->Value)); \
        break; \
    }

#define PROCESS_ELEM_C2_to_XX(name, func) PROCESS_ELEM_C2_to_XX_COMMON(name, func,)

#define PROCESS_GET_ELEM_SLOT_FB_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementSlot, suffix); \
        SetReg(playout->Value, \
                func((FrameDisplay*)GetNonVarReg(playout->Instance), reinterpret_cast<Js::FunctionProxy**>(this->m_functionBody->GetNestedFuncReference(playout->SlotIndex)))); \
        break; \
    }

#define PROCESS_GET_ELEM_SLOT_FB(name, func) PROCESS_GET_ELEM_SLOT_FB_COMMON(name, func,)

#define PROCESS_GET_SLOT_FB_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementSlotI1, suffix); \
        SetReg(playout->Value, \
               func(this->GetFrameDisplayForNestedFunc(), reinterpret_cast<Js::FunctionProxy**>(this->m_functionBody->GetNestedFuncReference(playout->SlotIndex)))); \
        break; \
    }

#define PROCESS_GET_SLOT_FB(name, func) PROCESS_GET_SLOT_FB_COMMON(name, func,)

#define PROCESS_GET_ELEM_IMem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementI, suffix); \
        SetReg(playout->Value, \
                func(GetReg(playout->Instance), GetReg(playout->Element), GetScriptContext())); \
        break; \
    }

#define PROCESS_GET_ELEM_IMem(name, func) PROCESS_GET_ELEM_IMem_COMMON(name, func,)

#define PROCESS_GET_ELEM_IMem_Strict_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementI, suffix); \
        SetReg(playout->Value, \
                func(GetReg(playout->Instance), GetReg(playout->Element), GetScriptContext(), PropertyOperation_StrictMode)); \
        break; \
    }

#define PROCESS_GET_ELEM_IMem_Strict(name, func) PROCESS_GET_ELEM_IMem_Strict_COMMON(name, func,)

#define PROCESS_BR(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Br,); \
        ip = func(playout); \
        break; \
    }

#ifdef BYTECODE_BRANCH_ISLAND
#define PROCESS_BRLONG(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrLong,); \
        ip = func(playout); \
        break; \
    }
#endif

#define PROCESS_BRS(name,func)  \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrS,); \
        if (func(playout->val,GetScriptContext())) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRB_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrReg1, suffix); \
        if (func(GetReg(playout->R1))) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRB(name, func) PROCESS_BRB_COMMON(name, func,)

#define PROCESS_BRB_ALLOW_STACK_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrReg1, suffix); \
        if (func(GetRegAllowStackVar(playout->R1))) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRB_ALLOW_STACK(name, func) PROCESS_BRB_ALLOW_STACK_COMMON(name, func,)

#define PROCESS_BRBS_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrReg1, suffix); \
        if (func(GetReg(playout->R1), GetScriptContext())) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRBS(name, func) PROCESS_BRBS_COMMON(name, func,)

#define PROCESS_BRBReturnP1toA1_COMMON(name, func, type, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrReg2, suffix); \
        SetReg(playout->R1, func((type)GetNonVarReg(playout->R2))); \
        if (!GetReg(playout->R1)) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRBReturnP1toA1(name, func, type) PROCESS_BRBReturnP1toA1_COMMON(name, func, type,)

#define PROCESS_BRBMem_ALLOW_STACK_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrReg1, suffix); \
        if (func(GetRegAllowStackVar(playout->R1),GetScriptContext())) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }
#define PROCESS_BRBMem_ALLOW_STACK(name, func) PROCESS_BRBMem_ALLOW_STACK_COMMON(name, func,)

#define PROCESS_BRCMem_COMMON(name, func,suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrReg2, suffix); \
        if (func(GetReg(playout->R1), GetReg(playout->R2),GetScriptContext())) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRCMem(name, func) PROCESS_BRCMem_COMMON(name, func,)

#define PROCESS_BRPROP(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrProperty,); \
        if (func(GetReg(playout->Instance), playout->PropertyIdIndex, GetScriptContext())) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRLOCALPROP(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrLocalProperty,); \
        if (func(this->localClosure, playout->PropertyIdIndex, GetScriptContext())) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_BRENVPROP(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, BrEnvProperty,); \
        if (func(LdEnv(), playout->SlotIndex, playout->PropertyIdIndex, GetScriptContext())) \
        { \
            ip = m_reader.SetCurrentRelativeOffset(ip, playout->RelativeJumpOffset); \
        } \
        break; \
    }

#define PROCESS_W1(name, func) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, W1,); \
        func(playout->C1, GetScriptContext()); \
        break; \
    }

#define PROCESS_U1toA1_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        SetReg(playout->R0, \
                func(playout->C1,GetScriptContext())); \
        break; \
    }
#define PROCESS_U1toA1(name, func) PROCESS_U1toA1_COMMON(name, func,)

#define PROCESS_U1toA1NonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        SetNonVarReg(playout->R0, \
                func(playout->C1)); \
        break; \
    }
#define PROCESS_U1toA1NonVar(name, func) PROCESS_U1toA1NonVar_COMMON(name, func,)

#define PROCESS_U1toA1NonVar_FuncBody_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        SetNonVarReg(playout->R0, \
                func(playout->C1,GetScriptContext(), this->m_functionBody)); \
        break; \
    }
#define PROCESS_U1toA1NonVar_FuncBody(name, func) PROCESS_U1toA1NonVar_FuncBody_COMMON(name, func,)

#define PROCESS_A1I2toXXNonVar_FuncBody(name, func) PROCESS_A1I2toXXNonVar_FuncBody_COMMON(name, func,)

#define PROCESS_A1I2toXXNonVar_FuncBody_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3, suffix); \
        func(playout->R0, playout->R1, playout->R2, GetScriptContext(), this->m_functionBody); \
        break; \
    }

#define PROCESS_A1U1toXX_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg1Unsigned1, suffix); \
        func(GetNonVarReg(playout->R0), playout->C1); \
        break; \
    }

#define PROCESS_A1U1toXX(name, func) PROCESS_A1U1toXX_COMMON(name, func,)

#define PROCESS_EnvU1toXX_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Unsigned1, suffix); \
        func(LdEnv(), playout->C1); \
        break; \
    }

#define PROCESS_EnvU1toXX(name, func) PROCESS_EnvU1toXX_COMMON(name, func,)

#define PROCESS_GET_ELEM_SLOTNonVar_COMMON(name, func, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        SetNonVarReg(playout->Value, func(GetNonVarReg(playout->Instance), playout)); \
        break; \
    }

#define PROCESS_GET_ELEM_SLOTNonVar(name, func, layout) PROCESS_GET_ELEM_SLOTNonVar_COMMON(name, func, layout,)

#define PROCESS_GET_ELEM_LOCALSLOTNonVar_COMMON(name, func, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        SetNonVarReg(playout->Value, func((Var*)GetLocalClosure(), playout)); \
        break; \
    }

#define PROCESS_GET_ELEM_LOCALSLOTNonVar(name, func, layout) PROCESS_GET_ELEM_LOCALSLOTNonVar_COMMON(name, func, layout,)

#define PROCESS_GET_ELEM_INNERSLOTNonVar_COMMON(name, func, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        SetNonVarReg(playout->Value, func(InnerScopeFromIndex(playout->SlotIndex1), playout)); \
        break; \
    }

#define PROCESS_GET_ELEM_INNERSLOTNonVar(name, func, layout) PROCESS_GET_ELEM_INNERSLOTNonVar_COMMON(name, func, layout,)

#define PROCESS_GET_ELEM_ENVSLOTNonVar_COMMON(name, func, layout, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, layout, suffix); \
        SetNonVarReg(playout->Value, func(LdEnv(), playout)); \
        break; \
    }

#define PROCESS_GET_ELEM_ENVSLOTNonVar(name, func, layout) PROCESS_GET_ELEM_ENVSLOTNonVar_COMMON(name, func, layout,)

#define PROCESS_SET_ELEM_SLOTNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementSlot, suffix); \
        func(GetNonVarReg(playout->Instance), playout->SlotIndex, GetRegAllowStackVarEnableOnly(playout->Value)); \
        break; \
    }

#define PROCESS_SET_ELEM_SLOTNonVar(name, func) PROCESS_SET_ELEM_SLOTNonVar_COMMON(name, func,)

#define PROCESS_SET_ELEM_LOCALSLOTNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementSlotI1, suffix); \
        func((Var*)GetLocalClosure(), playout->SlotIndex, GetRegAllowStackVarEnableOnly(playout->Value)); \
        break; \
    }

#define PROCESS_SET_ELEM_LOCALSLOTNonVar(name, func) PROCESS_SET_ELEM_LOCALSLOTNonVar_COMMON(name, func,)

#define PROCESS_SET_ELEM_INNERSLOTNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementSlotI2, suffix); \
        func(InnerScopeFromIndex(playout->SlotIndex1), playout->SlotIndex2, GetRegAllowStackVarEnableOnly(playout->Value)); \
        break; \
    }

#define PROCESS_SET_ELEM_INNERSLOTNonVar(name, func) PROCESS_SET_ELEM_INNERSLOTNonVar_COMMON(name, func,)

#define PROCESS_SET_ELEM_ENVSLOTNonVar_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, ElementSlotI2, suffix); \
        func(LdEnv(), playout->SlotIndex1, playout->SlotIndex2, GetRegAllowStackVarEnableOnly(playout->Value)); \
        break; \
    }

#define PROCESS_SET_ELEM_ENVSLOTNonVar(name, func) PROCESS_SET_ELEM_ENVSLOTNonVar_COMMON(name, func,)

/*---------------------------------------------------------------------------------------------- */
#define PROCESS_A3toA1Mem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg4, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1), GetReg(playout->R2), GetReg(playout->R3), GetScriptContext())); \
        break; \
    }

#define PROCESS_A3toA1Mem(name, func) PROCESS_A3toA1Mem_COMMON(name, func,)

/*---------------------------------------------------------------------------------------------- */
#define PROCESS_A2I1toA1Mem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3B1, suffix); \
        SetReg(playout->R0, \
                func(GetReg(playout->R1), GetReg(playout->R2), playout->B3, GetScriptContext())); \
        break; \
    }

#define PROCESS_A2I1toA1Mem(name, func) PROCESS_A2I1toA1Mem_COMMON(name, func,)

/*---------------------------------------------------------------------------------------------- */
#define PROCESS_A2I1toXXMem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg2B1, suffix); \
        func(GetReg(playout->R0), GetReg(playout->R1), playout->B2, scriptContext); \
        break; \
    }

#define PROCESS_A2I1toXXMem(name, func) PROCESS_A2I1toXXMem_COMMON(name, func,)

/*---------------------------------------------------------------------------------------------- */
#define PROCESS_A3I1toXXMem_COMMON(name, func, suffix) \
    case OpCode::name: \
    { \
        PROCESS_READ_LAYOUT(name, Reg3B1, suffix); \
        func(GetReg(playout->R0), GetReg(playout->R1), GetReg(playout->R2), playout->B3, scriptContext); \
        break; \
    }

#define PROCESS_A3I1toXXMem(name, func) PROCESS_A3I1toXXMem_COMMON(name, func,)

#if ENABLE_PROFILE_INFO
#define PROCESS_IP_TARG_IMPL(name, func, layoutSize) \
    case OpCode::name: \
    { \
        Assert(!switchProfileMode); \
        ip = func<layoutSize, INTERPRETERPROFILE>(ip); \
        if(switchProfileMode) \
        { \
            m_reader.SetIP(ip); \
            return nullptr; \
        } \
        break; \
    }
#else
#define PROCESS_IP_TARG_IMPL(name, func, layoutSize) \
    case OpCode::name: \
    { \
        ip = func<layoutSize, INTERPRETERPROFILE>(ip); \
       break; \
    }
#endif

#define PROCESS_IP_TARG_COMMON(name, func, suffix) PROCESS_IP_TARG##suffix(name, func)

#define PROCESS_IP_TARG_Large(name, func) PROCESS_IP_TARG_IMPL(name, func, Js::LargeLayout)
#define PROCESS_IP_TARG_Medium(name, func) PROCESS_IP_TARG_IMPL(name, func, Js::MediumLayout)
#define PROCESS_IP_TARG_Small(name, func) PROCESS_IP_TARG_IMPL(name, func, Js::SmallLayout)


namespace Js
{
    extern const __declspec(selectany) uint32 TypedArrayViewMask[] =
    {
        (uint32)~0 //TYPE_INT8
        , (uint32)~0 //TYPE_UINT8
        , (uint32)~1 //TYPE_INT16
        , (uint32)~1 //TYPE_UINT16
        , (uint32)~3 //TYPE_INT32
        , (uint32)~3 //TYPE_UINT32
        , (uint32)~3 //TYPE_FLOAT32
        , (uint32)~7 //TYPE_FLOAT64
    };

#ifndef TEMP_DISABLE_ASMJS
    typedef void(InterpreterStackFrame::*ArrFunc)(uint32, RegSlot);

    const ArrFunc InterpreterStackFrame::StArrFunc[8] =
    {
        &InterpreterStackFrame::OP_StArr<int8>,
        &InterpreterStackFrame::OP_StArr<uint8>,
        &InterpreterStackFrame::OP_StArr<int16>,
        &InterpreterStackFrame::OP_StArr<uint16>,
        &InterpreterStackFrame::OP_StArr<int32>,
        &InterpreterStackFrame::OP_StArr<uint32>,
        &InterpreterStackFrame::OP_StArr<float>,
        &InterpreterStackFrame::OP_StArr<double>,
    };

    const ArrFunc InterpreterStackFrame::LdArrFunc[8] =
    {
        &InterpreterStackFrame::OP_LdArr<int8>,
        &InterpreterStackFrame::OP_LdArr<uint8>,
        &InterpreterStackFrame::OP_LdArr<int16>,
        &InterpreterStackFrame::OP_LdArr<uint16>,
        &InterpreterStackFrame::OP_LdArr<int32>,
        &InterpreterStackFrame::OP_LdArr<uint32>,
        &InterpreterStackFrame::OP_LdArr<float>,
        &InterpreterStackFrame::OP_LdArr<double>,
    };
#endif

    Var InterpreterStackFrame::InnerScopeFromRegSlot(RegSlot reg) const
    {
        return InnerScopeFromIndex(reg - m_functionBody->FirstInnerScopeReg());
    }

    Var InterpreterStackFrame::InnerScopeFromIndex(uint32 index) const
    {
        if (index >= m_functionBody->GetInnerScopeCount())
        {
            AssertMsg(false, "Illegal byte code: bad inner scope index");
            Js::Throw::FatalInternalError();
        }
        Assert(this->innerScopeArray != nullptr);
        return this->innerScopeArray[index];
    }

    void InterpreterStackFrame::SetInnerScopeFromIndex(uint32 index, Var scope)
    {
        if (index >= m_functionBody->GetInnerScopeCount())
        {
            AssertMsg(false, "Illegal byte code: bad inner scope index");
            Js::Throw::FatalInternalError();
        }
        Assert(this->innerScopeArray != nullptr);
        this->innerScopeArray[index] = scope;
    }

    const int k_stackFrameVarCount = (sizeof(InterpreterStackFrame) + sizeof(Var) - 1) / sizeof(Var);
    InterpreterStackFrame::Setup::Setup(Js::ScriptFunction * function, Js::Arguments& args, bool inlinee)
        : function(function), inParams(args.Values), inSlotsCount(args.Info.Count), executeFunction(function->GetFunctionBody()), callFlags(args.Info.Flags), bailedOutOfInlinee(inlinee)
    {
        SetupInternal();
    }

    InterpreterStackFrame::Setup::Setup(Js::ScriptFunction * function, Var * inParams, int inSlotsCount, bool inlinee)
        : function(function), inParams(inParams), inSlotsCount(inSlotsCount), executeFunction(function->GetFunctionBody()), callFlags(CallFlags_None), bailedOutOfInlinee(inlinee)
    {
        SetupInternal();
    }

    void InterpreterStackFrame::Setup::SetupInternal()
    {
        if (this->function->GetHasInlineCaches() && Js::ScriptFunctionWithInlineCache::Is(this->function))
        {
            this->inlineCaches = Js::ScriptFunctionWithInlineCache::FromVar(this->function)->GetInlineCaches();
        }
        else
        {
            this->inlineCaches = this->executeFunction->GetInlineCaches();
        }
        this->inlineCacheCount = this->executeFunction->GetInlineCacheCount();

        //
        // Compute the amount of memory needed on the stack:
        // - We compute this in 'Atoms' instead of 'bytes' to keep everything natural word aligned.
        //

        this->localCount = this->executeFunction->GetLocalsCount();
        uint extraVarCount = 0;
#if ENABLE_PROFILE_INFO
        if (Js::DynamicProfileInfo::EnableImplicitCallFlags(this->executeFunction))
        {
            extraVarCount += (sizeof(ImplicitCallFlags) * this->executeFunction->GetLoopCount() + sizeof(Var) - 1) / sizeof(Var);
        }
#endif

        this->varAllocCount = k_stackFrameVarCount + localCount + this->executeFunction->GetOutParamsDepth() + extraVarCount + this->executeFunction->GetInnerScopeCount();

        if (this->executeFunction->DoStackNestedFunc() && this->executeFunction->GetNestedCount() != 0)
        {
            // Track stack funcs...
            this->varAllocCount += (sizeof(StackScriptFunction) * this->executeFunction->GetNestedCount()) / sizeof(Var);
            if (!this->bailedOutOfInlinee)
            {
                // Frame display (if environment depth is statically known)...
                if (this->executeFunction->DoStackFrameDisplay())
                {
                    uint16 envDepth = this->executeFunction->GetEnvDepth();
                    Assert(envDepth != (uint16)-1);
                    this->varAllocCount += sizeof(FrameDisplay) / sizeof(Var) + (envDepth + 1);
                }
                // ...and scope slots (if any)
                if (this->executeFunction->DoStackScopeSlots())
                {
                    uint32 scopeSlots = this->executeFunction->scopeSlotArraySize;
                    Assert(scopeSlots != 0);
                    this->varAllocCount += scopeSlots + Js::ScopeSlots::FirstSlotIndex;
                }
            }
        }
    }

    InterpreterStackFrame *
    InterpreterStackFrame::Setup::InitializeAllocation(__in_ecount(varAllocCount) Var * allocation, bool initParams, bool profileParams, Var loopHeaderArray, DWORD_PTR stackAddr
#if DBG
    , Var invalidStackVar
#endif
    )
    {

        //
        // Initialize the new InterpreterStackFrame instance on the program stack.
        //

        //This will fail if InterpreterStackFrame ever gets a non-empty ctor (you'll need to use
        //placement_new(allocation, InterpreterStackFrame) instead, though that will cause problems
        //if the placement_new is surrounded by a try/finally since this would mix C++/SEH exception
        //handling.

        __analysis_assume(varAllocCount >= k_stackFrameVarCount + localCount);
        InterpreterStackFrame* newInstance = (InterpreterStackFrame*)allocation;

        newInstance->scriptContext  = this->executeFunction->GetScriptContext();
        newInstance->m_inSlotsCount = this->inSlotsCount;
        newInstance->m_inParams     = this->inParams;
        newInstance->m_callFlags    = this->callFlags;
        newInstance->m_outParams    = newInstance->m_localSlots + localCount;
        newInstance->m_outSp        = newInstance->m_outParams;
        newInstance->m_arguments    = NULL;
        newInstance->function       = this->function;
        newInstance->m_functionBody = this->executeFunction;
        newInstance->inlineCaches   = this->inlineCaches;
        newInstance->inlineCacheCount = this->inlineCacheCount;
        newInstance->currentLoopNum = LoopHeader::NoLoop;
        newInstance->currentLoopCounter = 0;
        newInstance->m_flags        = InterpreterStackFrameFlags_None;
        newInstance->closureInitDone = false;
#if ENABLE_PROFILE_INFO
        newInstance->switchProfileMode = false;
        newInstance->isAutoProfiling = false;
        newInstance->switchProfileModeOnLoopEndNumber = 0u - 1;
#endif
        newInstance->ehBailoutData = nullptr;
        newInstance->nestedTryDepth = -1;
        newInstance->nestedCatchDepth = -1;
        newInstance->nestedFinallyDepth = -1;
        newInstance->retOffset = 0;
        newInstance->localFrameDisplay = nullptr;
        newInstance->localClosure = nullptr;
        newInstance->innerScopeArray = nullptr;

        bool doInterruptProbe = newInstance->scriptContext->GetThreadContext()->DoInterruptProbe(this->executeFunction);
#if ENABLE_NATIVE_CODEGEN
        bool doJITLoopBody =
            !this->executeFunction->GetScriptContext()->GetConfig()->IsNoNative() &&
            !(this->executeFunction->GetHasTry() && (PHASE_OFF((Js::JITLoopBodyInTryCatchPhase), this->executeFunction) || this->executeFunction->GetHasFinally())) &&
            (this->executeFunction->ForceJITLoopBody() || this->executeFunction->IsJitLoopBodyPhaseEnabled()) &&
            !this->executeFunction->GetScriptContext()->IsInDebugMode();
#else
        const bool doJITLoopBody = false;
#endif

        // Pick a version of the LoopBodyStart OpCode handlers that is hardcoded to do loop body JIT and
        // interrupt probes as needed.
        if (doInterruptProbe)
        {
#if ENABLE_NATIVE_CODEGEN
            if (doJITLoopBody)
            {
                newInstance->opProfiledLoopBodyStart = &InterpreterStackFrame::ProfiledLoopBodyStart<true, true>;
                newInstance->opLoopBodyStart = &InterpreterStackFrame::LoopBodyStart<true, true>;
            }
            else
#endif
            {
#if ENABLE_PROFILE_INFO
                newInstance->opProfiledLoopBodyStart = &InterpreterStackFrame::ProfiledLoopBodyStart<true, false>;
#endif
                newInstance->opLoopBodyStart = &InterpreterStackFrame::LoopBodyStart<true, false>;
            }
        }
        else
        {
#if ENABLE_NATIVE_CODEGEN
            if (doJITLoopBody)
            {
                newInstance->opProfiledLoopBodyStart = &InterpreterStackFrame::ProfiledLoopBodyStart<false, true>;
                newInstance->opLoopBodyStart = &InterpreterStackFrame::LoopBodyStart<false, true>;
            }
            else
#endif
            {
#if ENABLE_PROFILE_INFO
                newInstance->opProfiledLoopBodyStart = &InterpreterStackFrame::ProfiledLoopBodyStart<false, false>;
#endif
                newInstance->opLoopBodyStart = &InterpreterStackFrame::LoopBodyStart<false, false>;
            }
        }

        newInstance->loopHeaderArray = loopHeaderArray;
        newInstance->m_stackAddress = stackAddr;

#if ENABLE_PROFILE_INFO
        // the savedLoopImplicitCallFlags is allocated at the end of the out param array
        newInstance->savedLoopImplicitCallFlags = nullptr;
#endif
        char * nextAllocBytes = (char *)(newInstance->m_outParams + this->executeFunction->GetOutParamsDepth());

        if (this->executeFunction->GetInnerScopeCount())
        {
            newInstance->innerScopeArray = (Var*)nextAllocBytes;
            nextAllocBytes += this->executeFunction->GetInnerScopeCount() * sizeof(Var);
        }

        if (this->executeFunction->DoStackNestedFunc() && this->executeFunction->GetNestedCount() != 0)
        {
            newInstance->InitializeStackFunctions((StackScriptFunction *)nextAllocBytes);
            nextAllocBytes = nextAllocBytes + sizeof(StackScriptFunction) * this->executeFunction->GetNestedCount();

            if (!this->bailedOutOfInlinee)
            {
                if (this->executeFunction->DoStackFrameDisplay())
                {
                    uint16 envDepth = this->executeFunction->GetEnvDepth();
                    Assert(envDepth != (uint16)-1);
                    newInstance->localFrameDisplay = (FrameDisplay*)nextAllocBytes;
                    nextAllocBytes += sizeof(FrameDisplay) + (envDepth + 1) * sizeof(Var);
                }

                if (this->executeFunction->DoStackScopeSlots())
                {
                    uint32 scopeSlots = this->executeFunction->scopeSlotArraySize;
                    Assert(scopeSlots != 0);
                    newInstance->localClosure = nextAllocBytes;
                    nextAllocBytes += (scopeSlots + ScopeSlots::FirstSlotIndex) * sizeof(Var);
                }
            }
        }
#if ENABLE_PROFILE_INFO
        if (Js::DynamicProfileInfo::EnableImplicitCallFlags(this->executeFunction))
        {
            /*
            __analysis_assume(varAllocCount == (k_stackFrameVarCount + localCount + executeFunction->GetOutParamsDepth()
                                                + ((sizeof(ImplicitCallFlags) * executeFunction->GetLoopCount() + sizeof(Var) - 1) / sizeof(Var))));
           */
            newInstance->savedLoopImplicitCallFlags = (ImplicitCallFlags *)nextAllocBytes;
            for (uint i = 0; i < this->executeFunction->GetLoopCount(); i++)
            {
#pragma prefast(suppress:26015, "Above analysis assume doesn't work")
                newInstance->savedLoopImplicitCallFlags[i] = ImplicitCall_None;
            }


        }
#endif
#if DBG
        if (CONFIG_ISENABLED(InitializeInterpreterSlotsWithInvalidStackVarFlag))
        {
            // Fill the local slots with the invalid stack var so that we will crash deterministically if something goes wrong
            for (uint i = 0; i < localCount; ++i)
            {
                newInstance->m_localSlots[i] = invalidStackVar;
            }
        }
        else
        {
            memset(newInstance->m_localSlots, 0, sizeof(Js::Var) * localCount);
        }
#else
        if (newInstance->scriptContext->IsInDebugMode())
        {
            // In the debug mode zero out the local slot, so this could prevent locals being uninitialized in the case of setNextStatement.
            memset(newInstance->m_localSlots, 0, sizeof(Js::Var) * localCount);
        }
        // Zero out only the return slot. This is not a user local, so the byte code will not initialize
        // it to "undefined". And it's not an expression temp, so, for instance, a jitted loop body may expect
        // it to be valid on entry to the loop, where "valid" means either a var or null.
        newInstance->SetNonVarReg(0, NULL);
#endif

        // Initialize the low end of the local slots from the constant table.
        // Skip the slot for the return value register.
        this->executeFunction->InitConstantSlots(&newInstance->m_localSlots[FunctionBody::FirstRegSlot]);
        // Set local FD/SS pointers to null until after we've successfully probed the stack in the process loop.
        // That way we avoid trying to box these structures before they've been initialized in the byte code.
        if (this->executeFunction->DoStackFrameDisplay())
        {
            newInstance->SetNonVarReg(executeFunction->GetLocalFrameDisplayReg(), nullptr);
        }
        if (this->executeFunction->DoStackScopeSlots())
        {
            Assert(!executeFunction->HasScopeObject());
            newInstance->SetNonVarReg(executeFunction->GetLocalClosureReg(), nullptr);
        }

        Var *prestDest = &newInstance->m_localSlots[this->executeFunction->GetConstantCount()];
        if (initParams)
        {
#if ENABLE_PROFILE_INFO
            Assert(!this->executeFunction->NeedEnsureDynamicProfileInfo());
#endif
            if (profileParams)
            {
#if ENABLE_PROFILE_INFO
                Assert(this->executeFunction->HasExecutionDynamicProfileInfo());
#endif
                FunctionBody* functionBody = this->executeFunction;
                InitializeParams(newInstance, [functionBody](Var param, ArgSlot index)
                {
#if ENABLE_PROFILE_INFO
                    functionBody->GetDynamicProfileInfo()->RecordParameterInfo(functionBody, index - 1, param);
#endif
                }, &prestDest);
            }
            else
            {
                InitializeParams(newInstance, [](Var param, ArgSlot index) {}, &prestDest);
            }
        }

        if (this->executeFunction->GetHasRestParameter())
        {
            InitializeRestParam(newInstance, prestDest);
        }

        Js::RegSlot envReg = executeFunction->GetEnvReg();
        if (envReg != Js::Constants::NoRegister && envReg < executeFunction->GetConstantCount())
        {
            Assert(this->executeFunction->GetThisRegForEventHandler() == Constants::NoRegister);
            // The correct FD (possibly distinct from the one on the function) is passed in the constant table.
            this->function->SetEnvironment((Js::FrameDisplay*)newInstance->GetNonVarReg(envReg));
        }

        return newInstance;
    }

    template <class Fn>
    void InterpreterStackFrame::Setup::InitializeParams(InterpreterStackFrame * newInstance, Fn callback, Var **pprestDest)
    {
        ArgSlot requiredInParamCount = executeFunction->GetInParamsCount();
        Assert(requiredInParamCount > 1);
        if (this->inSlotsCount >= requiredInParamCount)
        {
            Var * pArg = &newInstance->m_localSlots[executeFunction->GetConstantCount()];
            Var * paGivenSrc = this->inParams + 1;
            ArgSlot paramIndex = 1;
            do
            {
                Var src = *paGivenSrc++;
                callback(src, paramIndex);
                *pArg++ = src;
                paramIndex++;
            }
            while (paramIndex < requiredInParamCount);
            *pprestDest = pArg;
        }
        else
        {
            InitializeParamsAndUndef(newInstance, callback, pprestDest);
        }
    }

    template <class Fn>
    void InterpreterStackFrame::Setup::InitializeParamsAndUndef(InterpreterStackFrame * newInstance, Fn callback, Var **pprestDest)
    {
        Var * pArg = &newInstance->m_localSlots[executeFunction->GetConstantCount()];
        Var * paGivenSrc = this->inParams + 1;
        ArgSlot requiredInParamCount = executeFunction->GetInParamsCount();
        ArgSlot paramIndex = 1;
        while (paramIndex < this->inSlotsCount)
        {
            Var src = *paGivenSrc++;
            callback(src, paramIndex);
            *pArg++ = src;
            paramIndex++;
        }
        Var varUndef = executeFunction->GetScriptContext()->GetLibrary()->GetUndefined();
        do
        {
            callback(varUndef, paramIndex);
            *pArg++ = varUndef;
            paramIndex++;
        }
        while (paramIndex < requiredInParamCount);

        *pprestDest = pArg;
    }

    void InterpreterStackFrame::Setup::InitializeRestParam(InterpreterStackFrame * newInstance, Var *dest)
    {
        Var *src = this->inParams + executeFunction->GetInParamsCount();

        if (this->inSlotsCount > executeFunction->GetInParamsCount())
        {
            // Create the rest array and copy the args directly into the contiguous head segment.
            int excess = this->inSlotsCount - executeFunction->GetInParamsCount();
            *dest = JavascriptArray::OP_NewScArray(excess, executeFunction->GetScriptContext());
            JavascriptArray *array = static_cast<JavascriptArray *>(*dest);
            Var *elements = ((SparseArraySegment<Var>*)array->GetHead())->elements;
            js_memcpy_s(elements, excess * sizeof(Var), src, excess * sizeof(Var));
        }
        else
        {
            // Rest is an empty array when there are no excess parameters.
            *dest = JavascriptArray::OP_NewScArray(0, executeFunction->GetScriptContext());
        }
    }

    FrameDisplay * InterpreterStackFrame::GetEnvForEvalCode()
    {
        FrameDisplay *pScope;
        if (m_functionBody->GetIsStrictMode() && m_functionBody->GetIsGlobalFunc())
        {
            pScope = this->GetLocalFrameDisplay();
        }
        else
        {
            pScope = (FrameDisplay*)this->LdEnv();
        }

        return pScope;
    }

    void InterpreterStackFrame::InitializeClosures()
    {
        FunctionBody *executeFunction = this->function->GetFunctionBody();
        Var environment;

        RegSlot thisRegForEventHandler = executeFunction->GetThisRegForEventHandler();
        if (thisRegForEventHandler != Constants::NoRegister)
        {
            Var varThis = OP_ArgIn0();
            SetReg(thisRegForEventHandler, varThis);
            environment = JavascriptOperators::OP_LdHandlerScope(varThis, GetScriptContext());
            this->SetEnv((FrameDisplay*)environment);
        }
        else
        {
            environment = this->LdEnv();
        }

        RegSlot closureReg = executeFunction->GetLocalClosureReg();
        if (closureReg != Js::Constants::NoRegister)
        {
            Assert(closureReg >= executeFunction->GetConstantCount());
            if (executeFunction->HasScopeObject())
            {
                Js::RegSlot funcExprScopeReg = executeFunction->GetFuncExprScopeReg();
                if (funcExprScopeReg != Constants::NoRegister)
                {
                    // t0 = NewPseudoScope
                    // t1 = LdFrameDisplay t0 env

                    Var funcExprScope = JavascriptOperators::OP_NewPseudoScope(GetScriptContext());
                    SetReg(funcExprScopeReg, funcExprScope);
                    environment = OP_LdFrameDisplay(funcExprScope, environment, GetScriptContext());
                }

                this->NewScopeObject();
            }
            else
            {
                this->NewScopeSlots();
            }
            this->SetNonVarReg(closureReg, nullptr);
        }

        Js::RegSlot frameDisplayReg = executeFunction->GetLocalFrameDisplayReg();
        if (frameDisplayReg != Js::Constants::NoRegister && closureReg != Js::Constants::NoRegister)
        {
            Assert(frameDisplayReg >= executeFunction->GetConstantCount());

            void *argHead = this->GetLocalClosure();
            this->SetLocalFrameDisplay(this->NewFrameDisplay(argHead, environment));

            this->SetNonVarReg(frameDisplayReg, nullptr);
        }

        this->closureInitDone = true;
    }

#ifdef _M_IX86
#ifndef TEMP_DISABLE_ASMJS
    int InterpreterStackFrame::GetAsmJsArgSize(AsmJsCallStackLayout* stack)
    {
        JavascriptFunction * func = stack->functionObject;
        AsmJsFunctionInfo* asmInfo = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        uint argSize = (uint)(asmInfo->GetArgByteSize());
        argSize = ::Math::Align<int32>(argSize, 8);
        // 2 * sizeof(Var) is for functionObject, and another push that DynamicInterpreterThunk does
        return argSize + 2 * sizeof(Var);
    }

    int InterpreterStackFrame::GetDynamicRetType(AsmJsCallStackLayout* stack)
    {
        return GetRetType(stack->functionObject);
    }

    int InterpreterStackFrame::GetRetType(JavascriptFunction* func)
    {
        AsmJsFunctionInfo* asmInfo = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        return asmInfo->GetReturnType().which();
    }

    DWORD InterpreterStackFrame::GetAsmIntDbValOffSet(AsmJsCallStackLayout* stack)
    {
        JavascriptFunction * func = stack->functionObject;
        ScriptContext* scriptContext = func->GetScriptContext();
        return (DWORD)scriptContext + ScriptContext::GetAsmIntDbValOffset();
    }

    DWORD InterpreterStackFrame::GetAsmSimdValOffSet(AsmJsCallStackLayout* stack)
    {
        JavascriptFunction * func = stack->functionObject;
        ScriptContext* scriptContext = func->GetScriptContext();
        return (DWORD)scriptContext + ScriptContext::GetAsmSimdValOffset();
    }

#ifdef ASMJS_PLAT
    /*
                            AsmInterpreterThunk
                            -------------------
        This is the entrypoint for all Asm Interpreter calls (external and internal)
        TODO - Make this a dynamic Interpreter thunk to support ETW
        Functionality:
        1) Prolog
        2) call AsmInterpreter passing the function object
        3) Get The return type
        4) Check for Double or Float return type
        5) If true then retrieve the value stored at a constant offset from the ScriptContext
        6) Get Argument Size for callee cleanup
        7) EpiLog
            a) Retrieve the frame pointer
            b) Store the return address in register (edx)
            c) Clean the arguments based on the arguments size
            d) push the return address back into the stack
    */
    __declspec(naked)
    void InterpreterStackFrame::InterpreterAsmThunk(AsmJsCallStackLayout* layout)
    {
            enum {
                Void       = AsmJsRetType::Void,
                Signed     = AsmJsRetType::Signed,
                Float      = AsmJsRetType::Float,
                Double     = AsmJsRetType::Double,
                Int32x4    = AsmJsRetType::Int32x4,
                Float32x4  = AsmJsRetType::Float32x4,
                Float64x2  = AsmJsRetType::Float64x2
            };

            //Prolog
            __asm
            {
                //Prologue
                push ebp;
                mov ebp, esp;
                push layout;   // push stack layout
                call InterpreterStackFrame::AsmJsInterpreter;
                push eax; // push the return value into the stack
                push layout;
                call InterpreterStackFrame::GetDynamicRetType;
                cmp eax, Void;
                je end;
                cmp eax, Signed;
                je end;
                cmp eax, Float;
                jne skipFloat;
                // float
                push layout;
                call InterpreterStackFrame::GetAsmIntDbValOffSet;
                cvtsd2ss xmm0, [eax];
                jmp end;
            skipFloat:
                cmp eax, Double;
                jne skipDouble;
                // double
                push layout;
                call InterpreterStackFrame::GetAsmIntDbValOffSet;
                movsd xmm0, [eax];
                jmp end;
            skipDouble:
                // simd value
                push layout;
                call InterpreterStackFrame::GetAsmSimdValOffSet;
                movups xmm0, [eax];
           end:
                push layout;
                call InterpreterStackFrame::GetAsmJsArgSize;
                mov ecx, eax;
                pop eax;  // pop the return value from AsmJsInterpreter to eax

                // Epilog, callee cleanup
                mov  esp, ebp;
                pop  ebp;
                // we need to move stack around in order to do callee cleanup
                // unfortunately, we don't really have enough registers to do this cleanly
                //
                // we are rearranging the stack from this:
                // 0x14 caller push scriptArg1
                // 0x10 caller push functionObject
                // 0x0C DynamicInterpreterThunk return address
                // 0x08 DynamicInterpreterThunk push ebp
                // 0x04 DynamicInterpreterThunk push functionObject
                // 0x00 InterpreterAsmThunk return address <- stack pointer
                // to this:
                // 0x14 DynamicInterpreterThunk return address
                // 0x10 DynamicInterpreterThunk push ebp
                // 0x0C InterpreterAsmThunk return address <- stack pointer

                push eax; // save eax
                mov eax, esp;
                add eax, ecx;
                add eax, 0xC; // eax will be our stack destination. we need to move backwards because memory might overlap
                mov edx, [esp+0x10];
                mov [eax], edx; // move the dynamic interpreter thunk return location
                sub eax, 0x4;
                mov edx, [esp+0xC];
                mov [eax], edx; // move the dynamic interpreter thunk "push ebp" location
                // skip "push functionObject"
                sub eax, 0x4;
                mov edx, [esp+0x4];
                mov [eax], edx; // move the return location
                pop eax;
                add  esp, ecx; // cleanup arguments
                ret;
            }
        }
#endif
#endif
#endif

#if DYNAMIC_INTERPRETER_THUNK
#ifdef _M_IX86
    __declspec(naked)
    Var InterpreterStackFrame::DelayDynamicInterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        __asm
        {
            push ebp
            mov ebp, esp
            push [esp+8]     // push function object
            call InterpreterStackFrame::EnsureDynamicInterpreterThunk;

#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            push eax
            mov  ecx, eax
            call[__guard_check_icall_fptr]
            pop eax
#endif

            pop ebp

            jmp eax
        }
    }
#endif
#endif

#if ENABLE_PROFILE_INFO
    JavascriptMethod InterpreterStackFrame::EnsureDynamicInterpreterThunk(Js::ScriptFunction * function)
    {
#if DYNAMIC_INTERPRETER_THUNK
        Assert(function);

        Js::FunctionBody *functionBody = function->GetFunctionBody();
        JavascriptMethod entrypoint = functionBody->EnsureDynamicInterpreterThunk(function->GetFunctionEntryPointInfo());
        Assert(!IsDelayDynamicInterpreterThunk(functionBody->GetDirectEntryPoint(function->GetEntryPointInfo())));
        if (function->GetEntryPoint() == InterpreterStackFrame::DelayDynamicInterpreterThunk)
        {
            // If we are not profiling, or the function object is not cross site, this is the direct entry point.
            // Change the entry point on the object
            Assert(functionBody->GetDirectEntryPoint(function->GetEntryPointInfo()) == entrypoint);
            function->ChangeEntryPoint(function->GetEntryPointInfo(), entrypoint);
        }
        // Return the original entry point to be called
        return entrypoint;
#else
        return function->GetEntryPoint();
#endif
    }
#endif

    bool InterpreterStackFrame::IsDelayDynamicInterpreterThunk(void * entryPoint)
    {
        return
#if DYNAMIC_INTERPRETER_THUNK
#if _M_X64
            entryPoint == InterpreterStackFrame::AsmJsDelayDynamicInterpreterThunk ||
#endif
            entryPoint == InterpreterStackFrame::DelayDynamicInterpreterThunk;
#else
            false;
#endif
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    __declspec(thread) int InterpreterThunkStackCountTracker::s_count = 0;
#endif

#if DYNAMIC_INTERPRETER_THUNK
    Var InterpreterStackFrame::InterpreterThunk(JavascriptCallStackLayout* layout)
    {
        Js::ScriptFunction * function = Js::ScriptFunction::FromVar(layout->functionObject);
        Js::ArgumentReader args(&layout->callInfo, layout->args);
        void* localReturnAddress = _ReturnAddress();
        void* localAddressOfReturnAddress = _AddressOfReturnAddress();
        return InterpreterHelper(function, args, localReturnAddress, localAddressOfReturnAddress);
    }
#else
#pragma optimize("", off)
    Var InterpreterStackFrame::InterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        void* localReturnAddress = _ReturnAddress();
        void* localAddressOfReturnAddress = _AddressOfReturnAddress();
        Assert(ScriptFunction::Is(function));
        return InterpreterHelper(ScriptFunction::FromVar(function), args, localReturnAddress, localAddressOfReturnAddress);
    }
#pragma optimize("", on)
#endif

    Var InterpreterStackFrame::InterpreterHelper(ScriptFunction* function, ArgumentReader args, void* returnAddress, void* addressOfReturnAddress, const bool isAsmJs)
    {

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        // Support for simulating partially initialized interpreter stack frame.
        InterpreterThunkStackCountTracker tracker;

        if (CONFIG_ISENABLED(InjectPartiallyInitializedInterpreterFrameErrorFlag) &&
            CONFIG_FLAG(InjectPartiallyInitializedInterpreterFrameError) == InterpreterThunkStackCountTracker::GetCount())
        {
            switch (CONFIG_FLAG(InjectPartiallyInitializedInterpreterFrameErrorType))
            {
            case 0:
                DebugBreak();
                break;
            case 1:
                Js::JavascriptError::MapAndThrowError(function->GetScriptContext(), VBSERR_InternalError);
                break;
            default:
                DebugBreak();
            }
        }
#endif
        ScriptContext* functionScriptContext = function->GetScriptContext();
        ThreadContext * threadContext = functionScriptContext->GetThreadContext();
        Assert(!threadContext->IsDisableImplicitException());
        functionScriptContext->VerifyAlive(!function->IsExternal());
        Assert(threadContext->IsScriptActive());
        Assert(threadContext->IsInScript());

        FunctionBody* executeFunction = JavascriptFunction::FromVar(function)->GetFunctionBody();
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (!isAsmJs && executeFunction->IsByteCodeDebugMode() != functionScriptContext->IsInDebugMode()) // debug mode mismatch
        {
            if (executeFunction->GetUtf8SourceInfo()->GetIsLibraryCode())
            {
                Assert(!executeFunction->IsByteCodeDebugMode()); // Library script byteCode is never in debug mode
            }
            else
            {
                Throw::FatalInternalError();
            }
        }
#endif

        if (executeFunction->interpretedCount == 0)
        {
            executeFunction->TraceInterpreterExecutionMode();
        }


        class AutoRestore
        {
        private:
            ThreadContext *const threadContext;
            const uint8 savedLoopDepth;

        public:
            AutoRestore(ThreadContext *const threadContext, FunctionBody *const executeFunction)
                : threadContext(threadContext),
                savedLoopDepth(threadContext->LoopDepth())
            {
                if (savedLoopDepth != 0 && !executeFunction->GetIsAsmJsFunction())
                {
                    executeFunction->SetWasCalledFromLoop();
                }
            }

            ~AutoRestore()
            {
                threadContext->SetLoopDepth(savedLoopDepth);
            }
        } autoRestore(threadContext, executeFunction);

#if ENABLE_PROFILE_INFO
        DynamicProfileInfo * dynamicProfileInfo = nullptr;
        const bool doProfile = executeFunction->GetInterpreterExecutionMode(false) == ExecutionMode::ProfilingInterpreter ||
                               functionScriptContext->IsInDebugMode() && DynamicProfileInfo::IsEnabled(executeFunction);
        if (doProfile)
        {
#if !DYNAMIC_INTERPRETER_THUNK
            executeFunction->EnsureDynamicProfileInfo();
#endif
            dynamicProfileInfo = executeFunction->GetDynamicProfileInfo();
            threadContext->ClearImplicitCallFlags();
        }
#else
        const bool doProfile = false;
#endif

        executeFunction->interpretedCount++;
#ifdef BGJIT_STATS
        functionScriptContext->interpretedCount++;
        functionScriptContext->maxFuncInterpret = max(functionScriptContext->maxFuncInterpret, executeFunction->interpretedCount);
#endif

        AssertMsg(!executeFunction->IsDeferredParseFunction(),
            "Non-intrinsic functions must provide byte-code to execute");

        bool fReleaseAlloc = false;
        InterpreterStackFrame* newInstance = nullptr;
        Var* allocation = nullptr;

        if (!isAsmJs && executeFunction->IsGenerator())
        {
            // If the FunctionBody is a generator then this call is being made by one of the three
            // generator resuming methods: next(), throw(), or return().  They all pass the generator
            // object as the first of two arguments.  The real user arguments are obtained from the
            // generator object.  The second argument is the ResumeYieldData which is only needed
            // when resuming a generator and so it only used here if a frame already exists on the
            // generator object.
            AssertMsg(args.Info.Count == 2, "Generator ScriptFunctions should only be invoked by generator APIs with the pair of arguments they pass in -- the generator object and a ResumeYieldData pointer");
            JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
            newInstance = generator->GetFrame();

            if (newInstance != nullptr)
            {
                ResumeYieldData* resumeYieldData = static_cast<ResumeYieldData*>(args[1]);
                newInstance->SetNonVarReg(executeFunction->GetYieldRegister(), resumeYieldData);

                // The debugger relies on comparing stack addresses of frames to decide when a step_out is complete so
                // give the InterpreterStackFrame a legit enough stack address to make this comparison work.
                newInstance->m_stackAddress = reinterpret_cast<DWORD_PTR>(&generator);
            }
            else
            {
                //
                // Allocate a new InterpreterStackFrame instance on the recycler heap.
                // It will live with the JavascriptGenerator object.
                //
                Arguments generatorArgs = generator->GetArguments();
                InterpreterStackFrame::Setup setup(function, generatorArgs);
                size_t varAllocCount = setup.GetAllocationVarCount();
                size_t varSizeInBytes = varAllocCount * sizeof(Var);
                DWORD_PTR stackAddr = reinterpret_cast<DWORD_PTR>(&generator); // as mentioned above, use any stack address from this frame to ensure correct debugging functionality
                Var loopHeaderArray = executeFunction->GetHasAllocatedLoopHeaders() ? executeFunction->GetLoopHeaderArrayPtr() : nullptr;

                allocation = RecyclerNewPlus(functionScriptContext->GetRecycler(), varSizeInBytes, Var);
                AnalysisAssert(allocation);
#if DBG
                // Allocate invalidVar on GC instead of stack since this InterpreterStackFrame will out live the current real frame
                Js::RecyclableObject* invalidVar = (Js::RecyclableObject*)RecyclerNewPlusLeaf(functionScriptContext->GetRecycler(), sizeof(Js::RecyclableObject), Var);
                AnalysisAssert(invalidVar);
                memset(invalidVar, 0xFE, sizeof(Js::RecyclableObject));
                newInstance = setup.InitializeAllocation(allocation, executeFunction->GetHasImplicitArgIns(), doProfile, loopHeaderArray, stackAddr, invalidVar);
#else
                newInstance = setup.InitializeAllocation(allocation, executeFunction->GetHasImplicitArgIns(), doProfile, loopHeaderArray, stackAddr);
#endif

                newInstance->m_reader.Create(executeFunction);

                generator->SetFrame(newInstance);
            }
        }
        else
        {
            InterpreterStackFrame::Setup setup(function, args);
            size_t varAllocCount = setup.GetAllocationVarCount();
            size_t varSizeInBytes = varAllocCount * sizeof(Var);

            //
            // Allocate a new InterpreterStackFrame instance on the interpreter's virtual stack.
            //
            DWORD_PTR stackAddr;

            // If the locals area exceeds a certain limit, allocate it from a private arena rather than
            // this frame. The current limit is based on an old assert on the number of locals we would allow here.
            if (varAllocCount > InterpreterStackFrame::LocalsThreshold)
            {
                ArenaAllocator *tmpAlloc = nullptr;
                fReleaseAlloc = functionScriptContext->EnsureInterpreterArena(&tmpAlloc);
                allocation = (Var*)tmpAlloc->Alloc(varSizeInBytes);
                stackAddr = reinterpret_cast<DWORD_PTR>(&allocation); // use a stack address so the debugger stepping logic works (step-out, for example, compares stack depths to determine when to complete the step)
            }
            else
            {
                PROBE_STACK_PARTIAL_INITIALIZED_INTERPRETER_FRAME(functionScriptContext, Js::Constants::MinStackInterpreter + varSizeInBytes);
                allocation = (Var*)_alloca(varSizeInBytes);
                stackAddr = reinterpret_cast<DWORD_PTR>(allocation);
            }

            /*
            * If the function has any loop headers, we allocate an array for the loop headers wrappers, and
            * reference the wrappers in the array. We then push the pointer to the array onto the stack itself.
            * We do this so that while the function is being interpreted, we don't want the jitted loop
            * bodies to be collected, even if the loop body isn't being executed. The loop body will
            * get collected when the function has been JITted, and when the function exits the interpreter.
            * The array contains nulls if the loop body isn't jitted (or hasn't been jitted yet) but
            * it's cheaper to just copy them all into the recycler array rather than just the ones that
            * have been jitted.
            */
            Var loopHeaderArray = nullptr;

            if (executeFunction->GetHasAllocatedLoopHeaders())
            {
                // Loop header array is recycler allocated, so we push it on the stack
                // When we scan the stack, we'll recognize it as a recycler allocated
                // object, and mark it's contents and keep the individual loop header
                // wrappers alive
                loopHeaderArray = executeFunction->GetLoopHeaderArrayPtr();
            }

#if DBG
            Js::RecyclableObject * invalidStackVar = (Js::RecyclableObject*)_alloca(sizeof(Js::RecyclableObject));
            memset(invalidStackVar, 0xFE, sizeof(Js::RecyclableObject));
            newInstance = setup.InitializeAllocation(allocation, executeFunction->GetHasImplicitArgIns() && !isAsmJs, doProfile, loopHeaderArray, stackAddr, invalidStackVar);
#else
            newInstance = setup.InitializeAllocation(allocation, executeFunction->GetHasImplicitArgIns() && !isAsmJs, doProfile, loopHeaderArray, stackAddr);
#endif

            newInstance->m_reader.Create(executeFunction);
        }
        //
        // Execute the function's byte-code, returning the return-value:
        // - Mark that the function is current executing and may not be modified.
        //

        executeFunction->BeginExecution();

        Var aReturn = nullptr;

        {
            if (!isAsmJs && functionScriptContext->IsInDebugMode())
            {
#if DYNAMIC_INTERPRETER_THUNK
                PushPopFrameHelper pushPopFrameHelper(newInstance, returnAddress, addressOfReturnAddress);
                aReturn = newInstance->DebugProcess();
#else
                aReturn = newInstance->DebugProcessThunk(_ReturnAddress(), _AddressOfReturnAddress());
#endif
            }
            else
            {
#if DYNAMIC_INTERPRETER_THUNK
                PushPopFrameHelper pushPopFrameHelper(newInstance, returnAddress, addressOfReturnAddress);
                aReturn = newInstance->Process();
#else
                aReturn = newInstance->ProcessThunk(_ReturnAddress(), _AddressOfReturnAddress());
#endif
            }
        }

        executeFunction->EndExecution();

        if (fReleaseAlloc)
        {
            functionScriptContext->ReleaseInterpreterArena();
        }

#if ENABLE_PROFILE_INFO
        if (doProfile)
        {
            dynamicProfileInfo->RecordImplicitCallFlags(threadContext->GetImplicitCallFlags());
        }
#endif

        if (isAsmJs)
        {
            return newInstance;
        }
        return aReturn;
    }

#ifndef TEMP_DISABLE_ASMJS
#if _M_IX86
    int InterpreterStackFrame::AsmJsInterpreter(AsmJsCallStackLayout* stack)
    {
        ScriptFunction * function = (ScriptFunction*)stack->functionObject;
        Var* paramsAddr = stack->args;
        int  flags = CallFlags_Value;
        ArgSlot nbArgs = UInt16Math::Add(function->GetFunctionBody()->GetAsmJsFunctionInfo()->GetArgCount(), 1);
        CallInfo callInfo((CallFlags)flags, nbArgs);
        ArgumentReader args(&callInfo, paramsAddr);
        void* returnAddress = _ReturnAddress();
        void* addressOfReturnAddress = _AddressOfReturnAddress();
#if ENABLE_PROFILE_INFO
        function->GetFunctionBody()->EnsureDynamicProfileInfo();
#endif
        InterpreterStackFrame* newInstance = (InterpreterStackFrame*)InterpreterHelper(function, args, returnAddress, addressOfReturnAddress, true);

        //Handle return value
        AsmJsRetType::Which retType = (AsmJsRetType::Which) GetRetType(function);
        int retVal = 0;

        switch (retType)
        {
        case AsmJsRetType::Int32x4:
        case AsmJsRetType::Float32x4:
        case AsmJsRetType::Float64x2:
            if (function->GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {
                function->GetScriptContext()->retAsmSimdVal = newInstance->m_localSimdSlots[0];
                break;
            }
            Assert(UNREACHED);
        // double return
        case AsmJsRetType::Double:
            function->GetScriptContext()->retAsmIntDbVal = newInstance->m_localDoubleSlots[0];
            break;
        // float return
        case AsmJsRetType::Float:
            function->GetScriptContext()->retAsmIntDbVal = (double)newInstance->m_localFloatSlots[0];
            break;
        // signed or void return
        case AsmJsRetType::Signed:
        case AsmJsRetType::Void:
            retVal = newInstance->m_localIntSlots[0];
            break;
        default:
            Assume(false);
        }
        return retVal;
    }

#elif _M_X64

    typedef double(*AsmJsInterpreterDoubleEP)(AsmJsCallStackLayout*, void *);
    typedef float(*AsmJsInterpreterFloatEP)(AsmJsCallStackLayout*, void *);
    typedef int(*AsmJsInterpreterIntEP)(AsmJsCallStackLayout*, void *);

    void * InterpreterStackFrame::GetAsmJsInterpreterEntryPoint(AsmJsCallStackLayout* stack)
    {
        JavascriptFunction * function = stack->functionObject;
        void * entryPoint = nullptr;
        switch (function->GetFunctionBody()->GetAsmJsFunctionInfo()->GetReturnType().which())
        {
        case Js::AsmJsRetType::Double:
        {
            entryPoint = (AsmJsInterpreterDoubleEP)Js::InterpreterStackFrame::AsmJsInterpreter < double > ;
            break;
        }
        case Js::AsmJsRetType::Float:
        {
            entryPoint = (AsmJsInterpreterFloatEP)Js::InterpreterStackFrame::AsmJsInterpreter < float > ;
            break;
        }
        case Js::AsmJsRetType::Signed:
        case Js::AsmJsRetType::Void:
        {
            entryPoint = (AsmJsInterpreterIntEP)Js::InterpreterStackFrame::AsmJsInterpreter < int > ;
            break;
        }
        case Js::AsmJsRetType::Int32x4:
        case Js::AsmJsRetType::Float32x4:
        case Js::AsmJsRetType::Float64x2:
        {
            entryPoint = Js::InterpreterStackFrame::AsmJsInterpreterSimdJs;
            break;
        }
        default:
            Assume(UNREACHED);
        }
        return entryPoint;
    }

    template<>
    int InterpreterStackFrame::GetAsmJsRetVal<int>(InterpreterStackFrame* instance)
    {
        return instance->m_localIntSlots[0];
    }
    template<>
    double InterpreterStackFrame::GetAsmJsRetVal<double>(InterpreterStackFrame* instance)
    {
        return instance->m_localDoubleSlots[0];
    }
    template<>
    float InterpreterStackFrame::GetAsmJsRetVal<float>(InterpreterStackFrame* instance)
    {
        return instance->m_localFloatSlots[0];
    }
    template<>
    X86SIMDValue InterpreterStackFrame::GetAsmJsRetVal<X86SIMDValue>(InterpreterStackFrame* instance)
    {
        return X86SIMDValue::ToX86SIMDValue(instance->m_localSimdSlots[0]);
    }


    template<typename T>
    T InterpreterStackFrame::AsmJsInterpreter(AsmJsCallStackLayout* layout)
    {
        Js::ScriptFunction * function = Js::ScriptFunction::FromVar(layout->functionObject);
        int  flags = CallFlags_Value;
        ArgSlot nbArgs = UInt16Math::Add(function->GetFunctionBody()->GetAsmJsFunctionInfo()->GetArgCount(), 1);

        CallInfo callInfo((CallFlags)flags, nbArgs);
        ArgumentReader args(&callInfo, (Var*)layout->args);
        void* returnAddress = _ReturnAddress();
        void* addressOfReturnAddress = _AddressOfReturnAddress();
        function->GetFunctionBody()->EnsureDynamicProfileInfo();
        InterpreterStackFrame* newInstance = (InterpreterStackFrame*)InterpreterHelper(function, args, returnAddress, addressOfReturnAddress, true);

        return GetAsmJsRetVal<T>(newInstance);
    }

    __m128 InterpreterStackFrame::AsmJsInterpreterSimdJs(AsmJsCallStackLayout* layout)
    {
        return AsmJsInterpreter<X86SIMDValue>(layout).m128_value;
    }
#endif
#endif

    ///----------------------------------------------------------------------------
    ///
    /// InterpreterStackFrame::SetOut()
    ///
    /// SetOut() change the Var value stored in the specified "out parameter"
    /// register.
    ///
    ///----------------------------------------------------------------------------

    inline void InterpreterStackFrame::SetOut(ArgSlot outRegisterID, Var aValue)
    {
        //
        // The "out" parameter slots are located at the end of the local register range, counting
        // forwards.  This results in the "in" parameter slots being disjoint from the rest of the
        // InterpreterStackFrame.
        //  ..., InterpreterStackFrame A, Locals A[], ..., Out A:0, Out A:1, Out A:2, ...
        //       |                               In B:0,  In B:1, ..., InterpreterStackFrame B, Locals B[], ...
        //       (current 'this')                                      |
        //                                                             (new 'this' after call)
        //

        Assert(m_outParams + outRegisterID < m_outSp);
        m_outParams[outRegisterID] = aValue;
    }

    inline void InterpreterStackFrame::SetOut(ArgSlot_OneByte outRegisterID, Var aValue)
    {
        Assert(m_outParams + outRegisterID < m_outSp);
        m_outParams[outRegisterID] = aValue;
    }

    inline void InterpreterStackFrame::OP_SetOutAsmDb( RegSlot outRegisterID, double val )
    {
        Assert( m_outParams + outRegisterID < m_outSp );
        m_outParams[outRegisterID] = JavascriptNumber::New( val, scriptContext );
    }

    inline void InterpreterStackFrame::OP_SetOutAsmInt( RegSlot outRegisterID, int val )
    {
        Assert( m_outParams + outRegisterID < m_outSp );
        m_outParams[outRegisterID] = JavascriptNumber::ToVar( val, scriptContext );
    }

    inline void InterpreterStackFrame::OP_I_SetOutAsmFlt(RegSlot outRegisterID, float val)
    {
        Assert(m_outParams + outRegisterID < m_outSp);
        *(float*)(&(m_outParams[outRegisterID])) = val;
    }

    inline void InterpreterStackFrame::OP_I_SetOutAsmInt(RegSlot outRegisterID, int val)
    {
        Assert(m_outParams + outRegisterID < m_outSp);
        *(int*)(&(m_outParams[outRegisterID])) = val;
    }

    inline void InterpreterStackFrame::OP_I_SetOutAsmDb(RegSlot outRegisterID, double val)
    {
        Assert(m_outParams + outRegisterID < m_outSp);
        *(double*)(&(m_outParams[outRegisterID])) = val;
    }

    inline void InterpreterStackFrame::OP_I_SetOutAsmSimd(RegSlot outRegisterID, AsmJsSIMDValue val)
    {
        Assert(m_outParams + outRegisterID < m_outSp);
        *(AsmJsSIMDValue*)(&(m_outParams[outRegisterID])) = val;
    }

    inline void InterpreterStackFrame::PushOut(Var aValue)
    {
        *m_outSp++ = aValue;
    }

    inline void InterpreterStackFrame::PopOut(ArgSlot argCount)
    {
        m_outSp -= (argCount+1);
        m_outParams = (Var*)*m_outSp;

        AssertMsg(m_localSlots + this->m_functionBody->GetLocalsCount() <= m_outSp &&
                  m_outSp < (m_localSlots + this->m_functionBody->GetLocalsCount() + this->m_functionBody->GetOutParamsDepth()),
                  "out args Stack pointer not in range after Pop");
    }

    void InterpreterStackFrame::ResetOut()
    {
        //
        // Reset the m_outParams and m_outSp
        //
        m_outParams    = m_localSlots + this->m_functionBody->GetLocalsCount();

        m_outSp        = m_outParams;
    }

    __declspec(noinline)
    Var InterpreterStackFrame::DebugProcessThunk(void* returnAddress, void* addressOfReturnAddress)
    {
        PushPopFrameHelper pushPopFrameHelper(this, returnAddress, addressOfReturnAddress);
        return this->DebugProcess();
    }

    //
    // Under debug mode allow the exception to be swallowed and execution to continue
    // if the debugger has specified that behavior.
    //
    Var InterpreterStackFrame::DebugProcess()
    {
        Assert(this->returnAddress != nullptr);
        while (true)
        {
            JavascriptExceptionObject *exception = nullptr;
            try
            {
                return this->ProcessWithDebugging();
            }
            catch (JavascriptExceptionObject *exception_)
            {
                Assert(exception_);
                exception = exception_;
            }

            if (exception)
            {
                bool skipException = false;
                if (exception != scriptContext->GetThreadContext()->GetPendingSOErrorObject()
                    && exception != scriptContext->GetThreadContext()->GetPendingOOMErrorObject())
                {
                    skipException = exception->IsDebuggerSkip();
                }
                if (skipException)
                {
                    // If we are going to swallow the exception then advance to the beginning of the next user statement
                    if (exception->IsIgnoreAdvanceToNextStatement()
                        || this->scriptContext->GetDebugContext()->GetProbeContainer()->AdvanceToNextUserStatement(this->m_functionBody, &this->m_reader))
                    {
                        // We must fix up the return value to at least be undefined:
                        this->SetReg((RegSlot)0,this->scriptContext->GetLibrary()->GetUndefined());

                        // If we recover from the exception, there may be a chance the out pointers in the InterpreterStackframe are not in a proper state.
                        // Reset them to correct the stack.
                        ResetOut();

                        // If we can successfully advance then continuing processing
                        continue;
                    }
                }

                exception = exception->CloneIfStaticExceptionObject(scriptContext);
                throw exception;
            }
        }
    }

    template<>
    OpCode InterpreterStackFrame::ReadByteOp<OpCode>(const byte *& ip
#if DBG_DUMP
        , bool isExtended /*= false*/
#endif
        )
    {
#if DBG || DBG_DUMP
        //
        // For debugging byte-code, store the current offset before the instruction is read:
        // - We convert this to "void *" to encourage the debugger to always display in hex,
        //   which matches the displayed offsets used by ByteCodeDumper.
        //
        this->DEBUG_currentByteOffset = (void *) m_reader.GetCurrentOffset();
#endif
        OpCode op = ByteCodeReader::ReadByteOp(ip);
#if DBG_DUMP

        this->scriptContext->byteCodeHistogram[(int)op]++;
        if (PHASE_TRACE(Js::InterpreterPhase, this->m_functionBody))
        {
            Output::Print(L"%d.%d:Executing %s at offset 0x%X\n", this->m_functionBody->GetSourceContextId(), this->m_functionBody->GetLocalFunctionId(), Js::OpCodeUtil::GetOpCodeName((Js::OpCode)(op+((int)isExtended<<8))), DEBUG_currentByteOffset);
        }
#endif
        return op;
    }

#ifndef TEMP_DISABLE_ASMJS
    template<>
    OpCodeAsmJs InterpreterStackFrame::ReadByteOp<OpCodeAsmJs>(const byte *& ip
#if DBG_DUMP
        , bool isExtended /*= false*/
#endif
        )
    {
#if DBG || DBG_DUMP
        //
        // For debugging byte-code, store the current offset before the instruction is read:
        // - We convert this to "void *" to encourage the debugger to always display in hex,
        //   which matches the displayed offsets used by ByteCodeDumper.
        //
        this->DEBUG_currentByteOffset = (void *) m_reader.GetCurrentOffset();
#endif
        OpCodeAsmJs op = (OpCodeAsmJs)ByteCodeReader::ReadByteOp(ip);
#if DBG_DUMP
        if (PHASE_TRACE(Js::AsmjsInterpreterPhase, this->m_functionBody))
        {
            Output::Print(L"%d.%d:Executing %s at offset 0x%X\n", this->m_functionBody->GetSourceContextId(), this->m_functionBody->GetLocalFunctionId(), Js::OpCodeUtilAsmJs::GetOpCodeName((Js::OpCodeAsmJs)(op+((int)isExtended<<8))), DEBUG_currentByteOffset);
        }
#endif
        return op;
    }
#endif

    __declspec(noinline)
    Var InterpreterStackFrame::ProcessThunk(void* address, void* addressOfReturnAddress)
    {
        PushPopFrameHelper pushPopFrameHelper(this, address, addressOfReturnAddress);
        return this->Process();
    }

    template<> uint32 InterpreterStackFrame::LogSizeOf<uint8>(){return 0;}
    template<> uint32 InterpreterStackFrame::LogSizeOf<int8>(){return 0;}
    template<> uint32 InterpreterStackFrame::LogSizeOf<uint16>(){return 1;}
    template<> uint32 InterpreterStackFrame::LogSizeOf<int16>(){return 1;}
    template<> uint32 InterpreterStackFrame::LogSizeOf<uint32>(){return 2;}
    template<> uint32 InterpreterStackFrame::LogSizeOf<int32>(){return 2;}
    template<> uint32 InterpreterStackFrame::LogSizeOf<float>(){return 2;}
    template<> uint32 InterpreterStackFrame::LogSizeOf<double>(){return 3;}

    Var InterpreterStackFrame::ProcessAsmJsModule()
    {
#ifdef ASMJS_PLAT
        Js::FunctionBody* asmJsModuleFunctionBody = GetFunctionBody();
        AsmJsModuleInfo* info = asmJsModuleFunctionBody->GetAsmJsModuleInfo();

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Configuration::Global.flags.ForceAsmJsLinkFail)
        {
            AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Forcing link failure");
            return this->ProcessLinkFailedAsmJsModule();
        }
#endif
        if( m_inSlotsCount != info->GetArgInCount() + 1 )
        {
            // Error reparse without asm.js
            AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Invalid module argument count");
            return this->ProcessLinkFailedAsmJsModule();
        }

        const AsmJsModuleMemory& moduleMemory = info->GetModuleMemory();
        Var* moduleMemoryPtr = RecyclerNewArray( scriptContext->GetRecycler(), Var, moduleMemory.mMemorySize );
        Var* arrayBufferPtr = moduleMemoryPtr + moduleMemory.mArrayBufferOffset;
        Assert(moduleMemory.mArrayBufferOffset == AsmJsModuleMemory::MemoryTableBeginOffset);
        Var* stdLibPtr = moduleMemoryPtr + moduleMemory.mStdLibOffset;
        int* localIntSlots        = (int*)(moduleMemoryPtr + moduleMemory.mIntOffset);
        float* localFloatSlots = (float*)(moduleMemoryPtr + moduleMemory.mFloatOffset);
        double* localDoubleSlots = (double*)(moduleMemoryPtr + moduleMemory.mDoubleOffset);
        Var* localFunctionImports = moduleMemoryPtr + moduleMemory.mFFIOffset ;
        Var* localModuleFunctions = moduleMemoryPtr + moduleMemory.mFuncOffset ;
        Var** localFunctionTables = (Var**)(moduleMemoryPtr + moduleMemory.mFuncPtrOffset) ;

        AsmJsSIMDValue* localSimdSlots = nullptr;
        if (scriptContext->GetConfig()->IsSimdjsEnabled())
        {
            localSimdSlots = ((AsmJsSIMDValue*)moduleMemoryPtr) + moduleMemory.mSimdOffset; // simdOffset is in SIMDValues
        }
#if 0
        // Align SIMD regs to 128 bits.
        // We only have space to align if there are any SIMD variables. Otherwise, leave unaligned.
        if (info->GetSimdRegCount())
        {
            AssertMsg((moduleMemory.mMemorySize / SIMD_SLOTS_SPACE) - moduleMemory.mSimdOffset >= 1, "Not enough space in module memory to align SIMD vars");
            localSimdSlots = (AsmJsSIMDValue*)::Math::Align<int>((int)localSimdSlots, sizeof(AsmJsSIMDValue));
        }
#endif

        ThreadContext* threadContext = this->scriptContext->GetThreadContext();
        *stdLibPtr = (m_inSlotsCount > 1) ? m_inParams[1] : nullptr;

        Var foreign = (m_inSlotsCount > 2) ? m_inParams[2] : nullptr;
        *arrayBufferPtr = (m_inSlotsCount > 3) ? m_inParams[3] : nullptr;
        //cache the current state of the disable implicit call flag
        DisableImplicitFlags prevDisableImplicitFlags = threadContext->GetDisableImplicitFlags();
        ImplicitCallFlags saveImplicitcallFlags = threadContext->GetImplicitCallFlags();
        // Disable implicit calls to check if any of the VarImport or Function Import leads to implicit calls
        threadContext->DisableImplicitCall();
        threadContext->SetImplicitCallFlags(ImplicitCallFlags::ImplicitCall_None);
        bool checkParamResult = ASMLink::CheckParams(this->scriptContext, info, *stdLibPtr, foreign, *arrayBufferPtr);
        if (!checkParamResult)
        {
            // don't need to print, because checkParams will do it for us
            goto linkFailure;
        }
        else if(this->CheckAndResetImplicitCall(prevDisableImplicitFlags, saveImplicitcallFlags))
        {
            AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Params have side effects");
             return this->ProcessLinkFailedAsmJsModule();
        }
        // Initialize Variables
        for (int i = 0; i < info->GetVarCount(); i++)
        {
            const auto& var = info->GetVar( i );
            const AsmJsVarType type(var.type);
            if(type.isInt() )
            {
                localIntSlots[var.location] = var.initialiser.intInit;
            }
            else if (type.isFloat())
            {
                localFloatSlots[var.location] = var.initialiser.floatInit;
            }
            else if (type.isDouble())
            {
                localDoubleSlots[var.location] = var.initialiser.doubleInit;
            }
            else if (scriptContext->GetConfig()->IsSimdjsEnabled() && type.isSIMD())
            {
                // e.g. var g = f4(0.0, 0.0, 0.0, 0.0);
                localSimdSlots[var.location] = var.initialiser.simdInit;
            }
            else {
                Assert(UNREACHED);
            }
        }

        // Load constant variables
        for( int i = 0; i < info->GetVarImportCount(); i++ )
        {
            const auto& import = info->GetVarImport( i );
            const AsmJsVarType type(import.type);
            // this might throw, but it would anyway in non-asm.js
            Var value = JavascriptOperators::OP_GetProperty( foreign, import.field, scriptContext );
            // check if there is implicit call and if there is implicit call then clear the disableimplicitcall flag
            if (this->CheckAndResetImplicitCall(prevDisableImplicitFlags, saveImplicitcallFlags))
            {
                AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Accessing var import %s has side effects", this->scriptContext->GetPropertyName(import.field)->GetBuffer());
                return this->ProcessLinkFailedAsmJsModule();
            }
            if (CONFIG_FLAG(AsmJsEdge))
            {
                // emscripten had a bug which caused this check to fail in some circumstances, so this check fails for some demos
                if (!TaggedNumber::Is(value) && (!RecyclableObject::Is(value) || DynamicType::Is(RecyclableObject::FromVar(value)->GetTypeId())))
                {
                    AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Var import %s must be primitive", this->scriptContext->GetPropertyName(import.field)->GetBuffer());
                    goto linkFailure;
                }
            }

            if(type.isInt() )
            {
                int val = JavascriptMath::ToInt32( value, scriptContext );
                localIntSlots[import.location] = val;
            }
            else if (type.isFloat())
            {
                float val = (float)JavascriptConversion::ToNumber(value, scriptContext);
                localFloatSlots[import.location] = val;
            }
            else if (type.isDouble())
            {
                double val = JavascriptConversion::ToNumber( value, scriptContext );
                localDoubleSlots[import.location] = val;
            }
            else if (scriptContext->GetConfig()->IsSimdjsEnabled() && type.isSIMD())
            {
                // e.g. var g = f4(imports.v);
                bool valid = false;
                AsmJsSIMDValue val;
                val.Zero();
                switch (type.which())
                {
                    case AsmJsVarType::Int32x4:
                        valid = JavascriptSIMDInt32x4::Is(value);
                        val = ((JavascriptSIMDInt32x4*)value)->GetValue();

                        break;
                    case AsmJsVarType::Float32x4:
                        valid = JavascriptSIMDFloat32x4::Is(value);
                        val = ((JavascriptSIMDFloat32x4*)value)->GetValue();
                        break;
                    case AsmJsVarType::Float64x2:
                        valid = JavascriptSIMDFloat64x2::Is(value);
                        val = ((JavascriptSIMDFloat64x2*)value)->GetValue();
                        break;
                    default:
                        Assert(UNREACHED);
                };
                if (!valid)
                {
                    // link failure of SIMD values imports.
                    AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Foreign var import %s is not SIMD type", this->scriptContext->GetPropertyName(import.field)->GetBuffer());
                    goto linkFailure;
                }
                localSimdSlots[import.location] = val;
            }

            // check for implicit call after converting to number
            if (this->CheckAndResetImplicitCall(prevDisableImplicitFlags, saveImplicitcallFlags))
            {
                // Runtime error
                AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Accessing var import %s has side effects", this->scriptContext->GetPropertyName(import.field)->GetBuffer());
                return this->ProcessLinkFailedAsmJsModule();
            }
        }
        // Load external functions
        for( int i = 0; i < info->GetFunctionImportCount(); i++ )
        {
            const auto& import = info->GetFunctionImport( i );
            // this might throw, but it would anyway in non-asm.js
            Var importFunc = JavascriptOperators::OP_GetProperty( foreign, import.field, scriptContext );
            // check if there is implicit call and if there is implicit call then clear the disableimplicitcall flag
            if (this->CheckAndResetImplicitCall(prevDisableImplicitFlags, saveImplicitcallFlags))
            {
                AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Accessing foreign function import %s has side effects", this->scriptContext->GetPropertyName(import.field)->GetBuffer());
                return this->ProcessLinkFailedAsmJsModule();
            }
            if( !JavascriptFunction::Is( importFunc ) )
            {
                AsmJSCompiler::OutputError(this->scriptContext, L"Asm.js Runtime Error : Foreign function import %s is not a function", this->scriptContext->GetPropertyName(import.field)->GetBuffer());
                goto linkFailure;
            }
            localFunctionImports[import.location] = importFunc;
        }
        if (*arrayBufferPtr)
        {
            (*(ArrayBuffer**)arrayBufferPtr)->SetIsAsmJsBuffer();
        }
        threadContext->SetDisableImplicitFlags(prevDisableImplicitFlags);
        threadContext->SetImplicitCallFlags(saveImplicitcallFlags);
        FrameDisplay* pDisplay = RecyclerNewPlus(scriptContext->GetRecycler(), sizeof(void*), FrameDisplay, 1);
        pDisplay->SetItem( 0, moduleMemoryPtr );
        for (int i = 0; i < info->GetFunctionCount(); i++)
        {
            const auto& modFunc = info->GetFunction(i);

            // TODO: add more runtime checks here
            auto proxy = m_functionBody->GetNestedFuncReference(i);

            AsmJsScriptFunction* scriptFuncObj = (AsmJsScriptFunction*)ScriptFunction::OP_NewScFunc(pDisplay, (FunctionProxy**)proxy);
            localModuleFunctions[modFunc.location] = scriptFuncObj;
            if (i == 0 && info->GetUsesChangeHeap())
            {
                scriptFuncObj->GetDynamicType()->SetEntryPoint(AsmJsChangeHeapBuffer);
            }
            else
            {
                if (scriptFuncObj->GetDynamicType()->GetEntryPoint() == DefaultDeferredDeserializeThunk)
                {
                    JavascriptFunction::DeferredDeserialize(scriptFuncObj);
                }
                scriptFuncObj->GetDynamicType()->SetEntryPoint(AsmJsExternalEntryPoint);
                scriptFuncObj->GetFunctionBody()->GetAsmJsFunctionInfo()->SetModuleFunctionBody(asmJsModuleFunctionBody);
            }
            scriptFuncObj->SetModuleMemory(moduleMemoryPtr);
            if (!info->IsRuntimeProcessed())
            {
                // don't reset entrypoint upon relinking
                FunctionEntryPointInfo* entypointInfo = (FunctionEntryPointInfo*)scriptFuncObj->GetEntryPointInfo();
                entypointInfo->SetIsAsmJSFunction(true);
                entypointInfo->SetModuleAddress((uintptr_t)moduleMemoryPtr);

#if DYNAMIC_INTERPRETER_THUNK
                if (!PHASE_ON1(AsmJsJITTemplatePhase))
                {
                    entypointInfo->address = AsmJsDefaultEntryThunk;
                }
#endif
            }
        }

        // Initialize function table arrays
        for( int i = 0; i < info->GetFunctionTableCount(); i++ )
        {
            const auto& modFuncTable = info->GetFunctionTable( i );
            Var* funcTableArray = RecyclerNewArray( scriptContext->GetRecycler(), Var, modFuncTable.size );
            for (uint j = 0; j < modFuncTable.size ; j++)
            {
                // get the module function index
                const RegSlot index = modFuncTable.moduleFunctionIndex[j];
                // assign the module function pointer to the array
                Var functionPtr = localModuleFunctions[index];
                funcTableArray[j] = functionPtr;
            }
            localFunctionTables[i] = funcTableArray;
        }
// Do MTJRC/MAIC:0 check
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if ((PHASE_ON1(Js::AsmJsJITTemplatePhase) && CONFIG_FLAG(MaxTemplatizedJitRunCount) == 0) || (!PHASE_ON1(Js::AsmJsJITTemplatePhase) && CONFIG_FLAG(MaxAsmJsInterpreterRunCount) == 0))
        {
            if (PHASE_TRACE1(AsmjsEntryPointInfoPhase))
            {
                Output::Print(L"%s Scheduling For Full JIT at callcount:%d\n", asmJsModuleFunctionBody->GetDisplayName(), 0);
                Output::Flush();
            }
            for (int i = 0; i < info->GetFunctionCount(); i++)
            {
                ScriptFunction* functionObj = (ScriptFunction*)localModuleFunctions[i];
                AnalysisAssert(functionObj != nullptr);
                // don't want to generate code for APIs like changeHeap
                if (functionObj->GetEntryPoint() == Js::AsmJsExternalEntryPoint)
                {
                    GenerateFunction(asmJsModuleFunctionBody->GetScriptContext()->GetNativeCodeGenerator(), functionObj->GetFunctionBody(), functionObj);
                }
            }
        }
#endif

        info->SetIsRuntimeProcessed(true);

        // create export object
        if( info->GetExportsCount() )
        {
            Var newObj = JavascriptOperators::NewScObjectLiteral( GetScriptContext(), info->GetExportsIdArray(),
                                                                  this->GetFunctionBody()->GetObjectLiteralTypeRef( 0 ) );
            for( int i = 0; i < info->GetExportsCount(); i++ )
            {
                auto ex = info->GetExport( i );
                Var func = localModuleFunctions[*ex.location];
                JavascriptOperators::OP_InitProperty( newObj, *ex.id, func );
            }
            SetReg( (RegSlot) 0, newObj );
            return newObj;
        }


        // export only 1 function
        Var exportFunc = localModuleFunctions[info->GetExportFunctionIndex()];
        SetReg((RegSlot)0, exportFunc);
        return exportFunc;

    linkFailure:
        threadContext->SetDisableImplicitFlags(prevDisableImplicitFlags);
        threadContext->SetImplicitCallFlags(saveImplicitcallFlags);
        return this->ProcessLinkFailedAsmJsModule();
    }

    Var InterpreterStackFrame::ProcessLinkFailedAsmJsModule()
    {
        AsmJSCompiler::OutputError(this->scriptContext, L"asm.js linking failed.");

        Js::FunctionBody* asmJsModuleFunctionBody = GetFunctionBody();
        AsmJsModuleInfo* info = asmJsModuleFunctionBody->GetAsmJsModuleInfo();

        // do not support relinking with failed relink
        if (info->IsRuntimeProcessed())
        {
            Js::Throw::OutOfMemory();
        }

        ScriptFunction * funcObj = GetJavascriptFunction();
        ScriptFunction::ReparseAsmJsModule(&funcObj);
        const bool doProfile =
            funcObj->GetFunctionBody()->GetInterpreterExecutionMode(false) == ExecutionMode::ProfilingInterpreter ||
            GetScriptContext()->IsInDebugMode() && DynamicProfileInfo::IsEnabled(funcObj->GetFunctionBody());

        DynamicProfileInfo * dynamicProfileInfo = nullptr;
        if (doProfile)
        {
            dynamicProfileInfo = funcObj->GetFunctionBody()->GetDynamicProfileInfo();
            funcObj->GetScriptContext()->GetThreadContext()->ClearImplicitCallFlags();
        }

        // after reparsing, we want to also use a new interpreter stack frame, as it will have different characteristics than the asm.js version
        InterpreterStackFrame::Setup setup(funcObj, m_inParams, m_inSlotsCount);
        size_t varAllocCount = setup.GetAllocationVarCount();
        size_t varSizeInBytes = varAllocCount * sizeof(Var);

        Var* allocation = nullptr;
        DWORD_PTR stackAddr;
        bool fReleaseAlloc = false;
        if (varAllocCount > InterpreterStackFrame::LocalsThreshold)
        {
            ArenaAllocator *tmpAlloc = nullptr;
            fReleaseAlloc = GetScriptContext()->EnsureInterpreterArena(&tmpAlloc);
            allocation = (Var*)tmpAlloc->Alloc(varSizeInBytes);
            // use a stack address so the debugger stepping logic works (step-out, for example, compares stack depths to determine when to complete the step)
            // debugger stepping does not matter here, but it's worth being consistent with normal stack frame
            stackAddr = reinterpret_cast<DWORD_PTR>(&allocation);
        }
        else
        {
            PROBE_STACK_PARTIAL_INITIALIZED_INTERPRETER_FRAME(GetScriptContext(), Js::Constants::MinStackInterpreter + varSizeInBytes);
            allocation = (Var*)_alloca(varSizeInBytes);
            stackAddr = reinterpret_cast<DWORD_PTR>(allocation);
        }

#if DBG
        Js::RecyclableObject * invalidStackVar = (Js::RecyclableObject*)_alloca(sizeof(Js::RecyclableObject));
        memset(invalidStackVar, 0xFE, sizeof(Js::RecyclableObject));
        InterpreterStackFrame * newInstance = newInstance = setup.InitializeAllocation(allocation, funcObj->GetFunctionBody()->GetHasImplicitArgIns(), doProfile, nullptr, stackAddr, invalidStackVar);
#else
        InterpreterStackFrame * newInstance = newInstance = setup.InitializeAllocation(allocation, funcObj->GetFunctionBody()->GetHasImplicitArgIns(), doProfile, nullptr, stackAddr);
#endif

        newInstance->m_reader.Create(funcObj->GetFunctionBody());
        // now that we have set up the new frame, let's interpret it!
        funcObj->GetFunctionBody()->BeginExecution();
        PushPopFrameHelper(newInstance, _ReturnAddress(), _AddressOfReturnAddress());
        Var retVal = newInstance->ProcessUnprofiled();

        if (doProfile)
        {
            dynamicProfileInfo->RecordImplicitCallFlags(GetScriptContext()->GetThreadContext()->GetImplicitCallFlags());
        }

        if (fReleaseAlloc)
        {
            GetScriptContext()->ReleaseInterpreterArena();
        }

        return retVal;
#else
        Assert(UNREACHED);
        return nullptr;
#endif
    }

#if DBG_DUMP
    int AsmJsCallDepth = 0;
#endif
    void InterpreterStackFrame::PrintStack(const int* const intSrc, const float* const fltSrc, const double* const dblSrc, int intConstCount, int floatConstCount, int doubleConstCount, const wchar_t* state)
    {
        Output::Print(L"\n");
        Output::Print(L"Interpreter Constant Stack Data(%s)\n", state);
        Output::Print(L"***************************************\n");
        Output::Print(L"Int Data\n");
        Output::Print(L"--------\n");
        for (int count = 0; count < intConstCount; count++)
        {
            Output::Print(L"Index:%d Value:%d \n", count, intSrc[count]);
        }
        Output::Print(L"\n");
        Output::Print(L"Float Data\n");
        Output::Print(L"----------\n");
        for (int count = 0; count < floatConstCount; count++)
        {
            Output::Print(L"Index:%d Value:%f \n", count, fltSrc[count]);
        }
        Output::Print(L"\n");
        Output::Print(L"Double Data\n");
        Output::Print(L"-----------\n");
        for (int count = 0; count < doubleConstCount; count++)
        {
            Output::Print(L"Index:%d Value:%g \n", count, dblSrc[count]);
        }
        Output::Print(L"\n");
    }

#ifndef TEMP_DISABLE_ASMJS 
    // Function memory allocation should be done the same way as
    // T AsmJsCommunEntryPoint(Js::ScriptFunction* func, ...)  (AsmJSJitTemplate.cpp)
    // update any changes there
    /*
    This function does the following fixup
    Stack Before                 Stack After
    ==============               ================
    | VarConstants |             |  VarConstants  |
    |--------------|             |-----------------
    | IntConstants |             |  IntConstants  |
    |--------------|             |  ------------  |
    | FloatConst   |             |  Int Vars+Tmps |
    |--------------|             |----------------|
    | DoubleConst  |             |  FloatConst    |
    |--------------|             |  ----------    |
    | Var&Temps    |             |  Flt Vars+tmps |
    |==============|             |----------------|
                                 | DoubleConst    |
                                 |  -----------   |
                                 | Dbl Vars+Tmps  |
                                  ================

    intSrc,FltSrc&DblSrc are pointers to the stack before the change
    m_localIntSlots,m_localFloatSlots,m_localDoubleSlots are pointers to the stack after the change
    */
    void InterpreterStackFrame::AlignMemoryForAsmJs()
    {
        FunctionBody *const functionBody = GetFunctionBody();
        ScriptFunction* func = GetJavascriptFunction();

        //schedule for codegen here only if TJ is collected
        if (!functionBody->GetIsAsmJsFullJitScheduled() && !PHASE_OFF(BackEndPhase, functionBody)
            && !PHASE_OFF(FullJitPhase, functionBody) && !this->scriptContext->GetConfig()->IsNoNative())
        {
            int callCount = ++((FunctionEntryPointInfo*)func->GetEntryPointInfo())->callsCount;
            bool doSchedule = false;
            const int minAsmJsInterpretRunCount = (int)CONFIG_FLAG(MinAsmJsInterpreterRunCount);

            if (callCount >= minAsmJsInterpretRunCount)
            {
                doSchedule = true;
            }
            if (doSchedule && !functionBody->GetIsAsmJsFullJitScheduled())
            {
#if ENABLE_NATIVE_CODEGEN
                if (PHASE_TRACE1(AsmjsEntryPointInfoPhase))
                {
                    Output::Print(L"Scheduling For Full JIT from Interpreter at callcount:%d\n", callCount);
                }
                GenerateFunction(functionBody->GetScriptContext()->GetNativeCodeGenerator(), functionBody, func);
#endif
                functionBody->SetIsAsmJsFullJitScheduled(true);
            }
        }
        AsmJsFunctionInfo* info = functionBody->GetAsmJsFunctionInfo();
        const int intConstCount = info->GetIntConstCount();
        const int doubleConstCount = info->GetDoubleConstCount();
        const int floatConstCount = info->GetFloatConstCount();

        const int simdConstCount = info->GetSimdConstCount();

        // Offset of doubles from (double*)m_localSlot
        const int intOffset = info->GetIntByteOffset() / sizeof(int);
        const int doubleOffset = info->GetDoubleByteOffset() / sizeof(double);
        const int floatOffset = info->GetFloatByteOffset() / sizeof(float);

        const int simdByteOffset = info->GetSimdByteOffset();// in bytes;


        int* intSrc = (int*)(m_localSlots + AsmJsFunctionMemory::RequiredVarConstants);

        // Where all int value starts
        m_localIntSlots = ((int*)m_localSlots) + intOffset;
        // where int arguments starts
        // int* intArgDst = m_localIntSlots + intConstCount;

        // Where float constants currently are
        float* floatSrc = (float*)(intSrc + intConstCount);
        // where all float value starts with the new layout
        m_localFloatSlots = ((float*)m_localSlots) + floatOffset;

        // Where double arguments starts
        // float* floatArgDst = m_localFloatSlots + floatConstCount;

        // Where double constants currently are
        double* doubleSrc = (double*)(floatSrc + floatConstCount);

        // where all double value starts
        m_localDoubleSlots = ((double*)m_localSlots) + doubleOffset;
        // Where double arguments starts
        // double* doubleArgDst = m_localDoubleSlots + doubleConstCount;


        AsmJsSIMDValue* simdSrc = nullptr;
        if (scriptContext->GetConfig()->IsSimdjsEnabled())
        {
            simdSrc = (AsmJsSIMDValue*)(doubleSrc + doubleConstCount);
            m_localSimdSlots = (AsmJsSIMDValue*)((char*)m_localSlots + simdByteOffset);
        }

        // Load module environment
        FrameDisplay* frame = this->function->GetEnvironment();
        m_localSlots[AsmJsFunctionMemory::ModuleEnvRegister] = frame->GetItem(0);
        m_localSlots[AsmJsFunctionMemory::ArrayBufferRegister] = (Var*)frame->GetItem(0) + AsmJsModuleMemory::MemoryTableBeginOffset;
        m_localSlots[AsmJsFunctionMemory::ArraySizeRegister] = 0; // do not cache ArraySize in the interpreter
        m_localSlots[AsmJsFunctionMemory::ScriptContextBufferRegister] = functionBody->GetScriptContext();

        if (PHASE_TRACE1(AsmjsInterpreterStackPhase))
        {
            PrintStack(intSrc, floatSrc, doubleSrc, intConstCount, floatConstCount, doubleConstCount, L"Before Shuffling");
        }

        // Copying has to happen in that order in order not to overwrite constants
        if (scriptContext->GetConfig()->IsSimdjsEnabled())
        {
            memcpy_s(m_localSimdSlots, simdConstCount*sizeof(AsmJsSIMDValue), simdSrc, simdConstCount*sizeof(AsmJsSIMDValue));
        }

        // Moving the double and floats  to their slot position. We must move the doubles first so that we do not overwrite the doubles stack with floats
        memcpy_s(m_localDoubleSlots, doubleConstCount*sizeof(double), doubleSrc, doubleConstCount*sizeof(double));
        memcpy_s(m_localFloatSlots, floatConstCount*sizeof(float), floatSrc, floatConstCount*sizeof(float));

        if (PHASE_TRACE1(AsmjsInterpreterStackPhase))
        {
            PrintStack(m_localIntSlots, m_localFloatSlots, m_localDoubleSlots, intConstCount, floatConstCount, doubleConstCount, L"After Shuffling");
        }

        int* intArg;
        double* doubleArg;
        float* floatArg;

        intArg = m_localIntSlots + intConstCount;
        doubleArg = m_localDoubleSlots + doubleConstCount;
        floatArg = m_localFloatSlots + floatConstCount;

        AsmJsSIMDValue* simdArg = m_localSimdSlots + simdConstCount;
        // Move the arguments to the right location
        ArgSlot argCount = info->GetArgCount();

#if _M_X64
        uint homingAreaSize = 0;
#endif

        uintptr argAddress = (uintptr)m_inParams;
        for (ArgSlot i = 0; i < argCount; i++)
        {
#if _M_X64
            // 3rd Argument should be at the end of the homing area.
            Assert(i != 3 || argAddress == (uintptr)m_inParams + homingAreaSize);
            if (i < 3)
            {
                // for x64 we spill the first 3 floating point args below the rest of the arguments on the stack
                // m_inParams will be from DynamicInterpreterThunk's frame. Floats are in InterpreterAsmThunk's frame. Stack will be set up like so:
                // DIT arg2 <- first scriptArg, m_inParams points here
                // DIT arg1
                // padding
                // IAT r9 home
                // IAT r8 home
                // IAT rdx home
                // IAT rcx home
                // IAT return address
                // IAT push rbp
                // IAT padding
                // IAT xmm3 spill
                // IAT xmm2 spill
                // IAT xmm1 spill <- floatSpillAddress for arg1

                // floats are spilled as xmmwords
                uintptr floatSpillAddress = (uintptr)m_inParams - MachPtr * (15 - 2*i);

                if (info->GetArgType(i).isInt())
                {
                    *intArg = *(int*)argAddress;
                    ++intArg;
                    homingAreaSize += MachPtr;
                }
                else if (info->GetArgType(i).isFloat())
                {
                    *floatArg = *(float*)floatSpillAddress;
                    ++floatArg;
                    homingAreaSize += MachPtr;
                }
                else if (info->GetArgType(i).isDouble())
                {
                    *doubleArg = *(double*)floatSpillAddress;
                    ++doubleArg;
                    homingAreaSize += MachPtr;
                }
                else
                {
                    Assert(info->GetArgType(i).isSIMD());
                    *simdArg = *(AsmJsSIMDValue*)floatSpillAddress;
                    ++simdArg;
                    homingAreaSize += sizeof(AsmJsSIMDValue);
                }
                if (scriptContext->GetConfig()->IsSimdjsEnabled() && i == 2) // last argument ?
                {
                    // If we have simd arguments, the homing area in m_inParams can be larger than 3 64-bit slots. This is because SIMD values are unboxed there too.
                    // After unboxing, the homing area is overwritten by rdx, r8 and r9, and we read/skip 64-bit slots from the homing area (argAddress += MachPtr).
                    // After the last argument of the 3 is read, we need to advance argAddress to skip over the possible extra space and to the start of the rest of the arguments.
                    argAddress = (uintptr)m_inParams + homingAreaSize;
                }
                else
                {
                    argAddress += MachPtr;
                }
            }
            else
#endif
            if (info->GetArgType(i).isInt())
            {
                *intArg = *(int*)argAddress;
                ++intArg;
                argAddress += MachPtr;
            }
            else if (info->GetArgType(i).isFloat())
            {
                *floatArg = *(float*)argAddress;
                ++floatArg;
                argAddress += MachPtr;
            }
            else if (info->GetArgType(i).isDouble())
            {
                Assert(info->GetArgType(i).isDouble());
                *doubleArg = *(double*)argAddress;
                ++doubleArg;
                argAddress += sizeof(double);
            }
            else if (scriptContext->GetConfig()->IsSimdjsEnabled() && info->GetArgType(i).isSIMD())
            {
                *simdArg = *(AsmJsSIMDValue*)argAddress;
                ++simdArg;
                argAddress += sizeof(AsmJsSIMDValue);
            }
            else
            {
                AssertMsg(UNREACHED, "Invalid function arg type.");
            }
        }

#if DBG_DUMP
        const bool tracingFunc = PHASE_TRACE( AsmjsFunctionEntryPhase, functionBody );
        if( tracingFunc )
        {
            if( AsmJsCallDepth )
            {
                Output::Print( L"%*c", AsmJsCallDepth,' ');
            }
            Output::Print( L"Executing function %s", functionBody->GetDisplayName());
            ++AsmJsCallDepth;
        }
#endif

#if DBG_DUMP
        if (tracingFunc)
        {
            Output::Print(L"){\n");
        }
#endif
        if( info->GetReturnType() == AsmJsRetType::Void )
        {
            m_localSlots[0] = JavascriptOperators::OP_LdUndef( scriptContext );
        }
    }
#endif

    ///----------------------------------------------------------------------------
    ///
    /// InterpreterStackFrame::Process
    ///
    /// Process() processes a single loop of execution for the current
    /// JavascriptFunction being executed:
    /// - Individual instructions are dispatched to specific handlers for different
    ///   OpCodes.
    ///
    ///----------------------------------------------------------------------------

#if ENABLE_PROFILE_INFO
#define INTERPRETERLOOPNAME ProcessProfiled
#define PROVIDE_INTERPRETERPROFILE
#include "Interpreterloop.inl"
#undef PROVIDE_INTERPRETERPROFILE
#undef INTERPRETERLOOPNAME
#endif

#define INTERPRETERLOOPNAME ProcessUnprofiled
#include "Interpreterloop.inl"
#undef INTERPRETERLOOPNAME

#ifndef TEMP_DISABLE_ASMJS
#define INTERPRETERLOOPNAME ProcessAsmJs
#define INTERPRETER_ASMJS
#include "InterpreterProcessOpCodeAsmJs.h"
#include "Interpreterloop.inl"
#undef INTERPRETER_ASMJS
#undef INTERPRETERLOOPNAME
#endif

// For now, always collect profile data when debugging,
// otherwise the backend will be confused if there's no profile data.

#define INTERPRETERLOOPNAME ProcessWithDebugging
#define PROVIDE_DEBUGGING
#if ENABLE_PROFILE_INFO
#define PROVIDE_INTERPRETERPROFILE
#endif
#include "Interpreterloop.inl"
#if ENABLE_PROFILE_INFO
#undef PROVIDE_INTERPRETERPROFILE
#endif
#undef PROVIDE_DEBUGGING
#undef INTERPRETERLOOPNAME

    Var InterpreterStackFrame::Process()
    {
#if ENABLE_PROFILE_INFO
        class AutoRestore
        {
        private:
            InterpreterStackFrame *const interpreterStackFrame;
            const uint32 savedSwitchProfileModeOnLoopEndNumber;
            const bool savedIsAutoProfiling;
            const bool savedSwitchProfileMode;

        public:
            AutoRestore(InterpreterStackFrame *const interpreterStackFrame)
                : interpreterStackFrame(interpreterStackFrame),
                savedIsAutoProfiling(interpreterStackFrame->isAutoProfiling),
                savedSwitchProfileMode(interpreterStackFrame->switchProfileMode),
                savedSwitchProfileModeOnLoopEndNumber(interpreterStackFrame->switchProfileModeOnLoopEndNumber)
            {
            }

            ~AutoRestore()
            {
                interpreterStackFrame->isAutoProfiling = savedIsAutoProfiling;
                interpreterStackFrame->switchProfileMode = savedSwitchProfileMode;
                interpreterStackFrame->switchProfileModeOnLoopEndNumber = savedSwitchProfileModeOnLoopEndNumber;
            }
        } autoRestore(this);
#endif

        if ((m_flags & Js::InterpreterStackFrameFlags_FromBailOut) && !(m_flags & InterpreterStackFrameFlags_ProcessingBailOutFromEHCode))
        {
            if (this->ehBailoutData)
            {
                m_flags |= Js::InterpreterStackFrameFlags_ProcessingBailOutFromEHCode;
                EHBailoutData * topLevelEHBailoutData = this->ehBailoutData;
                while (topLevelEHBailoutData->parent->nestingDepth != -1)
                {
                    topLevelEHBailoutData->parent->child = topLevelEHBailoutData;
                    topLevelEHBailoutData = topLevelEHBailoutData->parent;
                }
                ProcessTryCatchBailout(topLevelEHBailoutData, this->ehBailoutData->nestingDepth);
                m_flags &= ~Js::InterpreterStackFrameFlags_ProcessingBailOutFromEHCode;
                this->ehBailoutData = nullptr;
            }
        }
#ifndef TEMP_DISABLE_ASMJS
        FunctionBody *const functionBody = GetFunctionBody();
        if( functionBody->GetIsAsmjsMode() )
        {
            AsmJsFunctionInfo* asmInfo = functionBody->GetAsmJsFunctionInfo();
            if (asmInfo)
            {
                AlignMemoryForAsmJs();
                Var returnVar = ProcessAsmJs();
#if DBG_DUMP
                if( PHASE_TRACE( AsmjsFunctionEntryPhase, functionBody ) )
                {
                    --AsmJsCallDepth;
                    if( AsmJsCallDepth )
                    {
                        Output::Print( L"%*c}", AsmJsCallDepth, ' ' );
                    }
                    else
                    {
                        Output::Print( L"}" );
                    }
                    switch( asmInfo->GetReturnType().which() )
                    {
                    case AsmJsRetType::Void:
                        break;
                    case AsmJsRetType::Signed:
                        Output::Print( L" = %d", JavascriptMath::ToInt32( returnVar, scriptContext ) );
                        break;
                    case AsmJsRetType::Float:
                    case AsmJsRetType::Double:
                        Output::Print( L" = %.4f", JavascriptConversion::ToNumber( returnVar, scriptContext ) );
                        break;
                    default:
                        break;
                    }
                    Output::Print( L";\n" );
                }
#endif
                return returnVar;
            }
            else
            {
                Assert(functionBody->GetAsmJsModuleInfo());
                return ProcessAsmJsModule();
            }
        }
#endif

#if ENABLE_PROFILE_INFO
        switchProfileMode = false;
        switchProfileModeOnLoopEndNumber = 0u - 1;
#endif

#if ENABLE_PROFILE_INFO
        const ExecutionMode interpreterExecutionMode =
            functionBody->GetInterpreterExecutionMode(!!(GetFlags() & InterpreterStackFrameFlags_FromBailOut));
        if(interpreterExecutionMode == ExecutionMode::ProfilingInterpreter)
        {
            isAutoProfiling = false;
            return ProcessProfiled();
        }

        Assert(
            interpreterExecutionMode == ExecutionMode::Interpreter ||
            interpreterExecutionMode == ExecutionMode::AutoProfilingInterpreter);
        isAutoProfiling = interpreterExecutionMode == ExecutionMode::AutoProfilingInterpreter;

        Var result;
        while(true)
        {
            Assert(!switchProfileMode);
            result = ProcessUnprofiled();
            Assert(!(switchProfileMode && result));
            if(switchProfileMode)
            {
                switchProfileMode = false;
            }
            else
            {
                break;
            }
            Assert(isAutoProfiling);

        #if DBG_DUMP

            if(PHASE_TRACE(InterpreterAutoProfilePhase, functionBody))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"InterpreterAutoProfile - Func %s - Started profiling\n", functionBody->GetDebugNumberSet(debugStringBuffer));
                Output::Flush();
            }
        #endif

            Assert(!switchProfileMode);
            result = ProcessProfiled();
            Assert(!(switchProfileMode && result));
            if(switchProfileMode)
            {
                switchProfileMode = false;
            }
            else
            {
                break;
            }
            Assert(isAutoProfiling);

        #if DBG_DUMP
            if(PHASE_TRACE(InterpreterAutoProfilePhase, functionBody))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"InterpreterAutoProfile - Func %s - Stopped profiling\n", functionBody->GetDebugNumberSet(debugStringBuffer));
                Output::Flush();
            }
        #endif
        }
        return result;
#else
        return ProcessUnprofiled();
#endif
    }



    template <class T>
    void InterpreterStackFrame::OP_GetMethodProperty(unaligned T *playout)
    {
        Var varInstance = GetReg(playout->Instance);
        OP_GetMethodProperty(varInstance, playout);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetLocalMethodProperty(unaligned T *playout)
    {
        OP_GetMethodProperty(this->localClosure, playout);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetMethodProperty(Var varInstance, unaligned T *playout)
    {
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(varInstance);
#endif
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        RecyclableObject* obj = NULL;
        if (RecyclableObject::Is(varInstance))
        {
            obj = RecyclableObject::FromVar(varInstance);

            if ((propertyId == PropertyIds::apply || propertyId == PropertyIds::call) && ScriptFunction::Is(obj))
            {
                // If the property being loaded is "apply"/"call", make an optimistic assumption that apply/call is not overridden and
                // undefer the function right here if it was defer parsed before. This is required so that the load of "apply"/"call"
                // happens from the same "type". Otherwise, we will have a polymorphic cache for load of "apply"/"call".
                ScriptFunction *fn = ScriptFunction::FromVar(obj);
                if(fn->GetType()->GetEntryPoint() == JavascriptFunction::DeferredParsingThunk)
                {
                    JavascriptFunction::DeferredParse(&fn);
                }
            }
        }

        InlineCache *inlineCache = this->GetInlineCache(playout->inlineCacheIndex);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
        Var aValue;
        if (obj &&
            CacheOperators::TryGetProperty<true, true, false, false, false, false, true, false, false>(
                obj, false, obj, propertyId, &aValue, GetScriptContext(), nullptr, &info))
        {
            SetReg(playout->Value, aValue);
            return;
        }

        OP_GetMethodProperty_NoFastPath(varInstance, playout);
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::OP_GetMethodProperty_NoFastPath(Var instance, unaligned T *playout)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);

        Var value = JavascriptOperators::PatchGetMethod<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            instance,
            propertyId
        );

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetMethod throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetMethodProperty(instance, propertyId, value, true);
        }
#endif

        SetReg(playout->Value, value);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetRootMethodProperty(unaligned T *playout)
    {
        Assert(playout->inlineCacheIndex >= this->m_functionBody->GetRootObjectLoadInlineCacheStart());
        Js::Var instance = this->GetRootObject();
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        InlineCache *inlineCache = this->GetInlineCache(playout->inlineCacheIndex);
        DynamicObject *obj = DynamicObject::FromVar(instance);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
        Var aValue;
        if (CacheOperators::TryGetProperty<true, true, false, false, false, false, true, false, false>(
                obj, true, obj, propertyId, &aValue, GetScriptContext(), nullptr, &info))
        {
            SetReg(playout->Value, aValue);
            return;
        }

        OP_GetRootMethodProperty_NoFastPath(playout);
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::OP_GetRootMethodProperty_NoFastPath(unaligned T *playout)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);

        Var rootInstance = this->GetRootObject();

        Var value = JavascriptOperators::PatchGetRootMethod<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            DynamicObject::FromVar(rootInstance),
            propertyId
        );

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetMethod throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetMethodProperty(rootInstance, propertyId, value, true);
        }
#endif

        SetReg(playout->Value, value);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetMethodPropertyScoped(unaligned T *playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        Var varInstance = GetReg(playout->Instance);
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);

        RecyclableObject* obj = NULL;
        if (RecyclableObject::Is(varInstance))
        {
            obj = RecyclableObject::FromVar(varInstance);
        }

        InlineCache *inlineCache = this->GetInlineCache(playout->inlineCacheIndex);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
        Var aValue;
        if (obj &&
            CacheOperators::TryGetProperty<true, true, false, false, false, false, true, false, false>(
                obj, false, obj, propertyId, &aValue, GetScriptContext(), nullptr, &info))
        {
            threadContext->CheckAndResetImplicitCallAccessorFlag();
            threadContext->AddImplicitCallFlags(savedImplicitCallFlags);

            SetReg(playout->Value, aValue);
            return;
        }

        OP_GetMethodPropertyScoped_NoFastPath(playout);

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::OP_GetMethodPropertyScoped_NoFastPath(unaligned T *playout)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        Js::Var instance = GetReg(playout->Instance);

        Js::Var value = JavascriptOperators::PatchScopedGetMethod<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            instance,
            propertyId
        );

        SetReg(playout->Value, value);

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetMethod throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetMethodProperty(instance, propertyId, value, true);
        }
#endif

    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetMethodProperty(unaligned T *playout)
    {
        ProfiledGetProperty<T, false, true, false>(playout, GetReg(playout->Instance));
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetLocalMethodProperty(unaligned T *playout)
    {
        ProfiledGetProperty<T, false, true, false>(playout, this->localClosure);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetRootMethodProperty(unaligned T *playout)
    {
        ProfiledGetProperty<T, true, true, false>(playout, GetRootObject());
    }

    RecyclableObject *
    InterpreterStackFrame::OP_CallGetFunc(Var target)
    {
        return JavascriptOperators::GetCallableObjectOrThrow(target, GetScriptContext());
    }

    void InterpreterStackFrame::OP_AsmStartCall( const unaligned OpLayoutStartCall * playout )
    {
        OP_StartCall( playout->ArgCount/sizeof(Var) );
        m_outParams[0] = scriptContext->GetLibrary()->GetUndefined();
    }

    void InterpreterStackFrame::OP_StartCall(const unaligned OpLayoutStartCall * playout)
    {
        OP_StartCall(playout->ArgCount);
    }

    void InterpreterStackFrame::OP_StartCall(uint outParamCount)
    {
         // Save the outParams for the current callsite on the outparam stack
        PushOut(m_outParams);

        // Update outParams for the indicated callsite
        m_outParams = m_outSp;

        m_outSp += outParamCount;

        AssertMsg(m_localSlots + this->m_functionBody->GetLocalsCount() < m_outSp &&
            m_outSp <= (m_localSlots + this->m_functionBody->GetLocalsCount() + this->m_functionBody->GetOutParamsDepth()),
            "out args Stack pointer not in range after Push");

    }

#ifndef TEMP_DISABLE_ASMJS
#if _M_X64
    void InterpreterStackFrame::OP_CallAsmInternal(RecyclableObject * function)
    {
        AsmJsFunctionInfo* asmInfo = ((ScriptFunction*)function)->GetFunctionBody()->GetAsmJsFunctionInfo();
        uint argsSize = asmInfo->GetArgByteSize();
        ScriptFunction* scriptFunc = (ScriptFunction*)function;
        ScriptContext * scriptContext = function->GetScriptContext();
        PROBE_STACK_CALL(scriptContext, function, argsSize);

        Js::FunctionEntryPointInfo* entrypointInfo = (Js::FunctionEntryPointInfo*)scriptFunc->GetEntryPointInfo();
        switch (asmInfo->GetReturnType().which())
        {
        case AsmJsRetType::Void:
        case AsmJsRetType::Signed:
            m_localIntSlots[0] = JavascriptFunction::CallAsmJsFunction<int>(function, entrypointInfo->address, asmInfo->GetArgCount(), m_outParams);
            break;
        case AsmJsRetType::Double:
            m_localDoubleSlots[0] = JavascriptFunction::CallAsmJsFunction<double>(function, entrypointInfo->address, asmInfo->GetArgCount(), m_outParams);
            break;
        case AsmJsRetType::Float:
            m_localFloatSlots[0] = JavascriptFunction::CallAsmJsFunction<float>(function, entrypointInfo->address, asmInfo->GetArgCount(), m_outParams);
            break;
        case AsmJsRetType::Float32x4:
        case AsmJsRetType::Int32x4:
        case AsmJsRetType::Float64x2:
            X86SIMDValue simdVal;
            simdVal.m128_value = JavascriptFunction::CallAsmJsFunction<__m128>(function, entrypointInfo->address, asmInfo->GetArgCount(), m_outParams);
            m_localSimdSlots[0] = X86SIMDValue::ToSIMDValue(simdVal);
            break;
        }
        Assert((uint)((ArgSlot)asmInfo->GetArgCount() + 1) == (uint)(asmInfo->GetArgCount() + 1));
        if (scriptContext->GetConfig()->IsSimdjsEnabled())
        {
            PopOut((ArgSlot)(asmInfo->GetArgByteSize() / sizeof(Var)) + 1);
        }
        else
        {

            PopOut((ArgSlot)asmInfo->GetArgCount() + 1);
        }
        Assert(function);
    }
#elif _M_IX86
    void InterpreterStackFrame::OP_CallAsmInternal(RecyclableObject * function)
    {
        enum {
            Void = AsmJsRetType::Void,
            Signed = AsmJsRetType::Signed,
            Float = AsmJsRetType::Float,
            Double = AsmJsRetType::Double,
            Int32x4 = AsmJsRetType::Int32x4,
            Float32x4 = AsmJsRetType::Float32x4,
            Float64x2 = AsmJsRetType::Float64x2
        };

        AsmJsFunctionInfo* asmInfo = ((ScriptFunction*)function)->GetFunctionBody()->GetAsmJsFunctionInfo();
        Assert((uint)((ArgSlot)asmInfo->GetArgCount() + 1) == (uint)(asmInfo->GetArgCount() + 1));
        uint argsSize = asmInfo->GetArgByteSize();
        uint alignedSize = ::Math::Align<int32>(argsSize, 8);
        ScriptFunction* scriptFunc = (ScriptFunction*)function;
        ScriptContext * scriptContext = function->GetScriptContext();
        PROBE_STACK_CALL(scriptContext, function, alignedSize);

        Js::FunctionEntryPointInfo* entrypointInfo = (Js::FunctionEntryPointInfo*)scriptFunc->GetEntryPointInfo();

        int retIntVal = NULL;
        float retFloatVal = NULL;
        double retDoubleVal = NULL;

        AsmJsSIMDValue retSimdVal;
        retSimdVal.Zero();

        AsmJsRetType::Which retType = (AsmJsRetType::Which) GetRetType(scriptFunc);

        void *data = nullptr;
        JavascriptMethod entryPoint = (JavascriptMethod)entrypointInfo->address;
        void *savedEsp = nullptr;
        __asm
        {
            // Save ESP
            mov savedEsp, esp;
            mov eax, alignedSize;
            // Make sure we don't go beyond guard page
            cmp eax, 0x1000;
            jge alloca_probe;
            sub esp, eax;
            jmp dbl_align;
            alloca_probe :
            // Use alloca to allocate more then a page size
            // Alloca assumes eax, contains size, and adjust ESP while
            // probing each page.
            call _alloca_probe_16;
        dbl_align :
            and esp,-8
            mov data, esp;
        }

        {
            Var* outParam = m_outParams + 1;
            void* dest = (void*)data;
            memmove(dest, outParam, argsSize);

        }
        // call variable argument function provided in entryPoint
        __asm
        {
#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            mov  ecx, entryPoint
                call[__guard_check_icall_fptr]
                ; no need to restore ecx('call entryPoint' is a __cdecl call)
#endif
                push function;
            call entryPoint;
            mov ebx, retType;
            cmp ebx, Void;
            je VoidLabel;
            cmp ebx, Signed;
            je SignedLabel;
            cmp ebx, Float;
            je FloatLabel;
            cmp ebx, Double;
            je DoubleLabel;
            // simd
            movups retSimdVal, xmm0;
            jmp end
        VoidLabel:
        SignedLabel:
            mov retIntVal, eax;
            jmp end;
        FloatLabel:
            movss retFloatVal, xmm0;
            jmp end;
        DoubleLabel:
            movsd retDoubleVal, xmm0;
        end:
              // Restore ESP
              mov esp, savedEsp;
        }
        switch (retType)
        {
        case AsmJsRetType::Int32x4:
        case AsmJsRetType::Float32x4:
        case AsmJsRetType::Float64x2:
            if (scriptContext->GetConfig()->IsSimdjsEnabled())
            {
                m_localSimdSlots[0] = retSimdVal;
                break;
            }
            Assert(UNREACHED);
        case AsmJsRetType::Double:
            m_localDoubleSlots[0] = retDoubleVal;
            break;
        case AsmJsRetType::Float:
            m_localFloatSlots[0] = retFloatVal;
            break;
        case AsmJsRetType::Signed:
        case AsmJsRetType::Void:
            m_localIntSlots[0] = retIntVal;
            break;
        default:
            Assume(false);
        }
        PopOut((uint)((ArgSlot)argsSize/sizeof(Var)) + 1);
        Assert(function);
    }
#else
    void InterpreterStackFrame::OP_CallAsmInternal(RecyclableObject * function)
    {
        __debugbreak();
    }
#endif
#endif

    template <class T>
    void InterpreterStackFrame::OP_AsmCall(const unaligned T* playout)
    {
        OP_CallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), CallFlags_None);

        AsmJsModuleInfo::EnsureHeapAttached(this->function);
    }

    template <class T>
    void InterpreterStackFrame::OP_CallCommon(const unaligned T * playout, RecyclableObject * function, unsigned flags, const Js::AuxArray<uint32> *spreadIndices)
    {
        // Always save and restore implicit call flags when calling out
        // REVIEW: Can we avoid it if we don't collect dynamic profile info?
        ThreadContext * threadContext = scriptContext->GetThreadContext();
        Js::ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();

#if DBG
        if (scriptContext->IsInDebugMode())
        {
            JavascriptFunction::CheckValidDebugThunk(scriptContext, function);
        }
#endif

        if (playout->Return == Js::Constants::NoRegister)
        {
            flags |= CallFlags_NotUsed;
            Arguments args(CallInfo((CallFlags)flags, playout->ArgCount), m_outParams);
            AssertMsg(args.Info.Flags == flags, "Flags don't fit into the CallInfo field?");
            if (spreadIndices != nullptr)
            {
                JavascriptFunction::CallSpreadFunction(function, function->GetEntryPoint(), args, spreadIndices);
            }
            else
            {
                JavascriptFunction::CallFunction<true>(function, function->GetEntryPoint(), args);
            }
        }
        else
        {
            flags |= CallFlags_Value;
            Arguments args(CallInfo((CallFlags)flags, playout->ArgCount), m_outParams);
            AssertMsg(args.Info.Flags == flags, "Flags don't fit into the CallInfo field?");
            if (spreadIndices != nullptr)
            {
                SetReg((RegSlot)playout->Return, JavascriptFunction::CallSpreadFunction(function, function->GetEntryPoint(), args, spreadIndices));
            }
            else
            {
                SetReg((RegSlot)playout->Return, JavascriptFunction::CallFunction<true>(function, function->GetEntryPoint(), args));
            }
        }

        threadContext->SetImplicitCallFlags(savedImplicitCallFlags);
        PopOut(playout->ArgCount);
    }

    template <class T>
    void InterpreterStackFrame::OP_CallCommonI(const unaligned T * playout, RecyclableObject * function, unsigned flags)
    {
        OP_CallCommon(playout, function, flags); // CallCommon doesn't do anything with Member
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    void InterpreterStackFrame::OP_ProfileCallCommon(const unaligned T * playout, RecyclableObject * function, unsigned flags, ProfileId profileId, InlineCacheIndex inlineCacheIndex, const Js::AuxArray<uint32> *spreadIndices)
    {
        FunctionBody* functionBody = this->m_functionBody;
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        FunctionInfo* functionInfo = function->GetTypeId() == TypeIds_Function?
            JavascriptFunction::FromVar(function)->GetFunctionInfo() : nullptr;
        dynamicProfileInfo->RecordCallSiteInfo(functionBody, profileId, functionInfo, functionInfo ? static_cast<JavascriptFunction*>(function) : nullptr, playout->ArgCount, false, inlineCacheIndex);
        OP_CallCommon<T>(playout, function, flags, spreadIndices);
        if (playout->Return != Js::Constants::NoRegister)
        {
            dynamicProfileInfo->RecordReturnTypeOnCallSiteInfo(functionBody, profileId, GetReg((RegSlot)playout->Return));
        }
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfileReturnTypeCallCommon(const unaligned T * playout, RecyclableObject * function, unsigned flags, ProfileId profileId, const Js::AuxArray<uint32> *spreadIndices)
    {
        OP_CallCommon<T>(playout, function, flags, spreadIndices);
        FunctionBody* functionBody = this->m_functionBody;
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        if (playout->Return != Js::Constants::NoRegister)
        {
            dynamicProfileInfo->RecordReturnType(functionBody, profileId, GetReg((RegSlot)playout->Return));
        }
    }
#endif

    template <class T>
    void InterpreterStackFrame::OP_CallPutCommon(const unaligned T *playout, RecyclableObject * function)
    {
        Arguments args(CallInfo(CallFlags_None, playout->ArgCount), m_outParams);
        SetReg((RegSlot)playout->Return, function->InvokePut(args));
        PopOut(playout->ArgCount);
    }

    template <class T>
    void InterpreterStackFrame::OP_CallPutCommonI(const unaligned T *playout, RecyclableObject * function)
    {
        OP_CallPutCommon(playout, function);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetRootProperty(unaligned T* playout)
    {
        // Same fast path as in the backend.
        Assert(playout->inlineCacheIndex >= this->m_functionBody->GetRootObjectLoadInlineCacheStart());
        Js::Var instance = this->GetRootObject();

        InlineCache *inlineCache = this->GetInlineCache(playout->inlineCacheIndex);

        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        DynamicObject * obj = DynamicObject::FromVar(instance);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
        Var value;
        if(CacheOperators::TryGetProperty<true, false, false, false, false, false, true, false, false>(
                obj, true, obj, propertyId, &value, GetScriptContext(), nullptr, &info))
        {
            SetReg(playout->Value, value);
            return;
        }

        OP_GetRootProperty_NoFastPath(playout);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetRootPropertyForTypeOf(unaligned T* playout)
    {
        Var rootInstance = GetRootObject();
        PropertyId propertyId =  GetPropertyIdFromCacheId(playout->inlineCacheIndex);

        Var value = JavascriptOperators::PatchGetRootValueForTypeOf<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            DynamicObject::FromVar(rootInstance),
            propertyId
        );

        SetReg(playout->Value, value);

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetRootValue throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(rootInstance, propertyId, value, /*successful:*/true);
        }
#endif
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::OP_GetRootProperty_NoFastPath(unaligned T* playout)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        Var rootInstance = this->GetRootObject();

        Var value = JavascriptOperators::PatchGetRootValue<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            DynamicObject::FromVar(rootInstance),
            propertyId
        );

        SetReg(playout->Value, value);

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetRootValue throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(rootInstance, propertyId, value, /*successful:*/true);
        }
#endif
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    void InterpreterStackFrame::UpdateFldInfoFlagsForGetSetInlineCandidate(unaligned T* playout, FldInfoFlags& fldInfoFlags, CacheType cacheType,
                                                DynamicProfileInfo * dynamicProfileInfo, uint inlineCacheIndex, RecyclableObject * obj)
    {
        RecyclableObject *callee = nullptr;
        //TODO: Setter case once we stop sharing inline caches for these callsites.
        if ((cacheType & (CacheType_Getter | CacheType_Setter)) && GetInlineCache(inlineCacheIndex)->GetGetterSetter(obj->GetType(), &callee))
        {
            const auto functionBody = this->m_functionBody;
            bool canInline = dynamicProfileInfo->RecordLdFldCallSiteInfo(functionBody, callee, false /*callApplyTarget*/);
            if (canInline)
            {
                //updates this fldInfoFlags passed by reference.
                fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_InlineCandidate);
            }
        }
    }

    template <class T>
    void InterpreterStackFrame::UpdateFldInfoFlagsForCallApplyInlineCandidate(unaligned T* playout, FldInfoFlags& fldInfoFlags, CacheType cacheType,
                                                DynamicProfileInfo * dynamicProfileInfo, uint inlineCacheIndex, RecyclableObject * obj)
    {
        RecyclableObject *callee = nullptr;
        if (!(fldInfoFlags & FldInfo_Polymorphic) && GetInlineCache(inlineCacheIndex)->GetCallApplyTarget(obj, &callee))
        {
            const auto functionBody = this->m_functionBody;
            bool canInline = dynamicProfileInfo->RecordLdFldCallSiteInfo(functionBody, callee, true /*callApplyTarget*/);
            if (canInline)
            {
                //updates this fldInfoFlags passed by reference.
                fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_InlineCandidate);
            }
        }
    }

    template <class T, bool Root, bool Method, bool CallApplyTarget>
    void InterpreterStackFrame::ProfiledGetProperty(unaligned T* playout, const Var instance)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        Var value = ProfilingHelpers::ProfiledLdFld<Root, Method, CallApplyTarget>(
                instance,
                propertyId,
                GetInlineCache(playout->inlineCacheIndex),
                playout->inlineCacheIndex,
                GetFunctionBody(),
                instance);

        SetReg(playout->Value, value);

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetRootValue throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(instance, propertyId, value, /*successful:*/true);
        }
#endif
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetRootProperty(unaligned T* playout)
    {
        ProfiledGetProperty<T, true, false, false>(playout, GetRootObject());
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetRootPropertyForTypeOf(unaligned T* playout)
    {
        Var rootInstance = GetRootObject();
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        Var value = ProfilingHelpers::ProfiledLdFldForTypeOf<true, false, false>(
            rootInstance,
            propertyId,
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            GetFunctionBody());

        SetReg(playout->Value, value);

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetRootValue throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(rootInstance, propertyId, value, /*successful:*/true);
        }
#endif
    }
#endif

    template <class T>
    void InterpreterStackFrame::OP_GetPropertyForTypeOf(unaligned T* playout)
    {
        Var instance = GetReg(playout->Instance);
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        Var value = JavascriptOperators::PatchGetValueForTypeOf<false>(
                GetFunctionBody(),
                GetInlineCache(playout->inlineCacheIndex),
                playout->inlineCacheIndex,
                instance,
                propertyId
        );

        SetReg(playout->Value, value);

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetRootValue throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(instance, propertyId, value, /*successful:*/true);
        }
#endif
    }

    template <class T>
    void InterpreterStackFrame::OP_GetProperty(unaligned T* playout)
    {
        // Same fast path as in the backend.
        Var instance = GetReg(playout->Instance);
        OP_GetProperty(instance, playout);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetLocalProperty(unaligned T* playout)
    {
        // Same fast path as in the backend.
        Var instance = this->localClosure;
        OP_GetProperty(instance, playout);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetProperty(Var instance, unaligned T* playout)
    {
        InlineCache *inlineCache = GetInlineCache(playout->inlineCacheIndex);
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        if (RecyclableObject::Is(instance))
        {
            RecyclableObject* obj = RecyclableObject::FromVar(instance);
            PropertyValueInfo info;
            PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);

            Var value;
            if (CacheOperators::TryGetProperty<true, false, false, false, false, false, true, false, false>(
                    obj, false, obj, propertyId, &value, GetScriptContext(), nullptr, &info))
            {
                SetReg(playout->Value, value);
                return;
            }
        }
        OP_GetProperty_NoFastPath(instance, playout);
    }

    template <class T>
    void InterpreterStackFrame::OP_GetSuperProperty(unaligned T* playout)
    {
        // Same fast path as in the backend.
        Var instance = GetReg(playout->Instance);
        Var thisInstance = GetReg(playout->Value2);
        InlineCache *inlineCache = GetInlineCache(playout->PropertyIdIndex);
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->PropertyIdIndex);
        if (RecyclableObject::Is(instance) && RecyclableObject::Is(thisInstance))
        {
            RecyclableObject* superObj = RecyclableObject::FromVar(instance);
            RecyclableObject* thisObj = RecyclableObject::FromVar(thisInstance);
            PropertyValueInfo info;
            PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->PropertyIdIndex, true);

            Var value;
            if (CacheOperators::TryGetProperty<true, false, false, false, false, false, true, false, false>(
                thisObj, false, superObj, propertyId, &value, GetScriptContext(), nullptr, &info))
            {
                SetReg(playout->Value, value);
                return;
            }
        }
        SetReg(
            playout->Value,
            JavascriptOperators::PatchGetValueWithThisPtr<false>(
                GetFunctionBody(),
                GetInlineCache(playout->PropertyIdIndex),
                playout->PropertyIdIndex,
                GetReg(playout->Instance),
                GetPropertyIdFromCacheId(playout->PropertyIdIndex),
                GetReg(playout->Value2)));
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::OP_GetProperty_NoFastPath(Var instance, unaligned T* playout)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);

        Var value = JavascriptOperators::PatchGetValue<false>(
                GetFunctionBody(),
                GetInlineCache(playout->inlineCacheIndex),
                playout->inlineCacheIndex,
                instance,
                propertyId
        );

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetMethod throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(instance, propertyId, value, true);
        }
#endif

        SetReg(playout->Value, value);
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetProperty(unaligned T* playout)
    {
        ProfiledGetProperty<T, false, false, false>(playout, GetReg(playout->Instance));
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetLocalProperty(unaligned T* playout)
    {
        ProfiledGetProperty<T, false, false, false>(playout, this->localClosure);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetSuperProperty(unaligned T* playout)
    {
        SetReg(
            playout->Value,
            ProfilingHelpers::ProfiledLdFld<false, false, false>(
            GetReg(playout->Instance),
            GetPropertyIdFromCacheId(playout->PropertyIdIndex),
            GetInlineCache(playout->PropertyIdIndex),
            playout->PropertyIdIndex,
            GetFunctionBody(),
            GetReg(playout->Value2)));

    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetPropertyForTypeOf(unaligned T* playout)
    {
        Var instance = GetReg(playout->Instance);
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        Var value = ProfilingHelpers::ProfiledLdFldForTypeOf<false, false, false>(
            instance,
            propertyId,
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            GetFunctionBody()
        );

        SetReg(playout->Value, value);

#ifdef TELEMETRY_INTERPRETER
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {
            // `successful` will be true as PatchGetMethod throws an exception if not found.
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(instance, propertyId, value, true);
        }
#endif
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetPropertyCallApplyTarget(unaligned T* playout)
    {
        ProfiledGetProperty<T, false, false, true>(playout, GetReg(playout->Instance));
    }
#endif

    template <typename T>
    void InterpreterStackFrame::OP_GetPropertyScoped(const unaligned OpLayoutT_ElementP<T>* playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        // Get the property, using a scope stack rather than an individual instance.
        // Use the cache
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        FrameDisplay *pScope = this->GetEnvForEvalCode();
        InlineCache *inlineCache = this->GetInlineCache(playout->inlineCacheIndex);
        ScriptContext* scriptContext = GetScriptContext();
        int length = pScope->GetLength();
        if ( 1 == length )
        {
            DynamicObject *obj = (DynamicObject*)pScope->GetItem(0);
            PropertyValueInfo info;
            PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
            Var value;
            if (CacheOperators::TryGetProperty<true, false, false, false, false, false, true, false, false>(
                    obj, false, obj, propertyId, &value, scriptContext, nullptr, &info))
            {
                threadContext->CheckAndResetImplicitCallAccessorFlag();
                threadContext->AddImplicitCallFlags(savedImplicitCallFlags);

                SetReg(playout->Value, value);
                return;
            }
        }

        OP_GetPropertyScoped_NoFastPath(playout);

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }

    template <typename T>
    void InterpreterStackFrame::OP_GetPropertyForTypeOfScoped(const unaligned OpLayoutT_ElementP<T>* playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        // Get the property, using a scope stack rather than an individual instance.
        // Use the cache
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        FrameDisplay *pScope = this->GetEnvForEvalCode();
        InlineCache *inlineCache = this->GetInlineCache(playout->inlineCacheIndex);
        ScriptContext* scriptContext = GetScriptContext();
        int length = pScope->GetLength();
        if (1 == length)
        {
            DynamicObject *obj = (DynamicObject*)pScope->GetItem(0);
            PropertyValueInfo info;
            PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
            Var value;
            if (CacheOperators::TryGetProperty<true, false, false, false, false, false, true, false, false>(
                obj, false, obj, propertyId, &value, scriptContext, nullptr, &info))
            {
                threadContext->CheckAndResetImplicitCallAccessorFlag();
                threadContext->AddImplicitCallFlags(savedImplicitCallFlags);

                SetReg(playout->Value, value);
                return;
            }
        }

        SetReg(
            playout->Value,
            JavascriptOperators::PatchGetPropertyForTypeOfScoped<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            GetEnvForEvalCode(),
            GetPropertyIdFromCacheId(playout->inlineCacheIndex),
            GetReg(Js::FunctionBody::RootObjectRegSlot)));

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }


    template <typename T>
    __declspec(noinline) void InterpreterStackFrame::OP_GetPropertyScoped_NoFastPath(const unaligned OpLayoutT_ElementP<T>* playout)
    {
        // Implicit root object as default instance
        Var defaultInstance = GetReg(Js::FunctionBody::RootObjectRegSlot);

        // PatchGetPropertyScoped doesn't update type and slotIndex if the scope is not an array of length 1.
        SetReg(
            playout->Value,
            JavascriptOperators::PatchGetPropertyScoped<false>(
                GetFunctionBody(),
                GetInlineCache(playout->inlineCacheIndex),
                playout->inlineCacheIndex,
                GetEnvForEvalCode(),
                GetPropertyIdFromCacheId(playout->inlineCacheIndex),
                defaultInstance));
    }

    template <class T>
    void InterpreterStackFrame::OP_SetPropertyScoped(unaligned T* playout, PropertyOperationFlags flags)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        // Set the property, using a scope stack rather than an individual instance.
        // Use the cache
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        FrameDisplay *pScope = this->GetEnvForEvalCode();
        InlineCache *inlineCache = GetInlineCache(playout->inlineCacheIndex);
        ScriptContext* scriptContext = GetScriptContext();
        Var value = GetReg(playout->Value);

        DynamicObject *obj;
        int length = pScope->GetLength();
        if ( 1 == length )
        {
            obj = (DynamicObject*)pScope->GetItem(0);
            PropertyValueInfo info;
            PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
            if (CacheOperators::TrySetProperty<true, false, false, false, false, true, false, false>(
                    obj, false, propertyId, value, scriptContext, flags, nullptr, &info))
            {
                threadContext->CheckAndResetImplicitCallAccessorFlag();
                threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
                return;
            }
        }

        OP_SetPropertyScoped_NoFastPath(playout, flags);

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::OP_SetPropertyScoped_NoFastPath(unaligned T* playout, PropertyOperationFlags flags)
    {
        // Implicit root object as default instance
        Var defaultInstance = GetReg(Js::FunctionBody::RootObjectRegSlot);

        // PatchSetPropertyScoped doesn't update type and slotIndex if the scope is not an array of length 1.
        JavascriptOperators::PatchSetPropertyScoped<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            GetEnvForEvalCode(),
            GetPropertyIdFromCacheId(playout->inlineCacheIndex),
            GetReg(playout->Value),
            defaultInstance,
            flags);
    }

    template <class T>
    void InterpreterStackFrame::OP_SetPropertyScopedStrict(unaligned T* playout)
    {
        OP_SetPropertyScoped(playout, PropertyOperation_StrictMode);
    }

    template <class T>
    void InterpreterStackFrame::OP_ConsoleSetPropertyScoped(unaligned T* playout)
    {
        OP_SetPropertyScoped(playout, PropertyOperation_AllowUndeclInConsoleScope);
    }

    template <class T>
    __inline bool InterpreterStackFrame::TrySetPropertyLocalFastPath(unaligned T* playout, PropertyId pid, Var instance, InlineCache*& inlineCache, PropertyOperationFlags flags)
    {
        Assert(!TaggedNumber::Is(instance));

        RecyclableObject* obj = RecyclableObject::FromVar(instance);
        inlineCache = this->GetInlineCache(playout->inlineCacheIndex);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, GetFunctionBody(), inlineCache, playout->inlineCacheIndex, true);
        return
            CacheOperators::TrySetProperty<true, false, false, false, false, true, false, false>(
                obj,
                !!(flags & PropertyOperation_Root),
                pid,
                GetReg(playout->Value),
                GetScriptContext(),
                flags,
                nullptr,
                &info);
    }

    template <class T>
    __inline void InterpreterStackFrame::DoSetProperty(unaligned T* playout, Var instance, PropertyOperationFlags flags)
    {
        // Same fast path as in the backend.

        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        InlineCache *inlineCache;

        if (!TaggedNumber::Is(instance)
            && TrySetPropertyLocalFastPath(playout, propertyId, instance, inlineCache, flags))
        {
            if(GetJavascriptFunction()->GetConstructorCache()->NeedsUpdateAfterCtor())
            {
                // This function has only 'this' statements and is being used as a constructor. When the constructor exits, the
                // function object's constructor cache will be updated with the type produced by the constructor. From that
                // point on, when the same function object is used as a constructor, the a new object with the final type will
                // be created. Whatever is stored in the inline cache currently will cause cache misses after the constructor
                // cache update. So, just clear it now so that the caches won't be flagged as polymorphic.
                inlineCache->Clear();
            }
            return;
        }

        DoSetProperty_NoFastPath(playout, instance, flags);
    }


    template <class T>
    __inline void InterpreterStackFrame::DoSetSuperProperty(unaligned T* playout, Var instance, PropertyOperationFlags flags)
    {
        DoSetSuperProperty_NoFastPath(playout, instance, flags);
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::DoSetProperty_NoFastPath(unaligned T* playout, Var instance, PropertyOperationFlags flags)
    {
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        InlineCache *const inlineCache = GetInlineCache(playout->inlineCacheIndex);

        const auto PatchPutRootValue = &JavascriptOperators::PatchPutRootValueNoLocalFastPath<false, InlineCache>;
        const auto PatchPutValue = &JavascriptOperators::PatchPutValueNoLocalFastPath<false, InlineCache>;
        const auto PatchPut = flags & PropertyOperation_Root ? PatchPutRootValue : PatchPutValue;
        PatchPut(
            GetFunctionBody(),
            inlineCache,
            playout->inlineCacheIndex,
            instance,
            GetPropertyIdFromCacheId(playout->inlineCacheIndex),
            GetReg(playout->Value),
            flags);

        if(!TaggedNumber::Is(instance) && GetJavascriptFunction()->GetConstructorCache()->NeedsUpdateAfterCtor())
        {
            // This function has only 'this' statements and is being used as a constructor. When the constructor exits, the
            // function object's constructor cache will be updated with the type produced by the constructor. From that
            // point on, when the same function object is used as a constructor, the a new object with the final type will
            // be created. Whatever is stored in the inline cache currently will cause cache misses after the constructor
            // cache update. So, just clear it now so that the caches won't be flagged as polymorphic.
            inlineCache->Clear();
        }
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::DoSetSuperProperty_NoFastPath(unaligned T* playout, Var instance, PropertyOperationFlags flags)
    {
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        InlineCache *const inlineCache = GetInlineCache(playout->PropertyIdIndex);

        JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath<false, InlineCache>(
            GetFunctionBody(),
            inlineCache,
            playout->PropertyIdIndex,
            instance,
            GetPropertyIdFromCacheId(playout->PropertyIdIndex),
            GetReg(playout->Value),
            GetReg(playout->Value2),
            flags);

        if (!TaggedNumber::Is(instance) && GetJavascriptFunction()->GetConstructorCache()->NeedsUpdateAfterCtor())
        {
            // This function has only 'this' statements and is being used as a constructor. When the constructor exits, the
            // function object's constructor cache will be updated with the type produced by the constructor. From that
            // point on, when the same function object is used as a constructor, the a new object with the final type will
            // be created. Whatever is stored in the inline cache currently will cause cache misses after the constructor
            // cache update. So, just clear it now so that the caches won't be flagged as polymorphic.
            inlineCache->Clear();
        }
    }

#if ENABLE_PROFILE_INFO
    template <class T, bool Root>
    void InterpreterStackFrame::ProfiledSetProperty(unaligned T* playout, Var instance, PropertyOperationFlags flags)
    {
        Assert(!Root || flags & PropertyOperation_Root);

        ProfilingHelpers::ProfiledStFld<Root>(
            instance,
            GetPropertyIdFromCacheId(playout->inlineCacheIndex),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            GetReg(playout->Value),
            flags,
            GetJavascriptFunction(),
            instance);
    }

    template <class T, bool Root>
    void InterpreterStackFrame::ProfiledSetSuperProperty(unaligned T* playout, Var instance, Var thisInstance, PropertyOperationFlags flags)
    {
        Assert(!Root || flags & PropertyOperation_Root);

        ProfilingHelpers::ProfiledStFld<Root>(
            instance,
            GetPropertyIdFromCacheId(playout->PropertyIdIndex),
            GetInlineCache(playout->PropertyIdIndex),
            playout->PropertyIdIndex,
            GetReg(playout->Value),
            flags,
            GetJavascriptFunction(),
            thisInstance);
    }
#endif

    template <class T>
    void InterpreterStackFrame::OP_SetProperty(unaligned T* playout)
    {
        DoSetProperty(playout, GetReg(playout->Instance), PropertyOperation_None);
    }

    template <class T>
    void InterpreterStackFrame::OP_SetLocalProperty(unaligned T* playout)
    {
        DoSetProperty(playout, this->localClosure, PropertyOperation_None);
    }

    template <class T>
    void InterpreterStackFrame::OP_SetSuperProperty(unaligned T* playout)
    {
        DoSetSuperProperty(playout, GetReg(playout->Instance), PropertyOperation_None);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledSetProperty(unaligned T* playout)
    {
        ProfiledSetProperty<T, false>(playout, GetReg(playout->Instance), PropertyOperation_None);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledSetLocalProperty(unaligned T* playout)
    {
        ProfiledSetProperty<T, false>(playout, this->localClosure, PropertyOperation_None);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledSetSuperProperty(unaligned T* playout)
    {
        ProfiledSetSuperProperty<T, false>(playout, GetReg(playout->Instance), GetReg(playout->Value2), PropertyOperation_None);
    }

    template <class T>
    void InterpreterStackFrame::OP_SetRootProperty(unaligned T* playout)
    {
        DoSetProperty(playout, this->GetRootObject(), PropertyOperation_Root);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledSetRootProperty(unaligned T* playout)
    {
        ProfiledSetProperty<T, true>(playout, this->GetRootObject(), PropertyOperation_Root);
    }

    template <class T>
    void InterpreterStackFrame::OP_SetPropertyStrict(unaligned T* playout)
    {
        DoSetProperty(playout, GetReg(playout->Instance), PropertyOperation_StrictMode);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledSetPropertyStrict(unaligned T* playout)
    {
        ProfiledSetProperty<T, false>(playout, GetReg(playout->Instance), PropertyOperation_StrictMode);
    }

    template <class T>
    void InterpreterStackFrame::OP_SetRootPropertyStrict(unaligned T* playout)
    {
        DoSetProperty(playout, this->GetRootObject(), PropertyOperation_StrictModeRoot);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledSetRootPropertyStrict(unaligned T* playout)
    {
        ProfiledSetProperty<T, true>(playout, this->GetRootObject(), PropertyOperation_StrictModeRoot);
    }

#if ENABLE_PROFILE_INFO
    template <bool doProfile>
    Var InterpreterStackFrame::ProfiledDivide(Var aLeft, Var aRight, ScriptContext* scriptContext, ProfileId profileId)
    {
        Var result = JavascriptMath::Divide(aLeft, aRight,scriptContext);

        if (doProfile)
        {
            Js::FunctionBody* body = this->m_functionBody;
            body->GetDynamicProfileInfo()->RecordDivideResultType(body, profileId, result);
        }

        return result;
    }

    template <bool doProfile>
    Var InterpreterStackFrame::ProfileModulus(Var aLeft, Var aRight, ScriptContext* scriptContext, ProfileId profileId)
    {
        // If both arguments are TaggedInt, then try to do integer division
            // This case is not handled by the lowerer.
        if (doProfile)
        {
            Js::FunctionBody* body = this->function->GetFunctionBody();
            if(TaggedInt::IsPair(aLeft, aRight))
            {
                int nLeft    = TaggedInt::ToInt32(aLeft);
                int nRight   = TaggedInt::ToInt32(aRight);

                // nLeft is positive and nRight is +2^i
                // Fast path for Power of 2 divisor
                if (nLeft > 0 && ::Math::IsPow2(nRight))
                {
                    body->GetDynamicProfileInfo()->RecordModulusOpType(body, profileId, /*isModByPowerOf2*/ true);
                    return TaggedInt::ToVarUnchecked(nLeft & (nRight - 1));
                }
            }
            body->GetDynamicProfileInfo()->RecordModulusOpType(body, profileId, /*isModByPowerOf2*/ false);
        }

        return JavascriptMath::Modulus(aLeft, aRight,scriptContext);
    }

    template <bool doProfile>
    Var InterpreterStackFrame::ProfiledSwitch(Var exp, ProfileId profileId)
    {
        if (doProfile)
        {
            Js::FunctionBody* body = this->m_functionBody;
            body->GetDynamicProfileInfo()->RecordSwitchType(body, profileId, exp);
        }

        return exp;
    }
#else
    template <bool doProfile>
    Var InterpreterStackFrame::ProfiledDivide(Var aLeft, Var aRight, ScriptContext* scriptContext, ProfileId profileId)
    {
        Assert(!doProfile);
        return JavascriptMath::Divide(aLeft, aRight, scriptContext);
    }

    template <bool doProfile>
    Var InterpreterStackFrame::ProfileModulus(Var aLeft, Var aRight, ScriptContext* scriptContext, ProfileId profileId)
    {
        Assert(!doProfile);
        return JavascriptMath::Modulus(aLeft, aRight, scriptContext);
    }

    template <bool doProfile>
    Var InterpreterStackFrame::ProfiledSwitch(Var exp, ProfileId profileId)
    {
        Assert(!doProfile);
        return exp;
    }
#endif

    template <class T>
    void InterpreterStackFrame::DoInitProperty(unaligned T* playout, Var instance)
    {
        // Same fast path as in the backend.

        InlineCache *inlineCache = nullptr;

        Assert(!TaggedNumber::Is(instance));
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        if (TrySetPropertyLocalFastPath(playout, propertyId, instance, inlineCache))
        {
            return;
        }

        DoInitProperty_NoFastPath(playout, instance);
    }

    template <class T>
    __declspec(noinline) void InterpreterStackFrame::DoInitProperty_NoFastPath(unaligned T* playout, Var instance)
    {
        JavascriptOperators::PatchInitValue<false>(
            GetFunctionBody(),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            RecyclableObject::FromVar(instance),
            GetPropertyIdFromCacheId(playout->inlineCacheIndex),
            GetReg(playout->Value));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitClassMember(const unaligned T * playout)
    {
        uint inlineCacheIndex = playout->inlineCacheIndex;
        InlineCache * inlineCache = this->GetInlineCache(inlineCacheIndex);
        Var instance = GetReg(playout->Instance);
        PropertyOperationFlags flags = PropertyOperation_None;

        Assert(!TaggedNumber::Is(instance));
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        if (!TrySetPropertyLocalFastPath(playout, propertyId, instance, inlineCache, flags))
        {
            JavascriptOperators::OP_InitClassMember(instance, propertyId, GetReg(playout->Value));
        }
    }

    template <class T>
    void InterpreterStackFrame::OP_InitClassMemberGet(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitClassMemberGet(
            GetReg(playout->Instance),
            m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex),
            GetReg(playout->Value));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitClassMemberSet(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitClassMemberSet(
            GetReg(playout->Instance),
            m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex),
            GetReg(playout->Value));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitClassMemberSetComputedName(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitClassMemberSetComputedName(
            GetReg(playout->Instance),
            GetReg(playout->Element),
            GetReg(playout->Value),
            m_functionBody->GetScriptContext());
    }

    template <class T>
    void InterpreterStackFrame::OP_InitClassMemberGetComputedName(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitClassMemberGetComputedName(
            GetReg(playout->Instance),
            GetReg(playout->Element),
            GetReg(playout->Value),
            m_functionBody->GetScriptContext());
    }

    template <class T>
    void InterpreterStackFrame::OP_InitClassMemberComputedName(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitClassMemberComputedName(
            GetReg(playout->Instance),
            GetReg(playout->Element),
            GetReg(playout->Value),
            m_functionBody->GetScriptContext());
    }

    template <class T>
    void InterpreterStackFrame::DoInitLetFld(const unaligned T * playout, Var instance, PropertyOperationFlags flags)
    {
        uint inlineCacheIndex = playout->inlineCacheIndex;
        InlineCache * inlineCache = this->GetInlineCache(inlineCacheIndex);

        Assert(!TaggedNumber::Is(instance));
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        if (!TrySetPropertyLocalFastPath(playout, propertyId, instance, inlineCache, flags))
        {
            JavascriptOperators::OP_InitLetProperty(instance, propertyId, GetReg(playout->Value));
        }
    }

    template <class T>
    void InterpreterStackFrame::DoInitConstFld(const unaligned T * playout, Var instance, PropertyOperationFlags flags)
    {
        uint inlineCacheIndex = playout->inlineCacheIndex;
        InlineCache * inlineCache = this->GetInlineCache(inlineCacheIndex);

        Assert(!TaggedNumber::Is(instance));
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        if (!TrySetPropertyLocalFastPath(playout, propertyId, instance, inlineCache, flags))
        {
            JavascriptOperators::OP_InitConstProperty(instance, propertyId, GetReg(playout->Value));
        }
    }

    template <class T>
    void InterpreterStackFrame::OP_InitProperty(unaligned T* playout)
    {
        DoInitProperty(playout, GetReg(playout->Instance));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitLocalProperty(unaligned T* playout)
    {
        DoInitProperty(playout, this->localClosure);
    }

    template <class T>
    void InterpreterStackFrame::OP_InitInnerFld(const unaligned T* playout)
    {
        DoInitProperty(playout, InnerScopeFromIndex(playout->scopeIndex));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitLetFld(const unaligned T * playout)
    {
        DoInitLetFld(playout, GetReg(playout->Instance));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitInnerLetFld(const unaligned T * playout)
    {
        DoInitLetFld(playout, InnerScopeFromIndex(playout->scopeIndex));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitLocalLetFld(const unaligned T * playout)
    {
        DoInitLetFld(playout, this->localClosure);
    }

    template <class T>
    void InterpreterStackFrame::OP_InitConstFld(const unaligned T * playout)
    {
        DoInitConstFld(playout, GetReg(playout->Instance));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitRootProperty(unaligned T* playout)
    {
        Assert(playout->inlineCacheIndex >= this->m_functionBody->GetRootObjectLoadInlineCacheStart());

        DoInitProperty(playout, this->GetRootObject());
    }

    template <class T>
    void InterpreterStackFrame::OP_InitRootLetFld(const unaligned T * playout)
    {
        Assert(playout->inlineCacheIndex >= this->m_functionBody->GetRootObjectLoadInlineCacheStart());

        DoInitLetFld(playout, this->GetRootObject(), PropertyOperation_Root);
    }

    template <class T>
    void InterpreterStackFrame::OP_InitRootConstFld(const unaligned T * playout)
    {
        Assert(playout->inlineCacheIndex >= this->m_functionBody->GetRootObjectLoadInlineCacheStart());

        DoInitConstFld(playout, this->GetRootObject(), PropertyOperation_Root);
    }

    template <class T>
    void InterpreterStackFrame::OP_InitUndeclLetProperty(unaligned T* playout)
    {
        Var instance = InnerScopeFromIndex(playout->scopeIndex);
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        JavascriptOperators::OP_InitLetProperty(instance, propertyId, this->scriptContext->GetLibrary()->GetUndeclBlockVar());
    }

    template <class T>
    void InterpreterStackFrame::OP_InitUndeclLocalLetProperty(unaligned T* playout)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        JavascriptOperators::OP_InitLetProperty(this->localClosure, propertyId, this->scriptContext->GetLibrary()->GetUndeclBlockVar());
    }

    void InterpreterStackFrame::OP_InitUndeclRootLetProperty(uint propertyIdIndex)
    {
        Var instance = this->GetRootObject();
        PropertyId propertyId = this->m_functionBody->GetReferencedPropertyId(propertyIdIndex);
        JavascriptOperators::OP_InitUndeclRootLetProperty(instance, propertyId);
    }

    template <class T>
    void InterpreterStackFrame::OP_InitUndeclConstProperty(unaligned T* playout)
    {
        Var instance = InnerScopeFromIndex(playout->scopeIndex);
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        JavascriptOperators::OP_InitConstProperty(instance, propertyId, this->scriptContext->GetLibrary()->GetUndeclBlockVar());
    }

    template <class T>
    void InterpreterStackFrame::OP_InitUndeclLocalConstProperty(unaligned T* playout)
    {
        PropertyId propertyId = GetPropertyIdFromCacheId(playout->inlineCacheIndex);
        JavascriptOperators::OP_InitConstProperty(this->localClosure, propertyId, this->scriptContext->GetLibrary()->GetUndeclBlockVar());
    }

    void InterpreterStackFrame::OP_InitUndeclRootConstProperty(uint propertyIdIndex)
    {
        Var instance = this->GetRootObject();
        PropertyId propertyId = this->m_functionBody->GetReferencedPropertyId(propertyIdIndex);
        JavascriptOperators::OP_InitUndeclRootConstProperty(instance, propertyId);
    }

    template <class T>
    void InterpreterStackFrame::OP_InitUndeclConsoleLetProperty(unaligned T* playout)
    {
        FrameDisplay* pScope = (FrameDisplay*)this->LdEnv();
        AssertMsg(ConsoleScopeActivationObject::Is((DynamicObject*)pScope->GetItem(pScope->GetLength() - 1)), "How come we got this opcode without ConsoleScopeActivationObject?");
        PropertyId propertyId = m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex);
        JavascriptOperators::OP_InitLetProperty(pScope->GetItem(0), propertyId, this->scriptContext->GetLibrary()->GetUndeclBlockVar());
    }

    template <class T>
    void InterpreterStackFrame::OP_InitUndeclConsoleConstProperty(unaligned T* playout)
    {
        FrameDisplay* pScope = (FrameDisplay*)this->LdEnv();
        AssertMsg(ConsoleScopeActivationObject::Is((DynamicObject*)pScope->GetItem(pScope->GetLength() - 1)), "How come we got this opcode without ConsoleScopeActivationObject?");
        PropertyId propertyId = m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex);
        JavascriptOperators::OP_InitConstProperty(pScope->GetItem(0), propertyId, this->scriptContext->GetLibrary()->GetUndeclBlockVar());
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    void InterpreterStackFrame::ProfiledInitProperty(unaligned T* playout, Var instance)
    {
        ProfilingHelpers::ProfiledInitFld(
            RecyclableObject::FromVar(instance),
            GetPropertyIdFromCacheId(playout->inlineCacheIndex),
            GetInlineCache(playout->inlineCacheIndex),
            playout->inlineCacheIndex,
            GetReg(playout->Value),
            GetFunctionBody());
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledInitProperty(unaligned T* playout)
    {
        ProfiledInitProperty(playout, GetReg(playout->Instance));
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledInitLocalProperty(unaligned T* playout)
    {
        ProfiledInitProperty(playout, this->localClosure);
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledInitRootProperty(unaligned T* playout)
    {
        ProfiledInitProperty(playout, this->GetRootObject());
    }

    template <class T>
    void InterpreterStackFrame::OP_ProfiledGetElementI(const unaligned OpLayoutDynamicProfile<T>* playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        SetReg(
            playout->Value,
            ProfilingHelpers::ProfiledLdElem(
                GetReg(playout->Instance),
                GetReg(playout->Element),
                m_functionBody,
                playout->profileId));

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }
#endif

    template <typename T>
    void InterpreterStackFrame::OP_GetElementI(const unaligned T* playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        // Same fast path as in the backend.

        Var instance = GetReg(playout->Instance);

        // Only enable fast path if the javascript array is not cross site
        Var element;
#if ENABLE_PROFILE_INFO
        if (!TaggedNumber::Is(instance) && VirtualTableInfo<JavascriptArray>::HasVirtualTable(instance))
        {
            element =
                ProfilingHelpers::ProfiledLdElem_FastPath(
                    JavascriptArray::FromVar(instance),
                    GetReg(playout->Element),
                    GetScriptContext());
        }
        else
#endif
        {
            element = JavascriptOperators::OP_GetElementI(instance, GetReg(playout->Element), GetScriptContext());
        }

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);

        SetReg(playout->Value, element);
    }

    template <typename T>
    void InterpreterStackFrame::OP_SetElementI(const unaligned T* playout, PropertyOperationFlags flags)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        // Same fast path as in the backend.

        Var instance = GetReg(playout->Instance);
        const Var varIndex = GetReg(playout->Element);
        const Var value = GetReg(playout->Value);

#if ENABLE_PROFILE_INFO
        // Only enable fast path if the javascript array is not cross site
        if (!TaggedNumber::Is(instance) &&
            VirtualTableInfo<JavascriptArray>::HasVirtualTable(instance) &&
            !JavascriptOperators::SetElementMayHaveImplicitCalls(GetScriptContext()))
        {
            ProfilingHelpers::ProfiledStElem_FastPath(
                JavascriptArray::FromVar(instance),
                varIndex,
                value,
                GetScriptContext(),
                flags);
        }
        else
#endif
        {
            JavascriptOperators::OP_SetElementI(instance, varIndex, value, GetScriptContext(), flags);
        }

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }

#if ENABLE_PROFILE_INFO
    template <typename T>
    void InterpreterStackFrame::OP_ProfiledSetElementI(
        const unaligned OpLayoutDynamicProfile<T>* playout,
        PropertyOperationFlags flags)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        ProfilingHelpers::ProfiledStElem(
            GetReg(playout->Instance),
            GetReg(playout->Element),
            GetReg(playout->Value),
            m_functionBody,
            playout->profileId,
            flags);

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }
#endif

    template <typename T>
    void InterpreterStackFrame::OP_SetElementIStrict(const unaligned T* playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        OP_SetElementI(playout, PropertyOperation_StrictMode);

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }

#if ENABLE_PROFILE_INFO
    template <typename T>
    void InterpreterStackFrame::OP_ProfiledSetElementIStrict(const unaligned OpLayoutDynamicProfile<T>* playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        OP_ProfiledSetElementI(playout, PropertyOperation_StrictMode);

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
    }
#endif

    template <class T>
    void InterpreterStackFrame::OP_LdArrayHeadSegment(const unaligned T* playout)
    {
        JavascriptArray* array = JavascriptArray::FromAnyArray(GetReg(playout->R1));

        // The array is create by the built-in on the same script context
        Assert(array->GetScriptContext() == GetScriptContext());

        SetNonVarReg(playout->R0, array->GetHead());
    }

    template <class T>
    void InterpreterStackFrame::OP_SetArraySegmentItem_CI4(const unaligned T* playout)
    {
        SparseArraySegment<Var> * segment = (SparseArraySegment<Var> *)GetNonVarReg(playout->Instance);

        uint32 index = playout->Element;
        Var value = GetReg(playout->Value);

        Assert(segment->left == 0);
        Assert(index < segment->length);

        segment->elements[index] = value;
    }

    template <class T>
    void InterpreterStackFrame::OP_NewScArray(const unaligned T * playout)
    {
        JavascriptArray *arr;
        arr = scriptContext->GetLibrary()->CreateArrayLiteral(playout->C1);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif

        SetReg(playout->R0, arr);
    }

#if ENABLE_PROFILE_INFO
    template <bool Profiled, class T>
    void InterpreterStackFrame::ProfiledNewScArray(const unaligned OpLayoutDynamicProfile<T> * playout)
    {
        if(!Profiled && !isAutoProfiling)
        {
            OP_NewScArray(playout);
            return;
        }

        SetReg(
            playout->R0,
            ProfilingHelpers::ProfiledNewScArray(
                playout->C1,
                m_functionBody,
                playout->profileId));
    }
#else
    template <bool Profiled, class T>
    void InterpreterStackFrame::ProfiledNewScArray(const unaligned OpLayoutDynamicProfile<T> * playout)
    {
        Assert(!Profiled);
        OP_NewScArray(playout);
    }
#endif

    void InterpreterStackFrame::OP_NewScIntArray(const unaligned OpLayoutAuxiliary * playout)
    {
#if ENABLE_PROFILE_INFO
        if(isAutoProfiling)
        {
            OP_ProfiledNewScIntArray(static_cast<const unaligned OpLayoutDynamicProfile<OpLayoutAuxiliary> *>(playout));
            return;
        }
#endif

        const Js::AuxArray<int32> *ints = Js::ByteCodeReader::ReadAuxArray<int32>(playout->Offset, this->GetFunctionBody());

        JavascriptNativeIntArray *arr = scriptContext->GetLibrary()->CreateNativeIntArrayLiteral(ints->count);

        SparseArraySegment<int32> * segment = (SparseArraySegment<int32>*)arr->GetHead();

        JavascriptOperators::AddIntsToArraySegment(segment, ints);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif

        SetReg(playout->R0, arr);
    }

#if ENABLE_PROFILE_INFO
    void InterpreterStackFrame::OP_ProfiledNewScIntArray(const unaligned OpLayoutDynamicProfile<OpLayoutAuxiliary> * playout)
    {
        const Js::AuxArray<int32> *ints = Js::ByteCodeReader::ReadAuxArray<int32>(playout->Offset, this->GetFunctionBody());

        Js::ProfileId profileId = playout->profileId;
        FunctionBody *functionBody = this->m_functionBody;
        ArrayCallSiteInfo *arrayInfo = functionBody->GetDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);
        Assert(arrayInfo);

        JavascriptArray *arr;
        JavascriptLibrary *lib = scriptContext->GetLibrary();

        if (arrayInfo && arrayInfo->IsNativeIntArray())
        {
#if ENABLE_COPYONACCESS_ARRAY
            if (JavascriptLibrary::IsCopyOnAccessArrayCallSite(lib, arrayInfo, ints->count))
            {
                Assert(lib->cacheForCopyOnAccessArraySegments);
                arr = scriptContext->GetLibrary()->CreateCopyOnAccessNativeIntArrayLiteral(arrayInfo, functionBody, ints);
            }
            else
#endif
            {
                arr = scriptContext->GetLibrary()->CreateNativeIntArrayLiteral(ints->count);
                SparseArraySegment<int32> *segment = (SparseArraySegment<int32>*)arr->GetHead();
                JavascriptOperators::AddIntsToArraySegment(segment, ints);
            }

            JavascriptNativeIntArray *intArray = reinterpret_cast<JavascriptNativeIntArray*>(arr);
            Recycler *recycler = scriptContext->GetRecycler();
            intArray->SetArrayCallSite(profileId, recycler->CreateWeakReferenceHandle(functionBody));
        }
        else if (arrayInfo && arrayInfo->IsNativeFloatArray())
        {
            arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(ints->count);
            SparseArraySegment<double> * segment = (SparseArraySegment<double>*)arr->GetHead();
            for (uint i = 0; i < ints->count; i++)
            {
                segment->elements[i] = (double)ints->elements[i];
            }

            JavascriptNativeFloatArray *floatArray = reinterpret_cast<JavascriptNativeFloatArray*>(arr);
            Recycler *recycler = scriptContext->GetRecycler();
            floatArray->SetArrayCallSite(profileId, recycler->CreateWeakReferenceHandle(functionBody));
        }
        else
        {
            arr = scriptContext->GetLibrary()->CreateArrayLiteral(ints->count);
            SparseArraySegment<Var> * segment = (SparseArraySegment<Var>*)arr->GetHead();
            for (uint i = 0; i < ints->count; i++)
            {
                segment->elements[i] = JavascriptNumber::ToVar(ints->elements[i], scriptContext);
            }
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif

        SetReg(playout->R0, arr);
    }
#endif

    void InterpreterStackFrame::OP_NewScFltArray(const unaligned OpLayoutAuxiliary * playout )
    {
#if ENABLE_PROFILE_INFO
        if(isAutoProfiling)
        {
            OP_ProfiledNewScFltArray(static_cast<const unaligned OpLayoutDynamicProfile<OpLayoutAuxiliary> *>(playout));
            return;
        }
#endif

        const Js::AuxArray<double> *doubles = Js::ByteCodeReader::ReadAuxArray<double>(playout->Offset, this->GetFunctionBody());

        JavascriptNativeFloatArray *arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(doubles->count);

        SparseArraySegment<double> * segment = (SparseArraySegment<double>*)arr->GetHead();

        JavascriptOperators::AddFloatsToArraySegment(segment, doubles);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif

        SetReg(playout->R0, arr);
    }

#if ENABLE_PROFILE_INFO
    void InterpreterStackFrame::OP_ProfiledNewScFltArray(const unaligned OpLayoutDynamicProfile<OpLayoutAuxiliary> * playout)
    {
        const Js::AuxArray<double> *doubles = Js::ByteCodeReader::ReadAuxArray<double>(playout->Offset, this->GetFunctionBody());

        Js::ProfileId  profileId = playout->profileId;
        FunctionBody *functionBody = this->m_functionBody;
        ArrayCallSiteInfo *arrayInfo = functionBody->GetDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);
        Assert(arrayInfo);

        JavascriptArray *arr;
        if (arrayInfo && arrayInfo->IsNativeFloatArray())
        {
            arrayInfo->SetIsNotNativeIntArray();
            arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(doubles->count);
            SparseArraySegment<double> * segment = (SparseArraySegment<double>*)arr->GetHead();
            JavascriptOperators::AddFloatsToArraySegment(segment, doubles);

            JavascriptNativeFloatArray *floatArray = reinterpret_cast<JavascriptNativeFloatArray*>(arr);
            Recycler *recycler = scriptContext->GetRecycler();
            floatArray->SetArrayCallSite(profileId, recycler->CreateWeakReferenceHandle(functionBody));
        }
        else
        {
            arr = scriptContext->GetLibrary()->CreateArrayLiteral(doubles->count);
            SparseArraySegment<Var> * segment = (SparseArraySegment<Var>*)arr->GetHead();
            for (uint i = 0; i < doubles->count; i++)
            {
                segment->elements[i] = JavascriptNumber::ToVarNoCheck(doubles->elements[i], scriptContext);
            }
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        arr->CheckForceES5Array();
#endif

        SetReg(playout->R0, arr);
    }
#endif

    void InterpreterStackFrame::OP_SetArraySegmentVars(const unaligned OpLayoutAuxiliary * playout)
    {
        const Js::VarArray *vars = Js::ByteCodeReader::ReadAuxArray<Var>(playout->Offset, this->GetFunctionBody());

        SparseArraySegment<Var> * segment =(SparseArraySegment<Var> *)GetNonVarReg(playout->R0);

        JavascriptOperators::AddVarsToArraySegment(segment, vars);
    }

    template <class T>
    void InterpreterStackFrame::OP_SetArrayItemC_CI4(const unaligned T* playout)
    {
        JavascriptArray* array = JavascriptArray::FromAnyArray(GetReg(playout->Instance));
        uint32 index = playout->Element;
        Var value = GetReg(playout->Value);

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(value);
#endif

        // The array is create by the built-in on the same script context
        Assert(array->GetScriptContext() == GetScriptContext());
        TypeId typeId = array->GetTypeId();
        if (typeId == TypeIds_NativeIntArray)
        {
            JavascriptArray::OP_SetNativeIntElementC(reinterpret_cast<JavascriptNativeIntArray*>(array), index, value, array->GetScriptContext());
        }
        else if (typeId == TypeIds_NativeFloatArray)
        {
            JavascriptArray::OP_SetNativeFloatElementC(reinterpret_cast<JavascriptNativeFloatArray*>(array), index, value, array->GetScriptContext());
        }
        else
        {
            array->SetArrayLiteralItem(index, value);
        }
    }

    template <class T>
    void InterpreterStackFrame::OP_SetArrayItemI_CI4(const unaligned T* playout)
    {
        // Note that this code assumes that we only get here when we see an array literal,
        // so we know that the instance is truly an array, and the index is a uint32.
        // If/when we use this for cases like "a[0] = x", we'll at least have to check
        // whether "a" is really an array.

        JavascriptArray* array = JavascriptArray::FromAnyArray(GetReg(playout->Instance));

        // The array is create by the built-in on the same script context
        Assert(array->GetScriptContext() == GetScriptContext());

        uint32 index = playout->Element;
        Var value = GetReg(playout->Value);

        Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(array));
        SparseArraySegment<Var>* lastUsedSeg = (SparseArraySegment<Var>*)array->GetLastUsedSegment();
        if (index < lastUsedSeg->left)
        {
            goto helper;
        }

        uint32 index2 = index - lastUsedSeg->left;

        if (index2 < lastUsedSeg->size)
        {
            // Successful fastpath
            array->DirectSetItemInLastUsedSegmentAt(index2, value);
            return;
        }

    helper:
        ScriptContext* scriptContext = array->GetScriptContext();
        JavascriptOperators::SetItem(array, array, index, value, scriptContext);
    }

#if ENABLE_PROFILE_INFO
    Var InterpreterStackFrame::OP_ProfiledLdThis(Var thisVar, int moduleID, ScriptContext *scriptContext)
    {
        FunctionBody * functionBody = this->m_functionBody;
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        TypeId typeId = JavascriptOperators::GetTypeId(thisVar);

        if (JavascriptOperators::IsThisSelf(typeId))
        {
            Assert(typeId != TypeIds_GlobalObject || ((Js::GlobalObject*)thisVar)->ToThis() == thisVar);
            Assert(typeId != TypeIds_ModuleRoot || JavascriptOperators::GetThisFromModuleRoot(thisVar) == thisVar);

            // Record the fact that we saw a trivial LdThis.
            dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Simple);

            return thisVar;
        }

        thisVar = JavascriptOperators::OP_GetThis(thisVar, moduleID, scriptContext);

        // Record the fact that we saw a LdThis that had to map its source to something else, or at least
        // forced us to call a helper, e.g., a FastDOM object with an unrecognized type ID.
        dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Mapped);

        return thisVar;
    }

    Var InterpreterStackFrame::OP_ProfiledStrictLdThis(Var thisVar, ScriptContext* scriptContext)
    {
        FunctionBody * functionBody = this->m_functionBody;
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        TypeId typeId = JavascriptOperators::GetTypeId(thisVar);

        if (typeId == TypeIds_ActivationObject)
        {
            thisVar = scriptContext->GetLibrary()->GetUndefined();
            dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Mapped);
            return thisVar;
        }

        dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Simple);
        return thisVar;
    }
#endif

    void InterpreterStackFrame::OP_InitCachedFuncs(const unaligned OpLayoutAuxNoReg * playout)
    {
        const FuncInfoArray *info = Js::ByteCodeReader::ReadAuxArray<FuncInfoEntry>(playout->Offset, this->GetFunctionBody());
        JavascriptOperators::OP_InitCachedFuncs(this->localClosure, GetLocalFrameDisplay(), info, GetScriptContext());
    }

    Var InterpreterStackFrame::OP_GetCachedFunc(Var instance, int32 index)
    {
        ActivationObjectEx *obj = (ActivationObjectEx*)ActivationObjectEx::FromVar(instance);

        FuncCacheEntry *entry = obj->GetFuncCacheEntry((uint)index);
        return entry->func;
    }

    void InterpreterStackFrame::OP_CommitScope(const unaligned OpLayoutAuxNoReg * playout)
    {
        const Js::PropertyIdArray *propIds = Js::ByteCodeReader::ReadPropertyIdArray(playout->Offset, this->GetFunctionBody());
        this->OP_CommitScopeHelper(playout, propIds);
    }

    void InterpreterStackFrame::OP_CommitScopeHelper(const unaligned OpLayoutAuxNoReg *playout, const PropertyIdArray *propIds)
    {
        ActivationObjectEx *obj = (ActivationObjectEx*)ActivationObjectEx::FromVar(/*GetReg(playout->R0)*/this->localClosure);
        ScriptFunction *func = obj->GetParentFunc();

        Assert(obj->GetParentFunc() == func);
        if (func->GetCachedScope() == obj)
        {
            PropertyId firstVarSlot = ActivationObjectEx::GetFirstVarSlot(propIds);

            Var undef = scriptContext->GetLibrary()->GetUndefined();

            for (uint i = firstVarSlot; i < propIds->count; i++)
            {
                obj->SetSlot(SetSlotArguments(propIds->elements[i], i, undef));
            }

            obj->SetCommit(true);
        }
    }

    Var InterpreterStackFrame::OP_NewScObjectSimple()
    {
        Var object = scriptContext->GetLibrary()->CreateObject(true);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(object));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {
            object = JavascriptProxy::AutoProxyWrapper(object);
        }
#endif
        return object;
    }

    void InterpreterStackFrame::OP_NewScObjectLiteral(const unaligned OpLayoutAuxiliary * playout )
    {
        const Js::PropertyIdArray *propIds = Js::ByteCodeReader::ReadPropertyIdArray(playout->Offset, this->GetFunctionBody());

        Var newObj = JavascriptOperators::NewScObjectLiteral(GetScriptContext(), propIds,
            this->GetFunctionBody()->GetObjectLiteralTypeRef(playout->C1));

        SetReg(playout->R0, newObj);
    }

    void InterpreterStackFrame::OP_NewScObjectLiteral_LS(const unaligned OpLayoutAuxiliary * playout, RegSlot& target)
    {
        const Js::PropertyIdArray *propIds = Js::ByteCodeReader::ReadPropertyIdArray(playout->Offset, this->GetFunctionBody());

        target = playout->R0;

        Var newObj = JavascriptOperators::NewScObjectLiteral(GetScriptContext(), propIds,
            this->GetFunctionBody()->GetObjectLiteralTypeRef(playout->C1));

        SetReg(playout->R0, newObj);

        target = Js::Constants::NoRegister;
    }

    void InterpreterStackFrame::OP_LdPropIds(const unaligned OpLayoutAuxiliary * playout)
    {
        const Js::PropertyIdArray *propIds = Js::ByteCodeReader::ReadPropertyIdArray(playout->Offset, this->GetFunctionBody());

        SetNonVarReg(playout->R0, (Var)propIds);
    }

    bool InterpreterStackFrame::IsCurrentLoopNativeAddr(void * codeAddr) const
    {
        if (this->GetCurrentLoopNum() == LoopHeader::NoLoop)
        {
            return false;
        }
        // TODO: Do more verification?
        return true;
    }

#if ENABLE_PROFILE_INFO
    void InterpreterStackFrame::OP_RecordImplicitCall(uint loopNumber)
    {
        Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(GetFunctionBody()));
        Assert(loopNumber < this->m_functionBody->GetLoopCount());

        FunctionBody* functionBody = this->m_functionBody;
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        ThreadContext * threadContext = scriptContext->GetThreadContext();
        dynamicProfileInfo->RecordLoopImplicitCallFlags(functionBody, loopNumber, threadContext->GetImplicitCallFlags());
    }

    template <LayoutSize layoutSize, bool profiled>
    const byte * InterpreterStackFrame::OP_ProfiledLoopStart(const byte * ip)
    {
        const uint32 C1 = m_reader.GetLayout<OpLayoutT_Unsigned1<LayoutSizePolicy<layoutSize>>>(ip)->C1;
        if(!profiled && !isAutoProfiling)
        {
            return ip;
        }

        ThreadContext *const threadContext = GetScriptContext()->GetThreadContext();
        threadContext->IncrementLoopDepth();

        // Save the implicit call flags. The interpreter may switch to profiling mode during LoopBodyStart, so always do this.
        Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(GetFunctionBody()));
        this->savedLoopImplicitCallFlags[C1] = threadContext->GetImplicitCallFlags();
        threadContext->SetImplicitCallFlags(ImplicitCall_None);

        this->currentLoopCounter = 0;

        if(!profiled)
        {
            return ip;
        }

        LayoutSize localLayoutSize;
        OpCode peekOp = m_reader.PeekOp(ip, localLayoutSize);
        Assert(peekOp != OpCode::LoopBodyStart);
        if (peekOp == OpCode::ProfiledLoopBodyStart)
        {
            Assert(localLayoutSize == layoutSize);
            ip += Js::OpCodeUtil::EncodedSize(peekOp, layoutSize);
            // We are doing JIT loop body. Process the first ProfiledLoopBodyStart to avoid recording
            // the implicit call before the first iteration
            uint32 C2 = m_reader.GetLayout<OpLayoutT_Unsigned1<LayoutSizePolicy<layoutSize>>>(ip)->C1;
            Assert(C1 == C2);
            (this->*opProfiledLoopBodyStart)(C1, layoutSize, true /* isFirstIteration */);
            return m_reader.GetIP();
        }

        return ip;
    }

    template <LayoutSize layoutSize, bool profiled>
    const byte * InterpreterStackFrame::OP_ProfiledLoopEnd(const byte * ip)
    {
        uint32 loopNumber = m_reader.GetLayout<OpLayoutT_Unsigned1<LayoutSizePolicy<layoutSize>>>(ip)->C1;
        if(!profiled && !isAutoProfiling)
        {
            return ip;
        }

        this->CheckIfLoopIsHot(this->currentLoopCounter);
        Js::FunctionBody *fn = this->function->GetFunctionBody();
        if (fn->HasDynamicProfileInfo())
        {
            fn->GetAnyDynamicProfileInfo()->SetLoopInterpreted(loopNumber);
            // If the counter is 0, there is a high chance that some config disabled tracking that information. (ie: -off:jitloopbody)
            // Assume it is valid for memop in this case.
            if (this->currentLoopCounter >= (uint)CONFIG_FLAG(MinMemOpCount) ||
                (this->currentLoopCounter == 0 && !this->m_functionBody->DoJITLoopBody())
            )
            {
                // This flag becomes relevant only if the loop has been interpreted
                fn->GetAnyDynamicProfileInfo()->SetMemOpMinReached(loopNumber);
            }
        }

        this->currentLoopCounter = 0;

        if (profiled)
        {
            Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(GetFunctionBody()));
            OP_RecordImplicitCall(loopNumber);

            if(switchProfileModeOnLoopEndNumber == loopNumber)
            {
                // Stop profiling since the jitted loop body would be exiting the loop
                Assert(!switchProfileMode);
                switchProfileMode = true;
                switchProfileModeOnLoopEndNumber = 0u - 1;
            }
        }

        // Restore the implicit call flags state and add with flags in the loop as well
        ThreadContext *const threadContext = GetScriptContext()->GetThreadContext();
        threadContext->AddImplicitCallFlags(this->savedLoopImplicitCallFlags[loopNumber]);

        threadContext->DecrementLoopDepth();
        return ip;
    }

    template <LayoutSize layoutSize, bool profiled>
    const byte * InterpreterStackFrame::OP_ProfiledLoopBodyStart(const byte * ip)
    {
        uint32 C1 = m_reader.GetLayout<OpLayoutT_Unsigned1<LayoutSizePolicy<layoutSize>>>(ip)->C1;

        if(profiled || isAutoProfiling)
        {
            this->currentLoopCounter++;
        }

        if (profiled)
        {
            OP_RecordImplicitCall(C1);
        }

        (this->*(profiled ? opProfiledLoopBodyStart : opLoopBodyStart))(C1, layoutSize, false /* isFirstIteration */);
        return m_reader.GetIP();
    }

    template<bool InterruptProbe, bool JITLoopBody>
    void InterpreterStackFrame::ProfiledLoopBodyStart(uint32 loopNumber, LayoutSize layoutSize, bool isFirstIteration)
    {
        Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(GetFunctionBody()));

        if (InterruptProbe)
        {
            this->DoInterruptProbe();
        }

        if (!JITLoopBody || this->IsInCatchOrFinallyBlock())
        {
            // For functions having try-catch-finally, jit loop bodies for loops that are contained only in a try block,
            // not even indirect containment in a Catch or Finally.
            return;
        }

        LoopHeader const * loopHeader = DoLoopBodyStart(loopNumber, layoutSize, false, isFirstIteration);
        Assert(loopHeader == nullptr || this->m_functionBody->GetLoopNumber(loopHeader) == loopNumber);
        if (loopHeader != nullptr)
        {
            // We executed jitted loop body, no implicit call information available for this loop
            uint currentOffset = m_reader.GetCurrentOffset();

            if (!loopHeader->Contains(currentOffset) || (m_reader.PeekOp() == OpCode::ProfiledLoopEnd))
            {
                // Restore the outer loop's implicit call flags
                scriptContext->GetThreadContext()->SetImplicitCallFlags(this->savedLoopImplicitCallFlags[loopNumber]);
            }
            else
            {
                // We bailout from the loop, just continue collect implicit call flags for this loop
            }
        }
    }
#else
template <LayoutSize layoutSize, bool profiled>
const byte * InterpreterStackFrame::OP_ProfiledLoopStart(const byte * ip)
{
    Assert(!profiled);
    return ip;
}

template <LayoutSize layoutSize, bool profiled>
const byte * InterpreterStackFrame::OP_ProfiledLoopEnd(const byte * ip)
{
    Assert(!profiled);
    return ip;
}

template <LayoutSize layoutSize, bool profiled>
const byte * InterpreterStackFrame::OP_ProfiledLoopBodyStart(const byte * ip)
{
    uint32 C1 = m_reader.GetLayout<OpLayoutT_Unsigned1<LayoutSizePolicy<layoutSize>>>(ip)->C1;

    Assert(!profiled);

    (this->*opLoopBodyStart)(C1, layoutSize, false /* isFirstIteration */);
    return m_reader.GetIP();
}


#endif

    template<bool InterruptProbe, bool JITLoopBody>
    void InterpreterStackFrame::LoopBodyStart(uint32 loopNumber, LayoutSize layoutSize, bool isFirstIteration)
    {
        if (InterruptProbe)
        {
            this->DoInterruptProbe();
        }

        if (!JITLoopBody || this->IsInCatchOrFinallyBlock())
        {
            // For functions having try-catch-finally, jit loop bodies for loops that are contained only in a try block,
            // not even indirect containment in a Catch or Finally.
            return;
        }

        DoLoopBodyStart(loopNumber, layoutSize, true, isFirstIteration);
    }

    LoopHeader const * InterpreterStackFrame::DoLoopBodyStart(uint32 loopNumber, LayoutSize layoutSize, const bool doProfileLoopCheck, const bool isFirstIteration)
    {
#if ENABLE_PROFILE_INFO
        class AutoRestoreLoopNumbers
        {
        private:
            InterpreterStackFrame *const interpreterStackFrame;
            uint32 loopNumber;
            bool doProfileLoopCheck;

        public:
            AutoRestoreLoopNumbers(InterpreterStackFrame *const interpreterStackFrame, uint32 loopNumber, bool doProfileLoopCheck)
                : interpreterStackFrame(interpreterStackFrame), loopNumber(loopNumber), doProfileLoopCheck(doProfileLoopCheck)
            {
                Assert(interpreterStackFrame->currentLoopNum == LoopHeader::NoLoop);
                interpreterStackFrame->currentLoopNum = loopNumber;
                interpreterStackFrame->m_functionBody->SetRecentlyBailedOutOfJittedLoopBody(false);
            }

            ~AutoRestoreLoopNumbers()
            {
                interpreterStackFrame->currentLoopNum = LoopHeader::NoLoop;
                interpreterStackFrame->currentLoopCounter = 0;
                Js::FunctionBody* fn = interpreterStackFrame->m_functionBody;
                if (fn->RecentlyBailedOutOfJittedLoopBody())
                {
                    if (doProfileLoopCheck && interpreterStackFrame->isAutoProfiling)
                    {
                        // Start profiling the loop after a bailout. Some bailouts require subsequent profile data collection such
                        // that the rejitted loop body would not bail out again for the same reason.
                        Assert(!interpreterStackFrame->switchProfileMode);
                        interpreterStackFrame->switchProfileMode = true;
                        Assert(interpreterStackFrame->switchProfileModeOnLoopEndNumber == 0u - 1);
                        interpreterStackFrame->switchProfileModeOnLoopEndNumber = loopNumber;
                    }
                }
                else
                {
                    if (interpreterStackFrame->switchProfileModeOnLoopEndNumber == loopNumber)
                    {
                        // Stop profiling since the jitted loop body would be exiting the loop
                        Assert(!interpreterStackFrame->switchProfileMode);
                        interpreterStackFrame->switchProfileMode = true;
                        interpreterStackFrame->switchProfileModeOnLoopEndNumber = 0u - 1;
                    }

                    interpreterStackFrame->scriptContext->GetThreadContext()->DecrementLoopDepth();
                }
            }
        };
#endif

        Js::FunctionBody* fn = this->m_functionBody;

        Assert(loopNumber < fn->GetLoopCount());
        Assert(!this->IsInCatchOrFinallyBlock());

        Js::LoopHeader *loopHeader = fn->GetLoopHeader(loopNumber);
        loopHeader->isInTry = (this->m_flags & Js::InterpreterStackFrameFlags_WithinTryBlock);

        Js::LoopEntryPointInfo * entryPointInfo = loopHeader->GetCurrentEntryPointInfo();

        if (fn->ForceJITLoopBody() && loopHeader->interpretCount == 0 &&
            (entryPointInfo != NULL && entryPointInfo->IsNotScheduled()))
        {
#if ENABLE_PROFILE_INFO
            if (Js::DynamicProfileInfo::EnableImplicitCallFlags(GetFunctionBody()))
            {
                scriptContext->GetThreadContext()->AddImplicitCallFlags(this->savedLoopImplicitCallFlags[loopNumber]);
            }
#endif

#if ENABLE_NATIVE_CODEGEN
            GenerateLoopBody(scriptContext->GetNativeCodeGenerator(), fn, loopHeader, entryPointInfo, fn->GetLocalsCount(), this->m_localSlots);
#endif
        }

#if ENABLE_NATIVE_CODEGEN
        // If we have JITted the loop, call the JITted code
        if (entryPointInfo != NULL && entryPointInfo->IsCodeGenDone())
        {
#if DBG_DUMP
            if (PHASE_TRACE1(Js::JITLoopBodyPhase) && CONFIG_FLAG(Verbose))
            {
                fn->DumpFunctionId(true);
                Output::Print(L": %-20s LoopBody Execute  Loop: %2d\n", fn->GetDisplayName(), loopNumber);
                Output::Flush();
            }
            loopHeader->nativeCount++;
#endif
#ifdef BGJIT_STATS
            entryPointInfo->MarkAsUsed();
#endif

            entryPointInfo->EnsureIsReadyToCall();

            RegSlot envReg = this->m_functionBody->GetEnvReg();
            if (envReg != Constants::NoRegister)
            {
                this->SetNonVarReg(envReg, this->LdEnv());
            }

            RegSlot localClosureReg = this->m_functionBody->GetLocalClosureReg();
            RegSlot localFrameDisplayReg = this->m_functionBody->GetLocalFrameDisplayReg();

            if (entryPointInfo->HasJittedStackClosure())
            {
                // The jitted code is expecting the closure registers to point to known stack locations where
                // the closures can be found and possibly boxed.
                // In a jitted loop body, those locations are the local closure fields on the interpreter instance.
                if (localClosureReg != Constants::NoRegister)
                {
                    this->SetNonVarReg(localClosureReg, &this->localClosure);
                }

                if (localFrameDisplayReg != Constants::NoRegister)
                {
                    this->SetNonVarReg(localFrameDisplayReg, &this->localFrameDisplay);
                }
            }
            else
            {
                // In non-stack-closure jitted code, the closure registers are expected to hold the addresses
                // of the actual structures.
                if (localClosureReg != Constants::NoRegister)
                {
                    this->SetNonVarReg(localClosureReg, this->localClosure);
                }

                if (localFrameDisplayReg != Constants::NoRegister)
                {
                    this->SetNonVarReg(localFrameDisplayReg, this->localFrameDisplay);
                }
            }

            uint32 innerScopeCount = this->m_functionBody->GetInnerScopeCount();
            for (uint32 i = 0; i < innerScopeCount; i++)
            {
                // As with the function-level scope, transfer the inner scopes from the interpreter's side storage
                // to their dedicated register slots.
                SetNonVarReg(this->m_functionBody->FirstInnerScopeReg() + i, InnerScopeFromIndex(i));
            }

            uint newOffset = 0;
            if (fn->GetIsAsmJsFunction())
            {
                AutoRestoreLoopNumbers autoRestore(this, loopNumber, doProfileLoopCheck);
                newOffset = this->CallAsmJsLoopBody((JavascriptMethod)entryPointInfo->address);
            }
            else
            {
                AutoRestoreLoopNumbers autoRestore(this, loopNumber, doProfileLoopCheck);
                newOffset = this->CallLoopBody((JavascriptMethod)entryPointInfo->address);
            }

            if (envReg != Constants::NoRegister)
            {
                SetNonVarReg(envReg, nullptr);
            }

            if (localClosureReg != Constants::NoRegister)
            {
                SetNonVarReg(localClosureReg, nullptr);
            }

            if (localFrameDisplayReg != Constants::NoRegister)
            {
                SetNonVarReg(localFrameDisplayReg, nullptr);
            }

            for (uint32 i = 0; i < innerScopeCount; i++)
            {
                // Get the (possibly updated) scopes from their registers and put them back in side storage.
                // (Getting the updated values may not be necessary, actually, but it can't hurt.)
                // Then null out the registers.
                RegSlot reg = this->m_functionBody->FirstInnerScopeReg() + i;
                SetInnerScopeFromIndex(i, GetNonVarReg(reg));
                SetNonVarReg(reg, nullptr);
            }

            Assert(Js::OpCodeUtil::GetOpCodeLayout(OpCode::ProfiledLoopBodyStart) == Js::OpLayoutType::Unsigned1);
            Assert(Js::OpCodeUtil::GetOpCodeLayout(OpCode::LoopBodyStart) == Js::OpLayoutType::Unsigned1);
            Assert(Js::OpCodeUtil::EncodedSize(Js::OpCode::LoopBodyStart, layoutSize) == Js::OpCodeUtil::EncodedSize(Js::OpCode::ProfiledLoopBodyStart, layoutSize));
            uint byteCodeSize = Js::OpCodeUtil::EncodedSize(Js::OpCode::LoopBodyStart, layoutSize);
            if (layoutSize == SmallLayout)
            {
                byteCodeSize += sizeof(OpLayoutUnsigned1_Small);
            }
            else if (layoutSize == MediumLayout)
            {
                byteCodeSize += sizeof(OpLayoutUnsigned1_Medium);
            }
            else
            {
                byteCodeSize += sizeof(OpLayoutUnsigned1_Large);
            }

            if (newOffset == loopHeader->startOffset || newOffset == m_reader.GetCurrentOffset() - byteCodeSize)
            {
                // If we bail out back the start of the loop, or start of this LoopBodyStart just skip and interpret the loop
                // instead of trying to start the loop body again

                // Increment the interpret count of the loop
                loopHeader->interpretCount++;
            }
            else
            {
                // we do not support this in asmjs, need to add support in IrBuilderAsmjs if we need this support for asmjs
                if (!entryPointInfo->GetIsAsmJSFunction())
                {
                    this->CheckIfLoopIsHot(loopHeader->profiledLoopCounter);
                }
                m_reader.SetCurrentOffset(newOffset);
            }

            return loopHeader;
        }
#endif

        // Increment the interpret count of the loop
        loopHeader->interpretCount += !isFirstIteration;

        const uint loopInterpretCount = GetFunctionBody()->GetLoopInterpretCount(loopHeader);
        if (loopHeader->interpretCount > loopInterpretCount)
        {
            if (this->scriptContext->GetConfig()->IsNoNative())
            {
                return nullptr;
            }

            if (!fn->DoJITLoopBody())
            {
                return nullptr;
            }

#if ENABLE_NATIVE_CODEGEN
            // If the job is not scheduled then we need to schedule it now.
            // It is possible a job was scheduled earlier and we find ourselves looking at the same entry point
            // again. For example, if the function with the loop was JITed and bailed out then as we finish
            // the call in the interpreter we might encounter a loop for which we had scheduled a JIT job before
            // the function was initially scheduled. In such cases, that old JIT job will complete. If it completes
            // successfully then we can go ahead and use it. If it fails then it will eventually revert to the
            // NotScheduled state. Since transitions from NotScheduled can only occur on the main thread,
            // by checking the state we are safe from racing with the JIT thread when looking at the other fields
            // of the entry point.
            if (entryPointInfo != NULL && entryPointInfo->IsNotScheduled())
            {
                GenerateLoopBody(scriptContext->GetNativeCodeGenerator(), fn, loopHeader, entryPointInfo, fn->GetLocalsCount(), this->m_localSlots);
            }
#endif
        }
#if ENABLE_PROFILE_INFO
        else if(
            doProfileLoopCheck &&
            isAutoProfiling &&
            loopHeader->interpretCount > fn->GetLoopProfileThreshold(loopInterpretCount))
        {
            // Start profiling the loop so that the jitted loop body will have some profile data to use
            Assert(!switchProfileMode);
            switchProfileMode = true;
            Assert(switchProfileModeOnLoopEndNumber == 0u - 1);
            switchProfileModeOnLoopEndNumber = loopNumber;
        }
#endif

        return nullptr;
    }

    void
    InterpreterStackFrame::CheckIfLoopIsHot(uint profiledLoopCounter)
    {
        Js::FunctionBody *fn = this->function->GetFunctionBody();
        if (!fn->GetHasHotLoop() &&  profiledLoopCounter > (uint)CONFIG_FLAG(JitLoopBodyHotLoopThreshold))
        {
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            if(PHASE_TRACE(Js::JITLoopBodyPhase, fn))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                Output::Print(
                    L"Speculate Jit set for this function with loopbody: function: %s (%s)\n",
                    fn->GetDisplayName(),
                    fn->GetDebugNumberSet(debugStringBuffer));
                Output::Flush();
            }
#endif
            fn->SetHasHotLoop();
        }
    }

    bool InterpreterStackFrame::CheckAndResetImplicitCall(DisableImplicitFlags prevDisableImplicitFlags, ImplicitCallFlags savedImplicitCallFlags)
    {
        ImplicitCallFlags curImplicitCallFlags = this->scriptContext->GetThreadContext()->GetImplicitCallFlags();
        if (curImplicitCallFlags > ImplicitCall_None)
        {
            //error implicit bit is set , reparse without asmjs
            this->scriptContext->GetThreadContext()->SetDisableImplicitFlags(prevDisableImplicitFlags);
            this->scriptContext->GetThreadContext()->SetImplicitCallFlags(savedImplicitCallFlags);
            return true;
        }
        return false;
    }

    uint
    InterpreterStackFrame::CallLoopBody(JavascriptMethod address)
    {
#ifdef _M_IX86
        void *savedEsp = NULL;
        __asm
        {
            // Save ESP
            mov savedEsp, esp

            // 8-byte align frame to improve floating point perf of our JIT'd code.
            and esp, -8

            // Add an extra 4-bytes to the stack since we'll be pushing 3 arguments
            push eax
        }
#endif

#if defined(_M_ARM32_OR_ARM64)
        // For ARM we need to make sure that pipeline is synchronized with memory/cache for newly jitted code.
        // Note: this does not seem to affect perf, but if it was, we could add a boolean isCalled to EntryPointInfo
        //       and do ISB only for 1st time this entry point is called (potential working set regression though).
        _InstructionSynchronizationBarrier();
#endif
        uint newOffset = ::Math::PointerCastToIntegral<uint>(address(function, CallInfo(CallFlags_InternalFrame, 1), this));

#ifdef _M_IX86
        _asm
        {
            // Restore ESP
            mov esp, savedEsp
        }
#endif
        return newOffset;
    }


    uint
        InterpreterStackFrame::CallAsmJsLoopBody(JavascriptMethod address)
    {
#ifdef _M_IX86
            void *savedEsp = NULL;
            __asm
            {
                // Save ESP
                mov savedEsp, esp
               // Add an extra 4-bytes to the stack since we'll be pushing 3 arguments
                push eax
            }
#endif

#if defined(_M_ARM32_OR_ARM64)
            // For ARM we need to make sure that pipeline is synchronized with memory/cache for newly jitted code.
            // Note: this does not seem to affect perf, but if it was, we could add a boolean isCalled to EntryPointInfo
            //       and do ISB only for 1st time this entry point is called (potential working set regression though).
            _InstructionSynchronizationBarrier();
#endif
            uint newOffset = ::Math::PointerCastToIntegral<uint>(address(function, CallInfo(CallFlags_InternalFrame, 1), this));

#ifdef _M_IX86
            _asm
            {
                // Restore ESP
                mov esp, savedEsp
            }
#endif
            return newOffset;
        }

    template <class T>
    void InterpreterStackFrame::OP_NewScObjectNoCtorFull(const unaligned T* playout)
    {
        Var function = GetReg(playout->R1);
        Var newObj = JavascriptOperators::NewScObjectNoCtorFull(function, GetScriptContext());
        SetReg(playout->R0, newObj);
    }

    ///----------------------------------------------------------------------------
    ///
    /// InterpreterStackFrame::OP_NewScObject
    ///
    /// OP_NewScObject() allocates a new DynamicObject and initializes it with an
    /// optional "constructor" function.
    ///
    /// NOTE: The return register must be carefully chosen to ensure proper
    /// behavior:
    /// 1. OpCode::NewInstance should never specify "R0" as the register to
    ///    store the new instance, because it will get whacked from the
    ///    "constructor" function's return value:
    ///
    ///     var a1 = Date();        <-- a1 = string returned from Date() function
    ///     var a2 = new Date();    <-- a2 = instance return from NewInstance.
    ///                                      Date()'s return value is thrown away.
    ///
    /// 2. If an exception is thrown during construction, the destination
    ///    variable / field should __not__ be modified.  Therefore, the destination
    ///    register should always be a temporary and never a valid local variable.
    ///    After successfully returning from the constructor function, the new
    ///    instance is valid and may be stored in its final destination variable /
    ///    field.
    ///
    /// OPCODE NewObject:
    ///     T1 = new DynamicObject(Function.Prototype)
    ///     OutArg[0] = T1
    ///     Call(Function, ArgCount)
    ///     Local[Return] = T1
    ///
    /// - R0: Destination "local" register
    /// - R1: Optional constructor JavascriptFunction instance or 'null'
    ///
    ///----------------------------------------------------------------------------
    template <class T, bool Profiled, bool ICIndex>
    void InterpreterStackFrame::OP_NewScObject_Impl(const unaligned T* playout, InlineCacheIndex inlineCacheIndex, const Js::AuxArray<uint32> *spreadIndices)
    {
        if (ICIndex)
        {
            Assert(inlineCacheIndex != Js::Constants::NoInlineCacheIndex);
        }
        Var newVarInstance =
#if ENABLE_PROFILE_INFO
            Profiled ?
                ProfiledNewScObject_Helper(
                    GetReg(playout->Function),
                    playout->ArgCount,
                    static_cast<const unaligned OpLayoutDynamicProfile<T> *>(playout)->profileId,
                    inlineCacheIndex,
                    spreadIndices) :
#endif
            NewScObject_Helper(GetReg(playout->Function), playout->ArgCount, spreadIndices);
        SetReg((RegSlot)playout->Return, newVarInstance);
    }

    template <class T, bool Profiled>
    void InterpreterStackFrame::OP_NewScObjArray_Impl(const unaligned T* playout, const Js::AuxArray<uint32> *spreadIndices)
    {
        // Always profile this operation when auto-profiling so that array type changes are tracked
#if ENABLE_PROFILE_INFO
        if (!Profiled && !isAutoProfiling)
#else
        Assert(!Profiled);
#endif
        {
            OP_NewScObject_Impl<T, Profiled, false>(playout, Js::Constants::NoInlineCacheIndex, spreadIndices);
            return;
        }

#if ENABLE_PROFILE_INFO
        Arguments args(CallInfo(CallFlags_New, playout->ArgCount), m_outParams);

        uint32 spreadSize = 0;
        if (spreadIndices != nullptr)
        {
            spreadSize = JavascriptFunction::GetSpreadSize(args, spreadIndices, scriptContext);

            // Allocate room on the stack for the spread args.
            Arguments outArgs(CallInfo(CallFlags_New, 0), nullptr);
            outArgs.Info.Count = spreadSize;
            const unsigned STACK_ARGS_ALLOCA_THRESHOLD = 8; // Number of stack args we allow before using _alloca
            Var stackArgs[STACK_ARGS_ALLOCA_THRESHOLD];
            size_t outArgsSize = 0;
            if (outArgs.Info.Count > STACK_ARGS_ALLOCA_THRESHOLD)
            {
                PROBE_STACK(scriptContext, outArgs.Info.Count * sizeof(Var) + Js::Constants::MinStackDefault); // args + function call
                outArgsSize = outArgs.Info.Count * sizeof(Var);
                outArgs.Values = (Var*)_alloca(outArgsSize);
            }
            else
            {
                outArgs.Values = stackArgs;
                outArgsSize = STACK_ARGS_ALLOCA_THRESHOLD * sizeof(Var);
                ZeroMemory(outArgs.Values, outArgsSize); // We may not use all of the elements
            }
            JavascriptFunction::SpreadArgs(args, outArgs, spreadIndices, scriptContext);

            SetReg(
                (RegSlot)playout->Return,
                ProfilingHelpers::ProfiledNewScObjArray(
                GetReg(playout->Function),
                outArgs,
                function,
                static_cast<const unaligned OpLayoutDynamicProfile2<T> *>(playout)->profileId,
                static_cast<const unaligned OpLayoutDynamicProfile2<T> *>(playout)->profileId2));
        }
        else
        {
            SetReg(
            (RegSlot)playout->Return,
            ProfilingHelpers::ProfiledNewScObjArray(
                GetReg(playout->Function),
                args,
                function,
                static_cast<const unaligned OpLayoutDynamicProfile2<T> *>(playout)->profileId,
                static_cast<const unaligned OpLayoutDynamicProfile2<T> *>(playout)->profileId2));
        }
        PopOut(playout->ArgCount);
#endif
    }

    void InterpreterStackFrame::OP_NewScObject_A_Impl(const unaligned OpLayoutAuxiliary * playout, RegSlot *target)
    {
        const Js::VarArrayVarCount * vars = Js::ByteCodeReader::ReadVarArrayVarCount(playout->Offset, this->GetFunctionBody());

        int count = Js::TaggedInt::ToInt32(vars->count);

        // Push the parameters to stack
        for (int i=0;i<count; i++)
        {
            SetOut( (ArgSlot)(i+1), vars->elements[i]);
        }

        Var newVarInstance = NewScObject_Helper(GetReg((RegSlot)playout->C1), (ArgSlot)count+1);
        SetReg((RegSlot)playout->R0, newVarInstance);

    }

    Var InterpreterStackFrame::NewScObject_Helper(Var target, ArgSlot ArgCount, const Js::AuxArray<uint32> *spreadIndices)
    {
        Arguments args(CallInfo(CallFlags_New, ArgCount), m_outParams);

        Var newVarInstance = JavascriptOperators::NewScObject(target, args, GetScriptContext(), spreadIndices);

        PopOut(ArgCount);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(newVarInstance));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {
            newVarInstance = JavascriptProxy::AutoProxyWrapper(newVarInstance);
            // this might come from a different scriptcontext.
            newVarInstance = CrossSite::MarshalVar(GetScriptContext(), newVarInstance);
        }
#endif
#ifdef ENABLE_BASIC_TELEMETRY
        {
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().NewScriptObject( target, args, newVarInstance );
        }
#endif
        return newVarInstance;
    }

#if ENABLE_PROFILE_INFO
    Var InterpreterStackFrame::ProfiledNewScObject_Helper(Var target, ArgSlot ArgCount, ProfileId profileId, InlineCacheIndex inlineCacheIndex, const Js::AuxArray<uint32> *spreadIndices)
    {
        Arguments args(CallInfo(CallFlags_New, ArgCount), m_outParams);

        Var newVarInstance =
            ProfilingHelpers::ProfiledNewScObject(
                target,
                args,
                GetFunctionBody(),
                profileId,
                inlineCacheIndex,
                spreadIndices);

        PopOut(ArgCount);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(newVarInstance));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {
            newVarInstance = JavascriptProxy::AutoProxyWrapper(newVarInstance);
            // this might come from a different scriptcontext.
            newVarInstance = CrossSite::MarshalVar(GetScriptContext(), newVarInstance);
        }
#endif
#ifdef TELEMETRY_PROFILED
        {
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().NewScriptObject( target, args, newVarInstance );
        }
#endif
        return newVarInstance;
    }
#endif

    template <typename T>
    void InterpreterStackFrame::OP_LdElementUndefined(const unaligned OpLayoutT_ElementU<T>* playout)
    {
        if (this->m_functionBody->IsEval())
        {
            JavascriptOperators::OP_LoadUndefinedToElementDynamic(GetReg(playout->Instance),
                this->m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetScriptContext());
        }
        else
        {
            JavascriptOperators::OP_LoadUndefinedToElement(GetReg(playout->Instance),
                this->m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex));
        }
    }

    template <typename T>
    void InterpreterStackFrame::OP_LdLocalElementUndefined(const unaligned OpLayoutT_ElementRootU<T>* playout)
    {
        if (this->m_functionBody->IsEval())
        {
            JavascriptOperators::OP_LoadUndefinedToElementDynamic(this->localClosure,
                this->m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetScriptContext());
        }
        else
        {
            JavascriptOperators::OP_LoadUndefinedToElement(this->localClosure,
                this->m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex));
        }
    }

    template <typename T>
    void InterpreterStackFrame::OP_LdElementUndefinedScoped(const unaligned OpLayoutT_ElementScopedU<T>* playout)
    {
        // Implicit root object as default instance
        JavascriptOperators::OP_LoadUndefinedToElementScoped(GetEnvForEvalCode(),
            this->m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetReg(Js::FunctionBody::RootObjectRegSlot), GetScriptContext());
    }

    void InterpreterStackFrame::OP_ChkUndecl(Var aValue)
    {
        if (this->scriptContext->IsUndeclBlockVar(aValue))
        {
            JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
        }
    }

    void InterpreterStackFrame::OP_ChkNewCallFlag()
    {
        if (!(this->m_callFlags & CallFlags_New))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew);
        }
    }

    void InterpreterStackFrame::OP_EnsureNoRootProperty(uint propertyIdIndex)
    {
        Var instance = this->GetRootObject();
        JavascriptOperators::OP_EnsureNoRootProperty(instance, this->m_functionBody->GetReferencedPropertyId(propertyIdIndex));
    }

    void InterpreterStackFrame::OP_EnsureNoRootRedeclProperty(uint propertyIdIndex)
    {
        Var instance = this->GetRootObject();
        JavascriptOperators::OP_EnsureNoRootRedeclProperty(instance, this->m_functionBody->GetReferencedPropertyId(propertyIdIndex));
    }

    void InterpreterStackFrame::OP_ScopedEnsureNoRedeclProperty(Var aValue, uint propertyIdIndex, Var aValue2)
    {
        Js::PropertyId propertyId = this->m_functionBody->GetReferencedPropertyId(propertyIdIndex);
        JavascriptOperators::OP_ScopedEnsureNoRedeclProperty((FrameDisplay*)aValue, propertyId, aValue2);
    }

    Var InterpreterStackFrame::OP_InitUndecl()
    {
        return this->scriptContext->GetLibrary()->GetUndeclBlockVar();
    }

    void InterpreterStackFrame::OP_InitUndeclSlot(Var aValue, int32 slot)
    {
        this->OP_StSlot(aValue, slot, this->scriptContext->GetLibrary()->GetUndeclBlockVar());
    }

    void InterpreterStackFrame::OP_TryCatch(const unaligned OpLayoutBr* playout)
    {
        Js::JavascriptExceptionObject* exception = NULL;
        try
        {
            this->nestedTryDepth++;
            // mark the stackFrame as 'in try block'
            this->m_flags |= InterpreterStackFrameFlags_WithinTryBlock;

            Js::JavascriptExceptionOperators::AutoCatchHandlerExists autoCatchHandlerExists(scriptContext);

            if (scriptContext->IsInDebugMode())
            {
                this->ProcessWithDebugging();
                this->TrySetRetOffset();
            }
            else
            {
                this->Process();
                this->TrySetRetOffset();
            }
        }
        catch (Js::JavascriptExceptionObject * exceptionObject)
        {
            // We are using C++ exception handling which does not unwind the stack in the catch block.
            // For stack overflow and OOM exceptions, we cannot run user code here because the stack is not unwind.
            exception = exceptionObject;
        }

        if (--this->nestedTryDepth == -1)
        {
            // unmark the stackFrame as 'in try block'
            this->m_flags &= ~InterpreterStackFrameFlags_WithinTryBlock;
        }

        // Now that the stack is unwound, let's run the catch block.
        if (exception)
        {
            if (exception->IsGeneratorReturnException())
            {
                // Generator return scenario, so no need to go into the catch block and we must rethrow to propagate the exception to down level
                throw exception;
            }

            exception = exception->CloneIfStaticExceptionObject(scriptContext);
            // We've got a JS exception. Grab the exception object and assign it to the
            // catch object's location, then call the handler (i.e., we consume the Catch op here).
            Var catchObject = exception->GetThrownObject(scriptContext);

            m_reader.SetCurrentRelativeOffset((const byte *)(playout + 1), playout->RelativeJumpOffset);

            LayoutSize layoutSize;
            OpCode catchOp = m_reader.ReadOp(layoutSize);
#ifdef BYTECODE_BRANCH_ISLAND
            if (catchOp == Js::OpCode::BrLong)
            {
                Assert(layoutSize == SmallLayout);
                auto playoutBrLong = m_reader.BrLong();
                m_reader.SetCurrentRelativeOffset((const byte *)(playoutBrLong + 1), playoutBrLong->RelativeJumpOffset);
                catchOp = m_reader.ReadOp(layoutSize);
            }
#endif
            AssertMsg(catchOp == OpCode::Catch, "Catch op not found at catch offset");
            RegSlot reg = layoutSize == SmallLayout ? m_reader.Reg1_Small()->R0 :
                layoutSize == MediumLayout ? m_reader.Reg1_Medium()->R0 : m_reader.Reg1_Large()->R0;
            SetReg(reg, catchObject);

            ResetOut();

            this->nestedCatchDepth++;
            // mark the stackFrame as 'in catch block'
            this->m_flags |= InterpreterStackFrameFlags_WithinCatchBlock;

            this->ProcessCatch();

            if (--this->nestedCatchDepth == -1)
            {
                // unmark the stackFrame as 'in catch block'
                this->m_flags &= ~InterpreterStackFrameFlags_WithinCatchBlock;
            }
        }
    }

    void InterpreterStackFrame::ProcessCatch()
    {
        if (this->scriptContext->IsInDebugMode())
        {
            this->DebugProcess();
        }
        else
        {
            this->Process();
        }
    }

    int InterpreterStackFrame::ProcessFinally()
    {
        this->nestedFinallyDepth++;
        // mark the stackFrame as 'in finally block'
        this->m_flags |= InterpreterStackFrameFlags_WithinFinallyBlock;

        int newOffset = 0;
        if (scriptContext->IsInDebugMode())
        {
            newOffset = ::Math::PointerCastToIntegral<int>(this->DebugProcess());
        }
        else
        {
            newOffset = ::Math::PointerCastToIntegral<int>(this->Process());
        }

        if (--this->nestedFinallyDepth == -1)
        {
            // unmark the stackFrame as 'in finally block'
            this->m_flags &= ~InterpreterStackFrameFlags_WithinFinallyBlock;
        }
        return newOffset;
    }

    void InterpreterStackFrame::ProcessTryCatchBailout(EHBailoutData * ehBailoutData, uint32 tryNestingDepth)
    {
        int catchOffset = ehBailoutData->catchOffset;
        Js::JavascriptExceptionObject* exception = NULL;

        if (catchOffset != 0)
        {
            try
            {
                this->nestedTryDepth++;
                // mark the stackFrame as 'in try block'
                this->m_flags |= InterpreterStackFrameFlags_WithinTryBlock;

                if (tryNestingDepth != 0)
                {
                    this->ProcessTryCatchBailout(ehBailoutData->child, --tryNestingDepth);
                }

                Js::JavascriptExceptionOperators::AutoCatchHandlerExists autoCatchHandlerExists(scriptContext);

                if (scriptContext->IsInDebugMode())
                {
                    this->ProcessWithDebugging();
                    this->TrySetRetOffset();
                }
                else
                {
                    this->Process();
                    this->TrySetRetOffset();
                }
            }
            catch (Js::JavascriptExceptionObject * exceptionObject)
            {
                // We are using C++ exception handling which does not unwind the stack in the catch block.
                // For stack overflow and OOM exceptions, we cannot run user code here because the stack is not unwind.
                exception = exceptionObject;
            }
        }
        else
        {
            this->nestedCatchDepth++;
            // mark the stackFrame as 'in catch block'
            this->m_flags |= InterpreterStackFrameFlags_WithinCatchBlock;

            if (tryNestingDepth != 0)
            {
                this->ProcessTryCatchBailout(ehBailoutData->child, --tryNestingDepth);
            }
            this->ProcessCatch();

            if (--this->nestedCatchDepth == -1)
            {
                // unmark the stackFrame as 'in catch block'
                this->m_flags &= ~InterpreterStackFrameFlags_WithinCatchBlock;
            }
            return;
        }

        if (--this->nestedTryDepth == -1)
        {
            // unmark the stackFrame as 'in try block'
            this->m_flags &= ~InterpreterStackFrameFlags_WithinTryBlock;
        }

        // Now that the stack is unwound, let's run the catch block.
        if (exception)
        {
            if (exception->IsGeneratorReturnException())
            {
                // Generator return scenario, so no need to go into the catch block and we must rethrow to propagate the exception to down level
                throw exception;
            }

            exception = exception->CloneIfStaticExceptionObject(scriptContext);
            // We've got a JS exception. Grab the exception object and assign it to the
            // catch object's location, then call the handler (i.e., we consume the Catch op here).
            Var catchObject = exception->GetThrownObject(scriptContext);

            m_reader.SetCurrentOffset(catchOffset);

            LayoutSize layoutSize;
            OpCode catchOp = m_reader.ReadOp(layoutSize);
#ifdef BYTECODE_BRANCH_ISLAND
            if (catchOp == Js::OpCode::BrLong)
            {
                Assert(layoutSize == SmallLayout);
                auto playoutBrLong = m_reader.BrLong();
                m_reader.SetCurrentRelativeOffset((const byte *)(playoutBrLong + 1), playoutBrLong->RelativeJumpOffset);
                catchOp = m_reader.ReadOp(layoutSize);
            }
#endif
            AssertMsg(catchOp == OpCode::Catch, "Catch op not found at catch offset");
            RegSlot reg = layoutSize == SmallLayout ? m_reader.Reg1_Small()->R0 :
                layoutSize == MediumLayout ? m_reader.Reg1_Medium()->R0 : m_reader.Reg1_Large()->R0;
            SetReg(reg, catchObject);

            ResetOut();

            this->nestedCatchDepth++;
            // mark the stackFrame as 'in catch block'
            this->m_flags |= InterpreterStackFrameFlags_WithinCatchBlock;

            this->ProcessCatch();

            if (--this->nestedCatchDepth == -1)
            {
                // unmark the stackFrame as 'in catch block'
                this->m_flags &= ~InterpreterStackFrameFlags_WithinCatchBlock;
            }
        }
        return;
    }

    void InterpreterStackFrame::TrySetRetOffset()
    {
        Assert(this->m_flags & Js::InterpreterStackFrameFlags_WithinTryBlock);
        // It may happen that a JITted loop body returned the offset of RET. If the loop body was
        // called from a try, the interpreter "Process()" should also just return.
        if (this->retOffset != 0)
        {
            m_reader.SetCurrentOffset(this->retOffset);
        }
    }

    bool InterpreterStackFrame::IsInCatchOrFinallyBlock()
    {
        return (this->m_flags & Js::InterpreterStackFrameFlags_WithinCatchBlock) ||
               (this->m_flags & Js::InterpreterStackFrameFlags_WithinFinallyBlock);
    }

    void InterpreterStackFrame::OP_ResumeCatch()
    {
        this->m_flags |= InterpreterStackFrameFlags_WithinCatchBlock;

        if (scriptContext->IsInDebugMode())
        {
            this->DebugProcess();
        }
        else
        {
            this->Process();
        }

        this->m_flags &= ~InterpreterStackFrameFlags_WithinCatchBlock;
    }

    /// ---------------------------------------------------------------------------------------------------
    /// The behavior we want is the following:
    /// - If the control leaves the user's try without throwing, execute the finally and continue
    ///   after the end of the try.
    /// - If the user code throws, catch this exception and then execute this finally while unwinding to
    ///   the handler (if any).
    /// ---------------------------------------------------------------------------------------------------
    void InterpreterStackFrame::ProcessTryFinally(const byte* ip, Js::JumpOffset jumpOffset, Js::RegSlot regException, Js::RegSlot regOffset, bool hasYield)
    {
        Js::JavascriptExceptionObject* pExceptionObject = nullptr;
        bool skipFinallyBlock = false;

        try
        {
            Js::Var result = nullptr;

            this->nestedTryDepth++;
            // mark the stackFrame as 'in try block'
            this->m_flags |= InterpreterStackFrameFlags_WithinTryBlock;

            if (scriptContext->IsInDebugMode())
            {
                result = this->ProcessWithDebugging();
            }
            else
            {
                result = this->Process();
            }

            if (result == nullptr)
            {
                Assert(hasYield);
                skipFinallyBlock = true;
            }
        }
        catch (Js::JavascriptExceptionObject * e)
        {
            pExceptionObject = e;
        }

        if (--this->nestedTryDepth == -1)
        {
            // unmark the stackFrame as 'in try block'
            this->m_flags &= ~InterpreterStackFrameFlags_WithinTryBlock;
        }

        if (skipFinallyBlock)
        {
            // A leave occurred due to a yield
            return;
        }

        // Save the current IP so execution can continue there if the finally doesn't
        // take control of the flow.
        int newOffset = 0;
        int currOffset = m_reader.GetCurrentOffset();
        if (hasYield)
        {
            // save the exception if there is one to a register in case we yield during the finally block
            // and need to get that exception object back upon resume in OP_ResumeFinally
            SetNonVarReg(regException, pExceptionObject);
            SetNonVarReg(regOffset, reinterpret_cast<Js::Var>(currOffset));
        }

        if (pExceptionObject && !pExceptionObject->IsGeneratorReturnException())
        {
            // Clone static exception object early in case finally block overwrites it
            pExceptionObject = pExceptionObject->CloneIfStaticExceptionObject(scriptContext);
        }

        if (pExceptionObject && scriptContext->IsInDebugMode() &&
            pExceptionObject != scriptContext->GetThreadContext()->GetPendingSOErrorObject())
        {
            // Swallowing an exception that has triggered a finally is not implemented
            // (This appears to be the same behavior as ie8)
            pExceptionObject->SetDebuggerSkip(false);
        }

        // Call into the finally by setting the IP, consuming the Finally, and letting the interpreter recurse.
        m_reader.SetCurrentRelativeOffset(ip, jumpOffset);

        ResetOut();

        newOffset = this->ProcessFinally();

        bool endOfFinallyBlock = newOffset == 0;
        if (endOfFinallyBlock)
        {
            // Finally completed without taking over the flow. Resume where we left off before calling it.
            m_reader.SetCurrentOffset(currOffset);
        }
        else
        {
            // Finally seized the flow with a jump out of its scope. Resume at the jump target and
            // force the runtime to return to this frame without executing the catch.
            m_reader.SetCurrentOffset(newOffset);

            return;
        }

        if (pExceptionObject && (endOfFinallyBlock || !pExceptionObject->IsGeneratorReturnException()))
        {
            throw pExceptionObject;
        }
    }

    void InterpreterStackFrame::OP_TryFinally(const unaligned OpLayoutBr* playout)
    {
        ProcessTryFinally((const byte*)(playout + 1), playout->RelativeJumpOffset);
    }

    void InterpreterStackFrame::OP_TryFinallyWithYield(const byte* ip, Js::JumpOffset jumpOffset, Js::RegSlot regException, Js::RegSlot regOffset)
    {
        ProcessTryFinally(ip, jumpOffset, regException, regOffset, true);
    }

    void InterpreterStackFrame::OP_ResumeFinally(const byte* ip, Js::JumpOffset jumpOffset, RegSlot exceptionRegSlot, RegSlot offsetRegSlot)
    {
        this->m_flags |= InterpreterStackFrameFlags_WithinFinallyBlock;

        int newOffset = 0;
        if (scriptContext->IsInDebugMode())
        {
            newOffset = ::Math::PointerCastToIntegral<int>(this->DebugProcess());
        }
        else
        {
            newOffset = ::Math::PointerCastToIntegral<int>(this->Process());
        }

        this->m_flags &= ~InterpreterStackFrameFlags_WithinFinallyBlock;

        bool endOfFinallyBlock = newOffset == 0;
        if (endOfFinallyBlock)
        {
            // Finally completed without taking over the flow. Resume where we left off before calling it.
            int currOffset = ::Math::PointerCastToIntegral<int>(GetNonVarReg(offsetRegSlot));
            m_reader.SetCurrentOffset(currOffset);
        }
        else
        {
            // Finally seized the flow with a jump out of its scope. Resume at the jump target and
            // force the runtime to return to this frame without executing the catch.
            m_reader.SetCurrentOffset(newOffset);

            return;
        }

        Js::JavascriptExceptionObject* exceptionObj = (Js::JavascriptExceptionObject*)GetNonVarReg(exceptionRegSlot);
        if (exceptionObj && (endOfFinallyBlock || !exceptionObj->IsGeneratorReturnException()))
        {
            throw exceptionObj;
        }
    }

    template <typename T>
    void InterpreterStackFrame::OP_IsInst(const unaligned T* playout)
    {
        Var instance = GetReg(playout->R1);
        Var function = GetReg(playout->R2);
        IsInstInlineCache *inlineCache = this->GetIsInstInlineCache(playout->inlineCacheIndex);
        ScriptContext* scriptContext = GetScriptContext();

        Var result = JavascriptOperators::OP_IsInst(instance, function, scriptContext, inlineCache);

#ifdef ENABLE_BASIC_TELEMETRY
        {
            this->scriptContext->GetTelemetry().GetOpcodeTelemetry().IsInstanceOf(instance, function, result);
        }
#endif

        SetReg(playout->R0, result);
    }

    template <typename T>
    void InterpreterStackFrame::OP_ApplyArgs(const unaligned OpLayoutT_Reg5<T> * playout)
    {
        // Always save and restore implicit call flags when calling out
        // REVIEW: Can we avoid it if we don't collect dynamic profile info?
        ThreadContext * threadContext = scriptContext->GetThreadContext();
        Js::ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        // Currently ApplyArgs is equivalent to CallFldVoid (where we don't use the return value)
        Var v=GetNonVarReg(playout->R4);
        JavascriptOperators::OP_ApplyArgs(GetReg(playout->R1),GetReg(playout->R2),
            (void**)GetNonVarReg(playout->R3),*((CallInfo*)&v),GetScriptContext());
        threadContext->SetImplicitCallFlags(savedImplicitCallFlags);
    }

    void InterpreterStackFrame::OP_SpreadArrayLiteral(const unaligned OpLayoutReg2Aux * playout)
    {
        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        Var instance = GetReg(playout->R1);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        const Js::AuxArray<uint32> *spreadIndices = m_reader.ReadAuxArray<uint32>(playout->Offset, this->GetFunctionBody());
        ScriptContext* scriptContext = GetScriptContext();
        Var result =  JavascriptArray::SpreadArrayArgs(instance, spreadIndices, scriptContext);

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);

        SetReg(playout->R0, result);
    }

    FrameDisplay *
    InterpreterStackFrame::OP_LdInnerFrameDisplay(void *argHead, void *argEnv, ScriptContext *scriptContext)
    {
        JavascriptOperators::CheckInnerFrameDisplayArgument(argHead);
        return OP_LdFrameDisplay(argHead, argEnv, scriptContext);
    }

    FrameDisplay *
    InterpreterStackFrame::OP_LdInnerFrameDisplayNoParent(void *argHead, ScriptContext *scriptContext)
    {
        JavascriptOperators::CheckInnerFrameDisplayArgument(argHead);
        return OP_LdFrameDisplayNoParent<true>(argHead, scriptContext);
    }

    FrameDisplay *
    InterpreterStackFrame::OP_LdFrameDisplay(void *argHead, void *argEnv, ScriptContext *scriptContext)
    {
        FrameDisplay *frameDisplay;
        bool strict = this->m_functionBody->GetIsStrictMode();
        if (strict)
        {
            frameDisplay = JavascriptOperators::OP_LdStrictFrameDisplay(argHead, argEnv, scriptContext);
        }
        else
        {
            frameDisplay = JavascriptOperators::OP_LdFrameDisplay(argHead, argEnv, scriptContext);
        }
        return frameDisplay;
    }

    FrameDisplay *
    InterpreterStackFrame::OP_LdFrameDisplaySetLocal(void *argHead, void *argEnv, ScriptContext *scriptContext)
    {
        FrameDisplay *frameDisplay = OP_LdFrameDisplay(argHead, argEnv, scriptContext);
        this->SetLocalFrameDisplay(frameDisplay);
        return frameDisplay;
    }

    FrameDisplay *
    InterpreterStackFrame::NewFrameDisplay(void *argHead, void *argEnv)
    {
        FrameDisplay *frameDisplay;
        bool strict = this->m_functionBody->GetIsStrictMode();

        if (!this->m_functionBody->DoStackFrameDisplay() || !this->GetLocalFrameDisplay())
        {
            // Null local frame display probably indicates that we bailed out of an inlinee.
            // Once we support stack closures in inlined functions, we can just assert that this value
            // is never null if we should be allocating on the stack.
            return this->OP_LdFrameDisplaySetLocal(argHead, argEnv, this->GetScriptContext());
        }

        frameDisplay = this->GetLocalFrameDisplay();
        Assert(frameDisplay != nullptr);

        frameDisplay->SetTag(true);
        frameDisplay->SetStrictMode(strict);
        frameDisplay->SetLength(this->m_functionBody->GetEnvDepth() + 1);
        Assert(frameDisplay->GetLength() == ((FrameDisplay*)argEnv)->GetLength() + 1);

        for (uint i = 0; i < ((FrameDisplay*)argEnv)->GetLength(); i++)
        {
            frameDisplay->SetItem(i + 1, ((FrameDisplay*)argEnv)->GetItem(i));
        }
        frameDisplay->SetItem(0, argHead);

        return frameDisplay;
    }

    template<bool innerFD>
    FrameDisplay *
    InterpreterStackFrame::OP_LdFrameDisplayNoParent(void *argHead, ScriptContext *scriptContext)
    {
        FrameDisplay *frameDisplay;
        bool strict = this->m_functionBody->GetIsStrictMode();

        Var argEnv = nullptr;
        if (innerFD && this->m_functionBody->GetLocalFrameDisplayReg() != Constants::NoRegister)
        {
            argEnv = this->GetLocalFrameDisplay();
        }
        if (argEnv == nullptr && this->m_functionBody->GetEnvReg() != Constants::NoRegister)
        {
            argEnv = this->LdEnv();
        }

        if (argEnv == nullptr)
        {
            if (strict)
            {
                frameDisplay = JavascriptOperators::OP_LdStrictFrameDisplayNoParent(argHead, scriptContext);
            }
            else
            {
                frameDisplay = JavascriptOperators::OP_LdFrameDisplayNoParent(argHead, scriptContext);
            }
        }
        else
        {
            if (strict)
            {
                frameDisplay = JavascriptOperators::OP_LdStrictFrameDisplay(argHead, argEnv, scriptContext);
            }
            else
            {
                frameDisplay = JavascriptOperators::OP_LdFrameDisplay(argHead, argEnv, scriptContext);
            }
        }
        return frameDisplay;
    }

    FrameDisplay *
    InterpreterStackFrame::OP_LdFuncExprFrameDisplaySetLocal(void *argHead1, void *argHead2, ScriptContext *scriptContext)
    {
        FrameDisplay *frameDisplay = OP_LdFrameDisplayNoParent<false>(argHead2, scriptContext);
        frameDisplay = OP_LdFrameDisplay(argHead1, frameDisplay, scriptContext);
        this->SetLocalFrameDisplay(frameDisplay);
        return frameDisplay;
    }

    FrameDisplay* InterpreterStackFrame::GetLocalFrameDisplay() const
    {
        return this->localFrameDisplay;
    }

    void InterpreterStackFrame::SetLocalFrameDisplay(FrameDisplay* frameDisplay)
    {
        this->localFrameDisplay = frameDisplay;
    }

    Var InterpreterStackFrame::GetLocalClosure() const
    {
        return this->localClosure;
    }

    void InterpreterStackFrame::SetLocalClosure(Var closure)
    {
        this->localClosure = closure;
    }

    void
    InterpreterStackFrame::OP_NewInnerScopeSlots(uint innerScopeIndex, uint count, int scopeIndex, ScriptContext *scriptContext, FunctionBody *functionBody)
    {
        Var * slotArray;

        slotArray =
            JavascriptOperators::OP_NewScopeSlotsWithoutPropIds(count, scopeIndex, scriptContext, functionBody);
        this->SetInnerScopeFromIndex(innerScopeIndex, slotArray);
    }

    template <typename T>
    void InterpreterStackFrame::OP_CloneInnerScopeSlots(const unaligned OpLayoutT_Unsigned1<T> *playout)
    {
        uint innerScopeIndex = playout->C1;
        Var * slotArray;

        slotArray = (Var*)this->InnerScopeFromIndex(innerScopeIndex);
        slotArray = JavascriptOperators::OP_CloneScopeSlots(slotArray, scriptContext);
        this->SetInnerScopeFromIndex(innerScopeIndex, slotArray);
    }

    template <typename T>
    void InterpreterStackFrame::OP_CloneBlockScope(const unaligned OpLayoutT_Unsigned1<T> *playout)
    {
        uint innerScopeIndex = playout->C1;
        Var scope = this->InnerScopeFromIndex(innerScopeIndex);
        BlockActivationObject* blockScope = BlockActivationObject::FromVar(scope);

        scope = JavascriptOperators::OP_CloneBlockScope(blockScope, scriptContext);
        this->SetInnerScopeFromIndex(innerScopeIndex, scope);
    }

    Var *
    InterpreterStackFrame::NewScopeSlots(unsigned int size, ScriptContext *scriptContext, Var scope)
    {
        Var * slotArray;

        slotArray = JavascriptOperators::OP_NewScopeSlots(size, scriptContext, scope);
        this->SetLocalClosure(slotArray);
        return slotArray;
    }

    Var *
    InterpreterStackFrame::NewScopeSlots()
    {
        Var * slotArray;
        FunctionBody * functionBody = this->m_functionBody;
        uint scopeSlotCount = functionBody->scopeSlotArraySize;
        Assert(scopeSlotCount != 0);

        if (!functionBody->DoStackScopeSlots())
        {
            return this->NewScopeSlots(
                scopeSlotCount + ScopeSlots::FirstSlotIndex, this->GetScriptContext(), (Var)functionBody);
        }

        slotArray = (Var*)this->GetLocalClosure();
        Assert(slotArray != nullptr);

        ScopeSlots scopeSlots(slotArray);
        scopeSlots.SetCount(scopeSlotCount);
        scopeSlots.SetScopeMetadata((Var)functionBody);
        Var undef = functionBody->GetScriptContext()->GetLibrary()->GetUndefined();
        for (unsigned int i = 0; i < scopeSlotCount; i++)
        {
            scopeSlots.Set(i, undef);
        }

        return slotArray;
    }

    Var
    InterpreterStackFrame::NewScopeObject()
    {
        Var scopeObject;

        if (m_functionBody->HasCachedScopePropIds())
        {
            const Js::PropertyIdArray *propIds =
                Js::ByteCodeReader::ReadPropertyIdArray(0, this->GetFunctionBody(), ActivationObjectEx::ExtraSlotCount());
            Var funcExpr = this->GetFunctionExpression();
            PropertyId objectId = ActivationObjectEx::GetLiteralObjectRef(propIds);
            scopeObject = JavascriptOperators::OP_InitCachedScope(funcExpr, propIds,
                this->GetFunctionBody()->GetObjectLiteralTypeRef(objectId),
                propIds->hasNonSimpleParams, GetScriptContext());

        }
        else
        {
            scopeObject = JavascriptOperators::OP_NewScopeObject(GetScriptContext());
        }
        this->SetLocalClosure(scopeObject);
        return scopeObject;
    }

    FrameDisplay *
    InterpreterStackFrame::GetFrameDisplayForNestedFunc() const
    {
        if (this->localFrameDisplay == nullptr)
        {
            return (FrameDisplay*)LdEnv();
        }
        return this->localFrameDisplay;
    }

    template <class T>
    void InterpreterStackFrame::OP_NewStackScFunc(const unaligned T * playout)
    {
        uint funcIndex = playout->SlotIndex;
        FrameDisplay *frameDisplay = this->GetFrameDisplayForNestedFunc();
        SetRegAllowStackVarEnableOnly(playout->Value,
            StackScriptFunction::OP_NewStackScFunc(frameDisplay,
                reinterpret_cast<Js::FunctionProxy**>(this->m_functionBody->GetNestedFuncReference(funcIndex)),
                this->GetStackNestedFunction(funcIndex)));
    }

    template <class T>
    void InterpreterStackFrame::OP_NewInnerStackScFunc(const unaligned T * playout)
    {
        uint funcIndex = playout->SlotIndex;
        FrameDisplay *frameDisplay = (FrameDisplay*)GetNonVarReg(playout->Instance);
        SetRegAllowStackVarEnableOnly(playout->Value,
            StackScriptFunction::OP_NewStackScFunc(frameDisplay,
                reinterpret_cast<Js::FunctionProxy**>(this->m_functionBody->GetNestedFuncReference(funcIndex)),
                this->GetStackNestedFunction(funcIndex)));
    }

    template <class T>
    void InterpreterStackFrame::OP_DeleteFld(const unaligned T * playout)
    {
        Var result = JavascriptOperators::OP_DeleteProperty(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetScriptContext());
        SetReg(playout->Value, result);
    }

    template <class T>
    void InterpreterStackFrame::OP_DeleteLocalFld(const unaligned T * playout)
    {
        Var result = JavascriptOperators::OP_DeleteProperty(this->localClosure, m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetScriptContext());
        SetReg(playout->Instance, result);
    }

    template <class T>
    void InterpreterStackFrame::OP_DeleteRootFld(const unaligned T * playout)
    {
        Var result = JavascriptOperators::OP_DeleteRootProperty(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetScriptContext());
        SetReg(playout->Value, result);
    }

    template <class T>
    void InterpreterStackFrame::OP_DeleteFldStrict(const unaligned T * playout)
    {
        Var result = JavascriptOperators::OP_DeleteProperty(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetScriptContext(), PropertyOperation_StrictMode);
        SetReg(playout->Value, result);
    }

    template <class T>
    void InterpreterStackFrame::OP_DeleteRootFldStrict(const unaligned T * playout)
    {
        Var result = JavascriptOperators::OP_DeleteRootProperty(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetScriptContext(), PropertyOperation_StrictMode);
        SetReg(playout->Value, result);
    }

    template <typename T>
    void InterpreterStackFrame::OP_ScopedDeleteFld(const unaligned OpLayoutT_ElementScopedC<T> * playout)
    {
        // Implicit root object as default instance
        Var result = JavascriptOperators::OP_DeletePropertyScoped(GetEnvForEvalCode(),
            m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex),
            GetReg(Js::FunctionBody::RootObjectRegSlot), GetScriptContext());
        SetReg(playout->Value, result);
    }

    template <typename T>
    void InterpreterStackFrame::OP_ScopedDeleteFldStrict(const unaligned OpLayoutT_ElementScopedC<T> * playout)
    {
        // Implicit root object as default instance
        Var result = JavascriptOperators::OP_DeletePropertyScoped(GetEnvForEvalCode(),
            m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex),
            GetReg(Js::FunctionBody::RootObjectRegSlot), GetScriptContext(), PropertyOperation_StrictMode);
        SetReg(playout->Value, result);
    }

    template <class T>
    void InterpreterStackFrame::OP_ScopedLdInst(const unaligned T * playout)
    {
        Var thisVar;
        Var rootObject = GetFunctionBody()->GetRootObject();
        Var result = JavascriptOperators::OP_GetInstanceScoped(GetEnvForEvalCode(),
            m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), rootObject, &thisVar, GetScriptContext());
        SetReg(playout->Value, result);
        SetReg(playout->Value2, thisVar);
    }

    template <typename T>
    void InterpreterStackFrame::OP_ScopedInitFunc(const unaligned OpLayoutT_ElementScopedC<T> * playout)
    {
        JavascriptOperators::OP_InitFuncScoped(GetEnvForEvalCode(),
            m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex),
            GetReg(playout->Value), GetReg(Js::FunctionBody::RootObjectRegSlot), GetScriptContext());
    }

    template <class T>
    void InterpreterStackFrame::OP_ClearAttributes(const unaligned T * playout)
    {
        JavascriptOperators::OP_ClearAttributes(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitGetFld(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitGetter(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetReg(playout->Value));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitSetFld(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitSetter(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetReg(playout->Value));
    }

    template <class T>
    void InterpreterStackFrame::OP_InitSetElemI(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitElemSetter(
            GetReg(playout->Instance),
            GetReg(playout->Element),
            GetReg(playout->Value),
            m_functionBody->GetScriptContext()
        );
    }

    template <class T>
    void InterpreterStackFrame::OP_InitGetElemI(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitElemGetter(
            GetReg(playout->Instance),
            GetReg(playout->Element),
            GetReg(playout->Value),
            m_functionBody->GetScriptContext()
        );
    }

    template <class T>
    void InterpreterStackFrame::OP_InitComputedProperty(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitComputedProperty(
            GetReg(playout->Instance),
            GetReg(playout->Element),
            GetReg(playout->Value),
            m_functionBody->GetScriptContext()
            );
    }

    template <class T>
    void InterpreterStackFrame::OP_InitProto(const unaligned T * playout)
    {
        JavascriptOperators::OP_InitProto(GetReg(playout->Instance), m_functionBody->GetReferencedPropertyId(playout->PropertyIdIndex), GetReg(playout->Value));
    }

    void InterpreterStackFrame::DoInterruptProbe()
    {
        PROBE_STACK(scriptContext, 0);
    }

    void InterpreterStackFrame::InitializeStackFunctions(StackScriptFunction * scriptFunctions)
    {
        this->stackNestedFunctions = scriptFunctions;
        FunctionBody * functionBody = this->m_functionBody;
        uint nestedCount = functionBody->GetNestedCount();
        for (uint i = 0; i < nestedCount; i++)
        {
            StackScriptFunction * stackScriptFunction = scriptFunctions + i;
            FunctionProxy* nestedProxy = functionBody->GetNestedFunc(i);
            ScriptFunctionType* type = nestedProxy->EnsureDeferredPrototypeType();
            new (stackScriptFunction)StackScriptFunction(nestedProxy, type);
        }
    }

    StackScriptFunction * InterpreterStackFrame::GetStackNestedFunction(uint index)
    {
        Assert(index < this->m_functionBody->GetNestedCount());
        // Re-check if we have disable stack nested function
        if (this->m_functionBody->DoStackNestedFunc())
        {
            return this->stackNestedFunctions + index;
        }
        return nullptr;
    }

    void InterpreterStackFrame::SetExecutingStackFunction(ScriptFunction * scriptFunction)
    {
        Assert(ThreadContext::IsOnStack(this->function));
        Assert(this->m_functionBody == scriptFunction->GetFunctionBody());
        this->function = scriptFunction;
    }

    DWORD_PTR InterpreterStackFrame::GetStackAddress() const
    {
        return m_stackAddress;
    }

    void* InterpreterStackFrame::GetAddressOfReturnAddress() const
    {
        return this->addressOfReturnAddress;
    }

    template <class T>
    const byte * InterpreterStackFrame::OP_Br(const unaligned T * playout)
    {
        return m_reader.SetCurrentRelativeOffset((const byte *)(playout + 1), playout->RelativeJumpOffset);
    }

    template <class T>
    void InterpreterStackFrame::OP_InitClass(const unaligned OpLayoutT_Class<T> * playout)
    {
        JavascriptOperators::OP_InitClass(GetReg(playout->Constructor), playout->Extends != Js::Constants::NoRegister ? GetReg(playout->Extends) : NULL, GetScriptContext());
    }

    template <class T>
    void InterpreterStackFrame::OP_EmitTmpRegCount(const unaligned OpLayoutT_Unsigned1<T> * playout)
    {
        this->scriptContext->GetDebugContext()->GetProbeContainer()->SetCurrentTmpRegCount(playout->C1);
    }

    Var InterpreterStackFrame::OP_LdSuper(ScriptContext * scriptContext)
    {
        return JavascriptOperators::OP_LdSuper(function, scriptContext);
    }

    Var InterpreterStackFrame::OP_LdSuperCtor(ScriptContext * scriptContext)
    {
        return JavascriptOperators::OP_LdSuperCtor(function, scriptContext);
    }

    Var InterpreterStackFrame::OP_ScopedLdSuper(ScriptContext * scriptContext)
    {
        return JavascriptOperators::OP_ScopedLdSuper(function, scriptContext);
    }

    Var InterpreterStackFrame::OP_ScopedLdSuperCtor(ScriptContext * scriptContext)
    {
        return JavascriptOperators::OP_ScopedLdSuperCtor(function, scriptContext);
    }

    void InterpreterStackFrame::ValidateRegValue(Var value, bool allowStackVar, bool allowStackVarOnDisabledStackNestedFunc) const
    {
#if DBG
        if (value != nullptr && !TaggedNumber::Is(value))
        {
            if (!allowStackVar || !this->m_functionBody->DoStackNestedFunc())
            {
                Assert(!ThreadContext::IsOnStack(value)
                    || (allowStackVar && allowStackVarOnDisabledStackNestedFunc && StackScriptFunction::IsBoxed(value)));
            }
            Assert(!CrossSite::NeedMarshalVar(value, GetScriptContext()));
        }
#endif
    }

    template <typename RegSlotType>
    Var InterpreterStackFrame::GetReg(RegSlotType localRegisterID) const
    {
        Var value = m_localSlots[localRegisterID];
        ValidateRegValue(value);
        return value;
    }

    template <typename RegSlotType>
    void InterpreterStackFrame::SetReg(RegSlotType localRegisterID, Var value)
    {
        Assert(localRegisterID == 0 || localRegisterID >= m_functionBody->GetConstantCount());
        ValidateRegValue(value);
        m_localSlots[localRegisterID] = value;
    }

    template <typename T>
    T InterpreterStackFrame::GetRegRaw(RegSlot localRegisterID) const
    {
        return (T)m_localIntSlots[localRegisterID];
    }

    // specialized version for doubles
    template <>
    double InterpreterStackFrame::GetRegRaw(RegSlot localRegisterID) const
    {
        return (double)m_localDoubleSlots[localRegisterID];
    }

    template <>
    float InterpreterStackFrame::GetRegRaw(RegSlot localRegisterID) const
    {
        return (float)m_localFloatSlots[localRegisterID];
    }

    template <typename T>
    void InterpreterStackFrame::SetRegRaw(RegSlot localRegisterID, T bValue)
    {
        m_localIntSlots[localRegisterID] = (int)bValue;
    }

    template <>
    void InterpreterStackFrame::SetRegRaw(RegSlot localRegisterID, float bValue)
    {
        m_localFloatSlots[localRegisterID] = (float)bValue;
    }

    template <>
    void InterpreterStackFrame::SetRegRaw(RegSlot localRegisterID, double bValue)
    {
        m_localDoubleSlots[localRegisterID] = bValue;
    }

    template <typename RegSlotType>
    int InterpreterStackFrame::GetRegRawInt(RegSlotType localRegisterID) const
    {
        return m_localIntSlots[localRegisterID];
    }
    template <typename RegSlotType>
    double InterpreterStackFrame::GetRegRawDouble(RegSlotType localRegisterID) const
    {
        return m_localDoubleSlots[localRegisterID];
    }

    template <typename RegSlotType>
    float InterpreterStackFrame::GetRegRawFloat(RegSlotType localRegisterID) const
    {
        return m_localFloatSlots[localRegisterID];
    }

    template <typename RegSlotType>
    void InterpreterStackFrame::SetRegRawInt(RegSlotType localRegisterID, int bValue)
    {
        m_localIntSlots[localRegisterID] = bValue;
    }

    template <typename RegSlotType>
    void InterpreterStackFrame::SetRegRawDouble(RegSlotType localRegisterID, double bValue)
    {
        m_localDoubleSlots[localRegisterID] = bValue;
    }

    template <typename RegSlotType>
    void InterpreterStackFrame::SetRegRawFloat(RegSlotType localRegisterID, float bValue)
    {
        m_localFloatSlots[localRegisterID] = bValue;
    }

    template <typename RegSlotType>
    Var InterpreterStackFrame::GetRegAllowStackVar(RegSlotType localRegisterID) const
    {
        Var value = m_localSlots[localRegisterID];
        ValidateRegValue(value, true);
        return value;
    }

    template <typename RegSlotType>
    void InterpreterStackFrame::SetRegAllowStackVar(RegSlotType localRegisterID, Var value)
    {
        Assert(localRegisterID == 0 || localRegisterID >= m_functionBody->GetConstantCount());
        ValidateRegValue(value, true);
        m_localSlots[localRegisterID] = value;
    }

    template <typename RegSlotType>
    Var InterpreterStackFrame::GetRegAllowStackVarEnableOnly(RegSlotType localRegisterID) const
    {
        Var value = m_localSlots[localRegisterID];
        ValidateRegValue(value, true, false);
        return value;
    }

    template <typename RegSlotType>
    void InterpreterStackFrame::SetRegAllowStackVarEnableOnly(RegSlotType localRegisterID, Var value)
    {
        Assert(localRegisterID == 0 || localRegisterID >= m_functionBody->GetConstantCount());
        ValidateRegValue(value, true, false);
        m_localSlots[localRegisterID] = value;
    }

    template <>
    AsmJsSIMDValue InterpreterStackFrame::GetRegRaw(RegSlot localRegisterID) const
    {
        return (AsmJsSIMDValue)m_localSimdSlots[localRegisterID];
    }

    template<>
    void InterpreterStackFrame::SetRegRaw(RegSlot localRegisterID, AsmJsSIMDValue bValue)
    {
        m_localSimdSlots[localRegisterID] = bValue;
    }

    template <typename RegSlotType>
    AsmJsSIMDValue InterpreterStackFrame::GetRegRawSimd(RegSlotType localRegisterID) const
    {
        return m_localSimdSlots[localRegisterID];
    }

    template <typename RegSlotType>
    void InterpreterStackFrame::SetRegRawSimd(RegSlotType localRegisterID, AsmJsSIMDValue bValue)
    {
        m_localSimdSlots[localRegisterID] = bValue;
    }

    template <class T>
    void InterpreterStackFrame::OP_SimdLdArrGeneric(const unaligned T* playout)
    {
        Assert(playout->ViewType < 8);
        const uint32 index = (uint32)GetRegRawInt(playout->SlotIndex) & TypedArrayViewMask[playout->ViewType];
        JavascriptArrayBuffer* arr = *(JavascriptArrayBuffer**)GetNonVarReg(AsmJsFunctionMemory::ArrayBufferRegister);
        BYTE* buffer = arr->GetBuffer();
        uint8 dataWidth = playout->DataWidth;
        RegSlot dstReg = playout->Value;

        if (index < 0 || index + dataWidth > arr->GetByteLength())
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, L"Simd typed array access");
        }
        AsmJsSIMDValue *data = (AsmJsSIMDValue*)(buffer + index);
        AsmJsSIMDValue value;

        value = SIMDLdData(data, dataWidth);
        SetRegRawSimd(dstReg, value);
    }

    template <class T>
    void InterpreterStackFrame::OP_SimdLdArrConstIndex(const unaligned T* playout)
    {
        Assert(playout->ViewType < 8);
        const uint32 index = playout->SlotIndex;
        JavascriptArrayBuffer* arr = *(JavascriptArrayBuffer**)GetNonVarReg(AsmJsFunctionMemory::ArrayBufferRegister);
        BYTE* buffer = arr->GetBuffer();
        uint8 dataWidth = playout->DataWidth;
        RegSlot dstReg = playout->Value;

        if (index < 0 || index + dataWidth > arr->GetByteLength())
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, L"Simd typed array access");
        }
        AsmJsSIMDValue *data = (AsmJsSIMDValue*)(buffer + index);
        AsmJsSIMDValue value;

        value = SIMDLdData(data, dataWidth);
        SetRegRawSimd(dstReg, value);
    }

    template <class T>
    void InterpreterStackFrame::OP_SimdStArrGeneric(const unaligned T* playout)
    {
        Assert(playout->ViewType < 8);
        const uint32 index = (uint32)GetRegRawInt(playout->SlotIndex) & TypedArrayViewMask[playout->ViewType];
        JavascriptArrayBuffer* arr = *(JavascriptArrayBuffer**)GetNonVarReg(AsmJsFunctionMemory::ArrayBufferRegister);
        BYTE* buffer = arr->GetBuffer();
        uint8 dataWidth = playout->DataWidth;
        RegSlot srcReg = playout->Value;

        if (index < 0 || index + dataWidth > arr->GetByteLength())
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, L"Simd typed array access");
        }
        AsmJsSIMDValue *data = (AsmJsSIMDValue*)(buffer + index);
        AsmJsSIMDValue value = GetRegRawSimd(srcReg);
        SIMDStData(data, value, dataWidth);
    }

    template <class T>
    void InterpreterStackFrame::OP_SimdStArrConstIndex(const unaligned T* playout)
    {
        Assert(playout->ViewType < 8);
        const uint32 index = playout->SlotIndex;
        JavascriptArrayBuffer* arr = *(JavascriptArrayBuffer**)GetNonVarReg(AsmJsFunctionMemory::ArrayBufferRegister);
        BYTE* buffer = arr->GetBuffer();
        uint8 dataWidth = playout->DataWidth;
        RegSlot srcReg = playout->Value;

        if (index < 0 || index + dataWidth > arr->GetByteLength())
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, L"Simd typed array access");
        }
        AsmJsSIMDValue *data = (AsmJsSIMDValue*)(buffer + index);
        AsmJsSIMDValue value = GetRegRawSimd(srcReg);
        SIMDStData(data, value, dataWidth);

    }

    Var InterpreterStackFrame::GetNonVarReg(RegSlot localRegisterID) const
    {
        return m_localSlots[localRegisterID];
    }

    void InterpreterStackFrame::SetNonVarReg(RegSlot localRegisterID, Var aValue)
    {
        m_localSlots[localRegisterID] = aValue;
    }

    Var InterpreterStackFrame::GetRootObject() const
    {
        Var rootObject = GetReg(Js::FunctionBody::RootObjectRegSlot);
        Assert(rootObject == this->GetFunctionBody()->LoadRootObject());
        return rootObject;
    }

    Var InterpreterStackFrame::OP_ArgIn0()
    {
        return m_inParams[0];
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    void InterpreterStackFrame::OP_ProfiledArgOut_A(const unaligned T * playout)
    {
        FunctionBody* functionBody = this->m_functionBody;
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();

        Assert(playout->Reg > FunctionBody::FirstRegSlot && playout->Reg < functionBody->GetConstantCount());
        Var value = GetReg(playout->Reg);
        if (value != nullptr && TaggedInt::Is(value))
        {
            dynamicProfileInfo->RecordConstParameterAtCallSite(playout->profileId, playout->Arg);
        }
        SetOut(playout->Arg, GetReg(playout->Reg));
    }
#endif

    template <class T>
    void InterpreterStackFrame::OP_ArgOut_A(const unaligned T * playout)
    {
        SetOut(playout->Arg, GetReg(playout->Reg));
    }
#if DBG
    template <class T>
    void InterpreterStackFrame::OP_ArgOut_ANonVar(const unaligned T * playout)
    {
        SetOut(playout->Arg, GetNonVarReg(playout->Reg));
    }
#endif

    template <class T>
    void InterpreterStackFrame::OP_ArgOut_Env(const unaligned T * playout)
    {
        Var argEnv;
        if (this->m_functionBody->GetLocalFrameDisplayReg() != Constants::NoRegister)
        {
            argEnv = this->GetLocalFrameDisplay();
        }
        else
        {
            argEnv = this->LdEnv();
        }
        SetOut(playout->Arg, argEnv);
    }

    BOOL InterpreterStackFrame::OP_BrFalse_A(Var aValue, ScriptContext* scriptContext)
    {
        return !JavascriptConversion::ToBoolean(aValue, scriptContext);
    }

    BOOL InterpreterStackFrame::OP_BrTrue_A(Var aValue, ScriptContext* scriptContext)
    {
        return JavascriptConversion::ToBoolean(aValue, scriptContext);
    }

    BOOL InterpreterStackFrame::OP_BrNotNull_A(Var aValue)
    {
        return aValue != NULL;
    }

    BOOL InterpreterStackFrame::OP_BrUndecl_A(Var aValue)
    {
        return this->scriptContext->GetLibrary()->IsUndeclBlockVar(aValue);
    }

    BOOL InterpreterStackFrame::OP_BrNotUndecl_A(Var aValue)
    {
        return !this->scriptContext->GetLibrary()->IsUndeclBlockVar(aValue);
    }

    BOOL InterpreterStackFrame::OP_BrOnHasProperty(Var argInstance, uint propertyIdIndex, ScriptContext* scriptContext)
    {
        return JavascriptOperators::OP_HasProperty(argInstance,
            this->m_functionBody->GetReferencedPropertyId(propertyIdIndex), scriptContext);
    }

    BOOL InterpreterStackFrame::OP_BrOnNoProperty(Var argInstance, uint propertyIdIndex, ScriptContext* scriptContext)
    {
        return !JavascriptOperators::OP_HasProperty(argInstance,
            this->m_functionBody->GetReferencedPropertyId(propertyIdIndex), scriptContext);
    }

    BOOL InterpreterStackFrame::OP_BrOnNoEnvProperty(Var envInstance, int32 slotIndex, uint propertyIdIndex, ScriptContext* scriptContext)
    {
        Var instance = OP_LdFrameDisplaySlot(envInstance, slotIndex);
        return !JavascriptOperators::OP_HasProperty(instance,
            this->m_functionBody->GetReferencedPropertyId(propertyIdIndex), scriptContext);
    }

    BOOL InterpreterStackFrame::OP_BrOnClassConstructor(Var aValue)
    {
        return JavascriptOperators::IsClassConstructor(aValue);
    }

    template<class T>
    void InterpreterStackFrame::OP_LdLen(const unaligned T * const playout)
    {
        Assert(playout);

        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        const auto instance = GetReg(playout->R1);
        Var length = JavascriptOperators::OP_GetLength(instance, GetScriptContext());

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);

        SetReg(playout->R0, length);
    }

#if ENABLE_PROFILE_INFO
    template<class T>
    void InterpreterStackFrame::OP_ProfiledLdLen(const unaligned OpLayoutDynamicProfile<T> *const playout)
    {
        Assert(playout);

        const auto functionBody = m_functionBody;
        const auto profileData = functionBody->GetDynamicProfileInfo();

        const auto instance = GetReg(playout->R1);
        LdElemInfo ldElemInfo;
        ldElemInfo.arrayType = ValueType::Uninitialized.Merge(instance);

        ThreadContext* threadContext = this->GetScriptContext()->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        Var length = JavascriptOperators::OP_GetLength(instance, GetScriptContext());

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);

        ldElemInfo.elemType = ldElemInfo.elemType.Merge(length);
        profileData->RecordElementLoad(functionBody, playout->profileId, ldElemInfo);

        SetReg(playout->R0, length);
    }
#endif

    Var InterpreterStackFrame::GetFunctionExpression()
    {
        // Make sure we get the boxed function object if is there, (or the function itself)
        return StackScriptFunction::GetCurrentFunctionObject(this->function->GetRealFunctionObject());
    }

    template <class T>
    void InterpreterStackFrame::OP_LdFunctionExpression(const unaligned T * playout)
    {
        SetRegAllowStackVar(playout->R0, this->GetFunctionExpression());
    }

    template <class T>
    void InterpreterStackFrame::OP_StFunctionExpression(const unaligned T * playout)
    {
        OP_StFunctionExpression(GetReg(playout->Instance), GetReg(playout->Value), playout->PropertyIdIndex);
    }

    template <class T>
    void InterpreterStackFrame::OP_StLocalFunctionExpression(const unaligned T * playout)
    {
        OP_StFunctionExpression(this->localClosure, GetReg(playout->Instance), playout->PropertyIdIndex);
    }

    void InterpreterStackFrame::OP_StFunctionExpression(Var instance, Var value, PropertyIdIndexType index)
    {
        JavascriptOperators::OP_StFunctionExpression(instance,
                                                     this->m_functionBody->GetReferencedPropertyId(index), value);
    }

    template <class T>
    void InterpreterStackFrame::OP_LdNewTarget(const unaligned T* playout)
    {
        if (this->m_callFlags & CallFlags_NewTarget)
        {
            SetRegAllowStackVar(playout->R0, (Js::RecyclableObject*)this->m_inParams[this->m_inSlotsCount]);
        }
        else if (this->m_callFlags & CallFlags_New)
        {
            SetRegAllowStackVar(playout->R0, this->GetFunctionExpression());
        }
        else
        {
            SetReg(playout->R0, this->GetScriptContext()->GetLibrary()->GetUndefined());
        }
    }

    Var InterpreterStackFrame::OP_Ld_A(Var aValue)
    {
        return aValue;
    }

    Var InterpreterStackFrame::LdEnv() const
    {
        return this->function->GetEnvironment();
    }

    void InterpreterStackFrame::SetEnv(FrameDisplay *frameDisplay)
    {
        this->function->SetEnvironment(frameDisplay);
    }

    Var InterpreterStackFrame::OP_LdLocalObj()
    {
        if (!VirtualTableInfo<ActivationObject>::HasVirtualTable(this->localClosure) &&
            !VirtualTableInfo<ActivationObjectEx>::HasVirtualTable(this->localClosure))
        {
            Js::Throw::FatalInternalError();
        }
        return this->localClosure;
    }

#ifndef TEMP_DISABLE_ASMJS
    template <typename T2>
    void InterpreterStackFrame::OP_StArr(uint32 index, RegSlot value)
    {
        JavascriptArrayBuffer* arr = *(JavascriptArrayBuffer**)GetNonVarReg(AsmJsFunctionMemory::ArrayBufferRegister);
        if (index < (arr->GetByteLength()))
        {
            BYTE* buffer = arr->GetBuffer();
            *(T2*)(buffer + index) = (T2)GetRegRaw<T2>(value);
        }
    }
#endif

    template<> inline double InterpreterStackFrame::GetArrayViewOverflowVal()
    {
        return *(double*)&NumberConstants::k_Nan;
    }

    template<> inline float InterpreterStackFrame::GetArrayViewOverflowVal()
    {
        return (float)*(double*)&NumberConstants::k_Nan;
    }

    template<typename T> T InterpreterStackFrame::GetArrayViewOverflowVal()
    {
        return 0;
    }

    template <class T>
    void InterpreterStackFrame::OP_LdArrFunc(const unaligned T* playout)
    {
        Var* arr = (Var*)GetNonVarReg(playout->Instance);
        const uint32 index = (uint32)GetRegRawInt(playout->SlotIndex);
        m_localSlots[playout->Value] = arr[index];
    }

#ifndef TEMP_DISABLE_ASMJS
    template <typename T2>
    void InterpreterStackFrame::OP_LdArr(uint32 index, RegSlot value)
    {
        JavascriptArrayBuffer* arr = *(JavascriptArrayBuffer**)GetNonVarReg(AsmJsFunctionMemory::ArrayBufferRegister);
        BYTE* buffer = arr->GetBuffer();
        T2 val = index < (arr->GetByteLength()) ? *(T2*)(buffer + index) : GetArrayViewOverflowVal<T2>();
        SetRegRaw<T2>(value, val);
    }
#endif

    template <class T, typename T2>
    void InterpreterStackFrame::OP_StSlotPrimitive(const unaligned T* playout)
    {
        T2* buffer = (T2*)GetNonVarReg(playout->Instance);
        buffer[playout->SlotIndex] = GetRegRaw<T2>(playout->Value);
    }

    template <class T>
    Var InterpreterStackFrame::OP_LdAsmJsSlot(Var instance, const unaligned T* playout)
    {
        return ((Var*)instance)[playout->SlotIndex];
    }

    template <class T, typename T2>
    void InterpreterStackFrame::OP_LdSlotPrimitive(const unaligned T* playout)
    {
        T2* buffer = (T2*)GetNonVarReg(playout->Instance);
        SetRegRaw<T2>(playout->Value, buffer[playout->SlotIndex]);
    }

    template <class T>
    void InterpreterStackFrame::OP_LdArrGeneric(const unaligned T* playout)
    {
        Assert(playout->ViewType < 8);
        const uint32 index = (uint32)GetRegRawInt(playout->SlotIndex) & TypedArrayViewMask[playout->ViewType];
        (this->*LdArrFunc[playout->ViewType])(index, playout->Value);
    }
    template <class T>
    void InterpreterStackFrame::OP_LdArrConstIndex(const unaligned T* playout)
    {
        const uint32 index = playout->SlotIndex;
        Assert(playout->ViewType < 8);
        (this->*LdArrFunc[playout->ViewType])(index, playout->Value);
    }
    template <class T>
    void InterpreterStackFrame::OP_StArrGeneric(const unaligned T* playout)
    {
        Assert(playout->ViewType < 8);
        const uint32 index = (uint32)GetRegRawInt(playout->SlotIndex) & TypedArrayViewMask[playout->ViewType];
        (this->*StArrFunc[playout->ViewType])(index, playout->Value);
    }
    template <class T>
    void InterpreterStackFrame::OP_StArrConstIndex(const unaligned T* playout)
    {
        const uint32 index = playout->SlotIndex;
        Assert(playout->ViewType < 8);
        (this->*StArrFunc[playout->ViewType])(index, playout->Value);
    }

    Var InterpreterStackFrame::OP_LdSlot(Var instance, int32 slotIndex)
    {
        if (!PHASE_OFF(ClosureRangeCheckPhase, this->m_functionBody))
        {
            if ((uintptr_t)((Var*)instance)[ScopeSlots::EncodedSlotCountSlotIndex] <= (uintptr_t)(slotIndex - ScopeSlots::FirstSlotIndex))
            {
                Js::Throw::FatalInternalError();
            }
        }
        return ((Var*)(instance))[slotIndex];
    }

    template <class T>
    Var InterpreterStackFrame::OP_LdSlot(Var instance, const unaligned T* playout)
    {
        return OP_LdSlot(instance, playout->SlotIndex);
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    Var InterpreterStackFrame::OP_ProfiledLdSlot(Var instance, const unaligned T* playout)
    {
        Var value = OP_LdSlot(instance, playout->SlotIndex);
        ProfilingHelpers::ProfileLdSlot(value, GetFunctionBody(), playout->profileId);
        return value;
    }
#endif

    template <class T>
    Var InterpreterStackFrame::OP_LdInnerSlot(Var slotArray, const unaligned T* playout)
    {
        return OP_LdSlot(slotArray, playout->SlotIndex2);
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    Var InterpreterStackFrame::OP_ProfiledLdInnerSlot(Var slotArray, const unaligned T* playout)
    {
        Var value = OP_LdInnerSlot(slotArray, playout);
        ProfilingHelpers::ProfileLdSlot(value, GetFunctionBody(), playout->profileId);
        return value;
    }
#endif

    template <class T>
    Var InterpreterStackFrame::OP_LdInnerObjSlot(Var slotArray, const unaligned T* playout)
    {
        return OP_LdObjSlot(slotArray, playout->SlotIndex2);
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    Var InterpreterStackFrame::OP_ProfiledLdInnerObjSlot(Var slotArray, const unaligned T* playout)
    {
        Var value = OP_LdInnerObjSlot(slotArray, playout);
        ProfilingHelpers::ProfileLdSlot(value, GetFunctionBody(), playout->profileId);
        return value;
    }
#endif

    Var InterpreterStackFrame::OP_LdFrameDisplaySlot(Var instance, int32 slotIndex)
    {
        if (!PHASE_OFF(ClosureRangeCheckPhase, this->m_functionBody))
        {
            if (((FrameDisplay*)instance)->GetLength() < slotIndex - Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Var))
            {
                Js::Throw::FatalInternalError();
            }
        }
        return ((Var*)instance)[slotIndex];
    }

    template <class T>
    Var InterpreterStackFrame::OP_LdEnvObj(Var instance, const unaligned T* playout)
    {
        return OP_LdFrameDisplaySlot(instance, playout->SlotIndex);
    }

    template <class T>
    Var InterpreterStackFrame::OP_LdEnvSlot(Var instance, const unaligned T* playout)
    {
        Var slotArray = OP_LdFrameDisplaySlot(instance, playout->SlotIndex1);
        return OP_LdSlot(slotArray, playout->SlotIndex2);
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    Var InterpreterStackFrame::OP_ProfiledLdEnvSlot(Var instance, const unaligned T* playout)
    {
        Var value = OP_LdEnvSlot(instance, playout);
        ProfilingHelpers::ProfileLdSlot(value, GetFunctionBody(), playout->profileId);
        return value;
    }
#endif

    Var InterpreterStackFrame::OP_LdObjSlot(Var instance, int32 slotIndex)
    {
        Var *slotArray = *(Var**)((char*)instance + DynamicObject::GetOffsetOfAuxSlots());
        return slotArray[slotIndex];
    }

    template <class T>
    Var InterpreterStackFrame::OP_LdObjSlot(Var instance, const unaligned T* playout)
    {
        return OP_LdObjSlot(instance, playout->SlotIndex);
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    Var InterpreterStackFrame::OP_ProfiledLdObjSlot(Var instance, const unaligned T* playout)
    {
        Var value = OP_LdObjSlot(instance, playout->SlotIndex);
        ProfilingHelpers::ProfileLdSlot(value, GetFunctionBody(), playout->profileId);
        return value;
    }
#endif

    template <class T>
    Var InterpreterStackFrame::OP_LdEnvObjSlot(Var instance, const unaligned T* playout)
    {
        Var slotArray = OP_LdFrameDisplaySlot(instance, playout->SlotIndex1);
        return OP_LdObjSlot(slotArray, playout->SlotIndex2);
    }

#if ENABLE_PROFILE_INFO
    template <class T>
    Var InterpreterStackFrame::OP_ProfiledLdEnvObjSlot(Var instance, const unaligned T* playout)
    {
        Var value = OP_LdEnvObjSlot(instance, playout);
        ProfilingHelpers::ProfileLdSlot(value, GetFunctionBody(), playout->profileId);
        return value;
    }
#endif

    void InterpreterStackFrame::OP_StSlot(Var instance, int32 slotIndex, Var value)
    {
        // We emit OpCode::StSlot in the bytecode only for scope slot arrays, which are not recyclable objects.
        if (!PHASE_OFF(ClosureRangeCheckPhase, this->m_functionBody))
        {
            if ((uintptr_t)((Var*)instance)[ScopeSlots::EncodedSlotCountSlotIndex] <= (uintptr_t)(slotIndex - ScopeSlots::FirstSlotIndex))
            {
                Js::Throw::FatalInternalError();
            }
        }
        ((Var*)(instance))[slotIndex] = value;
    }

    void InterpreterStackFrame::OP_StEnvSlot(Var instance, int32 slotIndex1, int32 slotIndex2, Var value)
    {
        Var slotArray = (Var*)OP_LdFrameDisplaySlot(instance, slotIndex1);
        OP_StSlot(slotArray, slotIndex2, value);
    }

    void InterpreterStackFrame::OP_StSlotChkUndecl(Var instance, int32 slotIndex, Var value)
    {
        // We emit OpCode::StSlot in the bytecode only for scope slot arrays, which are not recyclable objects.
        if (!PHASE_OFF(ClosureRangeCheckPhase, this->m_functionBody))
        {
            if ((uintptr_t)((Var*)instance)[ScopeSlots::EncodedSlotCountSlotIndex] <= (uintptr_t)(slotIndex - ScopeSlots::FirstSlotIndex))
            {
                Js::Throw::FatalInternalError();
            }
        }
        OP_ChkUndecl(((Var*)instance)[slotIndex]);
        ((Var*)(instance))[slotIndex] = value;
    }

    void InterpreterStackFrame::OP_StEnvSlotChkUndecl(Var instance, int32 slotIndex1, int32 slotIndex2, Var value)
    {
        Var slotArray = (Var*)OP_LdFrameDisplaySlot(instance, slotIndex1);
        OP_StSlotChkUndecl(slotArray, slotIndex2, value);
    }

    void InterpreterStackFrame::OP_StObjSlot(Var instance, int32 slotIndex, Var value)
    {
        // It would be nice to assert that it's ok to store directly to slot, but we don't have the propertyId.
        Var *slotArray = *(Var**)((char*)instance + DynamicObject::GetOffsetOfAuxSlots());
        slotArray[slotIndex] = value;
    }

    void InterpreterStackFrame::OP_StObjSlotChkUndecl(Var instance, int32 slotIndex, Var value)
    {
        // It would be nice to assert that it's ok to store directly to slot, but we don't have the propertyId.
        Var *slotArray = *(Var**)((char*)instance + DynamicObject::GetOffsetOfAuxSlots());
        OP_ChkUndecl(slotArray[slotIndex]);
        slotArray[slotIndex] = value;
    }

    void InterpreterStackFrame::OP_StEnvObjSlot(Var instance, int32 slotIndex1, int32 slotIndex2, Var value)
    {
        // It would be nice to assert that it's ok to store directly to slot, but we don't have the propertyId.
        Var envInstance = (Var*)OP_LdFrameDisplaySlot(instance, slotIndex1);
        OP_StObjSlot(envInstance, slotIndex2, value);
    }

    void InterpreterStackFrame::OP_StEnvObjSlotChkUndecl(Var instance, int32 slotIndex1, int32 slotIndex2, Var value)
    {
        // It would be nice to assert that it's ok to store directly to slot, but we don't have the propertyId.
        Var envInstance = (Var*)OP_LdFrameDisplaySlot(instance, slotIndex1);
        OP_StObjSlotChkUndecl(envInstance, slotIndex2, value);
    }

    Var InterpreterStackFrame::OP_LdStackArgPtr(void)
    {
        // Return the address of the first param after "this".
        return m_inParams + 1;
    }

    // Called for the debug purpose, to create the arguments object explicitly even though script has not declared it.
    Var InterpreterStackFrame::CreateHeapArguments(ScriptContext* scriptContext)
    {
        return JavascriptOperators::LoadHeapArguments(this->function->GetRealFunctionObject(), this->m_inSlotsCount - 1, &this->m_inParams[1], scriptContext->GetLibrary()->GetNull(), (PropertyId*)scriptContext->GetLibrary()->GetNull(), scriptContext, false);
    }

    template <bool letArgs>
    Var InterpreterStackFrame::LdHeapArgumentsImpl(Var argsArray, ScriptContext* scriptContext)
    {
        Var frameObj;
        if (m_functionBody->HasScopeObject() && argsArray != scriptContext->GetLibrary()->GetNull())
        {
            frameObj = this->localClosure;
            Assert(frameObj);
        }
        else
        {
            frameObj = scriptContext->GetLibrary()->GetNull();
        }
        Var args = JavascriptOperators::LoadHeapArguments(this->function->GetRealFunctionObject(), this->m_inSlotsCount - 1, &this->m_inParams[1], frameObj, (PropertyId*)argsArray, scriptContext, letArgs);
        this->m_arguments = args;
        return args;
    }

    Var InterpreterStackFrame::OP_LdHeapArguments(Var argsArray, ScriptContext* scriptContext)
    {
        return LdHeapArgumentsImpl<false>(argsArray, scriptContext);
    }

    Var InterpreterStackFrame::OP_LdLetHeapArguments(Var argsArray, ScriptContext* scriptContext)
    {
        return LdHeapArgumentsImpl<true>(argsArray, scriptContext);
    }

    Var InterpreterStackFrame::OP_LdHeapArgsCached(ScriptContext* scriptContext)
    {
        uint32 formalsCount = this->m_functionBody->GetInParamsCount() - 1;
        Var args = JavascriptOperators::LoadHeapArgsCached(this->function->GetRealFunctionObject(), this->m_inSlotsCount - 1, formalsCount, &this->m_inParams[1], this->localClosure, scriptContext, false);
        this->m_arguments = args;
        return args;
    }

    Var InterpreterStackFrame::OP_LdLetHeapArgsCached(ScriptContext* scriptContext)
    {
        uint32 formalsCount = this->m_functionBody->GetInParamsCount() - 1;
        Var args = JavascriptOperators::LoadHeapArgsCached(this->function->GetRealFunctionObject(), this->m_inSlotsCount - 1, formalsCount, &this->m_inParams[1], this->localClosure, scriptContext, true);
        this->m_arguments = args;
        return args;
    }

    Var InterpreterStackFrame::OP_LdArgumentsFromFrame()
    {
        return this->m_arguments;
    }

    void* InterpreterStackFrame::OP_LdArgCnt()
    {
        return (void*)m_inSlotsCount;
    }

    Var InterpreterStackFrame::OP_ResumeYield(Var yieldDataVar, RegSlot yieldStarIterator)
    {
        ResumeYieldData* yieldData = static_cast<ResumeYieldData*>(yieldDataVar);
        RecyclableObject* iterator = yieldStarIterator != Constants::NoRegister ? RecyclableObject::FromVar(GetNonVarReg(yieldStarIterator)) : nullptr;

        return JavascriptOperators::OP_ResumeYield(yieldData, iterator);
    }

    void* InterpreterStackFrame::operator new(size_t byteSize, void* previousAllocation)
    {
        //
        // Placement 'new' is used by InterpreterStackFrame to initialize the C++ object on the RcInterpreter's
        // program stack:
        // - Unlike most other allocations, the previously allocated memory will __not__ be
        //   zero-initialized, as we do not want the overhead of zero-initializing the frame when
        //   calling functions.
        //
        // NOTE: If we wanted to add C# semantics of all locals are automatically zero-initialized,
        // need to determine the most efficient mechanism for this.
        //

        return previousAllocation;
    }

    void __cdecl InterpreterStackFrame::operator delete(void * allocationToFree, void * previousAllocation)
    {
        AssertMsg(allocationToFree == previousAllocation, "Memory locations should match");
        AssertMsg(false, "This function should never actually be called");
    }
} // namespace Js

// Make sure the macro and the layout for the op is consistent
#define DEF2(x, op, ...) \
    CompileAssert(!Js::OpCodeInfo<Js::OpCode::op>::HasMultiSizeLayout);  \
    CompileAssert(!Js::OpCodeInfo<Js::OpCode::op>::IsExtendedOpcode);
#define DEF3(x, op, ...) DEF2(x, op)
#define EXDEF2(x, op, ...) \
    CompileAssert(!Js::OpCodeInfo<Js::OpCode::op>::HasMultiSizeLayout);  \
    CompileAssert(Js::OpCodeInfo<Js::OpCode::op>::IsExtendedOpcode);
#define EXDEF3(x, op, ...) EXDEF2(x, op)
#define DEF2_WMS(x, op, ...) \
    CompileAssert(Js::OpCodeInfo<Js::OpCode::op>::HasMultiSizeLayout);  \
    CompileAssert(!Js::OpCodeInfo<Js::OpCode::op>::IsExtendedOpcode);
#define DEF3_WMS(x, op, ...) DEF2_WMS(x, op)
#define EXDEF2_WMS(x, op, ...) \
    CompileAssert(Js::OpCodeInfo<Js::OpCode::op>::HasMultiSizeLayout);  \
    CompileAssert(Js::OpCodeInfo<Js::OpCode::op>::IsExtendedOpcode);
#define EXDEF3_WMS(x, op, ...) EXDEF2_WMS(x, op)
#include "InterpreterHandler.inl"

// Make sure the macro and the layout for the op is consistent
#define DEF2(x, op, ...) \
    CompileAssert(!Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::HasMultiSizeLayout);  \
    CompileAssert(!Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::IsExtendedOpcode);
#define DEF3(x, op, ...) DEF2(x, op)
#define DEF4(x, op, ...) DEF2(x, op)
#define EXDEF2(x, op, ...) \
    CompileAssert(!Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::HasMultiSizeLayout);  \
    CompileAssert(Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::IsExtendedOpcode);
#define EXDEF3(x, op, ...) EXDEF2(x, op)
#define EXDEF4(x, op, ...) EXDEF2(x, op)
#define DEF2_WMS(x, op, ...) \
    CompileAssert(Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::HasMultiSizeLayout);  \
    CompileAssert(!Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::IsExtendedOpcode);
#define DEF3_WMS(x, op, ...) DEF2_WMS(x, op)
#define DEF4_WMS(x, op, ...) DEF2_WMS(x, op)
#define EXDEF2_WMS(x, op, ...) \
    CompileAssert(Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::HasMultiSizeLayout);  \
    CompileAssert(Js::OpCodeInfoAsmJs<Js::OpCodeAsmJs::op>::IsExtendedOpcode);
#define EXDEF3_WMS(x, op, ...) EXDEF2_WMS(x, op)
#define EXDEF4_WMS(x, op, ...) EXDEF2_WMS(x, op)
#include "InterpreterHandlerAsmJs.inl"
