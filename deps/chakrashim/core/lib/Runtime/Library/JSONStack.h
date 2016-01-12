//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JSON
{
    class StrictEqualsObjectComparer
    {
    public:
        static bool Equals(Js::Var x, Js::Var y);
    };

    class JSONStack
    {
    private:
        SList<Js::Var> jsObjectStack; // Consider: key-only dictionary here
        typedef JsUtil::List<Js::Var, ArenaAllocator, false, Js::CopyRemovePolicy,
            SpecializedComparer<Js::Var, JSON::StrictEqualsObjectComparer>::TComparerType> DOMObjectStack;
        DOMObjectStack *domObjectStack;
        ArenaAllocator *alloc;
        Js::ScriptContext *scriptContext;

    public:
        JSONStack(ArenaAllocator *allocator, Js::ScriptContext *context);

        static bool Equals(Js::Var x, Js::Var y);

        bool Has(Js::Var data, bool bJsObject = true) const;
        bool Push(Js::Var data, bool bJsObject = true);
        void Pop(bool bJsObject = true);

    private:
        void EnsuresDomObjectStack(void);
    };
} // namespace JSON
