//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct ByteCodeWriter
    {
    protected:
        struct DataChunk
        {
        private:
            byte* currentByte;
            byte* buffer;
            uint byteSize;
        public:
            DataChunk* nextChunk;
            DataChunk(ArenaAllocator* allocator, uint initSize) :
                nextChunk(nullptr),
                byteSize(initSize)
            {
                buffer = AnewArray(allocator, byte, initSize);
                currentByte = buffer;
            }
            inline uint GetSize()
            {
                return byteSize;
            }
            inline const byte* GetBuffer()
            {
                return buffer;
            }
            inline void Reset()
            {
                currentByte = buffer;
            }
            inline uint RemainingBytes()
            {
                Assert(byteSize >= GetCurrentOffset());
                return byteSize - GetCurrentOffset();
            }
            inline uint GetCurrentOffset()
            {
                Assert(currentByte >= buffer);
                return (uint) (currentByte - buffer);
            }
            inline void SetCurrentOffset(uint offset)
            {
                currentByte = buffer + offset;
            }

            // This does not do check if there is enough space for the copy to succeed.
            __inline void WriteUnsafe(__in_bcount(byteSize) const void* data, __in uint byteSize)
            {
                AssertMsg(RemainingBytes() >= byteSize, "We do not have enough room");

                js_memcpy_s(currentByte, this->RemainingBytes(), data, byteSize);
                currentByte += byteSize;
            }
        };

        // This is a linked list of data chunks. It is designed to be allocated
        // once and re-used. After a call to Reset(), this data structure can be reused to
        // store more data.
        struct Data
        {
        private:
            ArenaAllocator* tempAllocator;
            DataChunk* head;     // First chunk to be written to
            DataChunk* current;  // The current chunk being written to
            uint currentOffset;  // The global offset of last byte written to in the linked data structure
            bool fixedGrowthPolicy;

            __inline uint Write(__in_bcount(byteSize) const void* data, __in uint byteSize);
            __declspec(noinline) void SlowWrite(__in_bcount(byteSize) const void* data, __in uint byteSize);
            void AddChunk(uint byteSize);

        public:
            Data(bool fixedGrowthPolicy = false) : head(nullptr),
                current(nullptr),
                tempAllocator(nullptr),
                currentOffset(0),
                fixedGrowthPolicy(fixedGrowthPolicy)
            {
            }
            void Create(uint initSize, ArenaAllocator* tmpAlloc);
            inline uint GetCurrentOffset() const { return currentOffset; }
            inline DataChunk * GetCurrentChunk() const { return &(*current); }
            void SetCurrent(uint offset, DataChunk* currChunk);
            void Copy(Recycler* alloc, ByteBlock ** finalBlock);
            uint Encode(OpCode op, ByteCodeWriter* writer)
            {
                return EncodeT<Js::SmallLayout>(op, writer);
            }
            uint Encode(OpCode op, const void * rawData, int byteSize, ByteCodeWriter* writer)
            {
                return EncodeT<Js::SmallLayout>(op, rawData, byteSize, writer);
            }
            uint Encode(const void * rawData, int byteSize);

            template <LayoutSize layoutSize> uint EncodeT(OpCode op, ByteCodeWriter* writer);
            template <> uint EncodeT<SmallLayout>(OpCode op, ByteCodeWriter* writer);
            template <LayoutSize layoutSize> uint EncodeT(OpCode op, const void * rawData, int byteSize, ByteCodeWriter* writer);
            // asm.js encoding
            uint Encode(OpCodeAsmJs op, ByteCodeWriter* writer){ return EncodeT<Js::SmallLayout>(op, writer); }
            uint Encode(OpCodeAsmJs op, const void * rawData, int byteSize, ByteCodeWriter* writer, bool isPatching = false){ return EncodeT<Js::SmallLayout>(op, rawData, byteSize, writer, isPatching); }
            template <LayoutSize layoutSize> uint EncodeT(OpCodeAsmJs op, ByteCodeWriter* writer, bool isPatching = false);
            template <> uint EncodeT<SmallLayout>(OpCodeAsmJs op, ByteCodeWriter* writer, bool isPatching);
            template <LayoutSize layoutSize> uint EncodeT(OpCodeAsmJs op, const void * rawData, int byteSize, ByteCodeWriter* writer, bool isPatching = false);

            void Reset();
        };

        struct LoopHeaderData {
            uint startOffset;
            uint endOffset;
            bool isNested;
            LoopHeaderData() {}
            LoopHeaderData(uint startOffset, uint endOffset, bool isNested) : startOffset(startOffset), endOffset(endOffset), isNested(isNested){}
        };

        JsUtil::List<uint, ArenaAllocator> * m_labelOffsets;          // Label offsets, once defined
        struct JumpInfo
        {
            ByteCodeLabel labelId;
            uint patchOffset;
        };
#ifdef BYTECODE_BRANCH_ISLAND
        bool useBranchIsland;
        JsUtil::List<JumpInfo, ArenaAllocator> * m_longJumpOffsets;
        Js::OpCode lastOpcode;
        bool inEnsureLongBranch;
        uint firstUnknownJumpInfo;
        int nextBranchIslandOffset;

        // Size of emitting a jump around byte code instruction
        static size_t const JumpAroundSize = sizeof(byte) + sizeof(OpLayoutBr);
        // Size of emitting a long jump byte code instruction
        CompileAssert(OpCodeInfo<Js::OpCode::BrLong>::IsExtendedOpcode);    // extended opcode, opcode size is always sizeof(OpCode)
        static size_t const LongBranchSize = sizeof(OpCode) + sizeof(OpLayoutBrLong);
#endif
        JsUtil::List<JumpInfo, ArenaAllocator> * m_jumpOffsets;             // Offsets to replace "ByteCodeLabel" with actual destination
        JsUtil::List<LoopHeaderData, ArenaAllocator> * m_loopHeaders;       // Start/End offsets for loops
        SListBase<size_t>  rootObjectLoadInlineCacheOffsets;                // load inline cache offsets
        SListBase<size_t>  rootObjectStoreInlineCacheOffsets;               // load inline cache offsets
        SListBase<size_t>  rootObjectLoadMethodInlineCacheOffsets;

        FunctionBody* m_functionWrite;  // Function being written
        Data m_byteCodeData;            // Accumulated byte-code
        Data m_auxiliaryData;           // Optional accumulated auxiliary data
        Data m_auxContextData;          // Optional accumulated auxiliary ScriptContext specific data
        uint m_beginCodeSpan;           // Debug Info for where in  bytecode the current statement starts. (Statements do not nest.)
        ParseNode* m_pMatchingNode;     // Parse node for statement we are tracking debugInfo for.
        // This ref count mechanism ensures that nested Start/End Statement with matching parse node will not mess-up the upper Start/End balance.
        // This count will be incremented/decremented when the nested Start/End has the same m_pMatchingNode.
        int m_matchingNodeRefCount;
        struct SubexpressionNode
        {
            ParseNode* node;
            int beginCodeSpan;

            SubexpressionNode() {}
            SubexpressionNode(ParseNode* node, int beginCodeSpan) : node(node), beginCodeSpan(beginCodeSpan) {}
        };
        JsUtil::Stack<SubexpressionNode> * m_subexpressionNodesStack; // Parse nodes for Subexpressions not participating in debug-stepping actions
        SmallSpanSequenceIter spanIter;
        int m_loopNest;
        uint m_byteCodeCount;
        uint m_byteCodeWithoutLDACount; // Number of total bytecodes except LD_A and LdUndef
        uint m_byteCodeInLoopCount;
        uint32 m_tmpRegCount;
        bool m_doJitLoopBodies;
        bool m_hasLoop;
        bool m_isInDebugMode;
        bool m_doInterruptProbe;
    public:
        struct CacheIdUnit {
            uint cacheId;
            bool isRootObjectCache;
            CacheIdUnit() {}
            CacheIdUnit(uint cacheId, bool isRootObjectCache = false) : cacheId(cacheId), isRootObjectCache(isRootObjectCache) {}
        };

    protected:
        // A map of, bytecode register in which the function to be called is, to the inline cache index associated with the load of the function.
        typedef JsUtil::BaseDictionary<Js::RegSlot, CacheIdUnit, ArenaAllocator, PrimeSizePolicy> CallRegToLdFldCacheIndexMap;
        CallRegToLdFldCacheIndexMap* callRegToLdFldCacheIndexMap;

        // Debugger fields.
        Js::DebuggerScope* m_currentDebuggerScope;

#if DBG
        bool isInitialized;
        bool isInUse;
#endif

        void IncreaseByteCodeCount();
        void AddJumpOffset(Js::OpCode op, ByteCodeLabel labelId, uint fieldByteOffset);

        RegSlot ConsumeReg(RegSlot reg);

        inline void CheckOpen();
        inline void CheckLabel(ByteCodeLabel labelID);
        inline void CheckOp(OpCode op, OpLayoutType layoutType);
        inline void CheckReg(RegSlot registerID);

        bool DoDynamicProfileOpcode(Phase tag, bool noHeuristics = false) const;

        template <typename T>
        void PatchJumpOffset(JsUtil::List<JumpInfo, ArenaAllocator> * jumpOffsets, byte * byteBuffer, uint byteCount);

#ifdef BYTECODE_BRANCH_ISLAND
        static int32 GetBranchLimit();
        void AddLongJumpOffset(ByteCodeLabel labelId, uint fieldByteOffset);
        void EnsureLongBranch(Js::OpCode op);
        void UpdateNextBranchIslandOffset(uint firstUnknownJumpInfo, uint firstUnknownJumpOffset);
#endif

        // Debugger methods.
        void PushDebuggerScope(Js::DebuggerScope* debuggerScope);
        void PopDebuggerScope();

    public:
        ByteCodeWriter() : m_byteCodeCount(/*fixedGrowthPolicy=*/ true), m_currentDebuggerScope(nullptr) {}
#if DBG
        ~ByteCodeWriter() { Assert(!isInUse); }
#endif

        void Create();
        void InitData(ArenaAllocator* alloc, long initCodeBufferSize);
        void Begin(ByteCodeGenerator* byteCodeGenerator, FunctionBody* functionWrite, ArenaAllocator* alloc, bool doJitLoopBodies, bool hasLoop);
#ifdef LOG_BYTECODE_AST_RATIO
        void End(long currentAstSize, long maxAstSize);
#else
        void End();
#endif
        void Reset();

        void AllocateLoopHeaders();

#if DBG_DUMP
        uint ByteCodeDataSize();
        uint AuxiliaryDataSize();
        uint AuxiliaryContextDataSize();
#endif
        void Empty(OpCode op);
        void Reg1(OpCode op, RegSlot R0);
        void Reg2(OpCode op, RegSlot R0, RegSlot R1);
        void Reg3(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2);
        void Reg3C(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, uint cacheId);
        void Reg4(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3);
        void Reg1Unsigned1(OpCode op, RegSlot R0, uint C1);
        void Reg2B1(OpCode op, RegSlot R0, RegSlot R1, uint8 B3);
        void Reg3B1(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, uint8 B3);
        void Reg5(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4);
        void ArgIn0(RegSlot arg);
        template <bool isVar>
        void ArgOut(ArgSlot arg, RegSlot reg, ProfileId callSiteId);
        void ArgOutEnv(ArgSlot arg);
#ifdef BYTECODE_BRANCH_ISLAND
        void BrLong(OpCode op, ByteCodeLabel labelID);
#endif
        void Br(ByteCodeLabel labelID);
        void Br(OpCode op, ByteCodeLabel labelID);
        void BrReg1(OpCode op, ByteCodeLabel labelID, RegSlot R1);
        void BrS(OpCode op, ByteCodeLabel labelID, byte val);
        void BrReg2(OpCode op, ByteCodeLabel labelID, RegSlot R1, RegSlot R2);
        void BrProperty(OpCode op, ByteCodeLabel labelID, RegSlot R1, PropertyIdIndexType propertyIdIndex);
        void BrLocalProperty(OpCode op, ByteCodeLabel labelID, PropertyIdIndexType propertyIdIndex);
        void BrEnvProperty(OpCode op, ByteCodeLabel labelID, PropertyIdIndexType propertyIdIndex, int32 slotIndex);
        void StartCall(OpCode op, ArgSlot ArgCount);
        void CallI(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, ProfileId callSiteId, CallFlags callFlags = CallFlags_None);
        void CallIExtended(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallIExtendedOptions options, const void *buffer, uint byteCount, ProfileId callSiteId, CallFlags callFlags = CallFlags_None);
        void RemoveEntryForRegSlotFromCacheIdMap(RegSlot functionRegister);
        void Element(OpCode op, RegSlot value, RegSlot instance, RegSlot element, bool instanceAtReturnRegOK = false);
        void ElementUnsigned1(OpCode op, RegSlot value, RegSlot instance, uint32 element);
        void Property(OpCode op, RegSlot Value, RegSlot Instance, PropertyIdIndexType propertyIdIndex);
        void ScopedProperty(OpCode op, RegSlot Value, PropertyIdIndexType propertyIdIndex);
        void Slot(OpCode op, RegSlot value, RegSlot instance, int32 slotId);
        void Slot(OpCode op, RegSlot value, RegSlot instance, int32 slotId, ProfileId profileId);
        void SlotI1(OpCode op, RegSlot value, int32 slotId1);
        void SlotI1(OpCode op, RegSlot value, int32 slotId1, ProfileId profileId);
        void SlotI2(OpCode op, RegSlot value, int32 slotId1, int32 slotId2);
        void SlotI2(OpCode op, RegSlot value, int32 slotId1, int32 slotId2, ProfileId profileId);
        void ElementU(OpCode op, RegSlot instance, PropertyIdIndexType propertyIdIndex);
        void ElementScopedU(OpCode op, PropertyIdIndexType propertyIdIndex);
        void ElementRootU(OpCode op, PropertyIdIndexType propertyIdIndex);
        void ElementP(OpCode op, RegSlot value, uint cacheId, bool isCtor = false, bool registerCacheIdForCall = true);
        void ElementPIndexed(OpCode op, RegSlot value, uint cacheId, uint32 scopeIndex);
        void PatchableRootProperty(OpCode op, RegSlot value, uint cacheId, bool isLoadMethod, bool isStore, bool registerCacheIdForCall = true);
        void PatchableProperty(OpCode op, RegSlot value, RegSlot instance, uint cacheId, bool isCtor = false, bool registerCacheIdForCall = true);
        void PatchablePropertyWithThisPtr(OpCode op, RegSlot value, RegSlot instance, RegSlot thisInstance, uint cacheId, bool isCtor = false, bool registerCacheIdForCall = true);
        void ScopedProperty2(OpCode op, RegSlot Value, PropertyIdIndexType propertyIdIndex, RegSlot R2);
        void W1(OpCode op, unsigned short C1);
        void Reg1Int2(OpCode op, RegSlot R0, int C1, int C2);
        void Reg2Int1(OpCode op, RegSlot R0, RegSlot R1, int C1);
        void Unsigned1(OpCode op, uint C1);
        void Num3(OpCode op, RegSlot C0, RegSlot C1, RegSlot C2);

        template <typename SizePolicy> bool TryWriteReg1(OpCode op, RegSlot R0);
        template <typename SizePolicy> bool TryWriteReg2(OpCode op, RegSlot R0, RegSlot R1);
        template <typename SizePolicy> bool TryWriteReg2WithICIndex(OpCode op, RegSlot R0, RegSlot R1, uint32 inlineCacheIndex, bool isRootLoad);
        template <typename SizePolicy> bool TryWriteReg3(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2);
        template <typename SizePolicy> bool TryWriteReg3C(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, CacheId cacheId);
        template <typename SizePolicy> bool TryWriteReg4(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3);
        template <typename SizePolicy> bool TryWriteReg2B1(OpCode op, RegSlot R0, RegSlot R1, uint8 B2);
        template <typename SizePolicy> bool TryWriteReg3B1(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, uint8 B3);
        template <typename SizePolicy> bool TryWriteReg5(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4);
        template <typename SizePolicy> bool TryWriteUnsigned1(OpCode op, uint C1);
        template <typename SizePolicy> bool TryWriteArg(OpCode op, ArgSlot arg, RegSlot reg);
        template <typename SizePolicy> bool TryWriteArgNoSrc(OpCode op, ArgSlot arg);
        template <typename SizePolicy> bool TryWriteBrReg1(OpCode op, ByteCodeLabel labelID, RegSlot R1);
        template <typename SizePolicy> bool TryWriteBrReg2(OpCode op, ByteCodeLabel labelID, RegSlot R1, RegSlot R2);
        template <typename SizePolicy> bool TryWriteCallI(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount);
        template <typename SizePolicy> bool TryWriteCallIFlags(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallFlags callFlags);
        template <typename SizePolicy> bool TryWriteCallIWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, uint32 inlineCacheIndex, bool isRootLoad);
        template <typename SizePolicy> bool TryWriteCallIFlagsWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, uint32 inlineCacheIndex, bool isRootLoad, CallFlags callFlags);
        template <typename SizePolicy> bool TryWriteCallIExtended(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallIExtendedOptions options, uint32 spreadArgsOffset);
        template <typename SizePolicy> bool TryWriteCallIExtendedWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, InlineCacheIndex inlineCacheIndex, bool isRootLoad, CallIExtendedOptions options, uint32 spreadArgsOffset);
        template <typename SizePolicy> bool TryWriteCallIExtendedFlags(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallIExtendedOptions options, uint32 spreadArgsOffset, CallFlags callFlags);
        template <typename SizePolicy> bool TryWriteCallIExtendedFlagsWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, InlineCacheIndex inlineCacheIndex, bool isRootLoad, CallIExtendedOptions options, uint32 spreadArgsOffset, CallFlags callFlags);
        template <typename SizePolicy> bool TryWriteElementI(OpCode op, RegSlot Value, RegSlot Instance, RegSlot Element);
        template <typename SizePolicy> bool TryWriteElementUnsigned1(OpCode op, RegSlot Value, RegSlot Instance, uint32 Element);
        template <typename SizePolicy> bool TryWriteElementC(OpCode op, RegSlot value, RegSlot instance, PropertyIdIndexType propertyIdIndex);
        template <typename SizePolicy> bool TryWriteElementScopedC(OpCode op, RegSlot value, PropertyIdIndexType propertyIdIndex);
        template <typename SizePolicy> bool TryWriteElementSlot(OpCode op, RegSlot value, RegSlot instance, int32 slotId);
        template <typename SizePolicy> bool TryWriteElementSlotI1(OpCode op, RegSlot value, int32 slotId);
        template <typename SizePolicy> bool TryWriteElementSlotI2(OpCode op, RegSlot value, int32 slotId1, int32 slotId2);
        template <typename SizePolicy> bool TryWriteElementU(OpCode op, RegSlot instance, PropertyIdIndexType index);
        template <typename SizePolicy> bool TryWriteElementScopedU(OpCode op, PropertyIdIndexType index);
        template <typename SizePolicy> bool TryWriteElementRootU(OpCode op, PropertyIdIndexType index);
        template <typename SizePolicy> bool TryWriteElementRootCP(OpCode op, RegSlot value, uint cacheId, bool isLoadMethod, bool isStore);
        template <typename SizePolicy> bool TryWriteElementP(OpCode op, RegSlot value, CacheId cacheId);
        template <typename SizePolicy> bool TryWriteElementPIndexed(OpCode op, RegSlot value, uint32 scopeIndex, CacheId cacheId);
        template <typename SizePolicy> bool TryWriteElementCP(OpCode op, RegSlot value, RegSlot instance, CacheId cacheId);
        template <typename SizePolicy> bool TryWriteElementScopedC2(OpCode op, RegSlot value, PropertyIdIndexType propertyIdIndex, RegSlot instance2);
        template <typename SizePolicy> bool TryWriteElementC2(OpCode op, RegSlot value, RegSlot instance, PropertyIdIndexType propertyIdIndex, RegSlot instance2);
        template <typename SizePolicy> bool TryWriteClass(OpCode op, RegSlot constructor, RegSlot extends);
        template <typename SizePolicy> bool TryWriteReg1Unsigned1(OpCode op, RegSlot R0, uint C1);
        template <typename SizePolicy> bool TryWriteReg2Int1(OpCode op, RegSlot R0, RegSlot R1, int C1);

        void AuxiliaryContext(OpCode op, RegSlot destinationRegister, const void* buffer, int byteCount, Js::RegSlot C1);
        int Auxiliary(OpCode op, RegSlot destinationRegister, const void* buffer, int byteCount, int size);
        void Auxiliary(OpCode op, RegSlot destinationRegister, uint byteOffset, int size);
        int AuxNoReg(OpCode op, const void* buffer, int byteCount, int size);
        void AuxNoReg(OpCode op, uint byteOffset, int size);
        int Reg2Aux(OpCode op, RegSlot R0, RegSlot R1, const void* buffer, int byteCount, int size);
        void Reg2Aux(OpCode op, RegSlot R0, RegSlot R1, uint byteOffset, int size);
        uint InsertAuxiliaryData(const void* buffer, uint byteCount);

        void InitClass(RegSlot constructor, RegSlot extends = Js::Constants::NoRegister);
        void NewFunction(RegSlot destinationRegister, uint index, bool isGenerator);
        void NewInnerFunction(RegSlot destinationRegister, uint index, RegSlot environmentRegister, bool isGenerator);
        ByteCodeLabel DefineLabel();
        void MarkLabel(ByteCodeLabel labelID);
        void StartStatement(ParseNode* node, uint32 tmpRegCount);
        void EndStatement(ParseNode* node);
        void StartSubexpression(ParseNode* node);
        void EndSubexpression(ParseNode* node);
        void RecordFrameDisplayRegister(RegSlot slot);
        void RecordObjectRegister(RegSlot slot);
        uint GetCurrentOffset() const { return (uint)m_byteCodeData.GetCurrentOffset(); }
        DataChunk * GetCurrentChunk() const { return m_byteCodeData.GetCurrentChunk(); }
        void SetCurrent(uint offset, DataChunk * chunk) { m_byteCodeData.SetCurrent(offset, chunk); }
        bool ShouldIncrementCallSiteId(OpCode op);
        inline void SetCallSiteCount(Js::ProfileId callSiteId) { this->m_functionWrite->SetProfiledCallSiteCount(callSiteId); }

        // Debugger methods.
        DebuggerScope* RecordStartScopeObject(DiagExtraScopesType scopeType, RegSlot scopeLocation = Js::Constants::NoRegister, int* index = nullptr);
        void AddPropertyToDebuggerScope(DebuggerScope* debuggerScope, RegSlot location, Js::PropertyId propertyId, bool shouldConsumeRegister = true, DebuggerScopePropertyFlags flags = DebuggerScopePropertyFlags_None, bool isFunctionDeclaration = false);
        void RecordEndScopeObject();
        DebuggerScope* GetCurrentDebuggerScope() const { return m_currentDebuggerScope; }
        void UpdateDebuggerPropertyInitializationOffset(Js::DebuggerScope* currentDebuggerScope, Js::RegSlot location, Js::PropertyId propertyId, bool shouldConsumeRegister = true, int byteCodeOffset = Constants::InvalidOffset, bool isFunctionDeclaration = false);
        FunctionBody* GetFunctionWrite() const { return m_functionWrite; }

        void RecordStatementAdjustment(FunctionBody::StatementAdjustmentType type);
        void RecordCrossFrameEntryExitRecord(bool isEnterBlock);
        void RecordForInOrOfCollectionScope();

        uint EnterLoop(Js::ByteCodeLabel loopEntrance);
        void ExitLoop(uint loopId);

        bool DoJitLoopBodies() const { return m_doJitLoopBodies; }
        bool DoInterruptProbes() const { return m_doInterruptProbe; }

        static bool DoProfileCallOp(OpCode op)
        {
            return op >= OpCode::CallI && op <= OpCode::CallIExtendedFlags;
        }

        bool DoProfileNewScObjectOp(OpCode op)
        {
            return
                !PHASE_OFF(InlineConstructorsPhase, m_functionWrite) &&
                (op == OpCode::NewScObject || op == OpCode::NewScObjectSpread);
        }

        bool DoProfileNewScObjArrayOp(OpCode op)
        {
            return
                !PHASE_OFF(NativeArrayPhase, m_functionWrite) &&
                !m_functionWrite->GetScriptContext()->IsInDebugMode() &&
                (op == OpCode::NewScObjArray || op == OpCode::NewScObjArraySpread);
        }

        bool DoProfileNewScArrayOp(OpCode op)
        {
            return
                !PHASE_OFF(NativeArrayPhase, m_functionWrite) &&
                !PHASE_OFF(NativeNewScArrayPhase, m_functionWrite) &&
                !m_functionWrite->GetScriptContext()->IsInDebugMode() &&
                (op == OpCode::NewScIntArray || op == OpCode::NewScFltArray || op == OpCode::NewScArray);
        }

        uint ByteCodeWriter::GetTotalSize()
        {
            return m_byteCodeData.GetCurrentOffset() + m_auxiliaryData.GetCurrentOffset() + m_auxContextData.GetCurrentOffset();
        }

#if DBG
        bool IsInitialized() const { return isInitialized; }
        bool IsInUse() const { return isInUse; }
#endif
    };
}

namespace JsUtil
{
    template <>
    class ValueEntry<Js::ByteCodeWriter::CacheIdUnit>: public BaseValueEntry<Js::ByteCodeWriter::CacheIdUnit>
    {
    public:
        void Clear()
        {
            this->value = 0;
        }
    };
};
