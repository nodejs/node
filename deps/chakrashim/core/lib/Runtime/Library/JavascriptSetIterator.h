//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum class JavascriptSetIteratorKind
    {
        Value,
        KeyAndValue,
    };

    class JavascriptSetIterator : public DynamicObject
    {
    private:
        JavascriptSet*                          m_set;
        JavascriptSet::SetDataList::Iterator    m_setIterator;
        JavascriptSetIteratorKind               m_kind;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptSetIterator, DynamicObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptSetIterator);

    public:
        JavascriptSetIterator(DynamicType* type, JavascriptSet* set, JavascriptSetIteratorKind kind);

        static bool Is(Var aValue);
        static JavascriptSetIterator* FromVar(Var aValue);

        class EntryInfo
        {
        public:
            static FunctionInfo Next;
        };

        static Var EntryNext(RecyclableObject* function, CallInfo callInfo, ...);

    public:
        JavascriptSet* GetSetForHeapEnum() { return m_set; }
    };
} // namespace Js
