//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum JavascriptFunctionArgIndex {
        JavascriptFunctionArgIndex_Frame           = -2,
        JavascriptFunctionArgIndex_ArgumentsObject = JavascriptFunctionArgIndex_Frame - Js::Constants::ArgumentLocationOnFrame,
#if _M_IX86 || _M_AMD64
        JavascriptFunctionArgIndex_StackNestedFuncListWithNoArg = JavascriptFunctionArgIndex_Frame - Js::Constants::StackNestedFuncListWithNoArg,
        JavascriptFunctionArgIndex_StackFrameDisplayNoArg = JavascriptFunctionArgIndex_Frame - Js::Constants::StackFrameDisplayWithNoArg,
        JavascriptFunctionArgIndex_StackScopeSlotsNoArg = JavascriptFunctionArgIndex_Frame - Js::Constants::StackScopeSlotsWithNoArg,
#endif
        JavascriptFunctionArgIndex_StackNestedFuncList = JavascriptFunctionArgIndex_Frame - Js::Constants::StackNestedFuncList,
        JavascriptFunctionArgIndex_StackFrameDisplay = JavascriptFunctionArgIndex_Frame - Js::Constants::StackFrameDisplay,
        JavascriptFunctionArgIndex_StackScopeSlots = JavascriptFunctionArgIndex_Frame - Js::Constants::StackScopeSlots,
        JavascriptFunctionArgIndex_Function        = 0,
        JavascriptFunctionArgIndex_CallInfo        = 1,
        JavascriptFunctionArgIndex_This            = 2, /* (hidden) first script arg */
        JavascriptFunctionArgIndex_SecondScriptArg = 3
    };
}
