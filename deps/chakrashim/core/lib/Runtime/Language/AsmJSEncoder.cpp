//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#ifndef TEMP_DISABLE_ASMJS

#include "..\Backend\CodeGenAllocators.h"
#ifdef DBG_DUMP
#include "ByteCode\ByteCodeDumper.h"
#include "ByteCode\AsmJSByteCodeDumper.h"
#endif
#include "AsmJSEncoder.inl"

#if DBG_DUMP
#include "ByteCode\OpCodeUtilAsmJs.h"
#endif

namespace Js
{
    template<> int AsmJsEncoder::GetOffset<int>() const{return mIntOffset;}
    template<> int AsmJsEncoder::GetOffset<Var>() const{return AsmJsJitTemplate::Globals::StackVarCount * sizeof( Var );}
    template<> int AsmJsEncoder::GetOffset<double>() const{ return mDoubleOffset; }
    template<> int AsmJsEncoder::GetOffset<float>() const{ return mFloatOffset; }
    template<> int AsmJsEncoder::GetOffset<AsmJsSIMDValue>() const{ return mSimdOffset; }

    template<>
    void AsmJsEncoder::ReadOpTemplate<Js::SmallLayout>( OpCodeAsmJs op )
    {
        switch( op )
        {
#define DEF2(x, op, func) PROCESS_ENCODE_##x(op, func)
#define DEF3(x, op, func, y) PROCESS_ENCODE_##x(op, func, y)
#define DEF2_WMS(x, op, func) PROCESS_ENCODE_##x##_COMMON(op, func, _Small)
#define DEF3_WMS(x, op, func, y) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Small)
#define DEF4_WMS(x, op, func, y, t) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Small, t)
#define EXDEF2(x, op, func) PROCESS_ENCODE_##x(op, func)
#define EXDEF3(x, op, func, y) PROCESS_ENCODE_##x(op, func, y)
#define EXDEF2_WMS(x, op, func) PROCESS_ENCODE_##x##_COMMON(op, func, _Small)
#define EXDEF3_WMS(x, op, func, y) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Small)
#define EXDEF4_WMS(x, op, func, y, t) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Small, t)
#include "AsmJSEncoderHandler.inl"
        default:
            // Help the C++ optimizer by declaring that the cases we
            // have above are sufficient
#if DBG_DUMP
            Output::Print( L"Dispatch to bad opcode : %s\n", OpCodeUtilAsmJs::GetOpCodeName(op));
            Output::Flush();
#endif
            Assert( false );
            __assume( false );
        };
    }

    template<>
    void AsmJsEncoder::ReadOpTemplate<Js::MediumLayout>( OpCodeAsmJs op )
    {
        switch( op )
        {
#define DEF2_WMS(x, op, func) PROCESS_ENCODE_##x##_COMMON(op, func, _Medium)
#define DEF3_WMS(x, op, func, y) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Medium)
#define DEF4_WMS(x, op, func, y, t) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Medium, t)
#define EXDEF2_WMS(x, op, func) PROCESS_ENCODE_##x##_COMMON(op, func, _Medium)
#define EXDEF3_WMS(x, op, func, y) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Medium)
#define EXDEF4_WMS(x, op, func, y, t) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Medium, t)
#include "AsmJSEncoderHandler.inl"
        default:
            // Help the C++ optimizer by declaring that the cases we
            // have above are sufficient
#if DBG_DUMP
            Output::Print( L"Dispatch to bad opcode : %s\n", OpCodeUtilAsmJs::GetOpCodeName(op));
            Output::Flush();
#endif
            Assert( false );
            __assume( false );
        };
    }

    template<>
    void AsmJsEncoder::ReadOpTemplate<Js::LargeLayout>( OpCodeAsmJs op )
    {
        switch( op )
        {
#define DEF2_WMS(x, op, func) PROCESS_ENCODE_##x##_COMMON(op, func, _Large)
#define DEF3_WMS(x, op, func, y) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Large)
#define DEF4_WMS(x, op, func, y, t) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Large, t)
#define EXDEF2_WMS(x, op, func) PROCESS_ENCODE_##x##_COMMON(op, func, _Large)
#define EXDEF3_WMS(x, op, func, y) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Large)
#define EXDEF4_WMS(x, op, func, y, t) PROCESS_ENCODE_##x##_COMMON(op, func, y, _Large, t)
#include "AsmJSEncoderHandler.inl"
        default:
            // Help the C++ optimizer by declaring that the cases we
            // have above are sufficient
#if DBG_DUMP
            Output::Print( L"Dispatch to bad opcode : %s\n", OpCodeUtilAsmJs::GetOpCodeName(op));
            Output::Flush();
#endif
            Assert( false );
            __assume( false );
        };
    }

    bool AsmJsEncoder::ReadOp()
    {
#if DBG_DUMP
        int bytecodeoffset = mReader.GetCurrentOffset();
#endif
        LayoutSize layoutSize;
        OpCodeAsmJs op = (OpCodeAsmJs)mReader.ReadOp(layoutSize);
        ip = mReader.GetIP();
#if DBG_DUMP
        if (PHASE_TRACE(Js::AsmjsEncoderPhase, mFunctionBody))
        {
            Output::Print(L"%d.%d:Encoding ",
                           this->mFunctionBody->GetSourceContextId(),
                           this->mFunctionBody->GetLocalFunctionId());
            AsmJsByteCodeDumper::DumpOp( op, layoutSize, mReader, mFunctionBody );
            if( ip != mReader.GetIP() )
            {
                mReader.SetIP( ip );
            }
            Output::Print(L"  at offset 0x%X (buffer size = 0x%X)\n",
                           bytecodeoffset, (int)(mPc-mEncodeBuffer));
            Output::Flush();
        }
#endif
        if( op == OpCodeAsmJs::EndOfBlock )
        {
            Assert(mReader.GetCurrentOffset() == mFunctionBody->GetByteCode()->GetLength());
            // last bytecode
            return false;
        }
        switch( layoutSize )
        {
        case Js::SmallLayout:
            ReadOpTemplate<Js::SmallLayout>( op );
            break;
        case Js::MediumLayout:
            ReadOpTemplate<Js::MediumLayout>( op );
            break;
        case Js::LargeLayout:
            ReadOpTemplate<Js::LargeLayout>( op );
            break;
        default:
            break;
        }
        return true;
    }
    uint32 AsmJsEncoder::GetEncodeBufferSize(FunctionBody* functionBody)
    {
        // TODO: Make a good heuristic; this is completely arbitrary. As we emit each bytecode we can calculate the max instruction size.
        return UInt32Math::Add(
            UInt32Math::Mul(functionBody->GetByteCodeCount(), 30),
            49 /*prolog*/   + 11 /*epilog*/
            );
    }

    void* AsmJsEncoder::Encode( FunctionBody* functionBody )
    {
        Assert( functionBody );
        mFunctionBody = functionBody;
#if DBG_DUMP
        AsmJsJitTemplate::Globals::CurrentEncodingFunction = mFunctionBody;
#endif
        AsmJsFunctionInfo* asmInfo = functionBody->GetAsmJsFunctionInfo();
        FunctionEntryPointInfo* entryPointInfo = ((FunctionEntryPointInfo*)(functionBody->GetDefaultEntryPointInfo()));
        // number of var on the stack + ebp + eip
        mIntOffset = asmInfo->GetIntByteOffset() + GetOffset<Var>();
        mDoubleOffset = asmInfo->GetDoubleByteOffset() + GetOffset<Var>();
        mFloatOffset = asmInfo->GetFloatByteOffset() + GetOffset<Var>();
        mSimdOffset = asmInfo->GetSimdByteOffset() + GetOffset<Var>();

        NoRecoverMemoryArenaAllocator localAlloc(L"BE-AsmJsEncoder", GetPageAllocator(), Js::Throw::OutOfMemory);
        mLocalAlloc = &localAlloc;

        mRelocLabelMap = Anew( mLocalAlloc, RelocLabelMap, mLocalAlloc );
        mTemplateData = AsmJsJitTemplate::InitTemplateData();
        mEncodeBufferSize = GetEncodeBufferSize(functionBody);
        mEncodeBuffer = AnewArray((&localAlloc), BYTE, mEncodeBufferSize);
        mPc = mEncodeBuffer;
        mReader.Create( functionBody );
        ip = mReader.GetIP();

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if( PHASE_TRACE( Js::AsmjsEncoderPhase, mFunctionBody ) )
        {
            Output::Print( L"\n\n" );
            functionBody->DumpFullFunctionName();
            Output::Print( L"\n StackSize = %d , Offsets: Var = %d, Int = %d, Double = %d\n", mFunctionBody->GetAsmJsFunctionInfo()->GetTotalSizeinBytes(), GetOffset<Var>(), GetOffset<int>(), GetOffset<double>() );
        }
#endif

        AsmJsRetType retType = asmInfo->GetReturnType();
        AsmJsJitTemplate::FunctionEntry::ApplyTemplate( this, mPc );
        while( ReadOp() ){}
        AsmJsJitTemplate::FunctionExit::ApplyTemplate( this, mPc );

        AsmJsJitTemplate::FreeTemplateData( mTemplateData );
#if DBG_DUMP
        AsmJsJitTemplate::Globals::CurrentEncodingFunction = nullptr;
#endif
        ApplyRelocs();

        ptrdiff_t codeSize = mPc - mEncodeBuffer;
        if( codeSize > 0 )
        {
            Assert( ::Math::FitsInDWord( codeSize ) );

            BYTE *buffer;
            EmitBufferAllocation *allocation = GetCodeGenAllocator()->emitBufferManager.AllocateBuffer( codeSize, &buffer, false, 0, 0 );
            functionBody->GetAsmJsFunctionInfo()->mTJBeginAddress = buffer;

            Assert( allocation != nullptr );
            if( buffer == nullptr )
                Js::Throw::OutOfMemory();

            if (!GetCodeGenAllocator()->emitBufferManager.CommitBuffer(allocation, buffer, codeSize, mEncodeBuffer))
            {
                Js::Throw::OutOfMemory();
            }

            functionBody->GetScriptContext()->GetThreadContext()->SetValidCallTargetForCFG(buffer);

            // TODO: improve this once EntryPoint cleanup work is complete!
#if 0
            const wchar_t *const functionName = functionBody->GetDisplayName();
            const wchar_t *const suffix = L"TJ";
            wchar_t functionNameArray[256];
            const size_t functionNameCharLength = functionBody->GetDisplayNameLength();
            wcscpy_s(functionNameArray, 256, functionName);
            wcscpy_s(&functionNameArray[functionNameCharLength], 256 - functionNameCharLength, suffix);
#endif
            JS_ETW(EventWriteMethodLoad(functionBody->GetScriptContext(),
                (void *)buffer,
                codeSize,
                EtwTrace::GetFunctionId(functionBody),
                0 /* methodFlags - for future use*/,
                MethodType_Jit,
                EtwTrace::GetSourceId(functionBody),
                functionBody->GetLineNumber(),
                functionBody->GetColumnNumber(),
                functionBody->GetDisplayName()));
            entryPointInfo->SetTJCodeGenDone(); // set the codegen to done state for TJ
            entryPointInfo->SetCodeSize(codeSize);
            return buffer;
        }
        return nullptr;
    }



    void Js::AsmJsEncoder::AddReloc( const int labelOffset, BYTE* patchAddr )
    {
        EncoderRelocLabel* label = nullptr;
        if( mRelocLabelMap->TryGetReference( labelOffset, &label ) )
        {
            EncoderReloc::New( label, patchAddr, mPc, mLocalAlloc );
        }
        else
        {
            EncoderRelocLabel newLabel;
            EncoderReloc::New( &newLabel, patchAddr, mPc, mLocalAlloc );
            mRelocLabelMap->AddNew( labelOffset, newLabel );
        }
    }

    void AsmJsEncoder::ApplyRelocs()
    {
        const int size = mRelocLabelMap->Count();
        for (int i = 0; i < size ; i++)
        {
            EncoderRelocLabel* label = mRelocLabelMap->GetReferenceAt( i );
#if DBG_DUMP
            if( !label->labelSeen )
            {
                Output::Print( L"Label expected at bytecode offset 0x%x\n", mRelocLabelMap->GetKeyAt( i ) );
                Output::Flush();
            }
#endif
            Assert( label->labelSeen );

            EncoderReloc* reloc = label->relocList;
            ptrdiff_t offset1 = label->pc - mEncodeBuffer;
            this->GetAsmJsFunctionInfo()->mbyteCodeTJMap->AddNew(mRelocLabelMap->GetKeyAt(i), offset1);
            while( reloc )
            {
                ptrdiff_t offset = label->pc - reloc->pc;
                *(ptrdiff_t*)reloc->patchAddr = offset;
                reloc = reloc->next;
            }
        }
    }

    void AsmJsEncoder::EncoderReloc::New( EncoderRelocLabel* label, BYTE* _patchAddr, BYTE* _pc, ArenaAllocator* allocator )
    {
        AsmJsEncoder::EncoderReloc* reloc = AnewStruct( allocator, AsmJsEncoder::EncoderReloc );
        reloc->next = label->relocList;
        label->relocList = reloc;
        reloc->patchAddr = _patchAddr;
        reloc->pc = _pc;
    }

};
#endif
