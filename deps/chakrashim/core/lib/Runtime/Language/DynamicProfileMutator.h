//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_PROFILE_INFO
#ifdef DYNAMIC_PROFILE_MUTATOR

#define CREATE_MUTATOR_PROC_NAME CreateDynamicProfileMutator
class DynamicProfileMutator
{
public:
    virtual void Mutate(Js::DynamicProfileInfo * info) = 0;
    virtual void Delete() = 0;
    virtual void Initialize(const wchar_t * options) = 0;

    static void Mutate(Js::FunctionBody * functionBody);
    static DynamicProfileMutator * GetMutator();
    static char const * const CreateMutatorProcName;
    typedef DynamicProfileMutator * (*CreateMutatorFunc)();
};

#endif
#endif
