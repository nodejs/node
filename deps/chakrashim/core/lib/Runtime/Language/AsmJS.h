//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
// Portions of this file are copyright 2014 Mozilla Foundation, available under the Apache 2.0 license.
//-------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------
// Copyright 2014 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-------------------------------------------------------------------------------------------------------

#pragma once

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{
    struct ExclusiveContext
    {
        ByteCodeGenerator* byteCodeGenerator;
        ScriptContext *scriptContext;
        ExclusiveContext( ByteCodeGenerator *_byteCodeGenerator, ScriptContext * _scriptContext ) :byteCodeGenerator( _byteCodeGenerator ), scriptContext( _scriptContext ){};
    };

    class AsmJSCompiler
    {
    public:
        static bool CheckModule( ExclusiveContext *cx, AsmJSParser &parser, ParseNode *stmtList );
        static bool CheckIdentifier( AsmJsModuleCompiler &m, ParseNode *usepn, PropertyName name );
        static bool CheckModuleLevelName( AsmJsModuleCompiler &m, ParseNode *usepn, PropertyName name );
        static bool CheckFunctionHead( AsmJsModuleCompiler &m, ParseNode *fn, bool isGlobal = true );
        static bool CheckTypeAnnotation( AsmJsModuleCompiler &m, ParseNode *coercionNode, AsmJSCoercion *coercion, ParseNode **coercedExpr = nullptr);
        static bool CheckModuleArgument( AsmJsModuleCompiler &m, ParseNode *arg, PropertyName *name, AsmJsModuleArg::ArgType type);
        static bool CheckModuleArguments( AsmJsModuleCompiler &m, ParseNode *fn );
        static bool CheckModuleGlobals( AsmJsModuleCompiler &m );
        static bool CheckModuleGlobal( AsmJsModuleCompiler &m, ParseNode *var );
        static bool CheckGlobalDotImport( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode );
        static bool CheckNewArrayView( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *newExpr );
        static bool CheckFunction( AsmJsModuleCompiler &m, ParseNode* fncNode );
        static bool CheckFunctionsSequential(AsmJsModuleCompiler &m);
        static bool CheckChangeHeap(AsmJsModuleCompiler &m);
        static bool CheckByteLengthCall(AsmJsModuleCompiler &m, ParseNode * node, ParseNode * newBufferDecl);
        static bool CheckGlobalVariableInitImport( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode, bool isMutable = true );
        static bool CheckGlobalVariableImportExpr(AsmJsModuleCompiler &m, PropertyName varName, AsmJSCoercion coercion, ParseNode *coercedExpr);
        static bool CheckFunctionTables(AsmJsModuleCompiler& m);
        static bool CheckModuleReturn( AsmJsModuleCompiler& m );
        static bool CheckFuncPtrTables( AsmJsModuleCompiler &m );

        static void OutputError(ScriptContext * scriptContext, const wchar * message, ...);
        static void OutputMessage(ScriptContext * scriptContext, const DEBUG_EVENT_INFO_TYPE messageType, const wchar * message, ...);
        static void VOutputMessage(ScriptContext * scriptContext, const DEBUG_EVENT_INFO_TYPE messageType, const wchar * message, va_list argptr);
    public:
        bool static Compile(ExclusiveContext *cx, AsmJSParser parser, ParseNode *stmtList);
    };
}

#endif
