//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {
    //
    // ScopeInfo is used to persist Scope info of outer functions. When reparsing deferred nested
    // functions, use persisted ScopeInfo to restore outer closures.
    //
    class ScopeInfo
    {
        struct MapSymbolData
        {
            ByteCodeGenerator* byteCodeGenerator;
            FuncInfo* func;
        };

        struct SymbolInfo
        {
            union
            {
                PropertyId propertyId;
                PropertyRecord const* name;
            };
            SymbolType symbolType;
            bool hasFuncAssignment;
            bool isBlockVariable;
        };

    private:
        FunctionBody * const parent;    // link to parent function body
        ScopeInfo* funcExprScopeInfo;   // optional func expr scope info
        ScopeInfo* paramScopeInfo;      // optional param scope info

        BYTE isDynamic : 1;             // isDynamic bit affects how deferredChild access global ref
        BYTE isObject : 1;              // isObject bit affects how deferredChild access closure symbols
        BYTE mustInstantiate : 1;       // the scope must be instantiated as an object/array
        BYTE isCached : 1;              // indicates that local vars and functions are cached across invocations
        BYTE isGlobalEval : 1;
        BYTE areNamesCached : 1;

        Scope *scope;
        int scopeId;
        int symbolCount;                // symbol count in this scope
        SymbolInfo symbols[];           // symbol PropertyIDs, index == sym.scopeSlot

    private:
        ScopeInfo(FunctionBody * parent, int symbolCount)
            : parent(parent), funcExprScopeInfo(nullptr), paramScopeInfo(nullptr), symbolCount(symbolCount), scope(nullptr), areNamesCached(false)
        {
        }

        void SetFuncExprScopeInfo(ScopeInfo* funcExprScopeInfo)
        {
            this->funcExprScopeInfo = funcExprScopeInfo;
        }

        void SetParamScopeInfo(ScopeInfo* paramScopeInfo)
        {
            this->paramScopeInfo = paramScopeInfo;
        }

        void SetSymbolId(int i, PropertyId propertyId)
        {
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].propertyId = propertyId;
        }

        void SetSymbolType(int i, SymbolType symbolType)
        {
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].symbolType = symbolType;
        }

        void SetHasFuncAssignment(int i, bool has)
        {
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].hasFuncAssignment = has;
        }

        void SetIsBlockVariable(int i, bool is)
        {
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isBlockVariable = is;
        }

        void SetPropertyName(int i, PropertyRecord const* name)
        {
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].name = name;
        }

        PropertyId GetSymbolId(int i) const
        {
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].propertyId;
        }

        SymbolType GetSymbolType(int i) const
        {
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].symbolType;
        }

        bool GetHasFuncAssignment(int i)
        {
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].hasFuncAssignment;
        }

        bool GetIsBlockVariable(int i)
        {
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isBlockVariable;
        }

        PropertyRecord const* GetPropertyName(int i)
        {
            Assert(areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].name;
        }

        void SaveSymbolInfo(Symbol* sym, MapSymbolData* mapSymbolData);

        static ScopeInfo* FromParent(FunctionBody* parent);
        static ScopeInfo* FromScope(ByteCodeGenerator* byteCodeGenerator, FunctionBody* parent, Scope* scope, ScriptContext *scriptContext);
        static void SaveParentScopeInfo(FuncInfo* parentFunc, FuncInfo* func);
        static void SaveScopeInfo(ByteCodeGenerator* byteCodeGenerator, FuncInfo* parentFunc, FuncInfo* func);

    public:
        FunctionBody * GetParent() const
        {
            return parent;
        }

        ScopeInfo* GetParentScopeInfo() const
        {
            return parent->GetScopeInfo();
        }

        ScopeInfo* GetFuncExprScopeInfo() const
        {
            return funcExprScopeInfo;
        }

        ScopeInfo* GetParamScopeInfo() const
        {
            return paramScopeInfo;
        }

        void SetScope(Scope *scope)
        {
            this->scope = scope;
        }

        Scope * GetScope() const
        {
            return scope;
        }

        void SetScopeId(int id)
        {
            this->scopeId = id;
        }

        int GetScopeId() const
        {
            return scopeId;
        }

        int GetSymbolCount() const
        {
            return symbolCount;
        }

        bool IsGlobalEval() const
        {
            return isGlobalEval;
        }

        static void SaveScopeInfoForDeferParse(ByteCodeGenerator* byteCodeGenerator, FuncInfo* parentFunc, FuncInfo* func);

        ScopeInfo *CloneFor(ParseableFunctionInfo *body);
        void EnsurePidTracking(ScriptContext* scriptContext);

        void GetScopeInfo(Parser *parser, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, Scope* scope);

        //
        // Turn on capturesAll for a Scope temporarily. Restore old capturesAll when this object
        // goes out of scope.
        //
        class AutoCapturesAllScope
        {
        private:
            Scope* scope;
            bool oldCapturesAll;

        public:
            AutoCapturesAllScope(Scope* scope, bool turnOn);
            ~AutoCapturesAllScope();
            bool OldCapturesAll() const
            {
                return oldCapturesAll;
            }
        };
    };
}
