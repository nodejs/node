//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_NATIVE_CODEGEN

#include "..\BackEnd\i386\Reg.h"

static const BYTE RegEncode[] =
{
#define REGDAT(Name, Listing, Encoding, ...) Encoding,
#include "..\BackEnd\i386\RegList.h"
#undef REGDAT
};

#if DBG_DUMP || ENABLE_DEBUG_CONFIG_OPTIONS
extern wchar_t const * const RegNamesW[];
#endif

#include "AsmJsInstructionTemplate.h"
namespace Js
{
    // Mask of Registers that can be saved through function calls
    const uint MaskNonVolatileReg = 1 << RegEBX | 1 << RegEDI | 1 << RegESI;

    // Reserved RegEDI for ArrayBuffer length
    const RegNum ModuleEnvReg = RegEDI;
    const RegNum ArrayBufferReg = RegESI;

    // Registers that can't be chosen for general purposes
    const uint MaskUnavailableReg = 1 << RegESP | 1 << RegEBP | 1 << ModuleEnvReg | 1 << ArrayBufferReg | 1 << RegNOREG;

    // Mask for Register in enum RegNum [EAX,ECX,EDX,EBX,ESI,EDI]
    const uint Mask32BitsReg = ( ( 1 << ( FIRST_FLOAT_REG ) ) - 1 ) & ~MaskUnavailableReg ;

    // Mask for Register in enum RegNum [EAX,ECX,EDX,EBX] aka [al,cl,dl,bl]
    const uint Mask8BitsReg = Mask32BitsReg &~(1<<RegEBP|1<<RegESP|1<<RegESI|1<<RegEDI);

    // Mask for Register in enum RegNum [XMM0,XMM1,XMM2,XMM3,XMM4,XMM5,XMM6,XMM7]
    const uint Mask64BitsReg = ((1 << (FIRST_FLOAT_REG+XMM_REGCOUNT))-1) & ~MaskUnavailableReg & ~Mask32BitsReg;

    // Template version to access register mask
    template<typename T> uint GetRegMask();
    template<> uint GetRegMask<int>() { return Mask32BitsReg; }
    template<> uint GetRegMask<double>() { return Mask64BitsReg; }
    template<> uint GetRegMask<float>() { return Mask64BitsReg; }
    template<> uint GetRegMask<AsmJsSIMDValue>() { return Mask64BitsReg; }


    // Template version to access first register available
    template<typename T> RegNum GetFirstReg();
    template<> RegNum GetFirstReg<int>() { return FIRST_INT_REG; }
    template<> RegNum GetFirstReg<double>() { return FIRST_FLOAT_REG; }
    template<> RegNum GetFirstReg<float>() { return FIRST_FLOAT_REG; }
    template<> RegNum GetFirstReg<AsmJsSIMDValue>() { return FIRST_FLOAT_REG; }

    // Returns the last register available + 1, forms an upper bound  [GetFirstReg, GetLastReg[
    template<typename T> RegNum GetLastReg() { return RegNum(GetFirstReg<T>()+8); }

    struct InternalCallInfo
    {
        // size in bytes of arguments
        int argByteSize;
        int nextArgIndex;
        int currentOffset;
        InternalCallInfo* next;
    };

    struct X86TemplateData
    {
    private:
        InternalCallInfo* mCallInfoList;
        // Bit vector : 1 means a useful information is known for this RegNum.
        // Never set an unavailable register flag to 1
        int mAnyStackSaved;
        // Stack offset saved for registers
        int mRegisterStackOffsetSaved[RegNumCount];
        // Value range [0,8[ add GetFirstReg() for RegNum
        RegNum mNext32BitsReg, mNext64BitsReg;
        // Template version to access the Next Register
        template<typename T> RegNum GetNextRegister();
        template<typename T> void SetNextRegister(RegNum reg);
        int mBaseOffset;
        int mScriptContextOffSet;
        int mModuleSlotOffset;
        int mModuleEnvOffset;
        int mArrayBufferOffSet;
        int mArraySizeOffset;
        // Applies the register choosing algorithm and returns it
        template<typename T> RegNum GetNextReg(RegNum reg);
        template<> RegNum GetNextReg<int>(RegNum reg)
        {
            return RegNum((reg + 1) % GetLastReg<int>());
        }
        template<> RegNum GetNextReg<double>(RegNum reg)
        {
            RegNum nextReg = RegNum((reg + 1) % GetLastReg<double>());
            if (nextReg < GetFirstReg<double>())
            {
                return RegNum(GetFirstReg<double>());
            }
            return nextReg;
        }
        template<typename T> RegNum NextReg(const int registerRestriction)
        {
            RegNum reg = GetNextRegister<T>();
            const uint unavailable = registerRestriction | MaskUnavailableReg;
            Assert( unavailable != GetRegMask<T>() );
            if( (1<<reg) & unavailable )
            {
                while( (1<<reg) & unavailable )
                {
                    reg = GetNextReg<T>(reg);
                }
                Assert( !(1 << reg & unavailable) );
                return reg; // do not change the next register
            }
            RegNum next = reg;
            do
            {
                next = GetNextReg<T>(next);
            } while( ( 1 << next ) & MaskUnavailableReg );
            SetNextRegister<T>( next );
            Assert( !(1 << reg & unavailable) );
            return reg;
        }
    public:
        X86TemplateData()
        {
            Assert( !( (1<<GetFirstReg<int>()) & MaskUnavailableReg ) );
            Assert(!((1 << GetFirstReg<double>()) & MaskUnavailableReg));
            Assert(!((1 << GetFirstReg<float>()) & MaskUnavailableReg));
            mNext32BitsReg = GetFirstReg<int>();
            mNext64BitsReg = GetFirstReg<double>(); // it is the same for float
            mAnyStackSaved = 0;
            mCallInfoList = nullptr;
            for (int i = 0; i < RegNumCount ; i++)
            {
                mRegisterStackOffsetSaved[i] = 0;
            }
        }

        ~X86TemplateData()
        {
            Assert( !mCallInfoList );
        }

        InternalCallInfo* GetInternalCallInfo() const
        {
            return mCallInfoList;
        }

        void StartInternalCall( int argSizeByte )
        {
            InternalCallInfo* info = HeapNew( InternalCallInfo );
            info->argByteSize = argSizeByte;
            info->currentOffset = 0;
            info->nextArgIndex = 1;
            info->next = mCallInfoList;
            mCallInfoList = info;
        }

        void InternalCallDone()
        {
            Assert( mCallInfoList );
            Assert( mCallInfoList->currentOffset + MachPtr == mCallInfoList->argByteSize );
            InternalCallInfo* next = mCallInfoList->next;
            HeapDelete( mCallInfoList );
            mCallInfoList = next;
        }

        // Tells this register is holding the content located at the stackOffset
        void SetStackInfo( RegNum reg, int stackOffset )
        {
            Assert( !( 1 << reg & MaskUnavailableReg ) );
            mRegisterStackOffsetSaved[reg] = stackOffset;
            mAnyStackSaved |= 1 << reg;
        }

        // Call when register content is data dependent
        void InvalidateReg( RegNum reg )
        {
            mAnyStackSaved &= ~( 1 << reg );
        }

        void InvalidateAllVolatileReg()
        {
            mAnyStackSaved &= MaskNonVolatileReg;
        }

        void InvalidateAllReg()
        {
            mAnyStackSaved = 0;
        }

        // Call when stack value has changed
        void OverwriteStack( int stackOffset )
        {
            if( mAnyStackSaved )
            {
                // check all register with a stack offset saved
                int stackSavedReg = mAnyStackSaved;
                int reg = 0;
                while( stackSavedReg )
                {
                    // skip reg with no stack info
                    while( !(stackSavedReg & 1) )
                    {
                        stackSavedReg >>= 1;
                        ++reg;
                    }

                    // invalidate register with this stack location
                    if( mRegisterStackOffsetSaved[reg] == stackOffset )
                    {
                        InvalidateReg( RegNum( reg ) );
                    }

                    // next register
                    stackSavedReg >>= 1;
                    ++reg;
                }
            }
        }


        // Gets a register to use
        // registerRestriction : bit vector, 1 means the register cannot be chosen
        template<typename T> RegNum GetReg(const int registerRestriction = 0)
        {
            CompileAssert( sizeof(T) == 4 || sizeof(T) == 8 );
            const int mask = GetRegMask<T>() & ~registerRestriction;
            int stackSavedReg = mAnyStackSaved & mask;

            // No more register available
            if( stackSavedReg == mask )
            {
                RegNum reg = NextReg<T>(registerRestriction);
                Assert( !(1 << reg & registerRestriction) );
                return reg;
            }
            // making sure we don't choose the unavailable registers
            stackSavedReg |= MaskUnavailableReg|registerRestriction;

            int reg = GetFirstReg<T>();
            stackSavedReg >>= reg;
            // will always find a value under these conditions
            while( 1 )
            {
                // if the register hold no useful info, return it
                if( !( stackSavedReg & 1 ) )
                {
                     Assert( !(1 << reg & registerRestriction) );
                    return RegNum( reg );
                }
                stackSavedReg >>= 1;
                ++reg;
            }
        }

        // Gets a register to use
        // registerRestriction : bit vector, 1 means the register cannot be chosen
        template<> RegNum GetReg<float>(const int registerRestriction)
        {
            const int mask = GetRegMask<double>() & ~registerRestriction;
            int stackSavedReg = mAnyStackSaved & mask;

            // No more register available
            if (stackSavedReg == mask)
            {
                RegNum reg = NextReg<double>(registerRestriction);
                Assert(!(1 << reg & registerRestriction));
                return reg;
            }
            // making sure we don't choose the unavailable registers
            stackSavedReg |= MaskUnavailableReg | registerRestriction;

            int reg = GetFirstReg<double>();
            stackSavedReg >>= reg;
            // will always find a value under these conditions
            while (1)
            {
                // if the register hold no useful info, return it
                if (!(stackSavedReg & 1))
                {
                    Assert(!(1 << reg & registerRestriction));
                    return RegNum(reg);
                }
                stackSavedReg >>= 1;
                ++reg;
            }
        }

        template<> RegNum GetReg<AsmJsSIMDValue>(const int registerRestriction)
        {
            return GetReg<float>(registerRestriction);
        }

        // Search for a register already holding the value at this location
        template<typename T> bool FindRegWithStackOffset( RegNum& outReg, int stackOffset, int registerRestriction = 0 )
        {
            CompileAssert( sizeof(T) == 4 || sizeof(T) == 8 || sizeof(T) == 16);

            int stackSavedReg = mAnyStackSaved & GetRegMask<T>() & ~registerRestriction;
            if( stackSavedReg )
            {
                int reg = GetFirstReg<T>();
                stackSavedReg >>= reg;
                while( stackSavedReg )
                {
                    // skip reg with no stack info
                    while( !(stackSavedReg & 1) )
                    {
                        stackSavedReg >>= 1;
                        ++reg;
                    }

                    // invalidate register with this stack location
                    if( mRegisterStackOffsetSaved[reg] == stackOffset )
                    {
                        outReg = RegNum( reg );
                        return true;
                    }

                    // next register
                    stackSavedReg >>= 1;
                    ++reg;
                }
            }
            return false;
        }
        void SetBaseOffset(int baseOffSet)
        {
            // We subtract with the baseoffset as the layout of the stack has changed from the interpreter
            // Assume Stack is growing downwards
            // Interpreter - Stack is above EBP and offsets are positive
            // TJ - Stack is below EBP and offsets are negative
            mBaseOffset = baseOffSet;
            mModuleSlotOffset = AsmJsJitTemplate::Globals::ModuleSlotOffset - mBaseOffset;
            mModuleEnvOffset = AsmJsJitTemplate::Globals::ModuleEnvOffset - mBaseOffset;
            mArrayBufferOffSet = AsmJsJitTemplate::Globals::ArrayBufferOffset - mBaseOffset;
            mArraySizeOffset = AsmJsJitTemplate::Globals::ArraySizeOffset - mBaseOffset;
            mScriptContextOffSet = AsmJsJitTemplate::Globals::ScriptContextOffset - mBaseOffset;
        }
        int GetBaseOffSet()
        {
            return mBaseOffset;
        }
        int GetModuleSlotOffset()
        {
            return mModuleSlotOffset;
        }
        int GetModuleEnvOffset()
        {
            return mModuleEnvOffset;
        }
        int GetArrayBufferOffset()
        {
            return mArrayBufferOffSet;
        }
        int GetArraySizeOffset()
        {
            return mArraySizeOffset;
        }
        int GetScriptContextOffset()
        {
            return mScriptContextOffSet;
        }
        const int GetCalleSavedRegSizeInByte()
        {
            //EBX,ESI,EDI
            return 3 * sizeof(void*);
        }
        const int GetEBPOffsetCorrection()
        {
            //We computed the offset in BCG adjusting for push ebp and ret address
            return 2 * sizeof(void*);
        }
    };
    template<> RegNum X86TemplateData::GetNextRegister<int>() { return mNext32BitsReg; }
    template<> RegNum X86TemplateData::GetNextRegister<double>() { return mNext64BitsReg; }
    template<> void X86TemplateData::SetNextRegister<int>(RegNum reg) { mNext32BitsReg = reg; }
    template<> void X86TemplateData::SetNextRegister<double>(RegNum reg) { mNext64BitsReg = reg; }



    struct ReturnContent
    {
        union
        {
            int intVal;
            double doubleVal;
        };
        template<typename T> T GetReturnVal()const;
#if DBG_DUMP
        template<typename T> void Print()const;
#endif
    };
    template<> int ReturnContent::GetReturnVal<int>()const
    {
        return intVal;
    }
    template<> float ReturnContent::GetReturnVal<float>()const
    {
        return (float)doubleVal;
    }
    template<> double ReturnContent::GetReturnVal<double>()const
    {
        return doubleVal;
    }
#if DBG_DUMP
    template<> void ReturnContent::Print<int>()const
    {
        Output::Print( L" = %d", intVal );
    }
    template<> void ReturnContent::Print<double>()const
    {
        Output::Print( L" = %.4f", doubleVal );
    }
    template<> void ReturnContent::Print<float>()const
    {
        Output::Print( L" = %.4f", doubleVal );
    }
    int AsmJsCallDepth = 0;
#endif

    uint CallLoopBody(JavascriptMethod address, ScriptFunction* function, Var frameAddress)
    {
        void *savedEsp = NULL;
        __asm
        {
            // Save ESP
            mov savedEsp, esp
            // Add an extra 4-bytes to the stack since we'll be pushing 3 arguments
            push eax
        }
        uint newOffset = (uint)address(function, CallInfo(CallFlags_InternalFrame, 1), frameAddress);

        _asm
        {
            // Restore ESP
            mov esp, savedEsp
        }
        return newOffset;
    }

    uint DoLoopBodyStart(Js::ScriptFunction* function,Var ebpPtr,uint32 loopNumber)
    {

        FunctionBody* fn = function->GetFunctionBody();
        Assert(loopNumber < fn->GetLoopCount());

        Js::LoopHeader *loopHeader = fn->GetLoopHeader(loopNumber);
        Js::LoopEntryPointInfo * entryPointInfo = loopHeader->GetCurrentEntryPointInfo();
        ScriptContext* scriptContext = fn->GetScriptContext();
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
            Assert(entryPointInfo->address);
            uint newOffset = CallLoopBody((JavascriptMethod)entryPointInfo->address, function, ebpPtr);
            ptrdiff_t value = NULL;
            fn->GetAsmJsFunctionInfo()->mbyteCodeTJMap->TryGetValue(newOffset, &value);
            Assert(value != NULL); // value cannot be null
            BYTE* newAddress = fn->GetAsmJsFunctionInfo()->mTJBeginAddress + value;
            Assert(newAddress);
            return (uint)newAddress;
        }
        // interpreCount for loopHeader is incremented before calling DoLoopBody
        const uint loopInterpretCount = fn->GetLoopInterpretCount(loopHeader);
        if (loopHeader->interpretCount > loopInterpretCount)
        {
            if (!fn->DoJITLoopBody())
            {
                return 0;
            }

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
                entryPointInfo->SetIsAsmJSFunction(true);
                entryPointInfo->SetIsTJMode(true);
                GenerateLoopBody(scriptContext->GetNativeCodeGenerator(), fn, loopHeader, entryPointInfo, fn->GetLocalsCount(), &ebpPtr);
                //reset InterpretCount
                loopHeader->interpretCount = 0;
            }
        }

        return 0;
    }


    // Function memory allocation should be done the same way as
    // void InterpreterStackFrame::AlignMemoryForAsmJs()  (InterpreterStackFrame.cpp)
    // update any changes there
    void AsmJsCommonEntryPoint(Js::ScriptFunction* func, void* savedEbpPtr)
    {
        int savedEbp = (int)savedEbpPtr;
        FunctionBody* body = func->GetFunctionBody();
        Js::FunctionEntryPointInfo * entryPointInfo = body->GetDefaultFunctionEntryPointInfo();
        //CodeGenDone status is set by TJ
        if ((entryPointInfo->IsNotScheduled() || entryPointInfo->IsCodeGenDone()) && !PHASE_OFF(BackEndPhase, body) && !PHASE_OFF(FullJitPhase, body))
        {
            if (entryPointInfo->callsCount < 255)
            {
                ++entryPointInfo->callsCount;
            }
            const int minTemplatizedJitRunCount = (int)CONFIG_FLAG(MinTemplatizedJitRunCount);
            if ((entryPointInfo->callsCount >= minTemplatizedJitRunCount || body->IsHotAsmJsLoop()))
            {
                if (PHASE_TRACE1(AsmjsEntryPointInfoPhase))
                {
                    Output::Print(L"Scheduling %s For Full JIT at callcount:%d\n", body->GetDisplayName(), entryPointInfo->callsCount);
                }
                GenerateFunction(body->GetScriptContext()->GetNativeCodeGenerator(), body, func);
            }
        }
        void* constTable = body->GetConstTable();
        constTable = (void*)(((Var*)constTable)+AsmJsFunctionMemory::RequiredVarConstants-1);
        AsmJsFunctionInfo* asmInfo = body->GetAsmJsFunctionInfo();

        const int intConstCount = asmInfo->GetIntConstCount();
        const int doubleConstCount = asmInfo->GetDoubleConstCount();
        const int floatConstCount = asmInfo->GetFloatConstCount();
        const int simdConstCount = asmInfo->GetSimdConstCount();

        // Offset of doubles from (double*)m_localSlot
        const int intOffsets = asmInfo->GetIntByteOffset() / sizeof(int);
        const int doubleOffsets = asmInfo->GetDoubleByteOffset() / sizeof(double);
        const int floatOffset = asmInfo->GetFloatByteOffset() / sizeof(float);
        const int simdByteOffset = asmInfo->GetSimdByteOffset(); // in bytes

        // (2*sizeof(Var)) -- push ebp and ret address
        //sizeof(ScriptFunction*) -- this is the argument passed to the TJ function
        int argoffset = (2*sizeof(Var)) + sizeof(ScriptFunction*);
        argoffset = argoffset +  savedEbp ;
        // initialize argument location
        int* intArg;
        double* doubleArg;
        float* floatArg;
        AsmJsSIMDValue* simdArg;

        // setup stack memory
        FrameDisplay* frame = func->GetEnvironment();
        Var moduleEnv = frame->GetItem(0);
        Var* arrayBufferVar = (Var*)frame->GetItem(0) + AsmJsModuleMemory::MemoryTableBeginOffset;
        int arraySize = 0;
        BYTE* arrayPtr = nullptr;
        if (*arrayBufferVar && JavascriptArrayBuffer::Is(*arrayBufferVar))
        {
            JavascriptArrayBuffer* arrayBuffer = *(JavascriptArrayBuffer**)arrayBufferVar;
            arrayPtr = arrayBuffer->GetBuffer();
            arraySize = arrayBuffer->GetByteLength();
        }
        Var* m_localSlots;
        int* m_localIntSlots;
        double* m_localDoubleSlots;
        float* m_localFloatSlots;
        AsmJsSIMDValue* m_localSimdSlots;

#if DBG_DUMP
        const bool tracingFunc = PHASE_TRACE( AsmjsFunctionEntryPhase, body );
        if( tracingFunc )
        {
            if( AsmJsCallDepth )
            {
                Output::Print( L"%*c", AsmJsCallDepth,' ');
            }
            Output::Print( L"Executing function %s(", body->GetDisplayName());
            ++AsmJsCallDepth;
        }
#endif
        // two args i.e. (ScriptFunction and savedEbp) + 2* (void*) i.e.(ebp + return address)
        int beginSlotOffset = sizeof(ScriptFunction*) + sizeof(void*) + 2 * sizeof(void*);
        __asm
        {
            mov  eax, ebp
            add  eax, beginSlotOffset
            mov m_localSlots,eax
        };

        {
            const ArgSlot argCount = asmInfo->GetArgCount();
            m_localSlots[AsmJsFunctionMemory::ModuleEnvRegister] = moduleEnv;
            m_localSlots[AsmJsFunctionMemory::ArrayBufferRegister] = (Var)arrayPtr;
            m_localSlots[AsmJsFunctionMemory::ArraySizeRegister] = (Var)arraySize;
            m_localSlots[AsmJsFunctionMemory::ScriptContextBufferRegister] = body->GetScriptContext();
            m_localIntSlots = ((int*)m_localSlots) + intOffsets;
            memcpy_s(m_localIntSlots, intConstCount*sizeof(int), constTable, intConstCount*sizeof(int));
            constTable = (void*)(((int*)constTable) + intConstCount);

            m_localFloatSlots = ((float*)m_localSlots) + floatOffset;
            memcpy_s(m_localFloatSlots, floatConstCount*sizeof(float), constTable, floatConstCount*sizeof(float));
            constTable = (void*)(((float*)constTable) + floatConstCount);

            m_localDoubleSlots = ((double*)m_localSlots) + doubleOffsets;
            memcpy_s(m_localDoubleSlots, doubleConstCount*sizeof(double), constTable, doubleConstCount*sizeof(double));

            if (func->GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {
                // Copy SIMD constants to TJ stack frame. No data alignment.
                constTable = (void*)(((double*)constTable) + doubleConstCount);
                m_localSimdSlots = (AsmJsSIMDValue*)((char*)m_localSlots + simdByteOffset);
                memcpy_s(m_localSimdSlots, simdConstCount*sizeof(AsmJsSIMDValue), constTable, simdConstCount*sizeof(AsmJsSIMDValue));
            }

            intArg = m_localIntSlots + intConstCount;
            doubleArg = m_localDoubleSlots + doubleConstCount;
            floatArg = m_localFloatSlots + floatConstCount;
            simdArg = m_localSimdSlots + simdConstCount;

            for(ArgSlot i = 0; i < argCount; i++ )
            {
                if(asmInfo->GetArgType(i).isInt())
                {
                    __asm
                    {
                        mov eax, argoffset
                        mov eax, [eax]
                        mov ecx, intArg
                        mov [ecx], eax
                    };
#if DBG_DUMP
                    if( tracingFunc )
                    {
                        Output::Print( L" %d%c", *intArg, i+1 < argCount ? ',':' ');
                    }
#endif
                    ++intArg;
                    argoffset += sizeof( int );
                }
                else if (asmInfo->GetArgType(i).isFloat())
                {
                    __asm
                    {
                        mov eax, argoffset
                        movss xmm0, [eax]
                        mov eax, floatArg
                        movss[eax], xmm0
                    };
#if DBG_DUMP
                    if (tracingFunc)
                    {
                        Output::Print(L" %.4f%c", *floatArg, i + 1 < argCount ? ',' : ' ');
                    }
#endif
                    ++floatArg;
                    argoffset += sizeof(float);
                }
                else if (asmInfo->GetArgType(i).isDouble())
                {
                    __asm
                    {
                        mov eax, argoffset
                        movsd xmm0, [eax]
                        mov eax, doubleArg
                        movsd [eax], xmm0
                    };
#if DBG_DUMP
                    if( tracingFunc )
                    {
                        Output::Print( L" %.4f%c", *doubleArg, i+1 < argCount ? ',':' ');
                    }
#endif
                    ++doubleArg;
                    argoffset += sizeof( double );
                }
                else if (asmInfo->GetArgType(i).isSIMD())
                {
                    __asm
                    {
                        mov eax, argoffset
                        movups xmm0, [eax]
                        mov eax, simdArg
                        movups[eax], xmm0
                    };

#if DBG_DUMP
                    if (tracingFunc)
                    {
                        switch (asmInfo->GetArgType(i).which())
                        {
                        case AsmJsType::Int32x4:
                            Output::Print(L" I4(%d, %d, %d, %d)", \
                                simdArg->i32[SIMD_X], simdArg->i32[SIMD_Y], simdArg->i32[SIMD_Z], simdArg->i32[SIMD_W]);
                            break;
                        case AsmJsType::Float32x4:
                            Output::Print(L" F4(%.4f, %.4f, %.4f, %.4f)", \
                                simdArg->f32[SIMD_X], simdArg->f32[SIMD_Y], simdArg->f32[SIMD_Z], simdArg->f32[SIMD_W]);
                            break;
                        case AsmJsType::Float64x2:
                            Output::Print(L" D2(%.4f, %.4f)%c", \
                                simdArg->f64[SIMD_X], simdArg->f64[SIMD_Y]);
                            break;
                        }
                        Output::Print(L"%c", i + 1 < argCount ? ',' : ' ');
                    }
#endif
                    ++simdArg;
                    argoffset += sizeof(AsmJsSIMDValue);
                }
            }
        }
#if DBG_DUMP
        if( tracingFunc )
        {
            Output::Print( L"){\n");
        }
#endif
    }
#if DBG_DUMP
    void AsmJSCommonCallHelper(Js::ScriptFunction* func)
    {
        FunctionBody* body = func->GetFunctionBody();
        AsmJsFunctionInfo* asmInfo = body->GetAsmJsFunctionInfo();
        const bool tracingFunc = PHASE_TRACE(AsmjsFunctionEntryPhase, body);
        if (tracingFunc)
        {
            --AsmJsCallDepth;
            if (AsmJsCallDepth)
            {
                Output::Print(L"%*c}", AsmJsCallDepth, ' ');
            }
            else
            {
                Output::Print(L"}");
            }
            if (asmInfo->GetReturnType() != AsmJsRetType::Void)
            {
                //returnContent.Print<T>();
            }
            Output::Print(L";\n");
        }
    }
#endif
    Var ExternalCallHelper( JavascriptFunction* function, int nbArgs, Var* paramsAddr )
    {
        int flags = CallFlags_Value;
        Arguments args(CallInfo((CallFlags)flags, (ushort)nbArgs), paramsAddr);
        return JavascriptFunction::CallFunction<true>(function, function->GetEntryPoint(), args);
    }

    namespace AsmJsJitTemplate
    {
        const int Globals::ModuleSlotOffset = (AsmJsFunctionMemory::ModuleSlotRegister + Globals::StackVarCount)*sizeof(Var);
        const int Globals::ModuleEnvOffset = (AsmJsFunctionMemory::ModuleEnvRegister + Globals::StackVarCount)*sizeof(Var);
        const int Globals::ArrayBufferOffset = (AsmJsFunctionMemory::ArrayBufferRegister + Globals::StackVarCount)*sizeof(Var);
        const int Globals::ArraySizeOffset = (AsmJsFunctionMemory::ArraySizeRegister + Globals::StackVarCount)*sizeof(Var);
        const int Globals::ScriptContextOffset = (AsmJsFunctionMemory::ScriptContextBufferRegister + Globals::StackVarCount)*sizeof(Var);
#if DBG_DUMP
        FunctionBody* Globals::CurrentEncodingFunction = nullptr;
#endif

        // Jump relocation : fix the jump offset for a later point in the same template
        struct JumpRelocation
        {
            // buffer : where the instruction will be encoded
            // size : address of a variable tracking the instructions size encoded after the jump
            JumpRelocation( BYTE* buffer, int* size )
            {
#if DBG
                mRelocDone = false;
                mEncodingImmSize = -1;
#endif
                Init( buffer, size );
            }

            // Default Constructor, must call Init before using
            JumpRelocation()
            {
#if DBG
                mRelocDone = false;
                mEncodingImmSize = -1;
#endif
            }

#if DBG
            ~JumpRelocation()
            {
                // Make sure the relocation is done when destruction the object
                Assert( mRelocDone );
            }
#endif

            void Init( BYTE* buffer, int* size )
            {
#if DBG
                // this cannot be called twice
                Assert( mEncodingImmSize == -1 );
#endif
                mBuffer = buffer;
                mSize = size;
                mInitialSize = *mSize;
            }

            // to be called right after encoding a jump
            void JumpEncoded( const EncodingInfo& info )
            {
#if DBG
                // this cannot be called twice
                Assert( mEncodingImmSize == -1 );
#endif
                const int curSize = *mSize;
                // move the buffer to the point where we need to fix the value
                mBuffer += curSize - mInitialSize - info.immSize;
                mInitialSize = curSize;
#if DBG
                mEncodingImmSize = info.immSize;
#endif
            }

            // use when only 1 Byte was allocated
            template<typename OffsetType>
            void ApplyReloc()
            {
#if DBG
                Assert( mEncodingImmSize == sizeof(OffsetType) );
                mRelocDone = true;
#endif
                const int relocSize = *mSize - mInitialSize;

                // if we encoded only 1 byte, make sure it fits
                Assert( sizeof(OffsetType) != 1 || FitsInByte( relocSize ) );
                *(OffsetType*)mBuffer = (OffsetType)relocSize;
            }

#if DBG
            bool mRelocDone;
            int mEncodingImmSize;
#endif
            BYTE* mBuffer;
            int* mSize;
            int mInitialSize;
        };

#define GetTemplateData(context) ((X86TemplateData*)context->GetTemplateData())

        // Initialize template data
        void* InitTemplateData()
        {
            return HeapNew( X86TemplateData );
        }

        // Free template data for architecture specific
        void FreeTemplateData( void* userData )
        {
            HeapDelete( (X86TemplateData*)userData );
        }

        // Typedef to map a type to an instruction
        template<typename InstructionSize> struct InstructionBySize;
        template<> struct InstructionBySize < int > { typedef MOV MoveInstruction; };
        template<> struct InstructionBySize < double > { typedef MOVSD MoveInstruction; };
        template<> struct InstructionBySize < float > { typedef MOVSS MoveInstruction; };
        template<> struct InstructionBySize < AsmJsSIMDValue > { typedef MOVUPS MoveInstruction; };
        namespace EncodingHelpers
        {
            // put the value on the stack into a register
            template<typename RegisterSize>
            RegNum GetStackReg( BYTE*& buffer, X86TemplateData* templateData, int varOffset, int &size, const int registerRestriction = 0 )
            {
                RegNum reg;
                if( !templateData->FindRegWithStackOffset<RegisterSize>( reg, varOffset, registerRestriction ) )
                {
                    reg = templateData->GetReg<RegisterSize>( registerRestriction );
                    size += InstructionBySize<RegisterSize>::MoveInstruction::EncodeInstruction<RegisterSize>( buffer, InstrParamsRegAddr( reg, RegEBP, varOffset ) );
                    templateData->SetStackInfo( reg, varOffset );
                }
                return reg;
            }

            // put the value of a register on the stack
            template<typename RegisterSize>
            int SetStackReg( BYTE*& buffer, X86TemplateData* templateData, int targetOffset, RegNum reg )
            {
                CompileAssert(sizeof(RegisterSize) == 4 || sizeof(RegisterSize) == 8);
                templateData->OverwriteStack( targetOffset );
                templateData->SetStackInfo( reg, targetOffset );
                return InstructionBySize<RegisterSize>::MoveInstruction::EncodeInstruction<RegisterSize>( buffer, InstrParamsAddrReg( RegEBP, targetOffset, reg ) );
            }
            template<typename LaneType=int>
            int SIMDSetStackReg(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, RegNum reg)
            {
                CompileAssert(sizeof(LaneType) == 4 || sizeof(LaneType) == 8);
                AssertMsg(((1<<reg) & GetRegMask<AsmJsSIMDValue>()), "Expecting XMM reg.");

                // On a stack spill, we need to invalidate any registers holding lane values.
                int laneOffset = 0;
                while (laneOffset < sizeof(AsmJsSIMDValue))
                {
                    templateData->OverwriteStack(targetOffset + laneOffset);
                    laneOffset += sizeof(LaneType);
                }
                templateData->SetStackInfo(reg, targetOffset);
                return InstructionBySize<AsmJsSIMDValue>::MoveInstruction::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsAddrReg(RegEBP, targetOffset, reg));
            }
            /*
                Simply copy data from memory to memory.
                TODO: Optimize to initialize in XMM reg and then store to mem.
            */
            template<typename LaneType>
            int SIMDInitFromPrimitives(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, int srcOffset1, int srcOffset2, int srcOffset3 = 0, int srcOffset4 = 0)
            {
                CompileAssert(sizeof(LaneType) == 4 || sizeof(LaneType) == 8);

                int size = 0;
                int laneOffset = 0;
                RegNum reg;

                targetOffset -= templateData->GetBaseOffSet();
                srcOffset1 -= templateData->GetBaseOffSet();
                srcOffset2 -= templateData->GetBaseOffSet();
                srcOffset3 -= templateData->GetBaseOffSet();
                srcOffset4 -= templateData->GetBaseOffSet();

                // Since we overwrite all lanes, any register holding any lane value is invalidated.
                reg = EncodingHelpers::GetStackReg<LaneType>(buffer, templateData, srcOffset1, size);
                size += EncodingHelpers::SetStackReg<LaneType>(buffer, templateData, targetOffset + laneOffset, reg);
                templateData->InvalidateReg(reg);
                laneOffset += sizeof(LaneType);

                reg = EncodingHelpers::GetStackReg<LaneType>(buffer, templateData, srcOffset2, size);
                size += EncodingHelpers::SetStackReg<LaneType>(buffer, templateData, targetOffset + laneOffset, reg);
                templateData->InvalidateReg(reg);
                laneOffset += sizeof(LaneType);
                if (laneOffset < sizeof(AsmJsSIMDValue))
                {

                    reg = EncodingHelpers::GetStackReg<LaneType>(buffer, templateData, srcOffset3, size);
                    size += EncodingHelpers::SetStackReg<LaneType>(buffer, templateData, targetOffset + laneOffset, reg);
                    templateData->InvalidateReg(reg);
                    laneOffset += sizeof(LaneType);

                    reg = EncodingHelpers::GetStackReg<LaneType>(buffer, templateData, srcOffset4, size);
                    size += EncodingHelpers::SetStackReg<LaneType>(buffer, templateData, targetOffset + laneOffset, reg);
                    templateData->InvalidateReg(reg);
                }
                return size;
            }

            // Since SIMD data is unaligned, we cannot support "OP reg, [mem]" operations.
            template <typename Operation, typename LaneType=int>
            int SIMDUnaryOperation(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, int srcOffset, int registerRestriction = 0)
            {
                int size = 0;
                RegNum dstReg, srcReg;

                targetOffset -= templateData->GetBaseOffSet();
                srcOffset -= templateData->GetBaseOffSet();

                // MOVUPS
                srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset, size);
                // Get a new reg for dst, and keep src reg alive
                dstReg = templateData->GetReg<AsmJsSIMDValue>(1 << srcReg);
                // OP reg1, reg2
                size += Operation::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, srcReg));
                // MOVUPS
                size += EncodingHelpers::SIMDSetStackReg<LaneType>(buffer, templateData, targetOffset, dstReg);
                return size;
            }

            template <typename Operation, typename LaneType = int>
            int SIMDBinaryOperation(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, int srcOffset1, int srcOffset2)
            {
                int size = 0;
                RegNum srcReg1, srcReg2, dstReg;

                targetOffset -= templateData->GetBaseOffSet();
                srcOffset1 -= templateData->GetBaseOffSet();
                srcOffset2 -= templateData->GetBaseOffSet();

                // MOVUPS srcReg1, [srcOffset1]
                srcReg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset1, size);
                // MOVUPS srcReg2, [srcOffset2]
                srcReg2 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset2, size);
                // keep src regs alive
                // MOVAPS dstReg, srcReg1
                dstReg = templateData->GetReg<AsmJsSIMDValue>((1 << srcReg1) | (1 << srcReg2));
                size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, srcReg1));
                // OP dstReg, srcReg2
               size += Operation::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, srcReg2));
                // MOVUPS
                size += EncodingHelpers::SIMDSetStackReg<LaneType>(buffer, templateData, targetOffset, dstReg);
                return size;
            }

            // for CMP and Shuffle operations
            template <typename Operation, typename LaneType = int>
            int SIMDBinaryOperation(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, int srcOffset1, int srcOffset2, byte imm8)
            {
                int size = 0;
                RegNum srcReg1, srcReg2, dstReg;

                targetOffset -= templateData->GetBaseOffSet();
                srcOffset1 -= templateData->GetBaseOffSet();
                srcOffset2 -= templateData->GetBaseOffSet();

                // MOVUPS srcReg1, [srcOffset1]
                srcReg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset1, size);
                // MOVUPS srcReg2, [srcOffset2]
                srcReg2 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset2, size);
                // keep src regs alive
                // MOVAPS dstReg, srcReg1
                dstReg = templateData->GetReg<AsmJsSIMDValue>((1 << srcReg1) | (1 << srcReg2));
                size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, srcReg1));
                // OP dstReg, srcReg2, imm8
                size += Operation::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2RegImm<byte>(dstReg, srcReg2, imm8));
                // MOVUPS
                size += EncodingHelpers::SIMDSetStackReg<LaneType>(buffer, templateData, targetOffset, dstReg);
                return size;
            }

            template <typename Operation, typename LaneType>
            RegNum SIMDRcpOperation(BYTE*& buffer, X86TemplateData* templateData, RegNum srcReg, void *ones, int &size)
            {
                RegNum reg;
                // MOVAPS reg, [mask]
                reg = templateData->GetReg<AsmJsSIMDValue>(1 << srcReg);
                size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg, ones));
                // OP reg, srcReg
                size += Operation::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(reg, srcReg));
                return reg;
            }

            template <typename Operation, typename LaneType>
            int SIMDLdLaneOperation(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, int srcOffset, const int index, const bool reUseResult = true)
            {
                CompileAssert(sizeof(LaneType) == 4 || sizeof(LaneType) == 8);

                targetOffset -= templateData->GetBaseOffSet();
                srcOffset -= templateData->GetBaseOffSet();

                RegNum srcReg, tmpReg;
                int size = 0;

                // MOVUPS
                srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset, size);

                // MOVAPS tmpReg, srcReg
                tmpReg = templateData->GetReg<AsmJsSIMDValue>((1 << srcReg));
                size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg, srcReg));
                // PSRLDQ tmpREg, (index * sizeof(lane))
                size += PSRLDQ::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegImm<byte>(tmpReg, (byte)(sizeof(LaneType)* index)));

                templateData->OverwriteStack(targetOffset);
                if (reUseResult)
                {
                    // can re-use register for floats and doubles only.
                    templateData->SetStackInfo(tmpReg, targetOffset);
                }
                size += Operation::EncodeInstruction<LaneType>(buffer, InstrParamsAddrReg(RegEBP, targetOffset, tmpReg));
                return size;
            }

            template <typename LaneType, typename ShufOperation = SHUFPS>
            int SIMDSetLaneOperation(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, int srcOffset, int valOffset, const int laneIndex)
            {
                CompileAssert(sizeof(LaneType) == 4);

                targetOffset -= templateData->GetBaseOffSet();
                srcOffset -= templateData->GetBaseOffSet();
                valOffset -= templateData->GetBaseOffSet();
                int size = 0;
                RegNum srcReg, tmpReg, valReg;
                // load regs
                // MOVUPS srcReg, [src]
                srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset, size);

                // keep src alive
                // MOVAPS tmpReg, srcReg
                tmpReg = templateData->GetReg<AsmJsSIMDValue>(1 << srcReg);
                size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg, srcReg));

                // MOVSS valReg, [val] ; valReg is XMM
                valReg = EncodingHelpers::GetStackReg<float>(buffer, templateData, valOffset, size, (1 << srcReg) | (1 << tmpReg));
                if (laneIndex == 0)
                {
                    // MOVSS tmpReg, valReg
                    // Note: we use MOVSS for both F4 and I4. MOVD sets upper bits to zero, MOVSS leaves them unmodified.
                    size += MOVSS::EncodeInstruction<LaneType>(buffer, InstrParams2Reg(tmpReg, valReg));
                }
                else if (laneIndex == 1 || laneIndex == 3)
                {
                    // shuf, mov, shuf

                    byte shufMask;

                    shufMask = 0xE4; // 11 10 01 00
                    shufMask |= laneIndex; // 11 10 01 id
                    shufMask &= ~(0x03 << (laneIndex << 1)); // set 2 bits corresponding to lane index to 00

                    // shuf tempReg, tmpReg, shufMask
                    size += ShufOperation::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2RegImm<byte>(tmpReg, tmpReg, shufMask));
                    // MOVSS tmpReg, valReg
                    size += MOVSS::EncodeInstruction<LaneType>(buffer, InstrParams2Reg(tmpReg, valReg));
                    // shuf tempReg, tmpReg, shufMask
                    size += ShufOperation::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2RegImm<byte>(tmpReg, tmpReg, shufMask));
                }
                else
                {
                    Assert(laneIndex == 2);
                    RegNum tmpReg2 = templateData->GetReg<AsmJsSIMDValue>((1 << srcReg) | (1 << tmpReg));
                    // MOVHLPS tmpReg2, tmpReg
                    size += MOVHLPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg2, tmpReg));
                    // MOVSS tmpReg2, valReg
                    size += MOVSS::EncodeInstruction<LaneType>(buffer, InstrParams2Reg(tmpReg2, valReg));
                    // MOVHLPS tmpReg, tmpReg2
                    size += MOVLHPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg, tmpReg2));
                }

                size += EncodingHelpers::SIMDSetStackReg<LaneType>(buffer, templateData, targetOffset, tmpReg);
                return size;
            }

            template <>
            int SIMDSetLaneOperation<double>(BYTE*& buffer, X86TemplateData* templateData, int targetOffset, int srcOffset, int valOffset, const int laneIndex)
            {
                targetOffset -= templateData->GetBaseOffSet();
                srcOffset -= templateData->GetBaseOffSet();
                valOffset -= templateData->GetBaseOffSet();
                int size = 0;
                RegNum srcReg, tmpReg, valReg;
                // load regs
                // MOVUPS srcReg, [src]
                srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset, size);

                // keep src alive
                // MOVAPS tmpReg, srcReg
                tmpReg = templateData->GetReg<AsmJsSIMDValue>(1 << srcReg);
                size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg, srcReg));
                if (laneIndex == 0)
                {
                    // We have to load val to reg. MOVSD reg, [val] will zero upper bits.
                    // MOVSD valReg, [val]
                    valReg = EncodingHelpers::GetStackReg<double>(buffer, templateData, valOffset, size, (1 << srcReg) | (1 << tmpReg));
                    // MOVSD tmpReg, value
                    size += MOVSD::EncodeInstruction<double>(buffer, InstrParams2Reg(tmpReg, valReg));
                }
                else
                {
                    Assert(laneIndex == 1);
                    // MOVHPD tmpReg, [val]
                    size += MOVHPD::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegAddr(tmpReg, RegEBP, valOffset));
                }
                // MOVUPS
                size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffset, tmpReg);
                return size;
            }

            // Retrieve the value of the array buffer and put it in a register to use
            RegNum GetArrayBufferRegister( BYTE*& buffer, TemplateContext context, int &size, const int registerRestriction = 0 )
            {
                return ArrayBufferReg;
            }

            // Retrieve the value of the module environment and put it in a register to use
            RegNum GetModuleEnvironmentRegister( BYTE*& buffer, TemplateContext context, int &size, const int registerRestriction = 0 )
            {
                return ModuleEnvReg;
            }

            // Retrieve the value of the script context and put it in a register to use
            RegNum GetScriptContextRegister( BYTE*& buffer, TemplateContext context, int &size, const int registerRestriction = 0 )
            {
                X86TemplateData* templateData = GetTemplateData(context);
                return GetStackReg<int>(buffer, GetTemplateData(context), templateData->GetScriptContextOffset(), size, registerRestriction);
            }

            // Encode a Compare instruction between a register and the array length : format   cmp length, reg
            int CompareRegisterToArrayLength( BYTE*& buffer, TemplateContext context, RegNum reg, const int registerRestriction = 0 )
            {
                X86TemplateData* templateData = GetTemplateData(context);
                return CMP::EncodeInstruction<int>(buffer, InstrParamsAddrReg(RegEBP, templateData->GetArraySizeOffset(), reg));
            }

            // Encode a Compare instruction between an immutable value and the array length : format   cmp length, imm
            template<typename T>
            int CompareImmutableToArrayLength( BYTE*& buffer, TemplateContext context, T imm, const int registerRestriction = 0 )
            {
                X86TemplateData* templateData = GetTemplateData(context);
                return CMP::EncodeInstruction<int>(buffer, InstrParamsAddrImm<T>(RegEBP, templateData->GetArraySizeOffset(), imm));
            }

            // Encodes a short(1 Byte offset) jump instruction
            template<typename JCC>
            void EncodeShortJump( BYTE*& buffer, JumpRelocation& reloc, int* size )
            {
                Assert( size != nullptr );
                reloc.Init( buffer, size );
                EncodingInfo info;
                *size += JCC::EncodeInstruction<int8>( buffer, InstrParamsImm<int8>( 0 ), &info );
                reloc.JumpEncoded( info );
            }

            template<typename Operation, typename OperationSize>
            int CommutativeOperation( TemplateContext context, BYTE*& buffer, int leftOffset, int rightOffset, int* targetOffset = nullptr, RegNum* outReg = nullptr, int registerRestriction = 0 )
            {

                X86TemplateData* templateData = GetTemplateData( context );
                leftOffset -= templateData->GetBaseOffSet();
                rightOffset -= templateData->GetBaseOffSet();
                *targetOffset -= templateData->GetBaseOffSet();

                RegNum reg1, reg2;
                RegNum resultReg = RegNOREG;
                const int reg1Found = templateData->FindRegWithStackOffset<OperationSize>( reg1, leftOffset, registerRestriction );
                const int reg2Found = templateData->FindRegWithStackOffset<OperationSize>( reg2, rightOffset, registerRestriction );
                int size = 0;
                switch( reg1Found & ( reg2Found << 1 ) )
                {
                case 0: // none found
                    reg1 = templateData->GetReg<OperationSize>( registerRestriction );
                    size += InstructionBySize<OperationSize>::MoveInstruction::EncodeInstruction<OperationSize>(buffer, InstrParamsRegAddr(reg1, RegEBP, leftOffset));
                    if( leftOffset == rightOffset )
                    {
                        size += Operation::EncodeInstruction<OperationSize>( buffer, InstrParams2Reg( reg1, reg1 ) );
                    }
                    else
                    {
                        size += Operation::EncodeInstruction<OperationSize>(buffer, InstrParamsRegAddr(reg1, RegEBP, rightOffset));
                    }
                    resultReg = reg1;
                    break;
                case 1: // found 1 and not 2
                    size += Operation::EncodeInstruction<OperationSize>(buffer, InstrParamsRegAddr(reg1, RegEBP, rightOffset));
                    resultReg = reg1;
                    break;
                case 2: // found 2 and not 1
                    size += Operation::EncodeInstruction<OperationSize>(buffer, InstrParamsRegAddr(reg2, RegEBP, leftOffset));
                    resultReg = reg2;
                    break;
                case 3: // found both
                    size += Operation::EncodeInstruction<OperationSize>( buffer, InstrParams2Reg( reg1, reg2 ) );
                    resultReg = reg1;
                    break;
                default:
                    Assume(UNREACHED);
                }

                if( Operation::Flags & AffectOp1 )
                {
                    templateData->InvalidateReg( resultReg );
                }

                if( targetOffset )
                {
                    const int offset = *targetOffset;
                    size += InstructionBySize<OperationSize>::MoveInstruction::EncodeInstruction<OperationSize>(buffer, InstrParamsAddrReg(RegEBP, offset, resultReg));
                    templateData->OverwriteStack( offset );
                    templateData->SetStackInfo( resultReg, offset );
                }

                if( outReg )
                {
                    *outReg = resultReg;
                }

                return size;
            }

            template<typename Operation, typename OperationSize>
            int NonCommutativeOperation( TemplateContext context, BYTE*& buffer, int leftOffset, int rightOffset, int* targetOffset = nullptr, RegNum* outReg = nullptr, int registerRestriction = 0 )
            {
                X86TemplateData* templateData = GetTemplateData( context );
                leftOffset -= templateData->GetBaseOffSet();
                rightOffset -= templateData->GetBaseOffSet();

                RegNum reg1, reg2;
                const int reg1Found = templateData->FindRegWithStackOffset<OperationSize>( reg1, leftOffset, registerRestriction );
                int size = 0;
                if( !reg1Found )
                {
                    reg1 = templateData->GetReg<OperationSize>( registerRestriction );
                    size += InstructionBySize<OperationSize>::MoveInstruction::EncodeInstruction<OperationSize>( buffer, InstrParamsRegAddr( reg1, RegEBP, leftOffset ) );
                    templateData->SetStackInfo( reg1, leftOffset );
                }

                const int reg2Found = templateData->FindRegWithStackOffset<OperationSize>( reg2, rightOffset, registerRestriction );
                if( reg2Found )
                {
                    size += Operation::EncodeInstruction<OperationSize>( buffer, InstrParams2Reg( reg1, reg2 ) );
                }
                else
                {
                    size += Operation::EncodeInstruction<OperationSize>( buffer, InstrParamsRegAddr( reg1, RegEBP, rightOffset ) );
                }

                templateData->InvalidateReg( reg1 );
                if( targetOffset )
                {
                    int offset = *targetOffset;
                    offset -= templateData->GetBaseOffSet();
                    size += InstructionBySize<OperationSize>::MoveInstruction::EncodeInstruction<OperationSize>( buffer, InstrParamsAddrReg( RegEBP, offset, reg1 ) );
                    templateData->OverwriteStack( offset );
                    templateData->SetStackInfo( reg1, offset );
                }
                if( outReg )
                {
                    *outReg = reg1;
                }

                return size;
            }

            int ReloadArrayBuffer(TemplateContext context, BYTE*& buffer)
            {
                int size = 0;
                if (!context->GetFunctionBody()->GetAsmJsFunctionInfo()->IsHeapBufferConst())
                {
                    X86TemplateData* templateData = GetTemplateData(context);
                    // mov buffer, [mod+bufferOffset]
                    size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(ArrayBufferReg, ModuleEnvReg, AsmJsModuleMemory::MemoryTableBeginOffset));
                    // mov tmpReg, [buffer+lenOffset]
                    RegNum reg = templateData->GetReg<int>(1 << RegEAX);
                    templateData->InvalidateReg(reg);
                    size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(reg, ArrayBufferReg, ArrayBuffer::GetByteLengthOffset()));
                    // mov [mod+offset], tmpReg
                    size += EncodingHelpers::SetStackReg<int>(buffer, templateData, templateData->GetArraySizeOffset(), reg);
                    // mov buffer, [buffer+buffOffset]
                    size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(ArrayBufferReg, ArrayBufferReg, ArrayBuffer::GetBufferOffset()));
                }
                return size;
            }

            int CheckForArrayBufferDetached(TemplateContext context, BYTE*& buffer)
            {
                int size = 0;
                if (context->GetFunctionBody()->GetAsmJsFunctionInfo()->UsesHeapBuffer())
                {
                    X86TemplateData* templateData = GetTemplateData(context);
                    RegNum reg = templateData->GetReg<int>(1 << RegEAX);
                    templateData->InvalidateReg(reg);
                    // mov reg, [mod+bufferOffset]
                    size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(reg, ModuleEnvReg, AsmJsModuleMemory::MemoryTableBeginOffset));
                    // mov reg, [reg+detachedOffset]
                    size += MOV::EncodeInstruction<int8>(buffer, InstrParamsRegAddr(reg, reg, ArrayBuffer::GetIsDetachedOffset()));
                    // test  reg,reg
                    size += TEST::EncodeInstruction<int8>(buffer, InstrParams2Reg(reg, reg));
                    // JE Done
                    JumpRelocation relocDone;
                    EncodingHelpers::EncodeShortJump<JE>(buffer, relocDone, &size);
                    //call Js::Throw::OutOfMemory
                    int32 throwOOM = (int32)(void(*)())Throw::OutOfMemory;
                    size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(reg, throwOOM));
                    size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(reg));
                    // Done:
                    relocDone.ApplyReloc<int8>();
                }
                return size;
            }
        }

        int Br::ApplyTemplate(TemplateContext context, BYTE*& buffer, BYTE** relocAddr, bool isBackEdge)
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            if (isBackEdge)
            {
                RegNum regInc = templateData->GetReg<int>(0);
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(regInc, (int32)context->GetFunctionBody()));
                size += INC::EncodeInstruction<int>(buffer, InstrParamsAddr(regInc, context->GetFunctionBody()->GetAsmJsTotalLoopCountOffset()));
                templateData->InvalidateReg(regInc);
            }
            *relocAddr = buffer;
            EncodingInfo info;
            size += JMP::EncodeInstruction<int>( buffer, InstrParamsImm<int32>( 0 ), &info );
            *relocAddr += info.opSize;

            return size;
        }

        int BrEq::ApplyTemplate(TemplateContext context, BYTE*& buffer, int leftOffset, int rightOffset, BYTE** relocAddr, bool isBackEdge)
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();
            if (isBackEdge)
            {
                RegNum regInc = templateData->GetReg<int>(0);
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(regInc, (int32)context->GetFunctionBody()));
                size += INC::EncodeInstruction<int>(buffer, InstrParamsAddr(regInc, context->GetFunctionBody()->GetAsmJsTotalLoopCountOffset()));
                templateData->InvalidateReg(regInc);
            }
            RegNum reg1, reg2;
            const int reg1Found = templateData->FindRegWithStackOffset<int>( reg1, leftOffset );
            const int reg2Found = templateData->FindRegWithStackOffset<int>( reg2, rightOffset );
            switch( reg1Found & (reg2Found<<1) )
            {
            case 0:
                reg1 = templateData->GetReg<int>();
                size += MOV::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( reg1, RegEBP, leftOffset ) );
                size += CMP::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( reg1, RegEBP, rightOffset ) );
                templateData->SetStackInfo( reg1, leftOffset );
                break;
            case 1:
                size += CMP::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( reg1, RegEBP, rightOffset ) );
                break;
            case 2:
                size += CMP::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( reg2, RegEBP, leftOffset ) );
                break;
            case 3:
                if( reg1 == reg2 )
                {
                    templateData->InvalidateAllReg();
                    *relocAddr = buffer;
                    EncodingInfo info;
                    size += JMP::EncodeInstruction<int>( buffer, InstrParamsImm<int32>( 0 ), &info );
                    *relocAddr += info.opSize;
                    return size;
                }
                else
                {
                    size += CMP::EncodeInstruction<int32>( buffer, InstrParams2Reg( reg1, reg2 ) );
                }
            default:
                __assume( false );
            }

            *relocAddr = buffer;
            EncodingInfo info;
            size += JE::EncodeInstruction<int>( buffer, InstrParamsImm<int32>( 0 ), &info );
            *relocAddr += info.opSize;

            return size;
        }

        int BrTrue::ApplyTemplate( TemplateContext context, BYTE*& buffer, int offset, BYTE** relocAddr, bool isBackEdge)
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            offset -= templateData->GetBaseOffSet();
            RegNum reg;
            if (isBackEdge)
            {
                RegNum regInc = templateData->GetReg<int>(0);
                //see if we can change this to just Inc
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(regInc, (int32)context->GetFunctionBody()));
                size += INC::EncodeInstruction<int>(buffer, InstrParamsAddr(regInc, context->GetFunctionBody()->GetAsmJsTotalLoopCountOffset()));
                templateData->InvalidateReg(regInc);
            }

            if( templateData->FindRegWithStackOffset<int>( reg, offset ) )
            {
                size += CMP::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( reg, 0 ) );
            }
            else
            {
                size += CMP::EncodeInstruction<int>( buffer, InstrParamsAddrImm<int32>( RegEBP, offset, 0 ) );
            }
            *relocAddr = buffer;
            EncodingInfo info;
            size += JNE::EncodeInstruction<int>( buffer, InstrParamsImm<int32>( 0 ), &info );
            *relocAddr += info.opSize;

            return size;
        }

        int Label::ApplyTemplate( TemplateContext context, BYTE*& buffer )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            templateData->InvalidateAllReg();
            return 0;
        }

        int FunctionEntry::ApplyTemplate( TemplateContext context, BYTE*& buffer )
        {
            int size = 0;
            X86TemplateData* templateData = GetTemplateData(context);
            Var CommonEntryPoint = (void(*)(Js::ScriptFunction*, void*))AsmJsCommonEntryPoint;

            // Get the stack size
            FunctionBody* funcBody = context->GetFunctionBody();
            AsmJsFunctionInfo* asmInfo = funcBody->GetAsmJsFunctionInfo();
            int32 stackSize = asmInfo->GetTotalSizeinBytes();
            stackSize = ::Math::Align<int32>(stackSize, 8);

            //Prolog , save EBP and callee saved reg
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( RegEBP ) );
            size += MOV::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEBP, RegESP ) );

            //Start Stack Probe:
            // cmp  esp, ThreadContext::scriptStackLimit + frameSize
            // jg   done
            // push frameSize
            // call ThreadContext::ProbeCurrentStack

            int scriptStackLimit = (int)funcBody->GetScriptContext()->GetThreadContext()->GetScriptStackLimit();

            // cmp  esp, ThreadContext::scriptStackLimit + frameSize
            size += CMP::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegESP, scriptStackLimit + stackSize));

            // jg   Done
            JumpRelocation relocDone;
            EncodingHelpers::EncodeShortJump<JG>(buffer, relocDone, &size);

            // call ThreadContext::ProbeCurrentStack
             int probeStack = (int) (void(*)(size_t, Js::ScriptContext*, int, int))ThreadContext::ProbeCurrentStack;

             //push args

             //push scriptcontext
             size += PUSH::EncodeInstruction<int>(buffer, InstrParamsImm<int>((int)funcBody->GetScriptContext()));

             // push frameSize
             size += PUSH::EncodeInstruction<int>(buffer, InstrParamsImm<int>(stackSize));

             //call probestack
             size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, probeStack));
             size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(RegEAX));

            // Done:
            relocDone.ApplyReloc<int8>();

            //End Stack Probe:
            if (stackSize <= PAGESIZE)
            {
                //Stack Size
                size += SUB::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegESP, stackSize));
            }
            else
            {
                // call ChkStack
                int chkStk = (int)(void(*)(int))_chkstk;
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, stackSize));
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegECX, chkStk));
                size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(RegECX));
            }

            // Move the arg registers here ??? TODO

            //push ebx, push edi and push esi + 8 (offsets int bytecode calculated with two args i.e func obj and var args ??)
            int baseOffSet = stackSize + templateData->GetEBPOffsetCorrection();
            templateData->SetBaseOffset(baseOffSet);

            // push EBP and push funcobj
            int funcOffSet = 2 * sizeof(Var);
            //push args for CEP
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(RegEBP));
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsAddr(RegEBP, funcOffSet));

            // Call CEP
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, (int32)CommonEntryPoint));
            size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(RegEAX));

            //Push callee saved registers
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(RegEBX));
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(RegESI));
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(RegEDI));

            //SetESI and EDI
            //EnvOffset - SlotIndex 1 in the stack
            //ArrptrOffset - Slot Index 2 in the stack
            // stackSize + templateData->GetCalleSavedRegSizeInByte() - this gives the offset of the beginning of stack from EBP

            int envOffset = sizeof(Var) - stackSize;
            int arrPtrOffset = 2 * sizeof(Var) - stackSize;
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(RegEDI, RegEBP, envOffset));
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(RegESI, RegEBP, arrPtrOffset));

            size += EncodingHelpers::ReloadArrayBuffer(context, buffer);
            templateData->InvalidateAllVolatileReg();
            return size;
        }

        int FunctionExit::ApplyTemplate( TemplateContext context, BYTE*& buffer )
        {
            int size = 0;
#if DBG_DUMP
            if (PHASE_ON1(AsmjsFunctionEntryPhase))
            {
                Var CommonCallHelper = (void(*)(Js::ScriptFunction*))AsmJSCommonCallHelper;
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, (int32)CommonCallHelper));
                size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(RegEAX));
            }
#endif
            size += POP::EncodeInstruction<int>( buffer, InstrParamsReg( RegEDI ) );
            size += POP::EncodeInstruction<int>( buffer, InstrParamsReg( RegESI ) );
            size += POP::EncodeInstruction<int>(buffer, InstrParamsReg(RegEBX));
            size += MOV::EncodeInstruction<int>(buffer, InstrParams2Reg(RegESP, RegEBP));
            size += POP::EncodeInstruction<int>( buffer, InstrParamsReg( RegEBP ) );


            //arg size + func
            int argSize = context->GetFunctionBody()->GetAsmJsFunctionInfo()->GetArgByteSize();
            // to keep 8 byte alignment after the pop EIP in RET, we add MachPtr for the func object after alignment
            argSize = ::Math::Align<int32>(argSize, 8) + MachPtr;
            EncodingInfo info;
            size += RET::EncodeInstruction<int>(buffer, InstrParamsImm<int>(argSize), &info);
            return size;
        }

        int LdSlot_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int slotIndex )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister( buffer, context, size );
            RegNum reg2 = templateData->GetReg<int>(1<<reg);
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg2, reg, slotIndex*sizeof( int ) ) );
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset, reg2 );

            return size;
        }

        int LdSlot_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int slotIndex)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister(buffer, context, size);
            RegNum reg2 = templateData->GetReg<float>(1 << reg);
            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg2, reg, slotIndex*sizeof(float)));
            size += EncodingHelpers::SetStackReg<float>(buffer, templateData, targetOffset, reg2);

            return size;
        }

        int StSlot_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int srcOffset, int slotIndex )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister( buffer, context, size );
            RegNum reg2;
            if( !templateData->FindRegWithStackOffset<int>( reg2, srcOffset ) )
            {
                reg2 = templateData->GetReg<int>( 1 << reg );
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg2, RegEBP, srcOffset ) );
                templateData->SetStackInfo( reg2, srcOffset );
            }
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsAddrReg( reg, slotIndex*sizeof( int ) , reg2 ) );

            return size;
        }

        int StSlot_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffset, int slotIndex)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister(buffer, context, size);
            RegNum reg2;
            if (!templateData->FindRegWithStackOffset<float>(reg2, srcOffset))
            {
                reg2 = templateData->GetReg<float>(1 << reg);
                size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg2, RegEBP, srcOffset));
                templateData->SetStackInfo(reg2, srcOffset);
            }
            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(reg, slotIndex*sizeof(float), reg2));

            return size;
        }
        int Ld_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();
            if( targetOffset == rightOffset )
            {
                return 0;
            }

            int size = 0;

            RegNum reg = EncodingHelpers::GetStackReg<int>( buffer, templateData, rightOffset, size );

            size += MOV::EncodeInstruction<int32>( buffer, InstrParamsAddrReg(RegEBP, targetOffset, reg) );
            templateData->OverwriteStack( targetOffset );
            templateData->SetStackInfo( reg, targetOffset );

            return size;
        }

        int LdConst_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int offset, int value )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            offset -= templateData->GetBaseOffSet();
            templateData->OverwriteStack( offset );

            int size = MOV::EncodeInstruction<int32>( buffer, InstrParamsAddrImm<int32>(RegEBP, offset, value) );
            return size;
        }

        int SetReturn_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int offset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            offset -= templateData->GetBaseOffSet();
            RegNum reg = RegEAX;
            if( !templateData->FindRegWithStackOffset<int>( reg, offset ) )
            {
                templateData->SetStackInfo( RegEAX, offset );
                return MOV::EncodeInstruction<int32>( buffer, InstrParamsRegAddr(RegEAX, RegEBP, offset) );
            }
            else if( reg != RegEAX )
            {
                templateData->SetStackInfo( RegEAX, offset );
                return MOV::EncodeInstruction<int32>( buffer, InstrParams2Reg(RegEAX, reg) );
            }
            // value already in eax, do nothing
            return 0;
        }

        int Neg_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            rightOffset -= templateData->GetBaseOffSet();
            targetOffset -= templateData->GetBaseOffSet();
            int size = 0;

            if( targetOffset == rightOffset )
            {
                size += NEG::EncodeInstruction<int32>( buffer, InstrParamsAddr( RegEBP, targetOffset ) );
                templateData->OverwriteStack( targetOffset );
            }
            else
            {
                RegNum reg;
                if( !templateData->FindRegWithStackOffset<int>( reg, rightOffset ) )
                {
                    reg = templateData->GetReg<int>();
                    MOV::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( reg, RegEBP, rightOffset ) );
                }
                size += NEG::EncodeInstruction<int32>( buffer, InstrParamsReg( reg ) );

                size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg);
            }
            return size;
        }

        int Not_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();
            int size = 0;

            if( targetOffset == rightOffset )
            {
                size += NOT::EncodeInstruction<int32>( buffer, InstrParamsAddr( RegEBP, targetOffset ) );
                templateData->OverwriteStack( targetOffset );
            }
            else
            {
                RegNum reg;
                if( !templateData->FindRegWithStackOffset<int>( reg, rightOffset ) )
                {
                    reg = templateData->GetReg<int>();
                    MOV::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( reg, RegEBP, rightOffset ) );
                }
                size += NOT::EncodeInstruction<int32>( buffer, InstrParamsReg( reg ) );

                size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg);
            }
            return size;
        }

        int Int_To_Bool::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();
            RegNum reg = templateData->GetReg<int>(~Mask8BitsReg);
            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( reg, reg ) );
            size += CMP::EncodeInstruction<int>( buffer, InstrParamsAddrImm<int8>( RegEBP, rightOffset, 0 ) );
            size += SETNE::EncodeInstruction<int8>( buffer, InstrParamsReg( reg ) );
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg);

            return size;
        }

        int LogNot_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();
            RegNum reg = templateData->GetReg<int>(~Mask8BitsReg);
            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( reg, reg ) );
            size += CMP::EncodeInstruction<int>( buffer, InstrParamsAddrImm<int8>( RegEBP, rightOffset, 0 ) );
            size += SETE::EncodeInstruction<int8>( buffer, InstrParamsReg( reg ) );

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg);

            return size;
        }

        int Or_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            int size = 0;
            size += EncodingHelpers::CommutativeOperation<OR,int32>( context, buffer, leftOffset, rightOffset, &targetOffset );
            return size;
        }
        int And_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            int size = 0;
            size += EncodingHelpers::CommutativeOperation<AND,int32>( context, buffer, leftOffset, rightOffset, &targetOffset );
            return size;
        }

        int Xor_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            int size = 0;
            size += EncodingHelpers::CommutativeOperation<XOR,int32>( context, buffer, leftOffset, rightOffset, &targetOffset );
            return size;
        }

        int Shr_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg1, reg2;
            if( leftOffset != rightOffset )
            {
                if( !templateData->FindRegWithStackOffset<int>( reg1, leftOffset, 1<<RegECX ) )
                {
                    reg1 = templateData->GetReg<int>( 1 << RegECX );
                    size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg1, RegEBP, leftOffset ) );
                    templateData->SetStackInfo( reg1, leftOffset );
                }
            }
            else
            {
                reg1 = RegECX;
            }

            if( !templateData->FindRegWithStackOffset<int>( reg2, rightOffset ) )
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegECX, RegEBP, rightOffset ) );
                templateData->SetStackInfo( RegECX, rightOffset );
            }
            else if( reg2 != RegECX )
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParams2Reg( RegECX, reg2) );
                templateData->SetStackInfo( RegECX, rightOffset );
            }

            size += SAR::EncodeInstruction<int>( buffer, InstrParams2Reg( reg1, RegECX ) );

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg1);

            return size;
        }

        int Shl_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg1, reg2;
            if( leftOffset != rightOffset )
            {
                if( !templateData->FindRegWithStackOffset<int>( reg1, leftOffset, 1<<RegECX ) )
                {
                    reg1 = templateData->GetReg<int>( 1 << RegECX );
                    size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg1, RegEBP, leftOffset ) );
                    templateData->SetStackInfo( reg1, leftOffset );
                }
            }
            else
            {
                reg1 = RegECX;
            }

            if( !templateData->FindRegWithStackOffset<int>( reg2, rightOffset ) )
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegECX, RegEBP, rightOffset ) );
                templateData->SetStackInfo( RegECX, rightOffset );
            }
            else if( reg2 != RegECX )
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParams2Reg( RegECX, reg2) );
                templateData->SetStackInfo( RegECX, rightOffset );
            }

            // Encode  shl reg,cl
            size += SHL::EncodeInstruction<int>( buffer, InstrParams2Reg( reg1, RegECX ) );

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg1);

            return size;
        }

        int ShrU_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg1, reg2;
            if( leftOffset != rightOffset )
            {
                if( !templateData->FindRegWithStackOffset<int>( reg1, leftOffset, 1<<RegECX ) )
                {
                    reg1 = templateData->GetReg<int>( 1 << RegECX );
                    size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg1, RegEBP, leftOffset ) );
                    templateData->SetStackInfo( reg1, leftOffset );
                }
            }
            else
            {
                reg1 = RegECX;
            }

            if( !templateData->FindRegWithStackOffset<int>( reg2, rightOffset ) )
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegECX, RegEBP, rightOffset ) );
                templateData->SetStackInfo( RegECX, rightOffset );
            }
            else if( reg2 != RegECX )
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParams2Reg( RegECX, reg2) );
                templateData->SetStackInfo( RegECX, rightOffset );
            }

            // Encode  shr reg,cl
            size += SHR::EncodeInstruction<int>( buffer, InstrParams2Reg( reg1, RegECX ) );

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg1);

            return size;
        }

        int Add_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset  )
        {
            int size = 0;
            size += EncodingHelpers::CommutativeOperation<ADD,int32>( context, buffer, leftOffset, rightOffset, &targetOffset );
            return size;
        }

        int Sub_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset  )
        {
            int size = 0;
            size += EncodingHelpers::NonCommutativeOperation<SUB,int32>( context, buffer, leftOffset, rightOffset, &targetOffset );
            return size;
        }

        int Mul_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset  )
        {
            int size = 0;
            size += EncodingHelpers::CommutativeOperation<IMUL,int32>( context, buffer, leftOffset, rightOffset, &targetOffset );
            return size;
        }

        int Div_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset  )
        {
            X86TemplateData* templateData = GetTemplateData( context );

            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum rhsReg = EncodingHelpers::GetStackReg<int>(buffer,templateData,rightOffset,size,1<<RegEAX|1<<RegEDX);

            // test  reg,reg
            size += TEST::EncodeInstruction<int>(buffer, InstrParams2Reg(rhsReg, rhsReg));
            // JNE Label1
            JumpRelocation relocLabel1;
            EncodingHelpers::EncodeShortJump<JNE>( buffer, relocLabel1, &size );
            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEAX, RegEAX ) );
            // JMP LabelEnd
            JumpRelocation relocLabelEnd;
            EncodingHelpers::EncodeShortJump<JMP>( buffer, relocLabelEnd, &size );

            // Label1:
            relocLabel1.ApplyReloc<int8>();

            // MOV  eax, [leftOffset]
            RegNum lhsReg;
            if (!templateData->FindRegWithStackOffset<int>(lhsReg, leftOffset))
            {
                size += MOV::EncodeInstruction<int32>( buffer, InstrParamsRegAddr(RegEAX, RegEBP, leftOffset) );
            }
            else if (lhsReg != RegEAX)
            {
                size += MOV::EncodeInstruction<int32>(buffer, InstrParams2Reg(RegEAX, lhsReg));
            }

            size += CMP::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegEAX, 0x80000000 ) );
            // JNE LabelDoDiv
            JumpRelocation relocLabelDoDiv;
            EncodingHelpers::EncodeShortJump<JNE>( buffer, relocLabelDoDiv, &size );
            // CMP reg,-1
            size += CMP::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(rhsReg, -1));
            // JNE LabelDoDiv
            JumpRelocation relocLabelDoDiv2;
            EncodingHelpers::EncodeShortJump<JNE>( buffer, relocLabelDoDiv2, &size );
            // JMP LabelEnd
            JumpRelocation relocLabelEnd2;
            EncodingHelpers::EncodeShortJump<JMP>( buffer, relocLabelEnd2, &size );

            // LabelDoDiv:
            relocLabelDoDiv.ApplyReloc<int8>();
            relocLabelDoDiv2.ApplyReloc<int8>();

            // cdq
            size += CDQ::EncodeInstruction<int>( buffer );
            // idiv reg
            size += IDIV::EncodeInstruction<int>(buffer, InstrParamsReg(rhsReg));

            // LabelEnd:
            relocLabelEnd.ApplyReloc<int8>();
            relocLabelEnd2.ApplyReloc<int8>();

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset, RegEAX );
            templateData->InvalidateReg( RegEDX );

            return size;
        }

        int Rem_Int::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            //Xor   regedx , regedx
            size += XOR::EncodeInstruction<int>(buffer, InstrParams2Reg(RegEDX, RegEDX));

            //mov   eax , leftoffset
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(RegEAX, RegEBP, leftOffset));

            RegNum rhsReg = EncodingHelpers::GetStackReg<int>(buffer, templateData, rightOffset, size, 1 << RegEAX | 1 << RegEDX);
            //cmp   rightoffset, 0
            size += CMP::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(rhsReg, 0));
            //je    :L4
            JumpRelocation reloc(buffer, &size);
            EncodingInfo info;
            size += JE::EncodeInstruction<int8>(buffer, InstrParamsImm<int8>(0), &info);
            reloc.JumpEncoded(info);

            //cmp   leftoffset -2147483648
            size += CMP::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, 0x80000000));

            //jne   :L3
            JumpRelocation reloc2(buffer, &size);
            EncodingInfo info2;
            size += JNE::EncodeInstruction<int8>(buffer, InstrParamsImm<int8>(0), &info2);
            reloc2.JumpEncoded(info2);

            //cmp   rightoffset -1
            size += CMP::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(rhsReg, -1));

            //je    :L4
            JumpRelocation reloc3(buffer, &size);
            EncodingInfo info3;
            size += JE::EncodeInstruction<int8>(buffer, InstrParamsImm<int8>(0), &info3);
            reloc3.JumpEncoded(info3);

            //:L3
            reloc2.ApplyReloc<int8>();
            //cdq
            size += CDQ::EncodeInstruction<int>(buffer);

            //idiv  rightoffset
            size += IDIV::EncodeInstruction<int>(buffer, InstrParamsReg(rhsReg));

            //mov   targetoffset , edx
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset,RegEDX);

            //jmp   :L5
            JumpRelocation reloc4(buffer, &size);
            EncodingInfo info4;
            size += JMP::EncodeInstruction<int8>(buffer, InstrParamsImm<int8>(0), &info4);
            reloc4.JumpEncoded(info4);

            //:L4
            reloc.ApplyReloc<int8>();
            reloc3.ApplyReloc<int8>();

            //mov   targetoffset , 0
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsAddrImm<int32>(RegEBP, targetOffset, 0));

            //:L5
            reloc4.ApplyReloc<int8>();

            //mov   eax, targetoffset
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(RegEAX, RegEBP, targetOffset));
            templateData->InvalidateReg(RegEAX);
            templateData->InvalidateReg(RegEDX);

            return size;
        }

#define IntCmp(name, jmp) \
        int name::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )\
        {\
            X86TemplateData* templateData = GetTemplateData( context );\
            int size = 0;\
            RegNum resultReg = templateData->GetReg<int>();\
            size += XOR::EncodeInstruction<int32>( buffer, InstrParams2Reg( resultReg, resultReg ) );\
            size += EncodingHelpers::NonCommutativeOperation<CMP,int>( context, buffer, leftOffset, rightOffset, nullptr, nullptr, 1 << resultReg );\
            size += jmp::EncodeInstruction<int8>( buffer, InstrParamsImm<int8>(1) );\
            size += INC::EncodeInstruction<int32>( buffer, InstrParamsReg( resultReg ) );\
            targetOffset -= templateData->GetBaseOffSet();\
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , resultReg);\
            return size;\
        }
        IntCmp(Lt_Int,JGE)
        IntCmp(Le_Int,JG)
        IntCmp(Gt_Int,JLE)
        IntCmp(Ge_Int,JL)
        IntCmp(Eq_Int,JNE)
        IntCmp(Ne_Int,JE)

        IntCmp(Lt_UInt,JAE)
        IntCmp(Le_UInt,JA)
        IntCmp(Gt_UInt,JBE)
        IntCmp(Ge_UInt,JB)
#undef IntCmp

        int Min_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;

            RegNum reg1;
            size += EncodingHelpers::NonCommutativeOperation<CMP, int>( context, buffer, leftOffset, rightOffset, nullptr, &reg1 );
            RegNum reg2;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();
            if( templateData->FindRegWithStackOffset<int>( reg2, rightOffset ) )
            {
                size += CMOVG::EncodeInstruction<int>( buffer, InstrParams2Reg( reg1, reg2 ) );
            }
            else
            {
                size += CMOVG::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg1, RegEBP, rightOffset) );
            }

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg1);

            return size;
        }

        int Max_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            RegNum reg1;
            size += EncodingHelpers::NonCommutativeOperation<CMP, int>( context, buffer, leftOffset, rightOffset, nullptr, &reg1 );
            RegNum reg2;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            if( templateData->FindRegWithStackOffset<int>( reg2, rightOffset ) )
            {
                size += CMOVL::EncodeInstruction<int>( buffer, InstrParams2Reg( reg1, reg2 ) );
            }
            else
            {
                size += CMOVL::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg1, RegEBP, rightOffset) );
            }

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , reg1);

            return size;
        }

        int Abs_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            if( templateData->FindRegWithStackOffset<int>( reg, rightOffset ) )
            {
                if( reg != RegEAX )
                {
                    size += MOV::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEAX, reg ) );
                }
            }
            else
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegEAX, RegEBP, rightOffset ) );
            }

            size += CDQ::EncodeInstruction<int>( buffer );
            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEAX, RegEDX ) );
            size += SUB::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEAX, RegEDX ) );
            templateData->InvalidateReg( RegEDX );
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEAX);

            return size;
        }

        int Clz32_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            // BSR tmp, src
            // JE  $label32
            // MOV dst, 31
            // SUB dst, tmp
            // JMP $done
            // label32:
            // MOV dst, 32
            // $done
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum tmpReg = templateData->GetReg<int>();
            RegNum srcReg;
            if (templateData->FindRegWithStackOffset<int>(srcReg, rightOffset))
            {
                size += BSR::EncodeInstruction<int>(buffer, InstrParams2Reg(tmpReg, srcReg));
            }
            else
            {
                size += BSR::EncodeInstruction<int>(buffer, InstrParamsRegAddr(tmpReg, RegEBP, rightOffset));
            }
            JumpRelocation relocLabel32;
            EncodingHelpers::EncodeShortJump<JE>(buffer, relocLabel32, &size);

            RegNum dstReg = templateData->GetReg<int>(1 << tmpReg);
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int>(dstReg, 31));
            size += SUB::EncodeInstruction<int8>(buffer, InstrParams2Reg(dstReg, tmpReg));

            JumpRelocation relocLabelDone;
            EncodingHelpers::EncodeShortJump<JMP>(buffer, relocLabelDone, &size);

            relocLabel32.ApplyReloc<int8>();
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int>(dstReg, 32));

            relocLabelDone.ApplyReloc<int8>();

            templateData->InvalidateReg(tmpReg);
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , dstReg);

            return size;
        }

        int Mul_UInt::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg1, reg2;
            const int reg1Found = templateData->FindRegWithStackOffset<int>( reg1, rightOffset, 1<<RegEDX );
            const int reg2Found = templateData->FindRegWithStackOffset<int>( reg2, leftOffset, 1<<RegEDX );

            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg(RegEDX,RegEDX) );
            switch( reg1Found & ( reg2Found << 1 ) )
            {
            case 0: // none found
                reg1 = RegEAX;
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg1, RegEBP, leftOffset ) );
                templateData->SetStackInfo( reg1, leftOffset );
                size += MUL::EncodeInstruction<int>( buffer, InstrParamsAddr(RegEBP, rightOffset) );
                break;
            case 1: // found 2
                if( reg2 == RegEAX )
                {
                    size += MUL::EncodeInstruction<int>( buffer, InstrParamsAddr(RegEBP, leftOffset) );
                }
                else
                {
                    size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegEAX, RegEBP, leftOffset ) );
                    size += MUL::EncodeInstruction<int>( buffer, InstrParamsReg(reg2) );
                }
                break;
            case 2: // found 1
                if( reg1 == RegEAX )
                {
                    size += MUL::EncodeInstruction<int>( buffer, InstrParamsAddr(RegEBP, rightOffset) );
                }
                else
                {
                    size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegEAX, RegEBP, rightOffset ) );
                    size += MUL::EncodeInstruction<int>( buffer, InstrParamsReg(reg1) );
                }
                break;
            case 3: // found both
                if( reg1 == RegEAX )
                {
                    size += MUL::EncodeInstruction<int>( buffer, InstrParamsReg(reg2) );
                }
                else if( reg2 == RegEAX )
                {
                    size += MUL::EncodeInstruction<int>( buffer, InstrParamsReg(reg1) );
                }
                else
                {
                    size += MOV::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEAX, reg1 ) );
                    size += MUL::EncodeInstruction<int>( buffer, InstrParamsReg(reg2) );
                }
                break;
            default:
                __assume( false );
            }

            size += TEST::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEDX, RegEDX ) );
            JumpRelocation reloc;
            EncodingHelpers::EncodeShortJump<JE>( buffer, reloc, &size );
            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEAX, RegEAX ) );
            reloc.ApplyReloc<int8>();
            templateData->InvalidateReg( RegEDX );

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEAX);

            return size;
        }

        int Div_UInt::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetStackReg<int>( buffer, templateData, rightOffset, size, 1 << RegEDX | 1 << RegEAX );

            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEAX, RegEAX ) );
            size += CMP::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( reg, 0 ) );


            JumpRelocation reloc( buffer, &size );
            EncodingInfo info1;
            // JNE labelEnd
            size += JE::EncodeInstruction<int8>( buffer, InstrParamsImm<int8>( 0 ), &info1 );
            reloc.JumpEncoded( info1 );

            size += MOV::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( RegEAX, RegEBP, leftOffset ) );
            size += XOR::EncodeInstruction<int32>( buffer, InstrParams2Reg( RegEDX, RegEDX ) );
            size += DIV::EncodeInstruction<int32>( buffer, InstrParamsReg( reg ) );

            // labelEnd:
            reloc.ApplyReloc<int8>();

            templateData->InvalidateReg( RegEDX );
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEAX);

            return size;
        }

        int Rem_UInt::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( RegEDX, RegEDX ) );
            size += CMP::EncodeInstruction<int>( buffer, InstrParamsAddrImm<int>( RegEBP, rightOffset, 0 ) );
            size += JE::EncodeInstruction<int8>( buffer, InstrParamsImm<int8>( 0 ) );
            BYTE* reloc = &buffer[-1];
            int relocSize = 0;
            relocSize += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegEAX, RegEBP, leftOffset ) );
            relocSize += DIV::EncodeInstruction<int>( buffer, InstrParamsAddr( RegEBP, rightOffset ) );
            Assert( FitsInByte( relocSize ) );
            *reloc = (BYTE)relocSize;

            size += relocSize;
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEDX);

            templateData->InvalidateReg(RegEAX);
            return size;
        }

        int SetReturn_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int offset )
        {
            X86TemplateData* templateData = GetTemplateData(context);
            offset -= templateData->GetBaseOffSet();
            RegNum reg = RegXMM0;
            if (!templateData->FindRegWithStackOffset<double>(reg, offset))
            {
                templateData->SetStackInfo(RegXMM0, offset);
                return MOVSD::EncodeInstruction<double>(buffer, InstrParamsRegAddr(RegXMM0, RegEBP, offset));
            }
            if (reg != RegXMM0)
            {
                return MOVSD::EncodeInstruction<double>(buffer, InstrParams2Reg(RegXMM0, reg));
            }
            return 0;
        }

        int SetReturn_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int offset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            offset -= templateData->GetBaseOffSet();
            RegNum reg = RegXMM0;
            if (!templateData->FindRegWithStackOffset<float>(reg, offset))
            {
                templateData->SetStackInfo(RegXMM0, offset);
                return MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(RegXMM0, RegEBP, offset));
            }
            if (reg != RegXMM0)
            {
                return MOVSS::EncodeInstruction<float>(buffer, InstrParams2Reg(RegXMM0, reg));
            }
            return 0;
        }

        int SetFround_Db::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset,int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg = templateData->GetReg<float>();
            RegNum reg1;
            int size = 0;
            if (templateData->FindRegWithStackOffset<double>(reg1, rightOffset))
            {
                size += CVTSD2SS::EncodeInstruction<double>(buffer, InstrParams2Reg(reg, reg1));
            }
            else
            {
                size += CVTSD2SS::EncodeInstruction<double>(buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset));
            }
            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(RegEBP, targetOffset, reg));
            templateData->OverwriteStack(targetOffset);
            templateData->SetStackInfo(reg,targetOffset);
            return size;
        }

        int SetFround_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg = templateData->GetReg<float>();

            int size = 0;
            if (!templateData->FindRegWithStackOffset<float>(reg, rightOffset))
            {
                size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset));
                templateData->SetStackInfo(reg, rightOffset);
            }
            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(RegEBP, targetOffset, reg));
            templateData->OverwriteStack(targetOffset);
            return size;
        }

        int SetFround_Int::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg = templateData->GetReg<float>();
            RegNum reg1;

            int size = 0;
            if (templateData->FindRegWithStackOffset<int32>(reg1, rightOffset))
            {
                size += CVTSI2SS::EncodeInstruction<int32>(buffer, InstrParams2Reg(reg, reg1));
            }
            else
            {
                size += CVTSI2SS::EncodeInstruction<int32>(buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset));
            }

            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(RegEBP, targetOffset, reg));
            templateData->OverwriteStack(targetOffset);
            templateData->SetStackInfo(reg, targetOffset);

            return size;
        }

        int StSlot_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int srcOffset, int slotIndex )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();
            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister( buffer, context, size );
            RegNum reg2;
            if( !templateData->FindRegWithStackOffset<double>( reg2, srcOffset ) )
            {
                reg2 = templateData->GetReg<double>();
                size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( reg2, RegEBP, srcOffset ) );
                templateData->SetStackInfo( reg2, srcOffset );
            }
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg( reg, slotIndex*sizeof( double ) , reg2 ) );

            return size;
        }

        int LdSlot_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int slotIndex )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister( buffer, context, size );
            RegNum reg2 = templateData->GetReg<double>();
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( reg2, reg, slotIndex*sizeof( double ) ) );
            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , reg2);

            return size;
        }

        int LdAddr_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, const double* dbAddr )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum reg = templateData->GetReg<double>();
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegPtr( reg, (void*)dbAddr ) );
            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , reg);
            return size;
        }

        int Ld_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            if( targetOffset == rightOffset )
            {
                return 0;
            }

            RegNum reg = templateData->GetReg<double>();

            int size = 0;
            if( !templateData->FindRegWithStackOffset<double>( reg, rightOffset ) )
            {
                size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset) );
                templateData->SetStackInfo( reg, rightOffset );
            }
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg(RegEBP, targetOffset, reg) );
            templateData->OverwriteStack( targetOffset );

            return size;
        }
        int Ld_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            if (targetOffset == rightOffset)
            {
                return 0;
            }
            //get reg can be double registers for float too
            RegNum reg = templateData->GetReg<float>();

            int size = 0;
            if (!templateData->FindRegWithStackOffset<float>(reg, rightOffset))
            {
                size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset));
                templateData->SetStackInfo(reg, rightOffset);
            }
            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(RegEBP, targetOffset, reg));
            templateData->OverwriteStack(targetOffset);
            return size;
        }

        int Add_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            int size = 0;

            size += EncodingHelpers::CommutativeOperation<ADDSS, float>(context, buffer, leftOffset, rightOffset, &targetOffset);
            return size;
        }

        int Add_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            int size = 0;

            size += EncodingHelpers::CommutativeOperation<ADDSD, double>( context, buffer, leftOffset, rightOffset, &targetOffset );
            return size;
        }

        int Sub_Db::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            int size = 0;

            size += EncodingHelpers::NonCommutativeOperation<SUBSD, double>(context, buffer, leftOffset, rightOffset, &targetOffset);
            return size;
        }

        int Mul_Db::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            int size = 0;

            size += EncodingHelpers::CommutativeOperation<MULSD, double>(context, buffer, leftOffset, rightOffset, &targetOffset);
            return size;
        }

        int Div_Db::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            int size = 0;
            size += EncodingHelpers::CommutativeOperation<DIVSD, double>(context, buffer, leftOffset, rightOffset, &targetOffset);
            return size;
        }

        int Sub_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            int size = 0;
            size += EncodingHelpers::NonCommutativeOperation<SUBSS, float>(context, buffer, leftOffset, rightOffset, &targetOffset);
            return size;
        }

        int Mul_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            int size = 0;
            size += EncodingHelpers::CommutativeOperation<MULSS, float>(context, buffer, leftOffset, rightOffset, &targetOffset);
            return size;
        }

        int Div_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            int size = 0;

            size += EncodingHelpers::CommutativeOperation<DIVSS, double>(context, buffer, leftOffset, rightOffset, &targetOffset);
            return size;
        }



        int Rem_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            leftOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            //AsmJsMath::Rem < int > ;
            size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, 16 ) );
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( RegXMM0, RegEBP, rightOffset ) );
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg( RegESP, 8, RegXMM0 ) );
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( RegXMM0, RegEBP, leftOffset ) );
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg( RegESP, 0, RegXMM0 ) );
            void* ptr = (double (*)(double,double)) AsmJsMath::Rem < double > ;
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>(RegEAX,(int)ptr) );
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg(RegEAX) );

            templateData->InvalidateAllVolatileReg();
            size += FSTP::EncodeInstruction<double>( buffer, InstrParamsAddr( RegEBP, targetOffset ) );

            templateData->InvalidateReg( RegEAX );
            templateData->OverwriteStack( targetOffset );

            return size;
        }

        template<typename JCC, typename OperationSignature, typename Size>
        int CompareEq(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            RegNum resultReg = templateData->GetReg<int>(1 << RegEAX);
            size += XOR::EncodeInstruction<int32>(buffer, InstrParams2Reg(resultReg, resultReg));
            size += EncodingHelpers::NonCommutativeOperation<OperationSignature, Size>(context, buffer, leftOffset, rightOffset);
            size += LAHF::EncodeInstruction<int32>(buffer);
            size += TEST::EncodeInstruction<int8>(buffer, InstrParamsRegImm<int8>(RegNum(RegEAX), 0x44));
            /*fix for ah*/buffer[-2] |= 0x04;
            size += JCC::EncodeInstruction<int8>(buffer, InstrParamsImm<int8>(1));
            size += INC::EncodeInstruction<int32>(buffer, InstrParamsReg(resultReg));
            templateData->InvalidateReg(RegEAX);
            targetOffset -= templateData->GetBaseOffSet();
            size += EncodingHelpers::SetStackReg<int>(buffer, templateData, targetOffset, resultReg);
            return size;
        }

        int CmpEq_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareEq<JP, UCOMISS, float>(context, buffer, targetOffset, leftOffset, rightOffset);
        }

        int CmpNe_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareEq<JNP, UCOMISS, float>(context, buffer, targetOffset, leftOffset, rightOffset);
        }

        int CmpEq_Db::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareEq<JP, UCOMISD, double>(context, buffer, targetOffset, leftOffset, rightOffset);
        }

        int CmpNe_Db::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareEq<JNP, UCOMISD, double>(context, buffer, targetOffset, leftOffset, rightOffset);
        }

        template<typename JCC, typename OperationSignature, typename Size>
        int CompareDbOrFlt( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            // we are modifying the rightoffset and leftoffset in the call to  EncodingHelpers::NonCommutativeOperation
            targetOffset -= templateData->GetBaseOffSet();

            RegNum resultReg = templateData->GetReg<int>();
            size += XOR::EncodeInstruction<int32>( buffer, InstrParams2Reg( resultReg, resultReg ) );
            size += EncodingHelpers::NonCommutativeOperation<OperationSignature, Size>(context, buffer, leftOffset, rightOffset);
            size += JCC::EncodeInstruction<int8>( buffer, InstrParamsImm<int8>(1) );
            size += INC::EncodeInstruction<int32>( buffer, InstrParamsReg( resultReg ) );
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , resultReg);
            return size;
        }
        int CmpLt_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareDbOrFlt<JBE, COMISS, float>(context, buffer, targetOffset, rightOffset, leftOffset);
        }
        int CmpLe_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareDbOrFlt<JB, COMISS, float>(context, buffer, targetOffset, rightOffset, leftOffset);
        }
        int CmpGt_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareDbOrFlt<JBE, COMISS, float>(context, buffer, targetOffset, leftOffset, rightOffset);
        }
        int CmpGe_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset)
        {
            return CompareDbOrFlt<JB, COMISS, float>(context, buffer, targetOffset, leftOffset, rightOffset);
        }

        int CmpLt_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            return CompareDbOrFlt<JBE, COMISD, double>(context, buffer, targetOffset, rightOffset, leftOffset);
        }
        int CmpLe_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            return CompareDbOrFlt<JB, COMISD, double>(context, buffer, targetOffset, rightOffset, leftOffset);
        }
        int CmpGt_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            return CompareDbOrFlt<JBE, COMISD, double>(context, buffer, targetOffset, leftOffset, rightOffset);
        }
        int CmpGe_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int leftOffset, int rightOffset )
        {
            return CompareDbOrFlt<JB, COMISD, double>(context, buffer, targetOffset, leftOffset, rightOffset);
        }


        int UInt_To_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum regInt;
            RegNum regDouble = templateData->GetReg<double>();
            if( !templateData->FindRegWithStackOffset<int>( regInt, rightOffset ) )
            {
                regInt = templateData->GetReg<int>();
                size += MOV::EncodeInstruction<int32>( buffer, InstrParamsRegAddr( regInt, RegEBP, rightOffset ) );
            }
            size += MOVD::EncodeInstruction<double>( buffer, InstrParams2Reg(regDouble,regInt) );
            size += CVTDQ2PD::EncodeInstruction<double>( buffer, InstrParams2Reg( regDouble, regDouble ) );
            size += SHR::EncodeInstruction<int32>( buffer, InstrParamsRegImm<int8>( regInt, 31 ) );
            templateData->InvalidateReg( regInt );

            static __declspec(align(8)) const double MaskConvUintDouble[] = { 0.0, 4294967296.0 };
            size += ADDSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( regDouble, RegNOREG, regInt, 8, (int)MaskConvUintDouble ) );

            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , regDouble);

            return size;
        }


        int Int_To_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg = templateData->GetReg<double>(), regInt;
            if( templateData->FindRegWithStackOffset<int>( regInt, rightOffset ) )
            {
                size += CVTSI2SD::EncodeInstruction<double>( buffer, InstrParams2Reg( reg, regInt ) );
            }
            else
            {
                size += CVTSI2SD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( reg, RegEBP, rightOffset ) );
            }
            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , reg);

            return size;
        }

        int Float_To_Db::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg = templateData->GetReg<double>();
            RegNum reg1;
            if (templateData->FindRegWithStackOffset<float>(reg1, rightOffset))
            {
                size += CVTSS2SD::EncodeInstruction<float>(buffer, InstrParams2Reg(reg, reg1));
            }
            else
            {
                size += CVTSS2SD::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset));
            }
            size += EncodingHelpers::SetStackReg<double>(buffer, templateData, targetOffset, reg);

            return size;
        }

        int Float_To_Int::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg = templateData->GetReg<int>();
            RegNum reg1;
            if (templateData->FindRegWithStackOffset<float>(reg1, rightOffset))
            {
                size += CVTTSS2SI::EncodeInstruction<float>(buffer, InstrParams2Reg(reg, reg1));
            }
            else
            {
                size += CVTTSS2SI::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset));
            }
            size += EncodingHelpers::SetStackReg<int>(buffer, templateData, targetOffset, reg);

            return size;
        }

        int Db_To_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, 8 ) );
            if( !templateData->FindRegWithStackOffset<double>( reg, rightOffset ) )
            {
                reg = templateData->GetReg<double>();
                size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( reg, RegEBP, rightOffset ) );
                templateData->SetStackInfo( reg, rightOffset );
            }
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg( RegESP, 0, reg ) );
            void* addr = ((int(*)(double))JavascriptMath::ToInt32Core);
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegEAX, (int)addr ) );
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg( RegEAX ) );
            templateData->InvalidateAllVolatileReg();
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEAX);

            return size;
        }


        int Neg_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            if( !templateData->FindRegWithStackOffset<double>( reg, rightOffset ) )
            {
                reg = templateData->GetReg<double>();
                size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( reg, RegEBP, rightOffset ) );
            }

            static __declspec(align(16)) const double MaskNegDouble[] = { -0.0, -0.0 };
            static const BYTE temp[] = {
                0x66, 0x0F, 0x57, 0x05,
                (BYTE)( ( (int)( MaskNegDouble ) ) & 0xFF ),
                (BYTE)( ( ( (int)( MaskNegDouble ) ) >> 8 ) & 0xFF ),
                (BYTE)( ( ( (int)( MaskNegDouble ) ) >> 16 ) & 0xFF ),
                (BYTE)( ( ( (int)( MaskNegDouble ) ) >> 24 ) & 0xFF ),
            };
            size += ApplyCustomTemplate( buffer, temp, 8 );
            //fix template for register
            buffer[-5] |= RegEncode[reg] << 3;

            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , reg);

            return size;
        }

        int Neg_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int rightOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            rightOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            if (!templateData->FindRegWithStackOffset<float>(reg, rightOffset))
            {
                reg = templateData->GetReg<float>();
                size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg, RegEBP, rightOffset));
            }

            static const BYTE temp[] = {
                0x0F, 0x57, 0x05,
                (BYTE)(((int)(JavascriptNumber::MaskNegFloat)) & 0xFF),
                (BYTE)((((int)(JavascriptNumber::MaskNegFloat)) >> 8) & 0xFF),
                (BYTE)((((int)(JavascriptNumber::MaskNegFloat)) >> 16) & 0xFF),
                (BYTE)((((int)(JavascriptNumber::MaskNegFloat)) >> 24) & 0xFF),
            };
            size += ApplyCustomTemplate(buffer, temp, 7);
            //fix template for register
            buffer[-5] |= RegEncode[reg] << 3;


            size += EncodingHelpers::SetStackReg<float>(buffer, templateData, targetOffset, reg);

            return size;
        }

        int Call_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer, int nbOffsets, int* offsets, void* addr, bool addEsp )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            *offsets -= templateData->GetBaseOffSet();

            Assert( nbOffsets >= 1 );
            const int nbArgs = nbOffsets - 1;
            const int targetOffset = offsets[0];
            int* args = offsets + 1;
            int stackSize = nbArgs << 3;
            Assert( stackSize > nbArgs ); // check for overflow

            if( nbArgs > 0 )
            {
                RegNum reg = templateData->GetReg<double>();
                if( FitsInByte( stackSize ) )
                {
                    size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, (int8)( stackSize ) ) );
                }
                else
                {
                    size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegESP, stackSize ) );
                }

                int espOffset = stackSize - 8;
                for( int i = nbArgs - 1; i >= 0; i-- )
                {
                    // TODO: check for reg in template
                    int argOffset = args[i] - templateData->GetBaseOffSet();
                    size += MOVSD::EncodeInstruction<double>(buffer, InstrParamsRegAddr(reg, RegEBP, argOffset));
                    size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg( RegESP, espOffset, reg ) );
                    espOffset -= 8;
                }
                templateData->InvalidateReg( reg );
            }

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>(RegEAX,(int)addr) );
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg(RegEAX) );

            templateData->InvalidateAllVolatileReg();
            size += FSTP::EncodeInstruction<double>( buffer, InstrParamsAddr( RegEBP, targetOffset ) );
            templateData->InvalidateReg( RegEAX );
            templateData->OverwriteStack( targetOffset );

            if( addEsp )
            {
                if( FitsInByte( stackSize ) )
                {
                    size += ADD::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, (int8)stackSize ) );
                }
                else
                {
                    size += ADD::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegESP, stackSize ) );
                }
            }

            return size;
        }

        int Call_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int nbOffsets, int* offsets, void* addr, bool addEsp)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            *offsets -= templateData->GetBaseOffSet();

            Assert(nbOffsets >= 1);
            const int nbArgs = nbOffsets - 1;
            const int targetOffset = offsets[0];
            int* args = offsets + 1;
            // REVIEW: 4 bytes per arg for floats, do we want to maintain 8 byte stack alignment?
            int stackSize = nbArgs << 2;
            Assert(stackSize > nbArgs); // check for overflow

            if (nbArgs > 0)
            {
                RegNum reg = templateData->GetReg<float>();
                if (FitsInByte(stackSize))
                {
                    size += SUB::EncodeInstruction<int>(buffer, InstrParamsRegImm<int8>(RegESP, (int8)(stackSize)));
                }
                else
                {
                    size += SUB::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegESP, stackSize));
                }

                int espOffset = stackSize - 4;
                for (int i = nbArgs - 1; i >= 0; i--)
                {
                    // TODO: check for reg in template
                    int argOffset = args[i] - templateData->GetBaseOffSet();
                    size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(reg, RegEBP, argOffset));
                    size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(RegESP, espOffset, reg));
                    espOffset -= 4;
                }
                templateData->InvalidateReg(reg);
            }

            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, (int)addr));
            size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(RegEAX));

            templateData->InvalidateAllVolatileReg();
            size += FSTP::EncodeInstruction<float>(buffer, InstrParamsAddr(RegEBP, targetOffset));
            templateData->InvalidateReg(RegEAX);
            templateData->OverwriteStack(targetOffset);

            if (addEsp)
            {
                if (FitsInByte(stackSize))
                {
                    size += ADD::EncodeInstruction<int>(buffer, InstrParamsRegImm<int8>(RegESP, (int8)stackSize));
                }
                else
                {
                    size += ADD::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegESP, stackSize));
                }
            }

            return size;
        }
        int StartCall::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int argBytesSize )
        {
            int size = 0;
            // remove extra var from sub because we are using push to add it
            argBytesSize -= sizeof(Var);
            if( FitsInByte( argBytesSize ) )
            {
                size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, (int8)argBytesSize ) );
            }
            else
            {
                size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegESP, argBytesSize ) );
            }
            // pushing undefined as the first var
            const int undefinedVar = (int)context->GetFunctionBody()->GetScriptContext()->GetLibrary()->GetUndefined();
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsImm<int32>(undefinedVar) );

            return size;
        }
        int ArgOut_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int argIndex, int offset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            offset -= templateData->GetBaseOffSet();

            RegNum regScriptContext, regVariable;
            if( !templateData->FindRegWithStackOffset<int>( regScriptContext, templateData->GetScriptContextOffset() ) )
            {
                regScriptContext = templateData->GetReg<int>(1<<RegEAX);
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(regScriptContext, RegEBP, templateData->GetScriptContextOffset()));
                templateData->SetStackInfo(regScriptContext, templateData->GetScriptContextOffset());
            }
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( regScriptContext ) );

            if( !templateData->FindRegWithStackOffset<int>( regVariable, offset ) )
            {
                regVariable = templateData->GetReg<int>(1<<RegEAX);
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( regVariable, RegEBP, offset ) );
                templateData->SetStackInfo( regVariable, offset );
            }
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( regVariable ) );

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegEAX, (int32)(Var(*)(int,ScriptContext*))JavascriptNumber::ToVar) );
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg( RegEAX ) );

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsAddrReg( RegESP, argIndex << 2, RegEAX ) );

            templateData->InvalidateAllVolatileReg();
            return size;
        }
        int ArgOut_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int argIndex, int offset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            offset -= templateData->GetBaseOffSet();

            RegNum regScriptContext = EncodingHelpers::GetScriptContextRegister( buffer, context, size, 1 << RegEAX ), regVariable;
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( regScriptContext ) );

            if( !templateData->FindRegWithStackOffset<double>( regVariable, offset ) )
            {
                regVariable = templateData->GetReg<double>();
                size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegAddr( regVariable, RegEBP, offset ) );
                templateData->SetStackInfo( regVariable, offset );
            }
            size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, 8 ) );
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg( RegESP, 0, regVariable ) );

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegEAX, (int32)(Var(*)(double,ScriptContext*))JavascriptNumber::New) );
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg( RegEAX ) );

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsAddrReg( RegESP, argIndex << 2, RegEAX ) );

            templateData->InvalidateAllVolatileReg();

            return size;
        }

        int Call::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset, int funcOffset, int nbArgs )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            funcOffset -= templateData->GetBaseOffSet();

            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( RegESP ) );
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( (int8)nbArgs ) );
            RegNum reg;
            if( !templateData->FindRegWithStackOffset<int>( reg, funcOffset ) )
            {
                size += PUSH::EncodeInstruction<int>( buffer, InstrParamsAddr( RegEBP, funcOffset ) );
            }
            else
            {
                size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( reg ) );
            }

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegEAX, (int32)( Var( *)( JavascriptFunction*, int, Var* ) )ExternalCallHelper ));
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg( RegEAX ) );
            const int stackSize = nbArgs << 2;
            Assert( FitsInByte( stackSize ) );
            size += ADD::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, (int8)stackSize ) );
            templateData->InvalidateAllVolatileReg();
            size += EncodingHelpers::ReloadArrayBuffer(context, buffer);
            size += EncodingHelpers::CheckForArrayBufferDetached(context, buffer);
            if (targetOffset != templateData->GetModuleSlotOffset())
            {
                size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEAX);
            }
            templateData->SetStackInfo( RegEAX, targetOffset );

            return size;
        }

        int Conv_VTI::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset, int srcOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            srcOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            if( !templateData->FindRegWithStackOffset<int>( reg, srcOffset ) )
            {
                reg = templateData->GetReg<int>();
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg, RegEBP, srcOffset ) );
                templateData->SetStackInfo( reg, srcOffset );
            }
            RegNum regScriptContext = EncodingHelpers::GetScriptContextRegister( buffer, context, size, 1 << reg );
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( regScriptContext ) );
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( reg ) );

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegEAX, (int32)(int32(*)(Var,ScriptContext*))JavascriptMath::ToInt32) );
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg( RegEAX ) );

            templateData->InvalidateAllVolatileReg();
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEAX);

            return size;
        }
        int Conv_VTD::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset, int srcOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            srcOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            if( !templateData->FindRegWithStackOffset<int>( reg, srcOffset ) )
            {
                reg = templateData->GetReg<int>();
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg, RegEBP, srcOffset ) );
                templateData->SetStackInfo( reg, srcOffset );
            }
            RegNum regScriptContext = EncodingHelpers::GetScriptContextRegister( buffer, context, size, 1 << reg );
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( regScriptContext ) );
            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg( reg ) );

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegEAX, (int32)(double(*)(Var,ScriptContext*))JavascriptConversion::ToNumber) );
            size += CALL::EncodeInstruction<int>( buffer, InstrParamsReg( RegEAX ) );

            templateData->InvalidateAllVolatileReg();
            size += FSTP::EncodeInstruction<double>( buffer, InstrParamsAddr( RegEBP, targetOffset ) );
            templateData->OverwriteStack( targetOffset );
            return size;
        }
        //TODO - consider changing this to template (Conv_vtd and Conv_vtf)
        int Conv_VTF::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int srcOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            srcOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            if (!templateData->FindRegWithStackOffset<int>(reg, srcOffset))
            {
                reg = templateData->GetReg<int>();
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(reg, RegEBP, srcOffset));
                templateData->SetStackInfo(reg, srcOffset);
            }
            RegNum regScriptContext = EncodingHelpers::GetScriptContextRegister(buffer, context, size, 1 << reg);
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(regScriptContext));
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(reg));

            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, (int32)(float(*)(Var, ScriptContext*))JavascriptConversion::ToNumber));
            size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(RegEAX));

            templateData->InvalidateAllVolatileReg();
            size += FSTP::EncodeInstruction<float>(buffer, InstrParamsAddr(RegEBP, targetOffset));
            templateData->OverwriteStack(targetOffset);
            return size;
        }

        int I_StartCall::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int argBytesSize )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;

            templateData->StartInternalCall(argBytesSize);
            argBytesSize = ::Math::Align<int32>(argBytesSize - MachPtr, 8);
            if( FitsInByte( argBytesSize ) )
            {
                size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int8>( RegESP, (int8)argBytesSize ) );
            }
            else
            {
                size += SUB::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( RegESP, argBytesSize ) );
            }
            return size;
        }
        int I_ArgOut_Int::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int argIndex, int offset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            offset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetStackReg<int>( buffer, templateData, offset, size );
            InternalCallInfo* callInfo = templateData->GetInternalCallInfo();
            Assert( callInfo->nextArgIndex == argIndex );

            size += MOV::EncodeInstruction<int>( buffer, InstrParamsAddrReg( RegESP, callInfo->currentOffset, reg ) );
            callInfo->currentOffset += sizeof( int );
            ++callInfo->nextArgIndex;

            return size;
        }
        int I_ArgOut_Db::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int argIndex, int offset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            offset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetStackReg<double>( buffer, templateData, offset, size );
            InternalCallInfo* callInfo = templateData->GetInternalCallInfo();
            Assert( callInfo->nextArgIndex == argIndex );

            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsAddrReg( RegESP, callInfo->currentOffset, reg ) );
            callInfo->currentOffset += sizeof( double );
            callInfo->nextArgIndex += sizeof(double)/sizeof(Var);

            return size;
        }

        int I_ArgOut_Flt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int argIndex, int offset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            offset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetStackReg<float>(buffer, templateData, offset, size);
            InternalCallInfo* callInfo = templateData->GetInternalCallInfo();
            Assert(callInfo->nextArgIndex == argIndex);

            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(RegESP, callInfo->currentOffset, reg));
            callInfo->currentOffset += sizeof(float);
            ++callInfo->nextArgIndex;

            return size;
        }

        int I_Call::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int funcOffset, int nbArgs, AsmJsRetType retType)
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            funcOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetStackReg<int>( buffer, templateData, funcOffset, size );

            size += PUSH::EncodeInstruction<int>( buffer, InstrParamsReg(reg));

            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(RegEAX, reg, RecyclableObject::GetTypeOffset()));

            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(RegEAX, RegEAX, ScriptFunctionType::GetEntryPointInfoOffset()));

            //GetAddressOffset  from entrypointinfo
            size += CALL::EncodeInstruction<int>(buffer, InstrParamsAddr(RegEAX, ProxyEntryPointInfo::GetAddressOffset()));

            templateData->InvalidateAllVolatileReg();

            templateData->InternalCallDone();

            size += EncodingHelpers::ReloadArrayBuffer(context, buffer);
            return size;
        }
        int AsmJsLoopBody::ApplyTemplate(TemplateContext context, BYTE*& buffer, int loopNumber)
        {
            int size = 0;
            X86TemplateData* templateData = GetTemplateData(context);
            AsmJsFunctionInfo* funcInfo = context->GetFunctionBody()->GetAsmJsFunctionInfo();
            LoopHeader* loopHeader = context->GetFunctionBody()->GetLoopHeader(loopNumber);

            Var LoopEntryPoint = (LoopHeader(*)(Js::FunctionBody*, Var, uint32))DoLoopBodyStart;
            int offsetCorrection = templateData->GetEBPOffsetCorrection() - templateData->GetBaseOffSet(); // no EBP correction is needed here as the offset is not coming from bytecode
            int intOffset = funcInfo->GetIntByteOffset() + offsetCorrection;
            int floatOffset = funcInfo->GetFloatByteOffset() + offsetCorrection;
            int doubleOffset = funcInfo->GetDoubleByteOffset() + offsetCorrection;
            // Increment the loop count(TJCount) , reusing interpretcount in the loopHeader
            RegNum regInc = templateData->GetReg<int>(0);
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(regInc, (int32)loopHeader));
            size += INC::EncodeInstruction<int>(buffer, InstrParamsAddr(regInc, loopHeader->GetOffsetOfInterpretCount()));
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(regInc, regInc,loopHeader->GetOffsetOfInterpretCount()));

            // Compare  InterpretCount(TJCount) with the threshold set for LIC and if it is less then do schedule for JitLoopBody
            size += CMP::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(regInc, context->GetFunctionBody()->GetLoopInterpretCount(loopHeader)));
            // Jmp $LabelCount in case count is not equal to the threshold
            JumpRelocation relocLabelCount;
            EncodingHelpers::EncodeShortJump<JL>(buffer, relocLabelCount, &size);

            // If the loop is hot, Push the current EBP and loopNumber on the stack along with the function object
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, loopNumber));
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(RegEAX));
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsReg(RegEBP));
            size += PUSH::EncodeInstruction<int>(buffer, InstrParamsAddr(RegEBP, 2 * sizeof(Var)));
            // Call DoLoopBodyStart
            size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, (int32)LoopEntryPoint));
            size += CALL::EncodeInstruction<int>(buffer, InstrParamsReg(RegEAX));

            // invalidate all the volatile reg's as it is a return from a function call
            templateData->InvalidateAllVolatileReg();
            // Check the return value in EAX, this is the bytecode offset, if it is zero then loopBody is not yet jitted and we need to continue with TJ
            // Else Jump to the offset location stored in EAX
            size += CMP::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(RegEAX, 0));
            JumpRelocation relocLabel1;
            EncodingHelpers::EncodeShortJump<JE>(buffer, relocLabel1, &size);

            // reload the array buffer after JIT loop body
            size += EncodingHelpers::ReloadArrayBuffer(context, buffer);

            // Before we jump, move the result to EAX in case we return from there
            size += MOV::EncodeInstruction<int>(buffer, InstrParams2Reg(RegECX, RegEAX));
            templateData->InvalidateReg(RegECX);
            //get the output in the right register
            Js::AsmJsRetType retType = funcInfo->GetReturnType();
            switch (retType.which())
            {
            case Js::AsmJsRetType::Signed:
            case Js::AsmJsRetType::Void:
                size += MOV::EncodeInstruction<int>(buffer, InstrParamsRegAddr(RegEAX, RegEBP, intOffset));
                templateData->InvalidateReg(RegEAX);
                break;
            case Js::AsmJsRetType::Float:
                size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsRegAddr(RegXMM0, RegEBP, floatOffset));
                templateData->InvalidateReg(RegXMM0);
                break;
            case Js::AsmJsRetType::Double:
                size += MOVSD::EncodeInstruction<double>(buffer, InstrParamsRegAddr(RegXMM0, RegEBP, doubleOffset));
                templateData->InvalidateReg(RegXMM0);
                break;
            default:
                Assume(UNREACHED);
            }

            // Jump to the offset
            size += JMP::EncodeInstruction<int>(buffer, InstrParamsReg(RegECX));
            // Label1:
            relocLabel1.ApplyReloc<int8>();

            //$LabelCount:
            relocLabelCount.ApplyReloc<int8>();

            return size;
        }

        int I_Conv_VTI::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset, int srcOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            srcOffset -= templateData->GetBaseOffSet();

            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , RegEAX);

            return size;
        }
        int I_Conv_VTD::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset, int srcOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            srcOffset -= templateData->GetBaseOffSet();

            size += MOVSD::EncodeInstruction<double>(buffer, InstrParamsAddrReg(RegEBP, targetOffset,RegXMM0));
            templateData->OverwriteStack( targetOffset );

            return size;
        }

        int I_Conv_VTF::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int srcOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            srcOffset -= templateData->GetBaseOffSet();

            size += MOVSS::EncodeInstruction<float>(buffer, InstrParamsAddrReg(RegEBP, targetOffset, RegXMM0));
            templateData->OverwriteStack(targetOffset);

            return size;
        }

        int LdUndef::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            const int undefinedVar = (int)context->GetFunctionBody()->GetScriptContext()->GetLibrary()->GetUndefined();
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>(RegEAX,undefinedVar) );
            templateData->InvalidateReg( RegEAX );
            return size;
        }

        int LdArr_Func::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset, int arrOffset, int slotVarIndexOffset )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            arrOffset -= templateData->GetBaseOffSet();
            slotVarIndexOffset -= templateData->GetBaseOffSet();

            RegNum regArr, regIndex;
            if( !templateData->FindRegWithStackOffset<int>( regArr, arrOffset ) )
            {
                regArr = templateData->GetReg<int>();
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( regArr, RegEBP, arrOffset ) );
                templateData->SetStackInfo( regArr, arrOffset );
            }

            if( !templateData->FindRegWithStackOffset<int>(regIndex,slotVarIndexOffset) )
            {
                regIndex = templateData->GetReg<int>( 1 << regArr );
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( regIndex, RegEBP, slotVarIndexOffset ) );
                templateData->SetStackInfo( regIndex, slotVarIndexOffset );
            }

            // optimization because this value will be read only once right after this bytecode
            RegNum targetReg = targetOffset == templateData->GetModuleSlotOffset() ? RegEAX : templateData->GetReg<int>();
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( targetReg, regArr, regIndex, 4, 0 ) );

            if (targetOffset == templateData->GetModuleSlotOffset())
            {
                templateData->OverwriteStack( targetOffset );
                templateData->SetStackInfo( RegEAX, targetOffset );
            }
            else
            {
                size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , targetReg);
            }

            return size;
        }

        int LdSlot::ApplyTemplate( TemplateContext context, BYTE*& buffer,  int targetOffset, int arrOffset, int slotIndex )
        {
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            arrOffset -= templateData->GetBaseOffSet();

            RegNum reg;
            if( !templateData->FindRegWithStackOffset<int>( reg, arrOffset ) )
            {
                reg = templateData->GetReg<int>();
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( reg, RegEBP, arrOffset ) );
                templateData->SetStackInfo( reg, arrOffset );
            }
            if (targetOffset == templateData->GetModuleSlotOffset())
            {
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( RegEAX, reg, slotIndex*sizeof(Var) ) );
                templateData->OverwriteStack( targetOffset );
                templateData->SetStackInfo( RegEAX, targetOffset );
            }
            else
            {
                RegNum targetReg = templateData->GetReg<int>(1<<reg);
                size += MOV::EncodeInstruction<int>( buffer, InstrParamsRegAddr( targetReg, reg, slotIndex*sizeof(Var) ) );
                size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , targetReg);
            }

            return size;
        }

        typedef int( *MovEncodingFunc )( BYTE*&, const InstrParamsRegAddr&, EncodingInfo* info );
        static const MovEncodingFunc ldArrMovEncodingFunc[] = {
             MOVSX::EncodeInstruction<int8>//TYPE_INT8 = 0,
            ,MOVZX::EncodeInstruction<int8>//TYPE_UINT8,
            ,MOVSX::EncodeInstruction<int16>//TYPE_INT16,
            ,MOVZX::EncodeInstruction<int16>//TYPE_UINT16,
            ,MOV::EncodeInstruction<int>//TYPE_INT32,
            ,MOV::EncodeInstruction<int>//TYPE_UINT32,
            ,MOVSS::EncodeInstruction<float>//TYPE_FLOAT32,
            ,MOVSD::EncodeInstruction<double>//TYPE_FLOAT64,
        };

        typedef int( *StArrMovEncodingFunc )( BYTE*&, const InstrParamsAddrReg&, EncodingInfo* info );
        static const StArrMovEncodingFunc stArrMovEncodingFunc[] = {
             MOV::EncodeInstruction<int8>//TYPE_INT8 = 0,
            ,MOV::EncodeInstruction<int8>//TYPE_UINT8,
            ,MOV::EncodeInstruction<int16>//TYPE_INT16,
            ,MOV::EncodeInstruction<int16>//TYPE_UINT16,
            ,MOV::EncodeInstruction<int>//TYPE_INT32,
            ,MOV::EncodeInstruction<int>//TYPE_UINT32,
            ,MOVSS::EncodeInstruction<float>//TYPE_FLOAT32,
            ,MOVSD::EncodeInstruction<double>//TYPE_FLOAT64,
        };

        static const uint32 TypedArrayViewMask[] =
        {
             (uint32)~0 //TYPE_INT8
            ,(uint32)~0 //TYPE_UINT8
            ,(uint32)~1 //TYPE_INT16
            ,(uint32)~1 //TYPE_UINT16
            ,(uint32)~3 //TYPE_INT32
            ,(uint32)~3 //TYPE_UINT32
            ,(uint32)~3 //TYPE_FLOAT32
            ,(uint32)~7 //TYPE_FLOAT64
        };

        int LdArrDb::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int slotVarIndex, ArrayBufferView::ViewType viewType )
        {
            AnalysisAssert(viewType == ArrayBufferView::TYPE_FLOAT64);
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            slotVarIndex -= templateData->GetBaseOffSet();

            RegNum regIndex = EncodingHelpers::GetStackReg<int>( buffer, templateData, slotVarIndex, size );
            RegNum resultReg = templateData->GetReg<double>();
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister( buffer, context, size, 1 << regIndex );
            size += AND::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( regIndex, TypedArrayViewMask[viewType] ) );
            templateData->InvalidateReg( regIndex );
            size += EncodingHelpers::CompareRegisterToArrayLength( buffer, context, regIndex );

            // Jump to load value
            JumpRelocation reloc( buffer, &size );
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc.JumpEncoded( info );

            size += ldArrMovEncodingFunc[viewType]( buffer, InstrParamsRegAddr( resultReg, regArrayBuffer, regIndex, 1, 0 ), nullptr );
            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , resultReg);

            // Jump to load default value
            JumpRelocation reloc2( buffer, &size );
            size += JMP::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc2.JumpEncoded( info );

            reloc.ApplyReloc<int8>();
            int* nanAddr = (int*)&NumberConstants::k_Nan;
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegPtr( resultReg, (void*)nanAddr ) );
            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , resultReg);

            reloc2.ApplyReloc<int8>();

            return size;
        }

        int LdArrFlt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int slotVarIndex, ArrayBufferView::ViewType viewType)
        {
            AnalysisAssert(viewType == ArrayBufferView::TYPE_FLOAT32);
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            slotVarIndex -= templateData->GetBaseOffSet();

            RegNum regIndex = EncodingHelpers::GetStackReg<int>(buffer, templateData, slotVarIndex, size);
            RegNum resultReg = templateData->GetReg<float>();

            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister(buffer, context, size, 1 << regIndex);
            size += AND::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(regIndex, TypedArrayViewMask[viewType]));
            templateData->InvalidateReg(regIndex);
            size += EncodingHelpers::CompareRegisterToArrayLength(buffer, context, regIndex);

            // Jump to load value
            JumpRelocation reloc(buffer, &size);
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>(buffer, InstrParamsImm<int8>(0), &info);
            reloc.JumpEncoded(info);

            size += ldArrMovEncodingFunc[viewType](buffer, InstrParamsRegAddr(resultReg, regArrayBuffer, regIndex, 1, 0), nullptr);
            size += EncodingHelpers::SetStackReg<float>(buffer, templateData, targetOffset, resultReg);

            // Jump to load default value
            JumpRelocation reloc2(buffer, &size);
            size += JMP::EncodeInstruction<int>(buffer, InstrParamsImm<int8>(0), &info);
            reloc2.JumpEncoded(info);

            reloc.ApplyReloc<int8>();
            int* nanAddr = (int*)&NumberConstants::k_Nan;
            size += MOVSD::EncodeInstruction<double>(buffer, InstrParamsRegPtr(resultReg, (void*)nanAddr));
            size += CVTSD2SS::EncodeInstruction<double>(buffer, InstrParams2Reg(resultReg, resultReg));
            size += EncodingHelpers::SetStackReg<float>(buffer, templateData, targetOffset, resultReg);

            reloc2.ApplyReloc<int8>();

            return size;
        }

        int LdArr::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int slotVarIndex, ArrayBufferView::ViewType viewType )
        {
            AnalysisAssert(viewType >= ArrayBufferView::TYPE_INT8 && viewType < ArrayBufferView::TYPE_FLOAT32);
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();
            slotVarIndex -= templateData->GetBaseOffSet();

            RegNum regIndex = EncodingHelpers::GetStackReg<int>( buffer, templateData, slotVarIndex, size );
            RegNum resultReg = templateData->GetReg<int>( 1 << regIndex );
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister( buffer, context, size, 1 << regIndex | 1 << resultReg );
            if( viewType != ArrayBufferView::TYPE_INT8 && viewType != ArrayBufferView::TYPE_UINT8 )
            {
                size += AND::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( regIndex, TypedArrayViewMask[viewType] ) );
                templateData->InvalidateReg( regIndex );
            }
            size += EncodingHelpers::CompareRegisterToArrayLength( buffer, context, regIndex );
            // Jump to load value
            JumpRelocation reloc( buffer, &size );
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc.JumpEncoded( info );

            size += ldArrMovEncodingFunc[viewType]( buffer, InstrParamsRegAddr( resultReg, regArrayBuffer, regIndex, 1, 0 ), nullptr );
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , resultReg);

            // Jump to load default value
            JumpRelocation reloc2( buffer, &size );
            size += JMP::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc2.JumpEncoded( info );

            reloc.ApplyReloc<int8>();
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsAddrImm<int32>( RegEBP, targetOffset, 0 ) );
            // load the value into a register now since it will most likely be used very soon + avoids discrepancies in templateData between the 2 jumps
            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( resultReg, resultReg) );
            reloc2.ApplyReloc<int8>();

            return size;
        }

        int StArrDb::ApplyTemplate( TemplateContext context, BYTE*& buffer, int srcOffset, int slotVarIndex, ArrayBufferView::ViewType viewType )
        {
            AnalysisAssert(viewType == ArrayBufferView::TYPE_FLOAT64);
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();
            slotVarIndex -= templateData->GetBaseOffSet();

            RegNum regIndex = EncodingHelpers::GetStackReg<int>( buffer, templateData, slotVarIndex, size );
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister( buffer, context, size, 1 << regIndex );

            size += AND::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( regIndex, TypedArrayViewMask[viewType] ) );
            templateData->InvalidateReg( regIndex );
            size += EncodingHelpers::CompareRegisterToArrayLength( buffer, context, regIndex );
            // Jump to load value
            JumpRelocation reloc( buffer, &size );
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc.JumpEncoded( info );

            RegNum regVal;
            regVal = EncodingHelpers::GetStackReg<double>( buffer, templateData, srcOffset, size );

            size += stArrMovEncodingFunc[viewType]( buffer, InstrParamsAddrReg( regArrayBuffer, regIndex, 1, 0, regVal ), nullptr );
            // do nothing if index is out of range
            reloc.ApplyReloc<int8>();

            return size;
        }

        int StArrFlt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffset, int slotVarIndex, ArrayBufferView::ViewType viewType)
        {
            AnalysisAssert(viewType == ArrayBufferView::TYPE_FLOAT32);
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();
            slotVarIndex -= templateData->GetBaseOffSet();

            RegNum regIndex = EncodingHelpers::GetStackReg<int>(buffer, templateData, slotVarIndex, size);
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister(buffer, context, size, 1 << regIndex);

            size += AND::EncodeInstruction<int>(buffer, InstrParamsRegImm<int32>(regIndex, TypedArrayViewMask[viewType]));
            templateData->InvalidateReg(regIndex);
            size += EncodingHelpers::CompareRegisterToArrayLength(buffer, context, regIndex);
            // Jump to load value
            JumpRelocation reloc(buffer, &size);
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>(buffer, InstrParamsImm<int8>(0), &info);
            reloc.JumpEncoded(info);

            RegNum regVal;
            regVal = EncodingHelpers::GetStackReg<float>(buffer, templateData, srcOffset, size);
            size += stArrMovEncodingFunc[viewType](buffer, InstrParamsAddrReg(regArrayBuffer, regIndex, 1, 0, regVal), nullptr);
            // do nothing if index is out of range
            reloc.ApplyReloc<int8>();

            return size;
        }

        int StArr::ApplyTemplate( TemplateContext context, BYTE*& buffer, int srcOffset, int slotVarIndex, ArrayBufferView::ViewType viewType )
        {
            AnalysisAssert(viewType >= ArrayBufferView::TYPE_INT8 && viewType < ArrayBufferView::TYPE_FLOAT32);
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();
            slotVarIndex -= templateData->GetBaseOffSet();

            RegNum regIndex = EncodingHelpers::GetStackReg<int>( buffer, templateData, slotVarIndex, size );
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister( buffer, context, size, 1 << regIndex );
            if( viewType != ArrayBufferView::TYPE_INT8 && viewType != ArrayBufferView::TYPE_UINT8 )
            {
                size += AND::EncodeInstruction<int>( buffer, InstrParamsRegImm<int32>( regIndex, TypedArrayViewMask[viewType] ) );
                templateData->InvalidateReg( regIndex );
            }
            size += EncodingHelpers::CompareRegisterToArrayLength( buffer, context, regIndex );
            // Jump to load value
            JumpRelocation reloc( buffer, &size );
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc.JumpEncoded( info );

            int extraRestriction = viewType == ArrayBufferView::TYPE_INT8 || viewType == ArrayBufferView::TYPE_UINT8 ? ~Mask8BitsReg : 0;
            RegNum regVal = EncodingHelpers::GetStackReg<int>( buffer, templateData, srcOffset, size, 1 << regIndex | extraRestriction | 1 << regArrayBuffer );
            size += stArrMovEncodingFunc[viewType]( buffer, InstrParamsAddrReg( regArrayBuffer, regIndex, 1, 0, regVal ), nullptr );

            // do nothing if index is out of range
            reloc.ApplyReloc<int8>();

            return size;
        }

        // Version with const index
        int ConstLdArrDb::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int constIndex, ArrayBufferView::ViewType viewType )
        {
            AnalysisAssert(viewType == ArrayBufferView::TYPE_FLOAT64);
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum resultReg = templateData->GetReg<double>();
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister( buffer, context, size );
            size += EncodingHelpers::CompareImmutableToArrayLength<int32>( buffer, context, constIndex );

            // Jump to load value
            JumpRelocation reloc( buffer, &size );
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc.JumpEncoded( info );

            size += ldArrMovEncodingFunc[viewType]( buffer, InstrParamsRegAddr( resultReg, regArrayBuffer, constIndex ), nullptr );
            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , resultReg);

            // Jump to load default value
            JumpRelocation reloc2( buffer, &size );
            size += JMP::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc2.JumpEncoded( info );

            reloc.ApplyReloc<int8>();
            int* nanAddr = (int*)&NumberConstants::k_Nan;
            size += MOVSD::EncodeInstruction<double>( buffer, InstrParamsRegPtr( resultReg, (void*)nanAddr ) );
            size += EncodingHelpers::SetStackReg<double>( buffer, templateData, targetOffset , resultReg);

            reloc2.ApplyReloc<int8>();

            return size;
        }

        // Version with const index
        int ConstLdArrFlt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int constIndex, ArrayBufferView::ViewType viewType)
        {
            AnalysisAssert(viewType == ArrayBufferView::TYPE_FLOAT32);
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum resultReg = templateData->GetReg<float>();
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister(buffer, context, size);
            size += EncodingHelpers::CompareImmutableToArrayLength<int32>(buffer, context, constIndex);

            // Jump to load value
            JumpRelocation reloc(buffer, &size);
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>(buffer, InstrParamsImm<int8>(0), &info);
            reloc.JumpEncoded(info);

            size += ldArrMovEncodingFunc[viewType](buffer, InstrParamsRegAddr(resultReg, regArrayBuffer, constIndex), nullptr);
            size += EncodingHelpers::SetStackReg<float>(buffer, templateData, targetOffset, resultReg);

            // Jump to load default value
            JumpRelocation reloc2(buffer, &size);
            size += JMP::EncodeInstruction<int>(buffer, InstrParamsImm<int8>(0), &info);
            reloc2.JumpEncoded(info);

            reloc.ApplyReloc<int8>();
            int* nanAddr = (int*)&NumberConstants::k_Nan;
            size += MOVSD::EncodeInstruction<double>(buffer, InstrParamsRegPtr(resultReg, (void*)nanAddr));
            size += CVTSD2SS::EncodeInstruction<double>(buffer, InstrParams2Reg(resultReg, resultReg));
            size += EncodingHelpers::SetStackReg<float>(buffer, templateData, targetOffset, resultReg);

            reloc2.ApplyReloc<int8>();

            return size;
        }

        int ConstLdArr::ApplyTemplate( TemplateContext context, BYTE*& buffer, int targetOffset, int constIndex, ArrayBufferView::ViewType viewType )
        {
            AnalysisAssert(viewType < ArrayBufferView::TYPE_FLOAT32 && viewType >= ArrayBufferView::TYPE_INT8);
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum resultReg = templateData->GetReg<int>( );
            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister( buffer, context, size, 1 << resultReg );

            size += EncodingHelpers::CompareImmutableToArrayLength<int32>( buffer, context, constIndex );
            // Jump to load value
            JumpRelocation reloc( buffer, &size );
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc.JumpEncoded( info );

            size += ldArrMovEncodingFunc[viewType]( buffer, InstrParamsRegAddr( resultReg, regArrayBuffer, constIndex ), nullptr );
            size += EncodingHelpers::SetStackReg<int>( buffer, templateData, targetOffset , resultReg);

            // Jump to load default value
            JumpRelocation reloc2( buffer, &size );
            size += JMP::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc2.JumpEncoded( info );

            reloc.ApplyReloc<int8>();
            size += MOV::EncodeInstruction<int>( buffer, InstrParamsAddrImm<int32>( RegEBP, targetOffset, 0 ) );
            // load the value into a register now since it will most likely be used very soon + avoids discrepancies in templateData between the 2 jumps
            size += XOR::EncodeInstruction<int>( buffer, InstrParams2Reg( resultReg, resultReg) );
            reloc2.ApplyReloc<int8>();

            return size;
        }

        template<typename Size>
        int ConstStArrDbOrFlt(TemplateContext context, BYTE*& buffer, int srcOffset, int constIndex, ArrayBufferView::ViewType viewType)
        {
            AnalysisAssert(viewType == ArrayBufferView::TYPE_FLOAT32 || viewType == ArrayBufferView::TYPE_FLOAT64);
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();

            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister(buffer, context, size);

            size += EncodingHelpers::CompareImmutableToArrayLength<int32>(buffer, context, constIndex);
            // Jump to load value
            JumpRelocation reloc(buffer, &size);
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>(buffer, InstrParamsImm<int8>(0), &info);
            reloc.JumpEncoded(info);

            RegNum regVal;
            regVal = EncodingHelpers::GetStackReg<Size>(buffer, templateData, srcOffset, size);

            size += stArrMovEncodingFunc[viewType](buffer, InstrParamsAddrReg(regArrayBuffer, constIndex, regVal), nullptr);
            // do nothing if index is out of range
            reloc.ApplyReloc<int8>();

            return size;
        }

        int ConstStArrDb::ApplyTemplate( TemplateContext context, BYTE*& buffer, int srcOffset, int constIndex, ArrayBufferView::ViewType viewType )
        {
            Assert(viewType == ArrayBufferView::TYPE_FLOAT64);
            return ConstStArrDbOrFlt<double>(context, buffer, srcOffset, constIndex, viewType);
        }

        int ConstStArrFlt::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffset, int constIndex, ArrayBufferView::ViewType viewType)
        {
            Assert(viewType == ArrayBufferView::TYPE_FLOAT32);
            return ConstStArrDbOrFlt<float>(context, buffer, srcOffset, constIndex, viewType);
        }

        int ConstStArr::ApplyTemplate( TemplateContext context, BYTE*& buffer, int srcOffset, int constIndex, ArrayBufferView::ViewType viewType )
        {
            AnalysisAssert(viewType < ArrayBufferView::TYPE_FLOAT32 && viewType >= ArrayBufferView::TYPE_INT8);
            X86TemplateData* templateData = GetTemplateData( context );
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();

            RegNum regArrayBuffer = EncodingHelpers::GetArrayBufferRegister( buffer, context, size );

            size += EncodingHelpers::CompareImmutableToArrayLength<int32>( buffer, context, constIndex );
            // Jump to load value
            JumpRelocation reloc( buffer, &size );
            EncodingInfo info;
            size += JBE::EncodeInstruction<int>( buffer, InstrParamsImm<int8>( 0 ), &info );
            reloc.JumpEncoded( info );

            int extraRestriction = viewType == ArrayBufferView::TYPE_INT8 || viewType == ArrayBufferView::TYPE_UINT8 ? ~Mask8BitsReg : 0;
            RegNum regVal = EncodingHelpers::GetStackReg<int>( buffer, templateData, srcOffset, size, extraRestriction | 1 << regArrayBuffer );
            size += stArrMovEncodingFunc[viewType]( buffer, InstrParamsAddrReg( regArrayBuffer, constIndex, regVal ), nullptr );

            // do nothing if index is out of range
            reloc.ApplyReloc<int8>();

            return size;
        }

        int Simd128_Ld_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4, int srcOffsetF4)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4 -= templateData->GetBaseOffSet();
            srcOffsetF4 -= templateData->GetBaseOffSet();
            if (targetOffsetF4 == srcOffsetF4)
            {
                return 0;
            }

            int size = 0;

            RegNum reg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4, size);
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4, reg);

            return size;
        }

        int Simd128_Ld_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4, int srcOffsetI4)
        {
            return Simd128_Ld_F4::ApplyTemplate(context, buffer, targetOffsetI4, srcOffsetI4);
        }

        int Simd128_Ld_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2, int srcOffsetD2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetD2 -= templateData->GetBaseOffSet();
            srcOffsetD2 -= templateData->GetBaseOffSet();
            if (targetOffsetD2 == srcOffsetD2)
            {
                return 0;
            }

            int size = 0;

            RegNum reg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetD2, size);
            size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffsetD2, reg);

            return size;
        }

        int Simd128_LdSlot_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int slotIndex)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            targetOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister(buffer, context, size);
            RegNum reg2 = templateData->GetReg<AsmJsSIMDValue>(1 << reg);
            size += MOVUPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegAddr(reg2, reg, slotIndex*sizeof(AsmJsSIMDValue)));
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffset, reg2);

            return size;
        }

        int Simd128_LdSlot_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int slotIndex)
        {
            return Simd128_LdSlot_F4::ApplyTemplate(context, buffer, targetOffset, slotIndex);
        }

        int Simd128_LdSlot_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int slotIndex)
        {
            return Simd128_LdSlot_F4::ApplyTemplate(context, buffer, targetOffset, slotIndex);
        }

        int Simd128_StSlot_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffset, int slotIndex)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            srcOffset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetModuleEnvironmentRegister(buffer, context, size);
            RegNum reg2 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffset, size);

            size += MOVUPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsAddrReg(reg, slotIndex*sizeof(AsmJsSIMDValue), reg2));

            return size;
        }

        int Simd128_StSlot_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffset, int slotIndex)
        {
            return Simd128_StSlot_F4::ApplyTemplate(context, buffer, srcOffset, slotIndex);
        }

        int Simd128_StSlot_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffset, int slotIndex)
        {
            return Simd128_StSlot_F4::ApplyTemplate(context, buffer, srcOffset, slotIndex);
        }

        int Simd128_FloatsToF4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF1, int srcOffsetF2, int srcOffsetF3, int srcOffsetF4)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDInitFromPrimitives<float>(buffer, templateData, targetOffsetF4_0, srcOffsetF1, srcOffsetF2, srcOffsetF3, srcOffsetF4);
        }

        int Simd128_IntsToI4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI1, int srcOffsetI2, int srcOffsetI3, int srcOffsetI4)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDInitFromPrimitives<int>(buffer, templateData, targetOffsetI4_0, srcOffsetI1, srcOffsetI2, srcOffsetI3, srcOffsetI4);
        }

        int Simd128_DoublesToD2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD1, int srcOffsetD2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDInitFromPrimitives<double>(buffer, templateData, targetOffsetD2_0, srcOffsetD1, srcOffsetD2);
        }

        int Simd128_Return_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffsetF4)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            srcOffsetF4 -= templateData->GetBaseOffSet();
            RegNum reg = RegXMM0;
            if (!templateData->FindRegWithStackOffset<AsmJsSIMDValue>(reg, srcOffsetF4))
            {
                templateData->SetStackInfo(RegXMM0, srcOffsetF4);
                return MOVUPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegAddr(RegXMM0, RegEBP, srcOffsetF4));
            }
            if (reg != RegXMM0)
            {
                return MOVUPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(RegXMM0, reg));
            }
            return 0;
        }

        int Simd128_Return_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffsetI4)
        {
            return Simd128_Return_F4::ApplyTemplate(context, buffer, srcOffsetI4);
        }

        int Simd128_Return_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int srcOffsetD2)
        {
            return Simd128_Return_F4::ApplyTemplate(context, buffer, srcOffsetD2);
        }

        int Simd128_Splat_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetF1 -= templateData->GetBaseOffSet();
            int size = 0;
            RegNum reg = EncodingHelpers::GetStackReg<float>(buffer, templateData, srcOffsetF1, size);
            size += SHUFPS::EncodeInstruction<AsmJsSIMDValue, byte>(buffer, InstrParams2RegImm<byte>(reg, reg, 0x00));
            // MOVUPS
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, reg);
            return size;
        }

        int Simd128_Splat_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetI4_0 -= templateData->GetBaseOffSet();
            srcOffsetI1 -= templateData->GetBaseOffSet();

            int size = 0;
            // load as float: MOVSS XMM, [intVal]
            RegNum reg = EncodingHelpers::GetStackReg<float>(buffer, templateData, srcOffsetI1, size);

            size += PSHUFD::EncodeInstruction<AsmJsSIMDValue, byte>(buffer, InstrParams2RegImm<byte>(reg, reg, 0x00));

            // MOVUPS
            size += EncodingHelpers::SIMDSetStackReg<int>(buffer, templateData, targetOffsetI4_0, reg);
            return size;

        }

        int Simd128_Splat_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetD2_0 -= templateData->GetBaseOffSet();
            srcOffsetD1 -= templateData->GetBaseOffSet();

            int size = 0;
            // MOVSD
            RegNum reg = EncodingHelpers::GetStackReg<double>(buffer, templateData, srcOffsetD1, size);

            size += SHUFPD::EncodeInstruction<AsmJsSIMDValue, byte>(buffer, InstrParams2RegImm<byte>(reg, reg, 0x00));

            size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffsetD2_0, reg);
            return size;
        }

        // Type conversions
        int Simd128_FromFloat64x2_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetD2_1)
        {
            return EncodingHelpers::SIMDUnaryOperation<CVTPD2PS, float>(buffer, GetTemplateData(context), targetOffsetF4_0, srcOffsetD2_1);
        }
        int Simd128_FromInt32x4_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetI4_1)
        {
            return EncodingHelpers::SIMDUnaryOperation<CVTDQ2PS, float>(buffer, GetTemplateData(context), targetOffsetF4_0, srcOffsetI4_1);
        }
        int Simd128_FromFloat32x4_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetF4_1)
        {
            return EncodingHelpers::SIMDUnaryOperation<CVTTPS2DQ, int>(buffer, GetTemplateData(context), targetOffsetI4_0, srcOffsetF4_1);
        }
        int Simd128_FromFloat64x2_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetD2_1)
        {
            return EncodingHelpers::SIMDUnaryOperation<CVTTPD2DQ, int>(buffer, GetTemplateData(context), targetOffsetI4_0, srcOffsetD2_1);
        }
        int Simd128_FromFloat32x4_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetF4_1)
        {
            return EncodingHelpers::SIMDUnaryOperation<CVTPS2PD, double>(buffer, GetTemplateData(context), targetOffsetD2_0, srcOffsetF4_1);
        }
        int Simd128_FromInt32x4_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetI4_1)
        {
            return EncodingHelpers::SIMDUnaryOperation<CVTDQ2PD, float>(buffer, GetTemplateData(context), targetOffsetD2_0, srcOffsetI4_1);
        }

        // Bits conversions
        int Simd128_FromFloat64x2Bits_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetD2_1)
        {
            return Simd128_Ld_F4::ApplyTemplate(context, buffer, targetOffsetF4_0, srcOffsetD2_1);
        }
        int Simd128_FromInt32x4Bits_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetI4_1)
        {
            return Simd128_Ld_F4::ApplyTemplate(context, buffer, targetOffsetF4_0, srcOffsetI4_1);
        }
        int Simd128_FromFloat32x4Bits_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetF4_1)
        {
            return Simd128_Ld_I4::ApplyTemplate(context, buffer, targetOffsetI4_0, srcOffsetF4_1);
        }
        int Simd128_FromFloat64x2Bits_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetD2_1)
        {
            return Simd128_Ld_I4::ApplyTemplate(context, buffer, targetOffsetI4_0, srcOffsetD2_1);
        }
        int Simd128_FromFloat32x4Bits_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetF4_1)
        {
            return Simd128_Ld_D2::ApplyTemplate(context, buffer, targetOffsetD2_0, srcOffsetF4_1);
        }
        int Simd128_FromInt32x4Bits_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetI4_1)
        {
            return Simd128_Ld_D2::ApplyTemplate(context, buffer, targetOffsetD2_0, srcOffsetI4_1);
        }

        // Unary operations
        int Simd128_Abs_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetF4_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum reg1;
            // MOVUPS reg, [src]
            reg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_1, size);
            // ANDPS reg, [mask]
            size += ANDPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg1, &(X86_ABS_MASK_F4)));

            // MOVUPS [dst], reg
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, reg1);
            return size;
        }

        int Simd128_Abs_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetD2_0 -= templateData->GetBaseOffSet();
            srcOffsetD2_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum reg1;
            // MOVUPS reg, [src]
            reg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetD2_1, size);
            // ANDPS reg, [mask]
            size += ANDPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg1, &(X86_ABS_MASK_D2)));
            // MOVUPS [dst], reg
            size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffsetD2_0, reg1);
            return size;
        }

        int Simd128_Neg_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetF4_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum reg1;
            // MOVUPS reg, [src]
            reg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_1, size);
            // XORPS reg, [mask]
            size += XORPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg1, &(X86_NEG_MASK_F4)));
            // MOVUPS [dst], reg
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, reg1);
            return size;
        }

        int Simd128_Neg_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetI4_0 -= templateData->GetBaseOffSet();
            srcOffsetI4_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum reg1;
            // MOVUPS reg, [src]
            reg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetI4_1, size);
            // ANDNPS reg, [mask]
            size += ANDNPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg1, &(X86_ALL_NEG_ONES)));
            // PADDD reg, [all_ones]
            size += PADDD::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg1, &(X86_ALL_ONES_I4)));
            // MOVUPS [dst], reg
            size += EncodingHelpers::SIMDSetStackReg<int>(buffer, templateData, targetOffsetI4_0, reg1);
            return size;
        }

        int Simd128_Neg_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetD2_0 -= templateData->GetBaseOffSet();
            srcOffsetD2_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum reg1;
            // MOVUPS reg, [src]
            reg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetD2_1, size);
            // XORPS reg, [mask]
            size += XORPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg1, &(X86_NEG_MASK_D2)));

            // MOVUPS [dst], reg
            size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffsetD2_0, reg1);
            return size;
        }

        int Simd128_Rcp_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetF4_1 -= templateData->GetBaseOffSet();
            int size = 0;
            // MOVUPS srcReg, [src]
            RegNum srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_1, size);

            RegNum rcpReg = EncodingHelpers::SIMDRcpOperation<DIVPS, float>(buffer, templateData, srcReg, (void*)(&X86_ALL_ONES_F4), size);
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, rcpReg);
            return size;
        }

        int Simd128_Rcp_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetD2_0 -= templateData->GetBaseOffSet();
            srcOffsetD2_1 -= templateData->GetBaseOffSet();
            int size = 0;
            // MOVUPS reg1, [src]
            RegNum srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetD2_1, size);

            RegNum rcpReg = EncodingHelpers::SIMDRcpOperation<DIVPD, double>(buffer, templateData, srcReg, (void*)(&X86_ALL_ONES_D2), size);
            size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffsetD2_0, rcpReg);
            return size;
        }

        int Simd128_RcpSqrt_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetF4_1 -= templateData->GetBaseOffSet();
            int size = 0;
            RegNum srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_1, size);

            RegNum dstReg = EncodingHelpers::SIMDRcpOperation<DIVPS, float>(buffer, templateData, srcReg, (void*)(&X86_ALL_ONES_F4), size);

            size += SQRTPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, dstReg));

            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, dstReg);
            return size;
        }

        int Simd128_RcpSqrt_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetD2_0 -= templateData->GetBaseOffSet();
            srcOffsetD2_1 -= templateData->GetBaseOffSet();
            int size = 0;
            RegNum srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetD2_1, size);

            RegNum dstReg = EncodingHelpers::SIMDRcpOperation<DIVPD, double>(buffer, templateData, srcReg, (void*)(&X86_ALL_ONES_D2), size);

            size += SQRTPD::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, dstReg));

            size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffsetD2_0, dstReg);
            return size;
        }

        int Simd128_Sqrt_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetF4_1 -= templateData->GetBaseOffSet();
            int size = 0;

            RegNum reg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_1, size);
            size += SQRTPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(reg, reg));

            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, reg);
            return size;
        }

        int Simd128_Sqrt_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetD2_0 -= templateData->GetBaseOffSet();
            srcOffsetD2_1 -= templateData->GetBaseOffSet();
            int size = 0;

            RegNum reg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetD2_1, size);
            size += SQRTPD::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(reg, reg));

            size += EncodingHelpers::SIMDSetStackReg<double>(buffer, templateData, targetOffsetD2_0, reg);
            return size;
        }

        int Simd128_Not_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetF4_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum reg1;
            // MOVUPS reg, [src]
            reg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_1, size);
            // XORPS reg, [mask]
            size += XORPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegPtr(reg1, &(X86_ALL_NEG_ONES)));
            // MOVUPS [dst], reg
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, reg1);
            return size;
        }

        int Simd128_Not_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1)
        {
            return Simd128_Not_F4::ApplyTemplate(context, buffer, targetOffsetI4_0, srcOffsetI4_1);
        }

        int Simd128_Add_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<ADDPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2);
        }

        int Simd128_Add_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<PADDD, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI4_2);
        }

        int Simd128_Add_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<ADDPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2);
        }

        int Simd128_Sub_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<SUBPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2);
        }

        int Simd128_Sub_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<PSUBD, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI4_2);
        }

        int Simd128_Sub_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<SUBPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2);
        }

        int Simd128_Mul_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<MULPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2);
        }

        int Simd128_Mul_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            RegNum srcReg1, srcReg2, tmpReg;
            int size = 0;

            targetOffsetI4_0 -= templateData->GetBaseOffSet();
            srcOffsetI4_1 -= templateData->GetBaseOffSet();
            srcOffsetI4_2 -= templateData->GetBaseOffSet();

            // MOVUPS srcReg1, [src1]
            // MOVUPS srcReg1, [src2]
            srcReg1 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetI4_1, size);
            srcReg2 = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetI4_2, size);
            tmpReg = templateData->GetReg<AsmJsSIMDValue>((1 << srcReg1) | (1 << srcReg2));

            // MOVAPS tmpReg, srcReg1
            size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg, srcReg1));
            // PMULUDQ tmpReg, srcReg2
            size += PMULUDQ::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg, srcReg2));
            // PSRLDQ srcReg1, 0x04
            size += PSRLDQ::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegImm<byte>(srcReg1, 0x04));
            templateData->InvalidateReg(srcReg1);

            if (srcReg1 != srcReg2)
            {
                // PSRLDQ srcReg2, 0x04
                size += PSRLDQ::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsRegImm<byte>(srcReg2, 0x04));
                templateData->InvalidateReg(srcReg2);
            }

            // PMULUDQ srcReg1, srcReg2
            size += PMULUDQ::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(srcReg1, srcReg2));
            // PSHUFD tmpReg, tmpReg, b00001000
            size += PSHUFD::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2RegImm<byte>(tmpReg, tmpReg, 0x08));
            // PSHUFD srcReg1, srcReg1, b00001000
            size += PSHUFD::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2RegImm<byte>(srcReg1, srcReg1, 0x08));
            // PUNPCKLDQ srcReg1, tmpReg
            size += PUNPCKLDQ::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tmpReg, srcReg1));
            // MOVUPS [dst] srcReg1
            size += EncodingHelpers::SIMDSetStackReg<int>(buffer, templateData, targetOffsetI4_0, tmpReg);

            return size;
        }

        int Simd128_Mul_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<MULPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2);
        }

        int Simd128_Div_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<DIVPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2);
        }

        int Simd128_Div_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<DIVPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2);
        }

        int Simd128_Min_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<MINPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2);
        }

        int Simd128_Min_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<MINPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2);
        }

        int Simd128_Max_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<MAXPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2);
        }

        int Simd128_Max_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<MAXPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2);
        }

        // comparison
        int Simd128_Lt_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2, CMP_IMM8::LT);
        }
        int Simd128_Lt_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<PCMPGTD, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_2, srcOffsetI4_1);
        }
        int Simd128_Lt_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2, CMP_IMM8::LT);
        }

        int Simd128_Gt_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<CMPPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_2, srcOffsetF4_1, CMP_IMM8::LT);
        }
        int Simd128_Gt_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<PCMPGTD, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI4_2);
        }
        int Simd128_Gt_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<CMPPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_2, srcOffsetD2_1, CMP_IMM8::LT);
        }

        int Simd128_LtEq_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2, CMP_IMM8::LE);
        }
        int Simd128_LtEq_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2, CMP_IMM8::LE);
        }

        int Simd128_GtEq_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<CMPPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_2, srcOffsetF4_1, CMP_IMM8::LE);
        }
        int Simd128_GtEq_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<CMPPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_2, srcOffsetD2_1, CMP_IMM8::LE);
        }

        int Simd128_Eq_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2, CMP_IMM8::EQ);
        }
        int Simd128_Eq_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<PCMPEQD, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_2, srcOffsetI4_1);
        }
        int Simd128_Eq_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2, CMP_IMM8::EQ);
        }

        int Simd128_Neq_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF4_2, CMP_IMM8::NEQ);
        }
        int Simd128_Neq_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetD2_1, int srcOffsetD2_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<CMPPD, double>(buffer, templateData, targetOffsetD2_0, srcOffsetD2_1, srcOffsetD2_2, CMP_IMM8::NEQ);
        }

        int Simd128_And_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<ANDPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_2, srcOffsetF4_1);
        }
        int Simd128_And_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<PAND, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI4_2);
        }

        int Simd128_Or_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<ORPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_2, srcOffsetF4_1);
        }
        int Simd128_Or_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<POR, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI4_2);
        }

        int Simd128_Xor_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            // reversed operands
            return EncodingHelpers::SIMDBinaryOperation<XORPS, float>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_2, srcOffsetF4_1);
        }
        int Simd128_Xor_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDBinaryOperation<PXOR, int>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI4_2);
        }

        int Simd128_Select_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetI4_1, int srcOffsetF4_2, int srcOffsetF4_3)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetF4_0 -= templateData->GetBaseOffSet();
            srcOffsetI4_1 -= templateData->GetBaseOffSet();
            srcOffsetF4_2 -= templateData->GetBaseOffSet();
            srcOffsetF4_3 -= templateData->GetBaseOffSet();

            RegNum maskReg, tReg, fReg, tempReg;
            int size = 0;
            int restrictions = 0;

            maskReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetI4_1, size);
            restrictions |= (1 << maskReg);

            tReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_2, size, restrictions);
            restrictions |= (1 << tReg);

            fReg    = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_3, size, restrictions);
            restrictions |= (1 << fReg);

            tempReg = templateData->GetReg<AsmJsSIMDValue>(restrictions);

            // MOVAPS tempReg, maskReg
            size += MOVAPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tempReg, maskReg));
            // ANDPS tempReg, tReg
            size += ANDPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tempReg, tReg));
            // ANDNPS maskReg, fReg
            size += ANDNPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(maskReg, fReg));
            templateData->InvalidateReg(maskReg);
            // ORPS tempReg, maskReg
            size += ORPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(tempReg, maskReg));
            // MOVUPS [dst], tempReg
            size += EncodingHelpers::SIMDSetStackReg<float>(buffer, templateData, targetOffsetF4_0, tempReg);

            return size;
        }

        int Simd128_Select_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI4_2, int srcOffsetI4_3)
        {
            // ok to re-use F4, size of I4 lane >= size of F4 lane. Important for correct invalidation of regs upon store to stack.
            return Simd128_Select_F4::ApplyTemplate(context, buffer, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI4_2, srcOffsetI4_3);
        }

        int Simd128_Select_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetD2_0, int srcOffsetI4_1, int srcOffsetD2_2, int srcOffsetD2_3)
        {
            // ok to re-use F4, size of D2 lane >= size of F4 lane. Important for correct invalidation of regs upon store to stack.
            return Simd128_Select_F4::ApplyTemplate(context, buffer, targetOffsetD2_0, srcOffsetI4_1, srcOffsetD2_2, srcOffsetD2_3);
        }

        //Lane Access
        int Simd128_ExtractLane_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI0, int srcOffsetI4_1, int index)
        {
            AssertMsg(index >= 0 && index < 4, "Invalid lane index");
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDLdLaneOperation<MOVSS, int>(buffer, templateData, targetOffsetI0, srcOffsetI4_1, index, false);
        }

        int Simd128_ExtractLane_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF0, int srcOffsetF4_1, int index)
        {
            AssertMsg(index >= 0 && index < 4, "Invalid lane index");
            X86TemplateData* templateData = GetTemplateData(context);
            return EncodingHelpers::SIMDLdLaneOperation<MOVSS, int>(buffer, templateData, targetOffsetF0, srcOffsetF4_1, index, false);
        }

        int Simd128_ReplaceLane_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetF4_0, int srcOffsetF4_1, int srcOffsetF2, int laneIndex)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            AssertMsg(laneIndex >= 0 && laneIndex < 4, "Invalid lane index");
            return EncodingHelpers::SIMDSetLaneOperation<float, SHUFPS>(buffer, templateData, targetOffsetF4_0, srcOffsetF4_1, srcOffsetF2, laneIndex);

        }

        int Simd128_ReplaceLane_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI4_0, int srcOffsetI4_1, int srcOffsetI2, int laneIndex)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            AssertMsg(laneIndex >= 0 && laneIndex < 4, "Invalid lane index");
            return EncodingHelpers::SIMDSetLaneOperation<int, PSHUFD>(buffer, templateData, targetOffsetI4_0, srcOffsetI4_1, srcOffsetI2, laneIndex);
        }

        int Simd128_LdSignMask_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI0, int srcOffsetF4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetI0 -= templateData->GetBaseOffSet();
            srcOffsetF4_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum srcReg, dstReg;
            srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetF4_1, size);
            dstReg = templateData->GetReg<int>();
            size += MOVMSKPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, srcReg));
            size += EncodingHelpers::SetStackReg<int>(buffer, templateData, targetOffsetI0, dstReg);
            return size;
        }

        int Simd128_LdSignMask_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI0, int srcOffsetI4_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetI0 -= templateData->GetBaseOffSet();
            srcOffsetI4_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum srcReg, dstReg;
            srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetI4_1, size);
            dstReg = templateData->GetReg<int>();
            size += MOVMSKPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, srcReg));
            size += EncodingHelpers::SetStackReg<int>(buffer, templateData, targetOffsetI0, dstReg);
            return size;
        }

        int Simd128_LdSignMask_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffsetI0, int srcOffsetD2_1)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffsetI0 -= templateData->GetBaseOffSet();
            srcOffsetD2_1 -= templateData->GetBaseOffSet();

            int size = 0;
            RegNum srcReg, dstReg;
            srcReg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, srcOffsetD2_1, size);
            dstReg = templateData->GetReg<int>();
            size += MOVMSKPD::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParams2Reg(dstReg, srcReg));
            size += EncodingHelpers::SetStackReg<int>(buffer, templateData, targetOffsetI0, dstReg);
            return size;
        }

        int Simd128_I_ArgOut_F4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int argIndex, int offset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            int size = 0;
            offset -= templateData->GetBaseOffSet();

            RegNum reg = EncodingHelpers::GetStackReg<AsmJsSIMDValue>(buffer, templateData, offset, size);
            InternalCallInfo* callInfo = templateData->GetInternalCallInfo();
            Assert(callInfo->nextArgIndex == argIndex);

            size += MOVUPS::EncodeInstruction<AsmJsSIMDValue>(buffer, InstrParamsAddrReg(RegESP, callInfo->currentOffset, reg));
            callInfo->currentOffset += sizeof(AsmJsSIMDValue);
            callInfo->nextArgIndex += sizeof(AsmJsSIMDValue) / sizeof(Var);
            return size;
        }
        int Simd128_I_ArgOut_I4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int argIndex, int offset)
        {
            return Simd128_I_ArgOut_F4::ApplyTemplate(context, buffer, argIndex, offset);
        }
        int Simd128_I_ArgOut_D2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int argIndex, int offset)
        {
            return Simd128_I_ArgOut_F4::ApplyTemplate(context, buffer, argIndex, offset);
        }

        int Simd128_I_Conv_VTF4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int srcOffset)
        {
            X86TemplateData* templateData = GetTemplateData(context);
            targetOffset -= templateData->GetBaseOffSet();
            srcOffset -= templateData->GetBaseOffSet();

            return EncodingHelpers::SIMDSetStackReg(buffer, templateData, targetOffset, RegXMM0);
        }
        int Simd128_I_Conv_VTI4::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int srcOffset)
        {
            return Simd128_I_Conv_VTF4::ApplyTemplate(context, buffer, targetOffset, srcOffset);
        }
        int Simd128_I_Conv_VTD2::ApplyTemplate(TemplateContext context, BYTE*& buffer, int targetOffset, int srcOffset)
        {
            return Simd128_I_Conv_VTF4::ApplyTemplate(context, buffer, targetOffset, srcOffset);
        }
    };
}
#endif
