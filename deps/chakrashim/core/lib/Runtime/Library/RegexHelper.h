//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct RegexMatchState;

    class RegexHelper
    {
        static const int MinTrigramInputLength=250000;

        //
        // Dynamic compilation
        //

        static bool GetFlags(ScriptContext* scriptContext, __in_ecount(strLen) const wchar_t* str, CharCount strLen, UnifiedRegex::RegexFlags &flags);
    public:
        static UnifiedRegex::RegexPattern* CompileDynamic(ScriptContext *scriptContext, const wchar_t* psz, CharCount csz, const wchar_t* pszOpts, CharCount cszOpts, bool isLiteralSource);
        static UnifiedRegex::RegexPattern* CompileDynamic(ScriptContext *scriptContext, const wchar_t* psz, CharCount csz, UnifiedRegex::RegexFlags flags, bool isLiteralSource);
    private:
        static UnifiedRegex::RegexPattern* PrimCompileDynamic(ScriptContext *scriptContext, const wchar_t* psz, CharCount csz, const wchar_t* pszOpts, CharCount cszOpts, bool isLiteralSource);

        //
        // Primitives
        //

    public:
        static UnifiedRegex::GroupInfo SimpleMatch(ScriptContext * scriptContext, UnifiedRegex::RegexPattern * pattern, const wchar_t * inputStr,  CharCount inputLength, CharCount offset);
        static Var NonMatchValue(ScriptContext* scriptContext, bool isGlobalCtor);
        static Var GetString(ScriptContext* scriptContext, JavascriptString* input, Var nonMatchValue, UnifiedRegex::GroupInfo group);
        static Var GetGroup(ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern, JavascriptString* input, Var nonMatchValue, int groupId);
    private:
        static void PropagateLastMatch
            ( ScriptContext* scriptContext
            , bool isGlobal
            , bool isSticky
            , JavascriptRegExp* regularExpression
            , JavascriptString* lastInput
            , UnifiedRegex::GroupInfo lastSuccessfullMatch
            , UnifiedRegex::GroupInfo lastActualMatch
            , bool updateRegex
            , bool updateCtor
            , bool useSplitPattern = false );
        static void PropagateLastMatchToRegex
            ( ScriptContext* scriptContext
            , bool isGlobal
            , bool isSticky
            , JavascriptRegExp* regularExpression
            , UnifiedRegex::GroupInfo lastSuccessfullMatch
            , UnifiedRegex::GroupInfo lastActualMatch );
        static void PropagateLastMatchToCtor
            ( ScriptContext* scriptContext
            , JavascriptRegExp* regularExpression
            , JavascriptString* lastInput
            , UnifiedRegex::GroupInfo lastSuccessfullMatch
            , bool useSplitPattern );

        static bool GetInitialOffset(bool isGlobal, bool isSticky, JavascriptRegExp* regularExpression, CharCount inputLength, CharCount& offset);
        static JavascriptArray* CreateMatchResult(void *const stackAllocationPointer, ScriptContext* scriptContext, bool isGlobal, int numGroups, JavascriptString* input);
        static void FinalizeMatchResult(ScriptContext* scriptContext, bool isGlobal, JavascriptArray* arr, UnifiedRegex::GroupInfo match);
        static JavascriptArray* CreateExecResult(void *const stackAllocationPointer, ScriptContext* scriptContext, int numGroups, JavascriptString* input, UnifiedRegex::GroupInfo match);
        template<typename T> static T CheckCrossContextAndMarshalResult(T value, ScriptContext* targetContext);

        //
        // Regex entry points
        //

    public:
        static Var RegexMatchResultUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        static Var RegexMatchResultUsedAndMayBeTemp(void *const stackAllocationPointer, ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        static Var RegexMatchResultNotUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        static Var RegexMatch(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, bool noResult, void *const stackAllocationPointer = nullptr);
        static Var RegexMatchNoHistory(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, bool noResult);
        static Var RegexExecResultUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        static Var RegexExecResultUsedAndMayBeTemp(void *const stackAllocationPointer, ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        static Var RegexExecResultNotUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        static Var RegexExec(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, bool noResult, void *const stackAllocationPointer = nullptr);
        static BOOL RegexTest(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        template<bool mustMatchEntireInput> static BOOL RegexTest_NonScript(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, const wchar_t *const input, const CharCount inputLength);

    private:
        static void PrimBeginMatch(RegexMatchState& state, ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern, const wchar_t* input, CharCount inputLength, bool alwaysNeedAlloc);
        static UnifiedRegex::GroupInfo PrimMatch(RegexMatchState& state, ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern, CharCount inputLength, CharCount offset);
        static void PrimEndMatch(RegexMatchState& state, ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern);

        static void ReplaceFormatString
            ( ScriptContext* scriptContext
            , UnifiedRegex::RegexPattern* pattern
            , JavascriptString* input
            , UnifiedRegex::GroupInfo match
            , JavascriptString* replace
            , int substitutions
            , __in_ecount(substitutions) CharCount* substitutionOffsets
            , CompoundString::Builder<64 * sizeof(void *) / sizeof(wchar_t)>& concatenated );

    public:
        static Var RegexReplaceResultUsed(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace);
        static Var RegexReplaceResultNotUsed(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace);
        static Var RegexReplace(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace, JavascriptString* options, bool noResult);
        static Var RegexReplaceFunction(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptFunction* replacefn, JavascriptString* options);
        static Var StringReplace(JavascriptString* regularExpression, JavascriptString* input, JavascriptString* replace);
        static Var StringReplace(JavascriptString* regularExpression, JavascriptString* input, JavascriptFunction* replacefn);
        static Var RegexSplitResultUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit);
        static Var RegexSplitResultUsedAndMayBeTemp(void *const stackAllocationPointer, ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit);
        static Var RegexSplitResultNotUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit);
        static Var RegexSplit(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit, bool noResult, void *const stackAllocationPointer = nullptr);
        static Var RegexSearch(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        static Var StringSplit(JavascriptString* regularExpression, JavascriptString* input, CharCount limit);
        static bool IsResultNotUsed(CallFlags flags);

    private:
        static void AppendSubString(ScriptContext* scriptContext, JavascriptArray* ary, CharCount& numElems, JavascriptString* input, CharCount startInclusize, CharCount endExclusive);
        template <bool updateHistory>
        static Var RegexMatchImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, bool noResult, void *const stackAllocationPointer = nullptr);
        static Var RegexExecImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, bool noResult, void *const stackAllocationPointer = nullptr);
        static Var RegexReplaceImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace, JavascriptString* options, bool noResult);
        static Var RegexReplaceImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptFunction* replacefn, JavascriptString* options);
        static Var RegexSearchImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input);
        __inline static UnifiedRegex::RegexPattern *RegexHelper::GetSplitPattern(ScriptContext* scriptContext, JavascriptRegExp *regularExpression);
        static Var RegexSplitImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit, bool noResult, void *const stackAllocationPointer = nullptr);
        static int GetReplaceSubstitutions(const wchar_t * const replaceStr, CharCount const replaceLength, ArenaAllocator * const tempAllocator, CharCount** const substitutionOffsetsOut);
    };
}
