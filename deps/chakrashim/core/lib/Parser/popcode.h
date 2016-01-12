//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

/***************************************************************************
Node operators (indicates semantics of the parse node)
***************************************************************************/
enum OpCode : byte
{
#define PTNODE(nop,sn,pc,nk,ok,json)  nop,
#include "ptlist.h"
    knopLim
};

