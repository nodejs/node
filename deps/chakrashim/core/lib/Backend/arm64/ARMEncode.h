//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//
// Contains constants and tables used by the encoder.
//
// #include "AssemblyStep.h"

//THUMB2 specific decl
typedef unsigned int ENCODE_32;

//Add more, if required
#define IS_CONST_01FFFFFF(x) (((x) & ~0x01ffffff) == 0)

//Add more, if required
#define IS_CONST_NEG_26(x)   (((x) & ~0x01ffffff) == ~0x01ffffff)

//Add more, if required
#define IS_CONST_INT26(x)    (IS_CONST_01FFFFFF(x) || IS_CONST_NEG_26(x))
