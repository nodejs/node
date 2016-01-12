//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    //
    // This object throws an error when invoked.
    //
    class ThrowErrorObject : public RecyclableObject
    {
    private:
        JavascriptError* m_error;

    protected:
        ThrowErrorObject(StaticType* type, JavascriptError* error);
        DEFINE_VTABLE_CTOR(ThrowErrorObject, RecyclableObject);

    public:
        static Var DefaultEntryPoint(RecyclableObject* function, CallInfo callInfo, ...);

        static ThrowErrorObject* New(StaticType* type, JavascriptError* error, Recycler* recycler);
        static bool Is(Var aValue);
        static ThrowErrorObject* FromVar(Var aValue);

        static RecyclableObject* CreateThrowTypeErrorObject(ScriptContext* scriptContext, long hCode, PCWSTR varName);
        static RecyclableObject* CreateThrowTypeErrorObject(ScriptContext* scriptContext, long hCode, JavascriptString* varName);

    private:
        typedef JavascriptError* (JavascriptLibrary::*CreateErrorFunc)();
        static RecyclableObject* CreateThrowErrorObject(CreateErrorFunc createError, ScriptContext* scriptContext, long hCode, PCWSTR varName);
    };
}
