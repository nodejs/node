//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


interface ICmdLineArgsParser
{
    virtual BSTR GetCurrentString() = 0;
    virtual bool GetCurrentBoolean() = 0;
    virtual int GetCurrentInt() = 0;
};


interface ICustomConfigFlags
{
    virtual void PrintUsage() = 0;
    virtual bool ParseFlag(LPCWSTR flagsString, ICmdLineArgsParser * parser) = 0;
};

