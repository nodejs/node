//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{

    // ----------------------------------------------------------------------
    // Compiler (inlines etc)
    // ----------------------------------------------------------------------

    uint8* Compiler::Emit(size_t size)
    {
        Assert(size <= UINT32_MAX);

        if (instLen - instNext < size)
        {
            CharCount newLen = max(instLen, initInstBufSize);
            CharCount instLenPlus = (CharCount)(instLen + size - 1);

            // check for overflow
            if (instLenPlus < instLen || instLenPlus * 2 < instLenPlus)
            {
                Js::Throw::OutOfMemory();
            }

            while (newLen <= instLenPlus)
            {
                newLen *= 2;
            }

            instBuf = (uint8*)ctAllocator->Realloc(instBuf, instLen, newLen);
            instLen = newLen;
        }
        uint8* inst = instBuf + instNext;
        instNext += (CharCount)size;
        return inst;
    }


    template <typename T>
    T* Compiler::Emit()
    {
        return new(Emit(sizeof(T))) T;
    }

#define EMIT(compiler, T, ...) (new (compiler.Emit(sizeof(T))) T(__VA_ARGS__))
#define L2I(O, label) LabelToInstPointer<O##Inst>(Inst::O, label)

    // Remember: The machine address of an instruction is no longer valid after a subsequent emit,
    //           so all label fixups must be done using Compiler::GetFixup / Compiler::DoFixup

    // ----------------------------------------------------------------------
    // Node
    // ----------------------------------------------------------------------

    void Node::AppendLiteral(CharCount& litbufNext, CharCount litbufLen, __inout_ecount(litbufLen) Char* litbuf) const
    {
        Assert(false);
    }

    CharCount Node::EmitScanFirstSet(Compiler& compiler)
    {
        Assert(prevConsumes.IsExact(0));

        if (thisConsumes.CouldMatchEmpty())
            // Can't be sure of consuming something in FIRST
            return 0;

        if (firstSet->Count() > maxSyncToSetSize)
            // HEURISTIC: If FIRST is large we'll get too many false positives
            return 0;

        //
        // Compilation scheme:
        //
        //   SyncTo(Char|Char2|Set)And(Consume|Continue)
        //
        Char entries[CharSet<Char>::MaxCompact];
        int count = firstSet->GetCompactEntries(2, entries);
        if (SupportsPrefixSkipping(compiler))
        {
            if (count == 1)
                EMIT(compiler, SyncToCharAndConsumeInst, entries[0]);
            else if (count == 2)
                EMIT(compiler, SyncToChar2SetAndConsumeInst, entries[0], entries[1]);
            else
                EMIT(compiler, SyncToSetAndConsumeInst<false>)->set.CloneFrom(compiler.rtAllocator, *firstSet);
            return 1;
        }
        else
        {
            if (count == 1)
                EMIT(compiler, SyncToCharAndContinueInst, entries[0]);
            else if (count == 2)
                EMIT(compiler, SyncToChar2SetAndContinueInst, entries[0], entries[1]);
            else
                EMIT(compiler, SyncToSetAndContinueInst<false>)->set.CloneFrom(compiler.rtAllocator, *firstSet);
            return 0;
        }
    }

    bool Node::IsBetterSyncronizingNode(Compiler& compiler, Node* curr, Node* proposed)
    {
        int proposedNumLiterals = 0;
        CharCount proposedLength = proposed->MinSyncronizingLiteralLength(compiler, proposedNumLiterals);

        if (proposedLength == 0 || proposedNumLiterals > maxNumSyncLiterals)
            // Not a synchronizable node or too many literals.
            return false;

        if (curr == nullptr)
            // We'll take whatever we can get
            return true;

        int currNumLiterals = 0;
        CharCount currLength = curr->MinSyncronizingLiteralLength(compiler, currNumLiterals);

        // Lexicographic ordering based on
        //  - whether literal length is above a threshold (above is better)
        //  - number of literals (smaller is better)
        //  - upper bound on backup (finite is better)
        //  - minimum literal length (longer is better)
        //  - actual backup upper bound (shorter is better)

        if (proposedLength >= preferredMinSyncToLiteralLength
            && currLength < preferredMinSyncToLiteralLength)
        {
            return true;
        }
        if (proposedLength < preferredMinSyncToLiteralLength
            && currLength >= preferredMinSyncToLiteralLength)
        {
            return false;
        }

        if (proposedNumLiterals < currNumLiterals)
            return true;
        if (proposedNumLiterals > currNumLiterals)
            return false;

        if (!proposed->prevConsumes.IsUnbounded() && curr->prevConsumes.IsUnbounded())
            return true;
        if (proposed->prevConsumes.IsUnbounded() && !curr->prevConsumes.IsUnbounded())
            return false;

        if (proposedLength > currLength)
            return true;
        if (proposedLength < currLength)
            return false;

        return proposed->prevConsumes.upper < curr->prevConsumes.upper;
    }

    bool Node::IsSingleChar(Compiler& compiler, Char& outChar) const
    {
        if (tag != Node::MatchChar)
            return false;

        const MatchCharNode* node = (const MatchCharNode*)this;
        if (node->isEquivClass)
            return false;

        outChar = node->cs[0];
        return true;
    }

    bool Node::IsBoundedWord(Compiler& compiler) const
    {
        if (tag != Node::Concat)
            return false;

        const ConcatNode* concatNode = (const ConcatNode *)this;
        if (concatNode->head->tag != Node::WordBoundary ||
            concatNode->tail == 0 ||
            concatNode->tail->head->tag != Node::Loop ||
            concatNode->tail->tail == 0 ||
            concatNode->tail->tail->head->tag != Node::WordBoundary ||
            concatNode->tail->tail->tail != 0)
            return false;

        const WordBoundaryNode* enter = (const WordBoundaryNode*)concatNode->head;
        const LoopNode* loop = (const LoopNode*)concatNode->tail->head;
        const WordBoundaryNode* leave = (const WordBoundaryNode*)concatNode->tail->tail->head;

        if (enter->isNegation ||
            !loop->isGreedy ||
            loop->repeats.lower != 1 ||
            loop->repeats.upper != CharCountFlag ||
            loop->body->tag != Node::MatchSet ||
            leave->isNegation)
            return false;

        const MatchSetNode* wordSet = (const MatchSetNode*)loop->body;

        if (wordSet->isNegation)
            return false;

        return wordSet->set.IsEqualTo(*compiler.standardChars->GetWordSet());
    }

    bool Node::IsBOILiteral2(Compiler& compiler) const
    {
        if (tag != Node::Concat)
            return false;
        const ConcatNode* concatNode = (const ConcatNode *)this;
        if ((compiler.program->flags & (IgnoreCaseRegexFlag | MultilineRegexFlag)) != 0 ||
            concatNode->head->tag != Node::BOL ||
            concatNode->tail == nullptr ||
            concatNode->tail->head->tag != Node::MatchLiteral ||
            concatNode->tail->tail != nullptr ||
            ((MatchLiteralNode *)concatNode->tail->head)->isEquivClass ||
            ((MatchLiteralNode *)concatNode->tail->head)->length != 2)
        {
            return false;
        }
        return true;
    }

    bool Node::IsLeadingTrailingSpaces(Compiler& compiler, CharCount& leftMinMatch, CharCount& rightMinMatch) const
    {

        if (tag != Node::Alt)
            return false;

        if (compiler.program->flags & MultilineRegexFlag)
            return false;

        const AltNode* altNode = (const AltNode*)this;
        if (altNode->head->tag != Node::Concat ||
            altNode->tail == 0 ||
            altNode->tail->head->tag != Node::Concat ||
            altNode->tail->tail != 0)
            return false;

        const ConcatNode* left = (const ConcatNode*)altNode->head;
        const ConcatNode* right = (const ConcatNode*)altNode->tail->head;

        if (left->head->tag != Node::BOL ||
            left->tail == 0 ||
            left->tail->head->tag != Node::Loop ||
            left->tail->tail != 0)
            return false;

        if (right->head->tag != Node::Loop ||
            right->tail == 0 ||
            right->tail->head->tag != Node::EOL ||
            right->tail->tail != 0)
            return false;

        const LoopNode* leftLoop = (const LoopNode*)left->tail->head;
        const LoopNode* rightLoop = (const LoopNode*)right->head;

        if (!leftLoop->isGreedy ||
            leftLoop->repeats.upper != CharCountFlag ||
            leftLoop->body->tag != Node::MatchSet ||
            !rightLoop->isGreedy ||
            rightLoop->repeats.upper != CharCountFlag ||
            rightLoop->body->tag != Node::MatchSet)
            return false;

        const MatchSetNode* leftSet = (const MatchSetNode*)leftLoop->body;
        const MatchSetNode* rightSet = (const MatchSetNode*)rightLoop->body;

        if (leftSet->isNegation ||
            rightSet->isNegation)
            return false;

        leftMinMatch = leftLoop->repeats.lower;
        rightMinMatch = rightLoop->repeats.lower;

        return
            leftSet->set.IsEqualTo(*compiler.standardChars->GetWhitespaceSet()) &&
            rightSet->set.IsEqualTo(*compiler.standardChars->GetWhitespaceSet());
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void Node::PrintAnnotations(DebugWriter* w) const
    {
        if (firstSet != 0)
        {
            w->PrintEOL(L"<");
            w->Indent();

            w->Print(L"features: {");
            bool first = true;
            for (uint i = Empty; i <= Assertion; i++)
            {
                if ((features & (1 << i)) != 0)
                {
                    if (first)
                        first = false;
                    else
                        w->Print(L",");
                    switch (i)
                    {
                    case Empty: w->Print(L"Empty"); break;
                    case BOL: w->Print(L"BOL"); break;
                    case EOL: w->Print(L"EOL"); break;
                    case WordBoundary: w->Print(L"WordBoundary"); break;
                    case MatchLiteral: w->Print(L"MatchLiteral"); break;
                    case MatchChar: w->Print(L"MatchChar"); break;
                    case Concat: w->Print(L"Concat"); break;
                    case Alt: w->Print(L"Alt"); break;
                    case DefineGroup: w->Print(L"DefineGroup"); break;
                    case MatchGroup: w->Print(L"MatchGroup"); break;
                    case Loop: w->Print(L"Loop"); break;
                    case MatchSet: w->Print(L"MatchSet"); break;
                    case Assertion: w->Print(L"Assertion"); break;
                    }
                }
            }
            w->PrintEOL(L"}");

            w->Print(L"firstSet: ");
            firstSet->Print(w);
            if (isFirstExact)
                w->Print(L" (exact)");
            w->EOL();

            w->Print(L"followSet: ");
            followSet->Print(w);
            w->EOL();

            w->Print(L"prevConsumes: ");
            prevConsumes.Print(w);
            w->EOL();

            w->Print(L"thisConsumes: ");
            thisConsumes.Print(w);
            w->EOL();

            w->Print(L"followConsumes: ");
            followConsumes.Print(w);
            w->EOL();

            w->PrintEOL(L"isThisIrrefutable: %s", isThisIrrefutable ? L"true" : L"false");
            w->PrintEOL(L"isFollowIrrefutable: %s", isFollowIrrefutable ? L"true" : L"false");
            w->PrintEOL(L"isWord: %s", isWord ? L"true" : L"false");
            w->PrintEOL(L"isThisWillNotProgress: %s", isThisWillNotProgress ? L"true" : L"false");
            w->PrintEOL(L"isThisWillNotRegress: %s", isThisWillNotRegress ? L"true" : L"false");
            w->PrintEOL(L"isPrevWillNotProgress: %s", isPrevWillNotProgress ? L"true" : L"false");
            w->PrintEOL(L"isPrevWillNotRegress: %s", isPrevWillNotRegress ? L"true" : L"false");
            w->PrintEOL(L"isDeterministic: %s", isDeterministic ? L"true" : L"false");
            w->PrintEOL(L"isNotInLoop: %s", isNotInLoop ? L"true" : L"false");
            w->PrintEOL(L"isNotNegated: %s", isNotNegated ? L"true" : L"false");
            w->PrintEOL(L"isAtLeastOnce: %s", isAtLeastOnce ? L"true" : L"false");
            w->PrintEOL(L"hasInitialHardFailBOI: %s", hasInitialHardFailBOI ? L"true" : L"false");

            w->Unindent();
            w->PrintEOL(L">");
        }
    }
#endif

    // ----------------------------------------------------------------------
    // SimpleNode
    // ----------------------------------------------------------------------

    CharCount SimpleNode::LiteralLength() const
    {
        return 0;
    }

    bool SimpleNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount SimpleNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        return 0;
    }

    void SimpleNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
    }

    bool SimpleNode::IsRefiningAssertion(Compiler& compiler)
    {
        return tag == EOL && (compiler.program->flags & MultilineRegexFlag) != 0;
    }

    void SimpleNode::AnnotatePass0(Compiler& compiler)
    {
        isWord = false;
    }

    void SimpleNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        isFirstExact = false;
        thisConsumes.Exact(0);
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
        switch (tag)
        {
        case Empty:
            features = HasEmpty;
            firstSet = compiler.standardChars->GetEmptySet();
            isThisIrrefutable = true;
            break;
        case BOL:
            features = HasBOL;
            firstSet = compiler.standardChars->GetFullSet();
            isThisIrrefutable = false;
            break;
        case EOL:
            features = HasEOL;
            if ((compiler.program->flags & MultilineRegexFlag) != 0)
                firstSet = compiler.standardChars->GetNewlineSet();
            else
                firstSet = compiler.standardChars->GetEmptySet();
            isThisIrrefutable = false;
            break;
        default:
            Assert(false);
        }
    }

    void SimpleNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
    }

    void SimpleNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;

        hasInitialHardFailBOI = ((tag == BOL) &&
            prevConsumes.IsExact(0) &&
            (compiler.program->flags & MultilineRegexFlag) == 0 &&
            isAtLeastOnce &&
            isNotNegated &&
            isPrevWillNotRegress);
    }

    void SimpleNode::AnnotatePass4(Compiler& compiler)
    {
        isDeterministic = true;
    }

    bool SimpleNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return false;
    }

    Node* SimpleNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return 0;
    }

    CharCount SimpleNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        return 0;
    }

    void SimpleNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void SimpleNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
    }

    void SimpleNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
    }

    void SimpleNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        Assert(skipped == 0);

        switch (tag)
        {
        case Empty:
            // Nothing
            break;
        case BOL:
            {
                if ((compiler.program->flags & MultilineRegexFlag) != 0)
                {
                    //
                    // Compilation scheme:
                    //
                    //   BOLTest
                    //
                    EMIT(compiler, BOLTestInst);
                }
                else
                {
                    if (compiler.CurrentLabel() == 0)
                    {
                        // The first instruction is BOI, change the tag and only execute it once
                        // without looping every start position
                        compiler.SetBOIInstructionsProgramTag();
                    }
                    else
                    {
                        //
                        // Compilation scheme:
                        //
                        //   BOITest
                        //
                        // Obviously starting later in the string won't help, so can hard fail if:
                        //  - this pattern must always be matched
                        //  - not in an negative assertion
                        //  - backtracking could never rewind the input pointer
                        //
                        EMIT(compiler, BOITestInst, isAtLeastOnce && isNotNegated && isPrevWillNotRegress);
                    }
                }
                break;
            }
        case EOL:
            {
                if ((compiler.program->flags & MultilineRegexFlag) != 0)
                    //
                    // Compilation scheme:
                    //
                    //   EOLTest
                    //
                    EMIT(compiler, EOLTestInst);
                else
                    //
                    // Compilation scheme:
                    //
                    //   EOITest
                    //
                    // Can hard fail if
                    //  - this pattern must always be matched
                    //  - not in an negative assertion
                    //  - backtracking could never advance the input pointer
                    //
                    EMIT(compiler, EOITestInst, isAtLeastOnce && isNotNegated && isPrevWillNotProgress);
                break;
            }
        default:
            Assert(false);
        }
    }


    CharCount SimpleNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        Assert(false);
        return 0;
    }

    bool SimpleNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        return false;
    }

    bool SimpleNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        return tag == Empty;
    }

    bool SimpleNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(tag == Empty);
        if (cont == 0)
        {
            if (trie->Count() > 0)
                // This literal is a proper prefix of an earlier literal
                return false;
            trie->SetAccepting();
            return true;
        }

        return cont->BuildCharTrie(compiler, trie, 0, isAcceptFirst);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void SimpleNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        switch (tag)
        {
        case Empty:
            w->Print(L"Empty"); break;
        case BOL:
            w->Print(L"BOL"); break;
        case EOL:
            w->Print(L"EOL"); break;
        default:
            Assert(false);
        }
        w->PrintEOL(L"()");
        PrintAnnotations(w);
    }
#endif

    // ----------------------------------------------------------------------
    // WordBoundaryNode
    // ----------------------------------------------------------------------

    CharCount WordBoundaryNode::LiteralLength() const
    {
        return 0;
    }

    bool WordBoundaryNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount WordBoundaryNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        return 0;
    }

    void WordBoundaryNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        // WordChars and NonWordChars sets are already case invariant
    }

    bool WordBoundaryNode::IsRefiningAssertion(Compiler& compiler)
    {
        return mustIncludeEntering != mustIncludeLeaving;
    }

    void WordBoundaryNode::AnnotatePass0(Compiler& compiler)
    {
        isWord = false;
    }

    void WordBoundaryNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        features = HasWordBoundary;
        thisConsumes.Exact(0);
        isFirstExact = false;
        isThisIrrefutable = false;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
        if (isNegation)
            firstSet = compiler.standardChars->GetFullSet();
        else
        {
            if (mustIncludeEntering && !mustIncludeLeaving)
                firstSet = compiler.standardChars->GetWordSet();
            else if (mustIncludeLeaving && !mustIncludeEntering)
                firstSet = compiler.standardChars->GetNonWordSet();
            else
                firstSet = compiler.standardChars->GetFullSet();
        }
    }

    void WordBoundaryNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
    }

    void WordBoundaryNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
    }

    void WordBoundaryNode::AnnotatePass4(Compiler& compiler)
    {
        isDeterministic = true;
    }

    bool WordBoundaryNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return false;
    }

    Node* WordBoundaryNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return 0;
    }

    CharCount WordBoundaryNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        return 0;
    }

    void WordBoundaryNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void WordBoundaryNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
    }

    void WordBoundaryNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
    }

    void WordBoundaryNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        Assert(skipped == 0);
        //
        // Compilation scheme:
        //
        //   WordBoundaryTest
        //
        EMIT(compiler, WordBoundaryTestInst, isNegation);
    }

    CharCount WordBoundaryNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        Assert(false);
        return 0;
    }

    bool WordBoundaryNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        return false;
    }

    bool WordBoundaryNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        return false;
    }

    bool WordBoundaryNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        Assert(false);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void WordBoundaryNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->PrintEOL(L"WordBoundary(%s, %s, %s)", isNegation ? L"negative" : L"positive", mustIncludeEntering ? L"entering" : L"-", mustIncludeLeaving ? L"leaving" : L"-");
        PrintAnnotations(w);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchLiteralNode
    // ----------------------------------------------------------------------

    CharCount MatchLiteralNode::LiteralLength() const
    {
        return length;
    }

    void MatchLiteralNode::AppendLiteral(CharCount& litbufNext, CharCount litbufLen, __inout_ecount(litbufLen) Char* litbuf) const
    {
        // Called during parsing only, so literal always in original form
        Assert(!isEquivClass);
        Assert(litbufNext + length <= litbufLen && offset + length <= litbufLen);
#pragma prefast(suppress:26000, "The error said that offset + length >= litbufLen + 1, which is incorrect due to if statement below.")
        if (litbufNext + length <= litbufLen && offset + length <= litbufLen) // for prefast
        {
            js_wmemcpy_s(litbuf + litbufNext, litbufLen - litbufNext, litbuf + offset, length);
        }
        litbufNext += length;
    }

    bool MatchLiteralNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount MatchLiteralNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        Assert(length > 1);
        if ((compiler.program->flags & IgnoreCaseRegexFlag) != 0
            && !compiler.standardChars->IsTrivialString(compiler.program->GetCaseMappingSource(), litbuf + offset, length))
        {
            // We'll need to expand each character of literal into its equivalence class
            isEquivClass = true;
            return length * CaseInsensitive::EquivClassSize;
        }
        else
            return length;
    }

    void MatchLiteralNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        CharCount nextLit = compiler.program->rep.insts.litbufLen;
        if (isEquivClass)
        {
            Assert((compiler.program->flags & IgnoreCaseRegexFlag) != 0);
            // Expand literal according to character equivalence classes
            for (CharCount i = 0; i < length; i++)
            {
                compiler.standardChars->ToEquivs(
                    compiler.program->GetCaseMappingSource(),
                    litbuf[offset + i],
                    compiler.program->rep.insts.litbuf + nextLit + i * CaseInsensitive::EquivClassSize);
            }
            compiler.program->rep.insts.litbufLen += length * CaseInsensitive::EquivClassSize;
        }
        else
        {
            for (CharCount i = 0; i < length; i++)
                compiler.program->rep.insts.litbuf[nextLit + i] = litbuf[offset + i];
            compiler.program->rep.insts.litbufLen += length;
        }
        offset = nextLit;
    }

    void MatchLiteralNode::AnnotatePass0(Compiler& compiler)
    {
        const Char* litbuf = compiler.program->rep.insts.litbuf;
        for (CharCount i = offset; i < offset + length; i++)
        {
            if (!compiler.standardChars->IsWord(litbuf[i]))
            {
                isWord = false;
                return;
            }
        }
        isWord = true;
    }

    bool MatchLiteralNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void MatchLiteralNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        features = HasMatchLiteral;
        thisConsumes.Exact(length);
        firstSet = Anew(compiler.ctAllocator, UnicodeCharSet);
        for (int i = 0; i < (isEquivClass ? CaseInsensitive::EquivClassSize : 1); i++)
            firstSet->Set(compiler.ctAllocator, compiler.program->rep.insts.litbuf[offset + i]);
        isFirstExact = true;
        isThisIrrefutable = false;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
    }

    void MatchLiteralNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
    }

    void MatchLiteralNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
    }

    void MatchLiteralNode::AnnotatePass4(Compiler& compiler)
    {
        isDeterministic = true;
    }

    bool MatchLiteralNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return true;
    }

    Node* MatchLiteralNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return this;
    }

    CharCount MatchLiteralNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        numLiterals++;
        return length;
    }

    void MatchLiteralNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        ScannerMixin* scanner =
            scanners.Add(compiler.GetScriptContext()->GetRecycler(), compiler.GetProgram(), offset, length, isEquivClass);
        scanner->scanner.Setup(compiler.rtAllocator, compiler.program->rep.insts.litbuf + offset, length, isEquivClass ? CaseInsensitive::EquivClassSize : 1);
    }

    void MatchLiteralNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
        if (IsBetterSyncronizingNode(compiler, bestNode, this))
            bestNode = this;
    }

    void MatchLiteralNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
    }

    void MatchLiteralNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        if (skipped >= length)
        {
            // Asking to skip entire literal
            skipped -= length;
            return;
        }

        //
        // Compilation scheme:
        //
        //   Match(Char|Char4|Literal|LiteralEquiv)Inst
        //

        CharCount effectiveOffset = offset + skipped * (isEquivClass ? CaseInsensitive::EquivClassSize : 1);
        CharCount effectiveLength = length - skipped;
        skipped -= min(skipped, length);

        if (effectiveLength == 1)
        {
            Char* cs = compiler.program->rep.insts.litbuf + effectiveOffset;
            MatchCharNode::Emit(compiler, cs, isEquivClass);
        }
        else
        {
            if (isEquivClass)
                EMIT(compiler, MatchLiteralEquivInst, effectiveOffset, effectiveLength);
            else
                EMIT(compiler, MatchLiteralInst, effectiveOffset, effectiveLength);
        }
    }

    CompileAssert(CaseInsensitive::EquivClassSize == 4);
    CharCount MatchLiteralNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        //
        // Compilation scheme:
        //
        //   SyncTo(Literal|LiteralEquiv|LinearLiteral)And(Continue|Consume|Backup)
        //

        Char * litptr = compiler.program->rep.insts.litbuf + offset;

        if (isHeadSyncronizingNode)
        {
            // For a head literal there's no need to back up after finding the literal, so use a faster instruction
            Assert(prevConsumes.IsExact(0)); // there should not be any consumes before this node
            if (isEquivClass)
            {
                const uint lastPatCharIndex = length - 1;
                if (litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 1]
                    && litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 2]
                    && litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 3])
                {
                    EMIT(compiler, SyncToLiteralEquivTrivialLastPatCharAndConsumeInst, offset, length)->scanner.Setup(compiler.rtAllocator, litptr, length, CaseInsensitive::EquivClassSize);
                }
                else
                {
                    EMIT(compiler, SyncToLiteralEquivAndConsumeInst, offset, length)->scanner.Setup(compiler.rtAllocator, litptr, length, CaseInsensitive::EquivClassSize);
                }
            }
            else if (length == 1)
                EMIT(compiler, SyncToCharAndConsumeInst, litptr[0]);
            else if (length == 2)
                EMIT(compiler, SyncToChar2LiteralAndConsumeInst, litptr[0], litptr[1]);
            else
            {
                TextbookBoyerMooreSetup<wchar_t> setup(litptr, length);
                switch (setup.GetScheme())
                {
                case TextbookBoyerMooreSetup<wchar_t>::LinearScheme:
                    EMIT(compiler, SyncToLinearLiteralAndConsumeInst, offset, length)->scanner.Setup(compiler.rtAllocator, setup);
                    break;
                case TextbookBoyerMooreSetup<wchar_t>::DefaultScheme:
                    EMIT(compiler, SyncToLiteralAndConsumeInst, offset, length)->scanner.Setup(compiler.rtAllocator, setup);
                    break;
                };
            }
            return length;
        }
        else
        {
            // We're synchronizing on a non-head literal so we may need to back up. Or if we're syncing to the first literal
            // inside a group for instance, then we won't need to back up but we cannot consume the literal.
            if (prevConsumes.IsExact(0))
            {
                if (isEquivClass)
                {
                    const uint lastPatCharIndex = length - 1;
                    if (litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 1]
                        && litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 2]
                        && litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 3])
                    {
                        EMIT(compiler, SyncToLiteralEquivTrivialLastPatCharAndContinueInst, offset, length)->scanner.Setup(compiler.rtAllocator, litptr, length, CaseInsensitive::EquivClassSize);
                    }
                    else
                    {
                        EMIT(compiler, SyncToLiteralEquivAndContinueInst, offset, length)->scanner.Setup(compiler.rtAllocator, litptr, length, CaseInsensitive::EquivClassSize);
                    }
                }
                else if (length == 1)
                    EMIT(compiler, SyncToCharAndContinueInst, litptr[0]);
                else if (length == 2)
                    EMIT(compiler, SyncToChar2LiteralAndContinueInst, litptr[0], litptr[1]);
                else
                {
                    TextbookBoyerMooreSetup<wchar_t> setup(litptr, length);
                    switch (setup.GetScheme())
                    {
                    case TextbookBoyerMooreSetup<wchar_t>::LinearScheme:
                        EMIT(compiler, SyncToLinearLiteralAndContinueInst, offset, length)->scanner.Setup(compiler.rtAllocator, setup);
                        break;
                    case TextbookBoyerMooreSetup<wchar_t>::DefaultScheme:
                        EMIT(compiler, SyncToLiteralAndContinueInst, offset, length)->scanner.Setup(compiler.rtAllocator, setup);
                        break;
                    };
                }
            }
            else
            {
                if (isEquivClass)
                {
                    const uint lastPatCharIndex = length - 1;
                    if (litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 1]
                        && litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 2]
                        && litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize] == litptr[lastPatCharIndex * CaseInsensitive::EquivClassSize + 3])
                    {
                        EMIT(compiler, SyncToLiteralEquivTrivialLastPatCharAndBackupInst, offset, length, prevConsumes)->scanner.Setup(compiler.rtAllocator, litptr, length, CaseInsensitive::EquivClassSize);
                    }
                    else
                    {
                        EMIT(compiler, SyncToLiteralEquivAndBackupInst, offset, length, prevConsumes)->scanner.Setup(compiler.rtAllocator, litptr, length, CaseInsensitive::EquivClassSize);
                    }
                }
                else if (length == 1)
                    EMIT(compiler, SyncToCharAndBackupInst, litptr[0], prevConsumes);
                else if (length == 2)
                    EMIT(compiler, SyncToChar2LiteralAndBackupInst, litptr[0], litptr[1], prevConsumes);
                else
                {
                    TextbookBoyerMooreSetup<wchar_t> setup(litptr, length);
                    switch (setup.GetScheme())
                    {
                    case TextbookBoyerMooreSetup<wchar_t>::LinearScheme:
                        EMIT(compiler, SyncToLinearLiteralAndBackupInst, offset, length, prevConsumes)->scanner.Setup(compiler.rtAllocator, setup);
                        break;
                    case TextbookBoyerMooreSetup<wchar_t>::DefaultScheme:
                        EMIT(compiler, SyncToLiteralAndBackupInst, offset, length, prevConsumes)->scanner.Setup(compiler.rtAllocator, setup);
                        break;
                    };
                }
            }
            return 0;
        }
    }

    bool MatchLiteralNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        // We look for octoquad patterns before converting for case-insensitivity
        Assert(!isEquivClass);
        if (!oi->CouldAppend(length))
            return false;
        for (CharCount i = 0; i < length; i++)
        {
            if (!oi->AppendChar(compiler.standardChars->ToCanonical(compiler.program->GetCaseMappingSource(), compiler.program->rep.insts.litbuf[offset + i])))
                return false;
        }
        return true;
    }

    bool MatchLiteralNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        if (isEquivClass)
            // The literal would expand into length^3 alternatives
            return false;
        return true;
    }

    bool MatchLiteralNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(!isEquivClass);
        CharTrie* tail = trie;
        for (CharCount i = 0; i < length; i++)
        {
            if (tail->IsAccepting())
            {
                // An earlier literal is a prefix of this literal
                // If isAcceptFirst, can ignore suffix of already recognized literal.
                // Otherwise, must fail.
                return isAcceptFirst;
            }
            CharTrie* newTail = tail->Add(compiler.ctAllocator, compiler.program->rep.insts.litbuf[offset + i]);
            if (tail->Count() > maxTrieArmExpansion)
                return false;
            tail = newTail;
        }
        if (cont == 0)
        {
            if (tail->Count() > 0)
                // This literal is a proper prefix of an earlier literal
                return false;
            tail->SetAccepting();
        }
        else
        {
            if (!cont->BuildCharTrie(compiler, tail, 0, isAcceptFirst))
                return false;
        }
        return true;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void MatchLiteralNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->Print(L"MatchLiteral(");
        int skip = isEquivClass ? CaseInsensitive::EquivClassSize : 1;
        for (int i = 0; i < skip; i++)
        {
            if (i > 0)
                w->Print(L", ");
            w->Print(L"\"");
            for (CharCount j = 0; j < length; j++)
                w->PrintEscapedChar(litbuf[offset + j * skip + i]);
            w->Print(L"\"");
        }
        w->PrintEOL(L")");
        PrintAnnotations(w);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchCharNode
    // ----------------------------------------------------------------------

    CharCount MatchCharNode::LiteralLength() const
    {
        return 1;
    }

    void MatchCharNode::AppendLiteral(CharCount& litbufNext, CharCount litbufLen, __inout_ecount(litbufLen) Char* litbuf) const
    {
        Assert(!isEquivClass);
        Assert(litbufNext + 1 <= litbufLen);
        if (litbufNext + 1 <= litbufLen)  // for prefast
            litbuf[litbufNext++] = cs[0];
    }

    bool MatchCharNode::IsCharOrPositiveSet() const
    {
        return true;
    }

    CharCount MatchCharNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        if ((compiler.program->flags & IgnoreCaseRegexFlag) != 0)
        {
            Char equivs[CaseInsensitive::EquivClassSize];
            bool isNonTrivial = compiler.standardChars->ToEquivs(compiler.program->GetCaseMappingSource(), cs[0], equivs);
            if (isNonTrivial)
            {
                isEquivClass = true;
                for (int i = 0; i < CaseInsensitive::EquivClassSize; i++)
                {
                    cs[i] = equivs[i];
                }
            }
        }
        return 0;
    }

    void MatchCharNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
    }

    bool MatchCharNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void MatchCharNode::AnnotatePass0(Compiler& compiler)
    {
        // If c is a word char then all characters equivalent to c are word chars
        isWord = compiler.standardChars->IsWord(cs[0]);
    }

    void MatchCharNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        features = HasMatchChar;
        thisConsumes.Exact(1);
        firstSet = Anew(compiler.ctAllocator, UnicodeCharSet);
        for (int i = 0; i < (isEquivClass ? CaseInsensitive::EquivClassSize : 1); i++)
            firstSet->Set(compiler.ctAllocator, cs[i]);
        isFirstExact = true;
        isThisIrrefutable = false;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
    }

    void MatchCharNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
    }

    void MatchCharNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
    }

    void MatchCharNode::AnnotatePass4(Compiler& compiler)
    {
        isDeterministic = true;
    }

    bool MatchCharNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return true;
    }

    Node* MatchCharNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return this;
    }

    CharCount MatchCharNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        numLiterals++;
        return 1;
    }

    void MatchCharNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void MatchCharNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        if (IsBetterSyncronizingNode(compiler, bestNode, this))
        {
            bestNode = this;
        }
    }

    void MatchCharNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
    }

    CompileAssert(CaseInsensitive::EquivClassSize == 4);
    void MatchCharNode::Emit(Compiler& compiler, __in_ecount(4) Char * cs, bool isEquivClass)
    {
        if (isEquivClass)
        {
            Char uniqueEquivs[CaseInsensitive::EquivClassSize];
            CharCount uniqueEquivCount = FindUniqueEquivs(cs, uniqueEquivs);
            switch (uniqueEquivCount)
            {
            case 2:
                EMIT(compiler, MatchChar2Inst, uniqueEquivs[0], uniqueEquivs[1]);
                break;

            case 3:
                EMIT(compiler, MatchChar3Inst, uniqueEquivs[0], uniqueEquivs[1], uniqueEquivs[2]);
                break;

            default:
                EMIT(compiler, MatchChar4Inst, uniqueEquivs[0], uniqueEquivs[1], uniqueEquivs[2], uniqueEquivs[3]);
                break;
            }
        }
        else
            EMIT(compiler, MatchCharInst, cs[0]);
    }

    CharCount MatchCharNode::FindUniqueEquivs(const Char equivs[CaseInsensitive::EquivClassSize], __out_ecount(4) Char uniqueEquivs[CaseInsensitive::EquivClassSize])
    {
        uniqueEquivs[0] = equivs[0];
        CharCount uniqueCount = 1;
        for (CharCount equivIndex = 1; equivIndex < CaseInsensitive::EquivClassSize; ++equivIndex)
        {
            bool alreadyHave = false;
            for (CharCount uniqueIndex = 0; uniqueIndex < uniqueCount; ++uniqueIndex)
            {
                if (uniqueEquivs[uniqueIndex] == equivs[equivIndex])
                {
                    alreadyHave = true;
                    break;
                }
            }

            if (!alreadyHave)
            {
                uniqueEquivs[uniqueCount] = equivs[equivIndex];
                uniqueCount += 1;
            }
        }

        return uniqueCount;
    }

    void MatchCharNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        if (skipped >= 1)
        {
            // Asking to skip entire char
            skipped--;
            return;
        }

        //
        // Compilation scheme:
        //
        //   MatchChar(2|3|4)?
        //

        skipped -= min(skipped, static_cast<CharCount>(1));

        Emit(compiler, cs, isEquivClass);
    }

    CharCount MatchCharNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        //
        // Compilation scheme:
        //
        //   SyncTo(Char|Char2Set|Set)And(Consume|Continue|Backup)
        //

        if (isHeadSyncronizingNode)
        {
            // For a head literal there's no need to back up after finding the literal, so use a faster instruction
            Assert(prevConsumes.IsExact(0)); // there should not be any consumes before this node
            if (firstSet->IsSingleton())
                EMIT(compiler, SyncToCharAndConsumeInst, firstSet->Singleton());
            else
                EMIT(compiler, SyncToSetAndConsumeInst<false>)->set.CloneFrom(compiler.rtAllocator, *firstSet);
            return 1;
        }
        else
        {
            // We're synchronizing on a non-head literal so we may need to back up. Or if we're syncing to the first literal
            // inside a group for instance, then we won't need to back up but we cannot consume the literal. If we don't need to
            // back up, we can use SyncToCharAndContinue instead.
            if (prevConsumes.IsExact(0))
            {
                Char entries[CharSet<Char>::MaxCompact];
                int count = firstSet->GetCompactEntries(2, entries);
                if (count == 1)
                    EMIT(compiler, SyncToCharAndContinueInst, entries[0]);
                else if (count == 2)
                    EMIT(compiler, SyncToChar2SetAndContinueInst, entries[0], entries[1]);
                else
                    EMIT(compiler, SyncToSetAndContinueInst<false>)->set.CloneFrom(compiler.rtAllocator, *firstSet);
            }
            else
            {
                if (firstSet->IsSingleton())
                    EMIT(compiler, SyncToCharAndBackupInst, firstSet->Singleton(), prevConsumes);
                else
                    EMIT(compiler, SyncToSetAndBackupInst<false>, prevConsumes)->set.CloneFrom(compiler.rtAllocator, *firstSet);
            }
            return 0;
        }
    }

    bool MatchCharNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        // We look for octoquad patterns before converting for case-insensitivity
        Assert(!isEquivClass);
        return oi->AppendChar(compiler.standardChars->ToCanonical(compiler.program->GetCaseMappingSource(), cs[0]));
    }

    bool MatchCharNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        if (isEquivClass)
        {
            accNumAlts *= CaseInsensitive::EquivClassSize;
            if (accNumAlts > maxTrieArmExpansion)
                return false;
        }
        return true;
    }

    bool MatchCharNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        for (int i = 0; i < (isEquivClass ? CaseInsensitive::EquivClassSize : 1); i++)
        {
            if (trie->IsAccepting())
            {
                // An earlier literal is a prefix of this literal
                // If isAcceptFirst, can ignore suffix of already recognized literal.
                // Otherwise, must fail.
                return isAcceptFirst;
            }
            CharTrie* tail = trie->Add(compiler.ctAllocator, cs[i]);
            if (trie->Count() > maxTrieArmExpansion)
                return false;
            if (cont == 0)
            {
                if (tail->Count() > 0)
                    // This literal is a proper prefix of an earlier literal
                    return false;
                tail->SetAccepting();
            }
            else
            {
                if (!cont->BuildCharTrie(compiler, tail, 0, isAcceptFirst))
                    return false;
            }
        }
        return true;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void MatchCharNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->Print(L"MatchChar(");
        for (int i = 0; i < (isEquivClass ? CaseInsensitive::EquivClassSize : 1); i++)
        {
            if (i > 0)
                w->Print(L", ");
            w->PrintQuotedChar(cs[i]);
        }
        w->PrintEOL(L")");
        PrintAnnotations(w);
    }
#endif

    // ----------------------------------------------------------------------
    // MatchSetNode
    // ----------------------------------------------------------------------

    CharCount MatchSetNode::LiteralLength() const
    {
        return 0;
    }

    bool MatchSetNode::IsCharOrPositiveSet() const
    {
        return !isNegation;
    }

    CharCount MatchSetNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        return 0;
    }

    void MatchSetNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        if ((compiler.program->flags & IgnoreCaseRegexFlag) != 0 && this->needsEquivClass)
        {
            // If the set is negated, then at this point we could:
            //  (1) take each char in the set to its equivalence class and complement it
            //  (2) complement the set and take each char to its equivalence class
            // Thankfully the spec demands (1), so we don't need to actually calculate any complement, just
            // retain the isNegated flag
            CharSet<Char> newSet;
            // First include all the existing characters in the result set so that large ranges such as \x80-\uffff
            // don't temporarily turn into a large number of small ranges corresponding to the various equivalent
            // characters
            newSet.UnionInPlace(compiler.rtAllocator, set);
            set.ToEquivClass(compiler.rtAllocator, newSet);
            set = newSet;
        }
    }

    bool MatchSetNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void MatchSetNode::AnnotatePass0(Compiler& compiler)
    {
        if (isNegation || set.IsEmpty())
            isWord = false;
        else
            isWord = set.IsSubsetOf(*compiler.standardChars->GetWordSet());
    }

    void MatchSetNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        features = HasMatchSet;
        if (isNegation)
        {
            firstSet = Anew(compiler.ctAllocator, UnicodeCharSet);
            set.ToComplement(compiler.ctAllocator, *firstSet);
        }
        else
        {
            // CAUTION:
            // Be careful not to use firstSet after deleting the node.
            firstSet = &set;
        }
        isFirstExact = true;
        // If firstSet is empty then pattern will always fail
        thisConsumes.Exact(firstSet->IsEmpty() ? 0 : 1);
        isThisIrrefutable = false;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
    }

    void MatchSetNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
    }

    void MatchSetNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
    }

    void MatchSetNode::AnnotatePass4(Compiler& compiler)
    {
        isDeterministic = true;
    }

    bool MatchSetNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return true;
    }

    Node* MatchSetNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return this;
    }

    CharCount MatchSetNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        return 0;
    }

    void MatchSetNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void MatchSetNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
    }

    void MatchSetNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
    }

    void MatchSetNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        if (skipped >= 1)
        {
            // Asking to skip entire set
            skipped--;
            return;
        }

        //
        // Compilation scheme:
        //
        //   MatchSet
        //

        skipped -= min(skipped, static_cast<CharCount>(1));

        RuntimeCharSet<Char> *runtimeSet;
        if(isNegation)
            runtimeSet = &EMIT(compiler, MatchSetInst<true>)->set;
        else
            runtimeSet = &EMIT(compiler, MatchSetInst<false>)->set;
        runtimeSet->CloneFrom(compiler.rtAllocator, set);
    }

    CharCount MatchSetNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        //
        // Compilation scheme:
        //
        //   SyncToSetAnd(Consume|Continue|Backup)
        //

        RuntimeCharSet<Char> *runtimeSet;
        CharCount consumedChars;
        if (isHeadSyncronizingNode)
        {
            // For a head literal there's no need to back up after finding the literal, so use a faster instruction
            Assert(prevConsumes.IsExact(0)); // there should not be any consumes before this node
            if(isNegation)
                runtimeSet = &EMIT(compiler, SyncToSetAndConsumeInst<true>)->set;
            else
                runtimeSet = &EMIT(compiler, SyncToSetAndConsumeInst<false>)->set;
            consumedChars = 1;
        }
        else
        {
            // We're synchronizing on a non-head literal so we may need to back up. Or if we're syncing to the first literal
            // inside a group for instance, then we won't need to back up but we cannot consume the literal. If we don't need to
            // back up, we still cannot use SyncToSetAndContinue like in MatchCharNode::EmitScan, since SyncToSetAndContinue does not support negation
            // sets.
            if(prevConsumes.IsExact(0))
            {
                if(isNegation)
                    runtimeSet = &EMIT(compiler, SyncToSetAndContinueInst<true>)->set;
                else
                    runtimeSet = &EMIT(compiler, SyncToSetAndContinueInst<false>)->set;
            }
            else if(isNegation)
                runtimeSet = &EMIT(compiler, SyncToSetAndBackupInst<true>, prevConsumes)->set;
            else
                runtimeSet = &EMIT(compiler, SyncToSetAndBackupInst<false>, prevConsumes)->set;
            consumedChars = 0;
        }
        runtimeSet->CloneFrom(compiler.rtAllocator, set);
        return consumedChars;
    }

    bool MatchSetNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        if (isNegation || set.IsEmpty() || !oi->BeginUnions())
            return false;
        Assert(CharSet<Char>::MaxCompact >= TrigramAlphabet::AlphaCount);
        Char entries[CharSet<Char>::MaxCompact];
        int count = set.GetCompactEntries(TrigramAlphabet::AlphaCount, entries);
        if (count < 0)
            // Too many unique characters
            return false;
        for (int i = 0; i < count; i++)
        {
            if (!oi->UnionChar(compiler.standardChars->ToCanonical(compiler.program->GetCaseMappingSource(), entries[i])))
                // Too many unique characters
                return false;
        }
        oi->EndUnions(); // this doesn't need to be called if we return false earlier since the OctoquadPattern won't be used
        return true;
    }

    bool MatchSetNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        if (isNegation || !set.IsCompact())
            return false;
        const uint count = set.Count();
        if(count == 0)
            return false; // empty set always fails and consumes nothing, and therefore is not a char-trie arm
        accNumAlts *= count;
        if (accNumAlts > maxTrieArmExpansion)
            return false;
        return true;
    }

    bool MatchSetNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(!isNegation && set.IsCompact());
        Char entries[CharSet<Char>::MaxCompact];
        int count = set.GetCompactEntries(CharSet<Char>::MaxCompact, entries);
        Assert(count > 0);

        for (int i = 0; i < count; i++)
        {
            if (trie->IsAccepting())
            {
                // An earlier literal is a prefix of this literal
                // If isAcceptFirst, can ignore suffix of already recognized literal.
                // Otherwise, must fail.
                return isAcceptFirst;
            }
            CharTrie* tail = trie->Add(compiler.ctAllocator, entries[i]);
            if (trie->Count() > maxTrieArmExpansion)
                return false;
            if (cont == 0)
            {
                if (tail->Count() > 0)
                    // This literal is a proper prefix of an earlier literal
                    return false;
                tail->SetAccepting();
            }
            else
            {
                if (!cont->BuildCharTrie(compiler, tail, 0, isAcceptFirst))
                    return false;
            }
        }
        return true;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void MatchSetNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->Print(L"MatchSet(%s, ", isNegation ? L"negative" : L"positive");
        set.Print(w);
        w->PrintEOL(L")");
        PrintAnnotations(w);
    }
#endif

    // ----------------------------------------------------------------------
    // ConcatNode
    // ----------------------------------------------------------------------

    CharCount ConcatNode::LiteralLength() const
    {
        return 0;
    }

    bool ConcatNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount ConcatNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(tail != 0);
        CharCount n = 0;
#if DBG
        ConcatNode* prev = 0;
#endif
        for (ConcatNode* curr = this; curr != 0; curr = curr->tail)
        {
            Assert(curr->head->tag != Concat);
            Assert(prev == 0 || !(prev->head->LiteralLength() > 0 && curr->head->LiteralLength() > 0));
            n += curr->head->TransferPass0(compiler, litbuf);
#if DBG
            prev = curr;
#endif
        }
        return n;
    }

    void ConcatNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        for (ConcatNode *curr = this; curr != 0; curr = curr->tail)
            curr->head->TransferPass1(compiler, litbuf);
    }

    bool ConcatNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void ConcatNode::AnnotatePass0(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Node* prev = 0;
        for (ConcatNode* curr = this; curr != 0; curr = curr->tail)
        {
            curr->head->AnnotatePass0(compiler);
            if (prev != 0)
            {
                if (curr->head->tag == WordBoundary && prev->isWord)
                {
                    WordBoundaryNode* wb = (WordBoundaryNode*)curr->head;
                    wb->mustIncludeLeaving = true;
                }
                else if (prev->tag == WordBoundary && curr->head->isWord)
                {
                    WordBoundaryNode* wb = (WordBoundaryNode*)prev;
                    wb->mustIncludeEntering = true;
                }
            }
            prev = curr->head;
        }
    }

    void ConcatNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        features = HasConcat;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;

        // Pass 1: Count items
        int n = 0;
        for (ConcatNode *curr = this; curr != 0; curr = curr->tail)
            n++;

        // Pass 2: Annotate each item, accumulate features, consumes, find longest prefix of possibly-empty-accepting items,
        //         check if all items are irrefutable
        int emptyPrefix = 0;
        thisConsumes.Exact(0);
        isThisIrrefutable = true;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        int item = 0;
        for (ConcatNode* curr = this; curr != 0; curr = curr->tail, item++)
        {
            curr->head->AnnotatePass1(compiler, parentNotInLoop, parentAtLeastOnce, parentNotSpeculative, isNotNegated);
            features |= curr->head->features;
            thisConsumes.Add(curr->head->thisConsumes);
            if (!curr->head->isThisIrrefutable)
                isThisIrrefutable = false;
            if (!curr->head->isThisWillNotProgress)
                isThisWillNotProgress = false;
            if (!curr->head->isThisWillNotRegress)
                isThisWillNotRegress = false;
            if (emptyPrefix == item && curr->head->thisConsumes.CouldMatchEmpty())
                emptyPrefix++;
        }

        if (emptyPrefix == 0)
        {
            firstSet = head->firstSet;
            isFirstExact = head->isFirstExact;
        }
        else
        {
            // Pass 3: Overall first set is union of first's of emptyPrefx
            firstSet = Anew(compiler.ctAllocator, UnicodeCharSet);
            isFirstExact = true;
            item = 0;
            for (ConcatNode* curr = this; item <= min(emptyPrefix, n - 1); curr = curr->tail, item++)
            {
                AnalysisAssert(curr);
                firstSet->UnionInPlace(compiler.ctAllocator, *curr->head->firstSet);
                if (!curr->head->isFirstExact)
                    isFirstExact = false;
            }
        }
    }

    void ConcatNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
        for (ConcatNode* curr = this; curr != 0; curr = curr->tail)
        {
            curr->head->AnnotatePass2(compiler, accumConsumes, accumPrevWillNotProgress, accumPrevWillNotRegress);
            accumConsumes.Add(curr->head->thisConsumes);
            if (!curr->head->isThisWillNotProgress)
                accumPrevWillNotProgress = false;
            if (!curr->head->isThisWillNotRegress)
                accumPrevWillNotRegress = false;
        }
    }

    void ConcatNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;

        // Pass 1: Count items
        int n = 0;
        for (ConcatNode* curr = this; curr != 0; curr = curr->tail)
            n++;

        // Pass 2: Collect items so we can enumerate backwards
        Node** nodes = AnewArray(compiler.ctAllocator, Node *, n);
        int item = 0;
        for (ConcatNode* curr = this; curr != 0; curr = curr->tail, item++)
            nodes[item] = curr->head;

        // Pass 3: Work backwards propagating follow set, irrefutability and FollowEndLineOrPattern, and adding consumes
        CharSet<Char>* innerFollow = accumFollow;
        for (item = n - 1; item >= 0; item--)
        {
            nodes[item]->AnnotatePass3(compiler, accumConsumes, innerFollow, accumFollowIrrefutable, accumFollowEOL);
            if (!nodes[item]->isThisIrrefutable)
                accumFollowIrrefutable = false;
            if (!nodes[item]->IsEmptyOnly() && (compiler.program->flags & MultilineRegexFlag) == 0)
                accumFollowEOL = nodes[item]->tag == EOL;

            // ConcatNode has hardfail BOI test if any child has hardfail BOI
            hasInitialHardFailBOI = hasInitialHardFailBOI || nodes[item]->hasInitialHardFailBOI;

            if (item > 0)
            {
                CharSet<Char>* nextInnerFollow = Anew(compiler.ctAllocator, UnicodeCharSet);
                if (nodes[item]->thisConsumes.CouldMatchEmpty() && !nodes[item]->IsRefiningAssertion(compiler))
                {
                    // Later follows can shine through this item to the previous item
                    nextInnerFollow->UnionInPlace(compiler.ctAllocator, *innerFollow);
                }
                nextInnerFollow->UnionInPlace(compiler.ctAllocator, *nodes[item]->firstSet);
                innerFollow = nextInnerFollow;
                accumConsumes.Add(nodes[item]->thisConsumes);
            }
        }
    }

    void ConcatNode::AnnotatePass4(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        isDeterministic = true;
        for (ConcatNode* curr = this; curr != 0; curr = curr->tail)
        {
            curr->head->AnnotatePass4(compiler);
            if (!curr->head->isDeterministic)
                isDeterministic = false;
        }
    }

    bool ConcatNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        int prefix = 0;
        for (const ConcatNode* curr = this; curr != 0; curr = curr->tail)
        {
            if (curr->head->SupportsPrefixSkipping(compiler))
                prefix++;
            else
                break;
        }
        return prefix > 0;
    }

    Node* ConcatNode::HeadSyncronizingNode(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        return head->HeadSyncronizingNode(compiler);
    }

    void ConcatNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);

        for (ConcatNode *curr = this; curr != 0; curr = curr->tail)
            curr->head->AccumDefineGroups(scriptContext, minGroup, maxGroup);
    }

    CharCount ConcatNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        return 0;
    }

    void ConcatNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void ConcatNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        for (ConcatNode* curr = this; curr != 0; curr = curr->tail)
            curr->head->BestSyncronizingNode(compiler, bestNode);
    }

    void ConcatNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        //
        // Compilation scheme:
        //
        //   <item 1>
        //   ...
        //   <item n>
        //
        // :-)

        for (ConcatNode *curr = this; curr != 0; curr = curr->tail)
            curr->head->Emit(compiler, skipped);
    }

    CharCount ConcatNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        Assert(false);
        return 0;
    }

    bool ConcatNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        for (ConcatNode* curr = this; curr != 0; curr = curr->tail)
        {
            if (!curr->head->IsOctoquad(compiler, oi))
                return false;
        }
        return true;
    }

    bool ConcatNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        for (const ConcatNode* curr = this; curr != 0; curr = curr->tail)
        {
            if (!curr->head->IsCharTrieArm(compiler, accNumAlts))
                return false;
        }
        return true;
    }

    bool ConcatNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        if (cont != 0)
            // We don't want to manage a stack of continuations
            return false;
        // NOTE: This is the only place we use an internal node of a concat sequence as a sub-concat sequence
        return head->BuildCharTrie(compiler, trie, tail, isAcceptFirst);
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void ConcatNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->PrintEOL(L"Concat()");
        PrintAnnotations(w);
        w->PrintEOL(L"{");
        w->Indent();
        for (const ConcatNode *curr = this; curr != 0; curr = curr->tail)
            curr->head->Print(w, litbuf);
        w->Unindent();
        w->PrintEOL(L"}");
    }
#endif

    // ----------------------------------------------------------------------
    // AltNode
    // ----------------------------------------------------------------------

    CharCount AltNode::LiteralLength() const
    {
        return 0;
    }

    bool AltNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount AltNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(tail != 0);
        CharCount n = 0;
#if DBG
        AltNode* prev = 0;
#endif
        for (AltNode* curr = this; curr != 0; curr = curr->tail)
        {
            Assert(curr->head->tag != Alt);
            Assert(prev == 0 || !(prev->head->IsCharOrPositiveSet() && curr->head->IsCharOrPositiveSet()));
            n += curr->head->TransferPass0(compiler, litbuf);
#if DBG
            prev = curr;
#endif
        }
        return n;
    }

    void AltNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        for (AltNode *curr = this; curr != 0; curr = curr->tail)
            curr->head->TransferPass1(compiler, litbuf);
    }

    bool AltNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void AltNode::AnnotatePass0(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        isWord = true;
        for (AltNode* curr = this; curr != 0; curr = curr->tail)
        {
            curr->head->AnnotatePass0(compiler);
            if (!curr->head->isWord)
                isWord = false;
        }
    }

    void AltNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        features = HasAlt;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;

        // Overall alternative:
        //  - is irrefutable if at least one item is irrefutable
        //  - will not progress(regress) if each item will not progress(regress) and has strictly decreasing(increasing) consumes

        firstSet = Anew(compiler.ctAllocator, UnicodeCharSet);
        isThisIrrefutable = false;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isFirstExact = true;
        CountDomain prevConsumes;
        int item = 0;
        for (AltNode *curr = this; curr != 0; curr = curr->tail, item++)
        {
            curr->head->AnnotatePass1(compiler, parentNotInLoop, false, parentNotSpeculative, isNotNegated);
            features |= curr->head->features;
            if (!curr->head->isThisWillNotProgress)
                isThisWillNotProgress = false;
            if (!curr->head->isThisWillNotRegress)
                isThisWillNotRegress = false;
            if (item == 0)
                prevConsumes = thisConsumes = curr->head->thisConsumes;
            else
            {
                thisConsumes.Lub(curr->head->thisConsumes);
                if (!curr->head->thisConsumes.IsLessThan(prevConsumes))
                    isThisWillNotProgress = false;
                if (!curr->head->thisConsumes.IsGreaterThan(prevConsumes))
                    isThisWillNotRegress = false;
                prevConsumes = curr->head->thisConsumes;
            }
            firstSet->UnionInPlace(compiler.ctAllocator, *curr->head->firstSet);
            if (!curr->head->isFirstExact || curr->head->isThisIrrefutable)
                // If any item is irrefutable then later items may never be taken, so first set cannot be exact
                isFirstExact = false;
            if (curr->head->isThisIrrefutable)
                isThisIrrefutable = true;
        }
    }

    void AltNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
        for (AltNode* curr = this; curr != 0; curr = curr->tail)
            curr->head->AnnotatePass2(compiler, accumConsumes, accumPrevWillNotProgress, accumPrevWillNotRegress);
    }

    void AltNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
        for (AltNode* curr = this; curr != 0; curr = curr->tail)
        {
            curr->head->AnnotatePass3(compiler, accumConsumes, accumFollow, accumFollowIrrefutable, accumFollowEOL);

            // AltNode has hardfail BOI test if all child Nodes have hardfail BOI tests
            hasInitialHardFailBOI = curr->head->hasInitialHardFailBOI && (hasInitialHardFailBOI || (curr == this));
        }
    }

    void AltNode::AnnotatePass4(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        //
        // Simplification rule
        //
        // If the follow is irrefutable then we can ignore all items after an irrefutable item, since
        // we'll never be able to backtrack into them.
        // E.g.: (a*|b*)c* === a*c*
        //

        bool simplified = false;
        if (isFollowIrrefutable && isThisIrrefutable)
        {
            for (AltNode* curr = this; curr != 0; curr = curr->tail)
            {
                if (curr->head->isThisIrrefutable && curr->tail != 0)
                {
                    curr->tail = 0;
                    simplified = true;
                    break;
                }
            }
        }

        if (simplified)
        {
            Assert(!isFirstExact);
            // Recalculate firstSet. Since it can only get smaller, and alternative could not have had an exact
            // first set, this recalculation does not make any decisions already made based on the current firstSet
            // unsound.
            // NOTE: Is it worth recalculating the WillNotProgess/WillNotRegress bools?
            firstSet = Anew(compiler.ctAllocator, UnicodeCharSet);
            for (AltNode* curr = this; curr != 0; curr = curr->tail)
                firstSet->UnionInPlace(compiler.ctAllocator, *curr->head->firstSet);
        }

        //
        // Annotate items
        //
        isDeterministic = true;
        for (AltNode* curr = this; curr != 0; curr = curr->tail)
        {
            curr->head->AnnotatePass4(compiler);
            if (!curr->head->isDeterministic)
                isDeterministic = false;
        }

        //
        // Compilation scheme: Switch/Chain/Set, not isOptional
        //
        // If no item can match empty and all items' FIRST sets are pairwise disjoint then we can
        // commit to an item using a 1 char lookahead. We can fall-through to the last
        // item without guarding it since it will fail if the next character cannot match.
        // E.g.: (abc|def)
        //

        {
            // Pass 1: Items cannot match empty, accumulate counts
            bool fires = true;
            bool allCompact = true;
            bool allSimpleOneChar = true;
            int numItems = 0;
            uint totalChars = 0;
            for (AltNode* curr = this; curr != 0; curr = curr->tail)
            {
                if (curr->head->thisConsumes.CouldMatchEmpty())
                {
                    fires = false;
                    break;
                }
                numItems++;
                if (!curr->head->firstSet->IsCompact())
                    allCompact = false;
                if (!curr->head->IsSimpleOneChar())
                    allSimpleOneChar = false;
                totalChars += curr->head->firstSet->Count();
            }

            if (fires)
            {
                // To go from two to one items requires the first item
                // to be irrefutable, in which case it could match empty and this rule won't fire.
                Assert(numItems > 1);
                // Step 2: Check FIRST sets are disjoint
                if (totalChars == firstSet->Count())
                {
                    // **COMMIT**
                    if (allSimpleOneChar)
                        // This will probably never fire since the parser has already converted alts-of-chars/sets
                        // to sets. We include it for symmetry with below.
                        scheme = Set;
                    else if (allCompact && totalChars <= Switch20Inst::MaxCases)
                    {
                        // Can use a switch instruction to jump to item
                        scheme = Switch;
                        switchSize = totalChars;
                    }
                    else
                        // Must use a chain of jump instructions to jump to item
                        scheme = Chain;
                    isOptional = false;
                    return;
                }
            }
        }


        //
        // Compilation scheme: None/Switch/Chain/Set, isOptional
        //
        // Condition (1):
        // If some items are empty-only, the rest (if any) cannot match empty, follow cannot match empty, and
        // all items' FIRST sets are pairwise disjoint and disjoint from the FOLLOW set, then we can commit to
        // either a non-empty item or to the empty item using a 1 char lookahead. In this case we just emit each
        // non-empty item with a guard, and fall-through to follow if no guard fires.
        // E.g.: (abc||def)h
        //
        // Condition (2):
        // If some items are empty-only, the rest (if any) cannot match empty, follow is irrefutable, and all
        // items' FIRST sets are pairwise disjoint, then we can commit to either a non-empty item or to the empty
        // item using a 1 char lookahead, provided each non-empty item obeys the condition:
        //   ** the item can't fail if given an arbitrary input starting with a character in its FIRST set **
        // Currently, we can prove that only for IsSimpleOneChar items, though more analysis could widen the class.
        // Again, we emit each non-empty item with a guard, and fall-through to follow if no guard fires.
        // E.g.: ([abc]|)a*
        //
        // Condition (3):
        // If all items are empty-only, we can commit to a single empty-only item

        {
            // Pass 1
            bool fires = false;
            bool allSimpleOneChar = true;
            bool allCompact = true;
            int numNonEmpty = 0;
            uint totalChars = 0;
            for (AltNode* curr = this; curr != 0; curr = curr->tail)
            {
                if (curr->head->IsEmptyOnly())
                    fires = true;
                else if (curr->head->thisConsumes.CouldMatchEmpty())
                {
                    fires = false;
                    break;
                }
                else
                {
                    numNonEmpty++;
                    if (!curr->head->IsSimpleOneChar())
                        allSimpleOneChar = false;
                    if (!curr->head->firstSet->IsCompact())
                        allCompact = false;
                    totalChars += curr->head->firstSet->Count();
                }
            }

            if (fires)
            {
                // The firing condition is not strong enough yet.
                fires = false;
                // Check conditions (2) and (3) first because they're faster, then check condition (1).
                if (numNonEmpty == 0 || isFollowIrrefutable && allSimpleOneChar && totalChars == firstSet->Count())
                {
                    fires = true;
                }
                else if (!followConsumes.CouldMatchEmpty())
                {
                    // Check whether all FIRST sets are pairwise disjoint
                    // and disjoint from the FOLLOW set.
                    CharSet<Char> unionSet;
                    unionSet.UnionInPlace(compiler.ctAllocator, *firstSet);
                    unionSet.UnionInPlace(compiler.ctAllocator, *followSet);
                    if (totalChars + followSet->Count() == unionSet.Count())
                        fires = true;
                }

                if (fires)
                {
                    // **COMMIT**
                    if (numNonEmpty == 0)
                        scheme = None;
                    else if (allSimpleOneChar)
                        scheme = Set;
                    else if (numNonEmpty > 1 && allCompact && totalChars <= Switch20Inst::MaxCases)
                    {
                        switchSize = totalChars;
                        scheme = Switch;
                    }
                    else
                        scheme = Chain;
                    isOptional = true;
                    return;
                }
            }
        }

        //
        // Compilation scheme: Trie
        //
        // If alt is equivalent to the form:
        //   (literal1|...|literaln)
        // (we expand items with embedded character classes such as a[bc]d to (abd|acd)) and either:
        //  - follow is irrefutable and no later literal is a proper prefix of an earlier literal
        //    (and we may ignore later literals which have an earlier literal as proper prefix)
        //    E.g.: (ab|ac|abd)a* === (ab|ac)a*
        // or:
        //  - follow is not irrefutable and no literal is a proper prefix of any other literal
        //    and the branching factor of the resulting trie is smallish
        //    E.g.: (abc|abd|abe)f
        // then we can use a character trie to match the appropriate item.
        //

        {
            // Pass 1: Items must be structurally appropriate and not result in too many alternatives after expansion
            bool fires = true;
            for (AltNode* curr = this; curr != 0; curr = curr->tail)
            {
                uint numAlts = 1;
                if (!curr->head->IsCharTrieArm(compiler, numAlts))
                {
                    fires = false;
                    break;
                }
                if (numAlts > maxTrieArmExpansion)
                {
                    fires = false;
                    break;
                }
            }

            if (fires)
            {
                // Pass 2: Attempt to construct the trie, checking for prefixes.
                CharTrie trie;
                for (AltNode* curr = this; curr != 0; curr = curr->tail)
                {
                    if (!curr->head->BuildCharTrie(compiler, &trie, 0, isFollowIrrefutable))
                    {
                        fires = false;
                        break;
                    }
                }

                if (fires)
                {
                    // **COMMIT**
                    // If follow is irrefutable and first item is empty, the trie would be of depth zero.
                    // However, in this case, the first simplification rule would have replaced the alt with a
                    // single empty item, and the 'None' compilation scheme would have been selected above.
                    //
                    // Similarly, if all alternations are empty and follow is refutable, the trie would be
                    // of depth zero, and the 'None' compilation scheme would have been selected above.
                    Assert(!trie.IsDepthZero());
                    if (trie.IsDepthOne())
                    {
                        // This case will fire if follow is irrefutable and all non length one items have an
                        // earlier one-character item as prefix. In this case we don't need the trie: the
                        // firstSet has all the information.
                        isOptional = false;
                        scheme = Set;
                    }
                    else
                    {
                        // Root of trie will live in compile-time allocator, but body will be in run-time allocator
                        runtimeTrie = Anew(compiler.ctAllocator, RuntimeCharTrie);
                        runtimeTrie->CloneFrom(compiler.rtAllocator, trie);
                        scheme = Trie;
                    }
                    return;
                }
            }
        }

        //
        // Compilation scheme: Try
        //

        scheme = Try;
        isDeterministic = false; // NON-DETERMINISTIC
    }

    bool AltNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return false;
    }

    Node* AltNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return 0;
    }

    CharCount AltNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        // Here, we ignore nodes with length 1, which are Char nodes. The way the Alt node synchronization
        // is currently implemented, it expects all nodes to be Literal nodes. It requires quite a bit of
        // refactoring to have Alt nodes support Char nodes for synchronization, so Char nodes are ignored
        // for now.

        int localNumLiterals = numLiterals;
        CharCount minLen = head->MinSyncronizingLiteralLength(compiler, localNumLiterals);
        if (minLen <= 1)
            return 0;
        for (AltNode* curr = tail; curr != 0; curr = curr->tail)
        {
            CharCount thisLen = curr->head->MinSyncronizingLiteralLength(compiler, localNumLiterals);
            if (thisLen <= 1)
                return 0;
            minLen = min(minLen, thisLen);
        }
        numLiterals = localNumLiterals;

        if (minLen <= 1)
        {
            return 0;
        }

        return minLen;
    }

    void AltNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        for (const AltNode* curr = this; curr != 0; curr = curr->tail)
            curr->head->CollectSyncronizingLiterals(compiler, scanners);
    }

    void AltNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        if (IsBetterSyncronizingNode(compiler, bestNode, this))
            bestNode = this;
    }

    void AltNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);

        for (AltNode *curr = this; curr != 0; curr = curr->tail)
            curr->head->AccumDefineGroups(scriptContext, minGroup, maxGroup);
    }

    void AltNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(skipped == 0);
        switch (scheme)
        {
            case Try:
                {
                    //
                    // Compilation scheme:
                    //
                    //          Try((If|Match)(Char|Set))? L2
                    //          <item 1>
                    //          Jump Lexit
                    //   L2:    Try((If|Match)(Char|Set))? L3
                    //          <item 2>
                    //          Jump Lexit
                    //   L3:    <item 3>
                    //   Lexit:
                    //
                    Assert(!isOptional);
                    int numItems = 0;
                    for (AltNode* curr = this; curr != 0; curr = curr->tail)
                        numItems++;
                    Assert(numItems >= 1);
                    // Each item other than last needs to jump to exit on success
                    Label* jumpFixups = AnewArray(compiler.ctAllocator, Label, (numItems - 1));
                    Label lastTryFixup = 0;
                    int item = 0;
                    for (AltNode* curr = this; curr != 0; curr = curr->tail, item++)
                    {
                        if (item > 0)
                            // Fixup previous Try
                            compiler.DoFixup(lastTryFixup, compiler.CurrentLabel());
                        CharCount itemSkipped = 0;
                        if (item < numItems-1)
                        {
                            // HEURISTIC: if the first set of the alternative is exact or small, and the
                            //            alternative does not match empty, then it's probably worth using
                            //            a Try(If|Match)(Char|Set)
                            if (curr->head->firstSet != 0 &&
                                !curr->head->thisConsumes.CouldMatchEmpty() &&
                                (curr->head->isFirstExact || curr->head->firstSet->Count() <= maxCharsForConditionalTry))
                            {
                                if (curr->head->SupportsPrefixSkipping(compiler))
                                {
                                    if (curr->head->firstSet->IsSingleton())
                                        lastTryFixup = compiler.GetFixup(&EMIT(compiler, TryMatchCharInst, curr->head->firstSet->Singleton())->failLabel);
                                    else
                                    {
                                        TryMatchSetInst* const i = EMIT(compiler, TryMatchSetInst);
                                        i->set.CloneFrom(compiler.rtAllocator, *curr->head->firstSet);
                                        lastTryFixup = compiler.GetFixup(&i->failLabel);
                                    }
                                    itemSkipped = 1;
                                }
                                else
                                {
                                    if (curr->head->firstSet->IsSingleton())
                                        lastTryFixup = compiler.GetFixup(&EMIT(compiler, TryIfCharInst, curr->head->firstSet->Singleton())->failLabel);
                                    else
                                    {
                                        TryIfSetInst* const i = EMIT(compiler, TryIfSetInst);
                                        i->set.CloneFrom(compiler.rtAllocator, *curr->head->firstSet);
                                        lastTryFixup = compiler.GetFixup(&i->failLabel);
                                    }
                                }
                            }
                            else
                                lastTryFixup = compiler.GetFixup(&EMIT(compiler, TryInst)->failLabel);
                        }
                        curr->head->Emit(compiler, itemSkipped);
                        if (item < numItems-1)
                            jumpFixups[item] = compiler.GetFixup(&EMIT(compiler, JumpInst)->targetLabel);
                    }
                    // Fixup jumps
                    for (item = 0; item < numItems-1; item++)
                        compiler.DoFixup(jumpFixups[item], compiler.CurrentLabel());
                    break;
                }
            case None:
                {
                    Assert(isOptional);
                    // Nothing to emit
                    break;
                }
            case Trie:
                {
                    //
                    // Compilation scheme:
                    //
                    //     MatchTrie <trie>
                    //
                    EMIT(compiler, MatchTrieInst)->trie = *runtimeTrie;
                    break;
                }
            case Switch:
                {
                    //
                    // Compilation scheme:
                    //
                    //            Switch(AndConsume)?(10|20)(<dispatch to each arm>)
                    //            Fail                                (if non-optional)
                    //            Jump Lexit                          (if optional)
                    //     L1:    <item1>
                    //            Jump Lexit
                    //     L2:    <item2>
                    //            Jump Lexit
                    //     L3:    <item3>
                    //     Lexit:
                    //
                    Assert(switchSize <= Switch20Inst::MaxCases);
                    int numItems = 0;
                    bool allCanSkip = true;
                    for (AltNode* curr = this; curr != 0; curr = curr->tail)
                    {
                        if (curr->head->thisConsumes.CouldMatchEmpty())
                        {
                            Assert(isOptional);
                        }
                        else
                        {
                            numItems++;
                            if (!curr->head->SupportsPrefixSkipping(compiler))
                                allCanSkip = false;
                        }
                    }
                    Assert(numItems > 1);

                    // Each item other than last needs to jump to exit on success
                    Label* jumpFixups = AnewArray(compiler.ctAllocator, Label, (numItems - 1));
                    // We must remember where each item begins to fixup switch
                    Label* caseLabels = AnewArray(compiler.ctAllocator, Label, numItems);
                    // We must fixup the switch arms
                    Label switchLabel = compiler.CurrentLabel();
                    Assert(switchSize <= Switch20Inst::MaxCases);
                    if (allCanSkip)
                    {
                        if (switchSize > Switch10Inst::MaxCases)
                            EMIT(compiler, SwitchAndConsume20Inst);
                        else
                            EMIT(compiler, SwitchAndConsume10Inst);
                    }
                    else
                    {
                        if (switchSize > Switch10Inst::MaxCases)
                            EMIT(compiler, Switch20Inst);
                        else
                            EMIT(compiler, Switch10Inst);
                    }

                    Label defaultJumpFixup = 0;
                    if (isOptional)
                        // Must fixup default jump to exit
                        defaultJumpFixup = compiler.GetFixup(&EMIT(compiler, JumpInst)->targetLabel);
                    else
                        compiler.Emit<FailInst>();

                    // Emit each item
                    int item = 0;
                    for (AltNode* curr = this; curr != 0; curr = curr->tail)
                    {
                        if (!curr->head->thisConsumes.CouldMatchEmpty())
                        {
                            if (allCanSkip)
                                skipped = 1;
                            caseLabels[item] = compiler.CurrentLabel();
                            curr->head->Emit(compiler, skipped);
                            if (item < numItems - 1)
                                jumpFixups[item] = compiler.GetFixup(&EMIT(compiler, JumpInst)->targetLabel);
                            item++;
                        }
                    }

                    // Fixup exit labels
                    if (isOptional)
                        compiler.DoFixup(defaultJumpFixup, compiler.CurrentLabel());
                    for (item = 0; item < numItems - 1; item++)
                        compiler.DoFixup(jumpFixups[item], compiler.CurrentLabel());

                    // Fixup the switch entries
                    item = 0;
                    for (AltNode* curr = this; curr != 0; curr = curr->tail)
                    {
                        if (!curr->head->thisConsumes.CouldMatchEmpty())
                        {
                            Char entries[CharSet<Char>::MaxCompact];
                            int count = curr->head->firstSet->GetCompactEntries(CharSet<Char>::MaxCompact, entries);
                            Assert(count > 0);
                            for (int i = 0; i < count; i++)
                            {
                                if (allCanSkip)
                                {
                                    if (switchSize > Switch10Inst::MaxCases)
                                        compiler.L2I(SwitchAndConsume20, switchLabel)->AddCase(entries[i], caseLabels[item]);
                                    else
                                        compiler.L2I(SwitchAndConsume10, switchLabel)->AddCase(entries[i], caseLabels[item]);
                                }
                                else
                                {
                                    if (switchSize > Switch10Inst::MaxCases)
                                        compiler.L2I(Switch20, switchLabel)->AddCase(entries[i], caseLabels[item]);
                                    else
                                        compiler.L2I(Switch10, switchLabel)->AddCase(entries[i], caseLabels[item]);
                                }
                            }
                            item++;
                        }
                    }
                    break;
                }
            case Chain:
                {
                    //
                    // Compilation scheme:
                    //
                    //           JumpIfNot(Char|Set) L2
                    //           <item1>
                    //           Jump Lexit
                    //    L2:    JumpIfNot(Char|Set) L3
                    //           <item2>
                    //           Jump Lexit
                    //    L3:    <item3>                              (if non-optional)
                    //    L3:    JumpIfNot(Char|Set) Lexit            (if optional)
                    //           <item3>                              (if optional)
                    //    Lexit:
                    //
                    int numItems = 0;
                    for (AltNode* curr = this; curr != 0; curr = curr->tail)
                    {
                        if (curr->head->thisConsumes.CouldMatchEmpty())
                        {
                            Assert(isOptional);
                        }
                        else
                            numItems++;
                    }
                    Assert(numItems > 0);
                    // Each item other than last needs to jump to exit on success
                    Label* jumpFixups = AnewArray(compiler.ctAllocator, Label, (numItems - 1));
                    Label lastJumpFixup = 0;
                    int item = 0;
                    for (AltNode* curr = this; curr != 0; curr = curr->tail)
                    {
                        if (!curr->head->thisConsumes.CouldMatchEmpty())
                        {
                            if (item > 0)
                                // Fixup previous Jump
                                compiler.DoFixup(lastJumpFixup, compiler.CurrentLabel());

                            CharCount itemSkipped = 0;
                            if (item < numItems-1 || isOptional)
                            {
                                if (curr->head->firstSet->IsSingleton())
                                {
                                    if (curr->head->SupportsPrefixSkipping(compiler))
                                    {
                                        lastJumpFixup = compiler.GetFixup(&EMIT(compiler, MatchCharOrJumpInst, curr->head->firstSet->Singleton())->targetLabel);
                                        itemSkipped = 1;
                                    }
                                    else
                                        lastJumpFixup = compiler.GetFixup(&EMIT(compiler, JumpIfNotCharInst, curr->head->firstSet->Singleton())->targetLabel);
                                }
                                else
                                {
                                    if (curr->head->SupportsPrefixSkipping(compiler))
                                    {
                                        MatchSetOrJumpInst* const i = EMIT(compiler, MatchSetOrJumpInst);
                                        i->set.CloneFrom(compiler.rtAllocator, *curr->head->firstSet);
                                        lastJumpFixup = compiler.GetFixup(&i->targetLabel);
                                        itemSkipped = 1;
                                    }
                                    else
                                    {
                                        JumpIfNotSetInst* const i = EMIT(compiler, JumpIfNotSetInst);
                                        i->set.CloneFrom(compiler.rtAllocator, *curr->head->firstSet);
                                        lastJumpFixup = compiler.GetFixup(&i->targetLabel);
                                    }
                                }
                            }
                            curr->head->Emit(compiler, itemSkipped);
                            if (item < numItems-1)
                                jumpFixups[item] = compiler.GetFixup(&EMIT(compiler, JumpInst)->targetLabel);
                            item++;
                        }
                    }
                    // Fixup jumps to exit
                    for (item = 0; item < numItems-1; item++)
                        compiler.DoFixup(jumpFixups[item], compiler.CurrentLabel());
                    if (isOptional)
                        // Fixup last Jump to exit
                        compiler.DoFixup(lastJumpFixup, compiler.CurrentLabel());
                    break;
                }
            case Set:
                {
                    //
                    // Compilation scheme:
                    //
                    //          Match(Char|Set)    (non optional)
                    //          OptMatch(Char|Set) (optional)
                    //
                    if (isOptional)
                    {
                        if (firstSet->IsSingleton())
                            EMIT(compiler, OptMatchCharInst, firstSet->Singleton());
                        else
                            EMIT(compiler, OptMatchSetInst)->set.CloneFrom(compiler.rtAllocator, *firstSet);
                    }
                    else
                    {
                        if (firstSet->IsSingleton())
                            EMIT(compiler, MatchCharInst, firstSet->Singleton());
                        else
                            EMIT(compiler, MatchSetInst<false>)->set.CloneFrom(compiler.rtAllocator, *firstSet);
                    }
                    break;
                }
        }
    }

    CharCount AltNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(!isHeadSyncronizingNode);

        //
        // Compilation scheme:
        //
        //   SyncToLiteralsAndBackup
        //
        SyncToLiteralsAndBackupInst* i =
            EMIT(
                compiler,
                SyncToLiteralsAndBackupInst,
                compiler.GetScriptContext()->GetRecycler(),
                compiler.GetProgram(),
                prevConsumes);
        CollectSyncronizingLiterals(compiler, *i);
        return 0;
    }

    bool AltNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        if (tail == 0 || tail->tail != 0)
            // Must be exactly two alts
            return false;
        for (AltNode* curr = this; curr != 0; curr = curr->tail)
        {
            if (!oi->BeginConcat())
                return false;
            if (!curr->head->IsOctoquad(compiler, oi))
                return false;
        }
        return true;
    }

    bool AltNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        return false;
    }

    bool AltNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        Assert(false);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void AltNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->PrintEOL(L"Alt()");
        PrintAnnotations(w);
        w->PrintEOL(L"{");
        w->Indent();
        for (const AltNode *curr = this; curr != 0; curr = curr->tail)
            curr->head->Print(w, litbuf);
        w->Unindent();
        w->PrintEOL(L"}");
    }
#endif

    // ----------------------------------------------------------------------
    // DefineGroupNode
    // ----------------------------------------------------------------------

    CharCount DefineGroupNode::LiteralLength() const
    {
        return 0;
    }

    bool DefineGroupNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount DefineGroupNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(groupId > 0 && groupId < compiler.program->numGroups);
        return body->TransferPass0(compiler, litbuf);
    }

    void DefineGroupNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->TransferPass1(compiler, litbuf);
    }

    bool DefineGroupNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void DefineGroupNode::AnnotatePass0(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->AnnotatePass0(compiler);
        isWord = body->isWord;
    }

    void DefineGroupNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        features = HasDefineGroup;
        body->AnnotatePass1(compiler, parentNotInLoop, parentAtLeastOnce, parentNotSpeculative, parentNotNegated);
        features |= body->features;
        thisConsumes = body->thisConsumes;
        firstSet = body->firstSet;
        isFirstExact = body->isFirstExact;
        isThisIrrefutable = body->isThisIrrefutable;
        isThisWillNotProgress = body->isThisWillNotProgress;
        isThisWillNotRegress = body->isThisWillNotRegress;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
    }

    void DefineGroupNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
        body->AnnotatePass2(compiler, accumConsumes, accumPrevWillNotProgress, accumPrevWillNotRegress);
    }

    void DefineGroupNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
        body->AnnotatePass3(compiler, accumConsumes, accumFollow, accumFollowIrrefutable, accumFollowEOL);

        hasInitialHardFailBOI = body->hasInitialHardFailBOI;
    }

    void DefineGroupNode::AnnotatePass4(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->AnnotatePass4(compiler);
        isDeterministic = body->isDeterministic;

        // If the follow is irrefutable and we're not in an assertion, then we are not going to backtrack beyond this point, so
        // we don't need to save the group before updating it
        noNeedToSave = isFollowIrrefutable && isNotSpeculative;

        // Compilation scheme: Chomp
        //
        // Body consists of a loop node with a Chomp compilation scheme.

        if(body->tag == NodeTag::Loop)
        {
            const LoopNode *const loop = static_cast<const LoopNode *>(body);
            if(loop->scheme == LoopNode::CompilationScheme::Chomp && loop->repeats.lower <= 1 && loop->repeats.IsUnbounded())
            {
                // **COMMIT**
                scheme = Chomp;
                return;
            }
        }

        // Compilation scheme: Fixed
        //
        // Body has fixed width, so don't need a Begin instruction to keep track of the input start offset of the group.

        if (body->thisConsumes.IsFixed())
        {
            // **COMMIT**
            scheme = Fixed;
            return;
        }

        // Compilation scheme: BeginEnd
        //
        // If both the body and the follow are irrefutable, we're not in any loops, and we're not in an assertion,
        // then we don't need to save the group before updating it.

        // **COMMIT**
        scheme = BeginEnd;
    }

    bool DefineGroupNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        if (scheme != Fixed)
            // We can't skip over part of the match if the BeginDefineGroup must capture it's start
            return false;
        return body->SupportsPrefixSkipping(compiler);
    }

    Node* DefineGroupNode::HeadSyncronizingNode(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        if (scheme != Fixed)
            // Can't skip BeginDefineGroup
            return 0;
        return body->HeadSyncronizingNode(compiler);
    }

    CharCount DefineGroupNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        return body->MinSyncronizingLiteralLength(compiler, numLiterals);
    }

    void DefineGroupNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->CollectSyncronizingLiterals(compiler, scanners);
    }

    void DefineGroupNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->BestSyncronizingNode(compiler, bestNode);
    }

    void DefineGroupNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);

        if (groupId < minGroup)
            minGroup = groupId;
        if (groupId > maxGroup)
            maxGroup = groupId;
        body->AccumDefineGroups(scriptContext, minGroup, maxGroup);
    }

    void DefineGroupNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        switch (scheme)
        {
            case Chomp:
            {
                // Compilation scheme:
                //
                //   Chomp(Char|Set)Group
                Assert(body->tag == NodeTag::Loop);
                const LoopNode *const loop = static_cast<const LoopNode *>(body);
                const CharSet<Char> *const loopBodyFirstSet = loop->body->firstSet;
                const CountDomain &repeats = loop->repeats;
                Assert(repeats.lower <= 1 && repeats.IsUnbounded());
                if(loopBodyFirstSet->IsSingleton())
                {
                    const Char c = loopBodyFirstSet->Singleton();
                    if(repeats.lower == 0)
                        EMIT(compiler, ChompCharGroupInst<ChompMode::Star>, c, groupId, noNeedToSave);
                    else
                        EMIT(compiler, ChompCharGroupInst<ChompMode::Plus>, c, groupId, noNeedToSave);
                }
                else
                {
                    Assert(repeats.lower <= 1 && repeats.IsUnbounded());
                    RuntimeCharSet<Char> *runtimeSet;
                    if(repeats.lower == 0)
                        runtimeSet = &EMIT(compiler, ChompSetGroupInst<ChompMode::Star>, groupId, noNeedToSave)->set;
                    else
                        runtimeSet = &EMIT(compiler, ChompSetGroupInst<ChompMode::Plus>, groupId, noNeedToSave)->set;
                    runtimeSet->CloneFrom(compiler.rtAllocator, *loopBodyFirstSet);
                }
                break;
            }

            case Fixed:
            {
                // Compilation scheme:
                //
                //   <body>
                //   DefineGroup
                Assert(body->thisConsumes.IsFixed());
                body->Emit(compiler, skipped);
                EMIT(compiler, DefineGroupFixedInst, groupId, body->thisConsumes.lower, noNeedToSave);
                break;
            }

            case BeginEnd:
            {
                // Compilation scheme:
                //
                //   BeginDefineGroup
                //   <body>
                //   EndDefineGroup
                Assert(skipped == 0);
                EMIT(compiler, BeginDefineGroupInst, groupId);
                body->Emit(compiler, skipped);
                EMIT(compiler, EndDefineGroupInst, groupId, noNeedToSave);
                break;
            }
        }
    }

    CharCount DefineGroupNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        Assert(false);
        return 0;
    }

    bool DefineGroupNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        return false;
    }

    bool DefineGroupNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        return false;
    }

    bool DefineGroupNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        Assert(false);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void DefineGroupNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->PrintEOL(L"DefineGroup(%d)", groupId);
        PrintAnnotations(w);
        w->PrintEOL(L"{");
        w->Indent();
        body->Print(w, litbuf);
        w->Unindent();
        w->PrintEOL(L"}");
    }
#endif

    // ----------------------------------------------------------------------
    // MatchGroupNode
    // ----------------------------------------------------------------------

    CharCount MatchGroupNode::LiteralLength() const
    {
        return 0;
    }

    bool MatchGroupNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount MatchGroupNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        Assert(groupId > 0 && groupId < compiler.program->numGroups);
        return 0;
    }

    void MatchGroupNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
    }

    bool MatchGroupNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void MatchGroupNode::AnnotatePass0(Compiler& compiler)
    {
        isWord = false;
    }

    void MatchGroupNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        features = HasMatchGroup;
        thisConsumes.lower = 0;
        thisConsumes.upper = CharCountFlag;
        firstSet = compiler.standardChars->GetFullSet();
        isFirstExact = false;
        isThisIrrefutable = false;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
    }

    void MatchGroupNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
    }

    void MatchGroupNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
    }

    void MatchGroupNode::AnnotatePass4(Compiler& compiler)
    {
        isDeterministic = true;
    }

    bool MatchGroupNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return false;
    }

    Node* MatchGroupNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return 0;
    }

    CharCount MatchGroupNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        return 0;
    }

    void MatchGroupNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void MatchGroupNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
    }

    void MatchGroupNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
    }

    void MatchGroupNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        Assert(skipped == 0);
        //
        // Compilation scheme:
        //
        //   MatchGroup
        //
        EMIT(compiler, MatchGroupInst, groupId);
    }

    CharCount MatchGroupNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        Assert(false);
        return 0;
    }

    bool MatchGroupNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        return false;
    }

    bool MatchGroupNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        return false;
    }

    bool MatchGroupNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        Assert(false);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void MatchGroupNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->PrintEOL(L"MatchGroup(%d)", groupId);
        PrintAnnotations(w);
    }
#endif

    // ----------------------------------------------------------------------
    // LoopNode
    // ----------------------------------------------------------------------

    CharCount LoopNode::LiteralLength() const
    {
        return 0;
    }

    bool LoopNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount LoopNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(repeats.upper == CharCountFlag || repeats.upper > 0);
        Assert(repeats.upper == CharCountFlag || repeats.upper >= repeats.lower);
        Assert(!(repeats.lower == 1 && repeats.upper == 1));
        return body->TransferPass0(compiler, litbuf);
    }

    void LoopNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->TransferPass1(compiler, litbuf);
    }

    bool LoopNode::IsRefiningAssertion(Compiler& compiler)
    {
        return false;
    }

    void LoopNode::AnnotatePass0(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->AnnotatePass0(compiler);
        isWord = !repeats.CouldMatchEmpty() && body->isWord;
    }

    void LoopNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        features = HasLoop;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
        body->AnnotatePass1(compiler, false, parentAtLeastOnce && repeats.lower > 0, parentNotSpeculative, isNotNegated);
        features |= body->features;
        thisConsumes = body->thisConsumes;
        thisConsumes.Mult(repeats);
        firstSet = body->firstSet;
        isFirstExact = repeats.lower > 0 && body->isFirstExact;
        isThisIrrefutable = repeats.CouldMatchEmpty() || body->isThisIrrefutable;
        // Caution: Even if a greedy loop has a 'isThisWillNotProgress' body, if the body has choicepoints then
        // a backtrack could resume execution at an earlier loop iteration, which may then continue to repeat
        // the loop beyond the input offset which triggered the backtrack. Ideally we'd use the body's isDeterministic
        // flag to tell us when that can't happen, but it's not available till pass 4, so we must make do with
        // a simple-minded structural approximation.
        isThisWillNotProgress = (isGreedy || repeats.IsExact(1)) && body->isThisWillNotProgress && body->IsObviouslyDeterministic();
        isThisWillNotRegress = (!isGreedy || repeats.IsExact(1)) && body->isThisWillNotRegress && body->IsObviouslyDeterministic();
    }

    void LoopNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
        // May have already gone through loop when starting body
        CountDomain bodyConsumes = body->thisConsumes;
        CharCountOrFlag prevMax = repeats.upper;
        if (prevMax != CharCountFlag)
            prevMax--;
        CountDomain prevLoops(0, prevMax);
        bodyConsumes.Mult(prevLoops);
        accumConsumes.Add(bodyConsumes);
        body->AnnotatePass2(compiler, accumConsumes, accumPrevWillNotProgress && isThisWillNotProgress, accumPrevWillNotRegress && isThisWillNotRegress);
    }

    void LoopNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
        // May go through loop again when leaving body
        CountDomain bodyConsumes = body->thisConsumes;
        CharCountOrFlag nextMax = repeats.upper;
        if (nextMax != CharCountFlag)
            nextMax--;
        CountDomain nextLoops(0, nextMax);
        bodyConsumes.Mult(nextLoops);
        accumConsumes.Add(bodyConsumes);
        CharSet<Char>* innerFollow = Anew(compiler.ctAllocator, UnicodeCharSet);
        innerFollow->UnionInPlace(compiler.ctAllocator, *accumFollow);
        innerFollow->UnionInPlace(compiler.ctAllocator, *body->firstSet);

        /*
        All of the following must be true for the loop body's follow to be irrefutable:

            The loop's follow is irrefutable.

            The loop can complete the required minimum number of iterations of the body without backtracking into a completed
            iteration of the body.
                - If repeats.lower == 0, the required minimum number of iterations is met without executing the body
                - If repeats.lower == 1
                    - If the first iteration of the body fails, there is no previous iteration of the body to backtrack into
                    - After completing the first iteration of the body, the loop cannot reject the first iteration for not
                      making progress because the iteration is required for the loop to succeed
                - If repeats.lower >= 2
                    - If the second iteration of the body fails, it will backtrack into the first iteration of the body
                    - To prevent this, the body must be irrefutable

            After completing the required minimum number of iterations of the body, the loop cannot reject a subsequent
            completed iteration of the body for not making progress.
                - If !isGreedy || repeats.IsFixed(), there will not be any more iterations of the body, as it will proceed to
                  the irrefutable follow
                - If !body->thisConsumes.CouldMatchEmpty(), subsequent iterations of the body cannot complete without making
                  progress
        */
        const bool isBodyFollowIrrefutable =
            accumFollowIrrefutable &&
            (repeats.lower <= 1 || body->isThisIrrefutable) &&
            (!isGreedy || !body->thisConsumes.CouldMatchEmpty() || repeats.IsFixed());
        body->AnnotatePass3(compiler, accumConsumes, innerFollow, isBodyFollowIrrefutable, false);

        hasInitialHardFailBOI = body->hasInitialHardFailBOI;
    }

    void LoopNode::AnnotatePass4(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->AnnotatePass4(compiler);
        isDeterministic = body->isDeterministic;

        //
        // Loops can be defined by unfolding:
        //   r* === (rr*|)
        //   r*? === (|rr*?)
        // Thus many of the optimizations for alternatives carry over to loops.
        //

        //
        // Compilation scheme: None
        //
        // If overall loop is empty-only then emit nothing.
        // (Parser has already eliminated loops with upper == 0, so this can only happen if the body is empty-only)
        //
        // If loop is non-greedy with lower 0 and follow is irrefutable, then loop body will never be executed
        // no emit nothing.
        //
        if (body->IsEmptyOnly() ||
            (!isGreedy && repeats.lower == 0 && isFollowIrrefutable))
        {
            // **COMMIT**
            scheme = None;
            return;
        }

        //
        // Compilation scheme: Chain/Try
        //
        // If loop is greedy, with lower 0 and upper 1, then we'd like to treat it as body|<empty> so as to avoid all the loop
        // overhead. However, if the body could match empty, then a match against empty must be treated as an 'iteration' of the
        // loop body which made no progress. So we treat as a general loop in that case. Otherwise, we may inline two of
        // AltNode's compilation schemes:
        //     Examples:
        //         - /(a*)?/.exec("") must leave group 1 undefined rather than empty.
        //         - /(?:a||b)?/.exec("b") chooses the empty alt, then must backtrack due to no progress, and match 'b'.
        //           This is not the same as /a||b|/, as picking the first empty alt would result in success.
        //
        // (cf AltNode's None/Switch/Chain/Set, isOptional compilation scheme)
        // If body cannot match empty, follow cannot match empty, and the body FIRST set is disjoint from the FOLLOW
        // set, then we can commit to the body using a 1 char lookahead.
        //
        // If body cannot match empty, and follow is irrefutable, then we can commit to the body using a 1 char
        // lookahead provided:
        //   ** the body can't fail if given an arbitrary input starting with a character in its FIRST set **
        //
        // (cf AltNode's Try compilation scheme)
        // Otherwise, protect body by a Try instruction.
        //
        if (isGreedy && repeats.lower == 0 && repeats.upper == 1 && !body->thisConsumes.CouldMatchEmpty())
        {
            // **COMMIT**
            // Note that the FIRST of the loop is already the union of the body FIRST and the loop FOLLOW
            if (!body->thisConsumes.CouldMatchEmpty() &&
                ((!followConsumes.CouldMatchEmpty() && firstSet->Count() == body->firstSet->Count() + followSet->Count()) ||
                (isFollowIrrefutable && body->IsSimpleOneChar())))
            {
                if (body->IsSimpleOneChar())
                    scheme = Set;
                else
                    scheme = Chain;
            }
            else
            {
                scheme = Try;
                isDeterministic = false; // NON-DETERMINISTIC
            }
            return;
        }

        //
        // Compilation scheme: Chomp/ChompGroupLastChar
        //
        // If the body is a simple-one-char, or a group of a simple-one-char, and either:
        //  - follow is non-empty and FIRST and FOLLOW are disjoint
        //  - loop is greedy and follow is irrefutable
        //  - follow is EOL
        // then consume up to upper number of characters in FIRST and fail if number consumed is not >= lower.
        //

        if (body->IsSimpleOneChar() || (body->tag == DefineGroup && ((DefineGroupNode*)body)->body->IsSimpleOneChar()))
        {
            if (!followConsumes.CouldMatchEmpty())
            {
                CharSet<Char> unionSet;
                CharCount totalChars = 0;
                unionSet.UnionInPlace(compiler.ctAllocator, *body->firstSet);
                totalChars += body->firstSet->Count();
                unionSet.UnionInPlace(compiler.ctAllocator, *followSet);
                totalChars += followSet->Count();
                if (totalChars == unionSet.Count())
                {
                    // **COMMIT**
                    if (body->tag == DefineGroup)
                    {
                        noNeedToSave = isFollowIrrefutable && isNotInLoop && isNotSpeculative;
                        scheme = ChompGroupLastChar;
                    }
                    else
                        scheme = Chomp;
                    return;
                }
            }

            if ((isGreedy && isFollowIrrefutable) || isFollowEOL)
            {
                // **COMMIT**
                if (body->tag == DefineGroup)
                {
                    noNeedToSave = isFollowIrrefutable && isNotInLoop && isNotSpeculative;
                    scheme = ChompGroupLastChar;
                }
                else
                    scheme = Chomp;
                return;
            }
        }

        //
        // Compilation scheme: Guarded
        //
        // If body cannot match empty, follow cannot match empty, and FIRST of body and FOLLOW are
        // disjoint then can use 1 char lookahead to decide whether to commit to another loop body.
        // (If the loop body fails then we know the follow will fail even with one more/fewer iterations of the
        // loop body, so we can let that failure propagate without needing to push choicepoints.)
        //

        if (!body->thisConsumes.CouldMatchEmpty() && !followConsumes.CouldMatchEmpty())
        {
            CharSet<Char> unionSet;
            CharCount totalChars = 0;
            unionSet.UnionInPlace(compiler.ctAllocator, *body->firstSet);
            totalChars += body->firstSet->Count();
            unionSet.UnionInPlace(compiler.ctAllocator, *followSet);
            totalChars += followSet->Count();
            if (totalChars == unionSet.Count())
            {
                // **COMMIT**
                scheme = Guarded;
                return;
            }
        }

        //
        // Compilation scheme: Fixed/FixedSet/FixedGroupLastIteration
        //
        // If loop is greedy, body is deterministic, non-zero fixed width, and either does not define any groups
        // or has one outermost group, then we can keep track of the backtracking information in constant space.
        //
        // If body does have an outer group, we can avoid saving the existing group contents if the follow
        // is irrefutable, we're not in an outer loop, and we're not in an assertion.
        //

        if (isGreedy && body->isDeterministic && !body->thisConsumes.CouldMatchEmpty() && body->thisConsumes.IsFixed())
        {
            if (body->tag == DefineGroup)
            {
                DefineGroupNode* bodyGroup = (DefineGroupNode*)body;
                if (!bodyGroup->body->ContainsDefineGroup())
                {
                    // **COMMIT**
                    scheme = FixedGroupLastIteration;
                    noNeedToSave = isFollowIrrefutable && isNotInLoop && isNotSpeculative;
                    isDeterministic = false; // NON-DETERMINISTIC;
                    return;
                }
            }
            else if (body->IsSimpleOneChar())
            {
                // **COMMIT**
                scheme = FixedSet;
                isDeterministic = false; // NON-DETERMINISTIC
                return;
            }
            else if (!body->ContainsDefineGroup())
            {
                // **COMMIT**
                scheme = Fixed;
                isDeterministic = false; // NON-DETERMINISTIC
                return;
            }
        }

        //
        // Compilation scheme: GreedyNoBacktrack
        //
        // If loop is greedy with lower == 0 and upper == inf, the loop body is deterministic and does not define
        // groups, and follow is irrefutable, then we will never have to try fewer iterations of the loop once
        // entering the follow. Thus we only need one continuation record on the stack to protect against failure
        // for each attempt at the loop body.
        //

        if (isGreedy && repeats.lower == 0 && repeats.upper == CharCountFlag && body->isDeterministic && !body->ContainsDefineGroup() && isFollowIrrefutable)
        {
            // **COMMIT**
            scheme = GreedyNoBacktrack;
            return;
        }

        //
        // Compilation scheme: BeginEnd
        //
        scheme = BeginEnd;
        isDeterministic = false; // NON-DETERMINISTIC
    }

    bool LoopNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return false;
    }

    Node* LoopNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return 0;
    }

    CharCount LoopNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        return 0;
    }

    void LoopNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void LoopNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        if (repeats.lower > 0)
            body->BestSyncronizingNode(compiler, bestNode);
        // else: can't be sure loop will be taken
    }

    void LoopNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);

        body->AccumDefineGroups(scriptContext, minGroup, maxGroup);
    }

    void LoopNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(skipped == 0);

        switch (scheme)
        {
            case BeginEnd:
            {
                //
                // Compilation scheme:
                //
                //   Lloop: BeginLoop Lexit
                //          <loop body>
                //          RepeatLoop Lloop
                //   Lexit:
                //
                int minBodyGroupId = compiler.program->numGroups;
                int maxBodyGroupId = -1;
                body->AccumDefineGroups(compiler.scriptContext, minBodyGroupId, maxBodyGroupId);
                Label beginLabel = compiler.CurrentLabel();
                Label fixup = compiler.GetFixup(&EMIT(compiler, BeginLoopInst, compiler.NextLoopId(), repeats, !isNotInLoop, !body->isDeterministic, minBodyGroupId, maxBodyGroupId, isGreedy)->exitLabel);
                body->Emit(compiler, skipped);
                EMIT(compiler, RepeatLoopInst, beginLabel);
                compiler.DoFixup(fixup, compiler.CurrentLabel());
                break;
            }

            case None:
            {
                // Nothing to emit
                break;
            }

            case Chomp:
            {
                //
                // Compilation scheme:
                //
                //   Chomp(Char|Set)(Star|Plus|Bounded)
                //
                if(body->firstSet->IsSingleton())
                {
                    if(repeats.lower <= 1 && repeats.IsUnbounded())
                    {
                        if(repeats.lower == 0)
                            EMIT(compiler, ChompCharInst<ChompMode::Star>, body->firstSet->Singleton());
                        else
                            EMIT(compiler, ChompCharInst<ChompMode::Plus>, body->firstSet->Singleton());
                    }
                    else
                        EMIT(compiler, ChompCharBoundedInst, body->firstSet->Singleton(), repeats);
                }
                else
                {
                    if(repeats.lower <= 1 && repeats.IsUnbounded())
                    {
                        if(repeats.lower == 0)
                            EMIT(compiler, ChompSetInst<ChompMode::Star>)->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                        else
                            EMIT(compiler, ChompSetInst<ChompMode::Plus>)->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                    }
                    else
                        EMIT(compiler, ChompSetBoundedInst, repeats)->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                }
                break;
            }

            case ChompGroupLastChar:
            {
                //
                // Compilation scheme:
                //
                //   ChompSetGroup
                //
                Assert(body->tag == DefineGroup);
                DefineGroupNode* bodyGroup = (DefineGroupNode*)body;
                EMIT(compiler, ChompSetBoundedGroupLastCharInst, repeats, bodyGroup->groupId, noNeedToSave)->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                break;
            }

            case Guarded:
            {
                //
                // Compilation scheme:
                //
                //   Lloop: BeginLoopIf(Char|Set) Lexit
                //          <loop body>
                //          RepeatLoopIf(Char|Set) Lloop
                //   Lexit:
                //
                int minBodyGroupId = compiler.program->numGroups;
                int maxBodyGroupId = -1;
                body->AccumDefineGroups(compiler.scriptContext, minBodyGroupId, maxBodyGroupId);
                Label beginLabel = compiler.CurrentLabel();
                Label exitFixup;
                if (body->firstSet->IsSingleton())
                    exitFixup = compiler.GetFixup(&EMIT(compiler, BeginLoopIfCharInst, body->firstSet->Singleton(), compiler.NextLoopId(), repeats, !isNotInLoop, !body->isDeterministic, minBodyGroupId, maxBodyGroupId)->exitLabel);
                else
                {
                    BeginLoopIfSetInst* i = EMIT(compiler, BeginLoopIfSetInst, compiler.NextLoopId(), repeats, !isNotInLoop, !body->isDeterministic, minBodyGroupId, maxBodyGroupId);
                    i->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                    exitFixup = compiler.GetFixup(&i->exitLabel);
                }
                body->Emit(compiler, skipped);
                if (body->firstSet->IsSingleton())
                    EMIT(compiler, RepeatLoopIfCharInst, beginLabel);
                else
                    EMIT(compiler, RepeatLoopIfSetInst, beginLabel);
                compiler.DoFixup(exitFixup, compiler.CurrentLabel());
                break;
            }

            case Fixed:
            {
                //
                // Compilation scheme:
                //
                //   Lloop: BeginLoopFixed Lexit
                //          <loop body>
                //          RepeatLoopFixed Lloop
                //   Lexit:
                //
                Assert(!body->ContainsDefineGroup());
                Assert(body->thisConsumes.IsFixed());
                Assert(body->thisConsumes.lower > 0);
                Assert(body->isDeterministic);
                Label beginLabel = compiler.CurrentLabel();
                Label fixup = compiler.GetFixup(&EMIT(compiler, BeginLoopFixedInst, compiler.NextLoopId(), repeats, !isNotInLoop, body->thisConsumes.lower)->exitLabel);
                body->Emit(compiler, skipped);
                EMIT(compiler, RepeatLoopFixedInst, beginLabel);
                compiler.DoFixup(fixup, compiler.CurrentLabel());
                break;
            }

            case FixedSet:
            {
                //
                // Compilation scheme:
                //
                //   LoopSet
                //
                Assert(body->IsSimpleOneChar());
                EMIT(compiler, LoopSetInst, compiler.NextLoopId(), repeats, !isNotInLoop)->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                break;
            }

            case FixedGroupLastIteration:
            {
                //
                // Compilation scheme:
                //
                //   Lloop: BeginLoopFixedGroupLastIteration Lexit
                //          <loop body>
                //          RepeatLoopFixedGroupLastIteration Lloop
                //   Lexit:
                //
                Assert(body->tag == DefineGroup);
                DefineGroupNode* bodyGroup = (DefineGroupNode*)body;
                Assert(body->thisConsumes.IsFixed());
                Assert(body->thisConsumes.lower > 0);
                Assert(body->isDeterministic);
                Label beginLabel = compiler.CurrentLabel();
                Label fixup = compiler.GetFixup(&EMIT(compiler, BeginLoopFixedGroupLastIterationInst, compiler.NextLoopId(), repeats, !isNotInLoop, body->thisConsumes.lower, bodyGroup->groupId, noNeedToSave)->exitLabel);
                bodyGroup->body->Emit(compiler, skipped);
                EMIT(compiler, RepeatLoopFixedGroupLastIterationInst, beginLabel);
                compiler.DoFixup(fixup, compiler.CurrentLabel());
                break;
            }

            case GreedyNoBacktrack:
            {
                //
                // Compilation scheme:
                //
                //   Lloop: BeginGreedyLoopNoBacktrack Lexit
                //          <loop body>
                //          RepeatGreedyLoopNoBacktrack Lloop
                //   Lexit:
                //
                Assert(!body->ContainsDefineGroup());
                Assert(isGreedy);
                Assert(repeats.lower == 0);
                Assert(repeats.upper == CharCountFlag);
                Assert(body->isDeterministic);
                Label beginLabel = compiler.CurrentLabel();
                Label fixup = compiler.GetFixup(&EMIT(compiler, BeginGreedyLoopNoBacktrackInst, compiler.NextLoopId())->exitLabel);
                body->Emit(compiler, skipped);
                EMIT(compiler, RepeatGreedyLoopNoBacktrackInst, beginLabel);
                compiler.DoFixup(fixup, compiler.CurrentLabel());
                break;
            }

            case Set:
            {
                //
                // Compilation scheme:
                //
                //   OptMatch(Char|Set)
                //
                Assert(!body->ContainsDefineGroup() || !body->thisConsumes.CouldMatchEmpty());
                if (body->firstSet->IsSingleton())
                    EMIT(compiler, OptMatchCharInst, body->firstSet->Singleton());
                else
                    EMIT(compiler, OptMatchSetInst)->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                break;
            }

            case Chain:
            {
                //
                // Compilation scheme:
                //
                //          JumpIfNot(Char|Set) Lexit
                //          <body>
                //   Lexit:
                //
                //
                Assert(!body->ContainsDefineGroup() || !body->thisConsumes.CouldMatchEmpty());
                Label jumpFixup = 0;
                CharCount bodySkipped = 0;
                if (body->firstSet->IsSingleton())
                {
                    if (body->SupportsPrefixSkipping(compiler))
                    {
                        jumpFixup = compiler.GetFixup(&EMIT(compiler, MatchCharOrJumpInst, body->firstSet->Singleton())->targetLabel);
                        bodySkipped = 1;
                    }
                    else
                        jumpFixup = compiler.GetFixup(&EMIT(compiler, JumpIfNotCharInst, body->firstSet->Singleton())->targetLabel);
                }
                else
                {
                    if (body->SupportsPrefixSkipping(compiler))
                    {
                        MatchSetOrJumpInst* const i = EMIT(compiler, MatchSetOrJumpInst);
                        i->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                        jumpFixup = compiler.GetFixup(&i->targetLabel);
                        bodySkipped = 1;
                    }
                    else
                    {
                        JumpIfNotSetInst* const i = EMIT(compiler, JumpIfNotSetInst);
                        i->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                        jumpFixup = compiler.GetFixup(&i->targetLabel);
                    }
                }
                body->Emit(compiler, bodySkipped);
                compiler.DoFixup(jumpFixup, compiler.CurrentLabel());
                break;
            }

            case Try:
            {
                //
                // Compilation scheme:
                //
                //          Try((If|Match)(Char|Set))? Lexit
                //          <loop body>
                //   Lexit:
                //
                Assert(!body->ContainsDefineGroup() || !body->thisConsumes.CouldMatchEmpty());
                Label tryFixup = 0;
                CharCount bodySkipped = 0;
                // HEURISTIC: if the first set of the body is exact or small, and the
                //            body does not match empty, then it's probably worth using
                //            a Try(If|Match)(Char|Set)
                if (!body->thisConsumes.CouldMatchEmpty() &&
                    (body->isFirstExact || body->firstSet->Count() <= maxCharsForConditionalTry))
                {
                    if (body->SupportsPrefixSkipping(compiler))
                    {
                        if (body->firstSet->IsSingleton())
                            tryFixup = compiler.GetFixup(&EMIT(compiler, TryMatchCharInst, body->firstSet->Singleton())->failLabel);
                        else
                        {
                            TryMatchSetInst* const i = EMIT(compiler, TryMatchSetInst);
                            i->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                            tryFixup = compiler.GetFixup(&i->failLabel);
                        }
                        bodySkipped = 1;
                    }
                    else
                    {
                        if(body->firstSet->IsSingleton())
                            tryFixup = compiler.GetFixup(&EMIT(compiler, TryIfCharInst, body->firstSet->Singleton())->failLabel);
                        else
                        {
                            TryIfSetInst* const i = EMIT(compiler, TryIfSetInst);
                            i->set.CloneFrom(compiler.rtAllocator, *body->firstSet);
                            tryFixup = compiler.GetFixup(&i->failLabel);
                        }
                    }
                }
                else
                    tryFixup = compiler.GetFixup(&EMIT(compiler, TryInst)->failLabel);
                body->Emit(compiler, bodySkipped);
                // Fixup Try
                compiler.DoFixup(tryFixup, compiler.CurrentLabel());
                break;
            }
        }
    }

    CharCount LoopNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        Assert(false);
        return 0;
    }

    bool LoopNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        return false;
    }

    bool LoopNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        return false;
    }

    bool LoopNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        Assert(false);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void LoopNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->Print(L"Loop(");
        repeats.Print(w);
        w->PrintEOL(L", %s)", isGreedy ? L"greedy" : L"non-greedy");
        PrintAnnotations(w);
        w->PrintEOL(L"{");
        w->Indent();
        body->Print(w, litbuf);
        w->Unindent();
        w->PrintEOL(L"}");
    }
#endif

    // ----------------------------------------------------------------------
    // AssertionNode
    // ----------------------------------------------------------------------

    CharCount AssertionNode::LiteralLength() const
    {
        return 0;
    }

    bool AssertionNode::IsCharOrPositiveSet() const
    {
        return false;
    }

    CharCount AssertionNode::TransferPass0(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        return body->TransferPass0(compiler, litbuf);
    }

    void AssertionNode::TransferPass1(Compiler& compiler, const Char* litbuf)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->TransferPass1(compiler, litbuf);
    }

    bool AssertionNode::IsRefiningAssertion(Compiler& compiler)
    {
        return !isNegation;
    }

    void AssertionNode::AnnotatePass0(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        isWord = false;
        body->AnnotatePass0(compiler);
    }

    void AssertionNode::AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        features = HasAssertion;
        body->AnnotatePass1(compiler, parentNotInLoop, parentAtLeastOnce, false, parentNotNegated && !isNegation);
        features |= body->features;
        thisConsumes.Exact(0);
        if (isNegation)
            firstSet = compiler.standardChars->GetFullSet();
        else
            firstSet = body->firstSet;
        isFirstExact = false;
        if (isNegation)
            // This will always fail
            isThisIrrefutable = false;
        else
            // If body is irrefutable overall assertion is irrefutable
            isThisIrrefutable = body->isThisIrrefutable;
        isThisWillNotProgress = true;
        isThisWillNotRegress = true;
        isNotInLoop = parentNotInLoop;
        isAtLeastOnce = parentAtLeastOnce;
        isNotSpeculative = parentNotSpeculative;
        isNotNegated = parentNotNegated;
    }

    void AssertionNode::AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        prevConsumes = accumConsumes;
        isPrevWillNotProgress = accumPrevWillNotProgress;
        isPrevWillNotRegress = accumPrevWillNotRegress;
        body->AnnotatePass2(compiler, accumConsumes, accumPrevWillNotProgress, accumPrevWillNotRegress);
    }

    void AssertionNode::AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        followConsumes = accumConsumes;
        followSet = accumFollow;
        isFollowIrrefutable = accumFollowIrrefutable;
        isFollowEOL = accumFollowEOL;
        // Can't say anything about what the assertion body will see at its end
        CountDomain innerConsumes;
        CharSet<Char>* innerFollow = compiler.standardChars->GetFullSet();

        // We can never backtrack into the body of an assertion (the continuation stack is cut)
        body->AnnotatePass3(compiler, innerConsumes, innerFollow, true, false);

        hasInitialHardFailBOI = body->hasInitialHardFailBOI;
    }

    void AssertionNode::AnnotatePass4(Compiler& compiler)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        body->AnnotatePass4(compiler);
        // Even if body is non-deterministic we cut the choicepoints on exit from the assertion,
        // so overall assertion is deterministic.
        isDeterministic = true;

        //
        // Compilation scheme: Fail
        //
        // If body is irrefutable, assertion will always fail (and will leave groups empty).
        //
        if (isNegation && body->isThisIrrefutable)
        {
            // ***COMMIT***
            scheme = Fail;
            return;
        }

        //
        // Compilation scheme: Succ
        //
        // If body is irrefutable, assertion will always succeed. If it does not define groups
        // we can eliminate it altogether.
        //
        if (!isNegation && body->isThisIrrefutable && !body->ContainsDefineGroup())
        {
            // **COMMIT**
            scheme = Succ;
            return;
        }

        //
        // Compilation scheme: BeginEnd
        //
        scheme = BeginEnd;
    }

    bool AssertionNode::SupportsPrefixSkipping(Compiler& compiler) const
    {
        return false;
    }

    Node* AssertionNode::HeadSyncronizingNode(Compiler& compiler)
    {
        return 0;
    }

    CharCount AssertionNode::MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const
    {
        return 0;
    }

    void AssertionNode::CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const
    {
        Assert(false);
    }

    void AssertionNode::BestSyncronizingNode(Compiler& compiler, Node*& bestNode)
    {
    }

    void AssertionNode::AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);

        body->AccumDefineGroups(scriptContext, minGroup, maxGroup);
    }

    void AssertionNode::Emit(Compiler& compiler, CharCount& skipped)
    {
        PROBE_STACK(compiler.scriptContext, Js::Constants::MinStackRegex);

        Assert(skipped == 0);

        switch (scheme)
        {
        case BeginEnd:
            {
                //
                // Compilation scheme:
                //
                //          BeginAssertion Lexit
                //          <body>
                //          EndAssertion
                //   Lexit:
                //
                int minBodyGroupId = compiler.program->numGroups;
                int maxBodyGroupId = -1;
                body->AccumDefineGroups(compiler.scriptContext, minBodyGroupId, maxBodyGroupId);
                Label fixup = compiler.GetFixup(&EMIT(compiler, BeginAssertionInst, isNegation, minBodyGroupId, maxBodyGroupId)->nextLabel);
                body->Emit(compiler, skipped);
                EMIT(compiler, EndAssertionInst);
                compiler.DoFixup(fixup, compiler.CurrentLabel());
                break;
            }
        case Succ:
            {
                // Nothing to emit
                break;
            }
        case Fail:
            {
                //
                // Compilation scheme:
                //
                //     Fail
                //
                EMIT(compiler, FailInst);
                break;
            }
        }
    }

    CharCount AssertionNode::EmitScan(Compiler& compiler, bool isHeadSyncronizingNode)
    {
        Assert(false);
        return 0;
    }

    bool AssertionNode::IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi)
    {
        return false;
    }

    bool AssertionNode::IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const
    {
        return false;
    }

    bool AssertionNode::BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const
    {
        Assert(false);
        return false;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void AssertionNode::Print(DebugWriter* w, const Char* litbuf) const
    {
        w->PrintEOL(L"Assertion(%s)", isNegation ? L"negative" : L"positive");
        PrintAnnotations(w);
        w->PrintEOL(L"{");
        w->Indent();
        body->Print(w, litbuf);
        w->Unindent();
        w->PrintEOL(L"}");
    }
#endif

    // ----------------------------------------------------------------------
    // Compiler
    // ----------------------------------------------------------------------

    Compiler::Compiler
        ( Js::ScriptContext* scriptContext
        , ArenaAllocator* ctAllocator
        , ArenaAllocator* rtAllocator
        , StandardChars<Char>* standardChars
        , Program* program
#if ENABLE_REGEX_CONFIG_OPTIONS
        , DebugWriter* w
        , RegexStats* stats
#endif
        )
        : scriptContext(scriptContext)
        , ctAllocator(ctAllocator)
        , rtAllocator(rtAllocator)
        , standardChars(standardChars)
#if ENABLE_REGEX_CONFIG_OPTIONS
        , w(w)
        , stats(stats)
#endif
        , program(program)
        , instBuf(0)
        , instLen(0)
        , instNext(0)
        , nextLoopId(0)
    {}

    void Compiler::CaptureNoLiterals(Program* program)
    {
        program->rep.insts.litbuf = 0;
        program->rep.insts.litbufLen = 0;
    }

    void Compiler::CaptureLiterals(Node* root, const Char* litbuf)
    {
        // Program will own literal buffer. Prepare buffer and nodes for case-invariant matching if necessary.
        CharCount finalLen = root->TransferPass0(*this, litbuf);
        program->rep.insts.litbuf = finalLen == 0 ? 0 : RecyclerNewArrayLeaf(scriptContext->GetRecycler(), Char, finalLen);

        program->rep.insts.litbufLen = 0;
        root->TransferPass1(*this, litbuf);
        Assert(program->rep.insts.litbufLen == finalLen);
    }

    void Compiler::EmitAndCaptureSuccInst(Recycler* recycler, Program* program)
    {
        program->rep.insts.insts = (uint8*)RecyclerNewLeaf(recycler, SuccInst);
        program->rep.insts.instsLen = sizeof(SuccInst);
        program->numLoops = 0;
    }

    void Compiler::CaptureInsts()
    {
        program->rep.insts.insts = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), uint8, instNext);

        program->rep.insts.instsLen = instNext;
        memcpy_s(program->rep.insts.insts, instNext, instBuf, instNext);
        program->numLoops = nextLoopId;
    }

    void Compiler::FreeBody()
    {
        if (instBuf != 0)
        {
            ctAllocator->Free(instBuf, instLen);
            instBuf = 0;
            instLen = 0;
            instNext = 0;
        }
    }

    void Compiler::CompileEmptyRegex
        ( Program* program
        , RegexPattern* pattern
#if ENABLE_REGEX_CONFIG_OPTIONS
        , DebugWriter* w
        , RegexStats* stats
#endif
        )
    {
        program->tag = Program::InstructionsTag;
        CaptureNoLiterals(program);
        EmitAndCaptureSuccInst(pattern->GetScriptContext()->GetRecycler(), program);
    }

    void Compiler::Compile
        ( Js::ScriptContext* scriptContext
        , ArenaAllocator* ctAllocator
        , ArenaAllocator* rtAllocator
        , StandardChars<Char>* standardChars
        , Program *program
        , Node* root
        , const Char* litbuf
        , RegexPattern* pattern
#if ENABLE_REGEX_CONFIG_OPTIONS
        , DebugWriter* w
        , RegexStats* stats
#endif
        )
    {

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (w != 0)
        {
            w->PrintEOL(L"REGEX AST /%s/ {", program->source);
            w->Indent();
            root->Print(w, litbuf);
            w->Unindent();
            w->PrintEOL(L"}");
            w->Flush();
        }
#endif

        Compiler compiler
            ( scriptContext
            , ctAllocator
            , rtAllocator
            , standardChars
            , program
#if ENABLE_REGEX_CONFIG_OPTIONS
            , w
            , stats
#endif
            );

        bool compiled = false;

        if (REGEX_CONFIG_FLAG(RegexOptimize))
        {
            // SPECIAL CASE: Octoquad/trigrams
            // (must handle before converting to case-insensitive form since the later interferes with octoquad pattern recognizer)
            if (OctoquadIdentifier::Qualifies(program))
            {
                int numCodes;
                char localCodeToChar[TrigramAlphabet::AlphaCount];
                char localCharToCode[TrigramAlphabet::AsciiTableSize];
                char (*codeToChar)[TrigramAlphabet::AlphaCount];
                char (*charToCode)[TrigramAlphabet::AsciiTableSize];
                TrigramAlphabet *trigramAlphabet = scriptContext->GetTrigramAlphabet();
                if(trigramAlphabet)
                {
                    numCodes = TrigramAlphabet::AlphaCount;
                    codeToChar = &trigramAlphabet->alpha;
                    charToCode = &trigramAlphabet->alphaBits;
                }
                else
                {
                    numCodes = 0;
                    codeToChar = &localCodeToChar;
                    charToCode = &localCharToCode;
                }

                OctoquadIdentifier oi(numCodes, *codeToChar, *charToCode);
                // We haven't captured literals yet: temporarily set the program's litbuf to be the parser's litbuf
                Assert(program->rep.insts.litbuf == 0);
                program->rep.insts.litbuf = (Char*)litbuf;
                if (root->IsOctoquad(compiler, &oi) && oi.IsOctoquad())
                {
                    program->rep.insts.litbuf = 0;
                    oi.InitializeTrigramInfo(scriptContext, pattern);
                    program->tag = Program::OctoquadTag;
                    program->rep.octoquad.matcher = OctoquadMatcher::New(scriptContext->GetRecycler(), standardChars, program->GetCaseMappingSource(), &oi);
                    compiled = true;
                }
                else
                    program->rep.insts.litbuf = 0;
            }
        }

        if (!compiled)
        {
            if (REGEX_CONFIG_FLAG(RegexOptimize))
            {
                Char c;
                if (root->IsSingleChar(compiler, c))
                {
                    // SPECIAL CASE: c
                    program->tag = Program::SingleCharTag;
                    program->rep.singleChar.c = c;
                }
                else if (root->IsBoundedWord(compiler))
                {
                    // SPECIAL CASE: \b\w+\b
                    program->tag = Program::BoundedWordTag;
                }
                else if (root->IsLeadingTrailingSpaces(compiler,
                    program->rep.leadingTrailingSpaces.beginMinMatch,
                    program->rep.leadingTrailingSpaces.endMinMatch))
                {
                    // SPECIAL CASE: ^\s*|\s*$
                    program->tag = Program::LeadingTrailingSpacesTag;
                }
                else if (root->IsBOILiteral2(compiler))
                {
                    program->tag = Program::BOILiteral2Tag;
                    program->rep.boiLiteral2.literal = *(DWORD *)litbuf;
                }
                else
                {
                    program->tag = Program::InstructionsTag;
                    compiler.CaptureLiterals(root, litbuf);

                    root->AnnotatePass0(compiler);
                    root->AnnotatePass1(compiler, true, true, true, true);
                    // Nothing comes before or after overall pattern
                    CountDomain consumes(0);
                    // Match could progress from lhs (since we try successive start positions), but can never regress
                    root->AnnotatePass2(compiler, consumes, false, true);
                    // Anything could follow an end of pattern match
                    CharSet<Char>* follow = standardChars->GetFullSet();
                    root->AnnotatePass3(compiler, consumes, follow, true, false);
                    root->AnnotatePass4(compiler);

#if ENABLE_REGEX_CONFIG_OPTIONS
                    if (w != 0)
                    {
                        w->PrintEOL(L"REGEX ANNOTATED AST /%s/ {", program->source);
                        w->Indent();
                        root->Print(w, program->rep.insts.litbuf);
                        w->Unindent();
                        w->PrintEOL(L"}");
                        w->Flush();
                    }
#endif

                    CharCount skipped = 0;

                    // If the root Node has a hard fail BOI, we should not emit any synchronize Nodes
                    // since we can easily just search from the beginning.
                    if (root->hasInitialHardFailBOI == false)
                    {
                        // If the root Node doesn't have hard fail BOI but sticky flag is present don't synchronize Nodes
                        // since we can easily just search from the beginning. Instead set to special InstructionTag
                        if ((program->flags & StickyRegexFlag) != 0)
                        {
                            compiler.SetBOIInstructionsProgramForStickyFlagTag();
                        }
                        else
                        {
                            Node* bestSyncronizingNode = 0;
                            root->BestSyncronizingNode(compiler, bestSyncronizingNode);
                            Node* headSyncronizingNode = root->HeadSyncronizingNode(compiler);

                            if ((bestSyncronizingNode == 0 && headSyncronizingNode != 0) ||
                                (bestSyncronizingNode != 0 && headSyncronizingNode == bestSyncronizingNode))
                            {
                                // Scan and consume the head, continue with rest assuming head has been consumed
                                skipped = headSyncronizingNode->EmitScan(compiler, true);
                            }
                            else if (bestSyncronizingNode != 0)
                            {
                                // Scan for the synchronizing node, then backup ready for entire pattern
                                skipped = bestSyncronizingNode->EmitScan(compiler, false);
                                Assert(skipped == 0);

                                // We're synchronizing to a non-head node; if we have to back up, then try to synchronize to a character
                                // in the first set before running the remaining instructions
                                if (!bestSyncronizingNode->prevConsumes.CouldMatchEmpty()) // must back up at least one character
                                    skipped = root->EmitScanFirstSet(compiler);
                            }
                            else
                            {
                                // Optionally scan for a character in the overall pattern's FIRST set, possibly consume it,
                                // then match all or remainder of pattern
                                skipped = root->EmitScanFirstSet(compiler);
                            }
                        }
                    }

                    root->Emit(compiler, skipped);

                    compiler.Emit<SuccInst>();
                    compiler.CaptureInsts();
                }
            }
            else
            {
                program->tag = Program::InstructionsTag;
                compiler.CaptureLiterals(root, litbuf);
                CharCount skipped = 0;
                root->Emit(compiler, skipped);
                compiler.Emit<SuccInst>();
                compiler.CaptureInsts();
            }
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (w != 0)
        {
            w->PrintEOL(L"REGEX PROGRAM /%s/ ", program->source);
            program->Print(w);
            w->Flush();
        }
#endif
    }
}
