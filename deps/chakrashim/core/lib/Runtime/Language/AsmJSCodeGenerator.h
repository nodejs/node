//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{
    class ScriptContext;
    class AsmJsCodeGenerator
    {
        ScriptContext* mScriptContext;
        CodeGenAllocators* mForegroundAllocators;
        PageAllocator * mPageAllocator;
        AsmJsEncoder    mEncoder;
    public:
        AsmJsCodeGenerator( ScriptContext* scriptContext );
        void CodeGen( FunctionBody* functionBody );

    };
}
#endif
