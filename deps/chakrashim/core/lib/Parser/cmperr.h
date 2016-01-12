//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum
{
#undef LSC_ERROR_MSG
#define LSC_ERROR_MSG(err, name, str) name = MAKE_HR(err),
#include "perrors.h"

#undef LSC_ERROR_MSG
    MWUNUSED_ENUM
};

class ParseExceptionObject
{
public:
    ParseExceptionObject(HRESULT hr) : m_hr(hr) {}
    HRESULT GetError() { return m_hr; }
private:
    HRESULT m_hr;
};

typedef void (*ErrorCallback)(void *data, HRESULT hr);

class ErrHandler
{
public:
    HRESULT m_hr;

    void *m_data;
    ErrorCallback m_callback;

    __declspec(noreturn) void Throw(HRESULT hr);

#if DEBUG
    BOOL fInited;
    ErrHandler()
    { fInited = FALSE; }
#endif //DEBUG
};

