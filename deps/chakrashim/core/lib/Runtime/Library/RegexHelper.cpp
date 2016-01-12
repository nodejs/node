//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

// Parser Includes
#include "DebugWriter.h"
#include "RegexStats.h"
#include "OctoquadIdentifier.h"
#include "RegexCompileTime.h"
#include "RegexParser.h"
#include "RegexPattern.h"

namespace Js
{
    // ----------------------------------------------------------------------
    // Dynamic compilation
    // ----------------------------------------------------------------------

    // See also:
    //    UnifiedRegex::Parser::Options(...)
    bool RegexHelper::GetFlags(Js::ScriptContext* scriptContext, __in_ecount(strLen) const wchar_t* str, CharCount strLen, UnifiedRegex::RegexFlags &flags)
    {
        for (CharCount i = 0; i < strLen; i++)
        {
            switch (str[i])
            {
            case 'i':
                if ((flags & UnifiedRegex::IgnoreCaseRegexFlag) != 0)
                    return false;
                flags = (UnifiedRegex::RegexFlags)(flags | UnifiedRegex::IgnoreCaseRegexFlag);
                break;
            case 'g':
                if ((flags & UnifiedRegex::GlobalRegexFlag) != 0)
                    return false;
                flags = (UnifiedRegex::RegexFlags)(flags | UnifiedRegex::GlobalRegexFlag);
                break;
            case 'm':
                if ((flags & UnifiedRegex::MultilineRegexFlag) != 0)
                    return false;
                flags = (UnifiedRegex::RegexFlags)(flags | UnifiedRegex::MultilineRegexFlag);
                break;
            case 'u':
                if (scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled())
                {
                    if((flags & UnifiedRegex::UnicodeRegexFlag) != 0)
                        return false;
                    flags = (UnifiedRegex::RegexFlags)(flags | UnifiedRegex::UnicodeRegexFlag);
                    break;
                }
                return false;
            case 'y':
                if (scriptContext->GetConfig()->IsES6RegExStickyEnabled())
                {
                    if ((flags & UnifiedRegex::StickyRegexFlag) != 0)
                        return false;
                    flags = (UnifiedRegex::RegexFlags)(flags | UnifiedRegex::StickyRegexFlag);
                    break;
                }
                return false;
            default:
                return false;
            }
        }

        return true;
    }

    UnifiedRegex::RegexPattern* RegexHelper::CompileDynamic(ScriptContext *scriptContext, const wchar_t* psz, CharCount csz, const wchar_t* pszOpts, CharCount cszOpts, bool isLiteralSource)
    {
        Assert(psz != 0 && psz[csz] == 0);
        Assert(pszOpts != 0 || cszOpts == 0);
        Assert(pszOpts == 0 || pszOpts[cszOpts] == 0);

        UnifiedRegex::RegexFlags flags = UnifiedRegex::NoRegexFlags;

        if (pszOpts != NULL)
        {
            if (!GetFlags(scriptContext, pszOpts, cszOpts, flags))
            {
                // Compile in order to throw appropriate error for ill-formed flags
                PrimCompileDynamic(scriptContext, psz, csz, pszOpts, cszOpts, isLiteralSource);
                Assert(false);
            }
        }

        if(isLiteralSource)
        {
            // The source is from a literal regex, so we're cloning a literal regex. Don't use the dynamic regex MRU map since
            // these literal regex patterns' lifetimes are tied with the function body.
            return PrimCompileDynamic(scriptContext, psz, csz, pszOpts, cszOpts, isLiteralSource);
        }

        UnifiedRegex::RegexKey lookupKey(psz, csz, flags);
        UnifiedRegex::RegexPattern* pattern;
        RegexPatternMruMap* dynamicRegexMap = scriptContext->GetDynamicRegexMap();
        if (!dynamicRegexMap->TryGetValue(lookupKey, &pattern))
        {
            pattern = PrimCompileDynamic(scriptContext, psz, csz, pszOpts, cszOpts, isLiteralSource);

            // WARNING: Must calculate key again so that dictionary has copy of source associated with the pattern
            const auto source = pattern->GetSource();
            UnifiedRegex::RegexKey finalKey(source.GetBuffer(), source.GetLength(), flags);
            dynamicRegexMap->Add(finalKey, pattern);
        }
        return pattern;
    }

    UnifiedRegex::RegexPattern* RegexHelper::CompileDynamic(
        ScriptContext *scriptContext, const wchar_t* psz, CharCount csz, UnifiedRegex::RegexFlags flags, bool isLiteralSource)
    {
        //
        // Regex compilations are mostly string parsing based. To avoid duplicating validation rules,
        // generate a trivial options string right here on the stack and delegate to the string parsing
        // based implementation.
        //
        const CharCount OPT_BUF_SIZE = 6;
        wchar_t opts[OPT_BUF_SIZE];

        CharCount i = 0;
        if (flags & UnifiedRegex::IgnoreCaseRegexFlag)
        {
            opts[i++] = L'i';
        }
        if (flags & UnifiedRegex::GlobalRegexFlag)
        {
            opts[i++] = L'g';
        }
        if (flags & UnifiedRegex::MultilineRegexFlag)
        {
            opts[i++] = L'm';
        }
        if (flags & UnifiedRegex::UnicodeRegexFlag)
        {
            Assert(scriptContext->GetConfig()->IsES6UnicodeExtensionsEnabled());
            opts[i++] = L'u';
        }
        if (flags & UnifiedRegex::StickyRegexFlag)
        {
            Assert(scriptContext->GetConfig()->IsES6RegExStickyEnabled());
            opts[i++] = L'y';
        }
        Assert(i < OPT_BUF_SIZE);
        opts[i] = NULL;

        return CompileDynamic(scriptContext, psz, csz, opts, i, isLiteralSource);
    }

    UnifiedRegex::RegexPattern* RegexHelper::PrimCompileDynamic(ScriptContext *scriptContext, const wchar_t* psz, CharCount csz, const wchar_t* pszOpts, CharCount cszOpts, bool isLiteralSource)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackRegex);

        // SEE ALSO: Scanner<EncodingPolicy>::ScanRegExpConstant()
#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::RegexCompilePhase);
#endif
        ArenaAllocator* rtAllocator = scriptContext->RegexAllocator();
#if ENABLE_REGEX_CONFIG_OPTIONS
        UnifiedRegex::DebugWriter *dw = 0;
        if (REGEX_CONFIG_FLAG(RegexDebug))
            dw = scriptContext->GetRegexDebugWriter();
        UnifiedRegex::RegexStats* stats = 0;
#endif
        UnifiedRegex::RegexFlags flags = UnifiedRegex::NoRegexFlags;

        if(csz == 0 && cszOpts == 0)
        {
            // Fast path for compiling the empty regex with empty flags, for the RegExp constructor object and other cases.
            // These empty regexes are dynamic regexes and so this fast path only exists for dynamic regex compilation. The
            // standard chars in particular, do not need to be initialized to compile this regex.
            UnifiedRegex::Program* program = UnifiedRegex::Program::New(scriptContext->GetRecycler(), flags);
            UnifiedRegex::Parser<NullTerminatedUnicodeEncodingPolicy, false>::CaptureEmptySourceAndNoGroups(program);
            UnifiedRegex::RegexPattern* pattern = UnifiedRegex::RegexPattern::New(scriptContext, program, false);
            UnifiedRegex::Compiler::CompileEmptyRegex
                ( program
                , pattern
#if ENABLE_REGEX_CONFIG_OPTIONS
                , dw
                , stats
#endif
                );
#ifdef PROFILE_EXEC
            scriptContext->ProfileEnd(Js::RegexCompilePhase);
#endif
            return pattern;
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (REGEX_CONFIG_FLAG(RegexProfile))
            scriptContext->GetRegexStatsDatabase()->BeginProfile();
#endif
        BEGIN_TEMP_ALLOCATOR(ctAllocator, scriptContext, L"UnifiedRegexParseAndCompile");
        UnifiedRegex::StandardChars<wchar_t>* standardChars = scriptContext->GetThreadContext()->GetStandardChars((wchar_t*)0);
        UnifiedRegex::Node* root = 0;
        UnifiedRegex::Parser<NullTerminatedUnicodeEncodingPolicy, false> parser
            ( scriptContext
            , ctAllocator
            , standardChars
            , standardChars
            , false
#if ENABLE_REGEX_CONFIG_OPTIONS
            , dw
#endif
            );
        try
        {
            root = parser.ParseDynamic(psz, psz + csz, pszOpts, pszOpts + cszOpts, flags);
        }
        catch (UnifiedRegex::ParseError e)
        {
            END_TEMP_ALLOCATOR(ctAllocator, scriptContext);
#ifdef PROFILE_EXEC
            scriptContext->ProfileEnd(Js::RegexCompilePhase);
#endif
            Js::JavascriptError::ThrowSyntaxError(scriptContext, e.error);
            // never reached
        }

        const auto recycler = scriptContext->GetRecycler();
        UnifiedRegex::Program* program = UnifiedRegex::Program::New(recycler, flags);
        parser.CaptureSourceAndGroups(recycler, program, psz, csz);

        UnifiedRegex::RegexPattern* pattern = UnifiedRegex::RegexPattern::New(scriptContext, program, isLiteralSource);

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (REGEX_CONFIG_FLAG(RegexProfile))
        {
            stats = scriptContext->GetRegexStatsDatabase()->GetRegexStats(pattern);
            scriptContext->GetRegexStatsDatabase()->EndProfile(stats, UnifiedRegex::RegexStats::Parse);
        }
        if (REGEX_CONFIG_FLAG(RegexTracing))
        {
            UnifiedRegex::DebugWriter* tw = scriptContext->GetRegexDebugWriter();
            tw->Print(L"// REGEX COMPILE ");
            pattern->Print(tw);
            tw->EOL();
        }
        if (REGEX_CONFIG_FLAG(RegexProfile))
            scriptContext->GetRegexStatsDatabase()->BeginProfile();
#endif

        UnifiedRegex::Compiler::Compile
            ( scriptContext
            , ctAllocator
            , rtAllocator
            , standardChars
            , program
            , root
            , parser.GetLitbuf()
            , pattern
#if ENABLE_REGEX_CONFIG_OPTIONS
            , dw
            , stats
#endif
            );

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (REGEX_CONFIG_FLAG(RegexProfile))
            scriptContext->GetRegexStatsDatabase()->EndProfile(stats, UnifiedRegex::RegexStats::Compile);
#endif

        END_TEMP_ALLOCATOR(ctAllocator, scriptContext);
#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::RegexCompilePhase);
#endif

        return pattern;

    }

    // ----------------------------------------------------------------------
    // Primitives
    // ----------------------------------------------------------------------
#if ENABLE_REGEX_CONFIG_OPTIONS


    static void RegexHelperTrace(
        ScriptContext* scriptContext,
        UnifiedRegex::RegexStats::Use use,
        JavascriptRegExp* regExp,
        const wchar_t *const input,
        const CharCount inputLength,
        const wchar_t *const replace = 0,
        const CharCount replaceLength = 0)
    {
        Assert(regExp);
        Assert(input);

        if (REGEX_CONFIG_FLAG(RegexProfile))
        {
            UnifiedRegex::RegexStats* stats =
                scriptContext->GetRegexStatsDatabase()->GetRegexStats(regExp->GetPattern());
            stats->useCounts[use]++;
            stats->inputLength += inputLength;
        }
        if (REGEX_CONFIG_FLAG(RegexTracing))
        {
            UnifiedRegex::DebugWriter* w = scriptContext->GetRegexDebugWriter();
            w->Print(L"%s(", UnifiedRegex::RegexStats::UseNames[use]);
            regExp->GetPattern()->Print(w);
            w->Print(L", ");
            if (!CONFIG_FLAG(Verbose) && inputLength > 1024)
                w->Print(L"\"<string too large>\"");
            else
                w->PrintQuotedString(input, inputLength);
            if (replace != 0)
            {
                Assert(use == UnifiedRegex::RegexStats::Replace);
                w->Print(L", ");
                if (!CONFIG_FLAG(Verbose) && replaceLength > 1024)
                    w->Print(L"\"<string too large>\"");
                else
                    w->PrintQuotedString(replace, replaceLength);
            }
            w->PrintEOL(L");");
            w->Flush();
        }
    }

    static void RegexHelperTrace(ScriptContext* scriptContext, UnifiedRegex::RegexStats::Use use, JavascriptRegExp* regExp, JavascriptString* input)
    {
        Assert(regExp);
        Assert(input);

        RegexHelperTrace(scriptContext, use, regExp, input->GetString(), input->GetLength());
    }

    static void RegexHelperTrace(ScriptContext* scriptContext, UnifiedRegex::RegexStats::Use use, JavascriptRegExp* regExp, JavascriptString* input, JavascriptString* replace)
    {
        Assert(regExp);
        Assert(input);
        Assert(replace);

        RegexHelperTrace(scriptContext, use, regExp, input->GetString(), input->GetLength(), replace->GetString(), replace->GetLength());
    }
#endif

    // ----------------------------------------------------------------------
    // Regex entry points
    // ----------------------------------------------------------------------

    struct RegexMatchState
    {
        const wchar_t* input;
        TempArenaAllocatorObject* tempAllocatorObj;
        UnifiedRegex::Matcher* matcher;
    };

    // String.prototype.match (ES5 15.5.4.10)
    template <bool updateHistory>
    Var RegexHelper::RegexMatchImpl(ScriptContext* scriptContext, JavascriptRegExp *regularExpression, JavascriptString *input, bool noResult, void *const stackAllocationPointer)
    {
        UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();
        const wchar_t* inputStr = input->GetString();
        CharCount inputLength = input->GetLength();

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Match, regularExpression, input);
#endif

        UnifiedRegex::GroupInfo lastSuccessfullMatch; // initially undefined
        UnifiedRegex::GroupInfo lastActualMatch; // initially undefined

#ifdef REGEX_TRIGRAMS
        UnifiedRegex::TrigramAlphabet* trigramAlphabet = scriptContext->GetTrigramAlphabet();
        UnifiedRegex::TrigramInfo* trigramInfo= pattern->rep.unified.trigramInfo;
        if (trigramAlphabet!=NULL && inputLength>=MinTrigramInputLength && trigramInfo!=NULL)
        {
            if (trigramAlphabet->input==NULL)
                trigramAlphabet->MegaMatch((wchar_t*)inputStr,inputLength);

            if (trigramInfo->isTrigramPattern)
            {
                if (trigramInfo->resultCount > 0)
                {
                    lastSuccessfullMatch.offset=trigramInfo->offsets[trigramInfo->resultCount-1];
                    lastSuccessfullMatch.length=UnifiedRegex::TrigramInfo::PatternLength;
                }
                // else: leave lastMatch undefined

                // Make sure a matcher is allocated and holds valid last match in case the RegExp constructor
                // needs to fill-in details from the last match via JavascriptRegExpConstructor::EnsureValues
                Assert(pattern->rep.unified.program != 0);
                if (pattern->rep.unified.matcher == 0)
                    pattern->rep.unified.matcher = UnifiedRegex::Matcher::New(scriptContext, pattern);
                *pattern->rep.unified.matcher->GroupIdToGroupInfo(0) = lastSuccessfullMatch;

                Assert(pattern->IsGlobal());

                JavascriptArray* arrayResult = CreateMatchResult(stackAllocationPointer, scriptContext, /* isGlobal */ true, pattern->NumGroups(), input);
                FinalizeMatchResult(scriptContext, /* isGlobal */ true, arrayResult, lastSuccessfullMatch);

                if (trigramInfo->resultCount > 0)
                {
                    if (trigramInfo->hasCachedResultString)
                    {
                        for (int k = 0; k < trigramInfo->resultCount; k++)
                        {
                            arrayResult->DirectSetItemAt(k, trigramInfo->cachedResult[k]);
                        }
                    }
                    else
                    {
                        for (int k = 0;  k < trigramInfo->resultCount; k++)
                        {
                            JavascriptString * str = SubString::New(input, trigramInfo->offsets[k], UnifiedRegex::TrigramInfo::PatternLength);
                            trigramInfo->cachedResult[k] = str;
                            arrayResult->DirectSetItemAt(k, str);
                        }
                        trigramInfo->hasCachedResultString = true;
                    }
                } // otherwise, there are no results and null will be returned

                if (updateHistory)
                {
                    PropagateLastMatch(scriptContext, /* isGlobal */ true, pattern->IsSticky(), regularExpression, input, lastSuccessfullMatch, lastActualMatch, true, true);
                }

                return lastSuccessfullMatch.IsUndefined() ? scriptContext->GetLibrary()->GetNull() : arrayResult;
            }
        }
#endif

        // If global regex, result array holds substrings for each match, and group bindings are ignored
        // If non-global regex, result array holds overall substring and each group binding substring

        const bool isGlobal = pattern->IsGlobal();
        const bool isSticky = pattern->IsSticky();
        JavascriptArray* arrayResult = 0;
        RegexMatchState state;

        // If global = false and sticky = true, set offset = lastIndex, else set offset = 0
        CharCount offset = 0;
        if (!isGlobal && isSticky)
        {
            offset = regularExpression->GetLastIndex();
        }

        uint32 globalIndex = 0;
        PrimBeginMatch(state, scriptContext, pattern, inputStr, inputLength, false);

        do
        {
            if (offset > inputLength)
            {
                lastActualMatch.Reset();
                break;
            }
            lastActualMatch = PrimMatch(state, scriptContext, pattern, inputLength, offset);

            if (lastActualMatch.IsUndefined())
                break;
            lastSuccessfullMatch = lastActualMatch;
            if (!noResult)
            {
                if (arrayResult == 0)
                    arrayResult = CreateMatchResult(stackAllocationPointer, scriptContext, isGlobal, pattern->NumGroups(), input);
                JavascriptString *const matchedString = SubString::New(input, lastActualMatch.offset, lastActualMatch.length);
                if(isGlobal)
                    arrayResult->DirectSetItemAt(globalIndex, matchedString);
                else
                {
                    // The array's head segment up to length - 1 may not be filled. Write to the head segment element directly
                    // instead of calling a helper that expects the segment to be pre-filled.
                    Assert(globalIndex < arrayResult->GetHead()->length);
                    static_cast<SparseArraySegment<Var> *>(arrayResult->GetHead())->elements[globalIndex] = matchedString;
                }
                globalIndex++;
            }
            offset = lastActualMatch.offset + max(lastActualMatch.length, static_cast<CharCountOrFlag>(1));
        } while (isGlobal);
        PrimEndMatch(state, scriptContext, pattern);
        if(updateHistory)
        {
            PropagateLastMatch(scriptContext, isGlobal, isSticky, regularExpression, input, lastSuccessfullMatch, lastActualMatch, true, true);
        }

        if (arrayResult == 0)
        {
            return scriptContext->GetLibrary()->GetNull();
        }

        const int numGroups = pattern->NumGroups();
        if (!isGlobal)
        {
            if (numGroups > 1)
            {
                // Overall match already captured in index 0 by above, so just grab the groups
                Var nonMatchValue = NonMatchValue(scriptContext, false);
                Var *elements = ((SparseArraySegment<Var>*)arrayResult->GetHead())->elements;
                for (uint groupId = 1; groupId < (uint)numGroups; groupId++)
                {
                    Assert(groupId < arrayResult->GetHead()->left + arrayResult->GetHead()->length);
                    elements[groupId] = GetGroup(scriptContext, pattern, input, nonMatchValue, groupId);
                }
            }
            FinalizeMatchResult(scriptContext, /* isGlobal */ false, arrayResult, lastSuccessfullMatch);
        }
        else
        {
            FinalizeMatchResult(scriptContext, /* isGlobal */ true, arrayResult, lastSuccessfullMatch);
        }

        return arrayResult;
    }
    template Var RegexHelper::RegexMatchImpl<true>(ScriptContext* scriptContext, JavascriptRegExp *regularExpression, JavascriptString *input, bool noResult, void *const stackAllocationPointer);
    template Var RegexHelper::RegexMatchImpl<false>(ScriptContext* scriptContext, JavascriptRegExp *regularExpression, JavascriptString *input, bool noResult, void *const stackAllocationPointer);

    // RegExp.prototype.exec (ES5 15.10.6.2)
    Var RegexHelper::RegexExecImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, bool noResult, void *const stackAllocationPointer)
    {
        UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Exec, regularExpression, input);
#endif

        const bool isGlobal = pattern->IsGlobal();
        const bool isSticky = pattern->IsSticky();
        CharCount offset;
        CharCount inputLength = input->GetLength();
        if (!GetInitialOffset(isGlobal, isSticky, regularExpression, inputLength, offset))
        {
            return scriptContext->GetLibrary()->GetNull();
        }

        UnifiedRegex::GroupInfo match; // initially undefined
        if (offset <= inputLength)
        {
            const wchar_t* inputStr = input->GetString();
            match = SimpleMatch(scriptContext, pattern, inputStr, inputLength, offset);
        }

        // else: match remains undefined
        PropagateLastMatch(scriptContext, isGlobal, isSticky, regularExpression, input, match, match, true, true);

        if (noResult || match.IsUndefined())
        {
            return scriptContext->GetLibrary()->GetNull();
        }

        const int numGroups = pattern->NumGroups();
        Assert(numGroups >= 0);
        JavascriptArray* result = CreateExecResult(stackAllocationPointer, scriptContext, numGroups, input, match);
        Var nonMatchValue = NonMatchValue(scriptContext, false);
        Var *elements = ((SparseArraySegment<Var>*)result->GetHead())->elements;
        for (uint groupId = 0; groupId < (uint)numGroups; groupId++)
        {
            Assert(groupId < result->GetHead()->left + result->GetHead()->length);
            elements[groupId] = GetGroup(scriptContext, pattern, input, nonMatchValue, groupId);
        }
        return result;
    }

    // RegExp.prototype.test (ES5 15.10.6.3)
    BOOL RegexHelper::RegexTest(ScriptContext* scriptContext, JavascriptRegExp *regularExpression, JavascriptString *input)
    {
        UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();
        const wchar_t* inputStr = input->GetString();
        CharCount inputLength = input->GetLength();
        UnifiedRegex::GroupInfo match; // initially undefined

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Test, regularExpression, input);
#endif
        const bool isGlobal = pattern->IsGlobal();
        const bool isSticky = pattern->IsSticky();
        CharCount offset;
        if (!GetInitialOffset(isGlobal, isSticky, regularExpression, inputLength, offset))
            return false;

        if (offset <= inputLength)
        {
            match = SimpleMatch(scriptContext, pattern, inputStr, inputLength, offset);
        }

        // else: match remains undefined
        PropagateLastMatch(scriptContext, isGlobal, isSticky, regularExpression, input, match, match, true, true);

        return !match.IsUndefined();
    }

    void RegexHelper::ReplaceFormatString
        ( ScriptContext* scriptContext
        , UnifiedRegex::RegexPattern* pattern
        , JavascriptString* input
        , UnifiedRegex::GroupInfo match
        , JavascriptString* replace
        , int substitutions
        , __in_ecount(substitutions) CharCount* substitutionOffsets
        , CompoundString::Builder<64 * sizeof(void *) / sizeof(wchar_t)>& concatenated )
    {
        const int numGroups = pattern->NumGroups();
        Var nonMatchValue = NonMatchValue(scriptContext, false);
        const CharCount inputLength = input->GetLength();
        const wchar_t* replaceStr = replace->GetString();
        const CharCount replaceLength = replace->GetLength();

        CharCount offset = 0;
        for (int i = 0; i < substitutions; i++)
        {
            CharCount substitutionOffset = substitutionOffsets[i];
            concatenated.Append(replace, offset, substitutionOffset - offset);
            wchar_t currentChar = replaceStr[substitutionOffset + 1];
            if (currentChar >= L'0' && currentChar <= L'9')
            {
                int captureIndex = (int)(currentChar - L'0');
                offset = substitutionOffset + 2;

                if (offset < replaceLength)
                {
                    currentChar = replaceStr[substitutionOffset + 2];
                    if (currentChar >= L'0' && currentChar <= L'9')
                    {
                        int tempCaptureIndex = (10 * captureIndex) + (int)(currentChar - L'0');
                        if (tempCaptureIndex < numGroups)
                        {
                            captureIndex = tempCaptureIndex;
                            offset = substitutionOffset + 3;
                        }
                    }
                }

                if (captureIndex < numGroups && (captureIndex != 0))
                {
                    Var group = GetGroup(scriptContext, pattern, input, nonMatchValue, captureIndex);
                    if (JavascriptString::Is(group))
                        concatenated.Append(JavascriptString::FromVar(group));
                    else if (group != nonMatchValue)
                        concatenated.Append(replace, substitutionOffset, offset - substitutionOffset);
                }
                else
                    concatenated.Append(replace, substitutionOffset, offset - substitutionOffset);
            }
            else
            {
                switch (currentChar)
                {
                case L'$': // literal '$' character
                    concatenated.Append(L'$');
                    offset = substitutionOffset + 2;
                    break;
                case L'&': // matched string
                    concatenated.Append(input, match.offset, match.length);
                    offset = substitutionOffset + 2;
                    break;
                case L'`': // left context
                    concatenated.Append(input, 0, match.offset);
                    offset = substitutionOffset + 2;
                    break;
                case L'\'': // right context
                    concatenated.Append(input, match.EndOffset(), inputLength - match.EndOffset());
                    offset = substitutionOffset + 2;
                    break;
                default:
                    concatenated.Append(L'$');
                    offset = substitutionOffset + 1;
                    break;
                }
            }
        }
        concatenated.Append(replace, offset, replaceLength - offset);
    }

    int RegexHelper::GetReplaceSubstitutions(const wchar_t * const replaceStr, CharCount const replaceLength,
        ArenaAllocator * const tempAllocator, CharCount** const substitutionOffsetsOut)
    {
        int substitutions = 0;

        for (CharCount i = 0; i < replaceLength; i++)
        {
            if (replaceStr[i] == L'$')
            {
                if (++i < replaceLength)
                {
                    substitutions++;
                }
            }
        }

        if (substitutions > 0)
        {
            CharCount* substitutionOffsets = AnewArray(tempAllocator, CharCount, substitutions);
            substitutions = 0;
            for (CharCount i = 0; i < replaceLength; i++)
            {
                if (replaceStr[i] == L'$')
                {
                    if (i < (replaceLength - 1))
                    {
#pragma prefast(suppress:26000, "index doesn't overflow the buffer")
                        substitutionOffsets[substitutions] = i;
                        i++;
                        substitutions++;
                    }
                }
            }
            *substitutionOffsetsOut = substitutionOffsets;
        }

        return substitutions;
    }
    // String.prototype.replace, replace value has been converted to a string (ES5 15.5.4.11)
    Var RegexHelper::RegexReplaceImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace, JavascriptString* options, bool noResult)
    {
        UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();
        const wchar_t* replaceStr = replace->GetString();
        CharCount replaceLength = replace->GetLength();
        const wchar_t* inputStr = input->GetString();
        CharCount inputLength = input->GetLength();

        JavascriptString* newString = nullptr;

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Replace, regularExpression, input, replace);
#endif

        RegexMatchState state;
        PrimBeginMatch(state, scriptContext, pattern, inputStr, inputLength, true);

        UnifiedRegex::GroupInfo lastActualMatch;
        UnifiedRegex::GroupInfo lastSuccessfullMatch;
        const bool isGlobal = pattern->IsGlobal();
        const bool isSticky = pattern->IsSticky();

        // If global = false and sticky = true, set offset = lastIndex, else set offset = 0
        CharCount offset = 0;
        if (!isGlobal && isSticky)
        {
            offset = regularExpression->GetLastIndex();
        }

        if (!noResult)
        {
            CharCount* substitutionOffsets = nullptr;
            int substitutions = GetReplaceSubstitutions(replaceStr, replaceLength,
                 state.tempAllocatorObj->GetAllocator(), &substitutionOffsets);

            // Use to see if we already have partial result populated in concatenated
            CompoundString::Builder<64 * sizeof(void *) / sizeof(wchar_t)> concatenated(scriptContext);

            // If lastIndex > 0, append input[0..offset] characters to the result
            if (offset > 0)
            {
                concatenated.Append(input, 0, min(offset, inputLength));
            }

            do
            {
                if (offset > inputLength)
                {
                    lastActualMatch.Reset();
                    break;
                }

                lastActualMatch = PrimMatch(state, scriptContext, pattern, inputLength, offset);

                if (lastActualMatch.IsUndefined())
                    break;

                lastSuccessfullMatch = lastActualMatch;
                concatenated.Append(input, offset, lastActualMatch.offset - offset);
                if (substitutionOffsets != 0)
                {
                    ReplaceFormatString(scriptContext, pattern, input, lastActualMatch, replace, substitutions, substitutionOffsets, concatenated);
                }
                else
                {
                    concatenated.Append(replace);
                }
                if (lastActualMatch.length == 0)
                {
                    if (lastActualMatch.offset < inputLength)
                    {
                        concatenated.Append(inputStr[lastActualMatch.offset]);
                    }
                    offset = lastActualMatch.offset + 1;
                }
                else
                {
                    offset = lastActualMatch.EndOffset();
                }
            }
            while (isGlobal);

            if (offset == 0)
            {
                // There was no successful match so the result is the input string.
                newString = input;
            }
            else
            {
                if (offset < inputLength)
                {
                    concatenated.Append(input, offset, inputLength - offset);
                }
                newString = concatenated.ToString();
            }
            substitutionOffsets = 0;
        }
        else
        {
            do
            {
                if (offset > inputLength)
                {
                    lastActualMatch.Reset();
                    break;
                }
                lastActualMatch = PrimMatch(state, scriptContext, pattern, inputLength, offset);

                if (lastActualMatch.IsUndefined())
                    break;

                lastSuccessfullMatch = lastActualMatch;
                offset = lastActualMatch.length == 0? lastActualMatch.offset + 1 : lastActualMatch.EndOffset();
            }
            while (isGlobal);
            newString = scriptContext->GetLibrary()->GetEmptyString();
        }

        PrimEndMatch(state, scriptContext, pattern);
        PropagateLastMatch(scriptContext, isGlobal, isSticky, regularExpression, input, lastSuccessfullMatch, lastActualMatch, true, true);
        return newString;
    }

    // String.prototype.replace, replace value is a function (ES5 15.5.4.11)
    Var RegexHelper::RegexReplaceImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptFunction* replacefn, JavascriptString* options)
    {
        UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();
        const wchar_t* inputStr = input->GetString();
        CharCount inputLength = input->GetLength();
        JavascriptString* newString = nullptr;
        const int numGroups = pattern->NumGroups();
        Var nonMatchValue = NonMatchValue(scriptContext, false);
        UnifiedRegex::GroupInfo lastMatch; // initially undefined

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Replace, regularExpression, input, scriptContext->GetLibrary()->CreateStringFromCppLiteral(L"<replace function>"));
#endif

        RegexMatchState state;
        PrimBeginMatch(state, scriptContext, pattern, inputStr, inputLength, false);

        // NOTE: These must be kept out of the scope of the try below!
        const bool isGlobal = pattern->IsGlobal();
        const bool isSticky = pattern->IsSticky();

        // If global = true, set lastIndex to 0 in case it is used in replacefn
        if (isGlobal)
        {
            regularExpression->SetLastIndex(0);
        }

        // If global = false and sticky = true, set offset = lastIndex, else set offset = 0
        CharCount offset = 0;
        if (!isGlobal && isSticky)
        {
            offset = regularExpression->GetLastIndex();
        }

        CompoundString::Builder<64 * sizeof(void *) / sizeof(wchar_t)> concatenated(scriptContext);
        UnifiedRegex::GroupInfo lastActualMatch;
        UnifiedRegex::GroupInfo lastSuccessfullMatch;

        // Replace function must be called with arguments (<function's this>, group0, ..., groupn, offset, input)
        // The garbage collector must know about this array since it is being passed back into script land
        Var* replaceArgs;
        PROBE_STACK(scriptContext, (numGroups + 3) * sizeof(Var));
        replaceArgs = (Var*)_alloca((numGroups + 3) * sizeof(Var));
        replaceArgs[0] = scriptContext->GetLibrary()->GetUndefined();
        replaceArgs[numGroups + 2] = input;

        if (offset > 0)
        {
            concatenated.Append(input, 0, min(offset, inputLength));
        }

        do
        {
            if (offset > inputLength)
            {
                lastActualMatch.Reset();
                break;
            }

            lastActualMatch = PrimMatch(state, scriptContext, pattern, inputLength, offset);

            if (lastActualMatch.IsUndefined())
                break;

            lastSuccessfullMatch = lastActualMatch;
            for (int groupId = 0;  groupId < numGroups; groupId++)
                replaceArgs[groupId + 1] = GetGroup(scriptContext, pattern, input, nonMatchValue, groupId);
#pragma prefast(suppress:6386, "The write index numGroups + 1 is in the bound")
            replaceArgs[numGroups + 1] = JavascriptNumber::ToVar(lastActualMatch.offset, scriptContext);

            // The called function must see the global state updated by the current match
            // (Should the function reach into a RegExp field, the pattern will still be valid, thus there's no
            //  danger of the primitive regex matcher being re-entered)

            // WARNING: We go off into script land here, which way in turn invoke a regex operation, even on the
            //          same regex.
            JavascriptString* replace = JavascriptConversion::ToString(replacefn->CallFunction(Arguments(CallInfo((ushort)(numGroups + 3)), replaceArgs)), scriptContext);
            concatenated.Append(input, offset, lastActualMatch.offset - offset);
            concatenated.Append(replace);
            if (lastActualMatch.length == 0)
            {
                if (lastActualMatch.offset < inputLength)
                {
                    concatenated.Append(inputStr[lastActualMatch.offset]);
                }
                offset = lastActualMatch.offset + 1;
            }
            else
            {
                offset = lastActualMatch.EndOffset();
            }
        }
        while (isGlobal);

        PrimEndMatch(state, scriptContext, pattern);

        if (offset == 0)
        {
            // There was no successful match so the result is the input string.
            newString = input;
        }
        else
        {
            if (offset < inputLength)
            {
                concatenated.Append(input, offset, inputLength - offset);
            }
            newString = concatenated.ToString();
        }

        PropagateLastMatch(scriptContext, isGlobal, isSticky, regularExpression, input, lastSuccessfullMatch, lastActualMatch, true, true);
        return newString;
    }

    Var RegexHelper::StringReplace(JavascriptString* match, JavascriptString* input, JavascriptString* replace)
    {
        CharCount matchedIndex = JavascriptString::strstr(input, match, true);
        if (matchedIndex == CharCountFlag)
        {
            return input;
        }

        const wchar_t *const replaceStr = replace->GetString();

        // Unfortunately, due to the possibility of there being $ escapes, we can't just wmemcpy the replace string. Check if we
        // have a small replace string that we can quickly scan for '$', to see if we can just wmemcpy.
        bool definitelyNoEscapes = replace->GetLength() == 0;
        if(!definitelyNoEscapes && replace->GetLength() <= 8)
        {
            CharCount i = 0;
            for(; i < replace->GetLength() && replaceStr[i] != L'$'; ++i);
            definitelyNoEscapes = i >= replace->GetLength();
        }

        if(definitelyNoEscapes)
        {
            const wchar_t* inputStr = input->GetString();
            const wchar_t* prefixStr = inputStr;
            CharCount prefixLength = (CharCount)matchedIndex;
            const wchar_t* postfixStr = inputStr + prefixLength + match->GetLength();
            CharCount postfixLength = input->GetLength() - prefixLength - match->GetLength();
            CharCount newLength = prefixLength + postfixLength + replace->GetLength();
            BufferStringBuilder bufferString(newLength, match->GetScriptContext());
            bufferString.SetContent(prefixStr, prefixLength,
                                    replaceStr, replace->GetLength(),
                                    postfixStr, postfixLength);
            return bufferString.ToString();
        }

        CompoundString::Builder<64 * sizeof(void *) / sizeof(wchar_t)> concatenated(input->GetScriptContext());

        // Copy portion of input string that precedes the matched substring
        concatenated.Append(input, 0, matchedIndex);

        // Copy the replace string with substitutions
        CharCount i = 0, j = 0;
        for(; j < replace->GetLength(); ++j)
        {
            if(replaceStr[j] == L'$' && j + 1 < replace->GetLength())
            {
                switch(replaceStr[j + 1])
                {
                    case L'$': // literal '$'
                        ++j;
                        concatenated.Append(replace, i, j - i);
                        i = j + 1;
                        break;

                    case L'&': // matched substring
                        concatenated.Append(replace, i, j - i);
                        concatenated.Append(match);
                        ++j;
                        i = j + 1;
                        break;

                    case L'`': // portion of input string that precedes the matched substring
                        concatenated.Append(replace, i, j - i);
                        concatenated.Append(input, 0, matchedIndex);
                        ++j;
                        i = j + 1;
                        break;

                    case L'\'': // portion of input string that follows the matched substring
                        concatenated.Append(replace, i, j - i);
                        concatenated.Append(
                            input,
                            matchedIndex + match->GetLength(),
                            input->GetLength() - matchedIndex - match->GetLength());
                        ++j;
                        i = j + 1;
                        break;

                    default: // take both the initial '$' and the following character literally
                        ++j;
                }
            }
        }
        Assert(i <= j);
        concatenated.Append(replace, i, j - i);

        // Copy portion of input string that follows the matched substring
        concatenated.Append(input, matchedIndex + match->GetLength(), input->GetLength() - matchedIndex - match->GetLength());

        return concatenated.ToString();
    }

    Var RegexHelper::StringReplace(JavascriptString* match, JavascriptString* input, JavascriptFunction* replacefn)
    {
        CharCount indexMatched = JavascriptString::strstr(input, match, true);
        ScriptContext* scriptContext = replacefn->GetScriptContext();
        Assert(match->GetScriptContext() == scriptContext);
        Assert(input->GetScriptContext() == scriptContext);

        if (indexMatched != CharCountFlag)
        {
            Var pThis = scriptContext->GetLibrary()->GetUndefined();
            JavascriptString* replace = JavascriptConversion::ToString(replacefn->GetEntryPoint()(replacefn, 4, pThis, match, JavascriptNumber::ToVar((int)indexMatched, scriptContext), input), scriptContext);
            const wchar_t* inputStr = input->GetString();
            const wchar_t* prefixStr = inputStr;
            CharCount prefixLength = indexMatched;
            const wchar_t* postfixStr = inputStr + prefixLength + match->GetLength();
            CharCount postfixLength = input->GetLength() - prefixLength - match->GetLength();
            CharCount newLength = prefixLength + postfixLength + replace->GetLength();
            BufferStringBuilder bufferString(newLength, match->GetScriptContext());
            bufferString.SetContent(prefixStr, prefixLength,
                                    replace->GetString(), replace->GetLength(),
                                    postfixStr, postfixLength);
            return bufferString.ToString();
        }
        return input;
    }

    void RegexHelper::AppendSubString(ScriptContext* scriptContext, JavascriptArray* ary, CharCount& numElems, JavascriptString* input, CharCount startInclusive, CharCount endExclusive)
    {
        Assert(endExclusive >= startInclusive);
        Assert(endExclusive <= input->GetLength());
        CharCount length = endExclusive - startInclusive;
        if (length == 0)
        {
            ary->DirectSetItemAt(numElems++, scriptContext->GetLibrary()->GetEmptyString());
        }
        else if (length == 1)
            ary->DirectSetItemAt(numElems++, scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(input->GetString()[startInclusive]));
        else
            ary->DirectSetItemAt(numElems++, SubString::New(input, startInclusive, length));
    }

    __inline UnifiedRegex::RegexPattern *RegexHelper::GetSplitPattern(ScriptContext* scriptContext, JavascriptRegExp *regularExpression)
    {
        UnifiedRegex::RegexPattern* splitPattern = regularExpression->GetSplitPattern();
        if (!splitPattern)
        {
            UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();
            bool isSticky = (pattern->GetFlags() & UnifiedRegex::StickyRegexFlag) != 0;
            if (!isSticky)
            {
                splitPattern = pattern;
            }
            else
            {
                // When the sticky flag is present, the pattern will match the input only at
                // the beginning since "lastIndex" is set to 0 before the first iteration.
                // However, for split(), we need to look for the pattern anywhere in the input.
                //
                // One way to handle this is to use the original pattern with the sticky flag and
                // when it fails, move to the next character and retry.
                //
                // Another way, which is implemented here, is to create another pattern without the
                // sticky flag and have it automatically look for itself anywhere in the input. This
                // way, we can also take advantage of the optimizations for the global search (e.g.,
                // the Boyer-Moore string search).

                InternalString source = pattern->GetSource();
                UnifiedRegex::RegexFlags nonStickyFlags =
                    static_cast<UnifiedRegex::RegexFlags>(pattern->GetFlags() & ~UnifiedRegex::StickyRegexFlag);
                splitPattern = CompileDynamic(
                    scriptContext,
                    source.GetBuffer(),
                    source.GetLength(),
                    nonStickyFlags,
                    pattern->IsLiteral());
            }
            regularExpression->SetSplitPattern(splitPattern);
        }

        return splitPattern;
    }

    // String.prototype.split (ES5 15.5.4.14)
    Var RegexHelper::RegexSplitImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit, bool noResult, void *const stackAllocationPointer)
    {
        if (noResult && scriptContext->GetConfig()->SkipSplitOnNoResult())
        {
            // TODO: Fix this so that the side effect for PropagateLastMatch is done
            return scriptContext->GetLibrary()->GetNull();
        }

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Split, regularExpression, input);
#endif

        JavascriptArray* ary = scriptContext->GetLibrary()->CreateArrayOnStack(stackAllocationPointer);

        if (limit == 0)
        {
            // SPECIAL CASE: Zero limit
            return ary;
        }

        UnifiedRegex::RegexPattern *splitPattern = GetSplitPattern(scriptContext, regularExpression);

        const wchar_t* inputStr = input->GetString();
        CharCount inputLength = input->GetLength(); // s in spec
        const int numGroups = splitPattern->NumGroups();
        Var nonMatchValue = NonMatchValue(scriptContext, false);
        UnifiedRegex::GroupInfo lastSuccessfullMatch; // initially undefined

        RegexMatchState state;
        PrimBeginMatch(state, scriptContext, splitPattern, inputStr, inputLength, false);

        if (inputLength == 0)
        {
            // SPECIAL CASE: Empty string
            UnifiedRegex::GroupInfo match = PrimMatch(state, scriptContext, splitPattern, inputLength, 0);
            if (match.IsUndefined())
                ary->DirectSetItemAt(0, input);
            else
                lastSuccessfullMatch = match;
        }
        else
        {
            CharCount numElems = 0;    // i in spec
            CharCount copyOffset = 0;  // p in spec
            CharCount startOffset = 0; // q in spec

            CharCount inputLimit = inputLength;

            while (startOffset < inputLimit)
            {
                UnifiedRegex::GroupInfo match = PrimMatch(state, scriptContext, splitPattern, inputLength, startOffset);

                if (match.IsUndefined())
                    break;

                lastSuccessfullMatch = match;

                if (match.offset >= inputLimit)
                    break;

                startOffset = match.offset;
                CharCount endOffset = match.EndOffset(); // e in spec

                if (endOffset == copyOffset)
                    startOffset++;
                else
                {
                    AppendSubString(scriptContext, ary, numElems, input, copyOffset, startOffset);
                    if (numElems >= limit)
                        break;

                    startOffset = copyOffset = endOffset;

                    for (int groupId = 1; groupId < numGroups; groupId++)
                    {
                        ary->DirectSetItemAt(numElems++, GetGroup(scriptContext, splitPattern, input, nonMatchValue, groupId));
                        if (numElems >= limit)
                            break;
                    }
                }
            }

            if (numElems < limit)
                AppendSubString(scriptContext, ary, numElems, input, copyOffset, inputLength);
        }

        PrimEndMatch(state, scriptContext, splitPattern);
        Assert(!splitPattern->IsSticky());
        PropagateLastMatch
            ( scriptContext
            , splitPattern->IsGlobal()
            , /* isSticky */ false
            , regularExpression
            , input
            , lastSuccessfullMatch
            , UnifiedRegex::GroupInfo()
            , /* updateRegex */ true
            , /* updateCtor */ true
            , /* useSplitPattern */ true );

        return ary;
    }

    UnifiedRegex::GroupInfo
    RegexHelper::SimpleMatch(ScriptContext * scriptContext, UnifiedRegex::RegexPattern * pattern, const wchar_t * input,  CharCount inputLength, CharCount offset)
    {
        RegexMatchState state;
        PrimBeginMatch(state, scriptContext, pattern, input, inputLength, false);
        UnifiedRegex::GroupInfo match = PrimMatch(state, scriptContext, pattern, inputLength, offset);
        PrimEndMatch(state, scriptContext, pattern);
        return match;
    }

    // String.prototype.search (ES5 15.5.4.12)
    Var RegexHelper::RegexSearchImpl(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();
        const wchar_t* inputStr = input->GetString();
        CharCount inputLength = input->GetLength();

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Search, regularExpression, input);
#endif
        UnifiedRegex::GroupInfo match = RegexHelper::SimpleMatch(scriptContext, pattern, inputStr, inputLength, 0);

        PropagateLastMatch(scriptContext, pattern->IsGlobal(), pattern->IsSticky(), regularExpression, input, match, match, false, true);

        return JavascriptNumber::ToVar(match.IsUndefined() ? -1 : (int32)match.offset, scriptContext);
    }

    // String.prototype.split (ES5 15.5.4.14)
    Var RegexHelper::StringSplit(JavascriptString* match, JavascriptString* input, CharCount limit)
    {
        ScriptContext* scriptContext = match->GetScriptContext();
        JavascriptArray* ary;
        CharCount matchLen = match->GetLength();
        if (matchLen == 0)
        {
            CharCount count = min(input->GetLength(), limit);
            ary = scriptContext->GetLibrary()->CreateArray(count);
            const wchar_t * charString = input->GetString();
            for (CharCount i = 0; i < count; i++)
            {
                ary->DirectSetItemAt(i, scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(charString[i]));
            }
        }
        else
        {
            CharCount i = 0;
            CharCount offset = 0;
            ary = scriptContext->GetLibrary()->CreateArray(0);
            while (i < limit)
            {
                CharCount prevOffset = offset;
                offset = JavascriptString::strstr(input, match, false, prevOffset);
                if (offset != CharCountFlag)
                {
                    ary->DirectSetItemAt(i++, SubString::New(input, prevOffset, offset-prevOffset));
                    offset += max(matchLen, static_cast<CharCount>(1));
                    if (offset > input->GetLength())
                        break;
                }
                else
                {
                    ary->DirectSetItemAt(i++, SubString::New(input, prevOffset, input->GetLength() - prevOffset));
                    break;
                }
            }
        }
        return ary;
    }

    bool RegexHelper::IsResultNotUsed(CallFlags flags)
    {
        return !PHASE_OFF1(Js::RegexResultNotUsedPhase) && ((flags & CallFlags_NotUsed) != 0);
    }

    // ----------------------------------------------------------------------
    // Primitives
    // ----------------------------------------------------------------------
    void RegexHelper::PrimBeginMatch(RegexMatchState& state, ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern, const wchar_t* input, CharCount inputLength, bool alwaysNeedAlloc)
    {
        state.input = input;
        if (pattern->rep.unified.matcher == 0)
            pattern->rep.unified.matcher = UnifiedRegex::Matcher::New(scriptContext, pattern);
        if (alwaysNeedAlloc)
            state.tempAllocatorObj = scriptContext->GetTemporaryAllocator(L"RegexUnifiedExecTemp");
        else
            state.tempAllocatorObj = 0;
    }

    UnifiedRegex::GroupInfo
    RegexHelper::PrimMatch(RegexMatchState& state, ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern, CharCount inputLength, CharCount offset)
    {
        Assert(pattern->rep.unified.program != 0);
        Assert(pattern->rep.unified.matcher != 0);
#if ENABLE_REGEX_CONFIG_OPTIONS
        UnifiedRegex::RegexStats* stats = 0;
        if (REGEX_CONFIG_FLAG(RegexProfile))
        {
            stats = scriptContext->GetRegexStatsDatabase()->GetRegexStats(pattern);
            scriptContext->GetRegexStatsDatabase()->BeginProfile();
        }
        UnifiedRegex::DebugWriter* w = 0;
        if (REGEX_CONFIG_FLAG(RegexTracing) && CONFIG_FLAG(Verbose))
            w = scriptContext->GetRegexDebugWriter();
#endif

        pattern->rep.unified.matcher->Match
            (state.input
                , inputLength
                , offset
                , scriptContext
#if ENABLE_REGEX_CONFIG_OPTIONS
                , stats
                , w
#endif
                );

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (REGEX_CONFIG_FLAG(RegexProfile))
            scriptContext->GetRegexStatsDatabase()->EndProfile(stats, UnifiedRegex::RegexStats::Execute);
#endif
        return pattern->GetGroup(0);
    }

    void RegexHelper::PrimEndMatch(RegexMatchState& state, ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern)
    {
        if (state.tempAllocatorObj != 0)
            scriptContext->ReleaseTemporaryAllocator(state.tempAllocatorObj);
    }

    Var RegexHelper::NonMatchValue(ScriptContext* scriptContext, bool isGlobalCtor)
    {
        // SPEC DEVIATION: The $n properties of the RegExp ctor use empty strings rather than undefined to represent
        //                 the non-match value, even in ES5 mode.
        if (isGlobalCtor)
            return scriptContext->GetLibrary()->GetEmptyString();
        else
            return scriptContext->GetLibrary()->GetUndefined();
    }

    Var RegexHelper::GetString(ScriptContext* scriptContext, JavascriptString* input, Var nonMatchValue, UnifiedRegex::GroupInfo group)
    {
        if (group.IsUndefined())
            return nonMatchValue;
        switch (group.length)
        {
        case 0:
            return scriptContext->GetLibrary()->GetEmptyString();
        case 1:
        {
            const wchar_t* inputStr = input->GetString();
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(inputStr[group.offset]);
        }
        case 2:
        {
            const wchar_t* inputStr = input->GetString();
            PropertyString* propString = scriptContext->GetPropertyString2(inputStr[group.offset], inputStr[group.offset + 1]);
            if (propString != 0)
                return propString;
            // fall-through for default
        }
        default:
            return SubString::New(input, group.offset, group.length);
        }
    }

    Var RegexHelper::GetGroup(ScriptContext* scriptContext, UnifiedRegex::RegexPattern* pattern, JavascriptString* input, Var nonMatchValue, int groupId)
    {
        return GetString(scriptContext, input, nonMatchValue, pattern->GetGroup(groupId));
    }

    // ======================================================================
    // Match results propagate into three places:
    //  - The match result array. Generally the array has string entries for the overall match substring,
    //    followed by final bindings for each group, plus the fields:
    //     - 'input': string used in match
    //     - 'index': index of first character of match in input
    //     - 'lastIndex' (IE extension): one plus index of last character of match in input
    //    However, for String.match with a global match, the result is an array of all match results
    //    (ignoring any group bindings). But in IE8 mode we also bind the above fields to that array,
    //    using the results of the last successful primitive match.
    //  - The regular expression object has writable field:
    //     - 'lastIndex': one plus index of last character of last match in last input
    //     - 'lastInput
    //  - (Host extension) The RegExp constructor object has fields:
    //     - '$n': last match substrings, using "" for undefined in all modes
    //     - etc (see JavascriptRegExpConstructorType.cpp)
    //
    // There are also three influences on what gets propagated where and when:
    //  - Whether the regular expression is global
    //  - Whether the primitive operations runs the regular expression until failure (e.g. String.match) or
    //    just once (e.g. RegExp.exec), or use the underlying matching machinery implicitly (e.g. String.split).
    //
    // Here are the rules:
    //  - RegExp is updated for the last *successful* primitive match, except for String.replace.
    //    In particular, for String.match with a global regex, the final failing match *does not* reset RegExp.
    //  - Except for String.search in EC5 mode (which does not update 'lastIndex'), the regular expressions
    //    lastIndex is updated as follows:
    //     - ES5 mode, if a primitive match fails then the regular expression 'lastIndex' is set to 0. In particular,
    //       the final failing primitive match for String.match with a global regex forces 'lastIndex' to be reset.
    //       However, if a primitive match succeeds then the regular expression 'lastIndex' is updated only for
    //       a global regex.
    //       for success. However:
    //        - The last failing match in a String.match with a global regex does NOT reset 'lastIndex'.
    //        - If the regular expression matched empty, the last index is set assuming the pattern actually matched
    //          one input character. This applies even if the pattern matched empty one beyond the end of the string
    //          in a String.match with a global regex (!). For our own sanity, we isolate this particular case
    //          within JavascriptRegExp when setting the lastIndexVar value.
    //  - In all modes, 'lastIndex' determines the starting search index only for global regular expressions.
    //
    // ======================================================================

    void RegexHelper::PropagateLastMatch
        ( ScriptContext* scriptContext
        , bool isGlobal
        , bool isSticky
        , JavascriptRegExp* regularExpression
        , JavascriptString* lastInput
        , UnifiedRegex::GroupInfo lastSuccessfullMatch
        , UnifiedRegex::GroupInfo lastActualMatch
        , bool updateRegex
        , bool updateCtor
        , bool useSplitPattern )
    {
        if (updateRegex)
        {
            PropagateLastMatchToRegex(scriptContext, isGlobal, isSticky, regularExpression, lastSuccessfullMatch, lastActualMatch);
        }
        if (updateCtor)
        {
            PropagateLastMatchToCtor(scriptContext, regularExpression, lastInput, lastSuccessfullMatch, useSplitPattern);
        }
    }

    void RegexHelper::PropagateLastMatchToRegex
        ( ScriptContext* scriptContext
        , bool isGlobal
        , bool isSticky
        , JavascriptRegExp* regularExpression
        , UnifiedRegex::GroupInfo lastSuccessfullMatch
        , UnifiedRegex::GroupInfo lastActualMatch )
    {
        if (lastActualMatch.IsUndefined())
        {
            regularExpression->SetLastIndex(0);
        }
        else if (isGlobal || isSticky)
        {
            CharCount lastIndex = lastActualMatch.EndOffset();
            Assert(lastIndex <= MaxCharCount);
            regularExpression->SetLastIndex((int32)lastIndex);
        }
    }

    void RegexHelper::PropagateLastMatchToCtor
        ( ScriptContext* scriptContext
        , JavascriptRegExp* regularExpression
        , JavascriptString* lastInput
        , UnifiedRegex::GroupInfo lastSuccessfullMatch
        , bool useSplitPattern )
    {
        Assert(lastInput);

        if (!lastSuccessfullMatch.IsUndefined())
        {
            // Notes:
            // - SPEC DEVIATION: The RegExp ctor holds some details of the last successful match on any regular expression.
            // - For updating regex ctor's stats we are using entry function's context, rather than regex context,
            //   the rational is: use same context of RegExp.prototype, on which the function was called.
            //   So, if you call the function with remoteContext.regexInstance.exec.call(localRegexInstance, "match string"),
            //   we will update stats in the context related to the exec function, i.e. remoteContext.
            //   This is consistent with other browsers
            UnifiedRegex::RegexPattern* pattern = useSplitPattern
                ? regularExpression->GetSplitPattern()
                : regularExpression->GetPattern();
            scriptContext->GetLibrary()->GetRegExpConstructor()->SetLastMatch(pattern, lastInput, lastSuccessfullMatch);
        }
    }

    bool RegexHelper::GetInitialOffset(bool isGlobal, bool isSticky, JavascriptRegExp* regularExpression, CharCount inputLength, CharCount& offset)
    {
        if (isGlobal || isSticky)
        {
            offset = regularExpression->GetLastIndex();
            if (offset <= MaxCharCount)
                return true;
            else
            {
                regularExpression->SetLastIndex(0);
                return false;
            }
        }
        else
        {
            offset = 0;
            return true;
        }
    }

    JavascriptArray* RegexHelper::CreateMatchResult(void *const stackAllocationPointer, ScriptContext* scriptContext, bool isGlobal, int numGroups, JavascriptString* input)
    {
        if (isGlobal)
        {
            // Use an ordinary array, with default initial capacity
            return scriptContext->GetLibrary()->CreateArrayOnStack(stackAllocationPointer);
        }
        else
            return JavascriptRegularExpressionResult::Create(stackAllocationPointer, numGroups, input, scriptContext);
    }

    void RegexHelper::FinalizeMatchResult(ScriptContext* scriptContext, bool isGlobal, JavascriptArray* arr, UnifiedRegex::GroupInfo match)
    {
        if (!isGlobal)
            JavascriptRegularExpressionResult::SetMatch(arr, match);
        // else: arr is an ordinary array
    }

    JavascriptArray* RegexHelper::CreateExecResult(void *const stackAllocationPointer, ScriptContext* scriptContext, int numGroups, JavascriptString* input, UnifiedRegex::GroupInfo match)
    {
        JavascriptArray* res = JavascriptRegularExpressionResult::Create(stackAllocationPointer, numGroups, input, scriptContext);
        JavascriptRegularExpressionResult::SetMatch(res, match);
        return res;
    }

    template<bool mustMatchEntireInput>
    BOOL RegexHelper::RegexTest_NonScript(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, const wchar_t *const input, const CharCount inputLength)
    {
        // This version of the function should only be used when testing the regex against a non-javascript string. That is,
        // this call was not initiated by script code. Hence, the RegExp constructor is not updated with the last match. If
        // 'mustMatchEntireInput' is true, this function also ignores the global/sticky flag and the lastIndex property, since it tests
        // for a match on the entire input string; in that case, the lastIndex property is not modified.

        UnifiedRegex::RegexPattern* pattern = regularExpression->GetPattern();
        UnifiedRegex::GroupInfo match; // initially undefined

#if ENABLE_REGEX_CONFIG_OPTIONS
        RegexHelperTrace(scriptContext, UnifiedRegex::RegexStats::Test, regularExpression, input, inputLength);
#endif
        const bool isGlobal = pattern->IsGlobal();
        const bool isSticky = pattern->IsSticky();
        CharCount offset;
        if (mustMatchEntireInput)
            offset = 0; // needs to match the entire input, so ignore 'lastIndex' and always start from the beginning
        else if (!GetInitialOffset(isGlobal, isSticky, regularExpression, inputLength, offset))
            return false;

        if (mustMatchEntireInput || offset <= inputLength)
        {
            match = RegexHelper::SimpleMatch(scriptContext, pattern, input, inputLength, offset);
        }
        // else: match remains undefined

        if (!mustMatchEntireInput) // don't update 'lastIndex' when mustMatchEntireInput is true since the global flag is ignored
        {
            PropagateLastMatchToRegex(scriptContext, isGlobal, isSticky, regularExpression, match, match);
        }

        return mustMatchEntireInput ? match.offset == 0 && match.length == inputLength : !match.IsUndefined();
    }

    // explicit instantiation
    template BOOL RegexHelper::RegexTest_NonScript<true>(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, const wchar_t *const input, const CharCount inputLength);
    template BOOL RegexHelper::RegexTest_NonScript<false>(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, const wchar_t *const input, const CharCount inputLength);

    // Asserts if the value needs to be marshaled to target context.
    // Returns the resulting value.
    // This is supposed to be called for result/return value of the RegexXXX functions.
    // static
    template<typename T>
    T RegexHelper::CheckCrossContextAndMarshalResult(T value, ScriptContext* targetContext)
    {
        Assert(targetContext);
        Assert(!CrossSite::NeedMarshalVar(value, targetContext));
        return value;
    }

    Var RegexHelper::RegexMatchResultUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        return RegexHelper::RegexMatch(scriptContext, regularExpression, input, false);
    }

    Var RegexHelper::RegexMatchResultUsedAndMayBeTemp(void *const stackAllocationPointer, ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        return RegexHelper::RegexMatch(scriptContext, regularExpression, input, false, stackAllocationPointer);
    }

    Var RegexHelper::RegexMatchResultNotUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        if (!PHASE_OFF1(Js::RegexResultNotUsedPhase))
        {
            return RegexHelper::RegexMatch(scriptContext, regularExpression, input, true);
        }
        else
        {
            return RegexHelper::RegexMatch(scriptContext, regularExpression, input, false);
        }
    }

    Var RegexHelper::RegexMatch(ScriptContext* entryFunctionContext, JavascriptRegExp *regularExpression, JavascriptString *input, bool noResult, void *const stackAllocationPointer)
    {
        Var result = RegexHelper::RegexMatchImpl<true>(entryFunctionContext, regularExpression, input, noResult, stackAllocationPointer);
        return RegexHelper::CheckCrossContextAndMarshalResult(result, entryFunctionContext);
    }

    Var RegexHelper::RegexMatchNoHistory(ScriptContext* entryFunctionContext, JavascriptRegExp *regularExpression, JavascriptString *input, bool noResult)
    {
        Var result = RegexHelper::RegexMatchImpl<false>(entryFunctionContext, regularExpression, input, noResult);
        return RegexHelper::CheckCrossContextAndMarshalResult(result, entryFunctionContext);
    }

    Var RegexHelper::RegexExecResultUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        return RegexHelper::RegexExec(scriptContext, regularExpression, input, false);
    }

    Var RegexHelper::RegexExecResultUsedAndMayBeTemp(void *const stackAllocationPointer, ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        return RegexHelper::RegexExec(scriptContext, regularExpression, input, false, stackAllocationPointer);
    }

    Var RegexHelper::RegexExecResultNotUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        if (!PHASE_OFF1(Js::RegexResultNotUsedPhase))
        {
            return RegexHelper::RegexExec(scriptContext, regularExpression, input, true);
        }
        else
        {
            return RegexHelper::RegexExec(scriptContext, regularExpression, input, false);
        }
    }

    Var RegexHelper::RegexExec(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, bool noResult, void *const stackAllocationPointer)
    {
        Var result = RegexHelper::RegexExecImpl(entryFunctionContext, regularExpression, input, noResult, stackAllocationPointer);
        return RegexHelper::CheckCrossContextAndMarshalResult(result, entryFunctionContext);
    }

    Var RegexHelper::RegexReplaceResultUsed(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace)
    {
        return RegexHelper::RegexReplace(entryFunctionContext, regularExpression, input, replace, nullptr, false);
    }

    Var RegexHelper::RegexReplaceResultNotUsed(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace)
    {
        if (!PHASE_OFF1(Js::RegexResultNotUsedPhase))
        {
            return RegexHelper::RegexReplace(entryFunctionContext, regularExpression, input, replace, nullptr, true);
        }
        else
        {
            return RegexHelper::RegexReplace(entryFunctionContext, regularExpression, input, replace, nullptr, false);
        }

    }

    Var RegexHelper::RegexReplace(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptString* replace, JavascriptString* options, bool noResult)
    {
        Var result = RegexHelper::RegexReplaceImpl(entryFunctionContext, regularExpression, input, replace, options, noResult);
        return RegexHelper::CheckCrossContextAndMarshalResult(result, entryFunctionContext);
    }

    Var RegexHelper::RegexReplaceFunction(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, JavascriptFunction* replacefn, JavascriptString* options)
    {
        Var result = RegexHelper::RegexReplaceImpl(entryFunctionContext, regularExpression, input, replacefn, options);
        return RegexHelper::CheckCrossContextAndMarshalResult(result, entryFunctionContext);
    }

    Var RegexHelper::RegexSearch(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input)
    {
        Var result = RegexHelper::RegexSearchImpl(entryFunctionContext, regularExpression, input);
        return RegexHelper::CheckCrossContextAndMarshalResult(result, entryFunctionContext);
    }

    Var RegexHelper::RegexSplitResultUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit)
    {
        return RegexHelper::RegexSplit(scriptContext, regularExpression, input, limit, false);
    }

    Var RegexHelper::RegexSplitResultUsedAndMayBeTemp(void *const stackAllocationPointer, ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit)
    {
        return RegexHelper::RegexSplit(scriptContext, regularExpression, input, limit, false, stackAllocationPointer);
    }

    Var RegexHelper::RegexSplitResultNotUsed(ScriptContext* scriptContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit)
    {
        if (!PHASE_OFF1(Js::RegexResultNotUsedPhase))
        {
            return RegexHelper::RegexSplit(scriptContext, regularExpression, input, limit, true);
        }
        else
        {
            return RegexHelper::RegexSplit(scriptContext, regularExpression, input, limit, false);
        }
    }

    Var RegexHelper::RegexSplit(ScriptContext* entryFunctionContext, JavascriptRegExp* regularExpression, JavascriptString* input, CharCount limit, bool noResult, void *const stackAllocationPointer)
    {
        Var result = RegexHelper::RegexSplitImpl(entryFunctionContext, regularExpression, input, limit, noResult, stackAllocationPointer);
        return RegexHelper::CheckCrossContextAndMarshalResult(result, entryFunctionContext);
    }
}
