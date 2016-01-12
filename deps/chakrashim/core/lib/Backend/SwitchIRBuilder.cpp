//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "DataStructures\QuickSort.h"

///----------------------------------------------------------------------------
///
/// IRBuilderSwitchAdapter
///
///     Implementation for IRBuilderSwitchAdapter, which passes actions generated
///     by a SwitchIRBuilder to an IRBuilder instance
///----------------------------------------------------------------------------

void
IRBuilderSwitchAdapter::AddBranchInstr(IR::BranchInstr * instr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{
    BranchReloc * reloc = m_builder->AddBranchInstr(instr, offset, targetOffset);

    if (clearBackEdge)
    {
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderSwitchAdapter::AddInstr(IR::Instr * instr, uint32 offset)
{
    m_builder->AddInstr(instr, offset);
}

void
IRBuilderSwitchAdapter::CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{
    BranchReloc * reloc = m_builder->CreateRelocRecord(
        branchInstr,
        offset,
        targetOffset);

    if (clearBackEdge)
    {
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderSwitchAdapter::ConvertToBailOut(IR::Instr * instr, IR::BailOutKind kind)
{
    instr = instr->ConvertToBailOutInstr(instr, kind);

    Assert(instr->GetByteCodeOffset() < m_builder->m_offsetToInstructionCount);
    m_builder->m_offsetToInstruction[instr->GetByteCodeOffset()] = instr;
}

///----------------------------------------------------------------------------
///
/// IRBuilderAsmJsSwitchAdapter
///
///     Implementation for IRBuilderSwitchAdapter, which passes actions generated
///     by a SwitchIRBuilder to an IRBuilder instance
///----------------------------------------------------------------------------

void
IRBuilderAsmJsSwitchAdapter::AddBranchInstr(IR::BranchInstr * instr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{
    BranchReloc * reloc = m_builder->AddBranchInstr(instr, offset, targetOffset);

    if (clearBackEdge)
    {
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderAsmJsSwitchAdapter::AddInstr(IR::Instr * instr, uint32 offset)
{
    m_builder->AddInstr(instr, offset);
}

void
IRBuilderAsmJsSwitchAdapter::CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset, bool clearBackEdge)
{
    BranchReloc * reloc = m_builder->CreateRelocRecord(
        branchInstr,
        offset,
        targetOffset);

    if (clearBackEdge)
    {
        reloc->SetNotBackEdge();
    }
}

void
IRBuilderAsmJsSwitchAdapter::ConvertToBailOut(IR::Instr * instr, IR::BailOutKind kind)
{
    Assert(false);
    // ConvertToBailOut should never get called for AsmJs
    // switches, since we already know ahead of time that the
    // switch expression is Int32
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::Init
///
///     Initializes the function and temporary allocator for the SwitchIRBuilder
///----------------------------------------------------------------------------

void
SwitchIRBuilder::Init(Func * func, JitArenaAllocator * tempAlloc, bool isAsmJs)
{
    m_func = func;
    m_tempAlloc = tempAlloc;
    m_isAsmJs = isAsmJs;

    // caseNodes is a list of Case instructions
    m_caseNodes = CaseNodeList::New(tempAlloc);
    m_seenOnlySingleCharStrCaseNodes = true;
    m_intConstSwitchCases = JitAnew(tempAlloc, BVSparse<JitArenaAllocator>, tempAlloc);
    m_strConstSwitchCases = StrSwitchCaseList::New(tempAlloc);

    m_eqOp = isAsmJs ? Js::OpCode::BrEq_I4 : Js::OpCode::BrSrEq_A;
    m_ltOp = isAsmJs ? Js::OpCode::BrLt_I4 : Js::OpCode::BrLt_A;
    m_leOp = isAsmJs ? Js::OpCode::BrLe_I4 : Js::OpCode::BrLe_A;
    m_gtOp = isAsmJs ? Js::OpCode::BrGt_I4 : Js::OpCode::BrGt_A;
    m_geOp = isAsmJs ? Js::OpCode::BrGe_I4 : Js::OpCode::BrGe_A;
    m_subOp = isAsmJs ? Js::OpCode::Sub_I4 : Js::OpCode::Sub_A;
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BeginSwitch
///
///     Prepares the SwitchIRBuilder for building a new switch statement
///----------------------------------------------------------------------------

void
SwitchIRBuilder::BeginSwitch()
{
    m_intConstSwitchCases->ClearAll();
    m_strConstSwitchCases->Clear();

    if (m_isAsmJs)
    {
        // never build bailout information for asmjs
        m_switchOptBuildBail = false;
        // asm.js switch is always integer
        m_switchIntDynProfile = true;
        AssertMsg(!m_switchStrDynProfile, "String profiling should not be enabled for an asm.js switch statement");
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::EndSwitch
///
///     Notifies the switch builder the switch being generated has been completed
///----------------------------------------------------------------------------

void
SwitchIRBuilder::EndSwitch(uint32 offset, uint32 targetOffset)
{
    FlushCases(targetOffset);
    AssertMsg(m_caseNodes->Count() == 0, "Not all switch case nodes built by end of switch");

    // only generate the final unconditional jump at the end of the switch
    IR::BranchInstr * branch = IR::BranchInstr::New(Js::OpCode::Br, nullptr, m_func);
    m_adapter->AddBranchInstr(branch, offset, targetOffset, true);

    m_profiledSwitchInstr = nullptr;
}


///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::SetProfiledInstruction
///
///     Sets the profiled switch instruction for the switch statement that
///     is being built
///----------------------------------------------------------------------------


void
SwitchIRBuilder::SetProfiledInstruction(IR::Instr * instr, Js::ProfileId profileId)
{
    m_profiledSwitchInstr = instr;
    m_switchOptBuildBail = true;

    //don't optimize if the switch expression is not an Integer (obtained via dynamic profiling data of the BeginSwitch opcode)

    bool hasProfile = m_profiledSwitchInstr->IsProfiledInstr() && m_profiledSwitchInstr->m_func->HasProfileInfo();

    if (hasProfile)
    {
        const ValueType valueType(m_profiledSwitchInstr->m_func->GetProfileInfo()->GetSwitchProfileInfo(m_profiledSwitchInstr->m_func->GetJnFunction(), profileId));
        instr->AsProfiledInstr()->u.FldInfo().valueType = valueType;
        m_switchIntDynProfile = valueType.IsLikelyTaggedInt();
        m_switchStrDynProfile = valueType.IsLikelyString();

        if (PHASE_TESTTRACE1(Js::SwitchOptPhase))
        {
            char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            valueType.ToString(valueTypeStr);
#if ENABLE_DEBUG_CONFIG_OPTIONS
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            PHASE_PRINT_TESTTRACE1(Js::SwitchOptPhase, L"Func %s, Switch %d: Expression Type : %S\n",
                m_profiledSwitchInstr->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                m_profiledSwitchInstr->AsProfiledInstr()->u.profileId, valueTypeStr);
        }
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::OnCase
///
///     Handles a case instruction, generating the appropriate branches, or
///     storing the case to generate a more optimized branch later
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::OnCase(IR::RegOpnd * src1Opnd, IR::RegOpnd * src2Opnd, uint32 offset, uint32 targetOffset)
{
    IR::BranchInstr * branchInstr;

    if (src2Opnd->m_sym->m_isIntConst && m_intConstSwitchCases->TestAndSet(src2Opnd->m_sym->GetIntConstValue()))
    {
        // We've already seen a case statement with the same int const value. No need to emit anything for this.
        return;
    }

    if (src2Opnd->m_sym->m_isStrConst && TestAndAddStringCaseConst(Js::JavascriptString::FromVar(src2Opnd->GetStackSym()->GetConstAddress())))
    {
        // We've already seen a case statement with the same string const value. No need to emit anything for this.
        return;
    }

    branchInstr = IR::BranchInstr::New(m_eqOp, nullptr, src1Opnd, src2Opnd, m_func);
    branchInstr->m_isSwitchBr = true;

    /*
    //  Switch optimization
    //  For Integers - Binary Search or jump table optimization technique is used
    //  For Strings - Dictionary look up technique is used.
    //
    //  For optimizing, the Load instruction corresponding to the switch instruction is profiled in the interpreter.
    //  Based on the dynamic profile data, optimization technique is decided.
    */

    bool deferred = false;

    if (GlobOpt::IsSwitchOptEnabled(m_func->GetTopFunc()))
    {
        if (m_switchIntDynProfile && src2Opnd->m_sym->IsIntConst())
        {
            CaseNode* caseNode = JitAnew(m_tempAlloc, CaseNode, branchInstr, offset, targetOffset, src2Opnd);
            m_caseNodes->Add(caseNode);
            deferred = true;
        }
        else if (m_switchStrDynProfile && src2Opnd->m_sym->m_isStrConst)
        {
            CaseNode* caseNode = JitAnew(m_tempAlloc, CaseNode, branchInstr, offset, targetOffset, src2Opnd);
            m_caseNodes->Add(caseNode);
            m_seenOnlySingleCharStrCaseNodes = m_seenOnlySingleCharStrCaseNodes && caseNode->GetSrc2StringConst()->GetLength() == 1;
            deferred = true;
        }
    }

    if (!deferred)
    {
        FlushCases(offset);
        m_adapter->AddBranchInstr(branchInstr, offset, targetOffset);
    }
}


///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::FlushCases
///
///     Called when a scenario for which optimized switch cases cannot be
///     generated occurs, and generates optimized branches for all cases that
///     have been stored up to this point
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::FlushCases(uint32 targetOffset)
{
    if (m_caseNodes->Empty())
    {
        return;
    }

    if (m_switchIntDynProfile)
    {
        BuildCaseBrInstr(targetOffset);
    }
    else if (m_switchStrDynProfile)
    {
        BuildMultiBrCaseInstrForStrings(targetOffset);
    }
    else
    {
        Assert(false);
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::RefineCaseNodes
///
///     Filter IR instructions for case statements that contain no case blocks.
///     Also sets upper bound and lower bound for case instructions that has a
///     consecutive set of cases with just one case block.
///----------------------------------------------------------------------------

void
SwitchIRBuilder::RefineCaseNodes()
{
    m_caseNodes->Sort();

    CaseNodeList * tmpCaseNodes = CaseNodeList::New(m_tempAlloc);

    for (int currCaseIndex = 1; currCaseIndex < m_caseNodes->Count(); currCaseIndex++)
    {
        CaseNode * prevCaseNode = m_caseNodes->Item(currCaseIndex - 1);
        CaseNode * currCaseNode = m_caseNodes->Item(currCaseIndex);
        uint32 prevCaseTargetOffset = prevCaseNode->GetTargetOffset();
        uint32 currCaseTargetOffset = currCaseNode->GetTargetOffset();
        int prevCaseConstValue = prevCaseNode->GetSrc2IntConst();
        int currCaseConstValue = currCaseNode->GetSrc2IntConst();

        /*To handle empty case statements with/without repetition*/
        if (prevCaseTargetOffset == currCaseTargetOffset &&
            (prevCaseConstValue + 1 == currCaseConstValue || prevCaseConstValue == currCaseConstValue))
        {
            m_caseNodes->Item(currCaseIndex)->SetLowerBound(prevCaseNode->GetLowerBound());
        }
        else
        {
            if (tmpCaseNodes->Count() != 0)
            {
                int lastTmpCaseConstValue = tmpCaseNodes->Item(tmpCaseNodes->Count() - 1)->GetSrc2IntConst();
                /*To handle duplicate non empty case statements*/
                if (lastTmpCaseConstValue != prevCaseConstValue)
                {
                    tmpCaseNodes->Add(prevCaseNode);
                }
            }
            else
            {
                tmpCaseNodes->Add(prevCaseNode); //Adding for the first time in tmpCaseNodes
            }
        }

    }

    //Adds the last caseNode in the caseNodes list.
    tmpCaseNodes->Add(m_caseNodes->Item(m_caseNodes->Count() - 1));

    m_caseNodes = tmpCaseNodes;
}

///--------------------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildBinaryTraverseInstr
///
///     Build IR instructions for case statements in a binary search traversal fashion.
///     defaultLeafBranch: offset of the next instruction to be branched after
///                        the set of case instructions under investigation
///--------------------------------------------------------------------------------------

void
SwitchIRBuilder::BuildBinaryTraverseInstr(int start, int end, uint32 defaultLeafBranch)
{
    int mid;

    if (start > end)
    {
        return;
    }

    if (end - start <= CONFIG_FLAG(MaxLinearIntCaseCount) - 1) // -1 for handling zero index as the base
    {
        //if only 3 elements, then do linear search on the elements
        BuildLinearTraverseInstr(start, end, defaultLeafBranch);
        return;
    }

    mid = start + ((end - start + 1) / 2);
    CaseNode* midNode = m_caseNodes->Item(mid);
    CaseNode* startNode = m_caseNodes->Item(start);

    // if the value that we are switching on is greater than the start case value
    // then we branch right to the right half of the binary search
    IR::BranchInstr* caseInstr = startNode->GetCaseInstr();
    IR::BranchInstr* branchInstr = IR::BranchInstr::New(m_geOp, nullptr, caseInstr->GetSrc1(), midNode->GetLowerBound(), m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, startNode->GetOffset(), midNode->GetOffset(), true);

    BuildBinaryTraverseInstr(start, mid - 1, defaultLeafBranch);
    BuildBinaryTraverseInstr(mid, end, defaultLeafBranch);
}

///------------------------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildEmptyCasesInstr
///
///     Build IR instructions for Empty consecutive case statements (with only one case block).
///     defaultLeafBranch: offset of the next instruction to be branched after
///                        the set of case instructions under investigation
///
///------------------------------------------------------------------------------------------

void
SwitchIRBuilder::BuildEmptyCasesInstr(CaseNode* caseNode, uint32 fallThrOffset)
{
    IR::BranchInstr* branchInstr;
    IR::Opnd* src1Opnd;

    src1Opnd = caseNode->GetCaseInstr()->GetSrc1();

    AssertMsg(caseNode->GetLowerBound() != caseNode->GetUpperBound(), "The upper bound and lower bound should not be the same");

    //Generate <lb instruction
    branchInstr = IR::BranchInstr::New(m_ltOp, nullptr, src1Opnd, caseNode->GetLowerBound(), m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, caseNode->GetOffset(), fallThrOffset, true);

    //Generate <=ub instruction
    branchInstr = IR::BranchInstr::New(m_leOp, nullptr, src1Opnd, caseNode->GetUpperBound(), m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, caseNode->GetOffset(), caseNode->GetTargetOffset(), true);

    BuildBailOnNotInteger();
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildLinearTraverseInstr
///
///     Build IR instr for case statements less than a threshold.
///     defaultLeafBranch: offset of the next instruction to be branched after
///                        the set of case instructions under investigation
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::BuildLinearTraverseInstr(int start, int end, uint fallThrOffset)
{
    Assert(fallThrOffset);
    for (int index = start; index <= end; index++)
    {
        CaseNode* currCaseNode = m_caseNodes->Item(index);

        bool dontBuildEmptyCases = false;

        if (currCaseNode->IsSrc2IntConst())
        {
            int lowerBoundCaseConstValue = currCaseNode->GetLowerBound()->GetStackSym()->GetIntConstValue();
            int upperBoundCaseConstValue = currCaseNode->GetUpperBound()->GetStackSym()->GetIntConstValue();

            if (lowerBoundCaseConstValue == upperBoundCaseConstValue)
            {
                dontBuildEmptyCases = true;
            }
        }
        else if (currCaseNode->IsSrc2StrConst())
        {
            dontBuildEmptyCases = true;
        }
        else
        {
            AssertMsg(false, "An integer/String CaseNode is required for BuildLinearTraverseInstr");
        }

        if (dontBuildEmptyCases)
        {
            // only if the instruction is not part of a cluster of empty consecutive case statements.
            m_adapter->AddBranchInstr(currCaseNode->GetCaseInstr(), currCaseNode->GetOffset(), currCaseNode->GetTargetOffset());
        }
        else
        {
            BuildEmptyCasesInstr(currCaseNode, fallThrOffset);
        }
    }

    // Adds an unconditional branch instruction at the end

    IR::BranchInstr* branchInstr = IR::BranchInstr::New(Js::OpCode::Br, nullptr, m_func);
    branchInstr->m_isSwitchBr = true;
    m_adapter->AddBranchInstr(branchInstr, Js::Constants::NoByteCodeOffset, fallThrOffset, true);
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::ResetCaseNodes
///
///     Resets SwitchIRBuilder to begin building another switch statement.
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::ResetCaseNodes()
{
    m_caseNodes->Clear();
    m_seenOnlySingleCharStrCaseNodes = true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildCaseBrInstr
///     Generates the branch instructions to optimize the switch case execution flow
///     -Sorts, Refines and generates instructions in binary traversal fashion
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildCaseBrInstr(uint32 targetOffset)
{
    Assert(m_isAsmJs || m_profiledSwitchInstr);

    int start = 0;
    int end = m_caseNodes->Count() - 1;

    if (m_caseNodes->Count() <= CONFIG_FLAG(MaxLinearIntCaseCount))
    {
        BuildLinearTraverseInstr(start, end, targetOffset);
        ResetCaseNodes();
        return;
    }

    RefineCaseNodes();

    BuildOptimizedIntegerCaseInstrs(targetOffset);

    ResetCaseNodes(); // clear the list for the next new set of integers - or for a new switch case statement

                      //optimization is definitely performed when the number of cases is greater than the threshold
    if (end - start > CONFIG_FLAG(MaxLinearIntCaseCount) - 1) // -1 for handling zero index as the base
    {
        BuildBailOnNotInteger();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildOptimizedIntegerCaseInstrs
///     Identify chunks of integers cases(consecutive integers)
///     Apply  jump table or binary traversal based on the density of the case arms
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildOptimizedIntegerCaseInstrs(uint32 targetOffset)
{
    int startjmpTableIndex = 0;
    int endjmpTableIndex = 0;
    int startBinaryTravIndex = 0;
    int endBinaryTravIndex = 0;

    IR::MultiBranchInstr * multiBranchInstr = nullptr;

    /*
    *   Algorithm to find chunks of consecutive integers in a given set of case arms(sorted)
    *   -Start and end indices for jump table and binary tree are maintained.
    *   -The corresponding start and end indices indicate that they are suitable candidate for their respective category(binaryTree/jumpTable)
    *   -All holes are filled with an offset corresponding to the default fallthrough instruction and each block is filled with an offset corresponding to the start of the next block
    *    A Block here refers either to a jump table or to a binary tree.
    *   -Blocks of BinaryTrav/Jump table are traversed in a linear fashion.
    **/
    for (int currentIndex = 0; currentIndex < m_caseNodes->Count() - 1; currentIndex++)
    {
        int nextIndex = currentIndex + 1;
        //Check if there is no missing value between subsequent case arms
        if (m_caseNodes->Item(currentIndex)->GetSrc2IntConst() + 1 != m_caseNodes->Item(nextIndex)->GetSrc2IntConst())
        {
            //value of the case nodes are guaranteed to be 32 bits or less than 32bits at this point(if it is more, the Switch Opt will not kick in)
            Assert(nextIndex == endjmpTableIndex + 1);
            int64 speculatedEndJmpCaseValue = m_caseNodes->Item(nextIndex)->GetSrc2IntConst();
            int64 endJmpCaseValue = m_caseNodes->Item(endjmpTableIndex)->GetSrc2IntConst();
            int64 startJmpCaseValue = m_caseNodes->Item(startjmpTableIndex)->GetSrc2IntConst();

            int64 speculatedJmpTableSize = speculatedEndJmpCaseValue - startJmpCaseValue + 1;
            int64 jmpTableSize = endJmpCaseValue - startJmpCaseValue + 1;

            int numFilledEntries = nextIndex - startjmpTableIndex + 1;

            //Checks if the % of filled entries(unique targets from the case arms) in the jump table is within the threshold
            if (speculatedJmpTableSize != 0 && ((numFilledEntries)* 100 / speculatedJmpTableSize) < (100 - CONFIG_FLAG(SwitchOptHolesThreshold)))
            {
                if (jmpTableSize >= CONFIG_FLAG(MinSwitchJumpTableSize))
                {
                    uint32 fallThrOffset = m_caseNodes->Item(endjmpTableIndex)->GetOffset();
                    TryBuildBinaryTreeOrMultiBrForSwitchInts(multiBranchInstr, fallThrOffset, startjmpTableIndex, endjmpTableIndex, startBinaryTravIndex, targetOffset);

                    //Reset start/end indices of BinaryTrav to the next index.
                    startBinaryTravIndex = nextIndex;
                    endBinaryTravIndex = nextIndex;
                }

                //Reset start/end indices of the jump table to the next index.
                startjmpTableIndex = nextIndex;
                endjmpTableIndex = nextIndex;
            }
            else
            {
                endjmpTableIndex++;
            }
        }
        else
        {
            endjmpTableIndex++;
        }
    }

    int64 endJmpCaseValue = m_caseNodes->Item(endjmpTableIndex)->GetSrc2IntConst();
    int64 startJmpCaseValue = m_caseNodes->Item(startjmpTableIndex)->GetSrc2IntConst();
    int64 jmpTableSize = endJmpCaseValue - startJmpCaseValue + 1;

    if (jmpTableSize < CONFIG_FLAG(MinSwitchJumpTableSize))
    {
        endBinaryTravIndex = endjmpTableIndex;
        BuildBinaryTraverseInstr(startBinaryTravIndex, endBinaryTravIndex, targetOffset);
        if (multiBranchInstr)
        {
            FixUpMultiBrJumpTable(multiBranchInstr, multiBranchInstr->GetNextRealInstr()->GetByteCodeOffset());
            multiBranchInstr = nullptr;
        }
    }
    else
    {
        uint32 fallthrOffset = m_caseNodes->Item(endjmpTableIndex)->GetOffset();
        TryBuildBinaryTreeOrMultiBrForSwitchInts(multiBranchInstr, fallthrOffset, startjmpTableIndex, endjmpTableIndex, startBinaryTravIndex, targetOffset);
        FixUpMultiBrJumpTable(multiBranchInstr, targetOffset);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::TryBuildBinaryTreeOrMultiBrForSwitchInts
///     Builds a range of integer cases into either a binary tree or jump table.
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::TryBuildBinaryTreeOrMultiBrForSwitchInts(IR::MultiBranchInstr * &multiBranchInstr, uint32 fallthrOffset, int startjmpTableIndex, int endjmpTableIndex, int startBinaryTravIndex, uint32 defaultTargetOffset)
{
    int endBinaryTravIndex = startjmpTableIndex;

    //Try Building Binary tree, if there are available case arms, as indicated by the boundary offsets
    if (endBinaryTravIndex != startBinaryTravIndex)
    {
        endBinaryTravIndex = startjmpTableIndex - 1;
        BuildBinaryTraverseInstr(startBinaryTravIndex, endBinaryTravIndex, fallthrOffset);
        //Fix up the fallthrOffset for the previous multiBrInstr, if one existed
        //Example => Binary tree immediately succeeds a MultiBr Instr
        if (multiBranchInstr)
        {
            FixUpMultiBrJumpTable(multiBranchInstr, multiBranchInstr->GetNextRealInstr()->GetByteCodeOffset());
            multiBranchInstr = nullptr;
        }
    }

    //Fix up the fallthrOffset for the previous multiBrInstr, if one existed
    //Example -> A multiBr can be followed by another multiBr
    if (multiBranchInstr)
    {
        FixUpMultiBrJumpTable(multiBranchInstr, fallthrOffset);
        multiBranchInstr = nullptr;
    }
    multiBranchInstr = BuildMultiBrCaseInstrForInts(startjmpTableIndex, endjmpTableIndex, defaultTargetOffset);

    //We currently assign the offset of the multiBr Instr same as the offset of the last instruction of the case arm selected for building the jump table
    //AssertMsg(m_lastInstr->GetByteCodeOffset() == fallthrOffset, "The fallthrough offset to the multi branch instruction is wrong");
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::FixUpMultiBrJumpTable
///     Creates Reloc Records for the branch instructions that are generated with the MultiBr Instr
///     Also calls FixMultiBrDefaultTarget to fix the target offset in the MultiBr Instr
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::FixUpMultiBrJumpTable(IR::MultiBranchInstr * multiBranchInstr, uint32 targetOffset)
{
    multiBranchInstr->FixMultiBrDefaultTarget(targetOffset);

    uint32 offset = multiBranchInstr->GetByteCodeOffset();

    IR::Instr * subInstr = multiBranchInstr->GetPrevRealInstr();
    IR::Instr * upperBoundCheckInstr = subInstr->GetPrevRealInstr();
    IR::Instr * lowerBoundCheckInstr = upperBoundCheckInstr->GetPrevRealInstr();

    AssertMsg(subInstr->m_opcode == m_subOp, "Missing Offset Calculation instruction");
    AssertMsg(upperBoundCheckInstr->IsBranchInstr() && lowerBoundCheckInstr->IsBranchInstr(), "Invalid boundary check instructions");
    AssertMsg(upperBoundCheckInstr->m_opcode == m_gtOp && lowerBoundCheckInstr->m_opcode == m_ltOp, "Invalid boundary check instructions");

    m_adapter->CreateRelocRecord(upperBoundCheckInstr->AsBranchInstr(), offset, targetOffset, true);
    m_adapter->CreateRelocRecord(lowerBoundCheckInstr->AsBranchInstr(), offset, targetOffset, true);
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildBailOnNotInteger
///     Creates the necessary bailout for a switch case that expected an integer expression
///     but was not.
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildBailOnNotInteger()
{
    if (!m_switchOptBuildBail)
    {
        return;
    }

    m_adapter->ConvertToBailOut(m_profiledSwitchInstr, IR::BailOutExpectingInteger);
    m_switchOptBuildBail = false; // falsify this to avoid generating extra BailOuts when optimization is done again on the same switch statement

#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    PHASE_PRINT_TESTTRACE1(Js::SwitchOptPhase, L"Func %s, Switch %d:Optimized for Integers\n",
        m_profiledSwitchInstr->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
        m_profiledSwitchInstr->AsProfiledInstr()->u.profileId);
}

////////////////////////////////////////////////////////////////////////////////////////////
///
///SwitchIRBuilder::BuildBailOnNotString
///     Creates the necessary bailout for a switch case that expected an string expression
///     but was not.
///
////////////////////////////////////////////////////////////////////////////////////////////

void
SwitchIRBuilder::BuildBailOnNotString()
{
    if (!m_switchOptBuildBail)
    {
        return;
    }

    m_adapter->ConvertToBailOut(m_profiledSwitchInstr, IR::BailOutExpectingString);
    m_switchOptBuildBail = false; // falsify this to avoid generating extra BailOuts when optimization is done again on the same switch statement

#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    PHASE_PRINT_TESTTRACE1(Js::SwitchOptPhase, L"Func %s, Switch %d:Optimized for Strings\n",
        m_profiledSwitchInstr->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
        m_profiledSwitchInstr->AsProfiledInstr()->u.profileId);
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::TestAndAddStringCaseConst
///
///     Checks if strConstSwitchCases already has the string constant
///     - if yes, then return true
///     - if no, then add the string to the list 'strConstSwitchCases' and return false
///
///----------------------------------------------------------------------------

bool
SwitchIRBuilder::TestAndAddStringCaseConst(Js::JavascriptString * str)
{
    Assert(m_strConstSwitchCases);

    if (m_strConstSwitchCases->Contains(str))
    {
        return true;
    }
    else
    {
        m_strConstSwitchCases->Add(str);
        return false;
    }
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildMultiBrCaseInstrForStrings
///
///     Build Multi Branch IR instr for a set of Case statements(String case arms).
///     - Builds the multibranch target and adds the instruction
///
///----------------------------------------------------------------------------

void
SwitchIRBuilder::BuildMultiBrCaseInstrForStrings(uint32 targetOffset)
{
    Assert(m_caseNodes && m_caseNodes->Count() && m_profiledSwitchInstr && !m_isAsmJs);

    if (m_caseNodes->Count() < CONFIG_FLAG(MaxLinearStringCaseCount))
    {
        int start = 0;
        int end = m_caseNodes->Count() - 1;
        BuildLinearTraverseInstr(start, end, targetOffset);
        ResetCaseNodes();
        return;
    }

    IR::Opnd * srcOpnd = m_caseNodes->Item(0)->GetCaseInstr()->GetSrc1(); // Src1 is same in all the caseNodes
    IR::MultiBranchInstr * multiBranchInstr = IR::MultiBranchInstr::New(Js::OpCode::MultiBr, srcOpnd, m_func);

    uint32 lastCaseOffset = m_caseNodes->Item(m_caseNodes->Count() - 1)->GetOffset();
    uint caseCount = m_caseNodes->Count();

    bool generateDictionary = true;
    wchar_t minChar = USHORT_MAX;
    wchar_t maxChar = 0;

    // Either the jump table is within the limit (<= 128) or it is dense (<= 2 * case Count)
    uint const maxJumpTableSize = max<uint>(CONFIG_FLAG(MaxSingleCharStrJumpTableSize), CONFIG_FLAG(MaxSingleCharStrJumpTableRatio) * caseCount);
    if (this->m_seenOnlySingleCharStrCaseNodes)
    {
        generateDictionary = false;
        for (uint i = 0; i < caseCount; i++)
        {
            Js::JavascriptString * str = m_caseNodes->Item(i)->GetSrc2StringConst();
            Assert(str->GetLength() == 1);
            wchar_t currChar = str->GetString()[0];
            minChar = min(minChar, currChar);
            maxChar = max(maxChar, currChar);
            if ((uint)(maxChar - minChar) > maxJumpTableSize)
            {
                generateDictionary = true;
                break;
            }
        }
    }


    if (generateDictionary)
    {
        multiBranchInstr->CreateBranchTargetsAndSetDefaultTarget(caseCount, IR::MultiBranchInstr::StrDictionary, targetOffset);

        //Adding normal cases to the instruction (except the default case, which we do it later)
        for (uint i = 0; i < caseCount; i++)
        {
            Js::JavascriptString * str = m_caseNodes->Item(i)->GetSrc2StringConst();
            uint32 caseTargetOffset = m_caseNodes->Item(i)->GetTargetOffset();
            multiBranchInstr->AddtoDictionary(caseTargetOffset, str);
        }
    }
    else
    {
        // If we are only going to save 16 entries, just start from 0 so we don't have to subtract
        if (minChar < 16)
        {
            minChar = 0;
        }
        multiBranchInstr->m_baseCaseValue = minChar;
        multiBranchInstr->m_lastCaseValue = maxChar;
        uint jumpTableSize = maxChar - minChar + 1;
        multiBranchInstr->CreateBranchTargetsAndSetDefaultTarget(jumpTableSize, IR::MultiBranchInstr::SingleCharStrJumpTable, targetOffset);

        for (uint i = 0; i < jumpTableSize; i++)
        {
            // Initialize all the entries to the default target first.
            multiBranchInstr->AddtoJumpTable(targetOffset, i);
        }
        //Adding normal cases to the instruction (except the default case, which we do it later)
        for (uint i = 0; i < caseCount; i++)
        {
            Js::JavascriptString * str = m_caseNodes->Item(i)->GetSrc2StringConst();
            Assert(str->GetLength() == 1);
            uint32 caseTargetOffset = m_caseNodes->Item(i)->GetTargetOffset();
            multiBranchInstr->AddtoJumpTable(caseTargetOffset, str->GetString()[0] - minChar);
        }
    }

    multiBranchInstr->m_isSwitchBr = true;

    m_adapter->CreateRelocRecord(multiBranchInstr, lastCaseOffset, targetOffset);
    m_adapter->AddInstr(multiBranchInstr, lastCaseOffset);
    BuildBailOnNotString();

    ResetCaseNodes();
}

///----------------------------------------------------------------------------
///
/// SwitchIRBuilder::BuildMultiBrCaseInstrForInts
///
///     Build Multi Branch IR instr for a set of Case statements(Integer case arms).
///     - Builds the multibranch target and adds the instruction
///     - Add boundary checks for the jump table and calculates the offset in the jump table
///
///----------------------------------------------------------------------------

IR::MultiBranchInstr *
SwitchIRBuilder::BuildMultiBrCaseInstrForInts(uint32 start, uint32 end, uint32 targetOffset)
{
    Assert(m_caseNodes && m_caseNodes->Count() && (m_profiledSwitchInstr || m_isAsmJs));

    IR::Opnd * srcOpnd = m_caseNodes->Item(start)->GetCaseInstr()->GetSrc1(); // Src1 is same in all the caseNodes
    IR::MultiBranchInstr * multiBranchInstr = IR::MultiBranchInstr::New(Js::OpCode::MultiBr, srcOpnd, m_func);

    uint32 lastCaseOffset = m_caseNodes->Item(end)->GetOffset();

    int32 baseCaseValue = m_caseNodes->Item(start)->GetLowerBound()->GetStackSym()->GetIntConstValue();
    int32 lastCaseValue = m_caseNodes->Item(end)->GetUpperBound()->GetStackSym()->GetIntConstValue();

    multiBranchInstr->m_baseCaseValue = baseCaseValue;
    multiBranchInstr->m_lastCaseValue = lastCaseValue;

    uint32 jmpTableSize = lastCaseValue - baseCaseValue + 1;
    multiBranchInstr->CreateBranchTargetsAndSetDefaultTarget(jmpTableSize, IR::MultiBranchInstr::IntJumpTable, targetOffset);

    int caseIndex = end;
    int lowerBoundCaseConstValue = 0;
    int upperBoundCaseConstValue = 0;
    uint32 caseTargetOffset = 0;

    for (int jmpIndex = jmpTableSize - 1; jmpIndex >= 0; jmpIndex--)
    {
        if (caseIndex >= 0 && jmpIndex == m_caseNodes->Item(caseIndex)->GetSrc2IntConst() - baseCaseValue)
        {
            lowerBoundCaseConstValue = m_caseNodes->Item(caseIndex)->GetLowerBound()->GetStackSym()->GetIntConstValue();
            upperBoundCaseConstValue = m_caseNodes->Item(caseIndex)->GetUpperBound()->GetStackSym()->GetIntConstValue();
            caseTargetOffset = m_caseNodes->Item(caseIndex--)->GetTargetOffset();
            multiBranchInstr->AddtoJumpTable(caseTargetOffset, jmpIndex);
        }
        else
        {
            if (jmpIndex >= lowerBoundCaseConstValue - baseCaseValue && jmpIndex <= upperBoundCaseConstValue - baseCaseValue)
            {
                multiBranchInstr->AddtoJumpTable(caseTargetOffset, jmpIndex);
            }
            else
            {
                multiBranchInstr->AddtoJumpTable(targetOffset, jmpIndex);
            }
        }
    }

    //Insert Boundary checks for the jump table - Reloc records are created later for these instructions (in FixUpMultiBrJumpTable())
    IR::BranchInstr* lowerBoundChk = IR::BranchInstr::New(m_ltOp, nullptr, srcOpnd, m_caseNodes->Item(start)->GetLowerBound(), m_func);
    lowerBoundChk->m_isSwitchBr = true;
    m_adapter->AddInstr(lowerBoundChk, lastCaseOffset);

    IR::BranchInstr* upperBoundChk = IR::BranchInstr::New(m_gtOp, nullptr, srcOpnd, m_caseNodes->Item(end)->GetUpperBound(), m_func);
    upperBoundChk->m_isSwitchBr = true;
    m_adapter->AddInstr(upperBoundChk, lastCaseOffset);

    //Calculate the offset inside the jump table using the switch operand value and the lowest case arm value (in the considered set of consecutive integers)
    IR::IntConstOpnd *baseCaseValueOpnd = IR::IntConstOpnd::New(multiBranchInstr->m_baseCaseValue, TyInt32, m_func);

    IR::RegOpnd * offset = IR::RegOpnd::New(TyVar, m_func);

    IR::Instr * subInstr = IR::Instr::New(m_subOp, offset, multiBranchInstr->GetSrc1(), baseCaseValueOpnd, m_func);

    //We are sure that the SUB operation will not overflow the int range - It will either bailout or will not optimize if it finds a number that is out of the int range.
    subInstr->ignoreIntOverflow = true;

    m_adapter->AddInstr(subInstr, lastCaseOffset);

    //Source of the multi branch instr will now have the calculated offset
    multiBranchInstr->UnlinkSrc1();
    multiBranchInstr->SetSrc1(offset);
    multiBranchInstr->m_isSwitchBr = true;

    m_adapter->AddInstr(multiBranchInstr, lastCaseOffset);
    m_adapter->CreateRelocRecord(multiBranchInstr, lastCaseOffset, targetOffset);

    return multiBranchInstr;
}
