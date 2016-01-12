//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

BOOL FGetResourceString(long isz, __out_ecount(cchMax) OLECHAR *psz, int cchMax);
BSTR BstrGetResourceString(long isz);
