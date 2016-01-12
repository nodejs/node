//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{
#if DBG_DUMP
    FunctionBody* AsmJsJitTemplate::Globals::CurrentEncodingFunction = nullptr;
#endif

    void* AsmJsJitTemplate::InitTemplateData()
    {
        __debugbreak();
        return nullptr;
    }

    void AsmJsJitTemplate::FreeTemplateData(void* userData)
    {
        __debugbreak();
    }
}
#endif
