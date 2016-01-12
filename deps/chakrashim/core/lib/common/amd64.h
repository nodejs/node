//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// ARM-specific macro definitions

#pragma once

#ifndef _M_AMD64
#error Include amd64.h in builds of AMD64 targets only.
#endif

extern "C" VOID amd64_SAVE_REGISTERS(void*);
