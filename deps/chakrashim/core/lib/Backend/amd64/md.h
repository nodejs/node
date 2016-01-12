//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//
// Machine dependent constants.
//

const int MachInt = 4;
const int MachRegInt = 8;
__declspec(selectany) const int MachPtr = 8;
const int MachDouble = 8;
const int MachRegDouble = 8;
const int MachStackAlignment = MachPtr;
const int MachArgsSlotOffset = MachPtr;
const int MachMaxInstrSize = 12;
const unsigned __int64 MachSignBit = 0x8000000000000000;
const int MachSimd128 = 16;
//
//review:  shouldn't we use _PAGESIZE_ from heap.h instead of hardcoded 8KB.
//
const int PAGESIZE = 2 * 0x1000;
const IRType TyMachReg = TyInt64;
const IRType TyMachPtr = TyUint64;
const IRType TyMachDouble = TyFloat64;
const IRType TyMachSimd128F4 = TySimd128F4;
const IRType TyMachSimd128I4 = TySimd128I4;
const IRType TyMachSimd128D2 = TySimd128D2;

const DWORD EMIT_BUFFER_ALIGNMENT = 16;
const DWORD INSTR_ALIGNMENT = 1;
