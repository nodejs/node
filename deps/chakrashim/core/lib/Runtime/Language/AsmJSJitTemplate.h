//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{

    void AsmJsCommonEntryPoint(Js::ScriptFunction* func, void* savedEbp);

    namespace AsmJsJitTemplate
    {
        const int PAGESIZE = 0x1000;
        typedef AsmJsEncoder* TemplateContext;
        // Initialise template data for architecture specific
        void* InitTemplateData();
        // Free template data for architecture specific
        void  FreeTemplateData( void* userData );

        struct Globals
        {
#if DBG_DUMP
            static FunctionBody* CurrentEncodingFunction;
#endif
            // Number of Vars on the stack before the first variable
            static const int StackVarCount = 2;
            static const int ModuleSlotOffset   ;
            static const int ModuleEnvOffset    ;
            static const int ArrayBufferOffset  ;
            static const int ArraySizeOffset  ;
            static const int ScriptContextOffset;
        };
#ifdef _M_IX86
#define CreateTemplate(name,...) \
        struct name\
        {\
            static int ApplyTemplate( TemplateContext context, BYTE*& buffer,##__VA_ARGS__ );\
        }
#else
#define CreateTemplate(name,...)  \
        struct name\
        {\
            static int ApplyTemplate( TemplateContext context, BYTE*& buffer,##__VA_ARGS__ ) { __debugbreak(); return 0; }\
        }
#endif

        CreateTemplate( FunctionEntry );
        CreateTemplate( FunctionExit );
        CreateTemplate( Br, BYTE** relocAddr, bool isBackEdge);
        CreateTemplate( BrTrue, int offset, BYTE** relocAddr, bool isBackEdge);
        CreateTemplate( BrEq, int leftOffset, int rightOffset, BYTE** relocAddr, bool isBackEdge);
        CreateTemplate( Label );
        CreateTemplate( LdUndef, int targetOffset );
        CreateTemplate( LdSlot, int targetOffset, int arrOffset, int slotIndex );
        CreateTemplate( LdArr_Func, int targetOffset, int arrOffset, int slotVarIndex );

        // int operations
        CreateTemplate( Ld_Int, int targetOffset, int rightOffset );
        CreateTemplate( LdSlot_Int, int targetOffset, int slotIndex);
        CreateTemplate( LdSlot_Flt, int targetOffset, int slotIndex);
        CreateTemplate( StSlot_Flt, int srcOffset, int slotIndex);
        CreateTemplate( StSlot_Int, int srcOffset, int slotIndex);
        CreateTemplate( LdConst_Int, int offset, int value );
        CreateTemplate( SetReturn_Int, int offset );
        CreateTemplate( Db_To_Int, int targetOffset, int rightOffset );
        CreateTemplate( Int_To_Bool, int targetOffset, int rightOffset );

        CreateTemplate( LogNot_Int, int targetOffset, int rightOffset );
        CreateTemplate( Neg_Int, int targetOffset, int rightOffset );
        CreateTemplate( Not_Int, int targetOffset, int rightOffset );
        CreateTemplate( Or_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( And_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Xor_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Shl_Int , int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Shr_Int , int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( ShrU_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Add_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Sub_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Mul_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Div_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Rem_Int, int targetOffset, int leftOffset, int rightOffset );

        CreateTemplate( Lt_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Le_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Gt_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Ge_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Eq_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Ne_Int, int targetOffset, int leftOffset, int rightOffset );

        CreateTemplate( Min_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Max_Int, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Abs_Int, int targetOffset, int rightOffset );
        CreateTemplate( Clz32_Int,int targetOffset,int rightOffset );

        // uint operations
        CreateTemplate( Div_UInt, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Mul_UInt, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Rem_UInt, int targetOffset, int leftOffset, int rightOffset );

        CreateTemplate( Lt_UInt, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Le_UInt, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Gt_UInt, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Ge_UInt, int targetOffset, int leftOffset, int rightOffset );

        //Float Operations
        CreateTemplate( Add_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( Sub_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( Mul_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( Div_Flt, int targetOffset, int leftOffset, int rightOffset);
        // Double operations
        CreateTemplate( Ld_Db, int targetOffset, int rightOffset);
        CreateTemplate( Ld_Flt, int targetOffset, int rightOffset);
        CreateTemplate( LdAddr_Db, int targetOffset, const double* dbAddr);
        CreateTemplate( LdSlot_Db, int targetOffset, int slotIndex );
        CreateTemplate( StSlot_Db, int srcOffset, int slotIndex );
        CreateTemplate( SetReturn_Db, int offset);
        CreateTemplate( SetReturn_Flt, int offset);
        CreateTemplate( SetFround_Db, int targetOffset, int rightOffset);
        CreateTemplate( SetFround_Flt, int targetOffset, int rightOffset);
        CreateTemplate( SetFround_Int, int targetOffset, int rightOffset);
        CreateTemplate( Int_To_Db, int targetOffset, int rightOffset );
        CreateTemplate( Float_To_Db, int targetOffset, int rightOffset);
        CreateTemplate( Float_To_Int, int targetOffset, int rightOffset);
        CreateTemplate( UInt_To_Db, int targetOffset, int rightOffset);
        CreateTemplate( Neg_Db, int targetOffset, int rightOffset);
        CreateTemplate( Neg_Flt, int targetOffset, int rightOffset);

        CreateTemplate( Add_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Sub_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Mul_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Div_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( Rem_Db, int targetOffset, int leftOffset, int rightOffset );

        CreateTemplate( CmpLt_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( CmpLe_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( CmpGt_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( CmpGe_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( CmpEq_Db, int targetOffset, int leftOffset, int rightOffset );
        CreateTemplate( CmpNe_Db, int targetOffset, int leftOffset, int rightOffset );

        CreateTemplate( CmpLt_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( CmpLe_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( CmpGt_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( CmpGe_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( CmpEq_Flt, int targetOffset, int leftOffset, int rightOffset);
        CreateTemplate( CmpNe_Flt, int targetOffset, int leftOffset, int rightOffset);

        // offsets : array of offset for double variables, the first one is where the result should be put in
        //   D0 = func(D1,D2,D3); =>  offsets = [D0,D1,D2,D3]
        CreateTemplate(Call_Db, int nbOffsets, int* offsets, void* addr, bool addEsp);
        CreateTemplate(Call_Flt, int nbOffsets, int* offsets, void* addr, bool addEsp);

        //external calls
        CreateTemplate( StartCall, int argBytesSize );
        CreateTemplate( ArgOut_Int, int argIndex, int offset );
        CreateTemplate( ArgOut_Db, int argIndex, int offset);
        CreateTemplate( Call, int targetOffset, int funcOffset, int nbArgs);
        CreateTemplate( Conv_VTI, int targetOffset, int srcOffset);
        CreateTemplate( Conv_VTD, int targetOffset, int srcOffset);
        CreateTemplate( Conv_VTF, int targetOffset, int srcOffset);

        //internal calls
        CreateTemplate( I_StartCall, int argBytesSize );
        CreateTemplate( I_ArgOut_Int, int argIndex, int offset );
        CreateTemplate( I_ArgOut_Db, int argIndex, int offset);
        CreateTemplate( I_ArgOut_Flt, int argIndex, int offset);
        CreateTemplate( I_Call, int targetOffset, int funcOffset, int nbArgs, AsmJsRetType retType);
        CreateTemplate( I_Conv_VTI, int targetOffset, int srcOffset);
        CreateTemplate( I_Conv_VTD, int targetOffset, int srcOffset);
        CreateTemplate( I_Conv_VTF, int targetOffset, int srcOffset);

        CreateTemplate( LdArr, int targetOffset, int slotVarIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( LdArrDb, int targetOffset, int slotVarIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( LdArrFlt, int targetOffset, int slotVarIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( StArr, int srcOffset, int slotVarIndex, ArrayBufferView::ViewType viewType );
        CreateTemplate( StArrDb, int srcOffset, int slotVarIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( StArrFlt, int srcOffset, int slotVarIndex, ArrayBufferView::ViewType viewType);

        CreateTemplate( ConstLdArr, int targetOffset, int constIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( ConstLdArrDb, int targetOffset, int constIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( ConstLdArrFlt, int targetOffset, int constIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( ConstStArr, int srcOffset, int constIndex, ArrayBufferView::ViewType viewType );
        CreateTemplate( ConstStArrDb, int srcOffset, int constIndex, ArrayBufferView::ViewType viewType);
        CreateTemplate( ConstStArrFlt, int srcOffset, int constIndex, ArrayBufferView::ViewType viewType);

        CreateTemplate(AsmJsLoopBody, int offset);

        CreateTemplate(Simd128_Ld_F4, int targetOffsetF4, int srcOffsetF4);
        CreateTemplate(Simd128_Ld_I4, int targetOffsetI4, int srcOffsetI4);
        CreateTemplate(Simd128_Ld_D2, int targetOffsetD2, int srcOffsetD2);

        CreateTemplate(Simd128_LdSlot_F4, int targetOffset, int slotIndex);
        CreateTemplate(Simd128_LdSlot_I4, int targetOffset, int slotIndex);
        CreateTemplate(Simd128_LdSlot_D2, int targetOffset, int slotIndex);

        CreateTemplate(Simd128_StSlot_F4, int srcOffset, int slotIndex);
        CreateTemplate(Simd128_StSlot_I4, int srcOffset, int slotIndex);
        CreateTemplate(Simd128_StSlot_D2, int srcOffset, int slotIndex);

        CreateTemplate(Simd128_FloatsToF4, int targetOffsetF4_0, int srcOffsetF1, int srcOffsetF2, int srcOffsetF3, int srcOffsetF4);
        CreateTemplate(Simd128_IntsToI4, int targetOffsetI4_0, int srcOffsetI1, int srcOffsetI2, int srcOffsetI3, int srcOffsetI4);
        CreateTemplate(Simd128_DoublesToD2, int targetOffsetD2_0, int srcOffsetD0, int srcOffsetD1);

        CreateTemplate(Simd128_Return_F4, int srcOffsetF4);
        CreateTemplate(Simd128_Return_I4, int srcOffsetI4);
        CreateTemplate(Simd128_Return_D2, int srcOffsetD2);

        CreateTemplate(Simd128_Splat_F4, int targetOffsetF4_0, int srcOffsetF1);
        CreateTemplate(Simd128_Splat_I4, int targetOffsetI4_0, int srcOffsetI1);
        CreateTemplate(Simd128_Splat_D2, int targetOffsetD2_0, int srcOffsetD1);

        CreateTemplate(Simd128_FromFloat64x2_F4, int targetOffsetF4_0, int srcOffsetD2_1);
        CreateTemplate(Simd128_FromInt32x4_F4,   int targetOffsetF4_0, int srcOffsetI4_1);
        CreateTemplate(Simd128_FromFloat32x4_I4, int targetOffsetI4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_FromFloat64x2_I4, int targetOffsetI4_0, int srcOffsetD2_1);
        CreateTemplate(Simd128_FromFloat32x4_D2, int targetOffsetD2_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_FromInt32x4_D2, int targetOffsetD2_0, int srcOffsetI4_1);

        CreateTemplate(Simd128_FromFloat32x4Bits_D2, int targetOffsetD2_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_FromInt32x4Bits_D2, int targetOffsetD2_0, int srcOffsetI4_1);
        CreateTemplate(Simd128_FromFloat32x4Bits_I4, int targetOffsetI4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_FromFloat64x2Bits_I4, int targetOffsetI4_0, int srcOffsetD2_1);
        CreateTemplate(Simd128_FromFloat64x2Bits_F4, int targetOffsetF4_0, int srcOffsetD2_1);
        CreateTemplate(Simd128_FromInt32x4Bits_F4, int targetOffsetF4_0, int srcOffsetI4_1);

        CreateTemplate(Simd128_Abs_F4, int targetOffsetF4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_Abs_D2, int targetOffsetD2_0, int srcOffsetD2_1);

        CreateTemplate(Simd128_Neg_F4, int targetOffsetF4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_Neg_I4, int targetOffsetI4_0, int srcOffsetI4_1);
        CreateTemplate(Simd128_Neg_D2, int targetOffsetD2_0, int srcOffsetD2_1);

        CreateTemplate(Simd128_Rcp_F4, int targetOffsetF4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_Rcp_D2, int targetOffsetD2_0, int srcOffsetD2_1);

        CreateTemplate(Simd128_RcpSqrt_F4, int targetOffsetF4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_RcpSqrt_D2, int targetOffsetD2_0, int srcOffsetD2_1);

        CreateTemplate(Simd128_Sqrt_F4, int targetOffsetF4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_Sqrt_D2, int targetOffsetD2_0, int srcOffsetD2_1);

        CreateTemplate(Simd128_Not_F4, int targetOffsetF4_0, int srcOffsetF4_1);
        CreateTemplate(Simd128_Not_I4, int targetOffsetI4_0, int srcOffsetI4_1);

        CreateTemplate(Simd128_Add_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Add_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);
        CreateTemplate(Simd128_Add_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Sub_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Sub_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);
        CreateTemplate(Simd128_Sub_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Mul_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Mul_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);
        CreateTemplate(Simd128_Mul_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Div_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Div_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Min_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Min_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Max_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Max_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Lt_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Lt_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);
        CreateTemplate(Simd128_Lt_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Gt_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Gt_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);
        CreateTemplate(Simd128_Gt_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_LtEq_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_LtEq_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_GtEq_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_GtEq_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Eq_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Eq_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);
        CreateTemplate(Simd128_Eq_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_Neq_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Neq_D2, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2);

        CreateTemplate(Simd128_And_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_And_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);

        CreateTemplate(Simd128_Or_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Or_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);

        CreateTemplate(Simd128_Xor_F4, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2);
        CreateTemplate(Simd128_Xor_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2);

        CreateTemplate(Simd128_Select_F4, int targetOffsetF4_0, int srcOffsetI4_1, int srcOffsetF4_2, int srcOffsetF4_3);
        CreateTemplate(Simd128_Select_I4, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2, int srcOffsetI4_3);
        CreateTemplate(Simd128_Select_D2, int targetOffsetD2_0, int srcOffsetI4_1, int srcOffsetD2_2, int srcOffsetD2_3);

        CreateTemplate(Simd128_ExtractLane_I4, int targetOffsetI0, int srcOffsetI4_1, int index);
        CreateTemplate(Simd128_ExtractLane_F4, int targetOffsetF0, int srcOffsetF4_1, int index);
        CreateTemplate(Simd128_ReplaceLane_I4, int targetOffsetI4_0, int srcOffsetI4_1, int index, int srcOffsetI3);
        CreateTemplate(Simd128_ReplaceLane_F4, int targetOffsetF4_0, int srcOffsetF4_1, int index, int srcOffsetF3);

        CreateTemplate(Simd128_LdSignMask_F4, int targetOffsetI0, int srcOffsetF4_1);
        CreateTemplate(Simd128_LdSignMask_I4, int targetOffsetI0, int srcOffsetI4_1);
        CreateTemplate(Simd128_LdSignMask_D2, int targetOffsetI0, int srcOffsetD2_1);

        CreateTemplate(Simd128_I_ArgOut_F4, int argIndex, int offset );
        CreateTemplate(Simd128_I_ArgOut_I4, int argIndex, int offset);
        CreateTemplate(Simd128_I_ArgOut_D2, int argIndex, int offset);

        CreateTemplate(Simd128_I_Conv_VTF4, int targetOffset, int srcOffset);
        CreateTemplate(Simd128_I_Conv_VTI4, int targetOffset, int srcOffset);
        CreateTemplate(Simd128_I_Conv_VTD2, int targetOffset, int srcOffset);
    };

};
#endif
