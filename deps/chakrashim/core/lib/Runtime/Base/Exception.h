//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    class Exception
    {
        friend JsUtil::ExternalApi;

    private:

        enum {
            ExceptionKind_OutOfMemory,
            ExceptionKind_StackOverflow
        };

        static bool RaiseIfScriptActive(ScriptContext *scriptContext, unsigned kind, PVOID returnAddress = NULL);

        static void RecoverUnusedMemory();
    };

}
