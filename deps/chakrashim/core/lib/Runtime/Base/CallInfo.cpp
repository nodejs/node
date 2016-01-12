//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    const ushort CallInfo::ksizeofCount =  24;
    const ushort CallInfo::ksizeofCallFlags = 8;
    const uint CallInfo::kMaxCountArgs = (1 << ksizeofCount) - 1 ;
}
