//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// Regex parsing and AST-to-AST transformations/analysis
//

#pragma once

namespace UnifiedRegex
{
    // FORWARD
    class Compiler;

    // ----------------------------------------------------------------------
    // Node
    // ----------------------------------------------------------------------

    struct Node : protected Chars<wchar_t>
    {
        // Optimization heuristics
        static const int maxSyncToSetSize = 256;
        static const int preferredMinSyncToLiteralLength = 3;
        static const int maxNumSyncLiterals = ScannersMixin::MaxNumSyncLiterals;
        static const int minRemainLengthForTest = 4;
        static const int maxCharsForConditionalTry = 20;
        static const int maxTrieArmExpansion = 16;

        enum NodeTag : uint16
        {
            // SimpleNode
            Empty,                // (|...), etc
            BOL,                  // ^
            EOL,                  // $
            // WordBoundaryNode
            WordBoundary,         // \b, \B
            // MatchLiteralNode
            MatchLiteral,         // abc, non-empty
            // MatchCharNode
            MatchChar,            // a
            // ConcatNode
            Concat,               // e e
            // AltNode
            Alt,                  // e | e
            // DefineGroupNode
            DefineGroup,          // (e)
            // MatchGroupNode
            MatchGroup,           // \1
            // LoopNode
            Loop,                 // e*, e+, e{n,m}, e*?, e+?, e{n,m}?
            // MatchSetNode
            MatchSet,             // [...], [^...], \b, \B, \d, \D, \s, \S, \w, \W, .
            // AssertionNode
            Assertion             // (?=e), (?!e)
        };

        enum Features : uint16
        {
            HasEmpty = 1 << Empty,
            HasBOL = 1 << BOL,
            HasEOL = 1 << EOL,
            HasWordBoundary = 1 << WordBoundary,
            HasMatchLiteral = 1 << MatchLiteral,
            HasMatchChar = 1 << MatchChar,
            HasConcat = 1 << Concat,
            HasAlt = 1 << Alt,
            HasDefineGroup = 1 << DefineGroup,
            HasMatchGroup = 1 << MatchGroup,
            HasLoop = 1 << Loop,
            HasMatchSet = 1 << MatchSet,
            HasAssertion = 1 << Assertion
        };

        NodeTag tag;

        // Features of this and all child nodes
        uint16 features;

        // See comment for firstSet
        bool isFirstExact : 1;
        // True if pattern can never fail
        bool isThisIrrefutable : 1;
        // True if following patterns can never fail
        bool isFollowIrrefutable : 1;
        // True if pattern matches one or more word characters
        bool isWord : 1;
        // True if pattern will not consume more characters on backtracking
        bool isThisWillNotProgress : 1;
        // True if pattern will not consume fewer characters on backtracking
        bool isThisWillNotRegress : 1;
        // True if previous patterns will not consume more characters on backtracking
        bool isPrevWillNotProgress : 1;
        // True if previous patterns will not consume fewer characters on backtracking
        bool isPrevWillNotRegress : 1;
        // True if $ always follows pattern (and we are not in multi-line mode)
        bool isFollowEOL : 1;
        // True if pattern is deterministic (ie will never push a choicepoint during execution)
        // Determined in pass 4.
        bool isDeterministic : 1;
        // True if pattern is not in a loop context
        bool isNotInLoop : 1;
        // True if pattern will be matched against at least one segment of the input (ie will be executed at least once)
        bool isAtLeastOnce : 1;
        // True if pattern does not appear in an assertion
        bool isNotSpeculative : 1;
        // True if known to not be in a negative assertion context
        // (We do not play any games with double-negation)
        bool isNotNegated : 1;
        // True if this contains a hard fail BOI
        bool hasInitialHardFailBOI : 1;
        uint dummy : 17;

        // NOTE: The bodies of the following sets are allocated in the compile-time allocator and must be cloned
        //       into the run-time allocator if they end up being used by an instruction.
        // NOTE: Sets may be aliased between nodes, and may be one of the standard sets.

        // Upper bound of FIRST characters of this pattern.
        //  - Pattern will *never* match first characters not in this set
        //  - If isFirstExact, pattern will *always* match first characters in this set (but may fail on later characters)
        //  - If !isFirstExact, pattern *may* match first characters in this set, or may fail.
        CharSet<Char> *firstSet;
        // Upper bound of FOLLOW characters of this pattern.
        CharSet<Char> *followSet;
        // Range of number of characters already consumed before this pattern
        CountDomain prevConsumes;
        // Range of number of characters consumed by this pattern
        CountDomain thisConsumes;
        // Range of number of character consumed after this pattern
        CountDomain followConsumes;

        inline Node(NodeTag tag)
            : tag(tag)
            , features(0)
            , firstSet(0)
            , isFirstExact(false)
            , followSet(0)
            , isThisIrrefutable(false)
            , isFollowIrrefutable(false)
            , isWord(false)
            , isThisWillNotProgress(false)
            , isThisWillNotRegress(false)
            , isPrevWillNotProgress(false)
            , isPrevWillNotRegress(false)
            , isFollowEOL(false)
            , isDeterministic(false)
            , isNotInLoop(false)
            , isAtLeastOnce(false)
            , isNotSpeculative(false)
            , isNotNegated(false)
            , hasInitialHardFailBOI(false)
        {
        }

        //
        // Parse-time helpers
        //

        virtual CharCount LiteralLength() const = 0;
        virtual void AppendLiteral(CharCount& litbufNext, CharCount litbufLen, __inout_ecount(litbufLen) Char* litbuf) const;
        virtual bool IsCharOrPositiveSet() const = 0;

        // Transfer pass 0:
        //  - synthesize the total number of characters required to store all literals, including case-invariant
        //    expansions where required
        //  - adjust match char nodes to account for case invariance if necessary
        virtual CharCount TransferPass0(Compiler& compiler, const Char* litbuf) = 0;
        // Transfer pass 1:
        //  - transfer literals from given litbuf into newLitbuf, advancing nextLit as we go
        //  - adjust set nodes to account for case invariance if necessary
        virtual void TransferPass1(Compiler& compiler, const Char* litbuf) = 0;

        //
        // Compile-time helpers
        //

        // True if firstSet of this node can be used as the followSet of a previous node, even though this node may
        // accept empty. True only for simple assertions.
        virtual bool IsRefiningAssertion(Compiler& compiler) = 0;

        // Annotation pass 0:
        //  - bottom-up: isWord
        //  - refine WordBoundary nodes where possible
        virtual void AnnotatePass0(Compiler& compiler) = 0;
        // Annotation pass 1:
        //  - top-down: isNotInLoop, isAtLeastOnce, isNotSpeculative, isNotNegated
        //  - bottom-up: features, thisConsumes, firstSet, isFirstExact, isThisIrrefutable, isThisWillNotProgress, isThisWillNotRegress
        virtual void AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated) = 0;
        // Annotation pass 2
        //  - left-to-right: prevConsumes, isPrevWillNotProgress, isPrevWillNotRegress.
        virtual void AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress) = 0;
        // Annotation pass 3
        //  - right-to-left: followConsumes, followSet, isFollowIrrefutable, isFollowEOL
        virtual void AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL) = 0;
        // Annotation pass 4
        //  - possibly simplify the node in-place
        //  - decide on the compilation scheme for each node, possibly recording it within node-specific fields
        //  - bottom-up: isDeterministic
        virtual void AnnotatePass4(Compiler& compiler) = 0;

        // Return true if pattern can be complied assuming some fixed-length prefix of a matching input string has already been consumed
        virtual bool SupportsPrefixSkipping(Compiler& compiler) const = 0;

        // Return the Match(Char|Literal|Set) at the start of pattern, or 0 if no such unique node
        virtual Node* HeadSyncronizingNode(Compiler& compiler) = 0;

        // Count how many literals are in pattern and return their minimum length. Returns 0
        // if pattern not in a form which can be used by a SyncToLiterals instruction.
        virtual CharCount MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const = 0;

        // Collect the literals counted by above and build scanners for them.
        virtual void CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const = 0;

        // Find a MatchLiteral or Alt of MatchLiterals which must appear at least once in input string for pattern
        // to match, and which has the shortest prevConsumes.
        virtual void BestSyncronizingNode(Compiler& compiler, Node*& bestNode) = 0;

        // Accumulate the range of groups definitions in pattern.
        // NOTE: minGroup must be > largest group, and maxGroup must be < 0 on topmost call
        virtual void AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup) = 0;

        // Emit code to consume this pattern. The first skipped characters of pattern have been consumed by context.
        virtual void Emit(Compiler& compiler, CharCount& skipped) = 0;

        // Emit code to scan forward for the first occurence of pattern, or hard fail if no such occurence.
        //  - if isHeadSyncronizingNode, also consume the occurence and leave input pointer at first char after it
        //  - otherwise, leave input pointer at the latest point of input which could match the overall pattern
        //    (ie rewind from start of occurence accerding to the prevConsumes range)
        //  - may actually do nothing if nothing worthwhile to scan to
        // Return number of characters consumed.
        virtual CharCount EmitScan(Compiler& compiler, bool isHeadSyncronizingNode) = 0;

        CharCount EmitScanFirstSet(Compiler& compiler);

        inline bool IsObviouslyDeterministic() { return (features & (HasAlt | HasLoop)) == 0; }
        inline bool ContainsAssertion() { return (features & (HasBOL | HasEOL | HasWordBoundary | HasAssertion)) != 0; }
        inline bool ContainsDefineGroup() { return (features & HasDefineGroup) != 0; }
        inline bool ContainsMatchGroup() { return (features & HasMatchGroup) != 0; }
        inline bool IsSimple() { return !ContainsAssertion() && !ContainsDefineGroup(); }
        inline bool IsSimpleOneChar() { return IsSimple() && !isThisIrrefutable && isFirstExact && thisConsumes.IsExact(1); }
        inline bool IsEmptyOnly() { return IsSimple() && isThisIrrefutable && thisConsumes.IsExact(0); }

        static bool IsBetterSyncronizingNode(Compiler& compiler, Node* curr, Node* proposed);

        //
        // Recognizers
        //

        // Is regex c
        bool IsSingleChar(Compiler& compiler, Char& outChar) const;

        // Is regex \b\w+\b?
        bool IsBoundedWord(Compiler& compiler) const;

        // Is regex ^\s*|\s*$
        bool IsLeadingTrailingSpaces(Compiler& compiler, CharCount& leftMinMatch, CharCount& rightMinMatch) const;

        // Is regex ^literal
        bool IsBOILiteral2(Compiler& compiler) const;

        // Can this regex be recognized by an Octoquad/Megamatch matcher? Ie is in grammar:
        //   octoquad ::= atom{8} '|' atom{8}
        //   atom ::= A | '['...charset drawn from A's...']'
        // and A is a set of exactly four ASCII characters
        virtual bool IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi) = 0;

        // Can this regex be recognized by a CharTrie structure? Ie is in grammar:
        //   triearm ::= atom*
        //   atom ::= c | '[' ... ']'
        // and factoring out sets does not exceed arm limit
        virtual bool IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const = 0;

        // Assuming above returned true, expand 'trie' node to include all literals recognized in this regex, and
        // continue expanding from each leaf using given 'cont' regex. Return false if any trie node has too many
        // children.
        //  - If isAcceptFirst is true, ignore any literals which are proper extensions of a literal already in
        //    the trie, but return false if any later literal is a prefix of an earlier literal.
        //    (If the follow of the alt we are turning into a trie is irrefutable, we can simply stop at the
        //    first shortest match).
        //  - Otherwise, return false if any literal is a proper prefix of any other literal, irrespective of order.
        virtual bool BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const = 0;

#if ENABLE_REGEX_CONFIG_OPTIONS
        virtual void Print(DebugWriter* w, const Char* litbuf) const = 0;
        void PrintAnnotations(DebugWriter* w) const;
#endif
    };

#if ENABLE_REGEX_CONFIG_OPTIONS
#define NODE_PRINT void Print(DebugWriter* w, const Char* litbuf) const override;
#else
#define NODE_PRINT
#endif

#define NODE_DECL CharCount LiteralLength() const override; \
                  bool IsCharOrPositiveSet() const override; \
                  CharCount TransferPass0(Compiler& compiler, const Char* litbuf) override; \
                  void TransferPass1(Compiler& compiler, const Char* litbuf) override; \
                  bool IsRefiningAssertion(Compiler& compiler) override; \
                  void AnnotatePass0(Compiler& compiler) override; \
                  void AnnotatePass1(Compiler& compiler, bool parentNotInLoop, bool parentAtLeastOnce, bool parentNotSpeculative, bool parentNotNegated) override; \
                  void AnnotatePass2(Compiler& compiler, CountDomain accumConsumes, bool accumPrevWillNotProgress, bool accumPrevWillNotRegress) override; \
                  void AnnotatePass3(Compiler& compiler, CountDomain accumConsumes, CharSet<Char>* accumFollow, bool accumFollowIrrefutable, bool accumFollowEOL) override; \
                  void AnnotatePass4(Compiler& compiler) override; \
                  bool SupportsPrefixSkipping(Compiler& compiler) const override; \
                  Node* HeadSyncronizingNode(Compiler& compiler) override; \
                  CharCount MinSyncronizingLiteralLength(Compiler& compiler, int& numLiterals) const override; \
                  void CollectSyncronizingLiterals(Compiler& compiler, ScannersMixin& scanners) const override; \
                  void BestSyncronizingNode(Compiler& compiler, Node*& bestNode) override; \
                  void Emit(Compiler& compiler, CharCount& skipped) override; \
                  CharCount EmitScan(Compiler& compiler, bool isHeadSyncronizingNode) override; \
                  void AccumDefineGroups(Js::ScriptContext* scriptContext, int& minGroup, int& maxGroup) override; \
                  bool IsOctoquad(Compiler& compiler, OctoquadIdentifier* oi) override; \
                  bool IsCharTrieArm(Compiler& compiler, uint& accNumAlts) const override; \
                  bool BuildCharTrie(Compiler& compiler, CharTrie* trie, Node* cont, bool isAcceptFirst) const override; \
                  NODE_PRINT

    struct SimpleNode : Node
    {
        inline SimpleNode(NodeTag tag)
            : Node(tag)
        {
        }

        NODE_DECL
    };

    struct WordBoundaryNode : Node
    {
        bool isNegation;
        bool mustIncludeEntering; // isNegation => false
        bool mustIncludeLeaving;  // isNegation => false

        inline WordBoundaryNode(bool isNegation)
            : Node(WordBoundary)
            , isNegation(isNegation)
            , mustIncludeEntering(false)
            , mustIncludeLeaving(false)
        {
        }

        NODE_DECL
    };

    struct MatchLiteralNode : Node
    {
        CharCount offset;  // into literal buffer (initially parser's, then program's)
        CharCount length;  // always > 1
        bool isEquivClass; // True if each consecutive triplet of characters of literal represents equivalence
                           // class of characters to match against. Actual literal length will be 3 times the length
                           // given above

        inline MatchLiteralNode(CharCount offset, CharCount length)
            : Node(MatchLiteral)
            , offset(offset)
            , length(length)
            , isEquivClass(false)
        {
        }

        NODE_DECL

        void AppendLiteral(CharCount& litbufNext, CharCount litbufLen, __inout_ecount(litbufLen) Char* litbuf) const override;
    };

    struct MatchCharNode : Node
    {
        Char cs[CaseInsensitive::EquivClassSize];
        bool isEquivClass; // true if above characters represent equivalence class of characters, otherwise only
                           // first character is significant

        inline MatchCharNode(Char c)
            : Node(MatchChar)
            , isEquivClass(false)
        {
            cs[0] = c;
#if DBG
            for (int i = 1; i < CaseInsensitive::EquivClassSize; i++)
                cs[i] = (Char)-1;
#endif
        }

        NODE_DECL

        void AppendLiteral(CharCount& litbufNext, CharCount litbufLen, __inout_ecount(litbufLen) Char* litbuf) const override;


        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        static void Emit(Compiler& compiler, __in_ecount(4) Char * cs, bool isEquivClass);

    private:
        CompileAssert(CaseInsensitive::EquivClassSize == 4);
        static CharCount FindUniqueEquivs(
            const Char equivs[CaseInsensitive::EquivClassSize],
            __out_ecount(4) Char uniqueEquivs[CaseInsensitive::EquivClassSize]);
    };

    struct MatchSetNode : Node
    {
        bool isNegation;
        bool needsEquivClass;
        CharSet<Char> set; // contents always owned by compile-time allocator

        // Set must be filled in
        inline MatchSetNode(bool isNegation, bool needsEquivClass = true)
            : Node(MatchSet)
            , isNegation(isNegation)
            , needsEquivClass(true)
        {
        }

        NODE_DECL
    };

    struct ConcatNode : Node
    {
        Node* head;       // never a concat node
        ConcatNode* tail; // null for end, overall always length > 1, never consecutive literals/chars

        inline ConcatNode(Node* head, ConcatNode* tail)
            : Node(Concat)
            , head(head)
            , tail(tail)
        {
        }

        NODE_DECL
    };

    struct AltNode sealed : Node
    {
        Node* head;    // never an alt node
        AltNode* tail; // null for end, overall always length > 1, never consecutive chars/sets


        enum CompilationScheme
        {
            Try,      // Push choicepoint, try item, backtrack to next item on failure
            None,     // No items (deterministic)
            Trie,     // Match using trie of literals (deterministic)
            Switch,   // Switch using 1 char lookahead (deterministic)
            Chain,    // Chain of JumpIfNot(Char|Set)  using 1 char lookahead (deterministic)
            Set       // (Opt?)Match(Char|Set) (deterministic)
        };

        // Following determined in pass 4
        RuntimeCharTrie* runtimeTrie;      // significant only if scheme == Trie
        CompilationScheme scheme;
        bool isOptional;                   // significant only if scheme == None|Switch|Chain|Set
        int switchSize;                    // significant only if scheme == Switch

        inline AltNode(Node* head, AltNode* tail)
            : Node(Alt)
            , head(head)
            , tail(tail)
            , scheme(Try)
            , runtimeTrie(0)
            , isOptional(false)
            , switchSize(0)
        {
        }

        NODE_DECL
    };

    struct DefineGroupNode : Node
    {
        Node* body;
        int groupId;

        enum CompilationScheme
        {
            Chomp,    // Chomp matching characters and capture all into a group at the end
            Fixed,    // Capture fixed-length group at end
            BeginEnd  // Wrap by begin/end instructions
        };

        // Following determined in pass 4
        CompilationScheme scheme;
        bool noNeedToSave;

        inline DefineGroupNode(int groupId, Node* body)
            : Node(DefineGroup)
            , groupId(groupId)
            , body(body)
            , scheme(BeginEnd)
            , noNeedToSave(false)
        {
        }

        NODE_DECL
    };

    struct MatchGroupNode : Node
    {
        int groupId;

        inline MatchGroupNode(int groupId)
            : Node(MatchGroup)
            , groupId(groupId)
        {
        }

        NODE_DECL
    };

    struct LoopNode : Node
    {
        Node* body;
        CountDomain repeats;
        bool isGreedy;

        enum CompilationScheme
        {
            BeginEnd,                   // Push choicepoints for each unravelling (greedy) or deferred unravelling (non-greedy) of body
            None,                       // Loop matches empty only (deterministic)
            Chomp,                      // Chomp matching characters (deterministic)
            ChompGroupLastChar,         // Chomp matching characters and bind the last char to group (deterministic)
            Guarded,                    // Use 1 char lookahead to control loop repeats (deterministic)
            Fixed,                      // Loop body is non-zero fixed width, deterministic, group-free
            FixedSet,                   // Loop body is MatchSet
            FixedGroupLastIteration,    // Loop body is non-zero fixed width, deterministic, loop body has one outer group
            GreedyNoBacktrack,          // Can keep track of backtracking info in constant space (deterministic)
            Set,                        // Treat r? as r|<empty>, emit as for AltNode::Set
            Chain,                      // Treat r? as r|<empty>, emit as for AltNode::Chain
            Try                         // Treat r? as r|<empty>, emit as for AltNode::Try
        };

        // Following determined in pass 4
        bool noNeedToSave;  // defined for ChompGroupLastChar/FixedGroupLastIteration only
        CompilationScheme scheme;

        inline LoopNode(CharCount lower, CharCountOrFlag upper, bool isGreedy, Node* body)
            : Node(Loop)
            , repeats(lower, upper)
            , isGreedy(isGreedy)
            , body(body)
            , scheme(BeginEnd)
        {
        }

        NODE_DECL
    };

    struct AssertionNode : Node
    {
        Node* body;
        bool isNegation;

        enum CompilationScheme
        {
            BeginEnd,     // Protect assertion with begin/end instructions
            Succ,         // Assertion will always succeeed, without binding groups
            Fail          // Assertion will always fail
        };

        // Following determined in pass 4
        CompilationScheme scheme;

        inline AssertionNode(bool isNegation, Node* body)
            : Node(Assertion)
            , isNegation(isNegation)
            , body(body)
            , scheme(BeginEnd)
        {
        }

        NODE_DECL
    };

    // ----------------------------------------------------------------------
    // Compiler
    // ----------------------------------------------------------------------

    class Compiler : private Chars<wchar_t>
    {
        friend Node;
        friend SimpleNode;
        friend WordBoundaryNode;
        friend MatchLiteralNode;
        friend MatchCharNode;
        friend ConcatNode;
        friend AltNode;
        friend DefineGroupNode;
        friend MatchGroupNode;
        friend LoopNode;
        friend MatchSetNode;
        friend AssertionNode;

    private:
        static const CharCount initInstBufSize = 128;

        Js::ScriptContext* scriptContext;
        // Arena for nodes and items needed only during compliation
        ArenaAllocator* ctAllocator;
        // Arena for literals, sets and items needed during runtime
        ArenaAllocator* rtAllocator;
        StandardChars<Char>* standardChars;
#if ENABLE_REGEX_CONFIG_OPTIONS
        DebugWriter* w;
        RegexStats* stats;
#endif
        Program* program;
        uint8* instBuf;     // in compile-time allocator, owned by compiler
        CharCount instLen;  // size of instBuf in bytes
        CharCount instNext; // offset to emit next instruction into
        int nextLoopId;

    private:

        uint8* Emit(size_t size);

        template <typename T>
        T* Emit();

        // The instruction buffer may move, so we need to remember label fixup's relative to the instruction base
        // rather than as machine addresses
        inline Label Compiler::GetFixup(Label* pLabel)
        {
            Assert((uint8*)pLabel >= instBuf && (uint8*)pLabel < instBuf + instNext);
            return (Label)((uint8*)pLabel - instBuf);
        }

        inline void Compiler::DoFixup(Label fixup, Label label)
        {
            Assert(fixup < instNext);
            Assert(label <= instNext);
            *(Label*)(instBuf + fixup) = label;
        }

        inline Label Compiler::CurrentLabel()
        {
            return instNext;
        }

        template <typename T>
        inline T* LabelToInstPointer(Inst::InstTag tag, Label label)
        {
            Assert(label + sizeof(T) <= instNext);
            Assert(((Inst*)(instBuf + label))->tag == tag);
            return (T*)(instBuf + label);
        }

        inline int Compiler::NextLoopId()
        {
            return nextLoopId++;
        }

        inline Js::ScriptContext *GetScriptContext() const
        {
            return scriptContext;
        }

        inline Program *GetProgram() const
        {
            return program;
        }

        void SetBOIInstructionsProgramTag()
        {
            Assert(this->program->tag == Program::InstructionsTag
                || this->program->tag == Program::BOIInstructionsTag);
            Assert(this->CurrentLabel() == 0);
            this->program->tag = Program::BOIInstructionsTag;
        }

        void SetBOIInstructionsProgramForStickyFlagTag()
        {
            Assert(this->program->tag == Program::InstructionsTag
                || this->program->tag == Program::BOIInstructionsForStickyFlagTag);
            Assert(this->CurrentLabel() == 0);
            AssertMsg((this->program->flags & StickyRegexFlag) != 0, "Shouldn't set BOIInstructionsForStickyFlagTag, if sticky is false.");

            this->program->tag = Program::BOIInstructionsForStickyFlagTag;
        }

        static void CaptureNoLiterals(Program* program);
        void CaptureLiterals(Node* root, const Char *litbuf);
        static void EmitAndCaptureSuccInst(Recycler* recycler, Program* program);
        void CaptureInsts();
        void FreeBody();

        Compiler
            ( Js::ScriptContext* scriptContext
            , ArenaAllocator* ctAllocator
            , ArenaAllocator* rtAllocator
            , StandardChars<Char>* standardChars
            , Program* program
#if ENABLE_REGEX_CONFIG_OPTIONS
            , DebugWriter* w
            , RegexStats* stats
#endif
            );

    public:
        static void CompileEmptyRegex
            ( Program* program
            , RegexPattern* pattern
#if ENABLE_REGEX_CONFIG_OPTIONS
            , DebugWriter* w
            , RegexStats* stats
#endif
            );

        static void Compile
            ( Js::ScriptContext* scriptContext
            , ArenaAllocator* ctAllocator
            , ArenaAllocator* rtAllocator
            , StandardChars<Char>* standardChars
            , Program* program
            , Node* root
            , const Char* litbuf
            , RegexPattern* pattern
#if ENABLE_REGEX_CONFIG_OPTIONS
            , DebugWriter* w
            , RegexStats* stats
#endif
            );
    };
}
