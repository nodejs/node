//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// This call binary level provides control to turn on or off a feature.
// A default implementation that enables all features is included in jscript.common.common.lib

// To override, include an object that includes the definition of all the functions in this file
// on the linker command line.  The linker always processes symbols from objects on the command line
// first, thus the override will be chosen instead of the default one.

class BinaryFeatureControl
{
public:
    static bool RecyclerTest();
    static BOOL GetMitigationPolicyForProcess(__in HANDLE hProcess, __in PROCESS_MITIGATION_POLICY MitigationPolicy, __out_bcount(nLength) PVOID lpBuffer, __in SIZE_T nLength);
};
