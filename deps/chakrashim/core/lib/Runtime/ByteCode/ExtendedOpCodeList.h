//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// NOTE: This file is intended to be "#include" multiple times.  The call site must define the macro
// "DEF_OP" to be executed for each entry.
//
#if !defined(DEF_OP)
#error DEF_OP must be defined before including this file
#endif

// Define the extended byte code opcode range

#define MACRO_EXTEND(opcode, layout, attr) DEF_OP(opcode, layout, attr)
#define MACRO_EXTEND_WMS(opcode, layout, attr) DEF_OP(opcode, layout, OpHasMultiSizeLayout|attr )
#include "OpCodes.h"
