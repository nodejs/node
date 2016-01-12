//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// This class saves the FPU control word, and sets it to the default value.
// The default value will prevent all floating point exceptions other than
// those because of denormal operand from being generated.
// When the instance goes out of scope, the control word will be restored to the original value.
template <bool enabled>
class SmartFPUControlT
{
    static const uint INVALID_FPUCONTROL = (uint)-1;

public:

    SmartFPUControlT();
    ~SmartFPUControlT();

    bool HasErr() const
    {
        return m_err != 0;
    }

    HRESULT GetErr() const
    {
        Assert(HasErr());
        return HRESULT_FROM_WIN32(m_err);
    }

    void RestoreFPUControl();

private:
#if DBG
    uint m_oldFpuControlForConsistencyCheck = INVALID_FPUCONTROL;
#endif
    uint m_oldFpuControl;
    errno_t m_err;
};

// Set and Restore FPU control value by default
typedef SmartFPUControlT<true> SmartFPUControl;
