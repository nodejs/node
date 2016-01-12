//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    void ByteCodeWriter::Create()
    {
        m_loopNest = 0;
        m_byteCodeCount = 0;
        m_byteCodeWithoutLDACount = 0;
        m_byteCodeInLoopCount = 0;
        m_functionWrite = nullptr;
        m_pMatchingNode = nullptr;
        m_matchingNodeRefCount = 0;
        m_tmpRegCount = 0;
        DebugOnly(isInitialized = false);
        DebugOnly(isInUse = false);
    }

    void ByteCodeWriter::InitData(ArenaAllocator* alloc, long initCodeBufferSize)
    {
        Assert(!isInUse);
        Assert(!isInitialized);
        DebugOnly(isInitialized = true);

        m_labelOffsets = JsUtil::List<uint, ArenaAllocator>::New(alloc);
        m_jumpOffsets = JsUtil::List<JumpInfo, ArenaAllocator>::New(alloc);
        m_loopHeaders = JsUtil::List<LoopHeaderData, ArenaAllocator>::New(alloc);
        m_byteCodeData.Create(initCodeBufferSize, alloc);
        m_subexpressionNodesStack = Anew(alloc, JsUtil::Stack<SubexpressionNode>, alloc);

        // These data units have exponential growth strategy - let's start small and grow them
        m_auxiliaryData.Create(256, alloc);
        m_auxContextData.Create(256, alloc);
        callRegToLdFldCacheIndexMap = Anew(alloc, CallRegToLdFldCacheIndexMap,
            alloc,
            17);
#ifdef BYTECODE_BRANCH_ISLAND
        useBranchIsland = true;
        inEnsureLongBranch = false;
        lastOpcode = Js::OpCode::FunctionEntry;
        this->UpdateNextBranchIslandOffset(0, 0);
        m_longJumpOffsets = JsUtil::List<JumpInfo, ArenaAllocator>::New(alloc);
#endif
    }

    ///----------------------------------------------------------------------------
    ///
    /// Begin() configures this instance to generate byte-code for a specific
    /// JavascriptFunction:
    ///
    /// - Byte-code will be written until the caller uses End() to close and commit
    ///   the stream to the given function, or Reset() to discard and reset to an
    ///   empty state.
    ///
    /// - Each ByteCodeWriter may be used multiple times, but may only generate a
    ///   single byte-code stream for a single function at a time.
    ///
    ///----------------------------------------------------------------------------

    void ByteCodeWriter::Begin(ByteCodeGenerator* byteCodeGenerator, FunctionBody* functionWrite, ArenaAllocator* alloc, bool doJitLoopBodies, bool hasLoop)
    {
        Assert(!isInUse);
        AssertMsg(m_functionWrite == nullptr, "Cannot nest Begin() calls");
        AssertMsg(functionWrite != nullptr, "Must have valid function to write");
        AssertMsg(functionWrite->GetByteCode() == nullptr, "Function should not already have a byte-code body");
        AssertMsg(functionWrite->GetLocalsCount() > 0, "Must always have R0 for return-value");

        DebugOnly(isInUse = true);
        m_functionWrite = functionWrite;
        m_doJitLoopBodies = doJitLoopBodies;
        m_doInterruptProbe = functionWrite->GetScriptContext()->GetThreadContext()->DoInterruptProbe(functionWrite);
        m_hasLoop = hasLoop;
        m_isInDebugMode = byteCodeGenerator->IsInDebugMode();
    }

    template <typename T>
    void ByteCodeWriter::PatchJumpOffset(JsUtil::List<JumpInfo, ArenaAllocator> * jumpOffset, byte * byteBuffer, uint byteCount)
    {
        jumpOffset->Map([=](int index, JumpInfo& jumpInfo)
        {
            //
            // Read "labelID" stored at the offset within the byte-code.
            //
            uint jumpByteOffset = jumpInfo.patchOffset;
            AssertMsg(jumpByteOffset < byteCount - sizeof(T),
                "Must have valid jump site within byte-code to back-patch");

            unaligned T * pnBackPatch = reinterpret_cast<unaligned T *>(&byteBuffer[jumpByteOffset]);

            ByteCodeLabel labelID = jumpInfo.labelId;
            CheckLabel(labelID);

            uint offsetToEndOfLayoutByteSize = *pnBackPatch;
            Assert(offsetToEndOfLayoutByteSize < 0x20);

            //
            // Use "labelID" to lookup the destination offset, replacing the temporary data in the
            // byte-code.
            //

            uint labelByteOffset = m_labelOffsets->Item(labelID);
            AssertMsg(labelByteOffset != UINT_MAX, "ERROR: Destination labels must be marked before closing");

            int relativeJumpOffset = labelByteOffset - jumpByteOffset - offsetToEndOfLayoutByteSize;
#ifdef BYTECODE_BRANCH_ISLAND
            Assert(!useBranchIsland || (jumpOffset != m_jumpOffsets || (relativeJumpOffset < GetBranchLimit() && relativeJumpOffset >= -GetBranchLimit())));
#endif
            Assert((T)relativeJumpOffset == relativeJumpOffset);
            *pnBackPatch = (T)relativeJumpOffset;
        });
    }

    ///----------------------------------------------------------------------------
    ///
    /// End() completes generating byte-code for the given JavascriptFunction and
    /// commits it to the function's body.
    ///
    ///----------------------------------------------------------------------------
#ifdef LOG_BYTECODE_AST_RATIO
    void ByteCodeWriter::End(long currentAstSize, long maxAstSize)
#else
    void ByteCodeWriter::End()
#endif
    {
        Assert(isInUse);

        CheckOpen();
        Empty(OpCode::EndOfBlock);

        ByteBlock* finalByteCodeBlock;

        ScriptContext* scriptContext = m_functionWrite->GetScriptContext();
        m_byteCodeData.Copy(scriptContext->GetRecycler(), &finalByteCodeBlock);

        byte * byteBuffer = finalByteCodeBlock->GetBuffer();
        uint byteCount = m_byteCodeData.GetCurrentOffset();

        //
        // Update all branch targets with their actual label destinations.
        //
#ifdef BYTECODE_BRANCH_ISLAND
        if (useBranchIsland)
        {
            PatchJumpOffset<JumpOffset>(m_jumpOffsets, byteBuffer, byteCount);
            PatchJumpOffset<LongJumpOffset>(m_longJumpOffsets, byteBuffer, byteCount);
        }
        else
        {
            PatchJumpOffset<LongJumpOffset>(m_jumpOffsets, byteBuffer, byteCount);
        }
#else
        PatchJumpOffset<JumpOffset>(m_jumpOffsets, byteBuffer, byteCount);
#endif

        // Patch up the root object load inline cache with the start index
        uint rootObjectLoadInlineCacheStart = this->m_functionWrite->GetRootObjectLoadInlineCacheStart();
        rootObjectLoadInlineCacheOffsets.Map([=](size_t offset)
        {
            Assert(offset < byteCount - sizeof(int));
            unaligned uint * pnBackPatch = reinterpret_cast<unaligned uint *>(&byteBuffer[offset]);
            *pnBackPatch += rootObjectLoadInlineCacheStart;
        });

        // Patch up the root object load method inline cache with the start index
        uint rootObjectLoadMethodInlineCacheStart = this->m_functionWrite->GetRootObjectLoadMethodInlineCacheStart();
        rootObjectLoadMethodInlineCacheOffsets.Map([=](size_t offset)
        {
            Assert(offset < byteCount - sizeof(int));
            unaligned uint * pnBackPatch = reinterpret_cast<unaligned uint *>(&byteBuffer[offset]);
            *pnBackPatch += rootObjectLoadMethodInlineCacheStart;
        });

        // Patch up the root object store inline cache with the start index
        uint rootObjectStoreInlineCacheStart = this->m_functionWrite->GetRootObjectStoreInlineCacheStart();
        rootObjectStoreInlineCacheOffsets.Map([=](size_t offset)
        {
            Assert(offset < byteCount - sizeof(int));
            unaligned uint * pnBackPatch = reinterpret_cast<unaligned uint *>(&byteBuffer[offset]);
            *pnBackPatch += rootObjectStoreInlineCacheStart;
        });

        //
        // Store the final trimmed byte-code on the function.
        //
        ByteBlock* finalAuxiliaryBlock;
        ByteBlock* finalAuxiliaryContextBlock;

        m_auxiliaryData.Copy(m_functionWrite->GetScriptContext()->GetRecycler(), &finalAuxiliaryBlock);
        m_auxContextData.Copy(m_functionWrite->GetScriptContext()->GetRecycler(), &finalAuxiliaryContextBlock);

        m_functionWrite->AllocateInlineCache();
        m_functionWrite->AllocateObjectLiteralTypeArray();

        if (!PHASE_OFF(Js::ScriptFunctionWithInlineCachePhase, m_functionWrite) && !PHASE_OFF(Js::InlineApplyTargetPhase, m_functionWrite))
        {
            if (m_functionWrite->CanFunctionObjectHaveInlineCaches())
            {
                m_functionWrite->SetInlineCachesOnFunctionObject(true);
            }
        }

        if (this->DoJitLoopBodies() &&
            !this->m_functionWrite->GetFunctionBody()->GetHasFinally() &&
            !(this->m_functionWrite->GetFunctionBody()->GetHasTry() && PHASE_OFF(Js::JITLoopBodyInTryCatchPhase, this->m_functionWrite)))
        {
            AllocateLoopHeaders();
        }

        m_functionWrite->MarkScript(finalByteCodeBlock, finalAuxiliaryBlock, finalAuxiliaryContextBlock,
            m_byteCodeCount, m_byteCodeInLoopCount, m_byteCodeWithoutLDACount);


#if ENABLE_PROFILE_INFO
        m_functionWrite->LoadDynamicProfileInfo();
#endif

        JS_ETW(EventWriteJSCRIPT_BYTECODEGEN_METHOD(m_functionWrite->GetHostSourceContext(), m_functionWrite->GetScriptContext(), m_functionWrite->GetLocalFunctionId(), m_functionWrite->GetByteCodeCount(), this->GetTotalSize(), m_functionWrite->GetExternalDisplayName()));

#ifdef LOG_BYTECODE_AST_RATIO
        // log the bytecode AST ratio
        if (currentAstSize == maxAstSize)
        {
            float astBytecodeRatio = (float)currentAstSize / (float)byteCount;
            Output::Print(L"\tAST Bytecode ratio: %f\n", astBytecodeRatio);
        }
#endif

        // TODO: add validation for source mapping under #dbg
        //
        // Reset the writer to prepare for the next user.
        //

        Reset();
    }

    void ByteCodeWriter::AllocateLoopHeaders()
    {
        m_functionWrite->AllocateLoopHeaders();
        m_loopHeaders->Map([this](int index, ByteCodeWriter::LoopHeaderData& data)
        {
            LoopHeader *loopHeader = m_functionWrite->GetLoopHeader(index);
            loopHeader->startOffset = data.startOffset;
            loopHeader->endOffset = data.endOffset;
            loopHeader->isNested = data.isNested;
        });
    }

    ///----------------------------------------------------------------------------
    ///
    /// Reset() discards any current byte-code and resets to a known "empty" state:
    /// - This method may be called at any time between Create() and Dispose().
    ///
    ///----------------------------------------------------------------------------

    void ByteCodeWriter::Reset()
    {
        DebugOnly(isInUse = false);
        Assert(isInitialized);
        m_byteCodeData.Reset();
        m_auxiliaryData.Reset();
        m_auxContextData.Reset();
#ifdef BYTECODE_BRANCH_ISLAND
        lastOpcode = Js::OpCode::FunctionEntry;
        this->UpdateNextBranchIslandOffset(0, 0);
        m_longJumpOffsets->Clear();
#endif
        m_labelOffsets->Clear();
        m_jumpOffsets->Clear();
        m_loopHeaders->Clear();
        rootObjectLoadInlineCacheOffsets.Clear(m_labelOffsets->GetAllocator());
        rootObjectStoreInlineCacheOffsets.Clear(m_labelOffsets->GetAllocator());
        rootObjectLoadMethodInlineCacheOffsets.Clear(m_labelOffsets->GetAllocator());
        callRegToLdFldCacheIndexMap->ResetNoDelete();
        m_pMatchingNode = nullptr;
        m_matchingNodeRefCount = 0;
        m_functionWrite = nullptr;
        m_byteCodeCount = 0;
        m_byteCodeWithoutLDACount = 0;
        m_byteCodeInLoopCount = 0;
        m_loopNest = 0;
        m_currentDebuggerScope = nullptr;
    }

    inline Js::RegSlot ByteCodeWriter::ConsumeReg(Js::RegSlot reg)
    {
        CheckReg(reg);
        Assert(this->m_functionWrite);
        return this->m_functionWrite->MapRegSlot(reg);
    }

    inline void ByteCodeWriter::CheckOpen()
    {
        AssertMsg(m_functionWrite != nullptr, "Must Begin() a function to write byte-code into");
    }

    inline void ByteCodeWriter::CheckOp(OpCode op, OpLayoutType layoutType)
    {
        AssertMsg(OpCodeUtil::IsValidByteCodeOpcode(op), "Ensure valid OpCode");
#if ENABLE_NATIVE_CODEGEN
        AssertMsg(!OpCodeAttr::BackEndOnly(op), "Can't write back end only OpCode");
#endif
        AssertMsg(OpCodeUtil::GetOpCodeLayout(op) == layoutType, "Ensure correct layout for OpCode");
    }

    inline void ByteCodeWriter::CheckLabel(ByteCodeLabel labelID)
    {
        AssertMsg(labelID < m_labelOffsets->Count(),
            "Label must be previously defined before being marked in the byte-code");
    }

    inline void ByteCodeWriter::CheckReg(RegSlot registerID)
    {
        AssertMsg(registerID != Js::Constants::NoRegister, "bad register");
        if (registerID == Js::Constants::NoRegister)
            Js::Throw::InternalError();
    }

    void ByteCodeWriter::Empty(OpCode op)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Empty);

        m_byteCodeData.Encode(op, this);
    }

#define MULTISIZE_LAYOUT_WRITE(layout, ...) \
    if (!TryWrite##layout<SmallLayoutSizePolicy>(__VA_ARGS__) && !TryWrite##layout<MediumLayoutSizePolicy>(__VA_ARGS__)) \
    { \
        bool success = TryWrite##layout<LargeLayoutSizePolicy>(__VA_ARGS__); \
        Assert(success); \
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg1(OpCode op, RegSlot R0)
    {
        OpLayoutT_Reg1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg1(OpCode op, RegSlot R0)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);

        MULTISIZE_LAYOUT_WRITE(Reg1, op, R0);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg2WithICIndex(OpCode op, RegSlot R0, RegSlot R1, uint32 inlineCacheIndex, bool isRootLoad)
    {
        OpLayoutT_Reg2WithICIndex<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.inlineCacheIndex, inlineCacheIndex))
        {
            size_t offset = m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);

            if (isRootLoad)
            {
                size_t inlineCacheOffset = offset + OpCodeUtil::EncodedSize(op, SizePolicy::LayoutEnum)
                    + offsetof(OpLayoutT_Reg2WithICIndex<SizePolicy>, inlineCacheIndex);

                rootObjectLoadMethodInlineCacheOffsets.Prepend(m_labelOffsets->GetAllocator(), inlineCacheOffset);
            }
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg2(OpCode op, RegSlot R0, RegSlot R1)
    {
        OpLayoutT_Reg2<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg2(OpCode op, RegSlot R0, RegSlot R1)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg2);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        if (DoDynamicProfileOpcode(CheckThisPhase) ||
            DoDynamicProfileOpcode(TypedArrayTypeSpecPhase) ||
            DoDynamicProfileOpcode(ArrayCheckHoistPhase))
        {
            if (op == OpCode::StrictLdThis)
            {
                op = OpCode::ProfiledStrictLdThis;
            }
        }

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);

        CacheIdUnit unit;
        unit.cacheId = Js::Constants::NoInlineCacheIndex;
        callRegToLdFldCacheIndexMap->TryGetValueAndRemove(R1, &unit);

        bool isProfiled = false;
        bool isProfiled2 = false;
        bool isReg2WithICIndex = false;
        Js::ProfileId profileId = Js::Constants::NoProfileId;
        Js::ProfileId profileId2 = Js::Constants::NoProfileId;

        if (op == Js::OpCode::BeginSwitch && DoDynamicProfileOpcode(SwitchOptPhase) &&
            this->m_functionWrite->AllocProfiledSwitch(&profileId))
        {
            OpCodeUtil::ConvertNonCallOpToProfiled(op);
            isProfiled = true;
        }

        Assert(DoProfileNewScObjArrayOp(op) == false);

        Assert(DoProfileNewScObjectOp(op) == false);

        if (op == Js::OpCode::LdLen_A
            && (DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) ||
                DoDynamicProfileOpcode(FloatTypeSpecPhase) ||
                DoDynamicProfileOpcode(TypedArrayTypeSpecPhase) ||
                DoDynamicProfileOpcode(ArrayCheckHoistPhase))
            && this->m_functionWrite->AllocProfiledLdElemId(&profileId))
        {
            OpCodeUtil::ConvertNonCallOpToProfiled(op);
            isProfiled = true;
        }

        if (isReg2WithICIndex)
        {
            MULTISIZE_LAYOUT_WRITE(Reg2WithICIndex, op, R0, R1, unit.cacheId, unit.isRootObjectCache);
        }
        else
        {
            MULTISIZE_LAYOUT_WRITE(Reg2, op, R0, R1);
        }

        if (isProfiled)
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
            if (isProfiled2)
            {
                m_byteCodeData.Encode(&profileId2, sizeof(Js::ProfileId));
            }
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg3(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2)
    {
        OpLayoutT_Reg3<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg3(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg3);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);
        R2 = ConsumeReg(R2);

        ProfileId profileId = 0;
        bool isProfiled = false;
        if ((DoDynamicProfileOpcode(FloatTypeSpecPhase) && (op == Js::OpCode::Div_A || op == Js::OpCode::Rem_A)) &&
            this->m_functionWrite->AllocProfiledDivOrRem(&profileId))
        {
            isProfiled = true;
            OpCodeUtil::ConvertNonCallOpToProfiled(op);
        }

        MULTISIZE_LAYOUT_WRITE(Reg3, op, R0, R1, R2);
        if (isProfiled)
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg3C(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, CacheId cacheId)
    {
        OpLayoutT_Reg3C<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2)
            && SizePolicy::Assign(layout.inlineCacheIndex, cacheId))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg3C(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, uint cacheId)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg3C);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);
        R2 = ConsumeReg(R2);

        MULTISIZE_LAYOUT_WRITE(Reg3C, op, R0, R1, R2, cacheId);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg4(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3)
    {
        OpLayoutT_Reg4<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2)
            && SizePolicy::Assign(layout.R3, R3))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg4(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg4);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);
        R2 = ConsumeReg(R2);
        R3 = ConsumeReg(R3);

        MULTISIZE_LAYOUT_WRITE(Reg4, op, R0, R1, R2, R3);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg2B1(OpCode op, RegSlot R0, RegSlot R1, uint8 B2)
    {
        OpLayoutT_Reg2B1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.B2, B2))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg2B1(OpCode op, RegSlot R0, RegSlot R1, uint8 B2)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg2B1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);

        MULTISIZE_LAYOUT_WRITE(Reg2B1, op, R0, R1, B2);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg3B1(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, uint8 B3)
    {
        OpLayoutT_Reg3B1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2)
            && SizePolicy::Assign(layout.B3, B3))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg3B1(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, uint8 B3)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg3B1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);
        R2 = ConsumeReg(R2);

        MULTISIZE_LAYOUT_WRITE(Reg3B1, op, R0, R1, R2, B3);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg5(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4)
    {
        OpLayoutT_Reg5<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2)
            && SizePolicy::Assign(layout.R3, R3) && SizePolicy::Assign(layout.R4, R4))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg5(OpCode op, RegSlot R0, RegSlot R1, RegSlot R2, RegSlot R3, RegSlot R4)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg5);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);
        R2 = ConsumeReg(R2);
        R3 = ConsumeReg(R3);
        R4 = ConsumeReg(R4);

        MULTISIZE_LAYOUT_WRITE(Reg5, op, R0, R1, R2, R3, R4);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteUnsigned1(OpCode op, uint C1)
    {
        OpLayoutT_Unsigned1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.C1, C1))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Unsigned1(OpCode op, uint C1)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Unsigned1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        MULTISIZE_LAYOUT_WRITE(Unsigned1, op, C1);
    }

    void ByteCodeWriter::ArgIn0(RegSlot reg)
    {
        AssertMsg(0 < m_functionWrite->GetInParamsCount(),
            "Ensure source arg was declared in prologue");

        Reg1(OpCode::ArgIn0, reg);
    }

    template void ByteCodeWriter::ArgOut<true>(ArgSlot arg, RegSlot reg, ProfileId callSiteId);
    template void ByteCodeWriter::ArgOut<false>(ArgSlot arg, RegSlot reg, ProfileId callSiteId);

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteArg(OpCode op, ArgSlot arg, RegSlot reg)
    {
        OpLayoutT_Arg<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Arg, arg) && SizePolicy::Assign(layout.Reg, reg))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <bool isVar>
    void ByteCodeWriter::ArgOut(ArgSlot arg, RegSlot reg, ProfileId callSiteId)
    {
        CheckOpen();
        Assert(OpCodeAttr::HasMultiSizeLayout(OpCode::ArgOut_A) && OpCodeAttr::HasMultiSizeLayout(OpCode::ArgOut_ANonVar));

        // Note: don't "consume" the arg slot, as the passed-in value is the final one.
        reg = ConsumeReg(reg);

        OpCode op;
        if (isVar)
        {
            op = OpCode::ArgOut_A;
        }
        else
        {
            op = OpCode::ArgOut_ANonVar;
            MULTISIZE_LAYOUT_WRITE(Arg, op, arg, reg);
            return;
        }

        if (DoDynamicProfileOpcode(InlinePhase)
            && arg > 0 && arg < Js::Constants::MaximumArgumentCountForConstantArgumentInlining
            && (reg > FunctionBody::FirstRegSlot && reg < m_functionWrite->GetConstantCount())
            && callSiteId != Js::Constants::NoProfileId
            && !m_isInDebugMode // We don't inline in debug mode, so no need to emit ProfiledArgOut_A
            )
        {
            Assert((reg > FunctionBody::FirstRegSlot && reg < m_functionWrite->GetConstantCount()));
            MULTISIZE_LAYOUT_WRITE(Arg, Js::OpCode::ProfiledArgOut_A, arg, reg);
            m_byteCodeData.Encode(&callSiteId, sizeof(Js::ProfileId));
        }
        else
        {
            MULTISIZE_LAYOUT_WRITE(Arg, op, arg, reg);
            return;
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteArgNoSrc(OpCode op, ArgSlot arg)
    {
        OpLayoutT_ArgNoSrc<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Arg, arg))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ArgOutEnv(ArgSlot arg)
    {
        CheckOpen();
        Assert(OpCodeAttr::HasMultiSizeLayout(OpCode::ArgOut_Env));

        MULTISIZE_LAYOUT_WRITE(ArgNoSrc, OpCode::ArgOut_Env, arg);
    }

    void ByteCodeWriter::Br(ByteCodeLabel labelID)
    {
        Br(OpCode::Br, labelID);
    }

    // For switch case - default branching
    void ByteCodeWriter::Br(OpCode op, ByteCodeLabel labelID)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Br);
        CheckLabel(labelID);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));

        size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutBr) - offsetof(OpLayoutBr, RelativeJumpOffset);
        OpLayoutBr data;
        data.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
        AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
    }

    void ByteCodeWriter::BrS(OpCode op, ByteCodeLabel labelID, byte val)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::BrS);
        CheckLabel(labelID);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));

        size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutBrS) - offsetof(OpLayoutBrS, RelativeJumpOffset);
        OpLayoutBrS data;
        data.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
        data.val = val;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
        AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteBrReg1(OpCode op, ByteCodeLabel labelID, RegSlot R1)
    {
        OpLayoutT_BrReg1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R1, R1))
        {
            size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutT_BrReg1<SizePolicy>) - offsetof(OpLayoutT_BrReg1<SizePolicy>, RelativeJumpOffset);
            layout.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
            return true;
        }
        return false;
    }
    void ByteCodeWriter::BrReg1(OpCode op, ByteCodeLabel labelID, RegSlot R1)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::BrReg1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));
        CheckLabel(labelID);

        R1 = ConsumeReg(R1);

        MULTISIZE_LAYOUT_WRITE(BrReg1, op, labelID, R1);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteBrReg2(OpCode op, ByteCodeLabel labelID, RegSlot R1, RegSlot R2)
    {
        OpLayoutT_BrReg2<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.R2, R2))
        {
            size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutT_BrReg2<SizePolicy>) - offsetof(OpLayoutT_BrReg2<SizePolicy>, RelativeJumpOffset);
            layout.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::BrReg2(OpCode op, ByteCodeLabel labelID, RegSlot R1, RegSlot R2)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::BrReg2);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));
        CheckLabel(labelID);

        R1 = ConsumeReg(R1);
        R2 = ConsumeReg(R2);

        MULTISIZE_LAYOUT_WRITE(BrReg2, op, labelID, R1, R2);
    }

    void ByteCodeWriter::BrProperty(OpCode op, ByteCodeLabel labelID, RegSlot instance, PropertyIdIndexType index)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::BrProperty);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));
        CheckLabel(labelID);

        instance = ConsumeReg(instance);

        size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutBrProperty) - offsetof(OpLayoutBrProperty, RelativeJumpOffset);
        OpLayoutBrProperty data;
        data.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
        data.Instance = instance;
        data.PropertyIdIndex = index;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
        AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
    }

    void ByteCodeWriter::BrLocalProperty(OpCode op, ByteCodeLabel labelID, PropertyIdIndexType index)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::BrLocalProperty);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));
        CheckLabel(labelID);

        size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutBrLocalProperty) - offsetof(OpLayoutBrLocalProperty, RelativeJumpOffset);
        OpLayoutBrLocalProperty data;
        data.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
        data.PropertyIdIndex = index;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
        AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
    }

    void ByteCodeWriter::BrEnvProperty(OpCode op, ByteCodeLabel labelID, PropertyIdIndexType index, int32 slotIndex)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::BrEnvProperty);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));
        CheckLabel(labelID);

        size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutBrEnvProperty) - offsetof(OpLayoutBrEnvProperty, RelativeJumpOffset);
        OpLayoutBrEnvProperty data;
        data.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;
        data.SlotIndex = slotIndex;
        data.PropertyIdIndex = index;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
        AddJumpOffset(op, labelID, offsetOfRelativeJumpOffsetFromEnd);
    }

    bool ByteCodeWriter::DoDynamicProfileOpcode(Phase tag, bool noHeuristics) const
    {
#if ENABLE_PROFILE_INFO
        if (!DynamicProfileInfo::IsEnabled(tag, this->m_functionWrite))
        {
            return false;
        }

        // Other heuristics
        switch (tag)
        {
        case Phase::InlinePhase:
            // Do profile opcode everywhere if we are an inline candidate
            // Otherwise, only in loops if the function has loop
#pragma prefast(suppress:6236, "DevDiv bug 830883. False positive when PHASE_OFF is #defined as '(false)'.")
            return PHASE_FORCE(Phase::InlinePhase, this->m_functionWrite) ||
                (!this->m_functionWrite->GetDontInline() &&
                    (noHeuristics || !this->m_hasLoop || (this->m_loopNest != 0) ||
                        !(PHASE_OFF(InlineOutsideLoopsPhase, this->m_functionWrite))));

        default:
            return true;
        }
#else
        return false;
#endif
    }

    bool ByteCodeWriter::ShouldIncrementCallSiteId(OpCode op)
    {
        if ((DoProfileCallOp(op) && DoDynamicProfileOpcode(InlinePhase)) ||
            (DoProfileNewScObjArrayOp(op) && (DoDynamicProfileOpcode(NativeArrayPhase, true) || DoDynamicProfileOpcode(InlinePhase, true))) ||
            (DoProfileNewScObjectOp(op) && (DoDynamicProfileOpcode(InlinePhase, true) || DoDynamicProfileOpcode(FixedNewObjPhase, true))))
        {
            return true;
        }
        return false;
    }

    void ByteCodeWriter::StartCall(OpCode op, ArgSlot ArgCount)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::StartCall);

        OpLayoutStartCall data;
        data.ArgCount = ArgCount;
        m_byteCodeData.Encode(op, &data, sizeof(data), this);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallIExtended(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallIExtendedOptions options, uint32 spreadArgsOffset)
    {
        OpLayoutT_CallIExtended<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::Assign(layout.Options, options)
            && SizePolicy::Assign(layout.SpreadAuxOffset, spreadArgsOffset))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallIExtendedWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, InlineCacheIndex inlineCacheIndex, bool isRootLoad, CallIExtendedOptions options, uint32 spreadArgsOffset)
    {
        OpLayoutT_CallIExtendedWithICIndex<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::Assign(layout.inlineCacheIndex, inlineCacheIndex)
            && SizePolicy::Assign(layout.Options, options) && SizePolicy::Assign(layout.SpreadAuxOffset, spreadArgsOffset))
        {
            size_t offset = m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);

            if (isRootLoad)
            {
                size_t inlineCacheOffset = offset + OpCodeUtil::EncodedSize(op, SizePolicy::LayoutEnum)
                    + offsetof(OpLayoutT_CallIExtendedWithICIndex<SizePolicy>, inlineCacheIndex);

                rootObjectLoadMethodInlineCacheOffsets.Prepend(m_labelOffsets->GetAllocator(), inlineCacheOffset);
            }
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallIExtendedFlags(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallIExtendedOptions options, uint32 spreadArgsOffset, CallFlags callFlags)
    {
        OpLayoutT_CallIExtendedFlags<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::Assign(layout.Options, options)
            && SizePolicy::Assign(layout.SpreadAuxOffset, spreadArgsOffset) && SizePolicy::Assign(layout.callFlags, callFlags))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallIExtendedFlagsWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, InlineCacheIndex inlineCacheIndex, bool isRootLoad, CallIExtendedOptions options, uint32 spreadArgsOffset, CallFlags callFlags)
    {
        OpLayoutT_CallIExtendedFlagsWithICIndex<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::Assign(layout.inlineCacheIndex, inlineCacheIndex)
            && SizePolicy::Assign(layout.Options, options) && SizePolicy::Assign(layout.SpreadAuxOffset, spreadArgsOffset)
            && SizePolicy::Assign(layout.callFlags, callFlags))
        {
            size_t offset = m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);

            if (isRootLoad)
            {
                size_t inlineCacheOffset = offset + OpCodeUtil::EncodedSize(op, SizePolicy::LayoutEnum)
                    + offsetof(OpLayoutT_CallIExtendedFlagsWithICIndex<SizePolicy>, inlineCacheIndex);

                rootObjectLoadMethodInlineCacheOffsets.Prepend(m_labelOffsets->GetAllocator(), inlineCacheOffset);
            }
            return true;
        }
        return false;
    }

    void ByteCodeWriter::CallIExtended(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallIExtendedOptions options, const void *buffer, uint byteCount, ProfileId callSiteId, CallFlags callFlags)
    {
        CheckOpen();
        bool hasCallFlags = !(callFlags == CallFlags_None);
        if (hasCallFlags)
        {
            CheckOp(op, OpLayoutType::CallIExtendedFlags);
        }
        else
        {
            CheckOp(op, OpLayoutType::CallIExtended);
        }
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        // givenArgCount could be <, ==, or > than Function's "InParams" count

        if (returnValueRegister != Js::Constants::NoRegister)
        {
            returnValueRegister = ConsumeReg(returnValueRegister);
        }
        functionRegister = ConsumeReg(functionRegister);

        // CallISpread is not going to use the ldFld cache index, but still remove it from the map as we expect
        // the entry for a cache index to be removed once we have seen the corresponding call.
        CacheIdUnit unit;
        unit.cacheId = Js::Constants::NoInlineCacheIndex;
        callRegToLdFldCacheIndexMap->TryGetValueAndRemove(functionRegister, &unit);

        bool isProfiled = false, isProfiled2 = false;
        ProfileId profileId = callSiteId, profileId2 = Constants::NoProfileId;
        bool isCallWithICIndex = false;

        if (DoProfileCallOp(op))
        {
            if (DoDynamicProfileOpcode(InlinePhase) &&
                callSiteId != Js::Constants::NoProfileId)
            {
                op = Js::OpCodeUtil::ConvertCallOpToProfiled(op);
                isProfiled = true;
            }
            else if ((DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) || DoDynamicProfileOpcode(FloatTypeSpecPhase)) &&
                this->m_functionWrite->AllocProfiledReturnTypeId(&profileId))
            {
                op = Js::OpCodeUtil::ConvertCallOpToProfiledReturnType(op);
                isProfiled = true;
            }
        }
        else if (DoProfileNewScObjArrayOp(op) &&
            (DoDynamicProfileOpcode(NativeArrayPhase, true) || DoDynamicProfileOpcode(InlinePhase, true)) &&
            callSiteId != Js::Constants::NoProfileId &&
            this->m_functionWrite->AllocProfiledArrayCallSiteId(&profileId2))
        {
            OpCodeUtil::ConvertNonCallOpToProfiled(op);
            isProfiled = true;
            isProfiled2 = true;
        }
        else if (DoProfileNewScObjectOp(op) && (DoDynamicProfileOpcode(InlinePhase, true) || DoDynamicProfileOpcode(FixedNewObjPhase, true)) &&
            callSiteId != Js::Constants::NoProfileId)
        {
            OpCodeUtil::ConvertNonCallOpToProfiled(op);
            isProfiled = true;
        }

        uint spreadArgsOffset = 0;
        if (options & CallIExtended_SpreadArgs)
        {
            Assert(buffer != nullptr && byteCount > 0);
            spreadArgsOffset = InsertAuxiliaryData(buffer, byteCount);
        }

        if (isCallWithICIndex)
        {
            if (hasCallFlags == true)
            {
                MULTISIZE_LAYOUT_WRITE(CallIExtendedFlagsWithICIndex, op, returnValueRegister, functionRegister, givenArgCount, unit.cacheId, unit.isRootObjectCache, options, spreadArgsOffset, callFlags);
            }
            else
            {
                MULTISIZE_LAYOUT_WRITE(CallIExtendedWithICIndex, op, returnValueRegister, functionRegister, givenArgCount, unit.cacheId, unit.isRootObjectCache, options, spreadArgsOffset);
            }
        }
        else
        {
            if (hasCallFlags == true)
            {
                MULTISIZE_LAYOUT_WRITE(CallIExtendedFlags, op, returnValueRegister, functionRegister, givenArgCount, options, spreadArgsOffset, callFlags);
            }
            else
            {
                MULTISIZE_LAYOUT_WRITE(CallIExtended, op, returnValueRegister, functionRegister, givenArgCount, options, spreadArgsOffset);
            }
        }

        if (isProfiled)
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
            if (isProfiled2)
            {
                m_byteCodeData.Encode(&profileId2, sizeof(Js::ProfileId));
            }
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallIWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, InlineCacheIndex inlineCacheIndex, bool isRootLoad)
    {
        OpLayoutT_CallIWithICIndex<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::Assign(layout.inlineCacheIndex, inlineCacheIndex))
        {
            size_t offset = m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);

            if (isRootLoad)
            {
                size_t inlineCacheOffset = offset + OpCodeUtil::EncodedSize(op, SizePolicy::LayoutEnum)
                    + offsetof(OpLayoutT_CallIWithICIndex<SizePolicy>, inlineCacheIndex);

                rootObjectLoadMethodInlineCacheOffsets.Prepend(m_labelOffsets->GetAllocator(), inlineCacheOffset);
            }
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallIFlagsWithICIndex(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, InlineCacheIndex inlineCacheIndex, bool isRootLoad, CallFlags callFlags)
    {
        OpLayoutT_CallIFlagsWithICIndex<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::Assign(layout.inlineCacheIndex, inlineCacheIndex)
            && SizePolicy::Assign(layout.callFlags, callFlags))
        {
            size_t offset = m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);

            if (isRootLoad)
            {
                size_t inlineCacheOffset = offset + OpCodeUtil::EncodedSize(op, SizePolicy::LayoutEnum)
                    + offsetof(OpLayoutT_CallIFlagsWithICIndex<SizePolicy>, inlineCacheIndex);

                rootObjectLoadMethodInlineCacheOffsets.Prepend(m_labelOffsets->GetAllocator(), inlineCacheOffset);
            }
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallI(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount)
    {
        OpLayoutT_CallI<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteCallIFlags(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, CallFlags callFlags)
    {
        OpLayoutT_CallIFlags<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Return, returnValueRegister) && SizePolicy::Assign(layout.Function, functionRegister)
            && SizePolicy::Assign(layout.ArgCount, givenArgCount) && SizePolicy::Assign(layout.callFlags, callFlags))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::RemoveEntryForRegSlotFromCacheIdMap(RegSlot regSlot)
    {
        regSlot = ConsumeReg(regSlot);

        CacheIdUnit unit;
        unit.cacheId = Js::Constants::NoInlineCacheIndex;
        callRegToLdFldCacheIndexMap->TryGetValueAndRemove(regSlot, &unit);
    }

    void ByteCodeWriter::CallI(OpCode op, RegSlot returnValueRegister, RegSlot functionRegister, ArgSlot givenArgCount, ProfileId callSiteId, CallFlags callFlags)
    {
        CheckOpen();

        bool hasCallFlags = !(callFlags == CallFlags_None);
        if (hasCallFlags == true)
        {
            CheckOp(op, OpLayoutType::CallIFlags);
        }
        else
        {
            CheckOp(op, OpLayoutType::CallI);
        }

        Assert(OpCodeAttr::HasMultiSizeLayout(op));
        // givenArgCount could be <, ==, or > than Function's "InParams" count

        if (returnValueRegister != Js::Constants::NoRegister)
        {
            returnValueRegister = ConsumeReg(returnValueRegister);
        }
        functionRegister = ConsumeReg(functionRegister);

        bool isProfiled = false;
        bool isProfiled2 = false;
        bool isCallWithICIndex = false;
        ProfileId profileId = callSiteId;
        ProfileId profileId2 = Constants::NoProfileId;

        CacheIdUnit unit;
        unit.cacheId = Js::Constants::NoInlineCacheIndex;
        callRegToLdFldCacheIndexMap->TryGetValueAndRemove(functionRegister, &unit);
        if (DoProfileCallOp(op))
        {
            if (DoDynamicProfileOpcode(InlinePhase) &&
                callSiteId != Js::Constants::NoProfileId)
            {
                if (unit.cacheId == Js::Constants::NoInlineCacheIndex)
                {
                    op = Js::OpCodeUtil::ConvertCallOpToProfiled(op);
                    isProfiled = true;
                }
                else
                {
                    isCallWithICIndex = true;
                    op = Js::OpCodeUtil::ConvertCallOpToProfiled(op, true);
                    isProfiled = true;
                }
            }
            else if ((DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) || DoDynamicProfileOpcode(FloatTypeSpecPhase)) &&
                this->m_functionWrite->AllocProfiledReturnTypeId(&profileId))
            {
                op = Js::OpCodeUtil::ConvertCallOpToProfiledReturnType(op);
                isProfiled = true;
            }
        }
        else if (DoProfileNewScObjArrayOp(op) &&
            (DoDynamicProfileOpcode(NativeArrayPhase, true) || DoDynamicProfileOpcode(InlinePhase, true)) &&
            callSiteId != Js::Constants::NoProfileId &&
            this->m_functionWrite->AllocProfiledArrayCallSiteId(&profileId2))
        {
            OpCodeUtil::ConvertNonCallOpToProfiled(op);
            isProfiled = true;
            isProfiled2 = true;
        }
        else if (DoProfileNewScObjectOp(op) &&
            (DoDynamicProfileOpcode(InlinePhase, true) || DoDynamicProfileOpcode(FixedNewObjPhase, true)) &&
            callSiteId != Js::Constants::NoProfileId)
        {
            if (unit.cacheId == Js::Constants::NoInlineCacheIndex)
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
                isProfiled = true;
            }
            else
            {
                isCallWithICIndex = true;
                OpCodeUtil::ConvertNonCallOpToProfiledWithICIndex(op);
                isProfiled = true;
            }
        }

        if (isCallWithICIndex)
        {
            if (hasCallFlags == true)
            {
                MULTISIZE_LAYOUT_WRITE(CallIFlagsWithICIndex, op, returnValueRegister, functionRegister, givenArgCount, unit.cacheId, unit.isRootObjectCache, callFlags);
            }
            else
            {
                MULTISIZE_LAYOUT_WRITE(CallIWithICIndex, op, returnValueRegister, functionRegister, givenArgCount, unit.cacheId, unit.isRootObjectCache);
            }
        }
        else
        {
            if (hasCallFlags == true)
            {
                MULTISIZE_LAYOUT_WRITE(CallIFlags, op, returnValueRegister, functionRegister, givenArgCount, callFlags);
            }
            else
            {
                MULTISIZE_LAYOUT_WRITE(CallI, op, returnValueRegister, functionRegister, givenArgCount);
            }
        }
        if (isProfiled)
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
            if (isProfiled2)
            {
                m_byteCodeData.Encode(&profileId2, sizeof(Js::ProfileId));
            }
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementI(OpCode op, RegSlot Value, RegSlot Instance, RegSlot Element)
    {
        OpLayoutT_ElementI<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, Value) && SizePolicy::Assign(layout.Instance, Instance)
            && SizePolicy::Assign(layout.Element, Element))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Element(OpCode op, RegSlot Value, RegSlot Instance, RegSlot Element, bool instanceAtReturnRegOK)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementI);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        Value = ConsumeReg(Value);
        Instance = ConsumeReg(Instance);
        Element = ConsumeReg(Element);

        if (this->m_functionWrite->GetIsStrictMode())
        {
            if (op == OpCode::DeleteElemI_A)
            {
                op = OpCode::DeleteElemIStrict_A;
            }
        }

        bool isProfiledLayout = false;
        Js::ProfileId profileId = Js::Constants::NoProfileId;
        Assert(instanceAtReturnRegOK || Instance != 0);
        if (DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) ||
            DoDynamicProfileOpcode(FloatTypeSpecPhase) ||
            DoDynamicProfileOpcode(TypedArrayTypeSpecPhase) ||
            DoDynamicProfileOpcode(ArrayCheckHoistPhase))
        {
            OpCode newop;
            switch (op)
            {
            case OpCode::LdElemI_A:
                newop = OpCode::ProfiledLdElemI_A;
                if (this->m_functionWrite->AllocProfiledLdElemId(&profileId))
                {
                    isProfiledLayout = true;
                    op = newop;
                }
                break;

            case Js::OpCode::StElemI_A:
                newop = OpCode::ProfiledStElemI_A;
                goto StoreCommon;

            case Js::OpCode::StElemI_A_Strict:
                newop = OpCode::ProfiledStElemI_A_Strict;
StoreCommon:
                if (this->m_functionWrite->AllocProfiledStElemId(&profileId))
                {
                    isProfiledLayout = true;
                    op = newop;
                }
                break;
            }
        }

        MULTISIZE_LAYOUT_WRITE(ElementI, op, Value, Instance, Element);
        if (isProfiledLayout)
        {
            Assert(profileId != Js::Constants::NoProfileId);
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementUnsigned1(OpCode op, RegSlot Value, RegSlot Instance, uint32 Element)
    {
        OpLayoutT_ElementUnsigned1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, Value) && SizePolicy::Assign(layout.Instance, Instance)
            && SizePolicy::Assign(layout.Element, Element))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ElementUnsigned1(OpCode op, RegSlot Value, RegSlot Instance, uint32 Element)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementUnsigned1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        Value = ConsumeReg(Value);
        Instance = ConsumeReg(Instance);

        MULTISIZE_LAYOUT_WRITE(ElementUnsigned1, op, Value, Instance, Element);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementScopedC(OpCode op, RegSlot value, PropertyIdIndexType propertyIdIndex)
    {
        OpLayoutT_ElementScopedC<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value)
            && SizePolicy::Assign(layout.PropertyIdIndex, propertyIdIndex))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ScopedProperty(OpCode op, RegSlot value, PropertyIdIndexType propertyIdIndex)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementScopedC);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);

#if DBG
        switch (op)
        {
        case OpCode::ScopedDeleteFld:
        case OpCode::ScopedEnsureNoRedeclFld:
        case OpCode::ScopedInitFunc:
            break;


        default:
            AssertMsg(false, "The specified OpCode is not intended for scoped field-access");
            break;
        }
#endif

        if (this->m_functionWrite->GetIsStrictMode())
        {
            if (op == OpCode::ScopedDeleteFld)
            {
                op = OpCode::ScopedDeleteFldStrict;
            }
        }

        MULTISIZE_LAYOUT_WRITE(ElementScopedC, op, value, propertyIdIndex);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementC(OpCode op, RegSlot value, RegSlot instance, PropertyIdIndexType propertyIdIndex)
    {
        OpLayoutT_ElementC<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::Assign(layout.Instance, instance)
            && SizePolicy::Assign(layout.PropertyIdIndex, propertyIdIndex))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Property(OpCode op, RegSlot value, RegSlot instance, PropertyIdIndexType propertyIdIndex)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementC);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        instance = ConsumeReg(instance);

#if DBG
        switch (op)
        {
        case OpCode::InitSetFld:
        case OpCode::InitGetFld:
        case OpCode::InitClassMemberGet:
        case OpCode::InitClassMemberSet:
        case OpCode::InitProto:
        case OpCode::DeleteFld:
        case OpCode::DeleteRootFld:
        case OpCode::LdElemUndefScoped:
        case OpCode::StFuncExpr:
            break;

        default:
            AssertMsg(false, "The specified OpCode is not intended for field-access");
            break;
        }
#endif

        if (this->m_functionWrite->GetIsStrictMode())
        {
            if (op == OpCode::DeleteFld)
            {
                op = OpCode::DeleteFldStrict;
            }
            else if (op == OpCode::DeleteRootFld)
            {
                // We will reach here when in the language service mode, since in that mode we have skipped that error.
                op = OpCode::DeleteRootFldStrict;
            }
        }

        MULTISIZE_LAYOUT_WRITE(ElementC, op, value, instance, propertyIdIndex);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementSlot(OpCode op, RegSlot value, RegSlot instance, int32 slotId)
    {
        OpLayoutT_ElementSlot<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::Assign(layout.Instance, instance)
            && SizePolicy::Assign(layout.SlotIndex, slotId))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Slot(OpCode op, RegSlot value, RegSlot instance, int32 slotId)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementSlot);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        instance = ConsumeReg(instance);

#if DBG
        switch (op)
        {
#if ENABLE_NATIVE_CODEGEN
        case OpCode::LdSlotArr:
        case OpCode::StSlot:
        case OpCode::StSlotChkUndecl:
#endif
        case OpCode::StObjSlot:
        case OpCode::StObjSlotChkUndecl:
            break;

        default:
            AssertMsg(false, "The specified OpCode is not intended for slot access");
            break;
        }
#endif

        MULTISIZE_LAYOUT_WRITE(ElementSlot, op, value, instance, slotId);
    }

    void ByteCodeWriter::Slot(OpCode op, RegSlot value, RegSlot instance, int32 slotId, ProfileId profileId)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementSlot);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        instance = ConsumeReg(instance);

        switch (op)
        {
        case OpCode::LdSlot:
        case OpCode::LdObjSlot:
            if ((DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) || DoDynamicProfileOpcode(FloatTypeSpecPhase)) &&
                profileId != Constants::NoProfileId)
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;

        default:
            AssertMsg(false, "The specified OpCode is not intended for slot access");
            break;
        }

        MULTISIZE_LAYOUT_WRITE(ElementSlot, op, value, instance, slotId);
        if (OpCodeAttr::IsProfiledOp(op))
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementSlotI1(OpCode op, RegSlot value, int32 slotId)
    {
        OpLayoutT_ElementSlotI1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value)
            && SizePolicy::Assign(layout.SlotIndex, slotId))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::SlotI1(OpCode op, RegSlot value, int32 slotId)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementSlotI1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);

#if DBG
        switch (op)
        {
            case OpCode::LdEnvObj:
            case OpCode::StLocalSlot:
            case OpCode::StLocalObjSlot:
            case OpCode::StLocalSlotChkUndecl:
            case OpCode::StLocalObjSlotChkUndecl:
            {
                break;
            }

            default:
            {
                AssertMsg(false, "The specified OpCode is not intended for slot access");
                break;
            }
        }
#endif

        MULTISIZE_LAYOUT_WRITE(ElementSlotI1, op, value, slotId);
    }

    void ByteCodeWriter::SlotI1(OpCode op, RegSlot value, int32 slotId, ProfileId profileId)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementSlotI1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);

        switch (op)
        {
            case OpCode::LdLocalSlot:
            case OpCode::LdLocalObjSlot:
                if ((DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) || DoDynamicProfileOpcode(FloatTypeSpecPhase)) &&
                    profileId != Constants::NoProfileId)
                {
                    OpCodeUtil::ConvertNonCallOpToProfiled(op);
                }
                break;

            default:
            {
                AssertMsg(false, "The specified OpCode is not intended for slot access");
                break;
            }
        }

        MULTISIZE_LAYOUT_WRITE(ElementSlotI1, op, value, slotId);
        if (OpCodeAttr::IsProfiledOp(op))
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementSlotI2(OpCode op, RegSlot value, int32 slotId1, int32 slotId2)
    {
        OpLayoutT_ElementSlotI2<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value)
            && SizePolicy::Assign(layout.SlotIndex1, slotId1)
            && SizePolicy::Assign(layout.SlotIndex2, slotId2))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::SlotI2(OpCode op, RegSlot value, int32 slotId1, int32 slotId2)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementSlotI2);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);

#if DBG
        switch (op)
        {
            case OpCode::StInnerSlot:
            case OpCode::StInnerSlotChkUndecl:
            case OpCode::StInnerObjSlot:
            case OpCode::StInnerObjSlotChkUndecl:
            case OpCode::StEnvSlot:
            case OpCode::StEnvSlotChkUndecl:
            case OpCode::StEnvObjSlot:
            case OpCode::StEnvObjSlotChkUndecl:
            {
                break;
            }

            default:
            {
                AssertMsg(false, "The specified OpCode is not intended for slot access");
                break;
            }
        }
#endif

        MULTISIZE_LAYOUT_WRITE(ElementSlotI2, op, value, slotId1, slotId2);
    }

    void ByteCodeWriter::SlotI2(OpCode op, RegSlot value, int32 slotId1, int32 slotId2, ProfileId profileId)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementSlotI2);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);

        switch (op)
        {
            case OpCode::LdInnerSlot:
            case OpCode::LdInnerObjSlot:
            case OpCode::LdEnvSlot:
            case OpCode::LdEnvObjSlot:
                if ((DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) || DoDynamicProfileOpcode(FloatTypeSpecPhase)) &&
                    profileId != Constants::NoProfileId)
                {
                    OpCodeUtil::ConvertNonCallOpToProfiled(op);
                }
                break;

            default:
            {
                AssertMsg(false, "The specified OpCode is not intended for slot access");
                break;
            }
        }

        MULTISIZE_LAYOUT_WRITE(ElementSlotI2, op, value, slotId1, slotId2);
        if (OpCodeAttr::IsProfiledOp(op))
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
        }
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementU(OpCode op, RegSlot instance, PropertyIdIndexType index)
    {
        OpLayoutT_ElementU<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Instance, instance) && SizePolicy::Assign(layout.PropertyIdIndex, index))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ElementU(OpCode op, RegSlot instance, PropertyIdIndexType index)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementU);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        instance = ConsumeReg(instance);

        MULTISIZE_LAYOUT_WRITE(ElementU, op, instance, index);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementScopedU(OpCode op, PropertyIdIndexType index)
    {
        OpLayoutT_ElementScopedU<SizePolicy> layout;
        if (SizePolicy::Assign(layout.PropertyIdIndex, index))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ElementScopedU(OpCode op, PropertyIdIndexType index)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementScopedU);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        MULTISIZE_LAYOUT_WRITE(ElementScopedU, op, index);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementRootU(OpCode op, PropertyIdIndexType index)
    {
        OpLayoutT_ElementRootU<SizePolicy> layout;
        if (SizePolicy::Assign(layout.PropertyIdIndex, index))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ElementRootU(OpCode op, PropertyIdIndexType index)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementRootU);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        MULTISIZE_LAYOUT_WRITE(ElementRootU, op, index);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementRootCP(OpCode op, RegSlot value, uint cacheId, bool isLoadMethod, bool isStore)
    {
        Assert(!isLoadMethod || !isStore);
        OpLayoutT_ElementRootCP<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::Assign(layout.inlineCacheIndex, cacheId))
        {
            size_t offset = m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);

            size_t inlineCacheOffset = offset + OpCodeUtil::EncodedSize(op, SizePolicy::LayoutEnum)
                + offsetof(OpLayoutT_ElementRootCP<SizePolicy>, inlineCacheIndex);

            // Root object inline cache index are given out from 0, but it will be at index after
            // all the plain inline cache. Store the offset of the inline cache index to patch it up later.
            SListBase<size_t> * rootObjectInlineCacheOffsets = isStore ?
                &rootObjectStoreInlineCacheOffsets : isLoadMethod ? &rootObjectLoadMethodInlineCacheOffsets : &rootObjectLoadInlineCacheOffsets;
            rootObjectInlineCacheOffsets->Prepend(this->m_labelOffsets->GetAllocator(), inlineCacheOffset);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::PatchableRootProperty(OpCode op, RegSlot value, uint cacheId, bool isLoadMethod, bool isStore, bool registerCacheIdForCall)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementRootCP);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));
        Assert(!isLoadMethod || !isStore);

        value = ConsumeReg(value);

        switch (op)
        {
        case OpCode::LdRootFld:
        case OpCode::LdRootFldForTypeOf:
            if (DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) ||
                DoDynamicProfileOpcode(FloatTypeSpecPhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;
        case OpCode::LdRootMethodFld:
            if (registerCacheIdForCall)
            {
                CacheIdUnit unit(cacheId, true);
                Assert(!callRegToLdFldCacheIndexMap->TryGetValue(value, &unit));
                callRegToLdFldCacheIndexMap->Add(value, unit);
            }
        case OpCode::StRootFld:
        case OpCode::StRootFldStrict:
        case OpCode::InitRootFld:
            if (DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;
        case OpCode::InitRootLetFld:
        case OpCode::InitRootConstFld:
            break;
        default:
            AssertMsg(false, "The specified OpCode is not intended for patchable root field-access");
            break;
        }

        MULTISIZE_LAYOUT_WRITE(ElementRootCP, op, value, cacheId, isLoadMethod, isStore);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementP(OpCode op, RegSlot value, CacheId cacheId)
    {
        OpLayoutT_ElementP<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value)
            && SizePolicy::Assign(layout.inlineCacheIndex, cacheId))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ElementP(OpCode op, RegSlot value, uint cacheId, bool isCtor, bool registerCacheIdForCall)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementP);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        switch (op)
        {
        case OpCode::ScopedLdFld:
        case OpCode::ScopedLdFldForTypeOf:
        case OpCode::ScopedStFld:
        case OpCode::ConsoleScopedStFld:
        case OpCode::ScopedStFldStrict:
            break;

        case OpCode::LdLocalFld:
            if (isCtor) // The symbol loaded by this LdFld will be used as a constructor
            {
                if (registerCacheIdForCall)
                {
                    CacheIdUnit unit(cacheId);
                    Assert(!callRegToLdFldCacheIndexMap->TryGetValue(value, &unit));
                    callRegToLdFldCacheIndexMap->Add(value, unit);
                }
            }
            if (DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) ||
                DoDynamicProfileOpcode(FloatTypeSpecPhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;          

        case OpCode::LdLocalMethodFld:
            if (registerCacheIdForCall)
            {
                CacheIdUnit unit(cacheId);
                Assert(!callRegToLdFldCacheIndexMap->TryGetValue(value, &unit));
                callRegToLdFldCacheIndexMap->Add(value, unit);
            }
            // fall-through
        case OpCode::StLocalFld:
        case OpCode::InitLocalFld:
            if (DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;

        case OpCode::InitLocalLetFld:
        case OpCode::InitUndeclLocalLetFld:
        case OpCode::InitUndeclLocalConstFld:
            break;

        default:
            AssertMsg(false, "The specified OpCode not intended for base-less patchable field access");
            break;
        }

        MULTISIZE_LAYOUT_WRITE(ElementP, op, value, cacheId);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementPIndexed(OpCode op, RegSlot value, uint32 scopeIndex, CacheId cacheId)
    {
        OpLayoutT_ElementPIndexed<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value)
            && SizePolicy::Assign(layout.inlineCacheIndex, cacheId)
            && SizePolicy::Assign(layout.scopeIndex, scopeIndex))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ElementPIndexed(OpCode op, RegSlot value, uint32 scopeIndex, uint cacheId)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementPIndexed);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        switch (op)
        {
        case OpCode::InitInnerFld:
        case OpCode::InitInnerLetFld:
        case OpCode::InitUndeclLetFld:
        case OpCode::InitUndeclConstFld:
            break;

            break;

        default:
            AssertMsg(false, "The specified OpCode not intended for base-less patchable inner field access");
            break;
        }

        MULTISIZE_LAYOUT_WRITE(ElementPIndexed, op, value, scopeIndex, cacheId);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementCP(OpCode op, RegSlot value, RegSlot instance, CacheId cacheId)
    {
        OpLayoutT_ElementCP<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::Assign(layout.Instance, instance)
            && SizePolicy::Assign(layout.inlineCacheIndex, cacheId))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::PatchableProperty(OpCode op, RegSlot value, RegSlot instance, uint cacheId, bool isCtor, bool registerCacheIdForCall)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementCP);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        instance = ConsumeReg(instance);

        switch (op)
        {
        case OpCode::LdFldForTypeOf:
        case OpCode::LdFld:
            if (isCtor) // The symbol loaded by this LdFld will be used as a constructor
            {
                if (registerCacheIdForCall)
                {
                    CacheIdUnit unit(cacheId);
                    Assert(!callRegToLdFldCacheIndexMap->TryGetValue(value, &unit));
                    callRegToLdFldCacheIndexMap->Add(value, unit);
                }
            }
        case OpCode::LdFldForCallApplyTarget:
            if (DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) ||
                DoDynamicProfileOpcode(FloatTypeSpecPhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;
        case OpCode::LdMethodFld:
            if (registerCacheIdForCall)
            {
                CacheIdUnit unit(cacheId);
                Assert(!callRegToLdFldCacheIndexMap->TryGetValue(value, &unit));
                callRegToLdFldCacheIndexMap->Add(value, unit);
            }
            // fall-through
        case OpCode::StFld:
        case OpCode::StFldStrict:
        case OpCode::InitFld:
            if (DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;
        case OpCode::InitLetFld:
        case OpCode::InitConstFld:
        case OpCode::InitClassMember:
        case OpCode::ScopedLdMethodFld:
            break;
        default:
            AssertMsg(false, "The specified OpCode is not intended for patchable field-access");
            break;
        }

        MULTISIZE_LAYOUT_WRITE(ElementCP, op, value, instance, cacheId);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementC2(OpCode op, RegSlot value, RegSlot instance, PropertyIdIndexType propertyIdIndex, RegSlot value2)
    {
        OpLayoutT_ElementC2<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value) && SizePolicy::Assign(layout.Instance, instance)
            && SizePolicy::Assign(layout.PropertyIdIndex, propertyIdIndex) && SizePolicy::Assign(layout.Value2, value2))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::PatchablePropertyWithThisPtr(OpCode op, RegSlot value, RegSlot instance, RegSlot thisInstance, uint cacheId, bool isCtor, bool registerCacheIdForCall)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementC2);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        instance = ConsumeReg(instance);
        thisInstance = ConsumeReg(thisInstance);

        switch (op)
        {
        case OpCode::LdSuperFld:
            if (isCtor) // The symbol loaded by this LdSuperFld will be used as a constructor
            {
                if (registerCacheIdForCall)
                {
                    CacheIdUnit unit(cacheId);
                    Assert(!callRegToLdFldCacheIndexMap->TryGetValue(value, &unit));
                    callRegToLdFldCacheIndexMap->Add(value, unit);
                }
            }
            if (DoDynamicProfileOpcode(AggressiveIntTypeSpecPhase) ||
                DoDynamicProfileOpcode(FloatTypeSpecPhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;
        case OpCode::StSuperFld:
            if (DoDynamicProfileOpcode(ProfileBasedFldFastPathPhase) ||
                DoDynamicProfileOpcode(InlinePhase) ||
                DoDynamicProfileOpcode(ObjTypeSpecPhase))
            {
                OpCodeUtil::ConvertNonCallOpToProfiled(op);
            }
            break;
        default:
            AssertMsg(false, "The specified OpCode is not intended for patchable super field-access");
            break;
        }

        MULTISIZE_LAYOUT_WRITE(ElementC2, op, value, instance, cacheId, thisInstance);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteElementScopedC2(OpCode op, RegSlot value, PropertyIdIndexType propertyIdIndex, RegSlot value2)
    {
        OpLayoutT_ElementScopedC2<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Value, value)
            && SizePolicy::Assign(layout.PropertyIdIndex, propertyIdIndex) && SizePolicy::Assign(layout.Value2, value2))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::ScopedProperty2(OpCode op, RegSlot value, PropertyIdIndexType propertyIdIndex, RegSlot value2)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::ElementScopedC2);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        value = ConsumeReg(value);
        value2 = ConsumeReg(value2);

        switch (op)
        {
        case OpCode::ScopedLdInst:
            break;

        default:
            AssertMsg(false, "The specified OpCode is not intended for field-access with a second instance");
            break;
        }

        MULTISIZE_LAYOUT_WRITE(ElementScopedC2, op, value, propertyIdIndex, value2);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteClass(OpCode op, RegSlot constructor, RegSlot extends)
    {
        OpLayoutT_Class<SizePolicy> layout;
        if (SizePolicy::Assign(layout.Constructor, constructor) && SizePolicy::Assign(layout.Extends, extends))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::InitClass(RegSlot constructor, RegSlot extends)
    {
        Assert(OpCodeAttr::HasMultiSizeLayout(Js::OpCode::InitClass));

        CheckOpen();

        constructor = ConsumeReg(constructor);

        if (extends != Js::Constants::NoRegister)
        {
            extends = ConsumeReg(extends);
        }

        MULTISIZE_LAYOUT_WRITE(Class, Js::OpCode::InitClass, constructor, extends);
    }

    void ByteCodeWriter::NewFunction(RegSlot destinationRegister, uint index, bool isGenerator)
    {
        CheckOpen();

        destinationRegister = ConsumeReg(destinationRegister);
        OpCode opcode = isGenerator ?
            OpCode::NewScGenFunc :
            this->m_functionWrite->DoStackNestedFunc() ?
                OpCode::NewStackScFunc : OpCode::NewScFunc;
        Assert(OpCodeAttr::HasMultiSizeLayout(opcode));

        MULTISIZE_LAYOUT_WRITE(ElementSlotI1, opcode, destinationRegister, index);
    }

    void ByteCodeWriter::NewInnerFunction(RegSlot destinationRegister, uint index, RegSlot environmentRegister, bool isGenerator)
    {
        CheckOpen();

        destinationRegister = ConsumeReg(destinationRegister);
        environmentRegister = ConsumeReg(environmentRegister);
        OpCode opcode = isGenerator ?
                OpCode::NewInnerScGenFunc :
                this->m_functionWrite->DoStackNestedFunc() ?
                    OpCode::NewInnerStackScFunc : OpCode::NewInnerScFunc;
        Assert(OpCodeAttr::HasMultiSizeLayout(opcode));

        MULTISIZE_LAYOUT_WRITE(ElementSlot, opcode, destinationRegister, environmentRegister, index);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg1Unsigned1(OpCode op, RegSlot R0, uint C1)
    {
        OpLayoutT_Reg1Unsigned1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.C1, C1))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg1Unsigned1(OpCode op, RegSlot R0, uint C1)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg1Unsigned1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);

        ProfileId profileId = Constants::NoProfileId;
        bool isProfiled = DoProfileNewScArrayOp(op) &&
            DoDynamicProfileOpcode(NativeArrayPhase, true) &&
            this->m_functionWrite->AllocProfiledArrayCallSiteId(&profileId);

        if (isProfiled)
        {
            OpCodeUtil::ConvertNonCallOpToProfiled(op);
        }
        MULTISIZE_LAYOUT_WRITE(Reg1Unsigned1, op, R0, C1);
        if (isProfiled)
        {
            m_byteCodeData.Encode(&profileId, sizeof(Js::ProfileId));
        }
    }

    void ByteCodeWriter::W1(OpCode op, ushort C1)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::W1);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));

        OpLayoutW1 data;
        data.C1 = C1;
        m_byteCodeData.Encode(op, &data, sizeof(data), this);
    }

    void ByteCodeWriter::Reg1Int2(OpCode op, RegSlot R0, int C1, int C2)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg1Int2);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));

        R0 = ConsumeReg(R0);

        OpLayoutReg1Int2 data;
        data.R0 = R0;
        data.C1 = C1;
        data.C2 = C2;
        m_byteCodeData.Encode(op, &data, sizeof(data), this);
    }

    template <typename SizePolicy>
    bool ByteCodeWriter::TryWriteReg2Int1(OpCode op, RegSlot R0, RegSlot R1, int C1)
    {
        OpLayoutT_Reg2Int1<SizePolicy> layout;
        if (SizePolicy::Assign(layout.R0, R0) && SizePolicy::Assign(layout.R1, R1) && SizePolicy::Assign(layout.C1, C1))
        {
            m_byteCodeData.EncodeT<SizePolicy::LayoutEnum>(op, &layout, sizeof(layout), this);
            return true;
        }
        return false;
    }

    void ByteCodeWriter::Reg2Int1(OpCode op, RegSlot R0, RegSlot R1, int C1)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg2Int1);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        if (DoDynamicProfileOpcode(CheckThisPhase) ||
            DoDynamicProfileOpcode(TypedArrayTypeSpecPhase) ||
            DoDynamicProfileOpcode(ArrayCheckHoistPhase))
        {
            if (op == OpCode::LdThis)
            {
                op = OpCode::ProfiledLdThis;
            }
        }

        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);

        MULTISIZE_LAYOUT_WRITE(Reg2Int1, op, R0, R1, C1);
    }

    void ByteCodeWriter::Num3(OpCode op, RegSlot C0, RegSlot C1, RegSlot C2)
    {
        CheckOpen();
        CheckOp(op, OpLayoutType::Reg3);
        Assert(OpCodeAttr::HasMultiSizeLayout(op));

        MULTISIZE_LAYOUT_WRITE(Reg3, op, C0, C1, C2);
    }

    int ByteCodeWriter::AuxNoReg(OpCode op, const void* buffer, int byteCount, int C1)
    {
        CheckOpen();

        //
        // Write the buffer's contents
        //

        int currentOffset = InsertAuxiliaryData(buffer, byteCount);

        //
        // Write OpCode to create new auxiliary data
        //

        OpLayoutAuxNoReg data;
        data.Offset = currentOffset;
        data.C1 = C1;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);

        return currentOffset;
    }

    void ByteCodeWriter::AuxNoReg(OpCode op, uint byteOffset, int C1)
    {
        CheckOpen();

        //
        // Write the buffer's contents
        //

        Assert(byteOffset < m_auxiliaryData.GetCurrentOffset());

        OpLayoutAuxNoReg data;
        data.Offset = byteOffset;
        data.C1 = C1;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
    }

    int ByteCodeWriter::Auxiliary(OpCode op, RegSlot destinationRegister, const void* buffer, int byteCount, int C1)
    {
        CheckOpen();
        destinationRegister = ConsumeReg(destinationRegister);

        //
        // Write the buffer's contents
        //

        int currentOffset = InsertAuxiliaryData(buffer, byteCount);

        //
        // Write OpCode to create new auxiliary data
        //

        ProfileId profileId = Constants::NoProfileId;

        if (DoProfileNewScArrayOp(op) &&
            DoDynamicProfileOpcode(NativeArrayPhase, true) &&
            this->m_functionWrite->AllocProfiledArrayCallSiteId(&profileId))
        {
            OpCodeUtil::ConvertNonCallOpToProfiled(op);

            OpLayoutDynamicProfile<OpLayoutAuxiliary> data;
            data.R0 = destinationRegister;
            data.Offset = currentOffset;
            data.C1 = C1;
            data.profileId = profileId;

            m_byteCodeData.Encode(op, &data, sizeof(data), this);
        }
        else
        {
            OpLayoutAuxiliary data;
            data.R0 = destinationRegister;
            data.Offset = currentOffset;
            data.C1 = C1;

            m_byteCodeData.Encode(op, &data, sizeof(data), this);
        }

        return currentOffset;
    }

    void ByteCodeWriter::Auxiliary(OpCode op, RegSlot destinationRegister, uint byteOffset, int C1)
    {
        CheckOpen();
        destinationRegister = ConsumeReg(destinationRegister);

        //
        // Write the buffer's contents
        //

        Assert(byteOffset < m_auxiliaryData.GetCurrentOffset());

        OpLayoutAuxiliary data;
        data.R0 = destinationRegister;
        data.Offset = byteOffset;
        data.C1 = C1;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
    }

    int ByteCodeWriter::Reg2Aux(OpCode op, RegSlot R0, RegSlot R1, const void* buffer, int byteCount, int C1)
    {
        CheckOpen();
        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);

        //
        // Write the buffer's contents
        //

        int currentOffset   = InsertAuxiliaryData(buffer, byteCount);

        //
        // Write OpCode to create new auxiliary data
        //

        OpLayoutReg2Aux data;
        data.R0 = R0;
        data.R1 = R1;
        data.Offset = currentOffset;
        data.C1 = C1;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);

        return currentOffset;
    }

    void ByteCodeWriter::Reg2Aux(OpCode op, RegSlot R0, RegSlot R1, uint byteOffset, int C1)
    {
        CheckOpen();
        R0 = ConsumeReg(R0);
        R1 = ConsumeReg(R1);

        //
        // Write the buffer's contents
        //

        Assert(byteOffset < m_auxiliaryData.GetCurrentOffset());

        OpLayoutReg2Aux data;
        data.R0 = R0;
        data.R1 = R1;
        data.Offset = byteOffset;
        data.C1 = C1;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
    }

    void ByteCodeWriter::AuxiliaryContext(OpCode op, RegSlot destinationRegister, const void* buffer, int byteCount, Js::RegSlot C1)
    {
        CheckOpen();
        destinationRegister = ConsumeReg(destinationRegister);
        C1 = ConsumeReg(C1);

        //
        // Write the buffer's contents
        //

        int currentOffset = m_auxContextData.GetCurrentOffset();
        if (byteCount > 0)
        {
            m_auxContextData.Encode(buffer, byteCount);
        }

        //
        // Write OpCode to create new auxiliary data
        //

        OpLayoutAuxiliary data;
        data.R0 = destinationRegister;
        data.Offset = currentOffset;
        data.C1 = C1;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
    }

    uint ByteCodeWriter::InsertAuxiliaryData(const void* buffer, uint byteCount)
    {
        uint offset = m_auxiliaryData.GetCurrentOffset();
        if (byteCount > 0)
        {
            m_auxiliaryData.Encode(buffer, byteCount);
        }

        return offset;
    }

    ByteCodeLabel ByteCodeWriter::DefineLabel()
    {
#if defined(_M_X64_OR_ARM64)
        if (m_labelOffsets->Count() == INT_MAX)
        {
            // Reach our limit
            Js::Throw::OutOfMemory();
        }
#else
        // 32-bit machine don't have enough address space to get to INT_MAX
        Assert(m_labelOffsets->Count() < INT_MAX);
#endif

        //
        // Allocate a new label:
        // - All label locations start as "undefined: -1".  Once the label's location is marked in
        //   the byte-code, this will be updated.
        //

        return (ByteCodeLabel)m_labelOffsets->Add(UINT_MAX);
    }

    void ByteCodeWriter::MarkLabel(ByteCodeLabel labelID)
    {
        CheckOpen();
        CheckLabel(labelID);

#ifdef BYTECODE_BRANCH_ISLAND
        if (useBranchIsland)
        {
            // If we are going to emit a branch island, it should be before the label.
            EnsureLongBranch(Js::OpCode::Label);
        }
#endif
        //
        // Define the label as the current offset within the byte-code.
        //

        AssertMsg(m_labelOffsets->Item(labelID) == UINT_MAX, "A label may only be defined at one location");
        m_labelOffsets->SetExistingItem(labelID, m_byteCodeData.GetCurrentOffset());
    }

    void ByteCodeWriter::AddJumpOffset(Js::OpCode op, ByteCodeLabel labelId, uint fieldByteOffsetFromEnd) // Offset of "Offset" field in OpLayout, in bytes
    {
        AssertMsg(fieldByteOffsetFromEnd < 100, "Ensure valid field offset");
        CheckOpen();
        CheckLabel(labelId);

        uint jumpByteOffset = m_byteCodeData.GetCurrentOffset() - fieldByteOffsetFromEnd;
#ifdef BYTECODE_BRANCH_ISLAND
        if (useBranchIsland)
        {
            // Any Jump might need a long jump, account for that emit the branch island earlier.
            // Even if it is a back edge and we are going to emit a long jump, we will still
            // emit a branch around any way.
            this->nextBranchIslandOffset -= LongBranchSize;

            uint labelOffset = m_labelOffsets->Item(labelId);
            if (labelOffset != UINT_MAX)
            {
                // Back branch, see if it needs to be long
                Assert(labelOffset < m_byteCodeData.GetCurrentOffset());
                LongJumpOffset jumpOffset = labelOffset - m_byteCodeData.GetCurrentOffset();
                if (jumpOffset < -GetBranchLimit())
                {
                    // Create the long jump label and add the original jump offset to the list first
                    ByteCodeLabel longJumpLabel = this->DefineLabel();
                    JumpInfo jumpInfo = { longJumpLabel, jumpByteOffset };
                    m_jumpOffsets->Add(jumpInfo);

                    // Emit the jump around (if necessary)
                    ByteCodeLabel jumpAroundLabel = (ByteCodeLabel)-1;
                    if (OpCodeAttr::HasFallThrough(op))
                    {
                        // emit jump around.
                        jumpAroundLabel = this->DefineLabel();
                        this->Br(jumpAroundLabel);
                    }

                    // emit the long jump
                    this->MarkLabel(longJumpLabel);
                    this->BrLong(Js::OpCode::BrLong, labelId);

                    if (jumpAroundLabel != (ByteCodeLabel)-1)
                    {
                        this->MarkLabel(jumpAroundLabel);
                    }
                    return;
                }
            }
        }
#endif
        //
        // Branch targets are created in two passes:
        // - In the instruction stream, write "labelID" into "OpLayoutBrC.Offset".  Record this
        //   location in "m_jumpOffsets" to be patched later.
        // - When the byte-code is closed, update all "OpLayoutBrC.Offset"'s with their actual
        //   destinations.
        //

        JumpInfo jumpInfo = { labelId, jumpByteOffset };
        m_jumpOffsets->Add(jumpInfo);
    }

#ifdef BYTECODE_BRANCH_ISLAND
    int32 ByteCodeWriter::GetBranchLimit()
    {
#ifdef BYTECODE_TESTING
        if (Js::Configuration::Global.flags.IsEnabled(Js::ByteCodeBranchLimitFlag))
        {
            // minimum 64
            return min(max(Js::Configuration::Global.flags.ByteCodeBranchLimit, 64), SHRT_MAX + 1);
        }
#endif

        return SHRT_MAX + 1;
    }

    void ByteCodeWriter::AddLongJumpOffset(ByteCodeLabel labelId, uint fieldByteOffsetFromEnd) // Offset of "Offset" field in OpLayout, in bytes
    {
        Assert(useBranchIsland);
        AssertMsg(fieldByteOffsetFromEnd < 100, "Ensure valid field offset");

        //
        // Branch targets are created in two passes:
        // - In the instruction stream, write "labelID" into "OpLayoutBrC.Offset".  Record this
        //   location in "m_jumpOffsets" to be patched later.
        // - When the byte-code is closed, update all "OpLayoutBrC.Offset"'s with their actual
        //   destinations.
        //

        uint jumpByteOffset = m_byteCodeData.GetCurrentOffset() - fieldByteOffsetFromEnd;
        JumpInfo jumpInfo = { labelId, jumpByteOffset };
        m_longJumpOffsets->Add(jumpInfo);
    }

    void ByteCodeWriter::BrLong(OpCode op, ByteCodeLabel labelID)
    {
        Assert(useBranchIsland);
        CheckOpen();
        CheckOp(op, OpLayoutType::BrLong);
        CheckLabel(labelID);
        Assert(!OpCodeAttr::HasMultiSizeLayout(op));

        size_t const offsetOfRelativeJumpOffsetFromEnd = sizeof(OpLayoutBrLong) - offsetof(OpLayoutBrLong, RelativeJumpOffset);
        OpLayoutBrLong data;
        data.RelativeJumpOffset = offsetOfRelativeJumpOffsetFromEnd;

        m_byteCodeData.Encode(op, &data, sizeof(data), this);
        AddLongJumpOffset(labelID, offsetOfRelativeJumpOffsetFromEnd);
    }


    void ByteCodeWriter::UpdateNextBranchIslandOffset(uint firstUnknownJumpInfo, uint firstUnknownJumpOffset)
    {
        this->firstUnknownJumpInfo = firstUnknownJumpInfo;

        // We will need to emit the next branch from the first branch + branch limit.
        // But leave room for the jump around and one extra byte code instruction.
        // Also account for all the long branches we may have to emit as well.
        this->nextBranchIslandOffset = firstUnknownJumpOffset + GetBranchLimit()
            - JumpAroundSize - MaxLayoutSize - MaxOpCodeSize - LongBranchSize * (m_jumpOffsets->Count() - firstUnknownJumpInfo);
    }

    void ByteCodeWriter::EnsureLongBranch(Js::OpCode op)
    {
        Assert(useBranchIsland);
        int currentOffset = this->m_byteCodeData.GetCurrentOffset();

        // See if we need to emit branch island yet, and avoid recursion.
        if (currentOffset < this->nextBranchIslandOffset || this->inEnsureLongBranch)
        {
            lastOpcode = op;
            return;
        }

        // Leave actually may continue right after, it is only no fall through in the JIT.
        bool needBranchAround = OpCodeAttr::HasFallThrough(lastOpcode) || lastOpcode == Js::OpCode::Leave;
        lastOpcode = op;

        // If we are about to emit a no fall through op and the last was has fall through
        // then just emit the no fall through op, and then we can skip the branch around.
        // Except at label or StatementBoundary, we always want to emit before them.
        if ((needBranchAround && !OpCodeAttr::HasFallThrough(op))
            && op != Js::OpCode::StatementBoundary && op != Js::OpCode::Label)
        {
            return;
        }

        ByteCodeLabel branchAroundLabel = (Js::ByteCodeLabel)-1;
        bool foundUnknown = m_jumpOffsets->MapUntilFrom(firstUnknownJumpInfo,
            [=, &branchAroundLabel, &currentOffset](int index, JumpInfo& jumpInfo)
        {
            //
            // Read "labelID" stored at the offset within the byte-code.
            //
            uint jumpByteOffset = jumpInfo.patchOffset;
            AssertMsg(jumpByteOffset <= this->m_byteCodeData.GetCurrentOffset() - sizeof(JumpOffset),
                "Must have valid jump site within byte-code to back-patch");

            ByteCodeLabel labelID = jumpInfo.labelId;
            CheckLabel(labelID);

            // See if the label has bee marked yet.
            uint const labelByteOffset = m_labelOffsets->Item(labelID);
            if (labelByteOffset != UINT_MAX)
            {
                // If a label is already defined, then it should be short
                // (otherwise we should have emitted a branch island for it already).
                Assert((int)labelByteOffset - (int)jumpByteOffset < GetBranchLimit()
                    && (int)labelByteOffset - (int)jumpByteOffset >= -GetBranchLimit());
                return false;
            }

            this->UpdateNextBranchIslandOffset(index, jumpByteOffset);
            // Flush all the jump that are half of the way to the limit as well so we don't have
            // as many jump around of branch island.
            int flushNextBranchIslandOffset = this->nextBranchIslandOffset - GetBranchLimit() / 2;
            if (currentOffset < flushNextBranchIslandOffset)
            {
                // No need to for long branch yet. Terminate the loop.
                return true;
            }

            if (labelID == branchAroundLabel)
            {
                // Let's not flush the branchAroundLabel.
                // Should happen very rarely and mostly when the branch limit is very small.

                // This should be the last short jump we have just emitted (below).
                Assert(index == m_jumpOffsets->Count() - 1);
                Assert(currentOffset < this->nextBranchIslandOffset);
                return true;
            }

            // Emit long branch

            // Prevent recursion when we emit byte code here
            this->inEnsureLongBranch = true;

            // Create the branch label and update the jumpInfo.
            // Need to update the jumpInfo before we add the branch island as that might resize the m_jumpOffsets list.
            ByteCodeLabel longBranchLabel = this->DefineLabel();
            jumpInfo.labelId = longBranchLabel;

            // Emit the branch around if it hasn't been emitted already
            if (branchAroundLabel == (Js::ByteCodeLabel)-1 && needBranchAround)
            {
                branchAroundLabel = this->DefineLabel();
                this->Br(Js::OpCode::Br, branchAroundLabel);

                Assert(this->m_byteCodeData.GetCurrentOffset() - currentOffset == JumpAroundSize);
                currentOffset += JumpAroundSize;

                // Continue to count he jumpAroundSize, because we may have to emit
                // yet another branch island right after if the jumpAroundSize is included.
            }

            // Emit the long branch
            this->MarkLabel(longBranchLabel);
            this->BrLong(Js::OpCode::BrLong, labelID);

            this->inEnsureLongBranch = false;

            Assert(this->m_byteCodeData.GetCurrentOffset() - currentOffset == LongBranchSize);
            currentOffset += LongBranchSize;
            return false;
        });

        if (!foundUnknown)
        {
            // Nothing is found, just set the next branch island from the current offset
            this->UpdateNextBranchIslandOffset(this->m_jumpOffsets->Count(), currentOffset);
        }

        if (branchAroundLabel != (Js::ByteCodeLabel)-1)
        {
            // Make the branch around label if we needed one
            this->MarkLabel(branchAroundLabel);
        }
    }
#endif

    void ByteCodeWriter::StartStatement(ParseNode* node, uint32 tmpRegCount)
    {
        if (m_pMatchingNode)
        {
            if (m_pMatchingNode == node)
            {
                m_matchingNodeRefCount++;
            }
            return;
        }
#ifdef BYTECODE_BRANCH_ISLAND
        if (useBranchIsland)
        {
            // If we are going to emit a branch island, it should be before the statement start
            this->EnsureLongBranch(Js::OpCode::StatementBoundary);
        }
#endif
        m_pMatchingNode = node;
        m_beginCodeSpan = m_byteCodeData.GetCurrentOffset();

        if (m_isInDebugMode && m_tmpRegCount != tmpRegCount)
        {
            Unsigned1(OpCode::EmitTmpRegCount, tmpRegCount);
            m_tmpRegCount = tmpRegCount;
        }
    }

    void ByteCodeWriter::EndStatement(ParseNode* node)
    {
        AssertMsg(m_pMatchingNode, "EndStatement unmatched to StartStartment");
        if (m_pMatchingNode != node)
        {
            return;
        }
        else if (m_matchingNodeRefCount > 0)
        {
            m_matchingNodeRefCount--;
            return;
        }

        if (m_byteCodeData.GetCurrentOffset() != m_beginCodeSpan)
        {
            if (m_isInDebugMode)
            {
                FunctionBody::StatementMap* pCurrentStatement = FunctionBody::StatementMap::New(this->m_functionWrite->GetScriptContext()->GetRecycler());

                if (pCurrentStatement)
                {
                    pCurrentStatement->sourceSpan.begin = node->ichMin;
                    pCurrentStatement->sourceSpan.end = node->ichLim;

                    pCurrentStatement->byteCodeSpan.begin = m_beginCodeSpan;
                    pCurrentStatement->byteCodeSpan.end = m_byteCodeData.GetCurrentOffset() - 1;

                    m_functionWrite->RecordStatementMap(pCurrentStatement);
                }
            }
            else
            {
                StatementData currentStatement;

                currentStatement.sourceBegin = node->ichMin;
                currentStatement.bytecodeBegin = m_beginCodeSpan;

                m_functionWrite->RecordStatementMap(spanIter, &currentStatement);
            }
        }
        m_pMatchingNode = nullptr;
    }

    void ByteCodeWriter::StartSubexpression(ParseNode* node)
    {
        if (!m_isInDebugMode || !m_pMatchingNode) // Subexpression not in debug mode or not enclosed in regular statement
        {
            return;
        }
#ifdef BYTECODE_BRANCH_ISLAND
        // If we are going to emit a branch island, it should be before the statement start
        this->EnsureLongBranch(Js::OpCode::StatementBoundary);
#endif
        m_subexpressionNodesStack->Push(SubexpressionNode(node, m_byteCodeData.GetCurrentOffset()));
    }

    void ByteCodeWriter::EndSubexpression(ParseNode* node)
    {
        if (!m_isInDebugMode || m_subexpressionNodesStack->Empty() || m_subexpressionNodesStack->Peek().node != node)
        {
            return;
        }

        if (m_byteCodeData.GetCurrentOffset() != m_beginCodeSpan)
        {
            FunctionBody::StatementMap* pCurrentStatement = FunctionBody::StatementMap::New(this->m_functionWrite->GetScriptContext()->GetRecycler());

            if (pCurrentStatement)
            {
                pCurrentStatement->sourceSpan.begin = node->ichMin;
                pCurrentStatement->sourceSpan.end = node->ichLim;

                SubexpressionNode subexpressionNode = m_subexpressionNodesStack->Pop();
                pCurrentStatement->byteCodeSpan.begin = subexpressionNode.beginCodeSpan;
                pCurrentStatement->byteCodeSpan.end = m_byteCodeData.GetCurrentOffset() - 1;
                pCurrentStatement->isSubexpression = true;

                m_functionWrite->RecordStatementMap(pCurrentStatement);
            }
        }
    }

    // Pushes a new debugger scope onto the stack. This information is used when determining
    // what the current scope is for tracking of let/const initialization offsets (for detecting
    // dead zones).
    void ByteCodeWriter::PushDebuggerScope(Js::DebuggerScope* debuggerScope)
    {
        Assert(debuggerScope);

        debuggerScope->SetParentScope(m_currentDebuggerScope);
        m_currentDebuggerScope = debuggerScope;
        OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, L"PushDebuggerScope() - Pushed scope 0x%p of type %d.\n", m_currentDebuggerScope, m_currentDebuggerScope->scopeType);
    }

    // Pops the current debugger scope from the stack.
    void ByteCodeWriter::PopDebuggerScope()
    {
        Assert(m_currentDebuggerScope);

        OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, L"PopDebuggerScope() - Popped scope 0x%p of type %d.\n", m_currentDebuggerScope, m_currentDebuggerScope->scopeType);
        if (m_currentDebuggerScope != nullptr)
        {
            m_currentDebuggerScope = m_currentDebuggerScope->GetParentScope();
        }
    }

    DebuggerScope* ByteCodeWriter::RecordStartScopeObject(DiagExtraScopesType scopeType, RegSlot scopeLocation, int* index)
    {
        if (scopeLocation != Js::Constants::NoRegister)
        {
            scopeLocation = ConsumeReg(scopeLocation);
        }
        DebuggerScope* debuggerScope = m_functionWrite->RecordStartScopeObject(scopeType, m_byteCodeData.GetCurrentOffset(), scopeLocation, index);
        PushDebuggerScope(debuggerScope);
        return debuggerScope;
    }

    void ByteCodeWriter::AddPropertyToDebuggerScope(
        DebuggerScope* debuggerScope,
        RegSlot location,
        Js::PropertyId propertyId,
        bool shouldConsumeRegister /*= true*/,
        DebuggerScopePropertyFlags flags /*= DebuggerScopePropertyFlags_None*/,
        bool isFunctionDeclaration /*= false*/)
    {
        Assert(debuggerScope);

        // Activation object doesn't use register and slot array location represents the
        // index in the array. Only need to consume for register slots.
        if (shouldConsumeRegister)
        {
            Assert(location != Js::Constants::NoRegister);
            location = ConsumeReg(location);
        }

        debuggerScope->AddProperty(location, propertyId, flags);

        // Only need to update properties in debug mode (even for slot array, which is tracked in non-debug mode,
        // since the offset is only used for debugging).
        if (this->m_isInDebugMode && isFunctionDeclaration)
        {
            AssertMsg(this->m_currentDebuggerScope, "Function declarations can only be added in a block scope.");
            AssertMsg(debuggerScope == this->m_currentDebuggerScope
                || debuggerScope == this->m_currentDebuggerScope->siblingScope,
                "Function declarations should always be added to the current scope.");

            // If this is a function declaration, it doesn't have a dead zone region so
            // we just update its byte code initialization offset to the start of the block.
            this->UpdateDebuggerPropertyInitializationOffset(
                debuggerScope,
                location,
                propertyId,
                false /*shouldConsumeRegister*/, // Register would have already been consumed above, if needed.
                debuggerScope->GetStart(),
                isFunctionDeclaration);
        }
    }

    void ByteCodeWriter::RecordEndScopeObject()
    {
        Assert(this->m_currentDebuggerScope);

        m_functionWrite->RecordEndScopeObject(this->m_currentDebuggerScope, m_byteCodeData.GetCurrentOffset() - 1);
        PopDebuggerScope();
    }

    void ByteCodeWriter::UpdateDebuggerPropertyInitializationOffset(
        Js::DebuggerScope* currentDebuggerScope,
        Js::RegSlot location,
        Js::PropertyId propertyId,
        bool shouldConsumeRegister/* = true*/,
        int byteCodeOffset/* = Constants::InvalidOffset*/,
        bool isFunctionDeclaration /*= false*/)
    {
#if DBG
        bool isInDebugMode = m_isInDebugMode
#if DBG_DUMP
            || Js::Configuration::Global.flags.Debug
#endif // DBG_DUMP
            ;

        AssertMsg(isInDebugMode, "Property offsets should only ever be updated in debug mode (not used in non-debug).");
#endif // DBG

        Assert(currentDebuggerScope);

        if (shouldConsumeRegister)
        {
            Assert(location != Js::Constants::NoRegister);
            location = ConsumeReg(location);
        }

        if (byteCodeOffset == Constants::InvalidOffset)
        {
            // Use the current offset if no offset is passed in.
            byteCodeOffset = this->m_byteCodeData.GetCurrentOffset();
        }

        // Search through the scope chain starting with the current up through the parents to see if the
        // property can be found and updated.
        while (currentDebuggerScope != nullptr)
        {
            if (currentDebuggerScope->UpdatePropertyInitializationOffset(location, propertyId, byteCodeOffset, isFunctionDeclaration))
            {
                break;
            }

            currentDebuggerScope = currentDebuggerScope->GetParentScope();
        }
    }

    void ByteCodeWriter::RecordFrameDisplayRegister(RegSlot slot)
    {
        slot = ConsumeReg(slot);
        m_functionWrite->RecordFrameDisplayRegister(slot);
    }

    void ByteCodeWriter::RecordObjectRegister(RegSlot slot)
    {
        slot = ConsumeReg(slot);
        m_functionWrite->RecordObjectRegister(slot);
    }

    void ByteCodeWriter::RecordStatementAdjustment(FunctionBody::StatementAdjustmentType type)
    {
        if (m_isInDebugMode)
        {
            m_functionWrite->RecordStatementAdjustment(m_byteCodeData.GetCurrentOffset(), type);
        }
    }

    void ByteCodeWriter::RecordCrossFrameEntryExitRecord(bool isEnterBlock)
    {
        if (m_isInDebugMode)
        {
            m_functionWrite->RecordCrossFrameEntryExitRecord(m_byteCodeData.GetCurrentOffset(), isEnterBlock);
        }
    }

    void ByteCodeWriter::RecordForInOrOfCollectionScope()
    {
        if (m_isInDebugMode && this->m_currentDebuggerScope != nullptr)
        {
            this->m_currentDebuggerScope->UpdatePropertiesInForInOrOfCollectionScope();
        }
    }

    uint ByteCodeWriter::EnterLoop(Js::ByteCodeLabel loopEntrance)
    {
#ifdef BYTECODE_BRANCH_ISLAND
        if (useBranchIsland)
        {
            // If we are going to emit a branch island, it should be before the loop header
            this->EnsureLongBranch(Js::OpCode::StatementBoundary);
        }
#endif

        uint loopId = m_functionWrite->IncrLoopCount();
        Assert((uint)m_loopHeaders->Count() == loopId);

        m_loopHeaders->Add(LoopHeaderData(m_byteCodeData.GetCurrentOffset(), 0, m_loopNest > 0));
        m_loopNest++;
        m_functionWrite->SetHasNestedLoop(m_loopNest > 1);

        Js::OpCode loopBodyOpcode = Js::OpCode::LoopBodyStart;
#if ENABLE_PROFILE_INFO
        if (Js::DynamicProfileInfo::EnableImplicitCallFlags(GetFunctionWrite()))
        {
            this->Unsigned1(Js::OpCode::ProfiledLoopStart, loopId);
            loopBodyOpcode = Js::OpCode::ProfiledLoopBodyStart;
        }
#endif

        this->MarkLabel(loopEntrance);
        if (this->DoJitLoopBodies() || this->DoInterruptProbes())
        {
            this->Unsigned1(loopBodyOpcode, loopId);
        }

        return loopId;
    }

    void ByteCodeWriter::ExitLoop(uint loopId)
    {
#if ENABLE_PROFILE_INFO
        if (Js::DynamicProfileInfo::EnableImplicitCallFlags(GetFunctionWrite()))
        {
            this->Unsigned1(Js::OpCode::ProfiledLoopEnd, loopId);
        }
#endif
        Assert(m_loopNest > 0);
        m_loopNest--;
        m_loopHeaders->Item(loopId).endOffset = m_byteCodeData.GetCurrentOffset();
    }

    void ByteCodeWriter::IncreaseByteCodeCount()
    {
        m_byteCodeCount++;
        if (m_loopNest > 0)
        {
            m_byteCodeInLoopCount++;
        }
    }

    void ByteCodeWriter::Data::Create(uint initSize, ArenaAllocator* tmpAlloc)
    {
        //
        // Allocate the initial byte-code block to write into.
        //

        tempAllocator = tmpAlloc;
        AssertMsg(head == nullptr, "Missing dispose?");
        currentOffset = 0;
        head = Anew(tempAllocator, DataChunk, tempAllocator, initSize);
        current = head;
    }

    void ByteCodeWriter::Data::Reset()
    {
        currentOffset = 0;
        DataChunk* currentChunk = head;
        while (currentChunk)
        {
            // reset to the starting point
            currentChunk->Reset();
            currentChunk = currentChunk->nextChunk;
        }

        current = head;
    }

    void ByteCodeWriter::Data::SetCurrent(uint offset, DataChunk* currChunk)
    {
        this->current = currChunk;
        this->currentOffset = offset;
    }

    /// Copies its contents to a final contiguous section of memory.
    void ByteCodeWriter::Data::Copy(Recycler* alloc, ByteBlock ** finalBlock)
    {
        AssertMsg(finalBlock != nullptr, "Must have valid storage");

        uint cbFinalData = GetCurrentOffset();
        if (cbFinalData == 0)
        {
            *finalBlock = nullptr;
        }
        else
        {
            ByteBlock* finalByteCodeBlock = ByteBlock::New(alloc, /*intialContent*/nullptr, cbFinalData);

            DataChunk* currentChunk = head;
            size_t bytesLeftToCopy = cbFinalData;
            byte* currentDest = finalByteCodeBlock->GetBuffer();
            while (true)
            {
                if (bytesLeftToCopy <= currentChunk->GetSize())
                {
                    js_memcpy_s(currentDest, bytesLeftToCopy, currentChunk->GetBuffer(), bytesLeftToCopy);
                    break;
                }

                js_memcpy_s(currentDest, bytesLeftToCopy, currentChunk->GetBuffer(), currentChunk->GetSize());
                bytesLeftToCopy -= currentChunk->GetSize();
                currentDest += currentChunk->GetSize();

                currentChunk = currentChunk->nextChunk;
                AssertMsg(currentChunk, "We are copying more data than we have!");
            }

            *finalBlock = finalByteCodeBlock;
        }
    }

    template <>
    __inline uint ByteCodeWriter::Data::EncodeT<SmallLayout>(OpCode op, ByteCodeWriter* writer)
    {
#ifdef BYTECODE_BRANCH_ISLAND
        if (writer->useBranchIsland)
        {
            writer->EnsureLongBranch(op);
        }
#endif
        Assert(op < Js::OpCode::ByteCodeLast);
        Assert(!OpCodeAttr::BackEndOnly(op));
        uint offset;
        if (op <= Js::OpCode::MaxByteSizedOpcodes)
        {
            byte byteop = (byte)op;
            offset = Write(&byteop, sizeof(byte));
        }
        else
        {
            byte byteop = (byte)Js::OpCode::ExtendedOpcodePrefix;
            offset = Write(&byteop, sizeof(byte));
            byteop = (byte)op;
            Write(&byteop, sizeof(byte));
        }
        if (op != Js::OpCode::Ld_A)
        {
            writer->m_byteCodeWithoutLDACount++;
        }

        writer->IncreaseByteCodeCount();
        return offset;
    }

    template <LayoutSize layoutSize>
    __inline uint ByteCodeWriter::Data::EncodeT(OpCode op, ByteCodeWriter* writer)
    {
#ifdef BYTECODE_BRANCH_ISLAND
        if (writer->useBranchIsland)
        {
            writer->EnsureLongBranch(op);
        }
#endif

        Assert(op < Js::OpCode::ByteCodeLast);
        Assert(!OpCodeAttr::BackEndOnly(op));
        Assert(OpCodeAttr::HasMultiSizeLayout(op));
        CompileAssert(layoutSize != SmallLayout);
        const byte exop = (byte)((op <= Js::OpCode::MaxByteSizedOpcodes) ?
            (layoutSize == LargeLayout ? Js::OpCode::LargeLayoutPrefix : Js::OpCode::MediumLayoutPrefix) :
            (layoutSize == LargeLayout ? Js::OpCode::ExtendedLargeLayoutPrefix : Js::OpCode::ExtendedMediumLayoutPrefix));

        uint offset = Write(&exop, sizeof(byte));
        Write(&op, sizeof(byte));

        if (op != Js::OpCode::Ld_A)
        {
            writer->m_byteCodeWithoutLDACount++;
        }
        writer->IncreaseByteCodeCount();
        return offset;
    }

    template <LayoutSize layoutSize>
    __inline uint ByteCodeWriter::Data::EncodeT(OpCode op, const void* rawData, int byteSize, ByteCodeWriter* writer)
    {
        AssertMsg((rawData != nullptr) && (byteSize < 100), "Ensure valid data for opcode");

        uint offset = EncodeT<layoutSize>(op, writer);
        Write(rawData, byteSize);
        return offset;
    }

    __inline uint ByteCodeWriter::Data::Encode(const void* rawData, int byteSize)
    {
        AssertMsg(rawData != nullptr, "Ensure valid data for opcode");
        return Write(rawData, byteSize);
    }

    __inline uint ByteCodeWriter::Data::Write(__in_bcount(byteSize) const void* data, __in uint byteSize)
    {
        // Simple case where the current chunk has enough space.
        uint bytesFree = current->RemainingBytes();
        if (bytesFree >= byteSize)
        {
            current->WriteUnsafe(data, byteSize);
        }
        else
        {
            SlowWrite(data, byteSize);
        }

        uint offset = currentOffset;
        currentOffset = offset + byteSize;
        return offset;
    }

    /// Requires buffer extension.
    __declspec(noinline) void ByteCodeWriter::Data::SlowWrite(__in_bcount(byteSize) const void* data, __in uint byteSize)
    {
        AssertMsg(byteSize > current->RemainingBytes(), "We should not need an extension if there is enough space in the current chunk");
        uint bytesLeftToWrite = byteSize;
        byte* dataToBeWritten = (byte*)data;
        // the next chunk may already be created in the case that we are patching bytecode.
        // If so, we want to move the pointer to the beginning of the buffer
        if (current->nextChunk)
        {
            current->nextChunk->SetCurrentOffset(0);
        }
        while (true)
        {
            uint bytesFree = current->RemainingBytes();
            if (bytesFree >= bytesLeftToWrite)
            {
                current->WriteUnsafe(dataToBeWritten, bytesLeftToWrite);
                break;
            }

            current->WriteUnsafe(dataToBeWritten, bytesFree);
            bytesLeftToWrite -= bytesFree;
            dataToBeWritten += bytesFree;

            // Create a new chunk when needed
            if (!current->nextChunk)
            {
                AddChunk(bytesLeftToWrite);
            }
            current = current->nextChunk;
        }
    }

    void ByteCodeWriter::Data::AddChunk(uint byteSize)
    {
        AssertMsg(current->nextChunk == nullptr, "Do we really need to grow?");

        // For some data elements i.e. bytecode we have a good initial size and
        // therefore, we use a conservative growth strategy - and grow by a fixed size.
        uint newSize = fixedGrowthPolicy ? max(byteSize, static_cast<uint>(3 * AutoSystemInfo::PageSize)) : max(byteSize, static_cast<uint>(current->GetSize() * 2));

        DataChunk* newChunk = Anew(tempAllocator, DataChunk, tempAllocator, newSize);
        current->nextChunk = newChunk;
    }

#if DBG_DUMP
    uint ByteCodeWriter::ByteCodeDataSize()
    {
        return m_byteCodeData.GetCurrentOffset();
    }

    uint ByteCodeWriter::AuxiliaryDataSize()
    {
        return m_auxiliaryData.GetCurrentOffset();
    }

    uint ByteCodeWriter::AuxiliaryContextDataSize()
    {
        return m_auxContextData.GetCurrentOffset();
    }

#endif

} // namespace Js
