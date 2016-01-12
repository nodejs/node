//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language\SourceDynamicProfileManager.h"

using namespace Js;

// The general idea is that nextLocalFunctionId is assigned sequentially during parse
// EnsureInitialized does one things:
//  - It ensures that the startup function bit vector on the profile manager has sufficient capacity
// The startup function bitvector might have to be resized when we call this function
void SourceContextInfo::EnsureInitialized()
{
#if ENABLE_PROFILE_INFO
    uint oldFunctionBodyArraySize = (this->sourceDynamicProfileManager ? this->sourceDynamicProfileManager->GetStartupFunctionsLength() : 0);
    if (oldFunctionBodyArraySize >= this->nextLocalFunctionId)
    {
        return;
    }

    // Match the dictionaries resize policy in calculating the amount to grow by
    uint newFunctionBodyCount = max(this->nextLocalFunctionId, UInt32Math::Add(oldFunctionBodyArraySize, oldFunctionBodyArraySize / 3));

    if(sourceDynamicProfileManager)
    {
        sourceDynamicProfileManager->EnsureStartupFunctions(newFunctionBodyCount);
    }
#endif
}

bool SourceContextInfo::IsSourceProfileLoaded() const
{
#if ENABLE_PROFILE_INFO
    return sourceDynamicProfileManager != nullptr && sourceDynamicProfileManager->IsProfileLoaded();
#else
    return false;
#endif
}

SourceContextInfo* SourceContextInfo::Clone(Js::ScriptContext* scriptContext) const
{
    IActiveScriptDataCache* profileCache = NULL;
    
#if ENABLE_PROFILE_INFO
    if (this->sourceDynamicProfileManager != NULL)
    {
        profileCache = this->sourceDynamicProfileManager->GetProfileCache();
    }
#endif

    SourceContextInfo * newSourceContextInfo = scriptContext->GetSourceContextInfo(dwHostSourceContext, profileCache);
    if (newSourceContextInfo == nullptr)
    {
        wchar_t const * oldUrl = this->url;
        wchar_t const * oldSourceMapUrl = this->sourceMapUrl;
        newSourceContextInfo = scriptContext->CreateSourceContextInfo(
            dwHostSourceContext,
            oldUrl,
            oldUrl? wcslen(oldUrl) : 0,
            NULL,
            oldSourceMapUrl,
            oldSourceMapUrl ? wcslen(oldSourceMapUrl) : 0);
        newSourceContextInfo->nextLocalFunctionId = this->nextLocalFunctionId;
        newSourceContextInfo->sourceContextId = this->sourceContextId;
        newSourceContextInfo->EnsureInitialized();
    }
    return newSourceContextInfo;
}
