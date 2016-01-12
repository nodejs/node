//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum class JavascriptArrayIteratorKind
    {
        Key,
        Value,
        KeyAndValue,
    };

    class JavascriptArrayIterator : public DynamicObject
    {
    private:
        Var                         m_iterableObject;
        int64                       m_nextIndex;
        JavascriptArrayIteratorKind m_kind;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptArrayIterator, DynamicObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptArrayIterator);

    public:
        JavascriptArrayIterator(DynamicType* type, Var iterable, JavascriptArrayIteratorKind kind);

        static bool Is(Var aValue);
        static JavascriptArrayIterator* FromVar(Var aValue);

        class EntryInfo
        {
        public:
            static FunctionInfo Next;
        };

        static Var EntryNext(RecyclableObject* function, CallInfo callInfo, ...);

    public:
        Var GetIteratorObjectForHeapEnum() { return m_iterableObject; }
    };
} // namespace Js
