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
// Removed code from original location, if the expression is true, check if extra code needed
#define MaybeTodo( expr ) AssertMsg( !(expr), "Unhandled scenario in asm.js" )

namespace Js {
    static const int DOUBLE_SLOTS_SPACE = (sizeof(double) / sizeof(Var)); // 2 in x86 and 1 in x64
    static const double FLOAT_SLOTS_SPACE = (sizeof(float) / (double)sizeof(Var)); // 1 in x86 and 0.5 in x64
    static const double INT_SLOTS_SPACE = ( sizeof( int ) / (double)sizeof( Var ) ); // 1 in x86 and 0.5 in x64
    static const double SIMD_SLOTS_SPACE = (sizeof(SIMDValue) / sizeof(Var)); // 4 in x86 and 2 in x64

    Var AsmJsChangeHeapBuffer(RecyclableObject * function, CallInfo callInfo, ...);
    Var AsmJsExternalEntryPoint(Js::RecyclableObject* entryObject, Js::CallInfo callInfo, ...);
#if _M_X64
    int GetStackSizeForAsmJsUnboxing(ScriptFunction* func);
    void * UnboxAsmJsArguments(ScriptFunction* func, Var * origArgs, char * argDst, CallInfo callInfo);
    Var BoxAsmJsReturnValue(ScriptFunction* func, int intRetVal, double doubleRetVal, float floatRetVal);
#endif

    class AsmJsCompilationException
    {
        wchar_t msg_[256];
    public:
        AsmJsCompilationException( const wchar_t* _msg, ... );
        inline wchar_t* msg() { return msg_; }
    };

    class ParserWrapper
    {
    public:
        static PropertyName FunctionName( ParseNode *node );
        static PropertyName VariableName( ParseNode *node );
        static ParseNode* FunctionArgsList( ParseNode *node, ArgSlot &numformals );
        static ParseNode* NextVar( ParseNode *node );
        static ParseNode* NextInList( ParseNode *node );
        static inline ParseNode *GetListHead( ParseNode *node );
        static inline bool IsNameDeclaration(ParseNode *node);
        static inline bool IsUInt(ParseNode *node);
        static inline uint GetUInt(ParseNode *node);
        static inline bool IsNegativeZero(ParseNode* node);
        static inline bool IsMinInt(ParseNode *node){ return node && node->nop == knopFlt && node->sxFlt.maybeInt && node->sxFlt.dbl == -2147483648.0; };
        static inline bool IsUnsigned(ParseNode *node){ return node && node->nop == knopFlt && node->sxFlt.maybeInt && (((uint32)node->sxFlt.dbl) >> 31); };

        static bool IsDefinition( ParseNode *arg );
        static bool ParseVarOrConstStatement( AsmJSParser &parser, ParseNode **var );
        static inline bool IsNumericLiteral(ParseNode* node) { return node && (node->nop == knopInt || node->nop == knopFlt); }
        static inline bool IsFroundNumericLiteral(ParseNode* node) { return node && (IsNumericLiteral(node) || IsNegativeZero(node)); }
        static inline ParseNode* GetUnaryNode( ParseNode* node ){Assert(IsNodeUnary(node));return node->sxUni.pnode1;}
        static inline ParseNode* GetBinaryLeft( ParseNode* node ){Assert(IsNodeBinary(node));return node->sxBin.pnode1;}
        static inline ParseNode* GetBinaryRight( ParseNode* node ){Assert(IsNodeBinary(node));return node->sxBin.pnode2;}
        static inline ParseNode* DotBase( ParseNode *node );
        static inline bool IsDotMember( ParseNode *node );
        static inline PropertyName DotMember( ParseNode *node );
        // Get the VarDecl from the node or nullptr if unable to find
        static ParseNode* GetVarDeclList(ParseNode* node);
        // Goes through the nodes until the end of the list of VarDecl
        static void ReachEndVarDeclList( ParseNode** node );

        // nop utils
        static inline bool IsNodeBinary    (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & (fnopBin|fnopBinList)); }
        static inline bool IsNodeUnary     (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopUni        ); }
        static inline bool IsNodeConst     (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopConst      ); }
        static inline bool IsNodeLeaf      (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopLeaf       ); }
        static inline bool IsNodeRelational(ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopRel        ); }
        static inline bool IsNodeAssignment(ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopAsg        ); }
        static inline bool IsNodeBreak     (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopBreak      ); }
        static inline bool IsNodeContinue  (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopContinue   ); }
        static inline bool IsNodeCleanUp   (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopCleanup    ); }
        static inline bool IsNodeJump      (ParseNode* pnode){ return pnode && !!(ParseNode::Grfnop(pnode->nop) & fnopJump       ); }
        static inline bool IsNodeExpression(ParseNode* pnode){ return pnode &&  !(ParseNode::Grfnop(pnode->nop) & fnopNotExprStmt); }
    };

    bool ParserWrapper::IsNameDeclaration( ParseNode *node )
    {
        return node->nop == knopName || node->nop == knopStr;
    }

    bool ParserWrapper::IsNegativeZero(ParseNode *node)
    {
        return node && ((node->nop == knopFlt && JavascriptNumber::IsNegZero(node->sxFlt.dbl)) ||
            (node->nop == knopNeg && node->sxUni.pnode1->nop == knopInt && node->sxUni.pnode1->sxInt.lw == 0));
    }

    bool ParserWrapper::IsUInt( ParseNode *node )
    {
        return node->nop == knopInt || IsUnsigned(node);
    }

    uint ParserWrapper::GetUInt( ParseNode *node )
    {
        Assert( IsUInt( node ) );
        if( node->nop == knopInt )
        {
            return (uint)node->sxInt.lw;
        }
        Assert( node->nop == knopFlt );
        return (uint)node->sxFlt.dbl;
    }

    bool ParserWrapper::IsDotMember( ParseNode *node )
    {
        return node && (node->nop == knopDot || node->nop == knopIndex);
    }

    PropertyName ParserWrapper::DotMember( ParseNode *node )
    {
        Assert( IsDotMember(node) );
        if( IsNameDeclaration( GetBinaryRight( node ) ) )
        {
            return GetBinaryRight( node )->name();
        }
        return nullptr;
    }

    ParseNode* ParserWrapper::DotBase( ParseNode *node )
    {
        Assert( IsDotMember( node ) );
        return GetBinaryLeft( node );
    }

    ParseNode * ParserWrapper::GetListHead( ParseNode *node )
    {
        Assert( node->nop == knopList );
        return node->sxBin.pnode1;
    }
};
#endif
