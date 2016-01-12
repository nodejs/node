//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#ifndef TEMP_DISABLE_ASMJS
namespace Js{

    class ASMLink{
    public:
        static bool CheckParams(ScriptContext* scriptContext, AsmJsModuleInfo* info , const Var stdlib, const Var foreigh, Var bufferView);
    private:
        static bool CheckArrayBuffer(ScriptContext* scriptContext, const Var bufferView, const AsmJsModuleInfo* info);
        static bool CheckStdLib(ScriptContext* scriptContext, const AsmJsModuleInfo* info, const Var stdlib);
        static bool CheckFFI(ScriptContext* scriptContext, AsmJsModuleInfo* info, const Var foreign);
        static bool CheckArrayLibraryMethod(ScriptContext* scriptContext, const Var stdlib, const AsmJSTypedArrayBuiltinFunction arrayBuiltin);
        static bool CheckMathLibraryMethod(ScriptContext* scriptContext, const Var asmMathObject, const AsmJSMathBuiltinFunction mathBuiltin);
    };
}
#endif
