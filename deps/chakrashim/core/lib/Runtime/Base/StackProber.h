//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class StackProber
{
public:
    void Initialize();
    PBYTE GetScriptStackLimit() const { return stackLimit; }
#if DBG
    void AdjustKnownStackLimit(PBYTE sp, size_t size)
    {
        if (knownStackLimit == 0) knownStackLimit = sp - size;
        knownStackLimit = ((sp - size) < knownStackLimit) ? (sp - size) : knownStackLimit;
    }
#endif

private:
    PBYTE stackLimit;
#if DBG
    PBYTE knownStackLimit;
#endif
};
