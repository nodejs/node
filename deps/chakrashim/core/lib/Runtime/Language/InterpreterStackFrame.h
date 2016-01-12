//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

extern "C" PVOID _ReturnAddress(VOID);
#pragma intrinsic(_ReturnAddress)
class BailOutRecord;

extern "C" void __cdecl _alloca_probe_16();
namespace Js
{
    class EHBailoutData;
    enum InterpreterStackFrameFlags : UINT16
    {
        InterpreterStackFrameFlags_None = 0,
        InterpreterStackFrameFlags_WithinTryBlock = 1,
        InterpreterStackFrameFlags_WithinCatchBlock = 2,
        InterpreterStackFrameFlags_WithinFinallyBlock = 4,
        InterpreterStackFrameFlags_FromBailOut = 8,
        InterpreterStackFrameFlags_ProcessingBailOutFromEHCode = 0x10,
        InterpreterStackFrameFlags_All = 0xFFFF,
    };
    struct InterpreterStackFrame   /* Stack allocated, no virtuals */
    {
        PREVENT_COPY(InterpreterStackFrame)

        friend class BailOutRecord;
        friend class JavascriptGeneratorFunction;
        friend class JavascriptGenerator;

        class Setup
        {
        public:
            Setup(ScriptFunction * function, Arguments& args, bool inlinee = false);
            Setup(ScriptFunction * function, Var * inParams, int inSlotsCount, bool inlinee = false);
            size_t GetAllocationVarCount() const { return varAllocCount; }

            InterpreterStackFrame * AllocateAndInitialize(bool doProfile, bool * releaseAlloc);

#if DBG
            InterpreterStackFrame * InitializeAllocation(__in_ecount(varAllocCount) Var * allocation, bool initParams, bool profileParams, Var loopHeaderArray, DWORD_PTR stackAddr, Var invalidStackVar);
#else
            InterpreterStackFrame * InitializeAllocation(__in_ecount(varAllocCount) Var * allocation, bool initParams, bool profileParams, Var loopHeaderArray, DWORD_PTR stackAddr);
#endif
            uint GetLocalCount() const { return localCount; }

        private:
            template <class Fn>
            void InitializeParams(InterpreterStackFrame * newInstance, Fn callback, Var **pprestDest);
            template <class Fn>
            void InitializeParamsAndUndef(InterpreterStackFrame * newInstance, Fn callback, Var **pprestDest);
            void InitializeRestParam(InterpreterStackFrame * newInstance, Var *dest);
            void SetupInternal();

            Var * inParams;
            ScriptFunction * const function;
            FunctionBody * const executeFunction;
            void** inlineCaches;
            int inSlotsCount;
            uint localCount;
            uint varAllocCount;
            uint inlineCacheCount;
            Js::CallFlags callFlags;
            bool bailedOutOfInlinee;
        };
    private:
        ByteCodeReader m_reader;        // Reader for current function
        int m_inSlotsCount;             // Count of actual incoming parameters to this function
        Js::CallFlags m_callFlags;      // CallFlags passed to the current function
        Var* m_inParams;                // Range of 'in' parameters
        Var* m_outParams;               // Range of 'out' parameters (offset in m_localSlots)
        Var* m_outSp;                   // Stack pointer for next outparam
        Var  m_arguments;               // Dedicated location for this frame's arguments object
        StackScriptFunction * stackNestedFunctions;
        FrameDisplay * localFrameDisplay;
        Var localClosure;
        Var *innerScopeArray;
        ScriptContext* scriptContext;
        ScriptFunction * function;
        FunctionBody * m_functionBody;
        void** inlineCaches;
        void * returnAddress;
        void * addressOfReturnAddress;  // Tag this frame with stack position, used by (remote) stack walker to test partially initialized interpreter stack frame.
        InterpreterStackFrame *previousInterpreterFrame;
        Var  loopHeaderArray;          // Keeps alive any JITted loop bodies while the function is being interpreted

        // 'stack address' of the frame, used for recursion detection during stepping.
        // For frames created via interpreter path, we use 'this', for frames created by bailout we use stack addr of actual jitted frame
        // the interpreter frame is created for.
        DWORD_PTR m_stackAddress;

#if ENABLE_PROFILE_INFO
        ImplicitCallFlags * savedLoopImplicitCallFlags;
#endif

        uint inlineCacheCount;
        uint currentLoopNum;
        uint currentLoopCounter;       // This keeps tracks of how many times the current loop is executed. It's hit only in cases where jitloopbodies are not hit
                                       // such as loops inside try\catch.

        UINT16 m_flags;                // based on InterpreterStackFrameFlags

        bool closureInitDone : 1;
#if ENABLE_PROFILE_INFO
        bool switchProfileMode : 1;
        bool isAutoProfiling : 1;
        uint32 switchProfileModeOnLoopEndNumber;
#endif
        int16 nestedTryDepth;
        int16 nestedCatchDepth;
        uint retOffset;
        int16 nestedFinallyDepth;


        void (InterpreterStackFrame::*opLoopBodyStart)(uint32 loopNumber, LayoutSize layoutSize, bool isFirstIteration);
#if ENABLE_PROFILE_INFO
        void (InterpreterStackFrame::*opProfiledLoopBodyStart)(uint32 loopNumber, LayoutSize layoutSize, bool isFirstIteration);
#endif
#if DBG || DBG_DUMP
        void * DEBUG_currentByteOffset;
#endif

        // Asm.js stack pointer
        int* m_localIntSlots;
        double* m_localDoubleSlots;
        float* m_localFloatSlots;

         _SIMDValue* m_localSimdSlots;

        EHBailoutData * ehBailoutData;

        // 16-byte aligned
        __declspec(align(16)) Var m_localSlots[0];           // Range of locals and temporaries

        static const int LocalsThreshold = 32 * 1024; // Number of locals vars we'll allocate on the frame.
                                                      // If there are more, we'll use an arena.

        typedef void(InterpreterStackFrame::*ArrFunc)(uint32, RegSlot);

        static const ArrFunc StArrFunc[8];
        static const ArrFunc LdArrFunc[8];

        //This class must have an empty ctor (otherwise it will break the code in InterpreterStackFrame::InterpreterThunk
        inline InterpreterStackFrame() { }

        void ProcessTryFinally(const byte* ip, Js::JumpOffset jumpOffset, Js::RegSlot regException = Js::Constants::NoRegister, Js::RegSlot regOffset = Js::Constants::NoRegister, bool hasYield = false);
    public:
        void OP_SetOutAsmDb(RegSlot outRegisterID, double val);
        void OP_SetOutAsmInt(RegSlot outRegisterID, int val);
        void OP_I_SetOutAsmInt(RegSlot outRegisterID, int val);
        void OP_I_SetOutAsmDb(RegSlot outRegisterID, double val);
        void OP_I_SetOutAsmFlt(RegSlot outRegisterID, float val);

        void OP_I_SetOutAsmSimd(RegSlot outRegisterID, AsmJsSIMDValue val);

        void SetOut(ArgSlot outRegisterID, Var bValue);
        void SetOut(ArgSlot_OneByte outRegisterID, Var bValue);
        void PushOut(Var aValue);
        void PopOut(ArgSlot argCount);

        FrameDisplay * GetLocalFrameDisplay() const;
        FrameDisplay * GetFrameDisplayForNestedFunc() const;
        Var InnerScopeFromRegSlot(RegSlot reg) const;
        void SetClosureInitDone(bool done) { closureInitDone = done; }

        void ValidateRegValue(Var value, bool allowStackVar = false, bool allowStackVarOnDisabledStackNestedFunc = true) const;
        void ValidateSetRegValue(Var value, bool allowStackVar = false, bool allowStackVarOnDisabledStackNestedFunc = true) const;
        template <typename RegSlotType> Var GetReg(RegSlotType localRegisterID) const;
        template <typename RegSlotType> void SetReg(RegSlotType localRegisterID, Var bValue);
        template <typename RegSlotType> Var GetRegAllowStackVar(RegSlotType localRegisterID) const;
        template <typename RegSlotType> void SetRegAllowStackVar(RegSlotType localRegisterID, Var bValue);
        template <typename RegSlotType> int GetRegRawInt( RegSlotType localRegisterID ) const;
        template <typename RegSlotType> void SetRegRawInt( RegSlotType localRegisterID, int bValue );
        template <typename RegSlotType> double GetRegRawDouble(RegSlotType localRegisterID) const;
        template <typename RegSlotType> float GetRegRawFloat(RegSlotType localRegisterID) const;
        template <typename RegSlotType> void SetRegRawDouble(RegSlotType localRegisterID, double bValue);
        template <typename RegSlotType> void SetRegRawFloat(RegSlotType localRegisterID, float bValue);
        template <typename T> T GetRegRaw( RegSlot localRegisterID ) const;
        template <typename T> void SetRegRaw( RegSlot localRegisterID, T bValue );

        template <typename RegSlotType> AsmJsSIMDValue GetRegRawSimd(RegSlotType localRegisterID) const;
        template <typename RegSlotType> void           SetRegRawSimd(RegSlotType localRegisterID, AsmJsSIMDValue bValue);
        static DWORD GetAsmSimdValOffSet(AsmJsCallStackLayout* stack);
        template <class T> void OP_SimdLdArrGeneric(const unaligned T* playout);
        template <class T> void OP_SimdLdArrConstIndex(const unaligned T* playout);
        template <class T> void OP_SimdStArrGeneric(const unaligned T* playout);
        template <class T> void OP_SimdStArrConstIndex(const unaligned T* playout);

        template <typename RegSlotType>
        Var GetRegAllowStackVarEnableOnly(RegSlotType localRegisterID) const;
        template <typename RegSlotType>
        void SetRegAllowStackVarEnableOnly(RegSlotType localRegisterID, Var bValue);

        Var GetNonVarReg(RegSlot localRegisterID) const;
        void SetNonVarReg(RegSlot localRegisterID, void * bValue);
        ScriptContext* GetScriptContext() const { return scriptContext; }
        Var GetRootObject() const;
        ScriptFunction* GetJavascriptFunction() const { return function; }
        FunctionBody * GetFunctionBody() const { return m_functionBody; }
        ByteCodeReader* GetReader() { return &m_reader;}
        uint GetCurrentLoopNum() const { return currentLoopNum; }
        InterpreterStackFrame* GetPreviousFrame() const {return previousInterpreterFrame;}
        void SetPreviousFrame(InterpreterStackFrame *interpreterFrame) {previousInterpreterFrame = interpreterFrame;}
        Var GetArgumentsObject() const { return m_arguments; }
        void SetArgumentsObject(Var args) { m_arguments = args; }
        UINT16 GetFlags() const { return m_flags; }
        void OrFlags(UINT16 addTo) { m_flags |= addTo; }
        bool IsInCatchOrFinallyBlock();
        static bool IsDelayDynamicInterpreterThunk(void* entryPoint);

        Var LdEnv() const;
        void SetEnv(FrameDisplay *frameDisplay);
        Var * NewScopeSlots(unsigned int size, ScriptContext *scriptContext, Var scope);
        Var * NewScopeSlots();
        Var NewScopeObject();
        FrameDisplay * NewFrameDisplay(void *argHead, void *argEnv);

        Var CreateHeapArguments(ScriptContext* scriptContext);

        bool IsCurrentLoopNativeAddr(void * codeAddr) const;
        void * GetReturnAddress() { return returnAddress; }

        static uint32 GetOffsetOfLocals() { return offsetof(InterpreterStackFrame, m_localSlots); }
        static uint32 GetOffsetOfArguments() { return offsetof(InterpreterStackFrame, m_arguments); }
        static uint32 GetOffsetOfInParams() { return offsetof(InterpreterStackFrame, m_inParams); }
        static uint32 GetOffsetOfInSlotsCount() { return offsetof(InterpreterStackFrame, m_inSlotsCount); }
        void PrintStack(const int* const intSrc, const float* const fltSrc, const double* const dblSrc, int intConstCount, int floatConstCount, int doubleConstCount, const wchar_t* state);

        static uint32 GetStartLocationOffset() { return offsetof(InterpreterStackFrame, m_reader) + ByteCodeReader::GetStartLocationOffset(); }
        static uint32 GetCurrentLocationOffset() { return offsetof(InterpreterStackFrame, m_reader) + ByteCodeReader::GetCurrentLocationOffset(); }

        static bool IsBrLong(OpCode op, const byte * ip)
        {
#ifdef BYTECODE_BRANCH_ISLAND
            return (op == OpCode::ExtendedOpcodePrefix) && ((OpCode)(ByteCodeReader::PeekByteOp(ip) + (OpCode::ExtendedOpcodePrefix << 8)) == OpCode::BrLong);
#else
            return false;
#endif
        }

        DWORD_PTR GetStackAddress() const;
        void* GetAddressOfReturnAddress() const;

#if _M_IX86
        static int GetRetType(JavascriptFunction* func);
        static int GetAsmJsArgSize(AsmJsCallStackLayout * stack);
        static int GetDynamicRetType(AsmJsCallStackLayout * stack);
        static DWORD GetAsmIntDbValOffSet(AsmJsCallStackLayout * stack);
        __declspec(noinline)   static int  AsmJsInterpreter(AsmJsCallStackLayout * stack);
#elif _M_X64
        template <typename T>
        static T AsmJsInterpreter(AsmJsCallStackLayout* layout);
        static void * GetAsmJsInterpreterEntryPoint(AsmJsCallStackLayout* stack);
        template <typename T>
        static T GetAsmJsRetVal(InterpreterStackFrame* instance);

        static Var AsmJsDelayDynamicInterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...);

        static __m128 AsmJsInterpreterSimdJs(AsmJsCallStackLayout* func);

#endif

#ifdef ASMJS_PLAT
        static void InterpreterAsmThunk(AsmJsCallStackLayout* layout);
#endif

#if DYNAMIC_INTERPRETER_THUNK
        static Var DelayDynamicInterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...);
        __declspec(noinline) static Var InterpreterThunk(JavascriptCallStackLayout* layout);
#else
        __declspec(noinline) static Var InterpreterThunk(RecyclableObject* function, CallInfo callInfo, ...);
#endif
        static Var InterpreterHelper(ScriptFunction* function, ArgumentReader args, void* returnAddress, void* addressOfReturnAddress, const bool isAsmJs = false);
    private:
#if DYNAMIC_INTERPRETER_THUNK
        static JavascriptMethod EnsureDynamicInterpreterThunk(Js::ScriptFunction * function);
#endif
        template<typename T>
        T ReadByteOp( const byte *& ip
#if DBG_DUMP
                           , bool isExtended = false
#endif
                           );

        void* __cdecl operator new(size_t byteSize, void* previousAllocation) throw();
        void __cdecl operator delete(void* allocationToFree, void* previousAllocation) throw();


        __declspec(noinline) Var ProcessThunk(void* returnAddress, void* addressOfReturnAddress);
        __declspec(noinline) Var DebugProcessThunk(void* returnAddress, void* addressOfReturnAddress);

        void AlignMemoryForAsmJs();

        Var Process();
        Var ProcessAsmJsModule();
        Var ProcessLinkFailedAsmJsModule();
        Var ProcessAsmJs();
        Var ProcessProfiled();
        Var ProcessUnprofiled();

        Var ProcessWithDebugging();
        Var DebugProcess();

        // This will be called for reseting outs when resume from break on error happened
        void ResetOut();

        Var OP_ArgIn0();
        template <class T> void OP_ArgOut_Env(const unaligned T* playout);
        template <class T> void OP_ArgOut_A(const unaligned T* playout);
        template <class T> void OP_ProfiledArgOut_A(const unaligned T * playout);
#if DBG
        template <class T> void OP_ArgOut_ANonVar(const unaligned T* playout);
#endif
        FrameDisplay * GetEnvForEvalCode();

        BOOL OP_BrFalse_A(Var aValue, ScriptContext* scriptContext);
        BOOL OP_BrTrue_A(Var aValue, ScriptContext* scriptContext);
        BOOL OP_BrNotNull_A(Var aValue);
        BOOL OP_BrUndecl_A(Var aValue);
        BOOL OP_BrNotUndecl_A(Var aValue);
        BOOL OP_BrOnHasProperty(Var argInstance, uint propertyIdIndex, ScriptContext* scriptContext);
        BOOL OP_BrOnNoProperty(Var argInstance, uint propertyIdIndex, ScriptContext* scriptContext);
        BOOL OP_BrOnNoEnvProperty(Var envInstance, int32 slotIndex, uint propertyIdIndex, ScriptContext* scriptContext);
        BOOL OP_BrOnClassConstructor(Var aValue);

        RecyclableObject * OP_CallGetFunc(Var target);

        template <class T> const byte * OP_Br(const unaligned T * playout);
        void OP_AsmStartCall(const unaligned OpLayoutStartCall * playout);
        void OP_StartCall( const unaligned OpLayoutStartCall * playout );
        void OP_StartCall(uint outParamCount);
        template <class T> void OP_CallCommon(const unaligned T *playout, RecyclableObject * aFunc, unsigned flags, const Js::AuxArray<uint32> *spreadIndices = nullptr);
        void OP_CallAsmInternal( RecyclableObject * function);
        template <class T> void OP_I_AsmCall(const unaligned T* playout) { OP_CallAsmInternal((ScriptFunction*)OP_CallGetFunc(GetRegAllowStackVar(playout->Function))); }

        template <class T> void OP_CallCommonI(const unaligned T *playout, RecyclableObject * aFunc, unsigned flags);
        template <class T> void OP_ProfileCallCommon(const unaligned T *playout, RecyclableObject * aFunc, unsigned flags, ProfileId profileId, InlineCacheIndex inlineCacheIndex = Js::Constants::NoInlineCacheIndex, const Js::AuxArray<uint32> *spreadIndices = nullptr);
        template <class T> void OP_ProfileReturnTypeCallCommon(const unaligned T *playout, RecyclableObject * aFunc, unsigned flags, ProfileId profileId, const Js::AuxArray<uint32> *spreadIndices = nullptr);
        template <class T> void OP_CallPutCommon(const unaligned T *playout, RecyclableObject * aFunc);
        template <class T> void OP_CallPutCommonI(const unaligned T *playout, RecyclableObject * aFunc);

        template <class T> void OP_AsmCall(const unaligned T* playout);

        template <class T> void OP_CallI(const unaligned T* playout, unsigned flags) { OP_CallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags); }
        template <class T> void OP_CallIExtended(const unaligned T* playout, unsigned flags) { OP_CallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_CallIExtendedFlags(const unaligned T* playout, unsigned flags) { OP_CallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags | playout->callFlags, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_CallIFlags(const unaligned T* playout, unsigned flags) { playout->callFlags == Js::CallFlags::CallFlags_NewTarget ? OP_CallPutCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function))) : OP_CallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags | playout->callFlags); }

        template <class T> void OP_ProfiledCallI(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags, playout->profileId); }
        template <class T> void OP_ProfiledCallIExtended(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags, playout->profileId, Js::Constants::NoInlineCacheIndex, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_ProfiledCallIExtendedFlags(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags | playout->callFlags, playout->profileId, Js::Constants::NoInlineCacheIndex, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_ProfiledCallIWithICIndex(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags, playout->profileId, playout->inlineCacheIndex); }
        template <class T> void OP_ProfiledCallIExtendedWithICIndex(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags, playout->profileId, playout->inlineCacheIndex, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_ProfiledCallIExtendedFlagsWithICIndex(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags | playout->callFlags, playout->profileId, playout->inlineCacheIndex, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_ProfiledCallIFlags(const unaligned T* playout, unsigned flags) { playout->callFlags == Js::CallFlags::CallFlags_NewTarget ? OP_CallPutCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function))) : OP_ProfileCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags | playout->callFlags, playout->profileId); }

        template <class T> void OP_ProfiledReturnTypeCallI(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileReturnTypeCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags, playout->profileId); }
        template <class T> void OP_ProfiledReturnTypeCallIExtended(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileReturnTypeCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags, playout->profileId, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_ProfiledReturnTypeCallIExtendedFlags(const unaligned OpLayoutDynamicProfile<T>* playout, unsigned flags) { OP_ProfileReturnTypeCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags | playout->callFlags, playout->profileId, (playout->Options & CallIExtended_SpreadArgs) ? m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody()) : nullptr); }
        template <class T> void OP_ProfiledReturnTypeCallIFlags(const unaligned T* playout, unsigned flags) { playout->callFlags == Js::CallFlags::CallFlags_NewTarget ? OP_CallPutCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function))) : OP_ProfileReturnTypeCallCommon(playout, OP_CallGetFunc(GetRegAllowStackVar(playout->Function)), flags | playout->callFlags, playout->profileId); }

        // Patching Fastpath Operations
        template <class T> void OP_GetRootProperty(unaligned T* playout);
        template <class T> void OP_GetRootPropertyForTypeOf(unaligned T* playout);
        template <class T> void OP_GetRootProperty_NoFastPath(unaligned T* playout);
        template <class T, bool Root, bool Method, bool CallApplyTarget> void ProfiledGetProperty(unaligned T* playout, const Var instance);
        template <class T> void OP_ProfiledGetRootProperty(unaligned T* playout);
        template <class T> void OP_ProfiledGetRootPropertyForTypeOf(unaligned T* playout);
        template <class T> void OP_GetProperty(Var instance, unaligned T* playout);
        template <class T> void OP_GetProperty(unaligned T* playout);
        template <class T> void OP_GetLocalProperty(unaligned T* playout);
        template <class T> void OP_GetSuperProperty(unaligned T* playout);
        template <class T> void OP_GetPropertyForTypeOf(unaligned T* playout);
        template <class T> void OP_GetProperty_NoFastPath(Var instance, unaligned T* playout);
        template <class T> void OP_ProfiledGetProperty(unaligned T* playout);
        template <class T> void OP_ProfiledGetLocalProperty(unaligned T* playout);
        template <class T> void OP_ProfiledGetSuperProperty(unaligned T* playout);
        template <class T> void OP_ProfiledGetPropertyForTypeOf(unaligned T* playout);
        template <class T> void OP_ProfiledGetPropertyCallApplyTarget(unaligned T* playout);
        template <class T> void OP_GetRootMethodProperty(unaligned T* playout);
        template <class T> void OP_GetRootMethodProperty_NoFastPath(unaligned T* playout);
        template <class T> void OP_ProfiledGetRootMethodProperty(unaligned T* playout);
        template <class T> void OP_GetMethodProperty(unaligned T* playout);
        template <class T> void OP_GetLocalMethodProperty(unaligned T* playout);
        template <class T> void OP_GetMethodProperty(Var varInstance, unaligned T* playout);
        template <class T> void OP_GetMethodProperty_NoFastPath(Var varInstance, unaligned T* playout);
        template <class T> void OP_ProfiledGetMethodProperty(unaligned T* playout);
        template <class T> void OP_ProfiledGetLocalMethodProperty(unaligned T* playout);
        template <typename T> void OP_GetPropertyScoped(const unaligned OpLayoutT_ElementP<T>* playout);
        template <typename T> void OP_GetPropertyForTypeOfScoped(const unaligned OpLayoutT_ElementP<T>* playout);
        template <typename T> void OP_GetPropertyScoped_NoFastPath(const unaligned OpLayoutT_ElementP<T>* playout);
        template <class T> void OP_GetMethodPropertyScoped(unaligned T* playout);
        template <class T> void OP_GetMethodPropertyScoped_NoFastPath(unaligned T* playout);

#if ENABLE_PROFILE_INFO
        template <class T> void UpdateFldInfoFlagsForGetSetInlineCandidate(unaligned T* playout, FldInfoFlags& fldInfoFlags, CacheType cacheType,
                                                DynamicProfileInfo * dynamicProfileInfo, uint inlineCacheIndex, RecyclableObject * obj);

        template <class T> void UpdateFldInfoFlagsForCallApplyInlineCandidate(unaligned T* playout, FldInfoFlags& fldInfoFlags, CacheType cacheType,
                                                DynamicProfileInfo * dynamicProfileInfo, uint inlineCacheIndex, RecyclableObject * obj);
#endif

        template <class T> void OP_SetProperty(unaligned T* playout);
        template <class T> void OP_SetLocalProperty(unaligned T* playout);
        template <class T> void OP_SetSuperProperty(unaligned T* playout);
        template <class T> void OP_ProfiledSetProperty(unaligned T* playout);
        template <class T> void OP_ProfiledSetLocalProperty(unaligned T* playout);
        template <class T> void OP_ProfiledSetSuperProperty(unaligned T* playout);
        template <class T> void OP_SetRootProperty(unaligned T* playout);
        template <class T> void OP_ProfiledSetRootProperty(unaligned T* playout);
        template <class T> void OP_SetPropertyStrict(unaligned T* playout);
        template <class T> void OP_ProfiledSetPropertyStrict(unaligned T* playout);
        template <class T> void OP_SetRootPropertyStrict(unaligned T* playout);
        template <class T> void OP_ProfiledSetRootPropertyStrict(unaligned T* playout);
        template <class T> void OP_SetPropertyScoped(unaligned T* playout, PropertyOperationFlags flags = PropertyOperation_None);
        template <class T> void OP_SetPropertyScoped_NoFastPath(unaligned T* playout, PropertyOperationFlags flags);
        template <class T> void OP_SetPropertyScopedStrict(unaligned T* playout);
        template <class T> void OP_ConsoleSetPropertyScoped(unaligned T* playout);

        template <class T> void DoSetProperty(unaligned T* playout, Var instance, PropertyOperationFlags flags);
        template <class T> void DoSetSuperProperty(unaligned T* playout, Var instance, PropertyOperationFlags flags);
        template <class T> void DoSetProperty_NoFastPath(unaligned T* playout, Var instance, PropertyOperationFlags flags);
        template <class T> void DoSetSuperProperty_NoFastPath(unaligned T* playout, Var instance, PropertyOperationFlags flags);
        template <class T, bool Root> void ProfiledSetProperty(unaligned T* playout, Var instance, PropertyOperationFlags flags);
        template <class T, bool Root> void ProfiledSetSuperProperty(unaligned T* playout, Var instance, Var thisInstance, PropertyOperationFlags flags);

        template <class T> void OP_InitProperty(unaligned T* playout);
        template <class T> void OP_InitLocalProperty(unaligned T* playout);
        template <class T> void OP_InitRootProperty(unaligned T* playout);
        template <class T> void OP_InitUndeclLetProperty(unaligned T* playout);
        template <class T> void OP_InitUndeclLocalLetProperty(unaligned T* playout);
        void OP_InitUndeclRootLetProperty(uint propertyIdIndex);
        template <class T> void OP_InitUndeclConstProperty(unaligned T* playout);
        template <class T> void OP_InitUndeclLocalConstProperty(unaligned T* playout);
        void OP_InitUndeclRootConstProperty(uint propertyIdIndex);
        template <class T> void OP_InitUndeclConsoleLetProperty(unaligned T* playout);
        template <class T> void OP_InitUndeclConsoleConstProperty(unaligned T* playout);
        template <class T> void OP_ProfiledInitProperty(unaligned T* playout);
        template <class T> void OP_ProfiledInitLocalProperty(unaligned T* playout);
        template <class T> void OP_ProfiledInitRootProperty(unaligned T* playout);
        template <class T> void OP_ProfiledInitUndeclProperty(unaligned T* playout);

        template <class T> void DoInitProperty(unaligned T* playout, Var instance);
        template <class T> void DoInitProperty_NoFastPath(unaligned T* playout, Var instance);
        template <class T> void ProfiledInitProperty(unaligned T* playout, Var instance);

        template <class T> bool TrySetPropertyLocalFastPath(unaligned T* playout, PropertyId pid, Var instance, InlineCache*& inlineCache, PropertyOperationFlags flags = PropertyOperation_None);

        template <bool doProfile> Var ProfiledDivide(Var aLeft, Var aRight, ScriptContext* scriptContext, ProfileId profileId);
        template <bool doProfile> Var ProfileModulus(Var aLeft, Var aRight, ScriptContext* scriptContext, ProfileId profileId);
        template <bool doProfile> Var ProfiledSwitch(Var exp, ProfileId profileId);

        // Non-patching Fastpath operations
        template <typename T> void OP_GetElementI(const unaligned T* playout);
        template <typename T> void OP_ProfiledGetElementI(const unaligned OpLayoutDynamicProfile<T>* playout);

        template <typename T> void OP_SetElementI(const unaligned T* playout, PropertyOperationFlags flags = PropertyOperation_None);
        template <typename T> void OP_ProfiledSetElementI(const unaligned OpLayoutDynamicProfile<T>* playout, PropertyOperationFlags flags = PropertyOperation_None);
        template <typename T> void OP_SetElementIStrict(const unaligned T* playout);
        template <typename T> void OP_ProfiledSetElementIStrict(const unaligned OpLayoutDynamicProfile<T>* playout);

        template<class T> void OP_LdLen(const unaligned T *const playout);
        template<class T> void OP_ProfiledLdLen(const unaligned OpLayoutDynamicProfile<T> *const playout);

        Var OP_ProfiledLdThis(Var thisVar, int moduleID, ScriptContext* scriptContext);
        Var OP_ProfiledStrictLdThis(Var thisVar, ScriptContext* scriptContext);

        template <class T> void OP_SetArrayItemI_CI4(const unaligned T* playout);
        template <class T> void OP_SetArrayItemC_CI4(const unaligned T* playout);
        template <class T> void OP_SetArraySegmentItem_CI4(const unaligned T* playout);
        template <class T> void SetArrayLiteralItem(JavascriptArray *arr, uint32 index, T value);
        void OP_SetArraySegmentVars(const unaligned OpLayoutAuxiliary * playout);

        template <class T> void OP_NewScArray(const unaligned T * playout);
        template <bool Profiled, class T> void ProfiledNewScArray(const unaligned OpLayoutDynamicProfile<T> * playout);
        template <class T> void OP_ProfiledNewScArray(const unaligned OpLayoutDynamicProfile<T> * playout) { ProfiledNewScArray<true, T>(playout); }
        template <class T> void OP_ProfiledNewScArray_NoProfile(const unaligned OpLayoutDynamicProfile<T> * playout)  { ProfiledNewScArray<false, T>(playout); }
        void OP_NewScIntArray(const unaligned OpLayoutAuxiliary * playout);
        void OP_NewScFltArray(const unaligned OpLayoutAuxiliary * playout);
        void OP_ProfiledNewScIntArray(const unaligned OpLayoutDynamicProfile<OpLayoutAuxiliary> * playout);
        void OP_ProfiledNewScFltArray(const unaligned OpLayoutDynamicProfile<OpLayoutAuxiliary> * playout);

        template <class T> void OP_LdArrayHeadSegment(const unaligned T* playout);

        inline Var GetFunctionExpression();

        template <class T> inline void OP_LdFunctionExpression(const unaligned T* playout);
        template <class T> inline void OP_StFunctionExpression(const unaligned T* playout);
        template <class T> inline void OP_StLocalFunctionExpression(const unaligned T* playout);
        void OP_StFunctionExpression(Var instance, Var value, PropertyIdIndexType index);

        template <class T> inline void OP_LdNewTarget(const unaligned T* playout);

        inline Var OP_Ld_A(Var aValue);
        inline Var OP_LdLocalObj();
        void OP_ChkUndecl(Var aValue);
        void OP_ChkNewCallFlag();

        void OP_EnsureNoRootProperty(uint propertyIdIndex);
        void OP_EnsureNoRootRedeclProperty(uint propertyIdIndex);
        void OP_ScopedEnsureNoRedeclProperty(Var aValue, uint propertyIdIndex, Var aValue2);
        Var OP_InitUndecl();
        void OP_InitUndeclSlot(Var aValue, int32 slot);
        template <class T> inline void OP_InitInnerFld(const unaligned T * playout);
        template <class T> inline void OP_InitLetFld(const unaligned T * playout);
        template <class T> inline void OP_InitLocalLetFld(const unaligned T* playout);
        template <class T> inline void OP_InitInnerLetFld(const unaligned T * playout);
        template <class T> inline void OP_InitRootLetFld(const unaligned T * playout);
        template <class T> inline void OP_InitConstFld(const unaligned T * playout);
        template <class T> inline void OP_InitRootConstFld(const unaligned T * playout);
        template <class T> inline void DoInitLetFld(const unaligned T * playout, Var instance, PropertyOperationFlags flags = PropertyOperation_None);
        template <class T> inline void DoInitConstFld(const unaligned T * playout, Var instance, PropertyOperationFlags flags = PropertyOperation_None);
        template <class T> inline void OP_InitClassMember(const unaligned T * playout);
        template <class T> inline void OP_InitClassMemberComputedName(const unaligned T * playout);
        template <class T> inline void OP_InitClassMemberGet(const unaligned T * playout);
        template <class T> inline void OP_InitClassMemberSet(const unaligned T * playout);
        template <class T> inline void OP_InitClassMemberGetComputedName(const unaligned T * playout);
        template <class T> inline void OP_InitClassMemberSetComputedName(const unaligned T * playout);
        template<typename T> uint32 LogSizeOf();
        template <typename T2> inline void OP_LdArr(  uint32 index, RegSlot value  );
        template <class T> inline void OP_LdArrFunc(const unaligned T* playout);
        template <class T> inline void OP_ReturnDb(const unaligned T* playout);
        template<typename T> T GetArrayViewOverflowVal();
        template <typename T2> inline void OP_StArr( uint32 index, RegSlot value );
        template <class T> inline Var OP_LdAsmJsSlot(Var instance, const unaligned T* playout );
        template <class T, typename T2> inline void OP_StSlotPrimitive(const unaligned T* playout);
        template <class T, typename T2> inline void OP_LdSlotPrimitive( const unaligned T* playout );
        template <class T> inline void OP_LdArrGeneric   ( const unaligned T* playout );
        template <class T> inline void OP_LdArrConstIndex( const unaligned T* playout );
        template <class T> inline void OP_StArrGeneric   ( const unaligned T* playout );
        template <class T> inline void OP_StArrConstIndex( const unaligned T* playout );
        inline Var OP_LdSlot(Var instance, int32 slotIndex);
        inline Var OP_LdObjSlot(Var instance, int32 slotIndex);
        inline Var OP_LdFrameDisplaySlot(Var instance, int32 slotIndex);
        template <class T> inline Var OP_LdSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_ProfiledLdSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_LdInnerSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_ProfiledLdInnerSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_LdInnerObjSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_ProfiledLdInnerObjSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_LdEnvSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_ProfiledLdEnvSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_LdEnvObj(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_LdObjSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_ProfiledLdObjSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_LdEnvObjSlot(Var instance, const unaligned T* playout);
        template <class T> inline Var OP_ProfiledLdEnvObjSlot(Var instance, const unaligned T* playout);
        inline void OP_StSlot(Var instance, int32 slotIndex, Var value);
        inline void OP_StSlotChkUndecl(Var instance, int32 slotIndex, Var value);
        inline void OP_StEnvSlot(Var instance, int32 slotIndex1, int32 slotIndex2, Var value);
        inline void OP_StEnvSlotChkUndecl(Var instance, int32 slotIndex1, int32 slotIndex2, Var value);
        inline void OP_StObjSlot(Var instance, int32 slotIndex, Var value);
        inline void OP_StObjSlotChkUndecl(Var instance, int32 slotIndex, Var value);
        inline void OP_StEnvObjSlot(Var instance, int32 slotIndex1, int32 slotIndex2, Var value);
        inline void OP_StEnvObjSlotChkUndecl(Var instance, int32 slotIndex1, int32 slotIndex2, Var value);
        inline void* OP_LdArgCnt();
        template <bool letArgs> Var LdHeapArgumentsImpl(Var argsArray, ScriptContext* scriptContext);
        inline Var OP_LdHeapArguments(Var argsArray, ScriptContext* scriptContext);
        inline Var OP_LdLetHeapArguments(Var argsArray, ScriptContext* scriptContext);
        inline Var OP_LdHeapArgsCached(ScriptContext* scriptContext);
        inline Var OP_LdLetHeapArgsCached(ScriptContext* scriptContext);
        inline Var OP_LdStackArgPtr();
        inline Var OP_LdArgumentsFromFrame();
        Var OP_NewScObjectSimple();
        void OP_NewScObjectLiteral(const unaligned OpLayoutAuxiliary * playout);
        void OP_NewScObjectLiteral_LS(const unaligned OpLayoutAuxiliary * playout, RegSlot& target);
        void OP_LdPropIds(const unaligned OpLayoutAuxiliary * playout);
        template <bool Profile, bool JITLoopBody> void LoopBodyStart(uint32 loopNumber, LayoutSize layoutSize, bool isFirstIteration);
        LoopHeader const * DoLoopBodyStart(uint32 loopNumber, LayoutSize layoutSize, const bool doProfileLoopCheck, bool isFirstIteration);
        template <bool Profile, bool JITLoopBody> void ProfiledLoopBodyStart(uint32 loopNumber, LayoutSize layoutSize, bool isFirstIteration);
        void OP_RecordImplicitCall(uint loopNumber);
        template <class T, bool Profiled, bool ICIndex> void OP_NewScObject_Impl(const unaligned T* playout, InlineCacheIndex inlineCacheIndex = Js::Constants::NoInlineCacheIndex, const Js::AuxArray<uint32> *spreadIndices = nullptr);
        template <class T, bool Profiled> void OP_NewScObjArray_Impl(const unaligned T* playout, const Js::AuxArray<uint32> *spreadIndices = nullptr);
        template <class T> void OP_NewScObject(const unaligned T* playout) { OP_NewScObject_Impl<T, false, false>(playout); }
        template <class T> void OP_NewScObjectNoCtorFull(const unaligned T* playout);
        template <class T> void OP_NewScObjectSpread(const unaligned T* playout) { OP_NewScObject_Impl<T, false, false>(playout, Js::Constants::NoInlineCacheIndex, m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody())); }
        template <class T> void OP_NewScObjArray(const unaligned T* playout) { OP_NewScObjArray_Impl<T, false>(playout); }
        template <class T> void OP_NewScObjArraySpread(const unaligned T* playout) { OP_NewScObjArray_Impl<T, false>(playout, m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody())); }
        template <class T> void OP_ProfiledNewScObject(const unaligned OpLayoutDynamicProfile<T>* playout) { OP_NewScObject_Impl<T, true, false>(playout); }
        template <class T> void OP_ProfiledNewScObjectSpread(const unaligned OpLayoutDynamicProfile<T>* playout) { OP_NewScObject_Impl<T, true, false>(playout, Js::Constants::NoInlineCacheIndex, m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody())); }
        template <class T> void OP_ProfiledNewScObjectWithICIndex(const unaligned OpLayoutDynamicProfile<T>* playout) { OP_NewScObject_Impl<T, true, true>(playout, playout->inlineCacheIndex); }
        template <class T> void OP_ProfiledNewScObjArray(const unaligned OpLayoutDynamicProfile2<T>* playout) { OP_NewScObjArray_Impl<T, true>(playout); }
        template <class T> void OP_ProfiledNewScObjArray_NoProfile(const unaligned OpLayoutDynamicProfile2<T>* playout) { OP_NewScObjArray_Impl<T, false>(playout); }
        template <class T> void OP_ProfiledNewScObjArraySpread(const unaligned OpLayoutDynamicProfile2<T>* playout) { OP_NewScObjArray_Impl<T, true>(playout, m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody())); }
        template <class T> void OP_ProfiledNewScObjArraySpread_NoProfile(const unaligned OpLayoutDynamicProfile2<T>* playout) { OP_NewScObjArray_Impl<T, true>(playout, m_reader.ReadAuxArray<uint32>(playout->SpreadAuxOffset, this->GetFunctionBody())); }
        Var NewScObject_Helper(Var target, ArgSlot ArgCount, const Js::AuxArray<uint32> *spreadIndices = nullptr);
        Var ProfiledNewScObject_Helper(Var target, ArgSlot ArgCount, ProfileId profileId, InlineCacheIndex inlineCacheIndex, const Js::AuxArray<uint32> *spreadIndices = nullptr);
        template <class T, bool Profiled, bool ICIndex> Var OP_NewScObjectNoArg_Impl(const unaligned T *playout, InlineCacheIndex inlineCacheIndex = Js::Constants::NoInlineCacheIndex);
        void OP_NewScObject_A_Impl(const unaligned OpLayoutAuxiliary * playout, RegSlot *target = nullptr);
        void OP_NewScObject_A(const unaligned OpLayoutAuxiliary * playout) { return OP_NewScObject_A_Impl(playout); }
        void OP_InitCachedFuncs(const unaligned OpLayoutAuxNoReg * playout);
        Var OP_GetCachedFunc(Var instance, int32 index);
        void OP_CommitScope(const unaligned OpLayoutAuxNoReg * playout);
        void OP_CommitScopeHelper(const unaligned OpLayoutAuxNoReg *playout, const PropertyIdArray *propIds);
        void OP_TryCatch(const unaligned OpLayoutBr* playout);
        void ProcessCatch();
        int ProcessFinally();
        void ProcessTryCatchBailout(EHBailoutData * innermostEHBailoutData, uint32 tryNestingDepth);
        void OP_TryFinally(const unaligned OpLayoutBr* playout);
        void OP_TryFinallyWithYield(const byte* ip, Js::JumpOffset jumpOffset, Js::RegSlot regException, Js::RegSlot regOffset);
        void OP_ResumeCatch();
        void OP_ResumeFinally(const byte* ip, Js::JumpOffset jumpOffset, RegSlot exceptionRegSlot, RegSlot offsetRegSlot);
        inline Var OP_ResumeYield(Var yieldDataVar, RegSlot yieldStarIterator = Js::Constants::NoRegister);
        template <typename T> void OP_IsInst(const unaligned T * playout);
        template <class T> void OP_InitClass(const unaligned OpLayoutT_Class<T> * playout);
        inline Var OP_LdSuper(ScriptContext * scriptContext);
        inline Var OP_LdSuperCtor(ScriptContext * scriptContext);
        inline Var OP_ScopedLdSuper(ScriptContext * scriptContext);
        inline Var OP_ScopedLdSuperCtor(ScriptContext * scriptContext);
        template <typename T> void OP_LdElementUndefined(const unaligned OpLayoutT_ElementU<T>* playout);
        template <typename T> void OP_LdLocalElementUndefined(const unaligned OpLayoutT_ElementRootU<T>* playout);
        template <typename T> void OP_LdElementUndefinedScoped(const unaligned OpLayoutT_ElementScopedU<T>* playout);
        void OP_SpreadArrayLiteral(const unaligned OpLayoutReg2Aux * playout);
        template <LayoutSize layoutSize,bool profiled> const byte * OP_ProfiledLoopStart(const byte *ip);
        template <LayoutSize layoutSize,bool profiled> const byte * OP_ProfiledLoopEnd(const byte *ip);
        template <LayoutSize layoutSize,bool profiled> const byte * OP_ProfiledLoopBodyStart(const byte *ip);
        template <typename T> void OP_ApplyArgs(const unaligned OpLayoutT_Reg5<T> * playout);
        template <class T> void OP_EmitTmpRegCount(const unaligned OpLayoutT_Unsigned1<T> * ip);

        Var InnerScopeFromIndex(uint32 index) const;
        void SetInnerScopeFromIndex(uint32 index, Var scope);
        void OP_NewInnerScopeSlots(uint index, uint count, int scopeIndex, ScriptContext *scriptContext, FunctionBody *functionBody);
        template <typename T> void OP_CloneInnerScopeSlots(const unaligned OpLayoutT_Unsigned1<T> *playout);
        template <typename T> void OP_CloneBlockScope(const unaligned OpLayoutT_Unsigned1<T> *playout);
        FrameDisplay * OP_LdFrameDisplay(void *argHead, void *argEnv, ScriptContext *scriptContext);
        FrameDisplay * OP_LdFrameDisplaySetLocal(void *argHead, void *argEnv, ScriptContext *scriptContext);
        template <bool innerFD> FrameDisplay * OP_LdFrameDisplayNoParent(void *argHead, ScriptContext *scriptContext);
        FrameDisplay * OP_LdFuncExprFrameDisplaySetLocal(void *argHead1, void *argHead2, ScriptContext *scriptContext);
        FrameDisplay * OP_LdInnerFrameDisplay(void *argHead, void *argEnv, ScriptContext *scriptContext);
        FrameDisplay * OP_LdInnerFrameDisplayNoParent(void *argHead, ScriptContext *scriptContext);

        template <class T> void OP_NewStackScFunc(const unaligned T * playout);
        template <class T> void OP_NewInnerStackScFunc(const unaligned T * playout);
        template <class T> void OP_DeleteFld(const unaligned T * playout);
        template <class T> void OP_DeleteLocalFld(const unaligned T * playout);
        template <class T> void OP_DeleteRootFld(const unaligned T * playout);
        template <class T> void OP_DeleteFldStrict(const unaligned T * playout);
        template <class T> void OP_DeleteRootFldStrict(const unaligned T * playout);
        template <typename T> void OP_ScopedDeleteFld(const unaligned OpLayoutT_ElementScopedC<T> * playout);
        template <typename T> void OP_ScopedDeleteFldStrict(const unaligned OpLayoutT_ElementScopedC<T> * playout);
        template <class T> void OP_ScopedLdInst(const unaligned T * playout);
        template <typename T> void OP_ScopedInitFunc(const unaligned OpLayoutT_ElementScopedC<T> * playout);
        template <class T> void OP_ClearAttributes(const unaligned T * playout);
        template <class T> void OP_InitGetFld(const unaligned T * playout);
        template <class T> void OP_InitSetFld(const unaligned T * playout);
        template <class T> void OP_InitSetElemI(const unaligned T * playout);
        template <class T> void OP_InitGetElemI(const unaligned T * playout);
        template <class T> void OP_InitComputedProperty(const unaligned T * playout);
        template <class T> void OP_InitProto(const unaligned T * playout);

        uint CallLoopBody(JavascriptMethod address);
        uint CallAsmJsLoopBody(JavascriptMethod address);
        void DoInterruptProbe();
        void CheckIfLoopIsHot(uint profiledLoopCounter);
        bool CheckAndResetImplicitCall(DisableImplicitFlags prevDisableImplicitFlags,ImplicitCallFlags savedImplicitCallFlags);
        class PushPopFrameHelper
        {
        public:
            PushPopFrameHelper(InterpreterStackFrame *interpreterFrame, void *returnAddress, void *addressOfReturnAddress)
                : m_threadContext(interpreterFrame->GetScriptContext()->GetThreadContext()), m_interpreterFrame(interpreterFrame), m_isHiddenFrame(false)
            {
                interpreterFrame->returnAddress = returnAddress; // Ensure these are set before pushing to interpreter frame list
                interpreterFrame->addressOfReturnAddress = addressOfReturnAddress;
                if (interpreterFrame->GetFunctionBody()->GetIsAsmJsFunction())
                {
                    m_isHiddenFrame = true;
                }
                else
                {
                    m_threadContext->PushInterpreterFrame(interpreterFrame);
                }
            }
            ~ PushPopFrameHelper()
            {
                if (!m_isHiddenFrame)
                {
                    Js::InterpreterStackFrame *interpreterFrame = m_threadContext->PopInterpreterFrame();
                    AssertMsg(interpreterFrame == m_interpreterFrame,
                        "Interpreter frame chain corrupted?");
                }
            }
        private:
            ThreadContext *m_threadContext;
            InterpreterStackFrame *m_interpreterFrame;
            bool m_isHiddenFrame;
        };

        inline InlineCache* GetInlineCache(uint cacheIndex)
        {
            Assert(this->inlineCaches != nullptr);
            Assert(cacheIndex < this->inlineCacheCount);

            return reinterpret_cast<InlineCache *>(this->inlineCaches[cacheIndex]);
        }

        inline IsInstInlineCache* GetIsInstInlineCache(uint cacheIndex)
        {
            return m_functionBody->GetIsInstInlineCache(cacheIndex);
        }

        inline PropertyId GetPropertyIdFromCacheId(uint cacheIndex)
        {
            return m_functionBody->GetPropertyIdFromCacheId(cacheIndex);
        }

        void InitializeStackFunctions(StackScriptFunction * scriptFunctions);
        StackScriptFunction * GetStackNestedFunction(uint index);
        void SetExecutingStackFunction(ScriptFunction * scriptFunction);
        friend class StackScriptFunction;

        void InitializeClosures();
        void SetLocalFrameDisplay(FrameDisplay *frameDisplay);
        Var  GetLocalClosure() const;
        void SetLocalClosure(Var closure);
        void TrySetRetOffset();
    };

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    // Used to track how many interpreter stack frames we have on stack.
    class InterpreterThunkStackCountTracker
    {
    public:
        InterpreterThunkStackCountTracker()  { ++s_count; }
        ~InterpreterThunkStackCountTracker() { --s_count; }
        static int GetCount() { return s_count; }
    private:
        __declspec(thread) static int s_count;
    };
#endif

} // namespace Js
