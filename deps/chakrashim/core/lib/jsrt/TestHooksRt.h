//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// This flag will be used in conjunction with JsRuntimeAttributes (defined in chakrart.h)
// However this flag is separately defined in this file since it is only required for ch.exe and we
// don't want to polute the chakrart.h


// Summary : Runtime will generate bytecode buffer by treating current file as library file.

const DWORD JsRuntimeAttributeSerializeLibraryByteCode = 0x8000000;

