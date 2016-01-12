//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{
    class AsmJsEncoder
    {
        struct EncoderRelocLabel;
        struct EncoderReloc
        {
            static void New( EncoderRelocLabel* label, BYTE* _patchAddr, BYTE* _pc, ArenaAllocator* allocator );
            BYTE* patchAddr;
            BYTE* pc;
            EncoderReloc* next;
        };
        struct EncoderRelocLabel
        {
            EncoderRelocLabel() :labelSeen( false ), relocList( nullptr ){}
            EncoderRelocLabel(BYTE* _pc) :labelSeen( true ), pc(_pc), relocList( nullptr ){}
            bool labelSeen : 1;
            BYTE* pc;
            EncoderReloc* relocList;
        };
        // the key is the bytecode address
        typedef JsUtil::BaseDictionary<int, EncoderRelocLabel, ArenaAllocator> RelocLabelMap;
        const byte* ip;
        ByteCodeReader mReader;
        uint32 mEncodeBufferSize;
        BYTE* mEncodeBuffer;
        BYTE* mPc;
        PageAllocator* mPageAllocator;
        CodeGenAllocators* mForegroundAllocators;
        FunctionBody* mFunctionBody;
        RelocLabelMap* mRelocLabelMap;
        ArenaAllocator* mLocalAlloc;
        // Byte offset of first int and double
        int mIntOffset, mDoubleOffset, mFloatOffset;
        int mSimdOffset;
        // architecture dependant data to build templatized JIT
        void* mTemplateData;
    public:
        void* Encode( FunctionBody* functionBody );
        void* GetTemplateData() { return mTemplateData; }
        inline PageAllocator* GetPageAllocator() const{return mPageAllocator;}
        inline void SetPageAllocator( PageAllocator* val ){mPageAllocator = val;}
        inline CodeGenAllocators* GetCodeGenAllocator() const{return mForegroundAllocators;}
        inline void SetCodeGenAllocator( CodeGenAllocators* val ){mForegroundAllocators = val;}
        FunctionBody* GetFunctionBody() { return mFunctionBody; }

    private:
        void ApplyRelocs();
        void AddReloc( const int labelOffset, BYTE* patchAddr );
        uint32 GetEncodeBufferSize(FunctionBody* functionBody);
        AsmJsFunctionInfo* GetAsmJsFunctionInfo(){ return mFunctionBody->GetAsmJsFunctionInfo(); }
        bool ReadOp();
        template<LayoutSize T> void ReadOpTemplate( OpCodeAsmJs op );

        template<typename T> int GetOffset() const;

        template<typename T> int CalculateOffset(int stackLocation) { return stackLocation*sizeof(T)+GetOffset<T>(); }


        void OP_Label( const unaligned OpLayoutEmpty* playout );
        template <class T> void OP_LdUndef( const unaligned T* playout );
        template <class T> void OP_Br( const unaligned T* playout );
        template <class T> void OP_BrEq( const unaligned T* playout );
        template <class T> void OP_BrTrue( const unaligned T* playout );
        template <class T> void OP_Empty( const unaligned T* playout );
        template <class T> void Op_LdSlot_Db( const unaligned T* playout );
        template <class T> void Op_LdSlot_Int(const unaligned T* playout);
        template <class T> void Op_LdSlot_Flt(const unaligned T* playout);
        template <class T> void Op_StSlot_Db( const unaligned T* playout );
        template <class T> void Op_StSlot_Int(const unaligned T* playout);
        template <class T> void Op_StSlot_Flt(const unaligned T* playout);
        template <class T> void Op_LdConst_Int(const unaligned T* playout);

        template <class T> void Op_LdArr     ( const unaligned T* playout );
        template <class T> void Op_LdArrConst( const unaligned T* playout );
        template <class T> void Op_StArr     ( const unaligned T* playout );
        template <class T> void Op_StArrConst( const unaligned T* playout );

        template <class T> void OP_SetReturnInt( const unaligned T* playout );
        template <class T> void OP_SetReturnDouble(const unaligned T* playout);
        template <class T> void OP_SetReturnFloat(const unaligned T* playout);
        template <class T> void OP_SetFroundInt(const unaligned T* playout);
        template <class T> void OP_SetFroundDb(const unaligned T* playout);
        template <class T> void OP_SetFroundFlt(const unaligned T* playout);
        template <class T> void Op_Float_To_Int(const unaligned T* playout);
        template <class T> void Op_Float_To_Db(const unaligned T* playout);
        template <class T> void Op_UInt_To_Db(const unaligned T* playout);
        template <class T> void Op_Int_To_Db( const unaligned T* playout );
        template <class T> void Op_Db_To_Int( const unaligned T* playout );
        template <class T> void Op_LdAddr_Db( const unaligned T* playout );
        template <class T> void OP_LdSlot( const unaligned T* playout );

        template <class T> void OP_StartCall( const unaligned T* playout );
        template <class T> void OP_Call( const unaligned T* playout );
        template <class T> void OP_ArgOut_Db( const unaligned T* playout );
        template <class T> void OP_ArgOut_Int(const unaligned T* playout);
        template <class T> void OP_Conv_VTD( const unaligned T* playout );
        template <class T> void OP_Conv_VTI( const unaligned T* playout );
        template <class T> void OP_Conv_VTF( const unaligned T* playout );

        template <class T> void OP_I_StartCall( const unaligned T* playout );
        template <class T> void OP_I_Call( const unaligned T* playout );
        template <class T> void OP_I_ArgOut_Db( const unaligned T* playout );
        template <class T> void OP_I_ArgOut_Int(const unaligned T* playout);
        template <class T> void OP_I_ArgOut_Flt(const unaligned T* playout);
        template <class T> void OP_I_Conv_VTD( const unaligned T* playout );
        template <class T> void OP_I_Conv_VTI( const unaligned T* playout );
        template <class T> void OP_I_Conv_VTF( const unaligned T* playout );

        template <class T> void OP_AsmJsLoopBody(const unaligned T* playout);

        template <class T> void OP_Simd128_LdF4(const unaligned T* playout);
        template <class T> void OP_Simd128_LdI4(const unaligned T* playout);
        template <class T> void OP_Simd128_LdD2(const unaligned T* playout);

        template <class T> void OP_Simd128_LdSlotF4(const unaligned T* playout);
        template <class T> void OP_Simd128_LdSlotI4(const unaligned T* playout);
        template <class T> void OP_Simd128_LdSlotD2(const unaligned T* playout);
        template <class T> void OP_Simd128_StSlotF4(const unaligned T* playout);
        template <class T> void OP_Simd128_StSlotI4(const unaligned T* playout);
        template <class T> void OP_Simd128_StSlotD2(const unaligned T* playout);

        template <class T> void OP_Simd128_FloatsToF4(const unaligned T* playout);
        template <class T> void OP_Simd128_IntsToI4(const unaligned T* playout);
        template <class T> void OP_Simd128_DoublesToD2(const unaligned T* playout);

        template <class T> void OP_Simd128_ReturnF4(const unaligned T* playout);
        template <class T> void OP_Simd128_ReturnI4(const unaligned T* playout);
        template <class T> void OP_Simd128_ReturnD2(const unaligned T* playout);

        template <class T> void OP_Simd128_SplatF4(const unaligned T* playout);
        template <class T> void OP_Simd128_SplatI4(const unaligned T* playout);
        template <class T> void OP_Simd128_SplatD2(const unaligned T* playout);

        template <class T> void OP_Simd128_FromFloat64x2F4(const unaligned T* playout);
        template <class T> void OP_Simd128_FromInt32x4F4(const unaligned T* playout);
        template <class T> void OP_Simd128_FromFloat32x4I4(const unaligned T* playout);
        template <class T> void OP_Simd128_FromFloat64x2I4(const unaligned T* playout);
        template <class T> void OP_Simd128_FromFloat32x4D2(const unaligned T* playout);
        template <class T> void OP_Simd128_FromInt32x4D2(const unaligned T* playout);

        template <class T> void OP_Simd128_FromFloat32x4BitsD2(const unaligned T* playout);
        template <class T> void OP_Simd128_FromInt32x4BitsD2(const unaligned T* playout);
        template <class T> void OP_Simd128_FromFloat32x4BitsI4(const unaligned T* playout);
        template <class T> void OP_Simd128_FromFloat64x2BitsI4(const unaligned T* playout);
        template <class T> void OP_Simd128_FromFloat64x2BitsF4(const unaligned T* playout);
        template <class T> void OP_Simd128_FromInt32x4BitsF4(const unaligned T* playout);

        template <class T> void OP_Simd128_AbsF4(const unaligned T* playout);
        template <class T> void OP_Simd128_AbsD2(const unaligned T* playout);
        template <class T> void OP_Simd128_NegF4(const unaligned T* playout);
        template <class T> void OP_Simd128_NegI4(const unaligned T* playout);
        template <class T> void OP_Simd128_NegD2(const unaligned T* playout);
        template <class T> void OP_Simd128_RcpF4(const unaligned T* playout);
        template <class T> void OP_Simd128_RcpD2(const unaligned T* playout);
        template <class T> void OP_Simd128_RcpSqrtF4(const unaligned T* playout);
        template <class T> void OP_Simd128_RcpSqrtD2(const unaligned T* playout);
        template <class T> void OP_Simd128_SqrtF4(const unaligned T* playout);
        template <class T> void OP_Simd128_SqrtD2(const unaligned T* playout);

        template <class T> void OP_Simd128_NotF4(const unaligned T* playout);
        template <class T> void OP_Simd128_NotI4(const unaligned T* playout);
        template <class T> void OP_Simd128_AddF4(const unaligned T* playout);
        template <class T> void OP_Simd128_AddI4(const unaligned T* playout);
        template <class T> void OP_Simd128_AddD2(const unaligned T* playout);
        template <class T> void OP_Simd128_SubF4(const unaligned T* playout);
        template <class T> void OP_Simd128_SubI4(const unaligned T* playout);
        template <class T> void OP_Simd128_SubD2(const unaligned T* playout);
        template <class T> void OP_Simd128_MulF4(const unaligned T* playout);
        template <class T> void OP_Simd128_MulI4(const unaligned T* playout);
        template <class T> void OP_Simd128_MulD2(const unaligned T* playout);
        template <class T> void OP_Simd128_DivF4(const unaligned T* playout);
        template <class T> void OP_Simd128_DivD2(const unaligned T* playout);

        template <class T> void OP_Simd128_MinF4(const unaligned T* playout);
        template <class T> void OP_Simd128_MinD2(const unaligned T* playout);
        template <class T> void OP_Simd128_MaxF4(const unaligned T* playout);
        template <class T> void OP_Simd128_MaxD2(const unaligned T* playout);

        template <class T> void OP_Simd128_LtF4(const unaligned T* playout);
        template <class T> void OP_Simd128_LtI4(const unaligned T* playout);
        template <class T> void OP_Simd128_LtD2(const unaligned T* playout);
        template <class T> void OP_Simd128_GtF4(const unaligned T* playout);
        template <class T> void OP_Simd128_GtI4(const unaligned T* playout);
        template <class T> void OP_Simd128_GtD2(const unaligned T* playout);
        template <class T> void OP_Simd128_LtEqF4(const unaligned T* playout);
        template <class T> void OP_Simd128_LtEqD2(const unaligned T* playout);
        template <class T> void OP_Simd128_GtEqF4(const unaligned T* playout);
        template <class T> void OP_Simd128_GtEqD2(const unaligned T* playout);
        template <class T> void OP_Simd128_EqF4(const unaligned T* playout);
        template <class T> void OP_Simd128_EqI4(const unaligned T* playout);
        template <class T> void OP_Simd128_EqD2(const unaligned T* playout);
        template <class T> void OP_Simd128_NeqF4(const unaligned T* playout);
        template <class T> void OP_Simd128_NeqD2(const unaligned T* playout);

        template <class T> void OP_Simd128_AndF4(const unaligned T* playout);
        template <class T> void OP_Simd128_AndI4(const unaligned T* playout);
        template <class T> void OP_Simd128_OrF4(const unaligned T* playout);
        template <class T> void OP_Simd128_OrI4(const unaligned T* playout);
        template <class T> void OP_Simd128_XorF4(const unaligned T* playout);
        template <class T> void OP_Simd128_XorI4(const unaligned T* playout);

        template <class T> void OP_Simd128_SelectF4(const unaligned T* playout);
        template <class T> void OP_Simd128_SelectI4(const unaligned T* playout);
        template <class T> void OP_Simd128_SelectD2(const unaligned T* playout);

        template <class T> void OP_Simd128_LdSignMaskF4(const unaligned T* playout);
        template <class T> void OP_Simd128_LdSignMaskI4(const unaligned T* playout);
        template <class T> void OP_Simd128_LdSignMaskD2(const unaligned T* playout);

        template <class T> void OP_Simd128_ExtractLaneI4(const unaligned T* playout);
        template <class T> void OP_Simd128_ExtractLaneF4(const unaligned T* playout);
        template <class T> void OP_Simd128_ReplaceLaneI4(const unaligned T* playout);
        template <class T> void OP_Simd128_ReplaceLaneF4(const unaligned T* playout);

        template <class T> void OP_Simd128_I_ArgOutF4(const unaligned T* playout);
        template <class T> void OP_Simd128_I_ArgOutI4(const unaligned T* playout);
        template <class T> void OP_Simd128_I_ArgOutD2(const unaligned T* playout);

        template <class T> void OP_Simd128_I_Conv_VTF4(const unaligned T* playout);
        template <class T> void OP_Simd128_I_Conv_VTI4(const unaligned T* playout);
        template <class T> void OP_Simd128_I_Conv_VTD2(const unaligned T* playout);
    };
}

#endif
