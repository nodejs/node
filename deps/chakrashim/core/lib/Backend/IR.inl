//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace IR {

inline IRKind
Instr::GetKind() const
{
    return this->m_kind;
}

///----------------------------------------------------------------------------
///
/// Instr::IsEntryInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsEntryInstr() const
{
    return this->GetKind() == InstrKindEntry;
}

///----------------------------------------------------------------------------
///
/// Instr::AsEntryInstr
///
///     Return this as a EntryInstr *
///
///----------------------------------------------------------------------------

inline EntryInstr *
Instr::AsEntryInstr()
{
    AssertMsg(this->IsEntryInstr(), "Bad call to AsEntryInstr()");

    return reinterpret_cast<EntryInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsExitInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsExitInstr() const
{
    return this->GetKind() == InstrKindExit;
}

///----------------------------------------------------------------------------
///
/// Instr::AsExitInstr
///
///     Return this as a ExitInstr *
///
///----------------------------------------------------------------------------

inline ExitInstr *
Instr::AsExitInstr()
{
    AssertMsg(this->IsExitInstr(), "Bad call to AsExitInstr()");

    return reinterpret_cast<ExitInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsBranchInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsBranchInstr() const
{
    return this->GetKind() == InstrKindBranch;
}

///----------------------------------------------------------------------------
///
/// Instr::AsBranchInstr
///
///     Return this as a BranchInstr *
///
///----------------------------------------------------------------------------

inline BranchInstr *
Instr::AsBranchInstr()
{
    AssertMsg(this->IsBranchInstr(), "Bad call to AsBranchInstr()");

    return reinterpret_cast<BranchInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsLabelInstr
///
///----------------------------------------------------------------------------

__forceinline bool
Instr::IsLabelInstr() const
{
    return this->GetKind() == InstrKindLabel || this->IsProfiledLabelInstr();
}

///----------------------------------------------------------------------------
///
/// Instr::AsLabelInstr
///
///     Return this as a LabelInstr *
///
///----------------------------------------------------------------------------

inline LabelInstr *
Instr::AsLabelInstr()
{
    AssertMsg(this->IsLabelInstr(), "Bad call to AsLabelInstr()");

    return reinterpret_cast<LabelInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::AsMultiBrInstr
///
///     Return this as a MultiBrInstr *
///
///----------------------------------------------------------------------------
inline MultiBranchInstr *
BranchInstr::AsMultiBrInstr()
{
    AssertMsg(this->IsMultiBranch(), "Bad call to AsMultiBrInstr()");

    return static_cast<MultiBranchInstr *>(this);
}

///----------------------------------------------------------------------------
///
/// Instr::IsPragmaInstr
///
///----------------------------------------------------------------------------

inline bool
Instr::IsPragmaInstr() const
{
    return this->GetKind() == InstrKindPragma;
}

inline PragmaInstr *
Instr::AsPragmaInstr()
{
    AssertMsg(this->IsPragmaInstr(), "Bad call to AsPragmaInstr()");

    return reinterpret_cast<PragmaInstr *>(this);
}

inline bool
Instr::IsJitProfilingInstr() const
{
    return this->GetKind() == InstrKindJitProfiling;
}

inline JitProfilingInstr *
Instr::AsJitProfilingInstr()
{
    AssertMsg(this->IsJitProfilingInstr(), "Bad call to AsProfiledInstr()");

    return reinterpret_cast<JitProfilingInstr *>(this);
}

inline bool
Instr::IsProfiledInstr() const
{
    return this->GetKind() == InstrKindProfiled;
}

inline ProfiledInstr *
Instr::AsProfiledInstr()
{
    AssertMsg(this->IsProfiledInstr(), "Bad call to AsProfiledInstr()");

    return reinterpret_cast<ProfiledInstr *>(this);
}

inline bool
Instr::IsProfiledLabelInstr() const
{
    return this->GetKind() == InstrKindProfiledLabel;
}

inline ProfiledLabelInstr *
Instr::AsProfiledLabelInstr()
{
    AssertMsg(this->IsProfiledLabelInstr(), "Bad call to AsProfiledlLabelInstr()");

    return reinterpret_cast<ProfiledLabelInstr *>(this);
}

inline bool
Instr::IsByteCodeUsesInstr() const
{
    return GetKind() == IR::InstrKindByteCodeUses;
}

inline ByteCodeUsesInstr *
Instr::AsByteCodeUsesInstr()
{
    AssertMsg(this->IsByteCodeUsesInstr(), "Bad call to AsByteCodeUsesInstr()");
    return (ByteCodeUsesInstr *)this;
}

///----------------------------------------------------------------------------
///
/// Instr::IsLowered
///
///     Is this instr lowered to machine dependent opcode?
///
///----------------------------------------------------------------------------

inline bool
Instr::IsLowered() const
{
    return m_opcode > Js::OpCode::MDStart;
}

///----------------------------------------------------------------------------
///
/// Instr::StartsBasicBlock
///
///     Does this instruction mark the beginning of a basic block?
///
///----------------------------------------------------------------------------

inline bool
Instr::StartsBasicBlock() const
{
    return this->IsLabelInstr() || this->IsEntryInstr();
}

///----------------------------------------------------------------------------
///
/// Instr::EndsBasicBlock
///
///     Does this instruction mark the end of a basic block?
///
///----------------------------------------------------------------------------

inline bool
Instr::EndsBasicBlock() const
{
    return
        this->IsBranchInstr() ||
        this->IsExitInstr() ||
        this->m_opcode == Js::OpCode::Ret ||
        this->m_opcode == Js::OpCode::Throw ||
        this->m_opcode == Js::OpCode::RuntimeTypeError ||
        this->m_opcode == Js::OpCode::RuntimeReferenceError;
}

///----------------------------------------------------------------------------
///
/// Instr::HasFallThrough
///
///     Can execution fall through to the next instruction?
///
///----------------------------------------------------------------------------

inline bool
Instr::HasFallThrough() const
{
    return (!(this->IsBranchInstr() && const_cast<Instr*>(this)->AsBranchInstr()->IsUnconditional())
            && OpCodeAttr::HasFallThrough(this->m_opcode));

}


inline bool
Instr::IsNewScObjectInstr() const
{
    return this->m_opcode == Js::OpCode::NewScObject || this->m_opcode == Js::OpCode::NewScObjectNoCtor;
}

inline bool
Instr::IsInvalidInstr() const
{
    return this == (Instr*)InvalidInstrLayout;
}

inline Instr*
Instr::GetInvalidInstr()
{
    return (Instr*)InvalidInstrLayout;
}

///----------------------------------------------------------------------------
///
/// Instr::GetDst
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::GetDst() const
{
    return this->m_dst;
}

///----------------------------------------------------------------------------
///
/// Instr::GetSrc1
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::GetSrc1() const
{
    return this->m_src1;
}

///----------------------------------------------------------------------------
///
/// Instr::SetSrc1
///
///     Makes a copy if opnd is in use...
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::SetSrc1(Opnd * newSrc)
{
    AssertMsg(this->m_src1 == NULL, "Trying to overwrite existing src.");

    newSrc = newSrc->Use(m_func);
    this->m_src1 = newSrc;

    return newSrc;
}

///----------------------------------------------------------------------------
///
/// Instr::GetSrc2
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::GetSrc2() const
{
    return this->m_src2;
}

///----------------------------------------------------------------------------
///
/// Instr::SetSrc2
///
///     Makes a copy if opnd is in use...
///
///----------------------------------------------------------------------------

inline Opnd *
Instr::SetSrc2(Opnd * newSrc)
{
    AssertMsg(this->m_src2 == NULL, "Trying to overwrite existing src.");

    newSrc = newSrc->Use(m_func);
    this->m_src2 = newSrc;

    return newSrc;
}

// IsInlined - true if the function that contains the instr has been inlined
inline bool
Instr::IsInlined() const
{
    return this->m_func->IsInlined();
}


///----------------------------------------------------------------------------
///
/// Instr::ForEachCallDirectArgOutInstrBackward
///
///     Walks the ArgsOut of CallDirect backwards
///
///----------------------------------------------------------------------------
template <typename Fn>
inline bool
Instr::ForEachCallDirectArgOutInstrBackward(Fn fn, uint argsOpndLength) const
{
    Assert(this->m_opcode == Js::OpCode::CallDirect); // Right now we call this method only for partial inlining of split, match and exec. Can be modified for other uses also.

    // CallDirect src2
    IR::Opnd * linkOpnd = this->GetSrc2();

    // ArgOut_A_InlineSpecialized
    IR::Instr * tmpInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;
    Assert(tmpInstr->m_opcode == Js::OpCode::ArgOut_A_InlineSpecialized);

    // ArgOut_A_InlineSpecialized src2; link to actual argouts.
    linkOpnd = tmpInstr->GetSrc2();
    uint32 argCount = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();

    if (argCount != argsOpndLength)
    {
        return false;
    }

    while (linkOpnd->IsSymOpnd() && argCount > 0)
    {
        IR::SymOpnd *src2 = linkOpnd->AsSymOpnd();
        StackSym *sym = src2->m_sym->AsStackSym();
        Assert(sym->m_isSingleDef);
        IR::Instr *argInstr = sym->m_instrDef;
        Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A);
        if (fn(argInstr, argCount - 1))
        {
            return true;
        }
        argCount--;

        linkOpnd = argInstr->GetSrc2();
    }
    return false;
}

///----------------------------------------------------------------------------
///
/// BranchInstr::SetTarget
///
///----------------------------------------------------------------------------

inline void
BranchInstr::SetTarget(LabelInstr * labelInstr)
{
    Assert(!this->m_isMultiBranch);
    if (this->m_branchTarget)
    {
        this->m_branchTarget->RemoveLabelRef(this);
    }
    if (labelInstr)
    {
        labelInstr->AddLabelRef(this);
    }
    this->m_branchTarget = labelInstr;
}

inline void
BranchInstr::ClearTarget()
{
    if (this->IsMultiBranch())
    {
        this->AsMultiBrInstr()->ClearTarget();
    }
    else
    {
        this->SetTarget(nullptr);
    }
}

///----------------------------------------------------------------------------
///
/// BranchInstr::GetTarget
///
///----------------------------------------------------------------------------

inline LabelInstr *
BranchInstr::GetTarget() const
{
    return this->m_branchTarget;
}

///----------------------------------------------------------------------------
///
/// BranchInstr::IsConditional
///
///----------------------------------------------------------------------------

inline bool
BranchInstr::IsConditional() const
{
    return !this->IsUnconditional();
}

inline bool
BranchInstr::IsMultiBranch() const
{
    if (m_branchTarget)
    {
        Assert(!m_isMultiBranch);
        return false;
    }
    else
    {
        Assert(m_isMultiBranch);
        return true;    // it's a multi branch instr
    }

}

///----------------------------------------------------------------------------
///
/// BranchInstr::IsUnconditional
///
///----------------------------------------------------------------------------

inline bool
BranchInstr::IsUnconditional() const
{
    if (this->IsLowered())
    {
        return LowererMD::IsUnconditionalBranch(this);
    }
    else
    {
        return (this->m_opcode == Js::OpCode::Br || this->m_opcode == Js::OpCode::MultiBr);
    }
}

///----------------------------------------------------------------------------
///
/// MultiBranchInstr::AddtoDictionary
///     - Adds the string to the list with the targetoffset
///       In order for the dictionary to have the right value, MapBranchTargetAddress
///       needs to be called to populate the dictionary and then it'll be patched up
///       to the right values
///
///----------------------------------------------------------------------------
inline void
MultiBranchInstr::AddtoDictionary(uint32 offset, TBranchKey key)
{
    Assert(this->m_kind == StrDictionary);
    Assert(key);
    this->GetBranchDictionary()->dictionary.AddNew(key, (void*)offset);
}

inline void
MultiBranchInstr::AddtoJumpTable(uint32 offset, uint32 jmpIndex)
{
    Assert(this->m_kind == IntJumpTable || this->m_kind == SingleCharStrJumpTable);
    Assert(jmpIndex != -1);
    this->GetBranchJumpTable()->jmpTable[jmpIndex] = (void*)offset;
}

inline void
MultiBranchInstr::FixMultiBrDefaultTarget(uint32 targetOffset)
{
    this->GetBranchJumpTable()->defaultTarget = (void *)targetOffset;
}

inline void
MultiBranchInstr::CreateBranchTargetsAndSetDefaultTarget(int size, Kind kind, uint defaultTargetOffset)
{
    AssertMsg(size != 0, "The dictionary/jumpTable size should not be zero");

    NativeCodeData::Allocator * allocator = this->m_func->GetNativeCodeDataAllocator();
    m_kind = kind;
    switch (kind)
    {
    case IntJumpTable:
    case SingleCharStrJumpTable:
        {
            JitArenaAllocator * jitAllocator = this->m_func->GetTopFunc()->m_alloc;
            BranchJumpTableWrapper * branchTargets = BranchJumpTableWrapper::New(jitAllocator, size);
            branchTargets->defaultTarget = (void *)defaultTargetOffset;
            this->m_branchTargets = branchTargets;
            break;
        }
    case StrDictionary:
        {
            BranchDictionaryWrapper * branchTargets = BranchDictionaryWrapper::New(allocator, size);
            branchTargets->defaultTarget = (void *)defaultTargetOffset;
            this->m_branchTargets = branchTargets;
            break;
        }
    default:
        Assert(false);
    };
}

inline MultiBranchInstr::BranchDictionaryWrapper *
MultiBranchInstr::GetBranchDictionary()
{
    return reinterpret_cast<MultiBranchInstr::BranchDictionaryWrapper *>(m_branchTargets);
}

inline MultiBranchInstr::BranchJumpTable *
MultiBranchInstr::GetBranchJumpTable()
{
    return reinterpret_cast<MultiBranchInstr::BranchJumpTable *>(m_branchTargets);
}

inline void
MultiBranchInstr::ChangeLabelRef(LabelInstr * oldTarget, LabelInstr * newTarget)
{
    if (oldTarget)
    {
        oldTarget->RemoveLabelRef(this);
    }
    if (newTarget)
    {
        newTarget->AddLabelRef(this);
    }
}

///----------------------------------------------------------------------------
///
/// LabelInstr::SetPC
///
///----------------------------------------------------------------------------

inline void
LabelInstr::SetPC(BYTE * pc)
{
    this->m_pc.pc = pc;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::GetPC
///
///----------------------------------------------------------------------------

inline BYTE *
LabelInstr::GetPC(void) const
{
    return this->m_pc.pc;
}
///----------------------------------------------------------------------------
///
/// LabelInstr::ResetOffset
///
///----------------------------------------------------------------------------

inline void
LabelInstr::ResetOffset(uint32 offset)
{
    AssertMsg(this->isInlineeEntryInstr, "As of now only InlineeEntryInstr overwrites the offset at encoder stage");
    this->m_pc.offset = offset;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::SetOffset
///
///----------------------------------------------------------------------------

inline void
LabelInstr::SetOffset(uint32 offset)
{
    AssertMsg(this->m_pc.offset == 0, "Overwriting existing byte offset");
    this->m_pc.offset = offset;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::GetOffset
///
///----------------------------------------------------------------------------

inline uint32
LabelInstr::GetOffset(void) const
{

    return this->m_pc.offset;
}



///----------------------------------------------------------------------------
///
/// LabelInstr::SetBasicBlock
///
///----------------------------------------------------------------------------

inline void
LabelInstr::SetBasicBlock(BasicBlock * block)
{
    AssertMsg(this->m_block == nullptr || block == nullptr, "Overwriting existing block pointer");
    this->m_block = block;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::GetBasicBlock
///
///----------------------------------------------------------------------------

inline BasicBlock *
LabelInstr::GetBasicBlock(void) const
{
    return this->m_block;
}

inline void
LabelInstr::SetLoop(Loop* loop)
{
    Assert(this->m_isLoopTop);
    this->m_loop = loop;
}

inline Loop*
LabelInstr::GetLoop(void) const
{
    Assert(this->m_isLoopTop);
    return this->m_loop;
}

///----------------------------------------------------------------------------
///
/// LabelInstr::UnlinkBasicBlock
///
///----------------------------------------------------------------------------

inline void
LabelInstr::UnlinkBasicBlock(void)
{
    this->m_block = nullptr;
}

inline BOOL
LabelInstr::IsUnreferenced(void) const
{
    return labelRefs.Empty() && !m_hasNonBranchRef;
}

inline void
LabelInstr::SetRegion(Region * region)
{
    this->m_region = region;
}

inline Region *
LabelInstr::GetRegion(void) const
{
    return this->m_region;
}

} // namespace IR
