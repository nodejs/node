//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "core\ICustomConfigFlags.h"
class HostConfigFlags : public ICustomConfigFlags
{
public:
#define FLAG(Type, Name, Desc, Default) \
    Type Name; \
    bool Name##IsEnabled;
#include "HostConfigFlagsList.h"

    static HostConfigFlags flags;
    static LPWSTR* argsVal;
    static int argsCount;
    static void(__stdcall *pfnPrintUsage)();

    static void HandleArgsFlag(int& argc, _Inout_updates_to_(argc, argc) LPWSTR argv[]);

    virtual bool ParseFlag(LPCWSTR flagsString, ICmdLineArgsParser * parser) override;
    virtual void PrintUsage() override;
    static void PrintUsageString();

private:
    int nDummy;
    HostConfigFlags();

    template <typename T>
    void Parse(ICmdLineArgsParser * parser, T * value);
};
