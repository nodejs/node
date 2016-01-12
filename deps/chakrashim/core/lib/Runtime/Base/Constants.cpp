//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

using namespace Js;

const wchar_t Constants::AnonymousFunction[] = L"Anonymous function";
const wchar_t Constants::Anonymous[] = L"anonymous";
const wchar_t Constants::Empty[] = L"";
const wchar_t Constants::FunctionCode[] = L"Function code";
const wchar_t Constants::GlobalCode[] = L"Global code";
const wchar_t Constants::EvalCode[] = L"eval code";
const wchar_t Constants::GlobalFunction[] = L"glo";
const wchar_t Constants::UnknownScriptCode[] = L"Unknown script code";

#ifdef _M_AMD64
const PBYTE Constants::StackLimitForScriptInterrupt = (PBYTE)0x7fffffffffffffff;
#else
const PBYTE Constants::StackLimitForScriptInterrupt = (PBYTE)0x7fffffff;
#endif

#pragma warning(push)
#pragma warning(disable:4815) // Allow no storage for zero-sized array at end of NullFrameDisplay struct.
const Js::FrameDisplay Js::NullFrameDisplay = 0;
const Js::FrameDisplay Js::StrictNullFrameDisplay = FrameDisplay(0, true);
#pragma warning(pop)
