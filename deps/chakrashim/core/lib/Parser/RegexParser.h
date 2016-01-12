//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace UnifiedRegex
{
    struct ParseError
    {
        bool isBody;
        CharCount pos;           // Position in unicode characters
        CharCount encodedPos;    // Position in underlying characters (eg utf-8 bytes)
        HRESULT error;

        ParseError(bool isBody, CharCount pos, CharCount encodedPos, HRESULT error);
    };

    template <typename EncodingPolicy, const bool IsLiteral>
    class Parser : private EncodingPolicy, private Chars<wchar_t>
    {
    private:
        typedef typename EncodingPolicy::EncodedChar EncodedChar;

        // A linked list node to track indices of surrogate pairs.
        struct SurrogatePairTracker
        {
            const EncodedChar* location;
            // If this surrogate pair is inside a range, then rangeLocation isn't null.
            const EncodedChar* rangeLocation;
            codepoint_t value;
            uint32 length;
            size_t multiUnits;
            SurrogatePairTracker* next;

            SurrogatePairTracker(const EncodedChar* location, codepoint_t value, uint32 length, size_t multiUnits)
                : location(location)
                , next(nullptr)
                , value(value)
                , length(length)
                , multiUnits(multiUnits)
                , rangeLocation(nullptr)
            {
            }

            SurrogatePairTracker(const EncodedChar* location, const EncodedChar* rangeLocation, codepoint_t value, uint32 length, size_t multiUnits)
                : location(location)
                , next(nullptr)
                , value(value)
                , length(length)
                , multiUnits(multiUnits)
                , rangeLocation(rangeLocation)
            {
            }

            bool IsInsideRange() const
            {
                return this->rangeLocation != nullptr;
            }
        };

        static const CharCount initLitbufSize = 16;

        Js::ScriptContext* scriptContext;
        // Arena for nodes and items needed only during compliation
        ArenaAllocator* ctAllocator;
        // Standard characters using raw encoding character representation (eg char for utf-8)
        StandardChars<EncodedChar>* standardEncodedChars;
        // Standard characters using final character representation (eg wchar_t for Unicode)
        StandardChars<Char>* standardChars;
#if ENABLE_REGEX_CONFIG_OPTIONS
        DebugWriter* w;
#endif

        const EncodedChar* input;
        const EncodedChar* inputLim;
        const EncodedChar* next;
        bool inBody;

        int numGroups; // determined in first parse
        int nextGroupId;
        // Buffer accumulating all literals.
        // In compile-time allocator, must be transferred to runtime allocator when build program
        Char* litbuf;
        CharCount litbufLen;
        CharCount litbufNext;

        // During pass 0, if /u option for regex is provided, a linked list will be built up to
        // track positions of surrogate pairs in the buffer. During pass 1, these linked lists will be used
        // to figure out when to output a surrogate pair node.
        SurrogatePairTracker* surrogatePairList;
        SurrogatePairTracker* currentSurrogatePairNode;
        bool unicodeFlagPresent;
        bool caseInsensitiveFlagPresent;

        // The following two variables are used to determine if the the surrogate pair has been encountered
        // First holds the temporary location, second holds the value of the codepoint
        const EncodedChar* tempLocationOfSurrogatePair;
        // This will be set to a location when we are parsing a range in TermPass0, and cleared when we are out of it.
        const EncodedChar* tempLocationOfRange;
        codepoint_t codePointAtTempLocation;

        // When a surrogate is added for tracking, this will be updated.
        const EncodedChar* positionAfterLastSurrogate;
        codepoint_t valueOfLastSurrogate;

        // deferred error state.
        ParseError* deferredIfNotUnicodeError;
        ParseError* deferredIfUnicodeError;

    private:

        //
        // Input buffer management
        //

        void SetPosition(const EncodedChar* input, const EncodedChar* inputLim, bool inBody);

        // Current position in number of logical characters, regardless of underlying character encoding
        inline CharCount Pos();

        inline bool IsEOF();
        inline bool ECCanConsume(CharCount n);
        inline EncodedChar ECLookahead(CharCount n = 0);
        inline EncodedChar ECLookback(CharCount n = 0);
        inline void ECConsume(CharCount n = 1);
        inline void ECConsumeMultiUnit(CharCount n = 1);
        inline void ECRevert(CharCount n = 1);

        //
        // Helpers
        //
        int TryParseExtendedUnicodeEscape(Char& c, bool& previousSurrogatePart, bool trackSurrogatePair = false);
        void TrackIfSurrogatePair(codepoint_t codePoint, const EncodedChar* location, uint32 consumptionLength);
        Node* CreateSurrogatePairAtom(wchar_t lower, wchar_t upper);
        AltNode* AppendSurrogateRangeToDisjunction(codepoint_t lowerCodePoint, codepoint_t upperCodePoint, AltNode *lastAlttNode);
        AltNode* AppendSurrogatePairToDisjunction(codepoint_t codePoint, AltNode *lastAlttNode);

        //
        // Errors
        //

        void Fail(HRESULT error);
        void DeferredFailIfUnicode(HRESULT error);
        void DeferredFailIfNotUnicode(HRESULT error);
        inline void ECMust(EncodedChar ec, HRESULT error);
        inline Char NextChar();

        //
        // Patterns/Disjunctions/Alternatives
        //

        void PatternPass0();
        Node* PatternPass1();
        Node* UnionNodes(Node* prev, Node* curr);
        void DisjunctionPass0(int depth);
        Node* DisjunctionPass1();
        bool IsEndOfAlternative();
        void EnsureLitbuf(CharCount size);
        void AccumLiteral(MatchLiteralNode* deferredLiteralNode, Node* charOrLiteralNode);
        Node* FinalTerm(Node* node, MatchLiteralNode* deferredLiteralNode);
        void AlternativePass0(int depth);
        Node* AlternativePass1();

        //
        // Terms
        //

        Node* NewLoopNode(CharCount lower, CharCountOrFlag upper, bool isGreedy, Node* body);
        bool AtQuantifier();
        bool OptNonGreedy();
        CharCount RepeatCount();
        void TermPass0(int depth);
        Node* TermPass1(MatchCharNode* deferredCharNode, bool& previousSurrogatePart);
        bool AtomEscapePass0();
        bool AtomEscapePass1(Node*& node, MatchCharNode* deferredCharNode, bool& previousSurrogatePart);
        bool SurrogatePairPass1(Node*& node, MatchCharNode* deferredCharNode, bool& previousSurrogatePart);

        //
        // Classes
        //

        bool AtSecondSingletonClassAtom();
        void CharacterClassPass0();
        template <bool containsSurrogates>
        Node* CharacterClassPass1();
        bool ClassEscapePass0(Char& singleton, bool& previousSurrogatePart);
        Node* ClassEscapePass1(MatchCharNode* deferredCharNode, MatchSetNode* deferredSetNode, bool& previousSurrogatePart);
        Node* GetNodeWithValidCharacterSet(EncodedChar ch);

        //
        // Options
        //

        void Options(RegexFlags& flags);

    public:

        Parser
            ( Js::ScriptContext* scriptContext
            , ArenaAllocator* ctAllocator
            , StandardChars<EncodedChar>* standardEncodedChars
            , StandardChars<Char>* standardChars
            , bool isFromExternalSource
#if ENABLE_REGEX_CONFIG_OPTIONS
            , DebugWriter* w
#endif
            );

        //
        // Entry points
        //


        Node* ParseDynamic
            ( const EncodedChar* body           // non null, null terminated (may contain embedded nulls)
            , const EncodedChar* bodyLim        // points to terminating null of above
            , const EncodedChar* opts           // may be null if no options, otherwise null terminated
            , const EncodedChar* optsLim        // if above non-null, points to terminating null of above
            , RegexFlags& flags );

        // (*) For ParseLiteral:
        //  - input string must be null terminated
        //  - inputLim may point to the terminating null in above or before it
        //     - if the later, input is known to be syntactically well-formed so that the parser
        //       will find the natural end of the regex literal before passing inputLim
        //  - input may conatin nulls before the inputLim

        Node* ParseLiteral
            ( const EncodedChar* input          // non null, null terminated (may contain embedded nulls)
            , const EncodedChar* inputLim       // see (*) above
            , CharCount& outBodyEncodedChars    // in encoded characters, not including trailing '/'
            , CharCount& outTotalEncodedChars   // in encoded characters, including trailing '/' and any options
            , CharCount& outBodyChars           // in unicode characters, not including ttrailing '/'
            , CharCount& outTotalChars          // in unicode characters, including trailing '/' and any options
            , RegexFlags& flags );

        void ParseLiteralNoAST
            ( const EncodedChar* input          // non null, null terminated
            , const EncodedChar* inputLim       // see (*) above
            , CharCount& outBodyEncodedChars
            , CharCount& outTotalEncodedChars
            , CharCount& outBodyChars
            , CharCount& outTotalChars );

        template<const bool buildAST>
        RegexPattern* CompileProgram
            ( Node* root,
              const EncodedChar*& currentCharacter,
              const CharCount totalLen,
              const CharCount bodyChars,
              const CharCount totalChars,
              const RegexFlags flags );

        static void CaptureEmptySourceAndNoGroups(Program* program);

        // bodyChars is number of unicode characters in program body, which may be less than the number
        // of underlying UTF-8 characters
        void CaptureSourceAndGroups(Recycler* recycler, Program* program, const EncodedChar* body, CharCount bodyChars);

        inline const Char* GetLitbuf() { return litbuf; }

        void FreeBody();

        size_t GetMultiUnits() { return this->m_cMultiUnits; }
    };
}
