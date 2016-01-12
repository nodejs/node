//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#include "Library\ProfileString.h"
#include "Library\SingleCharString.h"

namespace Js
{
    CharStringCache::CharStringCache() : charStringCache(nullptr)
    {
        memset(charStringCacheA, 0, sizeof charStringCacheA);
    }

    JavascriptString* CharStringCache::GetStringForCharA(char c)
    {
        AssertMsg(JavascriptString::IsASCII7BitChar(c), "GetStringForCharA must be called with ASCII 7bit chars only");

        PropertyString * str = charStringCacheA[c];
        if (str == nullptr)
        {
            PropertyRecord const * propertyRecord;
            wchar_t wc = c;
            JavascriptLibrary * javascriptLibrary = JavascriptLibrary::FromCharStringCache(this);
            javascriptLibrary->GetScriptContext()->GetOrAddPropertyRecord(&wc, 1, &propertyRecord);
            str = javascriptLibrary->CreatePropertyString(propertyRecord);
            charStringCacheA[c] = str;
        }

        return str;
    }


    JavascriptString* CharStringCache::GetStringForChar(wchar_t c)
    {
#ifdef PROFILE_STRINGS
        StringProfiler::RecordSingleCharStringRequest(JavascriptLibrary::FromCharStringCache(this)->GetScriptContext());
#endif
        if (JavascriptString::IsASCII7BitChar(c))
        {
            return GetStringForCharA(JavascriptString::ToASCII7BitChar(c));
        }

        return GetStringForCharW(c);
    }

    JavascriptString* CharStringCache::GetStringForCharW(wchar_t c)
    {
        Assert(!JavascriptString::IsASCII7BitChar(c));
        JavascriptString* str;
        ScriptContext * scriptContext = JavascriptLibrary::FromCharStringCache(this)->GetScriptContext();
        if (!scriptContext->IsClosed())
        {
            if (charStringCache == nullptr)
            {
                Recycler * recycler = scriptContext->GetRecycler();
                charStringCache = RecyclerNew(recycler, CharStringCacheMap, recycler, 17);
            }
            if (!charStringCache->TryGetValue(c, &str))
            {
                str = SingleCharString::New(c, scriptContext);
                charStringCache->Add(c, str);
            }
        }
        else
        {
            str = SingleCharString::New(c, scriptContext);
        }
        return str;
    }

    JavascriptString* CharStringCache::GetStringForCharSP(codepoint_t c)
    {
        Assert(c >= 0x10000);
        CompileAssert(sizeof(wchar_t) * 2 == sizeof(codepoint_t));
        wchar_t buffer[2];

        Js::NumberUtilities::CodePointAsSurrogatePair(c, buffer, buffer + 1);
        JavascriptString* str = JavascriptString::NewCopyBuffer(buffer, 2, JavascriptLibrary::FromCharStringCache(this)->GetScriptContext());
        // TODO: perhaps do some sort of cache for supplementary characters
        return str;
    }
};
