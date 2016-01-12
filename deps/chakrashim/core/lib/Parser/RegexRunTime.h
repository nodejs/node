//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// Regex programs and their execution context
//

#pragma once

namespace UnifiedRegex
{
    typedef CharCount Label;

    // FORWARD
    struct ScannerInfo;
    class ContStack;
    class AssertionStack;
    class OctoquadMatcher;

    enum class ChompMode : uint8
    {
        Star,   // min = 0, max = infinite
        Plus    // min = 1, max = infinite
    };

    // ----------------------------------------------------------------------
    // Programs
    // ----------------------------------------------------------------------

    struct Program : private Chars<wchar_t>
    {
        friend class Compiler;
        friend struct MatchLiteralNode;
        friend struct AltNode;
        friend class Matcher;
        friend struct LoopInfo;

        template <typename ScannerT>
        friend struct SyncToLiteralAndConsumeInstT;
        template <typename ScannerT>
        friend struct SyncToLiteralAndContinueInstT;
        template <typename ScannerT>
        friend struct SyncToLiteralAndBackupInstT;
        template <typename ScannerT>
        friend struct ScannerMixinT;
        template <uint lastPatCharEquivClassSize>
        friend struct EquivScannerMixinT;

#define M(TagName) friend struct TagName##Inst;
#define MTemplate(TagName, TemplateDeclaration, GenericClassName, ...) TemplateDeclaration friend struct GenericClassName;
#include "RegexOpcodes.h"
#undef M
#undef MTemplate

    public:
        // Copy of original text of regex (without delimiting '/'s or trailing flags), null terminated.
        // In run-time allocator, owned by program
        Char* source;
        CharCount sourceLen; // length in wchar_t's, NOT including terminating null
        // Number of capturing groups (including implicit overall group at index 0)
        int numGroups;
        int numLoops;
        RegexFlags flags;

    private:
        enum ProgramTag : uint8
        {
            InstructionsTag,
            BOIInstructionsTag,
            BOIInstructionsForStickyFlagTag,
            SingleCharTag,
            BoundedWordTag,
            LeadingTrailingSpacesTag,
            OctoquadTag,
            BOILiteral2Tag
        };

        ProgramTag tag;

        struct Instructions
        {
            // Instruction array, in run-time allocator, owned by program, never null
            uint8* insts;
            CharCount instsLen; // in bytes
            // Literals
            // In run-time allocator, owned by program, may be 0
            CharCount litbufLen; // length of litbuf in wchar_t's, no terminating null
            Char* litbuf;

            // These scanner infos are used by ScannersMixin, which is used by only SyncToLiteralsAndBackupInst. There will only
            // ever be only one of those instructions per program. Since scanners are large (> 1 KB), for that instruction they
            // are allocated on the recycler with pointers stored here to reference them.
            ScannerInfo **scannersForSyncToLiterals;
        };

        struct SingleChar
        {
            Char c;
            uint8 padding[sizeof(Instructions) - sizeof(Char)];
        };

        struct Octoquad
        {
            OctoquadMatcher* matcher;
            uint8 padding[sizeof(Instructions) - sizeof(void*)];
        };

        struct BOILiteral2
        {
            DWORD literal;
            uint8 padding[sizeof(Instructions) - sizeof(DWORD)];
        };

        struct LeadingTrailingSpaces
        {
            CharCount beginMinMatch;
            CharCount endMinMatch;
            uint8 padding[sizeof(Instructions) - (sizeof(CharCount) * 2)];
        };

        struct Other
        {
            uint8 padding[sizeof(Instructions)];
        };

        union
        {
            Instructions insts;
            SingleChar singleChar;
            Octoquad octoquad;
            BOILiteral2 boiLiteral2;
            LeadingTrailingSpaces leadingTrailingSpaces;
            Other other;
        } rep;

    public:
        Program(RegexFlags flags);
        static Program *New(Recycler *recycler, RegexFlags flags);

        static size_t GetOffsetOfTag() { return offsetof(Program, tag); }
        static size_t GetOffsetOfRep() { return offsetof(Program, rep); }
        static size_t GetOffsetOfBOILiteral2Literal() { return offsetof(BOILiteral2, literal); }
        static ProgramTag GetBOILiteral2Tag() { return ProgramTag::BOILiteral2Tag; }

        ScannerInfo **CreateScannerArrayForSyncToLiterals(Recycler *const recycler);
        ScannerInfo *AddScannerForSyncToLiterals(
            Recycler *const recycler,
            const int scannerIndex,
            const CharCount offset,
            const CharCount length,
            const bool isEquivClass);

        void FreeBody(ArenaAllocator* rtAllocator);

        inline CaseInsensitive::MappingSource GetCaseMappingSource() const
        {
            return (flags & UnicodeRegexFlag) != 0
                ? CaseInsensitive::MappingSource::CaseFolding
                : CaseInsensitive::MappingSource::UnicodeData;
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w);
#endif
    };

    class Matcher;

    // ----------------------------------------------------------------------
    // CountDomain
    // ----------------------------------------------------------------------

    struct CountDomain : private Chars<wchar_t>
    {
        CharCount lower;
        CharCountOrFlag upper; // CharCountFlag => unbounded

        inline CountDomain() : lower(0), upper(CharCountFlag) {}

        inline CountDomain(CharCount exact) : lower(exact), upper(exact) {}

        inline CountDomain(CharCount lower, CharCountOrFlag upper) : lower(lower), upper(upper) {}

        inline void Exact(CharCount n)
        {
            lower = upper = n;
        }

        inline void Unknown()
        {
            lower = 0;
            upper = CharCountFlag;
        }

        inline void Lub(const CountDomain& other)
        {
            lower = min(lower, other.lower);
            upper = upper == CharCountFlag || other.upper == CharCountFlag ? CharCountFlag : max(upper, other.upper);
        }

        inline void Add(const CountDomain& other)
        {
            lower = lower + other.lower;
            upper = upper == CharCountFlag || other.upper == CharCountFlag ? CharCountFlag : upper + other.upper;
        }

        inline void Sub(const CountDomain& other)
        {
            lower = other.upper == CharCountFlag || other.upper > lower ? 0 : lower - other.upper;
            upper = upper == CharCountFlag ? CharCountFlag : (other.lower > upper ? 0 : upper - other.lower);
        }

        inline void Mult(const CountDomain& other)
        {
            if (lower != 0)
            {
                CharCount maxOther = MaxCharCount / lower;
                if (other.lower > maxOther)
                    // Clip to maximum
                    lower = MaxCharCount;
                else
                    lower *= other.lower;
            }
            if (upper != 0 && upper != CharCountFlag)
            {
                if (other.upper == CharCountFlag)
                    upper = CharCountFlag;
                else
                {
                    CharCount maxOther = MaxCharCount / upper;
                    if (other.upper > maxOther)
                        // Clip to 'unbounded'
                        upper = CharCountFlag;
                    else
                        upper *= other.upper;
                }
            }
        }

        inline bool CouldMatchEmpty() const
        {
            return lower == 0;
        }

        inline bool IsUnbounded() const
        {
            return upper == CharCountFlag;
        }

        inline bool IsFixed() const
        {
            return lower == upper;
        }

        inline bool IsExact(CharCount n) const
        {
            return lower == n && upper == n;
        }

        inline bool IsGreaterThan(const CountDomain& other) const
        {
            return other.upper != CharCountFlag && lower > other.upper;
        }

        inline bool IsLessThan(const CountDomain& other) const
        {
            return upper != CharCountFlag && upper < other.lower;
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    // ----------------------------------------------------------------------
    // Mix-in types
    // ----------------------------------------------------------------------

    // Contains information about how much to back up after syncing to a literal (for the SyncTo... instructions)
    struct BackupMixin
    {
        const CountDomain backup; // range of characters to backup, if upper is CharCountFlag then backup to existing matchStart

        inline BackupMixin(const CountDomain& backup) : backup(backup) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct CharMixin
    {
        wchar_t c;

        inline CharMixin(wchar_t c) : c(c) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct Char2Mixin
    {
        wchar_t cs[2];

        inline Char2Mixin(wchar_t c0, wchar_t c1) { cs[0] = c0; cs[1] = c1; }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct Char3Mixin
    {
        wchar_t cs[3];

        inline Char3Mixin(wchar_t c0, wchar_t c1, wchar_t c2) { cs[0] = c0; cs[1] = c1; cs[2] = c2; }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct Char4Mixin
    {
        wchar_t cs[4];

        inline Char4Mixin(wchar_t c0, wchar_t c1, wchar_t c2, wchar_t c3) { cs[0] = c0; cs[1] = c1; cs[2] = c2; cs[3] = c3; }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct LiteralMixin
    {
        CharCount offset;  // into program's literal buffer
        CharCount length;  // in wchar_t's

        inline LiteralMixin(CharCount offset, CharCount length) : offset(offset), length(length) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf, bool isEquivClass) const;
#endif
    };

    template<bool IsNegation>
    struct SetMixin
    {
        RuntimeCharSet<wchar_t> set; // contents always lives in run-time allocator

        // set must always be cloned from source

        void FreeBody(ArenaAllocator* rtAllocator);
#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct Char2LiteralScannerMixin : Char2Mixin
    {
        // scanner must be setup
        Char2LiteralScannerMixin(CharCount offset, CharCount length) : Char2Mixin(0, 0) { Assert(length == 2); }
        void Setup(wchar_t c0, wchar_t c1) { cs[0] = c0; cs[1] = c1; }
        CharCount GetLiteralLength() const { return 2; }
        bool Match(Matcher& matcher, const wchar_t* const input, const CharCount inputLength, CharCount& inputOffset) const;

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    template <typename ScannerT>
    struct ScannerMixinT : LiteralMixin
    {
        ScannerT scanner;

        // scanner must be setup
        ScannerMixinT(CharCount offset, CharCount length) : LiteralMixin(offset, length) {}
        CharCount GetLiteralLength() const { return length; }
        bool Match(Matcher& matcher, const wchar_t* const input, const CharCount inputLength, CharCount& inputOffset) const;

        void FreeBody(ArenaAllocator* rtAllocator);
#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf, bool isEquivClass = false) const;
#endif
    };

    typedef ScannerMixinT<TextbookBoyerMoore<wchar_t>> ScannerMixin;
    typedef ScannerMixinT<TextbookBoyerMooreWithLinearMap<wchar_t>> ScannerMixin_WithLinearCharMap;

    template <uint lastPatCharEquivCLassSize>
    struct EquivScannerMixinT : ScannerMixin
    {
        // scanner must be setup
        EquivScannerMixinT(CharCount offset, CharCount length) : ScannerMixin(offset, length) {}

        bool Match(Matcher& matcher, const wchar_t* const input, const CharCount inputLength, CharCount& inputOffset) const;

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    typedef EquivScannerMixinT<CaseInsensitive::EquivClassSize> EquivScannerMixin;
    typedef EquivScannerMixinT<1> EquivTrivialLastPatCharScannerMixin;

    struct ScannerInfo : ScannerMixin
    {
        bool isEquivClass;

        // scanner must be setup
        inline ScannerInfo(CharCount offset, CharCount length, bool isEquivClass) : ScannerMixin(offset, length), isEquivClass(isEquivClass) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct ScannersMixin
    {
        static const int MaxNumSyncLiterals = 4;

        int numLiterals;
        ScannerInfo** infos;

        // scanner mixins must be added
        inline ScannersMixin(Recycler *const recycler, Program *const program)
            : numLiterals(0), infos(program->CreateScannerArrayForSyncToLiterals(recycler))
        {
        }

        // Only used at compile time
        ScannerInfo* Add(Recycler *recycler, Program *program, CharCount offset, CharCount length, bool isEquivClass);
        void FreeBody(ArenaAllocator* rtAllocator);
#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct HardFailMixin
    {
        bool canHardFail;

        inline HardFailMixin(bool canHardFail) : canHardFail(canHardFail) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct GroupMixin
    {
        const int groupId;

        inline GroupMixin(int groupId) : groupId(groupId) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct ChompBoundedMixin
    {
        const CountDomain repeats; // if upper is CharCountFlag, consume as many characters as possible

        inline ChompBoundedMixin(const CountDomain& repeats) : repeats(repeats) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct JumpMixin
    {
        Label targetLabel;

        // targetLabel must always be fixed up
        inline JumpMixin()
        {
#if DBG
            targetLabel = (Label)-1;
#endif
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct BodyGroupsMixin
    {
        int minBodyGroupId;
        int maxBodyGroupId;

        inline BodyGroupsMixin(int minBodyGroupId, int maxBodyGroupId) : minBodyGroupId(minBodyGroupId), maxBodyGroupId(maxBodyGroupId) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct BeginLoopMixin
    {
        int loopId;
        const CountDomain repeats;
        bool hasOuterLoops;
        bool hasInnerNondet;
        Label exitLabel;

        // exitLabel must always be fixed up
        inline BeginLoopMixin(int loopId, const CountDomain& repeats, bool hasOuterLoops, bool hasInnerNondet)
            : loopId(loopId), repeats(repeats), hasOuterLoops(hasOuterLoops), hasInnerNondet(hasInnerNondet)
        {
#if DBG
            exitLabel = (Label)-1;
#endif
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct RepeatLoopMixin
    {
        Label beginLabel;  // label of the BeginLoopX instruction

        inline RepeatLoopMixin(Label beginLabel) : beginLabel(beginLabel) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct TryMixin
    {
        Label failLabel;

        // failLabel must always be fixed up
        inline TryMixin()
        {
#if DBG
            failLabel = (Label)-1;
#endif
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct FixedLengthMixin
    {
        CharCount length;

        inline FixedLengthMixin(CharCount length) : length(length) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct NoNeedToSaveMixin
    {
        bool noNeedToSave;

        inline NoNeedToSaveMixin(bool noNeedToSave) : noNeedToSave(noNeedToSave) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    struct SwitchCase
    {
        wchar_t c;
        Label targetLabel;

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    template <int n>
    struct SwitchMixin
    {
        static const int MaxCases = n;

        int numCases;
        // numCases cases, in increasing character order
        SwitchCase cases[MaxCases];

        // Cases must always be added
        inline SwitchMixin() : numCases(0)
        {
#if DBG
            for (int i = 0; i < MaxCases; i++)
            {
                cases[i].c = (wchar_t)-1;
                cases[i].targetLabel = (Label)-1;
            }
#endif
        }

        // Only used at compile time
        void AddCase(wchar_t c, Label targetLabel);

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const wchar_t* litbuf) const;
#endif
    };

    // ----------------------------------------------------------------------
    // Instructions
    // ----------------------------------------------------------------------

    struct Inst : protected Chars<wchar_t>
    {
        enum InstTag : uint32
        {
#define M(TagName) TagName,
#define MTemplate(TagName, ...) M(TagName)
#include "RegexOpcodes.h"
#undef M
#undef MTemplate
        };

        InstTag tag;

        inline Inst(InstTag tag) : tag(tag) {}
        void FreeBody(ArenaAllocator* rtAllocator) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        virtual int Print(DebugWriter*w, Label label, const Char* litbuf) const = 0;
#endif
    };

#define INST_BODY_FREE(T) \
    void FreeBody(ArenaAllocator* rtAllocator) \
    { \
        T::FreeBody(rtAllocator); \
        Inst::FreeBody(rtAllocator); \
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
#define INST_BODY_PRINT virtual int Print(DebugWriter*w, Label label, const Char* litbuf) const override;
#else
#define INST_BODY_PRINT
#endif

#define REGEX_INST_EXEC_PARAMETERS Matcher& matcher, const Char* const input, const CharCount inputLength, CharCount &matchStart, CharCount& inputOffset, CharCount &nextSyncInputOffset, const uint8*& instPointer, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks
#define INST_BODY bool Exec(REGEX_INST_EXEC_PARAMETERS) const; \
                  INST_BODY_PRINT

    //
    // Control flow
    //

    struct FailInst : Inst
    {
        inline FailInst() : Inst(Fail) {}

        INST_BODY
    };

    struct SuccInst : Inst
    {
        inline SuccInst() : Inst(Succ) {}

        INST_BODY
    };

    struct JumpInst : Inst, JumpMixin
    {
        // targetLabel must always be fixed up
        inline JumpInst() : Inst(Jump), JumpMixin() {}

        INST_BODY
    };

    struct JumpIfNotCharInst : Inst, CharMixin, JumpMixin
    {
        // targetLabel must always be fixed up
        inline JumpIfNotCharInst(Char c) : Inst(JumpIfNotChar), CharMixin(c), JumpMixin() {}

        INST_BODY
    };

    struct MatchCharOrJumpInst : Inst, CharMixin, JumpMixin
    {
        // targetLabel must always be fixed up
        inline MatchCharOrJumpInst(Char c) : Inst(MatchCharOrJump), CharMixin(c), JumpMixin() {}

        INST_BODY
    };

    struct JumpIfNotSetInst : Inst, SetMixin<false>, JumpMixin
    {
        // set must always be cloned from source
        // targetLabel must always be fixed up
        inline JumpIfNotSetInst() : Inst(JumpIfNotSet), JumpMixin() {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    struct MatchSetOrJumpInst : Inst, SetMixin<false>, JumpMixin
    {
        // set must always be cloned from source
        // targetLabel must always be fixed up
        inline MatchSetOrJumpInst() : Inst(MatchSetOrJump), JumpMixin() {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    struct Switch10Inst : Inst, SwitchMixin<10>
    {
        // Cases must always be added
        inline Switch10Inst() : Inst(Switch10), SwitchMixin() {}

        INST_BODY
    };

    struct Switch20Inst : Inst, SwitchMixin<20>
    {
        // Cases must always be added
        inline Switch20Inst() : Inst(Switch20), SwitchMixin() {}

        INST_BODY
    };

    struct SwitchAndConsume10Inst : Inst, SwitchMixin<10>
    {
        // Cases must always be added
        inline SwitchAndConsume10Inst() : Inst(SwitchAndConsume10), SwitchMixin() {}

        INST_BODY
    };

    struct SwitchAndConsume20Inst : Inst, SwitchMixin<20>
    {
        // Cases must always be added
        inline SwitchAndConsume20Inst() : Inst(SwitchAndConsume20), SwitchMixin() {}

        INST_BODY
    };

    //
    // Built-in assertions
    //

    struct BOITestInst : Inst, HardFailMixin
    {
        inline BOITestInst(bool canHardFail) : Inst(BOITest), HardFailMixin(canHardFail) {}

        INST_BODY
    };

    struct EOITestInst : Inst, HardFailMixin
    {
        inline EOITestInst(bool canHardFail) : Inst(EOITest), HardFailMixin(canHardFail) {}

        INST_BODY
    };

    struct BOLTestInst : Inst
    {
        inline BOLTestInst() : Inst(BOLTest) {}

        INST_BODY
    };

    struct EOLTestInst : Inst
    {
        inline EOLTestInst() : Inst(EOLTest) {}

        INST_BODY
    };

    struct WordBoundaryTestInst : Inst
    {
        bool isNegation;

        inline WordBoundaryTestInst(bool isNegation) : Inst(WordBoundaryTest), isNegation(isNegation) {}

        INST_BODY
    };

    //
    // Matching
    //

    struct MatchCharInst : Inst, CharMixin
    {
        inline MatchCharInst(Char c) : Inst(MatchChar), CharMixin(c) {}

        INST_BODY
    };

    struct MatchChar2Inst : Inst, Char2Mixin
    {
        inline MatchChar2Inst(Char c0, Char c1) : Inst(MatchChar2), Char2Mixin(c0, c1) {}

        INST_BODY
    };


    struct MatchChar3Inst : Inst, Char3Mixin
    {
        inline MatchChar3Inst(Char c0, Char c1, Char c2) : Inst(MatchChar3), Char3Mixin(c0, c1, c2) {}

        INST_BODY
    };

    struct MatchChar4Inst : Inst, Char4Mixin
    {
        inline MatchChar4Inst(Char c0, Char c1, Char c2, Char c3) : Inst(MatchChar4), Char4Mixin(c0, c1, c2, c3) {}

        INST_BODY
    };

    template<bool IsNegation>
    struct MatchSetInst : Inst, SetMixin<IsNegation>
    {
        // set must always be cloned from source
        inline MatchSetInst() : Inst(IsNegation ? MatchNegatedSet : MatchSet) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    struct MatchLiteralInst : Inst, LiteralMixin
    {
        inline MatchLiteralInst(CharCount offset, CharCount length) : Inst(MatchLiteral), LiteralMixin(offset, length) {}

        INST_BODY
    };

    struct MatchLiteralEquivInst : Inst, LiteralMixin
    {
        inline MatchLiteralEquivInst(CharCount offset, CharCount length) : Inst(MatchLiteralEquiv), LiteralMixin(offset, length) {}

        INST_BODY
    };

    struct MatchTrieInst : Inst
    {
        RuntimeCharTrie trie;

        // Trie must always be cloned
        inline MatchTrieInst() : Inst(MatchTrie) {}
        void FreeBody(ArenaAllocator* rtAllocator);

        INST_BODY
    };

    struct OptMatchCharInst : Inst, CharMixin
    {
        inline OptMatchCharInst(Char c) : Inst(OptMatchChar), CharMixin(c) {}

        INST_BODY
    };

    struct OptMatchSetInst : Inst, SetMixin<false>
    {
        // set must always be cloned from source
        inline OptMatchSetInst() : Inst(OptMatchSet) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    //
    // Synchronization:
    //   SyncTo(Char|Char2Set|Set|Char2Literal|Literal|LiteralEquiv|Literals)And(Consume|Continue|Backup)
    //

    struct SyncToCharAndContinueInst : Inst, CharMixin
    {
        inline SyncToCharAndContinueInst(Char c) : Inst(SyncToCharAndContinue), CharMixin(c) {}

        INST_BODY
    };

    struct SyncToChar2SetAndContinueInst : Inst, Char2Mixin
    {
        inline SyncToChar2SetAndContinueInst(Char c0, Char c1) : Inst(SyncToChar2SetAndContinue), Char2Mixin(c0, c1) {}

        INST_BODY
    };

    template<bool IsNegation>
    struct SyncToSetAndContinueInst : Inst, SetMixin<IsNegation>
    {
        // set must always be cloned from source
        inline SyncToSetAndContinueInst() : Inst(IsNegation ? SyncToNegatedSetAndContinue : SyncToSetAndContinue) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    template <typename ScannerT>
    struct SyncToLiteralAndContinueInstT : Inst, ScannerT
    {
        SyncToLiteralAndContinueInstT(InstTag tag, CharCount offset, CharCount length) : Inst(tag), ScannerT(offset, length) {}

        INST_BODY
    };

    // Specialized version of the SyncToLiteralAndContinueInst for a length 2 literal
    struct SyncToChar2LiteralAndContinueInst : SyncToLiteralAndContinueInstT<Char2LiteralScannerMixin>
    {
        SyncToChar2LiteralAndContinueInst(Char c0, Char c1) :
            SyncToLiteralAndContinueInstT(SyncToChar2LiteralAndContinue, 0, 2) { Char2LiteralScannerMixin::Setup(c0, c1); }
    };

    struct SyncToLiteralAndContinueInst : SyncToLiteralAndContinueInstT<ScannerMixin>
    {
        // scanner must be setup
        SyncToLiteralAndContinueInst(CharCount offset, CharCount length) :
            SyncToLiteralAndContinueInstT(SyncToLiteralAndContinue, offset, length) {}

        INST_BODY_FREE(ScannerMixin)
    };

    struct SyncToLinearLiteralAndContinueInst : SyncToLiteralAndContinueInstT<ScannerMixin_WithLinearCharMap>
    {
        // scanner must be setup
        SyncToLinearLiteralAndContinueInst(CharCount offset, CharCount length) :
            SyncToLiteralAndContinueInstT(SyncToLinearLiteralAndContinue, offset, length) {}

        INST_BODY_FREE(ScannerMixin_WithLinearCharMap)
    };

    struct SyncToLiteralEquivAndContinueInst : SyncToLiteralAndContinueInstT<EquivScannerMixin>
    {
        // scanner must be setup
        SyncToLiteralEquivAndContinueInst(CharCount offset, CharCount length) :
            SyncToLiteralAndContinueInstT(SyncToLiteralEquivAndContinue, offset, length) {}

        INST_BODY_FREE(EquivScannerMixin)
    };

    struct SyncToLiteralEquivTrivialLastPatCharAndContinueInst : SyncToLiteralAndContinueInstT<EquivTrivialLastPatCharScannerMixin>
    {
        // scanner must be setup
        SyncToLiteralEquivTrivialLastPatCharAndContinueInst(CharCount offset, CharCount length) :
            SyncToLiteralAndContinueInstT(SyncToLiteralEquivTrivialLastPatCharAndContinue, offset, length) {}

        INST_BODY_FREE(EquivTrivialLastPatCharScannerMixin)
    };

    struct SyncToCharAndConsumeInst : Inst, CharMixin
    {
        inline SyncToCharAndConsumeInst(Char c) : Inst(SyncToCharAndConsume), CharMixin(c) {}

        INST_BODY
    };

    struct SyncToChar2SetAndConsumeInst : Inst, Char2Mixin
    {
        inline SyncToChar2SetAndConsumeInst(Char c0, Char c1) : Inst(SyncToChar2SetAndConsume), Char2Mixin(c0, c1) {}

        INST_BODY
    };

    template<bool IsNegation>
    struct SyncToSetAndConsumeInst : Inst, SetMixin<IsNegation>
    {
        // set must always be cloned from source
        inline SyncToSetAndConsumeInst() : Inst(IsNegation ? SyncToNegatedSetAndConsume : SyncToSetAndConsume) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    template <typename ScannerT>
    struct SyncToLiteralAndConsumeInstT : Inst, ScannerT
    {
        SyncToLiteralAndConsumeInstT(InstTag tag, CharCount offset, CharCount length) : Inst(tag), ScannerT(offset, length) {}

        INST_BODY
    };

    // Specialized version of the SyncToLiteralAndConsumeInst for a length 2 literal
    struct SyncToChar2LiteralAndConsumeInst : SyncToLiteralAndConsumeInstT<Char2LiteralScannerMixin>
    {
        SyncToChar2LiteralAndConsumeInst(Char c0, Char c1) :
            SyncToLiteralAndConsumeInstT(SyncToChar2LiteralAndConsume, 0, 2) { Char2LiteralScannerMixin::Setup(c0, c1); }
    };

    struct SyncToLiteralAndConsumeInst : SyncToLiteralAndConsumeInstT<ScannerMixin>
    {
        // scanner must be setup
        SyncToLiteralAndConsumeInst(CharCount offset, CharCount length) :
            SyncToLiteralAndConsumeInstT(SyncToLiteralAndConsume, offset, length) {}

        INST_BODY_FREE(ScannerMixin)
    };

    struct SyncToLinearLiteralAndConsumeInst : SyncToLiteralAndConsumeInstT<ScannerMixin_WithLinearCharMap>
    {
        // scanner must be setup
        SyncToLinearLiteralAndConsumeInst(CharCount offset, CharCount length) :
            SyncToLiteralAndConsumeInstT(SyncToLinearLiteralAndConsume, offset, length) {}

        INST_BODY_FREE(ScannerMixin_WithLinearCharMap)
    };

    struct SyncToLiteralEquivAndConsumeInst : SyncToLiteralAndConsumeInstT<EquivScannerMixin>
    {
        // scanner must be setup
        SyncToLiteralEquivAndConsumeInst(CharCount offset, CharCount length) :
            SyncToLiteralAndConsumeInstT(SyncToLiteralEquivAndConsume,offset, length) {}

        INST_BODY_FREE(EquivScannerMixin)
    };

    struct SyncToLiteralEquivTrivialLastPatCharAndConsumeInst : SyncToLiteralAndConsumeInstT<EquivTrivialLastPatCharScannerMixin>
    {
        // scanner must be setup
        SyncToLiteralEquivTrivialLastPatCharAndConsumeInst(CharCount offset, CharCount length) :
            SyncToLiteralAndConsumeInstT(SyncToLiteralEquivTrivialLastPatCharAndConsume, offset, length) {}

        INST_BODY_FREE(EquivTrivialLastPatCharScannerMixin)
    };

    struct SyncToCharAndBackupInst : Inst, CharMixin, BackupMixin
    {
        inline SyncToCharAndBackupInst(Char c, const CountDomain& backup) : Inst(SyncToCharAndBackup), CharMixin(c), BackupMixin(backup) {}

        INST_BODY
    };

    template<bool IsNegation>
    struct SyncToSetAndBackupInst : Inst, SetMixin<IsNegation>, BackupMixin
    {
        // set must always be cloned from source
        inline SyncToSetAndBackupInst(const CountDomain& backup) : Inst(IsNegation ? SyncToNegatedSetAndBackup : SyncToSetAndBackup), BackupMixin(backup) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    template <typename ScannerT>
    struct SyncToLiteralAndBackupInstT : Inst, ScannerT, BackupMixin
    {
        SyncToLiteralAndBackupInstT(InstTag tag, CharCount offset, CharCount length, const CountDomain& backup) : Inst(tag), ScannerT(offset, length), BackupMixin(backup) {}

        INST_BODY
    };

    // Specialized version of the SyncToLiteralAndConsumeInst for a length 2 literal
    struct SyncToChar2LiteralAndBackupInst : SyncToLiteralAndBackupInstT<Char2LiteralScannerMixin>
    {
        SyncToChar2LiteralAndBackupInst(Char c0, Char c1, const CountDomain& backup) :
            SyncToLiteralAndBackupInstT(SyncToChar2LiteralAndBackup, 0, 2, backup) { Char2LiteralScannerMixin::Setup(c0, c1); }
    };

    struct SyncToLiteralAndBackupInst : SyncToLiteralAndBackupInstT<ScannerMixin>
    {
        // scanner must be setup
        SyncToLiteralAndBackupInst(CharCount offset, CharCount length, const CountDomain& backup) :
            SyncToLiteralAndBackupInstT(SyncToLiteralAndBackup, offset, length, backup) {}

        INST_BODY_FREE(ScannerMixin)
    };

    struct SyncToLinearLiteralAndBackupInst : SyncToLiteralAndBackupInstT<ScannerMixin_WithLinearCharMap>
    {
        // scanner must be setup
        SyncToLinearLiteralAndBackupInst(CharCount offset, CharCount length, const CountDomain& backup) :
            SyncToLiteralAndBackupInstT(SyncToLinearLiteralAndBackup, offset, length, backup) {}

        INST_BODY_FREE(ScannerMixin_WithLinearCharMap)
    };

    struct SyncToLiteralEquivAndBackupInst : SyncToLiteralAndBackupInstT<EquivScannerMixin>
    {
        // scanner must be setup
         SyncToLiteralEquivAndBackupInst(CharCount offset, CharCount length, const CountDomain& backup) :
             SyncToLiteralAndBackupInstT(SyncToLiteralEquivAndBackup, offset, length, backup) {}

        INST_BODY_FREE(EquivScannerMixin)
    };

    struct SyncToLiteralEquivTrivialLastPatCharAndBackupInst : SyncToLiteralAndBackupInstT<EquivTrivialLastPatCharScannerMixin>
    {
        // scanner must be setup
         SyncToLiteralEquivTrivialLastPatCharAndBackupInst(CharCount offset, CharCount length, const CountDomain& backup) :
             SyncToLiteralAndBackupInstT(SyncToLiteralEquivTrivialLastPatCharAndBackup, offset, length, backup) {}

        INST_BODY_FREE(EquivTrivialLastPatCharScannerMixin)
    };

    struct SyncToLiteralsAndBackupInst : Inst, ScannersMixin, BackupMixin
    {
        // scanner mixins must be setup
        inline SyncToLiteralsAndBackupInst(Recycler *recycler, Program *program, const CountDomain& backup)
            : Inst(SyncToLiteralsAndBackup), ScannersMixin(recycler, program), BackupMixin(backup)
        {
        }

        INST_BODY
        INST_BODY_FREE(ScannersMixin)
    };

    //
    // Groups
    //

    struct MatchGroupInst : Inst, GroupMixin
    {
        inline MatchGroupInst(int groupId) : Inst(MatchGroup), GroupMixin(groupId) {}

        INST_BODY
    };

    struct BeginDefineGroupInst : Inst, GroupMixin
    {
        inline BeginDefineGroupInst(int groupId) : Inst(BeginDefineGroup), GroupMixin(groupId) {}

        INST_BODY
    };

    struct EndDefineGroupInst : Inst, GroupMixin, NoNeedToSaveMixin
    {
        inline EndDefineGroupInst(int groupId, bool noNeedToSave)
            : Inst(EndDefineGroup), GroupMixin(groupId), NoNeedToSaveMixin(noNeedToSave)
        {
        }

        INST_BODY
    };

    struct DefineGroupFixedInst : Inst, GroupMixin, FixedLengthMixin, NoNeedToSaveMixin
    {
        inline DefineGroupFixedInst(int groupId, CharCount length, bool noNeedToSave) : Inst(DefineGroupFixed), GroupMixin(groupId), FixedLengthMixin(length), NoNeedToSaveMixin(noNeedToSave) {}

        INST_BODY
    };

    //
    // Loops
    //

    struct BeginLoopInst : Inst, BeginLoopMixin, BodyGroupsMixin
    {
        bool isGreedy;

        // exitLabel must always be fixed up
        inline BeginLoopInst(int loopId, const CountDomain& repeats, bool hasOuterLoops, bool hasInnerNondet, int minBodyGroupId, int maxBodyGroupId, bool isGreedy)
            : Inst(BeginLoop), BeginLoopMixin(loopId, repeats, hasOuterLoops, hasInnerNondet), BodyGroupsMixin(minBodyGroupId, maxBodyGroupId), isGreedy(isGreedy)
        {}

        INST_BODY
    };

    struct RepeatLoopInst : Inst, RepeatLoopMixin
    {
        inline RepeatLoopInst(Label beginLabel) : Inst(RepeatLoop), RepeatLoopMixin(beginLabel) {}

        INST_BODY
    };

    struct BeginLoopIfCharInst : Inst, CharMixin, BeginLoopMixin, BodyGroupsMixin
    {
        // exitLabel must always be fixed up
        inline BeginLoopIfCharInst(Char c, int loopId, const CountDomain& repeats, bool hasOuterLoops, bool hasInnerNondet, int minBodyGroupId, int maxBodyGroupId)
            : Inst(BeginLoopIfChar), CharMixin(c), BeginLoopMixin(loopId, repeats, hasOuterLoops, hasInnerNondet), BodyGroupsMixin(minBodyGroupId, maxBodyGroupId) {}

        INST_BODY
    };

    struct BeginLoopIfSetInst : Inst, SetMixin<false>, BeginLoopMixin, BodyGroupsMixin
    {
        // set must always be cloned from source
        // exitLabel must always be fixed up
        inline BeginLoopIfSetInst(int loopId, const CountDomain& repeats, bool hasOuterLoops, bool hasInnerNondet, int minBodyGroupId, int maxBodyGroupId)
            : Inst(BeginLoopIfSet), BeginLoopMixin(loopId, repeats, hasOuterLoops, hasInnerNondet), BodyGroupsMixin(minBodyGroupId, maxBodyGroupId) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    struct RepeatLoopIfCharInst : Inst, RepeatLoopMixin
    {
        inline RepeatLoopIfCharInst(Label beginLabel) : Inst(RepeatLoopIfChar), RepeatLoopMixin(beginLabel) {}

        INST_BODY
    };

    struct RepeatLoopIfSetInst : Inst, RepeatLoopMixin
    {
        inline RepeatLoopIfSetInst(Label beginLabel) : Inst(RepeatLoopIfSet), RepeatLoopMixin(beginLabel) {}

        INST_BODY
    };

    // Loop is greedy, fixed width, deterministic body, no inner groups
    struct BeginLoopFixedInst : Inst, BeginLoopMixin, FixedLengthMixin
    {
        // exitLabel must always be fixed up
        inline BeginLoopFixedInst(int loopId, const CountDomain& repeats, bool hasOuterLoops, CharCount length)
            : Inst(BeginLoopFixed), BeginLoopMixin(loopId, repeats, hasOuterLoops, false), FixedLengthMixin(length) {}

        INST_BODY
    };

    struct RepeatLoopFixedInst : Inst, RepeatLoopMixin
    {
        inline RepeatLoopFixedInst(Label beginLabel) : Inst(RepeatLoopFixed), RepeatLoopMixin(beginLabel) {}

        INST_BODY
    };

    // Loop is greedy, contains a MatchSet only
    struct LoopSetInst : Inst, SetMixin<false>
    {
        int loopId;
        const CountDomain repeats;
        bool hasOuterLoops;

        // set must always be cloned from source
        inline LoopSetInst(int loopId, const CountDomain& repeats, bool hasOuterLoops)
            : Inst(LoopSet), loopId(loopId), repeats(repeats), hasOuterLoops(hasOuterLoops) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    // Loop is greedy, fixed width, deterministic body, one outermost group
    struct BeginLoopFixedGroupLastIterationInst : Inst, BeginLoopMixin, FixedLengthMixin, GroupMixin, NoNeedToSaveMixin
    {
        // exitLabel must always be fixed up
        inline BeginLoopFixedGroupLastIterationInst(int loopId, const CountDomain& repeats, bool hasOuterLoops, CharCount length, int groupId, bool noNeedToSave)
            : Inst(BeginLoopFixedGroupLastIteration), BeginLoopMixin(loopId, repeats, hasOuterLoops, false), FixedLengthMixin(length), GroupMixin(groupId), NoNeedToSaveMixin(noNeedToSave) {}

        INST_BODY
    };

    struct RepeatLoopFixedGroupLastIterationInst : Inst, RepeatLoopMixin
    {
        inline RepeatLoopFixedGroupLastIterationInst(Label beginLabel) : Inst(RepeatLoopFixedGroupLastIteration), RepeatLoopMixin(beginLabel) {}

        INST_BODY
    };

    // Loop is greedy, deterministic body, lower == 0, upper == inf, follow is irrefutable, no inner groups
    struct BeginGreedyLoopNoBacktrackInst : Inst
    {
        int loopId;
        Label exitLabel;

        // exitLabel must always be fixed up
        inline BeginGreedyLoopNoBacktrackInst(int loopId) : Inst(BeginGreedyLoopNoBacktrack), loopId(loopId) {}

        INST_BODY
    };

    struct RepeatGreedyLoopNoBacktrackInst : Inst, RepeatLoopMixin
    {
        inline RepeatGreedyLoopNoBacktrackInst(Label beginLabel) : Inst(RepeatGreedyLoopNoBacktrack), RepeatLoopMixin(beginLabel) {}

        INST_BODY
    };

    template<ChompMode Mode>
    struct ChompCharInst : Inst, CharMixin
    {
        ChompCharInst(const Char c) : Inst(Mode == ChompMode::Star ? ChompCharStar : ChompCharPlus), CharMixin(c) {}

        INST_BODY
    };

    template<ChompMode Mode>
    struct ChompSetInst : Inst, SetMixin<false>
    {
        // set must always be cloned from source
        ChompSetInst() : Inst(Mode == ChompMode::Star ? ChompSetStar : ChompSetPlus) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    template<ChompMode Mode>
    struct ChompCharGroupInst : Inst, CharMixin, GroupMixin, NoNeedToSaveMixin
    {
        ChompCharGroupInst(const Char c, const int groupId, const bool noNeedToSave)
            : Inst(Mode == ChompMode::Star ? ChompCharGroupStar : ChompCharGroupPlus),
            CharMixin(c),
            GroupMixin(groupId),
            NoNeedToSaveMixin(noNeedToSave)
        {
        }

        INST_BODY
    };

    template<ChompMode Mode>
    struct ChompSetGroupInst : Inst, SetMixin<false>, GroupMixin, NoNeedToSaveMixin
    {
        // set must always be cloned from source
        ChompSetGroupInst(const int groupId, const bool noNeedToSave)
            : Inst(Mode == ChompMode::Star ? ChompSetGroupStar : ChompSetGroupPlus),
            GroupMixin(groupId),
            NoNeedToSaveMixin(noNeedToSave)
        {
        }

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    struct ChompCharBoundedInst : Inst, CharMixin, ChompBoundedMixin
    {
        inline ChompCharBoundedInst(Char c, const CountDomain& repeats) : Inst(ChompCharBounded), CharMixin(c), ChompBoundedMixin(repeats) {}

        INST_BODY
    };

    struct ChompSetBoundedInst : Inst, SetMixin<false>, ChompBoundedMixin
    {
        // set must always be cloned from source
        inline ChompSetBoundedInst(const CountDomain& repeats) : Inst(ChompSetBounded), ChompBoundedMixin(repeats) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    struct ChompSetBoundedGroupLastCharInst : Inst, SetMixin<false>, ChompBoundedMixin, GroupMixin, NoNeedToSaveMixin
    {
        // set must always be cloned from source
        inline ChompSetBoundedGroupLastCharInst(const CountDomain& repeats, int groupId, bool noNeedToSave) : Inst(ChompSetBoundedGroupLastChar), ChompBoundedMixin(repeats), GroupMixin(groupId), NoNeedToSaveMixin(noNeedToSave) {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    //
    // Choicepoints
    //

    struct TryInst : Inst, TryMixin
    {
        // failLabel must always be fixed up
        inline TryInst() : Inst(Try), TryMixin() {}

        INST_BODY
    };

    struct TryIfCharInst : Inst, CharMixin, TryMixin
    {
        // failLabel must always be fixed up
        inline TryIfCharInst(Char c) : Inst(TryIfChar), CharMixin(c), TryMixin() {}

        INST_BODY
    };

    struct TryMatchCharInst : Inst, CharMixin, TryMixin
    {
        // failLabel must always be fixed up
        inline TryMatchCharInst(Char c) : Inst(TryMatchChar), CharMixin(c), TryMixin() {}

        INST_BODY
    };

    struct TryIfSetInst : Inst, SetMixin<false>, TryMixin
    {
        // set is always same as matching BeginLoopIfSetInst set
        // failLabel must always be fixed up
        inline TryIfSetInst() : Inst(TryIfSet), TryMixin() {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    struct TryMatchSetInst : Inst, SetMixin<false>, TryMixin
    {
        // set is always same as matching BeginLoopIfSetInst set
        // failLabel must always be fixed up
        inline TryMatchSetInst() : Inst(TryMatchSet), TryMixin() {}

        INST_BODY
        INST_BODY_FREE(SetMixin)
    };

    //
    // User-defined assertions
    //

    struct BeginAssertionInst : Inst, BodyGroupsMixin
    {
        bool isNegation;
        Label nextLabel;

        // nextLabel must always be fixed up
        inline BeginAssertionInst(bool isNegation, int minBodyGroupId, int maxBodyGroupId) : Inst(BeginAssertion), isNegation(isNegation), BodyGroupsMixin(minBodyGroupId, maxBodyGroupId)
        {
#if DBG
            nextLabel = (Label)-1;
#endif
        }

        INST_BODY
    };

    struct EndAssertionInst : Inst
    {
        inline EndAssertionInst() : Inst(EndAssertion) {}

        INST_BODY
    };

    // ----------------------------------------------------------------------
    // Matcher state
    // ----------------------------------------------------------------------

    struct LoopInfo : protected Chars<wchar_t>
    {
        CharCount number;            // current iteration number
        CharCount startInputOffset;  // input offset where the iteration started

        inline void Reset()
        {
#if DBG
            // So debug prints will look nice
            number = 0;
            startInputOffset = 0;
#endif
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    struct GroupInfo : protected Chars<wchar_t>
    {
        CharCount offset;
        CharCountOrFlag length;  // CharCountFlag => group is undefined

        inline GroupInfo() : offset(0), length(CharCountFlag) {}

        inline GroupInfo(CharCount offset, CharCountOrFlag length) : offset(offset), length(length) {}
        //This constructor will only be called by a cross-site marshalling and thus we shouldn't clear offset and length
        GroupInfo(VirtualTableInfoCtorEnum) { }
        inline bool IsUndefined() const { return length == CharCountFlag; }
        inline CharCount EndOffset() const { Assert(length != CharCountFlag); return offset + (CharCount)length; }

        inline void Reset()
        {
            // The start offset must not be changed when backtracking into the group
            length = CharCountFlag;
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const Char* const input) const;
#endif
    };

    struct AssertionInfo : private Chars<wchar_t>
    {
        const Label beginLabel;        // label of BeginAssertion instruction
        CharCount startInputOffset;    // input offset when begun assertion (so can rewind)
        size_t contStackPosition;      // top of continuation stack when begun assertion (so can cut)

        inline AssertionInfo(Label beginLabel, CharCount startInputOffset, size_t contStackPosition)
            : beginLabel(beginLabel), startInputOffset(startInputOffset), contStackPosition(contStackPosition) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w) const;
#endif
    };

    // ----------------------------------------------------------------------
    // Continuations
    // ----------------------------------------------------------------------

    struct Cont : protected Chars<wchar_t>
    {
        enum ContTag : uint32
        {
#define M(O) O,
#include "RegexContcodes.h"
#undef M
        };

        ContTag tag;

        inline Cont(ContTag tag) : tag(tag) {}

#if ENABLE_REGEX_CONFIG_OPTIONS
        virtual int Print(DebugWriter*w, const Char* const input) const = 0;
#endif
    };

#if ENABLE_REGEX_CONFIG_OPTIONS
#define CONT_PRINT int Print(DebugWriter*w, const Char* const input) const override;
#else
#define CONT_PRINT
#endif

#define REGEX_CONT_EXEC_PARAMETERS Matcher& matcher, const Char* const input, CharCount& inputOffset, const uint8*& instPointer, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks
#define CONT_BODY bool Exec(REGEX_CONT_EXEC_PARAMETERS); \
                  CONT_PRINT

    struct ResumeCont : Cont
    {
        CharCount origInputOffset;
        Label origInstLabel;

        inline ResumeCont(CharCount origInputOffset, Label origInstLabel) : Cont(Resume), origInputOffset(origInputOffset), origInstLabel(origInstLabel) {}

        CONT_BODY
    };

    struct RestoreLoopCont : Cont
    {
        int loopId;
        LoopInfo origLoopInfo;

        inline RestoreLoopCont(int loopId, LoopInfo& origLoopInfo) : Cont(RestoreLoop), loopId(loopId), origLoopInfo(origLoopInfo) {}

        CONT_BODY
    };

    struct RestoreGroupCont : Cont
    {
        int groupId;
        GroupInfo origGroupInfo;

        RestoreGroupCont(int groupId, const GroupInfo &origGroupInfo)
            : Cont(RestoreGroup), groupId(groupId), origGroupInfo(origGroupInfo)
        {
        }

        CONT_BODY
    };

    struct ResetGroupCont : Cont
    {
        const int groupId;

        ResetGroupCont(const int groupId) : Cont(ResetGroup), groupId(groupId) {}

        CONT_BODY
    };

    struct ResetGroupRangeCont : Cont
    {
        const int fromGroupId;
        const int toGroupId;

        ResetGroupRangeCont(const int fromGroupId, const int toGroupId)
            : Cont(ResetGroupRange), fromGroupId(fromGroupId), toGroupId(toGroupId)
        {
            Assert(fromGroupId >= 0);
            Assert(toGroupId >= 0);
            Assert(fromGroupId < toGroupId);
        }

        CONT_BODY
    };

    struct RepeatLoopCont : Cont
    {
        Label beginLabel;           // label of BeginLoop instruction
        CharCount origInputOffset;  // where to go back to

        inline RepeatLoopCont(Label beginLabel, CharCount origInputOffset) : Cont(RepeatLoop), beginLabel(beginLabel), origInputOffset(origInputOffset) {}

        CONT_BODY
    };

    struct PopAssertionCont : Cont
    {
        inline PopAssertionCont() : Cont(PopAssertion) {}

        CONT_BODY
    };

    struct RewindLoopFixedCont : Cont
    {
        Label beginLabel;   // label of BeginLoopFixed instruction
        bool tryingBody;    // true if attempting an additional iteration of loop body, otherwise attempting loop follow

        inline RewindLoopFixedCont(Label beginLabel, bool tryingBody) : Cont(RewindLoopFixed), beginLabel(beginLabel), tryingBody(tryingBody) {}

        CONT_BODY
    };

    struct RewindLoopSetCont : Cont
    {
        Label beginLabel;   // label of LoopSet instruction

        inline RewindLoopSetCont(Label beginLabel) : Cont(RewindLoopSet), beginLabel(beginLabel) {}

        CONT_BODY
    };


    struct RewindLoopFixedGroupLastIterationCont : Cont
    {
        Label beginLabel;   // label of BeginLoopFixedGroupLastIteration instruction
        bool tryingBody;    // true if attempting an additional iteration of loop body, otherwise attempting loop follow

        inline RewindLoopFixedGroupLastIterationCont(Label beginLabel, bool tryingBody) : Cont(RewindLoopFixedGroupLastIteration), beginLabel(beginLabel), tryingBody(tryingBody) {}

        CONT_BODY
    };

    // ----------------------------------------------------------------------
    // Matcher
    // ----------------------------------------------------------------------

    class ContStack : public ContinuousPageStackOfVariableElements<Cont>, private Chars<wchar_t>
    {
    public:
        __inline ContStack(PageAllocator *const pageAllocator, void (*const outOfMemoryFunc)())
            : ContinuousPageStackOfVariableElements(pageAllocator, outOfMemoryFunc)
        {
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const Char* const input) const;
#endif
    };

    class AssertionStack : public ContinuousPageStackOfFixedElements<AssertionInfo>
    {
    public:
        __inline AssertionStack(PageAllocator *const pageAllocator, void (*const outOfMemoryFunc)())
            : ContinuousPageStackOfFixedElements(pageAllocator, outOfMemoryFunc)
        {
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const Matcher* const matcher) const;
#endif
    };

    struct RegexStacks
    {
        RegexStacks(PageAllocator * pageAllocator) :
            contStack(pageAllocator, Js::Throw::OutOfMemory),
            assertionStack(pageAllocator, Js::Throw::OutOfMemory) {};


        ContStack contStack;
        AssertionStack assertionStack;
    };

    enum HardFailMode
    {
        BacktrackAndLater,
        BacktrackOnly,
        LaterOnly,
        ImmediateFail
    };

    class Matcher : private Chars<wchar_t>
    {
#define M(TagName) friend struct TagName##Inst;
#define MTemplate(TagName, TemplateDeclaration, GenericClassName, ...) TemplateDeclaration friend struct GenericClassName;
#include "RegexOpcodes.h"
#undef M
#undef MTemplate

#define M(O) friend O##Cont;
#include "RegexContcodes.h"
#undef M

        template <typename ScannerT>
        friend struct SyncToLiteralAndConsumeInstT;
        template <typename ScannerT>
        friend struct SyncToLiteralAndContinueInstT;
        template <typename ScannerT>
        friend struct SyncToLiteralAndBackupInstT;
        template <typename ScannerT>
        friend struct ScannerMixinT;
        friend struct Char2LiteralScannerMixin;
        template <uint lastPatCharEquivClassSize>
        friend struct EquivScannerMixinT;

        friend GroupInfo;
        friend LoopInfo;

    public:
        static const uint TicksPerQc;
        static const uint TicksPerQcTimeCheck;
        static const uint TimePerQc; // milliseconds

    private:
        RegexPattern *const pattern;
        StandardChars<Char>* standardChars;
        const Program* program;

        GroupInfo* groupInfos;
        LoopInfo* loopInfos;

        uint previousQcTime;

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexStats* stats;
        DebugWriter* w;
#endif

    public:
        Matcher(Js::ScriptContext* scriptContext, RegexPattern* pattern);
        static Matcher *New(Js::ScriptContext* scriptContext, RegexPattern* pattern);

        bool Match
            ( const Char* const input
            , const CharCount inputLength
            , CharCount offset
            , Js::ScriptContext * scriptContext
#if ENABLE_REGEX_CONFIG_OPTIONS
            , RegexStats* stats
            , DebugWriter* w
#endif
            );

        __inline bool WasLastMatchSuccessful() const
        {
            return !groupInfos[0].IsUndefined();
        }

        __inline int NumGroups() const
        {
            return program->numGroups;
        }

        __inline GroupInfo GetGroup(int groupId) const
        {
            return *GroupIdToGroupInfo(groupId);
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        void Print(DebugWriter* w, const Char* const input, const CharCount inputLength, CharCount inputOffset, const uint8* instPointer, ContStack &contStack, AssertionStack &assertionStack) const;
#endif

    private:
#if ENABLE_REGEX_CONFIG_OPTIONS
        void PushStats(ContStack& contStack, const Char* const input) const;
        void PopStats(ContStack& contStack, const Char* const input) const;
        void UnPopStats(ContStack& contStack, const Char* const input) const;
        void CompStats() const;
        void InstStats() const;
#endif

    private:
        __inline void QueryContinue(uint &qcTicks);
        void DoQueryContinue(const uint qcTicks);
    public:
        static void TraceQueryContinue(const uint now);

    private:
        // Try backtracking, or return true if should stop. There could be a match using a later starting point.
        bool Fail(const Char* const input, CharCount &inputOffset, const uint8 *&instPointer, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks);
        bool RunContStack(const Char* const input, CharCount &inputOffset, const uint8 *&instPointer, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks);

        // As above, but control whether to try backtracking or later matches
        __inline bool HardFail(const Char* const input, const CharCount inputLength, CharCount &matchStart, CharCount &inputOffset, const uint8 *&instPointer, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks, HardFailMode mode);

        __inline void Run(const Char* const input, const CharCount inputLength, CharCount &matchStart, CharCount &nextSyncInputOffset, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks);
        __inline bool MatchHere(const Char* const input, const CharCount inputLength, CharCount &matchStart, CharCount &nextSyncInputOffset, ContStack &contStack, AssertionStack &assertionStack, uint &qcTicks);

        // Return true if assertion succeeded
        __inline bool PopAssertion(CharCount &inputOffset, const uint8 *&instPointer, ContStack &contStack, AssertionStack &assertionStack, bool isFailed);

        __inline Label InstPointerToLabel(const uint8* inst) const
        {
            Assert(inst >= program->rep.insts.insts && inst < program->rep.insts.insts + program->rep.insts.instsLen);
            return (Label)((uint8*)inst - program->rep.insts.insts);
        }

        __inline uint8* LabelToInstPointer(Label label) const
        {
            Assert(label < program->rep.insts.instsLen);
            return program->rep.insts.insts + label;
        }

        template <typename T>
        __inline T* LabelToInstPointer(Inst::InstTag tag, Label label) const
        {
            Assert(label + sizeof(T) <= program->rep.insts.instsLen);
            Assert(((Inst*)(program->rep.insts.insts + label))->tag == tag);
            return (T*)(program->rep.insts.insts + label);
        }

        __inline LoopInfo* LoopIdToLoopInfo(int loopId)
        {
            Assert(loopId >= 0 && loopId < program->numLoops);
            return loopInfos + loopId;
        }

    public:
        __inline GroupInfo* GroupIdToGroupInfo(int groupId) const
        {
            Assert(groupId >= 0 && groupId < program->numGroups);
            return groupInfos + groupId;
        }

        Matcher *CloneToScriptContext(Js::ScriptContext *scriptContext, RegexPattern *pattern);
    private:

        typedef bool (UnifiedRegex::Matcher::*ComparerForSingleChar)(const Char left, const Char right);
        ComparerForSingleChar comparerForSingleChar;

        // Specialized matcher for regex c - case insensitive
        __inline bool MatchSingleCharCaseInsensitive(const Char* const input, const CharCount inputLength, CharCount offset, const Char c);
        __inline bool MatchSingleCharCaseInsensitiveHere(CaseInsensitive::MappingSource mappingSource, const Char* const input, CharCount offset, const Char c);

        // Specialized matcher for regex c - case sensitive
        __inline bool MatchSingleCharCaseSensitive(const Char* const input, const CharCount inputLength, CharCount offset, const Char c);

        // Specialized matcher for regex \b\w+\b
        __inline bool MatchBoundedWord(const Char* const input, const CharCount inputLength, CharCount offset);

        // Specialized matcher for regex ^\s*|\s*$
        __inline bool MatchLeadingTrailingSpaces(const Char* const input, const CharCount inputLength, CharCount offset);

        // Specialized matcher for octoquad patterns
        __inline bool MatchOctoquad(const Char* const input, const CharCount inputLength, CharCount offset, OctoquadMatcher* matcher);

        // Specialized matcher for regex ^literal
        __inline bool MatchBOILiteral2(const Char * const input, const CharCount inputLength, CharCount offset, DWORD literal2);

        void SaveInnerGroups(const int fromGroupId, const int toGroupId, const bool reset, const Char *const input, ContStack &contStack);
        void DoSaveInnerGroups(const int fromGroupId, const int toGroupId, const bool reset, const Char *const input, ContStack &contStack);
        void SaveInnerGroups_AllUndefined(const int fromGroupId, const int toGroupId, const Char *const input, ContStack &contStack);
        void DoSaveInnerGroups_AllUndefined(const int fromGroupId, const int toGroupId, const Char *const input, ContStack &contStack);
        void ResetGroup(int groupId);
        void ResetInnerGroups(int minGroupId, int maxGroupId);
#if DBG
        void ResetLoopInfos();
#endif
    };
}

#undef INST_BODY_FREE
#undef INST_BODY_PRINT
#undef INST_BODY
#undef CONT_PRINT
#undef CONT_BODY
