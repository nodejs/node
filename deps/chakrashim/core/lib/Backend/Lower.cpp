//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "Debug\DebuggingFlags.h"
#include "Debug\DiagProbe.h"
#include "Debug\DebugManager.h"

// Parser includes
#include "RegexCommon.h"
#include "RegexPattern.h"

#include "ExternalLowerer.h"

///----------------------------------------------------------------------------
///
/// Lowerer::Lower
///
///     Lowerer's main entrypoint.  Lowers this function..
///
///----------------------------------------------------------------------------
void
Lowerer::Lower()
{

    this->m_func->StopMaintainByteCodeOffset();

    NoRecoverMemoryJitArenaAllocator localAlloc(L"BE-Lower", this->m_func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);
    this->m_alloc = &localAlloc;
    BVSparse<JitArenaAllocator> localInitializedTempSym(&localAlloc);
    this->initializedTempSym = &localInitializedTempSym;
    BVSparse<JitArenaAllocator> localAddToLiveOnBackEdgeSyms(&localAlloc);
    this->addToLiveOnBackEdgeSyms = &localAddToLiveOnBackEdgeSyms;
    Assert(this->m_func->GetCloneMap() == nullptr);

    m_lowererMD.Init(this);

    bool defaultDoFastPath = this->m_func->DoFastPaths();
    bool loopFastPath = this->m_func->DoLoopFastPaths();

    if (!loopFastPath || !defaultDoFastPath
#ifdef INLINE_CACHE_STATS
        || PHASE_STATS1(Js::PolymorphicInlineCachePhase)
#endif
        )
    {
        //arguments[] access is similar to array fast path hence disable when array fastpath is disabled.
        //loopFastPath is always true except explicitly disabled
        //defaultDoFastPath can be false when we the source code size is huge
        m_func->SetHasStackArgs(false);
    }

    if (m_func->HasAnyStackNestedFunc())
    {
        EnsureStackFunctionListStackSym();
    }
    if (m_func->DoStackFrameDisplay() && !m_func->IsLoopBody())
    {
        AllocStackClosure();
    }

    if (m_func->IsJitInDebugMode())
    {
        // Initialize metadata of local var slots.
        // Too late to wait until Register Allocator, as we need the offset when lowerering bailout for debugger.
        int32 hasLocalVarChangedOffset = m_func->GetHasLocalVarChangedOffset();
        if (hasLocalVarChangedOffset != Js::Constants::InvalidOffset)
        {
            // MOV [EBP + m_func->GetHasLocalVarChangedOffset()], 0
            StackSym* sym = StackSym::New(TyInt8, m_func);
            sym->m_offset = hasLocalVarChangedOffset;
            sym->m_allocated = true;
            IR::Opnd* opnd1 = IR::SymOpnd::New(sym, TyInt8, m_func);
            IR::Opnd* opnd2 = IR::IntConstOpnd::New(0, TyInt8, m_func);
            LowererMD::CreateAssign(opnd1, opnd2, m_func->GetFunctionEntryInsertionPoint());

#ifdef DBG
            // Pre-fill all local slots with a pattern. This will help identify non-initialized/garbage var values.
            // Note that in the beginning of the function in bytecode we should initialize all locals to undefined.
            uint32 localSlotCount = m_func->GetJnFunction()->GetEndNonTempLocalIndex() - m_func->GetJnFunction()->GetFirstNonTempLocalIndex();
            for (uint i = 0; i < localSlotCount; ++i)
            {
                int offset = m_func->GetLocalVarSlotOffset(i);

                IRType opnd1Type;
                opnd2;
                uint32 slotSize = Func::GetDiagLocalSlotSize();
                switch (slotSize)
                {
                case 4:
                    opnd1Type = TyInt32;
                    opnd2 = IR::IntConstOpnd::New(Func::c_debugFillPattern4, opnd1Type, m_func);
                    break;
                case 8:
                    opnd1Type = TyInt64;
                    opnd2 = IR::AddrOpnd::New((Js::Var)Func::c_debugFillPattern8, IR::AddrOpndKindConstant, m_func);
                    break;
                default:
                    AssertMsg(FALSE, "Unsupported slot size!");
                    opnd1Type = TyIllegal;
                    opnd2 = nullptr;
                }

                sym = StackSym::New(opnd1Type, m_func);
                sym->m_offset = offset;
                sym->m_allocated = true;
                opnd1 = IR::SymOpnd::New(sym, TyInt32, m_func);
                LowererMD::CreateAssign(opnd1, opnd2, m_func->GetFunctionEntryInsertionPoint());
            }
#endif
        }

        Assert(!m_func->HasAnyStackNestedFunc());
    }

    this->LowerRange(m_func->m_headInstr, m_func->m_tailInstr, defaultDoFastPath, loopFastPath);
    this->m_func->ClearCloneMap();

    if (m_func->HasAnyStackNestedFunc())
    {
        EnsureZeroLastStackFunctionNext();
    }

    if (!m_func->IsSimpleJit())
    {
        Js::EntryPointInfo* entryPointInfo = this->m_func->m_workItem->GetEntryPoint();
        Assert(entryPointInfo->GetJitTransferData() != nullptr && !entryPointInfo->GetJitTransferData()->GetIsReady());
    }

    this->initializedTempSym = nullptr;
    this->m_alloc = nullptr;

    this->m_func->DisableConstandAddressLoadHoist();
}

void
Lowerer::LowerRange(IR::Instr *instrStart, IR::Instr *instrEnd, bool defaultDoFastPath, bool defaultDoLoopFastPath)
{
    bool noMathFastPath;
    bool noFieldFastPath;
    bool fNoLower = false;
    noFieldFastPath = !defaultDoFastPath;
    noMathFastPath = !defaultDoFastPath;

#if DBG_DUMP
    wchar_t * globOptInstrString = nullptr;
#endif

    FOREACH_INSTR_BACKWARD_EDITING_IN_RANGE(instr, instrPrev, instrEnd, instrStart)
    {
        // Try to peep this`
        instr = this->PreLowerPeepInstr(instr, &instrPrev);

#if DBG
        IR::Instr * verifyLegalizeInstrNext = instr->m_next;
#endif

        // If we have debugger bailout as part of real instr (not separate BailForDebugger instr),
        // extract/split out BailOutForDebugger into separate instr, if needed.
        // The instr can have just debugger bailout, or debugger bailout + other shared bailout.
        // Note that by the time we get here, we should not have aux-only bailout (in globopt we promote it to normal bailout).
        if (m_func->IsJitInDebugMode() && instr->HasBailOutInfo() &&
            ((instr->GetBailOutKind() & IR::BailOutForDebuggerBits) && instr->m_opcode != Js::OpCode::BailForDebugger ||
            instr->HasAuxBailOut()))
        {
            instr = this->SplitBailForDebugger(instr);  // Change instr, as returned is the one we need to lower next.
            instrPrev = instr->m_prev;                  // Change just in case if instr got changed.
        }

#if DBG_DUMP
        if (!instr->IsLowered() && !instr->IsLabelInstr()
            && (CONFIG_FLAG(ForcePostLowerGlobOptInstrString) ||
            PHASE_DUMP(Js::LowererPhase, m_func) ||
            PHASE_DUMP(Js::LinearScanPhase, m_func) ||
            PHASE_DUMP(Js::RegAllocPhase, m_func) ||
            PHASE_DUMP(Js::PeepsPhase, m_func) ||
            PHASE_DUMP(Js::LayoutPhase, m_func) ||
            PHASE_DUMP(Js::EmitterPhase, m_func) ||
            PHASE_DUMP(Js::EncoderPhase, m_func) ||
            PHASE_DUMP(Js::BackEndPhase, m_func)))
        {
            if(instr->m_next && instr->m_next->m_opcode != Js::OpCode::StatementBoundary && !instr->m_next->IsLabelInstr())
            {
                instr->m_next->globOptInstrString = globOptInstrString;
            }

            globOptInstrString = instr->DumpString();
        }
#endif
        IR::Opnd *src1;
        IR::RegOpnd *srcReg1;
        IR::RegOpnd *srcReg2;

        if (instr->IsBranchInstr() && !instr->AsBranchInstr()->IsMultiBranch() && instr->AsBranchInstr()->GetTarget()->m_isLoopTop)
        {
            Loop * loop = instr->AsBranchInstr()->GetTarget()->GetLoop();
            if (this->outerMostLoopLabel == nullptr && !loop->isProcessed)
            {
               while (loop && loop->GetLoopTopInstr()) // some loops are optimized away so that they are not loops anymore.
                                                        // They do, however, stay in the loop graph but don't have loop top labels assigned to them
                {
                    this->outerMostLoopLabel = loop->GetLoopTopInstr();
                    Assert(this->outerMostLoopLabel->m_isLoopTop);
                    // landing pad must fall through to the loop
                    Assert(this->outerMostLoopLabel->m_prev->HasFallThrough());
                    loop = loop->parent;
                }
                this->initializedTempSym->ClearAll();
            }

            noFieldFastPath = !defaultDoLoopFastPath;
            noMathFastPath  = !defaultDoLoopFastPath;
        }

#ifdef INLINE_CACHE_STATS
        if(PHASE_STATS1(Js::PolymorphicInlineCachePhase))
        {
            // Always use the slow path, so we can track property accesses
            noFieldFastPath = true;
        }
#endif

        switch(instr->m_opcode)
        {
        case Js::OpCode::LdHandlerScope:
            this->LowerUnaryHelperMem(instr, IR::HelperScrObj_LdHandlerScope);
            break;

        case Js::OpCode::InitSetFld:
            instrPrev = this->LowerStFld(instr, IR::HelperOP_InitSetter, IR::HelperOP_InitSetter, false);
            break;

        case Js::OpCode::InitGetFld:
            instrPrev = this->LowerStFld(instr, IR::HelperOP_InitGetter, IR::HelperOP_InitGetter, false);
            break;

        case Js::OpCode::InitProto:
            instrPrev = this->LowerStFld(instr, IR::HelperOP_InitProto, IR::HelperOP_InitProto, false);
            break;

        case Js::OpCode::LdArgCnt:
            this->LoadArgumentCount(instr);
            break;

        case Js::OpCode::LdStackArgPtr:
            this->LoadStackArgPtr(instr);
            break;

        case Js::OpCode::LdHeapArguments:
        case Js::OpCode::LdLetHeapArguments:
            instrPrev = m_lowererMD.LoadHeapArguments(instr);
            break;

        case Js::OpCode::LdArgumentsFromStack:
            instrPrev =  this->LoadArgumentsFromStack(instr);
            break;

        case Js::OpCode::LdHeapArgsCached:
        case Js::OpCode::LdLetHeapArgsCached:
            m_lowererMD.LoadHeapArgsCached(instr);
            break;

        case Js::OpCode::InvalCachedScope:
            this->LowerBinaryHelper(instr, IR::HelperOP_InvalidateCachedScope);
            break;

        case Js::OpCode::NewScopeObject:
            m_lowererMD.ChangeToHelperCallMem(instr, IR::HelperOP_NewScopeObject);
            break;

        case Js::OpCode::NewStackScopeSlots:
            this->LowerNewScopeSlots(instr, m_func->DoStackScopeSlots());
            break;

        case Js::OpCode::NewScopeSlots:
            this->LowerNewScopeSlots(instr, false);
            break;

        case Js::OpCode::InitLocalClosure:
            // Real initialization of the stack pointers happens on entry to the function, so this instruction
            // (which exists to provide a def in the IR) can go away.
            instr->Remove();
            break;

        case Js::OpCode::NewScopeSlotsWithoutPropIds:
            this->LowerBinaryHelperMemWithFuncBody(instr, IR::HelperOP_NewScopeSlotsWithoutPropIds);
            break;

        case Js::OpCode::NewBlockScope:
            m_lowererMD.ChangeToHelperCallMem(instr, IR::HelperOP_NewBlockScope);
            break;

        case Js::OpCode::NewPseudoScope:
            m_lowererMD.ChangeToHelperCallMem(instr, IR::HelperOP_NewPseudoScope);
            break;

        case Js::OpCode::CloneInnerScopeSlots:
            this->LowerUnaryHelperMem(instr, IR::HelperOP_CloneInnerScopeSlots);
            break;

        case Js::OpCode::CloneBlockScope:
            this->LowerUnaryHelperMem(instr, IR::HelperOP_CloneBlockScope);
            break;

        case Js::OpCode::GetCachedFunc:
            m_lowererMD.LowerGetCachedFunc(instr);
            break;

        case Js::OpCode::BrFncCachedScopeEq:
        case Js::OpCode::BrFncCachedScopeNeq:
            this->LowerBrFncCachedScopeEq(instr);
            break;

        case Js::OpCode::CommitScope:
            m_lowererMD.LowerCommitScope(instr);
            break;

        case Js::OpCode::LdFldForTypeOf:
            instrPrev = GenerateCompleteLdFld<false>(instr, !noFieldFastPath, IR::HelperOp_PatchGetValueForTypeOf, IR::HelperOp_PatchGetValuePolymorphicForTypeOf,
                IR::HelperOp_PatchGetValueForTypeOf, IR::HelperOp_PatchGetValuePolymorphicForTypeOf);
            break;

        case Js::OpCode::LdFld:
        case Js::OpCode::LdFldForCallApplyTarget:
            instrPrev = GenerateCompleteLdFld<false>(instr, !noFieldFastPath, IR::HelperOp_PatchGetValue, IR::HelperOp_PatchGetValuePolymorphic,
                IR::HelperOp_PatchGetValue, IR::HelperOp_PatchGetValuePolymorphic);
            break;

        case Js::OpCode::LdSuperFld:
            instrPrev = GenerateCompleteLdFld<false>(instr, !noFieldFastPath, IR::HelperOp_PatchGetValueWithThisPtr, IR::HelperOp_PatchGetValuePolymorphicWithThisPtr,
                IR::HelperOp_PatchGetValueWithThisPtr, IR::HelperOp_PatchGetValuePolymorphicWithThisPtr);
            break;

        case Js::OpCode::LdRootFld:
            instrPrev = GenerateCompleteLdFld<true>(instr, !noFieldFastPath, IR::HelperOp_PatchGetRootValue, IR::HelperOp_PatchGetRootValuePolymorphic,
                IR::HelperOp_PatchGetRootValue, IR::HelperOp_PatchGetRootValuePolymorphic);
            break;

        case Js::OpCode::LdRootFldForTypeOf:
            instrPrev = GenerateCompleteLdFld<true>(instr, !noFieldFastPath, IR::HelperOp_PatchGetRootValueForTypeOf, IR::HelperOp_PatchGetRootValuePolymorphicForTypeOf,
                IR::HelperOp_PatchGetRootValueForTypeOf, IR::HelperOp_PatchGetRootValuePolymorphicForTypeOf);
            break;

        case Js::OpCode::LdMethodFldPolyInlineMiss:
            instrPrev = LowerLdFld(instr, IR::HelperOp_PatchGetMethod, IR::HelperOp_PatchGetMethodPolymorphic, true, nullptr, true);
            break;

        case Js::OpCode::LdMethodFld:
            instrPrev = GenerateCompleteLdFld<false>(instr, !noFieldFastPath, IR::HelperOp_PatchGetMethod, IR::HelperOp_PatchGetMethodPolymorphic,
                IR::HelperOp_PatchGetMethod, IR::HelperOp_PatchGetMethodPolymorphic);
            break;

        case Js::OpCode::LdRootMethodFld:
            instrPrev = GenerateCompleteLdFld<true>(instr, !noFieldFastPath, IR::HelperOp_PatchGetRootMethod, IR::HelperOp_PatchGetRootMethodPolymorphic,
                IR::HelperOp_PatchGetRootMethod, IR::HelperOp_PatchGetRootMethodPolymorphic);
            break;

        case Js::OpCode::ScopedLdMethodFld:
            // "Scoped" in ScopedLdMethodFld is a bit of a misnomer because it doesn't look through a scope chain.
            // Instead the op is to allow for either a LdRootMethodFld or LdMethodFld depending on whether the
            // object is the root object or not.
            instrPrev = GenerateCompleteLdFld<false>(instr, !noFieldFastPath, IR::HelperOp_ScopedGetMethod, IR::HelperOp_ScopedGetMethodPolymorphic,
                IR::HelperOp_ScopedGetMethod, IR::HelperOp_ScopedGetMethodPolymorphic);
            break;

        case Js::OpCode::LdMethodFromFlags:
            {
                Assert(instr->HasBailOutInfo());
                bool success = m_lowererMD.GenerateFastLdMethodFromFlags(instr);
                AssertMsg(success, "Not expected to generate helper block here");
                break;
            }

        case Js::OpCode::CheckFixedFld:
            AssertMsg(!PHASE_OFF(Js::FixedMethodsPhase, instr->m_func->GetJnFunction()) || !PHASE_OFF(Js::UseFixedDataPropsPhase, instr->m_func->GetJnFunction()), "CheckFixedFld with fixed prop(Data|Method) phase disabled?");
            this->GenerateCheckFixedFld(instr);
            break;

        case Js::OpCode::CheckPropertyGuardAndLoadType:
            instrPrev = this->GeneratePropertyGuardCheckBailoutAndLoadType(instr);
            break;

        case Js::OpCode::CheckObjType:
            this->GenerateCheckObjType(instr);
            break;

        case Js::OpCode::AdjustObjType:
            this->LowerAdjustObjType(instr);
            break;

        case Js::OpCode::DeleteFld:
            instrPrev = this->LowerDelFld(instr, IR::HelperOp_DeleteProperty, false, false);
            break;

        case Js::OpCode::DeleteRootFld:
            instrPrev = this->LowerDelFld(instr, IR::HelperOp_DeleteRootProperty, false, false);
            break;

        case Js::OpCode::DeleteFldStrict:
            instrPrev = this->LowerDelFld(instr, IR::HelperOp_DeleteProperty, false, true);
            break;

        case Js::OpCode::DeleteRootFldStrict:
            instrPrev = this->LowerDelFld(instr, IR::HelperOp_DeleteRootProperty, false, true);
            break;

        case Js::OpCode::ScopedLdFldForTypeOf:
            if (!noFieldFastPath)
            {
                m_lowererMD.GenerateFastScopedLdFld(instr);
            }
            instrPrev = this->LowerScopedLdFld(instr, IR::HelperOp_PatchGetPropertyForTypeOfScoped, true);
            break;

        case Js::OpCode::ScopedLdFld:
            if (!noFieldFastPath)
            {
                m_lowererMD.GenerateFastScopedLdFld(instr);
            }
            instrPrev = this->LowerScopedLdFld(instr, IR::HelperOp_PatchGetPropertyScoped, true);
            break;

        case Js::OpCode::ScopedLdInst:
            instrPrev = this->LowerScopedLdInst(instr, IR::HelperOp_GetInstanceScoped);
            break;

        case Js::OpCode::ScopedDeleteFld:
            instrPrev = this->LowerScopedDelFld(instr, IR::HelperOp_DeletePropertyScoped, false, false);
            break;

        case Js::OpCode::ScopedDeleteFldStrict:
            instrPrev = this->LowerScopedDelFld(instr, IR::HelperOp_DeletePropertyScoped, false, true);
            break;

        case Js::OpCode::NewScFunc:
            instrPrev = this->LowerNewScFunc(instr);
            break;

        case Js::OpCode::NewScGenFunc:
            instrPrev = this->LowerNewScGenFunc(instr);
            break;

        case Js::OpCode::StFld:
            instrPrev = GenerateCompleteStFld(instr, !noFieldFastPath, IR::HelperOp_PatchPutValueNoLocalFastPath, IR::HelperOp_PatchPutValueNoLocalFastPathPolymorphic,
                IR::HelperOp_PatchPutValue, IR::HelperOp_PatchPutValuePolymorphic, true, Js::PropertyOperation_None);
            break;

        case Js::OpCode::StSuperFld:
            instrPrev = GenerateCompleteStFld(instr, !noFieldFastPath, IR::HelperOp_PatchPutValueWithThisPtrNoLocalFastPath, IR::HelperOp_PatchPutValueWithThisPtrNoLocalFastPathPolymorphic,
                IR::HelperOp_PatchPutValueWithThisPtr, IR::HelperOp_PatchPutValueWithThisPtrPolymorphic, true, Js::PropertyOperation_None);
            break;

        case Js::OpCode::StRootFld:
            instrPrev = GenerateCompleteStFld(instr, !noFieldFastPath, IR::HelperOp_PatchPutRootValueNoLocalFastPath, IR::HelperOp_PatchPutRootValueNoLocalFastPathPolymorphic,
                IR::HelperOp_PatchPutRootValue, IR::HelperOp_PatchPutRootValuePolymorphic, true, Js::PropertyOperation_Root);
            break;

        case Js::OpCode::StFldStrict:
            instrPrev = GenerateCompleteStFld(instr, !noFieldFastPath, IR::HelperOp_PatchPutValueNoLocalFastPath, IR::HelperOp_PatchPutValueNoLocalFastPathPolymorphic,
                IR::HelperOp_PatchPutValue, IR::HelperOp_PatchPutValuePolymorphic, true, Js::PropertyOperation_StrictMode);
            break;

        case Js::OpCode::StRootFldStrict:
            instrPrev = GenerateCompleteStFld(instr, !noFieldFastPath, IR::HelperOp_PatchPutRootValueNoLocalFastPath, IR::HelperOp_PatchPutRootValueNoLocalFastPathPolymorphic,
                IR::HelperOp_PatchPutRootValue, IR::HelperOp_PatchPutRootValuePolymorphic, true, Js::PropertyOperation_StrictModeRoot);
            break;

        case Js::OpCode::InitFld:
        case Js::OpCode::InitRootFld:
            instrPrev = GenerateCompleteStFld(instr, !noFieldFastPath, IR::HelperOp_PatchInitValue, IR::HelperOp_PatchInitValuePolymorphic,
                IR::HelperOp_PatchInitValue, IR::HelperOp_PatchInitValuePolymorphic, false, Js::PropertyOperation_None);
            break;

        case Js::OpCode::ScopedInitFunc:
            instrPrev = this->LowerScopedStFld(instr, IR::HelperOp_InitFuncScoped, false);
            break;

        case Js::OpCode::ScopedStFld:
        case Js::OpCode::ScopedStFldStrict:
            if (!noFieldFastPath)
            {
                m_lowererMD.GenerateFastScopedStFld(instr);
            }
            instrPrev = this->LowerScopedStFld(instr, IR::HelperOp_PatchSetPropertyScoped, true, true,
                instr->m_opcode == Js::OpCode::ScopedStFld ? Js::PropertyOperation_None : Js::PropertyOperation_StrictMode);
            break;

        case Js::OpCode::ConsoleScopedStFld:
        {
            if (!noFieldFastPath)
            {
                m_lowererMD.GenerateFastScopedStFld(instr);
            }
            Js::PropertyOperationFlags flags = static_cast<Js::PropertyOperationFlags>(Js::PropertyOperation_None | Js::PropertyOperation_AllowUndeclInConsoleScope);
            instrPrev = this->LowerScopedStFld(instr, IR::HelperOp_ConsolePatchSetPropertyScoped, true, true, flags);
            break;
        }

        case Js::OpCode::LdStr:
            m_lowererMD.ChangeToAssign(instr);
            break;

        case Js::OpCode::CloneStr:
        {
            GenerateGetImmutableOrScriptUnreferencedString(instr->GetSrc1()->AsRegOpnd(), instr, IR::HelperOp_CompoundStringCloneForAppending, false);
            instr->Remove();
            break;
        }

        case Js::OpCode::NewScObjArray:
            instrPrev = this->LowerNewScObjArray(instr);
            break;

        case Js::OpCode::NewScObject:
        case Js::OpCode::NewScObjectSpread:
        case Js::OpCode::NewScObjArraySpread:
            instrPrev = this->LowerNewScObject(instr, true, true);
            break;

        case Js::OpCode::NewScObjectNoCtor:
            instrPrev = this->LowerNewScObject(instr, false, true);
            break;

        case Js::OpCode::NewScObjectNoCtorFull:
            instrPrev = this->LowerNewScObject(instr, false, true, true);
            break;

        case Js::OpCode::GetNewScObject:
            instrPrev = this->LowerGetNewScObject(instr);
            break;

        case Js::OpCode::UpdateNewScObjectCache:
            instrPrev = instr->m_prev;
            this->LowerUpdateNewScObjectCache(instr, instr->GetSrc2(), instr->GetSrc1(), true /* isCtorFunction */);
            instr->Remove();
            break;

        case Js::OpCode::NewScObjectSimple:
            this->LowerNewScObjectSimple(instr);
            break;

        case Js::OpCode::NewScObjectLiteral:
            this->LowerNewScObjectLiteral(instr);
            break;

        case Js::OpCode::LdPropIds:
            m_lowererMD.ChangeToAssign(instr);
            break;

        case Js::OpCode::StArrSegItem_A:
            instrPrev = this->LowerArraySegmentVars(instr);
            break;

        case Js::OpCode::InlineMathAcos:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Acos);
            break;

        case Js::OpCode::InlineMathAsin:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Asin);
            break;

        case Js::OpCode::InlineMathAtan:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Atan);
            break;

        case Js::OpCode::InlineMathAtan2:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Atan2);
            break;

        case Js::OpCode::InlineMathCos:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Cos);
            break;

        case Js::OpCode::InlineMathExp:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Exp);
            break;

        case Js::OpCode::InlineMathLog:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Log);
            break;

        case Js::OpCode::InlineMathPow:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Pow);
            break;

        case Js::OpCode::InlineMathSin:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Sin);
            break;

        case Js::OpCode::InlineMathSqrt:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, (IR::JnHelperMethod)0);
            break;

        case Js::OpCode::InlineMathTan:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Tan);
            break;

        case Js::OpCode::InlineMathFloor:
#if _M_X64
            if (!AutoSystemInfo::Data.SSE4_1Available() && instr->m_func->GetJnFunction()->GetIsAsmjsMode())
            {
                m_lowererMD.HelperCallForAsmMathBuiltin(instr, IR::HelperDirectMath_FloorFlt, IR::HelperDirectMath_FloorDb);
                break;
            }
#endif
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, (IR::JnHelperMethod)0);
            break;

        case Js::OpCode::InlineMathCeil:
#if _M_X64
            if (!AutoSystemInfo::Data.SSE4_1Available() && instr->m_func->GetJnFunction()->GetIsAsmjsMode())
            {
                m_lowererMD.HelperCallForAsmMathBuiltin(instr, IR::HelperDirectMath_CeilFlt, IR::HelperDirectMath_CeilDb);
                break;
            }
#endif
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, (IR::JnHelperMethod)0);
            break;

        case Js::OpCode::InlineMathRound:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, (IR::JnHelperMethod)0);
            break;

        case Js::OpCode::InlineMathAbs:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, (IR::JnHelperMethod)0);
            break;

        case Js::OpCode::InlineMathImul:
            GenerateFastInlineMathImul(instr);
            break;

        case Js::OpCode::InlineMathClz32:
            GenerateFastInlineMathClz32(instr);
            break;

        case Js::OpCode::InlineMathFround:
            GenerateFastInlineMathFround(instr);
            break;

        case Js::OpCode::InlineMathMin:
        case Js::OpCode::InlineMathMax:
            m_lowererMD.GenerateFastInlineBuiltInCall(instr, (IR::JnHelperMethod)0);
            break;

        case Js::OpCode::InlineMathRandom:
            this->GenerateFastInlineBuiltInMathRandom(instr);
            break;

#ifdef ENABLE_DOM_FAST_PATH
        case Js::OpCode::DOMFastPathGetter:
            this->LowerFastInlineDOMFastPathGetter(instr);
            break;
#endif

        case Js::OpCode::InlineArrayPush:
            this->GenerateFastInlineArrayPush(instr);
            break;

        case Js::OpCode::InlineArrayPop:
            this->GenerateFastInlineArrayPop(instr);
            break;

        //Now retrieve the function object from the ArgOut_A_InlineSpecialized instruction opcode to push it on the stack after all the other arguments have been pushed.
        //The lowering of the direct call to helper is handled by GenerateDirectCall (architecture specific).
        case Js::OpCode::CallDirect:
        {
            IR::Opnd * src1 = instr->GetSrc1();
            Assert(src1->IsHelperCallOpnd());
            switch (src1->AsHelperCallOpnd()->m_fnHelper)
            {
                case IR::JnHelperMethod::HelperString_Split:
                case IR::JnHelperMethod::HelperString_Match:
                    GenerateFastInlineStringSplitMatch(instr);
                    break;
                case IR::JnHelperMethod::HelperRegExp_Exec:
                    GenerateFastInlineRegExpExec(instr);
                    break;
                case IR::JnHelperMethod::HelperGlobalObject_ParseInt:
                    GenerateFastInlineGlobalObjectParseInt(instr);
                    break;
                case IR::JnHelperMethod::HelperString_FromCharCode:
                    GenerateFastInlineStringFromCharCode(instr);
                    break;
                case IR::JnHelperMethod::HelperString_FromCodePoint:
                    GenerateFastInlineStringFromCodePoint(instr);
                    break;
                case IR::JnHelperMethod::HelperString_CharAt:
                    GenerateFastInlineStringCharCodeAt(instr, Js::BuiltinFunction::String_CharAt);
                    break;
                case IR::JnHelperMethod::HelperString_CharCodeAt:
                    GenerateFastInlineStringCharCodeAt(instr, Js::BuiltinFunction::String_CharCodeAt);
                    break;
                case IR::JnHelperMethod::HelperString_Replace:
                    GenerateFastInlineStringReplace(instr);
                    break;
            }
            instrPrev = LowerCallDirect(instr);
            break;
        }

        case Js::OpCode::CallIDynamic:
        {
            Js::CallFlags flags = instr->GetDst() ? Js::CallFlags_Value : Js::CallFlags_NotUsed;
            instrPrev = this->LowerCallIDynamic(instr, (ushort)flags);
            break;
        }
        case Js::OpCode::CallIDynamicSpread:
        {
            Js::CallFlags flags = instr->GetDst() ? Js::CallFlags_Value : Js::CallFlags_NotUsed;
            instrPrev = this->LowerCallIDynamicSpread(instr, (ushort)flags);
            break;
        }

        case Js::OpCode::CallI:
        case Js::OpCode::CallINew:
        case Js::OpCode::CallIFixed:
        case Js::OpCode::CallINewTargetNew:
        {
            Js::CallFlags flags = Js::CallFlags_None;

            if (instr->isCtorCall)
            {
                flags = Js::CallFlags_New;
            }
            else
            {
                if (instr->m_opcode == Js::OpCode::CallINew)
                {
                    flags = Js::CallFlags_New;
                }
                else if (instr->m_opcode == Js::OpCode::CallINewTargetNew)
                {
                    flags = (Js::CallFlags) (Js::CallFlags_New | Js::CallFlags_ExtraArg | Js::CallFlags_NewTarget);
                }
                if (instr->GetDst())
                {
                    flags = (Js::CallFlags) (flags | Js::CallFlags_Value);
                }
                else
                {
                    flags = (Js::CallFlags) (flags | Js::CallFlags_NotUsed);
                }
            }

            if (!PHASE_OFF(Js::CallFastPathPhase, this->m_func) && !noMathFastPath)
            {
                // We shouldn't have turned this instruction into a fixed method call if we're calling one of the
                // built-ins we still inline in the lowerer.
                Assert(instr->m_opcode != Js::OpCode::CallIFixed || !Func::IsBuiltInInlinedInLowerer(instr->GetSrc1()));

                // Disable InlineBuiltInLibraryCall as it does not work well with 2nd chance reg alloc
                // and may invalidate live on back edge data by introducing refs across loops. See Winblue Bug: 577641
                //// Callee may still be a library built-in; if so, generate it inline.
                //if (this->InlineBuiltInLibraryCall(instr))
                //{
                //    m_lowererMD.LowerCallI(instr, (ushort)flags, true /*isHelper*/);
                //}
                //else
                //{
                    m_lowererMD.LowerCallI(instr, (ushort)flags);
                //}
            }
            else
            {
                m_lowererMD.LowerCallI(instr, (ushort)flags);
            }
            break;
        }
        case Js::OpCode::AsmJsCallI:
            m_lowererMD.LowerAsmJsCallI(instr);
            break;

        case Js::OpCode::AsmJsCallE:
            m_lowererMD.LowerAsmJsCallE(instr);
            break;

        case Js::OpCode::CallIEval:
        {
            Js::CallFlags flags = (Js::CallFlags)(Js::CallFlags_ExtraArg | (instr->GetDst() ? Js::CallFlags_Value : Js::CallFlags_NotUsed));
            if (IsSpreadCall(instr))
            {
                instrPrev = LowerSpreadCall(instr, flags);
            }
            else
            {
                m_lowererMD.LowerCallI(instr, (ushort)flags);
            }

#ifdef PERF_HINT
            if (PHASE_TRACE1(Js::PerfHintPhase))
            {
                WritePerfHint(PerfHints::CallsEval, this->m_func->GetJnFunction(), instr->GetByteCodeOffset());
            }
#endif
            break;
        }

        case Js::OpCode::CallIPut:
            m_lowererMD.LowerCallPut(instr);
            break;

        case Js::OpCode::CallHelper:
            instrPrev = m_lowererMD.LowerCallHelper(instr);
            break;

        case Js::OpCode::Ret:
            if (instr->m_next->m_opcode != Js::OpCode::FunctionExit)
            {
                // If this RET isn't at the end of the function, insert a branch to
                // the epilog.

                IR::Instr *exitPrev = m_func->m_exitInstr->m_prev;
                if (!exitPrev->IsLabelInstr())
                {
                    exitPrev = IR::LabelInstr::New(Js::OpCode::Label, m_func);
                    m_func->m_exitInstr->InsertBefore(exitPrev);
                }
                IR::BranchInstr *exitBr = IR::BranchInstr::New(Js::OpCode::Br,
                    exitPrev->AsLabelInstr(), m_func);
                instr->InsertAfter(exitBr);
                m_lowererMD.LowerUncondBranch(exitBr);
            }

            m_lowererMD.LowerRet(instr);
            break;

        case Js::OpCode::LdArgumentsFromFrame:
            this->LoadArgumentsFromFrame(instr);
            break;

        case Js::OpCode::LdC_A_I4:
            src1 = instr->UnlinkSrc1();
            AssertMsg(src1->IsIntConstOpnd(), "Source of LdC_A_I4 should be an IntConst...");

            instrPrev = this->LowerLoadVar(instr,
                IR::AddrOpnd::NewFromNumber(static_cast<int32>(src1->AsIntConstOpnd()->GetValue()), this->m_func));
            src1->Free(this->m_func);
            break;

        case Js::OpCode::LdC_A_R8:
            src1 = instr->UnlinkSrc1();
            AssertMsg(src1->IsFloatConstOpnd(), "Source of LdC_A_R8 should be a FloatConst...");
            instrPrev = this->LowerLoadVar(instr, src1->AsFloatConstOpnd()->GetAddrOpnd(this->m_func));
            src1->Free(this->m_func);
            break;

        case Js::OpCode::LdC_F8_R8:
            src1 = instr->UnlinkSrc1();
            AssertMsg(src1->IsFloatConstOpnd(), "Source of LdC_F8_R8 should be a FloatConst...");
            instrPrev = m_lowererMD.LoadFloatValue(instr->UnlinkDst()->AsRegOpnd(), src1->AsFloatConstOpnd()->m_value, instr);

            src1->Free(this->m_func);
            instr->Remove();

            break;

        case Js::OpCode::NewRegEx:
            instrPrev = this->LowerNewRegEx(instr);
            break;

        case Js::OpCode::Conv_Obj:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_ConvObject);
            break;

        case Js::OpCode::NewWithObject:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_NewWithObject);
            break;

        case Js::OpCode::LdCustomSpreadIteratorList:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_ToSpreadedFunctionArgument);
            break;

        case Js::OpCode::Conv_Num:
            this->LowerConvNum(instr, noMathFastPath);
            break;

        case Js::OpCode::Incr_A:
            if (PHASE_OFF(Js::MathFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerUnaryHelperMem(instr, IR::HelperOp_Increment);
            }
            else
            {
                instr->SetSrc2(IR::AddrOpnd::New(Js::TaggedInt::ToVarUnchecked(1), IR::AddrOpndKindConstantVar, this->m_func));
                m_lowererMD.GenerateFastAdd(instr);
                instr->FreeSrc2();
                this->LowerUnaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Increment));
            }
            break;

        case Js::OpCode::Decr_A:
            if (PHASE_OFF(Js::MathFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerUnaryHelperMem(instr, IR::HelperOp_Decrement);
            }
            else
            {
                instr->SetSrc2(IR::AddrOpnd::New(Js::TaggedInt::ToVarUnchecked(1), IR::AddrOpndKindConstantVar, this->m_func));
                m_lowererMD.GenerateFastSub(instr);
                instr->FreeSrc2();
                this->LowerUnaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Decrement));
            }
            break;

        case Js::OpCode::Neg_A:
            if (instr->GetDst()->IsFloat())
            {
                Assert(instr->GetSrc1()->IsFloat());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (PHASE_OFF(Js::MathFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerUnaryHelperMem(instr, IR::HelperOp_Negate);
            }
            else if (m_lowererMD.GenerateFastNeg(instr))
            {
                this->LowerUnaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Negate));
            }
            break;

        case Js::OpCode::Not_A:
            if (PHASE_OFF(Js::BitopsFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerUnaryHelperMem(instr, IR::HelperOp_Not);
            }
            else if (m_lowererMD.GenerateFastNot(instr))
            {
                this->LowerUnaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Not));
            }
            break;

        case Js::OpCode::BrEq_I4:
        case Js::OpCode::BrNeq_I4:
        case Js::OpCode::BrGt_I4:
        case Js::OpCode::BrGe_I4:
        case Js::OpCode::BrLt_I4:
        case Js::OpCode::BrLe_I4:
        case Js::OpCode::BrUnGt_I4:
        case Js::OpCode::BrUnGe_I4:
        case Js::OpCode::BrUnLt_I4:
        case Js::OpCode::BrUnLe_I4:
        {
            // See calls to MarkOneFltTmpSym under BrSrEq. This is to handle the case
            // where a branch is type-specialized and uses the result of a float pref op,
            // which must then be saved to var at the def.
            StackSym *sym = instr->GetSrc1()->GetStackSym();
            if (sym)
            {
                sym = sym->GetVarEquivSym(nullptr);
            }
            sym = instr->GetSrc2()->GetStackSym();
            if (sym)
            {
                sym = sym->GetVarEquivSym(nullptr);
            }
        }
        // FALLTHROUGH
        case Js::OpCode::Neg_I4:
        case Js::OpCode::Not_I4:
        case Js::OpCode::Add_I4:
        case Js::OpCode::Sub_I4:
        case Js::OpCode::Mul_I4:
        case Js::OpCode::Rem_I4:
        case Js::OpCode::Or_I4:
        case Js::OpCode::Xor_I4:
        case Js::OpCode::And_I4:
        case Js::OpCode::Shl_I4:
        case Js::OpCode::Shr_I4:
        case Js::OpCode::ShrU_I4:
        case Js::OpCode::BrTrue_I4:
        case Js::OpCode::BrFalse_I4:
            if(instr->HasBailOutInfo())
            {
                const auto bailOutKind = instr->GetBailOutKind();
                if(bailOutKind & IR::BailOutOnResultConditions ||
                    bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck)
                {
                    const auto nonBailOutInstr = SplitBailOnResultCondition(instr);
                    IR::LabelInstr *bailOutLabel, *skipBailOutLabel;
                    LowerBailOnResultCondition(instr, &bailOutLabel, &skipBailOutLabel);
                    LowerInstrWithBailOnResultCondition(nonBailOutInstr, bailOutKind, bailOutLabel, skipBailOutLabel);
                }
                else if(bailOutKind == IR::BailOnModByPowerOf2)
                {
                    Assert(instr->m_opcode == Js::OpCode::Rem_I4);
                    bool fastPath = GenerateSimplifiedInt4Rem(instr);
                    Assert(fastPath);
                    instr->FreeSrc1();
                    instr->FreeSrc2();
                    this->GenerateBailOut(instr);
                }
            }
            else
            {
                if (instr->m_opcode == Js::OpCode::Rem_I4)
                {
                    // fast path
                    this->GenerateSimplifiedInt4Rem(instr);
                    // slow path
                    this->LowerRemI4(instr);
                }
#if defined(_M_IX86) || defined(_M_X64)
                else if (instr->m_opcode == Js::OpCode::Mul_I4)
                {
                    if (!LowererMD::GenerateSimplifiedInt4Mul(instr))
                    {
                        m_lowererMD.EmitInt4Instr(instr);
                    }
                }
#endif
                else
                {
                    m_lowererMD.EmitInt4Instr(instr);
                }
            }
            break;

        case Js::OpCode::Div_I4:
            this->LowerDivI4(instr);
            break;

        case Js::OpCode::Add_Ptr:
            m_lowererMD.EmitPtrInstr(instr);
            break;

        case Js::OpCode::Typeof:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_Typeof);
            break;

        case Js::OpCode::TypeofElem:
            this->LowerLdElemI(instr, IR::HelperOp_TypeofElem, false);
            break;

        case Js::OpCode::LdLen_A:
        {
            bool fastPath = !noMathFastPath;
            if(!fastPath && instr->HasBailOutInfo())
            {
                // Some bailouts are generated around the helper call, and will work even if the fast path is disabled. Other
                // bailouts require the fast path.
                const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                if(bailOutKind & IR::BailOutKindBits)
                {
                    fastPath = true;
                }
                else
                {
                    const IR::BailOutKind bailOutKindMinusBits = bailOutKind & ~IR::BailOutKindBits;
                    fastPath =
                        bailOutKindMinusBits &&
                        bailOutKindMinusBits != IR::BailOutOnImplicitCalls &&
                        bailOutKindMinusBits != IR::BailOutOnImplicitCallsPreOp;
                }
            }

            bool instrIsInHelperBlock;
            if(!fastPath)
            {
                LowerLdLen(instr, false);
            }
            else if(GenerateFastLdLen(instr, &instrIsInHelperBlock))
            {
                Assert(
                    !instr->HasBailOutInfo() ||
                    (instr->GetBailOutKind() & ~IR::BailOutKindBits) != IR::BailOutOnIrregularLength);
                LowerLdLen(instr, instrIsInHelperBlock);
            }
            break;
        }

        case Js::OpCode::LdThis:
        {
            if (noFieldFastPath || !m_lowererMD.GenerateLdThisCheck(instr))
            {
                IR::JnHelperMethod meth;
                if (instr->IsJitProfilingInstr())
                {
                    Assert(instr->AsJitProfilingInstr()->profileId == Js::Constants::NoProfileId);
                    m_lowererMD.LoadHelperArgument(instr, CreateFunctionBodyOpnd(instr->m_func));
                    meth = IR::HelperSimpleProfiledLdThis;
                    this->LowerBinaryHelper(instr, meth);
                }
                else
                {
                    meth = IR::HelperLdThisNoFastPath;
                    this->LowerBinaryHelperMem(instr, meth);
                }
            }
            else
            {
                this->LowerBinaryHelperMem(instr, IR::HelperLdThis);
            }
            break;
        }

        case Js::OpCode::StrictLdThis:
            if (noFieldFastPath)
            {
                IR::JnHelperMethod meth;
                if (instr->IsJitProfilingInstr())
                {
                    Assert(instr->AsJitProfilingInstr()->profileId == Js::Constants::NoProfileId);
                    m_lowererMD.LoadHelperArgument(instr, CreateFunctionBodyOpnd(instr->m_func));
                    meth = IR::HelperSimpleProfiledStrictLdThis;
                    this->LowerUnaryHelper(instr, meth);
                }
                else
                {
                    meth = IR::HelperStrictLdThis;
                    this->LowerUnaryHelperMem(instr, meth);
                }
            }
            else
            {
                 m_lowererMD.GenerateLdThisStrict(instr);
                 instr->Remove();
            }
            break;

        case Js::OpCode::CheckThis:
            m_lowererMD.GenerateLdThisCheck(instr);
            instr->FreeSrc1();
            this->GenerateBailOut(instr);
            break;

        case Js::OpCode::StrictCheckThis:
            m_lowererMD.GenerateLdThisStrict(instr);
            instr->FreeSrc1();
            this->GenerateBailOut(instr);
            break;

        case Js::OpCode::NewScArray:
            instrPrev = this->LowerNewScArray(instr);
            break;

        case Js::OpCode::NewScArrayWithMissingValues:
            this->LowerUnaryHelperMem(instr, IR::HelperScrArr_OP_NewScArrayWithMissingValues);
            break;

        case Js::OpCode::NewScIntArray:
            instrPrev = this->LowerNewScIntArray(instr);
            break;

        case Js::OpCode::NewScFltArray:
            instrPrev = this->LowerNewScFltArray(instr);
            break;

        case Js::OpCode::GetForInEnumerator:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_OP_GetForInEnumerator);
            break;

        case Js::OpCode::ReleaseForInEnumerator:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_OP_ReleaseForInEnumerator);
            break;

        case Js::OpCode::Add_A:
            if (instr->GetDst()->IsFloat())
            {
                Assert(instr->GetSrc1()->IsFloat());
                Assert(instr->GetSrc2()->IsFloat());
                // we don't want to mix float32 and float64
                Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
                Assert(instr->GetDst()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (PHASE_OFF(Js::MathFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_Add);
            }
            else if (m_lowererMD.TryGenerateFastMulAdd(instr, &instrPrev))
            {
            }
            else
            {
                m_lowererMD.GenerateFastAdd(instr);
                this->LowerBinaryHelperMemWithTemp3(instr, IR_HELPER_OP_FULL_OR_INPLACE(Add), IR::HelperOp_AddLeftDead);
            }
            break;

        case Js::OpCode::Div_A:
        {
            if (instr->IsJitProfilingInstr()) {
                LowerProfiledBinaryOp(instr->AsJitProfilingInstr(), IR::HelperSimpleProfiledDivide);
            }
            else if (instr->GetDst()->IsFloat())
            {
                Assert(instr->GetSrc1()->IsFloat());
                Assert(instr->GetSrc2()->IsFloat());
                Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
                Assert(instr->GetDst()->GetType() == instr->GetSrc2()->GetType());

                m_lowererMD.LowerToFloat(instr);
            }
            else
            {
                if (!PHASE_OFF(Js::MathFastPathPhase, this->m_func) && !noMathFastPath)
                {
                    IR::AddrOpnd *src2 = instr->GetSrc2()->IsAddrOpnd() ? instr->GetSrc2()->AsAddrOpnd() : nullptr;
                    if (src2 && src2->IsVar() && Js::TaggedInt::Is(src2->m_address))
                    {
                        int32 value = Js::TaggedInt::ToInt32(src2->m_address);
                        if (Math::IsPow2(value))
                        {
                            m_lowererMD.GenerateFastDivByPow2(instr);
                        }
                    }
                }
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Divide));
            }
            break;
        }

        case Js::OpCode::Expo_A:
        {
            if (instr->GetDst()->IsFloat())
            {
                Assert(instr->GetSrc1()->IsFloat());
                Assert(instr->GetSrc2()->IsFloat());
                Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
                Assert(instr->GetDst()->GetType() == instr->GetSrc2()->GetType());

                m_lowererMD.GenerateFastInlineBuiltInCall(instr, IR::HelperDirectMath_Pow);
            }
            else
            {
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Exponentiation));
            }
            break;
        }

        case Js::OpCode::Mul_A:
            if (instr->GetDst()->IsFloat())
            {
                Assert(instr->GetSrc1()->IsFloat());
                Assert(instr->GetSrc2()->IsFloat());
                Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
                Assert(instr->GetDst()->GetType() == instr->GetSrc2()->GetType());

                m_lowererMD.LowerToFloat(instr);
            }
            else if (PHASE_OFF(Js::MathFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_Multiply);
            }
            else if (m_lowererMD.GenerateFastMul(instr))
            {
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Multiply));
            }
            break;

        case Js::OpCode::Rem_A:
            if (instr->GetDst()->IsFloat64())
            {
                this->LowerRemR8(instr);
            }
            else if (instr->IsJitProfilingInstr())
            {
                this->LowerProfiledBinaryOp(instr->AsJitProfilingInstr(), IR::HelperSimpleProfiledRemainder);
            }
            else
            {
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Modulus));
            }
            break;

        case Js::OpCode::Sub_A:
            if (instr->GetDst()->IsFloat())
            {
                Assert(instr->GetSrc1()->IsFloat());
                Assert(instr->GetSrc2()->IsFloat());
                Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
                Assert(instr->GetDst()->GetType() == instr->GetSrc2()->GetType());

                m_lowererMD.LowerToFloat(instr);
            }
            else if (PHASE_OFF(Js::MathFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_Subtract);
            }
            else if (m_lowererMD.TryGenerateFastMulAdd(instr, &instrPrev))
            {
            }
            else
            {
                m_lowererMD.GenerateFastSub(instr);
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Subtract));
            }
            break;

        case Js::OpCode::And_A:
            if (PHASE_OFF(Js::BitopsFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_And);
            }
            else if (m_lowererMD.GenerateFastAnd(instr))
            {
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(And));
            }
            break;

        case Js::OpCode::Or_A:
            if (PHASE_OFF(Js::BitopsFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_Or);
            }
            else if (m_lowererMD.GenerateFastOr(instr))
            {
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Or));
            }
            break;

        case Js::OpCode::Xor_A:
            if (PHASE_OFF(Js::BitopsFastPathPhase, this->m_func) || noMathFastPath || m_lowererMD.GenerateFastXor(instr))
            {
                this->LowerBinaryHelperMemWithTemp2(instr, IR_HELPER_OP_FULL_OR_INPLACE(Xor));
            }
            break;

        case Js::OpCode::Shl_A:
            if (PHASE_OFF(Js::BitopsFastPathPhase, this->m_func) || noMathFastPath || m_lowererMD.GenerateFastShiftLeft(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_ShiftLeft);
            }
            break;

        case Js::OpCode::Shr_A:
            if (PHASE_OFF(Js::BitopsFastPathPhase, this->m_func) || noMathFastPath || m_lowererMD.GenerateFastShiftRight(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_ShiftRight);
            }
            break;

        case Js::OpCode::ShrU_A:
            if (PHASE_OFF(Js::BitopsFastPathPhase, this->m_func) || noMathFastPath || m_lowererMD.GenerateFastShiftRight(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOp_ShiftRightU);
            }
            break;

        case Js::OpCode::CmEq_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                this->m_lowererMD.GenerateFastCmXxR8(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath && this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
            {
                if (!fNoLower)
                {
                    this->LowerBinaryHelperMem(instr, IR::HelperOP_CmEq_A);
                }
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmEq_A);
            }
            break;

        case Js::OpCode::CmNeq_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                this->m_lowererMD.GenerateFastCmXxR8(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath && this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
            {
                if (!fNoLower)
                {
                    this->LowerBinaryHelperMem(instr, IR::HelperOP_CmNeq_A);
                }
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmNeq_A);
            }
            break;

        case Js::OpCode::CmSrEq_A:
            if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath && this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
            {
                if (!fNoLower)
                {
                    this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrEq_A);
                }
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath && this->TryGenerateFastCmSrEq(instr))
            {
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrEq_A);
            }
            break;

        case Js::OpCode::CmSrNeq_A:
            if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath && this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
            {
                if (!fNoLower)
                {
                    this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrNeq_A);
                }
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrNeq_A);
            }
            break;

        case Js::OpCode::CmGt_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                this->m_lowererMD.GenerateFastCmXxR8(instr);
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmGt_A);
            }
            break;

        case Js::OpCode::CmGe_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                this->m_lowererMD.GenerateFastCmXxR8(instr);
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmGe_A);
            }
            break;

        case Js::OpCode::CmLt_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                this->m_lowererMD.GenerateFastCmXxR8(instr);
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmLt_A);
            }
            break;

        case Js::OpCode::CmLe_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                this->m_lowererMD.GenerateFastCmXxR8(instr);
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath || !m_lowererMD.GenerateFastCmXxTaggedInt(instr))
            {
                this->LowerBinaryHelperMem(instr, IR::HelperOP_CmLe_A);
            }
            break;

        case Js::OpCode::CmEq_I4:
        case Js::OpCode::CmNeq_I4:
        case Js::OpCode::CmGe_I4:
        case Js::OpCode::CmGt_I4:
        case Js::OpCode::CmLe_I4:
        case Js::OpCode::CmLt_I4:
        case Js::OpCode::CmUnGe_I4:
        case Js::OpCode::CmUnGt_I4:
        case Js::OpCode::CmUnLe_I4:
        case Js::OpCode::CmUnLt_I4:
            this->m_lowererMD.GenerateFastCmXxI4(instr);
            break;

        case Js::OpCode::Conv_Bool:
            instrPrev = this->m_lowererMD.GenerateConvBool(instr);
            break;

        case Js::OpCode::IsInst:
            m_lowererMD.GenerateFastIsInst(instr);
            instrPrev = this->LowerIsInst(instr, IR::HelperScrObj_OP_IsInst);
            break;

        case Js::OpCode::IsIn:
            this->LowerBinaryHelperMem(instr, IR::HelperOp_IsIn);
            break;

        case Js::OpCode::LdInt8ArrViewElem:
        case Js::OpCode::LdUInt8ArrViewElem:
        case Js::OpCode::LdInt16ArrViewElem:
        case Js::OpCode::LdUInt16ArrViewElem:
        case Js::OpCode::LdInt32ArrViewElem:
        case Js::OpCode::LdUInt32ArrViewElem:
        case Js::OpCode::LdFloat32ArrViewElem:
        case Js::OpCode::LdFloat64ArrViewElem:
            instrPrev = LowerLdArrViewElem(instr);
            break;

        case Js::OpCode::StInt8ArrViewElem:
        case Js::OpCode::StUInt8ArrViewElem:
        case Js::OpCode::StInt16ArrViewElem:
        case Js::OpCode::StUInt16ArrViewElem:
        case Js::OpCode::StInt32ArrViewElem:
        case Js::OpCode::StUInt32ArrViewElem:
        case Js::OpCode::StFloat32ArrViewElem:
        case Js::OpCode::StFloat64ArrViewElem:
            instrPrev = LowerStArrViewElem(instr);
            break;

        case Js::OpCode::Memset:
        case Js::OpCode::Memcopy:
        {
            LowerMemOp(instr);
            break;
        }

        case Js::OpCode::ArrayDetachedCheck:
            instrPrev = LowerArrayDetachedCheck(instr);
            break;

        case Js::OpCode::StElemI_A:
        case Js::OpCode::StElemI_A_Strict:
        {
            // Note: under debugger (Fast F12) don't let GenerateFastStElemI which calls into ToNumber_Helper
            //       which takes double, and currently our helper wrapper doesn't support double.
            bool fastPath = !noMathFastPath && !m_func->IsJitInDebugMode();
            if(!fastPath && instr->HasBailOutInfo())
            {
                // Some bailouts are generated around the helper call, and will work even if the fast path is disabled. Other
                // bailouts require the fast path.
                const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                const IR::BailOutKind bailOutKindBits = bailOutKind & IR::BailOutKindBits;
                if(bailOutKindBits & ~(IR::BailOutOnMissingValue | IR::BailOutConvertedNativeArray))
                {
                    fastPath = true;
                }
                else
                {
                    const IR::BailOutKind bailOutKindMinusBits = bailOutKind & ~IR::BailOutKindBits;
                    fastPath =
                        bailOutKindMinusBits &&
                        bailOutKindMinusBits != IR::BailOutOnImplicitCalls &&
                        bailOutKindMinusBits != IR::BailOutOnImplicitCallsPreOp;
                }
            }

            IR::Opnd * opnd = instr->GetDst();
            IR::Opnd * baseOpnd = opnd->AsIndirOpnd()->GetBaseOpnd();
            ValueType profiledBaseValueType = baseOpnd->AsRegOpnd()->GetValueType();
            if (profiledBaseValueType.IsUninitialized() && baseOpnd->AsRegOpnd()->m_sym->IsSingleDef())
            {
                baseOpnd->SetValueType(baseOpnd->FindProfiledValueType());
            }

            bool instrIsInHelperBlock;
            if (!fastPath)
            {
                this->LowerStElemI(
                    instr,
                    instr->m_opcode == Js::OpCode::StElemI_A ? Js::PropertyOperation_None : Js::PropertyOperation_StrictMode,
                    false);
            }
            else if (GenerateFastStElemI(instr, &instrIsInHelperBlock))
            {
#if DBG
                if(instr->HasBailOutInfo())
                {
                    const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                    Assert(
                        (bailOutKind & ~IR::BailOutKindBits) != IR::BailOutConventionalTypedArrayAccessOnly &&
                        !(
                            bailOutKind &
                            (IR::BailOutConventionalNativeArrayAccessOnly | IR::BailOutOnArrayAccessHelperCall)
                        ));
                }
#endif
                this->LowerStElemI(
                    instr,
                    instr->m_opcode == Js::OpCode::StElemI_A ? Js::PropertyOperation_None : Js::PropertyOperation_StrictMode,
                    instrIsInHelperBlock);
            }
            break;
        }

        case Js::OpCode::LdElemI_A:
        case Js::OpCode::LdMethodElem:
        {
            bool fastPath =
                !noMathFastPath &&
                (
                    instr->m_opcode != Js::OpCode::LdMethodElem ||
                    instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetValueType().IsLikelyObject()
                );
            if(!fastPath && instr->HasBailOutInfo())
            {
                // Some bailouts are generated around the helper call, and will work even if the fast path is disabled. Other
                // bailouts require the fast path.
                const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                if(bailOutKind & IR::BailOutKindBits)
                {
                    fastPath = true;
                }
                else
                {
                    const IR::BailOutKind bailOutKindMinusBits = bailOutKind & ~IR::BailOutKindBits;
                    fastPath =
                        bailOutKindMinusBits &&
                        bailOutKindMinusBits != IR::BailOutOnImplicitCalls &&
                        bailOutKindMinusBits != IR::BailOutOnImplicitCallsPreOp;
                }
            }

            IR::Opnd * opnd = instr->GetSrc1();
            IR::Opnd * baseOpnd = opnd->AsIndirOpnd()->GetBaseOpnd();
            ValueType profiledBaseValueType = baseOpnd->AsRegOpnd()->GetValueType();
            if (profiledBaseValueType.IsUninitialized() && baseOpnd->AsRegOpnd()->m_sym->IsSingleDef())
            {
                baseOpnd->SetValueType(baseOpnd->FindProfiledValueType());
            }

            bool instrIsInHelperBlock;
            if (!fastPath)
            {
                this->LowerLdElemI(
                    instr,
                    instr->m_opcode == Js::OpCode::LdElemI_A ? IR::HelperOp_GetElementI : IR::HelperOp_GetMethodElement,
                    false);
            }
            else if (GenerateFastLdElemI(instr, &instrIsInHelperBlock))
            {
#if DBG
                if(instr->HasBailOutInfo())
                {
                    const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                    Assert(
                        (bailOutKind & ~IR::BailOutKindBits) != IR::BailOutConventionalTypedArrayAccessOnly &&
                        !(
                            bailOutKind &
                            (IR::BailOutConventionalNativeArrayAccessOnly | IR::BailOutOnArrayAccessHelperCall)
                        ));
                }
#endif
                this->LowerLdElemI(
                    instr,
                    instr->m_opcode == Js::OpCode::LdElemI_A ? IR::HelperOp_GetElementI : IR::HelperOp_GetMethodElement,
                    instrIsInHelperBlock);
            }
            break;
        }

        case Js::OpCode::InitSetElemI:
            instrPrev = this->LowerStElemI(instr, Js::PropertyOperation_None, false, IR::HelperOP_InitElemSetter);
            break;

        case Js::OpCode::InitGetElemI:
            instrPrev = this->LowerStElemI(instr, Js::PropertyOperation_None, false, IR::HelperOP_InitElemGetter);
            break;

        case Js::OpCode::InitComputedProperty:
            instrPrev = this->LowerStElemI(instr, Js::PropertyOperation_None, false, IR::HelperOP_InitComputedProperty);
            break;

        case Js::OpCode::Delete_A:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_Delete);
            break;

        case Js::OpCode::DeleteElemI_A:
            this->LowerDeleteElemI(instr, false);
            break;

        case Js::OpCode::DeleteElemIStrict_A:
            this->LowerDeleteElemI(instr, true);
            break;

        case Js::OpCode::BytecodeArgOutCapture:
            m_lowererMD.ChangeToAssign(instr);
            break;

        case Js::OpCode::UnwrapWithObj:
            this->LowerUnaryHelper(instr, IR::HelperOp_UnwrapWithObj);
            break;

        case Js::OpCode::Ld_A:
        case Js::OpCode::Ld_I4:
        case Js::OpCode::InitConst:
            if (instr->IsJitProfilingInstr() && instr->AsJitProfilingInstr()->isBeginSwitch) {
                LowerProfiledBeginSwitch(instr->AsJitProfilingInstr());
                break;
            }
            m_lowererMD.ChangeToAssign(instr);
            if(instr->HasBailOutInfo())
            {
                IR::BailOutKind bailOutKind = instr->GetBailOutKind();

                if(bailOutKind == IR::BailOutExpectingString)
                {
                    this->LowerBailOnNotString(instr);
                }
                else
                {
                    // Should not reach here as there are only 1 BailOutKind (BailOutExpectingString) currently associated with the Load Instr
                    Assert(false);
                }
            }
            break;

        case Js::OpCode::LdIndir:
            Assert(instr->GetDst());
            Assert(instr->GetDst()->IsRegOpnd());
            Assert(instr->GetSrc1());
            Assert(instr->GetSrc1()->IsIndirOpnd());
            Assert(!instr->GetSrc2());
            m_lowererMD.ChangeToAssign(instr);
            break;

        case Js::OpCode::FromVar:
            Assert(instr->GetSrc1()->GetType() == TyVar);
            if (instr->GetDst()->GetType() == TyInt32)
            {
                if(m_lowererMD.EmitLoadInt32(instr))
                {
                    // Bail out instead of calling a helper
                    Assert(instr->GetBailOutKind() == IR::BailOutIntOnly || instr->GetBailOutKind() == IR::BailOutExpectingInteger);
                    Assert(!instr->GetSrc1()->GetValueType().IsInt());  // when we know it's an int, it should not have bailout info, to avoid generating a bailout path that will never be taken
                    instr->UnlinkSrc1();
                    instr->UnlinkDst();
                    GenerateBailOut(instr);
                }
            }
            else if (instr->GetDst()->IsFloat())
            {
                if (m_func->GetJnFunction()->GetIsAsmJsFunction())
                {
                    m_lowererMD.EmitLoadFloat(instr->GetDst(), instr->GetSrc1(), instr);
                    instr->Remove();
                }
                else
                {
                    m_lowererMD.EmitLoadFloatFromNumber(instr->GetDst(), instr->GetSrc1(), instr);
                }
            }
            // Support on IA only
#if defined(_M_IX86) || defined(_M_X64)
            else if (instr->GetDst()->IsSimd128())
            {
                // SIMD_JS
                m_lowererMD.GenerateCheckedSimdLoad(instr);
            }
#endif
            else
            {
                Assert(UNREACHED);
            }
            break;

        case Js::OpCode::ArgOut_A:
            // I don't know if this can happen in asm.js mode, but if it can, we might want to handle differently
            Assert(!m_func->GetJnFunction()->GetIsAsmjsMode());
            // fall-through

        case Js::OpCode::ArgOut_A_Inline:
        case Js::OpCode::ArgOut_A_Dynamic:
            {
                // ArgOut/StartCall are normally lowered by the lowering of the associated call instr.
                // If the call becomes unreachable, we could end up with an orphan ArgOut or StartCall.
                // Change the ArgOut into a store to the stack for bailouts
                instr->FreeSrc2();
                StackSym *argSym = instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
                argSym->m_offset = this->m_func->StackAllocate(sizeof(Js::Var));
                argSym->m_allocated = true;
                argSym->m_isOrphanedArg = true;
                this->m_lowererMD.ChangeToAssign(instr);
            }
            break;
        case Js::OpCode::LoweredStartCall:
        case Js::OpCode::StartCall:
            // ArgOut/StartCall are normally lowered by the lowering of the associated call instr.
            // If the call becomes unreachable, we could end up with an orphan ArgOut or StartCall.
            // We'll just delete these StartCalls during peeps.
            break;

        case Js::OpCode::ToVar:
            Assert(instr->GetDst()->GetType() == TyVar);
            if (instr->GetSrc1()->GetType() == TyInt32)
            {
                m_lowererMD.EmitLoadVar(instr);
            }
            else if (instr->GetSrc1()->GetType() == TyFloat64)
            {
                Assert(instr->GetSrc1()->IsRegOpnd());
                m_lowererMD.SaveDoubleToVar(
                    instr->GetDst()->AsRegOpnd(),
                    instr->GetSrc1()->AsRegOpnd(), instr, instr);
                instr->Remove();
            }
#if defined(_M_IX86) || defined(_M_X64)
            else if (IRType_IsSimd128(instr->GetSrc1()->GetType()))
            {
                m_lowererMD.GenerateSimdStore(instr);
            }
#endif
            else
            {
                Assert(UNREACHED);
            }
            break;

        case Js::OpCode::Conv_Prim:
            if (instr->GetDst()->IsFloat())
            {
                if (instr->GetSrc1()->IsIntConstOpnd())
                {
                    LoadFloatFromNonReg(instr->UnlinkSrc1(), instr->UnlinkDst(), instr);
                }
                else if (instr->GetSrc1()->IsInt32())
                {
                    m_lowererMD.EmitIntToFloat(instr->GetDst(), instr->GetSrc1(), instr);
                }
                else if (instr->GetSrc1()->IsUInt32())
                {
                    Assert(instr->GetDst()->IsFloat64());
                    m_lowererMD.EmitUIntToFloat(instr->GetDst(), instr->GetSrc1(), instr);
                }
                else
                {
                    Assert(instr->GetDst()->IsFloat64());
                    Assert(instr->GetSrc1()->IsFloat32());
                    m_lowererMD.EmitFloat32ToFloat64(instr->GetDst(), instr->GetSrc1(), instr);
                }
            }
            else
            {
                Assert(instr->GetDst()->IsInt32());
                Assert(instr->GetSrc1()->IsFloat());
                m_lowererMD.EmitFloatToInt(instr->GetDst(), instr->GetSrc1(), instr);
            }
            instr->Remove();
            break;

        case Js::OpCode::FunctionExit:
            LowerFunctionExit(instr);
            // The rest of Epilog generation happens after reg allocation
            break;

        case Js::OpCode::FunctionEntry:
            LowerFunctionEntry(instr);
            // The rest of Prolog generation happens after reg allocation
            break;

        case Js::OpCode::ArgIn_Rest:
        case Js::OpCode::ArgIn_A:
            if (m_func->GetJnFunction()->GetIsAsmjsMode() && !m_func->IsLoopBody())
            {
                instrPrev = LowerArgInAsmJs(instr);
            }
            else
            {
                instrPrev = LowerArgIn(instr);
            }
            break;

        case Js::OpCode::Label:
            if (instr->AsLabelInstr()->m_isLoopTop)
            {
                if (this->outerMostLoopLabel == instr)
                {
                    noFieldFastPath = !defaultDoFastPath;
                    noMathFastPath = !defaultDoFastPath;
                    this->outerMostLoopLabel = nullptr;
                    instr->AsLabelInstr()->GetLoop()->isProcessed = true;
                }
                this->m_func->MarkConstantAddressSyms(instr->AsLabelInstr()->GetLoop()->regAlloc.liveOnBackEdgeSyms);
                instr->AsLabelInstr()->GetLoop()->regAlloc.liveOnBackEdgeSyms->Or(this->addToLiveOnBackEdgeSyms);
            }
            break;

        case Js::OpCode::Br:
            m_lowererMD.LowerUncondBranch(instr);
            break;

        case Js::OpCode::BrFncEqApply:
          LowerBrFncApply(instr,IR::HelperOp_OP_BrFncEqApply);
          break;

        case Js::OpCode::BrFncNeqApply:
          LowerBrFncApply(instr,IR::HelperOp_OP_BrFncNeqApply);
          break;

        case Js::OpCode::BrHasSideEffects:
        case Js::OpCode::BrNotHasSideEffects:
            m_lowererMD.GenerateFastBrS(instr->AsBranchInstr());
            break;

        case Js::OpCode::BrFalse_A:
        case Js::OpCode::BrTrue_A:
            if (instr->GetSrc1()->IsFloat())
            {
                GenerateFastBrBool(instr->AsBranchInstr());
            }
            else if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) ||
                noMathFastPath ||
                GenerateFastBrBool(instr->AsBranchInstr()))
            {
                this->LowerBrBMem(instr, IR::HelperConv_ToBoolean);
            }
            break;

        case Js::OpCode::BrOnObject_A:
            if (PHASE_OFF(Js::BranchFastPathPhase, this->m_func) || noMathFastPath)
            {
                this->LowerBrOnObject(instr, IR::HelperOp_IsObject);
            }
            else
            {
                GenerateFastBrOnObject(instr);
            }
            break;

        case Js::OpCode::BrOnClassConstructor:
            this->LowerBrOnClassConstructor(instr, IR::HelperOp_IsClassConstructor);
            break;

        case Js::OpCode::BrAddr_A:
        case Js::OpCode::BrNotAddr_A:
        case Js::OpCode::BrNotNull_A:
            m_lowererMD.LowerCondBranch(instr);
            break;

        case Js::OpCode::BrEq_A:
        case Js::OpCode::BrNotNeq_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
            {
                bool needHelper = true;
                if (this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
                {
                    if (!fNoLower)
                    {
                        this->LowerBrCMem(instr, IR::HelperOp_Equal, false, false /*isHelper*/);
                    }
                }
                else if (this->TryGenerateFastBrEq(instr))
                {
                }
                else if (m_lowererMD.GenerateFastBrString(instr->AsBranchInstr()) || this->GenerateFastBrEqLikely(instr->AsBranchInstr(), &needHelper))
                {
                    if (needHelper)
                    {
                        this->LowerBrCMem(instr, IR::HelperOp_Equal, false);
                    }
                }
                else
                {
                    if (needHelper)
                    {
                        this->LowerBrCMem(instr, IR::HelperOp_Equal, false, false /*isHelper*/);
                    }
                }
                if (!needHelper)
                {
                    instr->Remove();
                }
            }
            else
            {
                this->LowerBrCMem(instr, IR::HelperOp_Equal, true, false /*isHelper*/);
            }
            break;

        case Js::OpCode::BrGe_A:
        case Js::OpCode::BrNotGe_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
            {
                this->LowerBrCMem(instr, IR::HelperOp_GreaterEqual, false, false /*isHelper*/);
            }
            else
            {
                this->LowerBrCMem(instr, IR::HelperOp_GreaterEqual, true, false /*isHelper*/);
            }
            break;

        case Js::OpCode::BrGt_A:
        case Js::OpCode::BrNotGt_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
            {
                this->LowerBrCMem(instr, IR::HelperOp_Greater, false, false /*isHelper*/);
            }
            else
            {
                this->LowerBrCMem(instr, IR::HelperOp_Greater, true, false /*isHelper*/);
            }
            break;

        case Js::OpCode::BrLt_A:
        case Js::OpCode::BrNotLt_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
            {
                this->LowerBrCMem(instr, IR::HelperOp_Less, false, false /*isHelper*/);
            }
            else
            {
                this->LowerBrCMem(instr, IR::HelperOp_Less, true, false /*isHelper*/);
            }
            break;

        case Js::OpCode::BrLe_A:
        case Js::OpCode::BrNotLe_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
            {
                this->LowerBrCMem(instr, IR::HelperOp_LessEqual, false, false /*isHelper*/);
            }
            else
            {
                this->LowerBrCMem(instr, IR::HelperOp_LessEqual, true, false /*isHelper*/);
            }
            break;

        case Js::OpCode::BrNeq_A:
        case Js::OpCode::BrNotEq_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
            {
                bool needHelper = true;
                if (this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
                {
                    if (!fNoLower)
                    {
                        this->LowerBrCMem(instr, IR::HelperOp_NotEqual, false, false /*isHelper*/);
                    }
                }
                else if (this->TryGenerateFastBrNeq(instr))
                {
                }
                else if (m_lowererMD.GenerateFastBrString(instr->AsBranchInstr()) || this->GenerateFastBrEqLikely(instr->AsBranchInstr(), &needHelper))
                {
                    this->LowerBrCMem(instr, IR::HelperOp_NotEqual, false);
                }
                else
                {
                    this->LowerBrCMem(instr, IR::HelperOp_NotEqual, false, false /*isHelper*/);
                }
            }
            else
            {
                this->LowerBrCMem(instr, IR::HelperOp_NotEqual, true, false /*isHelper*/);
            }
            break;

        case Js::OpCode::MultiBr:
        {
            IR::MultiBranchInstr * multiBranchInstr = instr->AsBranchInstr()->AsMultiBrInstr();
            switch (multiBranchInstr->m_kind)
            {
            case IR::MultiBranchInstr::StrDictionary:
                this->GenerateSwitchStringLookup(instr);
                break;
            case IR::MultiBranchInstr::SingleCharStrJumpTable:
                this->GenerateSingleCharStrJumpTableLookup(instr);
                m_func->m_totalJumpTableSizeInBytesForSwitchStatements += (multiBranchInstr->GetBranchJumpTable()->tableSize * sizeof(void*));
                break;
            case IR::MultiBranchInstr::IntJumpTable:
                this->LowerMultiBr(instr);
                m_func->m_totalJumpTableSizeInBytesForSwitchStatements += (multiBranchInstr->GetBranchJumpTable()->tableSize * sizeof(void*));
                break;
            default:
                Assert(false);
            }
            break;
        }

        case Js::OpCode::BrSrEq_A:
        case Js::OpCode::BrSrNotNeq_A:
        {
            srcReg1 = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd() : nullptr;
            srcReg2 = instr->GetSrc2()->IsRegOpnd() ? instr->GetSrc2()->AsRegOpnd() : nullptr;

            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath && this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
            {
                if (!fNoLower)
                {
                    this->LowerBrCMem(instr, IR::HelperOp_StrictEqual, false, false /*isHelper*/);
                }
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath && this->GenerateFastBrSrEq(instr, srcReg1, srcReg2, &instrPrev, noMathFastPath))
            {
            }
            else
            {
                bool needHelper = true;
                if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
                {
                    if (m_lowererMD.GenerateFastBrString(instr->AsBranchInstr()) || this->GenerateFastBrEqLikely(instr->AsBranchInstr(), &needHelper))
                    {
                        if (needHelper)
                        {
                            this->LowerBrCMem(instr, IR::HelperOp_StrictEqual, false);
                        }
                    }
                    else
                    {
                        if (needHelper)
                        {
                            this->LowerBrCMem(instr, IR::HelperOp_StrictEqual, false, false /*isHelper*/);
                        }
                    }
                    if (!needHelper)
                    {
                        instr->Remove();
                    }
                }
                else
                {
                    this->LowerBrCMem(instr, IR::HelperOp_StrictEqual, true, false /*isHelper*/);
                }
            }
            break;
        }

        case Js::OpCode::BrSrNeq_A:
        case Js::OpCode::BrSrNotEq_A:
            if (instr->GetSrc1()->IsFloat())
            {
                Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
                m_lowererMD.LowerToFloat(instr);
            }
            else if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func) && !noMathFastPath)
            {
                bool needHelper = true;
                if (this->TryGenerateFastBrOrCmTypeOf(instr, &instrPrev, &fNoLower))
                {
                    if (!fNoLower)
                    {
                        this->LowerBrCMem(instr, IR::HelperOp_NotStrictEqual, false, false /*isHelper*/);
                    }
                }
                else if (this->GenerateFastBrSrNeq(instr, &instrPrev))
                {
                }
                else if (m_lowererMD.GenerateFastBrString(instr->AsBranchInstr()) || this->GenerateFastBrEqLikely(instr->AsBranchInstr(), &needHelper))
                {
                    if (needHelper)
                    {
                        this->LowerBrCMem(instr, IR::HelperOp_NotStrictEqual, false);
                    }
                }
                else
                {
                    if (needHelper)
                    {
                        this->LowerBrCMem(instr, IR::HelperOp_NotStrictEqual, false, false /*isHelper*/);
                    }
                }
                if (!needHelper)
                {
                    instr->Remove();
                }
            }
            else
            {
                this->LowerBrCMem(instr, IR::HelperOp_NotStrictEqual, true, false /*isHelper*/);
            }
            break;

        case Js::OpCode::BrOnEmpty:
        case Js::OpCode::BrOnNotEmpty:
            if (!PHASE_OFF(Js::BranchFastPathPhase, this->m_func))
            {
                m_lowererMD.GenerateFastBrBReturn(instr);
                this->LowerBrBReturn(instr, IR::HelperOp_OP_BrOnEmpty, true);
            }
            else
            {
                this->LowerBrBReturn(instr, IR::HelperOp_OP_BrOnEmpty, false);
            }
            break;

        case Js::OpCode::BrOnHasProperty:
        case Js::OpCode::BrOnNoProperty:
            this->LowerBrProperty(instr, IR::HelperOp_HasProperty);
            break;

        case Js::OpCode::BrOnException:
            Assert(!this->m_func->DoGlobOpt());
            instr->Remove();
            break;

        case Js::OpCode::BrOnNoException:
            instr->m_opcode = LowererMD::MDUncondBranchOpcode;
            break;

        case Js::OpCode::StSlot:
            this->LowerStSlot(instr);
            break;

        case Js::OpCode::StSlotChkUndecl:
            this->LowerStSlotChkUndecl(instr);
            break;

        case Js::OpCode::ProfiledLoopStart:
            {
                Assert(m_func->DoSimpleJitDynamicProfile());
                Assert(instr->IsJitProfilingInstr());

                // Check for the helper instr from IRBuilding (it won't be there if there are no LoopEnds due to an infinite loop)
                auto prev = instr->m_prev;
                if (prev->IsJitProfilingInstr() && prev->AsJitProfilingInstr()->isLoopHelper)
                {
                    auto saveOpnd = prev->UnlinkDst();
                    instrPrev = prev->m_prev;
                    prev->Remove();

                    const auto starFlag = GetImplicitCallFlagsOpnd();
                    IR::AutoReuseOpnd a(starFlag, m_func);
                    this->InsertMove(saveOpnd, starFlag, instr);
                    this->InsertMove(starFlag, CreateClearImplicitCallFlagsOpnd(), instr);
                }
                else
                {
#if DBG
                    // Double check that we indeed do not have a LoopEnd that is part of the same loop for the rest of the function
                    auto cur = instr;
                    auto loopNumber = instr->AsJitProfilingInstr()->loopNumber;
                    while (cur)
                    {
                        Assert(cur->m_opcode != Js::OpCode::ProfiledLoopEnd || cur->IsJitProfilingInstr() && cur->AsJitProfilingInstr()->loopNumber != loopNumber);
                        cur = cur->m_next;
                    }
#endif
                }

                // If we turned off fulljit, there's no reason to do this.
                if (!m_func->GetJnFunction()->DoFullJit())
                {
                    instr->Remove();
                }
                else
                {
                    Assert(instr->GetDst());
                    instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperSimpleGetScheduledEntryPoint, m_func));
                    m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateUint32Opnd(instr->AsJitProfilingInstr()->loopNumber, m_func));
                    m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateFramePointerOpnd(m_func));
                    this->m_lowererMD.LowerCall(instr, 0);
                }
                break;
            }
        case Js::OpCode::ProfiledLoopBodyStart:
            {
                Assert(m_func->DoSimpleJitDynamicProfile());

                const auto loopNum = instr->AsJitProfilingInstr()->loopNumber;
                Assert(loopNum < m_func->GetJnFunction()->GetLoopCount());

                auto entryPointOpnd = instr->UnlinkSrc1();
                auto dobailout = instr->UnlinkDst();
                const auto dobailoutType = TyUint8;
                Assert(dobailout->GetType() == TyUint8 && sizeof(decltype(Js::SimpleJitHelpers::IsLoopCodeGenDone(nullptr))) == 1);

                m_lowererMD.LoadHelperArgument(instr, IR::IntConstOpnd::New(0, TyUint32, m_func)); // zero indicates that we do not want to add flags back in
                m_lowererMD.LoadHelperArgument(instr, IR::IntConstOpnd::New(loopNum, TyUint32, m_func));
                m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateFramePointerOpnd(m_func));
                instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperSimpleRecordLoopImplicitCallFlags, m_func));
                m_lowererMD.LowerCall(instr, 0);

                // Outline of JITed code:
                //
                // LoopStart:
                //   entryPoint = GetScheduledEntryPoint(framePtr, loopNum)
                // LoopBodyStart:
                //   uint8 dobailout;
                //   if (entryPoint) {
                //     dobailout = IsLoopCodeGenDone(entryPoint)
                //   } else {
                //     dobailout = ++interpretCount >= threshold
                //   }
                //   // already exists from IRBuilding:
                //   if (dobailout) {
                //       Bailout
                //   }

                if (!m_func->GetJnFunction()->DoFullJit() || !m_func->GetJnFunction()->DoJITLoopBody())
                {
                    // If we're not doing fulljit, we've turned off JitLoopBodies, or if we don't have loop headers allocated (the function has a Try,  etc)
                    //      just move false to dobailout
                    this->InsertMove(dobailout, IR::IntConstOpnd::New(0, dobailoutType, m_func, true), instr->m_next);
                }
                else if (m_func->GetJnFunction()->ForceJITLoopBody())
                {
                    // If we're forcing jit loop bodies, move true to dobailout
                    this->InsertMove(dobailout, IR::IntConstOpnd::New(1, dobailoutType, m_func, true), instr->m_next);
                }
                else
                {
                    // Put in the labels
                    auto entryPointIsNull = IR::LabelInstr::New(Js::OpCode::Label, m_func);
                    auto checkDoBailout = IR::LabelInstr::New(Js::OpCode::Label, m_func);
                    instr->InsertAfter(checkDoBailout);
                    instr->InsertAfter(entryPointIsNull);

                    this->InsertCompareBranch(entryPointOpnd, IR::AddrOpnd::New(nullptr, IR::AddrOpndKindDynamicMisc, m_func), Js::OpCode::BrEq_A, false, entryPointIsNull, instr->m_next);

                    // If the entry point is not null
                    auto isCodeGenDone = IR::Instr::New(Js::OpCode::Call, dobailout, IR::HelperCallOpnd::New(IR::HelperSimpleIsLoopCodeGenDone, m_func), m_func);
                    entryPointIsNull->InsertBefore(isCodeGenDone);
                    m_lowererMD.LoadHelperArgument(isCodeGenDone, entryPointOpnd);
                    m_lowererMD.LowerCall(isCodeGenDone, 0);
                    this->InsertBranch(LowererMD::MDUncondBranchOpcode, true, checkDoBailout, entryPointIsNull);

                    // If the entry point is null
                    auto head = m_func->GetJnFunction()->GetLoopHeader(loopNum);
                    Assert(head);

                    static_assert(sizeof(head->interpretCount) == 4, "Change the type in the following line");
                    const auto type = TyUint32;

                    auto countReg = IR::RegOpnd::New(type, m_func);
                    auto countAddr = IR::MemRefOpnd::New(&head->interpretCount, type, m_func);
                    IR::AutoReuseOpnd a(countReg, m_func), b(countAddr, m_func);
                    this->InsertAdd(false, countReg, countAddr, IR::IntConstOpnd::New(1, type, m_func, true), checkDoBailout);
                    this->InsertMove(countAddr, countReg, checkDoBailout);

                    this->InsertMove(dobailout, IR::IntConstOpnd::New(0, dobailoutType, m_func, true), checkDoBailout);

                    // GetLoopInterpretCount() is a dynamic quantity. It's computed at simple-JIT time here, but that's okay
                    // because there would have been sufficient iterations in interpreted mode to get a reasonable value.
                    const auto threshold = instr->m_func->GetJnFunction()->GetLoopInterpretCount(head);

                    this->InsertCompareBranch(countReg, IR::IntConstOpnd::New(threshold, type, m_func), Js::OpCode::BrLt_A, checkDoBailout, checkDoBailout);
                    this->InsertMove(dobailout, IR::IntConstOpnd::New(1, dobailoutType, m_func, true), checkDoBailout);
                    // fallthrough

                    // Label checkDoBailout (inserted above)
                }
            }
            break;

        case Js::OpCode::ProfiledLoopEnd:
            {
                Assert(m_func->DoSimpleJitDynamicProfile());

                // This is set up in IRBuilding
                Assert(instr->GetSrc1());
                IR::Opnd* savedFlags = instr->UnlinkSrc1();

                m_lowererMD.LoadHelperArgument(instr, savedFlags);
                m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateUint32Opnd(instr->AsJitProfilingInstr()->loopNumber, m_func));
                m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateFramePointerOpnd(m_func));
                instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperSimpleRecordLoopImplicitCallFlags, m_func));
                m_lowererMD.LowerCall(instr, 0);
            }
            break;

        case Js::OpCode::InitLoopBodyCount:
            Assert(this->m_func->IsLoopBody());
            instr->SetSrc1(IR::IntConstOpnd::New(0, TyUint32, this->m_func));
            this->m_lowererMD.ChangeToAssign(instr);
            break;

        case Js::OpCode::StLoopBodyCount:
            Assert(this->m_func->IsLoopBody());
            this->LowerStLoopBodyCount(instr);
            break;

        case Js::OpCode::IncrLoopBodyCount:
            Assert(this->m_func->IsLoopBody());
            instr->m_opcode = Js::OpCode::Add_I4;
            instr->SetSrc2(IR::IntConstOpnd::New(1, TyUint32, this->m_func));
            this->m_lowererMD.EmitInt4Instr(instr);
            break;

#if !FLOATVAR
        case Js::OpCode::StSlotBoxTemp:
            this->LowerStSlotBoxTemp(instr);
            break;
#endif

        case Js::OpCode::LdSlot:
        case Js::OpCode::LdSlotArr:
        {
            Js::ProfileId profileId;
            IR::Instr *profileBeforeInstr;
            if(instr->IsJitProfilingInstr())
            {
                profileId = instr->AsJitProfilingInstr()->profileId;
                Assert(profileId != Js::Constants::NoProfileId);
                profileBeforeInstr = instr->m_next;
            }
            else
            {
                profileId = Js::Constants::NoProfileId;
                profileBeforeInstr = nullptr;
            }

            this->LowerLdSlot(instr);

            if(profileId != Js::Constants::NoProfileId)
            {
                LowerProfileLdSlot(instr->GetDst(), instr->m_func, profileId, profileBeforeInstr);
            }
            break;
        }

        case Js::OpCode::LdAsmJsSlot:
            this->LowerLdSlot(instr);
            break;

        case Js::OpCode::StAsmJsSlot:
            this->LowerStSlot(instr);
            break;

        case Js::OpCode::ChkUndecl:
            instrPrev = this->LowerChkUndecl(instr);
            break;

        case Js::OpCode::LdArrHead:
            this->LowerLdArrHead(instr);
            break;

        case Js::OpCode::StElemC:
        case Js::OpCode::StArrSegElemC:
            this->LowerStElemC(instr);
            break;

        case Js::OpCode::LdEnv:
            instrPrev = this->LowerLdEnv(instr);
            break;

        case Js::OpCode::LdAsmJsEnv:
            instrPrev = this->LowerLdAsmJsEnv(instr);
            break;

        case Js::OpCode::LdElemUndef:
            this->LowerLdElemUndef(instr);
            break;

        case Js::OpCode::LdElemUndefScoped:
            this->LowerElementUndefinedScopedMem(instr, IR::HelperOp_LdElemUndefScoped);
            break;

        case Js::OpCode::EnsureNoRootFld:
            this->LowerElementUndefined(instr, IR::HelperOp_EnsureNoRootProperty);
            break;

        case Js::OpCode::EnsureNoRootRedeclFld:
            this->LowerElementUndefined(instr, IR::HelperOp_EnsureNoRootRedeclProperty);
            break;

        case Js::OpCode::ScopedEnsureNoRedeclFld:
            this->LowerElementUndefinedScoped(instr, IR::HelperOp_EnsureNoRedeclPropertyScoped);
            break;

        case Js::OpCode::LdFuncExpr:
            // src = function Expression
            m_lowererMD.LoadFuncExpression(instr);
            this->GenerateGetCurrentFunctionObject(instr);
            break;

        case Js::OpCode::LdNewTarget:
            this->GenerateLoadNewTarget(instr);
            break;

        case Js::OpCode::ChkNewCallFlag:
            this->GenerateCheckForCallFlagNew(instr);
            break;

        case Js::OpCode::StFuncExpr:
            // object.propid = src
            LowerStFld(instr, IR::HelperOp_StFunctionExpression, IR::HelperOp_StFunctionExpression, false);
            break;

        case Js::OpCode::InitLetFld:
        case Js::OpCode::InitRootLetFld:
            LowerStFld(instr, IR::HelperOp_InitLetFld, IR::HelperOp_InitLetFld, false);
            break;

        case Js::OpCode::InitConstFld:
        case Js::OpCode::InitRootConstFld:
            LowerStFld(instr, IR::HelperOp_InitConstFld, IR::HelperOp_InitConstFld, false);
            break;

        case Js::OpCode::InitUndeclRootLetFld:
            LowerElementUndefined(instr, IR::HelperOp_InitUndeclRootLetFld);
            break;

        case Js::OpCode::InitUndeclRootConstFld:
            LowerElementUndefined(instr, IR::HelperOp_InitUndeclRootConstFld);
            break;

        case Js::OpCode::InitUndeclConsoleLetFld:
            LowerElementUndefined(instr, IR::HelperOp_InitUndeclConsoleLetFld);
            break;

        case Js::OpCode::InitUndeclConsoleConstFld:
            LowerElementUndefined(instr, IR::HelperOp_InitUndeclConsoleConstFld);
            break;

        case Js::OpCode::InitClassMember:
            LowerStFld(instr, IR::HelperOp_InitClassMember, IR::HelperOp_InitClassMember, false);
            break;

        case Js::OpCode::InitClassMemberComputedName:
            instrPrev = this->LowerStElemI(instr, Js::PropertyOperation_None, false, IR::HelperOp_InitClassMemberComputedName);
            break;

        case Js::OpCode::InitClassMemberGetComputedName:
            instrPrev = this->LowerStElemI(instr, Js::PropertyOperation_None, false, IR::HelperOp_InitClassMemberGetComputedName);
            break;

        case Js::OpCode::InitClassMemberSetComputedName:
            instrPrev = this->LowerStElemI(instr, Js::PropertyOperation_None, false, IR::HelperOp_InitClassMemberSetComputedName);
            break;

        case Js::OpCode::InitClassMemberGet:
            instrPrev = this->LowerStFld(instr, IR::HelperOp_InitClassMemberGet, IR::HelperOp_InitClassMemberGet, false);
            break;

        case Js::OpCode::InitClassMemberSet:
            instrPrev = this->LowerStFld(instr, IR::HelperOp_InitClassMemberSet, IR::HelperOp_InitClassMemberSet, false);
            break;

        case Js::OpCode::NewStackFrameDisplay:
            this->LowerLdFrameDisplay(instr, m_func->DoStackFrameDisplay());
            break;

        case Js::OpCode::LdFrameDisplay:
            this->LowerLdFrameDisplay(instr, false);
            break;

        case Js::OpCode::LdInnerFrameDisplay:
            this->LowerLdInnerFrameDisplay(instr);
            break;

        case Js::OpCode::Throw:
        case Js::OpCode::InlineThrow:
        case Js::OpCode::EHThrow:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_Throw);
            break;

        case Js::OpCode::TryCatch:
            instrPrev = this->LowerTry(instr, true /*try-catch*/);
            break;

        case Js::OpCode::TryFinally:
            instrPrev = this->LowerTry(instr, false /*try-finally*/);
            break;

        case Js::OpCode::Catch:
            instrPrev = m_lowererMD.LowerCatch(instr);
            break;

        case Js::OpCode::LeaveNull:
            instrPrev = m_lowererMD.LowerLeaveNull(instr);
            break;

        case Js::OpCode::Leave:
            if (this->m_func->HasTry() && this->m_func->DoOptimizeTryCatch())
            {
                // Required in Register Allocator to mark region boundaries
                break;
            }
            instrPrev = m_lowererMD.LowerLeave(instr, instr->AsBranchInstr()->GetTarget(), false /*fromFinalLower*/, instr->AsBranchInstr()->m_isOrphanedLeave);
            break;

        case Js::OpCode::BailOnException:
            instrPrev = this->LowerBailOnException(instr);
            break;

        case Js::OpCode::RuntimeTypeError:
        case Js::OpCode::InlineRuntimeTypeError:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_RuntimeTypeError);
            break;

        case Js::OpCode::RuntimeReferenceError:
        case Js::OpCode::InlineRuntimeReferenceError:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_RuntimeReferenceError);
            break;

        case Js::OpCode::Break:
            // Inline breakpoint: for now do nothing.
            break;

        case Js::OpCode::Nop:
            // This may need support for debugging the JIT, but for now just remove the instruction.
            instr->Remove();
            break;

        case Js::OpCode::Unused:
            // Currently Unused is used with ScopedLdInst to keep the second dst alive, but we don't need to lower it.
            instr->Remove();
            break;

        case Js::OpCode::StatementBoundary:
            // This instruction is merely to help convey source info through the IR
            // and eventually generate the nativeOffset maps.
            break;

        case Js::OpCode::BailOnNotPolymorphicInlinee:
            instrPrev = LowerBailOnNotPolymorphicInlinee(instr);
            break;

        case Js::OpCode::BailOnNoSimdTypeSpec:
        case Js::OpCode::BailOnNoProfile:
            this->GenerateBailOut(instr, nullptr, nullptr);
            break;

        case Js::OpCode::BailOnNotSpreadable:
            instrPrev = this->LowerBailOnNotSpreadable(instr);
            break;

        case Js::OpCode::BailOnNotStackArgs:
            instrPrev = this->LowerBailOnNotStackArgs(instr);
            break;

        case Js::OpCode::BailOnEqual:
        case Js::OpCode::BailOnNotEqual:
            instrPrev = this->LowerBailOnEqualOrNotEqual(instr);
            break;

        case Js::OpCode::BailOnNegative:
            LowerBailOnNegative(instr);
            break;

        case Js::OpCode::BailForDebugger:
            instrPrev = this->LowerBailForDebugger(instr);
            break;

        case Js::OpCode::BailOnNotObject:
            instrPrev = this->LowerBailOnNotObject(instr);
            break;

        case Js::OpCode::BailOnNotBuiltIn:
            instrPrev = this->LowerBailOnNotBuiltIn(instr);
            break;

        case Js::OpCode::BailOnNotArray:
        {
            IR::Instr *bailOnNotArray, *bailOnMissingValue;
            SplitBailOnNotArray(instr, &bailOnNotArray, &bailOnMissingValue);
            IR::RegOpnd *const arrayOpnd = LowerBailOnNotArray(bailOnNotArray);
            if(bailOnMissingValue)
            {
                LowerBailOnMissingValue(bailOnMissingValue, arrayOpnd);
            }
            break;
        }

        case Js::OpCode::BoundCheck:
        case Js::OpCode::UnsignedBoundCheck:
            LowerBoundCheck(instr);
            break;

        case Js::OpCode::BailTarget:
            instrPrev = this->LowerBailTarget(instr);
            break;

        case Js::OpCode::InlineeStart:
            this->LowerInlineeStart(instr);
            break;

        case Js::OpCode::EndCallForPolymorphicInlinee:
            instr->Remove();
            break;

        case Js::OpCode::InlineeEnd:
            this->LowerInlineeEnd(instr);
            break;

        case Js::OpCode::InlineBuiltInEnd:
        case Js::OpCode::InlineNonTrackingBuiltInEnd:
            this->LowerInlineBuiltIn(instr);
            break;

        case Js::OpCode::ExtendArg_A:
            if (instr->GetSrc1()->IsRegOpnd())
            {
                IR::RegOpnd *src1 = instr->GetSrc1()->AsRegOpnd();
                this->addToLiveOnBackEdgeSyms->Clear(src1->m_sym->m_id);
            }
            instr->Remove();
            break;

        case Js::OpCode::InlineBuiltInStart:
        case Js::OpCode::BytecodeArgOutUse:
        case Js::OpCode::ArgOut_A_InlineBuiltIn:
            instr->Remove();
            break;

        case Js::OpCode::DeadBrEqual:
            this->LowerBinaryHelperMem(instr, IR::HelperOp_Equal);
            break;

        case Js::OpCode::DeadBrSrEqual:
            this->LowerBinaryHelperMem(instr, IR::HelperOp_StrictEqual);
            break;

        case Js::OpCode::DeadBrRelational:
            this->LowerBinaryHelperMem(instr, IR::HelperOp_Greater);
            break;

        case Js::OpCode::DeadBrOnHasProperty:
            this->LowerUnaryHelperMem(instr, IR::HelperOp_HasProperty);
            break;

        case Js::OpCode::DeletedNonHelperBranch:
            break;

        case Js::OpCode::InitClass:
            instrPrev = this->LowerInitClass(instr);
            break;

        case Js::OpCode::NewConcatStrMulti:
            this->LowerNewConcatStrMulti(instr);
            break;

        case Js::OpCode::NewConcatStrMultiBE:
            this->LowerNewConcatStrMultiBE(instr);
            break;

        case Js::OpCode::SetConcatStrMultiItem:
            this->LowerSetConcatStrMultiItem(instr);
            break;

        case Js::OpCode::SetConcatStrMultiItemBE:
            Assert(instr->GetSrc1()->IsRegOpnd());
            this->addToLiveOnBackEdgeSyms->Clear(instr->GetSrc1()->GetStackSym()->m_id);
            // code corresponding to it should already have been generated while lowering NewConcatStrMultiBE
            instr->Remove();
            break;

        case Js::OpCode::Conv_Str:
            this->LowerConvStr(instr);
            break;

        case Js::OpCode::Coerse_Str:
            this->LowerCoerseStr(instr);
            break;

        case Js::OpCode::Coerse_StrOrRegex:
            this->LowerCoerseStrOrRegex(instr);
            break;

        case Js::OpCode::Coerse_Regex:
            this->LowerCoerseRegex(instr);
            break;

        case Js::OpCode::Conv_PrimStr:
            this->LowerConvPrimStr(instr);
            break;

        case Js::OpCode::ObjectFreeze:
            this->LowerUnaryHelper(instr, IR::HelperOP_Freeze);
            break;

        case Js::OpCode::ClearAttributes:
            this->LowerBinaryHelper(instr, IR::HelperOP_ClearAttributes);
            break;

        case Js::OpCode::SpreadArrayLiteral:
            this->LowerSpreadArrayLiteral(instr);
            break;

        case Js::OpCode::CallIExtended:
        {
            // Currently, the only use for CallIExtended is a call that uses spread.
            Assert(IsSpreadCall(instr));
            instrPrev = this->LowerSpreadCall(instr, Js::CallFlags_None);
            break;
        }

        case Js::OpCode::CallIExtendedNew:
        {
            // Currently, the only use for CallIExtended is a call that uses spread.
            Assert(IsSpreadCall(instr));
            instrPrev = this->LowerSpreadCall(instr, Js::CallFlags_New);
            break;
        }

        case Js::OpCode::CallIExtendedNewTargetNew:
        {
            // Currently, the only use for CallIExtended is a call that uses spread.
            Assert(IsSpreadCall(instr));
            instrPrev = this->LowerSpreadCall(instr, (Js::CallFlags)(Js::CallFlags_New | Js::CallFlags_ExtraArg | Js::CallFlags_NewTarget));
            break;
        }

        case Js::OpCode::LdSpreadIndices:
            instr->Remove();
            break;

        case Js::OpCode::LdSuper:
            instrPrev = m_lowererMD.LowerLdSuper(instr, IR::HelperLdSuper);
            break;

        case Js::OpCode::LdSuperCtor:
            instrPrev = m_lowererMD.LowerLdSuper(instr, IR::HelperLdSuperCtor);
            break;

        case Js::OpCode::ScopedLdSuper:
            instrPrev = m_lowererMD.LowerLdSuper(instr, IR::HelperScopedLdSuper);
            break;

        case Js::OpCode::ScopedLdSuperCtor:
            instrPrev = m_lowererMD.LowerLdSuper(instr, IR::HelperScopedLdSuperCtor);
            break;

        case Js::OpCode::SetHomeObj:
        {
            IR::Opnd *src2Opnd = instr->UnlinkSrc2();
            IR::Opnd *src1Opnd = instr->UnlinkSrc1();

            m_lowererMD.LoadHelperArgument(instr, src2Opnd);
            m_lowererMD.LoadHelperArgument(instr, src1Opnd);

            m_lowererMD.ChangeToHelperCall(instr, IR::HelperSetHomeObj);

            break;
        }

        case Js::OpCode::SetComputedNameVar:
        {
            IR::Opnd *src2Opnd = instr->UnlinkSrc2();
            IR::Opnd *src1Opnd = instr->UnlinkSrc1();

            m_lowererMD.LoadHelperArgument(instr, src2Opnd);
            m_lowererMD.LoadHelperArgument(instr, src1Opnd);

            m_lowererMD.ChangeToHelperCall(instr, IR::HelperSetComputedNameVar);

            break;
        }

        case Js::OpCode::InlineeMetaArg:
        {
            m_lowererMD.ChangeToAssign(instr);
            break;
        }

        case Js::OpCode::Yield:
        {
            instr->FreeSrc1(); // Source is not actually used by the backend other than to calculate lifetime
            IR::Opnd* dstOpnd = instr->UnlinkDst();

            // prm2 is the ResumeYieldData pointer per calling convention established in JavascriptGenerator::CallGenerator
            // This is the value the bytecode expects to be in the dst register of the Yield opcode after resumption.
            // Load it here after the bail-in.

            StackSym *resumeYieldDataSym = StackSym::NewParamSlotSym(2, m_func);
            m_func->SetArgOffset(resumeYieldDataSym, (LowererMD::GetFormalParamOffset() + 1) * MachPtr);
            IR::SymOpnd * resumeYieldDataOpnd = IR::SymOpnd::New(resumeYieldDataSym, TyMachPtr, m_func);

            AssertMsg(instr->m_next->IsLabelInstr(), "Expect the resume label to immediately follow Yield instruction");
            m_lowererMD.CreateAssign(dstOpnd, resumeYieldDataOpnd, instr->m_next->m_next);

            GenerateBailOut(instr);

            break;
        }

        case Js::OpCode::ResumeYield:
        case Js::OpCode::ResumeYieldStar:
        {
            IR::Opnd *srcOpnd1 = instr->UnlinkSrc1();
            IR::Opnd *srcOpnd2 = instr->m_opcode == Js::OpCode::ResumeYieldStar ? instr->UnlinkSrc2() : IR::AddrOpnd::NewNull(m_func);
            m_lowererMD.LoadHelperArgument(instr, srcOpnd2);
            m_lowererMD.LoadHelperArgument(instr, srcOpnd1);
            m_lowererMD.ChangeToHelperCall(instr, IR::HelperResumeYield);
            break;
        }

        case Js::OpCode::GeneratorResumeJumpTable:
        {
            // Lowered in LowerPrologEpilog so that the jumps introduced are not considered to be part of the flow for the RegAlloc phase.

            // Introduce a BailOutNoSave label if there were yield points that were elided due to optimizations.  They could still be hit
            // if an active generator object had been paused at such a yield point when the function body was JITed.  So safe guard such a
            // case by having the native code simply jump back to the interpreter for such yield points.

            IR::LabelInstr *bailOutNoSaveLabel = nullptr;

            m_func->MapUntilYieldOffsetResumeLabels([this, &bailOutNoSaveLabel](int, const YieldOffsetResumeLabel& yorl)
            {
                if (yorl.Second() == nullptr)
                {
                    if (bailOutNoSaveLabel == nullptr)
                    {
                        bailOutNoSaveLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
                    }

                    return true;
                }

                return false;
            });

            // Insert the bailoutnosave label somewhere along with a call to BailOutNoSave helper
            if (bailOutNoSaveLabel != nullptr)
            {
                IR::Instr * exitPrevInstr = this->m_func->m_exitInstr->m_prev;
                IR::LabelInstr * exitTargetInstr;
                if (exitPrevInstr->IsLabelInstr())
                {
                    exitTargetInstr = exitPrevInstr->AsLabelInstr();
                    exitPrevInstr = exitPrevInstr->m_prev;
                }
                else
                {
                    exitTargetInstr = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, false);
                    exitPrevInstr->InsertAfter(exitTargetInstr);
                }

                bailOutNoSaveLabel->m_hasNonBranchRef = true;
                bailOutNoSaveLabel->isOpHelper = true;

                IR::Instr* bailOutCall = IR::Instr::New(Js::OpCode::Call, m_func);

                exitPrevInstr->InsertAfter(bailOutCall);
                exitPrevInstr->InsertAfter(bailOutNoSaveLabel);
                exitPrevInstr->InsertAfter(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, exitTargetInstr, m_func));

                IR::RegOpnd * frameRegOpnd = IR::RegOpnd::New(nullptr, LowererMD::GetRegFramePointer(), TyMachPtr, m_func);

                m_lowererMD.LoadHelperArgument(bailOutCall, frameRegOpnd);
                m_lowererMD.ChangeToHelperCall(bailOutCall, IR::HelperNoSaveRegistersBailOutForElidedYield);

                m_func->m_bailOutNoSaveLabel = bailOutNoSaveLabel;
            }

            break;
        }

        case Js::OpCode::AsyncSpawn:
            this->LowerBinaryHelperMem(instr, IR::HelperAsyncSpawn);
            break;

        case Js::OpCode::FrameDisplayCheck:
            instrPrev = this->LowerFrameDisplayCheck(instr);
            break;

        case Js::OpCode::SlotArrayCheck:
            instrPrev = this->LowerSlotArrayCheck(instr);
            break;

        default:
#if defined(_M_IX86) || defined(_M_X64)
            if (IsSimd128Opcode(instr->m_opcode))
            {
                instrPrev = m_lowererMD.Simd128Instruction(instr);
                break;
            }
#endif
            AssertMsg(instr->IsLowered(), "Unknown opcode");
            if(!instr->IsLowered())
            {
                Fatal();
            }
            break;
        }

#if DBG
        LegalizeVerifyRange(instrPrev ? instrPrev->m_next : instrStart,
            verifyLegalizeInstrNext ? verifyLegalizeInstrNext->m_prev : nullptr);
#endif
    } NEXT_INSTR_BACKWARD_EDITING_IN_RANGE;

    Assert(this->outerMostLoopLabel == nullptr);
}

IR::Instr *
Lowerer::LoadFunctionBody(IR::Instr * instr)
{
    return m_lowererMD.LoadHelperArgument(instr, LoadFunctionBodyOpnd(instr));
}

IR::Instr *
Lowerer::LoadScriptContext(IR::Instr * instr)
{
    return m_lowererMD.LoadHelperArgument(instr, LoadScriptContextOpnd(instr));
}

IR::Opnd *
Lowerer::LoadFunctionBodyOpnd(IR::Instr * instr)
{
    return IR::AddrOpnd::New(instr->m_func->GetJnFunction(), IR::AddrOpndKindDynamicFunctionBody, instr->m_func);
}

IR::Opnd *
Lowerer::LoadScriptContextOpnd(IR::Instr * instr)
{
    return IR::AddrOpnd::New(this->m_func->GetScriptContext(), IR::AddrOpndKindDynamicScriptContext, this->m_func);
}

IR::Opnd *
Lowerer::LoadScriptContextValueOpnd(IR::Instr * instr, ScriptContextValue valueType)
{
    Js::ScriptContext *scriptContext = instr->m_func->GetScriptContext();
    switch (valueType)
    {
    case ScriptContextValue::ScriptContextNumberAllocator:
        return IR::AddrOpnd::New(scriptContext->GetNumberAllocator(), IR::AddrOpndKindDynamicMisc, instr->m_func);
    case ScriptContextValue::ScriptContextRecycler:
        return IR::AddrOpnd::New(scriptContext->GetRecycler(), IR::AddrOpndKindDynamicMisc, instr->m_func);
    default:
        Assert(false);
        return nullptr;
    }

}

IR::Opnd *
Lowerer::LoadLibraryValueOpnd(IR::Instr * instr, LibraryValue valueType, RegNum regNum)
{
    Js::ScriptContext *scriptContext = instr->m_func->GetScriptContext();
    switch (valueType)
    {
    case LibraryValue::ValueEmptyString:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetEmptyString(), IR::AddrOpndKindDynamicVar, instr->m_func, true);
    case LibraryValue::ValueUndeclBlockVar:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetUndeclBlockVar(), IR::AddrOpndKindDynamicVar, instr->m_func, true);
    case LibraryValue::ValueUndefined:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetUndefined(), IR::AddrOpndKindDynamicVar, instr->m_func, true);
    case LibraryValue::ValueNull:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetNull(), IR::AddrOpndKindDynamicVar, instr->m_func, true);
    case LibraryValue::ValueTrue:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetTrue(), IR::AddrOpndKindDynamicVar, instr->m_func, true);
    case LibraryValue::ValueFalse:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetFalse(), IR::AddrOpndKindDynamicVar, instr->m_func, true);
    case LibraryValue::ValueNegativeZero:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetNegativeZero(), IR::AddrOpndKindDynamicVar, instr->m_func, true);
    case LibraryValue::ValueNumberTypeStatic:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetNumberTypeStatic(), IR::AddrOpndKindDynamicType, instr->m_func, true);
    case LibraryValue::ValueStringTypeStatic:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetStringTypeStatic(), IR::AddrOpndKindDynamicType, instr->m_func, true);
    case LibraryValue::ValueObjectType:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetObjectType(), IR::AddrOpndKindDynamicType, instr->m_func);
    case LibraryValue::ValueObjectHeaderInlinedType:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetObjectHeaderInlinedType(), IR::AddrOpndKindDynamicType, instr->m_func);
    case LibraryValue::ValueRegexType:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetRegexType(), IR::AddrOpndKindDynamicType, instr->m_func);
    case LibraryValue::ValueArrayConstructor:
        return IR::AddrOpnd::New(scriptContext->GetLibrary()->GetArrayConstructor(), IR::AddrOpndKindDynamicVar, instr->m_func);
    case LibraryValue::ValueJavascriptArrayType:
        return IR::AddrOpnd::New(Js::JavascriptArray::GetInitialType(scriptContext), IR::AddrOpndKindDynamicType, instr->m_func);
    case LibraryValue::ValueNativeIntArrayType:
        return IR::AddrOpnd::New(Js::JavascriptNativeIntArray::GetInitialType(scriptContext), IR::AddrOpndKindDynamicType, instr->m_func);
    case LibraryValue::ValueNativeFloatArrayType:
        return IR::AddrOpnd::New(Js::JavascriptNativeFloatArray::GetInitialType(scriptContext), IR::AddrOpndKindDynamicType, instr->m_func);
    case LibraryValue::ValueConstructorCacheDefaultInstance:
        return IR::AddrOpnd::New(&Js::ConstructorCache::DefaultInstance, IR::AddrOpndKindDynamicMisc, instr->m_func);
    case LibraryValue::ValueAbsDoubleCst:
        return IR::MemRefOpnd::New((void*)&Js::JavascriptNumber::AbsDoubleCst, TyMachDouble, instr->m_func, IR::AddrOpndKindDynamicDoubleRef);
    case LibraryValue::ValueCharStringCache:
        return IR::AddrOpnd::New((Js::Var)&scriptContext->GetLibrary()->GetCharStringCache(), IR::AddrOpndKindDynamicCharStringCache, instr->m_func);
    default:
        Assert(false);
        return nullptr;
    }
}

IR::Opnd *
Lowerer::LoadVTableValueOpnd(IR::Instr * instr, VTableValue vtableType)
{
    return IR::AddrOpnd::New((Js::Var)instr->m_func->GetScriptContext()->GetLibrary()->GetVTableAddresses()[vtableType], IR::AddrOpndKindDynamicVtable, this->m_func);
}

IR::Opnd *
Lowerer::LoadOptimizationOverridesValueOpnd(IR::Instr *instr, OptimizationOverridesValue valueType)
{
    Js::ScriptContext *scriptContext = instr->m_func->GetScriptContext();
    switch (valueType)
    {
    case OptimizationOverridesValue::OptimizationOverridesSideEffects:
        return IR::MemRefOpnd::New(scriptContext->optimizationOverrides.GetAddressOfSideEffects(), TyInt32, instr->m_func);
    case OptimizationOverridesValue::OptimizationOverridesArraySetElementFastPathVtable:
        return IR::MemRefOpnd::New(scriptContext->optimizationOverrides.GetAddressOfArraySetElementFastPathVtable(), TyMachPtr, instr->m_func);
    case OptimizationOverridesValue::OptimizationOverridesIntArraySetElementFastPathVtable:
        return IR::MemRefOpnd::New(scriptContext->optimizationOverrides.GetAddressOfIntArraySetElementFastPathVtable(), TyMachPtr, instr->m_func);
    case OptimizationOverridesValue::OptimizationOverridesFloatArraySetElementFastPathVtable:
        return IR::MemRefOpnd::New(scriptContext->optimizationOverrides.GetAddressOfFloatArraySetElementFastPathVtable(), TyMachPtr, instr->m_func);
    default:
        Assert(false);
        return nullptr;
    }
}

IR::Opnd *
Lowerer::LoadNumberAllocatorValueOpnd(IR::Instr *instr, NumberAllocatorValue valueType)
{
    Js::ScriptContext *scriptContext = instr->m_func->GetScriptContext();
    bool allowNativeCodeBumpAllocation = scriptContext->GetNumberAllocator()->AllowNativeCodeBumpAllocation();

    switch (valueType)
    {
    case NumberAllocatorValue::NumberAllocatorEndAddress:
        return IR::MemRefOpnd::New(((char *)scriptContext->GetNumberAllocator()) + Js::RecyclerJavascriptNumberAllocator::GetEndAddressOffset(), TyMachPtr, instr->m_func);
    case NumberAllocatorValue::NumberAllocatorFreeObjectList:
        return IR::MemRefOpnd::New(
            ((char *)scriptContext->GetNumberAllocator()) +
                (allowNativeCodeBumpAllocation ? Js::RecyclerJavascriptNumberAllocator::GetFreeObjectListOffset() : Js::RecyclerJavascriptNumberAllocator::GetEndAddressOffset()),
            TyMachPtr, instr->m_func);
    default:
        Assert(false);
        return nullptr;
    }
}

IR::Opnd *
Lowerer::LoadIsInstInlineCacheOpnd(IR::Instr * instr, uint inlineCacheIndex)
{
    Js::IsInstInlineCache * inlineCache = instr->m_func->GetJnFunction()->GetIsInstInlineCache(inlineCacheIndex);
    return IR::AddrOpnd::New(inlineCache,  IR::AddrOpndKindDynamicInlineCache, this->m_func);
}

IR::Opnd *
Lowerer::LoadRuntimeInlineCacheOpnd(IR::Instr * instr, IR::PropertySymOpnd * propertySymOpnd, bool isHelper)
{
    Assert(propertySymOpnd->m_runtimeInlineCache != nullptr);
    IR::Opnd * inlineCacheOpnd = nullptr;
    if (instr->m_func->GetJnFunction()->GetInlineCachesOnFunctionObject() && !instr->m_func->IsInlinee())
    {
        inlineCacheOpnd = this->GetInlineCacheFromFuncObjectForRuntimeUse(instr, propertySymOpnd, isHelper);
    }
    else
    {
        Js::InlineCache * inlineCache = propertySymOpnd->m_runtimeInlineCache;
        inlineCacheOpnd = IR::AddrOpnd::New(inlineCache, IR::AddrOpndKindDynamicInlineCache, this->m_func, /* dontEncode */ true);
    }
    return inlineCacheOpnd;
}

bool
Lowerer::TryGenerateFastCmSrEq(IR::Instr * instr)
{
    IR::RegOpnd *srcReg1 = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd() : nullptr;
    IR::RegOpnd *srcReg2 = instr->GetSrc2()->IsRegOpnd() ? instr->GetSrc2()->AsRegOpnd() : nullptr;

    if (srcReg2 && IsConstRegOpnd(srcReg2))
    {
        return m_lowererMD.GenerateFastCmSrEqConst(instr);
    }
    else if (srcReg1 && IsConstRegOpnd(srcReg1))
    {
        instr->SwapOpnds();
        return m_lowererMD.GenerateFastCmSrEqConst(instr);
    }
    else if (srcReg2 && (srcReg2->m_sym->m_isStrConst))
    {
        this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrEq_String);
        return true;
    }
    else if (srcReg1 && (srcReg1->m_sym->m_isStrConst))
    {
        instr->SwapOpnds();
        this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrEq_String);
        return true;
    }
    else if (srcReg2 && (srcReg2->m_sym->m_isStrEmpty))
    {
        this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrEq_EmptyString);
        return true;
    }
    else if (srcReg1 && (srcReg1->m_sym->m_isStrEmpty))
    {
        instr->SwapOpnds();
        this->LowerBinaryHelperMem(instr, IR::HelperOP_CmSrEq_EmptyString);
        return true;
    }

    return false;
}

bool
Lowerer::GenerateFastBrSrEq(IR::Instr * instr, IR::RegOpnd * srcReg1, IR::RegOpnd * srcReg2, IR::Instr ** pInstrPrev, bool noMathFastPath)
{
    if (srcReg2 && IsConstRegOpnd(srcReg2))
    {
        this->GenerateFastBrConst(instr->AsBranchInstr(), srcReg2->m_sym->GetConstOpnd(), true);
        instr->Remove();
        return true;
    }
    else if (srcReg1 && IsConstRegOpnd(srcReg1))
    {
        instr->SwapOpnds();
        this->GenerateFastBrConst(instr->AsBranchInstr(), srcReg1->m_sym->GetConstOpnd(), true);
        instr->Remove();
        return true;
    }
    else if (srcReg2 && (srcReg2->m_sym->m_isStrConst))
    {
        this->LowerBrCMem(instr, IR::HelperOp_StrictEqualString, noMathFastPath, false);
        return true;
    }
    else if (srcReg1 && (srcReg1->m_sym->m_isStrConst))
    {
        instr->SwapOpnds();
        this->LowerBrCMem(instr, IR::HelperOp_StrictEqualString, noMathFastPath, false);
        return true;
    }
    else if (srcReg2 && (srcReg2->m_sym->m_isStrEmpty))
    {
        this->LowerBrCMem(instr, IR::HelperOp_StrictEqualEmptyString, noMathFastPath, false);
        return true;
    }
    else if (srcReg1 && (srcReg1->m_sym->m_isStrConst))
    {
        instr->SwapOpnds();
        this->LowerBrCMem(instr, IR::HelperOp_StrictEqualEmptyString, noMathFastPath, false);
        return true;
    }

    return false;
}

///----------------------------------------------------------------------------
///
/// Lowerer::GenerateFastBrConst
///
///----------------------------------------------------------------------------
IR::BranchInstr *
Lowerer::GenerateFastBrConst(IR::BranchInstr *branchInstr, IR::Opnd * constOpnd, bool isEqual)
{
    Assert(constOpnd->IsAddrOpnd() || constOpnd->IsIntConstOpnd());

    //
    // Given:
    // BrSrEq_A $L1, s1, s2
    // where s2 is either 'null', 'undefined', 'true' or 'false'
    //
    // Generate:
    //
    // CMP s1, s2
    // JEQ/JNE $L1
    //

    Assert(this->IsConstRegOpnd(branchInstr->GetSrc2()->AsRegOpnd()));

    IR::Opnd    *opnd    = branchInstr->GetSrc1();

    if (!opnd->IsRegOpnd())
    {
        IR::RegOpnd *lhsReg = IR::RegOpnd::New(TyVar, m_func);
        LowererMD::CreateAssign(lhsReg, opnd, branchInstr);

        opnd = lhsReg;
    }

    Assert(opnd->IsRegOpnd());

    IR::BranchInstr *newBranch;
    newBranch = InsertCompareBranch(opnd, constOpnd, isEqual ? Js::OpCode::BrEq_A : Js::OpCode::BrNeq_A, branchInstr->GetTarget(), branchInstr);

    return newBranch;
}

bool
Lowerer::TryGenerateFastBrEq(IR::Instr * instr)
{
    IR::RegOpnd *srcReg1 = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd() : nullptr;
    IR::RegOpnd *srcReg2 = instr->GetSrc2()->IsRegOpnd() ? instr->GetSrc2()->AsRegOpnd() : nullptr;

    bool isConst = false;
    if (srcReg1 && this->IsNullOrUndefRegOpnd(srcReg1))
    {
        instr->SwapOpnds();
        isConst = true;
    }

    // Fast path for == null or == undefined
    // if (src == null || src == undefined)
    if (isConst || srcReg2 && this->IsNullOrUndefRegOpnd(srcReg2))
    {
        IR::BranchInstr *newBranch;
        newBranch = this->GenerateFastBrConst(instr->AsBranchInstr(),
            this->LoadLibraryValueOpnd(instr, LibraryValue::ValueNull),
            true);

        this->GenerateFastBrConst(instr->AsBranchInstr(),
            this->LoadLibraryValueOpnd(instr, LibraryValue::ValueUndefined),
            true);

        instr->Remove();
        return true;
    }

    return false;
}

bool
Lowerer::TryGenerateFastBrNeq(IR::Instr * instr)
{
    IR::RegOpnd *srcReg1 = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd() : nullptr;
    IR::RegOpnd *srcReg2 = instr->GetSrc2()->IsRegOpnd() ? instr->GetSrc2()->AsRegOpnd() : nullptr;

    bool isConst = false;
    if (srcReg1 && this->IsNullOrUndefRegOpnd(srcReg1))
    {
        instr->SwapOpnds();
        isConst = true;
    }

    // Fast path for != null or != undefined
    //      if (src != null && src != undefined)
    //
    // That is:
    //      if (src == NULL) goto labelEq
    //      if (src != undef) goto target
    //    labelEq:

    if (isConst || (srcReg2 && this->IsNullOrUndefRegOpnd(srcReg2)))
    {
        IR::LabelInstr *labelEq = instr->GetOrCreateContinueLabel();

        IR::BranchInstr *newBranch;
        newBranch = this->GenerateFastBrConst(instr->AsBranchInstr(),
            this->LoadLibraryValueOpnd(instr, LibraryValue::ValueNull),
            true);
        newBranch->AsBranchInstr()->SetTarget(labelEq);

        this->GenerateFastBrConst(instr->AsBranchInstr(),
            this->LoadLibraryValueOpnd(instr, LibraryValue::ValueUndefined),
            false);

        instr->Remove();
        return true;
    }

    return false;
}

bool
Lowerer::GenerateFastBrSrNeq(IR::Instr * instr, IR::Instr ** pInstrPrev)
{
    IR::RegOpnd *srcReg1 = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd() : nullptr;
    IR::RegOpnd *srcReg2 = instr->GetSrc2()->IsRegOpnd() ? instr->GetSrc2()->AsRegOpnd() : nullptr;

    if (srcReg2 && IsConstRegOpnd(srcReg2))
    {
        this->GenerateFastBrConst(instr->AsBranchInstr(), srcReg2->m_sym->GetConstOpnd(), false);
        instr->Remove();
        return true;
    }
    else if (srcReg1 && IsConstRegOpnd(srcReg1))
    {
        instr->SwapOpnds();
        this->GenerateFastBrConst(instr->AsBranchInstr(), srcReg1->m_sym->GetConstOpnd(), false);
        instr->Remove();
        return true;
    }

    return false;
}

void
Lowerer::GenerateDynamicObjectAlloc(IR::Instr * newObjInstr, uint inlineSlotCount, uint slotCount, IR::RegOpnd * newObjDst, IR::Opnd * typeSrc)
{
    size_t headerAllocSize = sizeof(Js::DynamicObject) + inlineSlotCount * sizeof(Js::Var);
    IR::SymOpnd * tempObjectSymOpnd;
    bool isZeroed = GenerateRecyclerOrMarkTempAlloc(newObjInstr, newObjDst, IR::HelperAllocMemForScObject, headerAllocSize, &tempObjectSymOpnd);

    if (tempObjectSymOpnd && !PHASE_OFF(Js::HoistMarkTempInitPhase, this->m_func) && this->outerMostLoopLabel)
    {
        // Hoist the vtable init to the outer most loop top as it never changes
        InsertMove(tempObjectSymOpnd,
            LoadVTableValueOpnd(this->outerMostLoopLabel, VTableValue::VtableDynamicObject), this->outerMostLoopLabel, false);
    }
    else
    {
        // MOV [newObjDst + offset(vtable)], DynamicObject::vtable
        GenerateMemInit(newObjDst, 0, LoadVTableValueOpnd(newObjInstr, VTableValue::VtableDynamicObject), newObjInstr, isZeroed);
    }
    // MOV [newObjDst + offset(type)], newObjectType
    GenerateMemInit(newObjDst, Js::DynamicObject::GetOffsetOfType(), typeSrc, newObjInstr, isZeroed);

    // CALL JavascriptOperators::AllocMemForVarArray((slotCount - inlineSlotCount) * sizeof(Js::Var))
    if (slotCount > inlineSlotCount)
    {
        size_t auxSlotsAllocSize = (slotCount - inlineSlotCount) * sizeof(Js::Var);
        IR::RegOpnd* auxSlots = IR::RegOpnd::New(TyMachPtr, m_func);

        GenerateRecyclerAllocAligned(IR::HelperAllocMemForVarArray, auxSlotsAllocSize, auxSlots, newObjInstr);
        GenerateMemInit(newObjDst, Js::DynamicObject::GetOffsetOfAuxSlots(), auxSlots, newObjInstr, isZeroed);

        IR::IndirOpnd* newObjAuxSlots = IR::IndirOpnd::New(newObjDst, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachPtr, m_func);
        this->m_lowererMD.CreateAssign(newObjAuxSlots, auxSlots, newObjInstr);
    }
    else
    {
        GenerateMemInitNull(newObjDst, Js::DynamicObject::GetOffsetOfAuxSlots(), newObjInstr, isZeroed);
    }

    GenerateMemInitNull(newObjDst, Js::DynamicObject::GetOffsetOfObjectArray(), newObjInstr, isZeroed);
}

void
Lowerer::LowerNewScObjectSimple(IR::Instr * instr)
{
    GenerateDynamicObjectAlloc(
        instr,
        0,
        0,
        instr->UnlinkDst()->AsRegOpnd(),
        LoadLibraryValueOpnd(
            instr,
            Js::FunctionBody::DoObjectHeaderInliningForEmptyObjects()
                ? LibraryValue::ValueObjectHeaderInlinedType
                : LibraryValue::ValueObjectType));
    instr->Remove();
}

void
Lowerer::LowerNewScObjectLiteral(IR::Instr *newObjInstr)
{
    Func * func = m_func;
    IR::IntConstOpnd * literalObjectIdOpnd = newObjInstr->UnlinkSrc2()->AsIntConstOpnd();
    Js::DynamicType ** literalTypeRef = newObjInstr->m_func->GetJnFunction()->GetObjectLiteralTypeRef(literalObjectIdOpnd->AsUint32());
    Js::DynamicType * literalType = *literalTypeRef;

    IR::LabelInstr * helperLabel = nullptr;
    IR::LabelInstr * allocLabel = nullptr;
    IR::Opnd * literalTypeRefOpnd;
    IR::Opnd * literalTypeOpnd;
    IR::Opnd * propertyArrayOpnd;

    IR::IntConstOpnd * propertyArrayIdOpnd = newObjInstr->UnlinkSrc1()->AsIntConstOpnd();
    const Js::PropertyIdArray * propIds = Js::ByteCodeReader::ReadPropertyIdArray(propertyArrayIdOpnd->AsUint32(), newObjInstr->m_func->GetJnFunction());
    Js::ScriptContext *const scriptContext = newObjInstr->m_func->GetJnFunction()->GetScriptContext();
    uint inlineSlotCapacity = Js::JavascriptOperators::GetLiteralInlineSlotCapacity(propIds, scriptContext);
    uint slotCapacity = Js::JavascriptOperators::GetLiteralSlotCapacity(propIds, scriptContext);
    IR::RegOpnd * dstOpnd;

    literalTypeRefOpnd = IR::AddrOpnd::New(literalTypeRef, IR::AddrOpndKindDynamicMisc, this->m_func);
    propertyArrayOpnd = IR::AddrOpnd::New((Js::Var)propIds, IR::AddrOpndKindDynamicMisc, this->m_func);

    if (literalType == nullptr || !literalType->GetIsShared())
    {
        helperLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        allocLabel = IR::LabelInstr::New(Js::OpCode::Label, func);

        literalTypeOpnd = IR::RegOpnd::New(TyMachPtr, func);
        InsertMove(literalTypeOpnd, IR::MemRefOpnd::New(literalTypeRef, TyMachPtr, func), newObjInstr);
        InsertTestBranch(literalTypeOpnd, literalTypeOpnd,
            Js::OpCode::BrEq_A, helperLabel, newObjInstr);
        InsertTestBranch(IR::IndirOpnd::New(literalTypeOpnd->AsRegOpnd(), Js::DynamicType::GetOffsetOfIsShared(), TyInt8, func),
            IR::IntConstOpnd::New(1, TyInt8, func, true), Js::OpCode::BrEq_A, helperLabel, newObjInstr);

        dstOpnd = newObjInstr->GetDst()->AsRegOpnd();
    }
    else
    {
        literalTypeOpnd = IR::AddrOpnd::New(literalType, IR::AddrOpndKindDynamicType, func);
        dstOpnd = newObjInstr->UnlinkDst()->AsRegOpnd();
        Assert(inlineSlotCapacity == literalType->GetTypeHandler()->GetInlineSlotCapacity());
        Assert(slotCapacity == (uint)literalType->GetTypeHandler()->GetSlotCapacity());
    }

    if (helperLabel)
    {
        InsertBranch(Js::OpCode::Br, allocLabel, newObjInstr);

        // Slow path to ensure the type is there
        newObjInstr->InsertBefore(helperLabel);
        IR::HelperCallOpnd * opndHelper = IR::HelperCallOpnd::New(IR::HelperEnsureObjectLiteralType, func);

        m_lowererMD.LoadHelperArgument(newObjInstr, literalTypeRefOpnd);
        m_lowererMD.LoadHelperArgument(newObjInstr, propertyArrayOpnd);
        LoadScriptContext(newObjInstr);

        IR::Instr * ensureTypeInstr = IR::Instr::New(Js::OpCode::Call, literalTypeOpnd, opndHelper, func);
        newObjInstr->InsertBefore(ensureTypeInstr);
        m_lowererMD.LowerCall(ensureTypeInstr, 0);

        newObjInstr->InsertBefore(allocLabel);
    }
    else
    {
        Assert(allocLabel == nullptr);
    }

    // For the next call:
    //     inlineSlotCapacity == Number of slots to allocate beyond the DynamicObject header
    //     slotCapacity - inlineSlotCapacity == Number of aux slots to allocate
    if(Js::FunctionBody::DoObjectHeaderInliningForObjectLiteral(propIds, scriptContext))
    {
        Assert(inlineSlotCapacity >= Js::DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity());
        Assert(inlineSlotCapacity == slotCapacity);
        slotCapacity = inlineSlotCapacity -= Js::DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity();
    }
    GenerateDynamicObjectAlloc(
        newObjInstr,
        inlineSlotCapacity,
        slotCapacity,
        dstOpnd,
        literalTypeOpnd);

    newObjInstr->Remove();
}

IR::Instr*
Lowerer::LowerProfiledNewScArray(IR::JitProfilingInstr* arrInstr)
{
    IR::Instr *instrPrev = arrInstr->m_prev;

    /*
        JavascriptArray *ProfilingHelpers::ProfiledNewScArray(
            const uint length,
            FunctionBody *const functionBody,
            const ProfileId profileId)
    */

    m_lowererMD.LoadHelperArgument(arrInstr, IR::Opnd::CreateProfileIdOpnd(arrInstr->profileId, m_func));
    m_lowererMD.LoadHelperArgument(arrInstr, CreateFunctionBodyOpnd(arrInstr->m_func));
    m_lowererMD.LoadHelperArgument(arrInstr, arrInstr->UnlinkSrc1());
    arrInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperProfiledNewScArray, m_func));
    m_lowererMD.LowerCall(arrInstr, 0);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerNewScArray(IR::Instr *arrInstr)
{
    if (arrInstr->IsJitProfilingInstr())
    {
        return LowerProfiledNewScArray(arrInstr->AsJitProfilingInstr());
    }


    IR::Instr *instrPrev = arrInstr->m_prev;
    IR::JnHelperMethod helperMethod = IR::HelperScrArr_OP_NewScArray;


    if (arrInstr->IsProfiledInstr() && arrInstr->m_func->HasProfileInfo())
    {
        RecyclerWeakReference<Js::FunctionBody> *weakFuncRef = arrInstr->m_func->GetWeakFuncRef();
        Assert(weakFuncRef);

        Js::ProfileId profileId = static_cast<Js::ProfileId>(arrInstr->AsProfiledInstr()->u.profileId);
        Js::FunctionBody *functionBody = arrInstr->m_func->GetJnFunction();
        Js::DynamicProfileInfo *profileInfo = functionBody->GetAnyDynamicProfileInfo();
        Js::ArrayCallSiteInfo *arrayInfo = profileInfo->GetArrayCallSiteInfo(functionBody, profileId);


        Assert(arrInstr->GetSrc1()->IsConstOpnd());
        GenerateProfiledNewScArrayFastPath(arrInstr, arrayInfo, weakFuncRef, arrInstr->GetSrc1()->AsIntConstOpnd()->AsUint32());

        if (arrInstr->GetDst() && arrInstr->GetDst()->GetValueType().IsLikelyNativeArray())
        {
            m_lowererMD.LoadHelperArgument(arrInstr, IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func));
            m_lowererMD.LoadHelperArgument(arrInstr, IR::AddrOpnd::New(arrayInfo, IR::AddrOpndKindDynamicArrayCallSiteInfo, m_func));
            helperMethod = IR::HelperScrArr_ProfiledNewScArray;
        }
    }

    LoadScriptContext(arrInstr);

    IR::Opnd *src1Opnd = arrInstr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(arrInstr, src1Opnd);
    m_lowererMD.ChangeToHelperCall(arrInstr, helperMethod);

    return instrPrev;
}

template <typename ArrayType>
BOOL Lowerer::IsSmallObject(uint32 length)
{
    if (ArrayType::HasInlineHeadSegment(length))
        return true;
    uint32 alignedHeadSegmentSize = Js::SparseArraySegment<typename ArrayType::TElement>::GetAlignedSize(length);
    size_t allocSize =  sizeof(Js::SparseArraySegment<typename ArrayType::TElement>) + alignedHeadSegmentSize * sizeof(typename ArrayType::TElement);
    return HeapInfo::IsSmallObject(HeapInfo::GetAlignedSizeNoCheck(allocSize));
}

void
Lowerer::GenerateProfiledNewScArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef, uint32 length)
{
    if (PHASE_OFF(Js::ArrayCtorFastPathPhase, m_func) || CONFIG_FLAG(ForceES5Array))
    {
        return;
    }

    Func * func = this->m_func;
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    uint32 size = length;
    bool isZeroed;
    IR::RegOpnd *dstOpnd = instr->GetDst()->AsRegOpnd();
    IR::RegOpnd *headOpnd;
    uint32 i = length;

    if (instr->GetDst() && instr->GetDst()->GetValueType().IsLikelyNativeIntArray())
    {
        if (!IsSmallObject<Js::JavascriptNativeIntArray>(length))
        {
            return;
        }
        GenerateArrayInfoIsNativeIntArrayTest(instr, arrayInfo, helperLabel);
        Assert(Js::JavascriptNativeIntArray::GetOffsetOfArrayFlags() + sizeof(uint16) == Js::JavascriptNativeIntArray::GetOffsetOfArrayCallSiteIndex());
        headOpnd = GenerateArrayAlloc<Js::JavascriptNativeIntArray>(instr, &size, arrayInfo, &isZeroed);
        const IR::AutoReuseOpnd autoReuseHeadOpnd(headOpnd, func);

        GenerateMemInit(dstOpnd, Js::JavascriptNativeIntArray::GetOffsetOfWeakFuncRef(), IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func), instr, isZeroed);
        for (; i < size; i++)
        {
            GenerateMemInit(headOpnd, sizeof(Js::SparseArraySegmentBase) + i * sizeof(int32),
                Js::JavascriptNativeIntArray::MissingItem, instr, isZeroed);
        }
    }
    else if (instr->GetDst() && instr->GetDst()->GetValueType().IsLikelyNativeFloatArray())
    {
        if (!IsSmallObject<Js::JavascriptNativeFloatArray>(length))
        {
            return;
        }
        GenerateArrayInfoIsNativeFloatAndNotIntArrayTest(instr, arrayInfo, helperLabel);
        Assert(Js::JavascriptNativeFloatArray::GetOffsetOfArrayFlags() + sizeof(uint16) == Js::JavascriptNativeFloatArray::GetOffsetOfArrayCallSiteIndex());
        headOpnd = GenerateArrayAlloc<Js::JavascriptNativeFloatArray>(instr, &size, arrayInfo, &isZeroed);
        const IR::AutoReuseOpnd autoReuseHeadOpnd(headOpnd, func);

        GenerateMemInit(dstOpnd, Js::JavascriptNativeFloatArray::GetOffsetOfWeakFuncRef(), IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func), instr, isZeroed);
        // Js::JavascriptArray::MissingItem is a Var, so it may be 32-bit or 64 bit.
        uint const offsetStart = sizeof(Js::SparseArraySegmentBase);
        uint const missingItemCount = size * sizeof(double) / sizeof(Js::JavascriptArray::MissingItem);
        i = i * sizeof(double) / sizeof(Js::JavascriptArray::MissingItem);
        for (; i < missingItemCount; i++)
        {
            GenerateMemInit(
                headOpnd, offsetStart + i * sizeof(Js::JavascriptArray::MissingItem),
                IR::AddrOpnd::New(Js::JavascriptArray::MissingItem, IR::AddrOpndKindConstant, m_func, true),
                instr, isZeroed);
        }
    }
    else
    {
        if (!IsSmallObject<Js::JavascriptArray>(length))
        {
            return;
        }
        uint const offsetStart = sizeof(Js::SparseArraySegmentBase);
        headOpnd = GenerateArrayAlloc<Js::JavascriptArray>(instr, &size, arrayInfo, &isZeroed);
        const IR::AutoReuseOpnd autoReuseHeadOpnd(headOpnd, func);
        for (; i < size; i++)
        {
            GenerateMemInit(
                headOpnd, offsetStart + i * sizeof(Js::Var),
                IR::AddrOpnd::New(Js::JavascriptArray::MissingItem, IR::AddrOpndKindConstant, m_func, true),
                instr, isZeroed);
        }
    }

    // Skip pass the helper call
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);
    instr->InsertBefore(helperLabel);

    instr->InsertAfter(doneLabel);
}

void
Lowerer::GenerateArrayInfoIsNativeIntArrayTest(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, IR::LabelInstr * helperLabel)
{
    Func * func = this->m_func;
    InsertTestBranch(IR::MemRefOpnd::New(((char *)arrayInfo) + Js::ArrayCallSiteInfo::GetOffsetOfBits(), TyUint8, func),
        IR::IntConstOpnd::New(Js::ArrayCallSiteInfo::NotNativeIntBit, TyUint8, func), Js::OpCode::BrNeq_A, helperLabel, instr);
}

void
Lowerer::GenerateArrayInfoIsNativeFloatAndNotIntArrayTest(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, IR::LabelInstr * helperLabel)
{
    Func * func = this->m_func;
    InsertCompareBranch(IR::MemRefOpnd::New(((char *)arrayInfo) + Js::ArrayCallSiteInfo::GetOffsetOfBits(), TyUint8, func),
        IR::IntConstOpnd::New(Js::ArrayCallSiteInfo::NotNativeIntBit, TyUint8, func), Js::OpCode::BrNeq_A, helperLabel, instr);
}

template <typename ArrayType>
static IR::JnHelperMethod GetArrayAllocMemHelper();
template <>
static IR::JnHelperMethod GetArrayAllocMemHelper<Js::JavascriptArray>()
{
    return IR::HelperAllocMemForJavascriptArray;
}
template <>
static IR::JnHelperMethod GetArrayAllocMemHelper<Js::JavascriptNativeIntArray>()
{
    return IR::HelperAllocMemForJavascriptNativeIntArray;
}
template <>
static IR::JnHelperMethod GetArrayAllocMemHelper<Js::JavascriptNativeFloatArray>()
{
    return IR::HelperAllocMemForJavascriptNativeFloatArray;
}

template <typename ArrayType>
IR::RegOpnd *
Lowerer::GenerateArrayAlloc(IR::Instr *instr, uint32 * psize, Js::ArrayCallSiteInfo * arrayInfo, bool * pIsHeadSegmentZeroed)
{
    Func * func = this->m_func;
    IR::RegOpnd * dstOpnd = instr->GetDst()->AsRegOpnd();

    // Generate code as in JavascriptArray::NewLiteral
    uint32 count = *psize;
    uint alignedHeadSegmentSize;
    size_t arrayAllocSize;

    IR::RegOpnd * headOpnd = IR::RegOpnd::New(TyMachPtr, func);
    const IR::AutoReuseOpnd autoReuseHeadOpnd(headOpnd, func, false);
    IR::Instr * leaHeadInstr = nullptr;
    bool isHeadSegmentZeroed = false;
    if (ArrayType::HasInlineHeadSegment(count))
    {
        uint32 allocCount = count == 0 ? Js::SparseArraySegmentBase::SMALL_CHUNK_SIZE : count;
        arrayAllocSize = Js::JavascriptArray::DetermineAllocationSize<ArrayType, 0>(allocCount, nullptr, &alignedHeadSegmentSize);
        leaHeadInstr = IR::Instr::New(Js::OpCode::LEA, headOpnd,
            IR::IndirOpnd::New(dstOpnd, sizeof(ArrayType), TyMachPtr, func), func);
        isHeadSegmentZeroed = true;
    }
    else
    {
        // Need to allocate the head segment first so that if it throws,
        // we doesn't have the memory assigned to dstOpnd yet

        // Even if the instruction is marked as dstIsTempObject, we still should not allocate
        // that big of a chunk on the stack.

        alignedHeadSegmentSize = Js::SparseArraySegment<typename ArrayType::TElement>::GetAlignedSize(count);
        GenerateRecyclerAlloc(
            IR::HelperAllocMemForSparseArraySegmentBase,
            sizeof(Js::SparseArraySegment<typename ArrayType::TElement>) +
                alignedHeadSegmentSize * sizeof(typename ArrayType::TElement),
            headOpnd,
            instr);

        arrayAllocSize = sizeof(ArrayType);
    }

    *psize = alignedHeadSegmentSize;

    IR::SymOpnd * tempObjectSymOpnd;
    bool isZeroed = GenerateRecyclerOrMarkTempAlloc(instr, dstOpnd,
        GetArrayAllocMemHelper<ArrayType>(), arrayAllocSize, &tempObjectSymOpnd);
    isHeadSegmentZeroed = isHeadSegmentZeroed & isZeroed;
    if (tempObjectSymOpnd && !PHASE_OFF(Js::HoistMarkTempInitPhase, this->m_func) && this->outerMostLoopLabel)
    {
        // Hoist the vtable init to the outer most loop top as it never changes
        InsertMove(tempObjectSymOpnd,
            this->LoadVTableValueOpnd(this->outerMostLoopLabel, ArrayType::VtableHelper()),
            this->outerMostLoopLabel, false);
    }
    else
    {
        GenerateMemInit(dstOpnd, 0, this->LoadVTableValueOpnd(instr, ArrayType::VtableHelper()), instr, isZeroed);
    }
    GenerateMemInit(dstOpnd, ArrayType::GetOffsetOfType(), this->LoadLibraryValueOpnd(instr, ArrayType::InitialTypeHelper()), instr, isZeroed);
    GenerateMemInitNull(dstOpnd, ArrayType::GetOffsetOfAuxSlots(), instr, isZeroed);

    // Emit the flags and call site index together
    Js::ProfileId arrayCallSiteIndex = (Js::ProfileId)instr->AsProfiledInstr()->u.profileId;
#if DBG
    if (instr->AsProfiledInstr()->u.profileId < Js::Constants::NoProfileId)
    {
        Js::FunctionBody * functionBody = instr->m_func->GetJnFunction();
        Assert((uint32)(arrayInfo - functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, 0)) == arrayCallSiteIndex);
    }
    else
    {
        Assert(arrayInfo == nullptr);
    }
#endif

    // The same at this:
    //  GenerateMemInit(dstOpnd, ArrayType::GetOffsetOfArrayFlags(), (uint16)Js::DynamicObjectFlags::InitialArrayValue, instr, isZeroed);
    //  GenerateMemInit(dstOpnd, ArrayType::GetOffsetOfArrayCallSiteIndex(), arrayCallSiteIndex, instr, isZeroed);
    GenerateMemInit(dstOpnd, ArrayType::GetOffsetOfArrayFlags(), (uint)Js::DynamicObjectFlags::InitialArrayValue | ((uint)arrayCallSiteIndex << 16), instr, isZeroed);

    GenerateMemInit(dstOpnd, ArrayType::GetOffsetOfLength(), count, instr, isZeroed);

    if (leaHeadInstr != nullptr)
    {
        instr->InsertBefore(leaHeadInstr);
        LowererMD::ChangeToLea(leaHeadInstr);
    }

    GenerateMemInit(dstOpnd, ArrayType::GetOffsetOfHead(), headOpnd, instr, isZeroed);
    GenerateMemInit(dstOpnd, ArrayType::GetOffsetOfLastUsedSegmentOrSegmentMap(), headOpnd, instr, isZeroed);

    // Initialize segment head
    GenerateMemInit(headOpnd, Js::SparseArraySegmentBase::GetOffsetOfLeft(), 0, instr, isHeadSegmentZeroed);
    GenerateMemInit(headOpnd, Js::SparseArraySegmentBase::GetOffsetOfLength(), count, instr, isHeadSegmentZeroed);
    GenerateMemInit(headOpnd, Js::SparseArraySegmentBase::GetOffsetOfSize(), alignedHeadSegmentSize, instr, isHeadSegmentZeroed);
    GenerateMemInitNull(headOpnd, Js::SparseArraySegmentBase::GetOffsetOfNext(), instr, isHeadSegmentZeroed);

    *pIsHeadSegmentZeroed = isHeadSegmentZeroed;
    return headOpnd;
}

void
Lowerer::GenerateProfiledNewScObjArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef, uint32 length)
{
    if (PHASE_OFF(Js::ArrayCtorFastPathPhase, m_func))
    {
        return;
    }

    Func * func = this->m_func;
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    uint32 size = length;
    bool isZeroed;
    IR::RegOpnd *dstOpnd = instr->GetDst()->AsRegOpnd();
    IR::RegOpnd *headOpnd;

    if (arrayInfo && arrayInfo->IsNativeIntArray())
    {
        GenerateArrayInfoIsNativeIntArrayTest(instr, arrayInfo, helperLabel);
        Assert(Js::JavascriptNativeIntArray::GetOffsetOfArrayFlags() + sizeof(uint16) == Js::JavascriptNativeIntArray::GetOffsetOfArrayCallSiteIndex());
        headOpnd = GenerateArrayAlloc<Js::JavascriptNativeIntArray>(instr, &size, arrayInfo, &isZeroed);

        GenerateMemInit(dstOpnd, Js::JavascriptNativeIntArray::GetOffsetOfWeakFuncRef(), IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func), instr, isZeroed);

        for (uint i = 0; i < size; i++)
        {
            GenerateMemInit(headOpnd, sizeof(Js::SparseArraySegmentBase) + i * sizeof(int32),
                            Js::JavascriptNativeIntArray::MissingItem, instr, isZeroed);
        }
    }
    else if (arrayInfo && arrayInfo->IsNativeFloatArray())
    {
        GenerateArrayInfoIsNativeFloatAndNotIntArrayTest(instr, arrayInfo, helperLabel);
        Assert(Js::JavascriptNativeFloatArray::GetOffsetOfArrayFlags() + sizeof(uint16) == Js::JavascriptNativeFloatArray::GetOffsetOfArrayCallSiteIndex());
        headOpnd = GenerateArrayAlloc<Js::JavascriptNativeFloatArray>(instr, &size, arrayInfo, &isZeroed);

        GenerateMemInit(dstOpnd, Js::JavascriptNativeFloatArray::GetOffsetOfWeakFuncRef(), IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func), instr, isZeroed);

        // Js::JavascriptArray::MissingItem is a Var, so it may be 32-bit or 64 bit.
        uint const offsetStart = sizeof(Js::SparseArraySegmentBase);
        uint const missingItemCount = size * sizeof(double) / sizeof(Js::JavascriptArray::MissingItem);
        for (uint i = 0; i < missingItemCount; i++)
        {
            GenerateMemInit(
                headOpnd, offsetStart + i * sizeof(Js::JavascriptArray::MissingItem),
                IR::AddrOpnd::New(Js::JavascriptArray::MissingItem, IR::AddrOpndKindConstant, m_func, true),
                instr, isZeroed);
        }
    }
    else
    {
        uint const offsetStart = sizeof(Js::SparseArraySegmentBase);
        headOpnd = GenerateArrayAlloc<Js::JavascriptArray>(instr, &size, arrayInfo, &isZeroed);
        for (uint i = 0; i < size; i++)
        {
            GenerateMemInit(
                headOpnd, offsetStart + i * sizeof(Js::Var),
                IR::AddrOpnd::New(Js::JavascriptArray::MissingItem, IR::AddrOpndKindConstant, m_func, true),
                instr, isZeroed);
        }
    }

    // Skip pass the helper call
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);
    instr->InsertBefore(helperLabel);

    instr->InsertAfter(doneLabel);
}

void
Lowerer::GenerateProfiledNewScIntArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef)
{
    // Helper will deal with ForceES5ARray
    if (PHASE_OFF(Js::ArrayLiteralFastPathPhase, m_func) || CONFIG_FLAG(ForceES5Array))
    {
        return;
    }

    if (!arrayInfo->IsNativeIntArray())
    {
        return;
    }

    Func * func = this->m_func;
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);

    GenerateArrayInfoIsNativeIntArrayTest(instr, arrayInfo, helperLabel);

    IR::AddrOpnd * elementsOpnd = instr->GetSrc1()->AsAddrOpnd();
    Js::AuxArray<int32> * ints = (Js::AuxArray<int32> *)elementsOpnd->m_address;
    uint32 size = ints->count;

    // Generate code as in JavascriptArray::NewLiteral
    bool isHeadSegmentZeroed;
    IR::RegOpnd * dstOpnd = instr->GetDst()->AsRegOpnd();
    Assert(Js::JavascriptNativeIntArray::GetOffsetOfArrayFlags() + sizeof(uint16) == Js::JavascriptNativeIntArray::GetOffsetOfArrayCallSiteIndex());
    IR::RegOpnd * headOpnd = GenerateArrayAlloc<Js::JavascriptNativeIntArray>(instr, &size, arrayInfo, &isHeadSegmentZeroed);
    const IR::AutoReuseOpnd autoReuseHeadOpnd(headOpnd, func);

    GenerateMemInit(dstOpnd, Js::JavascriptNativeIntArray::GetOffsetOfWeakFuncRef(), IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicMisc, m_func), instr, isHeadSegmentZeroed);

    // Initialize the elements
    uint i = 0;
    if (ints->count > 16)
    {
        // Do memcpy if > 16
        IR::RegOpnd * dstElementsOpnd = IR::RegOpnd::New(TyMachPtr, func);
        const IR::AutoReuseOpnd autoReuseDstElementsOpnd(dstElementsOpnd, func);
        IR::Opnd * srcOpnd = IR::AddrOpnd::New(ints->elements, IR::AddrOpndKindDynamicMisc, func);
        InsertLea(dstElementsOpnd, IR::IndirOpnd::New(headOpnd, sizeof(Js::SparseArraySegmentBase), TyMachPtr, func), instr);
        GenerateMemCopy(dstElementsOpnd, srcOpnd, ints->count * sizeof(int32), instr);
        i = ints->count;
    }
    else
    {
        for (; i < ints->count; i++)
        {
            GenerateMemInit(headOpnd, sizeof(Js::SparseArraySegmentBase) + i * sizeof(int32),
                ints->elements[i], instr, isHeadSegmentZeroed);
        }
    }
    Assert(i == ints->count);
    for (; i < size; i++)
    {
        GenerateMemInit(headOpnd, sizeof(Js::SparseArraySegmentBase) + i * sizeof(int32),
            Js::JavascriptNativeIntArray::MissingItem, instr, isHeadSegmentZeroed);
    }
    // Skip pass the helper call
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);
    instr->InsertBefore(helperLabel);

    instr->InsertAfter(doneLabel);
}

void
Lowerer::GenerateProfiledNewScFloatArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef)
{
    if (PHASE_OFF(Js::ArrayLiteralFastPathPhase, m_func) || CONFIG_FLAG(ForceES5Array))
    {
        return;
    }

    if (!arrayInfo->IsNativeFloatArray())
    {
        return;
    }

    Func * func = this->m_func;
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);

    // If the array info hasn't mark as not int array yet, go to the helper and mark it.
    // It really is just for assert purpose in JavascriptNativeFloatArray::ToVarArray
    GenerateArrayInfoIsNativeFloatAndNotIntArrayTest(instr, arrayInfo, helperLabel);

    IR::AddrOpnd * elementsOpnd = instr->GetSrc1()->AsAddrOpnd();
    Js::AuxArray<double> * doubles = (Js::AuxArray<double> *)elementsOpnd->m_address;
    uint32 size = doubles->count;

    // Generate code as in JavascriptArray::NewLiteral
    bool isHeadSegmentZeroed;
    IR::RegOpnd * dstOpnd = instr->GetDst()->AsRegOpnd();
    Assert(Js::JavascriptNativeFloatArray::GetOffsetOfArrayFlags() + sizeof(uint16) == Js::JavascriptNativeFloatArray::GetOffsetOfArrayCallSiteIndex());
    IR::RegOpnd * headOpnd = GenerateArrayAlloc<Js::JavascriptNativeFloatArray>(instr, &size, arrayInfo, &isHeadSegmentZeroed);
    const IR::AutoReuseOpnd autoReuseHeadOpnd(headOpnd, func);

    GenerateMemInit(dstOpnd, Js::JavascriptNativeFloatArray::GetOffsetOfWeakFuncRef(), IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func), instr, isHeadSegmentZeroed);

    // Initialize the elements

    IR::RegOpnd * dstElementsOpnd = IR::RegOpnd::New(TyMachPtr, func);
    const IR::AutoReuseOpnd autoReuseDstElementsOpnd(dstElementsOpnd, func);
    IR::Opnd * srcOpnd = IR::AddrOpnd::New(doubles->elements, IR::AddrOpndKindDynamicMisc, func);
    InsertLea(dstElementsOpnd, IR::IndirOpnd::New(headOpnd, sizeof(Js::SparseArraySegmentBase), TyMachPtr, func), instr);
    GenerateMemCopy(dstElementsOpnd, srcOpnd, doubles->count * sizeof(double), instr);

    // Js::JavascriptArray::MissingItem is a Var, so it may be 32-bit or 64 bit.
    uint const offsetStart = sizeof(Js::SparseArraySegmentBase) + doubles->count * sizeof(double);
    uint const missingItem = (size - doubles->count) * sizeof(double) / sizeof(Js::JavascriptArray::MissingItem);
    for (uint i = 0; i < missingItem; i++)
    {
        GenerateMemInit(headOpnd, offsetStart + i * sizeof(Js::JavascriptArray::MissingItem),
            IR::AddrOpnd::New(Js::JavascriptArray::MissingItem, IR::AddrOpndKindConstant, m_func, true), instr, isHeadSegmentZeroed);
    }
    // Skip pass the helper call
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);
    instr->InsertBefore(helperLabel);

    instr->InsertAfter(doneLabel);
}

IR::Instr *
Lowerer::LowerNewScIntArray(IR::Instr *arrInstr)
{
    IR::Instr *instrPrev = arrInstr->m_prev;
    IR::JnHelperMethod helperMethod = IR::HelperScrArr_OP_NewScIntArray;

    if ((arrInstr->IsJitProfilingInstr() || arrInstr->IsProfiledInstr()) && arrInstr->m_func->HasProfileInfo())
    {
        RecyclerWeakReference<Js::FunctionBody> *weakFuncRef = arrInstr->m_func->GetWeakFuncRef();
        if (weakFuncRef)
        {
            Js::FunctionBody *functionBody = arrInstr->m_func->GetJnFunction();
            // Technically a load of the same memory address either way.
            Js::ProfileId profileId =
                arrInstr->IsJitProfilingInstr()
                    ? arrInstr->AsJitProfilingInstr()->profileId
                    : static_cast<Js::ProfileId>(arrInstr->AsProfiledInstr()->u.profileId);
            Js::ArrayCallSiteInfo *arrayInfo =
                functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);

            // Only do fast-path if it isn't a JitProfiling instr and not copy-on-access array
            if (arrInstr->IsProfiledInstr()
                && (PHASE_OFF1(Js::Phase::CopyOnAccessArrayPhase) || arrayInfo->isNotCopyOnAccessArray) && !PHASE_FORCE1(Js::Phase::CopyOnAccessArrayPhase))
            {
                GenerateProfiledNewScIntArrayFastPath(arrInstr, arrayInfo, weakFuncRef);
            }

            m_lowererMD.LoadHelperArgument(arrInstr, IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func));
            m_lowererMD.LoadHelperArgument(arrInstr, IR::AddrOpnd::New(arrayInfo, IR::AddrOpndKindDynamicArrayCallSiteInfo, m_func));
            helperMethod = IR::HelperScrArr_ProfiledNewScIntArray;
        }
    }

    LoadScriptContext(arrInstr);

    IR::Opnd *elementsOpnd = arrInstr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(arrInstr, elementsOpnd);
    m_lowererMD.ChangeToHelperCall(arrInstr, helperMethod);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerNewScFltArray(IR::Instr *arrInstr)
{
    IR::Instr *instrPrev = arrInstr->m_prev;
    IR::JnHelperMethod helperMethod = IR::HelperScrArr_OP_NewScFltArray;

    if ((arrInstr->IsJitProfilingInstr() || arrInstr->IsProfiledInstr()) && arrInstr->m_func->HasProfileInfo())
    {
        RecyclerWeakReference<Js::FunctionBody> *weakFuncRef = arrInstr->m_func->GetWeakFuncRef();
        if (weakFuncRef)
        {
            Js::ProfileId profileId =
                arrInstr->IsJitProfilingInstr()
                    ? arrInstr->AsJitProfilingInstr()->profileId
                    : static_cast<Js::ProfileId>(arrInstr->AsProfiledInstr()->u.profileId);

            Js::FunctionBody *functionBody = arrInstr->m_func->GetJnFunction();
            Js::ArrayCallSiteInfo *arrayInfo =
                functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);

            // Only do fast-path if it isn't a JitProfiling instr
            if (arrInstr->IsProfiledInstr()) {
                GenerateProfiledNewScFloatArrayFastPath(arrInstr, arrayInfo, weakFuncRef);
            }

            m_lowererMD.LoadHelperArgument(arrInstr, IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, m_func));
            m_lowererMD.LoadHelperArgument(arrInstr, IR::AddrOpnd::New(arrayInfo, IR::AddrOpndKindDynamicArrayCallSiteInfo, m_func));
            helperMethod = IR::HelperScrArr_ProfiledNewScFltArray;
        }
    }

    LoadScriptContext(arrInstr);

    IR::Opnd *elementsOpnd = arrInstr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(arrInstr, elementsOpnd);
    m_lowererMD.ChangeToHelperCall(arrInstr, helperMethod);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerArraySegmentVars(IR::Instr *arrayInstr)
{
    IR::Instr * instrPrev;
    IR::HelperCallOpnd * opndHelper = IR::HelperCallOpnd::New(IR::HelperArraySegmentVars, m_func);

    instrPrev = m_lowererMD.LoadHelperArgument(arrayInstr, arrayInstr->UnlinkSrc2());
    m_lowererMD.LoadHelperArgument(arrayInstr, arrayInstr->UnlinkSrc1());

    arrayInstr->m_opcode = Js::OpCode::Call;
    arrayInstr->SetSrc1(opndHelper);

    m_lowererMD.LowerCall(arrayInstr, 0);

    return instrPrev;
}

IR::Instr* Lowerer::LowerProfiledNewArray(IR::JitProfilingInstr* instr, bool hasArgs)
{
    // Use the special helper which checks whether Array has been overwritten by the user and if
    //   it hasn't, possibly allocates a native array

    // Insert a temporary label before the instruction we're about to lower, so that we can return
    // the first instruction above that needs to be lowered after we're done - regardless of argument
    // list, StartCall, etc.
    IR::Instr* startMarkerInstr = InsertLoweredRegionStartMarker(instr);

    Assert(instr->isNewArray);
    Assert(instr->arrayProfileId != Js::Constants::NoProfileId);
    Assert(instr->profileId != Js::Constants::NoProfileId);

    bool isSpreadCall = instr->m_opcode == Js::OpCode::NewScObjectSpread || instr->m_opcode == Js::OpCode::NewScObjArraySpread;

    m_lowererMD.LoadNewScObjFirstArg(instr, IR::AddrOpnd::New(nullptr, IR::AddrOpndKindConstantVar, m_func, true), isSpreadCall ? 1 : 0);

    if (isSpreadCall)
    {
        this->LowerSpreadCall(instr, Js::CallFlags_New, true);
    }
    else
    {
        const int32 argCount = m_lowererMD.LowerCallArgs(instr, Js::CallFlags_New, 4);

        m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(instr->arrayProfileId, m_func));
        m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(instr->profileId, m_func));
        m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateFramePointerOpnd(m_func));
        m_lowererMD.LoadHelperArgument(instr, instr->UnlinkSrc1());

        instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperProfiledNewScObjArray, m_func));
        m_lowererMD.LowerCall(instr, static_cast<Js::ArgSlot>(argCount));
    }

    return RemoveLoweredRegionStartMarker(startMarkerInstr);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerNewScObject
///
///     Machine independent lowering of a CallI instr.
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerNewScObject(IR::Instr *newObjInstr, bool callCtor, bool hasArgs, bool isBaseClassConstructorNewScObject)
{
    if (newObjInstr->IsJitProfilingInstr() && newObjInstr->AsJitProfilingInstr()->isNewArray)
    {
        Assert(callCtor);
        return LowerProfiledNewArray(newObjInstr->AsJitProfilingInstr(), hasArgs);
    }

    bool isSpreadCall = newObjInstr->m_opcode == Js::OpCode::NewScObjectSpread ||
        newObjInstr->m_opcode == Js::OpCode::NewScObjArraySpread;

    Func* func = newObjInstr->m_func;

    // Insert a temporary label before the instruction we're about to lower, so that we can return
    // the first instruction above that needs to be lowered after we're done - regardless of argument
    // list, StartCall, etc.
    IR::Instr* startMarkerInstr = InsertLoweredRegionStartMarker(newObjInstr);

    IR::Opnd *ctorOpnd = newObjInstr->GetSrc1();
    IR::RegOpnd *newObjDst = newObjInstr->GetDst()->AsRegOpnd();

    Assert(!callCtor || !hasArgs || (newObjInstr->GetSrc2() != nullptr /*&& newObjInstr->GetSrc2()->IsSymOpnd()*/));

    bool skipNewScObj = false;
    bool returnNewScObj = false;
    bool emitBailOut = false;
    // If we haven't yet split NewScObject into NewScObjectNoCtor and CallI, we will need a temporary register
    // to hold the result of the object allocation.
    IR::RegOpnd* createObjDst = callCtor ? IR::RegOpnd::New(TyVar, func) : newObjDst;
    IR::LabelInstr* helperOrBailoutLabel = IR::LabelInstr::New(Js::OpCode::Label, func, /* isOpHelper = */ true);
    IR::LabelInstr* callCtorLabel = IR::LabelInstr::New(Js::OpCode::Label, func, /* isOpHelper = */ false);

    // Try to emit the fast allocation and construction path.
    bool usedFixedCtorCache = TryLowerNewScObjectWithFixedCtorCache(newObjInstr, createObjDst, helperOrBailoutLabel, callCtorLabel, skipNewScObj, returnNewScObj, emitBailOut);

    AssertMsg(!skipNewScObj || callCtor, "What will we return if we skip the default new object and don't call the ctor?");
    Assert(!skipNewScObj || !returnNewScObj);
    Assert(usedFixedCtorCache || !skipNewScObj);
    Assert(!usedFixedCtorCache || newObjInstr->HasFixedFunctionAddressTarget());
    Assert(!skipNewScObj || !emitBailOut);

#if DBG
    if (usedFixedCtorCache)
    {
        Js::JavascriptFunction* ctor = newObjInstr->GetFixedFunction();
        Js::FunctionInfo* ctorInfo = ctor->GetFunctionInfo();
        Assert((ctorInfo->GetAttributes() & Js::FunctionInfo::Attributes::ErrorOnNew) == 0);
        Assert(!!(ctorInfo->GetAttributes() & Js::FunctionInfo::Attributes::SkipDefaultNewObject) == skipNewScObj);
    }
#endif

    IR::Instr* startCallInstr = nullptr;
    if (callCtor && hasArgs)
    {
        hasArgs = !newObjInstr->HasEmptyArgOutChain(&startCallInstr);
    }

    // If we're not skipping the default new object, let's emit bailout or a call to NewScObject* helper
    IR::JnHelperMethod newScHelper = IR::HelperInvalid;
    IR::Instr *newScObjCall = nullptr;
    if (!skipNewScObj)
    {
        // If we emitted the fast path, this block is a helper block.
        if (usedFixedCtorCache)
        {
            newObjInstr->InsertBefore(helperOrBailoutLabel);
        }

        if (emitBailOut)
        {
            IR::Instr* bailOutInstr = newObjInstr;

            newObjInstr = IR::Instr::New(newObjInstr->m_opcode, func);
            bailOutInstr->TransferTo(newObjInstr);
            bailOutInstr->m_opcode = Js::OpCode::BailOut;
            bailOutInstr->InsertAfter(newObjInstr);

            GenerateBailOut(bailOutInstr);
        }
        else
        {
            Assert(!newObjDst->CanStoreTemp());
            // createObjDst = NewScObject...(ctorOpnd)
            newScHelper = !callCtor ?
                (isBaseClassConstructorNewScObject ?
                    (hasArgs ? IR::HelperNewScObjectNoCtorFull : IR::HelperNewScObjectNoArgNoCtorFull) :
                    (hasArgs ? IR::HelperNewScObjectNoCtor : IR::HelperNewScObjectNoArgNoCtor)) :
                (hasArgs || usedFixedCtorCache ? IR::HelperNewScObjectNoCtor : IR::HelperNewScObjectNoArg);

            LoadScriptContext(newObjInstr);
            m_lowererMD.LoadHelperArgument(newObjInstr, newObjInstr->GetSrc1());

            newScObjCall = IR::Instr::New(Js::OpCode::Call, createObjDst, IR::HelperCallOpnd::New(newScHelper, func), func);
            newObjInstr->InsertBefore(newScObjCall);
            m_lowererMD.LowerCall(newScObjCall, 0);
        }
    }

    // If we call HelperNewScObjectNoArg directly, we won't be calling the constructor from here, because the helper will do it.
    // We could probably avoid this complexity by converting NewScObjectNoArg to NewScObject in the IRBuilder, once we have dedicated
    // code paths for new Object() and new Array().
    callCtor &= hasArgs || usedFixedCtorCache;
    AssertMsg(!skipNewScObj || callCtor, "What will we return if we skip the default new object and don't call the ctor?");

    newObjInstr->InsertBefore(callCtorLabel);

    if (callCtor && usedFixedCtorCache)
    {
        IR::JnHelperMethod ctorHelper = IR::JnHelperMethodCount;

        // If we have no arguments (i.e. the argument chain is empty), we can recognize a couple of common special cases, such
        // as new Object() or new Array(), for which we have optimized helpers.
        Js::JavascriptFunction* ctor = newObjInstr->GetFixedFunction();
        Js::FunctionInfo* ctorInfo = ctor->GetFunctionInfo();
        if (!hasArgs && (ctorInfo == &Js::JavascriptObject::EntryInfo::NewInstance || ctorInfo == &Js::JavascriptArray::EntryInfo::NewInstance))
        {
            if (ctorInfo == &Js::JavascriptObject::EntryInfo::NewInstance)
            {
                Assert(skipNewScObj);
                ctorHelper = IR::HelperNewJavascriptObjectNoArg;
                callCtor = false;
            }
            else if (ctorInfo == &Js::JavascriptArray::EntryInfo::NewInstance)
            {
                Assert(skipNewScObj);
                ctorHelper = IR::HelperNewJavascriptArrayNoArg;
                callCtor = false;
            }

            if (!callCtor)
            {
                LoadScriptContext(newObjInstr);

                IR::Instr *ctorCall = IR::Instr::New(Js::OpCode::Call, newObjDst,  IR::HelperCallOpnd::New(ctorHelper, func), func);
                newObjInstr->InsertBefore(ctorCall);
                m_lowererMD.LowerCall(ctorCall, 0);
            }
        }
    }

    IR::AutoReuseOpnd autoReuseSavedCtorOpnd;
    if (callCtor)
    {
        // Load the first argument, which is either the object just created or null. Spread has an extra argument.
        IR::Instr * argInstr = this->m_lowererMD.LoadNewScObjFirstArg(newObjInstr, createObjDst, isSpreadCall ? 1 : 0);

        IR::Instr * insertAfterCtorInstr = newObjInstr->m_next;

        if (skipNewScObj)
        {
            // Since we skipped the default new object, we must be returning whatever the constructor returns
            // (which better be an Object), so let's just use newObjDst directly.
            // newObjDst = newObjInstr->m_src1(createObjDst, ...)
            Assert(newObjInstr->GetDst() == newObjDst);
            if (isSpreadCall)
            {
                newObjInstr = this->LowerSpreadCall(newObjInstr, Js::CallFlags_New);
            }
            else
            {
                newObjInstr = this->m_lowererMD.LowerCallI(newObjInstr, Js::CallFlags_New, false, argInstr);
            }
        }
        else
        {
            // We may need to return the default new object or whatever the constructor returns. Let's stash
            // away the constructor's return in a temporary operand, and do the right check, if necessary.
            // ctorResultObjOpnd = newObjInstr->m_src1(createObjDst, ...)
            IR::RegOpnd *ctorResultObjOpnd = IR::RegOpnd::New(TyVar, func);
            newObjInstr->UnlinkDst();
            newObjInstr->SetDst(ctorResultObjOpnd);

            if (isSpreadCall)
            {
                newObjInstr = this->LowerSpreadCall(newObjInstr, Js::CallFlags_New);
            }
            else
            {
                newObjInstr = this->m_lowererMD.LowerCallI(newObjInstr, Js::CallFlags_New, false, argInstr);
            }

            if (returnNewScObj)
            {
                // MOV newObjDst, createObjDst
                this->m_lowererMD.CreateAssign(newObjDst, createObjDst, insertAfterCtorInstr);
            }
            else
            {
                LowerGetNewScObjectCommon(ctorResultObjOpnd, ctorResultObjOpnd, createObjDst, insertAfterCtorInstr);
                this->m_lowererMD.CreateAssign(newObjDst, ctorResultObjOpnd, insertAfterCtorInstr);
            }
        }

        // We don't ever need to update the constructor cache, if we hard coded it.  Caches requiring update after constructor
        // don't get cloned, and those that don't require update will never need one anymore.
        if (!usedFixedCtorCache)
        {
            LowerUpdateNewScObjectCache(insertAfterCtorInstr, newObjDst, ctorOpnd, false /* isCtorFunction */);
        }
    }
    else
    {
        if (newObjInstr->IsJitProfilingInstr())
        {
            Assert(m_func->IsSimpleJit());
            Assert(!Js::FunctionBody::IsNewSimpleJit());

            // This path skipped calling the Ctor, which skips calling LowerCallI with newObjInstr, meaning that the call will not be profiled.
            //   So we insert it manually here.

            if(newScHelper == IR::HelperNewScObjectNoArg &&
                newObjDst &&
                ctorOpnd->IsRegOpnd() &&
                newObjDst->AsRegOpnd()->m_sym == ctorOpnd->AsRegOpnd()->m_sym)
            {
                Assert(newObjInstr->m_func->IsSimpleJit());
                Assert(createObjDst != newObjDst);

                // The function object sym is going to be overwritten, so save it in a temp for profiling
                IR::RegOpnd *const savedCtorOpnd = IR::RegOpnd::New(ctorOpnd->GetType(), newObjInstr->m_func);
                autoReuseSavedCtorOpnd.Initialize(savedCtorOpnd, newObjInstr->m_func);
                Lowerer::InsertMove(savedCtorOpnd, ctorOpnd, newObjInstr);
                ctorOpnd = savedCtorOpnd;
            }

            // It is a constructor (CallFlags_New) and therefore a single argument (this) would have been given.
            const auto info = Lowerer::MakeCallInfoConst(Js::CallFlags_New, 1, func);

            Assert(newScObjCall);
            IR::JitProfilingInstr *const newObjJitProfilingInstr = newObjInstr->AsJitProfilingInstr();
            GenerateCallProfiling(
                newObjJitProfilingInstr->profileId,
                newObjJitProfilingInstr->inlineCacheIndex,
                createObjDst,
                ctorOpnd,
                info,
                false,
                newScObjCall,
                newObjInstr);
        }

        // MOV newObjDst, createObjDst
        if (!skipNewScObj && createObjDst != newObjDst)
        {
            this->m_lowererMD.CreateAssign(newObjDst, createObjDst, newObjInstr);
        }
        newObjInstr->Remove();
    }

    // Return the first instruction above the region we've just lowered.
    return RemoveLoweredRegionStartMarker(startMarkerInstr);
}

IR::Instr*
Lowerer::GenerateCallProfiling(Js::ProfileId profileId, Js::InlineCacheIndex inlineCacheIndex, IR::Opnd* retval, IR::Opnd*calleeFunctionObjOpnd, IR::Opnd* callInfo, bool returnTypeOnly, IR::Instr*callInstr,IR::Instr*insertAfter)
{
    // This should only ever happen in profiling simplejit
    Assert(m_func->DoSimpleJitDynamicProfile());

    // Make sure they gave us the correct call instruction
#if defined(_M_IX86) || defined(_M_X64)
    Assert(callInstr->m_opcode == Js::OpCode::CALL);
#elif defined(_M_ARM)
    Assert(callInstr->m_opcode == Js::OpCode::BLX);
#endif
    Func*const func = insertAfter->m_func;

    {
        // First, we should save the implicit call flags
        const auto starFlag = GetImplicitCallFlagsOpnd();
        const auto saveOpnd = IR::RegOpnd::New(starFlag->GetType(), func);

        IR::AutoReuseOpnd a(starFlag, func), b(saveOpnd, func);
        //Save the flags (before call) and restore them (after the call)
        this->InsertMove(saveOpnd, starFlag, callInstr);
        // Note: On arm this is slightly inefficient because it forces a reload of the memory location to a reg (whereas x86 can load straight from hard-coded memory into a reg)
        //    But it works and making it not reload the memory location would force more refactoring.
        this->InsertMove(starFlag, saveOpnd, insertAfter->m_next);
    }

    // Profile a call that just happened: push some extra info on the stack and call the helper

    if (!retval)
    {
        if (returnTypeOnly)
        {
            // If we are only supposed to profile the return type but don't use the return value, we might
            // as well do nothing!
            return insertAfter;
        }
        retval = IR::AddrOpnd::NewNull(func);
    }

    IR::Instr* profileCall = IR::Instr::New(Js::OpCode::Call, func);

    bool needInlineCacheIndex;
    IR::JnHelperMethod helperMethod;
    if (returnTypeOnly)
    {
        needInlineCacheIndex = false;
        helperMethod = IR::HelperSimpleProfileReturnTypeCall;
    }
    else if(inlineCacheIndex == Js::Constants::NoInlineCacheIndex)
    {
        needInlineCacheIndex = false;
        helperMethod = IR::HelperSimpleProfileCall_DefaultInlineCacheIndex;
    }
    else
    {
        needInlineCacheIndex = true;
        helperMethod = IR::HelperSimpleProfileCall;
    }
    profileCall->SetSrc1(IR::HelperCallOpnd::New(helperMethod, func));

    insertAfter->InsertAfter(profileCall);

    m_lowererMD.LoadHelperArgument(profileCall, callInfo);
    m_lowererMD.LoadHelperArgument(profileCall, calleeFunctionObjOpnd);
    m_lowererMD.LoadHelperArgument(profileCall, retval);
    if(needInlineCacheIndex)
    {
        m_lowererMD.LoadHelperArgument(profileCall, IR::Opnd::CreateInlineCacheIndexOpnd(inlineCacheIndex, func));
    }
    m_lowererMD.LoadHelperArgument(profileCall, IR::Opnd::CreateProfileIdOpnd(profileId, func));
    // Push the frame pointer so that the profiling call can grab the stack layout
    m_lowererMD.LoadHelperArgument(profileCall, IR::Opnd::CreateFramePointerOpnd(func));

    // No args: the helper is stdcall
    return m_lowererMD.LowerCall(profileCall, 0);
}

bool Lowerer::TryLowerNewScObjectWithFixedCtorCache(IR::Instr* newObjInstr, IR::RegOpnd* newObjDst,
    IR::LabelInstr* helperOrBailoutLabel, IR::LabelInstr* callCtorLabel, bool& skipNewScObj, bool& returnNewScObj, bool& emitBailOut)
{
    skipNewScObj = false;
    returnNewScObj = false;

    AssertMsg(!PHASE_OFF(Js::ObjTypeSpecNewObjPhase, this->m_func) || !newObjInstr->HasBailOutInfo(),
        "Why do we have bailout on NewScObject when ObjTypeSpecNewObj is off?");

    if (PHASE_OFF(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()) && PHASE_OFF(Js::ObjTypeSpecNewObjPhase, this->m_func))
    {
        return false;
    }

    Js::JitTimeConstructorCache* ctorCache;

    if (newObjInstr->HasBailOutInfo())
    {
        Assert(newObjInstr->IsNewScObjectInstr());
        Assert(newObjInstr->IsProfiledInstr());
        Assert(newObjInstr->GetBailOutKind() == IR::BailOutFailedCtorGuardCheck);

        emitBailOut = true;

        ctorCache = newObjInstr->m_func->GetConstructorCache(static_cast<Js::ProfileId>(newObjInstr->AsProfiledInstr()->u.profileId));
        Assert(ctorCache != nullptr);
        Assert(!ctorCache->skipNewScObject);
        Assert(!ctorCache->typeIsFinal || ctorCache->ctorHasNoExplicitReturnValue);

        LinkCtorCacheToGuardedProperties(ctorCache);
    }
    else
    {
        if (newObjInstr->m_opcode == Js::OpCode::NewScObjArray || newObjInstr->m_opcode == Js::OpCode::NewScObjArraySpread)
        {
            // These instr's carry a profile that indexes the array call site info, not the ctor cache.
            return false;
        }

        ctorCache = newObjInstr->IsProfiledInstr() ? newObjInstr->m_func->GetConstructorCache(static_cast<Js::ProfileId>(newObjInstr->AsProfiledInstr()->u.profileId)) : nullptr;

        if (ctorCache == nullptr)
        {
            if (PHASE_TRACE(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()) || PHASE_TESTTRACE(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()))
            {
                Js::FunctionBody* callerFunctionBody = newObjInstr->m_func->GetJnFunction();
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"FixedNewObj: function %s (%s): lowering non-fixed new script object for %s, because %s.\n",
                    callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer), Js::OpCodeUtil::GetOpCodeName(newObjInstr->m_opcode),
                    newObjInstr->IsProfiledInstr() ? L"constructor cache hasn't been cloned" : L"instruction is not profiled");
                Output::Flush();
            }

            return false;
        }
    }

    Assert(ctorCache != nullptr);

    // We should only have cloned if the script contexts match.
    Assert(newObjInstr->m_func->GetScriptContext() == ctorCache->scriptContext);

    // Built-in constructors don't need a default new object.  Since we know which constructor we're calling, we can skip creating a default
    // object and call a specialized helper (or even constructor, directly) avoiding the checks in generic NewScObjectCommon.
    if (ctorCache->skipNewScObject)
    {
        if (PHASE_TRACE(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()) || PHASE_TESTTRACE(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()))
        {
            Js::FunctionBody* callerFunctionBody = newObjInstr->m_func->GetJnFunction();
            const Js::JavascriptFunction* ctor = ctorCache->constructor;
            Js::FunctionBody* ctorBody = ctor->GetFunctionInfo()->HasBody() ? ctor->GetFunctionInfo()->GetFunctionBody() : nullptr;
            const wchar_t* ctorName = ctorBody != nullptr ? ctorBody->GetDisplayName() : L"<unknown>";

            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(L"FixedNewObj: function %s (%s): lowering skipped new script object for %s with %s ctor <unknown> (%s %s).\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer2), Js::OpCodeUtil::GetOpCodeName(newObjInstr->m_opcode),
                newObjInstr->m_opcode == Js::OpCode::NewScObjectNoCtor ? L"inlined" : L"called",
                ctorName, ctorBody ? ctorBody->GetDebugNumberSet(debugStringBuffer) : L"(null)");
            Output::Flush();
        }

        // All built-in constructors share a special singleton cache that is never checked and never invalidated.  It cannot be used
        // as a guard to protect any property operations downstream from the constructor.  If this ever becomes a performance issue,
        // we could have a dedicated cache for each built-in constructor, populate it and invalidate it as any other constructor cache.
        AssertMsg(!emitBailOut, "Can't bail out on constructor cache guard for built-in constructors.");

        skipNewScObj = true;
        IR::AddrOpnd* zeroOpnd = IR::AddrOpnd::NewNull(this->m_func);
        this->m_lowererMD.CreateAssign(newObjDst, zeroOpnd, newObjInstr);
        return true;
    }

    AssertMsg(ctorCache->type != nullptr, "Why did we hard-code a mismatched, invalidated or polymorphic constructor cache?");

    if (PHASE_TRACE(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()) || PHASE_TESTTRACE(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()))
    {
        Js::FunctionBody* callerFunctionBody = newObjInstr->m_func->GetJnFunction();
        const Js::JavascriptFunction* constructor = ctorCache->constructor;
        Js::FunctionBody* constructorBody = constructor->GetFunctionInfo()->HasBody() ? constructor->GetFunctionInfo()->GetFunctionBody() : nullptr;
        const wchar_t* constructorName = constructorBody != nullptr ? constructorBody->GetDisplayName() : L"<unknown>";

        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

        if (PHASE_TRACE(Js::FixedNewObjPhase, newObjInstr->m_func->GetJnFunction()))
        {
            Output::Print(L"FixedNewObj: function %s (%s): lowering fixed new script object for %s with %s ctor <unknown> (%s %s): type = %p, slots = %d, inlined slots = %d.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer2), Js::OpCodeUtil::GetOpCodeName(newObjInstr->m_opcode),
                newObjInstr->m_opcode == Js::OpCode::NewScObjectNoCtor ? L"inlined" : L"called",
                constructorName, constructorBody ? constructorBody->GetDebugNumberSet(debugStringBuffer) : L"(null)",
                ctorCache->type, ctorCache->slotCount, ctorCache->inlineSlotCount);
        }
        else
        {
            Output::Print(L"FixedNewObj: function %s (%s): lowering fixed new script object for %s with %s ctor <unknown> (%s %s): slots = %d, inlined slots = %d.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer2), Js::OpCodeUtil::GetOpCodeName(newObjInstr->m_opcode),
                newObjInstr->m_opcode == Js::OpCode::NewScObjectNoCtor ? L"inlined" : L"called",
                constructorName, debugStringBuffer, ctorCache->slotCount, ctorCache->inlineSlotCount);
        }
        Output::Flush();
    }

    // If the constructor has no return statements, we can safely return the object that was created here.
    // No need to check what the constructor returned - it must be undefined.
    returnNewScObj = ctorCache->ctorHasNoExplicitReturnValue;

    Assert(Js::ConstructorCache::GetSizeOfGuardValue() == static_cast<size_t>(TySize[TyMachPtr]));
    IR::MemRefOpnd* guardOpnd = IR::MemRefOpnd::New(const_cast<void*>(ctorCache->runtimeCache->GetAddressOfGuardValue()), TyMachReg, this->m_func,
        IR::AddrOpndKindDynamicGuardValueRef);
    IR::AddrOpnd* zeroOpnd = IR::AddrOpnd::NewNull(this->m_func);
    InsertCompareBranch(guardOpnd, zeroOpnd, Js::OpCode::BrEq_A, helperOrBailoutLabel, newObjInstr);

    // If we are calling new on a class constructor, the contract is that we pass new.target as the 'this' argument.
    // function is the constructor on which we called new - which is new.target.
    Js::JavascriptFunction* ctor = newObjInstr->GetFixedFunction();
    Js::FunctionInfo* functionInfo = Js::JavascriptOperators::GetConstructorFunctionInfo(ctor, this->m_func->GetScriptContext());
    Assert(functionInfo);

    if (functionInfo->IsClassConstructor())
    {
        // MOV newObjDst, function
        this->m_lowererMD.CreateAssign(newObjDst, newObjInstr->GetSrc1(), newObjInstr);
    }
    else
    {
        const Js::DynamicType* newObjectType = ctorCache->type;
        Assert(newObjectType->GetIsShared());

        IR::AddrOpnd* typeSrc = IR::AddrOpnd::New(const_cast<void *>(reinterpret_cast<const void *>(newObjectType)), IR::AddrOpndKindDynamicType, m_func);

        // For the next call:
        //     inlineSlotSize == Number of slots to allocate beyond the DynamicObject header
        //     slotSize - inlineSlotSize == Number of aux slots to allocate
        int inlineSlotSize = ctorCache->inlineSlotCount;
        int slotSize = ctorCache->slotCount;
        if (newObjectType->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler())
        {
            Assert(inlineSlotSize >= Js::DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity());
            Assert(inlineSlotSize == slotSize);
            slotSize = inlineSlotSize -= Js::DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity();
        }
        GenerateDynamicObjectAlloc(newObjInstr, inlineSlotSize, slotSize, newObjDst, typeSrc);
    }

    // JMP $callCtor
    IR::BranchInstr *callCtorBranch = IR::BranchInstr::New(Js::OpCode::Br, callCtorLabel, m_func);
    newObjInstr->InsertBefore(callCtorBranch);
    this->m_lowererMD.LowerUncondBranch(callCtorBranch);

    return true;
}

void
Lowerer::GenerateRecyclerAllocAligned(IR::JnHelperMethod allocHelper, size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, bool inOpHelper)
{
    IR::LabelInstr * allocDoneLabel = nullptr;

    if (!PHASE_OFF(Js::JitAllocNewObjPhase, insertionPointInstr->m_func->GetJnFunction()) && HeapInfo::IsSmallObject(allocSize))
    {
        IR::LabelInstr * allocHelperLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        allocDoneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, inOpHelper);

        this->m_lowererMD.GenerateFastRecyclerAlloc(allocSize, newObjDst, insertionPointInstr, allocHelperLabel, allocDoneLabel);

        // $allocHelper:
        insertionPointInstr->InsertBefore(allocHelperLabel);
    }

    // call JavascriptOperators::AllocMemForScObject(allocSize, scriptContext->GetRecycler())
    this->m_lowererMD.LoadHelperArgument(insertionPointInstr, this->LoadScriptContextValueOpnd(insertionPointInstr, ScriptContextValue::ScriptContextRecycler));
    this->m_lowererMD.LoadHelperArgument(insertionPointInstr, IR::IntConstOpnd::New((int32)allocSize, TyUint32, m_func, true));
    IR::Instr *newObjCall = IR::Instr::New(Js::OpCode::Call, newObjDst,  IR::HelperCallOpnd::New(allocHelper, m_func), m_func);
    insertionPointInstr->InsertBefore(newObjCall);
    this->m_lowererMD.LowerCall(newObjCall, 0);

    if (allocDoneLabel != nullptr)
    {
        // $allocDone:
        insertionPointInstr->InsertBefore(allocDoneLabel);
    }
}

IR::Instr *
Lowerer::LowerGetNewScObject(IR::Instr *instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::GetNewScObject);
    Assert(instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc2());

    const auto instrPrev = instr->m_prev;
    Assert(instrPrev);

    LowerGetNewScObjectCommon(
        instr->GetDst()->AsRegOpnd(),
        instr->GetSrc1()->AsRegOpnd(),
        instr->GetSrc2()->AsRegOpnd(),
        instr);
    instr->Remove();

    return instrPrev;
}

void
Lowerer::LowerGetNewScObjectCommon(
    IR::RegOpnd *const resultObjOpnd,
    IR::RegOpnd *const constructorReturnOpnd,
    IR::RegOpnd *const newObjOpnd,
    IR::Instr *insertBeforeInstr)
{
    Assert(resultObjOpnd);
    Assert(constructorReturnOpnd);
    Assert(newObjOpnd);
    Assert(insertBeforeInstr);

    // (newObjOpnd == 'this' value passed to constructor)
    //
    // if (!IsJsObject(constructorReturnOpnd))
    //     goto notObjectLabel
    // newObjOpnd = constructorReturnOpnd
    // notObjectLabel:
    // resultObjOpnd = newObjOpnd

    if(!constructorReturnOpnd->IsEqual(newObjOpnd))
    {
        // Need to check whether the constructor returned an object

        IR::LabelInstr *notObjectLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
        Assert(insertBeforeInstr->m_prev);
        IR::LabelInstr *const doneLabel =  IR::LabelInstr::New(Js::OpCode::Label, m_func);
        insertBeforeInstr->InsertBefore(doneLabel);
        insertBeforeInstr = doneLabel;

#if defined(_M_ARM32_OR_ARM64)
        m_lowererMD.LoadHelperArgument(insertBeforeInstr, constructorReturnOpnd);

        IR::Opnd * targetOpnd = IR::RegOpnd::New(StackSym::New(TyInt32,m_func), TyInt32, m_func);
        IR::Instr * callIsObjectInstr = IR::Instr::New(Js::OpCode::Call, targetOpnd, m_func);
        insertBeforeInstr->InsertBefore(callIsObjectInstr);
        this->m_lowererMD.ChangeToHelperCall(callIsObjectInstr, IR::HelperOp_IsObject);

        InsertTestBranch( targetOpnd, targetOpnd, Js::OpCode::BrEq_A, notObjectLabel,insertBeforeInstr);
#else
        m_lowererMD.GenerateIsJsObjectTest(constructorReturnOpnd, insertBeforeInstr, notObjectLabel);
#endif

        // Value returned by constructor is an object (use constructorReturnOpnd)
        if(!resultObjOpnd->IsEqual(constructorReturnOpnd))
        {
            this->m_lowererMD.CreateAssign(resultObjOpnd, constructorReturnOpnd, insertBeforeInstr);
        }
        insertBeforeInstr->InsertBefore(
            m_lowererMD.LowerUncondBranch(IR::BranchInstr::New(Js::OpCode::Br, doneLabel, m_func)));

        // Value returned by constructor is not an object (use newObjOpnd)
        insertBeforeInstr->InsertBefore(notObjectLabel);
    }

    if(!resultObjOpnd->IsEqual(newObjOpnd))
    {
        this->m_lowererMD.CreateAssign(resultObjOpnd, newObjOpnd, insertBeforeInstr);
    }

    // fall through to insertBeforeInstr or doneLabel
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerUpdateNewScObjectCache
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerUpdateNewScObjectCache(IR::Instr * insertInstr, IR::Opnd *dst, IR::Opnd *src1, const bool isCtorFunction)
{
    // if (!isCtorFunction)
    // {
    //     MOV r1, [src1 + offset(type)]       -- check base TypeIds_Function
    //     CMP [r1 + offset(typeId)], TypeIds_Function
    // }
    // JNE $fallThru
    // MOV r2, [src1 + offset(constructorCache)]
    // MOV r3, [r2 + offset(updateAfterCtor)]
    // TEST r3, r3                         -- check if updateAfterCtor is 0
    // JEQ $fallThru
    // CALL UpdateNewScObjectCache(src1, dst, scriptContext)
    // $fallThru:
    IR::LabelInstr *labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, m_func);

    if (!src1->IsRegOpnd())
    {
        IR::RegOpnd *srcRegOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
        LowererMD::CreateAssign(srcRegOpnd, src1, insertInstr);
        src1 = srcRegOpnd;
    }

    // Check if constructor is a function if we don't already know it.
    if (!isCtorFunction)
    {
        //  MOV r1, [src1 + offset(type)]       -- check base TypeIds_Function
        IR::RegOpnd *r1 = IR::RegOpnd::New(TyMachReg, this->m_func);
        IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(src1->AsRegOpnd(), Js::RecyclableObject::GetOffsetOfType(), TyMachReg, this->m_func);
        LowererMD::CreateAssign(r1, indirOpnd, insertInstr);

        // CMP [r1 + offset(typeId)], TypeIds_Function
        // JNE $fallThru
        indirOpnd = IR::IndirOpnd::New(r1, Js::Type::GetOffsetOfTypeId(), TyInt32, this->m_func);
        IR::IntConstOpnd *intOpnd = IR::IntConstOpnd::New(Js::TypeIds_Function, TyInt32, this->m_func, true);
        InsertCompareBranch(indirOpnd, intOpnd, Js::OpCode::BrNeq_A, labelFallThru, insertInstr);
    }

    // Every function has a constructor cache, even if only the default blank one.
    // r2 = MOV JavascriptFunction->constructorCache
    IR::RegOpnd *r2 = IR::RegOpnd::New(TyVar, this->m_func);
    IR::IndirOpnd *opndIndir = IR::IndirOpnd::New(src1->AsRegOpnd(), Js::JavascriptFunction::GetOffsetOfConstructorCache(), TyMachReg, this->m_func);
    IR::Instr *instr = LowererMD::CreateAssign(r2, opndIndir, insertInstr);

    // r3 = constructorCache->updateAfterCtor
    IR::RegOpnd *r3 = IR::RegOpnd::New(TyInt8, this->m_func);
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(r2, Js::ConstructorCache::GetOffsetOfUpdateAfterCtor(), TyUint8, this->m_func);
    instr = LowererMD::CreateAssign(r3, indirOpnd, insertInstr);

    // TEST r3, r3                         -- check if updateAfterCtor is 0
    // JEQ $fallThru
    InsertTestBranch(r3, r3, Js::OpCode::BrEq_A, labelFallThru, insertInstr);

    // r2 = UpdateNewScObjectCache(src1, dst, scriptContext)
    insertInstr->InsertBefore(IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true)); // helper label for uncommon path
    IR::HelperCallOpnd * opndHelper = IR::HelperCallOpnd::New(IR::HelperUpdateNewScObjectCache, m_func);

    LoadScriptContext(insertInstr);
    m_lowererMD.LoadHelperArgument(insertInstr, dst);
    m_lowererMD.LoadHelperArgument(insertInstr, src1);

    instr = IR::Instr::New(Js::OpCode::Call, m_func);
    instr->SetSrc1(opndHelper);
    insertInstr->InsertBefore(instr);
    m_lowererMD.LowerCall(instr, 0);

    // $fallThru:
    insertInstr->InsertBefore(labelFallThru);

    return insertInstr;
}

IR::Instr *
Lowerer::LowerNewScObjArray(IR::Instr *newObjInstr)
{
    IR::Instr* startCallInstr;
    if (newObjInstr->HasEmptyArgOutChain(&startCallInstr))
    {
        newObjInstr->FreeSrc2();
        return LowerNewScObjArrayNoArg(newObjInstr);
    }

    IR::Instr* startMarkerInstr = nullptr;

    IR::Opnd *targetOpnd = newObjInstr->GetSrc1();
    Func *func = newObjInstr->m_func;

    if (!targetOpnd->IsAddrOpnd())
    {
        if (!newObjInstr->HasBailOutInfo())
        {
            return this->LowerNewScObject(newObjInstr, true, true);
        }

        // Insert a temporary label before the instruction we're about to lower, so that we can return
        // the first instruction above that needs to be lowered after we're done - regardless of argument
        // list, StartCall, etc.
        startMarkerInstr = InsertLoweredRegionStartMarker(newObjInstr);

        // For whatever reason, we couldn't do a fixed function check on the call target.
        // Generate a runtime check on the target.
        Assert(newObjInstr->GetBailOutKind() == IR::BailOutOnNotNativeArray);
        IR::LabelInstr *labelSkipBailOut = IR::LabelInstr::New(Js::OpCode::Label, func);
        InsertCompareBranch(
            targetOpnd,
            LoadLibraryValueOpnd(newObjInstr, LibraryValue::ValueArrayConstructor),
            Js::OpCode::BrEq_A,
            true,
            labelSkipBailOut,
            newObjInstr);

        IR::ProfiledInstr *instrNew = IR::ProfiledInstr::New(newObjInstr->m_opcode, newObjInstr->UnlinkDst(), newObjInstr->UnlinkSrc1(), newObjInstr->UnlinkSrc2(), func);
        instrNew->u.profileId = newObjInstr->AsProfiledInstr()->u.profileId;
        newObjInstr->InsertAfter(instrNew);
        newObjInstr->m_opcode = Js::OpCode::BailOut;
        GenerateBailOut(newObjInstr);

        instrNew->InsertBefore(labelSkipBailOut);
        newObjInstr = instrNew;
    }
    else
    {
        // Insert a temporary label before the instruction we're about to lower, so that we can return
        // the first instruction above that needs to be lowered after we're done - regardless of argument
        // list, StartCall, etc.
        startMarkerInstr = InsertLoweredRegionStartMarker(newObjInstr);
    }

    RecyclerWeakReference<Js::FunctionBody> *weakFuncRef = nullptr;
    Js::ArrayCallSiteInfo *arrayInfo = nullptr;
    Assert(newObjInstr->IsProfiledInstr());

    IR::RegOpnd *resultObjOpnd = newObjInstr->GetDst()->AsRegOpnd();
    IR::Instr * insertInstr = newObjInstr->m_next;

    Js::ProfileId profileId = static_cast<Js::ProfileId>(newObjInstr->AsProfiledInstr()->u.profileId);

    // We may not have profileId if we converted a NewScObject to NewScObjArray
    if (profileId != Js::Constants::NoProfileId)
    {
        Js::FunctionBody *functionBody = func->GetJnFunction();
        arrayInfo = functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);
        Assert(arrayInfo);
        weakFuncRef = func->GetWeakFuncRef();
        Assert(weakFuncRef);
    }

    IR::Opnd *opndSrc1 = newObjInstr->UnlinkSrc1();
    if (opndSrc1->IsImmediateOpnd())
    {
        intptr_t length = opndSrc1->GetImmediateValue();
        if (length >= 0 && length <= 8)
        {
            GenerateProfiledNewScObjArrayFastPath(newObjInstr, arrayInfo, weakFuncRef, (uint32)length);
        }
    }

    IR::Opnd *profileOpnd = IR::AddrOpnd::New(arrayInfo, IR::AddrOpndKindDynamicArrayCallSiteInfo, func);
    this->m_lowererMD.LoadNewScObjFirstArg(newObjInstr, profileOpnd);

    IR::JnHelperMethod helperMethod = IR::HelperScrArr_ProfiledNewInstance;

    newObjInstr->SetSrc1(IR::HelperCallOpnd::New(helperMethod, func));
    newObjInstr = GenerateDirectCall(newObjInstr, targetOpnd, Js::CallFlags_New);

    IR::LabelInstr *labelDone = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertCompareBranch(
        IR::IndirOpnd::New(resultObjOpnd, 0, TyMachPtr, func),
        LoadVTableValueOpnd(insertInstr, VTableValue::VtableJavascriptArray),
        Js::OpCode::BrEq_A,
        true,
        labelDone,
        insertInstr);
    // We know we have a native array, so store the weak ref and call site index.
    m_lowererMD.CreateAssign(
        IR::IndirOpnd::New(resultObjOpnd, Js::JavascriptNativeArray::GetOffsetOfArrayCallSiteIndex(), TyUint16, func),
        IR::Opnd::CreateProfileIdOpnd(profileId, func),
        insertInstr);
    m_lowererMD.CreateAssign(
        IR::IndirOpnd::New(resultObjOpnd, Js::JavascriptNativeArray::GetOffsetOfWeakFuncRef(), TyMachReg, func),
        IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, func),
        insertInstr);
    insertInstr->InsertBefore(labelDone);

    return RemoveLoweredRegionStartMarker(startMarkerInstr);
}

IR::Instr *
Lowerer::LowerNewScObjArrayNoArg(IR::Instr *newObjInstr)
{
    IR::Opnd *targetOpnd = newObjInstr->GetSrc1();
    Func *func = newObjInstr->m_func;

    IR::Instr* startMarkerInstr = nullptr;

    if (!targetOpnd->IsAddrOpnd())
    {
        if (!newObjInstr->HasBailOutInfo())
        {
            return this->LowerNewScObject(newObjInstr, true, false);
        }

        // Insert a temporary label before the instruction we're about to lower, so that we can return
        // the first instruction above that needs to be lowered after we're done - regardless of argument
        // list, StartCall, etc.
        startMarkerInstr = InsertLoweredRegionStartMarker(newObjInstr);

        // For whatever reason, we couldn't do a fixed function check on the call target.
        // Generate a runtime check on the target.
        Assert(newObjInstr->GetBailOutKind() == IR::BailOutOnNotNativeArray);
        IR::LabelInstr *labelSkipBailOut = IR::LabelInstr::New(Js::OpCode::Label, func);
        InsertCompareBranch(
            targetOpnd,
            LoadLibraryValueOpnd(newObjInstr, LibraryValue::ValueArrayConstructor),
            Js::OpCode::BrEq_A,
            true,
            labelSkipBailOut,
            newObjInstr);

        IR::ProfiledInstr *instrNew = IR::ProfiledInstr::New(newObjInstr->m_opcode, newObjInstr->UnlinkDst(), newObjInstr->UnlinkSrc1(), func);
        instrNew->u.profileId = newObjInstr->AsProfiledInstr()->u.profileId;
        newObjInstr->InsertAfter(instrNew);
        newObjInstr->m_opcode = Js::OpCode::BailOut;
        GenerateBailOut(newObjInstr);

        instrNew->InsertBefore(labelSkipBailOut);
        newObjInstr = instrNew;
    }
    else
    {
        // Insert a temporary label before the instruction we're about to lower, so that we can return
        // the first instruction above that needs to be lowered after we're done - regardless of argument
        // list, StartCall, etc.
        startMarkerInstr = InsertLoweredRegionStartMarker(newObjInstr);
    }

    Assert(newObjInstr->IsProfiledInstr());

    RecyclerWeakReference<Js::FunctionBody> *weakFuncRef = nullptr;
    Js::ArrayCallSiteInfo *arrayInfo = nullptr;
    Js::ProfileId profileId = static_cast<Js::ProfileId>(newObjInstr->AsProfiledInstr()->u.profileId);
    if (profileId != Js::Constants::NoProfileId)
    {
        Js::FunctionBody *functionBody = func->GetJnFunction();
        arrayInfo = functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);
        Assert(arrayInfo);
        weakFuncRef = func->GetWeakFuncRef();
        Assert(weakFuncRef);
    }

    GenerateProfiledNewScObjArrayFastPath(newObjInstr, arrayInfo, weakFuncRef, 0);

    m_lowererMD.LoadHelperArgument(newObjInstr, IR::AddrOpnd::New(weakFuncRef, IR::AddrOpndKindDynamicFunctionBodyWeakRef, func));
    m_lowererMD.LoadHelperArgument(newObjInstr, IR::AddrOpnd::New(arrayInfo, IR::AddrOpndKindDynamicArrayCallSiteInfo, func));

    LoadScriptContext(newObjInstr);

    m_lowererMD.LoadHelperArgument(newObjInstr, targetOpnd);
    newObjInstr->UnlinkSrc1();
    newObjInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperScrArr_ProfiledNewInstanceNoArg, func));
    m_lowererMD.LowerCall(newObjInstr, 0);

    return RemoveLoweredRegionStartMarker(startMarkerInstr);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerPrologEpilog
///
///----------------------------------------------------------------------------
void
Lowerer::LowerPrologEpilog()
{
    if (m_func->GetJnFunction()->IsGenerator())
    {
        LowerGeneratorResumeJumpTable();
    }

    IR::Instr * instr;

    instr = m_func->m_headInstr;
    AssertMsg(instr->IsEntryInstr(), "First instr isn't an EntryInstr...");

    m_lowererMD.LowerEntryInstr(instr->AsEntryInstr());

    instr = m_func->m_exitInstr;
    AssertMsg(instr->IsExitInstr(), "Last instr isn't an ExitInstr...");

    m_lowererMD.LowerExitInstr(instr->AsExitInstr());
}

void
Lowerer::LowerPrologEpilogAsmJs()
{
    IR::Instr * instr;

    instr = m_func->m_headInstr;
    AssertMsg(instr->IsEntryInstr(), "First instr isn't an EntryInstr...");

    m_lowererMD.LowerEntryInstrAsmJs(instr->AsEntryInstr());

    instr = m_func->m_exitInstr;
    AssertMsg(instr->IsExitInstr(), "Last instr isn't an ExitInstr...");

    m_lowererMD.LowerExitInstrAsmJs(instr->AsExitInstr());
}

void
Lowerer::LowerGeneratorResumeJumpTable()
{
    Assert(m_func->GetJnFunction()->IsGenerator());

    IR::Instr * jumpTableInstr = m_func->m_headInstr;
    AssertMsg(jumpTableInstr->IsEntryInstr(), "First instr isn't an EntryInstr...");

    // Hope to do away with this linked list scan by moving this lowering to a post-prolog-epilog/pre-encoder phase that is common to all architectures (currently such phase is only available on amd64/arm)
    while (jumpTableInstr->m_opcode != Js::OpCode::GeneratorResumeJumpTable)
    {
        jumpTableInstr = jumpTableInstr->m_next;
    }

    IR::Opnd * srcOpnd = jumpTableInstr->UnlinkSrc1();

    m_func->MapYieldOffsetResumeLabels([&](int i, const YieldOffsetResumeLabel& yorl)
    {
        uint32 offset = yorl.First();
        IR::LabelInstr * label = yorl.Second();

        if (label != nullptr && label->m_hasNonBranchRef)
        {
            // Also fix up the bailout at the label with the jump to epilog that was not emitted in GenerateBailOut()
            Assert(label->m_prev->HasBailOutInfo());
            GenerateJumpToEpilogForBailOut(label->m_prev->GetBailOutInfo(), label->m_prev);
        }
        else if (label == nullptr)
        {
            label = m_func->m_bailOutNoSaveLabel;
        }

        // For each offset label pair, insert a compare of the offset and branch if equal to the label
        InsertCompareBranch(srcOpnd, IR::IntConstOpnd::New(offset, TyUint32, m_func), Js::OpCode::BrSrEq_A, label, jumpTableInstr);
    });

    jumpTableInstr->Remove();
}

void
Lowerer::DoInterruptProbes()
{
    this->m_func->SetHasInstrNumber(true);
    uint instrCount = 1;
    FOREACH_INSTR_IN_FUNC(instr, this->m_func)
    {
        instr->SetNumber(instrCount++);
        if (instr->IsLabelInstr())
        {
            IR::LabelInstr *labelInstr = instr->AsLabelInstr();
            if (labelInstr->m_isLoopTop)
            {
                // For every loop top label, insert the following:

                //   cmp sp, ThreadContext::stackLimitForCurrentThread
                //   bgt $continue
                // $helper:
                //   call JavascriptOperators::ScriptAbort
                //   b $exit
                // $continue:

                IR::LabelInstr *newLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
                labelInstr->InsertAfter(newLabel);
                this->InsertOneLoopProbe(newLabel, newLabel);
            }
        }
    }
    NEXT_INSTR_IN_FUNC;
}

// Insert an interrupt probe at each loop back branch. (Currently uncalled, since we're inserting
// probes at loop tops instead of back edges, but kept around because it may prove useful.)
uint
Lowerer::DoLoopProbeAndNumber(IR::BranchInstr *branchInstr)
{
    IR::LabelInstr *labelInstr = branchInstr->GetTarget();
    if (labelInstr == nullptr || labelInstr->GetNumber() == 0)
    {
        // Forward branch (possibly an indirect jump after try-catch-finally); nothing to do.
        return branchInstr->GetNumber() + 1;
    }

    Assert(labelInstr->m_isLoopTop);

    // Insert a stack probe at this branch. Number all the instructions we insert
    // and return the next instruction number.

    uint number = branchInstr->GetNumber();
    IR::Instr *instrPrev = branchInstr->m_prev;
    IR::Instr *instrNext = branchInstr->m_next;
    if (branchInstr->IsUnconditional())
    {
        // B $loop ==>

        // cmp [], 0
        // beq $loop
        // $helper:
        // call abort
        // b $exit

        this->InsertOneLoopProbe(branchInstr, labelInstr);
        branchInstr->Remove();
    }
    else
    {
        // Bcc $loop ==>

        // Binv $notloop
        // cmp [], 0
        // beq $loop
        // $helper:
        // call abort
        // b $exit
        // $notloop:

        IR::LabelInstr *loopExitLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        branchInstr->SetTarget(loopExitLabel);
        LowererMD::InvertBranch(branchInstr);
        branchInstr->InsertAfter(loopExitLabel);

        this->InsertOneLoopProbe(loopExitLabel, labelInstr);
    }

    FOREACH_INSTR_IN_RANGE(instr, instrPrev->m_next, instrNext->m_prev)
    {
        instr->SetNumber(number++);
    }
    NEXT_INSTR_IN_RANGE;

    return number;
}

void
Lowerer::InsertOneLoopProbe(IR::Instr *insertInstr, IR::LabelInstr *loopLabel)
{
    // Insert one interrupt probe at the given instruction. Probe the stack and call the abort helper
    // directly if the probe fails.

    IR::Opnd *memRefOpnd = IR::MemRefOpnd::New(
        this->m_func->GetScriptContext()->GetThreadContext()->GetAddressOfStackLimitForCurrentThread(),
        TyMachReg, this->m_func);

    IR::RegOpnd *regStackPointer = IR::RegOpnd::New(
        NULL, this->m_lowererMD.GetRegStackPointer(), TyMachReg, this->m_func);

    InsertCompareBranch(regStackPointer, memRefOpnd, Js::OpCode::BrGt_A, loopLabel, insertInstr);

    IR::LabelInstr *helperLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    insertInstr->InsertBefore(helperLabel);

    IR::HelperCallOpnd *helperOpnd = IR::HelperCallOpnd::New(IR::HelperScriptAbort, this->m_func);
    IR::Instr *instr = IR::Instr::New(Js::OpCode::Call, this->m_func);
    instr->SetSrc1(helperOpnd);
    insertInstr->InsertBefore(instr);
    this->m_lowererMD.LowerCall(instr, 0);

    // Jump to the exit after the helper call. This instruction will never be reached, but the jump
    // indicates that nothing is live after the call (to avoid useless spills in code that will
    // be executed).
    instr = this->m_func->m_exitInstr->GetPrevRealInstrOrLabel();
    if (instr->IsLabelInstr())
    {
        helperLabel = instr->AsLabelInstr();
    }
    else
    {
        helperLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        this->m_func->m_exitInstr->InsertBefore(helperLabel);
    }

    instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, helperLabel, this->m_func);
    insertInstr->InsertBefore(instr);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LoadPropertySymAsArgument
///
///     Generate code to pass a fieldSym as argument to a helper.
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LoadPropertySymAsArgument(IR::Instr *instr, IR::Opnd *fieldSrc)
{
    IR::Instr * instrPrev;
    AssertMsg(fieldSrc->IsSymOpnd() && fieldSrc->AsSymOpnd()->m_sym->IsPropertySym(), "Expected fieldSym as src of LdFld");

    IR::SymOpnd *symOpnd = fieldSrc->AsSymOpnd();
    PropertySym * fieldSym = symOpnd->m_sym->AsPropertySym();

    IR::IntConstOpnd * indexOpnd = IR::IntConstOpnd::New(fieldSym->m_propertyId, TyInt32, m_func, /*dontEncode*/true);
    instrPrev = m_lowererMD.LoadHelperArgument(instr, indexOpnd);

    IR::RegOpnd * instanceOpnd = symOpnd->CreatePropertyOwnerOpnd(m_func);
    m_lowererMD.LoadHelperArgument(instr, instanceOpnd);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LoadFunctionBodyAsArgument
///
///     Special case: the "property ID" is a key into the ScriptContext's FunctionBody map
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LoadFunctionBodyAsArgument(IR::Instr *instr, IR::IntConstOpnd * functionBodySlotOpnd, IR::RegOpnd * envOpnd)
{
    IR::Instr * instrPrev;

    // We need to pass in the function reference, we can't embed the pointer to the function proxy here.
    // The function proxy may be deferred parsed/serialize, and may 'progress' to a real function body after it is undeferred
    // At which point the deferred function proxy may be collect.
    // Just pass it the address where we will find the function proxy/body

    Js::FunctionProxyPtrPtr proxyRef = instr->m_func->GetJnFunction()->GetNestedFuncReference((uint)functionBodySlotOpnd->GetValue());
    AssertMsg(proxyRef, "Expected FunctionProxy for index of NewScFunc or NewScGenFunc opnd");
    AssertMsg(*proxyRef, "Expected FunctionProxy for index of NewScFunc or NewScGenFunc opnd");

    IR::AddrOpnd * indexOpnd = IR::AddrOpnd::New((Js::Var)proxyRef, IR::AddrOpndKindDynamicMisc, m_func);
    instrPrev = m_lowererMD.LoadHelperArgument(instr, indexOpnd);

    m_lowererMD.LoadHelperArgument(instr, envOpnd);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerProfiledLdFld(IR::JitProfilingInstr *ldFldInstr)
{
    const auto instrPrev = ldFldInstr->m_prev;

    auto src = ldFldInstr->UnlinkSrc1();
    AssertMsg(src->IsSymOpnd() && src->AsSymOpnd()->m_sym->IsPropertySym(), "Expected property sym as src");

    IR::JnHelperMethod helper;
    switch (ldFldInstr->m_opcode)
    {
        case Js::OpCode::LdFld:
            helper = IR::HelperProfiledLdFld;
            goto ldFldCommon;
        case Js::OpCode::LdRootFld:
            helper = IR::HelperProfiledLdRootFld;
            goto ldFldCommon;
        case Js::OpCode::LdMethodFld:
            helper = IR::HelperProfiledLdMethodFld;
            goto ldFldCommon;
        case Js::OpCode::LdRootMethodFld:
            helper = IR::HelperProfiledLdRootMethodFld;
            goto ldFldCommon;
        case Js::OpCode::LdFldForCallApplyTarget:
            helper = IR::HelperProfiledLdFld_CallApplyTarget;
            goto ldFldCommon;
        case Js::OpCode::LdFldForTypeOf:
            helper = IR::HelperProfiledLdFldForTypeOf;
            goto ldFldCommon;
        case Js::OpCode::LdRootFldForTypeOf:
            helper = IR::HelperProfiledLdRootFldForTypeOf;
            goto ldFldCommon;

ldFldCommon:
        {
            Assert(ldFldInstr->profileId == Js::Constants::NoProfileId);

            /*
                Var ProfilingHelpers::ProfiledLdFld_Jit(
                    const Var instance,
                    const PropertyId propertyId,
                    const InlineCacheIndex inlineCacheIndex,
                    void *const framePointer)
            */

            m_lowererMD.LoadHelperArgument(ldFldInstr, IR::Opnd::CreateFramePointerOpnd(m_func));
            m_lowererMD.LoadHelperArgument(
                ldFldInstr,
                IR::Opnd::CreateInlineCacheIndexOpnd(src->AsPropertySymOpnd()->m_inlineCacheIndex, m_func));
            LoadPropertySymAsArgument(ldFldInstr, src);
            break;
        }

        case Js::OpCode::LdSuperFld:
        {
            Assert(ldFldInstr->profileId == Js::Constants::NoProfileId);

            IR::Opnd * src2 = nullptr;

            /*
                Var ProfilingHelpers::ProfiledLdSuperFld_Jit(
                    const Var instance,
                    const PropertyId propertyId,
                    const InlineCacheIndex inlineCacheIndex,
                    void *const framePointer,
                    const Var thisInstance)
            */

            src2 = ldFldInstr->UnlinkSrc2();

            m_lowererMD.LoadHelperArgument(ldFldInstr, src2 );
            m_lowererMD.LoadHelperArgument(ldFldInstr, IR::Opnd::CreateFramePointerOpnd(m_func));
            m_lowererMD.LoadHelperArgument(
                ldFldInstr,
                IR::Opnd::CreateInlineCacheIndexOpnd(src->AsPropertySymOpnd()->m_inlineCacheIndex, m_func));
            LoadPropertySymAsArgument(ldFldInstr, src);
            helper = IR::HelperProfiledLdSuperFld;
            break;
        }

        case Js::OpCode::LdLen_A:
            // If we want to profile this call, then push some extra args and call the profiling version
            m_lowererMD.LoadHelperArgument(ldFldInstr, IR::Opnd::CreateProfileIdOpnd(ldFldInstr->profileId, m_func));
            m_lowererMD.LoadHelperArgument(ldFldInstr, src->AsSymOpnd()->CreatePropertyOwnerOpnd(m_func));
            m_lowererMD.LoadHelperArgument(ldFldInstr, CreateFunctionBodyOpnd(ldFldInstr->m_func));
            helper = IR::HelperSimpleProfiledLdLen;
            break;

        default:
            Assert(false);
            __assume(false);
    }

    ldFldInstr->SetSrc1(IR::HelperCallOpnd::New(helper, m_func));
    m_lowererMD.LowerCall(ldFldInstr, 0);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerLdFld
///
/// Lower an instruction (LdFld, ScopedLdFld) that takes a property
/// reference as a source and puts a result in a register.
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerLdFld(
    IR::Instr * ldFldInstr,
    IR::JnHelperMethod helperMethod,
    IR::JnHelperMethod polymorphicHelperMethod,
    bool useInlineCache,
    IR::LabelInstr *labelBailOut,
    bool isHelper)
{
    if (ldFldInstr->IsJitProfilingInstr())
    {
        // If we want to profile then do something completely different
        return this->LowerProfiledLdFld(ldFldInstr->AsJitProfilingInstr());
    }

    IR::Opnd *src;
    IR::Instr *instrPrev = ldFldInstr->m_prev;

    src = ldFldInstr->UnlinkSrc1();
    if (ldFldInstr->m_opcode == Js::OpCode::LdSuperFld)
    {
        IR::Opnd * src2 = nullptr;
        src2 = ldFldInstr->UnlinkSrc2();
        m_lowererMD.LoadHelperArgument(ldFldInstr, src2);
    }

    AssertMsg(src->IsSymOpnd() && src->AsSymOpnd()->m_sym->IsPropertySym(), "Expected property sym as src");

    if (useInlineCache)
    {
        IR::Opnd * inlineCacheOpnd;
        AssertMsg(src->AsSymOpnd()->IsPropertySymOpnd(), "Need property sym operand to find the inline cache");
        if (src->AsPropertySymOpnd()->m_runtimePolymorphicInlineCache && polymorphicHelperMethod != helperMethod)
        {
            Js::PolymorphicInlineCache * polymorphicInlineCache = src->AsPropertySymOpnd()->m_runtimePolymorphicInlineCache;
            helperMethod = polymorphicHelperMethod;
            inlineCacheOpnd = IR::AddrOpnd::New(polymorphicInlineCache, IR::AddrOpndKindDynamicInlineCache, this->m_func);
        }
        else
        {
            // Need to load runtime inline cache opnd first before loading any helper argument
            // because LoadRuntimeInlineCacheOpnd may create labels marked as helper,
            // and cause op helper register push/pop save in x86, messing up with any helper arguments that is already pushed
            inlineCacheOpnd = this->LoadRuntimeInlineCacheOpnd(ldFldInstr, src->AsPropertySymOpnd(), isHelper);
        }
        this->LoadPropertySymAsArgument(ldFldInstr, src);
        this-> m_lowererMD.LoadHelperArgument(
            ldFldInstr,
            IR::Opnd::CreateInlineCacheIndexOpnd(src->AsPropertySymOpnd()->m_inlineCacheIndex, m_func));

        this->m_lowererMD.LoadHelperArgument(ldFldInstr, inlineCacheOpnd);
        this->m_lowererMD.LoadHelperArgument(ldFldInstr, LoadFunctionBodyOpnd(ldFldInstr));
    }
    else
    {
        LoadScriptContext(ldFldInstr);
        this->LoadPropertySymAsArgument(ldFldInstr, src);
    }

    // Do we need to reload the type and slot array after the helper returns?
    // (We do if there's a propertySymOpnd downstream that needs it, i.e., the type is not dead.)
    IR::RegOpnd *opndBase = src->AsSymOpnd()->CreatePropertyOwnerOpnd(m_func);
    m_lowererMD.ChangeToHelperCall(ldFldInstr, helperMethod, labelBailOut, opndBase, src->AsSymOpnd()->IsPropertySymOpnd() ? src->AsSymOpnd()->AsPropertySymOpnd() : nullptr, isHelper);

    return instrPrev;
}

bool
Lowerer::GenerateLdFldWithCachedType(IR::Instr * instrLdFld, bool* continueAsHelperOut, IR::LabelInstr** labelHelperOut, IR::RegOpnd** typeOpndOut)
{
    IR::Instr *instr;
    IR::Opnd *opnd;
    IR::LabelInstr *labelObjCheckFailed = nullptr;
    IR::LabelInstr *labelTypeCheckFailed = nullptr;
    IR::LabelInstr *labelDone = nullptr;

    Assert(continueAsHelperOut != nullptr);
    *continueAsHelperOut = false;

    Assert(labelHelperOut != nullptr);
    *labelHelperOut = nullptr;

    Assert(typeOpndOut != nullptr);
    *typeOpndOut = nullptr;

    Assert(instrLdFld->GetSrc1()->IsSymOpnd());
    if (!instrLdFld->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
    {
        return false;
    }

    IR::PropertySymOpnd *propertySymOpnd = instrLdFld->GetSrc1()->AsPropertySymOpnd();
    if (!propertySymOpnd->IsTypeCheckSeqCandidate())
    {
        return false;
    }

    AssertMsg(propertySymOpnd->TypeCheckSeqBitsSetOnlyIfCandidate(), "Property sym operand optimized despite not being a candidate?");

    if (!propertySymOpnd->IsTypeCheckSeqParticipant() && !propertySymOpnd->NeedsLocalTypeCheck())
    {
        return false;
    }

    Assert(!propertySymOpnd->NeedsTypeCheckAndBailOut() || (instrLdFld->HasBailOutInfo() && IR::IsTypeCheckBailOutKind(instrLdFld->GetBailOutKind())));

    // In the backwards pass we only add guarded property operations to instructions that are not already
    // protected by an upstream type check.
    Assert(!propertySymOpnd->IsTypeCheckProtected() || propertySymOpnd->GetGuardedPropOps() == nullptr);

    PHASE_PRINT_TESTTRACE(
        Js::ObjTypeSpecPhase,
        this->m_func,
        L"Field load: %s, property: %s, func: %s, cache ID: %d, cloned cache: true, layout: %s, redundant check: %s\n",
        Js::OpCodeUtil::GetOpCodeName(instrLdFld->m_opcode),
        this->m_func->GetScriptContext()->GetPropertyNameLocked(
            propertySymOpnd->m_sym->AsPropertySym()->m_propertyId)->GetBuffer(),
        this->m_func->GetJnFunction()->GetDisplayName(),
        propertySymOpnd->m_inlineCacheIndex,
        propertySymOpnd->GetCacheLayoutString(),
        propertySymOpnd->IsTypeChecked() ? L"true" : L"false");

    if (propertySymOpnd->HasFinalType() && !propertySymOpnd->IsLoadedFromProto())
    {
        propertySymOpnd->UpdateSlotForFinalType();
    }

    // TODO (ObjTypeSpec): If ((PropertySym*)propertySymOpnd->m_sym)->m_stackSym->m_isIntConst consider emitting a direct
    // jump to helper or bailout.  If we have a type check bailout, we could even abort compilation.

    bool hasTypeCheckBailout = instrLdFld->HasBailOutInfo() && IR::IsTypeCheckBailOutKind(instrLdFld->GetBailOutKind());

    // If the hard-coded type is not available here, do a type check, and branch to the helper if the check fails.
    // In the prototype case, we have to check the type even if it was checked upstream, to cover the case where
    // the property has been added locally. Note that this is not necessary if the proto chain has been checked,
    // because then we know there's been no store of the property since the type was checked.
    bool emitPrimaryTypeCheck = propertySymOpnd->NeedsPrimaryTypeCheck();
    bool emitLocalTypeCheck = propertySymOpnd->NeedsLocalTypeCheck();
    bool emitLoadFromProtoTypeCheck = propertySymOpnd->NeedsLoadFromProtoTypeCheck();

    if (emitPrimaryTypeCheck || emitLocalTypeCheck || emitLoadFromProtoTypeCheck)
    {
        if (emitLoadFromProtoTypeCheck)
        {
            propertySymOpnd->EnsureGuardedPropOps(this->m_func->m_alloc);
            propertySymOpnd->SetGuardedPropOp(propertySymOpnd->GetObjTypeSpecFldId());
        }
        labelTypeCheckFailed = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        labelObjCheckFailed = hasTypeCheckBailout ? labelTypeCheckFailed : IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        *typeOpndOut = this->GenerateCachedTypeCheck(instrLdFld, propertySymOpnd, labelObjCheckFailed, labelTypeCheckFailed);
    }

    IR::Opnd *opndSlotArray;
    if (propertySymOpnd->IsLoadedFromProto())
    {
        opndSlotArray = this->LoadSlotArrayWithCachedProtoType(instrLdFld, propertySymOpnd);
    }
    else
    {
        opndSlotArray = this->LoadSlotArrayWithCachedLocalType(instrLdFld, propertySymOpnd);
    }

    // Load the value from the slot, getting the slot ID from the cache.
    uint16 index = propertySymOpnd->GetSlotIndex();
    Assert(index != -1);

    if (opndSlotArray->IsRegOpnd())
    {
        opnd = IR::IndirOpnd::New(opndSlotArray->AsRegOpnd(), index * sizeof(Js::Var), TyMachReg, this->m_func);
    }
    else
    {
        Assert(opndSlotArray->IsMemRefOpnd());
        opnd = IR::MemRefOpnd::New((char*)opndSlotArray->AsMemRefOpnd()->GetMemLoc() + (index * sizeof(Js::Var)), TyMachReg, this->m_func, IR::AddrOpndKindDynamicPropertySlotRef);
    }
    Lowerer::InsertMove(instrLdFld->GetDst(), opnd, instrLdFld);

    // We eliminate the helper, or the type check succeeds, or we bail out before the operation.
    // Either delete the original instruction or replace it with a bailout.
    if (!emitPrimaryTypeCheck && !emitLocalTypeCheck && !emitLoadFromProtoTypeCheck)
    {
        Assert(labelTypeCheckFailed == nullptr);
        AssertMsg(!instrLdFld->HasBailOutInfo(), "Why does a direct field load have bailout?");
        instrLdFld->Remove();
        return true;
    }

    // Otherwise, branch around the bailout or helper.
    labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelDone, this->m_func);
    instrLdFld->InsertBefore(instr);

    // Insert the bailout or helper label here.
    instrLdFld->InsertBefore(labelTypeCheckFailed);
    instrLdFld->InsertAfter(labelDone);

    if (hasTypeCheckBailout)
    {
        AssertMsg(PHASE_ON1(Js::ObjTypeSpecIsolatedFldOpsWithBailOutPhase) || !propertySymOpnd->IsTypeDead(),
            "Why does a field load have a type check bailout, if its type is dead?");

        // Convert the original instruction to a bailout.
        if (instrLdFld->GetBailOutInfo()->bailOutInstr != instrLdFld)
        {
            // Set the cache index in the bailout info so that the bailout code will write it into the
            // bailout record at runtime.
            instrLdFld->GetBailOutInfo()->polymorphicCacheIndex = propertySymOpnd->m_inlineCacheIndex;
        }
        instrLdFld->FreeDst();
        instrLdFld->FreeSrc1();
        instrLdFld->m_opcode = Js::OpCode::BailOut;
        this->GenerateBailOut(instrLdFld);

        return true;
    }
    else
    {
        *continueAsHelperOut = true;
        Assert(labelObjCheckFailed != nullptr && labelObjCheckFailed != labelTypeCheckFailed);
        *labelHelperOut = labelObjCheckFailed;
        return false;
    }
}

template<bool isRoot>
IR::Instr* Lowerer::GenerateCompleteLdFld(IR::Instr* instr, bool emitFastPath, IR::JnHelperMethod monoHelperAfterFastPath, IR::JnHelperMethod polyHelperAfterFastPath,
    IR::JnHelperMethod monoHelperWithoutFastPath, IR::JnHelperMethod polyHelperWithoutFastPath)
{
    if(instr->CallsAccessor() && instr->HasBailOutInfo())
    {
        IR::BailOutKind kindMinusBits = instr->GetBailOutKind() & ~IR::BailOutKindBits;
        Assert(kindMinusBits != IR::BailOutOnImplicitCalls && kindMinusBits != IR::BailOutOnImplicitCallsPreOp);
    }

    IR::Instr* prevInstr = instr->m_prev;

    IR::LabelInstr* labelHelper = nullptr;
    IR::LabelInstr* labelBailOut = nullptr;
    bool isHelper = false;
    IR::RegOpnd* typeOpnd = nullptr;

    if (isRoot)
    {
        // Don't do the fast path here if emitFastPath is false, even if we can.
        if (emitFastPath && (this->GenerateLdFldWithCachedType(instr, &isHelper, &labelHelper, &typeOpnd) || this->GenerateNonConfigurableLdRootFld(instr)))
        {
            Assert(labelHelper == nullptr);
            return prevInstr;
        }
    }
    else
    {
        if (this->GenerateLdFldWithCachedType(instr, &isHelper, &labelHelper, &typeOpnd))
        {
            Assert(labelHelper == nullptr);
            return prevInstr;
        }
    }

    if (emitFastPath)
    {
        if (!GenerateFastLdFld(instr, monoHelperWithoutFastPath, polyHelperWithoutFastPath, &labelBailOut, typeOpnd, &isHelper, &labelHelper))
        {
            if (labelHelper != nullptr)
            {
                labelHelper->isOpHelper = isHelper;
                instr->InsertBefore(labelHelper);
            }
            prevInstr = LowerLdFld(instr, monoHelperAfterFastPath, polyHelperAfterFastPath, true, labelBailOut, isHelper);
        }
    }
    else
    {
        if (labelHelper != nullptr)
        {
            labelHelper->isOpHelper = isHelper;
            instr->InsertBefore(labelHelper);
        }
        prevInstr = LowerLdFld(instr, monoHelperWithoutFastPath, polyHelperWithoutFastPath, true, labelBailOut, isHelper);
    }

    return prevInstr;
}

bool
Lowerer::GenerateCheckFixedFld(IR::Instr * instrChkFld)
{
    IR::Instr *instr;
    IR::LabelInstr *labelBailOut = nullptr;
    IR::LabelInstr *labelDone = nullptr;

    AssertMsg(!PHASE_OFF(Js::FixedMethodsPhase, instrChkFld->m_func->GetJnFunction()) ||
        !PHASE_OFF(Js::UseFixedDataPropsPhase, instrChkFld->m_func->GetJnFunction()), "Lowering a check fixed field with fixed data/method phase disabled?");

    Assert(instrChkFld->GetSrc1()->IsSymOpnd() && instrChkFld->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd());
    IR::PropertySymOpnd *propertySymOpnd = instrChkFld->GetSrc1()->AsPropertySymOpnd();

    AssertMsg(propertySymOpnd->TypeCheckSeqBitsSetOnlyIfCandidate(), "Property sym operand optimized despite not being a candidate?");
    Assert(propertySymOpnd->MayNeedTypeCheckProtection());

    // In the backwards pass we only add guarded property operations to instructions that are not already
    // protected by an upstream type check.
    Assert(!propertySymOpnd->IsTypeCheckProtected() || propertySymOpnd->GetGuardedPropOps() == nullptr);

    // For the non-configurable properties on the global object we do not need a type check.  Otherwise,
    // we need a type check and bailout here unless this operation is part of the type check sequence and
    // is protected by a type check upstream.
    bool emitPrimaryTypeCheck = propertySymOpnd->NeedsPrimaryTypeCheck();
    // In addition, we may also need a local type check in case the property comes from the prototype and
    // it may have been overwritten on the instance after the primary type check upstream. If the property
    // comes from the instance, we must still protect against its value changing after the type check, but
    // for this a cheaper guard check is sufficient (see below).
    bool emitFixedFieldTypeCheck = propertySymOpnd->NeedsCheckFixedFieldTypeCheck() &&
        (!propertySymOpnd->IsTypeChecked() || propertySymOpnd->IsLoadedFromProto());

    PropertySym * propertySym = propertySymOpnd->m_sym->AsPropertySym();
    uint inlineCacheIndex = propertySymOpnd->m_inlineCacheIndex;

    OUTPUT_TRACE_FUNC(
        Js::ObjTypeSpecPhase,
        this->m_func,
        L"Fixed field check: %s, property: %s, cache ID: %u, cloned cache: true, layout: %s, redundant check: %s count of props: %u \n",
        Js::OpCodeUtil::GetOpCodeName(instrChkFld->m_opcode),
        this->m_func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer(),
        inlineCacheIndex, propertySymOpnd->GetCacheLayoutString(), propertySymOpnd->IsTypeChecked() ? L"true" : L"false",
        propertySymOpnd->GetGuardedPropOps() ? propertySymOpnd->GetGuardedPropOps()->Count() : 0);

    if (emitPrimaryTypeCheck || emitFixedFieldTypeCheck)
    {
        labelBailOut = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

        if(emitFixedFieldTypeCheck && propertySymOpnd->IsRootObjectNonConfigurableFieldLoad())
        {
            AssertMsg(!propertySymOpnd->GetGuardedPropOps() || propertySymOpnd->GetGuardedPropOps()->IsEmpty(), "This property Guard is used only for one property");
            //We need only cheaper Guard check, if the property belongs to the GlobalObject.
            GenerateFixedFieldGuardCheck(instrChkFld, propertySymOpnd, labelBailOut);
        }
        else
        {
            if (emitFixedFieldTypeCheck)
            {
                propertySymOpnd->EnsureGuardedPropOps(this->m_func->m_alloc);
                propertySymOpnd->SetGuardedPropOp(propertySymOpnd->GetObjTypeSpecFldId());
            }
            this->GenerateCachedTypeCheck(instrChkFld, propertySymOpnd, labelBailOut, labelBailOut);
        }
    }

    // We may still need this guard if we didn't emit the write protect type check above. This situation arises if we have
    // a fixed field from the instance (not proto) and a property of the same name has been written somewhere between the
    // primary type check and here. Note that we don't need a type check, because we know the fixed field exists on the
    // object even if it has been written since primary type check, but we need to verify the fixed value didn't get overwritten.
    if (!emitPrimaryTypeCheck && !emitFixedFieldTypeCheck && !propertySymOpnd->IsWriteGuardChecked())
    {
        if (!PHASE_OFF(Js::FixedFieldGuardCheckPhase, this->m_func))
        {
            Assert(labelBailOut == nullptr);
            labelBailOut = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            GenerateFixedFieldGuardCheck(instrChkFld, propertySymOpnd, labelBailOut);
        }
    }

    // Note that a type handler holds only a weak reference to the singleton instance it represents, so
    // it is possible that the instance gets collected before the type and handler do. Hence, the upstream
    // type check may succeed, even as the original instance no longer exists. However, this would happen
    // only if another instance reached the same type (otherwise we wouldn't ever pass the type check
    // upstream). In that case we would have invalidated all fixed fields on that type, and so the type
    // check (or property guard check, if necessary) above would fail. All in all, we would never attempt
    // to access a fixed field from an instance that has been collected.

    if (!emitPrimaryTypeCheck && !emitFixedFieldTypeCheck && propertySymOpnd->IsWriteGuardChecked())
    {
        Assert(labelBailOut == nullptr);
        AssertMsg(!instrChkFld->HasBailOutInfo(), "Why does a direct fixed field check have bailout?");
        instrChkFld->Remove();
        return true;
    }

    labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelDone, this->m_func);
    instrChkFld->InsertBefore(instr);

    // Insert the helper label here.
    instrChkFld->InsertBefore(labelBailOut);
    instrChkFld->InsertAfter(labelDone);

    // Convert the original instruction to a bailout.
    Assert(instrChkFld->HasBailOutInfo());

    if (instrChkFld->GetBailOutInfo()->bailOutInstr != instrChkFld)
    {
        // Set the cache index in the bailout info so that the bailout code will write it into the
        // bailout record at runtime.
        instrChkFld->GetBailOutInfo()->polymorphicCacheIndex = inlineCacheIndex;
    }

    instrChkFld->FreeSrc1();
    instrChkFld->m_opcode = Js::OpCode::BailOut;
    this->GenerateBailOut(instrChkFld);

    return true;
}

void
Lowerer::GenerateCheckObjType(IR::Instr * instrChkObjType)
{
    Assert(instrChkObjType->GetSrc1()->IsSymOpnd() && instrChkObjType->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd());
    IR::PropertySymOpnd *propertySymOpnd = instrChkObjType->GetSrc1()->AsPropertySymOpnd();

    // Why do we have an explicit type check if the cached type has been checked upstream?  The dead store pass should have
    // removed this instruction.
    Assert(propertySymOpnd->IsTypeCheckSeqCandidate() && !propertySymOpnd->IsTypeChecked());
    // Why do we have an explicit type check on a non-configurable root field load?
    Assert(!propertySymOpnd->IsRootObjectNonConfigurableFieldLoad());

    PropertySym * propertySym = propertySymOpnd->m_sym->AsPropertySym();
    uint inlineCacheIndex = propertySymOpnd->m_inlineCacheIndex;

    PHASE_PRINT_TESTTRACE(
        Js::ObjTypeSpecPhase,
        this->m_func,
        L"Object type check: %s, property: %s, func: %s, cache ID: %d, cloned cache: true, layout: %s, redundant check: %s\n",
        Js::OpCodeUtil::GetOpCodeName(instrChkObjType->m_opcode),
        this->m_func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer(),
        this->m_func->GetJnFunction()->GetDisplayName(),
        inlineCacheIndex, propertySymOpnd->GetCacheLayoutString(), L"false");

    IR::LabelInstr* labelBailOut = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    this->GenerateCachedTypeCheck(instrChkObjType, propertySymOpnd, labelBailOut, labelBailOut);

    IR::LabelInstr* labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    IR::Instr* instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelDone, this->m_func);
    instrChkObjType->InsertBefore(instr);

    // Insert the bailout label here.
    instrChkObjType->InsertBefore(labelBailOut);
    instrChkObjType->InsertAfter(labelDone);

    // Convert the original instruction to a bailout.
    Assert(instrChkObjType->HasBailOutInfo());

    if (instrChkObjType->GetBailOutInfo()->bailOutInstr != instrChkObjType)
    {
        // Set the cache index in the bailout info so that the bailout code will write it into the
        // bailout record at runtime.
        instrChkObjType->GetBailOutInfo()->polymorphicCacheIndex = inlineCacheIndex;
    }

    instrChkObjType->FreeSrc1();
    instrChkObjType->m_opcode = Js::OpCode::BailOut;
    this->GenerateBailOut(instrChkObjType);
}

void
Lowerer::LowerAdjustObjType(IR::Instr * instrAdjustObjType)
{
    IR::AddrOpnd *finalTypeOpnd = instrAdjustObjType->UnlinkDst()->AsAddrOpnd();
    IR::AddrOpnd *initialTypeOpnd = instrAdjustObjType->UnlinkSrc2()->AsAddrOpnd();
    IR::RegOpnd  *baseOpnd = instrAdjustObjType->UnlinkSrc1()->AsRegOpnd();

    this->GenerateAdjustBaseSlots(
        instrAdjustObjType, baseOpnd, (Js::Type*)initialTypeOpnd->m_address, (Js::Type*)finalTypeOpnd->m_address);

    this->m_func->PinTypeRef(finalTypeOpnd->m_address);

    IR::Opnd *opnd = IR::IndirOpnd::New(baseOpnd, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, instrAdjustObjType->m_func);
    this->m_lowererMD.CreateAssign(opnd, finalTypeOpnd, instrAdjustObjType);

    initialTypeOpnd->Free(instrAdjustObjType->m_func);
    instrAdjustObjType->Remove();
}

bool
Lowerer::GenerateNonConfigurableLdRootFld(IR::Instr * instrLdFld)
{
    if (!instrLdFld->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
    {
        return false;
    }
    IR::PropertySymOpnd *propertySymOpnd = instrLdFld->GetSrc1()->AsPropertySymOpnd();
    if (!propertySymOpnd->IsRootObjectNonConfigurableFieldLoad())
    {
        return false;
    }

    Assert(!PHASE_OFF(Js::RootObjectFldFastPathPhase, this->m_func->GetJnFunction()));
    Assert(!instrLdFld->HasBailOutInfo());
    IR::Opnd * srcOpnd;
    Js::RootObjectBase * rootObject = this->m_func->GetJnFunction()->GetRootObject();
    if (propertySymOpnd->UsesAuxSlot())
    {
        IR::RegOpnd * auxSlotOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
        this->InsertMove(auxSlotOpnd, IR::MemRefOpnd::New((byte *)rootObject + Js::DynamicObject::GetOffsetOfAuxSlots(),
                TyMachPtr, this->m_func), instrLdFld);

        srcOpnd = IR::IndirOpnd::New(auxSlotOpnd, propertySymOpnd->GetSlotIndex() * sizeof(Js::Var *),
            TyVar, this->m_func);
    }
    else
    {
        srcOpnd = IR::MemRefOpnd::New((Js::Var *)rootObject + propertySymOpnd->GetSlotIndex(),
            TyVar, this->m_func);
    }
    instrLdFld->ReplaceSrc1(srcOpnd);
    instrLdFld->m_opcode = Js::OpCode::Ld_A;
    LowererMD::ChangeToAssign(instrLdFld);
    return true;
}

IR::Instr *
Lowerer::LowerDelFld(IR::Instr *delFldInstr, IR::JnHelperMethod helperMethod, bool useInlineCache, bool strictMode)
{
    IR::Instr *instrPrev;

    Js::PropertyOperationFlags propertyOperationFlag = Js::PropertyOperation_None;

    if (strictMode)
    {
        propertyOperationFlag = Js::PropertyOperation_StrictMode;
    }

    instrPrev = m_lowererMD.LoadHelperArgument(delFldInstr, IR::IntConstOpnd::New((IntConstType)propertyOperationFlag, TyInt32, m_func, true));

    LowerLdFld(delFldInstr, helperMethod, helperMethod, useInlineCache);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerIsInst(IR::Instr * isInstInstr, IR::JnHelperMethod helperMethod)
{
    IR::Instr * instrPrev;
    IR::Instr * instrArg;
    IR::RegOpnd * argOpnd;

    // inlineCache
    instrPrev = m_lowererMD.LoadHelperArgument(isInstInstr, LoadIsInstInlineCacheOpnd(isInstInstr, isInstInstr->GetSrc1()->AsIntConstOpnd()->AsUint32()));
    isInstInstr->FreeSrc1();

    argOpnd = isInstInstr->UnlinkSrc2()->AsRegOpnd();
    Assert(argOpnd->m_sym->m_isSingleDef);
    instrArg = argOpnd->m_sym->m_instrDef;
    argOpnd->Free(m_func);

    // scriptContext
    LoadScriptContext(isInstInstr);

    // instance goes last, so remember it now
    IR::Opnd * instanceOpnd = instrArg->UnlinkSrc1();
    argOpnd = instrArg->UnlinkSrc2()->AsRegOpnd();
    Assert(argOpnd->m_sym->m_isSingleDef);
    instrArg->Remove();
    instrArg = argOpnd->m_sym->m_instrDef;
    argOpnd->Free(m_func);

    // function
    IR::Opnd *opnd = instrArg->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(isInstInstr, opnd);
    Assert(instrArg->GetSrc2() == NULL);
    instrArg->Remove();

    // instance
    m_lowererMD.LoadHelperArgument(isInstInstr, instanceOpnd);

    m_lowererMD.ChangeToHelperCall(isInstInstr, helperMethod);

    return instrPrev;
}

void
Lowerer::GenerateStackScriptFunctionInit(StackSym * stackSym, Js::FunctionProxyPtrPtr nestedProxy)
{
    Func * func = this->m_func;
    Assert(func->HasAnyStackNestedFunc());
    Assert(nextStackFunctionOpnd);

    IR::Instr * insertBeforeInstr = func->GetFunctionEntryInsertionPoint();

    IR::RegOpnd * addressOpnd = IR::RegOpnd::New(TyMachPtr, func);
    const IR::AutoReuseOpnd autoReuseAddressOpnd(addressOpnd, func);
    InsertLea(addressOpnd, IR::SymOpnd::New(stackSym, TyMachPtr, func), insertBeforeInstr);

    // Currently we don't initialize the environment until we actually allocate the function, we also
    // walk the list of stack function when we need to box them. so we should use initialize it to NullFrameDisplay
    GenerateStackScriptFunctionInit(addressOpnd, nestedProxy,
        IR::AddrOpnd::New((Js::Var)&Js::NullFrameDisplay, IR::AddrOpndKindDynamicMisc, func), insertBeforeInstr);

    // Establish the next link
    InsertMove(nextStackFunctionOpnd, addressOpnd, insertBeforeInstr);
    this->nextStackFunctionOpnd = IR::SymOpnd::New(stackSym, sizeof(Js::StackScriptFunction), TyMachPtr, func);
}

void
Lowerer::GenerateScriptFunctionInit(IR::RegOpnd * regOpnd, IR::Opnd * vtableAddressOpnd,
    Js::FunctionProxyPtrPtr nestedProxy, IR::Opnd * envOpnd, IR::Instr * insertBeforeInstr, bool isZeroed)
{
    Func * func = this->m_func;
    IR::Opnd * functionProxyOpnd;
    Js::FunctionProxy * functionProxy = *nestedProxy;
    IR::Opnd * typeOpnd = nullptr;
    bool doCheckTypeOpnd = true;
    if (functionProxy->IsDeferred())
    {
        functionProxyOpnd = IR::RegOpnd::New(TyMachPtr, func);
        InsertMove(functionProxyOpnd, IR::MemRefOpnd::New((Js::FunctionProxy**) nestedProxy, TyMachPtr, func), insertBeforeInstr);
        typeOpnd = IR::RegOpnd::New(TyMachPtr, func);
        InsertMove(typeOpnd, IR::IndirOpnd::New(functionProxyOpnd->AsRegOpnd(), Js::FunctionProxy::GetOffsetOfDeferredPrototypeType(),
            TyMachPtr, func), insertBeforeInstr);
    }
    else
    {
        Js::FunctionBody * functionBody = functionProxy->GetFunctionBody();
        functionProxyOpnd = CreateFunctionBodyOpnd(functionBody);
        Js::ScriptFunctionType * type = functionProxy->GetDeferredPrototypeType();
        if (type != nullptr)
        {
            typeOpnd = IR::AddrOpnd::New(type, IR::AddrOpndKindDynamicType, func);
            doCheckTypeOpnd = false;
        }
        else
        {
            typeOpnd = IR::RegOpnd::New(TyMachPtr, func);
            InsertMove(typeOpnd,
                IR::MemRefOpnd::New(((byte *)functionBody) + Js::FunctionProxy::GetOffsetOfDeferredPrototypeType(), TyMachPtr, func),
                insertBeforeInstr);
        }
    }

    if (doCheckTypeOpnd)
    {
        IR::LabelInstr * labelHelper = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        InsertTestBranch(typeOpnd, typeOpnd, Js::OpCode::BrEq_A, labelHelper, insertBeforeInstr);
        IR::LabelInstr * labelDone = IR::LabelInstr::New(Js::OpCode::Label, func, false);
        InsertBranch(Js::OpCode::Br, labelDone, insertBeforeInstr);
        insertBeforeInstr->InsertBefore(labelHelper);
        m_lowererMD.LoadHelperArgument(insertBeforeInstr, functionProxyOpnd);

        IR::Instr * callHelperInstr = IR::Instr::New(Js::OpCode::Call, typeOpnd,
            IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperEnsureFunctionProxyDeferredPrototypeType, func), func);
        insertBeforeInstr->InsertBefore(callHelperInstr);
        m_lowererMD.LowerCall(callHelperInstr, 0);
        insertBeforeInstr->InsertBefore(labelDone);
    }

    GenerateMemInit(regOpnd, 0, vtableAddressOpnd, insertBeforeInstr, isZeroed);
    GenerateMemInit(regOpnd, Js::ScriptFunction::GetOffsetOfType(), typeOpnd, insertBeforeInstr, isZeroed);
    GenerateMemInitNull(regOpnd, Js::ScriptFunction::GetOffsetOfAuxSlots(), insertBeforeInstr, isZeroed);
    GenerateMemInitNull(regOpnd, Js::ScriptFunction::GetOffsetOfObjectArray(), insertBeforeInstr, isZeroed);
    GenerateMemInit(regOpnd, Js::ScriptFunction::GetOffsetOfConstructorCache(),
        LoadLibraryValueOpnd(insertBeforeInstr, LibraryValue::ValueConstructorCacheDefaultInstance),
        insertBeforeInstr, isZeroed);
    GenerateMemInit(regOpnd, Js::ScriptFunction::GetOffsetOfFunctionInfo(), functionProxyOpnd, insertBeforeInstr, isZeroed);
    GenerateMemInit(regOpnd, Js::ScriptFunction::GetOffsetOfEnvironment(), envOpnd, insertBeforeInstr, isZeroed);
    GenerateMemInitNull(regOpnd, Js::ScriptFunction::GetOffsetOfCachedScopeObj(), insertBeforeInstr, isZeroed);
    GenerateMemInitNull(regOpnd, Js::ScriptFunction::GetOffsetOfHasInlineCaches(), insertBeforeInstr, isZeroed);
}

void
Lowerer::GenerateStackScriptFunctionInit(IR::RegOpnd * regOpnd, Js::FunctionProxyPtrPtr nestedProxy, IR::Opnd * envOpnd, IR::Instr * insertBeforeInstr)
{
    Func * func = this->m_func;
    GenerateScriptFunctionInit(regOpnd,
        LoadVTableValueOpnd(insertBeforeInstr, VTableValue::VtableStackScriptFunction),
        nestedProxy, envOpnd, insertBeforeInstr);
    InsertMove(IR::IndirOpnd::New(regOpnd, Js::StackScriptFunction::GetOffsetOfBoxedScriptFunction(), TyMachPtr, func),
        IR::AddrOpnd::NewNull(func), insertBeforeInstr);
}

void
Lowerer::EnsureStackFunctionListStackSym()
{
    Func * func = this->m_func;
    Assert(func->HasAnyStackNestedFunc());
#if defined(_M_IX86) || defined(_M_X64)
    Assert(func->m_localStackHeight == (func->HasArgumentSlot()? MachArgsSlotOffset : 0));
    StackSym * stackFunctionListStackSym = StackSym::New(TyMachPtr, func);
    func->StackAllocate(stackFunctionListStackSym, sizeof(Js::ScriptFunction *));
    nextStackFunctionOpnd = IR::SymOpnd::New(stackFunctionListStackSym, TyMachPtr, func);
#else
    Assert(func->m_localStackHeight == 0);
    nextStackFunctionOpnd = IR::IndirOpnd::New(IR::RegOpnd::New(NULL, FRAME_REG, TyMachReg, func),
        -(int32)(Js::Constants::StackNestedFuncList * sizeof(Js::Var)), TyMachPtr, func);
#endif
}

void
Lowerer::AllocStackClosure()
{
    m_func->StackAllocate(m_func->GetLocalFrameDisplaySym(), sizeof(Js::Var));
    m_func->StackAllocate(m_func->GetLocalClosureSym(), sizeof(Js::Var));
}

void
Lowerer::EnsureZeroLastStackFunctionNext()
{
    Assert(nextStackFunctionOpnd != nullptr);
    Func * func = this->m_func;
    IR::Instr * insertBeforeInstr = func->GetFunctionEntryInsertionPoint();
    InsertMove(nextStackFunctionOpnd, IR::AddrOpnd::NewNull(func), insertBeforeInstr);
}

IR::Instr *
Lowerer::GenerateNewStackScFunc(IR::Instr * newScFuncInstr)
{
    Assert(newScFuncInstr->m_func->DoStackNestedFunc());
    Func * func = newScFuncInstr->m_func;
    uint index = newScFuncInstr->GetSrc1()->AsIntConstOpnd()->AsUint32();
    Assert(index < func->GetJnFunction()->GetNestedCount());

    Js::FunctionProxyPtrPtr nestedProxy = func->GetJnFunction()->GetNestedFuncReference(index);
    // the stackAllocate Call below for this sym is passing a size that is not represented by any IRType and hence passing TyMisc for the constructor
    StackSym * stackSym = StackSym::New(TyMisc, func);
    // ScriptFunction and it's next pointer
    this->m_func->StackAllocate(stackSym, sizeof(Js::StackScriptFunction) + sizeof(Js::StackScriptFunction *));
    IR::Opnd * envOpnd = newScFuncInstr->GetSrc2();
    GenerateStackScriptFunctionInit(stackSym, nestedProxy);

    IR::LabelInstr * labelNoStackFunc = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    IR::LabelInstr * labelDone = IR::LabelInstr::New(Js::OpCode::Label, func);

    InsertTestBranch(IR::MemRefOpnd::New(func->GetJnFunction()->GetAddressOfFlags(), TyInt8, func),
        IR::IntConstOpnd::New(Js::FunctionBody::Flags_StackNestedFunc, TyInt8, func, true),
        Js::OpCode::BrEq_A, labelNoStackFunc, newScFuncInstr);

    InsertMove(IR::SymOpnd::New(stackSym, Js::ScriptFunction::GetOffsetOfEnvironment(), TyMachPtr, func),
        envOpnd,
        newScFuncInstr);

    IR::Instr * lea =
        InsertLea(newScFuncInstr->GetDst()->AsRegOpnd(), IR::SymOpnd::New(stackSym, TyMachPtr, func), newScFuncInstr);

    InsertBranch(Js::OpCode::Br, labelDone, newScFuncInstr);

    newScFuncInstr->InsertBefore(labelNoStackFunc);
    newScFuncInstr->InsertAfter(labelDone);

    return lea;
}

IR::Instr *
Lowerer::LowerNewScFunc(IR::Instr * newScFuncInstr)
{
    IR::Instr *stackNewScFuncInstr = nullptr;

    if (newScFuncInstr->m_func->DoStackNestedFunc())
    {
        stackNewScFuncInstr = GenerateNewStackScFunc(newScFuncInstr);
    }

    IR::IntConstOpnd * functionBodySlotOpnd = newScFuncInstr->UnlinkSrc1()->AsIntConstOpnd();
    IR::RegOpnd * envOpnd = newScFuncInstr->UnlinkSrc2()->AsRegOpnd();

    IR::Instr * instrPrev = this->LoadFunctionBodyAsArgument(newScFuncInstr, functionBodySlotOpnd, envOpnd);
    m_lowererMD.ChangeToHelperCall(newScFuncInstr, IR::HelperScrFunc_OP_NewScFunc );

    return stackNewScFuncInstr == nullptr? instrPrev : stackNewScFuncInstr;
}

IR::Instr *
Lowerer::LowerNewScGenFunc(IR::Instr * newScFuncInstr)
{
    IR::IntConstOpnd * functionBodySlotOpnd = newScFuncInstr->UnlinkSrc1()->AsIntConstOpnd();
    IR::RegOpnd * envOpnd = newScFuncInstr->UnlinkSrc2()->AsRegOpnd();

    IR::Instr * instrPrev = this->LoadFunctionBodyAsArgument(newScFuncInstr, functionBodySlotOpnd, envOpnd);
    m_lowererMD.ChangeToHelperCall(newScFuncInstr, IR::HelperScrFunc_OP_NewScGenFunc );

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerScopedLdFld
///
/// Lower a load instruction that takes an additional instance to use as a
/// a default if the scope chain provided doesn't contain the property.
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerScopedLdFld(IR::Instr * ldFldInstr, IR::JnHelperMethod helperMethod, bool withInlineCache)
{
    IR::Opnd *src;
    IR::Instr *instrPrev = ldFldInstr->m_prev;

    if(!withInlineCache)
    {
        LoadScriptContext(ldFldInstr);
    }

    src = ldFldInstr->UnlinkSrc2();
    AssertMsg(src->IsRegOpnd(), "Expected reg opnd as src2");
    instrPrev = m_lowererMD.LoadHelperArgument(ldFldInstr, src);

    src = ldFldInstr->UnlinkSrc1();
    AssertMsg(src->IsSymOpnd() && src->AsSymOpnd()->m_sym->IsPropertySym(), "Expected property sym as src");
    this->LoadPropertySymAsArgument(ldFldInstr, src);

    if (withInlineCache)
    {
        AssertMsg(src->AsSymOpnd()->IsPropertySymOpnd(), "Need property sym operand to find the inline cache");

        m_lowererMD.LoadHelperArgument(
            ldFldInstr,
            IR::Opnd::CreateInlineCacheIndexOpnd(src->AsPropertySymOpnd()->m_inlineCacheIndex, m_func));

        // Not using the polymorphic inline cache because the fast path only uses the monomorphic inline cache
        this->m_lowererMD.LoadHelperArgument(ldFldInstr, this->LoadRuntimeInlineCacheOpnd(ldFldInstr, src->AsPropertySymOpnd()));

        m_lowererMD.LoadHelperArgument(ldFldInstr, LoadFunctionBodyOpnd(ldFldInstr));
    }
    m_lowererMD.ChangeToHelperCall(ldFldInstr, helperMethod);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerScopedLdInst
///
/// Lower a load instruction that takes an additional instance to use as a
/// a default if the scope chain provided doesn't contain the property.
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerScopedLdInst(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    IR::Opnd *src;
    IR::Instr *instrPrev;

    // last argument is the scriptContext
    instrPrev = LoadScriptContext(instr);
    src = instr->UnlinkSrc2();
    AssertMsg(src->IsRegOpnd(), "Expected Reg opnd as src2");

    // __out Var*. The StackSym is allocated in irbuilder, and here we need to insert a lea
    StackSym* dstSym = src->GetStackSym();
    IR::Instr *load = this->m_lowererMD.LoadStackAddress(dstSym);
    instr->InsertBefore(load);
    IR::Opnd* tempOpnd = load->GetDst();
    m_lowererMD.LoadHelperArgument(instr, tempOpnd);

    // now 3rd last argument is the rootObject of the function. Need to add addrOpnd to
    // pass in the address of the roobObject.
    IR::Opnd * srcOpnd;
    Js::RootObjectBase * rootObject = this->m_func->GetJnFunction()->GetRootObject();
    srcOpnd = IR::AddrOpnd::New(rootObject, IR::AddrOpndKindDynamicVar, instr->m_func, true);
    instrPrev = m_lowererMD.LoadHelperArgument(instr, srcOpnd);

    // no change, the property field built from irbuilder.
    src = instr->UnlinkSrc1();
    AssertMsg(src->IsSymOpnd() && src->AsSymOpnd()->m_sym->IsPropertySym(), "Expected property sym as src");
    this->LoadPropertySymAsArgument(instr, src);

    instrPrev = m_lowererMD.ChangeToHelperCall(instr, helperMethod);

    IR::RegOpnd* regOpnd = IR::RegOpnd::New(dstSym, TyVar, this->m_func);
    IR::SymOpnd*symOpnd = IR::SymOpnd::New(dstSym, TyVar, this->m_func);
    this->m_lowererMD.CreateAssign(regOpnd, symOpnd, instrPrev);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerScopedDelFld(IR::Instr * delFldInstr, IR::JnHelperMethod helperMethod, bool withInlineCache, bool strictMode)
{
    IR::Instr *instrPrev;

    Js::PropertyOperationFlags propertyOperationFlag = Js::PropertyOperation_None;

    if (strictMode)
    {
        propertyOperationFlag = Js::PropertyOperation_StrictMode;
    }

    instrPrev = m_lowererMD.LoadHelperArgument(delFldInstr, IR::IntConstOpnd::New((IntConstType)propertyOperationFlag, TyInt32, m_func, true));

    LowerScopedLdFld(delFldInstr, helperMethod, withInlineCache);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerProfiledStFld(IR::JitProfilingInstr *stFldInstr, Js::PropertyOperationFlags flags)
{
    Assert(stFldInstr->profileId == Js::Constants::NoProfileId);

    IR::Instr *const instrPrev = stFldInstr->m_prev;

    /*
        void ProfilingHelpers::ProfiledInitFld_Jit(
            const Var instance,
            const PropertyId propertyId,
            const InlineCacheIndex inlineCacheIndex,
            const Var value,
            void *const framePointer)

        void ProfilingHelpers::ProfiledStFld_Jit(
            const Var instance,
            const PropertyId propertyId,
            const InlineCacheIndex inlineCacheIndex,
            const Var value,
            void *const framePointer)

        void ProfilingHelpers::ProfiledStSuperFld_Jit(
            const Var instance,
            const PropertyId propertyId,
            const InlineCacheIndex inlineCacheIndex,
            const Var value,
            void *const framePointer,
            const Var thisInstance)
    {
    */

    m_lowererMD.LoadHelperArgument(stFldInstr, IR::Opnd::CreateFramePointerOpnd(m_func));

    if (stFldInstr->m_opcode == Js::OpCode::StSuperFld)
    {
        m_lowererMD.LoadHelperArgument(stFldInstr, stFldInstr->UnlinkSrc2());
    }

    m_lowererMD.LoadHelperArgument(stFldInstr, stFldInstr->UnlinkSrc1());

    IR::Opnd *dst = stFldInstr->UnlinkDst();
    AssertMsg(dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsPropertySym(), "Expected property sym as dst of field store");
    m_lowererMD.LoadHelperArgument(
        stFldInstr,
        IR::Opnd::CreateInlineCacheIndexOpnd(dst->AsPropertySymOpnd()->m_inlineCacheIndex, m_func));

    LoadPropertySymAsArgument(stFldInstr, dst);

    IR::JnHelperMethod helper;
    switch (stFldInstr->m_opcode)
    {
    case Js::OpCode::InitFld:
    case Js::OpCode::InitRootFld:
        helper = IR::HelperProfiledInitFld;
        break;

    case Js::OpCode::StSuperFld:
        helper = IR::HelperProfiledStSuperFld;
        break;

    default:
        helper =
            flags & Js::PropertyOperation_Root
                ? flags & Js::PropertyOperation_StrictMode ? IR::HelperProfiledStRootFld_Strict : IR::HelperProfiledStRootFld
                : flags & Js::PropertyOperation_StrictMode ? IR::HelperProfiledStFld_Strict : IR::HelperProfiledStFld;
        break;
    }
    stFldInstr->SetSrc1(IR::HelperCallOpnd::New(helper, m_func));
    m_lowererMD.LowerCall(stFldInstr, 0);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerStFld
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerStFld(
    IR::Instr * stFldInstr,
    IR::JnHelperMethod helperMethod,
    IR::JnHelperMethod polymorphicHelperMethod,
    bool withInlineCache,
    IR::LabelInstr *labelBailOut,
    bool isHelper,
    bool withPutFlags,
    Js::PropertyOperationFlags flags)
{
    if (stFldInstr->IsJitProfilingInstr())
    {
        // If we want to profile then do something completely different
        return this->LowerProfiledStFld(stFldInstr->AsJitProfilingInstr(), flags);
    }

    IR::Instr *instrPrev = stFldInstr->m_prev;

    IR::Opnd *dst = stFldInstr->UnlinkDst();
    AssertMsg(dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsPropertySym(), "Expected property sym as dst of field store");

    IR::Opnd * inlineCacheOpnd = nullptr;
    if (withInlineCache)
    {
        AssertMsg(dst->AsSymOpnd()->IsPropertySymOpnd(), "Need property sym operand to find the inline cache");
        if (dst->AsPropertySymOpnd()->m_runtimePolymorphicInlineCache && polymorphicHelperMethod != helperMethod)
        {
            Js::PolymorphicInlineCache * polymorphicInlineCache = dst->AsPropertySymOpnd()->m_runtimePolymorphicInlineCache;
            helperMethod = polymorphicHelperMethod;
            inlineCacheOpnd = IR::AddrOpnd::New(polymorphicInlineCache, IR::AddrOpndKindDynamicInlineCache, this->m_func);
        }
        else
        {
            // Need to load runtime inline cache opnd first before loading any helper argument
            // because LoadRuntimeInlineCacheOpnd may create labels marked as helper
            // and cause op helper register push/pop save in x86, messing up with any helper arguments that is already pushed
            inlineCacheOpnd = this->LoadRuntimeInlineCacheOpnd(stFldInstr, dst->AsPropertySymOpnd(), isHelper);
        }
    }
    if (withPutFlags)
    {
        m_lowererMD.LoadHelperArgument(stFldInstr,
            IR::IntConstOpnd::New(static_cast<IntConstType>(flags), IRType::TyInt32, m_func, true));
    }

    IR::Opnd *src = stFldInstr->UnlinkSrc1();
    if (stFldInstr->m_opcode == Js::OpCode::StSuperFld)
    {
        m_lowererMD.LoadHelperArgument(stFldInstr, stFldInstr->UnlinkSrc2());
    }

    m_lowererMD.LoadHelperArgument(stFldInstr, src);

    this->LoadPropertySymAsArgument(stFldInstr, dst);

    if (withInlineCache)
    {
        Assert(inlineCacheOpnd != nullptr);
        this->m_lowererMD.LoadHelperArgument(
            stFldInstr,
            IR::Opnd::CreateInlineCacheIndexOpnd(dst->AsPropertySymOpnd()->m_inlineCacheIndex, m_func));

        this->m_lowererMD.LoadHelperArgument(stFldInstr, inlineCacheOpnd);
        this->m_lowererMD.LoadHelperArgument(stFldInstr, LoadFunctionBodyOpnd(stFldInstr));
    }

    IR::RegOpnd *opndBase = dst->AsSymOpnd()->CreatePropertyOwnerOpnd(m_func);
    m_lowererMD.ChangeToHelperCall(stFldInstr, helperMethod, labelBailOut, opndBase, dst->AsSymOpnd()->IsPropertySymOpnd() ? dst->AsSymOpnd()->AsPropertySymOpnd() : nullptr, isHelper);

    return instrPrev;
}

IR::Instr* Lowerer::GenerateCompleteStFld(IR::Instr* instr, bool emitFastPath, IR::JnHelperMethod monoHelperAfterFastPath, IR::JnHelperMethod polyHelperAfterFastPath,
    IR::JnHelperMethod monoHelperWithoutFastPath, IR::JnHelperMethod polyHelperWithoutFastPath, bool withPutFlags, Js::PropertyOperationFlags flags)
{
    if(instr->CallsAccessor() && instr->HasBailOutInfo())
    {
        IR::BailOutKind kindMinusBits = instr->GetBailOutKind() & ~IR::BailOutKindBits;
        Assert(kindMinusBits != IR::BailOutOnImplicitCalls && kindMinusBits != IR::BailOutOnImplicitCallsPreOp);
    }

    IR::Instr* prevInstr = instr->m_prev;

    IR::LabelInstr* labelBailOut = nullptr;
    IR::LabelInstr* labelHelper = nullptr;
    bool isHelper = false;
    IR::RegOpnd* typeOpnd = nullptr;
    if(emitFastPath && GenerateFastStFldForCustomProperty(instr, &labelHelper))
    {
        if(labelHelper)
        {
            Assert(labelHelper->isOpHelper);
            instr->InsertBefore(labelHelper);
            prevInstr = this->LowerStFld(instr, monoHelperWithoutFastPath, polyHelperWithoutFastPath, true, labelBailOut, isHelper, withPutFlags, flags);
        }
        else
        {
            instr->Remove();
            return prevInstr;
        }
    }
    else if (this->GenerateStFldWithCachedType(instr, &isHelper, &labelHelper, &typeOpnd))
    {
        Assert(labelHelper == nullptr);
        return prevInstr;
    }
    else if (emitFastPath)
    {
        if (!GenerateFastStFld(instr, monoHelperWithoutFastPath, polyHelperWithoutFastPath, &labelBailOut, typeOpnd, &isHelper, &labelHelper, withPutFlags, flags))
        {
            if (labelHelper != nullptr)
            {
                labelHelper->isOpHelper = isHelper;
                instr->InsertBefore(labelHelper);
            }
            prevInstr = this->LowerStFld(instr, monoHelperAfterFastPath, polyHelperAfterFastPath, true, labelBailOut, isHelper, withPutFlags, flags);
        }
    }
    else
    {
        if (labelHelper != nullptr)
        {
            labelHelper->isOpHelper = isHelper;
            instr->InsertBefore(labelHelper);
        }
        prevInstr = this->LowerStFld(instr, monoHelperWithoutFastPath, monoHelperWithoutFastPath, true, labelBailOut, isHelper, withPutFlags, flags);
    }

    return prevInstr;
}

void
Lowerer::GenerateDirectFieldStore(IR::Instr* instrStFld, IR::PropertySymOpnd* propertySymOpnd)
{
    Func* func = instrStFld->m_func;

    IR::Opnd *opndSlotArray = this->LoadSlotArrayWithCachedLocalType(instrStFld, propertySymOpnd);

    // Store the value to the slot, getting the slot index from the cache.
    uint16 index = propertySymOpnd->GetSlotIndex();
    Assert(index != -1);

#ifdef RECYCLER_RECYCLER_WRITE_BARRIER_JIT
    if (opndSlotArray->IsRegOpnd())
    {
        IR::IndirOpnd * opndDst = IR::IndirOpnd::New(opndSlotArray->AsRegOpnd(), index * sizeof(Js::Var), TyMachReg, func);
        LowererMD::GenerateWriteBarrierAssign(opndDst, instrStFld->GetSrc1(), instrStFld);
    }
    else
    {
        Assert(opndSlotArray->IsMemRefOpnd());
        IR::MemRefOpnd * opndDst = IR::MemRefOpnd::New((char*)opndSlotArray->AsMemRefOpnd()->GetMemLoc() + (index * sizeof(Js::Var)), TyMachReg, func);
        LowererMD::GenerateWriteBarrierAssign(opndDst, instrStFld->GetSrc1(), instrStFld);
    }
#else
    IR::Opnd *opnd;

    if (opndSlotArray->IsRegOpnd())
    {
        opnd = IR::IndirOpnd::New(opndSlotArray->AsRegOpnd(), index * sizeof(Js::Var), TyMachReg, func);
    }
    else
    {
        opnd = IR::MemRefOpnd::New((char*)opndSlotArray->AsMemRefOpnd()->GetMemLoc() + (index * sizeof(Js::Var)), TyMachReg, func);
    }

    this->m_lowererMD.CreateAssign(opnd, instrStFld->GetSrc1(), instrStFld);
#endif
}

bool
Lowerer::GenerateStFldWithCachedType(IR::Instr *instrStFld, bool* continueAsHelperOut, IR::LabelInstr** labelHelperOut, IR::RegOpnd** typeOpndOut)
{
    IR::Instr *instr;
    IR::RegOpnd *typeOpnd = nullptr;
    IR::LabelInstr* labelObjCheckFailed  = nullptr;
    IR::LabelInstr *labelTypeCheckFailed = nullptr;
    IR::LabelInstr *labelBothTypeChecksFailed = nullptr;
    IR::LabelInstr *labelDone = nullptr;

    Assert(continueAsHelperOut != nullptr);
    *continueAsHelperOut = false;

    Assert(labelHelperOut != nullptr);
    *labelHelperOut = nullptr;

    Assert(typeOpndOut != nullptr);
    *typeOpndOut = nullptr;

    Assert(instrStFld->GetDst()->IsSymOpnd());
    if (!instrStFld->GetDst()->AsSymOpnd()->IsPropertySymOpnd() || !instrStFld->GetDst()->AsPropertySymOpnd()->IsTypeCheckSeqCandidate())
    {
        return false;
    }

    IR::PropertySymOpnd *propertySymOpnd = instrStFld->GetDst()->AsPropertySymOpnd();

    // If we have any object type spec info, we better not believe this is a load from prototype, since this is a store
    // and we never share inline caches between loads and stores.
    Assert(!propertySymOpnd->HasObjTypeSpecFldInfo() || !propertySymOpnd->IsLoadedFromProto());

    AssertMsg(propertySymOpnd->TypeCheckSeqBitsSetOnlyIfCandidate(), "Property sym operand optimized despite not being a candidate?");

    if (!propertySymOpnd->IsTypeCheckSeqCandidate())
    {
        return false;
    }

    if (!propertySymOpnd->IsTypeCheckSeqParticipant() && !propertySymOpnd->NeedsLocalTypeCheck())
    {
        return false;
    }

    Assert(!propertySymOpnd->NeedsTypeCheckAndBailOut() || (instrStFld->HasBailOutInfo() && IR::IsTypeCheckBailOutKind(instrStFld->GetBailOutKind())));

    // In the backwards pass we only add guarded property operations to instructions that are not already
    // protected by an upstream type check.
    Assert(!propertySymOpnd->IsTypeCheckProtected() || propertySymOpnd->GetGuardedPropOps() == nullptr);

    PHASE_PRINT_TESTTRACE(
        Js::ObjTypeSpecPhase,
        this->m_func,
        L"Field store: %s, property: %s, func: %s, cache ID: %d, cloned cache: true, layout: %s, redundant check: %s\n",
        Js::OpCodeUtil::GetOpCodeName(instrStFld->m_opcode),
        this->m_func->GetScriptContext()->GetPropertyNameLocked(propertySymOpnd->m_sym->AsPropertySym()->m_propertyId)->GetBuffer(),
        this->m_func->GetJnFunction()->GetDisplayName(),
        propertySymOpnd->m_inlineCacheIndex, propertySymOpnd->GetCacheLayoutString(),
        propertySymOpnd->IsTypeChecked() ? L"true" : L"false");

    if (propertySymOpnd->HasFinalType() && !propertySymOpnd->IsLoadedFromProto())
    {
        propertySymOpnd->UpdateSlotForFinalType();
    }

    Func* func = instrStFld->m_func;

    // TODO (ObjTypeSpec): If ((PropertySym*)propertySymOpnd->m_sym)->m_stackSym->m_isIntConst consider emitting a direct
    // jump to helper or bailout.  If we have a type check bailout, we could even abort compilation.

    bool hasTypeCheckBailout = instrStFld->HasBailOutInfo() && IR::IsTypeCheckBailOutKind(instrStFld->GetBailOutKind());

    // If the type hasn't been checked upstream, see if it makes sense to check it here.
    bool isTypeChecked = propertySymOpnd->IsTypeChecked();
    if (!isTypeChecked)
    {
        // If the initial type has been checked, we can do a hard coded type transition without any type checks
        // (see GenerateStFldWithCachedFinalType), which is always worth doing, even if the type is not needed
        // downstream.  We're not introducing any additional bailouts.
        if (propertySymOpnd->HasFinalType() && propertySymOpnd->HasInitialType() && !propertySymOpnd->IsTypeDead())
        {
            // We have a final type in hand, so we can JIT (most of) the type transition work.
            return this->GenerateStFldWithCachedFinalType(instrStFld, propertySymOpnd);
        }

        if (propertySymOpnd->HasTypeMismatch())
        {
            // So we have a type mismatch, which happens when the type (and the type without property if ObjTypeSpecStore
            // is on) on this instruction didn't match the live type value according to the flow. We must have hit some
            // stale inline cache (perhaps inlined from a different function, or on a code path not taken for a while).
            // Either way, we know exactly what type the object must have at this point (fully determined by flow), but
            // we don't know whether that type already has the property we're storing here. All in all, we know exactly
            // what shape the object will have after this operation, but we're not sure what label (type) to give this
            // shape. Thus we can simply let the fast path do its thing based on the live inline cache. The downstream
            // instructions relying only on this shape (loads and stores) are safe, and those that need the next type
            // (i.e. adds) will do the same thing as this instruction.
            return false;
        }

        // If we're still here then we must need a primary type check on this instruction to protect
        // a sequence of field operations downstream, or a local type check for an isolated field store.
        Assert(propertySymOpnd->NeedsPrimaryTypeCheck() || propertySymOpnd->NeedsLocalTypeCheck());

        labelTypeCheckFailed = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        labelBothTypeChecksFailed = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        labelObjCheckFailed = hasTypeCheckBailout ? labelBothTypeChecksFailed : IR::LabelInstr::New(Js::OpCode::Label, func, true);
        typeOpnd = this->GenerateCachedTypeCheck(instrStFld, propertySymOpnd, labelObjCheckFailed, labelBothTypeChecksFailed, labelTypeCheckFailed);
        *typeOpndOut = typeOpnd;
    }

    // Either we are protected by a type check upstream or we just emitted a type check above,
    // now it's time to store the field value.
    GenerateDirectFieldStore(instrStFld, propertySymOpnd);

    // If we are protected by a type check upstream, we don't need a bailout or helper here, delete the instruction
    // and return "true" to indicate that we succeeded in eliminating it.
    if (isTypeChecked)
    {
        Assert(labelTypeCheckFailed == nullptr && labelBothTypeChecksFailed == nullptr);
        AssertMsg(!instrStFld->HasBailOutInfo(), "Why does a direct field store have bailout?");
        instrStFld->Remove();
        return true;
    }

    // Otherwise, branch around the helper on successful type check.
    labelDone = IR::LabelInstr::New(Js::OpCode::Label, func);
    instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelDone, func);
    instrStFld->InsertBefore(instr);

    // On failed type check, try the type without property if we've got one.
    instrStFld->InsertBefore(labelTypeCheckFailed);

    // Caution, this is one of the dusty corners of the JIT.  We only get here if this is an isolated StFld which adds a property, or
    // ObjTypeSpecStore is off.  In the former case no downstream operations depend on the final type produced here, and we can fall
    // back on live cache and helper if the type doesn't match.  In the latter we may have a cache with type transition, which must
    // produce a value for the type after transition, because that type is consumed downstream. Thus, if the object's type doesn't
    // match either the type with or the type without the property we're storing, we must bail out here.
    bool emitAddProperty = propertySymOpnd->IsMono() && propertySymOpnd->HasInitialType();

    if (emitAddProperty)
    {
        GenerateCachedTypeWithoutPropertyCheck(instrStFld, propertySymOpnd, typeOpnd, labelBothTypeChecksFailed);
        GenerateFieldStoreWithTypeChange(instrStFld, propertySymOpnd, propertySymOpnd->GetInitialType(), propertySymOpnd->GetType());
        instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelDone, func);
        instrStFld->InsertBefore(instr);
    }

    instrStFld->InsertBefore(labelBothTypeChecksFailed);
    instrStFld->InsertAfter(labelDone);

    if (hasTypeCheckBailout)
    {
        AssertMsg(PHASE_ON1(Js::ObjTypeSpecIsolatedFldOpsWithBailOutPhase) || !propertySymOpnd->IsTypeDead(),
            "Why does a field store have a type check bailout, if its type is dead?");

        if (instrStFld->GetBailOutInfo()->bailOutInstr != instrStFld)
        {
            // Set the cache index in the bailout info so that the generated code will write it into the
            // bailout record at runtime.
            instrStFld->GetBailOutInfo()->polymorphicCacheIndex = propertySymOpnd->m_inlineCacheIndex;
        }
        else
        {
            Assert(instrStFld->GetBailOutInfo()->polymorphicCacheIndex == propertySymOpnd->m_inlineCacheIndex);
        }

        instrStFld->m_opcode = Js::OpCode::BailOut;
        instrStFld->FreeSrc1();
        instrStFld->FreeDst();

        this->GenerateBailOut(instrStFld);
        return true;
    }
    else
    {
        *continueAsHelperOut = true;
        Assert(labelObjCheckFailed != nullptr && labelObjCheckFailed != labelBothTypeChecksFailed);
        *labelHelperOut = labelObjCheckFailed;
        return false;
    }
}

IR::RegOpnd *
Lowerer::GenerateCachedTypeCheck(IR::Instr *instrChk, IR::PropertySymOpnd *propertySymOpnd, IR::LabelInstr* labelObjCheckFailed, IR::LabelInstr *labelTypeCheckFailed, IR::LabelInstr *labelSecondChance)
{
    Assert(propertySymOpnd->MayNeedTypeCheckProtection());

    Func* func = instrChk->m_func;
    IR::RegOpnd *regOpnd = propertySymOpnd->CreatePropertyOwnerOpnd(func);
    regOpnd->SetValueType(propertySymOpnd->GetPropertyOwnerValueType());

    if (!regOpnd->IsNotTaggedValue())
    {
        m_lowererMD.GenerateObjectTest(regOpnd, instrChk, labelObjCheckFailed);
    }

    IR::Opnd *expectedTypeOpnd;
    bool emitDirectCheck = true;
    // Note: don't attempt equivalent type check if we're doing a final type optimization or if we have a monomorphic
    // cache and no type check bailout. In the latter case, we can wind up doing expensive failed equivalence checks
    // repeatedly and never rejit.
    bool doEquivTypeCheck =
        propertySymOpnd->HasEquivalentTypeSet() &&
        !(propertySymOpnd->HasFinalType() && propertySymOpnd->HasInitialType()) &&
        !propertySymOpnd->MustDoMonoCheck() &&
        (propertySymOpnd->IsPoly() || instrChk->HasTypeCheckBailOut());
    Assert(doEquivTypeCheck || !instrChk->HasEquivalentTypeCheckBailOut());

    Js::Type* type = doEquivTypeCheck ? propertySymOpnd->GetFirstEquivalentType() : propertySymOpnd->GetType();

    Js::PropertyGuard* typeCheckGuard = doEquivTypeCheck ?
        (Js::PropertyGuard*)CreateEquivalentTypeGuardAndLinkToGuardedProperties(type, propertySymOpnd) :
        (Js::PropertyGuard*)CreateTypePropertyGuardForGuardedProperties(type, propertySymOpnd);

    if (typeCheckGuard == nullptr)
    {
        Assert(type != nullptr);
        expectedTypeOpnd = IR::AddrOpnd::New(type, IR::AddrOpndKindDynamicType, func, true);
    }
    else
    {
        Assert(Js::PropertyGuard::GetSizeOfValue() == static_cast<size_t>(TySize[TyMachPtr]));
        expectedTypeOpnd = IR::MemRefOpnd::New((void*)(typeCheckGuard->GetAddressOfValue()), TyMachPtr, func, IR::AddrOpndKindDynamicGuardValueRef);
        emitDirectCheck = false;
    }

    if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, this->m_func))
    {
        OUTPUT_VERBOSE_TRACE_FUNC(Js::ObjTypeSpecPhase, this->m_func, L"Emitted %s type check for type 0x%p",
            emitDirectCheck ? L"direct" : propertySymOpnd->IsPoly() ? L"equivalent" : L"indirect", type);
#if DBG
        if (propertySymOpnd->GetGuardedPropOps() != nullptr)
        {
            Output::Print(L" guarding operations:\n    ");
            propertySymOpnd->GetGuardedPropOps()->Dump();
        }
        else
        {
            Output::Print(L"\n");
        }
#else
        Output::Print(L"\n");
#endif
        Output::Flush();
    }

    IR::RegOpnd* typeOpnd = IR::RegOpnd::New(TyMachReg, func);
    IR::Opnd *sourceType;
    if (regOpnd->m_sym->IsConst() && !regOpnd->m_sym->IsIntConst() && !regOpnd->m_sym->IsFloatConst())
    {
        sourceType = IR::MemRefOpnd::New((BYTE*)regOpnd->m_sym->GetConstAddress() +
            Js::RecyclableObject::GetOffsetOfType(), TyMachReg, func, IR::AddrOpndKindDynamicObjectTypeRef);
    }
    else
    {
        sourceType = IR::IndirOpnd::New(regOpnd, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, func);
    }
    m_lowererMD.CreateAssign(typeOpnd, sourceType, instrChk);

    if (doEquivTypeCheck)
    {
        // TODO (ObjTypeSpec): For isolated equivalent type checks it would be good to emit a check if the cache is still valid, and
        // if not go straight to live polymorphic cache.  This way we wouldn't have to bail out and re-JIT, and also wouldn't continue
        // to try the equivalent type cache, miss it and do the slow comparison. This may be as easy as sticking a null on the main
        // type in the equivalent type cache.
        IR::LabelInstr* labelCheckEquivalentType = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        InsertCompareBranch(typeOpnd, expectedTypeOpnd, Js::OpCode::BrNeq_A, labelCheckEquivalentType, instrChk);

        IR::LabelInstr *labelTypeCheckSucceeded = IR::LabelInstr::New(Js::OpCode::Label, func, false);
        InsertBranch(Js::OpCode::Br, labelTypeCheckSucceeded, instrChk);

        instrChk->InsertBefore(labelCheckEquivalentType);

        this->m_lowererMD.LoadHelperArgument(instrChk, IR::AddrOpnd::New((Js::Var)typeCheckGuard, IR::AddrOpndKindDynamicTypeCheckGuard, func, true));
        this->m_lowererMD.LoadHelperArgument(instrChk, typeOpnd);

        IR::RegOpnd* equivalentTypeCheckResultOpnd = IR::RegOpnd::New(TyUint8, func);
        IR::HelperCallOpnd* equivalentTypeCheckHelperCallOpnd = IR::HelperCallOpnd::New(IR::HelperCheckIfTypeIsEquivalent, func);
        IR::Instr* equivalentTypeCheckCallInstr = IR::Instr::New(Js::OpCode::Call, equivalentTypeCheckResultOpnd, equivalentTypeCheckHelperCallOpnd, func);
        instrChk->InsertBefore(equivalentTypeCheckCallInstr);
        this->m_lowererMD.LowerCall(equivalentTypeCheckCallInstr, 0);

        InsertTestBranch(equivalentTypeCheckResultOpnd, equivalentTypeCheckResultOpnd, Js::OpCode::BrEq_A, labelTypeCheckFailed, instrChk);

        // TODO (ObjTypeSpec): Consider emitting a shared bailout to which a specific bailout kind is written at runtime. This would allow us to distinguish
        // between non-equivalent type and other cases, such as invalidated guard (due to fixed field overwrite, perhaps) or too much thrashing on the
        // equivalent type cache. We could determine bailout kind based on the value returned by the helper. In the case of cache thrashing we could just
        // turn off the whole optimization for a given function.

        instrChk->InsertBefore(labelTypeCheckSucceeded);
    }
    else
    {
        InsertCompareBranch(typeOpnd, expectedTypeOpnd, Js::OpCode::BrNeq_A, labelSecondChance != nullptr ? labelSecondChance : labelTypeCheckFailed, instrChk);
    }

    // Don't pin the type for polymorphic operations. The code can successfully execute even if this type is no longer referenced by any objects,
    // as long as there are other objects with types equivalent on the properties referenced by this code. The type is kept alive until entry point
    // installation by the JIT transfer data, and after that by the equivalent type cache, so it will stay alive unless or until it gets evicted
    // from the cache.
    if (!doEquivTypeCheck)
    {
        PinTypeRef(type, type, instrChk, propertySymOpnd->m_sym->AsPropertySym()->m_propertyId);
    }

    return typeOpnd;
}

void
Lowerer::PinTypeRef(Js::Type* type, void* typeRef, IR::Instr* instr, Js::PropertyId propertyId)
{
    this->m_func->PinTypeRef(typeRef);

    if (PHASE_TRACE(Js::TracePinnedTypesPhase, this->m_func))
    {
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(L"PinnedTypes: function %s(%s) instr %s property %s(#%u) pinned %s reference 0x%p to type 0x%p.\n",
            this->m_func->GetJnFunction()->GetDisplayName(), this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
            Js::OpCodeUtil::GetOpCodeName(instr->m_opcode), GetScriptContext()->GetPropertyNameLocked(propertyId)->GetBuffer(), propertyId,
            typeRef == type ? L"strong" : L"weak", typeRef, type);
        Output::Flush();
    }
}

void
Lowerer::GenerateCachedTypeWithoutPropertyCheck(IR::Instr *instrInsert, IR::PropertySymOpnd *propertySymOpnd, IR::Opnd *typeOpnd, IR::LabelInstr *labelTypeCheckFailed)
{
    Assert(propertySymOpnd->IsMonoObjTypeSpecCandidate());
    Assert(propertySymOpnd->HasInitialType());

    Js::Type* typeWithoutProperty = propertySymOpnd->GetInitialType();

    // We should never add properties to objects of static types.
    Assert(Js::DynamicType::Is(typeWithoutProperty->GetTypeId()));

    if (typeOpnd == nullptr)
    {
        // No opnd holding the type was passed in, so we have to load the type here.
        IR::RegOpnd *baseOpnd = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);
        if (!baseOpnd->IsNotTaggedValue())
        {
            m_lowererMD.GenerateObjectTest(baseOpnd, instrInsert, labelTypeCheckFailed);
        }
        IR::Opnd *opnd = IR::IndirOpnd::New(baseOpnd, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, this->m_func);
        typeOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
        m_lowererMD.CreateAssign(typeOpnd, opnd, instrInsert);
    }

    Js::JitTypePropertyGuard* typePropertyGuard = CreateTypePropertyGuardForGuardedProperties(typeWithoutProperty, propertySymOpnd);

    IR::Opnd *expectedTypeOpnd;

    if (typePropertyGuard)
    {
        bool emitDirectCheck = true;

        Assert(typePropertyGuard != nullptr);
        Assert(Js::PropertyGuard::GetSizeOfValue() == static_cast<size_t>(TySize[TyMachPtr]));
        expectedTypeOpnd = IR::MemRefOpnd::New((void*)(typePropertyGuard->GetAddressOfValue()), TyMachPtr, this->m_func, IR::AddrOpndKindDynamicGuardValueRef);
        emitDirectCheck = false;

        OUTPUT_VERBOSE_TRACE_FUNC(Js::ObjTypeSpecPhase, this->m_func, L"Emitted %s type check for type 0x%p.\n",
            emitDirectCheck ? L"direct" : L"indirect", typeWithoutProperty);
    }
    else
    {
        expectedTypeOpnd = IR::AddrOpnd::New(typeWithoutProperty, IR::AddrOpndKindDynamicType, m_func, true);
    }

    InsertCompareBranch(typeOpnd, expectedTypeOpnd, Js::OpCode::BrNeq_A, labelTypeCheckFailed, instrInsert);

    // Technically, it should be enough to pin the final type, because it should keep all of its predecessors alive, but
    // just to be extra cautious, let's pin the initial type as well.
    PinTypeRef(typeWithoutProperty, typeWithoutProperty, instrInsert, propertySymOpnd->m_sym->AsPropertySym()->m_propertyId);
}

void
Lowerer::GenerateFixedFieldGuardCheck(IR::Instr *insertPointInstr, IR::PropertySymOpnd *propertySymOpnd, IR::LabelInstr *labelBailOut)
{
    GeneratePropertyGuardCheck(insertPointInstr, propertySymOpnd, labelBailOut);
}

Js::JitTypePropertyGuard*
Lowerer::CreateTypePropertyGuardForGuardedProperties(Js::Type* type, IR::PropertySymOpnd* propertySymOpnd)
{
    // We should always have a list of guarded properties.
    Assert(propertySymOpnd->GetGuardedPropOps() != nullptr);

    Js::JitTypePropertyGuard* guard = nullptr;

    Js::EntryPointInfo* entryPointInfo = this->m_func->m_workItem->GetEntryPoint();

    if (entryPointInfo->HasSharedPropertyGuards())
    {
        // Consider (ObjTypeSpec): Because we allocate these guards from the JIT thread we can't share guards for the same type across multiple functions.
        // This leads to proliferation of property guards on the thread context.  The alternative would be to pre-allocate shared (by value) guards
        // from the thread context during work item creation.  We would create too many of them (because some types aren't actually used as guards),
        // but we could share a guard for a given type between functions.  This may ultimately be better.

        LinkGuardToGuardedProperties(entryPointInfo, propertySymOpnd->GetGuardedPropOps(), [this, type, &guard](Js::PropertyId propertyId)
        {
            if (DoLazyFixedTypeBailout(this->m_func))
            {
                this->m_func->lazyBailoutProperties.Item(propertyId);
            }
            else
            {
                if (guard == nullptr)
                {
                    guard = this->m_func->GetOrCreateSingleTypeGuard(type);
                }

                if (PHASE_TRACE(Js::ObjTypeSpecPhase, this->m_func) || PHASE_TRACE(Js::TracePropertyGuardsPhase, this->m_func))
                {
                    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    wchar_t workItemName[256];
                    this->m_func->m_workItem->GetDisplayName(workItemName, _countof(workItemName));
                    Output::Print(L"ObjTypeSpec: function %s(%s) registered guard 0x%p with value 0x%p for property %s (%u).\n",
                        workItemName, this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                        guard, guard->GetValue(), this->GetScriptContext()->GetPropertyNameLocked(propertyId)->GetBuffer(), propertyId);
                    Output::Flush();
                }

                this->m_func->EnsurePropertyGuardsByPropertyId();
                this->m_func->LinkGuardToPropertyId(propertyId, guard);
            }
        });
    }

    return guard;
}

Js::JitEquivalentTypeGuard*
Lowerer::CreateEquivalentTypeGuardAndLinkToGuardedProperties(Js::Type* type, IR::PropertySymOpnd* propertySymOpnd)
{
    // We should always have a list of guarded properties.
    Assert(propertySymOpnd->HasObjTypeSpecFldInfo() && propertySymOpnd->HasEquivalentTypeSet() && propertySymOpnd->GetGuardedPropOps());

    Js::JitEquivalentTypeGuard* guard = this->m_func->CreateEquivalentTypeGuard(type, propertySymOpnd->GetObjTypeSpecFldId());

    Js::EntryPointInfo* entryPointInfo = this->m_func->m_workItem->GetEntryPoint();

    if (entryPointInfo->HasSharedPropertyGuards())
    {
        LinkGuardToGuardedProperties(entryPointInfo, propertySymOpnd->GetGuardedPropOps(), [=](Js::PropertyId propertyId)
        {
            if (PHASE_TRACE(Js::ObjTypeSpecPhase, this->m_func) || PHASE_TRACE(Js::TracePropertyGuardsPhase, this->m_func))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"ObjTypeSpec: function %s(%s) registered equivalent type spec guard 0x%p with value 0x%p for property %s (%u).\n",
                    this->m_func->GetJnFunction()->GetDisplayName(), this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                    guard, guard->GetValue(), GetScriptContext()->GetPropertyNameLocked(propertyId)->GetBuffer(), propertyId);
                Output::Flush();
            }

            this->m_func->EnsurePropertyGuardsByPropertyId();
            this->m_func->LinkGuardToPropertyId(propertyId, guard);
        });
    }

    Assert(guard->GetCache() != nullptr);
    Js::EquivalentTypeCache* cache = guard->GetCache();

    // TODO (ObjTypeSpec): If we delayed populating the types until encoder, we could bulk allocate all equivalent type caches
    // in one block from the heap. This would allow us to not allocate them from the native code data allocator and free them
    // when no longer needed. However, we would need to store the global property operation ID in the guard, so we can look up
    // the info in the encoder. Perhaps we could overload the cache pointer to be the ID until encoder.

    // Copy types from the type set to the guard's cache
    Js::EquivalentTypeSet* typeSet = propertySymOpnd->GetEquivalentTypeSet();
    uint16 cachedTypeCount = typeSet->GetCount() < EQUIVALENT_TYPE_CACHE_SIZE ? typeSet->GetCount() : EQUIVALENT_TYPE_CACHE_SIZE;
    for (uint16 ti = 0; ti < cachedTypeCount; ti++)
    {
        cache->types[ti] = typeSet->GetType(ti);
    }

    // Populate property ID and slot index arrays on the guard's cache. We iterate over the
    // bit vector of property operations protected by this guard, but some property operations
    // may be referring to the same property ID (but not share the same cache). We skip
    // redundant entries by maintaining a hash set of property IDs we've already encountered.

    auto propOps = propertySymOpnd->GetGuardedPropOps();
    uint propOpCount = propOps->Count();

    bool isTypeStatic = Js::StaticType::Is(type->GetTypeId());
    JsUtil::BaseDictionary<Js::PropertyId, Js::EquivalentPropertyEntry*, JitArenaAllocator> propIds(this->m_alloc, propOpCount);
    Js::EquivalentPropertyEntry* properties = AnewArray(this->m_alloc, Js::EquivalentPropertyEntry, propOpCount);
    uint propIdCount = 0;

    FOREACH_BITSET_IN_SPARSEBV(propOpId, propOps)
    {
        Js::ObjTypeSpecFldInfo* propOpInfo = this->m_func->GetGlobalObjTypeSpecFldInfo(propOpId);
        Js::PropertyId propertyId = propOpInfo->GetPropertyId();
        Js::PropertyIndex propOpIndex = Js::Constants::NoSlot;
        bool hasFixedValue = propOpInfo->HasFixedValue();
        if (hasFixedValue)
        {
            cache->SetHasFixedValue();
        }
        bool isLoadedFromProto = propOpInfo->IsLoadedFromProto();
        if (isLoadedFromProto)
        {
            cache->SetIsLoadedFromProto();
        }
        else
        {
            propOpIndex = propOpInfo->GetSlotIndex();
        }
        bool propOpUsesAuxSlot = propOpInfo->UsesAuxSlot();

        AssertMsg(!isTypeStatic || !propOpInfo->IsBeingStored(), "Why are we storing a field to an object of static type?");

        Js::EquivalentPropertyEntry* entry;
        if (propIds.TryGetValue(propertyId, &entry))
        {
            if (propOpIndex == entry->slotIndex && propOpUsesAuxSlot == entry->isAuxSlot)
            {
                entry->mustBeWritable |= propOpInfo->IsBeingStored();
            }
            else
            {
                // Due to inline cache sharing we have the same property accessed using different caches
                // with inconsistent info. This means a guaranteed bailout on the equivalent type check.
                // We'll just let it happen and turn off the optimization for this function. We could avoid
                // this problem by tracking property information on the value type in glob opt.
                if (PHASE_TRACE(Js::EquivObjTypeSpecPhase, this->m_func))
                {
                    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Js::FunctionBody* topFunctionBody = this->m_func->GetJnFunction();
                    Js::ScriptContext* scriptContext = topFunctionBody->GetScriptContext();
                    Output::Print(L"EquivObjTypeSpec: top function %s (%s): duplicate property clash on %s(#%d) \n",
                        topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer), propertyId, scriptContext->GetPropertyNameLocked(propertyId)->GetBuffer());
                    Output::Flush();
                }
                Assert(propIdCount < propOpCount);
                __analysis_assume(propIdCount < propOpCount);
                entry = &properties[propIdCount++];
                entry->propertyId = propertyId;
                entry->slotIndex = propOpIndex;
                entry->isAuxSlot = propOpUsesAuxSlot;
                entry->mustBeWritable = propOpInfo->IsBeingStored();
            }
        }
        else
        {
            Assert(propIdCount < propOpCount);
            __analysis_assume(propIdCount < propOpCount);
            entry = &properties[propIdCount++];
            entry->propertyId = propertyId;
            entry->slotIndex = propOpIndex;
            entry->isAuxSlot = propOpUsesAuxSlot;
            entry->mustBeWritable = propOpInfo->IsBeingStored();
            propIds.AddNew(propertyId, entry);
        }
    }
    NEXT_BITSET_IN_SPARSEBV;

    cache->record.propertyCount = propIdCount;
    cache->record.properties = NativeCodeDataNewArray(this->m_func->GetNativeCodeDataAllocator(), Js::EquivalentPropertyEntry, propIdCount);

    memcpy(cache->record.properties, properties, propIdCount * sizeof(Js::EquivalentPropertyEntry));

    return guard;
}

bool
Lowerer::LinkCtorCacheToGuardedProperties(Js::JitTimeConstructorCache* ctorCache)
{
    // We do not always have guarded properties. If the constructor is empty and the subsequent code doesn't load or store any of
    // the constructed object's properties, or if all inline caches are empty then this ctor cache doesn't guard any properties.
    if (ctorCache->GetGuardedPropOps() == nullptr)
    {
        return false;
    }

    bool linked = false;
    Js::EntryPointInfo* entryPointInfo = this->m_func->m_workItem->GetEntryPoint();

    if (entryPointInfo->HasSharedPropertyGuards())
    {
        linked = LinkGuardToGuardedProperties(entryPointInfo, ctorCache->GetGuardedPropOps(), [=](Js::PropertyId propertyId)
        {
            if (PHASE_TRACE(Js::ObjTypeSpecPhase, this->m_func) || PHASE_TRACE(Js::TracePropertyGuardsPhase, this->m_func))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"ObjTypeSpec: function %s(%s) registered ctor cache 0x%p with value 0x%p for property %s (%u).\n",
                    this->m_func->GetJnFunction()->GetDisplayName(), this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                    ctorCache->runtimeCache, ctorCache->type, GetScriptContext()->GetPropertyNameLocked(propertyId)->GetBuffer(), propertyId);
                Output::Flush();
            }

            this->m_func->EnsureCtorCachesByPropertyId();
            this->m_func->LinkCtorCacheToPropertyId(propertyId, ctorCache);
        });
    }
    return linked;
}

template<typename LinkFunc>
bool
Lowerer::LinkGuardToGuardedProperties(Js::EntryPointInfo* entryPointInfo, const BVSparse<JitArenaAllocator>* guardedPropOps, LinkFunc link)
{
    Assert(entryPointInfo != nullptr);
    Assert(entryPointInfo->HasSharedPropertyGuards());
    Assert(guardedPropOps != nullptr);
    bool linked = false;

    // For every entry in the bit vector, register the guard for the corresponding property ID.
    FOREACH_BITSET_IN_SPARSEBV(propertyOpId, guardedPropOps)
    {
        Js::ObjTypeSpecFldInfo* propertyOpInfo = this->m_func->GetGlobalObjTypeSpecFldInfo(propertyOpId);
        Js::PropertyId propertyId = propertyOpInfo->GetPropertyId();

        // It's okay for an equivalent type check to be registered as a guard against a property becoming read-only. This transpires if, there is
        // a different monomorphic type check upstream, which guarantees the actual type of the object needed for the hard-coded type transition,
        // but it is later followed by a sequence of polymorphic inline caches, which do not have that type in the type set. At the beginning of
        // that sequence we'll emit an equivalent type check to verify that the actual type has relevant properties on appropriate slots. Then in
        // the dead store pass we'll walk upwards and encounter this check first, thus we'll drop the guarded properties accumulated thus far
        // (including the one being added) on that check.
        // AssertMsg(!propertyOpInfo->IsBeingAdded() || !isEquivalentTypeGuard, "Why do we have an equivalent type check protecting a property add?");

        if (propertyOpInfo->IsBeingAdded() || propertyOpInfo->IsLoadedFromProto() || propertyOpInfo->HasFixedValue())
        {
            // Equivalent object type spec only supports fixed fields on prototypes.  This is to simplify the slow type equivalence check.
            // See JavascriptOperators::CheckIfTypeIsEquivalent.
            Assert(!propertyOpInfo->IsPoly() || (!propertyOpInfo->HasFixedValue() || propertyOpInfo->IsLoadedFromProto() || propertyOpInfo->UsesAccessor()));

            if (entryPointInfo->HasSharedPropertyGuard(propertyId))
            {
                link(propertyId);
                linked = true;
            }
            else
            {
#if TRUE
                AssertMsg(false, "Did we fail to create a shared property guard for a guarded property?");
#else
                if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, this->m_func) || PHASE_TRACE(Js::TracePropertyGuardsPhase, this->m_func))
                {
                    if (!this->m_func->m_workItem->GetEntryPoint()->HasSharedPropertyGuard(propertyId))
                    {
                        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                        Output::Print(L"ObjTypeStore: function %s(%s): no shared property guard for property % (%u).\n",
                            this->m_func->GetJnFunction()->GetDisplayName(), this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                            GetScriptContext()->GetPropertyNameLocked(propertyId)->GetBuffer(), propertyId);
                        Output::Flush();
                    }
                }
#endif
            }
        }
    }
    NEXT_BITSET_IN_SPARSEBV;

    return linked;
}

void
Lowerer::GeneratePropertyGuardCheck(IR::Instr *insertPointInstr, IR::PropertySymOpnd *propertySymOpnd, IR::LabelInstr *labelBailOut)
{
    Js::PropertyGuard* guard = propertySymOpnd->GetPropertyGuard();
    Assert(guard != nullptr);

    if (!DoLazyFixedDataBailout(this->m_func))
    {
        Assert(Js::PropertyGuard::GetSizeOfValue() == static_cast<size_t>(TySize[TyMachPtr]));
        IR::AddrOpnd* zeroOpnd = IR::AddrOpnd::NewNull(this->m_func);
        IR::MemRefOpnd* guardOpnd = IR::MemRefOpnd::New((void*)guard->GetAddressOfValue(), TyMachPtr, this->m_func, IR::AddrOpndKindDynamicGuardValueRef);
        InsertCompareBranch(guardOpnd, zeroOpnd, Js::OpCode::BrEq_A, labelBailOut, insertPointInstr);
    }
    else
    {
        this->m_func->lazyBailoutProperties.Item(propertySymOpnd->GetPropertyId());
    }
}

IR::Instr*
Lowerer::GeneratePropertyGuardCheckBailoutAndLoadType(IR::Instr *insertInstr)
{
    IR::Instr* instrPrev = insertInstr->m_prev;

    IR::Opnd* numberTypeOpnd = IR::AddrOpnd::New(insertInstr->m_func->GetScriptContext()->GetLibrary()->GetNumberTypeStatic(), IR::AddrOpndKindDynamicType, insertInstr->m_func);
    IR::PropertySymOpnd* propertySymOpnd = insertInstr->GetSrc1()->AsPropertySymOpnd();

    IR::LabelInstr* labelBailout = IR::LabelInstr::New(Js::OpCode::Label, insertInstr->m_func, true);
    IR::LabelInstr* labelContinue = IR::LabelInstr::New(Js::OpCode::Label, insertInstr->m_func);
    IR::LabelInstr* loadNumberTypeLabel = IR::LabelInstr::New(Js::OpCode::Label, insertInstr->m_func, true);

    GeneratePropertyGuardCheck(insertInstr, propertySymOpnd, labelBailout);

    IR::RegOpnd *baseOpnd = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);

    GenerateObjectTestAndTypeLoad(insertInstr, baseOpnd, insertInstr->GetDst()->AsRegOpnd(), loadNumberTypeLabel);

    insertInstr->InsertBefore(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelContinue, this->m_func));

    insertInstr->InsertBefore(loadNumberTypeLabel);
    this->m_lowererMD.CreateAssign(insertInstr->GetDst(), numberTypeOpnd, insertInstr);
    insertInstr->InsertBefore(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelContinue, this->m_func));

    insertInstr->InsertBefore(labelBailout);
    insertInstr->InsertAfter(labelContinue);

    insertInstr->FreeSrc1();
    insertInstr->m_opcode = Js::OpCode::BailOut;
    this->GenerateBailOut(insertInstr);

    return instrPrev;
}

void
Lowerer::GenerateNonWritablePropertyCheck(IR::Instr *instrInsert, IR::PropertySymOpnd *propertySymOpnd, IR::LabelInstr *labelBailOut)
{
    IR::Opnd *opnd;
    IR::Instr *instr;

    // Generate a check for non-writable properties, on the model of the work done by PatchPutValueetc.
    // Inline the check on the bit in the prototype object's type. If that check fails, call the helper.
    // If the helper finds a non-writable property, bail out, as we're counting on being able to add the property.

    Js::Type *typeWithoutProperty = propertySymOpnd->GetInitialType();
    Assert(typeWithoutProperty);
    Js::RecyclableObject *protoObject = typeWithoutProperty->GetPrototype();
    Assert(protoObject);

    // s1 = MOV [proto->type].ptr
    IR::RegOpnd *typeOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
    opnd = IR::MemRefOpnd::New((char*)protoObject + Js::RecyclableObject::GetOffsetOfType(), TyMachReg,
        this->m_func, IR::AddrOpndKindDynamicObjectTypeRef);
    m_lowererMD.CreateAssign(typeOpnd, opnd, instrInsert);

    //      TEST [s1->areThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties].u8, 1
    //     JNE $continue
    IR::LabelInstr *labelContinue = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    opnd = IR::IndirOpnd::New(typeOpnd, (int32)Js::Type::OffsetOfWritablePropertiesFlag(), TyUint8, this->m_func);
    InsertTestBranch(opnd, IR::IntConstOpnd::New(1, TyUint8, this->m_func), Js::OpCode::BrNeq_A, labelContinue, instrInsert);

    // $Lhelper:
    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    instrInsert->InsertBefore(labelHelper);

    //     s2 = CALL DoProtoCheck, prototype
    opnd = IR::AddrOpnd::New(protoObject, IR::AddrOpndKindDynamicVar, this->m_func, true);
    m_lowererMD.LoadHelperArgument(instrInsert, opnd);

    opnd = IR::HelperCallOpnd::New(IR::HelperCheckProtoHasNonWritable, this->m_func);
    instr = IR::Instr::New(Js::OpCode::Call, IR::RegOpnd::New(TyUint8, this->m_func), opnd, this->m_func);
    instrInsert->InsertBefore(instr);
    opnd = instr->GetDst();
    m_lowererMD.LowerCall(instr, 0);

    InsertTestBranch(opnd, opnd, Js::OpCode::BrEq_A, labelBailOut, instrInsert);

    // $Lcontinue:
    instrInsert->InsertBefore(labelContinue);
}

void
Lowerer::GenerateAdjustSlots(IR::Instr *instrInsert, IR::PropertySymOpnd *propertySymOpnd, Js::Type* initialType, Js::Type* finalType)
{
    IR::RegOpnd *baseOpnd = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);
    bool adjusted = this->GenerateAdjustBaseSlots(instrInsert, baseOpnd, initialType, finalType);
    if (!adjusted)
    {
        baseOpnd->Free(m_func);
    }
}

bool
Lowerer::GenerateAdjustBaseSlots(IR::Instr *instrInsert, IR::RegOpnd *baseOpnd, Js::Type* initialType, Js::Type* finalType)
{
    // Possibly allocate new slot capacity to accommodate a type transition.
    Js::DynamicType *oldType = static_cast<Js::DynamicType*>(initialType);
    Assert(oldType);
    Js::DynamicType *newType = static_cast<Js::DynamicType*>(finalType);
    Assert(newType);

    AssertMsg(Js::DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining(oldType->GetTypeHandler(), newType->GetTypeHandler()),
        "Incompatible typeHandler transition?");
    int oldCount = oldType->GetTypeHandler()->GetSlotCapacity();
    int newCount = newType->GetTypeHandler()->GetSlotCapacity();
    Js::PropertyIndex inlineSlotCapacity = oldType->GetTypeHandler()->GetInlineSlotCapacity();
    Js::PropertyIndex newInlineSlotCapacity = newType->GetTypeHandler()->GetInlineSlotCapacity();

    if (oldCount >= newCount || newCount <= inlineSlotCapacity)
    {
        // Already have enough slot capacity. Do nothing.
        return false;
    }

    // Call AdjustSlots using the new counts. Because AdjustSlots uses the "no dispose" flavor of alloc,
    // no implicit calls are possible, and we don't need an implicit call check and bailout.

    // CALL AdjustSlots, instance, newInlineSlotCapacity, newAuxSlotCapacity

    //3rd Param
    Assert(newCount > newInlineSlotCapacity);
    const int newAuxSlotCapacity = newCount - newInlineSlotCapacity;
    m_lowererMD.LoadHelperArgument(instrInsert, IR::IntConstOpnd::New(newAuxSlotCapacity, TyInt32, this->m_func));

    //2nd Param
    m_lowererMD.LoadHelperArgument(instrInsert, IR::IntConstOpnd::New(newInlineSlotCapacity, TyUint16, this->m_func));

    //1st Param (instance)
    m_lowererMD.LoadHelperArgument(instrInsert, baseOpnd);

    //CALL HelperAdjustSlots
    IR::Opnd *opnd = IR::HelperCallOpnd::New(IR::HelperAdjustSlots, this->m_func);
    IR::Instr *instr = IR::Instr::New(Js::OpCode::Call, this->m_func);
    instr->SetSrc1(opnd);
    instrInsert->InsertBefore(instr);
    m_lowererMD.LowerCall(instr, 0);

    return true;
}

void
Lowerer::GenerateFieldStoreWithTypeChange(IR::Instr * instrStFld, IR::PropertySymOpnd *propertySymOpnd, Js::Type* initialType, Js::Type* finalType)
{
    // Adjust instance slots, if necessary.
    this->GenerateAdjustSlots(instrStFld, propertySymOpnd, initialType, finalType);

    // We should never add properties to objects of static types.
    Assert(Js::DynamicType::Is(finalType->GetTypeId()));

    // Let's pin the final type to be sure its alive when we try to do the type transition.
    PinTypeRef(finalType, finalType, instrStFld, propertySymOpnd->m_sym->AsPropertySym()->m_propertyId);
    IR::Opnd *finalTypeOpnd = IR::AddrOpnd::New(finalType, IR::AddrOpndKindDynamicType, instrStFld->m_func, true);

    // Set the new type.
    IR::RegOpnd *baseOpnd = propertySymOpnd->CreatePropertyOwnerOpnd(instrStFld->m_func);
    IR::Opnd *opnd = IR::IndirOpnd::New(baseOpnd, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, instrStFld->m_func);
    this->m_lowererMD.CreateAssign(opnd, finalTypeOpnd, instrStFld);

    // Now do the store.
    GenerateDirectFieldStore(instrStFld, propertySymOpnd);
}

bool
Lowerer::GenerateStFldWithCachedFinalType(IR::Instr * instrStFld, IR::PropertySymOpnd *propertySymOpnd)
{
    // This function tries to treat a sequence of add-property stores as a single type transition.
    Assert(propertySymOpnd == instrStFld->GetDst()->AsPropertySymOpnd());
    Assert(propertySymOpnd->IsMonoObjTypeSpecCandidate());
    Assert(propertySymOpnd->HasFinalType());
    Assert(propertySymOpnd->HasInitialType());

    IR::Instr *instr;
    IR::LabelInstr *labelBailOut = nullptr;

    AssertMsg(!propertySymOpnd->IsTypeChecked(), "Why are we doing a type transition when we have the type we want?");

    // If the initial type must be checked here, do it.
    Assert(instrStFld->HasBailOutInfo());
    labelBailOut = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    GenerateCachedTypeWithoutPropertyCheck(instrStFld, propertySymOpnd, nullptr/*typeOpnd*/, labelBailOut);

    // Do the type transition.
    GenerateFieldStoreWithTypeChange(instrStFld, propertySymOpnd, propertySymOpnd->GetInitialType(), propertySymOpnd->GetFinalType());

    instrStFld->FreeSrc1();
    instrStFld->FreeDst();

    // Insert the bailout and let the main path branch around it.
    IR::LabelInstr *labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelDone, this->m_func);
    instrStFld->InsertBefore(instr);

    if (instrStFld->HasBailOutInfo())
    {
        Assert(labelBailOut != nullptr);
        instrStFld->InsertBefore(labelBailOut);
        instrStFld->InsertAfter(labelDone);

        instrStFld->m_opcode = Js::OpCode::BailOut;
        this->GenerateBailOut(instrStFld);
    }
    else
    {
        instrStFld->InsertAfter(labelDone);
        instrStFld->Remove();
    }

    return true;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerScopedStFld
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerScopedStFld(IR::Instr * stFldInstr, IR::JnHelperMethod helperMethod, bool withInlineCache,
    bool withPropertyOperationFlags, Js::PropertyOperationFlags flags)
{
    IR::Instr *instrPrev = stFldInstr->m_prev;

    if (withPropertyOperationFlags)
    {
        m_lowererMD.LoadHelperArgument(stFldInstr,
            IR::IntConstOpnd::New(static_cast<IntConstType>(flags), IRType::TyInt32, m_func, true));
    }

    if(!withInlineCache)
    {
        LoadScriptContext(stFldInstr);
    }

    // Pass the default instance
    IR::Opnd *src = stFldInstr->UnlinkSrc2();
    m_lowererMD.LoadHelperArgument(stFldInstr, src);

    // Pass the value to store
    src = stFldInstr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(stFldInstr, src);

    // Pass the property sym to store to
    IR::Opnd *dst = stFldInstr->UnlinkDst();
    AssertMsg(dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsPropertySym(), "Expected property sym as dst of field store");

    this->LoadPropertySymAsArgument(stFldInstr, dst);

    if (withInlineCache)
    {
        AssertMsg(dst->AsSymOpnd()->IsPropertySymOpnd(), "Need property sym operand to find the inline cache");

        m_lowererMD.LoadHelperArgument(
            stFldInstr,
            IR::Opnd::CreateInlineCacheIndexOpnd(dst->AsPropertySymOpnd()->m_inlineCacheIndex, m_func));

        // Not using the polymorphic inline cache because the fast path only uses the monomorphic inline cache
        this->m_lowererMD.LoadHelperArgument(stFldInstr, this->LoadRuntimeInlineCacheOpnd(stFldInstr, dst->AsPropertySymOpnd()));

        m_lowererMD.LoadHelperArgument(stFldInstr, LoadFunctionBodyOpnd(stFldInstr));
    }

    m_lowererMD.ChangeToHelperCall(stFldInstr, helperMethod);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerLoadVar
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerLoadVar(IR::Instr *instr, IR::Opnd *opnd)
{
    instr->SetSrc1(opnd);
    return m_lowererMD.ChangeToAssign(instr);
}

IR::Instr *
Lowerer::LoadHelperTemp(IR::Instr * instr, IR::Instr * instrInsert)
{
    IR::Opnd *tempOpnd;
    IR::Opnd *dst = instr->GetDst();
    AssertMsg(dst != nullptr, "Always expect a dst for these.");
    AssertMsg(instr->dstIsTempNumber, "Should only be loading temps here");

    Assert(dst->IsRegOpnd());
    StackSym * tempNumberSym = this->GetTempNumberSym(dst, instr->dstIsTempNumberTransferred);

    IR::Instr *load = this->m_lowererMD.LoadStackAddress(tempNumberSym);
    instrInsert->InsertBefore(load);
    tempOpnd = load->GetDst();
    m_lowererMD.LoadHelperArgument(instrInsert, tempOpnd);
    return load;
}

void
Lowerer::LoadArgumentCount(IR::Instr *const instr)
{
    Assert(instr);
    Assert(instr->GetDst());
    Assert(!instr->GetSrc1());
    Assert(!instr->GetSrc2());

    if(instr->m_func->IsInlinee())
    {
        // Argument count including 'this'
        instr->SetSrc1(IR::IntConstOpnd::New(instr->m_func->actualCount, TyUint32, instr->m_func, true));
        LowererMD::ChangeToAssign(instr);
    }
    else if (instr->m_func->GetJnFunction()->IsGenerator())
    {
        IR::SymOpnd* symOpnd = LoadCallInfo(instr);
        instr->SetSrc1(symOpnd);
        LowererMD::ChangeToAssign(instr);
    }
    else
    {
        m_lowererMD.LoadArgumentCount(instr);
    }
}

void
Lowerer::LoadStackArgPtr(IR::Instr *const instr)
{
    Assert(instr);
    Assert(instr->GetDst());
    Assert(!instr->GetSrc1());
    Assert(!instr->GetSrc2());

    if(instr->m_func->IsInlinee())
    {
        // Address of argument after 'this'
        const auto firstRealArgStackSym = instr->m_func->GetInlineeArgvSlotOpnd()->m_sym->AsStackSym();
        this->m_func->SetArgOffset(firstRealArgStackSym, firstRealArgStackSym->m_offset + MachPtr);
        instr->SetSrc1(IR::SymOpnd::New(firstRealArgStackSym, TyMachPtr, instr->m_func));
        LowererMD::ChangeToLea(instr);
    }
    else
    {
        m_lowererMD.LoadStackArgPtr(instr);
    }
}

void
Lowerer::LoadArgumentsFromFrame(IR::Instr *const instr)
{
    Assert(instr);
    Assert(instr->GetDst());
    Assert(!instr->GetSrc1());
    Assert(!instr->GetSrc2());

    if(instr->m_func->IsInlinee())
    {
        // Use the inline object meta arg slot for the arguments object
        instr->SetSrc1(instr->m_func->GetInlineeArgumentsObjectSlotOpnd());
        LowererMD::ChangeToAssign(instr);
    }
    else
    {
        m_lowererMD.LoadArgumentsFromFrame(instr);
    }
}

IR::Instr *
Lowerer::LowerUnaryHelper(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    IR::Instr *instrPrev;

    IR::Opnd *src1 = instr->UnlinkSrc1();
    instrPrev = m_lowererMD.LoadHelperArgument(instr, src1);

    m_lowererMD.ChangeToHelperCall(instr, helperMethod);

    return instrPrev;
}

// helper takes memory context as second argument
IR::Instr *
Lowerer::LowerUnaryHelperMem(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    IR::Instr *instrPrev;

    instrPrev = LoadScriptContext(instr);

    return this->LowerUnaryHelper(instr, helperMethod);
}

IR::Instr *
Lowerer::LowerUnaryHelperMemWithFuncBody(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    m_lowererMD.LoadHelperArgument(instr, this->LoadFunctionBodyOpnd(instr));
    return this->LowerUnaryHelperMem(instr, helperMethod);
}

IR::Instr *
Lowerer::LowerBinaryHelperMemWithFuncBody(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    AssertMsg(Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg1Int2, "Expected a binary instruction...");

    m_lowererMD.LoadHelperArgument(instr, this->LoadFunctionBodyOpnd(instr));
    return this->LowerBinaryHelperMem(instr, helperMethod);
}

IR::Instr *
Lowerer::LowerUnaryHelperMemWithTemp(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    AssertMsg(Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg2, "Expected a unary instruction...");

    IR::Instr * instrFirst;
    IR::Opnd * tempOpnd;
    if (instr->dstIsTempNumber)
    {
        instrFirst = this->LoadHelperTemp(instr, instr);
    }
    else
    {
        tempOpnd = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
        instrFirst = m_lowererMD.LoadHelperArgument(instr, tempOpnd);
    }

    this->LowerUnaryHelperMem(instr, helperMethod);

    return instrFirst;
}

IR::Instr *
Lowerer::LowerUnaryHelperMemWithTemp2(IR::Instr *instr, IR::JnHelperMethod helperMethod, IR::JnHelperMethod helperMethodWithTemp)
{
    AssertMsg(Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg2, "Expected a unary instruction...");

    if (instr->dstIsTempNumber)
    {
        IR::Instr * instrFirst = this->LoadHelperTemp(instr, instr);
        this->LowerUnaryHelperMem(instr, helperMethodWithTemp);
        return instrFirst;
    }

    return this->LowerUnaryHelperMem(instr, helperMethod);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerBinaryHelper
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerBinaryHelper(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    // The only case where this would still be null when we return is when
    // helperMethod == HelperOP_CmSrEq_EmptyString; in which case we ignore
    // instrPrev.
    IR::Instr *instrPrev = nullptr;

    AssertMsg((Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg1Unsigned1 && !instr->GetDst()) ||
              Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg3 ||
              Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg2Int1 ||
              Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg1Int2 ||
              Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::ElementU ||
              instr->m_opcode == Js::OpCode::InvalCachedScope, "Expected a binary instruction...");

    IR::Opnd *src2 = instr->UnlinkSrc2();
    if (helperMethod != IR::HelperOP_CmSrEq_EmptyString)
        instrPrev = m_lowererMD.LoadHelperArgument(instr, src2);

    IR::Opnd *src1 = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, src1);

    m_lowererMD.ChangeToHelperCall(instr, helperMethod);

    return instrPrev;
}

// helper takes memory context as third argument
IR::Instr *
Lowerer::LowerBinaryHelperMem(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    IR::Instr *instrPrev;

    AssertMsg(Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg3 ||
              Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg2Int1 ||
              Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg1Int2, "Expected a binary instruction...");

    instrPrev = LoadScriptContext(instr);

    return this->LowerBinaryHelper(instr, helperMethod);
}

IR::Instr *
Lowerer::LowerBinaryHelperMemWithTemp(IR::Instr *instr, IR::JnHelperMethod helperMethod)
{
    AssertMsg(Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg3, "Expected a binary instruction...");

    IR::Instr * instrFirst;
    IR::Opnd * tempOpnd;

    if (instr->dstIsTempNumber)
    {
        instrFirst = this->LoadHelperTemp(instr, instr);
    }
    else
    {
        tempOpnd = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
        instrFirst = m_lowererMD.LoadHelperArgument(instr, tempOpnd);
    }

    this->LowerBinaryHelperMem(instr, helperMethod);

    return instrFirst;
}

IR::Instr *
Lowerer::LowerBinaryHelperMemWithTemp2(
    IR::Instr *instr,
    IR::JnHelperMethod helperMethod,
    IR::JnHelperMethod helperMethodWithTemp
    )
{
    AssertMsg(Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::Reg3, "Expected a binary instruction...");

    if (instr->dstIsTempNumber && instr->GetDst() && instr->GetDst()->GetValueType().HasBeenNumber())
    {
        IR::Instr * instrFirst = this->LoadHelperTemp(instr, instr);
        this->LowerBinaryHelperMem(instr, helperMethodWithTemp);
        return instrFirst;
    }

    return this->LowerBinaryHelperMem(instr, helperMethod);
}

IR::Instr *
Lowerer::LowerAddLeftDeadForString(IR::Instr *instr)
{
    IR::Opnd *       opndLeft;
    IR::Opnd *       opndRight;

    opndLeft = instr->GetSrc1();
    opndRight = instr->GetSrc2();

    Assert(opndLeft && opndRight);

    bool generateFastPath = this->m_func->DoFastPaths();
    if (!generateFastPath
        || !opndLeft->IsRegOpnd()
        || !opndRight->IsRegOpnd()
        || !instr->GetDst()->IsRegOpnd()
        || !opndLeft->GetValueType().IsLikelyString()
        || !opndRight->GetValueType().IsLikelyString()
        || !opndLeft->IsEqual(instr->GetDst()->AsRegOpnd())
        || opndLeft->IsEqual(opndRight))
    {
        return this->LowerBinaryHelperMemWithTemp(instr, IR::HelperOp_AddLeftDead);
    }

    IR::LabelInstr * labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::LabelInstr * labelFallThrough = instr->GetOrCreateContinueLabel(false);
    IR::LabelInstr *insertBeforeInstr = labelHelper;

    instr->InsertBefore(labelHelper);

    if (!opndLeft->IsNotTaggedValue())
    {
        this->m_lowererMD.GenerateObjectTest(opndLeft->AsRegOpnd(), insertBeforeInstr, labelHelper);
    }

    InsertCompareBranch(
        IR::IndirOpnd::New(opndLeft->AsRegOpnd(), 0, TyMachPtr, m_func),
        this->LoadVTableValueOpnd(insertBeforeInstr, VTableValue::VtableCompoundString),
        Js::OpCode::BrNeq_A,
        labelHelper,
        insertBeforeInstr);

    GenerateStringTest(opndRight->AsRegOpnd(), insertBeforeInstr, labelHelper);

    // left->m_charLength <= JavascriptArray::MaxCharLength
    IR::IndirOpnd *indirLeftCharLengthOpnd = IR::IndirOpnd::New(opndLeft->AsRegOpnd(), Js::JavascriptString::GetOffsetOfcharLength(), TyUint32, m_func);
    IR::RegOpnd *regLeftCharLengthOpnd = IR::RegOpnd::New(TyUint32, m_func);
    InsertMove(regLeftCharLengthOpnd, indirLeftCharLengthOpnd, insertBeforeInstr);
    InsertCompareBranch(
        regLeftCharLengthOpnd,
        IR::IntConstOpnd::New(Js::JavascriptString::MaxCharLength, TyUint32, m_func),
        Js::OpCode::BrGt_A,
        labelHelper,
        insertBeforeInstr);

    // left->m_pszValue == NULL   (!left->IsFinalized())
    InsertCompareBranch(
        IR::IndirOpnd::New(opndLeft->AsRegOpnd(), offsetof(Js::JavascriptString, m_pszValue), TyMachPtr, this->m_func),
        IR::AddrOpnd::NewNull(m_func),
        Js::OpCode::BrNeq_A,
        labelHelper,
        insertBeforeInstr);

    // right->m_pszValue != NULL   (right->IsFinalized())
    InsertCompareBranch(
        IR::IndirOpnd::New(opndRight->AsRegOpnd(), offsetof(Js::JavascriptString, m_pszValue), TyMachPtr, this->m_func),
        IR::AddrOpnd::NewNull(m_func),
        Js::OpCode::BrEq_A,
        labelHelper,
        insertBeforeInstr);

    // if ownsLastBlock != 0
    InsertCompareBranch(
        IR::IndirOpnd::New(opndLeft->AsRegOpnd(), (int32)Js::CompoundString::GetOffsetOfOwnsLastBlock(), TyUint8, m_func),
        IR::IntConstOpnd::New(0, TyUint8, m_func),
        Js::OpCode::BrEq_A,
        labelHelper,
        insertBeforeInstr);

    // if right->m_charLength == 1
    InsertCompareBranch(IR::IndirOpnd::New(opndRight->AsRegOpnd(), offsetof(Js::JavascriptString, m_charLength), TyUint32, m_func),
        IR::IntConstOpnd::New(1, TyUint32, m_func),
        Js::OpCode::BrNeq_A, labelHelper, insertBeforeInstr);


    // if left->m_directCharLength == -1
    InsertCompareBranch(IR::IndirOpnd::New(opndLeft->AsRegOpnd(), (int32)Js::CompoundString::GetOffsetOfDirectCharLength(), TyUint32, m_func),
        IR::IntConstOpnd::New(UINT32_MAX, TyUint32, m_func),
        Js::OpCode::BrNeq_A, labelHelper, insertBeforeInstr);

    // if lastBlockInfo.charLength < lastBlockInfo.charCapacity
    IR::IndirOpnd *indirCharLength = IR::IndirOpnd::New(opndLeft->AsRegOpnd(), (int32)Js::CompoundString::GetOffsetOfLastBlockInfo()+ (int32)Js::CompoundString::GetOffsetOfLastBlockInfoCharLength(), TyMachPtr, m_func);
    IR::RegOpnd *charLengthOpnd = IR::RegOpnd::New(TyUint32, this->m_func);
    InsertMove(charLengthOpnd, indirCharLength, insertBeforeInstr);
    InsertCompareBranch(charLengthOpnd, IR::IndirOpnd::New(opndLeft->AsRegOpnd(), (int32)Js::CompoundString::GetOffsetOfLastBlockInfo() + (int32)Js::CompoundString::GetOffsetOfLastBlockInfoCharCapacity(), TyMachPtr, m_func), Js::OpCode::BrGe_A, labelHelper, insertBeforeInstr);

    // load c= right->m_pszValue[0]
    IR::RegOpnd *pszValue0Opnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::IndirOpnd *indirRightPszOpnd = IR::IndirOpnd::New(opndRight->AsRegOpnd(), offsetof(Js::JavascriptString, m_pszValue), TyMachPtr, this->m_func);
    InsertMove(pszValue0Opnd, indirRightPszOpnd, insertBeforeInstr);
    IR::RegOpnd *charResultOpnd = IR::RegOpnd::New(TyUint16, this->m_func);
    InsertMove(charResultOpnd, IR::IndirOpnd::New(pszValue0Opnd, 0, TyUint16, this->m_func), insertBeforeInstr);


    // lastBlockInfo.buffer[blockCharLength] = c;
    IR::RegOpnd *baseOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    InsertMove(baseOpnd, IR::IndirOpnd::New(opndLeft->AsRegOpnd(), (int32)Js::CompoundString::GetOffsetOfLastBlockInfo() + (int32)Js::CompoundString::GetOffsetOfLastBlockInfoBuffer(), TyMachPtr, m_func), insertBeforeInstr);
    IR::IndirOpnd *indirBufferToStore = IR::IndirOpnd::New(baseOpnd, charLengthOpnd, (byte)Math::Log2(sizeof(wchar_t)), TyUint16, m_func);
    InsertMove(indirBufferToStore, charResultOpnd, insertBeforeInstr);


    // left->m_charLength++
    InsertAdd(false, indirLeftCharLengthOpnd, regLeftCharLengthOpnd, IR::IntConstOpnd::New(1, TyUint32, this->m_func), insertBeforeInstr);

    // lastBlockInfo.charLength++
    InsertAdd(false, indirCharLength, indirCharLength, IR::IntConstOpnd::New(1, TyUint32, this->m_func), insertBeforeInstr);


    InsertBranch(Js::OpCode::Br, labelFallThrough, insertBeforeInstr);

    return this->LowerBinaryHelperMemWithTemp(instr, IR::HelperOp_AddLeftDead);
}

IR::Instr *
Lowerer::LowerBinaryHelperMemWithTemp3(IR::Instr *instr, IR::JnHelperMethod helperMethod, IR::JnHelperMethod helperMethodWithTemp, IR::JnHelperMethod helperMethodLeftDead)
{
    IR::Opnd *src1 = instr->GetSrc1();

    if (src1->IsRegOpnd() && src1->AsRegOpnd()->m_isTempLastUse && !src1->GetValueType().IsNotString())
    {
        Assert(helperMethodLeftDead == IR::HelperOp_AddLeftDead);
        return LowerAddLeftDeadForString(instr);
    }
    else
    {
        return this->LowerBinaryHelperMemWithTemp2(instr, helperMethod, helperMethodWithTemp);
    }
}

StackSym *
Lowerer::GetTempNumberSym(IR::Opnd * opnd, bool isTempTransferred)
{
    AssertMsg(opnd->IsRegOpnd(), "Expected regOpnd");

    if (isTempTransferred)
    {
        StackSym * tempNumberSym = StackSym::New(TyMisc, m_func);
        this->m_func->StackAllocate(tempNumberSym, sizeof(Js::JavascriptNumber));
        return tempNumberSym;
    }
    StackSym * stackSym = opnd->AsRegOpnd()->m_sym;
    StackSym * tempNumberSym = stackSym->m_tempNumberSym;

    if (tempNumberSym == nullptr)
    {
        tempNumberSym = StackSym::New(TyMisc, m_func);
        this->m_func->StackAllocate(tempNumberSym, sizeof(Js::JavascriptNumber));
        stackSym->m_tempNumberSym = tempNumberSym;
    }
    return tempNumberSym;
}

void Lowerer::LowerProfiledLdElemI(IR::JitProfilingInstr *const instr)
{
    Assert(instr);

    /*
        Var ProfilingHelpers::ProfiledLdElem(
            const Var base,
            const Var varIndex,
            FunctionBody *const functionBody,
            const ProfileId profileId)
    */

    Func *const func = instr->m_func;

    m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(instr->profileId, func));
    m_lowererMD.LoadHelperArgument(instr, CreateFunctionBodyOpnd(func));
    IR::IndirOpnd *const indir = instr->UnlinkSrc1()->AsIndirOpnd();
    IR::Opnd *const indexOpnd = indir->UnlinkIndexOpnd();
    Assert(indexOpnd || indir->GetOffset() >= 0 && !Js::TaggedInt::IsOverflow(indir->GetOffset()));
    m_lowererMD.LoadHelperArgument(
        instr,
        indexOpnd
            ? static_cast<IR::Opnd *>(indexOpnd)
            : IR::AddrOpnd::New(Js::TaggedInt::ToVarUnchecked(indir->GetOffset()), IR::AddrOpndKindDynamicVar, func));
    m_lowererMD.LoadHelperArgument(instr, indir->UnlinkBaseOpnd());
    indir->Free(func);

    instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperProfiledLdElem, func));
    m_lowererMD.LowerCall(instr, 0);
}

void Lowerer::LowerProfiledStElemI(IR::JitProfilingInstr *const instr, const Js::PropertyOperationFlags flags)
{
    Assert(instr);

    /*
        void ProfilingHelpers::ProfiledStElem(
            const Var base,
            const Var varIndex,
            const Var value,
            FunctionBody *const functionBody,
            const ProfileId profileId,
            const PropertyOperationFlags flags)
    */

    Func *const func = instr->m_func;

    IR::JnHelperMethod helper;
    if(flags == Js::PropertyOperation_None)
    {
        helper = IR::HelperProfiledStElem_DefaultFlags;
    }
    else
    {
        helper = IR::HelperProfiledStElem;
        m_lowererMD.LoadHelperArgument(instr, IR::IntConstOpnd::New(flags, TyInt32, func, true));
    }
    m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(instr->profileId, func));
    m_lowererMD.LoadHelperArgument(instr, CreateFunctionBodyOpnd(func));
    m_lowererMD.LoadHelperArgument(instr, instr->UnlinkSrc1());
    IR::IndirOpnd *const indir = instr->UnlinkDst()->AsIndirOpnd();
    IR::Opnd *const indexOpnd = indir->UnlinkIndexOpnd();
    Assert(indexOpnd || indir->GetOffset() >= 0 && !Js::TaggedInt::IsOverflow(indir->GetOffset()));
    m_lowererMD.LoadHelperArgument(
        instr,
        indexOpnd
            ? static_cast<IR::Opnd *>(indexOpnd)
            : IR::AddrOpnd::New(Js::TaggedInt::ToVarUnchecked(indir->GetOffset()), IR::AddrOpndKindDynamicVar, func));
    m_lowererMD.LoadHelperArgument(instr, indir->UnlinkBaseOpnd());
    indir->Free(func);

    instr->SetSrc1(IR::HelperCallOpnd::New(helper, func));
    m_lowererMD.LowerCall(instr, 0);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerStElemI
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerStElemI(IR::Instr * instr, Js::PropertyOperationFlags flags, bool isHelper, IR::JnHelperMethod helperMethod)
{
    IR::Instr *instrPrev = instr->m_prev;

    if (instr->IsJitProfilingInstr())
    {
        Assert(!isHelper);
        LowerProfiledStElemI(instr->AsJitProfilingInstr(), flags);
        return instrPrev;
    }

    IR::Opnd *src1 = instr->GetSrc1();
    IR::Opnd *dst = instr->GetDst();
    IR::Opnd *newDst = nullptr;
    IRType srcType = src1->GetType();

    AssertMsg(dst->IsIndirOpnd(), "Expected indirOpnd on StElementI");

#if !FLOATVAR
    if (dst->AsIndirOpnd()->GetBaseOpnd()->GetValueType().IsLikelyOptimizedTypedArray() && src1->IsRegOpnd())
    {
        // We allow the source of typedArray StElem to be marked as temp, since we just need the value,
        // however if the array turns out to be a non-typed array, or the index isn't valid (the value is then stored as a property)
        // the temp needs to be boxed if it is a float.  The BoxStackNumber helper will box JavascriptNumbers
        // which are on the stack.

        // regVar = BoxStackNumber(src1, scriptContext)
        IR::Instr *newInstr = IR::Instr::New(Js::OpCode::Call, this->m_func);
        IR::RegOpnd *regVar = IR::RegOpnd::New(TyVar, this->m_func);
        newInstr->SetDst(regVar);
        newInstr->SetSrc1(src1);
        instr->InsertBefore(newInstr);
        LowerUnaryHelperMem(newInstr, IR::HelperBoxStackNumber);

        // MOV src1, regVar
        newInstr = IR::Instr::New(Js::OpCode::Ld_A, src1, regVar, this->m_func);
        instr->InsertBefore(m_lowererMD.ChangeToAssign(newInstr));
    }
#endif

    if(instr->HasBailOutInfo())
    {
        IR::BailOutKind bailOutKind = instr->GetBailOutKind();
        if(bailOutKind & IR::BailOutOnInvalidatedArrayHeadSegment)
        {
            Assert(!(bailOutKind & IR::BailOutOnMissingValue));
            LowerBailOnInvalidatedArrayHeadSegment(instr, isHelper);
            bailOutKind ^= IR::BailOutOnInvalidatedArrayHeadSegment;
            Assert(!bailOutKind || instr->GetBailOutKind() == bailOutKind);
        }
        else if(bailOutKind & IR::BailOutOnMissingValue)
        {
            LowerBailOnCreatedMissingValue(instr, isHelper);
            bailOutKind ^= IR::BailOutOnMissingValue;
            Assert(!bailOutKind || instr->GetBailOutKind() == bailOutKind);
        }
        if(bailOutKind & IR::BailOutOnInvalidatedArrayLength)
        {
            LowerBailOnInvalidatedArrayLength(instr, isHelper);
            bailOutKind ^= IR::BailOutOnInvalidatedArrayLength;
            Assert(!bailOutKind || instr->GetBailOutKind() == bailOutKind);
        }
        if(bailOutKind & IR::BailOutConvertedNativeArray)
        {
            IR::LabelInstr *labelSkipBailOut = IR::LabelInstr::New(Js::OpCode::Label, m_func, isHelper);
            instr->InsertAfter(labelSkipBailOut);
            LowerOneBailOutKind(instr, IR::BailOutConvertedNativeArray, isHelper);
            newDst = IR::RegOpnd::New(TyMachReg, m_func);
            InsertTestBranch(newDst, newDst, Js::OpCode::BrEq_A, labelSkipBailOut, instr->m_next);
        }
    }

    instr->UnlinkDst();
    instr->UnlinkSrc1();

    IR::Opnd *indexOpnd = dst->AsIndirOpnd()->UnlinkIndexOpnd();

    Assert(
        helperMethod == IR::HelperOP_InitElemGetter ||
        helperMethod == IR::HelperOP_InitElemSetter ||
        helperMethod == IR::HelperOP_InitComputedProperty ||
        helperMethod == IR::HelperOp_SetElementI ||
        helperMethod == IR::HelperOp_InitClassMemberComputedName ||
        helperMethod == IR::HelperOp_InitClassMemberGetComputedName ||
        helperMethod == IR::HelperOp_InitClassMemberSetComputedName
        );

    if (indexOpnd && indexOpnd->GetType() != TyVar)
    {
        if (indexOpnd->GetType() == TyInt32)
        {
            helperMethod =
                srcType == TyVar ? IR::HelperOp_SetElementI_Int32 :
                srcType == TyInt32 ? IR::HelperOp_SetNativeIntElementI_Int32 :
                IR::HelperOp_SetNativeFloatElementI_Int32;
        }
        else if (indexOpnd->GetType() == TyUint32)
        {
            helperMethod =
                srcType == TyVar ? IR::HelperOp_SetElementI_UInt32 :
                srcType == TyInt32 ? IR::HelperOp_SetNativeIntElementI_UInt32 :
                IR::HelperOp_SetNativeFloatElementI_UInt32;
        }
        else
        {
            Assert(FALSE);
        }
    }
    else
    {
        if (indexOpnd == nullptr)
        {
            // No index; the offset identifies the element.
            IntConstType offset = (IntConstType)dst->AsIndirOpnd()->GetOffset();
            indexOpnd = IR::AddrOpnd::NewFromNumber(offset, m_func);
        }

        if (srcType != TyVar)
        {
            helperMethod =
                srcType == TyInt32 ? IR::HelperOp_SetNativeIntElementI : IR::HelperOp_SetNativeFloatElementI;
        }
    }

    if (srcType == TyFloat64)
    {
        // We don't support the X64 floating-point calling convention. So put this parameter on the end
        // and save directly to the stack slot.
#if _M_X64
        IR::Opnd *argOpnd = IR::SymOpnd::New(m_func->m_symTable->GetArgSlotSym(5), TyFloat64, m_func);
        m_lowererMD.CreateAssign(argOpnd, src1, instr);
#else
        m_lowererMD.LoadDoubleHelperArgument(instr, src1);
#endif
    }
    m_lowererMD.LoadHelperArgument(instr,
        IR::IntConstOpnd::New(static_cast<IntConstType>(flags), IRType::TyInt32, m_func, true));
    LoadScriptContext(instr);
    if (srcType != TyFloat64)
    {
        m_lowererMD.LoadHelperArgument(instr, src1);
    }
    m_lowererMD.LoadHelperArgument(instr, indexOpnd);

    IR::Opnd *baseOpnd = dst->AsIndirOpnd()->UnlinkBaseOpnd();
    m_lowererMD.LoadHelperArgument(instr, baseOpnd);

    dst->Free(this->m_func);
    if (newDst)
    {
        instr->SetDst(newDst);
    }

    m_lowererMD.ChangeToHelperCall(instr, helperMethod, nullptr, nullptr, nullptr, isHelper);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerLdElemI
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerLdElemI(IR::Instr * instr, IR::JnHelperMethod helperMethod, bool isHelper)
{
    IR::Instr *instrPrev = instr->m_prev;

    if(instr->IsJitProfilingInstr())
    {
        Assert(helperMethod == IR::HelperOp_GetElementI);
        Assert(!isHelper);
        LowerProfiledLdElemI(instr->AsJitProfilingInstr());
        return instrPrev;
    }

    if (!isHelper && instr->DoStackArgsOpt(this->m_func))
    {
        IR::LabelInstr * labelLdElem = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func);
        // Pass in null for labelFallThru to only generate the LdHeapArgument call
        GenerateFastArgumentsLdElemI(instr, labelLdElem, nullptr);
        instr->InsertBefore(labelLdElem);
    }

    IR::Opnd *src1 = instr->UnlinkSrc1();
    AssertMsg(src1->IsIndirOpnd(), "Expected indirOpnd");
    IR::IndirOpnd *indirOpnd = src1->AsIndirOpnd();
    bool loadScriptContext = true;
    IRType dstType = instr->GetDst()->GetType();

    IR::Opnd *indexOpnd = indirOpnd->UnlinkIndexOpnd();
    if (indexOpnd && indexOpnd->GetType() != TyVar)
    {
        Assert(indexOpnd->GetType() == TyUint32 || indexOpnd->GetType() == TyInt32);
        switch (helperMethod)
        {
        case IR::HelperOp_GetElementI:

            if (indexOpnd->GetType() == TyUint32)
            {
                helperMethod =
                    dstType == TyVar ? IR::HelperOp_GetElementI_UInt32 :
                    dstType == TyInt32 ? IR::HelperOp_GetNativeIntElementI_UInt32 :
                    IR::HelperOp_GetNativeFloatElementI_UInt32;
            }
            else
            {
                helperMethod =
                    dstType == TyVar ? IR::HelperOp_GetElementI_Int32 :
                    dstType == TyInt32 ? IR::HelperOp_GetNativeIntElementI_Int32 :
                    IR::HelperOp_GetNativeFloatElementI_Int32;
            }
            break;

        case IR::HelperOp_GetMethodElement:

            Assert(dstType == TyVar);
            helperMethod = indexOpnd->GetType() == TyUint32?
                IR::HelperOp_GetMethodElement_UInt32 : IR::HelperOp_GetMethodElement_Int32;
            break;

        case IR::HelperOp_TypeofElem:

            Assert(dstType == TyVar);
            helperMethod = indexOpnd->GetType() == TyUint32?
                IR::HelperOp_TypeofElem_UInt32 : IR::HelperOp_TypeofElem_Int32;
            break;

        default:
            Assert(false);
        }
    }
    else
    {
        if (indexOpnd == nullptr)
        {
            // No index; the offset identifies the element.
            IntConstType offset = (IntConstType)src1->AsIndirOpnd()->GetOffset();
            indexOpnd = IR::AddrOpnd::NewFromNumber(offset, m_func);
        }

        if (dstType != TyVar)
        {
            loadScriptContext = false;
            helperMethod =
                dstType == TyInt32 ? IR::HelperOp_GetNativeIntElementI : IR::HelperOp_GetNativeFloatElementI;
        }
    }

    // Jitted loop bodies have volatile information about values created outside the loop, so don't update array creation site
    // profile data from jitted loop bodies
    if(!m_func->IsLoopBody())
    {
        const ValueType baseValueType(indirOpnd->GetBaseOpnd()->GetValueType());
        if( baseValueType.IsLikelyObject() &&
            baseValueType.GetObjectType() == ObjectType::Array &&
            !baseValueType.HasIntElements())
        {
            switch(helperMethod)
            {
                case IR::HelperOp_GetElementI:
                    helperMethod =
                        baseValueType.HasFloatElements()
                            ? IR::HelperOp_GetElementI_ExpectingNativeFloatArray
                            : IR::HelperOp_GetElementI_ExpectingVarArray;
                    break;

                case IR::HelperOp_GetElementI_UInt32:
                    helperMethod =
                        baseValueType.HasFloatElements()
                            ? IR::HelperOp_GetElementI_UInt32_ExpectingNativeFloatArray
                            : IR::HelperOp_GetElementI_UInt32_ExpectingVarArray;
                    break;

                case IR::HelperOp_GetElementI_Int32:
                    helperMethod =
                        baseValueType.HasFloatElements()
                            ? IR::HelperOp_GetElementI_Int32_ExpectingNativeFloatArray
                            : IR::HelperOp_GetElementI_Int32_ExpectingVarArray;
                    break;
            }
        }
    }

    if (loadScriptContext)
    {
        LoadScriptContext(instr);
    }

    m_lowererMD.LoadHelperArgument(instr, indexOpnd);

    IR::Opnd *baseOpnd = indirOpnd->UnlinkBaseOpnd();
    m_lowererMD.LoadHelperArgument(instr, baseOpnd);

    src1->Free(this->m_func);

    m_lowererMD.ChangeToHelperCall(instr, helperMethod, nullptr, nullptr, nullptr, isHelper);

    return instrPrev;
}

void Lowerer::LowerLdLen(IR::Instr *const instr, const bool isHelper)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::LdLen_A);

    // LdLen has persisted to this point for the sake of pre-lower opts.
    // Turn it into a LdFld of the "length" property.
    // This is normally a load of the internal "length" of an Array, so it probably doesn't benefit
    // from inline caching.

    // Changing the opcode to LdFld is done in LowerLdFld and needs to remain that way to take into
    //    account ProfiledLdLen_A

    IR::RegOpnd * baseOpnd = instr->UnlinkSrc1()->AsRegOpnd();
    PropertySym* fieldSym = PropertySym::FindOrCreate(baseOpnd->m_sym->m_id, Js::PropertyIds::length, (uint32)-1, (uint)-1, PropertyKindData, m_func);
    baseOpnd->Free(this->m_func);
    instr->SetSrc1(IR::SymOpnd::New(fieldSym, TyVar, m_func));
    LowerLdFld(instr, IR::HelperOp_GetProperty, IR::HelperOp_GetProperty, false, nullptr, isHelper);
}

IR::Instr *
Lowerer::LowerLdArrViewElem(IR::Instr * instr)
{
    Assert(m_func->GetJnFunction()->GetIsAsmjsMode());
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::LdInt8ArrViewElem ||
        instr->m_opcode == Js::OpCode::LdUInt8ArrViewElem   ||
        instr->m_opcode == Js::OpCode::LdInt16ArrViewElem   ||
        instr->m_opcode == Js::OpCode::LdUInt16ArrViewElem  ||
        instr->m_opcode == Js::OpCode::LdInt32ArrViewElem   ||
        instr->m_opcode == Js::OpCode::LdUInt32ArrViewElem  ||
        instr->m_opcode == Js::OpCode::LdFloat32ArrViewElem ||
        instr->m_opcode == Js::OpCode::LdFloat64ArrViewElem);

    IR::Instr * instrPrev = instr->m_prev;

    IR::RegOpnd * indexOpnd = instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd();

    IR::Opnd * dst = instr->GetDst();
    IR::Opnd * src1 = instr->GetSrc1();
    IR::Opnd * src2 = instr->GetSrc2();

    IR::Instr * done;
    if (indexOpnd || (uint32)src1->AsIndirOpnd()->GetOffset() >= 0x1000000)
    {
        // CMP indexOpnd, src2(arrSize)
        // JA $helper
        // JMP $load
        // $helper:
        // MOV dst, 0
        // JMP $done
        // $load:
        // MOV dst, src1([arrayBuffer + indexOpnd])
        // $done:

        Assert(!dst->IsFloat32() || src1->IsFloat32());
        Assert(!dst->IsFloat64() || src1->IsFloat64());
        done = m_lowererMD.LowerAsmJsLdElemHelper(instr);
    }
    else
    {
        // any access below 0x1000000 is safe
        instr->UnlinkDst();
        instr->UnlinkSrc1();
        if (src2)
        {
            instr->FreeSrc2();
        }
        done = instr;
    }
    InsertMove(dst, src1, done);

    instr->Remove();
    return instrPrev;
}

void
Lowerer::LowerMemset(IR::Instr * instr, IR::RegOpnd * helperRet)
{
    IR::Opnd * dst = instr->UnlinkDst();
    IR::Opnd * src1 = instr->UnlinkSrc1();

    Assert(dst->IsIndirOpnd());
    IR::Opnd *baseOpnd = dst->AsIndirOpnd()->UnlinkBaseOpnd();
    IR::Opnd *indexOpnd = dst->AsIndirOpnd()->UnlinkIndexOpnd();

    IR::Opnd *sizeOpnd = instr->UnlinkSrc2();

    Assert(baseOpnd);
    Assert(sizeOpnd);
    Assert(indexOpnd);

    IR::JnHelperMethod helperMethod = IR::HelperOp_Memset;

    instr->SetDst(helperRet);
    LoadScriptContext(instr);
    m_lowererMD.LoadHelperArgument(instr, sizeOpnd);
    m_lowererMD.LoadHelperArgument(instr, src1);
    m_lowererMD.LoadHelperArgument(instr, indexOpnd);
    m_lowererMD.LoadHelperArgument(instr, baseOpnd);
    m_lowererMD.ChangeToHelperCall(instr, helperMethod);
    dst->Free(m_func);
}

void
Lowerer::LowerMemcopy(IR::Instr * instr, IR::RegOpnd * helperRet)
{
    IR::Opnd * dst = instr->UnlinkDst();
    IR::Opnd * src = instr->UnlinkSrc1();

    Assert(dst->IsIndirOpnd());
    Assert(src->IsIndirOpnd());

    IR::Opnd *dstBaseOpnd = dst->AsIndirOpnd()->UnlinkBaseOpnd();
    IR::Opnd *dstIndexOpnd = dst->AsIndirOpnd()->UnlinkIndexOpnd();

    IR::Opnd *srcBaseOpnd = src->AsIndirOpnd()->UnlinkBaseOpnd();
    IR::Opnd *srcIndexOpnd = src->AsIndirOpnd()->UnlinkIndexOpnd();

    IR::Opnd *sizeOpnd = instr->UnlinkSrc2();

    Assert(sizeOpnd);
    Assert(dstBaseOpnd);
    Assert(dstIndexOpnd);
    Assert(srcBaseOpnd);
    Assert(srcIndexOpnd);

    IR::JnHelperMethod helperMethod = IR::HelperOp_Memcopy;

    instr->SetDst(helperRet);
    LoadScriptContext(instr);
    m_lowererMD.LoadHelperArgument(instr, sizeOpnd);
    m_lowererMD.LoadHelperArgument(instr, srcIndexOpnd);
    m_lowererMD.LoadHelperArgument(instr, srcBaseOpnd);
    m_lowererMD.LoadHelperArgument(instr, dstIndexOpnd);
    m_lowererMD.LoadHelperArgument(instr, dstBaseOpnd);
    m_lowererMD.ChangeToHelperCall(instr, helperMethod);
    dst->Free(m_func);
    src->Free(m_func);
}

IR::Instr *
Lowerer::LowerMemOp(IR::Instr * instr)
{
    Assert(instr->m_opcode == Js::OpCode::Memset || instr->m_opcode == Js::OpCode::Memcopy);
    IR::Instr *instrPrev = instr->m_prev;

    IR::RegOpnd* helperRet = IR::RegOpnd::New(TyInt8, instr->m_func);
    const bool isHelper = false;
    AssertMsg(instr->HasBailOutInfo(), "Expected bailOut on MemOp instruction");
    if (instr->HasBailOutInfo())
    {
        IR::BailOutKind bailOutKind = instr->GetBailOutKind();
        if (bailOutKind & IR::BailOutOnInvalidatedArrayHeadSegment)
        {
            Assert(!(bailOutKind & IR::BailOutOnMissingValue));
            LowerBailOnInvalidatedArrayHeadSegment(instr, isHelper);
            bailOutKind ^= IR::BailOutOnInvalidatedArrayHeadSegment;
            Assert(!bailOutKind || instr->GetBailOutKind() == bailOutKind);
        }
        else if (bailOutKind & IR::BailOutOnMissingValue)
        {
            LowerBailOnCreatedMissingValue(instr, isHelper);
            bailOutKind ^= IR::BailOutOnMissingValue;
            Assert(!bailOutKind || instr->GetBailOutKind() == bailOutKind);
        }
        if (bailOutKind & IR::BailOutOnInvalidatedArrayLength)
        {
            LowerBailOnInvalidatedArrayLength(instr, isHelper);
            bailOutKind ^= IR::BailOutOnInvalidatedArrayLength;
            Assert(!bailOutKind || instr->GetBailOutKind() == bailOutKind);
        }

        AssertMsg(bailOutKind & IR::BailOutOnMemOpError, "Expected BailOutOnMemOpError on MemOp instruction");
        if (bailOutKind & IR::BailOutOnMemOpError)
        {
            // Insert or get continue label
            IR::LabelInstr *const skipBailOutLabel = instr->GetOrCreateContinueLabel(isHelper);
            Func *const func = instr->m_func;
            LowerOneBailOutKind(instr, IR::BailOutOnMemOpError, isHelper);
            IR::Instr *const insertBeforeInstr = instr->m_next;

            //     test helperRet, helperRet
            //     jz $skipBailOut
            InsertCompareBranch(
                helperRet,
                IR::IntConstOpnd::New(0, TyInt8, func),
                Js::OpCode::BrNeq_A,
                skipBailOutLabel,
                insertBeforeInstr);

            //     (Bail out with IR::BailOutOnMemOpError)
            //     $skipBailOut:

            bailOutKind ^= IR::BailOutOnMemOpError;
            Assert(!bailOutKind || instr->GetBailOutKind() == bailOutKind);
        }

        instr->ClearBailOutInfo();
    }

    if (instr->m_opcode == Js::OpCode::Memset)
    {
        LowerMemset(instr, helperRet);
    }
    else if (instr->m_opcode == Js::OpCode::Memcopy)
    {
        LowerMemcopy(instr, helperRet);
    }
    return instrPrev;
}

IR::Instr *
Lowerer::LowerStArrViewElem(IR::Instr * instr)
{
    Assert(m_func->GetJnFunction()->GetIsAsmjsMode());
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::StInt8ArrViewElem    ||
           instr->m_opcode == Js::OpCode::StUInt8ArrViewElem   ||
           instr->m_opcode == Js::OpCode::StInt16ArrViewElem   ||
           instr->m_opcode == Js::OpCode::StUInt16ArrViewElem  ||
           instr->m_opcode == Js::OpCode::StInt32ArrViewElem   ||
           instr->m_opcode == Js::OpCode::StUInt32ArrViewElem  ||
           instr->m_opcode == Js::OpCode::StFloat32ArrViewElem ||
           instr->m_opcode == Js::OpCode::StFloat64ArrViewElem);

    IR::Instr * instrPrev = instr->m_prev;

    IR::Opnd * dst = instr->GetDst();
    IR::Opnd * src1 = instr->GetSrc1();
    IR::Opnd * src2 = instr->GetSrc2();

    // type of dst is the type of array
    IR::RegOpnd * indexOpnd = dst->AsIndirOpnd()->GetIndexOpnd();

    Assert(!dst->IsFloat32() || src1->IsFloat32());
    Assert(!dst->IsFloat64() || src1->IsFloat64());

    IR::Instr * done;
    if (indexOpnd || (uint32)dst->AsIndirOpnd()->GetOffset() >= 0x1000000)
    {
        // CMP indexOpnd, src2(arrSize)
        // JA $helper
        // JMP $store
        // $helper:
        // JMP $done
        // $store:
        // MOV dst([arrayBuffer + indexOpnd]), src1
        // $done:

        done = m_lowererMD.LowerAsmJsStElemHelper(instr);
    }
    else
    {
        // any constant access below 0x1000000 is safe, as that is the min heap size
        instr->UnlinkDst();
        instr->UnlinkSrc1();
        done = instr;
        if (src2)
        {
            instr->FreeSrc2();
        }
    }
    InsertMove(dst, src1, done);
    instr->Remove();
    return instrPrev;
}

IR::Instr *
Lowerer::LowerArrayDetachedCheck(IR::Instr * instr)
{
    // TEST isDetached, isDetached
    // JE Done
    // Helper:
    // CALL Js::Throw::OutOfMemory
    // Done:

    Assert(m_func->GetJnFunction()->GetIsAsmjsMode());
    IR::Instr * instrPrev = instr->m_prev;

    IR::Opnd * isDetachedOpnd = instr->UnlinkSrc1();
    Assert(isDetachedOpnd->IsIndirOpnd() || isDetachedOpnd->IsMemRefOpnd());

    IR::LabelInstr * doneLabel = InsertLabel(false, instr->m_next);
    IR::LabelInstr * helperLabel = InsertLabel(true, instr);

    InsertTestBranch(isDetachedOpnd, isDetachedOpnd, Js::OpCode::BrNotNeq_A, doneLabel, helperLabel);

    m_lowererMD.ChangeToHelperCall(instr, IR::HelperOp_OutOfMemoryError);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerDeleteElemI
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerDeleteElemI(IR::Instr * instr, bool strictMode)
{
    IR::Instr *instrPrev;

    IR::Opnd *src1 = instr->UnlinkSrc1();

    AssertMsg(src1->IsIndirOpnd(), "Expected indirOpnd on DeleteElementI");

    Js::PropertyOperationFlags propertyOperationFlag = Js::PropertyOperation_None;

    if (strictMode)
    {
        propertyOperationFlag = Js::PropertyOperation_StrictMode;
    }

    instrPrev = instr->m_prev;
    IR::JnHelperMethod helperMethod = IR::HelperOp_DeleteElementI;
    IR::Opnd *indexOpnd = src1->AsIndirOpnd()->UnlinkIndexOpnd();
    if (indexOpnd)
    {
        if (indexOpnd->GetType() == TyInt32)
        {
            helperMethod = IR::HelperOp_DeleteElementI_Int32;
        }
        else if (indexOpnd->GetType() == TyUint32)
        {
            helperMethod = IR::HelperOp_DeleteElementI_UInt32;
        }
        else
        {
            Assert(indexOpnd->GetType() == TyVar);
        }
    }
    else
    {
        // No index; the offset identifies the element.
        IntConstType offset = (IntConstType)src1->AsIndirOpnd()->GetOffset();
        indexOpnd = IR::AddrOpnd::NewFromNumber(offset, m_func);
    }

    m_lowererMD.LoadHelperArgument(instr, IR::IntConstOpnd::New((IntConstType)propertyOperationFlag, TyInt32, m_func, true));
    LoadScriptContext(instr);
    m_lowererMD.LoadHelperArgument(instr, indexOpnd);

    IR::Opnd *baseOpnd = src1->AsIndirOpnd()->UnlinkBaseOpnd();
    m_lowererMD.LoadHelperArgument(instr, baseOpnd);

    src1->Free(this->m_func);

    m_lowererMD.ChangeToHelperCall(instr, helperMethod);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerBrB - lower 1-operand (boolean) conditional branch
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerBrBReturn(IR::Instr * instr, IR::JnHelperMethod helperMethod, bool isHelper)
{
    IR::Instr * instrPrev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd  * opndHelper;
    IR::Opnd  * opndSrc;
    IR::Opnd  * opndDst;

    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr, "Expected 1 src opnds on BrB");
    Assert(instr->m_opcode == Js::OpCode::BrOnEmpty || instr->m_opcode == Js::OpCode::BrOnNotEmpty);
    opndSrc = instr->UnlinkSrc1();
    instrPrev = m_lowererMD.LoadHelperArgument(instr, opndSrc);

    // Generate helper call to convert the unknown operand to boolean

    opndHelper = IR::HelperCallOpnd::New(helperMethod, this->m_func);

    opndDst = instr->UnlinkDst();
    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    // Branch on the result of the call
    instr->m_opcode = (instr->m_opcode == Js::OpCode::BrOnNotEmpty? Js::OpCode::BrTrue_A : Js::OpCode::BrFalse_A);
    instr->SetSrc1(opndDst);
    IR::Instr *loweredInstr;
    loweredInstr = this->LowerCondBranchCheckBailOut(instr->AsBranchInstr(), instrCall, isHelper);

#if DBG
    if (isHelper)
    {
        if (!loweredInstr->IsBranchInstr())
        {
            loweredInstr = loweredInstr->GetNextBranchOrLabel();
        }
        if (loweredInstr->IsBranchInstr())
        {
            loweredInstr->AsBranchInstr()->m_isHelperToNonHelperBranch = true;
        }
    }
#endif
    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerMultiBr
///     - Lowers the instruction for dictionary look up(string case arms)
///
///----------------------------------------------------------------------------
IR::Instr* Lowerer::LowerMultiBr(IR::Instr * instr, IR::JnHelperMethod helperMethod)
{
    IR::Instr * instrPrev = instr->m_prev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd  * opndHelper;
    IR::Opnd  * opndSrc;
    IR::Opnd  * opndDst;
    StackSym * symDst;

    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr, "Expected 1 src opnd on BrB");

    // Push the args in reverse order.

    // The end and start labels for the function are used to guarantee
    // that the dictionary jump destinations haven't been tampered with, so we
    // will always jump to some location within this function
    IR::LabelOpnd * endFuncOpnd = IR::LabelOpnd::New(m_func->EnsureFuncEndLabel(), m_func);
    m_lowererMD.LoadHelperArgument(instr, endFuncOpnd);

    IR::LabelOpnd * startFuncOpnd = IR::LabelOpnd::New(m_func->EnsureFuncStartLabel(), m_func);
    m_lowererMD.LoadHelperArgument(instr, startFuncOpnd);

    //Load the address of the dictionary pair- Js::StringDictionaryWrapper
    IR::AddrOpnd* nativestringDictionaryOpnd = IR::AddrOpnd::New(instr->AsBranchInstr()->AsMultiBrInstr()->GetBranchDictionary(), IR::AddrOpndKindDynamicMisc, this->m_func);
    m_lowererMD.LoadHelperArgument(instr, nativestringDictionaryOpnd);

    //Load the String passed in the Switch expression for look up - JavascriptString
    opndSrc = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, opndSrc);

    // Generate helper call for dictionary lookup.

    opndHelper = IR::HelperCallOpnd::New(helperMethod, this->m_func);

    symDst = StackSym::New(TyMachPtr,this->m_func);
    opndDst = IR::RegOpnd::New(symDst, TyMachPtr, this->m_func);

    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    instr->SetSrc1(instrCall->GetDst());
    m_lowererMD.LowerMultiBranch(instr);

    return instrPrev;
}

void
Lowerer::LowerJumpTableMultiBranch(IR::MultiBranchInstr * multiBrInstr, IR::RegOpnd * indexOpnd)
{
    Func * func = this->m_func;
    IR::Opnd * opndDst = IR::RegOpnd::New(TyMachPtr, func);
    //Move the native address of the jump table to a register
    IR::LabelInstr * nativeJumpTableLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    nativeJumpTableLabel->m_isDataLabel = true;
    IR::LabelOpnd * nativeJumpTable = IR::LabelOpnd::New(nativeJumpTableLabel, m_func);
    IR::RegOpnd * nativeJumpTableReg = IR::RegOpnd::New(TyMachPtr, func);
    m_lowererMD.CreateAssign(nativeJumpTableReg, nativeJumpTable, multiBrInstr);

    BranchJumpTableWrapper * branchJumpTable = multiBrInstr->GetBranchJumpTable();
    AssertMsg(branchJumpTable->labelInstr == nullptr, "Should not be already assigned");
    branchJumpTable->labelInstr = nativeJumpTableLabel;

    //Indirect addressing @ target location in the jump table.
    //MOV eax, [nativeJumpTableReg + (offset * indirScale)]
    BYTE indirScale = this->m_lowererMD.GetDefaultIndirScale();
    IR::Opnd * opndSrc = IR::IndirOpnd::New(nativeJumpTableReg, indexOpnd, indirScale, TyMachReg, this->m_func);

    IR::Instr * indirInstr = m_lowererMD.CreateAssign(opndDst, opndSrc, multiBrInstr);

    //MultiBr eax
    multiBrInstr->SetSrc1(indirInstr->GetDst());

    //Jump to the address at the target location in the jump table
    m_lowererMD.LowerMultiBranch(multiBrInstr);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerMultiBr
///     - Lowers the instruction for jump table(consecutive integer case arms)
///
///----------------------------------------------------------------------------
IR::Instr* Lowerer::LowerMultiBr(IR::Instr * instr)
{
    IR::Instr * instrPrev = instr->m_prev;

    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr, "Expected 1 src opnd on BrB");
    AssertMsg(instr->IsBranchInstr() && instr->AsBranchInstr()->IsMultiBranch(), "Bad Instruction Lowering Call to LowerMultiBr()");

    IR::MultiBranchInstr * multiBrInstr = instr->AsBranchInstr()->AsMultiBrInstr();
    IR::RegOpnd * offset = instr->UnlinkSrc1()->AsRegOpnd();
    LowerJumpTableMultiBranch(multiBrInstr, offset);

    return instrPrev;
}

IR::Instr* Lowerer::LowerBrBMem(IR::Instr * instr, IR::JnHelperMethod helperMethod)
{
    IR::Instr * instrPrev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd  * opndHelper;
    IR::Opnd  * opndSrc;
    IR::Opnd  * opndDst;
    StackSym * symDst;
    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr, "Expected 1 src opnds on BrB");

    instrPrev = LoadScriptContext(instr);
    opndSrc = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, opndSrc);

    // Generate helper call to convert the unknown operand to boolean

    opndHelper = IR::HelperCallOpnd::New(helperMethod, this->m_func);
    symDst = StackSym::New(TyVar, this->m_func);
    opndDst = IR::RegOpnd::New(symDst, TyVar, this->m_func);
    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    // Branch on the result of the call

    instr->SetSrc1(opndDst);
    m_lowererMD.LowerCondBranch(instr);

    return instrPrev;
}

IR::Instr* Lowerer::LowerBrOnObject(IR::Instr * instr, IR::JnHelperMethod helperMethod)
{
    IR::Instr * instrPrev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd  * opndHelper;
    IR::Opnd  * opndSrc;
    IR::Opnd  * opndDst;
    StackSym * symDst;
    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr, "Expected 1 src opnds on BrB");

    opndSrc = instr->UnlinkSrc1();
    instrPrev = m_lowererMD.LoadHelperArgument(instr, opndSrc);

    // Generate helper call to check if the operand's type is object

    opndHelper = IR::HelperCallOpnd::New(helperMethod, this->m_func);
    symDst = StackSym::New(TyVar, this->m_func);
    opndDst = IR::RegOpnd::New(symDst, TyVar, this->m_func);
    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    // Branch on the result of the call

    instr->SetSrc1(opndDst);
    m_lowererMD.LowerCondBranch(instr);

    return instrPrev;
}

IR::Instr * Lowerer::LowerBrOnClassConstructor(IR::Instr * instr, IR::JnHelperMethod helperMethod)
{
    IR::Instr * instrPrev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd * opndHelper;
    IR::Opnd * opndSrc;
    IR::Opnd * opndDst;
    StackSym * symDst;
    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr, "Expected 1 src opnds on BrB");

    opndSrc = instr->UnlinkSrc1();
    instrPrev = m_lowererMD.LoadHelperArgument(instr, opndSrc);

    // Generate helper call to check if the operand's type is object

    opndHelper = IR::HelperCallOpnd::New(helperMethod, this->m_func);
    symDst = StackSym::New(TyVar, this->m_func);
    opndDst = IR::RegOpnd::New(symDst, TyVar, this->m_func);
    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    // Branch on the result of the call

    instr->SetSrc1(opndDst);
    m_lowererMD.LowerCondBranch(instr);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerBrCMem(IR::Instr * instr, IR::JnHelperMethod helperMethod, bool noMathFastPath, bool isHelper)
{
    IR::Instr * instrPrev = instr->m_prev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd  * opndHelper;
    IR::Opnd  * opndSrc;
    IR::Opnd  * opndDst;
    StackSym * symDst;
    bool inverted = false;

    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() != nullptr, "Expected 2 src opnds on BrC");

    if (!noMathFastPath && !this->GenerateFastCondBranch(instr->AsBranchInstr(), &isHelper))
    {
        return instrPrev;
    }

    // Push the args in reverse order.
    const bool loadScriptContext = !(helperMethod == IR::HelperOp_StrictEqualString || helperMethod == IR::HelperOp_StrictEqualEmptyString);
    const bool loadArg2 = !(helperMethod == IR::HelperOp_StrictEqualEmptyString);

    if (helperMethod == IR::HelperOp_NotEqual)
    {
        // Op_NotEqual() returns !Op_Equal().  It is faster to call Op_Equal() directly.
        helperMethod = IR::HelperOp_Equal;
        instr->AsBranchInstr()->Invert();
        inverted = true;
    }
    else if(helperMethod == IR::HelperOp_NotStrictEqual)
    {
        // Op_NotStrictEqual() returns !Op_StrictEqual().  It is faster to call Op_StrictEqual() directly.
        helperMethod = IR::HelperOp_StrictEqual;
        instr->AsBranchInstr()->Invert();
        inverted = true;
    }

    if (loadScriptContext)
        LoadScriptContext(instr);

    opndSrc = instr->UnlinkSrc2();
    if (loadArg2)
        m_lowererMD.LoadHelperArgument(instr, opndSrc);

    opndSrc = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, opndSrc);

    // Generate helper call to compare the source operands.

    opndHelper = IR::HelperCallOpnd::New(helperMethod, this->m_func);
    symDst = StackSym::New(TyMachReg, this->m_func);
    opndDst = IR::RegOpnd::New(symDst, TyMachReg, this->m_func);
    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    switch (instr->m_opcode)
    {
    case Js::OpCode::BrNotEq_A:
    case Js::OpCode::BrNotNeq_A:
    case Js::OpCode::BrSrNotEq_A:
    case Js::OpCode::BrSrNotNeq_A:
        if (instr->HasBailOutInfo())
        {
            instr->GetBailOutInfo()->isInvertedBranch = true;
        }
        break;

    case Js::OpCode::BrNotGe_A:
    case Js::OpCode::BrNotGt_A:
    case Js::OpCode::BrNotLe_A:
    case Js::OpCode::BrNotLt_A:
        inverted = true;
        break;
    }

    // Branch if the result is "true".

    instr->SetSrc1(opndDst);
    instr->m_opcode = (inverted ? Js::OpCode::BrFalse_A : Js::OpCode::BrTrue_A);
    this->LowerCondBranchCheckBailOut(instr->AsBranchInstr(), instrCall, !noMathFastPath && isHelper);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerBrFncApply(IR::Instr * instr, IR::JnHelperMethod helperMethod) {
    IR::Instr * instrPrev = instr->m_prev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd  * opndHelper;
    IR::Opnd  * opndSrc;
    IR::Opnd  * opndDst;
    StackSym * symDst;

    AssertMsg(instr->GetSrc1() != nullptr, "Expected 1 src opnd on BrFncApply");

    LoadScriptContext(instr);

    opndSrc = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, opndSrc);

    // Generate helper call to compare the source operands.

    opndHelper = IR::HelperCallOpnd::New(helperMethod, this->m_func);
    symDst = StackSym::New(TyMachReg, this->m_func);
    opndDst = IR::RegOpnd::New(symDst, TyMachReg, this->m_func);
    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    // Branch if the result is "true".

    instr->SetSrc1(opndDst);
    instr->m_opcode = Js::OpCode::BrTrue_A;
    m_lowererMD.LowerCondBranch(instr);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerBrProperty - lower branch-on-has/no-property
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerBrProperty(IR::Instr * instr, IR::JnHelperMethod helper)
{
    IR::Instr * instrPrev;
    IR::Instr * instrCall;
    IR::HelperCallOpnd * opndHelper;
    IR::Opnd *  opndSrc;
    IR::Opnd *  opndDst;

    opndSrc = instr->UnlinkSrc1();
    AssertMsg(opndSrc->IsSymOpnd() && opndSrc->AsSymOpnd()->m_sym->IsPropertySym(),
              "Expected propertySym as src of BrProperty");

    instrPrev = LoadScriptContext(instr);
    this->LoadPropertySymAsArgument(instr, opndSrc);

    opndHelper = IR::HelperCallOpnd::New(helper, this->m_func);
    opndDst = IR::RegOpnd::New(StackSym::New(TyMachReg, this->m_func), TyMachReg, this->m_func);
    instrCall = IR::Instr::New(Js::OpCode::Call, opndDst, opndHelper, this->m_func);

    instr->InsertBefore(instrCall);
    instrCall = m_lowererMD.LowerCall(instrCall, 0);

    // Branch on the result of the call

    instr->SetSrc1(opndDst);
    switch (instr->m_opcode)
    {
    case Js::OpCode::BrOnHasProperty:
        instr->m_opcode = Js::OpCode::BrTrue_A;
        break;
    case Js::OpCode::BrOnNoProperty:
        instr->m_opcode = Js::OpCode::BrFalse_A;
        break;
    default:
        AssertMsg(0, "Unknown opcode on BrProperty branch");
        break;
    }
    this->LowerCondBranchCheckBailOut(instr->AsBranchInstr(), instrCall, false);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerElementUndefined
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerElementUndefined(IR::Instr * instr, IR::JnHelperMethod helper)
{
    IR::Opnd *dst = instr->UnlinkDst();
    AssertMsg(dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsPropertySym(), "Expected fieldSym as dst of Ld Undefined");

    // Pass the property sym to store to
    this->LoadPropertySymAsArgument(instr, dst);
    m_lowererMD.ChangeToHelperCall(instr, helper);

    return instr;
}

IR::Instr *
Lowerer::LowerElementUndefinedMem(IR::Instr * instr, IR::JnHelperMethod helper)
{
    // Pass script context
    IR::Instr * instrPrev = LoadScriptContext(instr);

    this->LowerElementUndefined(instr, helper);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerLdElemUndef(IR::Instr * instr)
{
    if (this->m_func->GetJnFunction()->IsEval())
    {
        return LowerElementUndefinedMem(instr, IR::HelperOp_LdElemUndefDynamic);
    }
    else
    {
        return LowerElementUndefined(instr, IR::HelperOp_LdElemUndef);
    }
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerElementUndefinedScoped
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerElementUndefinedScoped(IR::Instr * instr, IR::JnHelperMethod helper)
{
    IR::Instr * instrPrev = instr->m_prev;

    // Pass the default instance
    IR::Opnd *src = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, src);

    // Pass the property sym to store to
    IR::Opnd * dst = instr->UnlinkDst();
    AssertMsg(dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsPropertySym(), "Expected fieldSym as dst of Ld Undefined Scoped");

    this->LoadPropertySymAsArgument(instr, dst);

    m_lowererMD.ChangeToHelperCall(instr, helper);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerElementUndefinedScopedMem(IR::Instr * instr, IR::JnHelperMethod helper)
{
    // Pass script context
    IR::Instr * instrPrev = LoadScriptContext(instr);

    this->LowerElementUndefinedScoped(instr, helper);

    return instrPrev;
}

void
Lowerer::LowerStLoopBodyCount(IR::Instr* instr)
{
    Js::LoopHeader *header = ((JsLoopBodyCodeGen*)m_func->m_workItem)->loopHeader;

    IR::MemRefOpnd *loopBodyCounterOpnd = IR::MemRefOpnd::New((BYTE*)(header) + header->GetOffsetOfProfiledLoopCounter(), TyUint32, this->m_func);
    instr->SetDst(loopBodyCounterOpnd);
    instr->ReplaceSrc1(instr->GetSrc1()->AsRegOpnd()->UseWithNewType(TyUint32, this->m_func));
    IR::AutoReuseOpnd(loopBodyCounterOpnd, this->m_func);
    m_lowererMD.ChangeToAssign(instr);
    return;
}

#if !FLOATVAR
IR::Instr *
Lowerer::LowerStSlotBoxTemp(IR::Instr *stSlot)
{
    // regVar = BoxStackNumber(src, scriptContext)
    IR::RegOpnd * regSrc = stSlot->UnlinkSrc1()->AsRegOpnd();
    IR::Instr * instr = IR::Instr::New(Js::OpCode::Call, this->m_func);
    IR::RegOpnd *regVar = IR::RegOpnd::New(TyVar, this->m_func);
    instr->SetDst(regVar);
    instr->SetSrc1(regSrc);
    stSlot->InsertBefore(instr);
    this->LowerUnaryHelperMem(instr, IR::HelperBoxStackNumber);
    stSlot->SetSrc1(regVar);
    return this->LowerStSlot(stSlot);
}
#endif

IR::Opnd *
Lowerer::CreateOpndForSlotAccess(IR::Opnd * opnd)
{
    IR::SymOpnd * symOpnd = opnd->AsSymOpnd();
    PropertySym * dstSym = symOpnd->m_sym->AsPropertySym();

    if (!m_func->IsLoopBody() &&
        m_func->DoStackFrameDisplay() &&
        (dstSym->m_stackSym == m_func->GetLocalClosureSym() || dstSym->m_stackSym == m_func->GetLocalFrameDisplaySym()))
    {
        // Stack closure syms are made to look like slot accesses for the benefit of GlobOpt, so that it can do proper
        // copy prop and implicit call bailout. But what we really want is local stack load/store.
        // Don't do this for loop body, though, since we don't have the value saved on the stack.
        return IR::SymOpnd::New(dstSym->m_stackSym, 0, TyMachReg, this->m_func);
    }

    int32 offset = dstSym->m_propertyId;
    if (!m_func->GetJnFunction()->GetIsAsmJsFunction())
    {
        offset = offset * TySize[opnd->GetType()];
    }
    if (m_func->IsTJLoopBody())
    {
        offset = offset - m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetTotalSizeinBytes();
    }

    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(symOpnd->CreatePropertyOwnerOpnd(m_func),
       offset , opnd->GetType(), this->m_func);
    return indirOpnd;
}

IR::Instr *
Lowerer::LowerStSlot(IR::Instr *instr)
{
    // StSlot stores the nth Var in the buffer pointed to by the property sym's stack sym.

    IR::Opnd * dstOpnd = instr->UnlinkDst();
    AssertMsg(dstOpnd, "Expected dst opnd on StSlot");
    IR::Opnd * dstNew = this->CreateOpndForSlotAccess(dstOpnd);
    dstOpnd->Free(this->m_func);

    instr->SetDst(dstNew);
    m_lowererMD.ChangeToWriteBarrierAssign(instr);

    return instr;
}

IR::Instr *
Lowerer::LowerStSlotChkUndecl(IR::Instr *instrStSlot)
{
    Assert(instrStSlot->GetSrc2() != nullptr);

    // Src2 is required only to avoid dead store false positives during GlobOpt.
    instrStSlot->FreeSrc2();

    IR::Opnd *dstOpnd = this->CreateOpndForSlotAccess(instrStSlot->GetDst());
    IR::Instr *instr = this->LowerStSlot(instrStSlot);

    this->GenUndeclChk(instr, dstOpnd);

    return instr;
}

void Lowerer::LowerProfileLdSlot(IR::Opnd *const valueOpnd, Func *const ldSlotFunc, const Js::ProfileId profileId, IR::Instr *const insertBeforeInstr)
{
    Assert(valueOpnd);
    Assert(profileId != Js::Constants::NoProfileId);
    Assert(insertBeforeInstr);

    Func *const irFunc = insertBeforeInstr->m_func;

    m_lowererMD.LoadHelperArgument(insertBeforeInstr, IR::Opnd::CreateProfileIdOpnd(profileId, irFunc));
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, CreateFunctionBodyOpnd(ldSlotFunc));
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, valueOpnd);

    IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, irFunc);
    callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperProfileLdSlot, irFunc));
    insertBeforeInstr->InsertBefore(callInstr);
    m_lowererMD.LowerCall(callInstr, 0);
}

IR::Instr *
Lowerer::LowerLdSlot(IR::Instr *instr)
{
    IR::Opnd * srcOpnd = instr->UnlinkSrc1();
    AssertMsg(srcOpnd, "Expected src opnd on LdSlot");
    IR::Opnd * srcNew = this->CreateOpndForSlotAccess(srcOpnd);
    srcOpnd->Free(this->m_func);

    instr->SetSrc1(srcNew);
    m_lowererMD.ChangeToAssign(instr);
    return instr;
}

IR::Instr *
Lowerer::LowerChkUndecl(IR::Instr *instr)
{
    IR::Instr *instrPrev = instr->m_prev;
    this->GenUndeclChk(instr, instr->GetSrc1());
    instr->Remove();
    return instrPrev;
}

void
Lowerer::GenUndeclChk(IR::Instr *instrInsert, IR::Opnd *opnd)
{
    IR::LabelInstr *labelContinue = IR::LabelInstr::New(Js::OpCode::Label, m_func);

    InsertCompareBranch(
        opnd,
        LoadLibraryValueOpnd(instrInsert, LibraryValue::ValueUndeclBlockVar),
        Js::OpCode::BrNeq_A, labelContinue, instrInsert);

    IR::LabelInstr *labelThrow = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    instrInsert->InsertBefore(labelThrow);

    IR::Instr *instr = IR::Instr::New(
        Js::OpCode::RuntimeReferenceError,
        IR::RegOpnd::New(TyMachReg, m_func),
        IR::IntConstOpnd::New(SCODE_CODE(JSERR_UseBeforeDeclaration), TyInt32, m_func),
        m_func);
    instrInsert->InsertBefore(instr);
    this->LowerUnaryHelperMem(instr, IR::HelperOp_RuntimeReferenceError);

    instrInsert->InsertBefore(labelContinue);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerStElemC
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerStElemC(IR::Instr * stElem)
{
    IR::Instr *instrPrev = stElem->m_prev;
    IR::IndirOpnd * indirOpnd = stElem->GetDst()->AsIndirOpnd();
    IR::RegOpnd *indexOpnd = indirOpnd->UnlinkIndexOpnd();

    Assert(!indexOpnd || indexOpnd->m_sym->IsIntConst());

    IntConstType value;
    if (indexOpnd)
    {
        value = indexOpnd->AsRegOpnd()->m_sym->GetIntConstValue();
        indexOpnd->Free(this->m_func);
    }
    else
    {
        value = (IntConstType)indirOpnd->GetOffset();
    }

    if (stElem->IsJitProfilingInstr())
    {
        Assert(stElem->AsJitProfilingInstr()->profileId == Js::Constants::NoProfileId);
        m_lowererMD.LoadHelperArgument(stElem, stElem->UnlinkSrc1());

        const auto meth = stElem->m_opcode == Js::OpCode::StElemC ? IR::HelperSimpleStoreArrayHelper : IR::HelperSimpleStoreArraySegHelper;

        stElem->SetSrc1(IR::HelperCallOpnd::New(meth, m_func));

        m_lowererMD.LoadHelperArgument(stElem, IR::IntConstOpnd::New(value, TyUint32, m_func));
        m_lowererMD.LoadHelperArgument(stElem, indirOpnd->UnlinkBaseOpnd());

        stElem->UnlinkDst()->Free(m_func);

        m_lowererMD.LowerCall(stElem, 0);
        return instrPrev;
    }


    IntConstType base;
    IR::RegOpnd *baseOpnd = indirOpnd->GetBaseOpnd();
    const ValueType baseValueType(baseOpnd->GetValueType());
    if(baseValueType.IsLikelyNativeArray())
    {
        Assert(stElem->m_opcode == Js::OpCode::StElemC);

        IR::LabelInstr *labelBailOut = nullptr;
        IR::Instr *instrBailOut = nullptr;

        if (stElem->HasBailOutInfo())
        {
            labelBailOut = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
            instrBailOut = stElem;
            stElem = IR::Instr::New(instrBailOut->m_opcode, m_func);
            instrBailOut->TransferTo(stElem);
            instrBailOut->InsertBefore(stElem);

            IR::LabelInstr *labelDone = IR::LabelInstr::New(Js::OpCode::Label, m_func);
            InsertBranch(Js::OpCode::Br, labelDone, instrBailOut);

            instrBailOut->InsertBefore(labelBailOut);
            instrBailOut->InsertAfter(labelDone);

            instrBailOut->m_opcode = Js::OpCode::BailOut;
            GenerateBailOut(instrBailOut);
        }

        if (!baseValueType.IsObject())
        {
            // Likely native array: do a vtable check and bail if it fails.
            Assert(labelBailOut);
            GenerateArrayTest(baseOpnd, labelBailOut, labelBailOut, stElem, true);
        }

        if (stElem->GetSrc1()->GetType() == TyVar)
        {
            // Storing a non-specialized value. This may cause array conversion, which invalidates all the code
            // that depends on the array check we've already done.
            // Call a helper that returns the type ID of the resulting array, check it here against the one we
            // expect, and bail if it fails.

            Assert(labelBailOut);

            // Call a helper to (try and) unbox the var and store it.
            // If we had to convert the array to do the store, we'll bail.
            LoadScriptContext(stElem);

            m_lowererMD.LoadHelperArgument(stElem, stElem->UnlinkSrc1());

            IR::Opnd *indexOpnd = IR::IntConstOpnd::New(value, TyUint32, m_func);

            m_lowererMD.LoadHelperArgument(stElem, indexOpnd);
            m_lowererMD.LoadHelperArgument(stElem, indirOpnd->UnlinkBaseOpnd());

            IR::JnHelperMethod helperMethod;
            if (baseValueType.HasIntElements())
            {
                helperMethod = IR::HelperScrArr_SetNativeIntElementC;
            }
            else
            {
                helperMethod = IR::HelperScrArr_SetNativeFloatElementC;
            }

            IR::Instr *instrInsertBranch = stElem->m_next;
            IR::RegOpnd *typeIdOpnd = IR::RegOpnd::New(TyUint32, m_func);
            stElem->ReplaceDst(typeIdOpnd);
            m_lowererMD.ChangeToHelperCall(stElem, helperMethod);

            InsertCompareBranch(
                typeIdOpnd,
                IR::IntConstOpnd::New(
                    baseValueType.HasIntElements() ?
                    Js::TypeIds_NativeIntArray : Js::TypeIds_NativeFloatArray, TyUint32, m_func),
                Js::OpCode::BrNeq_A,
                labelBailOut,
                instrInsertBranch);

            return instrPrev;
        }
        else if (baseValueType.HasIntElements() && labelBailOut)
        {
            Assert(stElem->GetSrc1()->GetType() == GetArrayIndirType(baseValueType));
            IR::Opnd* missingElementOpnd = GetMissingItemOpnd(stElem->GetSrc1()->GetType(), m_func);
            if (!stElem->GetSrc1()->IsEqual(missingElementOpnd))
            {
                InsertCompareBranch(stElem->GetSrc1(), missingElementOpnd , Js::OpCode::BrEq_A, labelBailOut, stElem, true);
            }
            else
            {
                //Its a missing value store and data flow proves that src1 is always missing value. Array cannot be a int array at the first place
                //if this code was ever hit. Just bailout, this code path would be updated with the profile information next time around.
                InsertBranch(Js::OpCode::Br, labelBailOut, stElem);
#if DBG
                labelBailOut->m_noHelperAssert = true;
#endif
                stElem->Remove();
                return instrPrev;
            }
        }
        else
        {
            Assert(stElem->GetSrc1()->GetType() == GetArrayIndirType(baseValueType));
        }
        stElem->GetDst()->SetType(stElem->GetSrc1()->GetType());
        Assert(value <= Js::SparseArraySegmentBase::INLINE_CHUNK_SIZE);

        if(baseValueType.HasIntElements())
        {
            base = sizeof(Js::JavascriptNativeIntArray) + offsetof(Js::SparseArraySegment<int32>, elements);
        }
        else
        {
            base = sizeof(Js::JavascriptNativeFloatArray) + offsetof(Js::SparseArraySegment<double>, elements);
        }
    }
    else if(baseValueType.IsLikelyObject() && baseValueType.GetObjectType() == ObjectType::Array)
    {
        Assert(stElem->m_opcode == Js::OpCode::StElemC);
        Assert(value <= Js::SparseArraySegmentBase::INLINE_CHUNK_SIZE);
        base = sizeof(Js::JavascriptArray) + offsetof(Js::SparseArraySegment<Js::Var>, elements);
    }
    else
    {
        Assert(stElem->m_opcode == Js::OpCode::StElemC || stElem->m_opcode == Js::OpCode::StArrSegElemC);
        Assert(indirOpnd->GetBaseOpnd()->GetType() == TyVar);
        base = offsetof(Js::SparseArraySegment<Js::Var>, elements);
    }
    Assert(value >= 0);

    //  MOV [r3 + offset(element) + index], src
    const BYTE indirScale =
        baseValueType.IsLikelyAnyOptimizedArray() ? GetArrayIndirScale(baseValueType) : m_lowererMD.GetDefaultIndirScale();
    IntConstType offset = base + (value << indirScale);
    Assert(Math::FitsInDWord(offset));
    indirOpnd->SetOffset((int32)offset);
    m_lowererMD.ChangeToWriteBarrierAssign(stElem);

    return instrPrev;
}

void Lowerer::LowerLdArrHead(IR::Instr *const instr)
{
    IR::RegOpnd *array = instr->UnlinkSrc1()->AsRegOpnd();
    const ValueType arrayValueType(array->GetValueType());
    Assert(arrayValueType.IsAnyOptimizedArray());

    if(arrayValueType.GetObjectType() == ObjectType::ObjectWithArray)
    {
        array = LoadObjectArray(array, instr);
    }

    // mov arrayHeadSegment, [array + offset(headSegment)]
    instr->GetDst()->SetType(TyMachPtr);
    instr->SetSrc1(
        IR::IndirOpnd::New(
            array,
            GetArrayOffsetOfHeadSegment(arrayValueType),
            TyMachPtr,
            instr->m_func));
    LowererMD::ChangeToAssign(instr);
}

// Creates the rest parameter array.
// Var JavascriptArray::OP_NewScArrayWithElements(
//        uint32 elementCount,
//        Var *elements,
//        ScriptContext* scriptContext)
IR::Instr *Lowerer::LowerRestParameter(IR::Opnd *formalsOpnd, IR::Opnd *dstOpnd, IR::Opnd *excessOpnd, IR::Instr *instr, IR::RegOpnd *generatorArgsPtrOpnd)
{
    IR::Instr * helperCallInstr = IR::Instr::New(LowererMD::MDCallOpcode, dstOpnd, instr->m_func);
    instr->InsertAfter(helperCallInstr);

    // Var JavascriptArray::OP_NewScArrayWithElements(
    //        int32 elementCount,
    //        Var *elements,
    //        ScriptContext* scriptContext)
    IR::JnHelperMethod helperMethod = IR::HelperScrArr_OP_NewScArrayWithElements;

    LoadScriptContext(helperCallInstr);

    BOOL isGenerator = this->m_func->GetJnFunction()->IsGenerator();

    // Elements pointer = ebp + (formals count + formals offset + 1)*sizeof(Var)
    IR::RegOpnd *srcOpnd = isGenerator ? generatorArgsPtrOpnd : IR::Opnd::CreateFramePointerOpnd(this->m_func);
    uint16 actualOffset = isGenerator ? 0 : GetFormalParamOffset(); //4
    IR::RegOpnd *argPtrOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    InsertAdd(false, argPtrOpnd, srcOpnd, IR::IntConstOpnd::New((formalsOpnd->AsIntConstOpnd()->GetValue() + actualOffset) * MachPtr, TyUint32, this->m_func), helperCallInstr);
    m_lowererMD.LoadHelperArgument(helperCallInstr, argPtrOpnd);

    m_lowererMD.LoadHelperArgument(helperCallInstr, excessOpnd);
    m_lowererMD.ChangeToHelperCall(helperCallInstr, helperMethod);

    return helperCallInstr;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerArgIn
///
/// This function checks the passed-in argument count against the index of this
/// argument and uses null for a param value if the caller didn't explicitly
/// pass anything.
///
///----------------------------------------------------------------------------
IR::Instr *
Lowerer::LowerArgIn(IR::Instr *instrArgIn)
{
    IR::LabelInstr *   labelDone;
    IR::LabelInstr *   labelUndef;
    IR::LabelInstr *   labelNormal;
    IR::LabelInstr *   labelInit;
    IR::LabelInstr *   labelInitNext;
    IR::BranchInstr *  instrBranch;
    IR::Instr *        instrArgInNext;
    IR::Instr *        instrInsert;
    IR::Instr *        instrPrev;
    IR::Instr *        instrResume = nullptr;
    IR::Opnd *         dstOpnd;
    IR::Opnd *         srcOpnd;
    IR::Opnd *         opndUndef;
    Js::ArgSlot        argIndex;
    StackSym *         symParam;
    BOOLEAN            isDuplicate;
    IR::RegOpnd *      generatorArgsPtrOpnd = nullptr;

    // We start with:
    // s1 = ArgIn_A param1
    // s2 = ArgIn_A param2
    // ...
    // sn = ArgIn_A paramn
    //
    // We want to end up with:
    //
    // s1 = ArgIn_A param1            -- Note that this is unconditional
    // count = (load from param area)
    //     BrLt_A $start, count, n    -- Forward cbranch to the uncommon case
    //     Br $Ln
    // $start:
    //     sn = assign undef
    //     BrGe_A $Ln-1, count, n-1
    //     sn-1 = assign undef
    // ...
    //     s2 = assign undef
    //     Br $done
    // $Ln:
    //     sn = assign paramn
    // $Ln-1:
    //     sn-1 = assign paramn-1
    // ...
    //     s2 = assign param2
    // $done:

    IR::Opnd *restDst = nullptr;
    bool hasRest = instrArgIn->m_opcode == Js::OpCode::ArgIn_Rest;
    if (hasRest)
    {
        IR::Instr *restInstr = instrArgIn;
        restDst = restInstr->UnlinkDst();
        if (m_func->GetJnFunction()->GetHasImplicitArgIns() && m_func->GetInParamsCount() > 1)
        {
            while (instrArgIn->m_opcode != Js::OpCode::ArgIn_A)
            {
                instrArgIn = instrArgIn->m_prev;
                if (instrResume == nullptr)
                {
                    instrResume = instrArgIn;
                }
            }
            restInstr->Remove();
        }
        else
        {
            IR::Instr * instrCount = m_lowererMD.LoadInputParamCount(instrArgIn, -this->m_func->GetInParamsCount());
            IR::Opnd * excessOpnd = instrCount->GetDst();

            IR::LabelInstr *createRestArrayLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

            // BrGe $createRestArray, excess, 0
            InsertCompareBranch(excessOpnd, IR::IntConstOpnd::New(0, TyUint8, this->m_func), Js::OpCode::BrGe_A, createRestArrayLabel, instrArgIn);

            // MOV excess, 0
            InsertMove(excessOpnd, IR::IntConstOpnd::New(0, TyUint8, this->m_func), instrArgIn);

            // $createRestArray
            instrArgIn->InsertBefore(createRestArrayLabel);

            if (m_func->GetJnFunction()->IsGenerator())
            {
                generatorArgsPtrOpnd = LoadGeneratorArgsPtr(instrArgIn);
            }

            IR::IntConstOpnd * formalsOpnd = IR::IntConstOpnd::New(this->m_func->GetInParamsCount(), TyUint32, this->m_func);
            IR::Instr *prev = LowerRestParameter(formalsOpnd, restDst, excessOpnd, instrArgIn, generatorArgsPtrOpnd);
            instrArgIn->Remove();
            return prev;
        }
    }


    srcOpnd = instrArgIn->GetSrc1();
    symParam = srcOpnd->AsSymOpnd()->m_sym->AsStackSym();

    argIndex = symParam->GetParamSlotNum();
    if (argIndex == 1)
    {
        // The "this" argument is not source-dependent and doesn't need to be checked.
        if (m_func->GetJnFunction()->IsGenerator())
        {
            generatorArgsPtrOpnd = LoadGeneratorArgsPtr(instrArgIn);
            ConvertArgOpndIfGeneratorFunction(instrArgIn, generatorArgsPtrOpnd);
        }

        m_lowererMD.ChangeToAssign(instrArgIn);
        return instrResume == nullptr ? instrArgIn->m_prev : instrResume;
    }

    Js::ArgSlot formalsCount = this->m_func->GetInParamsCount();

    AssertMsg(argIndex == formalsCount, "Expect to see the ArgIn's in numerical order");

    // Because there may be instructions between the ArgIn's, such as saves to the frame object,
    // we find the top of the sequence of ArgIn's and insert everything there. This assumes that
    // ArgIn's use param symbols as src's and not the results of previous instructions.

    instrPrev = instrArgIn;
    instrInsert = instrArgIn->m_next;
    while (argIndex > 2)
    {
        instrPrev = instrPrev->m_prev;
        if (instrPrev->m_opcode == Js::OpCode::ArgIn_A)
        {
            srcOpnd = instrPrev->GetSrc1();
            symParam = srcOpnd->AsSymOpnd()->m_sym->AsStackSym();
            AssertMsg(symParam->GetParamSlotNum() == argIndex - 1, "ArgIn's not in numerical order");
            argIndex = symParam->GetParamSlotNum();
        }
        else
        {
            // Make sure that this instruction gets lowered.
            if (instrResume == nullptr)
            {
                instrResume = instrPrev;
            }
        }
    }
    // The loading of parameters will be inserted above this instruction.
    instrInsert = instrPrev;
    if (instrResume == nullptr)
    {
        // We found no intervening non-ArgIn's, so lowering can resume at the previous instruction.
        instrResume = instrInsert->m_prev;
    }

    // Now insert all the checks and undef-assigns.

    if (m_func->GetJnFunction()->IsGenerator())
    {
        generatorArgsPtrOpnd = LoadGeneratorArgsPtr(instrInsert);
    }


    // excessOpnd = (load from param area) - formalCounts
    IR::Instr * instrCount = this->m_lowererMD.LoadInputParamCount(instrInsert, -formalsCount, true);
    IR::Opnd * excessOpnd = instrCount->GetDst();

    labelUndef = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, /*helperLabel*/ true);
    Lowerer::InsertBranch(Js::OpCode::BrLt_A, labelUndef, instrInsert);

    //     Br $Ln

    labelNormal = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    labelInit = labelNormal;
    instrBranch = IR::BranchInstr::New(Js::OpCode::Br, labelNormal, this->m_func);
    instrInsert->InsertBefore(instrBranch);
    this->m_lowererMD.LowerUncondBranch(instrBranch);

    // Insert the labels

    instrInsert->InsertBefore(labelUndef);
    instrInsert->InsertBefore(labelNormal);

    // MOV undefReg, undefAddress
    IR::Opnd* opndUndefAddress = this->LoadLibraryValueOpnd(labelNormal, LibraryValue::ValueUndefined);
    opndUndef =  IR::RegOpnd::New(TyMachPtr, this->m_func);
    LowererMD::CreateAssign(opndUndef, opndUndefAddress, labelNormal);


    BVSparse<JitArenaAllocator> *formalsBv = JitAnew(this->m_func->m_alloc, BVSparse<JitArenaAllocator>, this->m_func->m_alloc);
    while (formalsCount > 2)
    {
        dstOpnd = instrArgIn->GetDst();

        Assert(dstOpnd->IsRegOpnd());
        isDuplicate = formalsBv->TestAndSet(dstOpnd->AsRegOpnd()->m_sym->AsStackSym()->m_id);

        // Now insert the undef initialization before the "normal" label

        //     sn = assign undef

        LowererMD::CreateAssign(dstOpnd, opndUndef, labelNormal);

        //     INC excessOpnd
        //     BrEq_A $Ln-1

        formalsCount--;
        InsertAdd(true, excessOpnd, excessOpnd, IR::IntConstOpnd::New(1, TyInt32, this->m_func), labelNormal);
        labelInitNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        InsertBranch(Js::OpCode::BrEq_A, labelInitNext, labelNormal);

        // And insert the "normal" initialization before the "done" label

        //     sn = assign paramn
        // $Ln-1:

        labelInit->InsertAfter(labelInitNext);
        labelInit = labelInitNext;

        instrArgInNext = instrArgIn->m_prev;
        instrArgIn->Unlink();

        //      function foo(x, x)  { use(x); }
        // This should refer to the second 'x'.  Since we reverse the order here however, we need to skip
        // the initialization of the first 'x' to not override the one for the second.  WOOB:1105504
        if (isDuplicate)
        {
            instrArgIn->Free();
        }
        else
        {
            ConvertArgOpndIfGeneratorFunction(instrArgIn, generatorArgsPtrOpnd);
            labelInit->InsertBefore(instrArgIn);
            this->m_lowererMD.ChangeToAssign(instrArgIn);
        }
        instrArgIn = instrArgInNext;

        while (instrArgIn->m_opcode != Js::OpCode::ArgIn_A)
        {
            instrArgIn = instrArgIn->m_prev;
            AssertMsg(instrArgIn, "???");
        }

        AssertMsg(instrArgIn->GetSrc1()->AsSymOpnd()->m_sym->AsStackSym()->GetParamSlotNum() == formalsCount,
                  "Expect all ArgIn's to be in numerical order by param slot");
    }

    // Insert final undef and normal initializations, jumping unconditionally to the end
    // rather than checking against the decremented formals count as we did inside the loop above.

    //     s2 = assign undef

    dstOpnd = instrArgIn->GetDst();
    Assert(dstOpnd->IsRegOpnd());
    isDuplicate = formalsBv->TestAndSet(dstOpnd->AsRegOpnd()->m_sym->AsStackSym()->m_id);

    LowererMD::CreateAssign(dstOpnd, opndUndef, labelNormal);

    if (hasRest)
    {
        InsertMove(excessOpnd, IR::IntConstOpnd::New(0, TyUint8, this->m_func), labelNormal);
    }

    //     Br $done

    labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instrBranch = IR::BranchInstr::New(Js::OpCode::Br, labelDone, this->m_func);
    labelNormal->InsertBefore(instrBranch);
    this->m_lowererMD.LowerUncondBranch(instrBranch);

    //     s2 = assign param2
    // $done:

    labelInit->InsertAfter(labelDone);

    if (hasRest)
    {
        // The formals count has been tainted, so restore it before lowering rest
        IR::IntConstOpnd * formalsOpnd = IR::IntConstOpnd::New(this->m_func->GetInParamsCount(), TyUint32, this->m_func);
        LowerRestParameter(formalsOpnd, restDst, excessOpnd, labelDone, generatorArgsPtrOpnd);
    }

    instrArgIn->Unlink();
    if (isDuplicate)
    {
        instrArgIn->Free();
    }
    else
    {
        ConvertArgOpndIfGeneratorFunction(instrArgIn, generatorArgsPtrOpnd);
        labelDone->InsertBefore(instrArgIn);
        this->m_lowererMD.ChangeToAssign(instrArgIn);
    }

    return instrResume;
}

void
Lowerer::ConvertArgOpndIfGeneratorFunction(IR::Instr *instrArgIn, IR::RegOpnd *generatorArgsPtrOpnd)
{
    if (this->m_func->GetJnFunction()->IsGenerator())
    {
        // Replace stack param operand with offset into arguments array held by
        // the generator object.
        IR::Opnd * srcOpnd = instrArgIn->UnlinkSrc1();
        StackSym * symParam = srcOpnd->AsSymOpnd()->m_sym->AsStackSym();
        Js::ArgSlot argIndex = symParam->GetParamSlotNum();

        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(generatorArgsPtrOpnd, (argIndex - 1) * MachPtr, TyMachPtr, this->m_func);

        srcOpnd->Free(this->m_func);
        instrArgIn->SetSrc1(indirOpnd);
    }
}

IR::RegOpnd *
Lowerer::LoadGeneratorArgsPtr(IR::Instr *instrInsert)
{
    IR::Instr * instr = LoadGeneratorObject(instrInsert);
    IR::RegOpnd * generatorRegOpnd = instr->GetDst()->AsRegOpnd();

    IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(generatorRegOpnd, Js::JavascriptGenerator::GetArgsPtrOffset(), TyMachPtr, instrInsert->m_func);
    IR::RegOpnd * argsPtrOpnd = IR::RegOpnd::New(TyMachReg, instrInsert->m_func);
    LowererMD::CreateAssign(argsPtrOpnd, indirOpnd, instrInsert);
    return argsPtrOpnd;
}

IR::Instr *
Lowerer::LoadGeneratorObject(IR::Instr * instrInsert)
{
    StackSym * generatorSym = StackSym::NewParamSlotSym(1, instrInsert->m_func);
    instrInsert->m_func->SetArgOffset(generatorSym, LowererMD::GetFormalParamOffset() * MachPtr);
    IR::SymOpnd * generatorSymOpnd = IR::SymOpnd::New(generatorSym, TyMachPtr, instrInsert->m_func);
    IR::RegOpnd * generatorRegOpnd = IR::RegOpnd::New(TyMachPtr, instrInsert->m_func);
    return LowererMD::CreateAssign(generatorRegOpnd, generatorSymOpnd, instrInsert);
}

IR::Instr *
Lowerer::LowerArgInAsmJs(IR::Instr * instrArgIn)
{
    Assert(m_func->GetJnFunction()->GetIsAsmjsMode());

    Js::ArgSlot argCount = m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetArgCount();
    IR::Instr * instr = instrArgIn;
    for (int argNum = argCount - 1; argNum >= 0; --argNum)
    {
        IR::Instr * instrPrev = instr->m_prev;
        m_lowererMD.ChangeToAssign(instr);
        instr = instrPrev;
    }

    return instr;
}

bool
Lowerer::InlineBuiltInLibraryCall(IR::Instr *callInstr)
{
    IR::Opnd *src1 = callInstr->GetSrc1();
    IR::Opnd *src2 = callInstr->GetSrc2();

    // Get the arg count by looking at the slot number of the last arg symbol.

    if (!src2->IsSymOpnd())
    {
        // No args? Not sure this is possible, but handle it.
        return false;
    }

    StackSym *argLinkSym = src2->AsSymOpnd()->m_sym->AsStackSym();
    // Subtract "this" from the arg count.
    IntConstType argCount = argLinkSym->GetArgSlotNum() - 1;

    // Find the callee's built-in index (if any).
    Js::BuiltinFunction index = Func::GetBuiltInIndex(src1);

    // Warning!
    // Don't add new built-in to following switch. Built-ins needs to be inlined in call direct way.
    // Following is only for prejit scenarios where we don't get inlining always and generate fast path in lowerer.
    // Generating fastpath here misses fixed functions and globopt optimizations.
    switch(index)
    {
        case Js::BuiltinFunction::String_CharAt:
        case Js::BuiltinFunction::String_CharCodeAt:
            if (argCount != 1)
            {
                return false;
            }
            if (!callInstr->GetDst())
            {
                // Optimization of Char[Code]At assumes result is used.
                return false;
            }
            break;
        case Js::BuiltinFunction::Math_Abs:
#ifdef _M_IX86
            if (!AutoSystemInfo::Data.SSE2Available())
            {
                return false;
            }
#endif
            if (argCount != 1)
            {
                return false;
            }
            if (!callInstr->GetDst())
            {
                // Optimization of Abs assumes result is used.
                return false;
            }
            break;

        case Js::BuiltinFunction::Array_Push:
        {
            if (argCount != 1)
            {
                return false;
            }
            if (callInstr->GetDst())
            {
                // Optimization of push assumes result is unused.
                return false;
            }

            StackSym *linkSym = callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym();
            Assert(linkSym->IsSingleDef());
            linkSym = linkSym->m_instrDef->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym();
            Assert(linkSym->IsSingleDef());

            IR::Opnd *const arrayOpnd = linkSym->m_instrDef->GetSrc1();
            if(!arrayOpnd->IsRegOpnd())
            {
                // This should be rare, but needs to be handled.
                // By now, we've already started some of the inlining.  Simply jmp to the helper.
                // The branch will get peeped later.
                return false;
            }

            if(!ShouldGenerateArrayFastPath(arrayOpnd, false, false, false) ||
               arrayOpnd->GetValueType().IsLikelyNativeArray())
            {
                // Rejecting native array for now, since we have to do a FromVar at the call site and bail out.
                return false;
            }

            break;
        }

        case Js::BuiltinFunction::String_Replace:
        {
            if(argCount != 2)
            {
                return false;
            }

            if(!ShouldGenerateStringReplaceFastPath(callInstr, argCount))
            {
                return false;
            }
            break;
        }

        default:
            return false;
    }

    Assert(Func::IsBuiltInInlinedInLowerer(callInstr->GetSrc1()));

    IR::Opnd *callTargetOpnd = callInstr->GetSrc1();

    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    IR::Opnd *objRefOpnd = IR::MemRefOpnd::New((void*)this->GetObjRefForBuiltInTarget(callTargetOpnd->AsRegOpnd()), TyMachReg, this->m_func);

    InsertCompareBranch(callTargetOpnd, objRefOpnd, Js::OpCode::BrNeq_A, labelHelper, callInstr);

    callInstr->InsertBefore(labelHelper);

    Assert(argCount <= 2);

    IR::Opnd *argsOpnd[3];
    IR::Opnd *linkOpnd = callInstr->GetSrc2();

    while(linkOpnd->IsSymOpnd())
    {
        IR::SymOpnd *src2 = linkOpnd->AsSymOpnd();
        StackSym *sym = src2->m_sym->AsStackSym();
        Assert(sym->m_isSingleDef);
        IR::Instr *argInstr = sym->m_instrDef;

        Assert(argCount >= 0);
        argsOpnd[argCount] = argInstr->GetSrc1();
        argCount--;

        argInstr->Unlink();
        labelHelper->InsertAfter(argInstr);

        linkOpnd = argInstr->GetSrc2();
    }
    AnalysisAssert(argCount == -1);

    //  Move startcall
    Assert(linkOpnd->IsRegOpnd());
    StackSym *sym = linkOpnd->AsRegOpnd()->m_sym;
    Assert(sym->m_isSingleDef);
    IR::Instr *startCall = sym->m_instrDef;
    Assert(startCall->m_opcode == Js::OpCode::StartCall);
    startCall->Unlink();
    labelHelper->InsertAfter(startCall);

    // $doneLabel:
    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    callInstr->InsertAfter(doneLabel);

    bool success = true;
    switch(index)
    {
        case Js::BuiltinFunction::Math_Abs:
            this->m_lowererMD.GenerateFastAbs(callInstr->GetDst(), argsOpnd[1], callInstr, labelHelper, labelHelper, doneLabel);
            break;

        case Js::BuiltinFunction::String_CharCodeAt:
        case Js::BuiltinFunction::String_CharAt:
            success = this->m_lowererMD.GenerateFastCharAt(index, callInstr->GetDst(), argsOpnd[0], argsOpnd[1],
                callInstr, labelHelper, labelHelper, doneLabel);
            break;

        case Js::BuiltinFunction::Array_Push:
            success = GenerateFastPush(argsOpnd[0], argsOpnd[1], callInstr, labelHelper, labelHelper, nullptr, doneLabel);
            break;

        case Js::BuiltinFunction::String_Replace:
            success = GenerateFastReplace(argsOpnd[0], argsOpnd[1], argsOpnd[2], callInstr, labelHelper, labelHelper, doneLabel);
            break;

        default:
            Assert(UNREACHED);
    }

    IR::Instr *instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, doneLabel, this->m_func);
    labelHelper->InsertBefore(instr);

    return success;
}

// Perform lowerer part of inlining built-in function.
// For details, see inline.cpp.
//
// Description of changes here (note that taking care of Argouts are similar to InlineeStart):
// - Move ArgOut_A_InlineBuiltIn next to the call instr -- used by bailout processing in register allocator.
// - Remove StartCall and InlineBuiltInStart for this call.
// Before:
//   StartCall fn
//   d1 = BIA s1, link1
//   ...
//   InlineBuiltInStart fn, link0
// After:
//   ...
//   d1 = BIA s1, NULL
void Lowerer::LowerInlineBuiltIn(IR::Instr* builtInEndInstr)
{
    Assert(builtInEndInstr->m_opcode == Js::OpCode::InlineBuiltInEnd || builtInEndInstr->m_opcode == Js::OpCode::InlineNonTrackingBuiltInEnd);
    IR::Instr* startCallInstr;
    builtInEndInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        startCallInstr = argInstr->GetSrc2()->GetStackSym()->m_instrDef;
        return false;
    });
    // Keep the startCall around as bailout refers to it. Just unlink it for now - do not delete it.
    startCallInstr->Unlink();
    builtInEndInstr->Remove();
}

Js::JavascriptFunction **
Lowerer::GetObjRefForBuiltInTarget(IR::RegOpnd * regOpnd)
{
    Js::JavascriptFunction ** mathFns =
        this->m_func->GetScriptContext()->GetLibrary()->GetBuiltinFunctions();
    Js::BuiltinFunction index = regOpnd->m_sym->m_builtInIndex;

    AssertMsg(index < Js::BuiltinFunction::Count, "Invalid built-in index on a call target marked as built-in");

    return mathFns + index;
}

IR::Instr *
Lowerer::LowerNewRegEx(IR::Instr * instr)
{
    IR::Opnd *src1 = instr->UnlinkSrc1();

    Assert(src1->IsAddrOpnd());

#if ENABLE_REGEX_CONFIG_OPTIONS
    if (REGEX_CONFIG_FLAG(RegexTracing))
    {
        Assert(!instr->GetDst()->CanStoreTemp());
        IR::Instr * instrPrev = LoadScriptContext(instr);
        instrPrev = m_lowererMD.LoadHelperArgument(instr, src1);
        m_lowererMD.ChangeToHelperCall(instr, IR::HelperScrRegEx_OP_NewRegEx);
        return instrPrev;
    }
#endif
    IR::Instr * instrPrev = instr->m_prev;
    IR::RegOpnd * dstOpnd = instr->UnlinkDst()->AsRegOpnd();
    IR::SymOpnd * tempObjectSymOpnd;
    bool isZeroed = GenerateRecyclerOrMarkTempAlloc(instr, dstOpnd, IR::HelperAllocMemForJavascriptRegExp, sizeof(Js::JavascriptRegExp), &tempObjectSymOpnd);
    if (tempObjectSymOpnd && !PHASE_OFF(Js::HoistMarkTempInitPhase, this->m_func) && this->outerMostLoopLabel)
    {
        // Hoist the vtable and pattern init to the outer most loop top as it never changes
        InsertMove(tempObjectSymOpnd,
            LoadVTableValueOpnd(this->outerMostLoopLabel, VTableValue::VtableJavascriptRegExp),
            this->outerMostLoopLabel, false);
    }
    else
    {
        GenerateMemInit(dstOpnd, 0, LoadVTableValueOpnd(instr, VTableValue::VtableJavascriptRegExp), instr, isZeroed);
    }
    GenerateMemInit(dstOpnd, Js::JavascriptRegExp::GetOffsetOfType(),
        this->LoadLibraryValueOpnd(instr, LibraryValue::ValueRegexType), instr, isZeroed);
    GenerateMemInitNull(dstOpnd, Js::JavascriptRegExp::GetOffsetOfAuxSlots(), instr, isZeroed);
    GenerateMemInitNull(dstOpnd, Js::JavascriptRegExp::GetOffsetOfObjectArray(), instr, isZeroed);
    if (tempObjectSymOpnd && !PHASE_OFF(Js::HoistMarkTempInitPhase, this->m_func) && this->outerMostLoopLabel)
    {
        InsertMove(IR::SymOpnd::New(tempObjectSymOpnd->m_sym,
            tempObjectSymOpnd->m_offset + Js::JavascriptRegExp::GetOffsetOfPattern(), TyMachPtr, this->m_func),
            src1, this->outerMostLoopLabel, false);
    }
    else
    {
        GenerateMemInit(dstOpnd, Js::JavascriptRegExp::GetOffsetOfPattern(), src1, instr, isZeroed);
    }
    GenerateMemInitNull(dstOpnd, Js::JavascriptRegExp::GetOffsetOfSplitPattern(), instr, isZeroed);
    GenerateMemInitNull(dstOpnd, Js::JavascriptRegExp::GetOffsetOfLastIndexVar(), instr, isZeroed);
    GenerateMemInit(dstOpnd, Js::JavascriptRegExp::GetOffsetOfLastIndexOrFlag(), 0, instr, isZeroed);
    instr->Remove();

    return instrPrev;
}

IR::Instr *
Lowerer::GenerateRuntimeError(IR::Instr * insertBeforeInstr, Js::MessageId errorCode, IR::JnHelperMethod helper /*= IR::JnHelperMethod::HelperOp_RuntimeTypeError*/)
{
    IR::Instr * runtimeErrorInstr = IR::Instr::New(Js::OpCode::RuntimeTypeError, this->m_func);
    runtimeErrorInstr->SetSrc1(IR::IntConstOpnd::New(errorCode, TyInt32, this->m_func, true));
    insertBeforeInstr->InsertBefore(runtimeErrorInstr);
    return this->LowerUnaryHelperMem(runtimeErrorInstr, helper);
}

bool Lowerer::IsNullOrUndefRegOpnd(IR::RegOpnd *opnd) const
{
    StackSym *sym = opnd->m_sym;

    if (!sym->IsConst() || sym->IsIntConst() || sym->IsFloatConst())
    {
        return false;
    }
    Js::Var var = sym->GetConstAddress();
    Js::TypeId typeId = Js::RecyclableObject::FromVar(var)->GetTypeId();
    return typeId == Js::TypeIds_Null || typeId == Js::TypeIds_Undefined;
}

bool Lowerer::IsConstRegOpnd(IR::RegOpnd *opnd) const
{
    StackSym *sym = opnd->m_sym;

    if (!sym->IsConst() || sym->IsIntConst() || sym->IsFloatConst())
    {
        return false;
    }
    Js::Var var = sym->GetConstAddress();
    Js::TypeId typeId = Js::RecyclableObject::FromVar(var)->GetTypeId();
    return typeId == Js::TypeIds_Null || typeId == Js::TypeIds_Undefined || typeId == Js::TypeIds_Boolean;
}

bool
Lowerer::HasSideEffects(IR::Instr *instr)
{
    if (LowererMD::IsCall(instr))
    {
#ifdef _M_IX86
        IR::Opnd *src1 = instr->GetSrc1();
        if (src1->IsHelperCallOpnd())
        {
            IR::HelperCallOpnd * helper = src1->AsHelperCallOpnd();

            switch(helper->m_fnHelper)
            {
            case IR::HelperOp_Int32ToAtomInPlace:
            case IR::HelperOp_Int32ToAtom:
            case IR::HelperOp_UInt32ToAtom:
                return false;
            }
        }
#endif
        return true;
    }
    return instr->HasAnySideEffects();
}

IR::Instr*
Lowerer::GenerateFastInlineBuiltInMathRandom(IR::Instr* instr)
{
    AssertMsg(instr->GetDst()->IsFloat(), "dst must be float.");
    IR::Instr* retInstr = instr->m_prev;
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* tmpdst = dst;
    if(!dst->IsRegOpnd())
    {
        tmpdst = IR::RegOpnd::New(dst->GetType(), instr->m_func);
    }

    LoadScriptContext(instr);
    IR::Instr * helperCallInstr = IR::Instr::New(LowererMD::MDCallOpcode, tmpdst, instr->m_func);
    instr->InsertBefore(helperCallInstr);
    m_lowererMD.ChangeToHelperCall(helperCallInstr, IR::JnHelperMethod::HelperDirectMath_Random);

    if(tmpdst != dst)
    {
        InsertMove(dst, tmpdst, instr);
    }

    instr->Remove();
    return retInstr;
}

IR::Instr *
Lowerer::LowerCallDirect(IR::Instr * instr)
{
    IR::Opnd* linkOpnd = instr->UnlinkSrc2();
    StackSym *linkSym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();
    IR::Instr* argInstr = linkSym->m_instrDef;
    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A_InlineSpecialized);
    IR::Opnd* funcObj = argInstr->UnlinkSrc1();
    instr->SetSrc2(argInstr->UnlinkSrc2());
    argInstr->Remove();

    if(instr->HasBailOutInfo())
    {
        IR::Instr * bailOutInstr = this->SplitBailOnImplicitCall(instr, instr->m_next, instr->m_next);
        this->LowerBailOnEqualOrNotEqual(bailOutInstr);
    }
    Js::CallFlags flags = instr->GetDst() ? Js::CallFlags_Value : Js::CallFlags_NotUsed;
    return this->GenerateDirectCall(instr, funcObj, (ushort)flags);
}

IR::Instr *
Lowerer::GenerateDirectCall(IR::Instr* inlineInstr, IR::Opnd* funcObj, ushort callflags)
{
    int32 argCount = m_lowererMD.LowerCallArgs(inlineInstr, callflags);
    m_lowererMD.LoadHelperArgument(inlineInstr, funcObj);
    m_lowererMD.LowerCall(inlineInstr, (Js::ArgSlot)argCount); //to account for function object and callinfo

    return inlineInstr->m_prev;
}

/*
*   GenerateHelperToArrayPushFastPath
*   Generates Helper Call and pushes arguments to the Push HelperCall
*/
IR::Instr *
Lowerer::GenerateHelperToArrayPushFastPath(IR::Instr * instr, IR::LabelInstr * bailOutLabelHelper)
{
    IR::Opnd * arrayHelperOpnd = instr->UnlinkSrc1();
    IR::Opnd * elementHelperOpnd = instr->UnlinkSrc2();
    IR::JnHelperMethod helperMethod;

    if(elementHelperOpnd->IsInt32())
    {
        Assert(arrayHelperOpnd->GetValueType().IsLikelyNativeIntArray());
        helperMethod = IR::HelperArray_NativeIntPush;

        m_lowererMD.LoadHelperArgument(instr, elementHelperOpnd);
    }
    else if(elementHelperOpnd->IsFloat())
    {
        Assert(arrayHelperOpnd->GetValueType().IsLikelyNativeFloatArray());
        helperMethod = IR::HelperArray_NativeFloatPush;

    //Currently, X64 floating-point calling convention is not supported. Hence store the
    // float value explicitly in RegXMM2 (RegXMM0 and RegXMM1 will be filled with ScriptContext and Var respectively)
#if _M_X64
        IR::RegOpnd* regXMM2 = IR::RegOpnd::New(nullptr, (RegNum)RegXMM2, TyMachDouble, this->m_func);
        regXMM2->m_isCallArg = true;
        IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOVSD, regXMM2, elementHelperOpnd, this->m_func);
        instr->InsertBefore(movInstr);
#else
        m_lowererMD.LoadDoubleHelperArgument(instr, elementHelperOpnd);
#endif

    }
    else
    {
        helperMethod = IR::HelperArray_VarPush;
        m_lowererMD.LoadHelperArgument(instr, elementHelperOpnd);
    }

    m_lowererMD.LoadHelperArgument(instr, arrayHelperOpnd);
    LoadScriptContext(instr);
    return m_lowererMD.ChangeToHelperCall(instr, helperMethod);
}

/*
*   GenerateHelperToArrayPopFastPath
*   Generates Helper Call and pushes arguments to the Pop HelperCall
*/
IR::Instr *
Lowerer::GenerateHelperToArrayPopFastPath(IR::Instr * instr, IR::LabelInstr * doneLabel, IR::LabelInstr * bailOutLabelHelper)
{
    IR::Opnd * arrayHelperOpnd = instr->UnlinkSrc1();
    ValueType arrayValueType = arrayHelperOpnd->GetValueType();

    IR::JnHelperMethod helperMethod;

    //Decide the helperMethod based on dst availability and nativity of the array.
    if(arrayValueType.IsLikelyNativeArray() && !instr->GetDst())
    {
        helperMethod = IR::HelperArray_NativePopWithNoDst;
    }
    else if(arrayValueType.IsLikelyNativeIntArray())
    {
        helperMethod = IR::HelperArray_NativeIntPop;
    }
    else if(arrayValueType.IsLikelyNativeFloatArray())
    {
        helperMethod = IR::HelperArray_NativeFloatPop;
    }
    else
    {
        helperMethod = IR::HelperArray_VarPop;
    }

    m_lowererMD.LoadHelperArgument(instr, arrayHelperOpnd);

    //We do not need scriptContext for HelperArray_NativePopWithNoDst call.
    if(helperMethod != IR::HelperArray_NativePopWithNoDst)
    {
        LoadScriptContext(instr);
    }

    IR::Instr * retInstr = m_lowererMD.ChangeToHelperCall(instr, helperMethod, bailOutLabelHelper);

    //We don't need missing item check for var arrays, as there it is taken care by the helper.
    if(arrayValueType.IsLikelyNativeArray())
    {
        if(retInstr->GetDst())
        {
            //Do this check only for native arrays with Dst. For Var arrays, this is taken care in the Runtime helper itself.
            InsertCompareBranch(GetMissingItemOpnd(retInstr->GetDst()->GetType(), m_func), retInstr->GetDst(), Js::OpCode::BrNeq_A, doneLabel, bailOutLabelHelper);
        }
        else
        {
            //We need unconditional jump to doneLabel, if there is no dst in Pop instr.
            InsertBranch(Js::OpCode::Br, true, doneLabel, bailOutLabelHelper);
        }
    }

    return retInstr;
}

IR::Instr *
Lowerer::LowerCondBranchCheckBailOut(IR::BranchInstr * branchInstr, IR::Instr * helperCall, bool isHelper)
{
    Assert(branchInstr->m_opcode == Js::OpCode::BrTrue_A || branchInstr->m_opcode == Js::OpCode::BrFalse_A);
    if (branchInstr->HasBailOutInfo())
    {
        IR::BailOutKind debuggerBailOutKind = IR::BailOutInvalid;
        if (branchInstr->HasAuxBailOut())
        {
            // We have shared debugger bailout. For branches we lower it here, not in SplitBailForDebugger.
            // See SplitBailForDebugger for details.
            AssertMsg(!(branchInstr->GetBailOutKind() & IR::BailOutForDebuggerBits), "There should be no debugger bits in main bailout kind.");

            debuggerBailOutKind = branchInstr->GetAuxBailOutKind() & IR::BailOutForDebuggerBits;
            AssertMsg((debuggerBailOutKind & ~(IR::BailOutIgnoreException | IR::BailOutForceByFlag)) == 0, "Only IR::BailOutIgnoreException|ForceByFlag supported here.");
        }

        IR::Instr * bailOutInstr = this->SplitBailOnImplicitCall(branchInstr, helperCall, branchInstr);
        IR::Instr* prevInstr = this->LowerBailOnEqualOrNotEqual(bailOutInstr, branchInstr, nullptr, nullptr, isHelper);

        if (debuggerBailOutKind != IR::BailOutInvalid)
        {
            // Note that by this time implicit calls bailout is already lowered.
            // What we do here is use same bailout info and lower debugger bailout which would be shared bailout.
            BailOutInfo* bailOutInfo = bailOutInstr->GetBailOutInfo();
            IR::BailOutInstr* debuggerBailoutInstr = IR::BailOutInstr::New(
                Js::OpCode::BailForDebugger, debuggerBailOutKind, bailOutInfo, bailOutInfo->bailOutFunc);
            prevInstr->InsertAfter(debuggerBailoutInstr);
            // The result of that is:
            // original helper op_* instr, then debugger bailout, then implicit calls bailout/etc with the branch instr.
            // Example:
            //    s35(eax).i32    =  CALL           Op_GreaterEqual.u32                     # -- original op_* helper
            //    s34.i32         =  MOV            s35(eax).i32                            #
            //                       BailForDebugger                                        # Bailout: #0042 (BailOutIgnoreException) -- the debugger bailout
            //                       CMP            [0x0003BDE0].i8, 1 (0x1).i8             # -- implicit calls check
            //                       JEQ            $L10                                    #
            //$L11: [helper]                                                                #
            //                       CALL           SaveAllRegistersAndBranchBailOut.u32    # Bailout: #0042 (BailOutOnImplicitCalls)
            //                       JMP            $L5                                     #
            //$L10: [helper]                                                                #
            //                       BrFalse_A      $L3, s34.i32                            #0034 -- The BrTrue/BrFalse branch (branch instr)
            //$L6: [helper]                                                                 #0042

            this->LowerBailForDebugger(debuggerBailoutInstr, isHelper);
            // After lowering this we will have a check which on bailout condition will JMP to $L11.
        }
    }

    return m_lowererMD.LowerCondBranch(branchInstr);
}

IR::Instr *
Lowerer::LoadArgumentsFromStack(IR::Instr * instr)
{
    IR::Instr * prevInstr = instr->m_prev;

    Assert(instr->GetDst()->IsRegOpnd());
    if (instr->m_func->IsInlinee())
    {
        instr->ReplaceSrc1(instr->m_func->GetInlineeArgumentsObjectSlotOpnd());
    }
    else
    {
        instr->ReplaceSrc1(this->m_lowererMD.CreateStackArgumentsSlotOpnd());
    }
    this->m_lowererMD.ChangeToAssign(instr);

    return prevInstr;
}

IR::SymOpnd *
Lowerer::LoadCallInfo(IR::Instr * instrInsert)
{
    IR::SymOpnd * srcOpnd;
    Func * func = instrInsert->m_func;

    if (func->GetJnFunction()->IsGenerator())
    {
        // Generator function arguments and ArgumentsInfo are not on the stack.  Instead they
        // are accessed off the generator object (which is prm1).
        StackSym * generatorSym = StackSym::NewParamSlotSym(1, func);
        func->SetArgOffset(generatorSym, LowererMD::GetFormalParamOffset() * MachPtr);
        IR::SymOpnd * generatorSymOpnd = IR::SymOpnd::New(generatorSym, TyMachPtr, func);
        IR::RegOpnd * generatorRegOpnd = IR::RegOpnd::New(TyMachPtr, func);
        LowererMD::CreateAssign(generatorRegOpnd, generatorSymOpnd, instrInsert);

        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(generatorRegOpnd, Js::JavascriptGenerator::GetCallInfoOffset(), TyMachPtr, func);
        IR::Instr * instr = LowererMD::CreateAssign(IR::RegOpnd::New(TyMachPtr, func), indirOpnd, instrInsert);

        StackSym * callInfoSym = StackSym::New(TyMachReg, func);
        IR::SymOpnd * callInfoSymOpnd = IR::SymOpnd::New(callInfoSym, TyMachReg, func);
        LowererMD::CreateAssign(callInfoSymOpnd, instr->GetDst(), instrInsert);

        srcOpnd = IR::SymOpnd::New(callInfoSym, TyMachReg, func);
    }
    else
    {
        // Otherwise callInfo is always the "second" argument.
        // The stack looks like this:
        //
        //       script param N
        //       ...
        //       script param 1
        //       callinfo
        //       function object
        //       return addr
        // FP -> FP chain

        StackSym * srcSym = LowererMD::GetImplicitParamSlotSym(1, func);
        srcOpnd = IR::SymOpnd::New(srcSym, TyMachReg, func);
    }

    return srcOpnd;
}

IR::Instr *
Lowerer::LowerBailOnNotStackArgs(IR::Instr * instr)
{
    if (!this->m_func->GetHasStackArgs())
    {
        throw Js::RejitException(RejitReason::InlineApplyDisabled);
    }

    IR::Instr * prevInstr = instr->m_prev;

    // Bail out test
    // Label to skip Bailout and continue
    IR::LabelInstr * continueLabelInstr;
    IR::Instr *instrNext = instr->m_next;
    if (instrNext->IsLabelInstr())
    {
        continueLabelInstr = instrNext->AsLabelInstr();
    }
    else
    {
        continueLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func, false);
        instr->InsertAfter(continueLabelInstr);
    }

    IR::LabelInstr * helperLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    if (!instr->m_func->IsInlinee())
    {
        //BailOut if it is not stack args or the number of actuals (except "this" argument) is greater than or equal to 15.
        IR::Opnd* stackArgs = instr->UnlinkSrc1();
        InsertCompareBranch(stackArgs, instr->UnlinkSrc2(), Js::OpCode::BrNeq_A, helperLabelInstr, instr);

        IR::RegOpnd* ldLenDstOpnd = IR::RegOpnd::New(TyUint32, instr->m_func);
        IR::Instr* ldLen = IR::Instr::New(Js::OpCode::LdLen_A, ldLenDstOpnd, stackArgs, instr->m_func);
        ldLenDstOpnd->SetValueType(ValueType::GetTaggedInt()); //LdLen_A works only on stack arguments
        instr->InsertBefore(ldLen);
        this->GenerateFastRealStackArgumentsLdLen(ldLen);
        this->InsertCompareBranch(ldLenDstOpnd, IR::IntConstOpnd::New(Js::InlineeCallInfo::MaxInlineeArgoutCount, TyUint32, m_func, true),  Js::OpCode::BrLt_A, true, continueLabelInstr, instr);
    }
    else
    {
        //For Inlined functions, we are sure actuals can't exceed Js::InlineeCallInfo::MaxInlineeArgoutCount (15).
        InsertCompareBranch(instr->UnlinkSrc1(), instr->UnlinkSrc2(), Js::OpCode::BrEq_A, continueLabelInstr, instr);
    }

    instr->InsertBefore(helperLabelInstr);
    this->GenerateBailOut(instr, nullptr, nullptr);
    return prevInstr;
}

IR::Instr *
Lowerer::LowerBailOnNotSpreadable(IR::Instr *instr)
{
    // We only avoid bailing out / throwing a rejit exception when the array operand is a simple, non-optimized, non-object array.
    IR::Instr * prevInstr = instr->m_prev;
    Func *func = instr->m_func;

    IR::RegOpnd *arrayOpnd = nullptr;
    IR::Opnd *arraySrcOpnd = instr->UnlinkSrc1();
    if (!arraySrcOpnd->IsRegOpnd())
    {
        arrayOpnd = IR::RegOpnd::New(TyMachPtr, func);
        LowererMD::CreateAssign(arrayOpnd, arraySrcOpnd, instr);
    }
    else
    {
        arrayOpnd = arraySrcOpnd->AsRegOpnd();
    }

    const ValueType baseValueType(arrayOpnd->GetValueType());

    // Check if we can just throw a rejit exception based on valuetype alone instead of bailing out.
    if (!baseValueType.IsLikelyArray()
        || baseValueType.IsLikelyAnyOptimizedArray()
        || (baseValueType.IsLikelyObject() && (baseValueType.GetObjectType() == ObjectType::ObjectWithArray))

        // Validate that GenerateArrayTest will not fail.
        || !(baseValueType.IsUninitialized() || baseValueType.HasBeenObject())

        || m_func->IsInlinee())
    {
        throw Js::RejitException(RejitReason::InlineSpreadDisabled);
    }

    // Past this point, we will need to use a bailout.
    IR::LabelInstr *bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true /* isOpHelper */);

    // See if we can skip various array checks on value type alone
    if (!baseValueType.IsArray())
    {
        GenerateArrayTest(arrayOpnd, bailOutLabel, bailOutLabel, instr, false);
    }
    if (!(baseValueType.IsArray() && baseValueType.HasNoMissingValues()))
    {
        InsertTestBranch(
            IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfArrayFlags(), TyUint8, func),
            IR::IntConstOpnd::New(static_cast<uint8>(Js::DynamicObjectFlags::HasNoMissingValues), TyUint8, func, true),
            Js::OpCode::BrEq_A,
            bailOutLabel,
            instr);
    }

    IR::IndirOpnd *arrayLenPtrOpnd = IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfLength(), TyUint32, func);
    InsertCompareBranch(arrayLenPtrOpnd, IR::IntConstOpnd::New(Js::InlineeCallInfo::MaxInlineeArgoutCount - 1, TyUint8, func), Js::OpCode::BrGt_A, true, bailOutLabel, instr);

    IR::LabelInstr *skipBailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertBranch(Js::OpCode::Br, skipBailOutLabel, instr);

    instr->InsertBefore(bailOutLabel);
    instr->InsertAfter(skipBailOutLabel);

    GenerateBailOut(instr);

    return prevInstr;
}

IR::Instr *
Lowerer::LowerBailOnNotPolymorphicInlinee(IR::Instr * instr)
{
    Assert(instr->HasBailOutInfo() && (instr->GetBailOutKind() == IR::BailOutOnFailedPolymorphicInlineTypeCheck || instr->GetBailOutKind() == IR::BailOutOnPolymorphicInlineFunction));
    IR::Instr* instrPrev = instr->m_prev;

    this->GenerateBailOut(instr, nullptr, nullptr);

    return instrPrev;
}

void
Lowerer::LowerBailoutCheckAndLabel(IR::Instr *instr, bool onEqual, bool isHelper)
{
    // Label to skip Bailout and continue
    IR::LabelInstr * continueLabelInstr;
    IR::Instr *instrNext = instr->m_next;
    if (instrNext->IsLabelInstr())
    {
        continueLabelInstr = instrNext->AsLabelInstr();
    }
    else
    {
        continueLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func, isHelper);
        instr->InsertAfter(continueLabelInstr);
    }

    if(instr->GetBailOutKind() == IR::BailOutInjected)
    {
        // BailOnEqual 0, 0
        Assert(onEqual);
        Assert(instr->GetSrc1()->IsEqual(instr->GetSrc2()));
        Assert(instr->GetSrc1()->AsIntConstOpnd()->GetValue() == 0);

        // The operands cannot be equal when generating a compare (assert) but since this is for testing purposes, hoist a src.
        // Ideally, we would just create a BailOut instruction that generates a guaranteed bailout, but there seem to be issues
        // with doing this in a non-helper path. So finally, it would generate:
        //     xor s0, s0
        //     test s0, s0
        //     jnz $continue
        //   $bailout:
        //     // bailout
        //   $continue:
        instr->HoistSrc1(LowererMD::GetLoadOp(instr->GetSrc1()->GetType()));
    }

    InsertCompareBranch(instr->UnlinkSrc1(), instr->UnlinkSrc2(),
        onEqual ? Js::OpCode::BrNeq_A : Js::OpCode::BrEq_A, continueLabelInstr, instr);

    if (!isHelper)
    {
        IR::LabelInstr * helperLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
        instr->InsertBefore(helperLabelInstr);
    }
}

IR::Instr *
Lowerer::LowerBailOnEqualOrNotEqual(IR::Instr * instr,
    IR::BranchInstr *branchInstr, // = nullptr
    IR::LabelInstr *labelBailOut, // = nullptr
    IR::PropertySymOpnd * propSymOpnd, // = nullptr
    bool isHelper)                // = false
{
    IR::Instr * prevInstr = instr->m_prev;

    // Bail out test
    bool onEqual = instr->m_opcode == Js::OpCode::BailOnEqual;

    LowerBailoutCheckAndLabel(instr, onEqual, isHelper);

    // BailOutOnImplicitCalls is a post-op bailout. Since we look at the profile info for LdFld/StFld to decide whether the instruction may or may not call an accessor,
    // we need to update this profile information on the bailout path for BailOutOnImplicitCalls if the implicit call was an accessor call.
    if(propSymOpnd && ((instr->GetBailOutKind() & ~IR::BailOutKindBits) == IR::BailOutOnImplicitCalls) && (propSymOpnd->m_inlineCacheIndex != -1) &&
        instr->m_func->GetJnFunction()->HasDynamicProfileInfo())
    {
        // result = AND implCallFlags, ~ImplicitCall_None
        //          TST result, ImplicitCall_Accessor
        //          JEQ $bail
        //          OR profiledFlags, FldInfoAccessor
        //          $bail

        IR::Opnd * implicitCallFlags = GetImplicitCallFlagsOpnd();
        IR::Opnd * accessorImplicitCall = IR::IntConstOpnd::New(Js::ImplicitCall_Accessor & ~Js::ImplicitCall_None, GetImplicitCallFlagsType(), instr->m_func, true);
        IR::Opnd * maskNoImplicitCall = IR::IntConstOpnd::New((Js::ImplicitCallFlags)~Js::ImplicitCall_None, GetImplicitCallFlagsType(), instr->m_func, true);
        IR::Opnd * fldInfoAccessor = IR::IntConstOpnd::New(Js::FldInfo_FromAccessor, GetFldInfoFlagsType(), instr->m_func, true);
        IR::LabelInstr * label = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, true);

        IR::Instr * andInstr = InsertAnd(IR::RegOpnd::New(GetImplicitCallFlagsType(), instr->m_func), implicitCallFlags, maskNoImplicitCall, instr);
        InsertTestBranch(andInstr->GetDst(), accessorImplicitCall, Js::OpCode::BrEq_A, label, instr);

        Js::FldInfo * info = instr->m_func->GetJnFunction()->GetAnyDynamicProfileInfo()->GetFldInfo(instr->m_func->GetJnFunction(), propSymOpnd->m_inlineCacheIndex);

        IR::Opnd * profiledFlags = IR::MemRefOpnd::New((char*)info + info->GetOffsetOfFlags(), TyInt8, instr->m_func);

        InsertOr(profiledFlags, profiledFlags, fldInfoAccessor, instr);
        instr->InsertBefore(label);
    }

    this->GenerateBailOut(instr, branchInstr, labelBailOut);
    return prevInstr;
}

void Lowerer::LowerBailOnNegative(IR::Instr *const instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::BailOnNegative);
    Assert(instr->HasBailOutInfo());
    Assert(!instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc1()->GetType() == TyInt32 || instr->GetSrc1()->GetType() == TyUint32);
    Assert(!instr->GetSrc2());

    IR::LabelInstr *const skipBailOutLabel = instr->GetOrCreateContinueLabel(false);
    LowerOneBailOutKind(instr, instr->GetBailOutKind(), false);
    Assert(!instr->HasBailOutInfo());
    IR::Instr *insertBeforeInstr = instr->m_next;

    Func *const func = instr->m_func;

    //     test src, src
    //     jns  $skipBailOut
    InsertCompareBranch(
        instr->UnlinkSrc1(),
        IR::IntConstOpnd::New(0, TyInt32, func, true),
        Js::OpCode::BrGe_A,
        skipBailOutLabel,
        insertBeforeInstr);

    instr->Remove();
}

IR::Instr *
Lowerer::LowerBailOnNotObject(IR::Instr       *instr,
                              IR::BranchInstr *branchInstr  /* = nullptr */,
                              IR::LabelInstr  *labelBailOut /* = nullptr */)
{
    IR::Instr      *prevInstr          = instr->m_prev;
    IR::LabelInstr *continueLabelInstr = IR::LabelInstr::New(Js::OpCode::Label,
                                                             m_func);
    instr->InsertAfter(continueLabelInstr);
    this->m_lowererMD.GenerateObjectTest(instr->UnlinkSrc1(),
                                         instr,
                                         continueLabelInstr,
                                         /* fContinueLabel = */ true);
    this->GenerateBailOut(instr, branchInstr, labelBailOut);

    return prevInstr;
}

IR::Instr *
Lowerer::LowerBailOnNotBuiltIn(IR::Instr       *instr,
                               IR::BranchInstr *branchInstr  /* = nullptr */,
                               IR::LabelInstr  *labelBailOut /* = nullptr */)
{
    Assert(instr->GetSrc2()->IsIntConstOpnd());
    IR::Instr *prevInstr = instr->m_prev;

    Js::JavascriptFunction ** builtInFuncs = this->m_func->GetScriptContext()->GetLibrary()->GetBuiltinFunctions();
    Js::BuiltinFunction builtInIndex = instr->UnlinkSrc2()->AsIntConstOpnd()->AsInt32();

    IR::Opnd *builtIn = IR::MemRefOpnd::New((void*)(builtInFuncs + builtInIndex), TyMachReg, instr->m_func);

#if TESTBUILTINFORNULL
    IR::LabelInstr * continueAfterTestLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func);
    InsertTestBranch(builtIn, builtIn, Js::OpCode::BrNeq_A, continueAfterTestLabel, instr);
    this->m_lowererMD.GenerateDebugBreak(instr);
    instr->InsertBefore(continueAfterTestLabel);
#endif

    IR::LabelInstr * continueLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func);
    instr->InsertAfter(continueLabel);
    InsertCompareBranch(instr->UnlinkSrc1(), builtIn, Js::OpCode::BrEq_A, continueLabel, instr);

    GenerateBailOut(instr, branchInstr, labelBailOut);

    return prevInstr;
}

IR::Instr *
Lowerer::LowerBailForDebugger(IR::Instr* instr, bool isInsideHelper /* = false */)
{
    IR::Instr * prevInstr = instr->m_prev;

    IR::BailOutKind bailOutKind = instr->GetBailOutKind();
    AssertMsg(bailOutKind, "bailOutKind should not be zero at this time.");
    AssertMsg(!(bailOutKind & IR::BailOutExplicit) || bailOutKind == IR::BailOutExplicit,
        "BailOutExplicit cannot be combined with any other bailout flags.");

    IR::LabelInstr* bailOutLabel = nullptr;

    if (!(bailOutKind & IR::BailOutExplicit))
    {
        Js::DebugManager* debugManager = this->GetScriptContext()->GetThreadContext()->GetDebugManager();
        DebuggingFlags* flags = debugManager->GetDebuggingFlags();

        // Check 1 (do we need to bail out?)
        // JXX bailoutLabel
        // Check 2 (do we need to bail out?)
        // JXX bailoutLabel
        // ...
        // JMP continueLabel
        // bailoutDocumentLabel:
        // (determine if document boundary reached - if not, JMP to continueLabel)
        //  NOTE: THIS BLOCK IS CONDITIONALLY GENERATED BASED ON doGenerateBailOutDocumentBlock
        // bailoutLabel:
        // bail out
        // continueLabel:
        // ...

        IR::LabelInstr* bailOutDocumentLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, /*isOpHelper*/ true);
        instr->InsertBefore(bailOutDocumentLabel);
        IR::LabelInstr* bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, /*isOpHelper*/ true);
        instr->InsertBefore(bailOutLabel);
        IR::LabelInstr* continueLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, /*isOpHelper*/ isInsideHelper);
        instr->InsertAfter(continueLabel);
        IR::BranchInstr* continueBranchInstr = this->InsertBranch(Js::OpCode::Br, continueLabel, bailOutDocumentLabel);    // JMP continueLabel.

        bool doGenerateBailOutDocumentBlock = false;

        const IR::BailOutKind c_forceAndIgnoreEx = IR::BailOutForceByFlag | IR::BailOutIgnoreException;
        if ((bailOutKind & c_forceAndIgnoreEx) == c_forceAndIgnoreEx)
        {
            // It's faster to check these together in 1 check rather than 2 separate checks at run time.
            // CMP [&(flags->m_forceInterpreter, flags->m_isIgnoreException)], 0
            // BNE bailout
            IR::Opnd* opnd1 = IR::MemRefOpnd::New((BYTE*)flags + DebuggingFlags::GetForceInterpreterOffset(), TyInt16, m_func);
            IR::Opnd* opnd2 = IR::IntConstOpnd::New(0, TyInt16, m_func, /*dontEncode*/ true);
            InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrNeq_A, bailOutLabel, continueBranchInstr);
            bailOutKind ^= c_forceAndIgnoreEx;
        }
        else
        {
            if (bailOutKind & IR::BailOutForceByFlag)
            {
                // CMP [&flags->m_forceInterpreter], 0
                // BNE bailout
                IR::Opnd* opnd1 = IR::MemRefOpnd::New((BYTE*)flags + DebuggingFlags::GetForceInterpreterOffset(), TyInt8, m_func);
                IR::Opnd* opnd2 = IR::IntConstOpnd::New(0, TyInt8, m_func, /*dontEncode*/ true);
                InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrNeq_A, bailOutLabel, continueBranchInstr);
                bailOutKind ^= IR::BailOutForceByFlag;
            }
            if (bailOutKind & IR::BailOutIgnoreException)
            {
                // CMP [&flags->m_byteCodeOffsetAfterIgnoreException], DebuggingFlags::InvalidByteCodeOffset
                // BNE bailout
                IR::Opnd* opnd1 = IR::MemRefOpnd::New((BYTE*)flags + flags->GetByteCodeOffsetAfterIgnoreExceptionOffset(), TyInt32, m_func);
                IR::Opnd* opnd2 = IR::IntConstOpnd::New(DebuggingFlags::InvalidByteCodeOffset, TyInt32, m_func, /*dontEncode*/ true);
                InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrNeq_A, bailOutLabel, continueBranchInstr);
                bailOutKind ^= IR::BailOutIgnoreException;
            }
        }

        if (bailOutKind & IR::BailOutBreakPointInFunction)
        {
            // CMP [&functionBody->m_sourceInfo.m_probeCount], 0
            // BNE bailout
            Js::FunctionBody* body = m_func->GetJnFunction();
            IR::Opnd* opnd1 = IR::MemRefOpnd::New(&body->GetSourceInfo()->m_probeCount, TyInt32, m_func);
            IR::Opnd* opnd2 = IR::IntConstOpnd::New(0, TyInt32, m_func, /*dontEncode*/ true);
            InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrNeq_A, bailOutLabel, continueBranchInstr);
            bailOutKind ^= IR::BailOutBreakPointInFunction;
        }

        // on method entry
        if(bailOutKind & IR::BailOutStep)
        {
            // TEST STEP_BAILOUT, [&stepController->StepType]
            // BNE BailoutLabel
            IR::Opnd* opnd1 = IR::MemRefOpnd::New((void*)(debugManager->stepController.GetAddressOfStepType()), TyInt8, m_func);
            IR::Opnd* opnd2 = IR::IntConstOpnd::New(Js::STEP_BAILOUT, TyInt8, this->m_func, /*dontEncode*/ true);
            InsertTestBranch(opnd1, opnd2, Js::OpCode::BrNeq_A, bailOutLabel, continueBranchInstr);

            // CMP  STEP_DOCUMENT, [&stepController->StepType]
            // BEQ BailoutDocumentLabel
            opnd1 = IR::MemRefOpnd::New((void*)(debugManager->stepController.GetAddressOfStepType()), TyInt8, m_func);
            opnd2 = IR::IntConstOpnd::New(Js::STEP_DOCUMENT, TyInt8, this->m_func, /*dontEncode*/ true);
            InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrEq_A, /*isUnsigned*/ true, bailOutDocumentLabel, continueBranchInstr);

            doGenerateBailOutDocumentBlock = true;

            bailOutKind ^= IR::BailOutStep;
        }

        // on method exit
        if (bailOutKind & IR::BailOutStackFrameBase)
        {
            // CMP EffectiveFrameBase, [&stepController->frameAddrWhenSet]
            // BA bailoutLabel
            RegNum effectiveFrameBaseReg;
#ifdef _M_X64
            effectiveFrameBaseReg = m_lowererMD.GetRegStackPointer();
#else
            effectiveFrameBaseReg = m_lowererMD.GetRegFramePointer();
#endif
            IR::Opnd* opnd1 = IR::RegOpnd::New(nullptr, effectiveFrameBaseReg, TyMachReg, m_func);
            IR::Opnd* opnd2 = IR::MemRefOpnd::New(debugManager->stepController.GetAddressOfFrameAddress(), TyMachReg, m_func);
            this->InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrGt_A, /*isUnsigned*/ true, bailOutLabel, continueBranchInstr);

            // CMP  STEP_DOCUMENT, [&stepController->StepType]
            // BEQ BailoutDocumentLabel
            opnd1 = IR::MemRefOpnd::New((void*)(debugManager->stepController.GetAddressOfStepType()), TyInt8, m_func);
            opnd2 = IR::IntConstOpnd::New(Js::STEP_DOCUMENT, TyInt8, this->m_func, /*dontEncode*/ true);
            InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrEq_A, /*isUnsigned*/ true, bailOutDocumentLabel, continueBranchInstr);

            doGenerateBailOutDocumentBlock = true;

            bailOutKind ^= IR::BailOutStackFrameBase;
        }

        if (bailOutKind & IR::BailOutLocalValueChanged)
        {
            int32 hasLocalVarChangedOffset = m_func->GetHasLocalVarChangedOffset();
            if (hasLocalVarChangedOffset != Js::Constants::InvalidOffset)
            {
                // CMP [EBP + hasLocalVarChangedStackOffset], 0
                // BNE bailout
                StackSym* sym = StackSym::New(TyInt8, m_func);
                sym->m_offset = hasLocalVarChangedOffset;
                sym->m_allocated = true;
                IR::Opnd* opnd1 = IR::SymOpnd::New(sym, TyInt8, m_func);
                IR::Opnd* opnd2 = IR::IntConstOpnd::New(0, TyInt8, m_func);
                InsertCompareBranch(opnd1, opnd2, Js::OpCode::BrNeq_A, bailOutLabel, continueBranchInstr);
            }
            bailOutKind ^= IR::BailOutLocalValueChanged;
        }

        if (doGenerateBailOutDocumentBlock)
        {
            // GENERATE the BailoutDocumentLabel
            // bailOutDocumentLabel:
            //   CMP CurrentScriptId, [&stepController->ScriptIdWhenSet]
            //   BEQ ContinueLabel
            // bailOutLabel:                // (fallthrough bailOutLabel)
            Js::FunctionBody* body = m_func->GetJnFunction();
            IR::Opnd* opnd1 = IR::MemRefOpnd::New(body->GetAddressOfScriptId(), TyInt32, m_func);

            IR::Opnd* opnd2 = IR::MemRefOpnd::New(debugManager->stepController.GetAddressOfScriptIdWhenSet(), TyInt32, m_func);
            IR::RegOpnd* reg1 = IR::RegOpnd::New(TyInt32, m_func);
            InsertMove(reg1, opnd2, bailOutLabel);

            InsertCompareBranch(opnd1, reg1, Js::OpCode::BrEq_A, /*isUnsigned*/ true, continueLabel, bailOutLabel);
        }

        AssertMsg(bailOutKind == (IR::BailOutKind)0, "Some of the bits in BailOutKind were not processed!");

        // Note: at this time the 'instr' is in between bailoutLabel and continueLabel.
    }
    else
    {
        // For explicit/unconditional bailout use label which is not a helper, otherwise we would get a helper in main code path
        // which breaks helper label consistency (you can only get to helper from a conditional branch in main code), see DbCheckPostLower.
        bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, false);
    }

    this->GenerateBailOut(instr, nullptr, bailOutLabel);

    return prevInstr;
}

IR::Instr*
Lowerer::LowerBailOnException(IR::Instr * instr)
{
    Assert(instr->HasBailOutInfo());
    IR::Instr * instrPrev = instr->m_prev;
    Assert(instrPrev->m_opcode == Js::OpCode::Catch);

    this->GenerateBailOut(instr, nullptr, nullptr);

    return instrPrev;
}

// Generate BailOut Lowerer Instruction if the value is INT_MIN.
// It it's not INT_MIN, we continue without bailout.
IR::Instr *
Lowerer::LowerBailOnIntMin(IR::Instr *instr, IR::BranchInstr *branchInstr  /* = nullptr */, IR::LabelInstr  *labelBailOut /* = nullptr */)
{
    Assert(instr);
    Assert(instr->GetSrc1());
    IR::LabelInstr *continueLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    instr->InsertAfter(continueLabelInstr);

    if(!instr->HasBailOutInfo())
    {
        instr->Remove();
    }
    else
    {
        Assert(instr->GetBailOutKind() == IR::BailOnIntMin);
        // Note: src1 must be int32 at this point.
        if (instr->GetSrc1()->IsIntConstOpnd())
        {
            // For consts we can check the value at JIT time. Note: without this check we'll have to legalize the CMP instr.
            IR::IntConstOpnd* intConst = instr->UnlinkSrc1()->AsIntConstOpnd();
            if (intConst->GetValue() == INT_MIN)
            {
                this->GenerateBailOut(instr, branchInstr, labelBailOut);
                intConst->Free(instr->m_func);
            }
            else
            {
                instr->Remove();
            }
        }
        else
        {
            InsertCompareBranch(instr->UnlinkSrc1(), IR::IntConstOpnd::New(INT_MIN, TyInt32, this->m_func), Js::OpCode::BrNeq_A, continueLabelInstr, instr);
            this->GenerateBailOut(instr, branchInstr, labelBailOut);
        }
    }

    return continueLabelInstr;
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerBailOnNotString
///      Generate BailOut Lowerer Instruction if not a String
///
///----------------------------------------------------------------------------
void Lowerer::LowerBailOnNotString(IR::Instr *instr)
{
    if (!instr->GetSrc1()->GetValueType().IsString())
    {
        /*Creating a MOV instruction*/
        IR::Instr * movInstr = IR::Instr::New(instr->m_opcode, instr->UnlinkDst(), instr->UnlinkSrc1(), instr->m_func);
        instr->InsertBefore(movInstr);

        IR::LabelInstr *continueLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func);
        IR::LabelInstr *helperLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
        instr->InsertAfter(continueLabelInstr);

        IR::RegOpnd *srcReg = movInstr->GetSrc1()->IsRegOpnd() ? movInstr->GetSrc1()->AsRegOpnd() : nullptr;

        this->GenerateStringTest(srcReg, instr, helperLabelInstr, continueLabelInstr);
        this->GenerateBailOut(instr, nullptr, helperLabelInstr);
    }
    else
    {
        instr->ClearBailOutInfo();
    }
}

void Lowerer::LowerOneBailOutKind(
    IR::Instr *const instr,
    const IR::BailOutKind bailOutKindToLower,
    const bool isInHelperBlock,
    const bool preserveBailOutKindInInstr)
{
    Assert(instr);
    Assert(bailOutKindToLower);
    Assert(!(bailOutKindToLower & IR::BailOutKindBits) || !(bailOutKindToLower & bailOutKindToLower - 1u));

    Func *const func = instr->m_func;

    // Split bailouts other than the one being lowered here
    BailOutInfo *const bailOutInfo = instr->GetBailOutInfo();
    IR::BailOutKind bailOutKind = instr->GetBailOutKind();
    Assert(
        bailOutKindToLower & IR::BailOutKindBits
            ? bailOutKind & bailOutKindToLower
            : (bailOutKind & ~IR::BailOutKindBits) == bailOutKindToLower);
    if(!preserveBailOutKindInInstr)
    {
        bailOutKind -= bailOutKindToLower;
    }
    if(bailOutKind)
    {
        if(bailOutInfo->bailOutInstr == instr)
        {
            // Create a shared bailout point for the split bailout checks
            IR::Instr *const sharedBail = instr->ShareBailOut();
            Assert(sharedBail->GetBailOutInfo() == bailOutInfo);
            GenerateBailOut(sharedBail);
        }
        instr->SetBailOutKind(bailOutKind);
    }
    else
    {
        instr->UnlinkBailOutInfo();
        if(bailOutInfo->bailOutInstr == instr)
        {
            bailOutInfo->bailOutInstr = nullptr;
        }
    }

    IR::Instr *const insertBeforeInstr = instr->m_next;

    //     (Bail out with the requested bail out kind)
    IR::BailOutInstr *const bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOut, bailOutKindToLower, bailOutInfo, func);
    bailOutInstr->SetByteCodeOffset(instr);
    insertBeforeInstr->InsertBefore(bailOutInstr);
    GenerateBailOut(bailOutInstr);

    // The caller is expected to generate code to decide whether to bail out
}

void Lowerer::SplitBailOnNotArray(
    IR::Instr *const instr,
    IR::Instr * *const bailOnNotArrayRef,
    IR::Instr * *const bailOnMissingValueRef)
{
    Assert(instr);
    Assert(!instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc1()->IsRegOpnd());
    Assert(!instr->GetSrc2());
    Assert(bailOnNotArrayRef);
    Assert(bailOnMissingValueRef);

    IR::Instr *&bailOnNotArray = *bailOnNotArrayRef;
    IR::Instr *&bailOnMissingValue = *bailOnMissingValueRef;

    bailOnNotArray = instr;
    bailOnMissingValue = nullptr;

    IR::BailOutKind bailOutKind = instr->GetBailOutKind();
    if(bailOutKind == IR::BailOutOnNotArray ||
       bailOutKind == IR::BailOutOnNotNativeArray)
    {
        return;
    }

    // Split array checks
    BailOutInfo *const bailOutInfo = instr->GetBailOutInfo();
    if(bailOutInfo->bailOutInstr == instr)
    {
        // Create a shared bailout point for the split bailout checks
        IR::Instr *const sharedBail = instr->ShareBailOut();
        Assert(sharedBail->GetBailOutInfo() == bailOutInfo);
        LowerBailTarget(sharedBail);
    }
    bailOutKind -= IR::BailOutOnMissingValue;
    Assert(bailOutKind == IR::BailOutOnNotArray ||
           bailOutKind == IR::BailOutOnNotNativeArray);
    instr->SetBailOutKind(bailOutKind);

    Func *const func = bailOutInfo->bailOutFunc;
    IR::Instr *const insertBeforeInstr = instr->m_next;

    // Split missing value checks
    bailOnMissingValue = IR::BailOutInstr::New(Js::OpCode::BailOnNotArray, IR::BailOutOnMissingValue, bailOutInfo, func);
    bailOnMissingValue->SetByteCodeOffset(instr);
    insertBeforeInstr->InsertBefore(bailOnMissingValue);
}

IR::RegOpnd *Lowerer::LowerBailOnNotArray(IR::Instr *const instr)
{
    Assert(instr);
    Assert(!instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc1()->IsRegOpnd());
    Assert(!instr->GetSrc2());

    Func *const func = instr->m_func;

    // Label to jump to (or fall through to) when bailing out
    const auto bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true /* isOpHelper */);
    instr->InsertBefore(bailOutLabel);

    // Label to jump to when not bailing out
    const auto skipBailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    instr->InsertAfter(skipBailOutLabel);

    // Do the array tests and jump to bailOutLabel if it's not an array. Fall through if it is an array.
    IR::RegOpnd *const arrayOpnd =
        GenerateArrayTest(instr->UnlinkSrc1()->AsRegOpnd(), bailOutLabel, bailOutLabel, bailOutLabel, true);

    // Skip bail-out when it is an array
    InsertBranch(Js::OpCode::Br, skipBailOutLabel, bailOutLabel);

    // Generate the bailout helper call. 'instr' will be changed to the CALL into the bailout function, so it can't be used for
    // ordering instructions anymore.
    GenerateBailOut(instr);

    return arrayOpnd;
}

void Lowerer::LowerBailOnMissingValue(IR::Instr *const instr, IR::RegOpnd *const arrayOpnd)
{
    Assert(instr);
    Assert(!instr->GetDst());
    Assert(!instr->GetSrc1());
    Assert(!instr->GetSrc2());
    Assert(arrayOpnd);
    Assert(arrayOpnd->GetValueType().IsArrayOrObjectWithArray());

    Func *const func = instr->m_func;

    // Label to jump to when not bailing out
    const auto skipBailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    instr->InsertAfter(skipBailOutLabel);

    // Skip bail-out when the array has no missing values
    //
    // test [array + offsetOf(objectArrayOrFlags)], Js::DynamicObjectFlags::HasNoMissingValues
    // jnz  $skipBailOut
    const IR::AutoReuseOpnd autoReuseArrayOpnd(arrayOpnd, func);
    CompileAssert(
        static_cast<Js::DynamicObjectFlags>(static_cast<uint8>(Js::DynamicObjectFlags::HasNoMissingValues)) ==
        Js::DynamicObjectFlags::HasNoMissingValues);
    InsertTestBranch(
        IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfArrayFlags(), TyUint8, func),
        IR::IntConstOpnd::New(static_cast<uint8>(Js::DynamicObjectFlags::HasNoMissingValues), TyUint8, func, true),
        Js::OpCode::BrNeq_A,
        skipBailOutLabel,
        instr);

    // Generate the bailout helper call. 'instr' will be changed to the CALL into the bailout function, so it can't be used for
    // ordering instructions anymore.
    GenerateBailOut(instr);
}

void Lowerer::LowerBailOnInvalidatedArrayHeadSegment(IR::Instr *const instr, const bool isInHelperBlock)
{
    /*
        // Generate checks for whether the head segment or the head segment length changed during the helper call

        if(!(baseValueType.IsArrayOrObjectWithArray() && arrayOpnd && arrayOpnd.HeadSegmentSym()))
        {
            // Record the array head segment before the helper call
            headSegmentBeforeHelperCall = Js::JavascriptArray::Jit_GetArrayHeadSegmentForArrayOrObjectWithArray(base)
        }
        if(!(baseValueType.IsArrayOrObjectWithArray() && arrayOpnd && arrayOpnd.HeadSegmentLengthSym()))
        {
            // Record the array head segment length before the helper call
            if(baseValueType.IsArrayOrObjectWithArray() && arrayOpnd && arrayOpnd.HeadSegmentSym())
            {
                mov headSegmentLengthBeforeHelperCall, [headSegmentBeforeHelperCall + offsetOf(length)]
            }
            else
            {
                headSegmentLengthBeforeHelperCall =
                    Js::JavascriptArray::Jit_GetArrayHeadSegmentLength(headSegmentBeforeHelperCall)
            }
        }

        helperCall:
            (Helper call and other bailout checks)

        // If the array has a different head segment or head segment length after the helper call, then this store needs to bail
        // out
        invalidatedHeadSegment =
            JavascriptArray::Jit_OperationInvalidatedArrayHeadSegment(
                headSegmentBeforeHelperCall,
                headSegmentLengthBeforeHelperCall,
                base)
        test invalidatedHeadSegment, invalidatedHeadSegment
        jz $skipBailOut

        (Bail out with IR::BailOutOnInvalidatedArrayHeadSegment)

        $skipBailOut:
    */

    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict || instr->m_opcode == Js::OpCode::Memset || instr->m_opcode == Js::OpCode::Memcopy);
    Assert(instr->GetDst());
    Assert(instr->GetDst()->IsIndirOpnd());

    Func *const func = instr->m_func;

    IR::RegOpnd *const baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    const ValueType baseValueType(baseOpnd->GetValueType());
    Assert(!baseValueType.IsNotArrayOrObjectWithArray());
    const bool isArrayOrObjectWithArray = baseValueType.IsArrayOrObjectWithArray();
    IR::ArrayRegOpnd *const arrayOpnd = baseOpnd->IsArrayRegOpnd() ? baseOpnd->AsArrayRegOpnd() : nullptr;

    IR::RegOpnd *headSegmentBeforeHelperCallOpnd;
    IR::AutoReuseOpnd autoReuseHeadSegmentBeforeHelperCallOpnd;
    if(isArrayOrObjectWithArray && arrayOpnd && arrayOpnd->HeadSegmentSym())
    {
        headSegmentBeforeHelperCallOpnd = IR::RegOpnd::New(arrayOpnd->HeadSegmentSym(), TyMachPtr, func);
        autoReuseHeadSegmentBeforeHelperCallOpnd.Initialize(headSegmentBeforeHelperCallOpnd, func);
    }
    else
    {
        // Record the array head segment before the helper call
        //     headSegmentBeforeHelperCall = Js::JavascriptArray::Jit_GetArrayHeadSegmentForArrayOrObjectWithArray(base)
        m_lowererMD.LoadHelperArgument(instr, baseOpnd);
        IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
        headSegmentBeforeHelperCallOpnd = IR::RegOpnd::New(StackSym::New(TyMachPtr, func), TyMachPtr, func);
        autoReuseHeadSegmentBeforeHelperCallOpnd.Initialize(headSegmentBeforeHelperCallOpnd, func);
        callInstr->SetDst(headSegmentBeforeHelperCallOpnd);
        instr->InsertBefore(callInstr);
        m_lowererMD.ChangeToHelperCall(callInstr, IR::HelperArray_Jit_GetArrayHeadSegmentForArrayOrObjectWithArray);
    }

    IR::RegOpnd *headSegmentLengthBeforeHelperCallOpnd;
    IR::AutoReuseOpnd autoReuseHeadSegmentLengthBeforeHelperCallOpnd;
    if(isArrayOrObjectWithArray && arrayOpnd && arrayOpnd->HeadSegmentLengthSym())
    {
        headSegmentLengthBeforeHelperCallOpnd = IR::RegOpnd::New(arrayOpnd->HeadSegmentLengthSym(), TyUint32, func);
        autoReuseHeadSegmentLengthBeforeHelperCallOpnd.Initialize(headSegmentLengthBeforeHelperCallOpnd, func);
    }
    else
    {
        headSegmentLengthBeforeHelperCallOpnd = IR::RegOpnd::New(StackSym::New(TyUint32, func), TyUint32, func);
        autoReuseHeadSegmentLengthBeforeHelperCallOpnd.Initialize(headSegmentLengthBeforeHelperCallOpnd, func);
        if(isArrayOrObjectWithArray && arrayOpnd && arrayOpnd->HeadSegmentSym())
        {
            // Record the array head segment length before the helper call
            //     mov headSegmentLengthBeforeHelperCall, [headSegmentBeforeHelperCall + offsetOf(length)]
            InsertMove(
                headSegmentLengthBeforeHelperCallOpnd,
                IR::IndirOpnd::New(
                    headSegmentBeforeHelperCallOpnd,
                    Js::SparseArraySegmentBase::GetOffsetOfLength(),
                    TyUint32,
                    func),
                instr);
        }
        else
        {
            // Record the array head segment length before the helper call
            //     headSegmentLengthBeforeHelperCall =
            //         Js::JavascriptArray::Jit_GetArrayHeadSegmentLength(headSegmentBeforeHelperCall)
            m_lowererMD.LoadHelperArgument(instr, headSegmentBeforeHelperCallOpnd);
            IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
            callInstr->SetDst(headSegmentLengthBeforeHelperCallOpnd);
            instr->InsertBefore(callInstr);
            m_lowererMD.ChangeToHelperCall(callInstr, IR::HelperArray_Jit_GetArrayHeadSegmentLength);
        }
    }

    IR::LabelInstr *const skipBailOutLabel = instr->GetOrCreateContinueLabel(isInHelperBlock);
    LowerOneBailOutKind(instr, IR::BailOutOnInvalidatedArrayHeadSegment, isInHelperBlock);
    IR::Instr *const insertBeforeInstr = instr->m_next;

    // If the array has a different head segment or head segment length after the helper call, then this store needs to bail out
    //     invalidatedHeadSegment =
    //         JavascriptArray::Jit_OperationInvalidatedArrayHeadSegment(
    //             headSegmentBeforeHelperCall,
    //             headSegmentLengthBeforeHelperCall,
    //             base)
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, baseOpnd);
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, headSegmentLengthBeforeHelperCallOpnd);
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, headSegmentBeforeHelperCallOpnd);
    IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
    IR::RegOpnd *const invalidatedHeadSegmentOpnd = IR::RegOpnd::New(TyUint8, func);
    const IR::AutoReuseOpnd autoReuseInvalidatedHeadSegmentOpnd(invalidatedHeadSegmentOpnd, func);
    callInstr->SetDst(invalidatedHeadSegmentOpnd);
    insertBeforeInstr->InsertBefore(callInstr);
    m_lowererMD.ChangeToHelperCall(callInstr, IR::HelperArray_Jit_OperationInvalidatedArrayHeadSegment);

    //     test invalidatedHeadSegment, invalidatedHeadSegment
    //     jz $skipBailOut
    InsertTestBranch(
        invalidatedHeadSegmentOpnd,
        invalidatedHeadSegmentOpnd,
        Js::OpCode::BrEq_A,
        skipBailOutLabel,
        insertBeforeInstr);

    //     (Bail out with IR::BailOutOnInvalidatedArrayHeadSegment)
    //     $skipBailOut:
}

void Lowerer::LowerBailOnInvalidatedArrayLength(IR::Instr *const instr, const bool isInHelperBlock)
{
    /*
        // Generate checks for whether the length changed during the helper call

        if(!(arrayOpnd && arrayOpnd.LengthSym() && arrayOpnd.LengthSym() != arrayOpnd.HeadSegmentLengthSym()))
        {
            // Record the array length before the helper call
            lengthBeforeHelperCall = Js::JavascriptArray::Jit_GetArrayLength(base)
        }

        helperCall:
            (Helper call and other bailout checks)

        // If the array has a different length after the helper call, then this store needs to bail out
        invalidatedLength = JavascriptArray::Jit_OperationInvalidatedArrayLength(lengthBeforeHelperCall, base)
        test invalidatedLength, invalidatedLength
        jz $skipBailOut

        (Bail out with IR::BailOutOnInvalidatedArrayLength)

        $skipBailOut:
    */

    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict || instr->m_opcode == Js::OpCode::Memset || instr->m_opcode == Js::OpCode::Memcopy);
    Assert(instr->GetDst());
    Assert(instr->GetDst()->IsIndirOpnd());

    Func *const func = instr->m_func;

    IR::RegOpnd *const baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    const ValueType baseValueType(baseOpnd->GetValueType());
    Assert(!baseValueType.IsNotArray());
    IR::ArrayRegOpnd *const arrayOpnd = baseOpnd->IsArrayRegOpnd() ? baseOpnd->AsArrayRegOpnd() : nullptr;

    IR::RegOpnd *lengthBeforeHelperCallOpnd;
    IR::AutoReuseOpnd autoReuseLengthBeforeHelperCallOpnd;
    if(arrayOpnd && arrayOpnd->LengthSym() && arrayOpnd->LengthSym() != arrayOpnd->HeadSegmentLengthSym())
    {
        lengthBeforeHelperCallOpnd = IR::RegOpnd::New(arrayOpnd->LengthSym(), arrayOpnd->LengthSym()->GetType(), func);
        autoReuseLengthBeforeHelperCallOpnd.Initialize(lengthBeforeHelperCallOpnd, func);
    }
    else
    {
        // Record the array length before the helper call
        //     lengthBeforeHelperCall = Js::JavascriptArray::Jit_GetArrayLength(base)
        m_lowererMD.LoadHelperArgument(instr, baseOpnd);
        IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
        lengthBeforeHelperCallOpnd = IR::RegOpnd::New(TyUint32, func);
        autoReuseLengthBeforeHelperCallOpnd.Initialize(lengthBeforeHelperCallOpnd, func);
        callInstr->SetDst(lengthBeforeHelperCallOpnd);
        instr->InsertBefore(callInstr);
        m_lowererMD.ChangeToHelperCall(callInstr, IR::HelperArray_Jit_GetArrayLength);
    }

    IR::LabelInstr *const skipBailOutLabel = instr->GetOrCreateContinueLabel(isInHelperBlock);
    LowerOneBailOutKind(instr, IR::BailOutOnInvalidatedArrayLength, isInHelperBlock);
    IR::Instr *const insertBeforeInstr = instr->m_next;

    // If the array has a different length after the helper call, then this store needs to bail out
    //     invalidatedLength = JavascriptArray::Jit_OperationInvalidatedArrayLength(lengthBeforeHelperCall, base)
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, baseOpnd);
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, lengthBeforeHelperCallOpnd);
    IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
    IR::RegOpnd *const invalidatedLengthOpnd = IR::RegOpnd::New(TyUint8, func);
    const IR::AutoReuseOpnd autoReuseInvalidatedLengthOpnd(invalidatedLengthOpnd, func);
    callInstr->SetDst(invalidatedLengthOpnd);
    insertBeforeInstr->InsertBefore(callInstr);
    m_lowererMD.ChangeToHelperCall(callInstr, IR::HelperArray_Jit_OperationInvalidatedArrayLength);

    //     test invalidatedLength, invalidatedLength
    //     jz $skipBailOut
    InsertTestBranch(
        invalidatedLengthOpnd,
        invalidatedLengthOpnd,
        Js::OpCode::BrEq_A,
        skipBailOutLabel,
        insertBeforeInstr);

    //     (Bail out with IR::BailOutOnInvalidatedArrayLength)
    //     $skipBailOut:
}

void Lowerer::LowerBailOnCreatedMissingValue(IR::Instr *const instr, const bool isInHelperBlock)
{
    /*
        // Generate checks for whether the first missing value was created during the helper call

        if(!(baseValueType.IsArrayOrObjectWithArray() && baseValueType.HasNoMissingValues()))
        {
            // Record whether the array has missing values before the helper call
            arrayFlagsBeforeHelperCall = Js::JavascriptArray::Jit_GetArrayFlagsForArrayOrObjectWithArray(base)
        }

        helperCall:
            (Helper call and other bailout checks)

        // If the array had no missing values before the helper call, and the array has missing values after the helper
        // call, then this store created the first missing value in the array and needs to bail out
        if(baseValueType.IsArrayOrObjectWithArray() && baseValueType.HasNoMissingValues())
            (arrayFlagsBeforeHelperCall = Js::DynamicObjectFlags::HasNoMissingValues)
        createdFirstMissingValue = JavascriptArray::Jit_OperationCreatedFirstMissingValue(arrayFlagsBeforeHelperCall, base)
        test createdFirstMissingValue, createdFirstMissingValue
        jz $skipBailOut

        (Bail out with IR::BailOutOnMissingValue)

        $skipBailOut:
    */

    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict || instr->m_opcode == Js::OpCode::Memset || instr->m_opcode == Js::OpCode::Memcopy);
    Assert(instr->GetDst());
    Assert(instr->GetDst()->IsIndirOpnd());

    Func *const func = instr->m_func;

    IR::RegOpnd *const baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    const ValueType baseValueType(baseOpnd->GetValueType());
    Assert(!baseValueType.IsNotArrayOrObjectWithArray());

    IR::Opnd *arrayFlagsBeforeHelperCallOpnd = nullptr;
    IR::AutoReuseOpnd autoReuseArrayFlagsBeforeHelperCallOpnd;
    const IRType arrayFlagsType = sizeof(uintptr_t) == sizeof(uint32) ? TyUint32 : TyUint64;
    if(!(baseValueType.IsArrayOrObjectWithArray() && baseValueType.HasNoMissingValues()))
    {
        // Record whether the array has missing values before the helper call
        //     arrayFlagsBeforeHelperCall = Js::JavascriptArray::Jit_GetArrayFlagsForArrayOrObjectWithArray(base)
        m_lowererMD.LoadHelperArgument(instr, baseOpnd);
        IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
        arrayFlagsBeforeHelperCallOpnd = IR::RegOpnd::New(arrayFlagsType, func);
        autoReuseArrayFlagsBeforeHelperCallOpnd.Initialize(arrayFlagsBeforeHelperCallOpnd, func);
        callInstr->SetDst(arrayFlagsBeforeHelperCallOpnd);
        instr->InsertBefore(callInstr);
        m_lowererMD.ChangeToHelperCall(callInstr, IR::HelperArray_Jit_GetArrayFlagsForArrayOrObjectWithArray);
    }

    IR::LabelInstr *const skipBailOutLabel = instr->GetOrCreateContinueLabel(isInHelperBlock);
    LowerOneBailOutKind(instr, IR::BailOutOnMissingValue, isInHelperBlock);
    IR::Instr *const insertBeforeInstr = instr->m_next;

    // If the array had no missing values before the helper call, and the array has missing values after the helper
    // call, then this store created the first missing value in the array and needs to bail out

    if(baseValueType.IsArrayOrObjectWithArray() && baseValueType.HasNoMissingValues())
    {
        //     (arrayFlagsBeforeHelperCall = Js::DynamicObjectFlags::HasNoMissingValues)
        Assert(!arrayFlagsBeforeHelperCallOpnd);
        arrayFlagsBeforeHelperCallOpnd =
            arrayFlagsType == TyUint32
                ? static_cast<IR::Opnd *>(
                    IR::IntConstOpnd::New(
                        static_cast<uintptr_t>(Js::DynamicObjectFlags::HasNoMissingValues),
                        arrayFlagsType,
                        func,
                        true))
                : IR::AddrOpnd::New(
                    reinterpret_cast<void *>(Js::DynamicObjectFlags::HasNoMissingValues),
                    IR::AddrOpndKindConstantVar,
                    func,
                    true);
        autoReuseArrayFlagsBeforeHelperCallOpnd.Initialize(arrayFlagsBeforeHelperCallOpnd, func);
    }
    else
    {
        Assert(arrayFlagsBeforeHelperCallOpnd);
    }

    //     createdFirstMissingValue = JavascriptArray::Jit_OperationCreatedFirstMissingValue(arrayFlagsBeforeHelperCall, base)
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, baseOpnd);
    m_lowererMD.LoadHelperArgument(insertBeforeInstr, arrayFlagsBeforeHelperCallOpnd);
    IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
    IR::RegOpnd *const createdFirstMissingValueOpnd = IR::RegOpnd::New(TyUint8, func);
    IR::AutoReuseOpnd autoReuseCreatedFirstMissingValueOpnd(createdFirstMissingValueOpnd, func);
    callInstr->SetDst(createdFirstMissingValueOpnd);
    insertBeforeInstr->InsertBefore(callInstr);
    m_lowererMD.ChangeToHelperCall(callInstr, IR::HelperArray_Jit_OperationCreatedFirstMissingValue);

    //     test createdFirstMissingValue, createdFirstMissingValue
    //     jz $skipBailOut
    InsertCompareBranch(
        createdFirstMissingValueOpnd,
        IR::IntConstOpnd::New(0, createdFirstMissingValueOpnd->GetType(), func, true),
        Js::OpCode::BrEq_A,
        skipBailOutLabel,
        insertBeforeInstr);

    //     (Bail out with IR::BailOutOnMissingValue)
    //     $skipBailOut:
}

void Lowerer::LowerBoundCheck(IR::Instr *const instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::BoundCheck || instr->m_opcode == Js::OpCode::UnsignedBoundCheck);

#if DBG
    if(instr->m_opcode == Js::OpCode::UnsignedBoundCheck)
    {
        // UnsignedBoundCheck is currently only supported for the pattern:
        //     UnsignedBoundCheck s1 <= s2 + c, where c == 0 || c == -1
        Assert(instr->GetSrc1()->IsRegOpnd());
        Assert(instr->GetSrc1()->IsInt32());
        Assert(instr->GetSrc2());
        Assert(!instr->GetSrc2()->IsIntConstOpnd());
        if(instr->GetDst())
        {
            const int32 c = instr->GetDst()->AsIntConstOpnd()->AsInt32();
            Assert(c == 0 || c == -1);
        }
    }
#endif

    const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
    Assert(
        bailOutKind == IR::BailOutOnArrayAccessHelperCall ||
        bailOutKind == IR::BailOutOnInvalidatedArrayHeadSegment ||
        bailOutKind == IR::BailOutOnFailedHoistedBoundCheck ||
        bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);

    IR::LabelInstr *const skipBailOutLabel = instr->GetOrCreateContinueLabel(false);
    LowerOneBailOutKind(instr, bailOutKind, false);
    Assert(!instr->HasBailOutInfo());
    IR::Instr *insertBeforeInstr = instr->m_next;

#if DBG
    const auto VerifyLeftOrRightOpnd = [&](IR::Opnd *const opnd, const bool isRightOpnd)
    {
        if(!opnd)
        {
            Assert(isRightOpnd);
            return;
        }
        if(opnd->IsIntConstOpnd())
        {
            Assert(!isRightOpnd || opnd->AsIntConstOpnd()->GetValue() != 0);
            return;
        }
        Assert(opnd->GetType() == TyInt32 || opnd->GetType() == TyUint32);
    };
#endif

    // left <= right + offset (src1 <= src2 + dst)
    IR::Opnd *leftOpnd = instr->UnlinkSrc1();
    DebugOnly(VerifyLeftOrRightOpnd(leftOpnd, false));
    IR::Opnd *rightOpnd = instr->UnlinkSrc2();
    DebugOnly(VerifyLeftOrRightOpnd(rightOpnd, true));
    Assert(!leftOpnd->IsIntConstOpnd() || rightOpnd && !rightOpnd->IsIntConstOpnd());
    IR::IntConstOpnd *offsetOpnd = instr->GetDst() ? instr->UnlinkDst()->AsIntConstOpnd() : nullptr;
    Assert(!offsetOpnd || offsetOpnd->GetValue() != 0);
    const bool doUnsignedCompare = instr->m_opcode == Js::OpCode::UnsignedBoundCheck;
    instr->Remove();

    Func *const func = insertBeforeInstr->m_func;

    IntConstType offset = offsetOpnd ? offsetOpnd->GetValue() : 0;
    Js::OpCode compareOpCode = Js::OpCode::BrLe_A;
    if(leftOpnd->IsIntConstOpnd() && rightOpnd->IsRegOpnd() && offset != IntConstMin)
    {
        // Put the constants together: swap the operands, negate the offset, and invert the branch
        IR::Opnd *const tempOpnd = leftOpnd;
        leftOpnd = rightOpnd;
        rightOpnd = tempOpnd;
        offset = -offset;
        compareOpCode = Js::OpCode::BrGe_A;
    }

    if(rightOpnd->IsIntConstOpnd())
    {
        // Try to aggregate right + offset into a constant offset
        IntConstType newOffset;
        if(!IntConstMath::Add(offset, rightOpnd->AsIntConstOpnd()->GetValue(), &newOffset))
        {
            offset = newOffset;
            rightOpnd = nullptr;
            offsetOpnd = nullptr;
        }
    }

    // Determine if the Add for (right + offset) is necessary, and the op code that will be used for the comparison
    IR::AutoReuseOpnd autoReuseAddResultOpnd;
    if(offset == -1 && compareOpCode == Js::OpCode::BrLe_A)
    {
        offset = 0;
        compareOpCode = Js::OpCode::BrLt_A;
    }
    else if(offset == 1 && compareOpCode == Js::OpCode::BrGe_A)
    {
        offset = 0;
        compareOpCode = Js::OpCode::BrGt_A;
    }
    else if(offset != 0 && rightOpnd)
    {
        // Need to Add (right + offset). If it overflows, bail out.
        IR::LabelInstr *const bailOutLabel = insertBeforeInstr->m_prev->GetOrCreateContinueLabel(true);
        insertBeforeInstr = bailOutLabel;

        //     mov  temp, right
        //     add  temp, offset
        //     jo   $bailOut
        // $bailOut: (insertBeforeInstr)
        Assert(!offsetOpnd || offsetOpnd->GetValue() == offset);
        IR::RegOpnd *const addResultOpnd = IR::RegOpnd::New(TyMachReg, func);
        autoReuseAddResultOpnd.Initialize(addResultOpnd, func);
        InsertAdd(
            true,
            addResultOpnd,
            rightOpnd,
            offsetOpnd ? offsetOpnd : IR::IntConstOpnd::New(offset, TyMachReg, func, true),
            insertBeforeInstr);
        InsertBranch(LowererMD::MDOverflowBranchOpcode, bailOutLabel, insertBeforeInstr);

        rightOpnd = addResultOpnd;
    }

    //     cmp  left, right
    //     jl[e] $skipBailOut
    // $bailOut:
    if(!rightOpnd)
    {
        rightOpnd = IR::IntConstOpnd::New(offset, TyInt32, func, true);
    }
    InsertCompareBranch(leftOpnd, rightOpnd, compareOpCode, doUnsignedCompare, skipBailOutLabel, insertBeforeInstr);
}

IR::Instr *
Lowerer::LowerBailTarget(IR::Instr * instr)
{
    // this is just a bailout target, just skip over it and generate a label before so other bailout can jump here.
    IR::Instr * prevInstr = instr->m_prev;

    IR::LabelInstr * continueLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    instr->InsertAfter(continueLabelInstr);

    IR::BranchInstr * skipInstr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, continueLabelInstr, this->m_func);
    instr->InsertBefore(skipInstr);

    this->GenerateBailOut(instr);

    return prevInstr;
}

IR::Instr *
Lowerer::SplitBailOnImplicitCall(IR::Instr *& instr)
{
    Assert(instr->IsPlainInstr() || instr->IsProfiledInstr());

    const auto bailOutKind = instr->GetBailOutKind();
    Assert(
        BailOutInfo::IsBailOutOnImplicitCalls(bailOutKind) ||
        bailOutKind == IR::BailOutOnLossyToInt32ImplicitCalls ||
        bailOutKind == IR::BailOutExpectingObject);

    IR::Opnd * implicitCallFlags = this->GetImplicitCallFlagsOpnd();
    const IR::AutoReuseOpnd autoReuseImplicitCallFlags(implicitCallFlags, instr->m_func);
    IR::IntConstOpnd * noImplicitCall = IR::IntConstOpnd::New(Js::ImplicitCall_None, TyInt8, this->m_func, true);
    const IR::AutoReuseOpnd autoReuseNoImplicitCall(noImplicitCall, instr->m_func);

    // Reset the implicit call flag on every helper call
    LowererMD::CreateAssign(implicitCallFlags, noImplicitCall, instr);

    IR::Instr *disableImplicitCallsInstr = nullptr, *enableImplicitCallsInstr = nullptr;
    if(bailOutKind == IR::BailOutOnLossyToInt32ImplicitCalls ||
       bailOutKind == IR::BailOutOnImplicitCallsPreOp)
    {
        const auto disableImplicitCallAddress =
            m_lowererMD.GenerateMemRef(
                instr->m_func->GetScriptContext()->GetThreadContext()->GetAddressOfDisableImplicitFlags(),
                TyInt8,
                instr);

        // Disable implicit calls since they will be called after bailing out
        disableImplicitCallsInstr =
            IR::Instr::New(
                Js::OpCode::Ld_A,
                disableImplicitCallAddress,
                IR::IntConstOpnd::New(
                    // LossyToInt32 is a special case where we need to disable exceptions because the helper can throw where the interpreter wouldn't
                    bailOutKind == IR::BailOutOnLossyToInt32ImplicitCalls ? DisableImplicitCallAndExceptionFlag : DisableImplicitCallFlag,
                    TyInt8, instr->m_func, true),
                instr->m_func);
        instr->InsertBefore(disableImplicitCallsInstr);

        // Create instruction for re-enabling implicit calls
        enableImplicitCallsInstr =
            IR::Instr::New(
                Js::OpCode::Ld_A,
                disableImplicitCallAddress,
                IR::IntConstOpnd::New(DisableImplicitNoFlag, TyInt8, instr->m_func, true),
                instr->m_func);
    }

    IR::Instr * bailOutInstr = instr;

    instr = IR::Instr::New(instr->m_opcode, instr->m_func);
    bailOutInstr->TransferTo(instr);
    bailOutInstr->InsertBefore(instr);

    if(disableImplicitCallsInstr)
    {
        // Re-enable implicit calls
        Assert(enableImplicitCallsInstr);
        bailOutInstr->InsertBefore(enableImplicitCallsInstr);

        // Lower both instructions. Lowering an instruction may free the instruction's original operands, so do that last.
        LowererMD::ChangeToAssign(disableImplicitCallsInstr);
        LowererMD::ChangeToAssign(enableImplicitCallsInstr);
    }

    bailOutInstr->m_opcode = Js::OpCode::BailOnNotEqual;

    bailOutInstr->SetSrc1(implicitCallFlags);
    bailOutInstr->SetSrc2(noImplicitCall);

    return bailOutInstr;
}

IR::Instr *
Lowerer::SplitBailOnImplicitCall(IR::Instr * instr, IR::Instr * helperCall, IR::Instr * insertBeforeInstr)
{
    IR::Opnd * implicitCallFlags = this->GetImplicitCallFlagsOpnd();
    const IR::AutoReuseOpnd autoReuseImplicitCallFlags(implicitCallFlags, instr->m_func);
    IR::IntConstOpnd * noImplicitCall = IR::IntConstOpnd::New(Js::ImplicitCall_None, TyInt8, this->m_func, true);
    const IR::AutoReuseOpnd autoReuseNoImplicitCall(noImplicitCall, instr->m_func);

    // Reset the implicit call flag on every helper call
    LowererMD::CreateAssign(implicitCallFlags, noImplicitCall, helperCall->m_prev);

    BailOutInfo * bailOutInfo = instr->GetBailOutInfo();
    if (bailOutInfo->bailOutInstr == instr)
    {
        bailOutInfo->bailOutInstr = nullptr;
    }
    IR::Instr * bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, IR::BailOutOnImplicitCalls, bailOutInfo, bailOutInfo->bailOutFunc);
    bailOutInstr->SetSrc1(implicitCallFlags);
    bailOutInstr->SetSrc2(noImplicitCall);

    insertBeforeInstr->InsertBefore(bailOutInstr);
    instr->ClearBailOutInfo();
    return bailOutInstr;
}

// Split out bailout for debugger into separate bailout instr out of real instr which has bailout for debugger.
// Returns the instr which needs to lower next, which would normally be last of splitted instr.
// IR on input:
// - Real instr with BailOutInfo but it's opcode is not BailForDebugger.
//   - debugger bailout is not shared. In this case we'll have debugger bailout in instr->GetBailOutKind().
//   - debugger bailout is shared. In this case we'll have debugger bailout in instr->GetAuxBailOutKind().
// IR on output:
// - Either of:
//   - real instr, then debuggerBailout -- in case we only had debugger bailout.
//   - real instr with BailOutInfo w/o debugger bailout, then debuggerBailout, then sharedBailout -- in case bailout for debugger was shared w/some other b.o.
IR::Instr* Lowerer::SplitBailForDebugger(IR::Instr* instr)
{
    Assert(m_func->IsJitInDebugMode() && instr->m_opcode != Js::OpCode::BailForDebugger);

    IR::BailOutKind debuggerBailOutKind;    // Used for splitted instr.
    BailOutInfo* bailOutInfo = instr->GetBailOutInfo();
    IR::Instr* sharedBailoutInstr = nullptr;

    if (instr->GetBailOutKind() & IR::BailOutForDebuggerBits)
    {
        // debugger bailout is not shared.
        Assert(!instr->HasAuxBailOut());
        AssertMsg(!(instr->GetBailOutKind() & ~IR::BailOutForDebuggerBits), "There should only be debugger bailout bits in the instr.");

        debuggerBailOutKind = instr->GetBailOutKind() & IR::BailOutForDebuggerBits;

        // There is no non-debugger bailout in the instr, still can't clear bailout info, as we use it for the splitted instr,
        // but we need to mark the bailout as hasn't been generated yet.
        if (bailOutInfo->bailOutInstr == instr)
        {
            // null will be picked up by following BailOutInstr::New which will change it to new bailout instr.
            bailOutInfo->bailOutInstr = nullptr;
        }

        // Remove bailout info from the original instr which from now on becomes just regular instr, w/o deallocating bailout info.
        instr->ClearBailOutInfo();
    }
    else if (instr->IsBranchInstr() && instr->HasBailOutInfo() && instr->HasAuxBailOut())
    {
        // Branches with shared bailout are lowered in LowerCondBranchCheckBailOut,
        // can't do here because we need to use BranchBailOutRecord but don't know which BrTrue/BrFalse to use for it.
        debuggerBailOutKind = IR::BailOutInvalid;
    }
    else if (instr->HasAuxBailOut() && instr->GetAuxBailOutKind() & IR::BailOutForDebuggerBits)
    {
        // debugger bailout is shared.
        AssertMsg(!(instr->GetBailOutKind() & IR::BailOutForDebuggerBits), "There should be no debugger bits in main bailout kind.");

        debuggerBailOutKind = instr->GetAuxBailOutKind() & IR::BailOutForDebuggerBits;

        // This will insert SharedBail instr after current instr and set bailOutInfo->bailOutInstr to the shared one.
        sharedBailoutInstr = instr->ShareBailOut();

        // As we extracted aux bail out, invalidate all tracks of it in the instr.
        instr->ResetAuxBailOut();
    }
    else
    {
        AssertMsg(FALSE, "shouldn't get here");
        debuggerBailOutKind = IR::BailOutInvalid;
    }

    if (debuggerBailOutKind != IR::BailOutInvalid)
    {
        IR::BailOutInstr* debuggerBailoutInstr = IR::BailOutInstr::New(
            Js::OpCode::BailForDebugger, debuggerBailOutKind, bailOutInfo, bailOutInfo->bailOutFunc);
        instr->InsertAfter(debuggerBailoutInstr);

        // Since we go backwards, we need to process extracted out bailout for debugger first.
        instr = sharedBailoutInstr ? sharedBailoutInstr : debuggerBailoutInstr;
    }

    return instr;
}

IR::Instr *
Lowerer::SplitBailOnResultCondition(IR::Instr *const instr) const
{
    Assert(instr);
    Assert(!instr->IsLowered());
    Assert(
        instr->GetBailOutKind() & IR::BailOutOnResultConditions ||
        instr->GetBailOutKind() == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);

    const auto nonBailOutInstr = IR::Instr::New(instr->m_opcode, instr->m_func);
    instr->TransferTo(nonBailOutInstr);
    instr->InsertBefore(nonBailOutInstr);
    return nonBailOutInstr;
}

void
Lowerer::LowerBailOnResultCondition(
    IR::Instr *const instr,
    IR::LabelInstr * *const bailOutLabel,
    IR::LabelInstr * *const skipBailOutLabel)
{
    Assert(instr);
    Assert(
        instr->GetBailOutKind() & IR::BailOutOnResultConditions ||
        instr->GetBailOutKind() == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);
    Assert(bailOutLabel);
    Assert(skipBailOutLabel);

    // Label to jump to (or fall through to) when bailing out. The actual bailout label
    // (bailOutInfo->bailOutInstr->AsLabelInstr()) may be shared, and code may be added to restore values before the jump to the
    // actual bailout label in the cloned bailout case, so always create a new bailout label for this particular path.
    *bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, true /* isOpHelper */);
    instr->InsertBefore(*bailOutLabel);

    // Label to jump to when not bailing out
    *skipBailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func);
    instr->InsertAfter(*skipBailOutLabel);

    // Generate the bailout helper call. 'instr' will be changed to the CALL into the bailout function, so it can't be used for
    // ordering instructions anymore.
    GenerateBailOut(instr);
}

void
Lowerer::PreserveSourcesForBailOnResultCondition(IR::Instr *const instr, IR::LabelInstr *const skipBailOutLabel) const
{
    Assert(instr);
    Assert(!instr->IsLowered());
    Assert(!instr->HasBailOutInfo());

    // Since this instruction may bail out, writing to the destination cannot overwrite one of the sources, or we may lose one
    // of the sources needed to redo the equivalent byte code instruction. Determine if the sources need to be preserved.

    const auto dst = instr->GetDst();
    Assert(dst);
    const auto dstStackSym = dst->GetStackSym();
    if(!dstStackSym || !dstStackSym->HasByteCodeRegSlot())
    {
        // We only need to ensure that a byte-code source is not being overwritten
        return;
    }

    switch(instr->m_opcode)
    {
        // The sources of these instructions don't need restoring, or will be restored in the bailout path
        case Js::OpCode::Neg_I4:
            // In case of overflow or zero, the result is the same as the operand
        case Js::OpCode::Add_I4:
        case Js::OpCode::Sub_I4:
            // In case of overflow, there is always enough information to restore the operands
            return;
    }

    Assert(instr->GetSrc1());
    if(!dst->IsEqual(instr->GetSrc1()) && !(instr->GetSrc2() && dst->IsEqual(instr->GetSrc2())))
    {
        // The destination is different from the sources
        return;
    }

    // The destination is the same as one of the sources and the original sources cannot be restored after the instruction, so
    // use a temporary destination for the result and move it back to the original destination after deciding not to bail out
    LowererMD::ChangeToAssign(instr->SinkDst(Js::OpCode::Ld_I4, RegNOREG, skipBailOutLabel));
}

void
Lowerer::LowerInstrWithBailOnResultCondition(
    IR::Instr *const instr,
    const IR::BailOutKind bailOutKind,
    IR::LabelInstr *const bailOutLabel,
    IR::LabelInstr *const skipBailOutLabel) const
{
    Assert(instr);
    Assert(!instr->IsLowered());
    Assert(!instr->HasBailOutInfo());
    Assert(bailOutKind & IR::BailOutOnResultConditions || bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);
    Assert(bailOutLabel);
    Assert(instr->m_next == bailOutLabel);
    Assert(skipBailOutLabel);

    // Preserve sources that are overwritten by the instruction if needed
    PreserveSourcesForBailOnResultCondition(instr, skipBailOutLabel);

    // Lower the instruction
    switch(instr->m_opcode)
    {
        case Js::OpCode::Neg_I4:
            LowererMD::LowerInt4NegWithBailOut(instr, bailOutKind, bailOutLabel, skipBailOutLabel);
            break;

        case Js::OpCode::Add_I4:
            LowererMD::LowerInt4AddWithBailOut(instr, bailOutKind, bailOutLabel, skipBailOutLabel);
            break;

        case Js::OpCode::Sub_I4:
            LowererMD::LowerInt4SubWithBailOut(instr, bailOutKind, bailOutLabel, skipBailOutLabel);
            break;

        case Js::OpCode::Mul_I4:
            LowererMD::LowerInt4MulWithBailOut(instr, bailOutKind, bailOutLabel, skipBailOutLabel);
            break;

        case Js::OpCode::Rem_I4:
            m_lowererMD.LowerInt4RemWithBailOut(instr, bailOutKind, bailOutLabel, skipBailOutLabel);
            break;

        default:
            Assert(false); // not implemented
            __assume(false);
    }
}

void
Lowerer::GenerateObjectTestAndTypeLoad(IR::Instr *instrLdSt, IR::RegOpnd *opndBase, IR::RegOpnd *opndType, IR::LabelInstr *labelHelper)
{
    IR::IndirOpnd *opndIndir;

    if (!opndBase->IsNotTaggedValue())
    {
        m_lowererMD.GenerateObjectTest(opndBase, instrLdSt, labelHelper);
    }

    opndIndir = IR::IndirOpnd::New(opndBase, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, this->m_func);
    m_lowererMD.CreateAssign(opndType, opndIndir, instrLdSt);
}

IR::LabelInstr *
Lowerer::GenerateBailOut(IR::Instr * instr, IR::BranchInstr * branchInstr, IR::LabelInstr *bailOutLabel)
{
    BailOutInfo * bailOutInfo = instr->GetBailOutInfo();
    IR::Instr * bailOutInstr = bailOutInfo->bailOutInstr;
    IR::LabelInstr *collectRuntimeStatsLabel = nullptr;
    if (instr->IsCloned())
    {
        Assert(bailOutInstr != instr);

        // jump to the cloned bail out label
        IR::LabelInstr * bailOutLabelInstr = bailOutInstr->AsLabelInstr();
        IR::BranchInstr * bailOutBranch = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, bailOutLabelInstr, this->m_func);
        instr->InsertBefore(bailOutBranch);
        instr->Remove();
        return bailOutLabel;
    }

    if (bailOutInstr != instr)
    {
        // this bailOutInfo is shared, just jump to the bailout target

        // Add helper label to trigger layout.
        collectRuntimeStatsLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        instr->InsertBefore(collectRuntimeStatsLabel);

        IR::MemRefOpnd *pIndexOpndForBailOutKind =
            IR::MemRefOpnd::New((BYTE*)bailOutInfo->bailOutRecord + BailOutRecord::GetOffsetOfBailOutKind(), TyUint32, this->m_func, IR::AddrOpndKindDynamicBailOutKindRef);
        m_lowererMD.CreateAssign(
            pIndexOpndForBailOutKind, IR::IntConstOpnd::New(instr->GetBailOutKind(), pIndexOpndForBailOutKind->GetType(), this->m_func), instr);

        // No point in doing this for BailOutFailedEquivalentTypeCheck or BailOutFailedEquivalentFixedFieldTypeCheck,
        // because the respective inline cache is already polymorphic, anyway.
        if (instr->GetBailOutKind() == IR::BailOutFailedTypeCheck || instr->GetBailOutKind() == IR::BailOutFailedFixedFieldTypeCheck)
        {
            // We have a type check bailout that shares a bailout record with other instructions.
            // Generate code to write the cache index into the bailout record before we jump to the call site.
            Assert(bailOutInfo->polymorphicCacheIndex != (uint)-1);
            Assert(bailOutInfo->bailOutRecord);

            IR::MemRefOpnd *pIndexOpnd =
                IR::MemRefOpnd::New((BYTE*)bailOutInfo->bailOutRecord + BailOutRecord::GetOffsetOfPolymorphicCacheIndex(), TyUint32, this->m_func);
            m_lowererMD.CreateAssign(
                pIndexOpnd, IR::IntConstOpnd::New(bailOutInfo->polymorphicCacheIndex, TyUint32, this->m_func), instr);
        }

        // GenerateBailOut should have replaced this as a label as we should have already lowered
        // the main bailOutInstr.
        IR::LabelInstr * bailOutTargetLabel = bailOutInstr->AsLabelInstr();
#if DBG
        if (bailOutTargetLabel->m_noHelperAssert)
        {
            collectRuntimeStatsLabel->m_noHelperAssert = true;
        }
#endif
        Assert(bailOutLabel == nullptr || bailOutLabel == bailOutTargetLabel);

        IR::BranchInstr * branchInstr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, bailOutTargetLabel, this->m_func);
        instr->InsertAfter(branchInstr);
        instr->Remove();
        return collectRuntimeStatsLabel ? collectRuntimeStatsLabel : bailOutLabel;
    }

    // The bailout hasn't be generated yet.
    Assert(!bailOutInstr->IsLabelInstr());

    // Add helper label to trigger layout.
    collectRuntimeStatsLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    instr->InsertBefore(collectRuntimeStatsLabel);

    // capture the condition for this bailout
    if (bailOutLabel == nullptr)
    {
        // Create a label and place it in the bailout info so that shared bailout point can jump to this one
        if (instr->m_prev->IsLabelInstr())
        {
            bailOutLabel = instr->m_prev->AsLabelInstr();
            Assert(bailOutLabel->isOpHelper);
        }
        else
        {
            bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            instr->InsertBefore(bailOutLabel);
        }
    }
    else
    {
        instr->InsertBefore(bailOutLabel);
    }

#if DBG
    if (bailOutInstr->m_opcode == Js::OpCode::BailOnNoSimdTypeSpec || bailOutInstr->m_opcode == Js::OpCode::BailOnNoProfile || bailOutInstr->m_opcode == Js::OpCode::BailOnException || bailOutInstr->m_opcode == Js::OpCode::Yield)
    {
        bailOutLabel->m_noHelperAssert = true;
    }
#endif

    bailOutInfo->bailOutInstr = bailOutLabel;
    bailOutLabel->m_hasNonBranchRef = true;

    // Create the bail out record
    Assert(bailOutInfo->bailOutRecord == nullptr);
    BailOutRecord * bailOutRecord;
    IR::JnHelperMethod helperMethod;
    if (branchInstr != nullptr)
    {
        Assert(branchInstr->GetSrc2() == nullptr);
        Assert(branchInstr->GetDst() == nullptr);

        IR::LabelInstr * targetLabel = branchInstr->GetTarget();
        Assert(targetLabel->GetByteCodeOffset() != Js::Constants::NoByteCodeOffset);

        uint32 trueOffset;
        uint32 falseOffset;
        IR::Opnd *condOpnd = branchInstr->GetSrc1();
        bool invertTarget = (branchInstr->m_opcode == Js::OpCode::BrFalse_A);

        if (bailOutInfo->isInvertedBranch)
        {
            // Flip the condition
            IR::Instr *subInstr = IR::Instr::New(Js::OpCode::Sub_I4, condOpnd, condOpnd, IR::IntConstOpnd::New(1, TyInt32, instr->m_func), instr->m_func);
            instr->InsertBefore(subInstr);
            this->m_lowererMD.EmitInt4Instr(subInstr);
            // We should really do a DEC/NEG for a full 2's complement flip from 0/1 to 1/0,
            // but DEC is sufficient to flip from 0/1 to -1/0, which is false/true to true/false...
            //instr->InsertBefore(IR::Instr::New(Js::OpCode::Neg_I4, condOpnd, condOpnd, instr->m_func));

            invertTarget = invertTarget ? false : true;
        }

        if (!invertTarget)
        {
            trueOffset = targetLabel->GetByteCodeOffset();
            falseOffset = bailOutInfo->bailOutOffset;
        }
        else
        {
            falseOffset = targetLabel->GetByteCodeOffset();
            trueOffset = bailOutInfo->bailOutOffset;
        }

        bailOutRecord = NativeCodeDataNewZ(this->m_func->GetNativeCodeDataAllocator(),
            BranchBailOutRecord, trueOffset, falseOffset, branchInstr->GetByteCodeReg(), instr->GetBailOutKind(), bailOutInfo->bailOutFunc);

        helperMethod = IR::HelperSaveAllRegistersAndBranchBailOut;
#ifdef _M_IX86
        if(!AutoSystemInfo::Data.SSE2Available())
        {
            helperMethod = IR::HelperSaveAllRegistersNoSse2AndBranchBailOut;
        }
#endif

        // Save the condition. The register allocator will generate arguments.
        bailOutInfo->branchConditionOpnd = branchInstr->GetSrc1()->Copy(branchInstr->m_func);
    }
    else
    {
        bailOutRecord = NativeCodeDataNewZ(this->m_func->GetNativeCodeDataAllocator(),
            BailOutRecord, bailOutInfo->bailOutOffset, bailOutInfo->polymorphicCacheIndex, instr->GetBailOutKind(), bailOutInfo->bailOutFunc);

        helperMethod = IR::HelperSaveAllRegistersAndBailOut;
#ifdef _M_IX86
        if(!AutoSystemInfo::Data.SSE2Available())
        {
            helperMethod = IR::HelperSaveAllRegistersNoSse2AndBailOut;
        }
#endif
    }

    // Save the bailout record. The register allocator will generate arguments.
    bailOutInfo->bailOutRecord = bailOutRecord;

#if ENABLE_DEBUG_CONFIG_OPTIONS
    bailOutRecord->bailOutOpcode = bailOutInfo->bailOutOpcode;
#endif

    // Call the bail out wrapper
    instr->m_opcode = Js::OpCode::Call;
    if(instr->GetDst())
    {
        // To facilitate register allocation, don't assign a destination. The result will anyway go into the return register,
        // but the register allocator does not need to kill that register for the call.
        instr->FreeDst();
    }
    instr->SetSrc1(IR::HelperCallOpnd::New(helperMethod, this->m_func));
    m_lowererMD.LowerCall(instr, 0);

    if (bailOutInstr->GetBailOutKind() != IR::BailOutForGeneratorYield)
    {
        // Defer introducing the JMP to epilog until LowerPrologEpilog phase for Yield bailouts so
        // that Yield does not appear to have flow out of its containing block for the RegAlloc phase.
        // Yield is an unconditional bailout but we want to simulate the flow as if the Yield were
        // just like a call.
        GenerateJumpToEpilogForBailOut(bailOutInfo, instr);
    }

    return collectRuntimeStatsLabel ? collectRuntimeStatsLabel : bailOutLabel;
}

void
Lowerer::GenerateJumpToEpilogForBailOut(BailOutInfo * bailOutInfo, IR::Instr *instr)
{
    IR::Instr * exitPrevInstr = this->m_func->m_exitInstr->m_prev;
    // JMP to the epilog
    IR::LabelInstr * exitTargetInstr;
    if (exitPrevInstr->IsLabelInstr())
    {
        exitTargetInstr = exitPrevInstr->AsLabelInstr();
    }
    else
    {
        exitTargetInstr = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, false);
        exitPrevInstr->InsertAfter(exitTargetInstr);
    }

    exitTargetInstr = m_lowererMD.GetBailOutStackRestoreLabel(bailOutInfo, exitTargetInstr);

    IR::Instr * instrAfter = instr->m_next;
    IR::BranchInstr * exitInstr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, exitTargetInstr, this->m_func);
    instrAfter->InsertBefore(exitInstr);
}

///----------------------------------------------------------------------------
///
/// Lowerer::GenerateFastCondBranch
///
///----------------------------------------------------------------------------
bool
Lowerer::GenerateFastCondBranch(IR::BranchInstr * instrBranch, bool *pIsHelper)
{
    // The idea is to do an inline compare if we can prove that both sources
    // are tagged ints
    //
    // Given:
    //
    //      Brxx_A $L, src1, src2
    //
    // Generate:
    //
    // (If not Int31's, goto $helper)
    //      Jxx $L, src1, src2
    //      JMP $fallthru
    // $helper:
    //      (caller will generate normal helper call sequence)
    // $fallthru:

    IR::LabelInstr *     labelHelper = nullptr;
    IR::LabelInstr *     labelFallThru;
    IR::BranchInstr *    instr;
    IR::Opnd *           opndSrc1;
    IR::Opnd *           opndSrc2;

    opndSrc1 = instrBranch->GetSrc1();
    opndSrc2 = instrBranch->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "BrC expects 2 src operands");

    // Not tagged ints?
    if (opndSrc1->IsRegOpnd() && opndSrc1->AsRegOpnd()->IsNotInt())
    {
        return true;
    }
    if (opndSrc2->IsRegOpnd() && opndSrc2->AsRegOpnd()->IsNotInt())
    {
        return true;
    }

    // Tagged ints?
    bool isTaggedInts = false;
    if (opndSrc1->IsTaggedInt())
    {
        if (opndSrc2->IsTaggedInt())
        {
            isTaggedInts = true;
        }
    }

    if (!isTaggedInts)
    {
        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        this->m_lowererMD.GenerateSmIntPairTest(instrBranch, opndSrc1, opndSrc2, labelHelper);
    }

    //      Jxx $L, src1, src2

    opndSrc1 = opndSrc1->UseWithNewType(TyInt32, this->m_func);
    opndSrc2 = opndSrc2->UseWithNewType(TyInt32, this->m_func);

    instr = IR::BranchInstr::New(instrBranch->m_opcode, instrBranch->GetTarget(), opndSrc1, opndSrc2, this->m_func);
    instrBranch->InsertBefore(instr);
    this->m_lowererMD.LowerCondBranch(instr);

    if (isTaggedInts)
    {
        instrBranch->Remove();

        // Skip lowering call to helper
        return false;
    }

    //      JMP $fallthru

    IR::Instr *instrNext = instrBranch->GetNextRealInstrOrLabel();
    if (instrNext->IsLabelInstr())
    {
        labelFallThru = instrNext->AsLabelInstr();
    }
    else
    {
        labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, /**pIsHelper*/FALSE);
        instrBranch->InsertAfter(labelFallThru);
    }
    instr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelFallThru, this->m_func);
    instrBranch->InsertBefore(instr);

    // $helper:
    //      (caller will generate normal helper call sequence)
    // $fallthru:

    AssertMsg(labelHelper, "Should not be NULL");
    instrBranch->InsertBefore(labelHelper);

    *pIsHelper = true;
    return true;
}

void
Lowerer::LowerInlineeStart(IR::Instr * inlineeStartInstr)
{
    IR::Opnd *linkOpnd   = inlineeStartInstr->GetSrc2();
    if (!linkOpnd)
    {
        Assert(inlineeStartInstr->m_func->m_hasInlineArgsOpt);
        return;
    }

    AssertMsg(inlineeStartInstr->m_func->firstActualStackOffset != -1, "This should have been already done in backward pass");

    IR::Instr *startCall;
    // Free the argOut links and lower them to MOVs
    inlineeStartInstr->IterateArgInstrs([&](IR::Instr* argInstr){
        Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A || argInstr->m_opcode == Js::OpCode::ArgOut_A_Inline);
        startCall = argInstr->GetSrc2()->GetStackSym()->m_instrDef;
        argInstr->FreeSrc2();
#pragma prefast(suppress:6235, "Non-Zero Constant in Condition")
        if (!PHASE_ON(Js::EliminateArgoutForInlineePhase, this->m_func) || inlineeStartInstr->m_func->GetJnFunction()->GetHasOrParentHasArguments())
        {
            m_lowererMD.ChangeToAssign(argInstr);
        }
        else
        {
            argInstr->m_opcode = Js::OpCode::ArgOut_A_InlineBuiltIn;
        }

        return false;
    });


    IR::Instr *argInsertInstr = inlineeStartInstr;
    uint i = 0;
    inlineeStartInstr->IterateMetaArgs( [&] (IR::Instr* metaArg)
    {
        if(i == 0)
        {
            LowererMD::CreateAssign(metaArg->m_func->GetNextInlineeFrameArgCountSlotOpnd(),
                IR::AddrOpnd::NewNull(metaArg->m_func),
                argInsertInstr);
        }
        if (i == Js::Constants::InlineeMetaArgIndex_FunctionObject)
        {
            metaArg->SetSrc1(inlineeStartInstr->GetSrc1());
        }
        metaArg->Unlink();
        argInsertInstr->InsertBefore(metaArg);
        IR::Instr* prev = metaArg->m_prev;
        m_lowererMD.ChangeToAssign(metaArg);
        if (i == Js::Constants::InlineeMetaArgIndex_Argc)
        {
#if defined(_M_IX86) || defined(_M_X64)
            Assert(metaArg == prev->m_next);
#else //defined(_M_ARM)
            Assert(prev->m_next->m_opcode == Js::OpCode::LDIMM);
#endif
            metaArg = prev->m_next;
            Assert(metaArg->GetSrc1()->AsAddrOpnd()->m_dontEncode == true);
            metaArg->isInlineeEntryInstr = true;
            LowererMD::Legalize(metaArg);
        }
        argInsertInstr = metaArg;
        i++;
        return false;
    });
    if (inlineeStartInstr->m_func->m_hasInlineArgsOpt)
    {
        inlineeStartInstr->FreeSrc1();
        inlineeStartInstr->FreeSrc2();
        inlineeStartInstr->FreeDst();
    }
    else
    {
        inlineeStartInstr->Remove();
    }
}

void
Lowerer::LowerInlineeEnd(IR::Instr *instr)
{
    Assert(instr->m_func->IsInlinee());
    Assert(m_func->IsTopFunc());

    // No need to emit code if the function wasn't marked as having implicit calls or bailout.  Dead-Store should have removed inline overhead.
    if (instr->m_func->GetHasImplicitCalls() || PHASE_OFF(Js::DeadStorePhase, this->m_func))
    {
        LowererMD::CreateAssign(instr->m_func->GetInlineeArgCountSlotOpnd(),
                                IR::AddrOpnd::New(0, IR::AddrOpndKindConstantVar, instr->m_func),
                                instr);
    }

    // Keep InlineeEnd around as it is used by register allocator, if we have optimized the arguments stack
    if (instr->m_func->m_hasInlineArgsOpt)
    {
        instr->FreeSrc1();
    }
    else
    {
        instr->Remove();
    }
}

IR::Instr *
Lowerer::LoadFloatFromNonReg(IR::Opnd * opndSrc, IR::Opnd * opndDst, IR::Instr * instrInsert)
{
    double value;

    if (opndSrc->IsAddrOpnd())
    {
        Js::Var var = opndSrc->AsAddrOpnd()->m_address;
        if (Js::TaggedInt::Is(var))
        {
            value = Js::TaggedInt::ToDouble(var);
        }
        else
        {
            value = Js::JavascriptNumber::GetValue(var);
        }
    }
    else if (opndSrc->IsIntConstOpnd())
    {
        if (opndSrc->IsUInt32())
        {
            value = (double)(uint32)opndSrc->AsIntConstOpnd()->GetValue();
        }
        else
        {
            value = (double)opndSrc->AsIntConstOpnd()->GetValue();
        }
    }
    else if (opndSrc->IsFloatConstOpnd())
    {
        value = (double)opndSrc->AsFloatConstOpnd()->m_value;
    }
    else
    {
        AssertMsg(0, "Unexpected opnd type");
        value = 0;
    }

    return LowererMD::LoadFloatValue(opndDst, value, instrInsert);
}

void
Lowerer::LoadInt32FromUntaggedVar(IR::Instr *const instrLoad)
{
    Assert(instrLoad);
    Assert(instrLoad->GetDst());
    Assert(instrLoad->GetDst()->IsRegOpnd());
    Assert(instrLoad->GetDst()->IsInt32());
    Assert(instrLoad->GetSrc1());
    Assert(instrLoad->GetSrc1()->IsRegOpnd());
    Assert(instrLoad->GetSrc1()->IsVar());
    Assert(!instrLoad->GetSrc2());

    //     push src
    //     int32Value = call JavascriptNumber::GetNonzeroInt32Value_NoChecks
    //     test int32Value, int32Value
    //     jne  $done
    //     (fall through to 'instrLoad'; caller will generate code here)
    //   $done:
    //     (rest of program)

    Func *const func = instrLoad->m_func;
    IR::LabelInstr *const doneLabel = instrLoad->GetOrCreateContinueLabel();

    //     push src
    //     int32Value = call JavascriptNumber::GetNonzeroInt32Value_NoChecks
    StackSym *const int32ValueSym = instrLoad->GetDst()->AsRegOpnd()->m_sym;
    IR::Instr *const instr =
        IR::Instr::New(
            Js::OpCode::Call,
            IR::RegOpnd::New(int32ValueSym, TyInt32, func),
            instrLoad->GetSrc1()->AsRegOpnd(),
            func);
    instrLoad->InsertBefore(instr);
    LowerUnaryHelper(instr, IR::HelperGetNonzeroInt32Value_NoTaggedIntCheck);

    //     test int32Value, int32Value
    //     jne  $done
    InsertCompareBranch(
        IR::RegOpnd::New(int32ValueSym, TyInt32, func),
        IR::IntConstOpnd::New(0, TyInt32, func, true),
        Js::OpCode::BrNeq_A,
        doneLabel,
        instrLoad);
}

bool
Lowerer::GetValueFromIndirOpnd(IR::IndirOpnd *indirOpnd, IR::Opnd **pValueOpnd, IntConstType *pValue)
{
    IR::RegOpnd *indexOpnd = indirOpnd->GetIndexOpnd();
    IR::Opnd* valueOpnd = nullptr;
    IntConstType value = 0;
    if (!indexOpnd)
    {
        value = (IntConstType)indirOpnd->GetOffset();
        if (value < 0)
        {
            // Can't do fast path for negative index
            return false;
        }
        valueOpnd = IR::IntConstOpnd::New(value, TyInt32, this->m_func);
    }
    else if (indexOpnd->m_sym->IsIntConst())
    {
        value = indexOpnd->AsRegOpnd()->m_sym->GetIntConstValue();
        if (value < 0)
        {
            // Can't do fast path for negative index
            return false;
        }
        valueOpnd = IR::IntConstOpnd::New(value, TyInt32, this->m_func);
    }
    *pValueOpnd = valueOpnd;
    *pValue = value;
    return true;
}

void
Lowerer::GenerateFastBrOnObject(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::BrOnObject_A);

    IR::RegOpnd      *object        = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd() : nullptr;
    IR::LabelInstr   *done          = instr->GetOrCreateContinueLabel();
    IR::LabelInstr   *target        = instr->AsBranchInstr()->GetTarget();
    IR::RegOpnd      *typeRegOpnd   = IR::RegOpnd::New(TyMachReg, m_func);
    IR::IntConstOpnd *typeIdOpnd    = IR::IntConstOpnd::New(Js::TypeIds_LastJavascriptPrimitiveType, TyInt32, instr->m_func);

    if (!object)
    {
        object = IR::RegOpnd::New(TyVar, m_func);
        LowererMD::CreateAssign(object, instr->GetSrc1(), instr);
    }

    // TEST object, 1
    // JNE $done
    // MOV typeRegOpnd, [object + offset(Type)]
    // CMP [typeRegOpnd + offset(TypeId)], TypeIds_LastJavascriptPrimitiveType
    // JGT $target
    // $done:

    m_lowererMD.GenerateObjectTest(object, instr, done);

    InsertMove(typeRegOpnd,
               IR::IndirOpnd::New(object, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, m_func),
               instr);

    InsertCompareBranch(
        IR::IndirOpnd::New(typeRegOpnd, Js::Type::GetOffsetOfTypeId(), TyInt32, m_func),
        typeIdOpnd, Js::OpCode::BrGt_A, target, instr);

    instr->Remove();
}

void Lowerer::GenerateObjectHeaderInliningTest(IR::RegOpnd *baseOpnd, IR::LabelInstr * target,IR::Instr *insertBeforeInstr)
{
    Assert(baseOpnd);
    Assert(target);
    AssertMsg(
        baseOpnd->GetValueType().IsLikelyObject() &&
        baseOpnd->GetValueType().GetObjectType() == ObjectType::ObjectWithArray,
        "Why are we here, when the object is already known not to have an ObjArray");
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    // mov  type, [base + offsetOf(type)]
    IR::RegOpnd *const opnd = IR::RegOpnd::New(TyMachPtr, func);

    m_lowererMD.CreateAssign(
        opnd,
        IR::IndirOpnd::New(
            baseOpnd,
            Js::DynamicObject::GetOffsetOfType(),
            opnd->GetType(),
            func),
        insertBeforeInstr);

    // mov typeHandler, [type + offsetOf(typeHandler)]
    m_lowererMD.CreateAssign(
        opnd,
        IR::IndirOpnd::New(
            opnd,
            Js::DynamicType::GetOffsetOfTypeHandler(),
            opnd->GetType(),
            func),
        insertBeforeInstr);

    IR::IndirOpnd * offsetOfInlineSlotOpnd = IR::IndirOpnd::New(opnd,Js::DynamicTypeHandler::GetOffsetOfOffsetOfInlineSlots(), TyInt16, func);
    IR::IntConstOpnd * objHeaderInlinedSlotOffset = IR::IntConstOpnd::New(Js::DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots(), TyInt16, func);

    // CMP [typeHandler + offsetOf(offsetOfInlineSlots)], objHeaderInlinedSlotOffset
    InsertCompareBranch(
        offsetOfInlineSlotOpnd,
        objHeaderInlinedSlotOffset,
        Js::OpCode::BrEq_A,
        target,
        insertBeforeInstr);
}

void Lowerer::GenerateObjectTypeTest(IR::RegOpnd *srcReg, IR::Instr *instrInsert, IR::LabelInstr *labelHelper)
{
    Assert(srcReg);
    if (!srcReg->IsNotTaggedValue())
    {
        m_lowererMD.GenerateObjectTest(srcReg, instrInsert, labelHelper);
    }

    // CMP [srcReg], Js::DynamicObject::`vtable'
    // JNE $helper
    InsertCompareBranch(
        IR::IndirOpnd::New(srcReg, 0, TyMachPtr, m_func),
        LoadVTableValueOpnd(instrInsert, VTableValue::VtableDynamicObject),
        Js::OpCode::BrNeq_A,
        labelHelper,
        instrInsert);
}

const VTableValue Lowerer::VtableAddresses[static_cast<ValueType::TSize>(ObjectType::Count)] =
{
    /* ObjectType::UninitializedObject      */ VTableValue::VtableInvalid,
    /* ObjectType::Object                   */ VTableValue::VtableInvalid,
    /* ObjectType::RegExp                   */ VTableValue::VtableInvalid,
    /* ObjectType::ObjectWithArray          */ VTableValue::VtableJavascriptArray,
    /* ObjectType::Array                    */ VTableValue::VtableJavascriptArray,
    /* ObjectType::Int8Array                */ VTableValue::VtableInt8Array,
    /* ObjectType::Uint8Array               */ VTableValue::VtableUint8Array,
    /* ObjectType::Uint8ClampedArray        */ VTableValue::VtableUint8ClampedArray,
    /* ObjectType::Int16Array               */ VTableValue::VtableInt16Array,
    /* ObjectType::Uint16Array              */ VTableValue::VtableUint16Array,
    /* ObjectType::Int32Array               */ VTableValue::VtableInt32Array,
    /* ObjectType::Uint32Array              */ VTableValue::VtableUint32Array,
    /* ObjectType::Float32Array             */ VTableValue::VtableFloat32Array,
    /* ObjectType::Float64Array             */ VTableValue::VtableFloat64Array,
    /* ObjectType::Int8VirtualArray         */ VTableValue::VtableInt8VirtualArray,
    /* ObjectType::Uint8VirtualArray        */ VTableValue::VtableUint8VirtualArray,
    /* ObjectType::Uint8ClampedVirtualArray */ VTableValue::VtableUint8ClampedVirtualArray,
    /* ObjectType::Int16VirtualArray        */ VTableValue::VtableInt16VirtualArray,
    /* ObjectType::Uint16VirtualArray       */ VTableValue::VtableUint16VirtualArray,
    /* ObjectType::Int32VirtualArray        */ VTableValue::VtableInt32VirtualArray,
    /* ObjectType::Uint32VirtualArray       */ VTableValue::VtableUint32VirtualArray,
    /* ObjectType::Float32VirtualArray      */ VTableValue::VtableFloat32VirtualArray,
    /* ObjectType::Float64VirtualArray      */ VTableValue::VtableFloat64VirtualArray,
    /* ObjectType::Int8MixedArray           */ VTableValue::VtableInt8Array,
    /* ObjectType::Uint8MixedArray          */ VTableValue::VtableUint8Array,
    /* ObjectType::Uint8ClampedMixedArray   */ VTableValue::VtableUint8ClampedArray,
    /* ObjectType::Int16MixedArray          */ VTableValue::VtableInt16Array,
    /* ObjectType::Uint16MixedArray         */ VTableValue::VtableUint16Array,
    /* ObjectType::Int32MixedArray          */ VTableValue::VtableInt32Array,
    /* ObjectType::Uint32MixedArray         */ VTableValue::VtableUint32Array,
    /* ObjectType::Float32MixedArray        */ VTableValue::VtableFloat32Array,
    /* ObjectType::Float64MixedArray        */ VTableValue::VtableFloat64Array,
    /* ObjectType::Int64Array               */ VTableValue::VtableInt64Array,
    /* ObjectType::Uint64Array              */ VTableValue::VtableUint64Array,
    /* ObjectType::BoolArray                */ VTableValue::VtableBoolArray,
    /* ObjectType::CharArray                */ VTableValue::VtableCharArray

};

const uint32 Lowerer::OffsetsOfHeadSegment[static_cast<ValueType::TSize>(ObjectType::Count)] =
{
    /* ObjectType::UninitializedObject        */ static_cast<uint32>(-1),
    /* ObjectType::Object                     */ static_cast<uint32>(-1),
    /* ObjectType::RegExp                     */ static_cast<uint32>(-1),
    /* ObjectType::ObjectWithArray            */ Js::JavascriptArray::GetOffsetOfHead(),
    /* ObjectType::Array                      */ Js::JavascriptArray::GetOffsetOfHead(),
    /* ObjectType::Int8Array                  */ Js::Int8Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint8Array                 */ Js::Uint8Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint8ClampedArray          */ Js::Uint8ClampedArray::GetOffsetOfBuffer(),
    /* ObjectType::Int16Array                 */ Js::Int16Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint16Array                */ Js::Uint16Array::GetOffsetOfBuffer(),
    /* ObjectType::Int32Array                 */ Js::Int32Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint32Array                */ Js::Uint32Array::GetOffsetOfBuffer(),
    /* ObjectType::Float32Array               */ Js::Float32Array::GetOffsetOfBuffer(),
    /* ObjectType::Float64Array               */ Js::Float64Array::GetOffsetOfBuffer(),
    /* ObjectType::Int8VirtualArray           */ Js::Int8VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Uint8VirtualArray          */ Js::Uint8VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Uint8ClampedVirtualArray   */ Js::Uint8ClampedVirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Int16VirtualArray          */ Js::Int16VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Uint16VirtualArray         */ Js::Uint16VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Int32VirtualArray          */ Js::Int32VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Uint32VirtualArray         */ Js::Uint32VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Float32VirtualArray        */ Js::Float32VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Float64VirtualArray        */ Js::Float64VirtualArray::GetOffsetOfBuffer(),
    /* ObjectType::Int8MixedArray             */ Js::Int8Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint8MixedArray            */ Js::Uint8Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint8ClampedMixedArray     */ Js::Uint8ClampedArray::GetOffsetOfBuffer(),
    /* ObjectType::Int16MixedArray            */ Js::Int16Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint16MixedArray           */ Js::Uint16Array::GetOffsetOfBuffer(),
    /* ObjectType::Int32MixedArray            */ Js::Int32Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint32MixedArray           */ Js::Uint32Array::GetOffsetOfBuffer(),
    /* ObjectType::Float32MixedArray          */ Js::Float32Array::GetOffsetOfBuffer(),
    /* ObjectType::Float64MixedArray          */ Js::Float64Array::GetOffsetOfBuffer(),
    /* ObjectType::Int64Array                 */ Js::Int64Array::GetOffsetOfBuffer(),
    /* ObjectType::Uint64Array                */ Js::Uint64Array::GetOffsetOfBuffer(),
    /* ObjectType::BoolArray                  */ Js::BoolArray::GetOffsetOfBuffer(),
    /* ObjectType::CharArray                  */ Js::CharArray::GetOffsetOfBuffer()
};

const uint32 Lowerer::OffsetsOfLength[static_cast<ValueType::TSize>(ObjectType::Count)] =
{
    /* ObjectType::UninitializedObject      */ static_cast<uint32>(-1),
    /* ObjectType::Object                   */ static_cast<uint32>(-1),
    /* ObjectType::RegExp                   */ static_cast<uint32>(-1),
    /* ObjectType::ObjectWithArray          */ Js::JavascriptArray::GetOffsetOfLength(),
    /* ObjectType::Array                    */ Js::JavascriptArray::GetOffsetOfLength(),
    /* ObjectType::Int8Array                */ Js::Int8Array::GetOffsetOfLength(),
    /* ObjectType::Uint8Array               */ Js::Uint8Array::GetOffsetOfLength(),
    /* ObjectType::Uint8ClampedArray        */ Js::Uint8ClampedArray::GetOffsetOfLength(),
    /* ObjectType::Int16Array               */ Js::Int16Array::GetOffsetOfLength(),
    /* ObjectType::Uint16Array              */ Js::Uint16Array::GetOffsetOfLength(),
    /* ObjectType::Int32Array               */ Js::Int32Array::GetOffsetOfLength(),
    /* ObjectType::Uint32Array              */ Js::Uint32Array::GetOffsetOfLength(),
    /* ObjectType::Float32Array             */ Js::Float32Array::GetOffsetOfLength(),
    /* ObjectType::Float64Array             */ Js::Float64Array::GetOffsetOfLength(),
    /* ObjectType::Int8VirtualArray         */ Js::Int8VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Uint8VirtualArray        */ Js::Uint8VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Uint8ClampedVirtualArray */ Js::Uint8ClampedVirtualArray::GetOffsetOfLength(),
    /* ObjectType::Int16VirtualArray        */ Js::Int16VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Uint16VirtualArray       */ Js::Uint16VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Int32VirtualArray        */ Js::Int32VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Uint32VirtualArray       */ Js::Uint32VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Float32VirtualArray      */ Js::Float32VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Float64VirtualArray      */ Js::Float64VirtualArray::GetOffsetOfLength(),
    /* ObjectType::Int8MixedArray           */ Js::Int8Array::GetOffsetOfLength(),
    /* ObjectType::Uint8MixedArray          */ Js::Uint8Array::GetOffsetOfLength(),
    /* ObjectType::Uint8ClampedMixedArray   */ Js::Uint8ClampedArray::GetOffsetOfLength(),
    /* ObjectType::Int16MixedArray          */ Js::Int16Array::GetOffsetOfLength(),
    /* ObjectType::Uint16MixedArray         */ Js::Uint16Array::GetOffsetOfLength(),
    /* ObjectType::Int32MixedArray          */ Js::Int32Array::GetOffsetOfLength(),
    /* ObjectType::Uint32MixedArray         */ Js::Uint32Array::GetOffsetOfLength(),
    /* ObjectType::Float32MixedArray        */ Js::Float32Array::GetOffsetOfLength(),
    /* ObjectType::Float64MixedArray        */ Js::Float64Array::GetOffsetOfLength(),
    /* ObjectType::Int64Array               */ Js::Int64Array::GetOffsetOfLength(),
    /* ObjectType::Uint64Array              */ Js::Uint64Array::GetOffsetOfLength(),
    /* ObjectType::BoolArray                */ Js::BoolArray::GetOffsetOfLength(),
    /* ObjectType::CharArray                */ Js::CharArray::GetOffsetOfLength()
};

const IRType Lowerer::IndirTypes[static_cast<ValueType::TSize>(ObjectType::Count)] =
{
    /* ObjectType::UninitializedObject      */ TyIllegal,
    /* ObjectType::Object                   */ TyIllegal,
    /* ObjectType::RegExp                   */ TyIllegal,
    /* ObjectType::ObjectWithArray          */ TyVar,
    /* ObjectType::Array                    */ TyVar,
    /* ObjectType::Int8Array                */ TyInt8,
    /* ObjectType::Uint8Array               */ TyUint8,
    /* ObjectType::Uint8ClampedArray        */ TyUint8,
    /* ObjectType::Int16Array               */ TyInt16,
    /* ObjectType::Uint16Array              */ TyUint16,
    /* ObjectType::Int32Array               */ TyInt32,
    /* ObjectType::Uint32Array              */ TyUint32,
    /* ObjectType::Float32Array             */ TyFloat32,
    /* ObjectType::Float64Array             */ TyFloat64,
    /* ObjectType::Int8VirtualArray         */ TyInt8,
    /* ObjectType::Uint8VirtualArray        */ TyUint8,
    /* ObjectType::Uint8ClampedVirtualArray */ TyUint8,
    /* ObjectType::Int16VirtualArray        */ TyInt16,
    /* ObjectType::Uint16vArray             */ TyUint16,
    /* ObjectType::Int32VirtualArray        */ TyInt32,
    /* ObjectType::Uint32VirtualArray       */ TyUint32,
    /* ObjectType::Float32VirtualArray      */ TyFloat32,
    /* ObjectType::Float64VirtualArray      */ TyFloat64,
    /* ObjectType::Int8MixedArray           */ TyInt8,
    /* ObjectType::Uint8MixedArray          */ TyUint8,
    /* ObjectType::Uint8ClampedMixedArray   */ TyUint8,
    /* ObjectType::Int16MixedArray          */ TyInt16,
    /* ObjectType::Uint16MixedArray         */ TyUint16,
    /* ObjectType::Int32MixedArray          */ TyInt32,
    /* ObjectType::Uint32MixedArray         */ TyUint32,
    /* ObjectType::Float32MixedArray        */ TyFloat32,
    /* ObjectType::Float64MixedArray        */ TyFloat64,
    /* ObjectType::Int64Array               */ TyInt64,
    /* ObjectType::Uint64Array              */ TyUint64,
    /* ObjectType::BoolArray                */ TyUint8,
    /* ObjectType::CharArray                */ TyUint16
};

const BYTE Lowerer::IndirScales[static_cast<ValueType::TSize>(ObjectType::Count)] =
{
    /* ObjectType::UninitializedObject      */ static_cast<BYTE>(-1),
    /* ObjectType::Object                   */ static_cast<BYTE>(-1),
    /* ObjectType::RegExp                   */ static_cast<BYTE>(-1),
    /* ObjectType::ObjectWithArray          */ LowererMD::GetDefaultIndirScale(),
    /* ObjectType::Array                    */ LowererMD::GetDefaultIndirScale(),
    /* ObjectType::Int8Array                */ 0, // log2(sizeof(int8))
    /* ObjectType::Uint8Array               */ 0, // log2(sizeof(uint8))
    /* ObjectType::Uint8ClampedArray        */ 0, // log2(sizeof(uint8))
    /* ObjectType::Int16Array               */ 1, // log2(sizeof(int16))
    /* ObjectType::Uint16Array              */ 1, // log2(sizeof(uint16))
    /* ObjectType::Int32Array               */ 2, // log2(sizeof(int32))
    /* ObjectType::Uint32Array              */ 2, // log2(sizeof(uint32))
    /* ObjectType::Float32Array             */ 2, // log2(sizeof(float))
    /* ObjectType::Float64Array             */ 3, // log2(sizeof(double))
    /* ObjectType::Int8VirtualArray         */ 0, // log2(sizeof(int8))
    /* ObjectType::Uint8VirtualArray        */ 0, // log2(sizeof(uint8))
    /* ObjectType::Uint8ClampedVirtualArray */ 0, // log2(sizeof(uint8))
    /* ObjectType::Int16VirtualArray        */ 1, // log2(sizeof(int16))
    /* ObjectType::Uint16VirtualArray       */ 1, // log2(sizeof(uint16))
    /* ObjectType::Int32VirtualArray        */ 2, // log2(sizeof(int32))
    /* ObjectType::Uint32VirtualArray       */ 2, // log2(sizeof(uint32))
    /* ObjectType::Float32VirtualArray      */ 2, // log2(sizeof(float))
    /* ObjectType::Float64VirtualArray      */ 3, // log2(sizeof(double))
    /* ObjectType::Int8MixedArray           */ 0, // log2(sizeof(int8))
    /* ObjectType::Uint8MixedArray          */ 0, // log2(sizeof(uint8))
    /* ObjectType::Uint8ClampedMixedArray   */ 0, // log2(sizeof(uint8))
    /* ObjectType::Int16MixedArray          */ 1, // log2(sizeof(int16))
    /* ObjectType::Uint16MixedArray         */ 1, // log2(sizeof(uint16))
    /* ObjectType::Int32MixedArray          */ 2, // log2(sizeof(int32))
    /* ObjectType::Uint32MixedArray         */ 2, // log2(sizeof(uint32))
    /* ObjectType::Float32MixedArray        */ 2, // log2(sizeof(float))
    /* ObjectType::Float64MixedArray        */ 3, // log2(sizeof(double))
    /* ObjectType::Int64Array               */ 3, // log2(sizeof(int64))
    /* ObjectType::Uint64Array              */ 3, // log2(sizeof(uint64))
    /* ObjectType::BoolArray                */ 0, // log2(sizeof(bool))
    /* ObjectType::CharArray                */ 1  // log2(sizeof(wchar_t))
};

VTableValue Lowerer::GetArrayVtableAddress(const ValueType valueType, bool getVirtual)
{
    Assert(valueType.IsLikelyAnyOptimizedArray());
    if(valueType.IsLikelyArrayOrObjectWithArray())
    {
        if(valueType.HasIntElements())
        {
            return VTableValue::VtableNativeIntArray;
        }
        else if(valueType.HasFloatElements())
        {
            return VTableValue::VtableNativeFloatArray;
        }
    }
    if (getVirtual && valueType.IsLikelyMixedTypedArrayType())
    {
        return VtableAddresses[static_cast<ValueType::TSize>(valueType.GetMixedToVirtualTypedArrayObjectType())];
    }
    return VtableAddresses[static_cast<ValueType::TSize>(valueType.GetObjectType())];
}

uint32 Lowerer::GetArrayOffsetOfHeadSegment(const ValueType valueType)
{
    Assert(valueType.IsLikelyAnyOptimizedArray());
    return OffsetsOfHeadSegment[static_cast<ValueType::TSize>(valueType.GetObjectType())];
}

uint32 Lowerer::GetArrayOffsetOfLength(const ValueType valueType)
{
    Assert(valueType.IsLikelyAnyOptimizedArray());
    return OffsetsOfLength[static_cast<ValueType::TSize>(valueType.GetObjectType())];
}

IRType Lowerer::GetArrayIndirType(const ValueType valueType)
{
    Assert(valueType.IsLikelyAnyOptimizedArray());
    if(valueType.IsLikelyArrayOrObjectWithArray())
    {
        if(valueType.HasIntElements())
        {
            return TyInt32;
        }
        else if(valueType.HasFloatElements())
        {
            return TyFloat64;
        }
    }

    return IndirTypes[static_cast<ValueType::TSize>(valueType.GetObjectType())];
}

BYTE Lowerer::GetArrayIndirScale(const ValueType valueType) const
{
    Assert(valueType.IsLikelyAnyOptimizedArray());
    if(valueType.IsLikelyArrayOrObjectWithArray())
    {
        if(valueType.HasIntElements())
        {
            return 2; // log2(sizeof(int32))
        }
        else if(valueType.HasFloatElements())
        {
            return 3; // log2(sizeof(double))
        }
    }

    return IndirScales[static_cast<ValueType::TSize>(valueType.GetObjectType())];
}

bool Lowerer::ShouldGenerateArrayFastPath(
    const IR::Opnd *const arrayOpnd,
    const bool supportsObjectsWithArrays,
    const bool supportsTypedArrays,
    const bool requiresSse2ForFloatArrays) const
{
    Assert(arrayOpnd);

    const ValueType arrayValueType(arrayOpnd->GetValueType());
    if(arrayValueType.IsUninitialized())
    {
        // Don't have info about the value type, better to generate the fast path anyway
        return true;
    }
    if (!arrayValueType.IsLikelyObject())
    {
        if (!arrayValueType.HasBeenObject() || arrayValueType.IsLikelyString())
        {
            return false;
        }
        //We have seen at least once there is an object in the code path. Generate fastpath hoping it to be array.
        //Its nice if we can get all the attributes set but valueType is only 16 bits. Consider expanding the same.
        return true;
    }

    if( !supportsObjectsWithArrays && arrayValueType.GetObjectType() == ObjectType::ObjectWithArray ||
        !supportsTypedArrays && arrayValueType.IsLikelyTypedArray())
    {
        // The fast path likely would not hit
        return false;
    }
    if(arrayValueType.GetObjectType() == ObjectType::UninitializedObject)
    {
        // Don't have info about the object type, better to generate the fast path anyway
        return true;
    }
#ifdef _M_IX86
    if(requiresSse2ForFloatArrays &&
        (
            arrayValueType.GetObjectType() == ObjectType::Float32Array ||
            arrayValueType.GetObjectType() == ObjectType::Float64Array
        ) &&
        !AutoSystemInfo::Data.SSE2Available())
    {
        // Fast paths for float arrays rely on SSE2
        return false;
    }
#endif
    return !arrayValueType.IsLikelyAnyUnOptimizedArray();
}

IR::RegOpnd *Lowerer::LoadObjectArray(IR::RegOpnd *const baseOpnd, IR::Instr *const insertBeforeInstr)
{
    Assert(baseOpnd);
    Assert(
        baseOpnd->GetValueType().IsLikelyObject() &&
        baseOpnd->GetValueType().GetObjectType() == ObjectType::ObjectWithArray);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    // mov  array, [base + offsetOf(objectArrayOrFlags)]
    IR::RegOpnd *const arrayOpnd =
        baseOpnd->IsArrayRegOpnd() ? baseOpnd->AsArrayRegOpnd()->CopyAsRegOpnd(func) : baseOpnd->Copy(func)->AsRegOpnd();
    arrayOpnd->m_sym = StackSym::New(TyVar, func);
    arrayOpnd->SetValueType(arrayOpnd->GetValueType().ToArray());
    const IR::AutoReuseOpnd autoReuseArrayOpnd(arrayOpnd, func, false /* autoDelete */);
    m_lowererMD.CreateAssign(
        arrayOpnd,
        IR::IndirOpnd::New(
            baseOpnd,
            Js::DynamicObject::GetOffsetOfObjectArray(),
            arrayOpnd->GetType(),
            func),
        insertBeforeInstr);

    return arrayOpnd;
}

void
Lowerer::GenerateIsEnabledArraySetElementFastPathCheck(
    IR::LabelInstr * isDisabledLabel,
    IR::Instr * const insertBeforeInstr)
{
    InsertCompareBranch(
        this->LoadOptimizationOverridesValueOpnd(insertBeforeInstr, OptimizationOverridesValue::OptimizationOverridesArraySetElementFastPathVtable),
        LoadVTableValueOpnd(insertBeforeInstr, VTableValue::VtableInvalid),
        Js::OpCode::BrEq_A,
        isDisabledLabel,
        insertBeforeInstr);
}

IR::RegOpnd *Lowerer::GenerateArrayTest(
    IR::RegOpnd *const baseOpnd,
    IR::LabelInstr *const isNotObjectLabel,
    IR::LabelInstr *const isNotArrayLabel,
    IR::Instr *const insertBeforeInstr,
    const bool forceFloat,
    const bool isStore,
    const bool allowDefiniteArray)
{
    Assert(baseOpnd);

    const ValueType baseValueType(baseOpnd->GetValueType());

    // Shouldn't request to do an array test when it's already known to be an array, or if it's unlikely to be an array
    Assert(!baseValueType.IsAnyOptimizedArray() || allowDefiniteArray || baseValueType.IsNativeArray());
    Assert(baseValueType.IsUninitialized() || baseValueType.HasBeenObject());

    Assert(isNotObjectLabel);
    Assert(isNotArrayLabel);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::RegOpnd *arrayOpnd;
    IR::AutoReuseOpnd autoReuseArrayOpnd;
    if(baseValueType.IsLikelyObject() && baseValueType.GetObjectType() == ObjectType::ObjectWithArray)
    {
        // Only DynamicObject is allowed (DynamicObject vtable is ensured) because some object types have special handling for
        // index properties - arguments object, string object, external object, etc.
        GenerateObjectTypeTest(baseOpnd, insertBeforeInstr, isNotObjectLabel);
        GenerateObjectHeaderInliningTest(baseOpnd, isNotArrayLabel, insertBeforeInstr);
        arrayOpnd = LoadObjectArray(baseOpnd, insertBeforeInstr);
        autoReuseArrayOpnd.Initialize(arrayOpnd, func, false /* autoDelete */);

        // test array, array
        // je   $isNotArrayLabel
        // test array, 1
        // jne  $isNotArrayLabel
        InsertTestBranch(
            arrayOpnd,
            arrayOpnd,
            Js::OpCode::BrEq_A,
            isNotArrayLabel,
            insertBeforeInstr);
        InsertTestBranch(
            arrayOpnd,
            IR::IntConstOpnd::New(1, TyUint8, func, true),
            Js::OpCode::BrNeq_A,
            isNotArrayLabel,
            insertBeforeInstr);
    }
    else
    {
        if(!baseOpnd->IsNotTaggedValue())
        {
            m_lowererMD.GenerateObjectTest(baseOpnd, insertBeforeInstr, isNotObjectLabel);
        }
        arrayOpnd = baseOpnd->Copy(func)->AsRegOpnd();
        if(!baseValueType.IsLikelyAnyOptimizedArray())
        {
            arrayOpnd->SetValueType(
                ValueType::GetObject(ObjectType::Array)
                    .ToLikely()
                    .SetHasNoMissingValues(false)
                    .SetArrayTypeId(Js::TypeIds_Array));
        }
        autoReuseArrayOpnd.Initialize(arrayOpnd, func, false /* autoDelete */);
    }

    VTableValue vtableAddress = baseValueType.IsLikelyAnyOptimizedArray()
                ? GetArrayVtableAddress(baseValueType)
                : VTableValue::VtableJavascriptArray;

    VTableValue virtualVtableAddress = VTableValue::VtableInvalid;
    if (baseValueType.IsLikelyMixedTypedArrayType())
    {
        virtualVtableAddress = GetArrayVtableAddress(baseValueType, true);
    }
    IR::Opnd * vtableOpnd;
    IR::Opnd * vtableVirtualOpnd = nullptr;
    if (isStore &&
        (vtableAddress == VTableValue::VtableJavascriptArray ||
         baseValueType.IsLikelyNativeArray()))
    {
        vtableOpnd = IR::RegOpnd::New(TyMachPtr, func);
        if (baseValueType.IsLikelyNativeArray())
        {
            if (baseValueType.HasIntElements())
            {
                InsertMove(vtableOpnd, this->LoadOptimizationOverridesValueOpnd(insertBeforeInstr, OptimizationOverridesValue::OptimizationOverridesIntArraySetElementFastPathVtable), insertBeforeInstr);
            }
            else
            {
                Assert(baseValueType.HasFloatElements());
                InsertMove(vtableOpnd, this->LoadOptimizationOverridesValueOpnd(insertBeforeInstr, OptimizationOverridesValue::OptimizationOverridesFloatArraySetElementFastPathVtable), insertBeforeInstr);
            }
        }
        else
        {
            InsertMove(vtableOpnd, this->LoadOptimizationOverridesValueOpnd(insertBeforeInstr, OptimizationOverridesValue::OptimizationOverridesArraySetElementFastPathVtable), insertBeforeInstr);
        }
    }
    else
    {
        vtableOpnd = LoadVTableValueOpnd(insertBeforeInstr, vtableAddress);
    }

    // cmp  [array], vtableAddress
    // jne  $isNotArrayLabel

    if (forceFloat && baseValueType.IsLikelyNativeFloatArray())
    {
        // We expect a native float array. If we get native int instead, convert it on the spot and bail out afterward.
        const auto goodArrayLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);

        InsertCompareBranch(
            IR::IndirOpnd::New(arrayOpnd, 0, TyMachPtr, func),
            vtableOpnd,
            Js::OpCode::BrEq_A,
            goodArrayLabel,
            insertBeforeInstr);

        IR::LabelInstr *notFloatArrayLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
        insertBeforeInstr->InsertBefore(notFloatArrayLabel);

        if (isStore)
        {
            vtableOpnd = IR::RegOpnd::New(TyMachPtr, func);
            InsertMove(vtableOpnd, IR::MemRefOpnd::New(
                           func->GetScriptContext()->optimizationOverrides.GetAddressOfIntArraySetElementFastPathVtable(),
                           TyMachPtr, func), insertBeforeInstr);
        }
        else
        {
            vtableOpnd = LoadVTableValueOpnd(insertBeforeInstr, VTableValue::VtableJavascriptNativeIntArray);
        }

        InsertCompareBranch(
            IR::IndirOpnd::New(arrayOpnd, 0, TyMachPtr, func),
            vtableOpnd,
            Js::OpCode::BrNeq_A,
            isNotArrayLabel,
            insertBeforeInstr);

        m_lowererMD.LoadHelperArgument(insertBeforeInstr, arrayOpnd);

        IR::Instr *helperInstr = IR::Instr::New(Js::OpCode::Call, m_func);
        insertBeforeInstr->InsertBefore(helperInstr);
        m_lowererMD.ChangeToHelperCall(helperInstr, IR::HelperIntArr_ToNativeFloatArray);

        // Branch to the (bailout) label, because converting the array may have made our array checks unsafe.
        InsertBranch(Js::OpCode::Br, isNotArrayLabel, insertBeforeInstr);

        insertBeforeInstr->InsertBefore(goodArrayLabel);
    }
    else
    {
        IR::LabelInstr* goodArrayLabel = nullptr;
        if (baseValueType.IsLikelyMixedTypedArrayType())
        {
            goodArrayLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
            InsertCompareBranch(
                IR::IndirOpnd::New(arrayOpnd, 0, TyMachPtr, func),
                vtableOpnd,
                Js::OpCode::BrEq_A,
                goodArrayLabel,
                insertBeforeInstr);
            Assert(virtualVtableAddress);
            vtableVirtualOpnd = LoadVTableValueOpnd(insertBeforeInstr, virtualVtableAddress);
            Assert(vtableVirtualOpnd);
            InsertCompareBranch(
                IR::IndirOpnd::New(arrayOpnd, 0, TyMachPtr, func),
                vtableVirtualOpnd,
                Js::OpCode::BrNeq_A,
                isNotArrayLabel,
                insertBeforeInstr);
            insertBeforeInstr->InsertBefore(goodArrayLabel);
        }
        else
        {
            InsertCompareBranch(
                IR::IndirOpnd::New(arrayOpnd, 0, TyMachPtr, func),
                vtableOpnd,
                Js::OpCode::BrNeq_A,
                isNotArrayLabel,
                insertBeforeInstr);
        }

    }

    ValueType arrayValueType(arrayOpnd->GetValueType());
    if(arrayValueType.IsLikelyArrayOrObjectWithArray() && !arrayValueType.IsObject())
    {
        arrayValueType = arrayValueType.SetHasNoMissingValues(false);
    }
    arrayValueType = arrayValueType.ToDefiniteObject();
    arrayOpnd->SetValueType(arrayValueType);
    return arrayOpnd;
}

IR::LabelInstr *Lowerer::InsertLabel(const bool isHelper, IR::Instr *const insertBeforeInstr)
{
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::LabelInstr *const instr = IR::LabelInstr::New(Js::OpCode::Label, func, isHelper);

    insertBeforeInstr->InsertBefore(instr);
    return instr;
}

IR::Instr *Lowerer::InsertMoveWithBarrier(IR::Opnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr)
{
    return Lowerer::InsertMove(dst, src, insertBeforeInstr, true);
}

IR::Instr *Lowerer::InsertMove(IR::Opnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr, bool generateWriteBarrier)
{
    Assert(dst);
    Assert(src);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    if(dst->IsFloat() && src->IsConstOpnd())
    {
        return LoadFloatFromNonReg(src, dst, insertBeforeInstr);
    }

    if(TySize[dst->GetType()] < TySize[src->GetType()])
    {
        src = src->UseWithNewType(dst->GetType(), func);
    }
    IR::Instr *const instr = IR::Instr::New(Js::OpCode::Ld_A, dst, src, func);

    insertBeforeInstr->InsertBefore(instr);
    if (generateWriteBarrier)
    {
        LowererMD::ChangeToWriteBarrierAssign(instr);
    }
    else
    {
        LowererMD::ChangeToAssign(instr);
    }

    return instr;
}

IR::BranchInstr *Lowerer::InsertBranch(
    const Js::OpCode opCode,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr)
{
    return InsertBranch(opCode, false /* isUnsigned */, target, insertBeforeInstr);
}

IR::BranchInstr *Lowerer::InsertBranch(
    const Js::OpCode opCode,
    const bool isUnsigned,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr)
{
    Assert(target);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::BranchInstr *const instr = IR::BranchInstr::New(opCode, target, func);
    if(!instr->IsLowered())
    {
        if(opCode == Js::OpCode::Br)
        {
            instr->m_opcode = LowererMD::MDUncondBranchOpcode;
        }
        else if(isUnsigned)
        {
            instr->m_opcode = LowererMD::MDUnsignedBranchOpcode(opCode);
        }
        else
        {
            instr->m_opcode = LowererMD::MDBranchOpcode(opCode);
        }
    }

    insertBeforeInstr->InsertBefore(instr);
    return instr;
}

IR::Instr *Lowerer::InsertCompare(IR::Opnd *const src1, IR::Opnd *const src2, IR::Instr *const insertBeforeInstr)
{
    Assert(src1);
    Assert(!src1->IsFloat64()); // not implemented
    Assert(src2);
    Assert(!src2->IsFloat64()); // not implemented
    Assert(!src1->IsEqual(src2));
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(Js::OpCode::CMP, func);
    instr->SetSrc1(src1);
    instr->SetSrc2(src2);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::Legalize(instr);
    return instr;
}

IR::BranchInstr *Lowerer::InsertCompareBranch(
    IR::Opnd *const compareSrc1,
    IR::Opnd *const compareSrc2,
    Js::OpCode branchOpCode,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr,
    const bool ignoreNaN)
{
    return InsertCompareBranch(compareSrc1, compareSrc2, branchOpCode, false /* isUnsigned */, target, insertBeforeInstr, ignoreNaN);
}

IR::BranchInstr *Lowerer::InsertCompareBranch(
    IR::Opnd *compareSrc1,
    IR::Opnd *compareSrc2,
    Js::OpCode branchOpCode,
    const bool isUnsigned,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr,
    const bool ignoreNaN)
{
    Assert(compareSrc1);
    Assert(compareSrc2);

    Func *const func = insertBeforeInstr->m_func;

    if(compareSrc1->IsFloat64())
    {
        Assert(compareSrc2->IsFloat64());
        Assert(!isUnsigned);
        IR::BranchInstr *const instr = IR::BranchInstr::New(branchOpCode, target, compareSrc1, compareSrc2, func);
        insertBeforeInstr->InsertBefore(instr);
        return LowererMD::LowerFloatCondBranch(instr, ignoreNaN);
    }

    Js::OpCode swapSrcsBranchOpCode;
    switch(branchOpCode)
    {
        case Js::OpCode::BrEq_A:
        case Js::OpCode::BrNeq_A:
            swapSrcsBranchOpCode = branchOpCode;
            goto Common_BrEqNeqGeGtLeLt;

        case Js::OpCode::BrGe_A:
            swapSrcsBranchOpCode = Js::OpCode::BrLe_A;
            goto Common_BrEqNeqGeGtLeLt;

        case Js::OpCode::BrGt_A:
            swapSrcsBranchOpCode = Js::OpCode::BrLt_A;
            goto Common_BrEqNeqGeGtLeLt;

        case Js::OpCode::BrLe_A:
            swapSrcsBranchOpCode = Js::OpCode::BrGe_A;
            goto Common_BrEqNeqGeGtLeLt;

        case Js::OpCode::BrLt_A:
            swapSrcsBranchOpCode = Js::OpCode::BrGt_A;
            // fall through

        Common_BrEqNeqGeGtLeLt:
            // Check if src1 is a constant and src2 is not, and facilitate folding the constant into the Cmp instruction
            if( (
                    compareSrc1->IsIntConstOpnd() ||
                    (
                        compareSrc1->IsAddrOpnd() &&
                        Math::FitsInDWord(reinterpret_cast<size_t>(compareSrc1->AsAddrOpnd()->m_address))
                    )
                ) &&
                !compareSrc2->IsIntConstOpnd() &&
                !compareSrc2->IsAddrOpnd())
            {
                // Swap the sources and branch
                IR::Opnd *const tempSrc = compareSrc1;
                compareSrc1 = compareSrc2;
                compareSrc2 = tempSrc;
                branchOpCode = swapSrcsBranchOpCode;
            }

            // Check for compare with zero, to prefer using Test instead of Cmp
            if( !compareSrc1->IsRegOpnd() ||
                !(
                    compareSrc2->IsIntConstOpnd() && compareSrc2->AsIntConstOpnd()->GetValue() == 0 ||
                    compareSrc2->IsAddrOpnd() && !compareSrc2->AsAddrOpnd()->m_address
                ) ||
                branchOpCode == Js::OpCode::BrGt_A || branchOpCode == Js::OpCode::BrLe_A)
            {
                goto Default;
            }
            if(branchOpCode == Js::OpCode::BrGe_A || branchOpCode == Js::OpCode::BrLt_A)
            {
                if(isUnsigned)
                {
                    goto Default;
                }
                branchOpCode = LowererMD::MDCompareWithZeroBranchOpcode(branchOpCode);
            }
            if(!compareSrc2->IsInUse())
            {
                compareSrc2->Free(func);
            }
            InsertTest(compareSrc1, compareSrc1, insertBeforeInstr);
            break;

        default:
        Default:
            InsertCompare(compareSrc1, compareSrc2, insertBeforeInstr);
            break;
    }

    return InsertBranch(branchOpCode, isUnsigned, target, insertBeforeInstr);
}

IR::Instr *Lowerer::InsertTest(IR::Opnd *const src1, IR::Opnd *const src2, IR::Instr *const insertBeforeInstr)
{
    Assert(src1);
    Assert(!src1->IsFloat64()); // not implemented
    Assert(src2);
    Assert(!src2->IsFloat64()); // not implemented
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(LowererMD::MDTestOpcode, func);
    instr->SetSrc1(src1);
    instr->SetSrc2(src2);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::Legalize(instr);
    return instr;
}

IR::BranchInstr *Lowerer::InsertTestBranch(
    IR::Opnd *const testSrc1,
    IR::Opnd *const testSrc2,
    const Js::OpCode branchOpCode,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr)
{
    return InsertTestBranch(testSrc1, testSrc2, branchOpCode, false /* isUnsigned */, target, insertBeforeInstr);
}

IR::BranchInstr *Lowerer::InsertTestBranch(
    IR::Opnd *const testSrc1,
    IR::Opnd *const testSrc2,
    const Js::OpCode branchOpCode,
    const bool isUnsigned,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr)
{
    InsertTest(testSrc1, testSrc2, insertBeforeInstr);
    return InsertBranch(branchOpCode, isUnsigned, target, insertBeforeInstr);
}

IR::Instr *Lowerer::InsertAdd(
    const bool needFlags,
    IR::Opnd *const dst,
    IR::Opnd *src1,
    IR::Opnd *src2,
    IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(src1);
    Assert(src2);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    if(src2->IsIntConstOpnd())
    {
        IR::IntConstOpnd *const intConstOpnd = src2->AsIntConstOpnd();
        const IntConstType value = intConstOpnd->GetValue();
        if(value < 0 && value != IntConstMin)
        {
            // Change (s1 = s1 + -5) into (s1 = s1 - 5)
            IR::IntConstOpnd *const newSrc2 = intConstOpnd->CopyInternal(func);
            newSrc2->SetValue(-value);
            return InsertSub(needFlags, dst, src1, newSrc2, insertBeforeInstr);
        }
    }
    else if(src1->IsIntConstOpnd())
    {
        IR::IntConstOpnd *const intConstOpnd = src1->AsIntConstOpnd();
        const IntConstType value = intConstOpnd->GetValue();
        if(value < 0 && value != IntConstMin)
        {
            // Change (s1 = -5 + s1) into (s1 = s1 - 5)
            IR::Opnd *const newSrc1 = src2;
            IR::IntConstOpnd *const newSrc2 = intConstOpnd->CopyInternal(func);
            newSrc2->SetValue(-value);
            return InsertSub(needFlags, dst, newSrc1, newSrc2, insertBeforeInstr);
        }
    }

    IR::Instr *const instr = IR::Instr::New(Js::OpCode::Add_A, dst, src1, src2, func);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::ChangeToAdd(instr, needFlags);
    LowererMD::Legalize(instr);
    return instr;
}

IR::Instr *Lowerer::InsertSub(
    const bool needFlags,
    IR::Opnd *const dst,
    IR::Opnd *src1,
    IR::Opnd *src2,
    IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(src1);
    Assert(src2);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    if(src2->IsIntConstOpnd())
    {
        IR::IntConstOpnd *const intConstOpnd = src2->AsIntConstOpnd();
        const IntConstType value = intConstOpnd->GetValue();
        if(value < 0 && value != IntConstMin)
        {
            // Change (s1 = s1 - -5) into (s1 = s1 + 5)
            IR::IntConstOpnd *const newSrc2 = intConstOpnd->CopyInternal(func);
            newSrc2->SetValue(-value);
            return InsertAdd(needFlags, dst, src1, newSrc2, insertBeforeInstr);
        }
    }

    IR::Instr *const instr = IR::Instr::New(Js::OpCode::Sub_A, dst, src1, src2, func);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::ChangeToSub(instr, needFlags);
    LowererMD::Legalize(instr);
    return instr;
}

IR::Instr *Lowerer::InsertLea(IR::RegOpnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(src);
    Assert(src->IsIndirOpnd() || src->IsSymOpnd());
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(Js::OpCode::LEA, dst, src, func);

    insertBeforeInstr->InsertBefore(instr);
    return LowererMD::ChangeToLea(instr);
}

IR::Instr *Lowerer::InsertAnd(
    IR::Opnd *const dst,
    IR::Opnd *const src1,
    IR::Opnd *const src2,
    IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(src1);
    Assert(src2);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(Js::OpCode::AND, dst, src1, src2, func);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::Legalize(instr);
    return instr;
}

IR::Instr *Lowerer::InsertOr(
    IR::Opnd *const dst,
    IR::Opnd *const src1,
    IR::Opnd *const src2,
    IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(src1);
    Assert(src2);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(LowererMD::MDOrOpcode, dst, src1, src2, func);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::Legalize(instr);
    return instr;
}

IR::Instr *Lowerer::InsertShift(
    const Js::OpCode opCode,
    const bool needFlags,
    IR::Opnd *const dst,
    IR::Opnd *const src1,
    IR::Opnd *const src2,
    IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(!dst->IsFloat64()); // not implemented
    Assert(src1);
    Assert(!src1->IsFloat64()); // not implemented
    Assert(src2);
    Assert(!src2->IsFloat64()); // not implemented
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(opCode, dst, src1, src2, func);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::ChangeToShift(instr, needFlags);
    LowererMD::Legalize(instr);
    return instr;
}

IR::Instr *Lowerer::InsertShiftBranch(
    const Js::OpCode shiftOpCode,
    IR::Opnd *const dst,
    IR::Opnd *const src1,
    IR::Opnd *const src2,
    const Js::OpCode branchOpCode,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr)
{
    return InsertShiftBranch(shiftOpCode, dst, src1, src2, branchOpCode, false /* isUnsigned */, target, insertBeforeInstr);
}

IR::Instr *Lowerer::InsertShiftBranch(
    const Js::OpCode shiftOpCode,
    IR::Opnd *const dst,
    IR::Opnd *const src1,
    IR::Opnd *const src2,
    const Js::OpCode branchOpCode,
    const bool isUnsigned,
    IR::LabelInstr *const target,
    IR::Instr *const insertBeforeInstr)
{
    InsertShift(shiftOpCode, true /* needFlags */, dst, src1, src2, insertBeforeInstr);
    return InsertBranch(branchOpCode, isUnsigned, target, insertBeforeInstr);
}

IR::Instr *Lowerer::InsertConvertFloat32ToFloat64(
    IR::Opnd *const dst,
    IR::Opnd *const src,
    IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(dst->IsFloat64());
    Assert(src);
    Assert(src->IsFloat32());
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(LowererMD::MDConvertFloat32ToFloat64Opcode, dst, src, func);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::Legalize(instr);
    return instr;
}

IR::Instr *Lowerer::InsertConvertFloat64ToFloat32(
    IR::Opnd *const dst,
    IR::Opnd *const src,
    IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(dst->IsFloat32());
    Assert(src);
    Assert(src->IsFloat64());
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::Instr *const instr = IR::Instr::New(LowererMD::MDConvertFloat64ToFloat32Opcode, dst, src, func);

    insertBeforeInstr->InsertBefore(instr);
    LowererMD::Legalize(instr);
    return instr;
}

void Lowerer::InsertIncUInt8PreventOverflow(
    IR::Opnd *const dst,
    IR::Opnd *const src,
    IR::Instr *const insertBeforeInstr,
    IR::Instr * *const onOverflowInsertBeforeInstrRef)
{
    LowererMD::InsertIncUInt8PreventOverflow(dst, src, insertBeforeInstr, onOverflowInsertBeforeInstrRef);
}

void Lowerer::InsertDecUInt8PreventOverflow(
    IR::Opnd *const dst,
    IR::Opnd *const src,
    IR::Instr *const insertBeforeInstr,
    IR::Instr * *const onOverflowInsertBeforeInstrRef)
{
    LowererMD::InsertDecUInt8PreventOverflow(dst, src, insertBeforeInstr, onOverflowInsertBeforeInstrRef);
}

void Lowerer::InsertFloatCheckForZeroOrNanBranch(
    IR::Opnd *const src,
    const bool branchOnZeroOrNan,
    IR::LabelInstr *const target,
    IR::LabelInstr *const fallthroughLabel,
    IR::Instr *const insertBeforeInstr)
{
    Assert(src);
    Assert(src->IsFloat64());
    Assert(target);
    Assert(!fallthroughLabel || fallthroughLabel != target);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    IR::BranchInstr *const branchOnEqualOrNotEqual =
        InsertCompareBranch(
            src,
            IR::MemRefOpnd::New((double*)&(Js::JavascriptNumber::k_Zero), TyFloat64, func),
            branchOnZeroOrNan ? Js::OpCode::BrEq_A : Js::OpCode::BrNeq_A,
            target,
            insertBeforeInstr,
            true /* ignoreNaN */);

    // x86/x64
    //     When NaN is ignored, on x86 and x64, JE branches when equal or unordered since an unordered result sets the zero
    //     flag, and JNE branches when not equal and not unordered. By comparing with zero, JE will branch when src is zero or
    //     NaN, and JNE will branch when src is not zero and not NaN.
    //
    // ARM
    //     When NaN is ignored, BEQ branches when equal and not unordered, and BNE branches when not equal or unordered. So,
    //     when comparing src with zero, an unordered check needs to be added before the BEQ/BNE.
    branchOnEqualOrNotEqual; // satisfy the compiler
#ifdef _M_ARM
    InsertBranch(
        Js::OpCode::BVS,
        branchOnZeroOrNan
            ? target
            : fallthroughLabel ? fallthroughLabel : insertBeforeInstr->m_prev->GetOrCreateContinueLabel(),
        branchOnEqualOrNotEqual);
#endif
}

IR::IndirOpnd *
Lowerer::GenerateFastElemICommon(
    IR::Instr * ldElem,
    bool isStore,
    IR::IndirOpnd * indirOpnd,
    IR::LabelInstr * labelHelper,
    IR::LabelInstr * labelCantUseArray,
    IR::LabelInstr *labelFallthrough,
    bool * pIsTypedArrayElement,
    bool * pIsStringIndex,
    bool *emitBailoutRef,
    IR::LabelInstr **pLabelSegmentLengthIncreased /*= nullptr*/,
    bool checkArrayLengthOverflow /*= true*/,
    bool forceGenerateFastPath /* = false */,
    bool returnLength,
    IR::LabelInstr *bailOutLabelInstr /* = nullptr*/)
{
    *pIsTypedArrayElement = false;
    *pIsStringIndex = false;
    if(pLabelSegmentLengthIncreased)
    {
        *pLabelSegmentLengthIncreased = nullptr;
    }
    IR::RegOpnd *baseOpnd = indirOpnd->GetBaseOpnd();
    AssertMsg(baseOpnd, "This shouldn't be NULL");

    // Caution: If making changes to the conditions under which we don't emit the typical array checks, make sure
    // the code in GlobOpt::ShouldAssumeIndirOpndHasNonNegativeIntIndex is updated accordingly.  We don't want the
    // global optimizer to type specialize instructions, for which the lowerer is forced to emit unconditional
    // bailouts.
    if (baseOpnd->IsTaggedInt())
    {
        return NULL;
    }

    IR::RegOpnd *indexOpnd = indirOpnd->GetIndexOpnd();
    if (indexOpnd)
    {
        if (indexOpnd->GetValueType().IsString())
        {
            if (!baseOpnd->GetValueType().IsLikelyOptimizedTypedArray())
            {
                // If profile data says that it's a typed array - do not generate the property string fast path as the src. could be a temp and that would cause a bug.
                *pIsTypedArrayElement = false;
                *pIsStringIndex = true;
                return m_lowererMD.GenerateFastElemIStringIndexCommon(ldElem, isStore, indirOpnd, labelHelper);
            }
            else
            {
                // There's no point in generating the int index fast path if we know the index has a string value.
                return nullptr;
            }
        }
    }
    return
        GenerateFastElemIIntIndexCommon(
            ldElem,
            isStore,
            indirOpnd,
            labelHelper,
            labelCantUseArray,
            labelFallthrough,
            pIsTypedArrayElement,
            emitBailoutRef,
            pLabelSegmentLengthIncreased,
            checkArrayLengthOverflow,
            false,
            returnLength,
            bailOutLabelInstr);
}

IR::IndirOpnd *
Lowerer::GenerateFastElemIIntIndexCommon(
    IR::Instr * ldElem,
    bool isStore,
    IR::IndirOpnd * indirOpnd,
    IR::LabelInstr * labelHelper,
    IR::LabelInstr * labelCantUseArray,
    IR::LabelInstr *labelFallthrough,
    bool * pIsTypedArrayElement,
    bool *emitBailoutRef,
    IR::LabelInstr **pLabelSegmentLengthIncreased,
    bool checkArrayLengthOverflow /*= true*/,
    bool forceGenerateFastPath /* = false */,
    bool returnLength,
    IR::LabelInstr *bailOutLabelInstr /* = nullptr*/)
{
    IR::RegOpnd *indexOpnd = indirOpnd->GetIndexOpnd();
    IR::RegOpnd *baseOpnd = indirOpnd->GetBaseOpnd();
    Assert(!baseOpnd->IsTaggedInt() || (indexOpnd && indexOpnd->IsNotInt()));

    BYTE indirScale = this->m_lowererMD.GetDefaultIndirScale();
    IRType indirType = TyVar;
    const ValueType baseValueType(baseOpnd->GetValueType());

    //  TEST base, AtomTag                  -- check base not tagged int
    //  JNE $helper
    //  if (base.GetValueType() != Array) {
    //      CMP [base], JavascriptArray::`vtable'
    //      JNE $helper
    //  }
    //  TEST index, 1                       -- index tagged int
    //  JEQ $helper
    //  if (inputIndex is not int const) {
    //      MOV index, inputIndex
    //      SAR index, Js::VarTag_Shift            -- remote atom tag
    //      JS $helper                          -- exclude negative index
    //  }
    //  MOV headSegment, [base + offset(head)]
    //  CMP [headSegment + offset(length)], index       -- bounds check
    //  if (opcode == StElemI_A) {
    //      JA  $done (for typedarray, JA $toNumberHelper)
    //      CMP [headSegment + offset(size)], index         -- chunk has room?
    //      JBE $helper
    //      if (index is not int const) {
    //          LEA newLength, [index + 1]
    //      } else {
    //          newLength = index + 1
    //      }
    //      MOV [headSegment + offset(length)], newLength       -- update length on chunk
    //      CMP [base  + offset(length)], newLength
    //      JAE $done
    //      MOV [base + offset(length)], newLength     -- update length on array
    //      if(length to be returned){
    //          SHL newLength, AtomTag
    //          INC newLength
    //          MOV dst, newLength
    //      }
    //     JMP $done
    //
    //     $toNumberHelper: Call HelperOp_ConvNumber_Full
    //     JMP $done
    //     $done
    //  } else {la
    //     JBE $helper
    //  }
    // return [headSegment + offset(elements) + index]

    // Caution: If making changes to the conditions under which we don't emit the typical array checks, make sure
    // the code in GlobOpt::ShouldAssumeIndirOpndHasNonNegativeIntIndex is updated accordingly.  We don't want the
    // global optimizer to type specialize instructions, for which the lowerer is forced to emit unconditional
    // bailouts.
    bool isIndexNotInt = false;
    IntConstType value = 0;
    IR::Opnd * indexValueOpnd = nullptr;
    bool invertBoundCheckComparison = false;

    if (indirOpnd->TryGetIntConstIndexValue(true, &value, &isIndexNotInt))
    {
        if (value >= 0)
        {
            indexValueOpnd = IR::IntConstOpnd::New(value, TyUint32, this->m_func);
            invertBoundCheckComparison = true; // facilitate folding the constant index into the compare instruction
        }
        else
        {
            // If the index is a negative int constant we go directly to helper.
            Assert(!forceGenerateFastPath);
            return nullptr;
        }
    }
    else if (isIndexNotInt)
    {
        // If we know the index is not an int we go directly to helper.
        Assert(!forceGenerateFastPath);
        return nullptr;
    }

    //At this point indexValueOpnd is either NULL or contains the valueOpnd

    if(!forceGenerateFastPath && !ShouldGenerateArrayFastPath(baseOpnd, true, true, true))
    {
        return nullptr;
    }

    if(baseValueType.IsLikelyAnyOptimizedArray())
    {
        indirScale = GetArrayIndirScale(baseValueType);
        indirType = GetArrayIndirType(baseValueType);
    }

    IRType elementType = TyIllegal;
    IR::Opnd * element = nullptr;

    if(ldElem->m_opcode == Js::OpCode::InlineArrayPush)
    {
        element = ldElem->GetSrc2();
        elementType = element->GetType();
    }
    else if(isStore && ldElem->GetSrc1())
    {
        element = ldElem->GetSrc1();
        elementType = element->GetType();
    }

    Assert(isStore || (element == nullptr && elementType == TyIllegal));

    if (isStore && baseValueType.IsLikelyNativeArray() && indirType != elementType)
    {
        // We're trying to write a value of the wrong type, which should force a conversion of the array.
        // Go to the helper for that.
        return nullptr;
    }

    IR::RegOpnd *arrayOpnd = baseOpnd;
    IR::RegOpnd *headSegmentOpnd = nullptr;
    IR::Opnd *headSegmentLengthOpnd = nullptr;
    IR::AutoReuseOpnd autoReuseHeadSegmentOpnd, autoReuseHeadSegmentLengthOpnd;
    bool indexIsNonnegative = indexValueOpnd || indexOpnd->GetType() == TyUint32 || !checkArrayLengthOverflow;
    bool indexIsLessThanHeadSegmentLength = false;
    if(!baseValueType.IsAnyOptimizedArray())
    {
        arrayOpnd = GenerateArrayTest(baseOpnd, labelCantUseArray, labelCantUseArray, ldElem, true, isStore);
    }
    else
    {
        if(arrayOpnd->IsArrayRegOpnd())
        {
            IR::ArrayRegOpnd *const arrayRegOpnd = arrayOpnd->AsArrayRegOpnd();
            if(arrayRegOpnd->HeadSegmentSym())
            {
                headSegmentOpnd = IR::RegOpnd::New(arrayRegOpnd->HeadSegmentSym(), TyMachPtr, m_func);
                DebugOnly(headSegmentOpnd->FreezeSymValue());
                autoReuseHeadSegmentOpnd.Initialize(headSegmentOpnd, m_func);
            }
            if(arrayRegOpnd->HeadSegmentLengthSym())
            {
                headSegmentLengthOpnd = IR::RegOpnd::New(arrayRegOpnd->HeadSegmentLengthSym(), TyUint32, m_func);
                DebugOnly(headSegmentLengthOpnd->AsRegOpnd()->FreezeSymValue());
                autoReuseHeadSegmentLengthOpnd.Initialize(headSegmentLengthOpnd, m_func);
            }
            if (arrayRegOpnd->EliminatedLowerBoundCheck())
            {
                indexIsNonnegative = true;
            }
            if(arrayRegOpnd->EliminatedUpperBoundCheck())
            {
                indexIsLessThanHeadSegmentLength = true;
            }
        }
    }
    IR::AutoReuseOpnd autoReuseArrayOpnd;
    if(arrayOpnd->GetValueType().GetObjectType() != ObjectType::ObjectWithArray)
    {
        autoReuseArrayOpnd.Initialize(arrayOpnd, m_func);
    }
    const auto EnsureObjectArrayLoaded = [&]()
    {
        if(arrayOpnd->GetValueType().GetObjectType() != ObjectType::ObjectWithArray)
        {
            return;
        }
        arrayOpnd = LoadObjectArray(arrayOpnd, ldElem);
        autoReuseArrayOpnd.Initialize(arrayOpnd, m_func);
    };

    const bool doUpperBoundCheck = checkArrayLengthOverflow && !indexIsLessThanHeadSegmentLength;
    if(!indexValueOpnd)
    {
        indexValueOpnd =
        m_lowererMD.LoadNonnegativeIndex(
            indexOpnd,
            (
                indexIsNonnegative
            #if !INT32VAR
                ||
                // On 32-bit platforms, skip the negative check since for now, the unsigned upper bound check covers it
                doUpperBoundCheck
            #endif
            ),
            labelCantUseArray,
            labelHelper,
            ldElem);
    }
    const IR::AutoReuseOpnd autoReuseIndexValueOpnd(indexValueOpnd, m_func);

    if (baseValueType.IsLikelyTypedArray())
    {
        *pIsTypedArrayElement = true;

        if(doUpperBoundCheck)
        {
            if(!headSegmentLengthOpnd)
            {
                // (headSegmentLength = [base + offset(length)])
                int lengthOffset;
                lengthOffset = Js::Float64Array::GetOffsetOfLength();
                headSegmentLengthOpnd = IR::IndirOpnd::New(arrayOpnd, lengthOffset, TyUint32, m_func);
                autoReuseHeadSegmentLengthOpnd.Initialize(headSegmentLengthOpnd, m_func);
            }

            //  CMP index, headSegmentLength  -- upper bound check
            if(!invertBoundCheckComparison)
            {
                InsertCompare(indexValueOpnd, headSegmentLengthOpnd, ldElem);
            }
            else
            {
                InsertCompare(headSegmentLengthOpnd, indexValueOpnd, ldElem);
            }
        }
    }
    else
    {
        *pIsTypedArrayElement = false;

        if (isStore &&
            baseValueType.IsLikelyNativeIntArray() &&
            (!element->IsIntConstOpnd() || Js::SparseArraySegment<int32>::GetMissingItem() == element->AsIntConstOpnd()->AsInt32()))
        {
            Assert(ldElem->m_opcode != Js::OpCode::InlineArrayPush || bailOutLabelInstr);

            // Check for a write of the MissingItem value.
            InsertCompareBranch(
                element,
                GetMissingItemOpnd(elementType, m_func),
                Js::OpCode::BrEq_A,
                ldElem->m_opcode == Js::OpCode::InlineArrayPush ? bailOutLabelInstr : labelCantUseArray,
                ldElem,
                true);
        }

        if(!headSegmentOpnd)
        {
            EnsureObjectArrayLoaded();

            //  MOV headSegment, [base + offset(head)]
            indirOpnd = IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfHead(), TyMachPtr, this->m_func);
            headSegmentOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
            autoReuseHeadSegmentOpnd.Initialize(headSegmentOpnd, m_func);
            InsertMove(headSegmentOpnd, indirOpnd, ldElem);
        }

        if(doUpperBoundCheck)
        {
            if(!headSegmentLengthOpnd)
            {
                // (headSegmentLength = [headSegment + offset(length)])
                headSegmentLengthOpnd =
                    IR::IndirOpnd::New(headSegmentOpnd, Js::SparseArraySegmentBase::GetOffsetOfLength(), TyUint32, m_func);
                autoReuseHeadSegmentLengthOpnd.Initialize(headSegmentLengthOpnd, m_func);
            }

            //  CMP index, headSegmentLength  -- upper bound check
            if(!invertBoundCheckComparison)
            {
                InsertCompare(indexValueOpnd, headSegmentLengthOpnd, ldElem);
            }
            else
            {
                InsertCompare(headSegmentLengthOpnd, indexValueOpnd, ldElem);
            }
        }
    }

    const IR::BailOutKind bailOutKind = ldElem->HasBailOutInfo() ? ldElem->GetBailOutKind() : IR::BailOutInvalid;
    if(indexIsLessThanHeadSegmentLength ||
        bailOutKind & (IR::BailOutOnArrayAccessHelperCall | IR::BailOutOnInvalidatedArrayHeadSegment))
    {
        if(bailOutKind & (IR::BailOutOnArrayAccessHelperCall | IR::BailOutOnInvalidatedArrayHeadSegment))
        {
            // The bailout must be pre-op because it will not have completed the operation
            Assert(ldElem->GetBailOutInfo()->bailOutOffset == ldElem->GetByteCodeOffset());

            // Verify other bailouts these can be combined with
            Assert(
                !(
                    bailOutKind &
                    IR::BailOutKindBits &
                    ~(
                        IR::BailOutOnArrayAccessHelperCall |
                        IR::BailOutOnInvalidatedArrayHeadSegment |
                        IR::BailOutOnInvalidatedArrayLength |
                        IR::BailOutConventionalNativeArrayAccessOnly |
                        (bailOutKind & IR::BailOutOnArrayAccessHelperCall ? IR::BailOutInvalid : IR::BailOutConvertedNativeArray)
                    )
                ));

            if(bailOutKind & IR::BailOutOnArrayAccessHelperCall)
            {
                // Omit the helper call and generate a bailout instead
                Assert(emitBailoutRef);
                *emitBailoutRef = true;
            }
        }

        if(indexIsLessThanHeadSegmentLength)
        {
            Assert(!(bailOutKind & IR::BailOutOnInvalidatedArrayHeadSegment));
        }
        else
        {
            IR::LabelInstr *bailOutLabel;
            if(bailOutKind & IR::BailOutOnInvalidatedArrayHeadSegment)
            {
                Assert(isStore);

                // Lower a separate (but shared) bailout for this case, and preserve the bailout kind in the instruction if the
                // helper call is going to be generated, because the bailout kind needs to be lowered again and differently in the
                // helper call path.
                //
                // Generate:
                //     (LdElem)
                //     jmp $continue
                //   $bailOut:
                //     Bail out with IR::BailOutOnInvalidatedArrayHeadSegment
                //   $continue:
                LowerOneBailOutKind(
                    ldElem,
                    IR::BailOutOnInvalidatedArrayHeadSegment,
                    false,
                    !(bailOutKind & IR::BailOutOnArrayAccessHelperCall));
                bailOutLabel = ldElem->GetOrCreateContinueLabel(true);
                InsertBranch(Js::OpCode::Br, labelFallthrough, bailOutLabel);
            }
            else
            {
                Assert(bailOutKind & IR::BailOutOnArrayAccessHelperCall);
                bailOutLabel = labelHelper;
            }

            // Bail out if the index is outside the head segment bounds
            //     jae $bailOut
            Assert(checkArrayLengthOverflow);
            InsertBranch(
                !invertBoundCheckComparison ? Js::OpCode::BrGe_A : Js::OpCode::BrLe_A,
                true /* isUnsigned */,
                bailOutLabel,
                ldElem);
        }
    }
    else if (isStore && !baseValueType.IsLikelyTypedArray()) //  #if (opcode == StElemI_A)
    {
        IR::LabelInstr *labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        IR::LabelInstr *labelSegmentLengthIncreased = nullptr;

        const bool isPush = ldElem->m_opcode != Js::OpCode::StElemI_A && ldElem->m_opcode != Js::OpCode::StElemI_A_Strict;

        // Put the head segment size check and length updates in a helper block since they're not the common path for StElem.
        // For push, that is the common path so keep it in a non-helper block.
        const bool isInHelperBlock = !isPush;

        if(checkArrayLengthOverflow)
        {
            if(pLabelSegmentLengthIncreased &&
                !(
                    baseValueType.IsArrayOrObjectWithArray() && baseValueType.HasNoMissingValues() ||
                    (ldElem->m_opcode == Js::OpCode::StElemI_A || ldElem->m_opcode == Js::OpCode::StElemI_A_Strict) &&
                        ldElem->IsProfiledInstr() && !ldElem->AsProfiledInstr()->u.stElemInfo->LikelyFillsMissingValue()
                ))
            {
                // For arrays that are not guaranteed to have no missing values, before storing to an element where
                // (index < length), the element value needs to be checked to see if it's a missing value, and if so, fall back
                // to the helper. This is done to keep the missing value tracking precise in arrays. So, create a separate label
                // for the case where the length was increased (index >= length), and pass it back to GenerateFastStElemI, which
                // will fill in the rest.
                labelSegmentLengthIncreased = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isInHelperBlock);
                *pLabelSegmentLengthIncreased = labelSegmentLengthIncreased;
            }
            else
            {
                labelSegmentLengthIncreased = labelDone;
            }

            //      JB  $done
            InsertBranch(
                !invertBoundCheckComparison ? Js::OpCode::BrLt_A : Js::OpCode::BrGt_A,
                true /* isUnsigned */,
                labelDone,
                ldElem);
        }

        if(isInHelperBlock)
        {
            InsertLabel(true /* isHelper */, ldElem);
        }

        EnsureObjectArrayLoaded();

        do // while(false);
        {
            if(checkArrayLengthOverflow)
            {
                if(ldElem->HasBailOutInfo() && ldElem->GetBailOutKind() & IR::BailOutOnMissingValue)
                {
                    // Need to bail out if this store would create a missing value. The store would cause a missing value to be
                    // created if (index > length && index < size). If (index >= size) we would go to helper anyway, and the bailout
                    // handling for this is done after the helper call, so just go to helper if (index > length).
                    //
                    // jne  $helper // branch for (cmp index, headSegmentLength)
                    InsertBranch(Js::OpCode::BrNeq_A, labelHelper, ldElem);
                }
                else
                {
                    // If (index < size) we will not call the helper, so the array flags must be updated to reflect that it no
                    // longer has no missing values.
                    //
                    //     jne  indexGreaterThanLength // branch for (cmp index, headSegmentLength)
                    //     cmp  index, [headSegment + offset(size)]
                    //     jae  $helper
                    //     jmp  indexLessThanSize
                    // indexGreaterThanLength:
                    //     cmp  index, [headSegment + offset(size)]
                    //     jae  $helper
                    //     and  [array + offsetOf(objectArrayOrFlags)], ~Js::DynamicObjectFlags::HasNoMissingValues
                    // indexLessThanSize:

                    IR::LabelInstr *const indexGreaterThanLengthLabel = InsertLabel(true /* isHelper */, ldElem);
                    IR::LabelInstr *const indexLessThanSizeLabel = InsertLabel(isInHelperBlock, ldElem);

                    //     jne  indexGreaterThanLength // branch for (cmp index, headSegmentLength)
                    //     cmp  index, [headSegment + offset(size)]
                    //     jae  $helper
                    //     jmp  indexLessThanSize
                    // indexGreaterThanLength:
                    InsertBranch(Js::OpCode::BrNeq_A, indexGreaterThanLengthLabel, indexGreaterThanLengthLabel);
                    InsertCompareBranch(
                        indexValueOpnd,
                        IR::IndirOpnd::New(headSegmentOpnd, offsetof(Js::SparseArraySegmentBase, size), TyUint32, m_func),
                        Js::OpCode::BrGe_A,
                        true /* isUnsigned */,
                        labelHelper,
                        indexGreaterThanLengthLabel);
                    InsertBranch(Js::OpCode::Br, indexLessThanSizeLabel, indexGreaterThanLengthLabel);

                    // indexGreaterThanLength:
                    //     cmp  index, [headSegment + offset(size)]
                    //     jae  $helper
                    //     and  [array + offsetOf(objectArrayOrFlags)], ~Js::DynamicObjectFlags::HasNoMissingValues
                    // indexLessThanSize:
                    InsertCompareBranch(
                        indexValueOpnd,
                        IR::IndirOpnd::New(headSegmentOpnd, offsetof(Js::SparseArraySegmentBase, size), TyUint32, m_func),
                        Js::OpCode::BrGe_A,
                        true /* isUnsigned */,
                        labelHelper,
                        indexLessThanSizeLabel);
                    CompileAssert(
                        static_cast<Js::DynamicObjectFlags>(static_cast<uint8>(Js::DynamicObjectFlags::HasNoMissingValues)) ==
                        Js::DynamicObjectFlags::HasNoMissingValues);
                    InsertAnd(
                        IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfArrayFlags(), TyUint8, m_func),
                        IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfArrayFlags(), TyUint8, m_func),
                        IR::IntConstOpnd::New(
                            static_cast<uint8>(~Js::DynamicObjectFlags::HasNoMissingValues),
                            TyUint8,
                            m_func,
                            true),
                        indexLessThanSizeLabel);

                    // indexLessThanSize:
                    break;
                }
            }

            //      CMP index, [headSegment + offset(size)]
            //      JAE $helper
            indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, offsetof(Js::SparseArraySegmentBase, size), TyUint32, this->m_func);
            InsertCompareBranch(indexValueOpnd, indirOpnd, Js::OpCode::BrGe_A, true /* isUnsigned */, labelHelper, ldElem);
        } while(false);

        if(isPush)
        {
            IR::LabelInstr *const updateLengthLabel = InsertLabel(isInHelperBlock, ldElem);

            if(!doUpperBoundCheck && !headSegmentLengthOpnd)
            {
                // (headSegmentLength = [headSegment + offset(length)])
                headSegmentLengthOpnd =
                    IR::IndirOpnd::New(headSegmentOpnd, Js::SparseArraySegmentBase::GetOffsetOfLength(), TyUint32, m_func);
                autoReuseHeadSegmentLengthOpnd.Initialize(headSegmentLengthOpnd, m_func);
            }

            // For push, it is guaranteed that (index >= length). We already know that (index < size), but we need to check if
            // (index > length) because in that case a missing value will be created and the missing value tracking in the array
            // needs to be updated.
            //
            //     cmp  index, headSegmentLength
            //     je   $updateLength
            //     and  [array + offsetOf(objectArrayOrFlags)], ~Js::DynamicObjectFlags::HasNoMissingValues
            // updateLength:
            InsertCompareBranch(
                indexValueOpnd,
                headSegmentLengthOpnd,
                Js::OpCode::BrEq_A,
                updateLengthLabel,
                updateLengthLabel);
            CompileAssert(
                static_cast<Js::DynamicObjectFlags>(static_cast<uint8>(Js::DynamicObjectFlags::HasNoMissingValues)) ==
                Js::DynamicObjectFlags::HasNoMissingValues);
            InsertAnd(
                IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfArrayFlags(), TyUint8, m_func),
                IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfArrayFlags(), TyUint8, m_func),
                IR::IntConstOpnd::New(
                    static_cast<uint8>(~Js::DynamicObjectFlags::HasNoMissingValues),
                    TyUint8,
                    m_func,
                    true),
                updateLengthLabel);
        }

        if (baseValueType.IsArrayOrObjectWithArray())
        {
            // We didn't emit an array check, but if we are going to grow the array
            // We need to go to helper if there is an ES5 array/objectarray used as prototype
            GenerateIsEnabledArraySetElementFastPathCheck(labelHelper, ldElem);
        }

        IR::Opnd *newLengthOpnd;
        IR::AutoReuseOpnd autoReuseNewLengthOpnd;
        if (indexValueOpnd->IsRegOpnd())
        {
            //      LEA newLength, [index + 1]
            newLengthOpnd = IR::RegOpnd::New(TyUint32, this->m_func);
            autoReuseNewLengthOpnd.Initialize(newLengthOpnd, m_func);
            InsertAdd(false /* needFlags */, newLengthOpnd, indexValueOpnd, IR::IntConstOpnd::New(1, TyUint32, m_func), ldElem);
        }
        else
        {
            newLengthOpnd = IR::IntConstOpnd::New(value + 1, TyUint32, this->m_func);
            autoReuseNewLengthOpnd.Initialize(newLengthOpnd, m_func);
        }

        //      MOV [headSegment + offset(length)], newLength
        indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, offsetof(Js::SparseArraySegmentBase, length), TyUint32, this->m_func);
        InsertMove(indirOpnd, newLengthOpnd, ldElem);

        if (checkArrayLengthOverflow)
        {
            //      CMP newLength, [base + offset(length)]
            //      JBE $segmentLengthIncreased
            Assert(labelSegmentLengthIncreased);
            indirOpnd = IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfLength(), TyUint32, this->m_func);
            InsertCompareBranch(
                newLengthOpnd,
                indirOpnd,
                Js::OpCode::BrLe_A,
                true /* isUnsigned */,
                labelSegmentLengthIncreased,
                ldElem);

            if(!isInHelperBlock)
            {
                InsertLabel(true /* isHelper */, ldElem);
            }
        }

        //      MOV [base + offset(length)], newLength
        indirOpnd = IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfLength(), TyUint32, this->m_func);
        InsertMove(indirOpnd, newLengthOpnd, ldElem);

        if(returnLength)
        {
            if(newLengthOpnd->GetSize() != MachPtr)
            {
                newLengthOpnd = newLengthOpnd->UseWithNewType(TyMachPtr, m_func)->AsRegOpnd();
            }

            //      SHL newLength, AtomTag
            //      INC newLength
            this->m_lowererMD.GenerateInt32ToVarConversion(newLengthOpnd, ldElem);

            //      MOV dst, newLength
            InsertMove(ldElem->GetDst(), newLengthOpnd, ldElem);
        }

        if(labelSegmentLengthIncreased && labelSegmentLengthIncreased != labelDone)
        {
            // labelSegmentLengthIncreased:
            ldElem->InsertBefore(labelSegmentLengthIncreased);
        }

        //     $done
        ldElem->InsertBefore(labelDone);
    }
    else //  #else
    {
        if (checkArrayLengthOverflow)
        {
            if (*pIsTypedArrayElement && isStore)
            {
                IR::LabelInstr *labelInlineSet = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

                //For positive index beyond length or negative index its essentially nop for typed array store
                InsertBranch(
                    !invertBoundCheckComparison ? Js::OpCode::BrLt_A : Js::OpCode::BrGt_A,
                    true /* isUnsigned */,
                    labelInlineSet,
                    ldElem);

                // For typed array, call ToNumber before we fallThrough.
                if (ldElem->GetSrc1()->GetType() == TyVar && !ldElem->GetSrc1()->GetValueType().IsPrimitive())
                {
                    IR::Instr *toNumberInstr = IR::Instr::New(Js::OpCode::Call, this->m_func);
                    toNumberInstr->SetSrc1(ldElem->GetSrc1());
                    ldElem->InsertBefore(toNumberInstr);

                    LowerUnaryHelperMem(toNumberInstr, IR::HelperOp_ConvNumber_Full);
                }
                InsertBranch(Js::OpCode::Br, labelFallthrough, ldElem);  //Jump to fallThrough

                ldElem->InsertBefore(labelInlineSet);
            }
            else
            {
                //      JAE $helper
                InsertBranch(
                    !invertBoundCheckComparison ? Js::OpCode::BrGe_A : Js::OpCode::BrLe_A,
                    true /* isUnsigned */,
                    labelHelper,
                    ldElem);
            }
        }

        EnsureObjectArrayLoaded();

        if (ldElem->m_opcode == Js::OpCode::InlineArrayPop)
        {
            Assert(!baseValueType.IsLikelyTypedArray());
            Assert(bailOutLabelInstr);

            if (indexValueOpnd->IsIntConstOpnd())
            {
                // indirOpnd = [headSegment + index + offset(elements)]
                IntConstType offset = offsetof(Js::SparseArraySegment<Js::Var>, elements) + (value << indirScale);
                // TODO: Assert(Math::FitsInDWord(offset));
                indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, (int32)offset, indirType, this->m_func);
            }
            else
            {
                //  indirOpnd = [headSegment + offset(elements) + (index << scale)]
                indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, indexValueOpnd->AsRegOpnd(), indirScale, indirType, this->m_func);
                indirOpnd->SetOffset(offsetof(Js::SparseArraySegment<Js::Var>, elements));
            }

            IR::Opnd * tmpDst = nullptr;
            IR::Opnd * dst = ldElem->GetDst();
            //Pop might not have a dst, if not don't worry about returning the last element. But we still have to
            // worry about gaps, because these force us to access the prototype chain, which may have side-effects.
            if (dst || !baseValueType.HasNoMissingValues())
            {
                if (!dst)
                {
                    dst = IR::RegOpnd::New(indirType, this->m_func);
                }
                else if (dst->AsRegOpnd()->m_sym == arrayOpnd->m_sym)
                {
                    tmpDst = IR::RegOpnd::New(TyVar, this->m_func);
                    dst = tmpDst;
                }

                //  MOV dst, [head + offset]
                InsertMove(dst, indirOpnd, ldElem);

                //If the array has missing values, check for one
                if (!baseValueType.HasNoMissingValues())
                {
                    InsertCompareBranch(
                        dst,
                        GetMissingItemOpnd(indirType, m_func),
                        Js::OpCode::BrEq_A,
                        bailOutLabelInstr,
                        ldElem,
                        true);
                }
            }
            //  MOV [head + offset], missing
            InsertMove(indirOpnd, GetMissingItemOpnd(indirType, m_func), ldElem);

            IR::Opnd *newLengthOpnd;
            IR::AutoReuseOpnd autoReuseNewLengthOpnd;
            if (indexValueOpnd->IsRegOpnd())
            {
                //      LEA newLength, [index]
                newLengthOpnd = indexValueOpnd;
                autoReuseNewLengthOpnd.Initialize(newLengthOpnd, m_func);
            }
            else
            {
                newLengthOpnd = IR::IntConstOpnd::New(value, TyUint32, this->m_func);
                autoReuseNewLengthOpnd.Initialize(newLengthOpnd, m_func);
            }

            //update segment length and array length
            //      MOV [headSegment + offset(length)], newLength
            IR::IndirOpnd *lengthIndirOpnd = IR::IndirOpnd::New(headSegmentOpnd, offsetof(Js::SparseArraySegmentBase, length), TyUint32, this->m_func);
            InsertMove(lengthIndirOpnd, newLengthOpnd, ldElem);

            //      MOV [base + offset(length)], newLength
            lengthIndirOpnd = IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfLength(), TyUint32, this->m_func);
            InsertMove(lengthIndirOpnd, newLengthOpnd, ldElem);

            if (tmpDst)
            {
                // The array opnd and the destination is the same, need to move the value in the tmp dst
                // to the actual dst
                InsertMove(ldElem->GetDst(), tmpDst, ldElem);
            }

            return indirOpnd;
        }
    } // #endif

    if (baseValueType.IsLikelyTypedArray())
    {
        if(!headSegmentOpnd)
        {
            //  MOV headSegment, [base + offset(arrayBuffer)]
            int bufferOffset;
            bufferOffset = Js::Float64Array::GetOffsetOfBuffer();
            indirOpnd = IR::IndirOpnd::New(arrayOpnd, bufferOffset, TyMachPtr, this->m_func);
            headSegmentOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
            autoReuseHeadSegmentOpnd.Initialize(headSegmentOpnd, m_func);
            InsertMove(headSegmentOpnd, indirOpnd, ldElem);
        }

        //  indirOpnd = [headSegment + index]
        if (indexValueOpnd->IsIntConstOpnd())
        {
            IntConstType offset = (value << indirScale);
            // TODO: Assert(Math::FitsInDWord(offset));
            indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, (int32)offset, indirType, this->m_func);
        }
        else
        {
            indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, indexValueOpnd->AsRegOpnd(), indirScale, indirType, this->m_func);
        }
    }
    else if (indexValueOpnd->IsIntConstOpnd())
    {
        // indirOpnd = [headSegment + index + offset(elements)]
        IntConstType offset = offsetof(Js::SparseArraySegment<Js::Var>, elements) + (value << indirScale);
        // TODO: Assert(Math::FitsInDWord(offset));
        indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, (int32)offset, indirType, this->m_func);
    }
    else
    {
        //  indirOpnd = [headSegment + offset(elements) + (index << scale)]
        indirOpnd = IR::IndirOpnd::New(headSegmentOpnd, indexValueOpnd->AsRegOpnd(), indirScale, indirType, this->m_func);
        indirOpnd->SetOffset(offsetof(Js::SparseArraySegment<Js::Var>, elements));
    }

    return indirOpnd;
}

void
Lowerer::GenerateTypeIdCheck(Js::TypeId typeId, IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateObjectCheck)
{
    if (generateObjectCheck && !opnd->IsNotTaggedValue())
    {
        m_lowererMD.GenerateObjectTest(opnd, insertBeforeInstr, labelFail);
    }

    //  MOV r1, [opnd + offset(type)]
    IR::RegOpnd *r1 = IR::RegOpnd::New(TyMachReg, this->m_func);
    const IR::AutoReuseOpnd autoReuseR1(r1, m_func);
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(opnd, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, this->m_func);
    InsertMove(r1, indirOpnd, insertBeforeInstr);

    //  CMP [r1 + offset(typeId)], typeid -- check src isString
    //  JNE $fail
    indirOpnd = IR::IndirOpnd::New(r1, Js::Type::GetOffsetOfTypeId(), TyInt32, this->m_func);
    InsertCompareBranch(
        indirOpnd,
        IR::IntConstOpnd::New(typeId, TyInt32, this->m_func),
        Js::OpCode::BrNeq_A,
        labelFail,
        insertBeforeInstr);
}

IR::RegOpnd *
Lowerer::GenerateUntagVar(IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateTagCheck)
{
    if (!opnd->IsVar())
    {
        AssertMsg(opnd->GetSize() == 4, "This should be 32-bit wide");
        return opnd;
    }
    return m_lowererMD.GenerateUntagVar(opnd, labelFail, insertBeforeInstr, generateTagCheck && !opnd->IsTaggedInt());
}

void
Lowerer::GenerateNotZeroTest( IR::Opnd * opndSrc, IR::LabelInstr * isZeroLabel, IR::Instr * insertBeforeInstr)
{
    InsertTestBranch(opndSrc, opndSrc, Js::OpCode::BrEq_A, isZeroLabel, insertBeforeInstr);
}

bool
Lowerer::GenerateFastStringLdElem(IR::Instr * ldElem, IR::LabelInstr * labelHelper, IR::LabelInstr * labelFallThru)
{
    IR::IndirOpnd * indirOpnd = ldElem->GetSrc1()->AsIndirOpnd();
    IR::RegOpnd * baseOpnd = indirOpnd->GetBaseOpnd();

    // don't generate the fast path if the instance is not likely string
    if (!baseOpnd->GetValueType().IsLikelyString())
    {
        return false;
    }

    Assert(!baseOpnd->IsTaggedInt());

    IR::RegOpnd * indexOpnd = indirOpnd->GetIndexOpnd();
    // Don't generate the fast path if the index operand is not likely int
    if (indexOpnd && !indexOpnd->GetValueType().IsLikelyInt())
    {
        return false;
    }

    // Make sure the instance is a string
    Assert(!indexOpnd || !indexOpnd->IsNotInt());
    GenerateStringTest(baseOpnd, ldElem, labelHelper);

    IR::Opnd * index32CmpOpnd;
    IR::RegOpnd * bufferOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    const IR::AutoReuseOpnd autoReuseBufferOpnd(bufferOpnd, m_func);
    IR::IndirOpnd * charIndirOpnd;
    if (indexOpnd)
    {
        // Untag the var and generate the indir into the string buffer
        IR::RegOpnd * index32Opnd = GenerateUntagVar(indexOpnd, labelHelper, ldElem);
        charIndirOpnd = IR::IndirOpnd::New(bufferOpnd, index32Opnd, 1, TyUint16, this->m_func);
        index32CmpOpnd = index32Opnd;
    }
    else
    {
        // Just use the offset to indirect into the string buffer
        charIndirOpnd = IR::IndirOpnd::New(bufferOpnd, indirOpnd->GetOffset() * sizeof(wchar_t), TyUint16, this->m_func);
        index32CmpOpnd = IR::IntConstOpnd::New(indirOpnd->GetOffset(), TyUint32, this->m_func);
    }

    // Check if the index is in range of the string length
    //  CMP [baseOpnd + offset(length)], indexOpnd     --  string length
    //  JBE $helper                                    -- unsigned compare, and string length are at most INT_MAX - 1
    //                                                 -- so that even if we have a negative index, this will fail
    InsertCompareBranch(IR::IndirOpnd::New(baseOpnd, offsetof(Js::JavascriptString, m_charLength), TyInt32, this->m_func)
        , index32CmpOpnd, Js::OpCode::BrLe_A, true, labelHelper, ldElem);

    // Load the string buffer and make sure it is not null
    //  MOV bufferOpnd, [baseOpnd + offset(m_pszValue)]
    //  TEST bufferOpnd, bufferOpnd
    //  JEQ $lableHelper
    indirOpnd = IR::IndirOpnd::New(baseOpnd, offsetof(Js::JavascriptString, m_pszValue), TyMachPtr, this->m_func);

    InsertMove(bufferOpnd, indirOpnd, ldElem);
    GenerateNotZeroTest(bufferOpnd, labelHelper, ldElem);

    // Load the character and check if it is 7bit ASCI (which we have the cache for)
    //  MOV charOpnd, [bufferOpnd + index32Opnd]
    //  CMP charOpnd, 0x80
    //  JAE $helper
    IR::RegOpnd * charOpnd = IR::RegOpnd::New(TyInt32, this->m_func);
    const IR::AutoReuseOpnd autoReuseCharOpnd(charOpnd, m_func);
    InsertMove(charOpnd, charIndirOpnd, ldElem);
    InsertCompareBranch(charOpnd, IR::IntConstOpnd::New(Js::CharStringCache::CharStringCacheSize, TyUint16, this->m_func),
        Js::OpCode::BrGe_A, true, labelHelper, ldElem);

    // Load the string from the cache
    //  MOV charStringCache, <charStringCache, address>
    //  MOV stringOpnd, [charStringCache + charOpnd * 4]

    IR::RegOpnd * cacheOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    const IR::AutoReuseOpnd autoReuseCacheOpnd(cacheOpnd, m_func);
    Assert(Js::JavascriptLibrary::GetCharStringCacheAOffset() == Js::JavascriptLibrary::GetCharStringCacheOffset());
    InsertMove(cacheOpnd, this->LoadLibraryValueOpnd(ldElem,  LibraryValue::ValueCharStringCache), ldElem);

    // Check if we have created the string or not
    //  TEST stringOpnd, stringOpnd
    //  JE $helper
    IR::RegOpnd * stringOpnd =  IR::RegOpnd::New(TyMachPtr, this->m_func);
    const IR::AutoReuseOpnd autoReuseStringOpnd(stringOpnd, m_func);
    InsertMove(stringOpnd, IR::IndirOpnd::New(cacheOpnd, charOpnd, this->m_lowererMD.GetDefaultIndirScale(), TyVar, this->m_func), ldElem);

    GenerateNotZeroTest(stringOpnd, labelHelper, ldElem);

    InsertMove(ldElem->GetDst(), stringOpnd, ldElem);
    InsertBranch(Js::OpCode::Br, labelFallThru, ldElem);

    return true;
}

bool
Lowerer::GenerateFastLdElemI(IR::Instr *& ldElem, bool *instrIsInHelperBlockRef)
{
    Assert(instrIsInHelperBlockRef);
    bool &instrIsInHelperBlock = *instrIsInHelperBlockRef;
    instrIsInHelperBlock = false;

    IR::LabelInstr *    labelHelper;
    IR::LabelInstr *    labelFallThru;
    IR::LabelInstr *    labelBailOut = nullptr;
    IR::LabelInstr *    labelMissingNative = nullptr;
    IR::Opnd *src1 =    ldElem->GetSrc1();

    AssertMsg(src1->IsIndirOpnd(), "Expected indirOpnd on LdElementI");

    IR::IndirOpnd *     indirOpnd = src1->AsIndirOpnd();

    // From FastElemICommon:
    //  TEST base, AtomTag                  -- check base not tagged int
    //  JNE $helper
    //  MOV r1, [base + offset(type)]       -- check base isArray
    //  CMP [r1 + offset(typeId)], TypeIds_Array
    //  JNE $helper
    //  TEST index, 1                       -- index tagged int
    //  JEQ $helper
    //  MOV r2, index
    //  SAR r2, Js::VarTag_Shift            -- remote atom tag
    //  JS $helper                          -- exclude negative index
    //  MOV r4, [base + offset(head)]
    //  CMP r2, [r4 + offset(length)]       -- bounds check
    //  JAE $helper
    //  MOV r3, [r4 + offset(elements)]

    // Generated here:
    //  MOV dst, [r3 + r2]
    //  TEST dst, dst
    //  JNE $fallthrough

    if(ldElem->m_opcode == Js::OpCode::LdMethodElem && indirOpnd->GetBaseOpnd()->GetValueType().IsLikelyOptimizedTypedArray())
    {
        // Typed arrays don't return objects, so it's not worth generating a fast path for LdMethodElem. Calling the helper also
        // generates a better error message. Skip the fast path and just generate a helper call.
        return true;
    }

    labelFallThru = ldElem->GetOrCreateContinueLabel();
    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    // If we know for sure (based on flow graph) we're loading from the arguments object, then ignore the (path-based) profile info.
    bool isNativeArrayLoad = !ldElem->DoStackArgsOpt(this->m_func) && indirOpnd->GetBaseOpnd()->GetValueType().IsLikelyNativeArray();
    bool needMissingValueCheck = true;
    bool emittedFastPath = false;
    bool emitBailout = false;

    if (ldElem->DoStackArgsOpt(this->m_func))
    {
        emittedFastPath = GenerateFastArgumentsLdElemI(ldElem, labelHelper, labelFallThru);
    }
    else if (GenerateFastStringLdElem(ldElem, labelHelper, labelFallThru))
    {
        emittedFastPath = true;
    }
    else
    {
        IR::LabelInstr * labelCantUseArray = labelHelper;
        if (isNativeArrayLoad)
        {
            if (ldElem->GetDst()->GetType() == TyVar)
            {
                // Skip the fast path and just generate a helper call
                return true;
            }

            // Specialized native array lowering for LdElem requires that it is profiled. When not profiled, GlobOpt should not
            // have specialized it.
            Assert(ldElem->IsProfiledInstr());

            labelBailOut = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            labelCantUseArray = labelBailOut;
        }
        bool isTypedArrayElement, isStringIndex;
        indirOpnd =
            GenerateFastElemICommon(
                ldElem,
                false,
                src1->AsIndirOpnd(),
                labelHelper,
                labelCantUseArray,
                labelFallThru,
                &isTypedArrayElement,
                &isStringIndex,
                &emitBailout);

        IR::Opnd *dst = ldElem->GetDst();
        IRType dstType = dst->AsRegOpnd()->GetType();

        // The index is negative or not int.
        if (indirOpnd == nullptr)
        {
            Assert(!(ldElem->HasBailOutInfo() && ldElem->GetBailOutKind() & IR::BailOutOnArrayAccessHelperCall));

            // The global optimizer should never type specialize a LdElem for which the index is not int or an integer constant
            // with a negative value. This would force an unconditional bail out on the main code path.
            if (dst->IsVar())
            {
                if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->m_func) && PHASE_TRACE(Js::LowererPhase, this->m_func))
                {
                    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Output::Print(L"Typed Array Lowering: function: %s (%s): instr %s, not specialized by glob opt due to negative or not likely int index.\n",
                        this->m_func->GetJnFunction()->GetDisplayName(),
                        this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                        Js::OpCodeUtil::GetOpCodeName(ldElem->m_opcode));
                    Output::Flush();
                }

                // We must be dealing with some unconventional index value.  Don't emit fast path, but go directly to helper.
                emittedFastPath = false;
                return true;
            }
            else
            {
                AssertMsg(false, "Global optimizer shouldn't have specialized this instruction.");
                Assert(dst->IsRegOpnd());

                // If global optimizer failed to notice the unconventional index and type specialized the dst,
                // there is nothing to do but bail out. This could happen if global optimizer's information based
                // on value tracking fails to recognize a non-integer index or a constant int index that is negative.
                // The bailout below ensures that we behave correctly in retail builds even under
                // these (unlikely) conditions. To satisfy the downstream code we must populate the type specialized operand
                // with some made up values, even though we will unconditionally bail out here and the values will never be
                // used.
                IR::IntConstOpnd *constOpnd = IR::IntConstOpnd::New(0, TyInt32, this->m_func, true);
                InsertMove(dst, constOpnd, ldElem);

                ldElem->UnlinkSrc1();
                ldElem->UnlinkDst();
                GenerateBailOut(ldElem, nullptr, nullptr);
                emittedFastPath = true;
                return false;
            }
        }

        const IR::AutoReuseOpnd autoReuseIndirOpnd(indirOpnd, m_func);
        const ValueType baseValueType(src1->AsIndirOpnd()->GetBaseOpnd()->GetValueType());

        if (ldElem->HasBailOutInfo() &&
            ldElem->GetByteCodeOffset() != Js::Constants::NoByteCodeOffset &&
            ldElem->GetBailOutInfo()->bailOutOffset <= ldElem->GetByteCodeOffset() &&
            dst->IsEqual(src1->AsIndirOpnd()->GetBaseOpnd()) ||
            (src1->AsIndirOpnd()->GetIndexOpnd() && dst->IsEqual(src1->AsIndirOpnd()->GetIndexOpnd())))
        {
            // This is a pre-op bailout where the dst is the same as one of the srcs. The dst may be trashed before bailing out,
            // but since the operation will be processed again in the interpreter, src values need to be kept intact. Use a
            // temporary dst until after the operation is complete.
            IR::Instr *instrSink = ldElem->SinkDst(Js::OpCode::Ld_A);

            // The sink instruction needs to be on the fall-through path
            instrSink->Unlink();
            labelFallThru->InsertAfter(instrSink);

            LowererMD::ChangeToAssign(instrSink);
            dst = ldElem->GetDst();
        }

        if (isTypedArrayElement)
        {
            // For typedArrays, convert the loaded element to the appropriate type
            IR::RegOpnd *reg;
            IR::AutoReuseOpnd autoReuseReg;

            Assert(dst->IsRegOpnd());

            if(indirOpnd->IsFloat())
            {
                AssertMsg((dstType == TyFloat64) || (dstType == TyVar), "For Float32Array LdElemI's dst should be specialized to TyFloat64 or not at all.");

                if(indirOpnd->IsFloat32())
                {
                    // MOVSS reg32.f32, indirOpnd.f32
                    IR::RegOpnd *reg32 = IR::RegOpnd::New(TyFloat32, this->m_func);
                    const IR::AutoReuseOpnd autoReuseReg32(reg32, m_func);
                    InsertMove(reg32, indirOpnd, ldElem);

                    // CVTPS2PD dst/reg.f64, reg32.f64
                    reg = dstType == TyFloat64 ? dst->AsRegOpnd() : IR::RegOpnd::New(TyFloat64, this->m_func);
                    autoReuseReg.Initialize(reg, m_func);
                    InsertConvertFloat32ToFloat64(reg, reg32, ldElem);
                }
                else
                {
                    Assert(indirOpnd->IsFloat64());

                    // MOVSD dst/reg.f64, indirOpnd.f64
                    reg = dstType == TyFloat64 ? dst->AsRegOpnd() : IR::RegOpnd::New(TyFloat64, this->m_func);
                    autoReuseReg.Initialize(reg, m_func);
                    InsertMove(reg, indirOpnd, ldElem);
                }

                if (dstType != TyFloat64)
                {
                    // Convert reg.f64 to var
                    m_lowererMD.SaveDoubleToVar(dst->AsRegOpnd(), reg, ldElem, ldElem);
                }

#if FLOATVAR
                // For NaNs, go to the helper to guarantee we don't have an illegal NaN
                // UCOMISD reg, reg
                {
                    IR::Instr *const instr = IR::Instr::New(Js::OpCode::UCOMISD, this->m_func);
                    instr->SetSrc1(reg);
                    instr->SetSrc2(reg);
                    ldElem->InsertBefore(instr);
                }

                // JP $helper
                {
                    IR::Instr *const instr = IR::BranchInstr::New(Js::OpCode::JP, labelHelper, this->m_func);
                    ldElem->InsertBefore(instr);
                }
#endif

                if(dstType == TyFloat64)
                {
                    emitBailout = true;
                }
            }
            else
            {
                AssertMsg((dstType == TyInt32) || (dstType == TyVar), "For Int/UintArray LdElemI's dst should be specialized to TyInt32 or not at all.");

                reg = dstType == TyInt32 ? dst->AsRegOpnd() : IR::RegOpnd::New(TyInt32, this->m_func);
                const IR::AutoReuseOpnd autoReuseReg(reg, m_func);

                // Int32 and Uint32 arrays could overflow an int31, but the others can't
                if (indirOpnd->GetType() != TyUint32
#if !INT32VAR
                    && indirOpnd->GetType() != TyInt32
#endif
                    )
                {
                    reg->SetValueType(ValueType::GetTaggedInt());  // Fits as a tagged-int
                }

                // MOV/MOVZX/MOVSX dst/reg.int32, IndirOpnd.type
                IR::Instr *const instr = InsertMove(reg, indirOpnd, ldElem);

                if (dstType == TyInt32)
                {
                    instr->dstIsTempNumber = ldElem->dstIsTempNumber;
                    instr->dstIsTempNumberTransferred = ldElem->dstIsTempNumberTransferred;

                    if (indirOpnd->GetType() == TyUint32)
                    {
                        // TEST dst, dst
                        // JSB $helper (bailout)
                        InsertCompareBranch(
                            reg,
                            IR::IntConstOpnd::New(0, TyUint32, this->m_func, /* dontEncode = */ true),
                            Js::OpCode::BrLt_A,
                            labelHelper,
                            ldElem);
                    }

                    emitBailout = true;
                }
                else
                {
                    // MOV dst, reg
                    IR::Instr *const instr = IR::Instr::New(Js::OpCode::ToVar, dst, reg, this->m_func);
                    instr->dstIsTempNumber = ldElem->dstIsTempNumber;
                    instr->dstIsTempNumberTransferred = ldElem->dstIsTempNumberTransferred;
                    ldElem->InsertBefore(instr);

                    // Convert dst to var
                    m_lowererMD.EmitLoadVar(instr, /* isFromUint32 = */ (indirOpnd->GetType() == TyUint32));
                }
            }

            //  JMP $fallthrough
            InsertBranch(Js::OpCode::Br, labelFallThru, ldElem);

            emittedFastPath = true;

            if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->m_func) && PHASE_TRACE(Js::LowererPhase, this->m_func))
            {
                char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                baseValueType.ToString(baseValueTypeStr);
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"Typed Array Lowering: function: %s (%s), instr: %s, base value type: %S, %s.",
                    this->m_func->GetJnFunction()->GetDisplayName(),
                    this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                    Js::OpCodeUtil::GetOpCodeName(ldElem->m_opcode),
                    baseValueTypeStr,
                    (!dst->IsVar() ? L"specialized" : L"not specialized"));
                Output::Print(L"\n");
                Output::Flush();
            }
        }
        else
        {
            // MOV dst, indirOpnd
            InsertMove(dst, indirOpnd, ldElem);

            // The string index fast path does not operate on index properties (we don't get a PropertyString in that case), so
            // we don't need to do any further checks in that case

            // For LdMethodElem, if the loaded value is a tagged number, the error message generated by the helper call is
            // better than if we were to just try to call the number. Also, the call arguments need to be evaluated before
            // throwing the error, so just test whether it's an object and jump to helper if it's not.
            const bool needObjectTest = !isStringIndex && !isNativeArrayLoad && ldElem->m_opcode == Js::OpCode::LdMethodElem;
            needMissingValueCheck =
                !isStringIndex && !(baseValueType.IsArrayOrObjectWithArray() && baseValueType.HasNoMissingValues());
            if(needMissingValueCheck)
            {
                //  TEST dst, dst
                //  JEQ $helper | JNE $fallthrough
                InsertCompareBranch(
                    dst,
                    GetMissingItemOpnd(dst->GetType(), m_func),
                    needObjectTest ? Js::OpCode::BrEq_A : Js::OpCode::BrNeq_A,
                    needObjectTest ? labelHelper : labelFallThru,
                    ldElem,
                    true);

                if (isNativeArrayLoad)
                {
                    Assert(!needObjectTest);
                    Assert(labelHelper != labelBailOut);
                    if(ldElem->AsProfiledInstr()->u.ldElemInfo->GetElementType().HasBeenUndefined())
                    {
                        // We're going to bail out trying to load "missing value" into a type-spec'd opnd.
                        // Branch to a point where we'll convert the array so that we don't keep bailing here.
                        // (Gappy arrays are not well-suited to nativeness.)
                        labelMissingNative = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
                        InsertBranch(Js::OpCode::Br, labelMissingNative, ldElem);
                    }
                    else
                    {
                        // If the value has not been profiled to be undefined at some point, jump directly to bail out
                        InsertBranch(Js::OpCode::Br, labelBailOut, ldElem);
                    }
                }
            }
            if(needObjectTest)
            {
                //  GenerateObjectTest(dst)
                //  JIsObject $fallthrough
                m_lowererMD.GenerateObjectTest(dst, ldElem, labelFallThru, true);
            }
            else if(!needMissingValueCheck)
            {
                //  JMP $fallthrough
                InsertBranch(Js::OpCode::Br, labelFallThru, ldElem);
            }

            emittedFastPath = true;
        }
    }
    // $helper:
    //      bailout or caller generated helper call
    // $fallthru:

    if (!emittedFastPath)
    {
        labelHelper->isOpHelper = false;
    }

    ldElem->InsertBefore(labelHelper);
    instrIsInHelperBlock = true;

    if (isNativeArrayLoad)
    {
        Assert(ldElem->HasBailOutInfo());
        Assert(labelHelper != labelBailOut);

        // Transform the original instr:
        //
        // $helper:
        // dst = LdElemI_A src (BailOut)
        // $fallthrough:
        //
        // to:
        //
        // b $fallthru  <--- we get here if we loaded a valid element directly
        // $helper:
        // dst = LdElemI_A src
        // cmp dst, MissingItem
        // bne $fallthrough
        // $bailout:
        //       BailOut
        // $fallthrough:

        LowerOneBailOutKind(ldElem, IR::BailOutConventionalNativeArrayAccessOnly, instrIsInHelperBlock);
        IR::Instr *const insertBeforeInstr = ldElem->m_next;

        // Do missing value check on value returned from helper so that we don't have to check the index against
        // array length. (We already checked it above against the segment length.)

        bool hasBeenUndefined = ldElem->AsProfiledInstr()->u.ldElemInfo->GetElementType().HasBeenUndefined();
        if (hasBeenUndefined)
        {
            if(!emitBailout)
            {
                if (labelMissingNative == nullptr)
                {
                    labelMissingNative = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
                }

                InsertCompareBranch(GetMissingItemOpnd(ldElem->GetDst()->GetType(), m_func), ldElem->GetDst(), Js::OpCode::BrEq_A, labelMissingNative, insertBeforeInstr, true);
            }
            InsertBranch(Js::OpCode::Br, labelFallThru, insertBeforeInstr);
            if(labelMissingNative)
            {
                // We're going to bail out on a load from a gap, but convert the array to Var first, so we don't just
                // bail here over and over. Gappy arrays are not well suited to nativeness.
                // NOTE: only emit this call if the profile tells us that this has happened before ("hasBeenUndefined").
                // Emitting this in Navier-Stokes brutalizes the score.
                insertBeforeInstr->InsertBefore(labelMissingNative);
                IR::JnHelperMethod helperMethod;
                indirOpnd = ldElem->GetSrc1()->AsIndirOpnd();
                if (indirOpnd->GetBaseOpnd()->GetValueType().HasIntElements())
                {
                    helperMethod = IR::HelperIntArr_ToVarArray;
                }
                else
                {
                    Assert(indirOpnd->GetBaseOpnd()->GetValueType().HasFloatElements());
                    helperMethod = IR::HelperFloatArr_ToVarArray;
                }
                m_lowererMD.LoadHelperArgument(insertBeforeInstr, indirOpnd->GetBaseOpnd());
                IR::Instr *instrHelper = IR::Instr::New(Js::OpCode::Call, m_func);
                instrHelper->SetSrc1(IR::HelperCallOpnd::New(helperMethod, m_func));
                insertBeforeInstr->InsertBefore(instrHelper);
                m_lowererMD.LowerCall(instrHelper, 0);
            }
        }
        else
        {
            if(!emitBailout)
            {
                InsertCompareBranch(GetMissingItemOpnd(ldElem->GetDst()->GetType(), m_func), ldElem->GetDst(), Js::OpCode::BrEq_A, labelBailOut, insertBeforeInstr, true);
            }

            InsertBranch(Js::OpCode::Br, labelFallThru, insertBeforeInstr);
        }

        insertBeforeInstr->InsertBefore(labelBailOut);
    }

    if (emitBailout)
    {
        ldElem->UnlinkSrc1();
        ldElem->UnlinkDst();
        GenerateBailOut(ldElem, nullptr, nullptr);
    }

    return !emitBailout;
}

IR::Opnd *
Lowerer::GetMissingItemOpnd(IRType type, Func *func)
{
    if (type == TyVar)
    {
        return IR::AddrOpnd::New(Js::JavascriptArray::MissingItem, IR::AddrOpndKindConstant, func, true);
    }
    if (type == TyInt32)
    {
        return IR::IntConstOpnd::New(Js::JavascriptNativeIntArray::MissingItem, TyInt32, func, true);
    }
    Assert(type == TyFloat64);
    return IR::MemRefOpnd::New((BYTE*)&Js::JavascriptNativeFloatArray::MissingItem, TyFloat64, func);
}

bool
Lowerer::GenerateFastStElemI(IR::Instr *& stElem, bool *instrIsInHelperBlockRef)
{
    Assert(instrIsInHelperBlockRef);
    bool &instrIsInHelperBlock = *instrIsInHelperBlockRef;
    instrIsInHelperBlock = false;

    IR::LabelInstr *    labelHelper;
    IR::LabelInstr *    labelSegmentLengthIncreased;
    IR::LabelInstr *    labelFallThru;
    IR::LabelInstr *    labelBailOut = nullptr;
    IR::Opnd *dst =     stElem->GetDst();
    IR::IndirOpnd *     indirOpnd = dst->AsIndirOpnd();

    AssertMsg(dst->IsIndirOpnd(), "Expected indirOpnd on StElementI");

    // From FastElemICommon:
    //  TEST base, AtomTag                  -- check base not tagged int
    //  JNE $helper
    //  MOV r1, [base + offset(type)]       -- check base isArray
    //  CMP [r1 + offset(typeId)], TypeIds_Array
    //  JNE $helper
    //  TEST index, 1                       -- index tagged int
    //  JEQ $helper
    //  MOV r2, index
    //  SAR r2, Js::VarTag_Shift            -- remote atom tag
    //  JS $helper                          -- exclude negative index
    //  MOV r4, [base + offset(head)]
    //  CMP r2, [r4 + offset(length)]       -- bounds check
    //  JB  $done
    //  CMP r2, [r4 + offset(size)]         -- chunk has room?
    //  JAE $helper
    //  LEA r5, [r2 + 1]
    //  MOV [r4 + offset(length)], r5       -- update length on chunk
    //  CMP r5, [base  + offset(length)]
    //  JBE $done
    //  MOV [base + offset(length)], r5     -- update length on array
    // $done
    //  LEA r3, [r4 + offset(elements)]

    // Generated here.
    //  MOV [r3 + r2], src

    labelFallThru = stElem->GetOrCreateContinueLabel();
    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    bool emitBailout = false;

    bool isNativeArrayStore = indirOpnd->GetBaseOpnd()->GetValueType().IsLikelyNativeArray();
    IR::LabelInstr * labelCantUseArray = labelHelper;
    if (isNativeArrayStore)
    {
        if (stElem->GetSrc1()->GetType() != GetArrayIndirType(indirOpnd->GetBaseOpnd()->GetValueType()))
        {
            // Skip the fast path and just generate a helper call
            return true;
        }

        if(stElem->HasBailOutInfo())
        {
            const IR::BailOutKind bailOutKind = stElem->GetBailOutKind();
            if (bailOutKind & IR::BailOutConventionalNativeArrayAccessOnly)
            {
                labelBailOut = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
                labelCantUseArray = labelBailOut;
            }
        }
    }

    bool isTypedArrayElement, isStringIndex;
    indirOpnd =
        GenerateFastElemICommon(
            stElem,
            true,
            indirOpnd,
            labelHelper,
            labelCantUseArray,
            labelFallThru,
            &isTypedArrayElement,
            &isStringIndex,
            &emitBailout,
            &labelSegmentLengthIncreased);

    IR::Opnd *src = stElem->GetSrc1();
    const IR::AutoReuseOpnd autoReuseSrc(src, m_func);

    // The index is negative or not int.
    if (indirOpnd == nullptr)
    {
        Assert(!(stElem->HasBailOutInfo() && stElem->GetBailOutKind() & IR::BailOutOnArrayAccessHelperCall));

        // The global optimizer should never type specialize a StElem for which we know the index is not int or is a negative
        // int constant.  This would result in an unconditional bailout on the main code path.
        if (src->IsVar())
        {
            if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->m_func) && PHASE_TRACE(Js::LowererPhase, this->m_func))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"Typed Array Lowering: function: %s (%s): instr %s, not specialized by glob opt due to negative or not likely int index.\n",
                    this->m_func->GetJnFunction()->GetDisplayName(),
                    this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                    Js::OpCodeUtil::GetOpCodeName(stElem->m_opcode));
                Output::Flush();
            }
            // We must be dealing with some atypical index value.  Don't emit fast path, but go directly to helper.
            return true;
        }
        else
        {
            // If global optimizer failed to notice the unconventional index and type specialized the src,
            // there is nothing to do but bail out.  We should never hit this code path, unless the global optimizer's conditions
            // for not specializing the instruction don't match the lowerer's conditions for not emitting the array checks (see above).
            // This could happen if global optimizer's information based on value tracking fails to recognize a non-integer index or
            // a constant int index that is negative.  The bailout below ensures that we behave correctly in retail builds even under
            // these (unlikely) conditions.
            AssertMsg(false, "Global optimizer shouldn't have specialized this instruction.");

            stElem->UnlinkSrc1();
            stElem->UnlinkDst();
            GenerateBailOut(stElem, nullptr, nullptr);
            return false;
        }
    }

    const IR::AutoReuseOpnd autoReuseIndirOpnd(indirOpnd, m_func);

    const ValueType baseValueType(dst->AsIndirOpnd()->GetBaseOpnd()->GetValueType());
    if (isTypedArrayElement)
    {
        if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->m_func) && PHASE_TRACE(Js::LowererPhase, this->m_func))
        {
            char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            baseValueType.ToString(baseValueTypeStr);
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(L"Typed Array Lowering: function: %s (%s), instr: %s, base value type: %S, %s.",
                this->m_func->GetJnFunction()->GetDisplayName(),
                this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                Js::OpCodeUtil::GetOpCodeName(stElem->m_opcode),
                baseValueTypeStr,
                (!src->IsVar() ? L"specialized" : L"not specialized"));
            Output::Print(L"\n");
            Output::Flush();
        }

        ObjectType objectType = baseValueType.GetObjectType();

        if(indirOpnd->IsFloat())
        {
            if (src->GetType() == TyFloat64)
            {
                IR::RegOpnd *const regSrc = src->AsRegOpnd();

                if (indirOpnd->IsFloat32())
                {
                    // CVTSD2SS reg.f32, regSrc.f64    -- Convert regSrc from f64 to f32
                    IR::RegOpnd *const reg = IR::RegOpnd::New(TyFloat32, this->m_func);
                    const IR::AutoReuseOpnd autoReuseReg(reg, m_func);
                    InsertConvertFloat64ToFloat32(reg, regSrc, stElem);

                    // MOVSS indirOpnd, reg
                    InsertMove(indirOpnd, reg, stElem);
                }
                else
                {
                    // MOVSD indirOpnd, regSrc
                    InsertMove(indirOpnd, regSrc, stElem);
                }
                emitBailout = true;
            }
            else
            {
                Assert(src->GetType() == TyVar);

                // MOV reg, src
                IR::RegOpnd *const reg = IR::RegOpnd::New(TyVar, this->m_func);
                const IR::AutoReuseOpnd autoReuseReg(reg, m_func);
                InsertMove(reg, src, stElem);

                // Convert to float, and assign to indirOpnd
                if (baseValueType.IsLikelyOptimizedVirtualTypedArray())
                {
                    IR::RegOpnd* dstReg = IR::RegOpnd::New(indirOpnd->GetType(), this->m_func);
                    m_lowererMD.EmitLoadFloat(dstReg, reg, stElem);
                    InsertMove(indirOpnd, dstReg, stElem);
                }
                else
                {
                    m_lowererMD.EmitLoadFloat(indirOpnd, reg, stElem);
                }


            }
        }
        else if (objectType == ObjectType::Uint8ClampedArray || objectType == ObjectType::Uint8ClampedVirtualArray || objectType == ObjectType::Uint8ClampedMixedArray)
        {
            Assert(indirOpnd->GetType() == TyUint8);

            IR::RegOpnd *regSrc;
            IR::AutoReuseOpnd autoReuseRegSrc;
            if(src->IsRegOpnd())
            {
                regSrc = src->AsRegOpnd();
            }
            else
            {
                regSrc = IR::RegOpnd::New(StackSym::New(src->GetType(), m_func), src->GetType(), m_func);
                autoReuseRegSrc.Initialize(regSrc, m_func);

                InsertMove(regSrc, src, stElem);
            }

            IR::Opnd *bitMaskOpnd;
            IRType srcType = regSrc->GetType();

            if ((srcType == TyFloat64) || (srcType == TyInt32))
            {
                // if (srcType == TyInt32) {
                //     TEST regSrc, ~255
                //     JE $storeValue
                //     JSB $handleNegative
                //     MOV indirOpnd, 255
                //     JMP $fallThru
                //     $handleNegative [isHelper = false]
                //     MOV indirOpnd, 0
                //     JMP $fallThru
                //     $storeValue
                //     MOV indirOpnd, regSrc
                // }
                // else {
                //     MOVSD regTmp, regSrc
                //     ADDSD regTmp, 0.5
                //     CVTTSD2SI regOpnd, regTmp
                //     TEST regOpnd, ~255
                //     JE $storeValue
                //     $handleOutOfBounds [isHelper = true]
                //     COMISD regSrc, [&FloatZero]
                //     JB $handleNegative
                //     MOV regOpnd, 255
                //     JMP $storeValue
                //     $handleNegative [isHelper = true]
                //     MOV regOpnd, 0
                //     $storeValue
                //     MOV indirOpnd, regOpnd
                // }
                // $fallThru

                IR::RegOpnd *regOpnd;
                IR::AutoReuseOpnd autoReuseRegOpnd;
                if (srcType == TyInt32)
                {
                    // When srcType == TyInt32 we will never call the helper and we will never
                    // modify the regOpnd.  Therefore, it's okay to use regSrc directly, and it
                    // reduces register pressure.
                    regOpnd = regSrc;
                }
                else
                {
#ifdef _M_IX86
                    AssertMsg(AutoSystemInfo::Data.SSE2Available(), "GlobOpt shouldn't have specialized Uint8ClampedArray StElem to float64 if SSE2 is unavailable.");
#endif

                    regOpnd = IR::RegOpnd::New(TyInt32, this->m_func);
                    autoReuseRegOpnd.Initialize(regOpnd, m_func);

                    Assert(objectType == ObjectType::Uint8ClampedArray || objectType == ObjectType::Uint8ClampedVirtualArray || objectType == ObjectType::Uint8ClampedMixedArray);

                    // Uint8ClampedArray follows IEEE 754 rounding rules for ties which round up
                    // odd integers and round down even integers. Both ties result in the nearest
                    // even integer value.
                    //
                    // CVTSD2SI regOpnd, regSrc
                    LowererMD::InsertConvertFloat64ToInt32(RoundModeHalfToEven, regOpnd, regSrc, stElem);
                }

                IR::LabelInstr *labelStoreValue = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, false);
#ifndef _M_ARM
                // TEST regOpnd, ~255
                // JE $storeValue
                bitMaskOpnd = IR::IntConstOpnd::New(~255, TyInt32, this->m_func, true);
                InsertTestBranch(regOpnd, bitMaskOpnd, Js::OpCode::BrEq_A, labelStoreValue, stElem);
#else // ARM
                // Special case for ARM, a shift may be better
                //
                // ASRS tempReg, src, 8
                // BEQ $inlineSet
                InsertShiftBranch(
                    Js::OpCode::Shr_A,
                    IR::RegOpnd::New(TyInt32, this->m_func),
                    regOpnd,
                    IR::IntConstOpnd::New(8, TyInt8, this->m_func),
                    Js::OpCode::BrEq_A,
                    labelStoreValue,
                    stElem);
#endif

                IR::LabelInstr *labelHandleNegative = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, srcType == TyFloat64);

                if (srcType == TyInt32)
                {
                    // JSB $handleNegativeOrOverflow
                    InsertBranch(
                        LowererMD::MDCompareWithZeroBranchOpcode(Js::OpCode::BrLt_A),
                        labelHandleNegative,
                        stElem);

                    // MOV IndirOpnd.u8, 255
                    InsertMove(indirOpnd, IR::IntConstOpnd::New(255, TyUint8, this->m_func, true), stElem);

                    // JMP $fallThru
                    InsertBranch(Js::OpCode::Br, labelFallThru, stElem);

                    // $handleNegative [isHelper = false]
                    stElem->InsertBefore(labelHandleNegative);

                    // MOV IndirOpnd.u8, 0
                    InsertMove(indirOpnd, IR::IntConstOpnd::New(0, TyUint8, this->m_func, true), stElem);

                    // JMP $fallThru
                    InsertBranch(Js::OpCode::Br, labelFallThru, stElem);
                }
                else
                {
                    Assert(regOpnd != regSrc);

                    // This label is just to ensure the following code is moved to the helper block.
                    // $handleOutOfBounds [isHelper = true]
                    IR::LabelInstr *labelHandleOutOfBounds = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
                    stElem->InsertBefore(labelHandleOutOfBounds);

                    // COMISD regSrc, FloatZero
                    // JB labelHandleNegative
                    IR::MemRefOpnd * zeroOpnd = IR::MemRefOpnd::New((double*)&(Js::JavascriptNumber::k_Zero), TyMachDouble, this->m_func);
                    InsertCompareBranch(regSrc, zeroOpnd, Js::OpCode::BrNotGe_A, labelHandleNegative, stElem);

                    // MOV regOpnd, 255
                    InsertMove(regOpnd, IR::IntConstOpnd::New(255, TyUint8, this->m_func, true), stElem);

                    // JMP $storeValue
                    InsertBranch(Js::OpCode::Br, labelStoreValue, stElem);

                    // $handleNegative [isHelper = true]
                    stElem->InsertBefore(labelHandleNegative);

                    // MOV regOpnd, 0
                    InsertMove(regOpnd, IR::IntConstOpnd::New(0, TyUint8, this->m_func, true), stElem);
                }

                // $storeValue
                stElem->InsertBefore(labelStoreValue);

                // MOV IndirOpnd.u8, regOpnd.u8
                InsertMove(indirOpnd, regOpnd, stElem);

                emitBailout = true;
            }
            else
            {
                Assert(srcType == TyVar);

#if INT32VAR
                bitMaskOpnd = IR::AddrOpnd::New((Js::Var)~(INT_PTR)(Js::TaggedInt::ToVarUnchecked(255)), IR::AddrOpndKindConstantVar, this->m_func, true);
#else
                bitMaskOpnd = IR::IntConstOpnd::New(~(INT_PTR)(Js::TaggedInt::ToVarUnchecked(255)), TyMachReg, this->m_func, true);
#endif
                // Note: We are assuming that if no bits other than ~(TaggedInt(255)) are 1, that we have a tagged
                //  int value between 0 - 255.
                // #if INT32VAR
                //      This works for pointers because tagged int bit can't be on, and first 64k are not valid addresses
                //      This works for floats because a valid float would have one of the upper 13 bits on.
                // #else
                //      Any pointer is larger than 512 because first 64k memory is reserved by the OS
                // #endif

                IR::LabelInstr *labelInlineSet = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
#ifndef _M_ARM
                //      TEST src, ~(TaggedInt(255))      -- Check for tagged int >= 255 and <= 0
                //      JEQ $inlineSet
                InsertTestBranch(regSrc, bitMaskOpnd, Js::OpCode::BrEq_A, labelInlineSet, stElem);
#else // ARM
                // Special case for ARM, a shift may be better
                //
                // ASRS tempReg, src, 8
                // BEQ $inlineSet
                InsertShiftBranch(
                    Js::OpCode::Shr_A,
                    IR::RegOpnd::New(TyInt32, this->m_func),
                    regSrc,
                    IR::IntConstOpnd::New(8, TyInt8, this->m_func),
                    Js::OpCode::BrEq_A,
                    labelInlineSet,
                    stElem);
#endif

                // Uint8ClampedArray::DirectSetItem(array, index, value);

                m_lowererMD.LoadHelperArgument(stElem, regSrc);
                IR::Opnd *indexOpnd = indirOpnd->GetIndexOpnd();
                if (indexOpnd == nullptr)
                {
                    indexOpnd = IR::IntConstOpnd::New(indirOpnd->GetOffset(), TyInt32, this->m_func);
                }
                else
                {
                    Assert(indirOpnd->GetOffset() == 0);
                }
                m_lowererMD.LoadHelperArgument(stElem, indexOpnd);
                m_lowererMD.LoadHelperArgument(stElem, stElem->GetDst()->AsIndirOpnd()->GetBaseOpnd());

                IR::Instr *instr = IR::Instr::New(Js::OpCode::Call, this->m_func);

                Assert(objectType == ObjectType::Uint8ClampedArray || objectType == ObjectType::Uint8ClampedMixedArray || objectType == ObjectType::Uint8ClampedVirtualArray);
                instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperUint8ClampedArraySetItem, this->m_func));

                stElem->InsertBefore(instr);
                m_lowererMD.LowerCall(instr, 0);

                // JMP $fallThrough
                InsertBranch(Js::OpCode::Br, labelFallThru, stElem);

                //$inlineSet
                stElem->InsertBefore(labelInlineSet);

                IR::RegOpnd *regOpnd;
                IR::AutoReuseOpnd autoReuseRegOpnd;
#if INT32VAR
                regOpnd = regSrc;
#else
                // MOV r1, src
                // SAR r1, 1
                regOpnd = IR::RegOpnd::New(TyInt32, this->m_func);
                autoReuseRegOpnd.Initialize(regOpnd, m_func);
                InsertShift(
                    Js::OpCode::Shr_A,
                    false /* needFlags */,
                    regOpnd,
                    regSrc,
                    IR::IntConstOpnd::New(1, TyInt8, this->m_func),
                    stElem);
#endif

                // MOV IndirOpnd.u8, reg.u8
                InsertMove(indirOpnd, regOpnd, stElem);
            }
        }
        else
        {
            if (src->IsInt32())
            {
                // MOV indirOpnd, src
                InsertMove(indirOpnd, src, stElem);

                emitBailout = true;
            }
            else if (src->IsFloat64())
            {
                AssertMsg(indirOpnd->GetType() == TyUint32, "Only StElemI to Uint32Array could be specialized to float64.");
#ifdef _M_IX86
                AssertMsg(AutoSystemInfo::Data.SSE2Available(), "GloOpt shouldn't have specialized Uint32Array StElemI to float64 if SSE2 is unavailable.");
#endif

                IR::RegOpnd *const reg = IR::RegOpnd::New(TyInt32, this->m_func);
                const IR::AutoReuseOpnd autoReuseReg(reg, m_func);
                m_lowererMD.EmitFloatToInt(reg, src, stElem);

                // MOV indirOpnd, reg
                InsertMove(indirOpnd, reg, stElem);

                emitBailout = true;
            }
            else
            {
                Assert(src->IsVar());

                if(src->IsAddrOpnd())
                {
                    IR::AddrOpnd *const addrSrc = src->AsAddrOpnd();
                    Assert(addrSrc->IsVar());
                    Assert(Js::TaggedInt::Is(addrSrc->m_address));

                    // MOV indirOpnd, intValue
                    InsertMove(
                        indirOpnd,
                        IR::IntConstOpnd::New(Js::TaggedInt::ToInt32(addrSrc->m_address), TyInt32, m_func),
                        stElem);
                }
                else
                {
                    IR::RegOpnd *const regSrc = src->AsRegOpnd();

                    // FromVar reg, Src
                    IR::RegOpnd *const reg = IR::RegOpnd::New(TyInt32, this->m_func);
                    const IR::AutoReuseOpnd autoReuseReg(reg, m_func);
                    IR::Instr *const instr = IR::Instr::New(Js::OpCode::FromVar, reg, regSrc, stElem->m_func);
                    stElem->InsertBefore(instr);

                    // Convert reg to int32
                    // Note: ToUint32 is implemented as (uint32)ToInt32()
                    m_lowererMD.EmitLoadInt32(instr);

                    // MOV indirOpnd, reg
                    InsertMove(indirOpnd, reg, stElem);
                }
            }
        }
    }
    else
    {
        if(labelSegmentLengthIncreased)
        {
            IR::Instr *const insertBeforeInstr = labelSegmentLengthIncreased->m_next;

            // labelSegmentLengthIncreased:
            //     mov  [segment + index], src
            //     jmp  $fallThru
            InsertMove(indirOpnd, src, insertBeforeInstr);
            InsertBranch(Js::OpCode::Br, labelFallThru, insertBeforeInstr);
        }

        if (!(isStringIndex || baseValueType.IsArrayOrObjectWithArray() && baseValueType.HasNoMissingValues()))
        {
            if(!stElem->IsProfiledInstr() || stElem->AsProfiledInstr()->u.stElemInfo->LikelyFillsMissingValue())
            {
                // Check whether the store is filling a missing value. If so, fall back to the helper so that it can check whether
                // this store is filling the last missing value in the array. This is necessary to keep the missing value tracking
                // in arrays precise. The check is omitted when profile data says that the store is likely to create missing values.
                //
                //     cmp  [segment + index], Js::SparseArraySegment::MissingValue
                //     je   $helper
                InsertCompareBranch(
                    indirOpnd,
                    GetMissingItemOpnd(src->GetType(), m_func),
                    Js::OpCode::BrEq_A,
                    labelHelper,
                    stElem,
                    true);
            }
            else
            {
                GenerateIsEnabledArraySetElementFastPathCheck(labelHelper, stElem);
            }
        }

        //  MOV [r3 + r2], src
        InsertMoveWithBarrier(indirOpnd, src, stElem);
    }

    // JMP $fallThru
    InsertBranch(Js::OpCode::Br, labelFallThru, stElem);

    // $helper:
    // bailout or caller generated helper call
    // $fallThru:

    stElem->InsertBefore(labelHelper);
    instrIsInHelperBlock = true;

    if (isNativeArrayStore && !isStringIndex)
    {
        Assert(stElem->HasBailOutInfo());
        Assert(labelHelper != labelBailOut);

        // Transform the original instr:
        //
        // $helper:
        // dst = LdElemI_A src (BailOut)
        // $fallthrough:
        //
        // to:
        //
        // $helper:
        // dst = LdElemI_A src
        // b $fallthrough
        // $bailout:
        //       BailOut
        // $fallthrough:

        LowerOneBailOutKind(stElem, IR::BailOutConventionalNativeArrayAccessOnly, instrIsInHelperBlock);
        IR::Instr *const insertBeforeInstr = stElem->m_next;
        InsertBranch(Js::OpCode::Br, labelFallThru, insertBeforeInstr);
        insertBeforeInstr->InsertBefore(labelBailOut);
    }

    if (emitBailout)
    {
        stElem->UnlinkSrc1();
        stElem->UnlinkDst();
        GenerateBailOut(stElem, nullptr, nullptr);
    }

    return !emitBailout;
}

bool
Lowerer::GenerateFastLdLen(IR::Instr *ldLen, bool *instrIsInHelperBlockRef)
{
    Assert(instrIsInHelperBlockRef);
    bool &instrIsInHelperBlock = *instrIsInHelperBlockRef;
    instrIsInHelperBlock = false;

    //     TEST src, AtomTag                  -- check src not tagged int
    //     JNE $helper
    //     CMP [src], JavascriptArray::`vtable' -- check base isArray
    //     JNE $string
    //     MOV length, [src + offset(length)]     -- Load array length
    //     JMP $tovar
    // $string:
    //     CMP [src + offset(type)], static_string_type -- check src isString
    //     JNE $helper
    //     MOV length, [src + offset(length)]     -- Load string length
    // $toVar:
    //     TEST length, 0xC0000000       -- test for overflow of SHL, or negative
    //     JNE $helper
    //     SHL length, Js::VarTag_Shift  -- restore the var tag on the result
    //     INC length
    //     MOV dst, length
    //     JMP $fallthru
    // $helper:
    //     CALL GetProperty(src, length_property_id, scriptContext)
    // $fallthru:

    IR::RegOpnd *       opnd = ldLen->GetSrc1()->AsRegOpnd();
    IR::RegOpnd *       dst = ldLen->GetDst()->AsRegOpnd();
    IR::RegOpnd *       src = opnd->AsRegOpnd();
    const ValueType     srcValueType(src->GetValueType());

    AssertMsg(src->IsRegOpnd(), "Expected regOpnd on LdLen");

    IR::LabelInstr *const labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    if (ldLen->DoStackArgsOpt(this->m_func))
    {
        GenerateFastArgumentsLdLen(ldLen, labelHelper, ldLen->GetOrCreateContinueLabel());
    }
    else
    {
        const bool arrayFastPath = ShouldGenerateArrayFastPath(src, false, true, false);

        // HasBeenString instead of IsLikelyString because it could be a merge between StringObject and String, and this
        // information about whether it's a StringObject or some other object is not available in the profile data
        const bool stringFastPath = srcValueType.IsUninitialized() || srcValueType.HasBeenString();

        if(!(arrayFastPath || stringFastPath))
        {
            return true;
        }

        const int32 arrayOffsetOfLength =
            srcValueType.IsLikelyAnyOptimizedArray()
                ? GetArrayOffsetOfLength(srcValueType)
                : Js::JavascriptArray::GetOffsetOfLength();
        IR::LabelInstr *labelString = nullptr;
        IR::RegOpnd *arrayOpnd = src;
        IR::RegOpnd *arrayLengthOpnd = nullptr;
        IR::AutoReuseOpnd autoReuseArrayLengthOpnd;
        if(arrayFastPath)
        {
            if(!srcValueType.IsAnyOptimizedArray())
            {
                if(stringFastPath)
                {
                    // If we don't have info about the src value type or its object type, the array and string fast paths are
                    // generated
                    labelString = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
                }

                arrayOpnd = GenerateArrayTest(src, labelHelper, stringFastPath ? labelString : labelHelper, ldLen, false);
            }
            else if(src->IsArrayRegOpnd())
            {
                IR::ArrayRegOpnd *const arrayRegOpnd = src->AsArrayRegOpnd();
                if(arrayRegOpnd->LengthSym())
                {
                    arrayLengthOpnd = IR::RegOpnd::New(arrayRegOpnd->LengthSym(), TyUint32, m_func);
                    DebugOnly(arrayLengthOpnd->FreezeSymValue());
                    autoReuseArrayLengthOpnd.Initialize(arrayLengthOpnd, m_func);
                }
            }
        }
        const IR::AutoReuseOpnd autoReuseArrayOpnd(arrayOpnd, m_func);

        IR::RegOpnd *lengthOpnd = nullptr;
        IR::AutoReuseOpnd autoReuseLengthOpnd;
        const auto EnsureLengthOpnd = [&]()
        {
            if(lengthOpnd)
            {
                return;
            }

            lengthOpnd = IR::RegOpnd::New(TyUint32, m_func);
            autoReuseLengthOpnd.Initialize(lengthOpnd, m_func);
        };

        if(arrayFastPath)
        {
            if(arrayLengthOpnd)
            {
                lengthOpnd = arrayLengthOpnd;
                autoReuseLengthOpnd.Initialize(lengthOpnd, m_func);
                Assert(!stringFastPath);
            }
            else
            {
                //     MOV length, [array + offset(length)]     -- Load array length
                EnsureLengthOpnd();
                IR::IndirOpnd *const indirOpnd = IR::IndirOpnd::New(arrayOpnd, arrayOffsetOfLength, TyUint32, this->m_func);
                InsertMove(lengthOpnd, indirOpnd, ldLen);
            }
        }

        if(stringFastPath)
        {
            IR::LabelInstr *labelToVar = nullptr;
            if(arrayFastPath)
            {
                //     JMP $tovar
                labelToVar = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
                InsertBranch(Js::OpCode::Br, labelToVar, ldLen);

                // $string:
                ldLen->InsertBefore(labelString);
            }

            //  CMP [src + offset(type)], static_stringtype -- check src isString
            //  JNE $helper
            GenerateStringTest(src, ldLen, labelHelper, nullptr, !arrayFastPath);

            //  MOV length, [src + offset(length)]     -- Load string length
            EnsureLengthOpnd();
            IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(src, offsetof(Js::JavascriptString, m_charLength), TyUint32, this->m_func);
            InsertMove(lengthOpnd, indirOpnd, ldLen);

            if(arrayFastPath)
            {
                // $toVar:
                ldLen->InsertBefore(labelToVar);
            }
        }

        Assert(lengthOpnd);

        if(ldLen->HasBailOutInfo() && (ldLen->GetBailOutKind() & ~IR::BailOutKindBits) == IR::BailOutOnIrregularLength)
        {
            Assert(ldLen->GetBailOutKind() == IR::BailOutOnIrregularLength);
            Assert(dst->IsInt32());

            // Since the length is an unsigned int32, verify that when interpreted as a signed int32, it is not negative
            //     test length, length
            //     js $helper
            //     mov dst, length
            //     jmp $fallthrough
            InsertCompareBranch(
                lengthOpnd,
                IR::IntConstOpnd::New(0, lengthOpnd->GetType(), m_func, true),
                Js::OpCode::BrLt_A,
                labelHelper,
                ldLen);
            InsertMove(dst, lengthOpnd, ldLen);
            InsertBranch(Js::OpCode::Br, ldLen->GetOrCreateContinueLabel(), ldLen);

            // $helper:
            //     (Bail out with IR::BailOutOnIrregularLength)
            ldLen->InsertBefore(labelHelper);
            instrIsInHelperBlock = true;
            ldLen->FreeDst();
            ldLen->FreeSrc1();
            GenerateBailOut(ldLen);
            return false;
        }

    #if INT32VAR
        // Since the length is an unsigned int32, verify that when interpreted as a signed int32, it is not negative
        //     test length, length
        //     js $helper
        InsertCompareBranch(
            lengthOpnd,
            IR::IntConstOpnd::New(0, lengthOpnd->GetType(), m_func, true),
            Js::OpCode::BrLt_A,
            labelHelper,
            ldLen);
    #else
        // Since the length is an unsigned int32, verify that when interpreted as a signed int32, it is not negative.
        // Additionally, verify that the signed value's width is not greater than 31 bits, since it needs to be tagged.
        //     test length, 0xC0000000
        //     jne $helper
        InsertTestBranch(
            lengthOpnd,
            IR::IntConstOpnd::New(0xC0000000, TyUint32, this->m_func, true),
            Js::OpCode::BrNeq_A,
            labelHelper,
            ldLen);
    #endif

    #if INT32VAR
        //
        // dst_32 = MOV length
        // dst_64 = OR dst_64, Js::AtomTag_IntPtr
        //
        Assert(dst->GetType() == TyVar);

        IR::Opnd *dst32 = dst->Copy(this->m_func);
        dst32->SetType(TyInt32);

        // This will clear the top bits.
        InsertMove(dst32, lengthOpnd, ldLen);

        m_lowererMD.GenerateInt32ToVarConversion(dst, ldLen);
    #else
        // dst = SHL length, Js::VarTag_Shift  -- restore the var tag on the result
        InsertShift(
            Js::OpCode::Shl_A,
            false /* needFlags */,
            dst,
            lengthOpnd,
            IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func),
            ldLen);

        // dst = ADD dst, AtomTag
        InsertAdd(
            false /* needFlags */,
            dst,
            dst,
            IR::IntConstOpnd::New(Js::AtomTag_Int32, TyUint32, m_func, true),
            ldLen);
    #endif

        //  JMP $fallthrough
        InsertBranch(Js::OpCode::Br, ldLen->GetOrCreateContinueLabel(), ldLen);
    }

    // $helper:
    //      (caller generates helper call)
    ldLen->InsertBefore(labelHelper);
    instrIsInHelperBlock = true;

    return true; // fast path was generated, helper call will be in a helper block
}

void
Lowerer::GenerateFastInlineStringCodePointAt(IR::Instr* lastInstr, Func* func, IR::Opnd *strLength, IR::Opnd *srcIndex, IR::RegOpnd *lowerChar, IR::RegOpnd *strPtr)
{
    //// Required State:
    // strLength - UInt32
    // srcIndex  - TyVar if not Address
    // lowerChar - TyMachReg
    // strPtr    - Addr
    //// Instructions
    //                  CMP [strLength], srcIndex + 1
    //                  JBE charCodeAt
    //                  CMP lowerChar 0xDC00
    //                  JGE charCodeAt
    //                  CMP lowerChar 0xD7FF
    //                  JLE charCodeAt
    // upperChar      = MOVZX [strPtr + srcIndex + 1]
    //                  CMP upperChar 0xE000
    //                  JGE charCodeAt
    //                  CMP lowerChar 0xDBFF
    //                  JLE charCodeAt
    // lowerChar      = SUB lowerChar - 0xD800
    // lowerChar      = SHL lowerChar, 10
    // lowerChar      = ADD lowerChar + upperChar
    // lowerChar      = ADD lowerChar + 0x2400
    // :charCodeAt
    // :done

    // Asserts
    // Arm should change to Uint32 for the strLength
    Assert(strLength->GetType() == TyUint32 || strLength->GetType() == TyMachReg);
    Assert(srcIndex->GetType() == TyVar || srcIndex->IsAddrOpnd());
    Assert(lowerChar->GetType() == TyMachReg || lowerChar->GetType() == TyUint32);
    Assert(strPtr->IsRegOpnd());

    IR::RegOpnd *tempReg = IR::RegOpnd::New(TyMachReg, func);
    IR::LabelInstr *labelCharCodeAt = IR::LabelInstr::New(Js::OpCode::Label, func);
    IR::IndirOpnd *tempIndirOpnd;

    if (srcIndex->IsAddrOpnd())
    {
        uint32 length = Js::TaggedInt::ToUInt32(srcIndex->AsAddrOpnd()->m_address) + 1U;
        InsertCompareBranch(strLength, IR::IntConstOpnd::New(length, TyUint32, func), Js::OpCode::BrLe_A, true, labelCharCodeAt, lastInstr);
        tempIndirOpnd = IR::IndirOpnd::New(strPtr, (length) * sizeof(wchar_t), TyUint16, func);
    }
    else
    {
        InsertMove(tempReg, srcIndex, lastInstr);

#if INT32VAR
        IR::Opnd * reg32Bit = tempReg->UseWithNewType(TyInt32, func);
        InsertMove(tempReg, reg32Bit, lastInstr);
        tempReg = reg32Bit->AsRegOpnd();
#else
        InsertShift(Js::OpCode::Shr_A, false, tempReg, tempReg, IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, func), lastInstr);
#endif
        InsertAdd(false, tempReg, tempReg, IR::IntConstOpnd::New(1, TyInt32, func), lastInstr);

        InsertCompareBranch(strLength, tempReg, Js::OpCode::BrLe_A, true, labelCharCodeAt, lastInstr);

        if(tempReg->GetSize() != MachPtr)
        {
            tempReg = tempReg->UseWithNewType(TyMachPtr, func)->AsRegOpnd();
        }

        tempIndirOpnd = IR::IndirOpnd::New(strPtr, tempReg, 1, TyUint16, func);
    }

    // By this point, we have added instructions before labelCharCodeAt to check for extra length required for the surrogate pair
    // The branching for that is already handled, all we have to do now is to check for correct values.
    // Validate char is in range [D800, DBFF]; otherwise just get a charCodeAt
    InsertCompareBranch(lowerChar, IR::IntConstOpnd::New(0xDC00, TyUint32, func), Js::OpCode::BrGe_A, labelCharCodeAt, lastInstr);
    InsertCompareBranch(lowerChar, IR::IntConstOpnd::New(0xD7FF, TyUint32, func), Js::OpCode::BrLe_A, labelCharCodeAt, lastInstr);

    // upperChar      = MOVZX r3, [r1 + r3 * 2]  -- this is the value of the upper surrogate pair char
    IR::RegOpnd *upperChar = IR::RegOpnd::New(TyInt32, func);
    InsertMove(upperChar, tempIndirOpnd, lastInstr);

    // Validate upper is in range [DC00, DFFF]; otherwise just get a charCodeAt
    InsertCompareBranch(upperChar, IR::IntConstOpnd::New(0xE000, TyUint32, func), Js::OpCode::BrGe_A, labelCharCodeAt, lastInstr);
    InsertCompareBranch(upperChar, IR::IntConstOpnd::New(0xDBFF, TyUint32, func), Js::OpCode::BrLe_A, labelCharCodeAt, lastInstr);

    // (lower - 0xD800) << 10 + second - 0xDC00 + 0x10000 -- 0x10000 - 0xDC00 = 0x2400
    // lowerChar      = SUB lowerChar - 0xD800
    // lowerChar      = SHL lowerChar, 10
    // lowerChar      = ADD lowerChar + upperChar
    // lowerChar      = ADD lowerChar + 0x2400
    InsertSub(false, lowerChar, lowerChar, IR::IntConstOpnd::New(0xD800, TyUint32, func), lastInstr);
    InsertShift(Js::OpCode::Shl_A, false, lowerChar, lowerChar, IR::IntConstOpnd::New(10, TyUint32, func), lastInstr);
    InsertAdd(false, lowerChar, lowerChar, upperChar, lastInstr);
    InsertAdd(false, lowerChar, lowerChar, IR::IntConstOpnd::New(0x2400, TyUint32, func), lastInstr);

    lastInstr->InsertBefore(labelCharCodeAt);
}

bool
Lowerer::GenerateFastInlineStringFromCodePoint(IR::Instr* instr)
{
    Assert(instr->m_opcode == Js::OpCode::CallDirect);

//  ArgOut sequence
//  s8.var          =  StartCall      2 (0x2).i32                             #000c
//  arg1(s9)<0>.var =  ArgOut_A       s2.var, s8.var                          #0014 //Implicit this, String object
//  arg2(s10)<4>.var = ArgOut_A       s3.var, arg1(s9)<0>.var                 #0018 //First argument to FromCharCode
//  arg1(s11)<0>.u32 = ArgOut_A_InlineSpecialized  0x012C26C0 (DynamicObject).var, arg2(s10)<4>.var #
//  s0[LikelyTaggedInt].var = CallDirect  String_FromCodePoint.u32, arg1(s11)<0>.u32 #001c

    IR::Opnd * linkOpnd = instr->GetSrc2();
    IR::Instr * tmpInstr = Inline::GetDefInstr(linkOpnd);// linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;
    linkOpnd = tmpInstr->GetSrc2();

#if DBG
    IntConstType argCount = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();
    Assert(argCount == 2);
#endif

    IR::Instr *argInstr = Inline::GetDefInstr(linkOpnd);
    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A);
    IR::Opnd *src1 = argInstr->GetSrc1();

    if (src1->GetValueType().IsLikelyNumber())
    {
        //Trying to generate this code
        //      MOV resultOpnd, dst
        //      MOV fromCharCodeIntArgOpnd, src1
        //      SAR fromCharCodeIntArgOpnd, Js::VarTag_Shift
        //      JAE $Helper
        //      CMP fromCharCodeIntArgOpnd, Js::ScriptContext::CharStringCacheSize
        //
        //      JAE $labelWCharStringCheck                                   <
        //      MOV resultOpnd, GetCharStringCache[fromCharCodeIntArgOpnd]
        //      TST resultOpnd, resultOpnd          //Check for null
        //      JEQ $helper
        //      JMP $Done
        //
        //$labelWCharStringCheck:
        //      resultOpnd =  Call HelperGetStringForCharW
        //      JMP $Done
        //$helper:
        IR::RegOpnd * resultOpnd = nullptr;
        if (!instr->GetDst()->IsRegOpnd() || instr->GetDst()->IsEqual(src1))
        {
            resultOpnd = IR::RegOpnd::New(TyVar, this->m_func);
        }
        else
        {
            resultOpnd = instr->GetDst()->AsRegOpnd();
        }

        IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

        IR::RegOpnd * fromCodePointIntArgOpnd = IR::RegOpnd::New(TyVar, instr->m_func);
        IR::AutoReuseOpnd autoReuseFromCodePointIntArgOpnd(fromCodePointIntArgOpnd, instr->m_func);
        InsertMove(fromCodePointIntArgOpnd, src1, instr);

        //Check for tagged int and get the untagged version.
        fromCodePointIntArgOpnd = GenerateUntagVar(fromCodePointIntArgOpnd, labelHelper, instr);

        GenerateGetSingleCharString(fromCodePointIntArgOpnd, resultOpnd, labelHelper, doneLabel, instr, true);

        instr->InsertBefore(labelHelper);

        instr->InsertAfter(doneLabel);

        RelocateCallDirectToHelperPath(tmpInstr, labelHelper);
    }
    return true;
}

bool
Lowerer::GenerateFastInlineStringFromCharCode(IR::Instr* instr)
{
    Assert(instr->m_opcode == Js::OpCode::CallDirect);

//  ArgOut sequence
//  s8.var          =  StartCall      2 (0x2).i32                             #000c
//  arg1(s9)<0>.var =  ArgOut_A       s2.var, s8.var                          #0014 //Implicit this, String object
//  arg2(s10)<4>.var = ArgOut_A       s3.var, arg1(s9)<0>.var                 #0018 //First argument to FromCharCode
//  arg1(s11)<0>.u32 = ArgOut_A_InlineSpecialized  0x012C26C0 (DynamicObject).var, arg2(s10)<4>.var #
//  s0[LikelyTaggedInt].var = CallDirect  String_FromCharCode.u32, arg1(s11)<0>.u32 #001c

    IR::Opnd * linkOpnd = instr->GetSrc2();
    IR::Instr * tmpInstr = Inline::GetDefInstr(linkOpnd);// linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;
    linkOpnd = tmpInstr->GetSrc2();

#if DBG
    IntConstType argCount = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();
    Assert(argCount == 2);
#endif

    IR::Instr *argInstr = Inline::GetDefInstr(linkOpnd);
    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A);
    IR::Opnd *src1 = argInstr->GetSrc1();

    if (src1->GetValueType().IsLikelyNumber())
    {
        //Trying to generate this code
        //      MOV resultOpnd, dst
        //      MOV fromCharCodeIntArgOpnd, src1
        //      SAR fromCharCodeIntArgOpnd, Js::VarTag_Shift
        //      JAE $Helper
        //      CMP fromCharCodeIntArgOpnd, Js::ScriptContext::CharStringCacheSize
        //
        //      JAE $labelWCharStringCheck                                   <
        //      MOV resultOpnd, GetCharStringCache[fromCharCodeIntArgOpnd]
        //      TST resultOpnd, resultOpnd          //Check for null
        //      JEQ $helper
        //      JMP $Done
        //
        //$labelWCharStringCheck:
        //      resultOpnd =  Call HelperGetStringForCharW
        //      JMP $Done
        //$helper:
        IR::RegOpnd * resultOpnd = nullptr;
        if (!instr->GetDst()->IsRegOpnd() || instr->GetDst()->IsEqual(src1))
        {
            resultOpnd = IR::RegOpnd::New(TyVar, this->m_func);
        }
        else
        {
            resultOpnd = instr->GetDst()->AsRegOpnd();
        }

        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

        IR::RegOpnd * fromCharCodeIntArgOpnd = IR::RegOpnd::New(TyVar, instr->m_func);
        IR::AutoReuseOpnd autoReuseFromCharCodeIntArgOpnd(fromCharCodeIntArgOpnd, instr->m_func);
        InsertMove(fromCharCodeIntArgOpnd, src1, instr);

        //Check for tagged int and get the untagged version.
        fromCharCodeIntArgOpnd = GenerateUntagVar(fromCharCodeIntArgOpnd, labelHelper, instr);

        IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        GenerateGetSingleCharString(fromCharCodeIntArgOpnd, resultOpnd, labelHelper, doneLabel, instr, false);

        instr->InsertBefore(labelHelper);

        instr->InsertAfter(doneLabel);

        RelocateCallDirectToHelperPath(tmpInstr, labelHelper);
    }
    return true;
}

void
Lowerer::GenerateGetSingleCharString(IR::RegOpnd * charCodeOpnd, IR::Opnd * resultOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * doneLabel, IR::Instr * instr, bool isCodePoint)
{
    //      MOV cacheReg, CharStringCache
    //      CMP charCodeOpnd, Js::ScriptContext::CharStringCacheSize
    //      JAE $labelWCharStringCheck                                   <
    //      MOV resultOpnd, cacheReg[charCodeOpnd]
    //      TST resultOpnd, resultOpnd          //Check for null
    //      JEQ $helper
    //      JMP $Done
    //
    //$labelWCharStringCheck:
    //            Arg1 =  charCodeOpnd
    //            Arg0 =  cacheReg
    //      resultOpnd =  Call HelperGetStringForCharW/CodePoint
    //      JMP $Done
    //$helper:

    IR::LabelInstr *labelWCharStringCheck = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    //Try to load from in  CharStringCacheA
    IR::RegOpnd *cacheRegOpnd = IR::RegOpnd::New(TyVar, instr->m_func);
    IR::AutoReuseOpnd autoReuseCacheRegOpnd(cacheRegOpnd, instr->m_func);

    Assert(Js::JavascriptLibrary::GetCharStringCacheAOffset() == Js::JavascriptLibrary::GetCharStringCacheOffset());
    InsertMove(cacheRegOpnd, this->LoadLibraryValueOpnd(instr, LibraryValue::ValueCharStringCache), instr);

    InsertCompareBranch(charCodeOpnd, IR::IntConstOpnd::New(Js::CharStringCache::CharStringCacheSize, TyUint32, this->m_func), Js::OpCode::BrGe_A, true, labelWCharStringCheck, instr);
    InsertMove(resultOpnd, IR::IndirOpnd::New(cacheRegOpnd, charCodeOpnd, this->m_lowererMD.GetDefaultIndirScale(), TyVar, instr->m_func), instr);

    InsertTestBranch(resultOpnd, resultOpnd, Js::OpCode::BrEq_A, labelHelper, instr);

    InsertMove(instr->GetDst(), resultOpnd, instr);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);

    instr->InsertBefore(labelWCharStringCheck);

    IR::JnHelperMethod helperMethod;
    if (isCodePoint)
    {
        helperMethod = IR::HelperGetStringForCharCodePoint;
    }
    else
    {
        InsertMove(charCodeOpnd, charCodeOpnd->UseWithNewType(TyUint16, instr->m_func), instr);
        helperMethod = IR::HelperGetStringForChar;
    }

    //Try to load from in  CharStringCacheW or CharStringCacheCodePoint, this is a helper call.

    this->m_lowererMD.LoadHelperArgument(instr, charCodeOpnd);
    this->m_lowererMD.LoadHelperArgument(instr, cacheRegOpnd);
    IR::Instr* helperCallInstr = IR::Instr::New(Js::OpCode::Call, resultOpnd, IR::HelperCallOpnd::New(helperMethod, this->m_func), this->m_func);
    instr->InsertBefore(helperCallInstr);
    this->m_lowererMD.LowerCall(helperCallInstr, 0);

    InsertMove(instr->GetDst(), resultOpnd, instr);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);
}

bool
Lowerer::GenerateFastInlineGlobalObjectParseInt(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::CallDirect);

//  ArgOut sequence
//  s8.var          =  StartCall      2 (0x2).i32                             #000c
//  arg1(s9)<0>.var =  ArgOut_A       s2.var, s8.var                          #0014 //Implicit this, global object
//  arg2(s10)<4>.var = ArgOut_A       s3.var, arg1(s9)<0>.var                 #0018 //First argument to parseInt
//  arg1(s11)<0>.u32 = ArgOut_A_InlineSpecialized  0x012C26C0 (DynamicObject).var, arg2(s10)<4>.var #
//  s0[LikelyTaggedInt].var = CallDirect  GlobalObject_ParseInt.u32, arg1(s11)<0>.u32 #001c

    IR::Opnd * linkOpnd = instr->GetSrc2();
    IR::Instr * tmpInstr = Inline::GetDefInstr(linkOpnd);// linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;
    linkOpnd = tmpInstr->GetSrc2();

#if DBG
    IntConstType argCount = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();
    Assert(argCount == 2);
#endif

    IR::Instr *argInstr = Inline::GetDefInstr(linkOpnd);
    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A);
    IR::Opnd *parseIntArgOpnd = argInstr->GetSrc1();

    if (parseIntArgOpnd->GetValueType().IsLikelyNumber())
    {
        //If likely int check for tagged int and set the dst
        IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

        if (!parseIntArgOpnd->IsTaggedInt())
        {
            this->m_lowererMD.GenerateSmIntTest(parseIntArgOpnd, instr, labelHelper);
        }
        if (instr->GetDst())
        {
            this->m_lowererMD.CreateAssign(instr->GetDst(), parseIntArgOpnd, instr);
        }
        InsertBranch(Js::OpCode::Br, doneLabel, instr);
        instr->InsertBefore(labelHelper);
        instr->InsertAfter(doneLabel);

        RelocateCallDirectToHelperPath(tmpInstr, labelHelper);
    }
    return true;
}

void
Lowerer::GenerateFastInlineArrayPop(IR::Instr * instr)
{
    Assert(instr->m_opcode == Js::OpCode::InlineArrayPop);

    IR::Opnd *arrayOpnd = instr->GetSrc1();

    IR::LabelInstr *bailOutLabelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    bool isLikelyNativeArray = arrayOpnd->GetValueType().IsLikelyNativeArray();

    if (ShouldGenerateArrayFastPath(arrayOpnd, false, false, false))
    {
        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

        if(isLikelyNativeArray)
        {
            //We bailOut on cases like length == 0, Array Test failing cases (Runtime helper cannot handle these cases)
            GenerateFastPop(arrayOpnd, instr, labelHelper, doneLabel, bailOutLabelHelper);
        }
        else
        {
            //We jump to helper on cases like length == 0, Array Test failing cases
            GenerateFastPop(arrayOpnd, instr, labelHelper, doneLabel, labelHelper);
        }

        instr->InsertBefore(labelHelper);

        ///JMP to $doneLabel
        InsertBranch(Js::OpCode::Br, true, doneLabel, labelHelper);
    }
    else
    {
        //We assume here that the array will be a Var array. - Runtime Helper calls assume this.
        Assert(!isLikelyNativeArray);
    }

    instr->InsertAfter(doneLabel);

    if(isLikelyNativeArray)
    {
        //Lower IR::BailOutConventionalNativeArrayAccessOnly here.
        LowerOneBailOutKind(instr, IR::BailOutConventionalNativeArrayAccessOnly, false, false);
        instr->InsertAfter(bailOutLabelHelper);
    }

    GenerateHelperToArrayPopFastPath(instr, doneLabel, bailOutLabelHelper);
}

bool
Lowerer::ShouldGenerateStringReplaceFastPath(IR::Instr * callInstr, IntConstType argCount)
{
    // a.replace(b,c)
    // We want to emit the fast path if 'a' and 'c' are strings and 'b' is a regex
    //
    // argout sequence:
    // arg1(s12)<0>.var = ArgOut_A       s2.var, s11.var                         #0014   <---- a
    // arg2(s13)<4>.var = ArgOut_A       s3.var, arg1(s12)<0>.var                #0018   <---- b
    // arg3(s14)<8>.var = ArgOut_A       s4.var, arg2(s13)<4>.var                #001c   <---- c
    // s0[LikelyString].var = CallI      s5[ffunc].var, arg3(s14)<8>.var         #0020

    IR::Opnd *linkOpnd = callInstr->GetSrc2();
    Assert(argCount == 2);

    while(linkOpnd->IsSymOpnd())
    {
        IR::SymOpnd *src2 = linkOpnd->AsSymOpnd();
        StackSym *sym = src2->m_sym->AsStackSym();
        Assert(sym->m_isSingleDef);
        IR::Instr *argInstr = sym->m_instrDef;

        Assert(argCount >= 0);
        // check to see if 'a' and 'c' are likely strings
        if((argCount == 2 || argCount == 0) && (!argInstr->GetSrc1()->GetValueType().IsLikelyString()))
        {
            return false;
        }
        // we want 'b' to be regex. Don't generate fastpath if it is a tagged int
        if((argCount == 1) && (argInstr->GetSrc1()->IsTaggedInt()))
        {
            return false;
        }
        argCount--;
        linkOpnd = argInstr->GetSrc2();
    }
    return true;
}

bool
Lowerer::GenerateFastReplace(IR::Opnd* strOpnd, IR::Opnd* src1, IR::Opnd* src2, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel)
{
    // a.replace(b,c)
    // We want to emit the fast path if 'a' and 'c' are strings and 'b' is a regex
    //
    // strOpnd --> a
    // src1    --> b
    // src2    --> c

    IR::Opnd * callDst = callInstr->GetDst();

    Assert(strOpnd->GetValueType().IsLikelyString() && src2->GetValueType().IsLikelyString());

    if(!strOpnd->GetValueType().IsString())
    {
        if(!strOpnd->IsRegOpnd())
        {
            IR::RegOpnd *strOpndReg = IR::RegOpnd::New(TyVar, m_func);
            LowererMD::CreateAssign(strOpndReg, strOpnd, insertInstr);
            strOpnd = strOpndReg;
        }

        this->GenerateStringTest(strOpnd->AsRegOpnd(), insertInstr, labelHelper);
    }

    if(!src1->IsNotTaggedValue())
    {
        m_lowererMD.GenerateObjectTest(src1, insertInstr, labelHelper);
    }

    IR::Opnd * vtableOpnd = LoadVTableValueOpnd(insertInstr, VTableValue::VtableJavascriptRegExp);

    // cmp  [regex], vtableAddress
    // jne  $labelHelper
    if(!src1->IsRegOpnd())
    {
        IR::RegOpnd *src1Reg = IR::RegOpnd::New(TyVar, m_func);
        LowererMD::CreateAssign(src1Reg, src1, insertInstr);
        src1 = src1Reg;
    }
    InsertCompareBranch(
        IR::IndirOpnd::New(src1->AsRegOpnd(), 0, TyMachPtr, insertInstr->m_func),
        vtableOpnd,
        Js::OpCode::BrNeq_A,
        labelHelper,
        insertInstr);

    if(!src2->GetValueType().IsString())
    {
        if(!src2->IsRegOpnd())
        {
            IR::RegOpnd *src2Reg = IR::RegOpnd::New(TyVar, m_func);
            LowererMD::CreateAssign(src2Reg, src2, insertInstr);
            src2 = src2Reg;
        }
        this->GenerateStringTest(src2->AsRegOpnd(), insertInstr, labelHelper);
    }

    //scriptContext, pRegEx, pThis, pReplace (to be pushed in reverse order)

    // pReplace, pThis, pRegEx
    this->m_lowererMD.LoadHelperArgument(insertInstr, src2);
    this->m_lowererMD.LoadHelperArgument(insertInstr, strOpnd);
    this->m_lowererMD.LoadHelperArgument(insertInstr, src1);

    // script context
    LoadScriptContext(insertInstr);

    IR::Instr * helperCallInstr = IR::Instr::New(LowererMD::MDCallOpcode, insertInstr->m_func);
    if(callDst)
    {
        helperCallInstr->SetDst(callDst);
    }
    insertInstr->InsertBefore(helperCallInstr);
    if(callDst)
    {
        m_lowererMD.ChangeToHelperCall(helperCallInstr, IR::JnHelperMethod::HelperRegExp_ReplaceStringResultUsed);
    }
    else
    {
        m_lowererMD.ChangeToHelperCall(helperCallInstr, IR::JnHelperMethod::HelperRegExp_ReplaceStringResultNotUsed);
    }

    return true;
}

///----

void
Lowerer::GenerateFastInlineStringSplitMatch(IR::Instr * instr)
{
    // a.split(b,c (optional) )
    // We want to emit the fast path when
    //     1. c is not present, and
    //     2. 'a' is a string and 'b' is a regex.
    //
    // a.match(b)
    // We want to emit the fast path when 'a' is a string and 'b' is a regex.

    Assert(instr->m_opcode == Js::OpCode::CallDirect);
    IR::Opnd * callDst = instr->GetDst();

    //helperCallOpnd
    IR::Opnd * src1 = instr->GetSrc1();

    //ArgOut_A_InlineSpecialized
    IR::Instr * tmpInstr = instr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;

    IR::Opnd * argsOpnd[2];
    if(!instr->FetchOperands(argsOpnd, 2))
    {
        return;
    }

    if(!argsOpnd[0]->GetValueType().IsLikelyString() || argsOpnd[1]->IsTaggedInt())
    {
        return;
    }

    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    if(!argsOpnd[0]->GetValueType().IsString())
    {
        if(!argsOpnd[0]->IsRegOpnd())
        {
            IR::RegOpnd *opndReg = IR::RegOpnd::New(TyVar, m_func);
            LowererMD::CreateAssign(opndReg, argsOpnd[0], instr);
            argsOpnd[0] = opndReg;
        }
        this->GenerateStringTest(argsOpnd[0]->AsRegOpnd(), instr, labelHelper);
    }

    if(!argsOpnd[1]->IsNotTaggedValue())
    {
        m_lowererMD.GenerateObjectTest(argsOpnd[1], instr, labelHelper);
    }

    IR::Opnd * vtableOpnd = LoadVTableValueOpnd(instr, VTableValue::VtableJavascriptRegExp);

    // cmp  [regex], vtableAddress
    // jne  $labelHelper
    if(!argsOpnd[1]->IsRegOpnd())
    {
        IR::RegOpnd *opndReg = IR::RegOpnd::New(TyVar, m_func);
        LowererMD::CreateAssign(opndReg, argsOpnd[1], instr);
        argsOpnd[1] = opndReg;
    }
    InsertCompareBranch(
        IR::IndirOpnd::New(argsOpnd[1]->AsRegOpnd(), 0, TyMachPtr, instr->m_func),
        vtableOpnd,
        Js::OpCode::BrNeq_A,
        labelHelper,
        instr);

    // [stackAllocationPointer, ]scriptcontext, regexp, input[, limit] (to be pushed in reverse order)

    if(src1->AsHelperCallOpnd()->m_fnHelper == IR::JnHelperMethod::HelperString_Split)
    {
        //limit
        //As we are optimizing only for two operands, make limit UINT_MAX
        IR::Opnd* limit = IR::IntConstOpnd::New(UINT_MAX, TyUint32, instr->m_func);
        this->m_lowererMD.LoadHelperArgument(instr, limit);
    }

    //input, regexp
    this->m_lowererMD.LoadHelperArgument(instr, argsOpnd[0]);
    this->m_lowererMD.LoadHelperArgument(instr, argsOpnd[1]);

    // script context
    LoadScriptContext(instr);

    IR::JnHelperMethod helperMethod;
    IR::AutoReuseOpnd autoReuseStackAllocationOpnd;
    if(callDst && instr->dstIsTempObject)
    {
        switch(src1->AsHelperCallOpnd()->m_fnHelper)
        {
            case IR::JnHelperMethod::HelperString_Split:
                helperMethod = IR::JnHelperMethod::HelperRegExp_SplitResultUsedAndMayBeTemp;
                break;

            case IR::JnHelperMethod::HelperString_Match:
                helperMethod = IR::JnHelperMethod::HelperRegExp_MatchResultUsedAndMayBeTemp;
                break;

            default:
                Assert(false);
                __assume(false);
        }

        // Allocate some space on the stack for the result array
        IR::RegOpnd *const stackAllocationOpnd = IR::RegOpnd::New(TyVar, m_func);
        autoReuseStackAllocationOpnd.Initialize(stackAllocationOpnd, m_func);
        stackAllocationOpnd->SetValueType(callDst->GetValueType());
        GenerateMarkTempAlloc(stackAllocationOpnd, Js::JavascriptArray::StackAllocationSize, instr);
        m_lowererMD.LoadHelperArgument(instr, stackAllocationOpnd);
    }
    else
    {
        switch(src1->AsHelperCallOpnd()->m_fnHelper)
        {
            case IR::JnHelperMethod::HelperString_Split:
                helperMethod =
                    callDst
                        ? IR::JnHelperMethod::HelperRegExp_SplitResultUsed
                        : IR::JnHelperMethod::HelperRegExp_SplitResultNotUsed;
                break;

            case IR::JnHelperMethod::HelperString_Match:
                helperMethod =
                    callDst
                        ? IR::JnHelperMethod::HelperRegExp_MatchResultUsed
                        : IR::JnHelperMethod::HelperRegExp_MatchResultNotUsed;
                break;

            default:
                Assert(false);
                __assume(false);
        }
    }

    IR::Instr * helperCallInstr = IR::Instr::New(LowererMD::MDCallOpcode, instr->m_func);
    if(callDst)
    {
        helperCallInstr->SetDst(callDst);
    }
    instr->InsertBefore(helperCallInstr);

    m_lowererMD.ChangeToHelperCall(helperCallInstr, helperMethod);

    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr->InsertAfter(doneLabel);
    instr->InsertBefore(labelHelper);
    InsertBranch(Js::OpCode::Br, true, doneLabel, labelHelper);

    RelocateCallDirectToHelperPath(tmpInstr, labelHelper);
}

void
Lowerer::GenerateFastInlineRegExpExec(IR::Instr * instr)
{
    // a.exec(b)
    // We want to emit the fast path when 'a' is a regex and 'b' is a string

    Assert(instr->m_opcode == Js::OpCode::CallDirect);
    IR::Opnd * callDst = instr->GetDst();

    //ArgOut_A_InlineSpecialized
    IR::Instr * tmpInstr = instr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;

    IR::Opnd * argsOpnd[2];
    if (!instr->FetchOperands(argsOpnd, 2))
    {
        return;
    }

    IR::Opnd *opndString = argsOpnd[1];
    if(!opndString->GetValueType().IsLikelyString() || argsOpnd[0]->IsTaggedInt())
    {
        return;
    }

    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    if(!opndString->GetValueType().IsString())
    {
        if(!opndString->IsRegOpnd())
        {
            IR::RegOpnd *opndReg = IR::RegOpnd::New(TyVar, m_func);
            LowererMD::CreateAssign(opndReg, opndString, instr);
            opndString = opndReg;
        }
        this->GenerateStringTest(opndString->AsRegOpnd(), instr, labelHelper);
    }

    IR::Opnd *opndRegex = argsOpnd[0];
    if(!opndRegex->IsNotTaggedValue())
    {
        m_lowererMD.GenerateObjectTest(opndRegex, instr, labelHelper);
    }

    IR::Opnd * vtableOpnd = LoadVTableValueOpnd(instr, VTableValue::VtableJavascriptRegExp);

    // cmp  [regex], vtableAddress
    // jne  $labelHelper
    if(!opndRegex->IsRegOpnd())
    {
        IR::RegOpnd *opndReg = IR::RegOpnd::New(TyVar, m_func);
        LowererMD::CreateAssign(opndReg, opndRegex, instr);
        opndRegex = opndReg;
    }
    InsertCompareBranch(
        IR::IndirOpnd::New(opndRegex->AsRegOpnd(), 0, TyMachPtr, instr->m_func),
        vtableOpnd,
        Js::OpCode::BrNeq_A,
        labelHelper,
        instr);

    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);

    if (!PHASE_OFF(Js::ExecBOIFastPathPhase, m_func))
    {
        // Load pattern from regex operand
        IR::RegOpnd *opndPattern = IR::RegOpnd::New(TyMachPtr, m_func);
        LowererMD::CreateAssign(
            opndPattern,
            IR::IndirOpnd::New(opndRegex->AsRegOpnd(), Js::JavascriptRegExp::GetOffsetOfPattern(), TyMachPtr, m_func),
            instr);

        // Load program from pattern
        IR::RegOpnd *opndProgram = IR::RegOpnd::New(TyMachPtr, m_func);
        LowererMD::CreateAssign(
            opndProgram,
            IR::IndirOpnd::New(opndPattern, offsetof(UnifiedRegex::RegexPattern, rep) + offsetof(UnifiedRegex::RegexPattern::UnifiedRep, program), TyMachPtr, m_func),
            instr);

        IR::LabelInstr *labelFastHelper = IR::LabelInstr::New(Js::OpCode::Label, m_func);

        // We want the program's tag to be BOILiteral2Tag
        InsertCompareBranch(
            IR::IndirOpnd::New(opndProgram, (int32)UnifiedRegex::Program::GetOffsetOfTag(), TyUint8, m_func),
            IR::IntConstOpnd::New(UnifiedRegex::Program::GetBOILiteral2Tag(), TyUint8, m_func),
            Js::OpCode::BrNeq_A,
            labelFastHelper,
            instr);

        // Test the program's flags for "global"
        InsertTestBranch(
            IR::IndirOpnd::New(opndProgram, offsetof(UnifiedRegex::Program, flags), TyUint8, m_func),
            IR::IntConstOpnd::New(UnifiedRegex::GlobalRegexFlag, TyUint8, m_func),
            Js::OpCode::BrNeq_A,
            labelFastHelper,
            instr);

        IR::LabelInstr *labelNoMatch = IR::LabelInstr::New(Js::OpCode::Label, m_func);

        // If string length < 2...
        InsertCompareBranch(
            IR::IndirOpnd::New(opndString->AsRegOpnd(), offsetof(Js::JavascriptString, m_charLength), TyUint32, m_func),
            IR::IntConstOpnd::New(2, TyUint32, m_func),
            Js::OpCode::BrLt_A,
            labelNoMatch,
            instr);

        // ...or the DWORD doesn't match the pattern...
        IR::RegOpnd *opndBuffer = IR::RegOpnd::New(TyMachReg, m_func);
        LowererMD::CreateAssign(
            opndBuffer,
            IR::IndirOpnd::New(opndString->AsRegOpnd(), offsetof(Js::JavascriptString, m_pszValue), TyMachPtr, m_func),
            instr);

        IR::LabelInstr *labelGotString = IR::LabelInstr::New(Js::OpCode::Label, m_func);

        InsertTestBranch(opndBuffer, opndBuffer, Js::OpCode::BrNeq_A, labelGotString, instr);

        m_lowererMD.LoadHelperArgument(instr, opndString);
        IR::Instr *instrCall = IR::Instr::New(Js::OpCode::Call, opndBuffer, IR::HelperCallOpnd::New(IR::HelperString_GetSz, m_func), m_func);
        instr->InsertBefore(instrCall);
        m_lowererMD.LowerCall(instrCall, 0);

        instr->InsertBefore(labelGotString);

        IR::RegOpnd *opndBufferDWORD = IR::RegOpnd::New(TyUint32, m_func);
        LowererMD::CreateAssign(
            opndBufferDWORD,
            IR::IndirOpnd::New(opndBuffer, 0, TyUint32, m_func),
            instr);

        InsertCompareBranch(
            IR::IndirOpnd::New(opndProgram, (int32)(UnifiedRegex::Program::GetOffsetOfRep() + UnifiedRegex::Program::GetOffsetOfBOILiteral2Literal()), TyUint32, m_func),
            opndBufferDWORD,
            Js::OpCode::BrEq_A,
            labelFastHelper,
            instr);

        // ...then set the last index to 0...
        instr->InsertBefore(labelNoMatch);

        LowererMD::CreateAssign(
            IR::IndirOpnd::New(opndRegex->AsRegOpnd(), Js::JavascriptRegExp::GetOffsetOfLastIndexVar(), TyVar, m_func),
            IR::AddrOpnd::NewNull(m_func),
            instr);

        LowererMD::CreateAssign(
            IR::IndirOpnd::New(opndRegex->AsRegOpnd(), Js::JavascriptRegExp::GetOffsetOfLastIndexOrFlag(), TyUint32, m_func),
            IR::IntConstOpnd::New(0, TyUint32, m_func),
            instr);

        // ...and set the dst to null...
        if (callDst)
        {
            LowererMD::CreateAssign(
                callDst,
                LoadLibraryValueOpnd(instr, LibraryValue::ValueNull),
                instr);
        }

        // ...and we're done.
        this->InsertBranch(Js::OpCode::Br, doneLabel, instr);

        instr->InsertBefore(labelFastHelper);
    }

    // [stackAllocationPointer, ]scriptcontext, regexp, string (to be pushed in reverse order)

    //string, regexp
    this->m_lowererMD.LoadHelperArgument(instr, opndString);
    this->m_lowererMD.LoadHelperArgument(instr, opndRegex);

    // script context
    LoadScriptContext(instr);

    IR::JnHelperMethod helperMethod;
    IR::AutoReuseOpnd autoReuseStackAllocationOpnd;
    if(callDst)
    {
        if(instr->dstIsTempObject)
        {
            helperMethod = IR::JnHelperMethod::HelperRegExp_ExecResultUsedAndMayBeTemp;

            // Allocate some space on the stack for the result array
            IR::RegOpnd *const stackAllocationOpnd = IR::RegOpnd::New(TyVar, m_func);
            autoReuseStackAllocationOpnd.Initialize(stackAllocationOpnd, m_func);
            stackAllocationOpnd->SetValueType(callDst->GetValueType());
            GenerateMarkTempAlloc(stackAllocationOpnd, Js::JavascriptArray::StackAllocationSize, instr);
            m_lowererMD.LoadHelperArgument(instr, stackAllocationOpnd);
        }
        else
        {
            helperMethod = IR::JnHelperMethod::HelperRegExp_ExecResultUsed;
        }
    }
    else
    {
        helperMethod = IR::JnHelperMethod::HelperRegExp_ExecResultNotUsed;
    }

    IR::Instr * helperCallInstr = IR::Instr::New(LowererMD::MDCallOpcode, instr->m_func);
    if(callDst)
    {
        helperCallInstr->SetDst(callDst);
    }
    instr->InsertBefore(helperCallInstr);
    m_lowererMD.ChangeToHelperCall(helperCallInstr, helperMethod);

    instr->InsertAfter(doneLabel);
    instr->InsertBefore(labelHelper);
    InsertBranch(Js::OpCode::Br, true, doneLabel, labelHelper);

    RelocateCallDirectToHelperPath(tmpInstr, labelHelper);
}

void
Lowerer::RelocateCallDirectToHelperPath(IR::Instr* argoutInlineSpecialized, IR::LabelInstr* labelHelper)
{
    IR::Opnd *linkOpnd = argoutInlineSpecialized->GetSrc2(); //ArgOut_A_InlineSpecialized src2; link to actual argouts.

    argoutInlineSpecialized->Unlink();
    labelHelper->InsertAfter(argoutInlineSpecialized);

    while(linkOpnd->IsSymOpnd())
    {
        IR::SymOpnd *src2 = linkOpnd->AsSymOpnd();
        StackSym *sym = src2->m_sym->AsStackSym();
        Assert(sym->m_isSingleDef);
        IR::Instr *argInstr = sym->m_instrDef;
        Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A);

        argInstr->Unlink();
        labelHelper->InsertAfter(argInstr);

        linkOpnd = argInstr->GetSrc2();
    }

    //  Move startcall
    Assert(linkOpnd->IsRegOpnd());
    StackSym *sym = linkOpnd->AsRegOpnd()->m_sym;
    Assert(sym->m_isSingleDef);
    IR::Instr *startCall = sym->m_instrDef;
    Assert(startCall->m_opcode == Js::OpCode::StartCall);
    startCall->Unlink();
    labelHelper->InsertAfter(startCall);
}

bool
Lowerer::GenerateFastInlineStringCharCodeAt(IR::Instr * instr, Js::BuiltinFunction index)
{
    Assert(instr->m_opcode == Js::OpCode::CallDirect);

    //CallDirect src2
    IR::Opnd * linkOpnd = instr->GetSrc2();
    //ArgOut_A_InlineSpecialized
    IR::Instr * tmpInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;

    IR::Opnd * argsOpnd[2] = {0};
    bool result  = instr->FetchOperands(argsOpnd, 2);
    Assert(result);

    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr->InsertAfter(doneLabel);

    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    bool success = this->m_lowererMD.GenerateFastCharAt(index, instr->GetDst(), argsOpnd[0], argsOpnd[1],
            instr, instr, labelHelper, doneLabel);

    instr->InsertBefore(labelHelper);
    if (!success)
    {
        return false;
    }

    InsertBranch(Js::OpCode::Br, true, doneLabel, labelHelper);

    RelocateCallDirectToHelperPath(tmpInstr, labelHelper);

    return true;
}

void
Lowerer::GenerateFastInlineMathClz32(IR::Instr* instr)
{
    Assert(instr->GetDst()->IsInt32());
    Assert(instr->GetSrc1()->IsInt32());
    m_lowererMD.GenerateClz(instr);
}

void
Lowerer::GenerateFastInlineMathImul(IR::Instr* instr)
{
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* src2 = instr->GetSrc2();
    IR::Opnd* dst = instr->GetDst();

    Assert(dst->IsInt32());
    Assert(src1->IsInt32());
    Assert(src2->IsInt32());

    IR::Instr* imul = IR::Instr::New(LowererMD::MDImulOpcode, dst, src1, src2, instr->m_func);
    instr->InsertBefore(imul);

    LowererMD::Legalize(imul);

    instr->Remove();
}

void
Lowerer::GenerateFastInlineMathFround(IR::Instr* instr)
{
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* dst = instr->GetDst();

    Assert(dst->IsFloat());
    Assert(src1->IsFloat());

    IR::Instr* fcvt64to32 = IR::Instr::New(LowererMD::MDConvertFloat64ToFloat32Opcode, dst, src1, instr->m_func);

    instr->InsertBefore(fcvt64to32);
    LowererMD::Legalize(fcvt64to32);

    if (dst->IsFloat64())
    {
        IR::Instr* fcvt32to64 = IR::Instr::New(LowererMD::MDConvertFloat32ToFloat64Opcode, dst, dst, instr->m_func);
        instr->InsertBefore(fcvt32to64);
        LowererMD::Legalize(fcvt32to64);
    }

    instr->Remove();
    return;
}

bool
Lowerer::GenerateFastInlineStringReplace(IR::Instr * instr)
{
    Assert(instr->m_opcode == Js::OpCode::CallDirect);

    //CallDirect src2
    IR::Opnd * linkOpnd = instr->GetSrc2();
    //ArgOut_A_InlineSpecialized
    IR::Instr * tmpInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_instrDef;

    IR::Opnd * argsOpnd[3] = {0};
    bool result  = instr->FetchOperands(argsOpnd, 3);
    Assert(result);
    AnalysisAssert(argsOpnd[0] && argsOpnd[1] && argsOpnd[2]);

    if (!argsOpnd[0]->GetValueType().IsLikelyString()
        || argsOpnd[1]->GetValueType().IsNotObject()
        || !argsOpnd[2]->GetValueType().IsLikelyString())
    {
        return false;
    }

    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr->InsertAfter(doneLabel);

    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    bool success = this->GenerateFastReplace(argsOpnd[0], argsOpnd[1], argsOpnd[2],
            instr, instr, labelHelper, doneLabel);

    instr->InsertBefore(labelHelper);
    if (!success)
    {
        return false;
    }

    InsertBranch(Js::OpCode::Br, true, doneLabel, labelHelper);

    RelocateCallDirectToHelperPath(tmpInstr, labelHelper);

    return true;
}

#ifdef ENABLE_DOM_FAST_PATH
/*
    Lower the DOMFastPathGetter opcode
    We have inliner generated bytecode:
    (dst)helpArg1: ExtendArg_A (src1)thisObject (src2)null
    (dst)helpArg2: ExtendArg_A (src1)funcObject (src2)helpArg1
    method: DOMFastPathGetter (src1)HelperCall (src2)helpArg2

    We'll convert it to a JavascriptFunction entry method call:
    CALL Helper funcObject CallInfo(CallFlags_Value, 3) thisObj
*/
void
Lowerer::LowerFastInlineDOMFastPathGetter(IR::Instr* instr)
{
    IR::Opnd* helperOpnd = instr->UnlinkSrc1();
    Assert(helperOpnd->IsHelperCallOpnd());

    IR::Opnd *linkOpnd = instr->UnlinkSrc2();
    Assert(linkOpnd->IsRegOpnd());

    IR::Instr* prevInstr = linkOpnd->AsRegOpnd()->m_sym->m_instrDef;
    Assert(prevInstr->m_opcode == Js::OpCode::ExtendArg_A);
    IR::Opnd* funcObj = prevInstr->GetSrc1();

    Assert(funcObj->IsRegOpnd());
    // If the Extended_arg was CSE's across a loop or hoisted out of a loop,
    // adding a new reference down here might cause funcObj to now be liveOnBackEdge.
    // Use the addToLiveOnBackEdgeSyms bit vector to add it to a loop if we encounter one.
    // We'll clear it once we reach the Extended arg.
    this->addToLiveOnBackEdgeSyms->Set(funcObj->AsRegOpnd()->m_sym->m_id);

    Assert(prevInstr->GetSrc2() != nullptr);
    prevInstr = prevInstr->GetSrc2()->AsRegOpnd()->m_sym->m_instrDef;
    Assert(prevInstr->m_opcode == Js::OpCode::ExtendArg_A);
    IR::Opnd* thisObj = prevInstr->GetSrc1();
    Assert(prevInstr->GetSrc2() == nullptr);

    Assert(thisObj->IsRegOpnd());
    this->addToLiveOnBackEdgeSyms->Set(thisObj->AsRegOpnd()->m_sym->m_id);

    const auto info = Lowerer::MakeCallInfoConst(Js::CallFlags_Value, 1, m_func);

    m_lowererMD.LoadHelperArgument(instr, thisObj);
    m_lowererMD.LoadHelperArgument(instr, info);
    m_lowererMD.LoadHelperArgument(instr, funcObj);

    instr->m_opcode = Js::OpCode::Call;

    IR::HelperCallOpnd *helperCallOpnd = Lowerer::CreateHelperCallOpnd(helperOpnd->AsHelperCallOpnd()->m_fnHelper, 3, m_func);
    instr->SetSrc1(helperCallOpnd);

    m_lowererMD.LowerCall(instr, 3);  // we have funcobj, callInfo, and this.
}
#endif

void
Lowerer::GenerateFastInlineArrayPush(IR::Instr * instr)
{
    Assert(instr->m_opcode == Js::OpCode::InlineArrayPush);

    IR::Opnd * baseOpnd = instr->GetSrc1();
    IR::Opnd * srcOpnd = instr->GetSrc2();

    bool returnLength = false;
    if(instr->GetDst())
    {
        returnLength = true;
    }

    IR::LabelInstr * bailOutLabelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr->InsertAfter(doneLabel);

    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    //Don't Generate fast path according to ShouldGenerateArrayFastPath()
    //AND, Don't Generate fast path if the array is LikelyNative and the element is not specialized
    if(ShouldGenerateArrayFastPath(baseOpnd, false, false, false) &&
        !(baseOpnd->GetValueType().IsLikelyNativeArray() && srcOpnd->IsVar()))
    {
        GenerateFastPush(baseOpnd, srcOpnd, instr, instr, labelHelper, doneLabel, bailOutLabelHelper, returnLength);
        instr->InsertBefore(labelHelper);
        InsertBranch(Js::OpCode::Br, true, doneLabel, labelHelper);
    }

    if(baseOpnd->GetValueType().IsLikelyNativeArray())
    {
        //Lower IR::BailOutConventionalNativeArrayAccessOnly here.
        LowerOneBailOutKind(instr, IR::BailOutConventionalNativeArrayAccessOnly, false, false);
        instr->InsertAfter(bailOutLabelHelper);
        InsertBranch(Js::OpCode::Br, doneLabel, bailOutLabelHelper);
    }

    GenerateHelperToArrayPushFastPath(instr, bailOutLabelHelper);

}

bool Lowerer::GenerateFastPop(IR::Opnd *baseOpndParam, IR::Instr *callInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel, IR::LabelInstr * bailOutLabelHelper)
{
    Assert(ShouldGenerateArrayFastPath(baseOpndParam, false, false, false));

    //     TEST baseOpnd, AtomTag                  -- check baseOpnd not tagged int
    //     JNE $helper
    //     CMP [baseOpnd], JavascriptArray::`vtable' -- check baseOpnd isArray
    //     JNE $helper
    //     MOV r2, [baseOpnd + offset(length)]     -- Load array length

    IR::RegOpnd *       baseOpnd = baseOpndParam->AsRegOpnd();
    const IR::AutoReuseOpnd autoReuseBaseOpnd(baseOpnd, m_func);

    ValueType arrValueType(baseOpndParam->GetValueType());
    IR::RegOpnd *arrayOpnd = baseOpnd;
    IR::RegOpnd *arrayLengthOpnd = nullptr;
    IR::AutoReuseOpnd autoReuseArrayLengthOpnd;
    if(!arrValueType.IsAnyOptimizedArray())
    {
        arrayOpnd = GenerateArrayTest(baseOpnd, bailOutLabelHelper, bailOutLabelHelper, callInstr, false, true);
        arrValueType = arrayOpnd->GetValueType().ToDefiniteObject().SetHasNoMissingValues(false);
    }
    else if(arrayOpnd->IsArrayRegOpnd())
    {
        IR::ArrayRegOpnd *const arrayRegOpnd = arrayOpnd->AsArrayRegOpnd();
        if(arrayRegOpnd->LengthSym())
        {
            arrayLengthOpnd = IR::RegOpnd::New(arrayRegOpnd->LengthSym(), arrayRegOpnd->LengthSym()->GetType(), m_func);
            DebugOnly(arrayLengthOpnd->FreezeSymValue());
            autoReuseArrayLengthOpnd.Initialize(arrayLengthOpnd, m_func);
        }
    }
    const IR::AutoReuseOpnd autoReuseArrayOpnd(arrayOpnd, m_func);

    IR::AutoReuseOpnd autoReuseMutableArrayLengthOpnd;
    {
        IR::RegOpnd *const mutableArrayLengthOpnd = IR::RegOpnd::New(TyUint32, m_func);
        autoReuseMutableArrayLengthOpnd.Initialize(mutableArrayLengthOpnd, m_func);
        if(arrayLengthOpnd)
        {
            // mov mutableArrayLength, arrayLength
            InsertMove(mutableArrayLengthOpnd, arrayLengthOpnd, callInstr);
        }
        else
        {
            // MOV mutableArrayLength, [array + offset(length)]     -- Load array length
            // We know this index is safe since, so mark it as UInt32 to avoid unnecessary conversion/checks
            InsertMove(
                mutableArrayLengthOpnd,
                IR::IndirOpnd::New(
                    arrayOpnd,
                    Js::JavascriptArray::GetOffsetOfLength(),
                    mutableArrayLengthOpnd->GetType(),
                    this->m_func),
                callInstr);
        }
        arrayLengthOpnd = mutableArrayLengthOpnd;
    }

    InsertCompareBranch(arrayLengthOpnd, IR::IntConstOpnd::New(0, TyUint32, this->m_func), Js::OpCode::BrEq_A, true, bailOutLabelHelper, callInstr);
    InsertSub(false, arrayLengthOpnd, arrayLengthOpnd, IR::IntConstOpnd::New(1, TyUint32, this->m_func),callInstr);

    IR::IndirOpnd *arrayRef = IR::IndirOpnd::New(arrayOpnd, arrayLengthOpnd, TyVar, this->m_func);
    arrayRef->GetBaseOpnd()->SetValueType(arrValueType);

    //Array length is going to overflow, hence don't check for Array.length and Segment.length overflow.
    bool isTypedArrayElement, isStringIndex;
    IR::IndirOpnd *const indirOpnd =
        GenerateFastElemICommon(
            callInstr,
            false,
            arrayRef,
            labelHelper,
            labelHelper,
            nullptr,
            &isTypedArrayElement,
            &isStringIndex,
            nullptr,
            nullptr /*pLabelSegmentLengthIncreased*/,
            true /*checkArrayLengthOverflow*/,
            true /* forceGenerateFastPath */,
            false/* = returnLength */,
            bailOutLabelHelper /* = bailOutLabelInstr*/);
    Assert(!isTypedArrayElement);
    Assert(indirOpnd);
    return true;
}

bool Lowerer::GenerateFastPush(IR::Opnd *baseOpndParam, IR::Opnd *src, IR::Instr *callInstr,
    IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel, IR::LabelInstr * bailOutLabelHelper, bool returnLength)
{
    Assert(ShouldGenerateArrayFastPath(baseOpndParam, false, false, false));

    //     TEST baseOpnd, AtomTag                  -- check baseOpnd not tagged int
    //     JNE $helper
    //     CMP [baseOpnd], JavascriptArray::`vtable' -- check baseOpnd isArray
    //     JNE $helper
    //     MOV r2, [baseOpnd + offset(length)]     -- Load array length

    IR::RegOpnd *       baseOpnd = baseOpndParam->AsRegOpnd();
    const IR::AutoReuseOpnd autoReuseBaseOpnd(baseOpnd, m_func);

    ValueType arrValueType(baseOpndParam->GetValueType());
    IR::RegOpnd *arrayOpnd = baseOpnd;
    IR::RegOpnd *arrayLengthOpnd = nullptr;
    IR::AutoReuseOpnd autoReuseArrayLengthOpnd;
    if(!arrValueType.IsAnyOptimizedArray())
    {
        arrayOpnd = GenerateArrayTest(baseOpnd, labelHelper, labelHelper, insertInstr, false, true);
        arrValueType = arrayOpnd->GetValueType().ToDefiniteObject().SetHasNoMissingValues(false);
    }
    else if(arrayOpnd->IsArrayRegOpnd())
    {
        IR::ArrayRegOpnd *const arrayRegOpnd = arrayOpnd->AsArrayRegOpnd();
        if(arrayRegOpnd->LengthSym())
        {
            arrayLengthOpnd = IR::RegOpnd::New(arrayRegOpnd->LengthSym(), arrayRegOpnd->LengthSym()->GetType(), m_func);
            DebugOnly(arrayLengthOpnd->FreezeSymValue());
            autoReuseArrayLengthOpnd.Initialize(arrayLengthOpnd, m_func);
        }
    }
    const IR::AutoReuseOpnd autoReuseArrayOpnd(arrayOpnd, m_func);

    if(!arrayLengthOpnd)
    {
        //     MOV arrayLength, [array + offset(length)]     -- Load array length
        // We know this index is safe since, so mark it as UInt32 to avoid unnecessary conversion/checks
        arrayLengthOpnd = IR::RegOpnd::New(TyUint32, m_func);
        autoReuseArrayLengthOpnd.Initialize(arrayLengthOpnd, m_func);
        InsertMove(
            arrayLengthOpnd,
            IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfLength(), arrayLengthOpnd->GetType(), this->m_func),
            insertInstr);
    }

    IR::IndirOpnd *arrayRef = IR::IndirOpnd::New(arrayOpnd, arrayLengthOpnd, TyVar, this->m_func);
    arrayRef->GetBaseOpnd()->SetValueType(arrValueType);

    if (returnLength && src->IsEqual(insertInstr->GetDst()))
    {
        //If the dst is same as the src, then dst is going to be overridden by GenerateFastElemICommon in process of updating the length.
        //Save it in a temp register.
        IR::RegOpnd *opnd = IR::RegOpnd::New(src->GetType(), this->m_func);
        InsertMove(opnd, src, insertInstr);
        src = opnd;
    }

    //Array length is going to overflow, hence don't check for Array.length and Segment.length overflow.
    bool isTypedArrayElement, isStringIndex;
    IR::IndirOpnd *const indirOpnd =
        GenerateFastElemICommon(
            insertInstr,
            true,
            arrayRef,
            labelHelper,
            labelHelper,
            nullptr,
            &isTypedArrayElement,
            &isStringIndex,
            nullptr,
            nullptr /*pLabelSegmentLengthIncreased*/,
            false /*checkArrayLengthOverflow*/,
            true /* forceGenerateFastPath */,
            returnLength,
            bailOutLabelHelper);

    Assert(!isTypedArrayElement);
    Assert(indirOpnd);

    //  MOV [r3 + r2], src
    InsertMoveWithBarrier(indirOpnd, src, insertInstr);

    return true;
}

IR::Opnd*
Lowerer::GenerateArgOutForInlineeStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr)
{
    Assert(callInstr->m_func->IsInlinee());
    Func *func = callInstr->m_func;
    uint32 actualCount = func->actualCount - 1; // don't count this pointer
    Assert(actualCount < Js::InlineeCallInfo::MaxInlineeArgoutCount);

    const auto firstRealArgStackSym = func->GetInlineeArgvSlotOpnd()->m_sym->AsStackSym();
    this->m_func->SetArgOffset(firstRealArgStackSym, firstRealArgStackSym->m_offset + MachPtr); //Start after this pointer
    IR::SymOpnd *firstArg = IR::SymOpnd::New(firstRealArgStackSym, TyMachPtr, func);
    const IR::AutoReuseOpnd autoReuseFirstArg(firstArg, func);
    IR::RegOpnd* argInOpnd = IR::RegOpnd::New(TyMachReg, func);
    const IR::AutoReuseOpnd autoReuseArgInOpnd(argInOpnd, func);
    InsertLea(argInOpnd, firstArg, callInstr);

    IR::IndirOpnd *argIndirOpnd = nullptr;
    IR::Instr* argout = nullptr;

#if defined(_M_IX86)
    // Maintain alignment
    if ((actualCount & 1) == 0)
    {
        IR::Instr *alignPush = IR::Instr::New(Js::OpCode::PUSH, this->m_func);
        alignPush->SetSrc1(IR::IntConstOpnd::New(1, TyInt32, this->m_func));
        callInstr->InsertBefore(alignPush);
    }
#endif

    for(uint i = actualCount; i > 0; i--)
    {
        argIndirOpnd = IR::IndirOpnd::New(argInOpnd, (i - 1) * MachPtr, TyMachReg, func);
        argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, func);
        argout->SetSrc1(argIndirOpnd);
        callInstr->InsertBefore(argout);
        // i represents ith arguments from actuals, with is i + 3 counting this, callInfo and function object
        this->m_lowererMD.LoadDynamicArgument(argout, i + 3);
    }
    return IR::IntConstOpnd::New(func->actualCount, TyInt32, func);
}

// For AMD64 and ARM only.
void
Lowerer::LowerInlineSpreadArgOutLoopUsingRegisters(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd)
{
    Func *const func = callInstr->m_func;

    IR::LabelInstr *oneArgLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertCompareBranch(indexOpnd, IR::IntConstOpnd::New(1, TyUint8, func), Js::OpCode::BrEq_A, true, oneArgLabel, callInstr);

    IR::LabelInstr *startLoopLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    startLoopLabel->m_isLoopTop = true;
    Loop *loop = JitAnew(func->m_alloc, Loop, func->m_alloc, this->m_func);
    startLoopLabel->SetLoop(loop);
    loop->SetLoopTopInstr(startLoopLabel);
    loop->regAlloc.liveOnBackEdgeSyms = AllocatorNew(JitArenaAllocator, func->m_alloc, BVSparse<JitArenaAllocator>, func->m_alloc);
    loop->regAlloc.liveOnBackEdgeSyms->Set(indexOpnd->m_sym->m_id);
    loop->regAlloc.liveOnBackEdgeSyms->Set(arrayElementsStartOpnd->m_sym->m_id);
    callInstr->InsertBefore(startLoopLabel);

    InsertSub(false, indexOpnd, indexOpnd, IR::IntConstOpnd::New(1, TyInt8, func), callInstr);

    IR::IndirOpnd *elemPtrOpnd = IR::IndirOpnd::New(arrayElementsStartOpnd, indexOpnd, this->m_lowererMD.GetDefaultIndirScale(), TyMachPtr, func);

    // Generate argout for n+2 arg (skipping function object + this)
    IR::Instr *argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, func);

    // X64 requires a reg opnd
    IR::RegOpnd *elemRegOpnd = IR::RegOpnd::New(TyMachPtr, func);
    LowererMD::CreateAssign(elemRegOpnd, elemPtrOpnd, callInstr);
    argout->SetSrc1(elemRegOpnd);
    argout->SetSrc2(indexOpnd);
    callInstr->InsertBefore(argout);
    this->m_lowererMD.LoadDynamicArgumentUsingLength(argout);

    InsertCompareBranch(indexOpnd, IR::IntConstOpnd::New(1, TyUint8, func), Js::OpCode::BrNeq_A, true, startLoopLabel, callInstr);

    // Emit final argument into register 4 on AMD64 and ARM
    callInstr->InsertBefore(oneArgLabel);
    argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, func);
    argout->SetSrc1(elemPtrOpnd);
    callInstr->InsertBefore(argout);
    this->m_lowererMD.LoadDynamicArgument(argout, 4); //4 to denote this is 4th register after this, callinfo & function object
}

IR::Instr *
Lowerer::LowerCallIDynamicSpread(IR::Instr *callInstr, ushort callFlags)
{
    Assert(callInstr->m_opcode == Js::OpCode::CallIDynamicSpread);

    IR::Instr * insertBeforeInstrForCFG = nullptr;

    Func *const func = callInstr->m_func;

    if (func->IsInlinee())
    {
        throw Js::RejitException(RejitReason::InlineSpreadDisabled);
    }

    IR::Instr *spreadArrayInstr = callInstr;
    IR::SymOpnd    *argLinkOpnd = spreadArrayInstr->UnlinkSrc2()->AsSymOpnd();
    StackSym       *argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    AssertMsg(argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
    argLinkOpnd->Free(this->m_func);
    spreadArrayInstr = argLinkSym->m_instrDef;

    Assert(spreadArrayInstr->m_opcode == Js::OpCode::ArgOut_A_SpreadArg);

    IR::RegOpnd *arrayOpnd = nullptr;
    IR::Opnd *arraySrcOpnd = spreadArrayInstr->UnlinkSrc1();
    if (!arraySrcOpnd->IsRegOpnd())
    {
        arrayOpnd = IR::RegOpnd::New(TyMachPtr, func);
        LowererMD::CreateAssign(arrayOpnd, arraySrcOpnd, spreadArrayInstr);
    }
    else
    {
        arrayOpnd = arraySrcOpnd->AsRegOpnd();
    }

    argLinkOpnd = spreadArrayInstr->UnlinkSrc2()->AsSymOpnd();

    // Walk the arg chain and find the start call
    argLinkSym = argLinkOpnd->m_sym->AsStackSym();
    AssertMsg(argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
    argLinkOpnd->Free(this->m_func);

    // Nothing to be done for the function object, emit as normal
    IR::Instr *thisInstr = argLinkSym->m_instrDef;
    IR::RegOpnd *thisOpnd = thisInstr->UnlinkSrc2()->AsRegOpnd();
    argLinkSym = thisOpnd->m_sym->AsStackSym();
    thisInstr->Unlink();
    thisInstr->FreeDst();

    // Remove the array ArgOut instr and StartCall, they are no longer needed
    spreadArrayInstr->Unlink();
    spreadArrayInstr->FreeDst();
    IR::Instr *startCallInstr = argLinkSym->m_instrDef;
    Assert(startCallInstr->m_opcode == Js::OpCode::StartCall);
    insertBeforeInstrForCFG = startCallInstr->GetNextRealInstr();
    startCallInstr->Remove();

    IR::RegOpnd *argsLengthOpnd = IR::RegOpnd::New(TyUint32, func);
    IR::IndirOpnd *arrayLengthPtrOpnd = IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfLength(), TyUint32, func);
    LowererMD::CreateAssign(argsLengthOpnd, arrayLengthPtrOpnd, callInstr);

    // Don't bother expanding args if there are zero
    IR::LabelInstr *zeroArgsLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    InsertCompareBranch(argsLengthOpnd, IR::IntConstOpnd::New(0, TyInt8, func), Js::OpCode::BrEq_A, true, zeroArgsLabel, callInstr);

    IR::RegOpnd *indexOpnd = IR::RegOpnd::New(TyUint32, func);
    LowererMD::CreateAssign(indexOpnd, argsLengthOpnd, callInstr);

    // Get the array head offset and length
    IR::IndirOpnd *arrayHeadPtrOpnd = IR::IndirOpnd::New(arrayOpnd, Js::JavascriptArray::GetOffsetOfHead(), TyMachPtr, func);
    IR::RegOpnd *arrayElementsStartOpnd = IR::RegOpnd::New(TyMachPtr, func);
    InsertAdd(false, arrayElementsStartOpnd, arrayHeadPtrOpnd, IR::IntConstOpnd::New(offsetof(Js::SparseArraySegment<Js::Var>, elements), TyUint8, func), callInstr);

    this->m_lowererMD.LowerInlineSpreadArgOutLoop(callInstr, indexOpnd, arrayElementsStartOpnd);

    // Resume if we have zero args
    callInstr->InsertBefore(zeroArgsLabel);

    // Lower call
    callInstr->m_opcode = Js::OpCode::CallIDynamic;
    callInstr = m_lowererMD.LowerCallIDynamic(callInstr, thisInstr, argsLengthOpnd, callFlags, insertBeforeInstrForCFG);

    return callInstr;
}

IR::Instr *
Lowerer::LowerCallIDynamic(IR::Instr * callInstr, ushort callFlags)
{
    if (!this->m_func->GetHasStackArgs())
    {
        throw Js::RejitException(RejitReason::InlineApplyDisabled);
    }

    IR::Instr * insertBeforeInstrForCFG = nullptr;

    // Lower args and look for StartCall
    IR::Instr * argInstr = callInstr;
    IR::SymOpnd *   argLinkOpnd = argInstr->UnlinkSrc2()->AsSymOpnd();
    StackSym *      argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    AssertMsg(argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
    argLinkOpnd->Free(this->m_func);
    argInstr = argLinkSym->m_instrDef;
    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A_Dynamic);

    IR::Instr* saveThisArgOutInstr = argInstr;
    saveThisArgOutInstr->Unlink();
    saveThisArgOutInstr->FreeDst();

    argLinkOpnd = argInstr->UnlinkSrc2()->AsSymOpnd();
    argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    AssertMsg(argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
    argLinkOpnd->Free(this->m_func);
    argInstr = argLinkSym->m_instrDef;
    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);

    IR::Opnd* argsLength = m_lowererMD.GenerateArgOutForStackArgs(callInstr, argInstr);

    IR::RegOpnd* startCallDstOpnd = argInstr->UnlinkSrc2()->AsRegOpnd();
    argLinkSym  = startCallDstOpnd->m_sym->AsStackSym();
    startCallDstOpnd->Free(this->m_func);
    argInstr->Remove();// Remove ArgOut_A_FromStackArgs

    argInstr = argLinkSym->m_instrDef;
    Assert(argInstr->m_opcode == Js::OpCode::StartCall);
    insertBeforeInstrForCFG = argInstr->GetNextRealInstr();
    argInstr->Remove(); //Remove start call

    return m_lowererMD.LowerCallIDynamic(callInstr, saveThisArgOutInstr, argsLength, callFlags, insertBeforeInstrForCFG);
}

//This is only for x64 & ARM.
IR::Opnd*
Lowerer::GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr)
{
//    s25.var       =  LdLen_A          s4.var
//    s26.var       =  Ld_A             s25.var
//                    BrNeq_I4          $L3, s25.var,0
//    $L2:
//                    BrNeq_I4          $L4, s25.var,1
//    s25.var       = SUB_I4            s25.var, 0x1
//    s10.var       = LdElemI_A         [s4.var+s25.var].var
//                    ArgOut_A_Dynamic  s10.var, s25.var
//                    Br $L2
//    $L4:
//    s10.var       = LdElemI_A         [s4.var].var
//                    ArgOut_A_Dynamic  s10.var, 4
//    $L3
#if defined(_M_IX86)
     Assert(false);
#endif

    Assert(stackArgsInstr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);
    Assert(callInstr->m_opcode == Js::OpCode::CallIDynamic);

    this->m_lowererMD.GenerateFunctionObjectTest(callInstr, callInstr->GetSrc1()->AsRegOpnd(), false);

    if (callInstr->m_func->IsInlinee())
    {
        return this->GenerateArgOutForInlineeStackArgs(callInstr, stackArgsInstr);
    }
    Func *func = callInstr->m_func;
    IR::RegOpnd* stackArgs = stackArgsInstr->GetSrc1()->AsRegOpnd();

    IR::RegOpnd* ldLenDstOpnd = IR::RegOpnd::New(TyUint32, func);
    IR::Instr* ldLen = IR::Instr::New(Js::OpCode::LdLen_A, ldLenDstOpnd ,stackArgs, func);
    ldLenDstOpnd->SetValueType(ValueType::GetTaggedInt()); /*LdLen_A works only on stack arguments*/
    callInstr->InsertBefore(ldLen);
    GenerateFastRealStackArgumentsLdLen(ldLen);

    IR::Instr* saveLenInstr = IR::Instr::New(Js::OpCode::MOV, IR::RegOpnd::New(TyUint32, func), ldLenDstOpnd, func);
    saveLenInstr->GetDst()->SetValueType(ValueType::GetTaggedInt());
    callInstr->InsertBefore(saveLenInstr);

    IR::LabelInstr* doneArgs = IR::LabelInstr::New(Js::OpCode::Label, func);
    IR::Instr* branchDoneArgs = IR::BranchInstr::New(Js::OpCode::BrEq_I4, doneArgs, ldLenDstOpnd, IR::IntConstOpnd::New(0, TyInt8, func),func);
    callInstr->InsertBefore(branchDoneArgs);
    this->m_lowererMD.EmitInt4Instr(branchDoneArgs);

    IR::LabelInstr* startLoop = IR::LabelInstr::New(Js::OpCode::Label, func);
    IR::LabelInstr* endLoop = IR::LabelInstr::New(Js::OpCode::Label, func);
    startLoop->m_isLoopTop = true;
    Loop *loop = JitAnew(func->m_alloc, Loop, func->m_alloc, this->m_func);
    startLoop->SetLoop(loop);
    loop->SetLoopTopInstr(startLoop);
    loop->regAlloc.liveOnBackEdgeSyms = AllocatorNew(JitArenaAllocator, func->m_alloc, BVSparse<JitArenaAllocator>, func->m_alloc);

    callInstr->InsertBefore(startLoop);

    IR::Instr* branchOutOfLoop = IR::BranchInstr::New(Js::OpCode::BrEq_I4, endLoop, ldLenDstOpnd, IR::IntConstOpnd::New(1, TyInt8, func),func);
    callInstr->InsertBefore(branchOutOfLoop);
    this->m_lowererMD.EmitInt4Instr(branchOutOfLoop);

    IR::Instr* subInstr = IR::Instr::New(Js::OpCode::Sub_I4, ldLenDstOpnd, ldLenDstOpnd, IR::IntConstOpnd::New(1, TyInt8, func),func);
    callInstr->InsertBefore(subInstr);
    this->m_lowererMD.EmitInt4Instr(subInstr);

    IR::IndirOpnd *nthArgument = IR::IndirOpnd::New(stackArgs, ldLenDstOpnd, TyMachReg, func);
    IR::RegOpnd* ldElemDstOpnd = IR::RegOpnd::New(TyMachReg,func);
    IR::Instr* ldElem = IR::Instr::New(Js::OpCode::LdElemI_A, ldElemDstOpnd, nthArgument, func);
    callInstr->InsertBefore(ldElem);
    GenerateFastStackArgumentsLdElemI(ldElem);

    IR::Instr* argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, func);
    argout->SetSrc1(ldElemDstOpnd);
    argout->SetSrc2(ldLenDstOpnd);
    callInstr->InsertBefore(argout);
    this->m_lowererMD.LoadDynamicArgumentUsingLength(argout);

    IR::BranchInstr *tailBranch = IR::BranchInstr::New(Js::OpCode::Br, startLoop, func);
    callInstr->InsertBefore(tailBranch);
    callInstr->InsertBefore(endLoop);
    this->m_lowererMD.LowerUncondBranch(tailBranch);

    loop->regAlloc.liveOnBackEdgeSyms->Set(ldLenDstOpnd->m_sym->m_id);

    subInstr = IR::Instr::New(Js::OpCode::Sub_I4, ldLenDstOpnd, ldLenDstOpnd, IR::IntConstOpnd::New(1, TyInt8, func),func);
    callInstr->InsertBefore(subInstr);
    this->m_lowererMD.EmitInt4Instr(subInstr);

    nthArgument = IR::IndirOpnd::New(stackArgs, ldLenDstOpnd, TyMachReg, func);
    ldElemDstOpnd = IR::RegOpnd::New(TyMachReg,func);
    ldElem = IR::Instr::New(Js::OpCode::LdElemI_A, ldElemDstOpnd, nthArgument, func);
    callInstr->InsertBefore(ldElem);
    GenerateFastStackArgumentsLdElemI(ldElem);

    argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, func);
    argout->SetSrc1(ldElemDstOpnd);
    callInstr->InsertBefore(argout);
    this->m_lowererMD.LoadDynamicArgument(argout, 4); //4 to denote this is 4th register after this, callinfo & function object

    callInstr->InsertBefore(doneArgs);

    /*return the length which will be used for callInfo generations & stack allocation*/
    return saveLenInstr->GetDst()->AsRegOpnd();
}

//This function assumes there is stackargs bailout and index is always on the range.
bool
Lowerer::GenerateFastStackArgumentsLdElemI(IR::Instr* ldElem)
{
    //  MOV dst, ebp [(valueOpnd + 5) *4]  // 5 for the stack layout
    //

    IR::IndirOpnd *indirOpnd = ldElem->GetSrc1()->AsIndirOpnd();
    // Now load the index and check if it is an integer.
    IR::RegOpnd   *indexOpnd = indirOpnd->GetIndexOpnd();
    Assert (indexOpnd && indexOpnd->IsTaggedInt());

    if(ldElem->m_func->IsInlinee())
    {
        IR::IndirOpnd *argIndirOpnd = GetArgsIndirOpndForInlinee(ldElem, indexOpnd);

        LowererMD::CreateAssign(ldElem->GetDst(), argIndirOpnd, ldElem);
    }
    else
    {
        // Load argument set dst = [ebp + index].
        IR::RegOpnd *ebpOpnd = IR::Opnd::CreateFramePointerOpnd(m_func);
        IR::IndirOpnd *argIndirOpnd = nullptr;

        // The stack looks like this:
        //       ...
        //       arguments[1]
        //       arguments[0]
        //       this
        //       callinfo
        //       function object
        //       return addr
        // EBP-> EBP chain

        //actual arguments offset is LowererMD::GetFormalParamOffset() + 1 (this)

        int32 actualOffset = GetFormalParamOffset() + 1 + indirOpnd->GetOffset();
        Assert(GetFormalParamOffset() + 1 == 5);
        const BYTE indirScale = this->m_lowererMD.GetDefaultIndirScale();
        argIndirOpnd = IR::IndirOpnd::New(ebpOpnd->AsRegOpnd(), indexOpnd->AsRegOpnd(), indirScale, TyMachReg, this->m_func);

        // Need to offset valueOpnd by 5. Instead of changing valueOpnd, we can just add an offset to the indir. Changing
        // valueOpnd requires creation of a temp sym (if it's not already a temp) so that the value of the sym that
        // valueOpnd represents is not changed.
        argIndirOpnd->SetOffset(actualOffset << indirScale);
        LowererMD::CreateAssign(ldElem->GetDst(), argIndirOpnd, ldElem);
    }
    ldElem->Remove();
    return false;
}

IR::IndirOpnd*
Lowerer::GetArgsIndirOpndForInlinee(IR::Instr* ldElem, IR::Opnd* valueOpnd)
{
    Assert(ldElem->m_func->IsInlinee());
    IR::IndirOpnd* argIndirOpnd = nullptr;

    // Address of argument after 'this'
    const auto firstRealArgStackSym = ldElem->m_func->GetInlineeArgvSlotOpnd()->m_sym->AsStackSym();
    this->m_func->SetArgOffset(firstRealArgStackSym, firstRealArgStackSym->m_offset + MachPtr); //Start after this pointer
    IR::SymOpnd *firstArg = IR::SymOpnd::New(firstRealArgStackSym, TyMachPtr, ldElem->m_func);
    const IR::AutoReuseOpnd autoReuseFirstArg(firstArg, m_func);

    IR::RegOpnd *const baseOpnd = IR::RegOpnd::New(TyMachReg, ldElem->m_func);
    const IR::AutoReuseOpnd autoReuseBaseOpnd(baseOpnd, m_func);
    InsertLea(baseOpnd, firstArg, ldElem);

    if (valueOpnd->IsIntConstOpnd())
    {
        IntConstType offset = valueOpnd->AsIntConstOpnd()->GetValue() * MachPtr;
        // TODO: Assert(Math::FitsInDWord(offset));
        argIndirOpnd = IR::IndirOpnd::New(baseOpnd, (int32)offset, TyMachReg, ldElem->m_func);
    }
    else
    {
        Assert(valueOpnd->IsRegOpnd());
        const BYTE indirScale = this->m_lowererMD.GetDefaultIndirScale();
        argIndirOpnd = IR::IndirOpnd::New(baseOpnd, valueOpnd->AsRegOpnd(), indirScale, TyMachReg, ldElem->m_func);
    }
    return argIndirOpnd;
}

IR::IndirOpnd*
Lowerer::GetArgsIndirOpndForTopFunction(IR::Instr* ldElem, IR::Opnd* valueOpnd)
{
    // Load argument set dst = [ebp + index] (or grab from the generator object if m_func is a generator function).
    IR::RegOpnd *baseOpnd = m_func->GetJnFunction()->IsGenerator() ? LoadGeneratorArgsPtr(ldElem) : IR::Opnd::CreateFramePointerOpnd(m_func);
    IR::IndirOpnd* argIndirOpnd = nullptr;
    // The stack looks like this:
    //       ...
    //       arguments[1]
    //       arguments[0]
    //       this
    //       callinfo
    //       function object
    //       return addr
    // EBP-> EBP chain

    //actual arguments offset is LowererMD::GetFormalParamOffset() + 1 (this)

    uint16 actualOffset = m_func->GetJnFunction()->IsGenerator() ? 1 : GetFormalParamOffset() + 1; //5
    Assert(actualOffset == 5 || m_func->GetJnFunction()->IsGenerator());
    if (valueOpnd->IsIntConstOpnd())
    {
        IntConstType offset = (valueOpnd->AsIntConstOpnd()->GetValue() + actualOffset) * MachPtr;
        // TODO: Assert(Math::FitsInDWord(offset));
        argIndirOpnd = IR::IndirOpnd::New(baseOpnd, (int32)offset, TyMachReg, this->m_func);
    }
    else
    {
        const BYTE indirScale = this->m_lowererMD.GetDefaultIndirScale();
        argIndirOpnd = IR::IndirOpnd::New(baseOpnd->AsRegOpnd(), valueOpnd->AsRegOpnd(), indirScale, TyMachReg, this->m_func);

        // Need to offset valueOpnd by 5. Instead of changing valueOpnd, we can just add an offset to the indir. Changing
        // valueOpnd requires creation of a temp sym (if it's not already a temp) so that the value of the sym that
        // valueOpnd represents is not changed.
        argIndirOpnd->SetOffset(actualOffset << indirScale);
    }
    return argIndirOpnd;
}

void
Lowerer::GenerateCheckForArgumentsLength(IR::Instr* ldElem, IR::LabelInstr* labelCreateHeapArgs, IR::Opnd* actualParamOpnd, IR::Opnd* valueOpnd, Js::OpCode opcode)
{
    // Check if index < nr_actuals.
    InsertCompare(actualParamOpnd, valueOpnd, ldElem);
    // Jump to helper if index >= nr_actuals.
    // Do an unsigned check here so that a negative index will also fail.
    // (GenerateLdValueFromCheckedIndexOpnd does not guarantee positive index on x86.)
    InsertBranch(opcode, true, labelCreateHeapArgs, ldElem);
}

bool
Lowerer::GenerateFastArgumentsLdElemI(IR::Instr* ldElem, IR::LabelInstr * labelHelper, IR::LabelInstr *labelFallThru)
{
    //  TEST argsSlot, argsSlot
    //  JNE $helper  // There is an arguments object created jump to helper.
    //  ---GenerateSmIntTest
    //  ---GenerateLdValueFromCheckedIndexOpnd
    //  ---LoadInputParamCount
    //  CMP actualParamOpnd, valueOpnd //Compare between the actual count & the index count (say i in arguments[i])
    //  JLE $labelCreateHeapArgs
    //  MOV dst, ebp [(valueOpnd + 5) *4]  // 5 for the stack layout
    //  JMP $fallthrough
    //
    //labelCreateHeapArgs:
    //  ---LoadHeapArguments

    Assert(ldElem->DoStackArgsOpt(this->m_func));

    IR::IndirOpnd *indirOpnd = ldElem->GetSrc1()->AsIndirOpnd();
    bool isInlinee = ldElem->m_func->IsInlinee();
    Func *func = ldElem->m_func;


    // First check the slot on the frame to see if there is a heap arguments object.
    IR::Opnd       *cachedArgsObjectSlotOpnd = isInlinee? ldElem->m_func->GetInlineeArgumentsObjectSlotOpnd() : this->m_lowererMD.CreateStackArgumentsSlotOpnd() ;
    // Re-use the base pointer here so that we're loading the current heap args into the reg we will pass
    // to the helper if necessary.
    IR::RegOpnd    *argsObjRegOpnd           = indirOpnd->GetBaseOpnd();
    LowererMD::CreateAssign(argsObjRegOpnd, cachedArgsObjectSlotOpnd, ldElem);

    InsertTest(argsObjRegOpnd, argsObjRegOpnd, ldElem);

    IR::LabelInstr *labelCreateHeapArgs = IR::LabelInstr::New(Js::OpCode::Label, func, true);

    InsertBranch(Js::OpCode::BrNeq_A, labelHelper, ldElem);

    // Now load the index and check if it is an integer.
    bool emittedFastPath = false;
    bool isNotInt = false;
    IntConstType   value     = 0;
    IR::RegOpnd   *indexOpnd = indirOpnd->GetIndexOpnd();
    IR::Opnd      *valueOpnd        = nullptr;
    IR::Opnd      *actualParamOpnd = nullptr;

    bool hasIntConstIndex = indirOpnd->TryGetIntConstIndexValue(true, &value, &isNotInt);

    if (isInlinee && hasIntConstIndex && value >= (ldElem->m_func->actualCount - 1))
    {
        //Outside the range of actuals, skip
    }
    else if (labelFallThru != nullptr && !(hasIntConstIndex && value < 0)) //if index is not a negative int constant
    {
        if (isInlinee)
        {
            actualParamOpnd = IR::IntConstOpnd::New(ldElem->m_func->actualCount - 1, TyInt32, func);
        }
        else
        {
            // Load actuals count, LoadHeapArguments will reuse the generated instructions here
            IR::Instr      *loadInputParamCountInstr = this->m_lowererMD.LoadInputParamCount(ldElem, -1 /* don't include 'this' while counting actuals. */);
            actualParamOpnd = loadInputParamCountInstr->GetDst()->AsRegOpnd();
        }

        if (hasIntConstIndex)
        {
            //Constant index
            valueOpnd = IR::IntConstOpnd::New(value, TyInt32, func);
        }
        else
        {
            //Load valueOpnd from the index
            valueOpnd =
                m_lowererMD.LoadNonnegativeIndex(
                    indexOpnd,
                    (
                    #if INT32VAR
                        indexOpnd->GetType() == TyUint32
                    #else
                        // On 32-bit platforms, skip the negative check since for now, the unsigned upper bound check covers it
                        true
                    #endif
                    ),
                    labelCreateHeapArgs,
                    labelCreateHeapArgs,
                    ldElem);
        }

        if (isInlinee)
        {
            if (!hasIntConstIndex)
            {
                //Runtime check if to make sure length is within the arguments.length range.
                GenerateCheckForArgumentsLength(ldElem, labelCreateHeapArgs, valueOpnd, actualParamOpnd, Js::OpCode::BrGe_A);
            }
        }
        else
        {
            GenerateCheckForArgumentsLength(ldElem, labelCreateHeapArgs, actualParamOpnd, valueOpnd, Js::OpCode::BrLe_A);
        }

        IR::Opnd *argIndirOpnd = nullptr;
        if (isInlinee)
        {
            argIndirOpnd = GetArgsIndirOpndForInlinee(ldElem, valueOpnd);
        }
        else
        {
            argIndirOpnd = GetArgsIndirOpndForTopFunction(ldElem, valueOpnd);
        }

        LowererMD::CreateAssign(ldElem->GetDst(), argIndirOpnd, ldElem);

        // JMP $done
        InsertBranch(Js::OpCode::Br, labelFallThru, ldElem);
        // $labelCreateHeapArgs:
        ldElem->InsertBefore(labelCreateHeapArgs);
        emittedFastPath = true;
    }

    IR::Opnd *nullOpnd = this->LoadLibraryValueOpnd(ldElem, LibraryValue::ValueNull);
    IR::Instr *instrArgs = IR::Instr::New(Js::OpCode::LdHeapArguments,
        indirOpnd->GetBaseOpnd(),
        nullOpnd,
        nullOpnd,
        func);
    ldElem->InsertBefore(instrArgs);
    this->m_lowererMD.LoadHeapArguments(instrArgs, true, actualParamOpnd);

    return emittedFastPath;
}

bool
Lowerer::GenerateFastRealStackArgumentsLdLen(IR::Instr *ldLen)
{
    if(ldLen->m_func->IsInlinee())
    {
        //Get the length of the arguments
        LowererMD::CreateAssign(ldLen->GetDst(),
                               IR::IntConstOpnd::New(ldLen->m_func->actualCount - 1, TyUint32, ldLen->m_func),
                               ldLen);
    }
    else
    {
        IR::Instr *loadInputParamCountInstr = this->m_lowererMD.LoadInputParamCount(ldLen, -1);
        IR::RegOpnd *actualCountOpnd          = loadInputParamCountInstr->GetDst()->AsRegOpnd();
        LowererMD::CreateAssign(ldLen->GetDst(), actualCountOpnd, ldLen);
    }
    ldLen->Remove();
    return false;
}

bool
Lowerer::GenerateFastArgumentsLdLen(IR::Instr *ldLen, IR::LabelInstr* labelHelper, IR::LabelInstr* labelFallThru)
{
    // TEST argslot, argslot  //Test if the arguments slot is zero
    // JNE $helper
    // actualCountOpnd <-LoadInputParamCount fastpath
    // SHL actualCountOpnd, actualCountOpnd, 1 // Left shift for tagging
    // INC actualCountOpnd                     // Tagging
    // MOV dst, actualCountOpnd
    // JMP $fallthrough
    //$helper:

    Assert(ldLen->DoStackArgsOpt(this->m_func));

    if(ldLen->m_func->IsInlinee())
    {
        IR::Opnd       *cachedArgsObjectSlotOpnd = ldLen->m_func->GetInlineeArgumentsObjectSlotOpnd();
        // Re-use the LdLen_A source here so that we're loading the current heap args into the reg we will pass
        // to the helper if necessary.
        IR::RegOpnd    *argsObjectRegOpnd        = ldLen->GetSrc1()->AsRegOpnd();

        LowererMD::CreateAssign(argsObjectRegOpnd, cachedArgsObjectSlotOpnd, ldLen);
        InsertTest(argsObjectRegOpnd, argsObjectRegOpnd, ldLen);
        InsertBranch(Js::OpCode::BrNeq_A, labelHelper, ldLen);

        //Get the length of the arguments
        LowererMD::CreateAssign(ldLen->GetDst(),
                                IR::AddrOpnd::New(Js::TaggedInt::ToVarUnchecked(ldLen->m_func->actualCount - 1), IR::AddrOpndKindConstantVar, ldLen->m_func), // -1 to exclude this pointer
                                ldLen);
    }
    else
    {
        IR::Opnd       *cachedArgsObjectSlotOpnd = this->m_lowererMD.CreateStackArgumentsSlotOpnd();
        // Re-use the LdLen_A source here so that we're loading the current heap args into the reg we will pass
        // to the helper if necessary.
        IR::RegOpnd    *argsObjectRegOpnd        = ldLen->GetSrc1()->AsRegOpnd();
        LowererMD::CreateAssign(argsObjectRegOpnd, cachedArgsObjectSlotOpnd, ldLen);
        InsertTest(argsObjectRegOpnd, argsObjectRegOpnd, ldLen);
        InsertBranch(Js::OpCode::BrNeq_A, labelHelper, ldLen);


        IR::Instr      *loadInputParamCountInstr = this->m_lowererMD.LoadInputParamCount(ldLen, -1);
        IR::RegOpnd    *actualCountOpnd          = loadInputParamCountInstr->GetDst()->AsRegOpnd();

        this->m_lowererMD.GenerateInt32ToVarConversion(actualCountOpnd, ldLen);
        LowererMD::CreateAssign(ldLen->GetDst(), actualCountOpnd, ldLen);
    }
    InsertBranch(Js::OpCode::Br, labelFallThru, ldLen);
    return true;
}

IR::RegOpnd*
Lowerer::GenerateFunctionTypeFromFixedFunctionObject(IR::Instr *insertInstrPt, IR::Opnd* functionObjOpnd)
{
    IR::RegOpnd * functionTypeRegOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::Opnd *functionTypeOpnd = nullptr;

    if(functionObjOpnd->IsAddrOpnd())
    {
        IR::AddrOpnd* functionObjAddrOpnd = functionObjOpnd->AsAddrOpnd();
        // functionTypeRegOpnd = MOV [fixed function address + type offset]
        functionObjAddrOpnd->m_address;
        functionTypeOpnd = IR::MemRefOpnd::New((void *)((intptr)functionObjAddrOpnd->m_address + Js::RecyclableObject::GetOffsetOfType()), TyMachPtr, this->m_func,
            IR::AddrOpndKindDynamicObjectTypeRef);
    }
    else
    {
        functionTypeOpnd = IR::IndirOpnd::New(functionObjOpnd->AsRegOpnd(), Js::RecyclableObject::GetOffsetOfType(), TyMachPtr, this->m_func);
    }
    LowererMD::CreateAssign(functionTypeRegOpnd, functionTypeOpnd, insertInstrPt);
    return functionTypeRegOpnd;
}

void
Lowerer::FinalLower()
{
    this->m_lowererMD.FinalLower();

    // ensure that the StartLabel and EndLabel are inserted
    // before the prolog and after the epilog respectively
    IR::LabelInstr * startLabel = m_func->GetFuncStartLabel();
    if (startLabel != nullptr)
    {
        m_func->m_headInstr->InsertAfter(startLabel);
    }

    IR::LabelInstr * endLabel = m_func->GetFuncEndLabel();
    if (endLabel != nullptr)
    {
        m_func->m_tailInstr->GetPrevRealInstr()->InsertBefore(endLabel);
    }
}

void
Lowerer::EHBailoutPatchUp()
{
    Assert(this->m_func->isPostLayout);
    // 1. Insert return thunks for all the regions.
    // 2. Set the hasBailedOut bit to true on all bailout paths in EH regions.
    // 3. Insert code after every bailout in a try or catch region to save the return value on the stack, and jump to the return thunk (See Region.h) of that region.
    // 4. Insert code right before the epilog, to restore the return value (saved in 2.) from a bailout into eax.

    IR::LabelInstr * restoreReturnValueFromBailoutLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    IR::LabelInstr * epilogLabel;
    IR::Instr * exitPrevInstr = this->m_func->m_exitInstr->GetPrevRealInstrOrLabel();
    if (exitPrevInstr->IsLabelInstr())
    {
        epilogLabel = exitPrevInstr->AsLabelInstr();
    }
    else
    {
        epilogLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        this->m_func->m_exitInstr->InsertBefore(epilogLabel);
    }

    IR::Instr * tmpInstr = nullptr;
    bool restoreReturnFromBailoutEmitted = false;
    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->m_func)
    {
        if (instr->IsLabelInstr())
        {
            this->currentRegion = instr->AsLabelInstr()->GetRegion();
        }

        // Consider (radua): Assert(this->currentRegion) here?
        if (this->currentRegion)
        {
            RegionType currentRegionType = this->currentRegion->GetType();
            if (currentRegionType == RegionTypeTry || currentRegionType == RegionTypeCatch)
            {
                this->InsertReturnThunkForRegion(this->currentRegion, restoreReturnValueFromBailoutLabel);
                if (instr->HasBailOutInfo())
                {
                    this->SetHasBailedOut(instr);
                    tmpInstr = this->EmitEHBailoutStackRestore(instr);
                    this->EmitSaveEHBailoutReturnValueAndJumpToRetThunk(tmpInstr);
                    if (!restoreReturnFromBailoutEmitted)
                    {
                        this->EmitRestoreReturnValueFromEHBailout(restoreReturnValueFromBailoutLabel, epilogLabel);
                        restoreReturnFromBailoutEmitted = true;
                    }
                }
            }
        }
    }
    NEXT_INSTR_IN_FUNC_EDITING
}

bool
Lowerer::GenerateFastLdFld(IR::Instr * const instrLdFld, IR::JnHelperMethod helperMethod, IR::JnHelperMethod polymorphicHelperMethod,
    IR::LabelInstr ** labelBailOut, IR::RegOpnd* typeOpnd, bool* pIsHelper, IR::LabelInstr** pLabelHelper)
{
        // Generates:
    //
    // r1 = object->type
    // if (r1 is taggedInt) goto helper
    // Load inline cache
    //    if monomorphic
    //        r2 = address of the monomorphic inline cache
    //    if polymorphic
    //        r2 = address of the polymorphic inline cache array
    //        r3 = (type >> PIC shift amount) & (PIC size - 1)
    //        r2 = r2 + r3
    // Try load property using proto cache (if protoFirst)
    // Try load property using local cache
    // Try loading property using proto cache (if !protoFirst)
    // Try loading property using flags cache
    //
    // Loading property using local cache:
    //    if (r1 == r2->u.local.type)
    //        result = load inline slot r2->u.local.slotIndex from r1
    //        goto fallthru
    //    if ((r1 | InlineCacheAuxSlotTypeTag) == r2->u.local.type)
    //        result = load aux slot r2->u.local.slotIndex from r1
    //        goto fallthru
    //
    // Loading property using proto cache:
    //    if (r1 == r2->u.proto.type)
    //        r3 = r2->u.proto.prototypeObject
    //        result = load inline slot r2->u.proto.slotIndex from r3
    //        goto fallthru
    //    if (r1 | InlineCacheAuxSlotTypeTag) == r2.u.proto.type)
    //        r3 = r2->u.proto.prototypeObject
    //        result = load aux slot r2->u.proto.slotIndex from r3
    //        goto fallthru
    //
    // Loading property using flags cache:
    //    if (r2->u.accessor.flags & (Js::InlineCacheGetterFlag | Js::InlineCacheSetterFlag) == 0)
    //        if (r1 == r2->u.accessor.type)
    //            result = load inline slot r2->u.accessor.slotIndex from r1
    //            goto fallthru
    //        if ((r1 | InlineCacheAuxSlotTypeTag) == r2->u.accessor.type)
    //            result = load aux slot r2->u.accessor.slotIndex from r1
    //            goto fallthru
    //
    // Loading an inline slot:
    //    result = [r1 + slotIndex * sizeof(Var)]
    //
    // Loading an aux slot:
    //    slotArray = r1->auxSlots
    //    result = [slotArray + slotIndex * sizeof(Var)]
    //
    // We only emit the code block for a type of cache (local/proto/flags) if the profile data
    // indicates that type of cache was used to load the property in the past.
    // We don't emit the type check with aux slot tag if the profile data indicates that we didn't
    // load the property from an aux slot before.
    // We don't emit the type check without an aux slot tag if the profile data indicates that we didn't
    // load the property from an inline slot before.

    IR::Opnd * opndSrc = instrLdFld->GetSrc1();
    AssertMsg(opndSrc->IsSymOpnd() && opndSrc->AsSymOpnd()->IsPropertySymOpnd() && opndSrc->AsSymOpnd()->m_sym->IsPropertySym(), "Expected PropertySym as src of LdFld");

    Assert(!instrLdFld->DoStackArgsOpt(this->m_func));

    IR::PropertySymOpnd * propertySymOpnd = opndSrc->AsPropertySymOpnd();
    PropertySym * propertySym = propertySymOpnd->m_sym->AsPropertySym();

    PHASE_PRINT_TESTTRACE(
        Js::ObjTypeSpecPhase,
        this->m_func,
        L"Field load: %s, property: %s, func: %s, cache ID: %d, cloned cache: false\n",
        Js::OpCodeUtil::GetOpCodeName(instrLdFld->m_opcode),
        this->m_func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer(),
        this->m_func->GetJnFunction()->GetDisplayName(),
        propertySymOpnd->m_inlineCacheIndex);

    Assert(pIsHelper != nullptr);
    bool& isHelper = *pIsHelper;

    Assert(pLabelHelper != nullptr);
    IR::LabelInstr*& labelHelper = *pLabelHelper;

    bool doLocal = true;
    bool doProto = instrLdFld->m_opcode == Js::OpCode::LdMethodFld
        || instrLdFld->m_opcode == Js::OpCode::LdRootMethodFld
        || instrLdFld->m_opcode == Js::OpCode::ScopedLdMethodFld;
    bool doProtoFirst = doProto;
    bool doInlineSlots = true;
    bool doAuxSlots = true;
    if (!PHASE_OFF(Js::ProfileBasedFldFastPathPhase, this->m_func) && instrLdFld->IsProfiledInstr())
    {
        IR::ProfiledInstr * profiledInstrLdFld = instrLdFld->AsProfiledInstr();
        if (profiledInstrLdFld->u.FldInfo().flags != Js::FldInfo_NoInfo)
        {
            doProto = !!(profiledInstrLdFld->u.FldInfo().flags & Js::FldInfo_FromProto);
            doLocal = !!(profiledInstrLdFld->u.FldInfo().flags & Js::FldInfo_FromLocal);

            if ((profiledInstrLdFld->u.FldInfo().flags & (Js::FldInfo_FromInlineSlots | Js::FldInfo_FromAuxSlots)) == Js::FldInfo_FromInlineSlots)
            {
                // If the inline slots flag is set and the aux slots flag is not, only generate the inline slots check
                doAuxSlots = false;
            }
            else if ((profiledInstrLdFld->u.FldInfo().flags & (Js::FldInfo_FromInlineSlots | Js::FldInfo_FromAuxSlots)) == Js::FldInfo_FromAuxSlots)
            {
                // If the aux slots flag is set and the inline slots flag is not, only generate the aux slots check
                doInlineSlots = false;
            }
        }
        else if (!profiledInstrLdFld->u.FldInfo().valueType.IsUninitialized())
        {
            // We have value type info about the field but no flags. This means we shouldn't generate any
            // fast paths for this field load.
            doLocal = false;
            doProto = false;
        }
    }

    if (!doLocal && !doProto)
    {
        return false;
    }

    IR::LabelInstr * labelFallThru = instrLdFld->GetOrCreateContinueLabel();

    if (labelHelper == nullptr)
    {
        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
    }

    IR::RegOpnd * opndBase = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);
    bool usePolymorphicInlineCache = !!propertySymOpnd->m_runtimePolymorphicInlineCache;


    IR::RegOpnd * opndInlineCache = IR::RegOpnd::New(TyMachPtr, this->m_func);
    if (usePolymorphicInlineCache)
    {
        LowererMD::CreateAssign(opndInlineCache, IR::AddrOpnd::New(propertySymOpnd->m_runtimePolymorphicInlineCache->GetInlineCaches(), IR::AddrOpndKindDynamicInlineCache, this->m_func, true), instrLdFld);
    }
    else
    {
        LowererMD::CreateAssign(opndInlineCache, this->LoadRuntimeInlineCacheOpnd(instrLdFld, propertySymOpnd, isHelper), instrLdFld);
    }

    if (typeOpnd == nullptr)
    {
        typeOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
        GenerateObjectTestAndTypeLoad(instrLdFld, opndBase, typeOpnd, labelHelper);
    }

    if (usePolymorphicInlineCache)
    {
        LowererMD::GenerateLoadPolymorphicInlineCacheSlot(instrLdFld, opndInlineCache, typeOpnd, propertySymOpnd->m_runtimePolymorphicInlineCache->GetSize());
    }

    IR::LabelInstr * labelNext = nullptr;
    IR::Opnd * opndDst = instrLdFld->GetDst();
    IR::RegOpnd * opndTaggedType = nullptr;
    IR::BranchInstr * labelNextBranchToPatch = nullptr;

    if (doProto && doProtoFirst)
    {
        if (doInlineSlots)
        {
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            labelNextBranchToPatch = LowererMD::GenerateProtoInlineCacheCheck(instrLdFld, typeOpnd, opndInlineCache, labelNext);
            LowererMD::GenerateLdFldFromProtoInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, true);
            instrLdFld->InsertBefore(labelNext);
        }
        if (doAuxSlots)
        {
            if (opndTaggedType == nullptr)
            {
                opndTaggedType = IR::RegOpnd::New(TyMachPtr, this->m_func);
                LowererMD::GenerateLoadTaggedType(instrLdFld, typeOpnd, opndTaggedType);
            }
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            labelNextBranchToPatch = LowererMD::GenerateProtoInlineCacheCheck(instrLdFld, opndTaggedType, opndInlineCache, labelNext);
            LowererMD::GenerateLdFldFromProtoInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, false);
            instrLdFld->InsertBefore(labelNext);
        }
    }
    if (doLocal)
    {
        if (doInlineSlots)
        {
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            labelNextBranchToPatch = LowererMD::GenerateLocalInlineCacheCheck(instrLdFld, typeOpnd, opndInlineCache, labelNext);
            LowererMD::GenerateLdFldFromLocalInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, true);
            instrLdFld->InsertBefore(labelNext);
        }
        if (doAuxSlots)
        {
            if (opndTaggedType == nullptr)
            {
                opndTaggedType = IR::RegOpnd::New(TyMachPtr, this->m_func);
                LowererMD::GenerateLoadTaggedType(instrLdFld, typeOpnd, opndTaggedType);
            }
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            labelNextBranchToPatch = LowererMD::GenerateLocalInlineCacheCheck(instrLdFld, opndTaggedType, opndInlineCache, labelNext);
            LowererMD::GenerateLdFldFromLocalInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, false);
            instrLdFld->InsertBefore(labelNext);
        }
    }
    if (doProto && !doProtoFirst)
    {
        if (doInlineSlots)
        {
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            labelNextBranchToPatch = LowererMD::GenerateProtoInlineCacheCheck(instrLdFld, typeOpnd, opndInlineCache, labelNext);
            LowererMD::GenerateLdFldFromProtoInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, true);
            instrLdFld->InsertBefore(labelNext);
        }
        if (doAuxSlots)
        {
            if (opndTaggedType == nullptr)
            {
                opndTaggedType = IR::RegOpnd::New(TyMachPtr, this->m_func);
                LowererMD::GenerateLoadTaggedType(instrLdFld, typeOpnd, opndTaggedType);
            }
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            labelNextBranchToPatch = LowererMD::GenerateProtoInlineCacheCheck(instrLdFld, opndTaggedType, opndInlineCache, labelNext);
            LowererMD::GenerateLdFldFromProtoInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, false);
            instrLdFld->InsertBefore(labelNext);
        }
    }

    Assert(labelNextBranchToPatch);
    labelNextBranchToPatch->SetTarget(labelHelper);
    labelNext->Remove();

    // $helper:
    //     dst = CALL Helper(inlineCache, base, field, scriptContext)
    // $fallthru:
    isHelper = true;

    // Return false to indicate the original instruction was not lowered.  Caller will insert the helper label.
    return false;
}

void
Lowerer::GenerateAuxSlotAdjustmentRequiredCheck(
    IR::Instr * instrToInsertBefore,
    IR::RegOpnd * opndInlineCache,
    IR::LabelInstr * labelHelper)
{
    // regSlotCap = MOV [&(inlineCache->u.local.rawUInt16)] // sized to 16 bits
    IR::RegOpnd * regSlotCap = IR::RegOpnd::New(TyMachReg, instrToInsertBefore->m_func);
    IR::IndirOpnd * memSlotCap = IR::IndirOpnd::New(opndInlineCache, (int32)offsetof(Js::InlineCache, u.local.rawUInt16), TyUint16, instrToInsertBefore->m_func);
    InsertMove(regSlotCap, memSlotCap, instrToInsertBefore);

    // SAR regSlotCap, Js::InlineCache::CacheLayoutSelectorBitCount
    IR::IntConstOpnd * constSelectorBitCount = IR::IntConstOpnd::New(Js::InlineCache::CacheLayoutSelectorBitCount, TyUint16, instrToInsertBefore->m_func, /* dontEncode = */ true);
    InsertShiftBranch(Js::OpCode::Shr_A, regSlotCap, regSlotCap, constSelectorBitCount, Js::OpCode::BrNeq_A, true, labelHelper, instrToInsertBefore);
}

void
Lowerer::GenerateSetObjectTypeFromInlineCache(
    IR::Instr * instrToInsertBefore,
    IR::RegOpnd * opndBase,
    IR::RegOpnd * opndInlineCache,
    bool isTypeTagged)
{
    // regNewType = MOV [&(inlineCache->u.local.type)]
    IR::RegOpnd * regNewType = IR::RegOpnd::New(TyMachReg, instrToInsertBefore->m_func);
    IR::IndirOpnd * memNewType = IR::IndirOpnd::New(opndInlineCache, (int32)offsetof(Js::InlineCache, u.local.type), TyMachReg, instrToInsertBefore->m_func);
    InsertMove(regNewType, memNewType, instrToInsertBefore);

    // AND regNewType, ~InlineCacheAuxSlotTypeTag
    if (isTypeTagged)
    {
        // On 64-bit platforms IntConstOpnd isn't big enough to hold TyMachReg values.
        IR::AddrOpnd * constTypeTagComplement = IR::AddrOpnd::New((Js::Var)~InlineCacheAuxSlotTypeTag, IR::AddrOpndKindConstant, instrToInsertBefore->m_func, /* dontEncode = */ true);
        InsertAnd(regNewType, regNewType, constTypeTagComplement, instrToInsertBefore);
    }

    // MOV base->type, regNewType
    IR::IndirOpnd * memObjType = IR::IndirOpnd::New(opndBase, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, instrToInsertBefore->m_func);
    InsertMove(memObjType, regNewType, instrToInsertBefore);
}

bool
Lowerer::GenerateFastStFld(IR::Instr * const instrStFld, IR::JnHelperMethod helperMethod, IR::JnHelperMethod polymorphicHelperMethod, IR::LabelInstr ** labelBailOut, IR::RegOpnd* typeOpnd,
    bool* pIsHelper, IR::LabelInstr** pLabelHelper, bool withPutFlags, Js::PropertyOperationFlags flags)
{
    // Generates:
    //
    // r1 = object->type
    // if (r1 is taggedInt) goto helper
    // Load inline cache
    //    if monomorphic
    //        r2 = address of the monomorphic inline cache
    //    if polymorphic
    //        r2 = address of the polymorphic inline cache array
    //        r3 = (type >> PIC shift amount) & (PIC size - 1)
    //        r2 = r2 + r3
    // Try store property using local cache
    //
    // Loading property using local cache:
    //    if (r1 == r2->u.local.type)
    //        store value to inline slot r2->u.local.slotIndex on r1
    //        goto fallthru
    //    if ((r1 | InlineCacheAuxSlotTypeTag) == r2->u.local.type)
    //        store value to aux slot r2->u.local.slotIndex on r1
    //        goto fallthru
    //
    // Storing to an inline slot:
    //    [r1 + slotIndex * sizeof(Var)] = value
    //
    // Storing to an aux slot:
    //    slotArray = r1->auxSlots
    //    [slotArray + slotIndex * sizeof(Var)] = value
    //
    // We don't emit the type check with aux slot tag if the profile data indicates that we didn't
    // store the property to an aux slot before.
    // We don't emit the type check without an aux slot tag if the profile data indicates that we didn't
    // store the property to an inline slot before.

    IR::Opnd * opndSrc = instrStFld->GetSrc1();
    IR::Opnd * opndDst = instrStFld->GetDst();
    AssertMsg(opndDst->IsSymOpnd() && opndDst->AsSymOpnd()->IsPropertySymOpnd() && opndDst->AsSymOpnd()->m_sym->IsPropertySym(), "Expected PropertySym as dst of StFld");

    IR::PropertySymOpnd * propertySymOpnd = opndDst->AsPropertySymOpnd();
    PropertySym * propertySym = propertySymOpnd->m_sym->AsPropertySym();
    PHASE_PRINT_TESTTRACE(
        Js::ObjTypeSpecPhase,
        this->m_func,
        L"Field store: %s, property: %s, func: %s, cache ID: %d, cloned cache: false\n",
        Js::OpCodeUtil::GetOpCodeName(instrStFld->m_opcode),
        this->m_func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer(),
        this->m_func->GetJnFunction()->GetDisplayName(),
        propertySymOpnd->m_inlineCacheIndex);

    Assert(pIsHelper != nullptr);
    bool& isHelper = *pIsHelper;

    Assert(pLabelHelper != nullptr);
    IR::LabelInstr*& labelHelper = *pLabelHelper;

    bool doStore = true;
    bool doAdd = false;
    bool doInlineSlots = true;
    bool doAuxSlots = true;
    if (!PHASE_OFF(Js::ProfileBasedFldFastPathPhase, this->m_func) && instrStFld->IsProfiledInstr())
    {
        IR::ProfiledInstr * profiledInstrStFld = instrStFld->AsProfiledInstr();
        if (profiledInstrStFld->u.FldInfo().flags != Js::FldInfo_NoInfo)
        {
            if (!(profiledInstrStFld->u.FldInfo().flags & (Js::FldInfo_FromLocal | Js::FldInfo_FromLocalWithoutProperty)))
            {
                return false;
            }

            if (!PHASE_OFF(Js::AddFldFastPathPhase, this->m_func))
            {
                // We always try to do the store field fast path, unless the profile specifically says we never set, but always add a property here.
                if ((profiledInstrStFld->u.FldInfo().flags & (Js::FldInfo_FromLocal | Js::FldInfo_FromLocalWithoutProperty)) == Js::FldInfo_FromLocalWithoutProperty)
                {
                    doStore = false;
                }

                // On the other hand, we only emit the add field fast path, if the profile explicitly says we do add properties here.
                if (!!(profiledInstrStFld->u.FldInfo().flags & Js::FldInfo_FromLocalWithoutProperty))
                {
                    doAdd = true;
                }
            }
            else
            {
                #if ENABLE_DEBUG_CONFIG_OPTIONS
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                #endif
                PHASE_PRINT_TRACE(Js::AddFldFastPathPhase, this->m_func,
                    L"AddFldFastPath: function: %s(%s) property: %s(#%d) no fast path, because the phase is off.\n",
                    this->m_func->GetJnFunction()->GetDisplayName(), this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                    this->m_func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer(), propertySym->m_propertyId);
            }

            if ((profiledInstrStFld->u.FldInfo().flags & (Js::FldInfo_FromInlineSlots | Js::FldInfo_FromAuxSlots)) == Js::FldInfo_FromInlineSlots)
            {
                // If the inline slots flag is set and the aux slots flag is not, only generate the inline slots check
                doAuxSlots = false;
            }
            else if ((profiledInstrStFld->u.FldInfo().flags & (Js::FldInfo_FromInlineSlots | Js::FldInfo_FromAuxSlots)) == Js::FldInfo_FromAuxSlots)
            {
                // If the aux slots flag is set and the inline slots flag is not, only generate the aux slots check
                doInlineSlots = false;
            }
        }
        else if (!profiledInstrStFld->u.FldInfo().valueType.IsUninitialized())
        {
            // We have value type info about the field but no flags. This means we shouldn't generate any
            // fast paths for this field store.
            return false;
        }
    }

    Assert(doStore || doAdd);

    if (labelHelper == nullptr)
    {
        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    }

    IR::LabelInstr * labelFallThru = instrStFld->GetOrCreateContinueLabel();
    IR::RegOpnd * opndBase = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);
    bool usePolymorphicInlineCache = !!propertySymOpnd->m_runtimePolymorphicInlineCache;

    if (doAdd)
    {
#if ENABLE_DEBUG_CONFIG_OPTIONS
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        PHASE_PRINT_TRACE(Js::AddFldFastPathPhase, this->m_func,
            L"AddFldFastPath: function: %s(%s) property: %s(#%d) %s fast path for %s.\n",
            this->m_func->GetJnFunction()->GetDisplayName(), this->m_func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
            this->m_func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer(), propertySym->m_propertyId,
            usePolymorphicInlineCache ? L"poly" : L"mono", doStore ? L"store and add" : L"add only");
    }

    IR::RegOpnd * opndInlineCache = IR::RegOpnd::New(TyMachPtr, this->m_func);
    if (usePolymorphicInlineCache)
    {
        LowererMD::CreateAssign(opndInlineCache, IR::AddrOpnd::New(propertySymOpnd->m_runtimePolymorphicInlineCache->GetInlineCaches(), IR::AddrOpndKindDynamicInlineCache, this->m_func, true), instrStFld);
    }
    else
    {
        LowererMD::CreateAssign(opndInlineCache, this->LoadRuntimeInlineCacheOpnd(instrStFld, propertySymOpnd, isHelper), instrStFld);
    }

    if (typeOpnd == nullptr)
    {
        typeOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
        GenerateObjectTestAndTypeLoad(instrStFld, opndBase, typeOpnd, labelHelper);
    }

    if (usePolymorphicInlineCache)
    {
        LowererMD::GenerateLoadPolymorphicInlineCacheSlot(instrStFld, opndInlineCache, typeOpnd, propertySymOpnd->m_runtimePolymorphicInlineCache->GetSize());
    }

    IR::LabelInstr * labelNext = nullptr;
    IR::RegOpnd * opndTaggedType = nullptr;
    IR::BranchInstr * lastBranchToNext = nullptr;

    if (doStore)
    {
        if (doInlineSlots)
        {
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            lastBranchToNext = LowererMD::GenerateLocalInlineCacheCheck(instrStFld, typeOpnd, opndInlineCache, labelNext);
            LowererMD::GenerateStFldFromLocalInlineCache(instrStFld, opndBase, opndSrc, opndInlineCache, labelFallThru, true);
            instrStFld->InsertBefore(labelNext);
        }
        if (doAuxSlots)
        {
            if (opndTaggedType == nullptr)
            {
                opndTaggedType = IR::RegOpnd::New(TyMachPtr, this->m_func);
                LowererMD::GenerateLoadTaggedType(instrStFld, typeOpnd, opndTaggedType);
            }
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            lastBranchToNext = LowererMD::GenerateLocalInlineCacheCheck(instrStFld, opndTaggedType, opndInlineCache, labelNext);
            LowererMD::GenerateStFldFromLocalInlineCache(instrStFld, opndBase, opndSrc, opndInlineCache, labelFallThru, false);
            instrStFld->InsertBefore(labelNext);
        }
    }

    if (doAdd)
    {
        if (doInlineSlots)
        {
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            lastBranchToNext = LowererMD::GenerateLocalInlineCacheCheck(instrStFld, typeOpnd, opndInlineCache, labelNext, true);
            GenerateSetObjectTypeFromInlineCache(instrStFld, opndBase, opndInlineCache, false);
            LowererMD::GenerateStFldFromLocalInlineCache(instrStFld, opndBase, opndSrc, opndInlineCache, labelFallThru, true);
            instrStFld->InsertBefore(labelNext);
        }
        if (doAuxSlots)
        {
            if (opndTaggedType == nullptr)
            {
                opndTaggedType = IR::RegOpnd::New(TyMachPtr, this->m_func);
                LowererMD::GenerateLoadTaggedType(instrStFld, typeOpnd, opndTaggedType);
            }
            labelNext = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
            lastBranchToNext = LowererMD::GenerateLocalInlineCacheCheck(instrStFld, opndTaggedType, opndInlineCache, labelNext, true);
            GenerateAuxSlotAdjustmentRequiredCheck(instrStFld, opndInlineCache, labelHelper);
            GenerateSetObjectTypeFromInlineCache(instrStFld, opndBase, opndInlineCache, true);
            LowererMD::GenerateStFldFromLocalInlineCache(instrStFld, opndBase, opndSrc, opndInlineCache, labelFallThru, false);
            instrStFld->InsertBefore(labelNext);
        }
    }

    Assert(lastBranchToNext);
    lastBranchToNext->SetTarget(labelHelper);
    labelNext->Remove();

    // $helper:
    //     CALL Helper(inlineCache, base, field, src, scriptContext)
    // $fallthru:
    isHelper = true;

    // Return false to indicate the original instruction was not lowered.  Caller will insert the helper label.
    return false;
}

bool Lowerer::GenerateFastStFldForCustomProperty(IR::Instr *const instr, IR::LabelInstr * *const labelHelperRef)
{
    Assert(instr);
    Assert(labelHelperRef);
    Assert(!*labelHelperRef);

    switch(instr->m_opcode)
    {
        case Js::OpCode::StFld:
        case Js::OpCode::StFldStrict:
            break;

        default:
            return false;
    }

    IR::SymOpnd *const symOpnd = instr->GetDst()->AsSymOpnd();
    PropertySym *const propertySym = symOpnd->m_sym->AsPropertySym();
    if(propertySym->m_propertyId != Js::PropertyIds::lastIndex || !symOpnd->IsPropertySymOpnd())
    {
        return false;
    }

    const ValueType objectValueType(symOpnd->GetPropertyOwnerValueType());
    if(!objectValueType.IsLikelyRegExp())
    {
        return false;
    }

    if(instr->HasBailOutInfo())
    {
        const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
        if(!BailOutInfo::IsBailOutOnImplicitCalls(bailOutKind) || bailOutKind & IR::BailOutKindBits)
        {
            // Other bailout kinds will likely need bailout checks that would not be generated here. In particular, if a type
            // check is necessary here to guard against downstream property accesses on the same object, the type check will
            // fail and cause a bailout if the object is a RegExp object since the "lastIndex" property accesses are not cached.
            return false;
        }
    }

    Func *const func = instr->m_func;

    IR::RegOpnd *const objectOpnd = symOpnd->CreatePropertyOwnerOpnd(func);
    const IR::AutoReuseOpnd autoReuseObjectOpnd(objectOpnd, func);

    IR::LabelInstr *labelHelper = nullptr;
    if(!objectOpnd->IsNotTaggedValue())
    {
        //     test object, 1
        //     jnz $helper
        if(!labelHelper)
        {
            *labelHelperRef = labelHelper = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        }
        m_lowererMD.GenerateObjectTest(objectOpnd, instr, labelHelper);
    }

    if(!objectValueType.IsObject())
    {
        //     cmp [object], Js::JavascriptRegExp::vtable
        //     jne $helper
        if(!labelHelper)
        {
            *labelHelperRef = labelHelper = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        }
        InsertCompareBranch(
            IR::IndirOpnd::New(objectOpnd, 0, TyMachPtr, func),
            LoadVTableValueOpnd(instr, VTableValue::VtableJavascriptRegExp),
            Js::OpCode::BrNeq_A,
            labelHelper,
            instr);
        objectOpnd->SetValueType(objectValueType.ToDefiniteObject());
    }

    //     mov [object + offset(lastIndexVar)], src
    //     mov [object + offset(lastIndexOrFlag)], Js::JavascriptRegExp::NotCachedValue
    //     jmp $done
    InsertMove(
        IR::IndirOpnd::New(objectOpnd, Js::JavascriptRegExp::GetOffsetOfLastIndexVar(), TyVar, func),
        instr->GetSrc1(),
        instr);
    InsertMove(
        IR::IndirOpnd::New(objectOpnd, Js::JavascriptRegExp::GetOffsetOfLastIndexOrFlag(), TyUint32, func),
        IR::IntConstOpnd::New(Js::JavascriptRegExp::NotCachedValue, TyUint32, func, true),
        instr);
    InsertBranch(Js::OpCode::Br, instr->GetOrCreateContinueLabel(), instr);

    return true;
}

IR::RegOpnd *
Lowerer::GenerateIsBuiltinRecyclableObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool checkObjectAndDynamicObject, IR::LabelInstr *labelContinue)
{
    // CMP [srcReg], Js::DynamicObject::`vtable'
    // JEQ $fallThough
    // MOV r1, [src1 + offset(type)]                      -- get the type id
    // MOV r1, [r1 + offset(typeId)]
    // ADD r1, ~TypeIds_LastStaticType       -- if (typeId > TypeIds_LastStaticType && typeId <= TypeIds_LastBuiltinDynamicObject)
    // CMP r1, (TypeIds_LastBuiltinDynamicObject - TypeIds_LastStaticType - 1)
    // JA $helper
    //fallThrough:

    IR::LabelInstr *labelFallthrough = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    if (checkObjectAndDynamicObject)
    {
        if (!regOpnd->IsNotTaggedValue())
        {
            m_lowererMD.GenerateObjectTest(regOpnd, insertInstr, labelHelper);
        }

        m_lowererMD.GenerateIsDynamicObject(regOpnd, insertInstr, labelFallthrough, true);
    }

    IR::RegOpnd * typeRegOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::RegOpnd * typeIdRegOpnd = IR::RegOpnd::New(TyInt32, this->m_func);
    IR::IndirOpnd *indirOpnd;

    //  MOV typeRegOpnd, [src1 + offset(type)]
    indirOpnd = IR::IndirOpnd::New(regOpnd, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, this->m_func);
    m_lowererMD.CreateAssign(typeRegOpnd, indirOpnd, insertInstr);

    //  MOV typeIdRegOpnd, [typeRegOpnd + offset(typeId)]
    indirOpnd = IR::IndirOpnd::New(typeRegOpnd, Js::Type::GetOffsetOfTypeId(), TyInt32, this->m_func);
    m_lowererMD.CreateAssign(typeIdRegOpnd, indirOpnd, insertInstr);

    // ADD typeIdRegOpnd, ~TypeIds_LastStaticType
    InsertAdd(false, typeIdRegOpnd, typeIdRegOpnd,
              IR::IntConstOpnd::New(~Js::TypeIds_LastStaticType, TyInt32, this->m_func, true), insertInstr);

    // CMP typeIdRegOpnd, (TypeIds_LastBuiltinDynamicObject - TypeIds_LastStaticType - 1)
    InsertCompare(
        typeIdRegOpnd,
        IR::IntConstOpnd::New(Js::TypeIds_LastBuiltinDynamicObject - Js::TypeIds_LastStaticType - 1, TyInt32, this->m_func),
        insertInstr);

    if (labelContinue)
    {
        // On success, go to continuation label.
        InsertBranch(Js::OpCode::BrLe_A, true, labelContinue, insertInstr);
    }
    else
    {
        // On failure, go to helper.
        InsertBranch(Js::OpCode::BrGt_A, true, labelHelper, insertInstr);
    }

    // $fallThrough
    insertInstr->InsertBefore(labelFallthrough);

    return typeRegOpnd;
}

bool Lowerer::GenerateFastBrEqLikely(IR::BranchInstr * instrBranch, bool *pNeedHelper)
{
    IR::Opnd *src1 = instrBranch->GetSrc1();
    IR::Opnd *src2 = instrBranch->GetSrc2();
    IR::LabelInstr *targetInstr = instrBranch->GetTarget();

    IR::LabelInstr *labelBooleanCmp = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    IR::LabelInstr *labelTrue = instrBranch->GetOrCreateContinueLabel();
    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    bool isStrictBr = false;
    bool isStrictMode = this->m_func->GetJnFunction()->GetIsStrictMode();
    *pNeedHelper = true;

    switch (instrBranch->m_opcode)
    {
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrSrNotNeq_A:
    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrSrNotEq_A:
        isStrictBr = true;
        break;
    }

    if (src1->GetValueType().IsLikelyBoolean() && src2->GetValueType().IsLikelyBoolean())
    {
        //
        // Booleans
        //
        if (isStrictBr)
        {
            if (!src1->GetValueType().IsBoolean() && !src2->GetValueType().IsBoolean())
            {
                this->m_lowererMD.GenerateObjectTest(src2->AsRegOpnd(), instrBranch, labelHelper, false);
                if (this->m_lowererMD.GenerateJSBooleanTest(src2->AsRegOpnd(), instrBranch, labelBooleanCmp, true))
                {
                    instrBranch->InsertBefore(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelHelper, this->m_func));
                }
            }
            else
            {
                *pNeedHelper = false;
            }
        }
        else
        {
            this->m_lowererMD.GenerateObjectTest(src1->AsRegOpnd(), instrBranch, labelHelper, false);
            this->m_lowererMD.GenerateJSBooleanTest(src1->AsRegOpnd(), instrBranch, labelHelper, false);
            this->m_lowererMD.GenerateObjectTest(src2->AsRegOpnd(), instrBranch, labelHelper, false);
            if (this->m_lowererMD.GenerateJSBooleanTest(src2->AsRegOpnd(), instrBranch, labelBooleanCmp, true))
            {
                instrBranch->InsertBefore(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelHelper, this->m_func));
            }
        }
    }
    else if (src1->GetValueType().IsLikelyObject() && src2->GetValueType().IsLikelyObject())
    {
        //
        // Objects
        //
        IR::LabelInstr *labelTypeIdCheck = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

        if (!isStrictBr)
        {
            // If not strictBr, verify both sides are dynamic objects
            this->m_lowererMD.GenerateObjectTest(src1->AsRegOpnd(), instrBranch, labelHelper, false);
            this->m_lowererMD.GenerateObjectTest(src2->AsRegOpnd(), instrBranch, labelHelper, false);
            this->m_lowererMD.GenerateIsDynamicObject(src1->AsRegOpnd(), instrBranch, labelTypeIdCheck, false);
        }
        else
        {
            this->m_lowererMD.GenerateObjectTest(src2->AsRegOpnd(), instrBranch, labelHelper, false);
        }
        this->m_lowererMD.GenerateIsDynamicObject(src2->AsRegOpnd(), instrBranch, labelBooleanCmp, true);

        instrBranch->InsertBefore(labelTypeIdCheck);

        if (isStrictMode)
        {
            labelTypeIdCheck->isOpHelper = true;
            IR::BranchInstr *branchToHelper = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelHelper, this->m_func);
            instrBranch->InsertBefore(branchToHelper);
        }
        else
        {
            if (!ExternalLowerer::TryGenerateFastExternalEqTest(src1, src2, instrBranch, labelHelper, labelBooleanCmp, this, isStrictBr))
            {
                if (!isStrictBr)
                {
                    GenerateIsBuiltinRecyclableObject(src1->AsRegOpnd(), instrBranch, labelHelper, false /*checkObjectAndDynamicObject*/);
                }
                GenerateIsBuiltinRecyclableObject(src2->AsRegOpnd(), instrBranch, labelHelper, false /*checkObjectAndDynamicObject*/);
            }
        }
    }
    else
    {
        return false;
    }
    instrBranch->InsertBefore(labelBooleanCmp);

    IR::BranchInstr *newBranch = IR::BranchInstr::New(instrBranch->m_opcode, targetInstr, src1, src2, this->m_func);
    instrBranch->InsertBefore(newBranch);

    this->m_lowererMD.LowerCondBranch(newBranch);

    newBranch = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelTrue, this->m_func);
    instrBranch->InsertBefore(newBranch);

    instrBranch->InsertBefore(labelHelper);

    return true;
}

bool Lowerer::GenerateFastBrBool(IR::BranchInstr *const instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::BrFalse_A || instr->m_opcode == Js::OpCode::BrTrue_A);

    Func *const func = instr->m_func;

    if(!instr->GetSrc1()->IsRegOpnd())
    {
        LowererMD::ChangeToAssign(instr->HoistSrc1(Js::OpCode::Ld_A));
    }
    IR::RegOpnd *const src = instr->GetSrc1()->Copy(func)->AsRegOpnd();
    const IR::AutoReuseOpnd autoReuseSrc(src, func);
    const ValueType srcOriginalValueType(src->GetValueType());
    ValueType srcValueType(srcOriginalValueType);

    IR::LabelInstr *const labelTarget = instr->GetTarget();
    IR::LabelInstr *const labelFallthrough = instr->GetOrCreateContinueLabel();
    if(labelTarget == labelFallthrough)
    {
        // Nothing to do
        instr->Remove();
        return false;
    }

    const bool branchOnFalse = instr->m_opcode == Js::OpCode::BrFalse_A;
    IR::LabelInstr *const labelFalse = branchOnFalse ? labelTarget : labelFallthrough;
    IR::LabelInstr *const labelTrue = branchOnFalse ? labelFallthrough : labelTarget;
    const Js::OpCode compareWithFalseBranchToTargetOpCode = branchOnFalse ? Js::OpCode::BrEq_A : Js::OpCode::BrNeq_A;
    IR::LabelInstr *lastLabelBeforeHelper = nullptr;

    /// Typespec'd float
    if (instr->GetSrc1()->GetType() == TyFloat64)
    {
        InsertFloatCheckForZeroOrNanBranch(instr->GetSrc1(), branchOnFalse, labelTarget, labelFallthrough, instr);
        Lowerer::InsertBranch(Js::OpCode::Br, labelFallthrough, instr);
        instr->Remove();
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Null fast path

    if (srcValueType.HasBeenNull() || srcOriginalValueType.IsUninitialized())
    {
        if(srcValueType.IsNull())
        {
            //     jmp $false
            InsertBranch(Js::OpCode::Br, labelFalse, instr);

            // Skip lowering call to helper
            Assert(instr->m_prev->IsBranchInstr());
            instr->Remove();
            return false;
        }

        //     cmp src, null
        //     je $false
        InsertCompareBranch(
            src,
            LoadLibraryValueOpnd(instr, LibraryValue::ValueNull),
            Js::OpCode::BrEq_A,
            labelFalse,
            instr);

        src->SetValueType(srcValueType = srcValueType.SetIsNotAnyOf(ValueType::Null));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Undefined fast path

    if(srcValueType.HasBeenUndefined() || srcOriginalValueType.IsUninitialized())
    {
        if(srcValueType.IsUndefined())
        {
            //     jmp $false
            InsertBranch(Js::OpCode::Br, labelFalse, instr);

            // Skip lowering call to helper
            Assert(instr->m_prev->IsBranchInstr());
            instr->Remove();
            return false;
        }

        //     cmp src, undefined
        //     je $false
        InsertCompareBranch(
            src,
            LoadLibraryValueOpnd(instr, LibraryValue::ValueUndefined),
            Js::OpCode::BrEq_A,
            labelFalse,
            instr);

        src->SetValueType(srcValueType = srcValueType.SetIsNotAnyOf(ValueType::Undefined));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Tagged int fast path

    const bool isNotInt = src->IsNotInt();
    bool checkedForTaggedInt = isNotInt;
    if( (
            srcValueType.HasBeenInt() ||
            srcValueType.HasBeenUnknownNumber() ||
            srcOriginalValueType.IsUninitialized()
        ) && !isNotInt)
    {
        checkedForTaggedInt = true;
        IR::LabelInstr *notTaggedIntLabel = nullptr;
        if(!src->IsTaggedInt())
        {
            //     test src, 1
            //     jz $notTaggedInt
            notTaggedIntLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
            m_lowererMD.GenerateSmIntTest(src, instr, notTaggedIntLabel);
        }

        //     cmp src, tag(0)
        //     je/jne $target
        m_lowererMD.GenerateTaggedZeroTest(src, instr);
        Lowerer::InsertBranch(compareWithFalseBranchToTargetOpCode, labelTarget, instr);

        if(src->IsTaggedInt())
        {
            // Skip lowering call to helper
            Assert(instr->m_prev->IsBranchInstr());
            instr->Remove();
            return false;
        }

        //     jmp $fallthrough
        Lowerer::InsertBranch(Js::OpCode::Br, labelFallthrough, instr);

        // $notTaggedInt:
        if(notTaggedIntLabel)
        {
            instr->InsertBefore(notTaggedIntLabel);
            lastLabelBeforeHelper = notTaggedIntLabel;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Float fast path

    bool generateFloatTest = srcValueType.IsLikelyFloat();
#ifdef _M_IX86
    if (!AutoSystemInfo::Data.SSE2Available())
    {
        generateFloatTest = false;
    }
#endif
    bool checkedForTaggedFloat =
#if FLOATVAR
        srcValueType.IsNotNumber();
#else
        true; // there are no tagged floats, indicate that it has been checked
#endif
    if (generateFloatTest)
    {
        // if(srcValueType.IsFloat()) // skip tagged int check?
        //
        // ValueType::IsFloat() does not guarantee that the storage is not in a tagged int.
        // The tagged int check is necessary. It does, however, guarantee that as long as the value is not
        // stored in a tagged int, that it is definitely stored in a JavascriptNumber/TaggedFloat.

        IR::LabelInstr *const notFloatLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
        if(!checkedForTaggedInt)
        {
            checkedForTaggedInt = true;
            m_lowererMD.GenerateSmIntTest(src, instr, notFloatLabel, nullptr, true);
        }

        //     cmp [src], JavascriptNumber::vtable
        //     jne $notFloat
    #if FLOATVAR
        checkedForTaggedFloat = true;
        IR::RegOpnd *const floatOpnd = m_lowererMD.CheckFloatAndUntag(src, instr, notFloatLabel);
    #else
        m_lowererMD.GenerateFloatTest(src, instr, notFloatLabel);
        IR::IndirOpnd *const floatOpnd = IR::IndirOpnd::New(src, Js::JavascriptNumber::GetValueOffset(), TyMachDouble, func);
    #endif

        //     cmp src, 0.0
        //     jp $false
        //     je/jne $target
        //     jmp $fallthrough
        InsertFloatCheckForZeroOrNanBranch(floatOpnd, branchOnFalse, labelTarget, labelFallthrough, instr);
        Lowerer::InsertBranch(Js::OpCode::Br, labelFallthrough, instr);

        // $notFloat:
        instr->InsertBefore(notFloatLabel);
        lastLabelBeforeHelper = notFloatLabel;

        src->SetValueType(srcValueType = srcValueType.SetIsNotAnyOf(ValueType::AnyNumber));
    }

    IR::LabelInstr *labelHelper = nullptr;
    bool _didObjectTest = checkedForTaggedInt && checkedForTaggedFloat;
    const auto EnsureObjectTest = [&]()
    {
        if(_didObjectTest)
        {
            return;
        }
        if(!labelHelper)
        {
            labelHelper = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        }
        m_lowererMD.GenerateObjectTest(src, instr, labelHelper);
        _didObjectTest = true;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Boolean fast path

    if (srcValueType.HasBeenBoolean() || srcOriginalValueType.IsUninitialized())
    {
        IR::LabelInstr *notBooleanLabel = nullptr;
        if (!srcValueType.IsBoolean())
        {
            EnsureObjectTest();

            //     cmp [src], JavascriptBoolean::vtable
            //     jne $notBoolean
            notBooleanLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
            InsertCompareBranch(
                IR::IndirOpnd::New(src, 0, TyMachPtr, func),
                LoadVTableValueOpnd(instr, VTableValue::VtableJavascriptBoolean),
                Js::OpCode::BrNeq_A,
                notBooleanLabel,
                instr);
        }

        //     cmp src, false
        //     je/jne $target
        InsertCompareBranch(
            src,
            LoadLibraryValueOpnd(instr, LibraryValue::ValueFalse),
            compareWithFalseBranchToTargetOpCode,
            labelTarget,
            instr);

        if (srcValueType.IsBoolean())
        {
            // Skip lowering call to helper
            Assert(!labelHelper);
            Assert(instr->m_prev->IsBranchInstr());
            instr->Remove();
            return false;
        }
        //     jmp $fallthrough
        Lowerer::InsertBranch(Js::OpCode::Br, labelFallthrough, instr);

        if (notBooleanLabel)
        {
            instr->InsertBefore(notBooleanLabel);
            lastLabelBeforeHelper = notBooleanLabel;

        }

        src->SetValueType(srcValueType = srcValueType.SetIsNotAnyOf(ValueType::Boolean));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // String fast path

    if(srcValueType.HasBeenString())
    {
        IR::LabelInstr *notStringLabel = nullptr;
        if(!srcValueType.IsString())
        {
            EnsureObjectTest();

            notStringLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
            GenerateStringTest(src, instr, notStringLabel, nullptr, false);
        }

        //     cmp [src + offset(length)], 0
        //     jeq/jne $target
        InsertCompareBranch(
            IR::IndirOpnd::New(src, Js::JavascriptString::GetOffsetOfcharLength(), TyUint32, func),
            IR::IntConstOpnd::New(0, TyUint32, func, true),
            compareWithFalseBranchToTargetOpCode,
            labelTarget,
            instr);

        if(srcValueType.IsString())
        {
            // Skip lowering call to helper
            Assert(!labelHelper);
            Assert(instr->m_prev->IsBranchInstr());
            instr->Remove();
            return false;
        }

        //     jmp $fallthrough
        Lowerer::InsertBranch(Js::OpCode::Br, labelFallthrough, instr);

        if(notStringLabel)
        {
            instr->InsertBefore(notStringLabel);
            lastLabelBeforeHelper = notStringLabel;
        }

        src->SetValueType(srcValueType = srcValueType.SetIsNotAnyOf(ValueType::String));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Object fast path

    if (srcValueType.IsLikelyObject())
    {
        if(srcValueType.IsObject())
        {
            if(srcValueType.GetObjectType() > ObjectType::Object)
            {
                // Specific object types that are tracked are equivalent to 'true'
                //     jmp $true
                InsertBranch(Js::OpCode::Br, labelTrue, instr);

                // Skip lowering call to helper
                Assert(!labelHelper);
                Assert(instr->m_prev->IsBranchInstr());
                instr->Remove();
                return false;
            }
        }
        else
        {
            EnsureObjectTest();
        }

        //     mov srcType, [src + offset(type)]       -- load type
        IR::RegOpnd *const srcType = IR::RegOpnd::New(TyMachPtr, func);
        const IR::AutoReuseOpnd autoReuseR1(srcType, func);
        InsertMove(srcType, IR::IndirOpnd::New(src, Js::RecyclableObject::GetOffsetOfType(), TyMachPtr, func), instr);

        //     test [srcType + offset(flags)], TypeFlagMask_IsFalsy       -- check  if falsy
        //     jnz $false
        InsertTestBranch(
            IR::IndirOpnd::New(srcType, Js::Type::GetOffsetOfFlags(), TyUint8, func),
            IR::IntConstOpnd::New(TypeFlagMask_IsFalsy, TyUint8, func),
            Js::OpCode::BrNeq_A,
            labelFalse,
            instr);

        //     cmp [srcType + offset(typeId)], TypeIds_LastJavascriptPrimitiveType  -- check base TypeIds_LastJavascriptPrimitiveType
        //     ja $true
        InsertCompareBranch(
            IR::IndirOpnd::New(srcType, Js::Type::GetOffsetOfTypeId(), TyInt32, func),
            IR::IntConstOpnd::New(Js::TypeIds_LastJavascriptPrimitiveType, TyInt32, func),
            Js::OpCode::BrGt_A,
            true /* isUnsigned */,
            labelTrue,
            instr);

        if(!labelHelper)
        {
            labelHelper = IR::LabelInstr::New(Js::OpCode::Label, func, true);
        }
        lastLabelBeforeHelper = nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Helper call

    // $helper:
    if(lastLabelBeforeHelper)
    {
        Assert(instr->m_prev == lastLabelBeforeHelper);
        lastLabelBeforeHelper->isOpHelper = true;
    }
    if (labelHelper)
    {
        Assert(labelHelper->isOpHelper);
        instr->InsertBefore(labelHelper);
    }

    //     call JavascriptConversion::ToBoolean
    IR::RegOpnd *const toBoolDst = IR::RegOpnd::New(TyInt32, func);
    const IR::AutoReuseOpnd autoReuseToBoolDst(toBoolDst, func);
    IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, toBoolDst, instr->GetSrc1(), func);
    instr->InsertBefore(callInstr);
    LowerUnaryHelperMem(callInstr, IR::HelperConv_ToBoolean);

    //     test eax, eax
    InsertTest(toBoolDst, toBoolDst, instr);

    //     je/jne $target
    Assert(instr->IsBranchInstr());
    instr->FreeSrc1();
    instr->m_opcode = LowererMD::MDBranchOpcode(compareWithFalseBranchToTargetOpCode);
    Assert(instr->AsBranchInstr()->GetTarget() == labelTarget);

    // Skip lowering another call to helper
    return false;
}

// Helper method used in LowerMD by all platforms.
// Creates HelperCallOpnd or DiagHelperCallOpnd, based on helperMethod and state.
// static
IR::HelperCallOpnd*
Lowerer::CreateHelperCallOpnd(IR::JnHelperMethod helperMethod, int helperArgCount, Func* func)
{
    Assert(func);

    IR::HelperCallOpnd* helperCallOpnd;
    if (CONFIG_FLAG(EnableContinueAfterExceptionWrappersForHelpers) &&
        func->IsJitInDebugMode() &&
        HelperMethodAttributes::CanThrow(helperMethod))
    {
        // Create DiagHelperCallOpnd to indicate that it's needed to wrap original helper with try-catch wrapper,
        // so that we can ignore exception and bailout to next stmt in debugger.
        // For details, see: Lib\Runtime\Debug\DiagHelperMethodWrapper.{h,cpp}.
        helperCallOpnd = IR::DiagHelperCallOpnd::New(helperMethod, func, helperArgCount);
    }
    else
    {
         helperCallOpnd = IR::HelperCallOpnd::New(helperMethod, func);
    }

    return helperCallOpnd;
}

bool
Lowerer::TryGenerateFastBrOrCmTypeOf(IR::Instr *instr, IR::Instr **prev, bool *pfNoLower)
{
    Assert(prev);
    Assert(instr->m_opcode == Js::OpCode::BrSrEq_A     ||
           instr->m_opcode == Js::OpCode::BrSrNeq_A    ||
           instr->m_opcode == Js::OpCode::BrSrNotEq_A  ||
           instr->m_opcode == Js::OpCode::BrSrNotNeq_A ||
           instr->m_opcode == Js::OpCode::CmSrEq_A     ||
           instr->m_opcode == Js::OpCode::CmSrNeq_A    ||
           instr->m_opcode == Js::OpCode::BrEq_A       ||
           instr->m_opcode == Js::OpCode::BrNeq_A      ||
           instr->m_opcode == Js::OpCode::BrNotEq_A    ||
           instr->m_opcode == Js::OpCode::BrNotNeq_A   ||
           instr->m_opcode == Js::OpCode::CmEq_A       ||
           instr->m_opcode == Js::OpCode::CmNeq_A);

    //
    // instr         - (Br/Cm)Sr(N)eq_A
    // instr->m_prev - typeOf
    //
    IR::Instr *instrLd = instr->GetPrevRealInstrOrLabel();
    bool skippedLoads = false;
    //Skip intermediate Ld_A which might be inserted by flow graph peeps
    while (instrLd && instrLd->m_opcode == Js::OpCode::Ld_A )
    {
        if (!(instrLd->GetDst()->IsRegOpnd() && instrLd->GetDst()->AsRegOpnd()->m_fgPeepTmp))
        {
            return false;
        }
        if (instrLd->HasBailOutInfo())
        {
            return false;
        }
        instrLd = instrLd->GetPrevRealInstrOrLabel();
        skippedLoads = true;
    }

    IR::Instr *typeOf = instrLd;

    if (typeOf && (typeOf->m_opcode == Js::OpCode::Typeof))
    {
        IR::RegOpnd *typeOfDst = typeOf->GetDst()->IsRegOpnd() ? typeOf->GetDst()->AsRegOpnd() : nullptr;
        IR::RegOpnd *instrSrc1 = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd() : nullptr;
        IR::RegOpnd *instrSrc2 = instr->GetSrc2()->IsRegOpnd() ? instr->GetSrc2()->AsRegOpnd() : nullptr;

        if (typeOfDst && instrSrc1 && instrSrc2)
        {
            if (instrSrc1->m_sym == typeOfDst->m_sym)
            {
                if (!instrSrc1->m_isTempLastUse)
                {
                    return false;
                }

                if (!(instrSrc2->m_sym->m_isSingleDef && instrSrc2->m_sym->m_isStrConst))
                {
                    return false;
                }

                // The second argument to [Cm|Br]TypeOf is the typeid.
                IR::IntConstOpnd *typeIdOpnd = nullptr;

                Assert(instrSrc2->m_sym->m_isSingleDef);
                Assert(instrSrc2->m_sym->m_instrDef->GetSrc1()->IsAddrOpnd());

                // We can't optimize non-javascript type strings.
                Js::JavascriptString *typeNameJsString = Js::JavascriptString::FromVar(instrSrc2->m_sym->m_instrDef->GetSrc1()->AsAddrOpnd()->m_address);
                const wchar_t        *typeName         = typeNameJsString->GetString();

                Js::InternalString typeNameString(typeName, typeNameJsString->GetLength());
                if (Js::InternalStringComparer::Equals(typeNameString, Js::Type::UndefinedTypeNameString))
                {
                    typeIdOpnd = IR::IntConstOpnd::New(Js::TypeIds_Undefined, TyInt32, instr->m_func);
                }
                else if (Js::InternalStringComparer::Equals(typeNameString, Js::Type::ObjectTypeNameString))
                {
                    typeIdOpnd = IR::IntConstOpnd::New(Js::TypeIds_Object, TyInt32, instr->m_func);
                }
                else if (Js::InternalStringComparer::Equals(typeNameString, Js::Type::BooleanTypeNameString))
                {
                    typeIdOpnd = IR::IntConstOpnd::New(Js::TypeIds_Boolean, TyInt32, instr->m_func);
                }
                else if (Js::InternalStringComparer::Equals(typeNameString, Js::Type::NumberTypeNameString))
                {
                    typeIdOpnd = IR::IntConstOpnd::New(Js::TypeIds_Number, TyInt32, instr->m_func);
                }
                else if (Js::InternalStringComparer::Equals(typeNameString, Js::Type::StringTypeNameString))
                {
                    typeIdOpnd = IR::IntConstOpnd::New(Js::TypeIds_String, TyInt32, instr->m_func);
                }
                else if (Js::InternalStringComparer::Equals(typeNameString, Js::Type::FunctionTypeNameString))
                {
                    typeIdOpnd = IR::IntConstOpnd::New(Js::TypeIds_Function, TyInt32, instr->m_func);
                }
                else
                {
                    return false;
                }

                if (skippedLoads)
                {
                    //validate none of dst of Ld_A overlaps with typeof src or dst
                    IR::Opnd* typeOfSrc = typeOf->GetSrc1();
                    instrLd = typeOf->GetNextRealInstr();
                    while (instrLd != instr)
                    {
                        if (instrLd->GetDst()->IsEqual(typeOfDst) || instrLd->GetDst()->IsEqual(typeOfSrc))
                        {
                            return false;
                        }
                        instrLd = instrLd->GetNextRealInstr();
                    }
                    typeOf->Unlink();
                    instr->InsertBefore(typeOf);
                }
                // The first argument to [Cm|Br]TypeOf is the first arg to the TypeOf instruction.
                IR::Opnd *objectOpnd = typeOf->GetSrc1();
                Assert(objectOpnd->IsRegOpnd());

                // Now emit this instruction and remove the ldstr and typeOf.
                *prev = typeOf->m_prev;
                *pfNoLower = false;
                if (instr->IsBranchInstr())
                {
                    GenerateFastBrTypeOf(instr, objectOpnd->AsRegOpnd(), typeIdOpnd, typeOf, pfNoLower);
                }
                else
                {
                    GenerateFastCmTypeOf(instr, objectOpnd->AsRegOpnd(), typeIdOpnd, typeOf, pfNoLower);
                }

                return true;
            }
        }
    }

    return false;
}

void
Lowerer::GenerateFalsyObjectTest(IR::Instr *insertInstr, IR::RegOpnd *TypeOpnd, Js::TypeId typeIdToCheck, IR::LabelInstr* target, IR::LabelInstr* done, bool isNeqOp)
{
    if (!this->m_func->GetScriptContext()->GetThreadContext()->CanBeFalsy(typeIdToCheck) && typeIdToCheck != Js::TypeIds_Undefined)
    {
        // Don't need the check for falsy, the typeId we are looking for doesn't care
        return;
    }

    IR::Opnd *flagsOpnd = IR::IndirOpnd::New(TypeOpnd, Js::Type::GetOffsetOfFlags(), TyInt32, this->m_func);
    InsertTest(flagsOpnd, IR::IntConstOpnd::New(TypeFlagMask_IsFalsy, TyInt32, this->m_func), insertInstr);

    if (typeIdToCheck == Js::TypeIds_Undefined)
    {
        //Falsy object returns true for undefined ((typeof falsyObj) == "undefined")
        InsertBranch( Js::OpCode::BrNeq_A, true, isNeqOp ? done : target, insertInstr);
    }
    else
    {
        //Falsy object returns false for all other types ((typeof falsyObj) != "function")
        InsertBranch( Js::OpCode::BrNeq_A, true, isNeqOp? target : done , insertInstr);
    }
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastBrTypeOf
///
///----------------------------------------------------------------------------
void
Lowerer::GenerateFastBrTypeOf(IR::Instr *branch, IR::RegOpnd *object, IR::IntConstOpnd *typeIdOpnd, IR::Instr *typeOf, bool *pfNoLower)
{
    Js::TypeId      typeId   = static_cast<Js::TypeId>(typeIdOpnd->GetValue());
    IR::LabelInstr *target   = branch->AsBranchInstr()->GetTarget();
    IR::LabelInstr *done     = IR::LabelInstr::New(Js::OpCode::Label, m_func, false);
    IR::LabelInstr *helper = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::RegOpnd    *typeRegOpnd = IR::RegOpnd::New(TyMachReg, m_func);
    bool            isNeqOp;

    switch(branch->m_opcode)
    {
    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrNotEq_A:
    case Js::OpCode::BrNotEq_A:
        isNeqOp = true;
        break;

    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrSrNotNeq_A:
    case Js::OpCode::BrNotNeq_A:
        isNeqOp = false;
        break;
    default:
        Assert(UNREACHED);
        __assume(UNREACHED);
    }

    // JNE/BNE (typeId == Js::TypeIds_Number) ? $target : $done
    IR::LabelInstr *label = (typeId == Js::TypeIds_Number) ? target : done;
    if (isNeqOp)
        label = (label == target) ? done : target;

    m_lowererMD.GenerateObjectTest(object, branch, label);

    // MOV typeRegOpnd, [object + offset(Type)]
    InsertMove(typeRegOpnd,
               IR::IndirOpnd::New(object, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, m_func),
               branch);

    GenerateFalsyObjectTest(branch, typeRegOpnd, typeId, target, done, isNeqOp);

    // MOV objTypeId, [typeRegOpnd + offset(TypeId)]
    IR::RegOpnd* objTypeIdOpnd = IR::RegOpnd::New(TyInt32, m_func);
    InsertMove(objTypeIdOpnd,
               IR::IndirOpnd::New(typeRegOpnd, Js::Type::GetOffsetOfTypeId(), TyInt32, m_func),
               branch);

    // CMP objTypeId, typeId
    // JEQ/JGE $done
    if (typeId == Js::TypeIds_Object)
    {
        InsertCompareBranch(objTypeIdOpnd, typeIdOpnd, Js::OpCode::BrGe_A, isNeqOp ? done : target, branch);
    }
    else if (typeId == Js::TypeIds_Function)
    {
        InsertCompareBranch(objTypeIdOpnd, typeIdOpnd, Js::OpCode::BrEq_A, isNeqOp ? done : target, branch);
    }
    else if (typeId == Js::TypeIds_Number)
    {
        //Check for the typeIds between  TypeIds_FirstNumberType <= typeIds <= TypeIds_LastNumberType
        InsertSub(false, objTypeIdOpnd, objTypeIdOpnd, IR::IntConstOpnd::New(Js::TypeIds_FirstNumberType, TyInt32, branch->m_func),branch);

        InsertCompare(objTypeIdOpnd, IR::IntConstOpnd::New(Js::TypeIds_LastNumberType - Js::TypeIds_FirstNumberType, TyInt32, branch->m_func), branch);
        InsertBranch(isNeqOp ? Js::OpCode::BrGt_A : Js::OpCode::BrLe_A, true, target, branch);
    }
    else
    {
        InsertCompare(objTypeIdOpnd, typeIdOpnd, branch);
        InsertBranch(isNeqOp ? Js::OpCode::BrNeq_A : Js::OpCode::BrEq_A, target, branch);
    }

    // This could be 'null' which, for historical reasons, has a TypeId < TypeIds_Object but
    // is still a Javascript "object."
    if (typeId == Js::TypeIds_Object)
    {
        // CMP object, 0xXXXXXXXX
        // JEQ isNeqOp ? $done : $target
        InsertCompareBranch(object,
                            LoadLibraryValueOpnd(branch, LibraryValue::ValueNull),
                            Js::OpCode::BrEq_A,
                            isNeqOp ? done : target,
                            branch);
    }

    branch->InsertAfter(done); // Get this label first

    // "object" or "function" may come from HostDispatch. Needs helper if that's the case.
    if (typeId == Js::TypeIds_Object || typeId == Js::TypeIds_Function)
    {
        // CMP objTypeId, TypeIds_Proxy. typeof proxy could be 'object' or 'function' depends on the target
        // JNE isNeqOp ? $target : $done
        InsertCompareBranch(objTypeIdOpnd,
            IR::IntConstOpnd::New(Js::TypeIds_Proxy, TyInt32, m_func),
            Js::OpCode::BrEq_A,
            helper,
            branch);

        // CMP objTypeId, TypeIds_HostDispatch
        // JNE isNeqOp ? $target : $done
        InsertCompareBranch(objTypeIdOpnd,
                            IR::IntConstOpnd::New(Js::TypeIds_HostDispatch, TyInt32, m_func),
                            Js::OpCode::BrNeq_A,
                            isNeqOp ? target : done,
                            branch);

        // Now emit Typeof and lower it like we would've for the helper call.
        {
            branch->InsertBefore(helper);
            typeOf->Unlink();
            branch->InsertBefore(typeOf);
            LowerUnaryHelperMem(typeOf, IR::HelperOp_Typeof);
        }
    }
    else // Other primitive types don't need helper
    {
        typeOf->Remove();
        branch->Remove();
        *pfNoLower = true;
    }

    // $done:
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastCmTypeOf
///
///----------------------------------------------------------------------------
void
Lowerer::GenerateFastCmTypeOf(IR::Instr *compare, IR::RegOpnd *object, IR::IntConstOpnd *typeIdOpnd, IR::Instr *typeOf, bool *pfNoLower)
{
    Assert(compare->m_opcode == Js::OpCode::CmSrEq_A  ||
           compare->m_opcode == Js::OpCode::CmEq_A    ||
           compare->m_opcode == Js::OpCode::CmSrNeq_A ||
           compare->m_opcode == Js::OpCode::CmNeq_A);

    Js::TypeId      typeId   = static_cast<Js::TypeId>(typeIdOpnd->GetValue());
    IR::LabelInstr *movFalse = IR::LabelInstr::New(Js::OpCode::Label, m_func, false);
    IR::LabelInstr *done = IR::LabelInstr::New(Js::OpCode::Label, m_func, false);
    IR::LabelInstr *helper= IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::RegOpnd    *dst      = compare->GetDst()->IsRegOpnd() ? compare->GetDst()->AsRegOpnd() : nullptr;
    IR::RegOpnd    *typeRegOpnd  = IR::RegOpnd::New(TyMachReg, m_func);
    bool isNeqOp             = compare->m_opcode == Js::OpCode::CmSrNeq_A ||
                               compare->m_opcode == Js::OpCode::CmNeq_A;

    Assert(dst);

    if (dst->IsEqual(object))
    {
        //dst same as the src of typeof. As we need to move true to dst first we need to save the src to a new opnd
        IR::RegOpnd *newObject = IR::RegOpnd::New(object->GetType(), m_func);
        InsertMove(newObject, object, compare); //Save src
        object = newObject;
    }

    // mov dst, 'true'
    InsertMove(dst,
               LoadLibraryValueOpnd(compare, LibraryValue::ValueTrue),
               compare);

    // TEST object, 1
    // JNE (typeId == Js::TypeIds_Number) ? $done : $movFalse
    IR::LabelInstr *target = (typeId == Js::TypeIds_Number) ? done : movFalse;
    if (isNeqOp)
    {
        target = (target == done) ? movFalse : done;
    }

    m_lowererMD.GenerateObjectTest(object, compare, target);


    // MOV typeRegOpnd, [object + offset(Type)]
    InsertMove(typeRegOpnd,
               IR::IndirOpnd::New(object, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, m_func),
               compare);

    GenerateFalsyObjectTest(compare, typeRegOpnd, typeId, done, movFalse, isNeqOp);

    // MOV objTypeId, [typeRegOpnd + offset(TypeId)]
    IR::RegOpnd* objTypeIdOpnd = IR::RegOpnd::New(TyInt32, m_func);
    InsertMove(objTypeIdOpnd,
               IR::IndirOpnd::New(typeRegOpnd, Js::Type::GetOffsetOfTypeId(), TyInt32, m_func),
               compare);

    // CMP objTypeId, typeId
    // JEQ/JGE $done
    if (typeId == Js::TypeIds_Object)
    {
        InsertCompareBranch(objTypeIdOpnd, typeIdOpnd, Js::OpCode::BrGe_A, isNeqOp ? movFalse : done, compare);
    }
    else if (typeId == Js::TypeIds_Function)
    {
        InsertCompareBranch(objTypeIdOpnd, typeIdOpnd, Js::OpCode::BrEq_A, isNeqOp ? movFalse : done, compare);
    }
    else if (typeId == Js::TypeIds_Number)
    {
        //Check for the typeIds between  TypeIds_FirstNumberType <= typeIds <= TypeIds_LastNumberType
        InsertCompareBranch(objTypeIdOpnd,
                                IR::IntConstOpnd::New(Js::TypeIds_LastNumberType, TyInt32, compare->m_func),
                                Js::OpCode::BrGt_A,
                                isNeqOp ? done : movFalse,
                                compare);

        InsertCompareBranch(objTypeIdOpnd,
                                IR::IntConstOpnd::New(Js::TypeIds_FirstNumberType, TyInt32, compare->m_func),
                                isNeqOp? Js::OpCode::BrLt_A : Js::OpCode::BrGe_A,
                                done,
                                compare);
    }
    else
    {
        InsertCompareBranch(objTypeIdOpnd, typeIdOpnd, isNeqOp ? Js::OpCode::BrNeq_A : Js::OpCode::BrEq_A, done, compare);
    }

    // This could be 'null' which, for historical reasons, has a TypeId < TypeIds_Object but
    // is still a Javascript "object."
    if (typeId == Js::TypeIds_Object)
    {
        // CMP object, 0xXXXXXXXX
        // JEQ isNeqOp ? $movFalse : $done
        InsertCompareBranch(object,
                            LoadLibraryValueOpnd(compare, LibraryValue::ValueNull),
                            Js::OpCode::BrEq_A,
                            isNeqOp ? movFalse : done,
                            compare);
    }

    compare->InsertAfter(done); // Get this label first

    // "object" or "function" may come from HostDispatch. Needs helper if that's the case.
    if (typeId == Js::TypeIds_Object || typeId == Js::TypeIds_Function)
    {
        // CMP objTypeId, TypeIds_Proxy
        // JNE isNeqOp ? $done : $movFalse
        InsertCompareBranch(objTypeIdOpnd,
            IR::IntConstOpnd::New(Js::TypeIds_Proxy, TyInt32, m_func),
            Js::OpCode::BrEq_A,
            helper,
            compare);

        // CMP objTypeId, TypeIds_HostDispatch
        // JNE isNeqOp ? $done : $movFalse
        InsertCompareBranch(objTypeIdOpnd,
                            IR::IntConstOpnd::New(Js::TypeIds_HostDispatch, TyInt32, m_func),
                            Js::OpCode::BrNeq_A,
                            isNeqOp ? done : movFalse,
                            compare);

        // Now emit Typeof like we would've for the helper call.
        {
            compare->InsertBefore(helper);
            typeOf->Unlink();
            compare->InsertBefore(typeOf);
            LowerUnaryHelperMem(typeOf, IR::HelperOp_Typeof);
        }

        // JMP/B $done
        InsertBranch(Js::OpCode::Br, done, done);
    }
    else // Other primitive types don't need helper
    {
        typeOf->Remove();
        compare->Remove();
        *pfNoLower = true;
    }

    // $movFalse: (insert before $done)
    done->InsertBefore(movFalse);

    // MOV dst, 'false'
    InsertMove(dst, LoadLibraryValueOpnd(done, LibraryValue::ValueFalse), done);

    // $done:
}

void
Lowerer::GenerateCheckForCallFlagNew(IR::Instr* instrInsert)
{
    Func *func = instrInsert->m_func;
    IR::LabelInstr * labelDone = IR::LabelInstr::New(Js::OpCode::Label, func, false);

    Assert(!func->IsInlinee());

    // MOV s1, [ebp + 4]                        // s1 = call info
    // AND s2, s1, Js::CallFlags_New            // s2 = s1 & Js::CallFlags_New
    // CMP s2, 0
    // JNE $Done
    // CALL RuntimeTypeError
    // $Done

    IR::SymOpnd* callInfoOpnd = Lowerer::LoadCallInfo(instrInsert);
    Assert(Js::CallInfo::ksizeofCount == 24);

    IR::RegOpnd* isNewFlagSetRegOpnd = IR::RegOpnd::New(TyUint32, func);

    InsertAnd(isNewFlagSetRegOpnd, callInfoOpnd, IR::IntConstOpnd::New((IntConstType)Js::CallFlags_New << Js::CallInfo::ksizeofCount, TyUint32, func, true), instrInsert);
    InsertTestBranch(isNewFlagSetRegOpnd, isNewFlagSetRegOpnd, Js::OpCode::BrNeq_A, labelDone, instrInsert);

    IR::Instr *throwInstr = IR::Instr::New(
        Js::OpCode::RuntimeTypeError,
        IR::RegOpnd::New(TyMachReg, m_func),
        IR::IntConstOpnd::New(SCODE_CODE(JSERR_ClassConstructorCannotBeCalledWithoutNew), TyInt32, m_func),
        m_func);
    instrInsert->InsertBefore(throwInstr);
    this->LowerUnaryHelperMem(throwInstr, IR::HelperOp_RuntimeTypeError);

    instrInsert->InsertBefore(labelDone);
    instrInsert->Remove();
}

void
Lowerer::GenerateLoadNewTarget(IR::Instr* instrInsert)
{
    Func *func = instrInsert->m_func;

    IR::LabelInstr * labelDone = IR::LabelInstr::New(Js::OpCode::Label, func, false);
    IR::LabelInstr * labelLoadArgNewTarget = IR::LabelInstr::New(Js::OpCode::Label, func, false);
    IR::Opnd* opndUndefAddress = this->LoadLibraryValueOpnd(instrInsert, LibraryValue::ValueUndefined);

    Assert(!func->IsInlinee());

    if (func->GetJnFunction()->IsGenerator())
    {
        instrInsert->SetSrc1(opndUndefAddress);
        LowererMD::ChangeToAssign(instrInsert);
        return;
    }

    // MOV dst, undefined                       // dst = undefined
    // MOV s1, [ebp + 4]                        // s1 = call info
    // AND s2, s1, Js::CallFlags_NewTarget      // s2 = s1 & Js::CallFlags_NewTarget
    // CMP s2, 0
    // JNE $LoadLastArgument
    // AND s2, s1, Js::CallFlags_New            // s2 = s1 & Js::CallFlags_New
    // CMP s2, 0
    // JE $Done
    // MOV dst, [ebp + 8]                       // dst = function object
    // JMP $Done
    // $LoadLastArgument
    // AND s2, s1, (0x00FFFFFF)
    // MOV s3, ebp
    // MOV dst, [s3 + 5 * sizeof(Var) + s2]     // s3 = last argument
    // $Done

    IR::Opnd * dstOpnd = instrInsert->GetDst();
    Assert(dstOpnd->IsRegOpnd());
    LowererMD::CreateAssign(dstOpnd, opndUndefAddress, instrInsert);

    IR::SymOpnd* callInfoOpnd = Lowerer::LoadCallInfo(instrInsert);
    Assert(Js::CallInfo::ksizeofCount == 24);

    IR::RegOpnd* isNewFlagSetRegOpnd = IR::RegOpnd::New(TyUint32, func);

    InsertAnd(isNewFlagSetRegOpnd, callInfoOpnd, IR::IntConstOpnd::New((IntConstType)Js::CallFlags_NewTarget << Js::CallInfo::ksizeofCount, TyUint32, func, true), instrInsert);
    InsertTestBranch(isNewFlagSetRegOpnd, isNewFlagSetRegOpnd, Js::OpCode::BrNeq_A, labelLoadArgNewTarget, instrInsert);

    InsertAnd(isNewFlagSetRegOpnd, callInfoOpnd, IR::IntConstOpnd::New((IntConstType)Js::CallFlags_New << Js::CallInfo::ksizeofCount, TyUint32, func, true), instrInsert);
    GenerateNotZeroTest(isNewFlagSetRegOpnd, labelDone, instrInsert);

    IR::Instr* loadFuncInstr = IR::Instr::New(Js::OpCode::AND, func);
    loadFuncInstr->SetDst(instrInsert->GetDst());
    m_lowererMD.LoadFuncExpression(loadFuncInstr);

    instrInsert->InsertBefore(loadFuncInstr);
    InsertBranch(Js::OpCode::Br, labelDone, instrInsert);

    instrInsert->InsertBefore(labelLoadArgNewTarget);

    IR::RegOpnd* argCountOpnd = isNewFlagSetRegOpnd;
    InsertAnd(argCountOpnd, callInfoOpnd, IR::IntConstOpnd::New(0x00FFFFFF, TyUint32, func, true), instrInsert);

    IR::RegOpnd *baseOpnd = IR::RegOpnd::New(TyMachReg, func);
    StackSym *paramSym = StackSym::New(TyMachReg, this->m_func);
    instrInsert->InsertBefore(this->m_lowererMD.LoadStackAddress(paramSym, baseOpnd));

    const BYTE indirScale = this->m_lowererMD.GetDefaultIndirScale();
    IR::IndirOpnd* argIndirOpnd = IR::IndirOpnd::New(baseOpnd->AsRegOpnd(), argCountOpnd, indirScale, TyMachReg, this->m_func);

    // Need to offset valueOpnd by 5. Instead of changing valueOpnd, we can just add an offset to the indir. Changing
    // valueOpnd requires creation of a temp sym (if it's not already a temp) so that the value of the sym that
    // valueOpnd represents is not changed.
    uint16 actualOffset = GetFormalParamOffset() + 1; //5
    argIndirOpnd->SetOffset(actualOffset << indirScale);
    LowererMD::CreateAssign(dstOpnd, argIndirOpnd, instrInsert);
    instrInsert->InsertBefore(labelDone);
    instrInsert->Remove();
}


void
Lowerer::GenerateGetCurrentFunctionObject(IR::Instr * instr)
{
    Func * func = this->m_func;
    IR::Instr * insertBeforeInstr = instr->m_next;
    IR::RegOpnd * functionObjectOpnd = instr->GetDst()->AsRegOpnd();
    IR::Opnd * vtableAddressOpnd = this->LoadVTableValueOpnd(insertBeforeInstr, VTableValue::VtableStackScriptFunction);
    IR::LabelInstr * labelDone = IR::LabelInstr::New(Js::OpCode::Label, func, false);
    InsertCompareBranch(IR::IndirOpnd::New(functionObjectOpnd, 0, TyMachPtr, func), vtableAddressOpnd,
        Js::OpCode::BrNeq_A, true, labelDone, insertBeforeInstr);
    IR::RegOpnd * boxedFunctionObjectOpnd = IR::RegOpnd::New(TyMachPtr, func);
    InsertMove(boxedFunctionObjectOpnd, IR::IndirOpnd::New(functionObjectOpnd,
        Js::StackScriptFunction::GetOffsetOfBoxedScriptFunction(), TyMachPtr, func), insertBeforeInstr);
    InsertTestBranch(boxedFunctionObjectOpnd, boxedFunctionObjectOpnd, Js::OpCode::BrEq_A, true, labelDone, insertBeforeInstr);
    InsertMove(functionObjectOpnd, boxedFunctionObjectOpnd, insertBeforeInstr);
    insertBeforeInstr->InsertBefore(labelDone);
}

IR::Opnd *
Lowerer::GetInlineCacheFromFuncObjectForRuntimeUse(IR::Instr * instr, IR::PropertySymOpnd * propSymOpnd, bool isHelper)
{
    // MOV s1, [ebp + 8]                        //s1 = function object
    // MOV s2, [s1 + offset(hasInlineCaches)]
    // TEST s2, s2
    // JE $L1
    // MOV s3, [s1 + offset(m_inlineCaches)]    //s3 = inlineCaches from function object
    // MOV s4, [s3 + index*scale]               //s4 = inlineCaches[index]
    // JMP $L2
    // $L1
    // MOV s3, propSym->m_runtimeCache
    // $L2

    byte indirScale = this->m_lowererMD.GetDefaultIndirScale();

    IR::RegOpnd * funcObjOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::Instr * funcObjInstr = IR::Instr::New(Js::OpCode::Ld_A, funcObjOpnd, instr->m_func);
    instr->InsertBefore(funcObjInstr);
    this->m_lowererMD.LoadFuncExpression(funcObjInstr);

    IR::RegOpnd * funcObjHasInlineCachesOpnd = IR::RegOpnd::New(TyMachPtr, instr->m_func);
    this->m_lowererMD.CreateAssign(funcObjHasInlineCachesOpnd, IR::IndirOpnd::New(funcObjOpnd, Js::ScriptFunction::GetOffsetOfHasInlineCaches(), TyUint8, instr->m_func), instr);

    IR::LabelInstr * inlineCachesNullLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, isHelper);
    InsertTestBranch(funcObjHasInlineCachesOpnd, funcObjHasInlineCachesOpnd, Js::OpCode::BrEq_A, inlineCachesNullLabel, instr);

    IR::RegOpnd * inlineCachesOpnd = IR::RegOpnd::New(TyMachPtr, instr->m_func);
    Lowerer::InsertMove(inlineCachesOpnd, IR::IndirOpnd::New(funcObjOpnd, Js::ScriptFunctionWithInlineCache::GetOffsetOfInlineCaches(), TyMachPtr, instr->m_func), instr);

    IR::RegOpnd * inlineCacheOpnd = IR::RegOpnd::New(TyMachPtr, instr->m_func);
    IR::RegOpnd * indexOpnd = IR::RegOpnd::New(TyMachReg, instr->m_func);
    int inlineCacheOffset;
    if (!Int32Math::Mul(sizeof(Js::InlineCache *), propSymOpnd->m_inlineCacheIndex, &inlineCacheOffset))
    {
        Lowerer::InsertMove(inlineCacheOpnd, IR::IndirOpnd::New(inlineCachesOpnd, inlineCacheOffset, TyMachPtr, instr->m_func), instr);
    }
    else
    {
        Lowerer::InsertMove(indexOpnd, IR::IntConstOpnd::New(propSymOpnd->m_inlineCacheIndex, TyUint32, instr->m_func), instr);
        Lowerer::InsertMove(inlineCacheOpnd, IR::IndirOpnd::New(inlineCachesOpnd, indexOpnd, indirScale, TyMachPtr, instr->m_func), instr);
    }

    IR::LabelInstr * continueLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, isHelper);
    InsertBranch(LowererMD::MDUncondBranchOpcode, continueLabel, instr);

    IR::Instr * ldCacheFromPropSymOpndInstr = this->m_lowererMD.CreateAssign(inlineCacheOpnd, IR::AddrOpnd::New(propSymOpnd->m_runtimeInlineCache, IR::AddrOpndKindDynamicInlineCache, this->m_func), instr);
    ldCacheFromPropSymOpndInstr->InsertBefore(inlineCachesNullLabel);

    ldCacheFromPropSymOpndInstr->InsertAfter(continueLabel);

    return inlineCacheOpnd;
}

IR::Instr *
Lowerer::LowerInitClass(IR::Instr * instr)
{
    // scriptContext
    IR::Instr   * prevInstr = LoadScriptContext(instr);

    // extends
    if (instr->GetSrc2() != nullptr)
    {
        IR::Opnd * extendsOpnd = instr->UnlinkSrc2();
        m_lowererMD.LoadHelperArgument(instr, extendsOpnd);
    }
    else
    {
        IR::AddrOpnd* extendsOpnd = IR::AddrOpnd::NewNull(this->m_func);
        m_lowererMD.LoadHelperArgument(instr, extendsOpnd);
    }

    // constructor
    IR::Opnd * ctorOpnd = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, ctorOpnd);

    // call
    m_lowererMD.ChangeToHelperCall(instr, IR::HelperOP_InitClass);

    return prevInstr;
}

void
Lowerer::LowerNewConcatStrMulti(IR::Instr * instr)
{
    IR::IntConstOpnd * countOpnd = instr->UnlinkSrc1()->AsIntConstOpnd();
    IR::RegOpnd * dstOpnd = instr->UnlinkDst()->AsRegOpnd();
    uint8 count = (uint8)countOpnd->GetValue();

    Assert(dstOpnd->GetValueType().IsString());

    GenerateRecyclerAlloc(IR::HelperAllocMemForConcatStringMulti, Js::ConcatStringMulti::GetAllocSize(count), dstOpnd, instr);

    GenerateRecyclerMemInit(dstOpnd, 0, this->LoadVTableValueOpnd(instr, VTableValue::VtableConcatStringMulti), instr);
    GenerateRecyclerMemInit(dstOpnd, Js::ConcatStringMulti::GetOffsetOfType(),
        this->LoadLibraryValueOpnd(instr, LibraryValue::ValueStringTypeStatic), instr);
    GenerateRecyclerMemInitNull(dstOpnd, Js::ConcatStringMulti::GetOffsetOfpszValue(), instr);
    GenerateRecyclerMemInit(dstOpnd, Js::ConcatStringMulti::GetOffsetOfcharLength(), 0, instr);
    GenerateRecyclerMemInit(dstOpnd, Js::ConcatStringMulti::GetOffsetOfSlotCount(), countOpnd->AsUint32(), instr);

    instr->Remove();
}

void
Lowerer::LowerNewConcatStrMultiBE(IR::Instr * instr)
{
    // Lower
    // t1 = SetConcatStrMultiBE s1
    // t2 = SetConcatStrMultiBE s2, t1
    // t3 = SetConcatStrMultiBE s3, t2
    // s  = NewConcatStrMultiBE 3, t3
    //            to
    // s   = new concat string
    // s+0 = s1
    // s+1 = s2
    // s+2 = s3
    Assert(instr->GetSrc1()->IsConstOpnd());
    Assert(instr->GetDst()->IsRegOpnd());

    IR::RegOpnd * newString = instr->GetDst()->AsRegOpnd();

    IR::Opnd * newConcatItemOpnd = nullptr;
    uint index = instr->GetSrc1()->AsIntConstOpnd()->AsUint32() - 1;
    IR::Instr * concatItemInstr = nullptr;
    IR::Opnd * linkOpnd = instr->GetSrc2();
    while (linkOpnd)
    {
        Assert(linkOpnd->IsRegOpnd());
        concatItemInstr = linkOpnd->GetStackSym()->GetInstrDef();
        Assert(concatItemInstr->m_opcode == Js::OpCode::SetConcatStrMultiItemBE);

        IR::Opnd * concatItemOpnd = concatItemInstr->GetSrc1();
        Assert(concatItemOpnd->IsRegOpnd());

        // If one of the concat items is equal to the dst of the concat expressions (s = s + a + b),
        // hoist the load of that item to before the setting of the new string to the dst.
        if (concatItemOpnd->IsEqual(newString))
        {
            if (!newConcatItemOpnd)
            {
                IR::Instr * hoistSrcInstr = concatItemInstr->HoistSrc1(Js::OpCode::Ld_A);
                newConcatItemOpnd = hoistSrcInstr->GetDst();
            }
            concatItemOpnd = newConcatItemOpnd;
        }
        else
        {
            // If only some of the SetConcatStrMultiItemBE instructions were CSE'd and the rest, along with the NewConcatStrMultiBE
            // instruction, were in a loop, the strings on the CSE'd Set*BE instructions will become live on back edge. Add them to
            // addToLiveOnBackEdgeSyms here and clear when we reach the Set*BE instruction.

            // Note that we are doing this only for string opnds which are not the same as the dst of the concat expression. Reasoning
            // behind this is that if a loop has a concat expression with one of its sources same as the dst, the Set*BE instruction
            // for the dst wouldn't have been CSE'd as the dst's value is changing in the loop and the backward pass should have set the
            // symbol as live on backedge.
            this->addToLiveOnBackEdgeSyms->Set(concatItemOpnd->GetStackSym()->m_id);
        }
        IR::Instr * newConcatItemInstr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItem,
                                                        IR::IndirOpnd::New(newString, index, TyVar, instr->m_func),
                                                        concatItemOpnd,
                                                        instr->m_func);
        instr->InsertAfter(newConcatItemInstr);
        this->LowerSetConcatStrMultiItem(newConcatItemInstr);

        linkOpnd = concatItemInstr->GetSrc2();
        index--;
    }
    Assert(index == -1);
    this->LowerNewConcatStrMulti(instr);
}

void
Lowerer::LowerSetConcatStrMultiItem(IR::Instr * instr)
{
    Func * func = this->m_func;
    IR::IndirOpnd * dstOpnd = instr->GetDst()->AsIndirOpnd();
    IR::RegOpnd * concatStrOpnd = dstOpnd->GetBaseOpnd();
    IR::RegOpnd * srcOpnd = instr->UnlinkSrc1()->AsRegOpnd();

    Assert(concatStrOpnd->GetValueType().IsString());
    Assert(srcOpnd->GetValueType().IsString());
    srcOpnd = GenerateGetImmutableOrScriptUnreferencedString(srcOpnd, instr, IR::HelperOp_CompoundStringCloneForConcat);
    instr->SetSrc1(srcOpnd);

    IR::IndirOpnd * dstLength = IR::IndirOpnd::New(concatStrOpnd, Js::ConcatStringMulti::GetOffsetOfcharLength(), TyUint32, func);
    IR::Opnd * srcLength;

    if (srcOpnd->m_sym->m_isStrConst)
    {
        srcLength = IR::IntConstOpnd::New(Js::JavascriptString::FromVar(srcOpnd->m_sym->GetConstAddress())->GetLength(),
            TyUint32, func);
    }
    else
    {
        srcLength = IR::RegOpnd::New(TyUint32, func);
        InsertMove(srcLength, IR::IndirOpnd::New(srcOpnd, Js::ConcatStringMulti::GetOffsetOfcharLength(), TyUint32, func), instr);
    }
    InsertAdd(false, dstLength, dstLength, srcLength, instr);

    dstOpnd->SetOffset(dstOpnd->GetOffset() * sizeof(Js::JavascriptString *) + Js::ConcatStringMulti::GetOffsetOfSlots());
    this->m_lowererMD.ChangeToAssign(instr);
}

IR::RegOpnd *
Lowerer::GenerateGetImmutableOrScriptUnreferencedString(IR::RegOpnd * strOpnd, IR::Instr * insertBeforeInstr, IR::JnHelperMethod helperMethod, bool reloadDst)
{
    if (strOpnd->m_sym->m_isStrConst)
    {
        return strOpnd;
    }

    Func * const func = this->m_func;
    IR::RegOpnd  *dstOpnd = reloadDst == true ? IR::RegOpnd::New(TyVar, func) : strOpnd;
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, func);

    if (!strOpnd->IsNotTaggedValue())
    {
        this->m_lowererMD.GenerateObjectTest(strOpnd, insertBeforeInstr, doneLabel);
    }
    // CMP [strOpnd], Js::CompoundString::`vtable'
    // JEQ $helper
    InsertCompareBranch(
        IR::IndirOpnd::New(strOpnd, 0, TyMachPtr, func),
        this->LoadVTableValueOpnd(insertBeforeInstr, VTableValue::VtableCompoundString),
        Js::OpCode::BrEq_A,
        helperLabel,
        insertBeforeInstr);

    if (reloadDst)
    {
        InsertMove(dstOpnd, strOpnd, insertBeforeInstr);
    }

    InsertBranch(Js::OpCode::Br, doneLabel, insertBeforeInstr);
    insertBeforeInstr->InsertBefore(helperLabel);

    this->m_lowererMD.LoadHelperArgument(insertBeforeInstr, strOpnd);
    IR::Instr* callInstr = IR::Instr::New(Js::OpCode::Call, dstOpnd, func);
    callInstr->SetSrc1(IR::HelperCallOpnd::New(helperMethod, func));
    insertBeforeInstr->InsertBefore(callInstr);
    this->m_lowererMD.LowerCall(callInstr, 0);

    insertBeforeInstr->InsertBefore(doneLabel);

    return dstOpnd;
}

void
Lowerer::LowerConvStrCommon(IR::JnHelperMethod helper,  IR::Instr * instr)
{
    IR::RegOpnd * src1Opnd = instr->UnlinkSrc1()->AsRegOpnd();
    if (!src1Opnd->GetValueType().IsNotString())
    {
        IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
        IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
        this->GenerateStringTest(src1Opnd, instr, helperLabel);
        InsertMove(instr->GetDst(), src1Opnd, instr);
        InsertBranch(Js::OpCode::Br, doneLabel, instr);
        instr->InsertBefore(helperLabel);
        instr->InsertAfter(doneLabel);
    }
    if (instr->GetSrc2())
    {
        this->m_lowererMD.LoadHelperArgument(instr, instr->UnlinkSrc2());
    }

    this->LoadScriptContext(instr);
    this->m_lowererMD.LoadHelperArgument(instr, src1Opnd);
    this->m_lowererMD.ChangeToHelperCall(instr, helper);
}

void
Lowerer::LowerConvStr(IR::Instr * instr)
{
    LowerConvStrCommon(IR::HelperOp_ConvString, instr);
}

void
Lowerer::LowerCoerseStr(IR::Instr* instr)
{
    LowerConvStrCommon(IR::HelperOp_CoerseString, instr);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerCoerseStrOrRegex - This method is used for String.Replace(arg1, arg2)
/// where arg1 is regex or string
/// if arg1 is not regex, then do String.Replace(CoerseStr(arg1), arg2);
///
///     CoerseStrOrRegex arg1
///
///     if (value == regex) goto :done
///     else
///helper:
///     ConvStr value
///done:
///----------------------------------------------------------------------------
void
Lowerer::LowerCoerseStrOrRegex(IR::Instr* instr)
{
    IR::RegOpnd * src1Opnd = instr->GetSrc1()->AsRegOpnd();

    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);

    // if (value == regex) goto :done
    if (!src1Opnd->IsNotTaggedValue())
    {
        this->m_lowererMD.GenerateObjectTest(src1Opnd, instr, helperLabel);
    }

    IR::Opnd * vtableOpnd = LoadVTableValueOpnd(instr, VTableValue::VtableJavascriptRegExp);

    InsertCompareBranch(IR::IndirOpnd::New(src1Opnd, 0, TyMachPtr, instr->m_func),
        vtableOpnd, Js::OpCode::BrNeq_A, helperLabel, instr);

    InsertMove(instr->GetDst(), src1Opnd, instr);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);
    instr->InsertBefore(helperLabel);

    instr->InsertAfter(doneLabel);

    // helper: ConvStr value
    LowerConvStr(instr);
}

///----------------------------------------------------------------------------
///
/// Lowerer::LowerCoerseRegex - This method is used for String.Match(arg1)
/// if arg1 is regex, then pass CreateRegEx(arg1) to String.Match
///
///----------------------------------------------------------------------------
void
Lowerer::LowerCoerseRegex(IR::Instr* instr)
{
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    IR::RegOpnd * src1Opnd = instr->UnlinkSrc1()->AsRegOpnd();
    if (!src1Opnd->IsNotTaggedValue())
    {
        this->m_lowererMD.GenerateObjectTest(src1Opnd, instr, helperLabel);
    }

    IR::Opnd * vtableOpnd = LoadVTableValueOpnd(instr, VTableValue::VtableJavascriptRegExp);

    InsertCompareBranch(IR::IndirOpnd::New(src1Opnd, 0, TyMachPtr, instr->m_func),
        vtableOpnd, Js::OpCode::BrNeq_A, helperLabel, instr);

    InsertMove(instr->GetDst(), src1Opnd, instr);
    InsertBranch(Js::OpCode::Br, doneLabel, instr);
    instr->InsertBefore(helperLabel);
    instr->InsertAfter(doneLabel);

    this->LoadScriptContext(instr);
    this->m_lowererMD.LoadHelperArgument(instr, IR::AddrOpnd::NewNull(instr->m_func));   // option
    this->m_lowererMD.LoadHelperArgument(instr, src1Opnd); // regex
    this->m_lowererMD.ChangeToHelperCall(instr, IR::HelperOp_CoerseRegex);
}

void
Lowerer::LowerConvPrimStr(IR::Instr * instr)
{
    LowerConvStrCommon(IR::HelperOp_ConvPrimitiveString, instr);
}

void
Lowerer::GenerateRecyclerAlloc(IR::JnHelperMethod allocHelper, size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, bool inOpHelper)
{
    size_t alignedSize = HeapInfo::GetAlignedSizeNoCheck(allocSize);
    this->GenerateRecyclerAllocAligned(allocHelper, alignedSize, newObjDst, insertionPointInstr, inOpHelper);
}

void
Lowerer::GenerateMemInit(IR::RegOpnd * opnd, int32 offset, int value, IR::Instr * insertBeforeInstr, bool isZeroed)
{
    IRType type = TyInt32;
    if (isZeroed)
    {
        if (value == 0)
        {
            // Recycler memory are zero initialized
            return;
        }

        if (value > 0 && value <= USHORT_MAX)
        {
            // Recycler memory are zero initialized, so we can just initialize the 8 or 16 bits of value
            type = (value <= UCHAR_MAX)? TyUint8 : TyUint16;
        }
    }
    Func * func = this->m_func;
    InsertMove(IR::IndirOpnd::New(opnd, offset, type, func), IR::IntConstOpnd::New(value, type, func), insertBeforeInstr);
}

void
Lowerer::GenerateMemInit(IR::RegOpnd * opnd, int32 offset, uint32 value, IR::Instr * insertBeforeInstr, bool isZeroed)
{
    IRType type = TyUint32;
    if (isZeroed)
    {
        if (value == 0)
        {
            // Recycler memory are zero initialized
            return;
        }

        if (value <= USHORT_MAX)
        {
            // Recycler memory are zero initialized, so we can just initialize the 8 or 16 bits of value
            type = (value <= UCHAR_MAX)? TyUint8 : TyUint16;
        }
    }

    Func * func = this->m_func;
    InsertMove(IR::IndirOpnd::New(opnd, offset, type, func), IR::IntConstOpnd::New(value, type, func), insertBeforeInstr);
}

void
Lowerer::GenerateMemInitNull(IR::RegOpnd * opnd, int32 offset, IR::Instr * insertBeforeInstr, bool isZeroed)
{
    if (isZeroed)
    {
        return;
    }
    GenerateMemInit(opnd, offset, IR::AddrOpnd::NewNull(m_func), insertBeforeInstr);
}

void
Lowerer::GenerateMemInit(IR::RegOpnd * opnd, int32 offset, IR::Opnd * value, IR::Instr * insertBeforeInstr, bool isZeroed)
{
    IRType type = value->GetType();

    Func * func = this->m_func;
    InsertMove(IR::IndirOpnd::New(opnd, offset, type, func), value, insertBeforeInstr);
}

void
Lowerer::GenerateRecyclerMemInit(IR::RegOpnd * opnd, int32 offset, int32 value, IR::Instr * insertBeforeInstr)
{
    GenerateMemInit(opnd, offset, value, insertBeforeInstr, true);
}

void
Lowerer::GenerateRecyclerMemInit(IR::RegOpnd * opnd, int32 offset, uint32 value, IR::Instr * insertBeforeInstr)
{
    GenerateMemInit(opnd, offset, value, insertBeforeInstr, true);
}

void
Lowerer::GenerateRecyclerMemInitNull(IR::RegOpnd * opnd, int32 offset, IR::Instr * insertBeforeInstr)
{
    GenerateMemInitNull(opnd, offset, insertBeforeInstr, true);
}

void
Lowerer::GenerateRecyclerMemInit(IR::RegOpnd * opnd, int32 offset, IR::Opnd * value, IR::Instr * insertBeforeInstr)
{
    GenerateMemInit(opnd, offset, value, insertBeforeInstr, true);
}

void
Lowerer::GenerateMemCopy(IR::Opnd * dst, IR::Opnd * src, uint32 size, IR::Instr * insertBeforeInstr)
{
    Func * func = this->m_func;
    this->m_lowererMD.LoadHelperArgument(insertBeforeInstr, IR::IntConstOpnd::New(size, TyUint32, func));
    this->m_lowererMD.LoadHelperArgument(insertBeforeInstr, src);
    this->m_lowererMD.LoadHelperArgument(insertBeforeInstr, dst);
    IR::Instr * memcpyInstr = IR::Instr::New(Js::OpCode::Call, func);
    memcpyInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperMemCpy, func));
    insertBeforeInstr->InsertBefore(memcpyInstr);
    m_lowererMD.LowerCall(memcpyInstr, 3);
}

bool
Lowerer::GenerateSimplifiedInt4Rem(
    IR::Instr *const remInstr,
    IR::LabelInstr *const skipBailOutLabel) const
{
    Assert(remInstr);
    Assert(remInstr->m_opcode == Js::OpCode::Rem_I4);

    auto *dst = remInstr->GetDst(), *src1 = remInstr->GetSrc1(), *src2 = remInstr->GetSrc2();

    Assert(src1 && src2);
    Assert(dst->IsRegOpnd());

    bool isModByPowerOf2 = (remInstr->HasBailOutInfo() && remInstr->GetBailOutKind() == IR::BailOnModByPowerOf2);

    if (PHASE_OFF(Js::Phase::MathFastPathPhase, remInstr->m_func->GetTopFunc()) && !isModByPowerOf2)
        return false;

    if (!(src2->IsIntConstOpnd() && Math::IsPow2(src2->AsIntConstOpnd()->AsInt32())) && !isModByPowerOf2)
    {
        return false;
    }
    // We have:
    //     s3 = s1 % s2 , where s2 = +2^i
    //
    // Generate:
    //     test s1, s1
    //     js   $slowPathLabel
    //     s3 = and s1, 0x00..fff  (2^i - 1)
    //     jmp  $doneLabel
    //   $slowPathLabel:
    //     (Slow path)
    //     (Neg zero check)
    //     (Bailout code)
    //   $doneLabel:

    IR::LabelInstr *doneLabel = skipBailOutLabel, *slowPathLabel;

    if (!doneLabel)
    {
        doneLabel = IR::LabelInstr::New(Js::OpCode::Label, remInstr->m_func);
        remInstr->InsertAfter(doneLabel);
    }
    slowPathLabel = IR::LabelInstr::New(Js::OpCode::Label, remInstr->m_func, isModByPowerOf2);
    remInstr->InsertBefore(slowPathLabel);

    // test s1, s1
    InsertTest(src1, src1, slowPathLabel);

    // jsb $slowPathLabel
    InsertBranch(LowererMD::MDCompareWithZeroBranchOpcode(Js::OpCode::BrLt_A), slowPathLabel, slowPathLabel);

    // s3 = and s1, 0x00..fff (2^i - 1)
    IR::Opnd* maskOpnd;

    if(isModByPowerOf2)
    {
        Assert(isModByPowerOf2);
        maskOpnd = IR::RegOpnd::New(TyInt32, remInstr->m_func);

        // mov maskOpnd, s2
        InsertMove(maskOpnd, src2, slowPathLabel);

        // dec maskOpnd
        InsertSub(/*needFlags*/ true, maskOpnd, maskOpnd, IR::IntConstOpnd::New(1, TyInt32, this->m_func, /*dontEncode*/true), slowPathLabel);

        // maskOpnd < 0 goto $slowPath
        InsertBranch(LowererMD::MDCompareWithZeroBranchOpcode(Js::OpCode::BrLt_A), slowPathLabel, slowPathLabel);

        // TEST src2, maskOpnd
        InsertTestBranch(src2, maskOpnd, Js::OpCode::BrNeq_A, slowPathLabel, slowPathLabel);
    }
    else
    {
        Assert(src2->IsIntConstOpnd());
        int32 mask =  src2->AsIntConstOpnd()->AsInt32() - 1;
        maskOpnd = IR::IntConstOpnd::New(mask, TyInt32, remInstr->m_func);
    }

    // dst = src1 & maskOpnd
    InsertAnd(dst, src1, maskOpnd, slowPathLabel);

    // jmp $doneLabel
    InsertBranch(Js::OpCode::Br, doneLabel, slowPathLabel);
    return true;
}


#if DBG
bool
Lowerer::ValidOpcodeAfterLower(IR::Instr* instr, Func * func)
{
    Js::OpCode opcode = instr->m_opcode;
    if (opcode > Js::OpCode::MDStart)
    {
        return true;
    }
    switch (opcode)
    {
    case Js::OpCode::Label:
    case Js::OpCode::StatementBoundary:
    case Js::OpCode::DeletedNonHelperBranch:
    case Js::OpCode::FunctionEntry:
    case Js::OpCode::FunctionExit:
    case Js::OpCode::TryCatch:
    case Js::OpCode::TryFinally:
    case Js::OpCode::Catch:
    case Js::OpCode::GeneratorResumeJumpTable:

    case Js::OpCode::Break:

#ifdef _M_X64
    case Js::OpCode::PrologStart:
    case Js::OpCode::PrologEnd:
#endif
#ifdef _M_IX86
    case Js::OpCode::BailOutStackRestore:
#endif
        return true;

    case Js::OpCode::RestoreOutParam:
        Assert(func->isPostRegAlloc);
        return true;

    // These may be removed by peep
    case Js::OpCode::StartCall:
    case Js::OpCode::LoweredStartCall:
    case Js::OpCode::Nop:
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
        return func && !func->isPostPeeps;

    case Js::OpCode::InlineeStart:
    case Js::OpCode::InlineeEnd:
        return instr->m_func->m_hasInlineArgsOpt;
#ifdef _M_X64
    case Js::OpCode::LdArgSize:
    case Js::OpCode::LdSpillSize:
        return func && !func->isPostFinalLower;
#endif

    case Js::OpCode::Leave:
        Assert(!func->IsLoopBodyInTry());
        Assert(func->HasTry() && func->DoOptimizeTryCatch());
        return func && !func->isPostFinalLower; //Lowered in FinalLower phase
    };

    return false;

}
#endif

void Lowerer::LowerProfiledBeginSwitch(IR::JitProfilingInstr* instr)
{
    Assert(instr->isBeginSwitch);

    m_lowererMD.LoadHelperArgument(instr, instr->UnlinkSrc1());
    m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(instr->profileId, m_func));
    m_lowererMD.LoadHelperArgument(instr, CreateFunctionBodyOpnd(instr->m_func));
    instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperSimpleProfiledSwitch, m_func));
    m_lowererMD.LowerCall(instr, 0);
}

void Lowerer::LowerProfiledBinaryOp(IR::JitProfilingInstr* instr, IR::JnHelperMethod meth)
{
    m_lowererMD.LoadHelperArgument(instr, instr->UnlinkSrc2());
    m_lowererMD.LoadHelperArgument(instr, instr->UnlinkSrc1());
    m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(instr->profileId, m_func));
    m_lowererMD.LoadHelperArgument(instr, CreateFunctionBodyOpnd(instr->m_func));
    instr->SetSrc1(IR::HelperCallOpnd::New(meth, m_func));
    m_lowererMD.LowerCall(instr, 0);
}

void Lowerer::GenerateNullOutGeneratorFrame(IR::Instr* insertInstr)
{
    // null out frame pointer on generator object to signal completion to JavascriptGenerator::CallGenerator
    // s = MOV prm1
    // s[offset of JavascriptGenerator::frame] = MOV nullptr
    StackSym *symSrc = StackSym::NewParamSlotSym(1, m_func);
    m_func->SetArgOffset(symSrc, LowererMD::GetFormalParamOffset() * MachPtr);
    IR::SymOpnd *srcOpnd = IR::SymOpnd::New(symSrc, TyMachPtr, m_func);
    IR::RegOpnd *dstOpnd = IR::RegOpnd::New(TyMachReg, m_func);
    m_lowererMD.CreateAssign(dstOpnd, srcOpnd, insertInstr);

    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(dstOpnd, Js::JavascriptGenerator::GetFrameOffset(), TyMachPtr, m_func);
    IR::AddrOpnd *addrOpnd = IR::AddrOpnd::NewNull(m_func);
    m_lowererMD.CreateAssign(indirOpnd, addrOpnd, insertInstr);
}

void Lowerer::LowerFunctionExit(IR::Instr* funcExit)
{
    if (m_func->GetJnFunction()->IsGenerator())
    {
        GenerateNullOutGeneratorFrame(funcExit->m_prev);
    }

    if (!m_func->DoSimpleJitDynamicProfile())
    {
        return;
    }

    IR::Instr* callInstr = IR::Instr::New(Js::OpCode::Call, m_func);
    callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperSimpleCleanImplicitCallFlags, m_func));
    funcExit->m_prev->InsertBefore(callInstr);

    m_lowererMD.LoadHelperArgument(callInstr, CreateFunctionBodyOpnd(funcExit->m_func));
    m_lowererMD.LowerCall(callInstr, 0);
}

void Lowerer::LowerFunctionEntry(IR::Instr* funcEntry)
{
    Assert(funcEntry->m_opcode == Js::OpCode::FunctionEntry);

    //Don't do a body call increment for loops or asm.js
    if (m_func->IsLoopBody() || m_func->GetJnFunction()->GetIsAsmjsMode())
    {
        return;
    }

    IR::Instr *const insertBeforeInstr = this->m_func->GetFunctionEntryInsertionPoint();

    LowerFunctionBodyCallCountChange(insertBeforeInstr);

    if (m_func->DoSimpleJitDynamicProfile())
    {
        const auto jn = m_func->GetJnFunction();
        // Only generate the argument profiling if the function expects to have some arguments to profile and only if
        //    it has implicit ArgIns (the latter is a restriction imposed by the Interpreter, so it is mirrored in SimpleJit)

        if (jn->GetInParamsCount() > 1 && jn->GetHasImplicitArgIns())
        {
            // Call out to the argument profiling helper
            IR::Instr* callInstr = IR::Instr::New(Js::OpCode::Call, m_func);
            callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperSimpleProfileParameters, m_func));
            insertBeforeInstr->InsertBefore(callInstr);
            m_lowererMD.LoadHelperArgument(callInstr, IR::Opnd::CreateFramePointerOpnd(m_func));
            m_lowererMD.LowerCall(callInstr, 0);
        }

        // Clear existing ImplicitCallFlags
        const auto starFlag = GetImplicitCallFlagsOpnd();
        this->InsertMove(starFlag, CreateClearImplicitCallFlagsOpnd(), insertBeforeInstr);
    }
}

void Lowerer::LowerFunctionBodyCallCountChange(IR::Instr *const insertBeforeInstr)
{
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;
    const bool isSimpleJit = func->IsSimpleJit();

    if ((isSimpleJit && !func->GetTopFunc()->GetJnFunction()->DoFullJit()))
    {
        return;
    }

    // mov countAddress, <countAddress>
    IR::RegOpnd *const countAddressOpnd = IR::RegOpnd::New(StackSym::New(TyMachPtr, func), TyMachPtr, func);
    const IR::AutoReuseOpnd autoReuseCountAddressOpnd(countAddressOpnd, func);
    InsertMove(
        countAddressOpnd,
        IR::AddrOpnd::New(func->GetCallsCountAddress(), IR::AddrOpndKindDynamicMisc, func, true),
        insertBeforeInstr);

    IR::IndirOpnd *const countOpnd = IR::IndirOpnd::New(countAddressOpnd, 0, TyUint8, func);
    const IR::AutoReuseOpnd autoReuseCountOpnd(countOpnd, func);
    if(!isSimpleJit)
    {
        // InsertIncUint8PreventOverflow [countAddress]
        InsertIncUInt8PreventOverflow(countOpnd, countOpnd, insertBeforeInstr);
        return;
    }

    // InsertDecUint8PreventOverflow [countAddress]
    IR::Instr *onOverflowInsertBeforeInstr;
    InsertDecUInt8PreventOverflow(
        countOpnd,
        countOpnd,
        insertBeforeInstr,
        &onOverflowInsertBeforeInstr);

    //   ($overflow:)
    //     TransitionFromSimpleJit(framePointer)
    m_lowererMD.LoadHelperArgument(onOverflowInsertBeforeInstr, IR::Opnd::CreateFramePointerOpnd(func));
    IR::Instr *const callInstr = IR::Instr::New(Js::OpCode::Call, func);
    callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperTransitionFromSimpleJit, func));
    onOverflowInsertBeforeInstr->InsertBefore(callInstr);
    m_lowererMD.LowerCall(callInstr, 0);
}

IR::Opnd*
Lowerer::GetImplicitCallFlagsOpnd()
{
    return GetImplicitCallFlagsOpnd(m_func);
}

IR::Opnd*
Lowerer::GetImplicitCallFlagsOpnd(Func * func)
{
    return IR::MemRefOpnd::New(func->GetScriptContext()->GetThreadContext()->GetAddressOfImplicitCallFlags(), GetImplicitCallFlagsType(), func);
}

IR::Opnd*
Lowerer::CreateClearImplicitCallFlagsOpnd()
{
    return IR::IntConstOpnd::New(Js::ImplicitCall_None, GetImplicitCallFlagsType(), m_func);
}

void
Lowerer::LowerSpreadArrayLiteral(IR::Instr *instr)
{
    LoadScriptContext(instr);

    IR::Opnd *src2Opnd = instr->UnlinkSrc2();
    m_lowererMD.LoadHelperArgument(instr, src2Opnd);

    IR::Opnd *src1Opnd = instr->UnlinkSrc1();
    m_lowererMD.LoadHelperArgument(instr, src1Opnd);

    this->m_lowererMD.ChangeToHelperCall(instr, IR::HelperSpreadArrayLiteral);
}

IR::Instr *
Lowerer::LowerSpreadCall(IR::Instr *instr, Js::CallFlags callFlags, bool setupProfiledVersion)
{
    // Get the target function object, and emit function object test.
    IR::RegOpnd * functionObjOpnd = instr->UnlinkSrc1()->AsRegOpnd();
    functionObjOpnd->m_isCallArg = true;

    if (!(callFlags & Js::CallFlags_New) && !setupProfiledVersion)
    {
        IR::LabelInstr* continueAfterExLabel = InsertContinueAfterExceptionLabelForDebugger(m_func, instr, false);
        this->m_lowererMD.GenerateFunctionObjectTest(instr, functionObjOpnd, false, continueAfterExLabel);
    }

    IR::Instr *spreadIndicesInstr;

    spreadIndicesInstr = GetLdSpreadIndicesInstr(instr);
    Assert(spreadIndicesInstr->m_opcode == Js::OpCode::LdSpreadIndices);

    // Get AuxArray
    IR::Opnd *spreadIndicesOpnd = spreadIndicesInstr->UnlinkSrc1();
    // Remove LdSpreadIndices from the argument chain
    instr->ReplaceSrc2(spreadIndicesInstr->UnlinkSrc2());

    // Emit the normal args
    callFlags = (Js::CallFlags)(callFlags | (instr->GetDst() ? Js::CallFlags_Value : Js::CallFlags_NotUsed));

    // Profiled helper call requires three more parameters, ArrayProfileId, profileId, and the frame pointer.
    // This is just following the convention of HelperProfiledNewScObjArray call.
    const unsigned short extraArgsCount = setupProfiledVersion ? 5 : 2; // function object and AuxArray
    int32 argCount = this->m_lowererMD.LowerCallArgs(instr, (ushort)callFlags, extraArgsCount);

    // Emit our extra (first) args for the Spread helper in reverse order
    if (setupProfiledVersion)
    {
        IR::JitProfilingInstr* jitInstr = (IR::JitProfilingInstr*)instr;
        m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(jitInstr->arrayProfileId, m_func));
        m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateProfileIdOpnd(jitInstr->profileId, m_func));
        m_lowererMD.LoadHelperArgument(instr, IR::Opnd::CreateFramePointerOpnd(m_func));
    }

    m_lowererMD.LoadHelperArgument(instr, functionObjOpnd);
    m_lowererMD.LoadHelperArgument(instr, spreadIndicesOpnd);

    // Change the call target to our helper
    IR::HelperCallOpnd *helperOpnd = IR::HelperCallOpnd::New(setupProfiledVersion ? IR::HelperProfiledNewScObjArraySpread : IR::HelperSpreadCall, this->m_func);
    instr->SetSrc1(helperOpnd);

    return this->m_lowererMD.LowerCall(instr, (Js::ArgSlot)argCount);
}

void
Lowerer::LowerDivI4Common(IR::Instr * instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Rem_I4 || instr->m_opcode == Js::OpCode::Div_I4);
    Assert(m_func->GetJnFunction()->GetIsAsmjsMode());

    // MIN_INT/-1 path is only needed for signed operations

    //       TEST src2, src2
    //       JEQ $div0
    //       CMP src1, MIN_INT
    //       JEQ $minInt
    //       JMP $div
    // $div0: [helper]
    //       MOV dst, 0
    //       JMP $done
    // $minInt: [helper]
    //       CMP src2, -1
    //       JNE $div
    // dst = MOV src1 / 0
    //       JMP $done
    // $div:
    // dst = IDIV src2, src1
    // $done:


    IR::LabelInstr * div0Label = InsertLabel(true, instr);
    IR::LabelInstr * divLabel = InsertLabel(false, instr);
    IR::LabelInstr * doneLabel = InsertLabel(false, instr->m_next);

    InsertTestBranch(instr->GetSrc2(), instr->GetSrc2(), Js::OpCode::BrEq_A, div0Label, div0Label);

    InsertMove(instr->GetDst(), IR::IntConstOpnd::New(0, TyInt32, m_func), divLabel);
    InsertBranch(Js::OpCode::Br, doneLabel, divLabel);

    if (instr->GetSrc1()->GetType() == TyInt32)
    {
        IR::LabelInstr * minIntLabel = nullptr;
        // we need to check for INT_MIN/-1 if divisor is either -1 or variable, and dividend is either INT_MIN or variable
        bool needsMinOverNeg1Check = !(instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() != -1);
        if (instr->GetSrc1()->IsIntConstOpnd())
        {
            if (needsMinOverNeg1Check && instr->GetSrc1()->AsIntConstOpnd()->GetValue() == INT_MIN)
            {
                minIntLabel = InsertLabel(true, divLabel);
                InsertBranch(Js::OpCode::Br, minIntLabel, div0Label);
            }
            else
            {
                needsMinOverNeg1Check = false;
            }
        }
        else if(needsMinOverNeg1Check)
        {
            minIntLabel = InsertLabel(true, divLabel);
            InsertCompareBranch(instr->GetSrc1(), IR::IntConstOpnd::New(INT_MIN, TyInt32, m_func), Js::OpCode::BrEq_A, minIntLabel, div0Label);
        }
        if (needsMinOverNeg1Check)
        {
            Assert(minIntLabel);
            Assert(!instr->GetSrc2()->IsIntConstOpnd() || instr->GetSrc2()->AsIntConstOpnd()->GetValue() == -1);
            if (!instr->GetSrc2()->IsIntConstOpnd())
            {
                InsertCompareBranch(instr->GetSrc2(), IR::IntConstOpnd::New(-1, TyInt32, m_func), Js::OpCode::BrNeq_A, divLabel, divLabel);
            }
            InsertMove(instr->GetDst(), instr->m_opcode == Js::OpCode::Div_I4 ? instr->GetSrc1() : IR::IntConstOpnd::New(0, TyInt32, m_func), divLabel);
            InsertBranch(Js::OpCode::Br, doneLabel, divLabel);
        }
    }
    InsertBranch(Js::OpCode::Br, divLabel, div0Label);

    m_lowererMD.EmitInt4Instr(instr);
}

void
Lowerer::LowerRemI4(IR::Instr * instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Rem_I4);
    if (m_func->GetJnFunction()->GetIsAsmjsMode())
    {
        LowerDivI4Common(instr);
    }
    else
    {
        m_lowererMD.EmitInt4Instr(instr);
    }
}

void
Lowerer::LowerDivI4(IR::Instr * instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Div_I4);

    if (m_func->GetJnFunction()->GetIsAsmjsMode())
    {
        LowerDivI4Common(instr);
        return;
    }

    if(!instr->HasBailOutInfo())
    {
        m_lowererMD.EmitInt4Instr(instr);
        return;
    }

    Assert(!(instr->GetBailOutKind() & ~(IR::BailOnDivResultNotInt | IR::BailOutOnNegativeZero | IR::BailOutOnDivByZero | IR::BailOutOnDivOfMinInt)));

    IR::BailOutKind bailOutKind = instr->GetBailOutKind();

    // Split out and generate the bailout instruction
    const auto nonBailOutInstr = IR::Instr::New(instr->m_opcode, instr->m_func);
    instr->TransferTo(nonBailOutInstr);
    instr->InsertBefore(nonBailOutInstr);

    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func);
    instr->InsertAfter(doneLabel);

    // Generate the bailout helper call. 'instr' will be changed to the CALL into the bailout function, so it can't be used for
    // ordering instructions anymore.
    IR::LabelInstr * bailOutLabel = GenerateBailOut(instr);

    IR::Opnd * denominatorOpnd = nonBailOutInstr->GetSrc2();
    IR::Opnd * nominatorOpnd = nonBailOutInstr->GetSrc1();

    if (bailOutKind & IR::BailOutOnDivOfMinInt)
    {
        // Bailout if numerator is MIN_INT  (could also check for denominator being -1
        // before bailing out, but does not seem worth the extra code..)
        InsertCompareBranch(nominatorOpnd, IR::IntConstOpnd::New(INT32_MIN, TyInt32, this->m_func, true), Js::OpCode::BrEq_A, bailOutLabel, nonBailOutInstr);
    }

    if (denominatorOpnd->IsIntConstOpnd() && Math::IsPow2(denominatorOpnd->AsIntConstOpnd()->AsInt32()))
    {
        Assert((bailOutKind & (IR::BailOutOnNegativeZero | IR::BailOutOnDivByZero)) == 0);
        int pow2 = denominatorOpnd->AsIntConstOpnd()->AsInt32();
        InsertTestBranch(nominatorOpnd, IR::IntConstOpnd::New(pow2 - 1, TyInt32, this->m_func, true),
            Js::OpCode::BrNeq_A, bailOutLabel, nonBailOutInstr);
        nonBailOutInstr->m_opcode = Js::OpCode::Shr_A;
        nonBailOutInstr->ReplaceSrc2(IR::IntConstOpnd::New(Math::Log2(pow2), TyInt32, this->m_func, true));
        LowererMD::ChangeToShift(nonBailOutInstr, false);
        LowererMD::Legalize(nonBailOutInstr);
    }
    else
    {
        if (bailOutKind & IR::BailOutOnDivByZero)
        {
            // Bailout if denominator is 0
            InsertTestBranch(denominatorOpnd, denominatorOpnd, Js::OpCode::BrEq_A, bailOutLabel, nonBailOutInstr);
        }

        // Lower the div and bailout if there is a reminder (machine specific)
        IR::Instr * insertBeforeInstr = m_lowererMD.LowerDivI4AndBailOnReminder(nonBailOutInstr, bailOutLabel);

        IR::Opnd * resultOpnd = nonBailOutInstr->GetDst();
        if (bailOutKind & IR::BailOutOnNegativeZero)
        {
            //      TEST result, result
            //      JNE skipNegDenominatorCheckLabel         // Result not 0
            //      TEST denominator, denominator
            //      JNSB/BMI bailout                         // bail if negative
            // skipNegDenominatorCheckLabel:

            IR::LabelInstr * skipNegDenominatorCheckLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
            // Skip negative denominator check if the result is not 0
            InsertTestBranch(resultOpnd, resultOpnd, Js::OpCode::BrNeq_A, skipNegDenominatorCheckLabel, insertBeforeInstr);

            IR::LabelInstr * negDenominatorCheckLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
            insertBeforeInstr->InsertBefore(negDenominatorCheckLabel);
            // Jump to done if the denominator is not negative
            InsertTestBranch(denominatorOpnd, denominatorOpnd,
                LowererMD::MDCompareWithZeroBranchOpcode(Js::OpCode::BrLt_A), bailOutLabel, insertBeforeInstr);
            insertBeforeInstr->InsertBefore(skipNegDenominatorCheckLabel);
        }
    }

    // We are all fine, jump around the bailout to done
    InsertBranch(Js::OpCode::Br, doneLabel, bailOutLabel);
}

void
Lowerer::LowerRemR8(IR::Instr * instr)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Rem_A);
    Assert(m_func->GetJnFunction()->GetIsAsmjsMode());

    m_lowererMD.LoadDoubleHelperArgument(instr, instr->UnlinkSrc2());
    m_lowererMD.LoadDoubleHelperArgument(instr, instr->UnlinkSrc1());
    instr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperOp_Rem_Double, m_func));
    m_lowererMD.LowerCall(instr, 0);
}

void
Lowerer::LowerNewScopeSlots(IR::Instr * instr, bool doStackSlots)
{
    Func * func = m_func;
    if (PHASE_OFF(Js::NewScopeSlotFastPathPhase, func))
    {
        this->LowerUnaryHelperMemWithFuncBody(instr, IR::HelperOP_NewScopeSlots);
        return;
    }

    uint const count = instr->GetSrc1()->AsIntConstOpnd()->AsUint32();
    uint const allocSize = count * sizeof(Js::Var);
    uint const actualSlotCount = count - Js::ScopeSlots::FirstSlotIndex;

    IR::RegOpnd * dst = instr->UnlinkDst()->AsRegOpnd();

    // dst = RecyclerAlloc(allocSize)
    // dst[EncodedSlotCountSlotIndex = EncodedSlotCountSlotIOndex];
    // dst[ScopeMetadataSlotIndex] = FunctionBody;
    // mov undefinedOpnd, undefined
    // dst[FirstSlotIndex..count] = undefinedOpnd;

    // Note: stack allocation of both scope slots and frame display are done together
    // in lowering of NewStackFrameDisplay
    if (!doStackSlots)
    {
        GenerateRecyclerAlloc(IR::HelperAllocMemForVarArray, allocSize, dst, instr);
    }
    GenerateMemInit(dst, Js::ScopeSlots::EncodedSlotCountSlotIndex * sizeof(Js::Var),
        min<uint>(actualSlotCount, Js::ScopeSlots::MaxEncodedSlotCount), instr, !doStackSlots);
    IR::Opnd * functionBodyOpnd = this->LoadFunctionBodyOpnd(instr);
    GenerateMemInit(dst, Js::ScopeSlots::ScopeMetadataSlotIndex * sizeof(Js::Var),
        functionBodyOpnd, instr, !doStackSlots);

    IR::Opnd * undefinedOpnd = this->LoadLibraryValueOpnd(instr, LibraryValue::ValueUndefined);
    const IR::AutoReuseOpnd autoReuseUndefinedOpnd(undefinedOpnd, func);

    // avoid using a register for the undefined pointer if we are going to assign 1 or 2

    if (actualSlotCount > 2 && !undefinedOpnd->IsRegOpnd())
    {

        // mov undefinedOpnd, undefined
        IR::RegOpnd * regOpnd = IR::RegOpnd::New(TyVar, func);
        InsertMove(regOpnd, undefinedOpnd, instr);
        undefinedOpnd = regOpnd;
    }

    int const loopUnrollCount = 8;

    if (actualSlotCount <= loopUnrollCount * 2)
    {
        // Just generate all the assignment in straight line code
        //  mov[dst + Js::FirstSlotIndex], undefinedOpnd
        // ...
        //  mov[dst + count - 1], undefinedOpnd
        for (unsigned int i = Js::ScopeSlots::FirstSlotIndex; i < count; i++)
        {
            GenerateMemInit(dst, sizeof(Js::Var) * i, undefinedOpnd, instr, !doStackSlots);
        }
    }
    else
    {
        // Just generate all the assignment in loop of loopUnroolCount and the rest as straight line code
        //
        //      lea currOpnd, [dst + sizeof(Var) * (loopAssignCount + Js::ScopeSlots::FirstSlotIndex - loopUnrollCount)];
        //      mov [currOpnd + loopUnrollCount + leftOverAssignCount - 1] , undefinedOpnd
        //      mov [currOpnd + loopUnrollCount + leftOverAssignCount - 2] , undefinedOpnd
        //      ...
        //      mov [currOpnd + loopUnrollCount], undefinedOpnd
        // $LoopTop:
        //      mov [currOpnd + loopUnrollCount - 1], undefinedOpnd
        //      mov [currOpnd + loopUnrollCount - 2], undefinedOpnd
        //      ...
        //      mov [currOpnd], undefinedOpnd
        //      lea currOpnd, [currOpnd - loopUnrollCount]
        //      cmp dst, currOpnd
        //      jlt $Looptop

        uint nLoop = actualSlotCount / loopUnrollCount;
        uint loopAssignCount = nLoop * loopUnrollCount;
        uint leftOverAssignCount = actualSlotCount - loopAssignCount;        // The left over assignments

        IR::RegOpnd * currOpnd = IR::RegOpnd::New(TyMachPtr, func);
        const IR::AutoReuseOpnd autoReuseCurrOpnd(currOpnd, m_func);
        InsertLea(
            currOpnd,
            IR::IndirOpnd::New(
                dst,
                sizeof(Js::Var) * (loopAssignCount + Js::ScopeSlots::FirstSlotIndex - loopUnrollCount),
                TyMachPtr,
                func),
            instr);

        for (unsigned int i = 0; i < leftOverAssignCount; i++)
        {
            GenerateMemInit(currOpnd, sizeof(Js::Var) * (loopUnrollCount + leftOverAssignCount - i - 1), undefinedOpnd, instr, !doStackSlots);
        }

        IR::LabelInstr * loopTop = IR::LabelInstr::New(Js::OpCode::Label, func);
        instr->InsertBefore(loopTop);
        loopTop->m_isLoopTop = true;
        Loop *loop = JitAnew(func->m_alloc, Loop, func->m_alloc, this->m_func);
        loopTop->SetLoop(loop);
        loop->SetLoopTopInstr(loopTop);
        loop->regAlloc.liveOnBackEdgeSyms = JitAnew(func->m_alloc, BVSparse<JitArenaAllocator>, func->m_alloc);

        for (unsigned int i = 0; i < loopUnrollCount; i++)
        {
            GenerateMemInit(currOpnd, sizeof(Js::Var) * (loopUnrollCount - i - 1), undefinedOpnd, instr, !doStackSlots);
        }
        InsertLea(currOpnd, IR::IndirOpnd::New(currOpnd, -((int)sizeof(Js::Var) * loopUnrollCount), TyMachPtr, func), instr);

        InsertCompareBranch(dst, currOpnd, Js::OpCode::BrLt_A, true, loopTop, instr);

        loop->regAlloc.liveOnBackEdgeSyms->Set(currOpnd->m_sym->m_id);
        loop->regAlloc.liveOnBackEdgeSyms->Set(dst->m_sym->m_id);
        loop->regAlloc.liveOnBackEdgeSyms->Set(undefinedOpnd->AsRegOpnd()->m_sym->m_id);
    }

    if (!doStackSlots)
    {
        InsertMove(IR::RegOpnd::New(instr->m_func->GetLocalClosureSym(), TyMachPtr, func), dst, instr);
    }
    instr->Remove();
}

void Lowerer::LowerLdInnerFrameDisplay(IR::Instr *instr)
{
    bool isStrict = instr->m_func->GetJnFunction()->GetIsStrictMode();
    if (isStrict)
    {
        if (instr->GetSrc2())
        {
            this->LowerBinaryHelperMem(instr, IR::HelperScrObj_LdStrictInnerFrameDisplay);
        }
        else
        {
#if DBG
            instr->m_opcode = Js::OpCode::LdInnerFrameDisplayNoParent;
#endif
            this->LowerUnaryHelperMem(instr, IR::HelperScrObj_LdStrictInnerFrameDisplayNoParent);
        }
    }
    else
    {
        if (instr->GetSrc2())
        {
            this->LowerBinaryHelperMem(instr, IR::HelperScrObj_LdInnerFrameDisplay);
        }
        else
        {
#if DBG
            instr->m_opcode = Js::OpCode::LdInnerFrameDisplayNoParent;
#endif
            this->LowerUnaryHelperMem(instr, IR::HelperScrObj_LdInnerFrameDisplayNoParent);
        }
    }
}

void Lowerer::LowerLdFrameDisplay(IR::Instr *instr, bool doStackFrameDisplay)
{
    bool isStrict = instr->m_func->GetJnFunction()->GetIsStrictMode();
    uint16 envDepth = instr->m_func->GetJnFunction()->GetEnvDepth();
    Func *func = this->m_func;

    // envDepth of -1 indicates unknown depth (eval expression or HTML event handler).
    // We could still fast-path these by generating a loop over the (dynamically loaded) scope chain length,
    // but I doubt it's worth it.
    // If the dst opnd is a byte code temp, that indicates we're prepending a block scope or some such and
    // shouldn't attempt to do this.
    if (envDepth == (uint16)-1 ||
        (!doStackFrameDisplay && instr->GetDst()->AsRegOpnd()->m_sym->IsTempReg(instr->m_func)) ||
        PHASE_OFF(Js::FrameDisplayFastPathPhase, func))
    {
        if (isStrict)
        {
            if (instr->GetSrc2())
            {
                this->LowerBinaryHelperMem(instr, IR::HelperScrObj_LdStrictFrameDisplay);
            }
            else
            {
#if DBG
                instr->m_opcode = Js::OpCode::LdFrameDisplayNoParent;
#endif
                this->LowerUnaryHelperMem(instr, IR::HelperScrObj_LdStrictFrameDisplayNoParent);
            }
        }
        else
        {
            if (instr->GetSrc2())
            {
                this->LowerBinaryHelperMem(instr, IR::HelperScrObj_LdFrameDisplay);
            }
            else
            {
#if DBG
                instr->m_opcode = Js::OpCode::LdFrameDisplayNoParent;
#endif
                this->LowerUnaryHelperMem(instr, IR::HelperScrObj_LdFrameDisplayNoParent);
            }
        }
        return;
    }

    uint16 frameDispLength = envDepth + 1;
    Assert(frameDispLength > 0);

    IR::RegOpnd *dstOpnd = instr->UnlinkDst()->AsRegOpnd();
    IR::RegOpnd *currentFrameOpnd = instr->UnlinkSrc1()->AsRegOpnd();

    uint allocSize = sizeof(Js::FrameDisplay) + (frameDispLength * sizeof(Js::Var));
    if (doStackFrameDisplay)
    {
        IR::Instr *insertInstr = func->GetFunctionEntryInsertionPoint();

        // Initialize stack pointers for scope slots and frame display together at the top of the function
        // (in case we bail out before executing the instructions).
        IR::LabelInstr *labelNoStackFunc = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
        IR::LabelInstr *labelDone = IR::LabelInstr::New(Js::OpCode::Label, m_func);

        // Check whether stack functions have been disabled since we jitted.
        // If they have, then we must allocate closure memory on the heap.
        InsertTestBranch(IR::MemRefOpnd::New(m_func->GetJnFunction()->GetAddressOfFlags(), TyInt8, m_func),
                         IR::IntConstOpnd::New(Js::FunctionBody::Flags_StackNestedFunc, TyInt8, m_func, true),
                         Js::OpCode::BrEq_A, labelNoStackFunc, insertInstr);
        // allocSize is greater than TyMachPtr and hence changing the initial size to TyMisc
        StackSym * stackSym = StackSym::New(TyMisc, instr->m_func);
        m_func->StackAllocate(stackSym, allocSize);

        InsertLea(dstOpnd, IR::SymOpnd::New(stackSym, TyMachPtr, func), insertInstr);

        uint scopeSlotAllocSize =
            (m_func->GetJnFunction()->scopeSlotArraySize + Js::ScopeSlots::FirstSlotIndex) * sizeof(Js::Var);

        stackSym = StackSym::New(TyMisc, instr->m_func);
        m_func->StackAllocate(stackSym, scopeSlotAllocSize);

        InsertLea(currentFrameOpnd, IR::SymOpnd::New(stackSym, TyMachPtr, func), insertInstr);
        InsertBranch(Js::OpCode::Br, labelDone, insertInstr);

        insertInstr->InsertBefore(labelNoStackFunc);
        GenerateRecyclerAlloc(IR::HelperAllocMemForFrameDisplay, allocSize, dstOpnd, insertInstr, true);
        GenerateRecyclerAlloc(IR::HelperAllocMemForVarArray, scopeSlotAllocSize, currentFrameOpnd, insertInstr, true);

        insertInstr->InsertBefore(labelDone);
        m_lowererMD.CreateAssign(IR::SymOpnd::New(m_func->GetLocalFrameDisplaySym(), 0, TyMachReg, m_func), dstOpnd, insertInstr);
        m_lowererMD.CreateAssign(IR::SymOpnd::New(m_func->GetLocalClosureSym(), 0, TyMachReg, m_func), currentFrameOpnd, insertInstr);
    }
    else
    {
        GenerateRecyclerAlloc(IR::HelperAllocMemForFrameDisplay, allocSize, dstOpnd, instr);
    }

    // Copy contents of environment
    // Work back to front to leave the head element(s) in cache
    if (envDepth > 0)
    {
        IR::RegOpnd *envOpnd = instr->UnlinkSrc2()->AsRegOpnd();
        for (uint16 i = envDepth; i >= 1; i--)
        {
            IR::Opnd *scopeOpnd = IR::RegOpnd::New(TyMachReg, func);
            IR::Opnd *envLoadOpnd =
                IR::IndirOpnd::New(envOpnd, Js::FrameDisplay::GetOffsetOfScopes() + ((i - 1) * sizeof(Js::Var)), TyMachReg, func);
            m_lowererMD.CreateAssign(scopeOpnd, envLoadOpnd, instr);

            IR::Opnd *dstStoreOpnd =
                IR::IndirOpnd::New(dstOpnd, Js::FrameDisplay::GetOffsetOfScopes() + (i * sizeof(Js::Var)), TyMachReg, func);
            m_lowererMD.CreateAssign(dstStoreOpnd, scopeOpnd, instr);
        }
    }

    // Assign current element.
    m_lowererMD.CreateAssign(
        IR::IndirOpnd::New(dstOpnd, Js::FrameDisplay::GetOffsetOfScopes(), TyMachReg, func),
        currentFrameOpnd,
        instr);

    // Combine tag, strict mode flag, and length
    uintptr_t bits = 1 |
        (isStrict << (Js::FrameDisplay::GetOffsetOfStrictMode() * 8)) |
        (frameDispLength << (Js::FrameDisplay::GetOffsetOfLength() * 8));
    m_lowererMD.CreateAssign(
        IR::IndirOpnd::New(dstOpnd, 0, TyMachReg, func),
        IR::AddrOpnd::New((void*)bits, IR::AddrOpndKindConstant, func, true),
        instr);

    instr->Remove();
}

IR::AddrOpnd *Lowerer::CreateFunctionBodyOpnd(Func *const func) const
{
    return CreateFunctionBodyOpnd(func->GetJnFunction());
}

IR::AddrOpnd *Lowerer::CreateFunctionBodyOpnd(Js::FunctionBody *const functionBody) const
{
    return IR::AddrOpnd::New(functionBody, IR::AddrOpndKindDynamicFunctionBody, m_func, true);
}

bool
Lowerer::GenerateRecyclerOrMarkTempAlloc(IR::Instr * instr, IR::RegOpnd * dstOpnd, IR::JnHelperMethod allocHelper, size_t allocSize, IR::SymOpnd ** tempObjectSymOpnd)
{
    if (instr->dstIsTempObject)
    {
        *tempObjectSymOpnd = GenerateMarkTempAlloc(dstOpnd, allocSize, instr);
        return false;
    }

    this->GenerateRecyclerAlloc(allocHelper, allocSize, dstOpnd, instr);
    *tempObjectSymOpnd = nullptr;
    return true;
}

IR::SymOpnd *
Lowerer::GenerateMarkTempAlloc(IR::RegOpnd *const dstOpnd, const size_t allocSize, IR::Instr *const insertBeforeInstr)
{
    Assert(dstOpnd);
    Assert(allocSize != 0);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    // Allocate stack space for the reg exp instance, and a slot for the boxed value
    StackSym *const tempObjectSym = StackSym::New(TyMisc, func);
    m_func->StackAllocate(tempObjectSym, (int)(allocSize + sizeof(void *)));
    IR::SymOpnd * tempObjectOpnd = IR::SymOpnd::New(tempObjectSym, sizeof(void *), TyVar, func);
    InsertLea(dstOpnd, tempObjectOpnd, insertBeforeInstr);

    // Initialize the boxed instance slot
    if (this->outerMostLoopLabel == nullptr)
    {
        GenerateMemInit(dstOpnd, -(int)sizeof(void *), IR::AddrOpnd::NewNull(func), insertBeforeInstr, false);
    }
    else if (!PHASE_OFF(Js::HoistMarkTempInitPhase, this->m_func))
    {
        InsertMove(IR::SymOpnd::New(tempObjectSym, TyMachPtr, func), IR::AddrOpnd::NewNull(func), this->outerMostLoopLabel, false);
    }
    return tempObjectOpnd;
}

void Lowerer::LowerBrFncCachedScopeEq(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::BrFncCachedScopeEq || instr->m_opcode == Js::OpCode::BrFncCachedScopeNeq);
    Js::OpCode opcode = (instr->m_opcode == Js::OpCode::BrFncCachedScopeEq ? Js::OpCode::BrEq_A : Js::OpCode::BrNeq_A);

    IR::RegOpnd *src1Reg = instr->UnlinkSrc1()->AsRegOpnd();

    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(src1Reg, Js::ScriptFunction::GetOffsetOfCachedScopeObj(), TyMachReg, this->m_func);
    this->InsertCompareBranch(indirOpnd, instr->UnlinkSrc2(), opcode, false, instr->AsBranchInstr()->GetTarget(), instr->m_next);

    instr->Remove();
}


IR::Instr* Lowerer::InsertLoweredRegionStartMarker(IR::Instr* instrToInsertBefore)
{
    AssertMsg(instrToInsertBefore->m_prev != nullptr, "Can't insert lowered region start marker as the first instr in the func.");
    IR::LabelInstr* startMarkerLabel = IR::LabelInstr::New(Js::OpCode::Label, instrToInsertBefore->m_func);
    instrToInsertBefore->InsertBefore(startMarkerLabel);
    return startMarkerLabel;
}

IR::Instr* Lowerer::RemoveLoweredRegionStartMarker(IR::Instr* startMarkerInstr)
{
    AssertMsg(startMarkerInstr->m_prev != nullptr, "Lowered region start marker became the first instruction in the func after lowering?");
    IR::Instr* prevInstr = startMarkerInstr->m_prev;
    startMarkerInstr->Remove();
    return prevInstr;
}

IR::Instr* Lowerer::GetLdSpreadIndicesInstr(IR::Instr *instr)
{
    IR::Opnd *src2 = instr->GetSrc2();
    if (!src2->IsSymOpnd())
    {
        return nullptr;
    }

    IR::SymOpnd *   argLinkOpnd = src2->AsSymOpnd();
    StackSym *      argLinkSym  = argLinkOpnd->m_sym->AsStackSym();

    Assert(argLinkSym->IsSingleDef());

    return argLinkSym->m_instrDef;
}

bool Lowerer::IsSpreadCall(IR::Instr *instr)
{
    IR::Instr *lastInstr = GetLdSpreadIndicesInstr(instr);
    return lastInstr && lastInstr->m_opcode == Js::OpCode::LdSpreadIndices;
}

// When under debugger, generate a new label to be used as safe place to jump after ignore exception,
// insert it after insertAfterInstr, and return the label inserted.
// Returns nullptr/NoOP for non-debugger code path.
//static
IR::LabelInstr* Lowerer::InsertContinueAfterExceptionLabelForDebugger(Func* func, IR::Instr* insertAfterInstr, bool isHelper)
{
    Assert(func);
    Assert(insertAfterInstr);

    IR::LabelInstr* continueAfterExLabel = nullptr;
    if (func->IsJitInDebugMode())
    {
        continueAfterExLabel = IR::LabelInstr::New(Js::OpCode::Label, func, isHelper);
        insertAfterInstr->InsertAfter(continueAfterExLabel);
    }
    return continueAfterExLabel;
}

void Lowerer::GenerateSingleCharStrJumpTableLookup(IR::Instr * instr)
{
    IR::MultiBranchInstr * multiBrInstr = instr->AsBranchInstr()->AsMultiBrInstr();
    Func * func = instr->m_func;
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    IR::LabelInstr * continueLabel = IR::LabelInstr::New(Js::OpCode::Label, func);

    // MOV strLengthOpnd, str->length
    IR::RegOpnd * strLengthOpnd = IR::RegOpnd::New(TyUint32, func);
    InsertMove(strLengthOpnd, IR::IndirOpnd::New(instr->GetSrc1()->AsRegOpnd(), Js::JavascriptString::GetOffsetOfcharLength(), TyUint32, func), instr);

    // CMP strLengthOpnd, 1
    // JNE defaultLabel
    IR::LabelInstr * defaultLabelInstr = (IR::LabelInstr *)multiBrInstr->GetBranchJumpTable()->defaultTarget;
    InsertCompareBranch(strLengthOpnd, IR::IntConstOpnd::New(1, TyUint32, func), Js::OpCode::BrNeq_A, defaultLabelInstr, instr);

    // MOV strBuffer, str->psz
    IR::RegOpnd * strBufferOpnd = IR::RegOpnd::New(TyMachPtr, func);
    InsertMove(strBufferOpnd, IR::IndirOpnd::New(instr->GetSrc1()->AsRegOpnd(), Js::JavascriptString::GetOffsetOfpszValue(), TyMachPtr, func), instr);

    // TST strBuffer, strBuffer
    // JNE $continue
    InsertTestBranch(strBufferOpnd, strBufferOpnd, Js::OpCode::BrNeq_A, continueLabel, instr);

    // $helper:
    // PUSH str
    // CALL JavascriptString::GetSzHelper
    // MOV strBuffer, eax
    // $continue:
    instr->InsertBefore(helperLabel);
    m_lowererMD.LoadHelperArgument(instr, instr->GetSrc1());
    IR::Instr * instrCall = IR::Instr::New(Js::OpCode::Call, strBufferOpnd, IR::HelperCallOpnd::New(IR::HelperString_GetSz, func), func);
    instr->InsertBefore(instrCall);
    m_lowererMD.LowerCall(instrCall, 0);
    instr->InsertBefore(continueLabel);

    // MOV charOpnd, [strBuffer]
    IR::RegOpnd * charOpnd = IR::RegOpnd::New(TyUint32, func);
    InsertMove(charOpnd, IR::IndirOpnd::New(strBufferOpnd, 0, TyUint16, func), instr);

    if (multiBrInstr->m_baseCaseValue != 0)
    {
        // SUB charOpnd, baseIndex
        InsertSub(false, charOpnd, charOpnd, IR::IntConstOpnd::New(multiBrInstr->m_baseCaseValue, TyUint32, func), instr);
    }

    // CMP charOpnd, lastCaseIndex - baseCaseIndex
    // JA defaultLabel
    InsertCompareBranch(charOpnd, IR::IntConstOpnd::New(multiBrInstr->m_lastCaseValue - multiBrInstr->m_baseCaseValue, TyUint32, func, true),
        Js::OpCode::BrGt_A, true, defaultLabelInstr, instr);

    instr->UnlinkSrc1();
    LowerJumpTableMultiBranch(multiBrInstr, charOpnd);
}

void Lowerer::GenerateSwitchStringLookup(IR::Instr * instr)
{
    /* Collect information about string length in all the case*/
    charcount_t minLength = UINT_MAX;
    charcount_t maxLength = 0;
    BVUnit32 bvLength;
    instr->AsBranchInstr()->AsMultiBrInstr()->GetBranchDictionary()->dictionary.Map([&](Js::JavascriptString * str, void *)
    {
        charcount_t len = str->GetLength();
        minLength = min(minLength, str->GetLength());
        maxLength = max(maxLength, str->GetLength());
        if (len < 32)
        {
            bvLength.Set(len);
        }
    });

    Func * func = instr->m_func;
    IR::RegOpnd * strLengthOpnd = IR::RegOpnd::New(TyUint32, func);
    InsertMove(strLengthOpnd, IR::IndirOpnd::New(instr->GetSrc1()->AsRegOpnd(), Js::JavascriptString::GetOffsetOfcharLength(), TyUint32, func), instr);
    IR::LabelInstr * defaultLabelInstr = (IR::LabelInstr *)instr->AsBranchInstr()->AsMultiBrInstr()->GetBranchDictionary()->defaultTarget;
    if (minLength == maxLength)
    {
        // Generate single length filter
        InsertCompareBranch(strLengthOpnd, IR::IntConstOpnd::New(minLength, TyUint32, func), Js::OpCode::BrNeq_A, defaultLabelInstr, instr);
    }
    else if (maxLength < 32)
    {
        // Generate bit filter

        // Jump to default label if the bit is not on for the length % 32
        IR::IntConstOpnd * lenBitMaskOpnd = IR::IntConstOpnd::New(bvLength.GetWord(), TyUint32, func);
        InsertBitTestBranch(lenBitMaskOpnd, strLengthOpnd, false, defaultLabelInstr, instr);
        // Jump to default label if the bit is > 32
        InsertTestBranch(strLengthOpnd, IR::IntConstOpnd::New(UINT32_MAX ^ 31, TyUint32, func), Js::OpCode::BrNeq_A, defaultLabelInstr, instr);
    }
    else
    {
        // CONSIDER: Generate range filter
    }
    this->LowerMultiBr(instr, IR::HelperOp_SwitchStringLookUp);
}

IR::Instr *
Lowerer::LowerTry(IR::Instr* instr, bool tryCatch)
{
    if (this->m_func->hasBailout)
    {
        this->EnsureBailoutReturnValueSym();
    }
    this->EnsureHasBailedOutSym();
    IR::SymOpnd * hasBailedOutOpnd = IR::SymOpnd::New(this->m_func->m_hasBailedOutSym, TyUint32, this->m_func);
    IR::Instr * setInstr = IR::Instr::New(LowererMD::GetStoreOp(TyUint32), hasBailedOutOpnd, IR::IntConstOpnd::New(0, TyUint32, this->m_func), this->m_func);
    instr->InsertBefore(setInstr);
    LowererMD::Legalize(setInstr);

    return m_lowererMD.LowerTry(instr, tryCatch ? IR::HelperOp_TryCatch : IR::HelperOp_TryFinally);
}

void
Lowerer::EnsureBailoutReturnValueSym()
{
    if (this->m_func->m_bailoutReturnValueSym == nullptr)
    {
        this->m_func->m_bailoutReturnValueSym = StackSym::New(TyVar, this->m_func);
        this->m_func->StackAllocate(this->m_func->m_bailoutReturnValueSym, sizeof(Js::Var));
    }
}

void
Lowerer::EnsureHasBailedOutSym()
{
    if (this->m_func->m_hasBailedOutSym == nullptr)
    {
        this->m_func->m_hasBailedOutSym = StackSym::New(TyUint32, this->m_func);
        this->m_func->StackAllocate(this->m_func->m_hasBailedOutSym, MachRegInt);
    }
}

void
Lowerer::InsertReturnThunkForRegion(Region* region, IR::LabelInstr* restoreLabel)
{
    Assert(this->m_func->isPostLayout);
    Assert(region->GetType() == RegionTypeTry || region->GetType() == RegionTypeCatch);

    if (!region->returnThunkEmitted)
    {
        this->m_func->m_exitInstr->InsertAfter(region->GetBailoutReturnThunkLabel());

        bool newLastInstrInserted = false;
        IR::Instr * insertBeforeInstr = region->GetBailoutReturnThunkLabel()->m_next;
        if (insertBeforeInstr == nullptr)
        {
            Assert(this->m_func->m_exitInstr == this->m_func->m_tailInstr);
            insertBeforeInstr = IR::Instr::New(Js::OpCode::Nop, this->m_func);
            newLastInstrInserted = true;
            region->GetBailoutReturnThunkLabel()->InsertAfter(insertBeforeInstr);
            this->m_func->m_tailInstr = insertBeforeInstr;
        }

        IR::LabelOpnd * continuationAddr;
        if (region->GetParent()->GetType() != RegionTypeRoot)
        {
            continuationAddr = IR::LabelOpnd::New(region->GetParent()->GetBailoutReturnThunkLabel(), this->m_func);
        }
        else
        {
            continuationAddr = IR::LabelOpnd::New(restoreLabel, this->m_func);
        }

        IR::Instr * lastInstr = m_lowererMD.LowerEHRegionReturn(insertBeforeInstr, continuationAddr);
        if (newLastInstrInserted)
        {
            Assert(this->m_func->m_tailInstr == insertBeforeInstr);
            insertBeforeInstr->Remove();
            this->m_func->m_tailInstr = lastInstr;
        }

        region->returnThunkEmitted = true;
    }
}

void
Lowerer::SetHasBailedOut(IR::Instr * bailoutInstr)
{
    Assert(this->m_func->isPostLayout);
    IR::SymOpnd * hasBailedOutOpnd = IR::SymOpnd::New(this->m_func->m_hasBailedOutSym, TyUint32, this->m_func);
    IR::Instr * setInstr = IR::Instr::New(LowererMD::GetStoreOp(TyUint32), hasBailedOutOpnd, IR::IntConstOpnd::New(1, TyUint32, this->m_func), this->m_func);
    bailoutInstr->InsertBefore(setInstr);
    LowererMD::Legalize(setInstr, true);
}

IR::Instr*
Lowerer::EmitEHBailoutStackRestore(IR::Instr * bailoutInstr)
{
    Assert(this->m_func->isPostLayout);

#ifdef _M_IX86
    BailOutInfo * bailoutInfo = bailoutInstr->GetBailOutInfo();
    if (bailoutInfo->startCallCount != 0)
    {
        uint totalStackToBeRestored = 0;
        uint stackAlignmentAdjustment = 0;
        for (uint i = 0; i < bailoutInfo->startCallCount; i++)
        {
            uint startCallOutParamCount = bailoutInfo->GetStartCallOutParamCount(i);
            if ((Math::Align<int32>(startCallOutParamCount * MachPtr, MachStackAlignment) - (startCallOutParamCount * MachPtr)) != 0)
            {
                stackAlignmentAdjustment++;
            }
        }
        totalStackToBeRestored = (bailoutInfo->totalOutParamCount + stackAlignmentAdjustment) * MachPtr;

        IR::RegOpnd * espOpnd = IR::RegOpnd::New(NULL, LowererMD::GetRegStackPointer(), TyMachReg, this->m_func);
        IR::Opnd * opnd = IR::IndirOpnd::New(espOpnd, totalStackToBeRestored, TyMachReg, this->m_func);
        IR::Instr * stackRestoreInstr = IR::Instr::New(Js::OpCode::LEA, espOpnd, opnd, this->m_func);

        bailoutInstr->InsertAfter(stackRestoreInstr);
        return stackRestoreInstr;
    }
#endif

    return bailoutInstr;
}

void
Lowerer::EmitSaveEHBailoutReturnValueAndJumpToRetThunk(IR::Instr * insertAfterInstr)
{
    Assert(this->m_func->isPostLayout);
    // After the CALL SaveAllRegistersAndBailout instruction, emit
    //
    // MOV bailoutReturnValueSym, eax
    // JMP $currentRegion->bailoutReturnThunkLabel

    IR::SymOpnd * bailoutReturnValueSymOpnd = IR::SymOpnd::New(this->m_func->m_bailoutReturnValueSym, TyVar, this->m_func);
    IR::RegOpnd *eaxOpnd = IR::RegOpnd::New(NULL, LowererMD::GetRegReturn(TyMachReg), TyMachReg, this->m_func);
    IR::Instr * movInstr = IR::Instr::New(LowererMD::GetStoreOp(TyVar), bailoutReturnValueSymOpnd, eaxOpnd, this->m_func);
    insertAfterInstr->InsertAfter(movInstr);
    LowererMD::Legalize(movInstr, true);

    IR::BranchInstr * jumpInstr = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, this->currentRegion->GetBailoutReturnThunkLabel(), this->m_func);
    movInstr->InsertAfter(jumpInstr);
}

void
Lowerer::EmitRestoreReturnValueFromEHBailout(IR::LabelInstr * restoreLabel, IR::LabelInstr * epilogLabel)
{
    Assert(this->m_func->isPostLayout);
    //          JMP $epilog
    // $restore:
    //          MOV eax, bailoutReturnValueSym
    // $epilog:

    IR::SymOpnd * bailoutReturnValueSymOpnd = IR::SymOpnd::New(this->m_func->m_bailoutReturnValueSym, TyVar, this->m_func);
    IR::RegOpnd * eaxOpnd = IR::RegOpnd::New(NULL, LowererMD::GetRegReturn(TyMachReg), TyMachReg, this->m_func);

    IR::Instr * movInstr = IR::Instr::New(LowererMD::GetLoadOp(TyVar), eaxOpnd, bailoutReturnValueSymOpnd, this->m_func);

    epilogLabel->InsertBefore(restoreLabel);
    epilogLabel->InsertBefore(movInstr);
    LowererMD::Legalize(movInstr, true);
    restoreLabel->InsertBefore(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, epilogLabel, this->m_func));
}

void
Lowerer::InsertBitTestBranch(IR::Opnd * bitMaskOpnd, IR::Opnd * bitIndex, bool jumpIfBitOn, IR::LabelInstr * targetLabel, IR::Instr * insertBeforeInstr)
{
#if defined(_M_IX86) || defined(_M_AMD64)
    // Generate bit test and branch
    // BT bitMaskOpnd, bitIndex
    // JB/JAE targetLabel
    Func * func = this->m_func;
    IR::Instr * instr = IR::Instr::New(Js::OpCode::BT, func);
    instr->SetSrc1(bitMaskOpnd);
    instr->SetSrc2(bitIndex);
    insertBeforeInstr->InsertBefore(instr);

    if (!(bitMaskOpnd->IsRegOpnd() || bitMaskOpnd->IsIndirOpnd() || bitMaskOpnd->IsMemRefOpnd()))
    {
        instr->HoistSrc1(Js::OpCode::MOV);
    }

    InsertBranch(jumpIfBitOn ? Js::OpCode::JB : Js::OpCode::JAE, targetLabel, insertBeforeInstr);
#elif defined(_M_ARM)
    // ARM don't have bit test instruction, so just generated
    // MOV r1, 1
    // SHL r1, bitIndex
    // TEST bitMaskOpnd, r1
    // BEQ/BNEQ targetLabel
    Func * func = this->m_func;
    IR::RegOpnd * lenBitOpnd = IR::RegOpnd::New(TyUint32, func);
    InsertMove(lenBitOpnd, IR::IntConstOpnd::New(1, TyUint32, this->m_func), insertBeforeInstr);
    InsertShift(Js::OpCode::Shl_I4, false, lenBitOpnd, lenBitOpnd, bitIndex, insertBeforeInstr);
    InsertTestBranch(lenBitOpnd, bitMaskOpnd, jumpIfBitOn? Js::OpCode::BrNeq_A :Js::OpCode::BrEq_A, targetLabel, insertBeforeInstr);
#else
    AssertMsg(false, "Not implemented");
#endif
}

//
// Generates an object test and then a string test with the static string type
//
void
Lowerer::GenerateStringTest(IR::RegOpnd *srcReg, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr * continueLabel, bool generateObjectCheck)
{
    Assert(srcReg);
    if (!srcReg->GetValueType().IsString())
    {
        if (generateObjectCheck && !srcReg->IsNotTaggedValue())
        {
            this->m_lowererMD.GenerateObjectTest(srcReg, insertInstr, labelHelper);
        }

        // CMP [regSrcStr + offset(type)] , static string type   -- check base string type
        // BrEq/BrNeq labelHelper.
        IR::IndirOpnd * src1 = IR::IndirOpnd::New(srcReg, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, m_func);
        IR::Opnd * src2 = this->LoadLibraryValueOpnd(insertInstr, LibraryValue::ValueStringTypeStatic);
        if (continueLabel)
        {
            InsertCompareBranch(src1, src2, Js::OpCode::BrEq_A, continueLabel, insertInstr);
        }
        else
        {
            InsertCompareBranch(src1, src2, Js::OpCode::BrNeq_A, labelHelper, insertInstr);
        }
    }
}

void
Lowerer::LowerConvNum(IR::Instr *instrLoad, bool noMathFastPath)
{
    if (PHASE_OFF(Js::OtherFastPathPhase, this->m_func) || noMathFastPath || !instrLoad->GetSrc1()->IsRegOpnd())
    {
        this->LowerUnaryHelperMemWithTemp2(instrLoad, IR_HELPER_OP_FULL_OR_INPLACE(ConvNumber));
        return;
    }

    //      MOV dst, src1
    //      TEST src1, 1
    //      JNE $done
    //      call ToNumber
    //$done:

    bool isInt = false;
    bool isNotInt = false;
    IR::RegOpnd *src1 = instrLoad->GetSrc1()->AsRegOpnd();
    IR::LabelInstr *labelDone = NULL;
    IR::Instr *instr;

    if (src1->IsTaggedInt())
    {
        isInt = true;
    }
    else if (src1->IsNotInt())
    {
        isNotInt = true;
    }
    if (!isNotInt)
    {
        //      MOV dst, src1

        instr = LowererMD::CreateAssign(instrLoad->GetDst(), src1, instrLoad);

        if (!isInt)
        {
            labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
            bool didTest = m_lowererMD.GenerateObjectTest(src1, instrLoad, labelDone);

            if (didTest)
            {
                // This label is needed only to mark the helper block
                IR::LabelInstr * labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
                instrLoad->InsertBefore(labelHelper);
            }
        }
    }

    if (!isInt)
    {
        if (labelDone)
        {
            instrLoad->InsertAfter(labelDone);
        }
        this->LowerUnaryHelperMemWithTemp2(instrLoad, IR_HELPER_OP_FULL_OR_INPLACE(ConvNumber));
    }
    else
    {
        instrLoad->Remove();
    }
}

IR::Opnd *
Lowerer::LoadSlotArrayWithCachedLocalType(IR::Instr * instrInsert, IR::PropertySymOpnd *propertySymOpnd)
{
    IR::RegOpnd *opndBase = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);
    if (propertySymOpnd->UsesAuxSlot())
    {
        // If we use the auxiliary slot array, load it and return it
        IR::RegOpnd *opndSlotArray = IR::RegOpnd::New(TyMachReg, this->m_func);
        IR::Opnd *opndIndir = IR::IndirOpnd::New(opndBase, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, this->m_func);
        LowererMD::CreateAssign(opndSlotArray, opndIndir, instrInsert);

        return opndSlotArray;
    }
    else
    {
        // If we use inline slot return the address to the object header
        return opndBase;
    }
}

IR::Opnd *
Lowerer::LoadSlotArrayWithCachedProtoType(IR::Instr * instrInsert, IR::PropertySymOpnd *propertySymOpnd)
{
    // Get the prototype object from the cache
    Js::RecyclableObject *prototypeObject = propertySymOpnd->GetProtoObject();
    Assert(prototypeObject != nullptr);

    if (propertySymOpnd->UsesAuxSlot())
    {
        // If we use the auxiliary slot array, load it from the prototype object and return it
        IR::RegOpnd *opndSlotArray = IR::RegOpnd::New(TyMachReg, this->m_func);
        IR::Opnd *opnd = IR::MemRefOpnd::New((char*)prototypeObject + Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, this->m_func, IR::AddrOpndKindDynamicAuxSlotArrayRef);
        LowererMD::CreateAssign(opndSlotArray, opnd, instrInsert);
        return opndSlotArray;
    }
    else
    {
        // If we use inline slot return the address of the prototype object
        return IR::MemRefOpnd::New(prototypeObject, TyMachReg, this->m_func);
    }
}

IR::Instr *
Lowerer::LowerLdAsmJsEnv(IR::Instr * instr)
{
    Assert(m_func->GetJnFunction()->GetIsAsmJsFunction());
    IR::Opnd * functionObjOpnd;
    IR::Instr * instrPrev = this->m_lowererMD.LoadFunctionObjectOpnd(instr, functionObjOpnd);
    Assert(!instr->GetSrc1());
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(functionObjOpnd->AsRegOpnd(), Js::AsmJsScriptFunction::GetOffsetOfModuleMemory(), TyMachPtr, m_func);
    instr->SetSrc1(indirOpnd);

    LowererMD::ChangeToAssign(instr);
    return instrPrev;
}

IR::Instr *
Lowerer::LowerLdEnv(IR::Instr * instr)
{
    IR::Opnd * src1 = instr->GetSrc1();
    IR::Opnd * functionObjOpnd;
    IR::Instr * instrPrev = this->m_lowererMD.LoadFunctionObjectOpnd(instr, functionObjOpnd);
    Assert(!instr->GetSrc1());
    if (src1 == nullptr || functionObjOpnd->IsRegOpnd())
    {
        IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(functionObjOpnd->AsRegOpnd(),
            Js::ScriptFunction::GetOffsetOfEnvironment(), TyMachPtr, m_func);
        instr->SetSrc1(indirOpnd);
    }
    else
    {
        Assert(functionObjOpnd->IsAddrOpnd());
        IR::AddrOpnd* functionObjAddrOpnd = functionObjOpnd->AsAddrOpnd();
        IR::MemRefOpnd* functionEnvMemRefOpnd = IR::MemRefOpnd::New((void *)((intptr)functionObjAddrOpnd->m_address + Js::ScriptFunction::GetOffsetOfEnvironment()),
            TyMachPtr, this->m_func, IR::AddrOpndKindDynamicFunctionEnvironmentRef);
        instr->SetSrc1(functionEnvMemRefOpnd);
    }

    LowererMD::ChangeToAssign(instr);
    return instrPrev;
}

IR::Instr *
Lowerer::LowerFrameDisplayCheck(IR::Instr * instr)
{
    IR::Instr *instrPrev = instr->m_prev;
    IR::Instr *insertInstr = instr->m_next;
    IR::AddrOpnd *addrOpnd = instr->UnlinkSrc2()->AsAddrOpnd();
    FrameDisplayCheckRecord *record = (FrameDisplayCheckRecord*)addrOpnd->m_address;

    IR::LabelInstr *errorLabel = nullptr;
    IR::LabelInstr *continueLabel = nullptr;
    IR::RegOpnd *envOpnd = instr->GetDst()->AsRegOpnd();
    uint32 frameDisplayOffset = Js::FrameDisplay::GetOffsetOfScopes()/sizeof(Js::Var);

    if (record->slotId != (uint32)-1 && record->slotId > frameDisplayOffset)
    {
        // Check that the frame display has enough scopes in it to satisfy the code.
        errorLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
        continueLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);

        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(envOpnd,
                                                       Js::FrameDisplay::GetOffsetOfLength(),
                                                       TyUint16, m_func, true);

        IR::IntConstOpnd *slotIdOpnd = IR::IntConstOpnd::New(record->slotId - frameDisplayOffset, TyUint16, m_func);
        InsertCompareBranch(indirOpnd, slotIdOpnd, Js::OpCode::BrLe_A, true, errorLabel, insertInstr);
    }

    if (record->table)
    {
        // Check the size of each of the slot arrays in the scope chain.
        FOREACH_HASHTABLE_ENTRY(uint32, bucket, record->table)
        {
            uint32 slotId = bucket.element;
            if (slotId != (uint32)-1 && slotId > Js::ScopeSlots::FirstSlotIndex)
            {
                if (errorLabel == nullptr)
                {
                    errorLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
                    continueLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);
                }

                IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(envOpnd,
                                                               bucket.value * sizeof(Js::Var),
                                                               TyVar, m_func, true);
                IR::RegOpnd * slotArrayOpnd = IR::RegOpnd::New(TyVar, m_func);
                InsertMove(slotArrayOpnd, indirOpnd, insertInstr);

                indirOpnd = IR::IndirOpnd::New(slotArrayOpnd,
                                               Js::ScopeSlots::EncodedSlotCountSlotIndex * sizeof(Js::Var),
                                               TyUint32, m_func, true);
                IR::IntConstOpnd * slotIdOpnd = IR::IntConstOpnd::New(slotId - Js::ScopeSlots::FirstSlotIndex,
                                                                      TyUint32, m_func);
                InsertCompareBranch(indirOpnd, slotIdOpnd, Js::OpCode::BrLe_A, true, errorLabel, insertInstr);
            }
        }
        NEXT_HASHTABLE_ENTRY;
    }

    if (errorLabel)
    {
        InsertBranch(Js::OpCode::Br, continueLabel, insertInstr);

        insertInstr->InsertBefore(errorLabel);
        IR::Instr * instrHelper = IR::Instr::New(Js::OpCode::Call, m_func);
        insertInstr->InsertBefore(instrHelper);
        m_lowererMD.ChangeToHelperCall(instrHelper, IR::HelperOp_FatalInternalError);
        insertInstr->InsertBefore(continueLabel);
    }

    m_lowererMD.ChangeToAssign(instr);

    return instrPrev;
}

IR::Instr *
Lowerer::LowerSlotArrayCheck(IR::Instr * instr)
{
    IR::Instr *instrPrev = instr->m_prev;
    IR::Instr *insertInstr = instr->m_next;

    IR::RegOpnd *slotArrayOpnd = instr->GetDst()->AsRegOpnd();
    StackSym *stackSym = slotArrayOpnd->m_sym;

    IR::IntConstOpnd *slotIdOpnd = instr->UnlinkSrc2()->AsIntConstOpnd();
    uint32 slotId = (uint32)slotIdOpnd->GetValue();
    Assert(slotId != (uint32)-1 && slotId >= Js::ScopeSlots::FirstSlotIndex);

    if (slotId > Js::ScopeSlots::FirstSlotIndex)
    {
        if (m_func->DoStackFrameDisplay() && stackSym->m_id == m_func->GetLocalClosureSym()->m_id)
        {
            // The pointer we loaded points to the reserved/known address where the slot array can be boxed.
            // Deref to get the real value.
            IR::IndirOpnd * srcOpnd = IR::IndirOpnd::New(IR::RegOpnd::New(stackSym, TyVar, m_func), 0, TyVar, m_func);
            IR::RegOpnd * dstOpnd = IR::RegOpnd::New(TyVar, m_func);
            InsertMove(dstOpnd, srcOpnd, insertInstr);
            stackSym = dstOpnd->m_sym;
        }

        IR::LabelInstr *errorLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
        IR::LabelInstr *continueLabel = IR::LabelInstr::New(Js::OpCode::Label, m_func);

        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(IR::RegOpnd::New(stackSym, TyVar, m_func),
                                                       Js::ScopeSlots::EncodedSlotCountSlotIndex * sizeof(Js::Var),
                                                       TyUint32, m_func, true);

        slotIdOpnd->SetValue(slotId - Js::ScopeSlots::FirstSlotIndex);
        InsertCompareBranch(indirOpnd, slotIdOpnd, Js::OpCode::BrGt_A, true, continueLabel, insertInstr);

        insertInstr->InsertBefore(errorLabel);
        IR::Instr * instrHelper = IR::Instr::New(Js::OpCode::Call, m_func);
        insertInstr->InsertBefore(instrHelper);
        m_lowererMD.ChangeToHelperCall(instrHelper, IR::HelperOp_FatalInternalError);
        insertInstr->InsertBefore(continueLabel);
    }

    m_lowererMD.ChangeToAssign(instr);

    return instrPrev;
}

IR::RegOpnd *
Lowerer::LoadIndexFromLikelyFloat(
    IR::RegOpnd *indexOpnd,
    const bool skipNegativeCheck,
    IR::LabelInstr *const notIntLabel,
    IR::LabelInstr *const negativeLabel,
    IR::Instr *const insertBeforeInstr)
{
#ifdef _M_IX86
    // We should only generate this if sse2 is available
    Assert(AutoSystemInfo::Data.SSE2Available());
#endif

    Func *func = insertBeforeInstr->m_func;

    IR::LabelInstr * convertToUint = IR::LabelInstr::New(Js::OpCode::Label, func);
    IR::LabelInstr * fallThrough = IR::LabelInstr::New(Js::OpCode::Label, func);

    // First generate test for tagged int even though profile data says likely float. Indices are usually int and we need a fast path before we try to convert float to int

    //     mov  intIndex, index
    //     sar  intIndex, 1
    //     jae  convertToInt
    IR::RegOpnd *int32IndexOpnd = GenerateUntagVar(indexOpnd, convertToUint, insertBeforeInstr, !indexOpnd->IsTaggedInt());

    if (!skipNegativeCheck)
    {
        //     test index, index
        //     js   $notTaggedIntOrNegative
        InsertTestBranch(int32IndexOpnd, int32IndexOpnd, LowererMD::MDCompareWithZeroBranchOpcode(Js::OpCode::BrLt_A), negativeLabel, insertBeforeInstr);
    }
    InsertBranch(Js::OpCode::Br, fallThrough, insertBeforeInstr);

    insertBeforeInstr->InsertBefore(convertToUint);

    // try to convert float to int in a fast path
#if FLOATVAR
    IR::RegOpnd* floatIndexOpnd = m_lowererMD.CheckFloatAndUntag(indexOpnd, insertBeforeInstr, notIntLabel);
#else
    m_lowererMD.GenerateFloatTest(indexOpnd, insertBeforeInstr, notIntLabel);
    IR::IndirOpnd * floatIndexOpnd = IR::IndirOpnd::New(indexOpnd, Js::JavascriptNumber::GetValueOffset(), TyMachDouble, this->m_func);
#endif

    IR::LabelInstr * doneConvUint32 = IR::LabelInstr::New(Js::OpCode::Label, func);
    IR::LabelInstr * helperConvUint32 = IR::LabelInstr::New(Js::OpCode::Label, func, true /*helper*/);
    m_lowererMD.ConvertFloatToInt32(int32IndexOpnd, floatIndexOpnd, helperConvUint32, doneConvUint32, insertBeforeInstr);

    // helper path
    insertBeforeInstr->InsertBefore(helperConvUint32);
    m_lowererMD.LoadDoubleHelperArgument(insertBeforeInstr, floatIndexOpnd);

    IR::Instr * helperCall = IR::Instr::New(Js::OpCode::Call, int32IndexOpnd, this->m_func);
    insertBeforeInstr->InsertBefore(helperCall);
    m_lowererMD.ChangeToHelperCall(helperCall, IR::HelperConv_ToUInt32Core);

    // main path
    insertBeforeInstr->InsertBefore(doneConvUint32);

    //Convert uint32 to back to float for comparison that conversion was indeed successful
    IR::RegOpnd *floatOpndFromUint32 = IR::RegOpnd::New(TyFloat64, func);
    m_lowererMD.EmitUIntToFloat(floatOpndFromUint32, int32IndexOpnd, insertBeforeInstr);

    // compare with float from the original indexOpnd, we need floatIndex == (float64)(uint32)floatIndex
    InsertCompareBranch(floatOpndFromUint32, floatIndexOpnd, Js::OpCode::BrNeq_A, notIntLabel, insertBeforeInstr, false);

    insertBeforeInstr->InsertBefore(fallThrough);
    return int32IndexOpnd;
}

#if DBG
void
Lowerer::LegalizeVerifyRange(IR::Instr * instrStart, IR::Instr * instrLast)
{
    FOREACH_INSTR_IN_RANGE(verifyLegalizeInstr, instrStart, instrLast)
    {
        LowererMD::Legalize<true>(verifyLegalizeInstr);
    }
    NEXT_INSTR_IN_RANGE;
}
#endif
