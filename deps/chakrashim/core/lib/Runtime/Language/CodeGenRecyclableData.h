//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class FunctionCodeGenJitTimeData;

    // Keeps data relevant to a function body that is needed for jitting the function, alive until jitting is complete
    class CodeGenRecyclableData sealed : public JsUtil::DoublyLinkedListElement<CodeGenRecyclableData>
    {
    private:
        const FunctionCodeGenJitTimeData *const jitTimeData;

    public:
        CodeGenRecyclableData(const FunctionCodeGenJitTimeData *const jitTimeData);

    public:
        const FunctionCodeGenJitTimeData *JitTimeData() const;

        PREVENT_COPY(CodeGenRecyclableData);
    };
}
