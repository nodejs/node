//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    // ----------------------------------------------------------------------
    // CountDomain
    // ----------------------------------------------------------------------

#if ENABLE_REGEX_CONFIG_OPTIONS
    void CountDomain::Print(DebugWriter* w) const
    {
        if (upper != CharCountFlag && lower == (CharCount)upper)
            w->Print(L"[%u]", lower);
        else
        {
            w->Print(L"[%u-", lower);
            if (upper == CharCountFlag)
                w->Print(L"inf]");
            else
                w->Print(L"%u]", (CharCount)upper);
        }
    }
#endif

    // ----------------------------------------------------------------------
    // Matcher (inlined, called from instruction Exec methods)
    // ----------------------------------------------------------------------

#define PUSH(contStack, T, ...) (new (contStack.Push<T>()) T(__VA_ARGS__))
#define PUSHA(assertionStack, T, ...) (new (assertionStack.Push()) T(__VA_ARGS__))
#define L2I(O, label) LabelToInstPointer<O##Inst>(Inst::O, label)

#define FAIL_PARAMETERS input, inputOffset, instPointer, contStack, assertionStack, qcTicks
#define HARDFAIL_PARAMETERS(mode) input, inputLength, matchStart, inputOffset, instPointer, contStack, assertionStack, qcTicks, mode

    // Regex QC heuristics:
    // - TicksPerQC
    //     - Number of ticks from a previous QC needed to cause another QC. The value affects how often QC will be triggered, so
    //       on slower machines or debug builds, the value needs to be smaller to maintain a reasonable frequency of QCs.
    // - TicksPerQcTimeCheck
    //     - Number of ticks from a previous QC needed to trigger a time check. Elapsed time from the previous QC is checked to
    //       see if a QC needs to be triggered. The value must be less than TicksPerQc and small enough to reasonably guarantee
    //       a QC every TimePerQc milliseconds without affecting perf.
    // - TimePerQc
    //     - The target time between QCs

#if defined(_M_ARM)
    const uint Matcher::TicksPerQc = 1u << 19
#else
    const uint Matcher::TicksPerQc = 1u << (AutoSystemInfo::ShouldQCMoreFrequently() ? 17 : 21)
#endif
#if DBG
        >> 2
#endif
        ;

    const uint Matcher::TicksPerQcTimeCheck = Matcher::TicksPerQc >> 2;
    const uint Matcher::TimePerQc = AutoSystemInfo::ShouldQCMoreFrequently() ? 50 : 100; // milliseconds

#if ENABLE_REGEX_CONFIG_OPTIONS
    void Matcher::PushStats(ContStack& contStack, const Char* const input) const
    {
        if (stats != 0)
        {
            stats->numPushes++;
            if (contStack.Position() > stats->stackHWM)
                stats->stackHWM = contStack.Position();
        }
        if (w != 0)
        {
            w->Print(L"PUSH ");
            contStack.Top()->Print(w, input);
        }
    }

    void Matcher::PopStats(ContStack& contStack, const Char* const input) const
    {
        if (stats != 0)
            stats->numPops++;
        if (w != 0)
        {
            const Cont* top = contStack.Top();
            if (top == 0)
                w->PrintEOL(L"<empty stack>");
            else
            {
                w->Print(L"POP ");
                top->Print(w, input);
            }
        }
    }

    void Matcher::UnPopStats(ContStack& contStack, const Char* const input) const
    {
        if (stats != 0)
            stats->numPops--;
        if (w != 0)
        {
            const Cont* top = contStack.Top();
            if (top == 0)
                w->PrintEOL(L"<empty stack>");
            else
            {
                w->Print(L"UNPOP ");
                top->Print(w, input);
            }
        }
    }

    void Matcher::CompStats() const
    {
        if (stats != 0)
            stats->numCompares++;
    }

    void Matcher::InstStats() const
    {
        if (stats != 0)
            stats->numInsts++;
    }
#endif

    __inline void Matcher::QueryContinue(uint &qcTicks)
    {
        // See definition of TimePerQc for description of regex QC heuristics

        Assert(!(TicksPerQc & TicksPerQc - 1)); // must be a power of 2
        Assert(!(TicksPerQcTimeCheck & TicksPerQcTimeCheck - 1)); // must be a power of 2
        Assert(TicksPerQcTimeCheck < TicksPerQc);

        if(PHASE_OFF1(Js::RegexQcPhase))
            return;
        if(++qcTicks & TicksPerQcTimeCheck - 1)
            return;
        DoQueryContinue(qcTicks);
    }

    __inline bool Matcher::HardFail
        ( const Char* const input
        , const CharCount inputLength
        , CharCount &matchStart
        , CharCount &inputOffset
        , const uint8 *&instPointer
        , ContStack &contStack
        , AssertionStack &assertionStack
        , uint &qcTicks
        , HardFailMode mode )
    {
        switch (mode)
        {
        case BacktrackAndLater:
            return Fail(FAIL_PARAMETERS);
        case BacktrackOnly:
            if (Fail(FAIL_PARAMETERS))
            {
                // No use trying any more start positions
                matchStart = inputLength;
                return true; // STOP EXECUTING
            }
            else
                return false;
        case LaterOnly:
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (w != 0)
                w->PrintEOL(L"CLEAR");
#endif
            contStack.Clear();
            assertionStack.Clear();
            return true; // STOP EXECUTING
        case ImmediateFail:
            // No use trying any more start positions
            matchStart = inputLength;
            return true; // STOP EXECUTING
        default:
            Assume(false);
        }
        return true;
    }

    __inline bool Matcher::PopAssertion(CharCount &inputOffset, const uint8 *&instPointer, ContStack &contStack, AssertionStack &assertionStack, bool succeeded)
    {
        AssertionInfo* info = assertionStack.Top();
        Assert(info != 0);
        assertionStack.Pop();
        BeginAssertionInst* begin = L2I(BeginAssertion, info->beginLabel);

        // Cut the existing continuations (we never backtrack into an assertion)
        // NOTE: We don't include the effective pops in the stats
#if ENABLE_REGEX_CONFIG_OPTIONS
        if (w != 0)
            w->PrintEOL(L"POP TO %llu", (unsigned long long)info->contStackPosition);
#endif
        contStack.PopTo(info->contStackPosition);

        // succeeded  isNegation  action
        // ---------  ----------  ----------------------------------------------------------------------------------
        // false      false       Fail into outer continuations (inner group bindings will have been undone)
        // true       false       Jump to next label (inner group bindings are now frozen)
        // false      true        Jump to next label (inner group bindings will have been undone and are now frozen)
        // true       true        Fail into outer continuations (inner group binding MUST BE CLEARED)

        if (succeeded && begin->isNegation)
            ResetInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId);

        if (succeeded == begin->isNegation)
        {
            // Assertion failed
            return false;
        }
        else
        {
            // Continue with next label but from original input position
            inputOffset = info->startInputOffset;
            instPointer = LabelToInstPointer(begin->nextLabel);

            return true;
        }
    }

    __inline void Matcher::SaveInnerGroups(
        const int fromGroupId,
        const int toGroupId,
        const bool reset,
        const Char *const input,
        ContStack &contStack)
    {
        if(toGroupId >= 0)
            DoSaveInnerGroups(fromGroupId, toGroupId, reset, input, contStack);
    }

    void Matcher::DoSaveInnerGroups(
        const int fromGroupId,
        const int toGroupId,
        const bool reset,
        const Char *const input,
        ContStack &contStack)
    {
        Assert(fromGroupId >= 0);
        Assert(toGroupId >= 0);
        Assert(fromGroupId <= toGroupId);

        int undefinedRangeFromId = -1;
        int groupId = fromGroupId;
        do
        {
            GroupInfo *const groupInfo = GroupIdToGroupInfo(groupId);
            if(groupInfo->IsUndefined())
            {
                if(undefinedRangeFromId < 0)
                    undefinedRangeFromId = groupId;
                continue;
            }

            if(undefinedRangeFromId >= 0)
            {
                Assert(groupId > 0);
                DoSaveInnerGroups_AllUndefined(undefinedRangeFromId, groupId - 1, input, contStack);
                undefinedRangeFromId = -1;
            }

            PUSH(contStack, RestoreGroupCont, groupId, *groupInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            PushStats(contStack, input);
#endif

            if(reset)
                groupInfo->Reset();
        } while(++groupId <= toGroupId);
        if(undefinedRangeFromId >= 0)
        {
            Assert(toGroupId >= 0);
            DoSaveInnerGroups_AllUndefined(undefinedRangeFromId, toGroupId, input, contStack);
        }
    }

    __inline void Matcher::SaveInnerGroups_AllUndefined(
        const int fromGroupId,
        const int toGroupId,
        const Char *const input,
        ContStack &contStack)
    {
        if(toGroupId >= 0)
            DoSaveInnerGroups_AllUndefined(fromGroupId, toGroupId, input, contStack);
    }

    void Matcher::DoSaveInnerGroups_AllUndefined(
        const int fromGroupId,
        const int toGroupId,
        const Char *const input,
        ContStack &contStack)
    {
        Assert(fromGroupId >= 0);
        Assert(toGroupId >= 0);
        Assert(fromGroupId <= toGroupId);

#if DBG
        for(int groupId = fromGroupId; groupId <= toGroupId; ++groupId)
        {
            Assert(GroupIdToGroupInfo(groupId)->IsUndefined());
        }
#endif

        if(fromGroupId == toGroupId)
            PUSH(contStack, ResetGroupCont, fromGroupId);
        else
            PUSH(contStack, ResetGroupRangeCont, fromGroupId, toGroupId);
#if ENABLE_REGEX_CONFIG_OPTIONS
        PushStats(contStack, input);
#endif
    }

    __inline void Matcher::ResetGroup(int groupId)
    {
        GroupInfo* info = GroupIdToGroupInfo(groupId);
        info->Reset();
    }

    __inline void Matcher::ResetInnerGroups(int minGroupId, int maxGroupId)
    {
        for (int i = minGroupId; i <= maxGroupId; i++)
            ResetGroup(i);
    }

    // ----------------------------------------------------------------------
    // Mixins
    // ----------------------------------------------------------------------

#if ENABLE_REGEX_CONFIG_OPTIONS
    void BackupMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"backup: ");
        backup.Print(w);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void CharMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"c: ");
        w->PrintQuotedChar(c);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void Char2Mixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"c0: ");
        w->PrintQuotedChar(cs[0]);
        w->Print(L", c1: ");
        w->PrintQuotedChar(cs[1]);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void Char3Mixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"c0: ");
        w->PrintQuotedChar(cs[0]);
        w->Print(L", c1: ");
        w->PrintQuotedChar(cs[1]);
        w->Print(L", c2: ");
        w->PrintQuotedChar(cs[2]);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void Char4Mixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"c0: ");
        w->PrintQuotedChar(cs[0]);
        w->Print(L", c1: ");
        w->PrintQuotedChar(cs[1]);
        w->Print(L", c2: ");
        w->PrintQuotedChar(cs[2]);
        w->Print(L", c3: ");
        w->PrintQuotedChar(cs[3]);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void LiteralMixin::Print(DebugWriter* w, const wchar_t* litbuf, bool isEquivClass) const
    {
        if (isEquivClass)
        {
            w->Print(L"equivLiterals: ");
            for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
            {
                if (i > 0)
                    w->Print(L", ");
                w->Print(L"\"");
                for (CharCount j = 0; j < length; j++)
                    w->PrintEscapedChar(litbuf[offset + j * CaseInsensitive::EquivClassSize + i]);
                w->Print(L"\"");
            }
        }
        else
        {
            w->Print(L"literal: ");
            w->PrintQuotedString(litbuf + offset, length);
        }
    }
#endif

    // ----------------------------------------------------------------------
    // Char2LiteralScannerMixin
    // ----------------------------------------------------------------------

    bool Char2LiteralScannerMixin::Match(Matcher& matcher, const wchar_t* const input, const CharCount inputLength, CharCount& inputOffset) const
    {
        if (inputLength == 0)
        {
            return false;
        }

        const uint matchC0 = Chars<wchar_t>::CTU(cs[0]);
        const uint matchC1 = Chars<wchar_t>::CTU(cs[1]);

        const wchar_t * currentInput = input + inputOffset;
        const wchar_t * endInput = input + inputLength - 1;

        while (currentInput < endInput)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            while (true)
            {
                const uint c1 = Chars<wchar_t>::CTU(currentInput[1]);
                if (c1 != matchC1)
                {
                    if (c1 == matchC0)
                    {
                        break;
                    }
                    currentInput += 2;
                    if (currentInput >= endInput)
                    {
                        return false;
                    }
                    continue;
                }
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.CompStats();
#endif
                // Check the first character
                const uint c0 = Chars<wchar_t>::CTU(*currentInput);
                if (c0 == matchC0)
                {
                    inputOffset = (CharCount)(currentInput - input);
                    return true;
                }
                if (matchC0 == matchC1)
                {
                    break;
                }
                currentInput +=2;
                if (currentInput >= endInput)
                {
                    return false;
                }
            }

            // If the second character in the buffer matches the first in the pattern, continue
            // to see if the next character has the second in the pattern
            currentInput++;
            while (currentInput < endInput)
            {
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.CompStats();
#endif
                const uint c1 = Chars<wchar_t>::CTU(currentInput[1]);
                if (c1 == matchC1)
                {
                    inputOffset = (CharCount)(currentInput - input);
                    return true;
                }
                if (c1 != matchC0)
                {
                    currentInput += 2;
                    break;
                }
                currentInput++;
            }
        }
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void Char2LiteralScannerMixin::Print(DebugWriter* w, const wchar_t * litbuf) const
    {
        Char2Mixin::Print(w, litbuf);
        w->Print(L" (with two character literal scanner)");
    }
#endif

    // ----------------------------------------------------------------------
    // ScannerMixinT
    // ----------------------------------------------------------------------

    template <typename ScannerT>
    void ScannerMixinT<ScannerT>::FreeBody(ArenaAllocator* rtAllocator)
    {
        scanner.FreeBody(rtAllocator, length);
    }

    template <typename ScannerT>
    __inline bool
    ScannerMixinT<ScannerT>::Match(Matcher& matcher, const wchar_t * const input, const CharCount inputLength, CharCount& inputOffset) const
    {
        Assert(length <= matcher.program->rep.insts.litbufLen - offset);
        return scanner.Match<1>
            ( input
            , inputLength
            , inputOffset
            , matcher.program->rep.insts.litbuf + offset
            , length
#if ENABLE_REGEX_CONFIG_OPTIONS
            , matcher.stats
#endif
            );
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template <typename ScannerT>
    void ScannerMixinT<ScannerT>::Print(DebugWriter* w, const wchar_t* litbuf, bool isEquivClass) const
    {
        LiteralMixin::Print(w, litbuf, isEquivClass);
        w->Print(L" (with %s scanner)", ScannerT::GetName());
    }
#endif

    // explicit instantiation
    template ScannerMixinT<TextbookBoyerMoore<wchar_t>>;
    template ScannerMixinT<TextbookBoyerMooreWithLinearMap<wchar_t>>;

    // ----------------------------------------------------------------------
    // EquivScannerMixinT
    // ----------------------------------------------------------------------

    template <uint lastPatCharEquivClassSize>
    __inline bool EquivScannerMixinT<lastPatCharEquivClassSize>::Match(Matcher& matcher, const wchar_t* const input, const CharCount inputLength, CharCount& inputOffset) const
    {
        Assert(length * CaseInsensitive::EquivClassSize <= matcher.program->rep.insts.litbufLen - offset);
        CompileAssert(lastPatCharEquivClassSize >= 1 && lastPatCharEquivClassSize <= CaseInsensitive::EquivClassSize);
        return scanner.Match<CaseInsensitive::EquivClassSize, lastPatCharEquivClassSize>
            ( input
            , inputLength
            , inputOffset
            , matcher.program->rep.insts.litbuf + offset
            , length
#if ENABLE_REGEX_CONFIG_OPTIONS
            , matcher.stats
#endif
            );
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template <uint lastPatCharEquivClassSize>
    void EquivScannerMixinT<lastPatCharEquivClassSize>::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        __super::Print(w, litbuf, true);
        w->Print(L" (last char equiv size:%d)", lastPatCharEquivClassSize);
    }

    // explicit instantiation
    template struct EquivScannerMixinT<1>;
#endif

    // ----------------------------------------------------------------------
    // ScannerInfo
    // ----------------------------------------------------------------------

#if ENABLE_REGEX_CONFIG_OPTIONS
    void ScannerInfo::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        ScannerMixin::Print(w, litbuf, isEquivClass);
    }
#endif

    ScannerInfo* ScannersMixin::Add(Recycler *recycler, Program *program, CharCount offset, CharCount length, bool isEquivClass)
    {
        Assert(numLiterals < MaxNumSyncLiterals);
        return program->AddScannerForSyncToLiterals(recycler, numLiterals++, offset, length, isEquivClass);
    }

    void ScannersMixin::FreeBody(ArenaAllocator* rtAllocator)
    {
        for (int i = 0; i < numLiterals; i++)
        {
            infos[i]->FreeBody(rtAllocator);
#if DBG
            infos[i] = 0;
#endif
        }
#if DBG
        numLiterals = 0;
#endif
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void ScannersMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"literals: {");
        for (int i = 0; i < numLiterals; i++)
        {
            if (i > 0)
                w->Print(L", ");
            infos[i]->Print(w, litbuf);
        }
        w->Print(L"}");
    }
#endif

    template<bool IsNegation>
    void SetMixin<IsNegation>::FreeBody(ArenaAllocator* rtAllocator)
    {
        set.FreeBody(rtAllocator);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<bool IsNegation>
    void SetMixin<IsNegation>::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"set: ");
        if (IsNegation)
            w->Print(L"not ");
        set.Print(w);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void HardFailMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"hardFail: %s", canHardFail ? L"true" : L"false");
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void GroupMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"groupId: %d", groupId);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void ChompBoundedMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"repeats: ");
        repeats.Print(w);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void JumpMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"targetLabel: L%04x", targetLabel);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void BodyGroupsMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"minBodyGroupId: %d, maxBodyGroupId: %d", minBodyGroupId, maxBodyGroupId);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void BeginLoopMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"loopId: %d, repeats: ", loopId);
        repeats.Print(w);
        w->Print(L", exitLabel: L%04x, hasOuterLoops: %s, hasInnerNondet: %s", exitLabel, hasOuterLoops ? L"true" : L"false", hasInnerNondet ? L"true" : L"false");
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void RepeatLoopMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"beginLabel: L%04x", beginLabel);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void TryMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"failLabel: L%04x", failLabel);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void FixedLengthMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"length: %u", length);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void NoNeedToSaveMixin::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->Print(L"noNeedToSave: %s", noNeedToSave ? L"true" : L"false");
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void SwitchCase::Print(DebugWriter* w) const
    {
        w->Print(L"case ");
        w->PrintQuotedChar(c);
        w->PrintEOL(L": Jump(L%04x)", targetLabel);
    }
#endif

    template <int n>
    void SwitchMixin<n>::AddCase(wchar_t c, Label targetLabel)
    {
        Assert(numCases < MaxCases);
        int i;
        __analysis_assume(numCases < MaxCases);
        for (i = 0; i < numCases; i++)
        {
            Assert(cases[i].c != c);
            if (cases[i].c > c)
                break;
        }
        __analysis_assume(numCases < MaxCases);
        for (int j = numCases; j > i; j--)
            cases[j] = cases[j - 1];
        cases[i].c = c;
        cases[i].targetLabel = targetLabel;
        numCases++;
    }

    void UnifiedRegexSwitchMixinForceAllInstantiations()
    {
        {
            SwitchMixin<10> x;
            x.AddCase(0, 0);
#if ENABLE_REGEX_CONFIG_OPTIONS
            x.Print(0, 0);
#endif
        }
        {
            SwitchMixin<20> x;
            x.AddCase(0, 0);
#if ENABLE_REGEX_CONFIG_OPTIONS
            x.Print(0, 0);
#endif
        }
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template <int n>
    void SwitchMixin<n>::Print(DebugWriter* w, const wchar_t* litbuf) const
    {
        w->EOL();
        w->Indent();
        for (int i = 0; i < numCases; i++)
            cases[i].Print(w);
        w->Unindent();
    }
#endif

    // ----------------------------------------------------------------------
    // FailInst
    // ----------------------------------------------------------------------

    __inline bool FailInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        return matcher.Fail(FAIL_PARAMETERS);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int FailInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: Fail()", label);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SuccInst
    // ----------------------------------------------------------------------

    __inline bool SuccInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        GroupInfo* info = matcher.GroupIdToGroupInfo(0);
        info->offset = matchStart;
        info->length = inputOffset - matchStart;
        return true; // STOP MATCHING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SuccInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: Succ()", label);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // JumpInst
    // ----------------------------------------------------------------------

    __inline bool JumpInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        instPointer = matcher.LabelToInstPointer(targetLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int JumpInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: Jump(", label);
        JumpMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // JumpIfNotCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool JumpIfNotCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && input[inputOffset] == c)
            instPointer += sizeof(*this);
        else
            instPointer = matcher.LabelToInstPointer(targetLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int JumpIfNotCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: JumpIfNotChar(", label);
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        JumpMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchCharOrJumpInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool MatchCharOrJumpInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && input[inputOffset] == c)
        {
            inputOffset++;
            instPointer += sizeof(*this);
        }
        else
            instPointer = matcher.LabelToInstPointer(targetLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchCharOrJumpInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchCharOrJump(", label);
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        JumpMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif


    // ----------------------------------------------------------------------
    // JumpIfNotSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool JumpIfNotSetInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && set.Get(input[inputOffset]))
            instPointer += sizeof(*this);
        else
            instPointer = matcher.LabelToInstPointer(targetLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int JumpIfNotSetInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: JumpIfNotSet(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L",  ");
        JumpMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchSetOrJumpInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool MatchSetOrJumpInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && set.Get(input[inputOffset]))
        {
            inputOffset++;
            instPointer += sizeof(*this);
        }
        else
            instPointer = matcher.LabelToInstPointer(targetLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchSetOrJumpInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchSetOrJump(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L",  ");
        JumpMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // Switch10Inst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool Switch10Inst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (inputOffset >= inputLength)
            return matcher.Fail(FAIL_PARAMETERS);
#if 0
        int l = 0;
        int h = numCases - 1;
        while (l <= h)
        {
            int m = (l + h) / 2;
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[m].c == input[inputOffset])
            {
                instPointer = matcher.LabelToInstPointer(cases[m].targetLabel);
                return false;
            }
            else if (cases[m].c < input[inputOffset])
                l = m + 1;
            else
                h = m - 1;
        }
#else
        const int localNumCases = numCases;
        for (int i = 0; i < localNumCases; i++)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[i].c == input[inputOffset])
            {
                instPointer = matcher.LabelToInstPointer(cases[i].targetLabel);
                return false;
            }
            else if (cases[i].c > input[inputOffset])
                break;
        }
#endif

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int Switch10Inst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: Switch10(", label);
        SwitchMixin<MaxCases>::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // Switch20Inst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool Switch20Inst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (inputOffset >= inputLength)
            return matcher.Fail(FAIL_PARAMETERS);
#if 0
        int l = 0;
        int h = numCases - 1;
        while (l <= h)
        {
            int m = (l + h) / 2;
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[m].c == input[inputOffset])
            {
                instPointer = matcher.LabelToInstPointer(cases[m].targetLabel);
                return false;
            }
            else if (cases[m].c < input[inputOffset])
                l = m + 1;
            else
                h = m - 1;
        }
#else
        const int localNumCases = numCases;
        for (int i = 0; i < localNumCases; i++)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[i].c == input[inputOffset])
            {
                instPointer = matcher.LabelToInstPointer(cases[i].targetLabel);
                return false;
            }
            else if (cases[i].c > input[inputOffset])
                break;
        }
#endif

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int Switch20Inst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: Switch20(", label);
        SwitchMixin<MaxCases>::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SwitchAndConsume10Inst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SwitchAndConsume10Inst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (inputOffset >= inputLength)
            return matcher.Fail(FAIL_PARAMETERS);
#if 0
        int l = 0;
        int h = numCases - 1;
        while (l <= h)
        {
            int m = (l + h) / 2;
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[m].c == input[inputOffset])
            {
                inputOffset++;
                instPointer = matcher.LabelToInstPointer(cases[m].targetLabel);
                return false;
            }
            else if (cases[m].c < input[inputOffset])
                l = m + 1;
            else
                h = m - 1;
        }
#else
        const int localNumCases = numCases;
        for (int i = 0; i < localNumCases; i++)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[i].c == input[inputOffset])
            {
                inputOffset++;
                instPointer = matcher.LabelToInstPointer(cases[i].targetLabel);
                return false;
            }
            else if (cases[i].c > input[inputOffset])
                break;
        }
#endif

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SwitchAndConsume10Inst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SwitchAndConsume10(", label);
        SwitchMixin<MaxCases>::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SwitchAndConsume20Inst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SwitchAndConsume20Inst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (inputOffset >= inputLength)
            return matcher.Fail(FAIL_PARAMETERS);
#if 0
        int l = 0;
        int h = numCases - 1;
        while (l <= h)
        {
            int m = (l + h) / 2;
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[m].c == input[inputOffset])
            {
                inputOffset++;
                instPointer = matcher.LabelToInstPointer(cases[m].targetLabel);
                return false;
            }
            else if (cases[m].c < input[inputOffset])
                l = m + 1;
            else
                h = m - 1;
        }
#else
        const int localNumCases = numCases;
        for (int i = 0; i < localNumCases; i++)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (cases[i].c == input[inputOffset])
            {
                inputOffset++;
                instPointer = matcher.LabelToInstPointer(cases[i].targetLabel);
                return false;
            }
            else if (cases[i].c > input[inputOffset])
                break;
        }
#endif

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SwitchAndConsume20Inst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SwitchAndConsume20(", label);
        SwitchMixin<MaxCases>::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BOITestInst
    // ----------------------------------------------------------------------

    __inline bool BOITestInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (inputOffset > 0)
        {
            if (canHardFail)
                // Clearly trying to start from later in the input won't help, and we know backtracking can't take us earlier in the input
                return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));
            else
                return matcher.Fail(FAIL_PARAMETERS);
        }
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BOITestInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BOITest(", label);
        HardFailMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // EOITestInst
    // ----------------------------------------------------------------------

    __inline bool EOITestInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (inputOffset < inputLength)
        {
            if (canHardFail)
                // We know backtracking can never take us later in the input, but starting from later in the input could help
                return matcher.HardFail(HARDFAIL_PARAMETERS(LaterOnly));
            else
                return matcher.Fail(FAIL_PARAMETERS);
        }
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int EOITestInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: EOITest(", label);
        HardFailMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BOLTestInst
    // ----------------------------------------------------------------------

    __inline bool BOLTestInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset > 0 && !matcher.standardChars->IsNewline(input[inputOffset - 1]))
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BOLTestInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: BOLTest()", label);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // EOLTestInst
    // ----------------------------------------------------------------------

    __inline bool EOLTestInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && !matcher.standardChars->IsNewline(input[inputOffset]))
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int EOLTestInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: EOLTest()", label);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // WordBoundaryTestInst
    // ----------------------------------------------------------------------

    __inline bool WordBoundaryTestInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        const bool prev = inputOffset > 0 && matcher.standardChars->IsWord(input[inputOffset - 1]);
        const bool curr = inputOffset < inputLength && matcher.standardChars->IsWord(input[inputOffset]);
        if (isNegation == (prev != curr))
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int WordBoundaryTestInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: WordBoundaryTest(isNegation: %s)", label, isNegation ? L"true" : L"false");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchCharInst
    // ----------------------------------------------------------------------

    __inline bool MatchCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset >= inputLength || input[inputOffset] != c)
            return matcher.Fail(FAIL_PARAMETERS);

        inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchChar(", label);
        CharMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchChar2Inst
    // ----------------------------------------------------------------------

    __inline bool MatchChar2Inst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset >= inputLength || (input[inputOffset] != cs[0] && input[inputOffset] != cs[1]))
            return matcher.Fail(FAIL_PARAMETERS);

        inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchChar2Inst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchChar2(", label);
        Char2Mixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchChar3Inst
    // ----------------------------------------------------------------------

    __inline bool MatchChar3Inst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset >= inputLength || (input[inputOffset] != cs[0] && input[inputOffset] != cs[1] && input[inputOffset] != cs[2]))
            return matcher.Fail(FAIL_PARAMETERS);

        inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchChar3Inst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchChar3(", label);
        Char3Mixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchChar4Inst
    // ----------------------------------------------------------------------

    __inline bool MatchChar4Inst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset >= inputLength || (input[inputOffset] != cs[0] && input[inputOffset] != cs[1] && input[inputOffset] != cs[2] && input[inputOffset] != cs[3]))
            return matcher.Fail(FAIL_PARAMETERS);

        inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchChar4Inst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchChar4(", label);
        Char4Mixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchSetInst
    // ----------------------------------------------------------------------

    template<bool IsNegation>
    __inline bool MatchSetInst<IsNegation>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset >= inputLength || set.Get(input[inputOffset]) == IsNegation)
            return matcher.Fail(FAIL_PARAMETERS);

        inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<bool IsNegation>
    int MatchSetInst<IsNegation>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchSet(", label);
        SetMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchLiteralInst
    // ----------------------------------------------------------------------

    __inline bool MatchLiteralInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        Assert(length <= matcher.program->rep.insts.litbufLen - offset);

        if (length > inputLength - inputOffset)
            return matcher.Fail(FAIL_PARAMETERS);

        const Char *const literalBuffer = matcher.program->rep.insts.litbuf;
        const Char * literalCurr = literalBuffer + offset;
        const Char * inputCurr = input + inputOffset;

#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (*literalCurr != *inputCurr)
        {
            inputOffset++;
            return matcher.Fail(FAIL_PARAMETERS);
        }

        const Char *const literalEnd = literalCurr + length;
        literalCurr++;
        inputCurr++;

        while (literalCurr < literalEnd)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (*literalCurr != *inputCurr++)
            {
                inputOffset = (CharCount)(inputCurr - input);
                return matcher.Fail(FAIL_PARAMETERS);
            }
            literalCurr++;
        }

        inputOffset = (CharCount)(inputCurr - input);
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchLiteralInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchLiteral(", label);
        LiteralMixin::Print(w, litbuf, false);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchLiteralEquivInst
    // ----------------------------------------------------------------------

    __inline bool MatchLiteralEquivInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (length > inputLength - inputOffset)
            return matcher.Fail(FAIL_PARAMETERS);

        const Char *const literalBuffer = matcher.program->rep.insts.litbuf;
        CharCount literalOffset = offset;
        const CharCount literalEndOffset = offset + length * CaseInsensitive::EquivClassSize;

        Assert(literalEndOffset <= matcher.program->rep.insts.litbufLen);
        CompileAssert(CaseInsensitive::EquivClassSize == 4);

        do
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            if (input[inputOffset] != literalBuffer[literalOffset]
                && input[inputOffset] != literalBuffer[literalOffset + 1]
                && input[inputOffset] != literalBuffer[literalOffset + 2]
                && input[inputOffset] != literalBuffer[literalOffset + 3])
            {
                return matcher.Fail(FAIL_PARAMETERS);
            }
            inputOffset++;
            literalOffset += CaseInsensitive::EquivClassSize;
        }
        while (literalOffset < literalEndOffset);

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchLiteralEquivInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchLiteralEquiv(", label);
        LiteralMixin::Print(w, litbuf, true);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchTrieInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool MatchTrieInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (!trie.Match
            ( input
            , inputLength
            , inputOffset
#if ENABLE_REGEX_CONFIG_OPTIONS
            , matcher.stats
#endif
            ))
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer += sizeof(*this);
        return false;
    }

    void MatchTrieInst::FreeBody(ArenaAllocator* rtAllocator)
    {
        trie.FreeBody(rtAllocator);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchTrieInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: MatchTrie(", label);
        trie.Print(w);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // OptMatchCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool OptMatchCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && input[inputOffset] == c)
            inputOffset++;

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int OptMatchCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: OptMatchChar(", label);
        CharMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // OptMatchSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool OptMatchSetInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && set.Get(input[inputOffset]))
            inputOffset++;

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int OptMatchSetInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: OptMatchSet(", label);
        SetMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToCharAndContinueInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SyncToCharAndContinueInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const Char matchC = c;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputLength && input[inputOffset] != matchC)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        matchStart = inputOffset;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SyncToCharAndContinueInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToCharAndContinue(", label);
        CharMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToChar2SetAndContinueInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SyncToChar2SetAndContinueInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const Char matchC0 = cs[0];
        const Char matchC1 = cs[1];
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputLength && input[inputOffset] != matchC0 && input[inputOffset] != matchC1)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        matchStart = inputOffset;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SyncToChar2SetAndContinueInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToChar2SetAndContinue(", label);
        Char2Mixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif


    // ----------------------------------------------------------------------
    // SyncToSetAndContinueInst (optimized instruction)
    // ----------------------------------------------------------------------

    template<bool IsNegation>
    __inline bool SyncToSetAndContinueInst<IsNegation>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const RuntimeCharSet<Char>& matchSet = set;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif

        while (inputOffset < inputLength && matchSet.Get(input[inputOffset]) == IsNegation)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        matchStart = inputOffset;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<bool IsNegation>
    int SyncToSetAndContinueInst<IsNegation>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToSetAndContinue(", label);
        SetMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToLiteralAndContinueInst (optimized instruction)
    // ----------------------------------------------------------------------

    template <typename ScannerT>
    __inline bool SyncToLiteralAndContinueInstT<ScannerT>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (!Match(matcher, input, inputLength, inputOffset))
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        matchStart = inputOffset;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template <typename ScannerT>
    int SyncToLiteralAndContinueInstT<ScannerT>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToLiteralAndContinue(", label);
        ScannerT::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }

    // explicit instantiation
    template struct SyncToLiteralAndContinueInstT<Char2LiteralScannerMixin>;
    template struct SyncToLiteralAndContinueInstT<ScannerMixin>;
    template struct SyncToLiteralAndContinueInstT<ScannerMixin_WithLinearCharMap>;
    template struct SyncToLiteralAndContinueInstT<EquivScannerMixin>;
    template struct SyncToLiteralAndContinueInstT<EquivTrivialLastPatCharScannerMixin>;
#endif

    // ----------------------------------------------------------------------
    // SyncToCharAndConsumeInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SyncToCharAndConsumeInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const Char matchC = c;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputLength && input[inputOffset] != matchC)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset >= inputLength)
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        matchStart = inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SyncToCharAndConsumeInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToCharAndConsume(", label);
        CharMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToChar2SetAndConsumeInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SyncToChar2SetAndConsumeInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const Char matchC0 = cs[0];
        const Char matchC1 = cs[1];
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputLength && (input[inputOffset] != matchC0 && input[inputOffset] != matchC1))
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset >= inputLength)
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        matchStart = inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SyncToChar2SetAndConsumeInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToChar2SetAndConsume(", label);
        Char2Mixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToSetAndConsumeInst (optimized instruction)
    // ----------------------------------------------------------------------

    template<bool IsNegation>
    __inline bool SyncToSetAndConsumeInst<IsNegation>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const RuntimeCharSet<Char>& matchSet = set;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputLength && matchSet.Get(input[inputOffset]) == IsNegation)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset >= inputLength)
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        matchStart = inputOffset++;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<bool IsNegation>
    int SyncToSetAndConsumeInst<IsNegation>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToSetAndConsume(", label);
        SetMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToLiteralAndConsumeInst (optimized instruction)
    // ----------------------------------------------------------------------

    template <typename ScannerT>
    __inline bool SyncToLiteralAndConsumeInstT<ScannerT>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (!Match(matcher, input, inputLength, inputOffset))
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        matchStart = inputOffset;
        inputOffset += ScannerT::GetLiteralLength();
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template <typename ScannerT>
    int SyncToLiteralAndConsumeInstT<ScannerT>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToLiteralAndConsume(", label);
        ScannerT::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }

    // explicit instantiation
    template struct SyncToLiteralAndConsumeInstT<Char2LiteralScannerMixin>;
    template struct SyncToLiteralAndConsumeInstT<ScannerMixin>;
    template struct SyncToLiteralAndConsumeInstT<ScannerMixin_WithLinearCharMap>;
    template struct SyncToLiteralAndConsumeInstT<EquivScannerMixin>;
    template struct SyncToLiteralAndConsumeInstT<EquivTrivialLastPatCharScannerMixin>;
#endif

    // ----------------------------------------------------------------------
    // SyncToCharAndBackupInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SyncToCharAndBackupInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (backup.lower > inputLength - matchStart)
            // Even match at very end doesn't allow for minimum backup
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        if(inputOffset < nextSyncInputOffset)
        {
            // We have not yet reached the offset in the input we last synced to before backing up, so it's unnecessary to sync
            // again since we'll sync to the same point in the input and back up to the same place we are at now
            instPointer += sizeof(*this);
            return false;
        }

        if (backup.lower > inputOffset - matchStart)
            // No use looking for match until minimum backup is possible
            inputOffset = matchStart + backup.lower;

        const Char matchC = c;
        while (inputOffset < inputLength && input[inputOffset] != matchC)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset >= inputLength)
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        nextSyncInputOffset = inputOffset + 1;

        if (backup.upper != CharCountFlag)
        {
            // Backup at most by backup.upper for new start
            CharCount maxBackup = inputOffset - matchStart;
            matchStart = inputOffset - min(maxBackup, (CharCount)backup.upper);
        }
        // else: leave start where it is

        // Move input to new match start
        inputOffset = matchStart;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SyncToCharAndBackupInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToCharAndBackup(", label);
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        BackupMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToSetAndBackupInst (optimized instruction)
    // ----------------------------------------------------------------------

    template<bool IsNegation>
    __inline bool SyncToSetAndBackupInst<IsNegation>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (backup.lower > inputLength - matchStart)
            // Even match at very end doesn't allow for minimum backup
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        if(inputOffset < nextSyncInputOffset)
        {
            // We have not yet reached the offset in the input we last synced to before backing up, so it's unnecessary to sync
            // again since we'll sync to the same point in the input and back up to the same place we are at now
            instPointer += sizeof(*this);
            return false;
        }

        if (backup.lower > inputOffset - matchStart)
            // No use looking for match until minimum backup is possible
            inputOffset = matchStart + backup.lower;

        const RuntimeCharSet<Char>& matchSet = set;
        while (inputOffset < inputLength && matchSet.Get(input[inputOffset]) == IsNegation)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset >= inputLength)
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        nextSyncInputOffset = inputOffset + 1;

        if (backup.upper != CharCountFlag)
        {
            // Backup at most by backup.upper for new start
            CharCount maxBackup = inputOffset - matchStart;
            matchStart = inputOffset - min(maxBackup, (CharCount)backup.upper);
        }
        // else: leave start where it is

        // Move input to new match start
        inputOffset = matchStart;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<bool IsNegation>
    int SyncToSetAndBackupInst<IsNegation>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToSetAndBackup(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L", ");
        BackupMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // SyncToLiteralAndBackupInst (optimized instruction)
    // ----------------------------------------------------------------------
    template <typename ScannerT>
    __inline bool SyncToLiteralAndBackupInstT<ScannerT>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (backup.lower > inputLength - matchStart)
            // Even match at very end doesn't allow for minimum backup
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        if(inputOffset < nextSyncInputOffset)
        {
            // We have not yet reached the offset in the input we last synced to before backing up, so it's unnecessary to sync
            // again since we'll sync to the same point in the input and back up to the same place we are at now
            instPointer += sizeof(*this);
            return false;
        }

        if (backup.lower > inputOffset - matchStart)
            // No use looking for match until minimum backup is possible
            inputOffset = matchStart + backup.lower;

        if (!Match(matcher, input, inputLength, inputOffset))
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        nextSyncInputOffset = inputOffset + 1;

        if (backup.upper != CharCountFlag)
        {
            // Set new start at most backup.upper from start of literal
            CharCount maxBackup = inputOffset - matchStart;
            matchStart = inputOffset - min(maxBackup, (CharCount)backup.upper);
        }
        // else: leave start where it is

        // Move input to new match start
        inputOffset = matchStart;

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template <typename ScannerT>
    int SyncToLiteralAndBackupInstT<ScannerT>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToLiteralAndBackup(", label);
        ScannerT::Print(w, litbuf);
        w->Print(L", ");
        BackupMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }

    // explicit instantiation
    template struct SyncToLiteralAndBackupInstT<Char2LiteralScannerMixin>;
    template struct SyncToLiteralAndBackupInstT<ScannerMixin>;
    template struct SyncToLiteralAndBackupInstT<ScannerMixin_WithLinearCharMap>;
    template struct SyncToLiteralAndBackupInstT<EquivScannerMixin>;
    template struct SyncToLiteralAndBackupInstT<EquivTrivialLastPatCharScannerMixin>;
#endif

    // ----------------------------------------------------------------------
    // SyncToLiteralsAndBackupInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool SyncToLiteralsAndBackupInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (backup.lower > inputLength - matchStart)
            // Even match at very end doesn't allow for minimum backup
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        if(inputOffset < nextSyncInputOffset)
        {
            // We have not yet reached the offset in the input we last synced to before backing up, so it's unnecessary to sync
            // again since we'll sync to the same point in the input and back up to the same place we are at now
            instPointer += sizeof(*this);
            return false;
        }

        if (backup.lower > inputOffset - matchStart)
            // No use looking for match until minimum backup is possible
            inputOffset = matchStart + backup.lower;

        int besti = -1;
        CharCount bestMatchOffset = 0;

        for (int i = 0; i < numLiterals; i++)
        {
            CharCount thisMatchOffset = inputOffset;

            if (infos[i]->isEquivClass ?
                    (infos[i]->scanner.Match<CaseInsensitive::EquivClassSize>
                    ( input
                    , inputLength
                    , thisMatchOffset
                    , matcher.program->rep.insts.litbuf + infos[i]->offset
                    , infos[i]->length
#if ENABLE_REGEX_CONFIG_OPTIONS
                    , matcher.stats
#endif
                    )) :
                    (infos[i]->scanner.Match<1>
                    ( input
                    , inputLength
                    , thisMatchOffset
                    , matcher.program->rep.insts.litbuf + infos[i]->offset
                    , infos[i]->length
#if ENABLE_REGEX_CONFIG_OPTIONS
                    , matcher.stats
#endif
                    )))
            {
                if (besti < 0 || thisMatchOffset < bestMatchOffset)
                {
                    besti = i;
                    bestMatchOffset = thisMatchOffset;
                }
            }
        }

        if (besti < 0)
            // No literals matched
            return matcher.HardFail(HARDFAIL_PARAMETERS(ImmediateFail));

        nextSyncInputOffset = bestMatchOffset + 1;

        if (backup.upper != CharCountFlag)
        {
            // Set new start at most backup.upper from start of literal
            CharCount maxBackup = bestMatchOffset - matchStart;
            matchStart = bestMatchOffset - min(maxBackup, (CharCount)backup.upper);
        }
        // else: leave start where it is

        // Move input to new match start
        inputOffset = matchStart;
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int SyncToLiteralsAndBackupInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: SyncToLiteralsAndBackup(", label);
        ScannersMixin::Print(w, litbuf);
        w->Print(L", ");
        BackupMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchGroupInst
    // ----------------------------------------------------------------------

    __inline bool MatchGroupInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        GroupInfo* const info = matcher.GroupIdToGroupInfo(groupId);
        if (!info->IsUndefined() && info->length > 0)
        {
            if (info->length > inputLength - inputOffset)
                return matcher.Fail(FAIL_PARAMETERS);

            CharCount groupOffset = info->offset;
            const CharCount groupEndOffset = groupOffset + info->length;

            bool isCaseInsensitiveMatch = (matcher.program->flags & IgnoreCaseRegexFlag) != 0;
            bool isCodePointList = (matcher.program->flags & UnicodeRegexFlag) != 0;

            // This is the only place in the runtime machinery we need to convert characters to their equivalence class
            if (isCaseInsensitiveMatch && isCodePointList)
            {
                auto getNextCodePoint = [=](CharCount &offset, CharCount endOffset, codepoint_t &codePoint) {
                    if (endOffset <= offset)
                    {
                        return false;
                    }

                    Char lowerPart = input[offset];
                    if (!Js::NumberUtilities::IsSurrogateLowerPart(lowerPart) || offset + 1 == endOffset)
                    {
                        codePoint = lowerPart;
                        offset += 1;
                        return true;
                    }

                    Char upperPart = input[offset + 1];
                    if (!Js::NumberUtilities::IsSurrogateUpperPart(upperPart))
                    {
                        codePoint = lowerPart;
                        offset += 1;
                    }
                    else
                    {
                        codePoint = Js::NumberUtilities::SurrogatePairAsCodePoint(lowerPart, upperPart);
                        offset += 2;
                    }

                    return true;
                };

                codepoint_t equivs[CaseInsensitive::EquivClassSize];
                while (true)
                {
                    codepoint_t groupCodePoint;
                    bool hasGroupCodePoint = getNextCodePoint(groupOffset, groupEndOffset, groupCodePoint);
                    if (!hasGroupCodePoint)
                    {
                        break;
                    }

                    // We don't need to verify that there is a valid input code point since at the beginning
                    // of the function, we make sure that the length of the input is at least as long as the
                    // length of the group.
                    codepoint_t inputCodePoint;
                    getNextCodePoint(inputOffset, inputLength, inputCodePoint);

                    bool doesMatch = false;
                    if (!Js::NumberUtilities::IsInSupplementaryPlane(groupCodePoint))
                    {
                        auto toCanonical = [&](codepoint_t c) {
                            return matcher.standardChars->ToCanonical(
                                CaseInsensitive::MappingSource::CaseFolding,
                                static_cast<wchar_t>(c));
                        };
                        doesMatch = (toCanonical(groupCodePoint) == toCanonical(inputCodePoint));
                    }
                    else
                    {
                        uint tblidx = 0;
                        uint acth = 0;
                        CaseInsensitive::RangeToEquivClass(tblidx, groupCodePoint, groupCodePoint, acth, equivs);
                        CompileAssert(CaseInsensitive::EquivClassSize == 4);
                        doesMatch =
                            inputCodePoint == equivs[0]
                            || inputCodePoint == equivs[1]
                            || inputCodePoint == equivs[2]
                            || inputCodePoint == equivs[3];
                    }

                    if (!doesMatch)
                    {
                        return matcher.Fail(FAIL_PARAMETERS);
                    }
                }
            }
            else if (isCaseInsensitiveMatch)
            {
                do
                {
#if ENABLE_REGEX_CONFIG_OPTIONS
                    matcher.CompStats();
#endif
                    auto toCanonical = [&](CharCount &offset) {
                        return matcher.standardChars->ToCanonical(CaseInsensitive::MappingSource::UnicodeData, input[offset++]);
                    };
                    if (toCanonical(groupOffset) != toCanonical(inputOffset))
                    {
                        return matcher.Fail(FAIL_PARAMETERS);
                    }
                }
                while (groupOffset < groupEndOffset);
            }
            else
            {
                do
                {
#if ENABLE_REGEX_CONFIG_OPTIONS
                    matcher.CompStats();
#endif
                    if (input[groupOffset++] != input[inputOffset++])
                        return matcher.Fail(FAIL_PARAMETERS);
                }
                while (groupOffset < groupEndOffset);
            }
        }
        // else: trivially match empty string

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int MatchGroupInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: MatchGroup(", label);
        GroupMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BeginDefineGroupInst
    // ----------------------------------------------------------------------

    __inline bool BeginDefineGroupInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        GroupInfo *const groupInfo = matcher.GroupIdToGroupInfo(groupId);
        Assert(groupInfo->IsUndefined());
        groupInfo->offset = inputOffset;
        Assert(groupInfo->IsUndefined());

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginDefineGroupInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BeginDefineGroup(", label);
        GroupMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // EndDefineGroupInst
    // ----------------------------------------------------------------------

    __inline bool EndDefineGroupInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (!noNeedToSave)
        {
            // UNDO ACTION: Restore group on backtrack
            PUSH(contStack, ResetGroupCont, groupId);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        GroupInfo *const groupInfo = matcher.GroupIdToGroupInfo(groupId);
        Assert(groupInfo->IsUndefined());
        Assert(inputOffset >= groupInfo->offset);
        groupInfo->length = inputOffset - groupInfo->offset;

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int EndDefineGroupInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: EndDefineGroup(", label);
        GroupMixin::Print(w, litbuf);
        w->Print(L", ");
        NoNeedToSaveMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // DefineGroupFixedInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool DefineGroupFixedInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (!noNeedToSave)
        {
            // UNDO ACTION: Restore group on backtrack
            PUSH(contStack, ResetGroupCont, groupId);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        GroupInfo *const groupInfo = matcher.GroupIdToGroupInfo(groupId);
        Assert(groupInfo->IsUndefined());
        groupInfo->offset = inputOffset - length;
        groupInfo->length = length;

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int DefineGroupFixedInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: DefineGroupFixed(", label);
        GroupMixin::Print(w, litbuf);
        w->Print(L", ");
        FixedLengthMixin::Print(w, litbuf);
        w->Print(L", ");
        NoNeedToSaveMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BeginLoopInst
    // ----------------------------------------------------------------------

    __inline bool BeginLoopInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(loopId);

        // If loop has outer loops, the continuation stack may have choicepoints from an earlier "run" of this loop
        // which, when backtracked to, may expect the loopInfo state to be as it was at the time the choicepoint was
        // pushed.
        //  - If the loop is greedy with deterministic body, there may be Resumes into the follow of the loop, but
        //    they won't look at the loopInfo state so there's nothing to do.
        //  - If the loop is greedy, or if it is non-greedy with lower > 0, AND it has a non-deterministic body,
        //    we may have Resume entries which will resume inside the loop body, which may then run to a
        //    RepeatLoop, which will then look at the loopInfo state. However, each iteration is protected by
        //    a RestoreLoop by RepeatLoopInst below. (****)
        //  - If the loop is non-greedy there may be a RepeatLoop on the stack, so we must restore the loopInfo
        //    state before backtracking to it.
        if (!isGreedy && hasOuterLoops)
        {
            PUSH(contStack, RestoreLoopCont, loopId, *loopInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        // The loop body must always begin with empty inner groups
        //  - if the loop is not in an outer they will be empty due to the reset when the match began
        //  - if the loop is in an outer loop, they will have been reset by the outer loop's RepeatLoop instruction
#if DBG
        for (int i = minBodyGroupId; i <= maxBodyGroupId; i++)
        {
            Assert(matcher.GroupIdToGroupInfo(i)->IsUndefined());
        }
#endif

        loopInfo->number = 0;
        loopInfo->startInputOffset = inputOffset;

        if (repeats.lower == 0)
        {
            if (isGreedy)
            {
                // CHOICEPOINT: Try one iteration of body, if backtrack continue from here with no iterations
                PUSH(contStack, ResumeCont, inputOffset, exitLabel);
                instPointer += sizeof(*this);
            }
            else
            {
                // CHOICEPOINT: Try no iterations of body, if backtrack do one iteration of body from here
                Assert(instPointer == (uint8*)this);
                PUSH(contStack, RepeatLoopCont, matcher.InstPointerToLabel(instPointer), inputOffset);
                instPointer = matcher.LabelToInstPointer(exitLabel);
            }
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }
        else
        {
            // Must match minimum iterations, so continue to loop body
            instPointer += sizeof(*this);
        }

        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginLoopInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BeginLoop(", label);
        BeginLoopMixin::Print(w, litbuf);
        w->Print(L", ");
        BodyGroupsMixin::Print(w, litbuf);
        w->PrintEOL(L", greedy: %s)", isGreedy ? L"true" : L"false");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RepeatLoopInst
    // ----------------------------------------------------------------------

    __inline bool RepeatLoopInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        BeginLoopInst* begin = matcher.L2I(BeginLoop, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        // See comment (****) above.
        if (begin->hasInnerNondet)
        {
            PUSH(contStack, RestoreLoopCont, begin->loopId, *loopInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        loopInfo->number++;

        if (loopInfo->number < begin->repeats.lower)
        {
            // Must match another iteration of body.
            loopInfo->startInputOffset = inputOffset;
            if(begin->hasInnerNondet)
            {
                // If it backtracks into the loop body of an earlier iteration, it must restore inner groups for that iteration.
                // Save the inner groups and reset them for the next iteration.
                matcher.SaveInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId, true, input, contStack);
            }
            else
            {
                // If it backtracks, the entire loop will fail, so no need to restore groups. Just reset the inner groups for
                // the next iteration.
                matcher.ResetInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId);
            }
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopInst));
        }
        else if (inputOffset == loopInfo->startInputOffset && loopInfo->number > begin->repeats.lower)
        {
            // The minimum number of iterations has been satisfied but the last iteration made no progress.
            //   - With greedy & deterministic body, FAIL so as to undo that iteration and restore group bindings.
            //   - With greedy & non-deterministic body, FAIL so as to try another body alternative
            //   - With non-greedy, we're trying an additional iteration because the follow failed. But
            //     since we didn't consume anything the follow will fail again, so fail
            //
            return matcher.Fail(FAIL_PARAMETERS);
        }
        else if (begin->repeats.upper != CharCountFlag && loopInfo->number >= (CharCount)begin->repeats.upper)
        {
            // Success: proceed to remainder.
            instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        }
        else if (begin->isGreedy)
        {
            // CHOICEPOINT: Try one more iteration of body, if backtrack continue from here with no more iterations
            PUSH(contStack, ResumeCont, inputOffset, begin->exitLabel);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
            loopInfo->startInputOffset = inputOffset;

            // If backtrack, we must continue with previous group bindings
            matcher.SaveInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId, true, input, contStack);
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopInst));
        }
        else
        {
            // CHOICEPOINT: Try no more iterations of body, if backtrack do one more iteration of body from here
            PUSH(contStack, RepeatLoopCont, beginLabel, inputOffset);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
            instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        }
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RepeatLoopInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: RepeatLoop(", label);
        RepeatLoopMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BeginLoopIfCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool BeginLoopIfCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && input[inputOffset] == c)
        {
            // Commit to at least one iteration of loop
            LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(loopId);

            // All inner groups must begin reset
#if DBG
            for (int i = minBodyGroupId; i <= maxBodyGroupId; i++)
            {
                Assert(matcher.GroupIdToGroupInfo(i)->IsUndefined());
            }
#endif
            loopInfo->number = 0;
            instPointer += sizeof(*this);
            return false;
        }

        if (repeats.lower > 0)
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer = matcher.LabelToInstPointer(exitLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginLoopIfCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BeginLoopIfChar(", label);
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        BeginLoopMixin::Print(w, litbuf);
        w->Print(L", ");
        BodyGroupsMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BeginLoopIfSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool BeginLoopIfSetInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && set.Get(input[inputOffset]))
        {
            // Commit to at least one iteration of loop
            LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(loopId);

            // All inner groups must be begin reset
#if DBG
            for (int i = minBodyGroupId; i <= maxBodyGroupId; i++)
            {
                Assert(matcher.GroupIdToGroupInfo(i)->IsUndefined());
            }
#endif

            loopInfo->startInputOffset = inputOffset;
            loopInfo->number = 0;
            instPointer += sizeof(*this);
            return false;
        }

        if (repeats.lower > 0)
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer = matcher.LabelToInstPointer(exitLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginLoopIfSetInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BeginLoopIfSet(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L", ");
        BeginLoopMixin::Print(w, litbuf);
        w->Print(L", ");
        BodyGroupsMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RepeatLoopIfCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool RepeatLoopIfCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        BeginLoopIfCharInst* begin = matcher.L2I(BeginLoopIfChar, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        if (begin->hasInnerNondet)
        {
            // May end up backtracking into loop body for iteration just completed: see above.
            PUSH(contStack, RestoreLoopCont, begin->loopId, *loopInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        loopInfo->number++;

#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && input[inputOffset] == begin->c)
        {
            if (begin->repeats.upper != CharCountFlag && loopInfo->number >= (CharCount)begin->repeats.upper)
            {
                // If the loop body's first set and the loop's follow set are disjoint, we can just fail here since
                // we know the next character in the input is in the loop body's first set.
                return matcher.Fail(FAIL_PARAMETERS);
            }

            // Commit to one more iteration
            if(begin->hasInnerNondet)
            {
                // If it backtracks into the loop body of an earlier iteration, it must restore inner groups for that iteration.
                // Save the inner groups and reset them for the next iteration.
                matcher.SaveInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId, true, input, contStack);
            }
            else
            {
                // If it backtracks, the entire loop will fail, so no need to restore groups. Just reset the inner groups for
                // the next iteration.
                matcher.ResetInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId);
            }
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopIfCharInst));
            return false;
        }

        if (loopInfo->number < begin->repeats.lower)
            return matcher.Fail(FAIL_PARAMETERS);

        // Proceed to exit
        instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RepeatLoopIfCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: RepeatLoopIfChar(%d, ", label);
        RepeatLoopMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RepeatLoopIfSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool RepeatLoopIfSetInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        BeginLoopIfSetInst* begin = matcher.L2I(BeginLoopIfSet, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        if (begin->hasInnerNondet)
        {
            // May end up backtracking into loop body for iteration just completed: see above.
            PUSH(contStack, RestoreLoopCont, begin->loopId, *loopInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        loopInfo->number++;

#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && begin->set.Get(input[inputOffset]))
        {
            if (begin->repeats.upper != CharCountFlag && loopInfo->number >= (CharCount)begin->repeats.upper)
            {
                // If the loop body's first set and the loop's follow set are disjoint, we can just fail here since
                // we know the next character in the input is in the loop body's first set.
                return matcher.Fail(FAIL_PARAMETERS);
            }

            // Commit to one more iteration
            if(begin->hasInnerNondet)
            {
                // If it backtracks into the loop body of an earlier iteration, it must restore inner groups for that iteration.
                // Save the inner groups and reset them for the next iteration.
                matcher.SaveInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId, true, input, contStack);
            }
            else
            {
                // If it backtracks, the entire loop will fail, so no need to restore groups. Just reset the inner groups for
                // the next iteration.
                matcher.ResetInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId);
            }
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopIfSetInst));
            return false;
        }

        if (loopInfo->number < begin->repeats.lower)
            return matcher.Fail(FAIL_PARAMETERS);

        // Proceed to exit
        instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RepeatLoopIfSetInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: RepeatLoopIfSet(", label);
        RepeatLoopMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BeginLoopFixedInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool BeginLoopFixedInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(loopId);

        // If loop is contained in an outer loop, continuation stack may already have a RewindLoopFixed entry for
        // this loop. We must make sure it's state is preserved on backtrack.
        if (hasOuterLoops)
        {
            PUSH(contStack, RestoreLoopCont, loopId, *loopInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        // startInputOffset will stay here for all iterations, and we'll use number of length to figure out
        // where in the input to rewind to
        loopInfo->number = 0;
        loopInfo->startInputOffset = inputOffset;

        if (repeats.lower == 0)
        {
            // CHOICEPOINT: Try one iteration of body. Failure of body will rewind input to here and resume with follow.
            Assert(instPointer == (uint8*)this);
            PUSH(contStack, RewindLoopFixedCont, matcher.InstPointerToLabel(instPointer), true);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }
        // else: Must match minimum iterations, so continue to loop body. Failure of body signals failure of entire loop.

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginLoopFixedInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BeginLoopFixed(", label);
        BeginLoopMixin::Print(w, litbuf);
        w->Print(L", ");
        FixedLengthMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RepeatLoopFixedInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool RepeatLoopFixedInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        BeginLoopFixedInst* begin = matcher.L2I(BeginLoopFixed, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        loopInfo->number++;

        if (loopInfo->number < begin->repeats.lower)
        {
            // Must match another iteration of body. Failure of body signals failure of the entire loop.
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopFixedInst));
        }
        else if (begin->repeats.upper != CharCountFlag && loopInfo->number >= (CharCount)begin->repeats.upper)
        {
            // Matched maximum number of iterations. Continue with follow.
            if (begin->repeats.lower < begin->repeats.upper)
            {
                // Failure of follow will try one fewer iterations (subject to repeats.lower).
                // Since loop body is non-deterministic and does not define groups the rewind continuation must be on top of the stack.
                Cont *top = contStack.Top();
                Assert(top != 0);
                Assert(top->tag == Cont::RewindLoopFixed);
                RewindLoopFixedCont* rewind = (RewindLoopFixedCont*)top;
                rewind->tryingBody = false;
            }
            // else: we never pushed a rewind continuation
            instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        }
        else
        {
            // CHOICEPOINT: Try one more iteration of body. Failure of body will rewind input to here and
            // try follow.
            if (loopInfo->number == begin->repeats.lower)
            {
                // i.e. begin->repeats.lower > 0, so continuation won't have been pushed in BeginLoopFixed
                PUSH(contStack, RewindLoopFixedCont, beginLabel, true);
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.PushStats(contStack, input);
#endif
            }
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopFixedInst));
        }
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RepeatLoopFixedInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: RepeatLoopFixed(", label);
        RepeatLoopMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // LoopSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool LoopSetInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(loopId);

        // If loop is contained in an outer loop, continuation stack may already have a RewindLoopFixed entry for
        // this loop. We must make sure it's state is preserved on backtrack.
        if (hasOuterLoops)
        {
            PUSH(contStack, RestoreLoopCont, loopId, *loopInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        // startInputOffset will stay here for all iterations, and we'll use number of length to figure out
        // where in the input to rewind to
        loopInfo->startInputOffset = inputOffset;

        // Consume as many elements of set as possible
        const RuntimeCharSet<Char>& matchSet = set;
        const CharCount loopMatchStart = inputOffset;
        const CharCountOrFlag repeatsUpper = repeats.upper;
        const CharCount inputEndOffset =
            static_cast<CharCount>(repeatsUpper) >= inputLength - inputOffset
                ? inputLength
                : inputOffset + static_cast<CharCount>(repeatsUpper);
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputEndOffset && matchSet.Get(input[inputOffset]))
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        loopInfo->number = inputOffset - loopMatchStart;
        if (loopInfo->number < repeats.lower)
            return matcher.Fail(FAIL_PARAMETERS);
        if (loopInfo->number > repeats.lower)
        {
            // CHOICEPOINT: If follow fails, try consuming one fewer characters
            Assert(instPointer == (uint8*)this);
            PUSH(contStack, RewindLoopSetCont, matcher.InstPointerToLabel(instPointer));
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }
        // else: failure of follow signals failure of entire loop

        // Continue with follow
        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int LoopSetInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: LoopSet(loopId: %d, ", label, loopId);
        repeats.Print(w);
        w->Print(L", hasOuterLoops: %s, ", hasOuterLoops ? L"true" : L"false");
        SetMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BeginLoopFixedGroupLastIterationInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool BeginLoopFixedGroupLastIterationInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        Assert(matcher.GroupIdToGroupInfo(groupId)->IsUndefined());

        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(loopId);

        // If loop is contained in an outer loop, continuation stack may already have a RewindLoopFixedGroupLastIteration entry
        // for this loop. We must make sure it's state is preserved on backtrack.
        if (hasOuterLoops)
        {
            PUSH(contStack, RestoreLoopCont, loopId, *loopInfo);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        // If loop is contained in an outer loop or assertion, we must reset the group binding if we backtrack all the way out
        if (!noNeedToSave)
        {
            PUSH(contStack, ResetGroupCont, groupId);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }

        // startInputOffset will stay here for all iterations, and we'll use number of length to figure out
        // where in the input to rewind to
        loopInfo->number = 0;
        loopInfo->startInputOffset = inputOffset;

        if (repeats.lower == 0)
        {
            // CHOICEPOINT: Try one iteration of body. Failure of body will rewind input to here and resume with follow.
            Assert(instPointer == (uint8*)this);
            PUSH(contStack, RewindLoopFixedGroupLastIterationCont, matcher.InstPointerToLabel(instPointer), true);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
        }
        // else: Must match minimum iterations, so continue to loop body. Failure of body signals failure of entire loop.

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginLoopFixedGroupLastIterationInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BeginLoopFixedGroupLastIteration(", label);
        BeginLoopMixin::Print(w, litbuf);
        w->Print(L", ");
        FixedLengthMixin::Print(w, litbuf);
        w->Print(L", ");
        GroupMixin::Print(w, litbuf);
        w->Print(L", ");
        NoNeedToSaveMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RepeatLoopFixedGroupLastIterationInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool RepeatLoopFixedGroupLastIterationInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        BeginLoopFixedGroupLastIterationInst* begin = matcher.L2I(BeginLoopFixedGroupLastIteration, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        loopInfo->number++;

        if (loopInfo->number < begin->repeats.lower)
        {
            // Must match another iteration of body. Failure of body signals failure of the entire loop.
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopFixedGroupLastIterationInst));
        }
        else if (begin->repeats.upper != CharCountFlag && loopInfo->number >= (CharCount)begin->repeats.upper)
        {
            // Matched maximum number of iterations. Continue with follow.
            if (begin->repeats.lower < begin->repeats.upper)
            {
                // Failure of follow will try one fewer iterations (subject to repeats.lower).
                // Since loop body is non-deterministic and does not define groups the rewind continuation must be on top of the stack.
                Cont *top = contStack.Top();
                Assert(top != 0);
                Assert(top->tag == Cont::RewindLoopFixedGroupLastIteration);
                RewindLoopFixedGroupLastIterationCont* rewind = (RewindLoopFixedGroupLastIterationCont*)top;
                rewind->tryingBody = false;
            }
            // else: we never pushed a rewind continuation

            // Bind group
            GroupInfo* groupInfo = matcher.GroupIdToGroupInfo(begin->groupId);
            groupInfo->offset = inputOffset - begin->length;
            groupInfo->length = begin->length;

            instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        }
        else
        {
            // CHOICEPOINT: Try one more iteration of body. Failure of body will rewind input to here and
            // try follow.
            if (loopInfo->number == begin->repeats.lower)
            {
                // i.e. begin->repeats.lower > 0, so continuation won't have been pushed in BeginLoopFixed
                PUSH(contStack, RewindLoopFixedGroupLastIterationCont, beginLabel, true);
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.PushStats(contStack, input);
#endif
            }
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopFixedGroupLastIterationInst));
        }
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RepeatLoopFixedGroupLastIterationInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: RepeatLoopFixedGroupLastIteration(", label);
        RepeatLoopMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif


    // ----------------------------------------------------------------------
    // BeginGreedyLoopNoBacktrackInst
    // ----------------------------------------------------------------------

    __inline bool BeginGreedyLoopNoBacktrackInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(loopId);

        loopInfo->number = 0;
        loopInfo->startInputOffset = inputOffset;

        // CHOICEPOINT: Try one iteration of body, if backtrack continue from here with no iterations
        PUSH(contStack, ResumeCont, inputOffset, exitLabel);
        instPointer += sizeof(*this);
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.PushStats(contStack, input);
#endif

        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginGreedyLoopNoBacktrackInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: BeginGreedyLoopNoBacktrack(loopId: %d)", label, loopId);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RepeatGreedyLoopNoBacktrackInst
    // ----------------------------------------------------------------------

    __inline bool RepeatGreedyLoopNoBacktrackInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        BeginGreedyLoopNoBacktrackInst* begin = matcher.L2I(BeginGreedyLoopNoBacktrack, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        loopInfo->number++;

        if (inputOffset == loopInfo->startInputOffset)
        {
            // No progress
            return matcher.Fail(FAIL_PARAMETERS);
        }
        else
        {
            // CHOICEPOINT: Try one more iteration of body, if backtrack, continue from here with no more iterations.
            // Since the loop body is deterministic and group free, it wouldn't have left any continuation records.
            // Therefore we can simply update the Resume continuation still on the top of the stack with the current
            // input pointer.
            Cont* top = contStack.Top();
            Assert(top != 0 && top->tag == Cont::Resume);
            ResumeCont* resume = (ResumeCont*)top;
            resume->origInputOffset = inputOffset;

            loopInfo->startInputOffset = inputOffset;
            instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginGreedyLoopNoBacktrackInst));
        }
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RepeatGreedyLoopNoBacktrackInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: RepeatGreedyLoopNoBacktrack(", label);
        RepeatLoopMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ChompCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    template<ChompMode Mode>
    __inline bool ChompCharInst<Mode>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const Char matchC = c;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if(Mode == ChompMode::Star || inputOffset < inputLength && input[inputOffset] == matchC)
        {
            while(true)
            {
                if(Mode != ChompMode::Star)
                    ++inputOffset;
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.CompStats();
#endif
                if(inputOffset < inputLength && input[inputOffset] == matchC)
                {
                    if(Mode == ChompMode::Star)
                        ++inputOffset;
                    continue;
                }
                break;
            }

            instPointer += sizeof(*this);
            return false;
        }

        return matcher.Fail(FAIL_PARAMETERS);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<ChompMode Mode>
    int ChompCharInst<Mode>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: ChompChar<%S>(", label, Mode == ChompMode::Star ? "Star" : "Plus");
        CharMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ChompSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    template<ChompMode Mode>
    __inline bool ChompSetInst<Mode>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const RuntimeCharSet<Char>& matchSet = set;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if(Mode == ChompMode::Star || inputOffset < inputLength && matchSet.Get(input[inputOffset]))
        {
            while(true)
            {
                if(Mode != ChompMode::Star)
                    ++inputOffset;
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.CompStats();
#endif
                if(inputOffset < inputLength && matchSet.Get(input[inputOffset]))
                {
                    if(Mode == ChompMode::Star)
                        ++inputOffset;
                    continue;
                }
                break;
            }

            instPointer += sizeof(*this);
            return false;
        }

        return matcher.Fail(FAIL_PARAMETERS);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<ChompMode Mode>
    int ChompSetInst<Mode>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: ChompSet<%S>(", label, Mode == ChompMode::Star ? "Star" : "Plus");
        SetMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ChompCharGroupInst (optimized instruction)
    // ----------------------------------------------------------------------

    template<ChompMode Mode>
    __inline bool ChompCharGroupInst<Mode>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        Assert(matcher.GroupIdToGroupInfo(groupId)->IsUndefined());

        const CharCount inputStartOffset = inputOffset;
        const Char matchC = c;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if(Mode == ChompMode::Star || inputOffset < inputLength && input[inputOffset] == matchC)
        {
            while(true)
            {
                if(Mode != ChompMode::Star)
                    ++inputOffset;
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.CompStats();
#endif
                if(inputOffset < inputLength && input[inputOffset] == matchC)
                {
                    if(Mode == ChompMode::Star)
                        ++inputOffset;
                    continue;
                }
                break;
            }

            if(!noNeedToSave)
            {
                // UNDO ACTION: Restore group on backtrack
                PUSH(contStack, ResetGroupCont, groupId);
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.PushStats(contStack, input);
#endif
            }

            GroupInfo *const groupInfo = matcher.GroupIdToGroupInfo(groupId);
            groupInfo->offset = inputStartOffset;
            groupInfo->length = inputOffset - inputStartOffset;

            instPointer += sizeof(*this);
            return false;
        }

        return matcher.Fail(FAIL_PARAMETERS);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<ChompMode Mode>
    int ChompCharGroupInst<Mode>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: ChompCharGroup<%S>(", label, Mode == ChompMode::Star ? "Star" : "Plus");
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        GroupMixin::Print(w, litbuf);
        w->Print(L", ");
        NoNeedToSaveMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ChompSetGroupInst (optimized instruction)
    // ----------------------------------------------------------------------

    template<ChompMode Mode>
    __inline bool ChompSetGroupInst<Mode>::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        Assert(matcher.GroupIdToGroupInfo(groupId)->IsUndefined());

        const CharCount inputStartOffset = inputOffset;
        const RuntimeCharSet<Char>& matchSet = set;
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if(Mode == ChompMode::Star || inputOffset < inputLength && matchSet.Get(input[inputOffset]))
        {
            while(true)
            {
                if(Mode != ChompMode::Star)
                    ++inputOffset;
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.CompStats();
#endif
                if(inputOffset < inputLength && matchSet.Get(input[inputOffset]))
                {
                    if(Mode == ChompMode::Star)
                        ++inputOffset;
                    continue;
                }
                break;
            }

            if(!noNeedToSave)
            {
                // UNDO ACTION: Restore group on backtrack
                PUSH(contStack, ResetGroupCont, groupId);
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.PushStats(contStack, input);
#endif
            }

            GroupInfo *const groupInfo = matcher.GroupIdToGroupInfo(groupId);
            groupInfo->offset = inputStartOffset;
            groupInfo->length = inputOffset - inputStartOffset;

            instPointer += sizeof(*this);
            return false;
        }

        return matcher.Fail(FAIL_PARAMETERS);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    template<ChompMode Mode>
    int ChompSetGroupInst<Mode>::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: ChompSetGroup<%S>(", label, Mode == ChompMode::Star ? "Star" : "Plus");
        SetMixin::Print(w, litbuf);
        w->Print(L", ");
        GroupMixin::Print(w, litbuf);
        w->Print(L", ");
        NoNeedToSaveMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ChompCharBoundedInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool ChompCharBoundedInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const Char matchC = c;
        const CharCount loopMatchStart = inputOffset;
        const CharCountOrFlag repeatsUpper = repeats.upper;
        const CharCount inputEndOffset =
            static_cast<CharCount>(repeatsUpper) >= inputLength - inputOffset
                ? inputLength
                : inputOffset + static_cast<CharCount>(repeatsUpper);
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputEndOffset && input[inputOffset] == matchC)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset - loopMatchStart < repeats.lower)
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int ChompCharBoundedInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: ChompCharBounded(", label);
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        ChompBoundedMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ChompSetBoundedInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool ChompSetBoundedInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        const RuntimeCharSet<Char>& matchSet = set;
        const CharCount loopMatchStart = inputOffset;
        const CharCountOrFlag repeatsUpper = repeats.upper;
        const CharCount inputEndOffset =
            static_cast<CharCount>(repeatsUpper) >= inputLength - inputOffset
                ? inputLength
                : inputOffset + static_cast<CharCount>(repeatsUpper);
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputEndOffset && matchSet.Get(input[inputOffset]))
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset - loopMatchStart < repeats.lower)
            return matcher.Fail(FAIL_PARAMETERS);

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int ChompSetBoundedInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: ChompSetBounded(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L", ");
        ChompBoundedMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ChompSetBoundedGroupLastCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool ChompSetBoundedGroupLastCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        Assert(matcher.GroupIdToGroupInfo(groupId)->IsUndefined());

        const RuntimeCharSet<Char>& matchSet = set;
        const CharCount loopMatchStart = inputOffset;
        const CharCountOrFlag repeatsUpper = repeats.upper;
        const CharCount inputEndOffset =
            static_cast<CharCount>(repeatsUpper) >= inputLength - inputOffset
                ? inputLength
                : inputOffset + static_cast<CharCount>(repeatsUpper);
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        while (inputOffset < inputEndOffset && matchSet.Get(input[inputOffset]))
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.CompStats();
#endif
            inputOffset++;
        }

        if (inputOffset - loopMatchStart < repeats.lower)
            return matcher.Fail(FAIL_PARAMETERS);

        if (inputOffset > loopMatchStart)
        {
            if (!noNeedToSave)
            {
                PUSH(contStack, ResetGroupCont, groupId);
#if ENABLE_REGEX_CONFIG_OPTIONS
                matcher.PushStats(contStack, input);
#endif
            }

            GroupInfo *const groupInfo = matcher.GroupIdToGroupInfo(groupId);
            groupInfo->offset  = inputOffset - 1;
            groupInfo->length = 1;
        }

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int ChompSetBoundedGroupLastCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: ChompSetBoundedGroupLastChar(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L", ");
        ChompBoundedMixin::Print(w, litbuf);
        w->Print(L", ");
        GroupMixin::Print(w, litbuf);
        w->Print(L", ");
        NoNeedToSaveMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // TryInst
    // ----------------------------------------------------------------------

    __inline bool TryInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        // CHOICEPOINT: Resume at fail label on backtrack
        PUSH(contStack, ResumeCont, inputOffset, failLabel);
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.PushStats(contStack, input);
#endif

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int TryInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: Try(", label);
        TryMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // TryIfCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool TryIfCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && input[inputOffset] == c)
        {
            // CHOICEPOINT: Resume at fail label on backtrack
            PUSH(contStack, ResumeCont, inputOffset, failLabel);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
            instPointer += sizeof(*this);
            return false;
        }

        // Proceed directly to exit
        instPointer = matcher.LabelToInstPointer(failLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int TryIfCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: TryIfChar(", label);
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        TryMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // TryMatchCharInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool TryMatchCharInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && input[inputOffset] == c)
        {
            // CHOICEPOINT: Resume at fail label on backtrack
            PUSH(contStack, ResumeCont, inputOffset, failLabel);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
            inputOffset++;
            instPointer += sizeof(*this);
            return false;
        }

        // Proceed directly to exit
        instPointer = matcher.LabelToInstPointer(failLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int TryMatchCharInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: TryMatchChar(", label);
        CharMixin::Print(w, litbuf);
        w->Print(L", ");
        TryMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // TryIfSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool TryIfSetInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && set.Get(input[inputOffset]))
        {
            // CHOICEPOINT: Resume at fail label on backtrack
            PUSH(contStack, ResumeCont, inputOffset, failLabel);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
            instPointer += sizeof(*this);
            return false;
        }

        // Proceed directly to exit
        instPointer = matcher.LabelToInstPointer(failLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int TryIfSetInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: TryIfSet(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L", ");
        TryMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // TryMatchSetInst (optimized instruction)
    // ----------------------------------------------------------------------

    __inline bool TryMatchSetInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.CompStats();
#endif
        if (inputOffset < inputLength && set.Get(input[inputOffset]))
        {
            // CHOICEPOINT: Resume at fail label on backtrack
            PUSH(contStack, ResumeCont, inputOffset, failLabel);
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.PushStats(contStack, input);
#endif
            inputOffset++;
            instPointer += sizeof(*this);
            return false;
        }

        // Proceed directly to exit
        instPointer = matcher.LabelToInstPointer(failLabel);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int TryMatchSetInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: TryMatchSet(", label);
        SetMixin::Print(w, litbuf);
        w->Print(L", ");
        TryMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // BeginAssertionInst
    // ----------------------------------------------------------------------

    __inline bool BeginAssertionInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        Assert(instPointer == (uint8*)this);

        if (!isNegation)
        {
            // If the positive assertion binds some groups then on success any RestoreGroup continuations pushed
            // in the assertion body will be cut. Hence if the entire assertion is backtracked over we must restore
            // the current inner group bindings.
            matcher.SaveInnerGroups(minBodyGroupId, maxBodyGroupId, false, input, contStack);
        }

        PUSHA(assertionStack, AssertionInfo, matcher.InstPointerToLabel(instPointer), inputOffset, contStack.Position());
        PUSH(contStack, PopAssertionCont);
#if ENABLE_REGEX_CONFIG_OPTIONS
        matcher.PushStats(contStack, input);
#endif

        instPointer += sizeof(*this);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int BeginAssertionInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->Print(L"L%04x: BeginAssertion(isNegation: %s, nextLabel: L%04x, ", label, isNegation ? L"true" : L"false", nextLabel);
        BodyGroupsMixin::Print(w, litbuf);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // EndAssertionInst
    // ----------------------------------------------------------------------

    __inline bool EndAssertionInst::Exec(REGEX_INST_EXEC_PARAMETERS) const
    {
        if (!matcher.PopAssertion(inputOffset, instPointer, contStack, assertionStack, true))
            // Body of negative assertion succeeded, so backtrack
            return matcher.Fail(FAIL_PARAMETERS);

        // else: body of positive assertion succeeded, instruction pointer already at next instruction
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int EndAssertionInst::Print(DebugWriter* w, Label label, const Char* litbuf) const
    {
        w->PrintEOL(L"L%04x: EndAssertion()", label);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // Matcher state
    // ----------------------------------------------------------------------

#if ENABLE_REGEX_CONFIG_OPTIONS
    void LoopInfo::Print(DebugWriter* w) const
    {
        w->Print(L"number: %u, startInputOffset: %u", number, startInputOffset);
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void GroupInfo::Print(DebugWriter* w, const Char* const input) const
    {
        if (IsUndefined())
            w->Print(L"<undefined> (%u)", offset);
        else
        {
            w->PrintQuotedString(input + offset, (CharCount)length);
            w->Print(L" (%u+%u)", offset, (CharCount)length);
        }
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void AssertionInfo::Print(DebugWriter* w) const
    {
        w->PrintEOL(L"beginLabel: L%04x, startInputOffset: %u, contStackPosition: $llu", beginLabel, startInputOffset, static_cast<unsigned long long>(contStackPosition));
    }
#endif

    // ----------------------------------------------------------------------
    // ResumeCont
    // ----------------------------------------------------------------------

    __inline bool ResumeCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        inputOffset = origInputOffset;
        instPointer = matcher.LabelToInstPointer(origInstLabel);
        return true; // STOP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int ResumeCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"Resume(origInputOffset: %u, origInstLabel: L%04x)", origInputOffset, origInstLabel);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RestoreLoopCont
    // ----------------------------------------------------------------------

    __inline bool RestoreLoopCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        matcher.QueryContinue(qcTicks);

        *matcher.LoopIdToLoopInfo(loopId) = origLoopInfo;
        return false; // KEEP BACKTRACKING
    }


#if ENABLE_REGEX_CONFIG_OPTIONS
    int RestoreLoopCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->Print(L"RestoreLoop(loopId: %d, ", loopId);
        origLoopInfo.Print(w);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RestoreGroupCont
    // ----------------------------------------------------------------------

    __inline bool RestoreGroupCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        *matcher.GroupIdToGroupInfo(groupId) = origGroupInfo;
        return false; // KEEP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RestoreGroupCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->Print(L"RestoreGroup(groupId: %d, ", groupId);
        origGroupInfo.Print(w, input);
        w->PrintEOL(L")");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ResetGroupCont
    // ----------------------------------------------------------------------

    __inline bool ResetGroupCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        matcher.ResetGroup(groupId);
        return false; // KEEP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int ResetGroupCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"ResetGroup(groupId: %d)", groupId);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // ResetGroupRangeCont
    // ----------------------------------------------------------------------

    __inline bool ResetGroupRangeCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        matcher.ResetInnerGroups(fromGroupId, toGroupId);
        return false; // KEEP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int ResetGroupRangeCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"ResetGroupRange(fromGroupId: %d, toGroupId: %d)", fromGroupId, toGroupId);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RepeatLoopCont
    // ----------------------------------------------------------------------

    __inline bool RepeatLoopCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        matcher.QueryContinue(qcTicks);

        // Try one more iteration of a non-greedy loop
        BeginLoopInst* begin = matcher.L2I(BeginLoop, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);
        loopInfo->startInputOffset = inputOffset = origInputOffset;
        instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(BeginLoopInst));
        if(begin->hasInnerNondet)
        {
            // If it backtracks into the loop body of an earlier iteration, it must restore inner groups for that iteration.
            // Save the inner groups and reset them for the next iteration.
            matcher.SaveInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId, true, input, contStack);
        }
        else
        {
            // If it backtracks, the entire loop will fail, so no need to restore groups. Just reset the inner groups for
            // the next iteration.
            matcher.ResetInnerGroups(begin->minBodyGroupId, begin->maxBodyGroupId);
        }
        return true; // STOP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RepeatLoopCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"RepeatLoop(beginLabel: L%04x, origInputOffset: %u)", beginLabel, origInputOffset);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // PopAssertionCont
    // ----------------------------------------------------------------------

    __inline bool PopAssertionCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        Assert(!assertionStack.IsEmpty());
        if (matcher.PopAssertion(inputOffset, instPointer, contStack, assertionStack, false))
            // Body of negative assertion failed
            return true; // STOP BACKTRACKING
        else
            // Body of positive assertion failed
            return false; // CONTINUE BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int PopAssertionCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"PopAssertion()");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RewindLoopFixedCont
    // ----------------------------------------------------------------------

    __inline bool RewindLoopFixedCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        matcher.QueryContinue(qcTicks);

        BeginLoopFixedInst* begin = matcher.L2I(BeginLoopFixed, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        if (tryingBody)
        {
            tryingBody = false;
            // loopInfo->number is the number of iterations completed before trying body
            Assert(loopInfo->number >= begin->repeats.lower);
        }
        else
        {
            // loopInfo->number is the number of iterations completed before trying follow
            Assert(loopInfo->number > begin->repeats.lower);
            // Try follow with one fewer iteration
            loopInfo->number--;
        }

        // Rewind input
        inputOffset = loopInfo->startInputOffset + loopInfo->number * begin->length;

        if (loopInfo->number > begin->repeats.lower)
        {
            // Un-pop the continuation ready for next time
            contStack.UnPop<RewindLoopFixedCont>();
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.UnPopStats(contStack, input);
#endif
        }
        // else: Can't try any fewer iterations if follow fails, so leave continuation as popped and let failure propagate

        instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        return true; // STOP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RewindLoopFixedCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"RewindLoopFixed(beginLabel: L%04x, tryingBody: %s)", beginLabel, tryingBody ? L"true" : L"false");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RewindLoopSetCont
    // ----------------------------------------------------------------------

    __inline bool RewindLoopSetCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        matcher.QueryContinue(qcTicks);

        LoopSetInst* begin = matcher.L2I(LoopSet, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);

        // >loopInfonumber is the number of iterations completed before trying follow
        Assert(loopInfo->number > begin->repeats.lower);
        // Try follow with one fewer iteration
        loopInfo->number--;

        // Rewind input
        inputOffset = loopInfo->startInputOffset + loopInfo->number;

        if (loopInfo->number > begin->repeats.lower)
        {
            // Un-pop the continuation ready for next time
            contStack.UnPop<RewindLoopSetCont>();
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.UnPopStats(contStack, input);
#endif
        }
        // else: Can't try any fewer iterations if follow fails, so leave continuation as popped and let failure propagate

        instPointer = matcher.LabelToInstPointer(beginLabel + sizeof(LoopSetInst));
        return true; // STOP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RewindLoopSetCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"RewindLoopSet(beginLabel: L%04x)", beginLabel);
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // RewindLoopFixedGroupLastIterationCont
    // ----------------------------------------------------------------------

    __inline bool RewindLoopFixedGroupLastIterationCont::Exec(REGEX_CONT_EXEC_PARAMETERS)
    {
        matcher.QueryContinue(qcTicks);

        BeginLoopFixedGroupLastIterationInst* begin = matcher.L2I(BeginLoopFixedGroupLastIteration, beginLabel);
        LoopInfo* loopInfo = matcher.LoopIdToLoopInfo(begin->loopId);
        GroupInfo* groupInfo = matcher.GroupIdToGroupInfo(begin->groupId);

        if (tryingBody)
        {
            tryingBody = false;
            // loopInfo->number is the number of iterations completed before current attempt of body
            Assert(loopInfo->number >= begin->repeats.lower);
        }
        else
        {
            // loopInfo->number is the number of iterations completed before trying follow
            Assert(loopInfo->number > begin->repeats.lower);
            // Try follow with one fewer iteration
            loopInfo->number--;
        }

        // Rewind input
        inputOffset = loopInfo->startInputOffset + loopInfo->number * begin->length;

        if (loopInfo->number > 0)
        {
            // Bind previous iteration's body
            groupInfo->offset = inputOffset - begin->length;
            groupInfo->length = begin->length;
        }
        else
            groupInfo->Reset();

        if (loopInfo->number > begin->repeats.lower)
        {
            // Un-pop the continuation ready for next time
            contStack.UnPop<RewindLoopFixedGroupLastIterationCont>();
#if ENABLE_REGEX_CONFIG_OPTIONS
            matcher.UnPopStats(contStack, input);
#endif
        }
        // else: Can't try any fewer iterations if follow fails, so leave continuation as popped and let failure propagate

        instPointer = matcher.LabelToInstPointer(begin->exitLabel);
        return true; // STOP BACKTRACKING
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    int RewindLoopFixedGroupLastIterationCont::Print(DebugWriter* w, const Char* const input) const
    {
        w->PrintEOL(L"RewindLoopFixedGroupLastIteration(beginLabel: L%04x, tryingBody: %s)", beginLabel, tryingBody ? L"true" : L"false");
        return sizeof(*this);
    }
#endif

    // ----------------------------------------------------------------------
    // Matcher
    // ----------------------------------------------------------------------

#if ENABLE_REGEX_CONFIG_OPTIONS
    void ContStack::Print(DebugWriter* w, const Char* const input) const
    {
        for(Iterator it(*this); it; ++it)
        {
            w->Print(L"%4llu: ", static_cast<unsigned long long>(it.Position()));
            it->Print(w, input);
        }
    }
#endif

#if ENABLE_REGEX_CONFIG_OPTIONS
    void AssertionStack::Print(DebugWriter* w, const Matcher* matcher) const
    {
        for(Iterator it(*this); it; ++it)
        {
            it->Print(w);
        }
    }
#endif

    Matcher::Matcher(Js::ScriptContext* scriptContext, RegexPattern* pattern)
        : pattern(pattern)
        , standardChars(scriptContext->GetThreadContext()->GetStandardChars((wchar_t*)0))
        , program(pattern->rep.unified.program)
        , groupInfos(nullptr)
        , loopInfos(nullptr)
        , previousQcTime(0)
#if ENABLE_REGEX_CONFIG_OPTIONS
        , stats(0)
        , w(0)
#endif
    {
        const auto recycler = scriptContext->GetRecycler();

        // Don't need to zero out - the constructor for GroupInfo should take care of it
        groupInfos = RecyclerNewArrayLeaf(recycler, GroupInfo, program->numGroups);

        if (program->numLoops > 0)
        {
            loopInfos = RecyclerNewArrayLeafZ(recycler, LoopInfo, program->numLoops);
        }
    }

    Matcher *Matcher::New(Js::ScriptContext* scriptContext, RegexPattern* pattern)
    {
        return RecyclerNew(scriptContext->GetRecycler(), Matcher, scriptContext, pattern);
    }

    Matcher *Matcher::CloneToScriptContext(Js::ScriptContext *scriptContext, RegexPattern *pattern)
    {
        Matcher *result = New(scriptContext, pattern);
        if (groupInfos)
        {
            size_t size = program->numGroups * sizeof(GroupInfo);
            js_memcpy_s(result->groupInfos, size, groupInfos, size);
        }
        if (loopInfos)
        {
            size_t size = program->numLoops * sizeof(LoopInfo);
            js_memcpy_s(result->loopInfos, size, loopInfos, size);
        }

        return result;
    }

#if DBG
    const uint32 contTags[] = {
#define M(O) Cont::O,
#include "RegexContcodes.h"
#undef M
    };

    const uint32 minContTag = contTags[0];
    const uint32 maxContTag = contTags[(sizeof(contTags) / sizeof(uint32)) - 1];
#endif

    void Matcher::DoQueryContinue(const uint qcTicks)
    {
        // See definition of TimePerQc for description of regex QC heuristics

        const uint before = previousQcTime;
        const uint now = GetTickCount();
        if((!before || now - before < TimePerQc) && qcTicks & TicksPerQc - 1)
            return;

        previousQcTime = now;
        TraceQueryContinue(now);

        // Query-continue can be reentrant and run the same regex again. To prevent the matcher and other persistent objects
        // from being reused reentrantly, save and restore them around the QC call.
        class AutoCleanup
        {
        private:
            RegexPattern *const pattern;
            Matcher *const matcher;
            RegexStacks * regexStacks;

        public:
            AutoCleanup(RegexPattern *const pattern, Matcher *const matcher) : pattern(pattern), matcher(matcher)
            {
                Assert(pattern);
                Assert(matcher);
                Assert(pattern->rep.unified.matcher == matcher);

                pattern->rep.unified.matcher = nullptr;

                const auto scriptContext = pattern->GetScriptContext();
                regexStacks = scriptContext->SaveRegexStacks();
            }

            ~AutoCleanup()
            {
                pattern->rep.unified.matcher = matcher;

                const auto scriptContext = pattern->GetScriptContext();
                scriptContext->RestoreRegexStacks(regexStacks);
            }
        } autoCleanup(pattern, this);

        pattern->GetScriptContext()->GetThreadContext()->CheckScriptInterrupt();
    }

    void Matcher::TraceQueryContinue(const uint now)
    {
        if(!PHASE_TRACE1(Js::RegexQcPhase))
            return;

        Output::Print(L"Regex QC");

        static uint n = 0;
        static uint firstQcTime = 0;

        ++n;
        if(firstQcTime)
            Output::Print(L" - frequency: %0.1f", static_cast<double>(n * 1000) / (now - firstQcTime));
        else
            firstQcTime = now;

        Output::Print(L"\n");
        Output::Flush();
    }

    bool Matcher::Fail(const Char* const input, CharCount &inputOffset, const uint8 *&instPointer, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks)
    {
        if (!contStack.IsEmpty())
        {
            if (!RunContStack(input, inputOffset, instPointer, contStack, assertionStack, qcTicks))
            {
                return false;
            }
        }

        Assert(assertionStack.IsEmpty());
        groupInfos[0].Reset();
        return true; // STOP EXECUTION
    }

    __inline bool Matcher::RunContStack(const Char* const input, CharCount &inputOffset, const uint8 *&instPointer, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks)
    {
        while (true)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            PopStats(contStack, input);
#endif
            Cont* cont = contStack.Pop();
            if (cont == 0)
                break;

            Assert(cont->tag >= minContTag && cont->tag <= maxContTag);
            // All these cases RESUME EXECUTION if backtracking finds a stop point
            const Cont::ContTag tag = cont->tag;
            switch (tag)
            {
#define M(O) case Cont::O: if (((O##Cont*)cont)->Exec(*this, input, inputOffset, instPointer, contStack, assertionStack, qcTicks)) return false; break;
#include "RegexContcodes.h"
#undef M
            default:
                Assert(false); // should never be reached
                return false;  // however, can't use complier optimization if we wnat to return false here
            }
        }
        return true;
    }

#if DBG
    const uint32 instTags[] = {
#define M(TagName) Inst::TagName,
#define MTemplate(TagName, ...) M(TagName)
#include "RegexOpcodes.h"
#undef M
#undef MTemplate
    };

    const uint32 minInstTag = instTags[0];
    const uint32 maxInstTag = instTags[(sizeof(instTags) / sizeof(uint32)) - 1];
#endif

    __inline void Matcher::Run(const Char* const input, const CharCount inputLength, CharCount &matchStart, CharCount &nextSyncInputOffset, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks)
    {
        CharCount inputOffset = matchStart;
        const uint8 *instPointer = program->rep.insts.insts;
        Assert(instPointer != 0);

        while (true)
        {
            Assert(inputOffset >= matchStart && inputOffset <= inputLength);
            Assert(instPointer >= program->rep.insts.insts && instPointer < program->rep.insts.insts + program->rep.insts.instsLen);
            Assert(((Inst*)instPointer)->tag >= minInstTag && ((Inst*)instPointer)->tag <= maxInstTag);
#if ENABLE_REGEX_CONFIG_OPTIONS
            if (w != 0)
                Print(w, input, inputLength, inputOffset, instPointer, contStack, assertionStack);
            InstStats();
#endif
            const Inst *inst = (const Inst*)instPointer;
            const Inst::InstTag tag = inst->tag;
            switch (tag)
            {
#define MBase(TagName, ClassName) \
                case Inst::TagName: \
                    if (((const ClassName *)inst)->Exec(*this, input, inputLength, matchStart, inputOffset, nextSyncInputOffset, instPointer, contStack, assertionStack, qcTicks)) \
                        return; \
                    break;
#define M(TagName) MBase(TagName, TagName##Inst)
#define MTemplate(TagName, TemplateDeclaration, GenericClassName, SpecializedClassName) MBase(TagName, SpecializedClassName)
#include "RegexOpcodes.h"
#undef MBase
#undef M
#undef MTemplate
            default:
                Assert(false);
                __assume(false);
            }
        }
    }

#if DBG
    void Matcher::ResetLoopInfos()
    {
        for (int i = 0; i < program->numLoops; i++)
            loopInfos[i].Reset();
    }
#endif

    __inline bool Matcher::MatchHere(const Char* const input, const CharCount inputLength, CharCount &matchStart, CharCount &nextSyncInputOffset, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks)
    {
        // Reset the continuation and assertion stacks ready for fresh run
        // NOTE: We used to do this after the Run, but it's safer to do it here in case unusual control flow exits
        //       the matcher without executing the clears.
        contStack.Clear();
        // assertionStack may be non-empty since we can hard fail directly out of matcher without popping assertion
        assertionStack.Clear();

        Assert(contStack.IsEmpty());
        Assert(assertionStack.IsEmpty());

        ResetInnerGroups(0, program->numGroups - 1);
#if DBG
        ResetLoopInfos();
#endif

        Run(input, inputLength, matchStart, nextSyncInputOffset, contStack, assertionStack, qcTicks);

        // Leave the continuation and assertion stack memory in place so we don't have to alloc next time

        return WasLastMatchSuccessful();
    }

    __inline bool Matcher::MatchSingleCharCaseInsensitive(const Char* const input, const CharCount inputLength, CharCount offset, const Char c)
    {
        CaseInsensitive::MappingSource mappingSource = program->GetCaseMappingSource();

        // If sticky flag is present, break since the 1st character didn't match the pattern character
        if ((program->flags & StickyRegexFlag) != 0)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            CompStats();
#endif
            if (MatchSingleCharCaseInsensitiveHere(mappingSource, input, offset, c))
            {
                GroupInfo* const info = GroupIdToGroupInfo(0);
                info->offset = offset;
                info->length = 1;
                return true;
            }
            else
            {
                ResetGroup(0);
                return false;
            }
        }

        while (offset < inputLength)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            CompStats();
#endif
            if (MatchSingleCharCaseInsensitiveHere(mappingSource, input, offset, c))
            {
                GroupInfo* const info = GroupIdToGroupInfo(0);
                info->offset = offset;
                info->length = 1;
                return true;
            }
            offset++;
        }

        ResetGroup(0);
        return false;
    }

    __inline bool Matcher::MatchSingleCharCaseInsensitiveHere(
        CaseInsensitive::MappingSource mappingSource,
        const Char* const input,
        const CharCount offset,
        const Char c)
    {
        return (standardChars->ToCanonical(mappingSource, input[offset]) == standardChars->ToCanonical(mappingSource, c));
    }

    __inline bool Matcher::MatchSingleCharCaseSensitive(const Char* const input, const CharCount inputLength, CharCount offset, const Char c)
    {
        // If sticky flag is present, break since the 1st character didn't match the pattern character
        if ((program->flags & StickyRegexFlag) != 0)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            CompStats();
#endif
            if (input[offset] == c)
            {
                GroupInfo* const info = GroupIdToGroupInfo(0);
                info->offset = offset;
                info->length = 1;
                return true;
            }
            else
            {
                ResetGroup(0);
                return false;
            }
        }

        while (offset < inputLength)
        {
#if ENABLE_REGEX_CONFIG_OPTIONS
            CompStats();
#endif
            if (input[offset] == c)
            {
                GroupInfo* const info = GroupIdToGroupInfo(0);
                info->offset = offset;
                info->length = 1;
                return true;
            }
            offset++;
        }

        ResetGroup(0);
        return false;
    }

    __inline bool Matcher::MatchBoundedWord(const Char* const input, const CharCount inputLength, CharCount offset)
    {
        const StandardChars<Char>& stdchrs = *standardChars;

        if (offset >= inputLength)
        {
            ResetGroup(0);
            return false;
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        CompStats();
#endif

        if ((offset == 0 && stdchrs.IsWord(input[0])) ||
            (offset > 0 && (!stdchrs.IsWord(input[offset - 1]) && stdchrs.IsWord(input[offset]))))
        {
            // Already at start of word
        }
        // If sticky flag is present, return false since we are not at the beginning of the word yet
        else if ((program->flags & StickyRegexFlag) == StickyRegexFlag)
        {
            ResetGroup(0);
            return false;
        }
        else
        {
            if (stdchrs.IsWord(input[offset]))
            {
                // Scan for end of current word
                while (true)
                {
                    offset++;
                    if (offset >= inputLength)
                    {
                        ResetGroup(0);
                        return false;
                    }
#if ENABLE_REGEX_CONFIG_OPTIONS
                    CompStats();
#endif
                    if (!stdchrs.IsWord(input[offset]))
                        break;
                }
            }

            // Scan for start of next word
            while (true)
            {
                offset++;
                if (offset >= inputLength)
                {
                    ResetGroup(0);
                    return false;
                }
#if ENABLE_REGEX_CONFIG_OPTIONS
                CompStats();
#endif
                if (stdchrs.IsWord(input[offset]))
                    break;
            }
        }

        GroupInfo* const info = GroupIdToGroupInfo(0);
        info->offset = offset;

        // Scan for end of word
        do
        {
            offset++;
#if ENABLE_REGEX_CONFIG_OPTIONS
            CompStats();
#endif
        }
        while (offset < inputLength && stdchrs.IsWord(input[offset]));

        info->length = offset - info->offset;
        return true;
    }

    __inline bool Matcher::MatchLeadingTrailingSpaces(const Char* const input, const CharCount inputLength, CharCount offset)
    {
        GroupInfo* const info = GroupIdToGroupInfo(0);
        Assert(offset <= inputLength);
        Assert((program->flags & MultilineRegexFlag) == 0);

        if (offset >= inputLength)
        {
            Assert(offset == inputLength);
            if (program->rep.leadingTrailingSpaces.endMinMatch == 0 ||
                (offset == 0 && program->rep.leadingTrailingSpaces.beginMinMatch == 0))
            {
                info->offset = offset;
                info->length = 0;
                return true;
            }
            info->Reset();
            return false;
        }

        const StandardChars<Char> &stdchrs = *standardChars;
        if (offset == 0)
        {
            while (offset < inputLength && stdchrs.IsWhitespaceOrNewline(input[offset]))
            {
                offset++;
#if ENABLE_REGEX_CONFIG_OPTIONS
                CompStats();
#endif
            }
            if (offset >= program->rep.leadingTrailingSpaces.beginMinMatch)
            {
                info->offset = 0;
                info->length = offset;
                return true;
            }
        }

        Assert(inputLength > 0);
        const CharCount initOffset = offset;
        offset = inputLength - 1;
        while (offset >= initOffset && stdchrs.IsWhitespaceOrNewline(input[offset]))
        {
            // This can never underflow since initOffset > 0
            Assert(offset > 0);
            offset--;
#if ENABLE_REGEX_CONFIG_OPTIONS
            CompStats();
#endif
        }
        offset++;
        CharCount length = inputLength - offset;
        if (length >= program->rep.leadingTrailingSpaces.endMinMatch)
        {
            info->offset = offset;
            info->length = length;
            return true;
        }
        info->Reset();
        return false;
    }

    __inline bool Matcher::MatchOctoquad(const Char* const input, const CharCount inputLength, CharCount offset, OctoquadMatcher* matcher)
    {
        if (matcher->Match
            ( input
            , inputLength
            , offset
#if ENABLE_REGEX_CONFIG_OPTIONS
            , stats
#endif
            ))
        {
            GroupInfo* const info = GroupIdToGroupInfo(0);
            info->offset = offset;
            info->length = TrigramInfo::PatternLength;
            return true;
        }
        else
        {
            ResetGroup(0);
            return false;
        }
    }

    __inline bool Matcher::MatchBOILiteral2(const Char* const input, const CharCount inputLength, CharCount offset, DWORD literal2)
    {
        if (offset == 0 && inputLength >= 2)
        {
            CompileAssert(sizeof(Char) == 2);
            const Program * program = this->program;
            if (program->rep.boiLiteral2.literal == *(DWORD *)input)
            {
                GroupInfo* const info = GroupIdToGroupInfo(0);
                info->offset = 0;
                info->length = 2;
                return true;
            }
        }
        ResetGroup(0);
        return false;
    }

    bool Matcher::Match
        ( const Char* const input
        , const CharCount inputLength
        , CharCount offset
        , Js::ScriptContext * scriptContext
#if ENABLE_REGEX_CONFIG_OPTIONS
        , RegexStats* stats
        , DebugWriter* w
#endif
        )
    {
#if ENABLE_REGEX_CONFIG_OPTIONS
        this->stats = stats;
        this->w = w;
#endif

        Assert(offset <= inputLength);
        bool res;
        bool loopMatchHere = true;
        Program const *prog = this->program;
        bool isStickyPresent = this->pattern->IsSticky();
        switch (prog->tag)
        {
        case Program::BOIInstructionsTag:
            if (offset != 0)
            {
                groupInfos[0].Reset();
                res = false;
                break;
            }

            // fall through

        case Program::BOIInstructionsForStickyFlagTag:
            AssertMsg(prog->tag == Program::BOIInstructionsTag || isStickyPresent, "prog->tag should be BOIInstructionsForStickyFlagTag if sticky = true.");

            loopMatchHere = false;

            // fall through

        case Program::InstructionsTag:
            {
                previousQcTime = 0;
                uint qcTicks = 0;

                // This is the next offset in the input from where we will try to sync. For sync instructions that back up, this
                // is used to avoid trying to sync when we have not yet reached the offset in the input we last synced to before
                // backing up.
                CharCount nextSyncInputOffset = offset;

                RegexStacks * regexStacks = scriptContext->RegexStacks();

                // Need to continue matching even if matchStart == inputLim since some patterns may match an empty string at the end
                // of the input. For instance: /a*$/.exec("b")
                do
                {
                    // Let there be only one call to MatchHere(), as that call expands the interpreter loop in-place. Having
                    // multiple calls to MatchHere() would bloat the code.
                    res = MatchHere(input, inputLength, offset, nextSyncInputOffset, regexStacks->contStack, regexStacks->assertionStack, qcTicks);
                } while(!res && loopMatchHere && ++offset <= inputLength);

                break;
            }

        case Program::SingleCharTag:
            if (this->pattern->IsIgnoreCase())
            {
                res = MatchSingleCharCaseInsensitive(input, inputLength, offset, prog->rep.singleChar.c);
            }
            else
            {
                res = MatchSingleCharCaseSensitive(input, inputLength, offset, prog->rep.singleChar.c);
            }

            break;

        case Program::BoundedWordTag:
            res = MatchBoundedWord(input, inputLength, offset);
            break;

        case Program::LeadingTrailingSpacesTag:
            res = MatchLeadingTrailingSpaces(input, inputLength, offset);
            break;

        case Program::OctoquadTag:
            res = MatchOctoquad(input, inputLength, offset, prog->rep.octoquad.matcher);
            break;

        case Program::BOILiteral2Tag:
            res = MatchBOILiteral2(input, inputLength, offset, prog->rep.boiLiteral2.literal);
            break;

        default:
            Assert(false);
            __assume(false);
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        this->stats = 0;
        this->w = 0;
#endif

        return res;
    }


#if ENABLE_REGEX_CONFIG_OPTIONS
    void Matcher::Print(DebugWriter* w, const Char* const input, const CharCount inputLength, CharCount inputOffset, const uint8* instPointer, ContStack &contStack, AssertionStack &assertionStack) const
    {
        w->PrintEOL(L"Matcher {");
        w->Indent();
        w->Print(L"program:      ");
        w->PrintQuotedString(program->source, program->sourceLen);
        w->EOL();
        w->Print(L"inputPointer: ");
        if (inputLength == 0)
            w->PrintEOL(L"<empty input>");
        else if (inputLength > 1024)
            w->PrintEOL(L"<string too large>");
        else
        {
            w->PrintEscapedString(input, inputOffset);
            if (inputOffset >= inputLength)
                w->Print(L"<<<>>>");
            else
            {
                w->Print(L"<<<");
                w->PrintEscapedChar(input[inputOffset]);
                w->Print(L">>>");
                w->PrintEscapedString(input + inputOffset + 1, inputLength - inputOffset - 1);
            }
            w->EOL();
        }
        if (program->tag == Program::BOIInstructionsTag || program->tag == Program::InstructionsTag)
        {
            w->Print(L"instPointer: ");
            ((const Inst*)instPointer)->Print(w, InstPointerToLabel(instPointer), program->rep.insts.litbuf);
            w->PrintEOL(L"groups:");
            w->Indent();
            for (int i = 0; i < program->numGroups; i++)
            {
                w->Print(L"%d: ", i);
                groupInfos[i].Print(w, input);
                w->EOL();
            }
            w->Unindent();
            w->PrintEOL(L"loops:");
            w->Indent();
            for (int i = 0; i < program->numLoops; i++)
            {
                w->Print(L"%d: ", i);
                loopInfos[i].Print(w);
                w->EOL();
            }
            w->Unindent();
            w->PrintEOL(L"contStack: (top to bottom)");
            w->Indent();
            contStack.Print(w, input);
            w->Unindent();
            w->PrintEOL(L"assertionStack: (top to bottom)");
            w->Indent();
            assertionStack.Print(w, this);
            w->Unindent();
        }
        w->Unindent();
        w->PrintEOL(L"}");
        w->Flush();
    }
#endif

    // ----------------------------------------------------------------------
    // Program
    // ----------------------------------------------------------------------

    Program::Program(RegexFlags flags)
        : source(0)
        , sourceLen(0)
        , flags(flags)
        , numGroups(0)
        , numLoops(0)
    {
        tag = InstructionsTag;
        rep.insts.insts = 0;
        rep.insts.instsLen = 0;
        rep.insts.litbuf = 0;
        rep.insts.litbufLen = 0;
        rep.insts.scannersForSyncToLiterals = 0;
    }

    Program *Program::New(Recycler *recycler, RegexFlags flags)
    {
        return RecyclerNew(recycler, Program, flags);
    }

    ScannerInfo **Program::CreateScannerArrayForSyncToLiterals(Recycler *const recycler)
    {
        Assert(tag == InstructionsTag);
        Assert(!rep.insts.scannersForSyncToLiterals);
        Assert(recycler);

        return
            rep.insts.scannersForSyncToLiterals =
                RecyclerNewArrayZ(recycler, ScannerInfo *, ScannersMixin::MaxNumSyncLiterals);
    }

    ScannerInfo *Program::AddScannerForSyncToLiterals(
        Recycler *const recycler,
        const int scannerIndex,
        const CharCount offset,
        const CharCount length,
        const bool isEquivClass)
    {
        Assert(tag == InstructionsTag);
        Assert(rep.insts.scannersForSyncToLiterals);
        Assert(recycler);
        Assert(scannerIndex >= 0);
        Assert(scannerIndex < ScannersMixin::MaxNumSyncLiterals);
        Assert(!rep.insts.scannersForSyncToLiterals[scannerIndex]);

        return
            rep.insts.scannersForSyncToLiterals[scannerIndex] =
                RecyclerNewLeaf(recycler, ScannerInfo, offset, length, isEquivClass);
    }

    void Program::FreeBody(ArenaAllocator* rtAllocator)
    {
        if(tag != InstructionsTag || !rep.insts.insts)
            return;

        Inst *inst = reinterpret_cast<Inst *>(rep.insts.insts);
        const auto instEnd = reinterpret_cast<Inst *>(reinterpret_cast<uint8 *>(inst) + rep.insts.instsLen);
        Assert(inst < instEnd);
        do
        {
            switch(inst->tag)
            {
#define MBase(TagName, ClassName) \
                case Inst::TagName: \
                { \
                    const auto actualInst = static_cast<ClassName *>(inst); \
                    actualInst->FreeBody(rtAllocator); \
                    inst = actualInst + 1; \
                    break; \
                }
#define M(TagName) MBase(TagName, TagName##Inst)
#define MTemplate(TagName, TemplateDeclaration, GenericClassName, SpecializedClassName) MBase(TagName, SpecializedClassName)
#include "RegexOpcodes.h"
#undef MBase
#undef M
#undef MTemplate
                default:
                    Assert(false);
                    __assume(false);
            }
        } while(inst < instEnd);
        Assert(inst == instEnd);

#if DBG
        rep.insts.insts = 0;
        rep.insts.instsLen = 0;
#endif
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void Program::Print(DebugWriter* w)
    {
        w->PrintEOL(L"Program {");
        w->Indent();
        w->PrintEOL(L"source:       %s", source);
        w->Print(L"flags:        ");
        if ((flags & GlobalRegexFlag) != 0) w->Print(L"global ");
        if ((flags & MultilineRegexFlag) != 0) w->Print(L"multiline ");
        if ((flags & IgnoreCaseRegexFlag) != 0) w->Print(L"ignorecase");
        if ((flags & UnicodeRegexFlag) != 0) w->Print(L"unicode");
        if ((flags & StickyRegexFlag) != 0) w->Print(L"sticky");
        w->EOL();
        w->PrintEOL(L"numGroups:    %d", numGroups);
        w->PrintEOL(L"numLoops:     %d", numLoops);
        switch (tag)
        {
        case BOIInstructionsTag:
        case InstructionsTag:
            {
                w->PrintEOL(L"instructions: {");
                w->Indent();
                if (tag == BOIInstructionsTag)
                {
                    w->PrintEOL(L"       BOITest(hardFail: true)");
                }
                uint8* instsLim = rep.insts.insts + rep.insts.instsLen;
                uint8* curr = rep.insts.insts;
                while (curr != instsLim)
                    curr += ((Inst*)curr)->Print(w, (Label)(curr - rep.insts.insts), rep.insts.litbuf);
                w->Unindent();
                w->PrintEOL(L"}");
            }
            break;
        case SingleCharTag:
            w->Print(L"special form: <match single char ");
            w->PrintQuotedChar(rep.singleChar.c);
            w->PrintEOL(L">");
            break;
        case BoundedWordTag:
            w->PrintEOL(L"special form: <match bounded word>");
            break;
        case LeadingTrailingSpacesTag:
            w->PrintEOL(L"special form: <match leading/trailing spaces: minBegin=%d minEnd=%d>",
                rep.leadingTrailingSpaces.beginMinMatch, rep.leadingTrailingSpaces.endMinMatch);
            break;
        case OctoquadTag:
            w->Print(L"special form: <octoquad ");
            rep.octoquad.matcher->Print(w);
            w->PrintEOL(L">");
            break;
        }
        w->Unindent();
        w->PrintEOL(L"}");
    }
#endif

#define M(...)
#define MTemplate(TagName, TemplateDeclaration, GenericClassName, SpecializedClassName) template struct SpecializedClassName;
#include "RegexOpcodes.h"
#undef M
#undef MTemplate
}
