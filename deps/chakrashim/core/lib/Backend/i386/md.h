//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//
// Machine dependent constants.
//

const int MachInt = 4;
const int MachRegInt = 4;
__declspec(selectany) const int MachPtr = 4;
const int MachDouble = 8;
const int MachRegDouble = 8;
const int MachMaxInstrSize = 11;
const int MachArgsSlotOffset = MachPtr;
const int MachStackAlignment = MachDouble;
const unsigned int MachSignBit = 0x80000000;
const int MachSimd128 = 16;

const int PAGESIZE = 0x1000;

const IRType TyMachReg = TyInt32;
const IRType TyMachPtr = TyUint32;
const IRType TyMachDouble = TyFloat64;
const IRType TyMachSimd128F4 = TySimd128F4;
const IRType TyMachSimd128I4 = TySimd128I4;
const IRType TyMachSimd128D2 = TySimd128D2;

const DWORD EMIT_BUFFER_ALIGNMENT = 16;
const DWORD INSTR_ALIGNMENT = 1;
