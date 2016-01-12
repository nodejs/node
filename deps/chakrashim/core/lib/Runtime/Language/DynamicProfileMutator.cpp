//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_PROFILE_INFO
#ifdef DYNAMIC_PROFILE_MUTATOR
#include "DynamicProfileMutator.h"

char const * const DynamicProfileMutator::CreateMutatorProcName = STRINGIZE(CREATE_MUTATOR_PROC_NAME);

void
DynamicProfileMutator::Mutate(Js::FunctionBody * functionBody)
{
    Js::ScriptContext * scriptContext = functionBody->GetScriptContext();
    DynamicProfileMutator * dynamicProfileMutator = scriptContext->GetThreadContext()->dynamicProfileMutator;
    if (dynamicProfileMutator != nullptr)
    {
        if (functionBody->dynamicProfileInfo == nullptr)
        {
            functionBody->dynamicProfileInfo = Js::DynamicProfileInfo::New(scriptContext->GetRecycler(), functionBody);
        }

        dynamicProfileMutator->Mutate(functionBody->dynamicProfileInfo);
        // Save the profile information, it will help in case of Crash/Failure
        Js::DynamicProfileInfo::Save(scriptContext);
    }
}

DynamicProfileMutator *
DynamicProfileMutator::GetMutator()
{
    if (!Js::Configuration::Global.flags.IsEnabled(Js::DynamicProfileMutatorFlag))
    {
        return nullptr;
    }

    wchar_t const * dllname = Js::Configuration::Global.flags.DynamicProfileMutatorDll;
    HMODULE hModule = ::LoadLibraryW(dllname);
    if (hModule == nullptr)
    {
        Output::Print(L"ERROR: Unable to load dynamic profile mutator dll %s\n", dllname);
        Js::Throw::FatalInternalError();
    }

    CreateMutatorFunc procAddress = (CreateMutatorFunc)::GetProcAddress(hModule, CreateMutatorProcName);

    if (procAddress == nullptr)
    {
        Output::Print(L"ERROR: Unable to get function %S from dll %s\n", CreateMutatorProcName, dllname);
        Js::Throw::FatalInternalError();
    }

    DynamicProfileMutator * mutator = procAddress();
    if (mutator == nullptr)
    {
        Output::Print(L"ERROR: Failed to create mutator from dll %s\n", dllname);
        Js::Throw::FatalInternalError();
    }
    mutator->Initialize(Js::Configuration::Global.flags.DynamicProfileMutator);
    return mutator;
}

#endif
#endif
