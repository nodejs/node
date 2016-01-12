//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    RegexPattern::RegexPattern(Js::JavascriptLibrary *const library, Program* program, bool isLiteral)
        : library(library), isLiteral(isLiteral), isShallowClone(false)
    {
        rep.unified.program = program;
        rep.unified.matcher = 0;
        rep.unified.trigramInfo = 0;
    }

    RegexPattern *RegexPattern::New(Js::ScriptContext *scriptContext, Program* program, bool isLiteral)
    {
        return
            RecyclerNewFinalized(
                scriptContext->GetRecycler(),
                RegexPattern,
                scriptContext->GetLibrary(),
                program,
                isLiteral);
    }
    void RegexPattern::Finalize(bool isShutdown)
    {
        if(isShutdown)
            return;

        const auto scriptContext = GetScriptContext();
        if(!scriptContext)
            return;

#if DBG
        if(!isLiteral && !scriptContext->IsClosed())
        {
            const auto source = GetSource();
            RegexPattern *p;
            Assert(
                !GetScriptContext()->GetDynamicRegexMap()->TryGetValue(
                    RegexKey(source.GetBuffer(), source.GetLength(), GetFlags()),
                    &p) ||
                p != this);
        }
#endif

        if(isShallowClone)
            return;

        rep.unified.program->FreeBody(scriptContext->RegexAllocator());
    }

    void RegexPattern::Dispose(bool isShutdown)
    {
    }

    Js::ScriptContext *RegexPattern::GetScriptContext() const
    {
        return library->GetScriptContext();
    }

    Js::InternalString RegexPattern::GetSource() const
    {
        return Js::InternalString(rep.unified.program->source, rep.unified.program->sourceLen);
    }

    RegexFlags RegexPattern::GetFlags() const
    {
        return rep.unified.program->flags;
    }

    int RegexPattern::NumGroups() const
    {
        return rep.unified.program->numGroups;
    }

    bool RegexPattern::IsIgnoreCase() const
    {
        return (rep.unified.program->flags & IgnoreCaseRegexFlag) != 0;
    }

    bool RegexPattern::IsGlobal() const
    {
        return (rep.unified.program->flags & GlobalRegexFlag) != 0;
    }

    bool RegexPattern::IsMultiline() const
    {
        return (rep.unified.program->flags & MultilineRegexFlag) != 0;
    }

    bool RegexPattern::IsUnicode() const
    {
        return GetScriptContext()->GetConfig()->IsES6UnicodeExtensionsEnabled() && (rep.unified.program->flags & UnicodeRegexFlag) != 0;
    }

    bool RegexPattern::IsSticky() const
    {
        return GetScriptContext()->GetConfig()->IsES6RegExStickyEnabled() && (rep.unified.program->flags & StickyRegexFlag) != 0;
    }

    bool RegexPattern::WasLastMatchSuccessful() const
    {
        return rep.unified.matcher != 0 && rep.unified.matcher->WasLastMatchSuccessful();
    }

    GroupInfo RegexPattern::GetGroup(int groupId) const
    {
        Assert(groupId == 0 || WasLastMatchSuccessful());
        Assert(groupId >= 0 && groupId < NumGroups());
        return rep.unified.matcher->GetGroup(groupId);
    }

    RegexPattern *RegexPattern::CopyToScriptContext(Js::ScriptContext *scriptContext)
    {
        // This routine assumes that this instance will outlive the copy, which is the case for copy-on-write,
        // and therefore doesn't copy the immutable parts of the pattern. This should not be confused with a
        // would be CloneToScriptContext which will would clone the immutable parts as well because the lifetime
        // of a clone might be longer than the original.

        RegexPattern *result = UnifiedRegex::RegexPattern::New(scriptContext, rep.unified.program, isLiteral);
        Matcher *matcherClone = rep.unified.matcher ? rep.unified.matcher->CloneToScriptContext(scriptContext, result) : nullptr;
        result->rep.unified.matcher = matcherClone;
        result->isShallowClone = true;
        return result;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void RegexPattern::Print(DebugWriter* w)
    {
        w->Print(L"/");

        Js::InternalString str = GetSource();
        if (str.GetLength() == 0)
            w->Print(L"(?:)");
        else
        {
            for (charcount_t i = 0; i < str.GetLength(); ++i)
            {
                const wchar_t c = str.GetBuffer()[i];
                switch(c)
                {
                case L'/':
                    w->Print(L"\\%lc", c);
                    break;
                case L'\n':
                case L'\r':
                case L'\x2028':
                case L'\x2029':
                    w->PrintEscapedChar(c);
                    break;
                case L'\\':
                    Assert(i + 1 < str.GetLength()); // cannot end in a '\'
                    w->Print(L"\\%lc", str.GetBuffer()[++i]);
                    break;
                default:
                    w->PrintEscapedChar(c);
                    break;
                }
            }
        }
        w->Print(L"/");
        if (IsIgnoreCase())
            w->Print(L"i");
        if (IsGlobal())
            w->Print(L"g");
        if (IsMultiline())
            w->Print(L"m");
        if (IsUnicode())
            w->Print(L"u");
        if (IsSticky())
            w->Print(L"y");
        w->Print(L" /* ");
        w->Print(L", ");
        w->Print(isLiteral ? L"literal" : L"dynamic");
        w->Print(L" */");
    }
#endif
}
