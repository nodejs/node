//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

#if DEBUG
#include <stdarg.h>
#endif //DEBUG

void ErrHandler::Throw(HRESULT hr)
{
    Assert(fInited);
    Assert(FAILED(hr));
    m_hr = hr;
    throw ParseExceptionObject(hr);
}
