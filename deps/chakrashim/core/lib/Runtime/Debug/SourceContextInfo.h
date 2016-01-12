//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
class RemoteSourceContextInfo;
class SourceDynamicProfileManager;
};

//
// This object is created per script source file or dynamic script buffer.
//
class SourceContextInfo
{
public:
    uint sourceContextId;
    Js::LocalFunctionId nextLocalFunctionId;           // Count of functions seen so far

#if DBG
    bool closed;
#endif

    DWORD_PTR dwHostSourceContext;      // Context passed in to ParseScriptText
    bool isHostDynamicDocument;         // will be set to true when current doc is treated dynamic from the host side. (IActiveScriptContext::IsDynamicDocument)

    union
    {
        struct
        {
            wchar_t const * url;            // The url of the file
            wchar_t const * sourceMapUrl;   // The url of the source map, such as actual non-minified source of JS on the server.
        };
        uint      hash;                 // hash for dynamic scripts
    };
#if ENABLE_PROFILE_INFO
    Js::SourceDynamicProfileManager * sourceDynamicProfileManager;
#endif

    void EnsureInitialized();
    bool IsDynamic() const { return dwHostSourceContext == Js::Constants::NoHostSourceContext || isHostDynamicDocument; }
    bool IsSourceProfileLoaded() const;
    SourceContextInfo* Clone(Js::ScriptContext* scriptContext) const;
};
