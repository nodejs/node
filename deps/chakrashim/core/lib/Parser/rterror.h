//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//
//  Defines trappable runtime error handling.
//
#include "rterrors_limits.h"

enum rtErrors
{
#define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) name = MAKE_HR(errnum),
#define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) name = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_JSCRIPT, errnum),
#include "rterrors.h"
#undef  RT_PUBLICERROR_MSG
#undef  RT_ERROR_MSG
    MWUNUSED_rtError
};
