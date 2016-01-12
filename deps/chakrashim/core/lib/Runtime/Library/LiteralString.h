//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class LiteralString : public JavascriptString
    {
    protected:
        LiteralString(StaticType* type);
        LiteralString(StaticType* type, const wchar_t* content, charcount_t charLength);
        DEFINE_VTABLE_CTOR(LiteralString, JavascriptString);
        DECLARE_CONCRETE_STRING_CLASS;

    public:
        static LiteralString* New(StaticType* type, const wchar_t* content, charcount_t charLength, Recycler* recycler);
        static LiteralString* CreateEmptyString(StaticType* type);
    };

    class ArenaLiteralString sealed : public JavascriptString
    {
    protected:
        ArenaLiteralString(StaticType* type, const wchar_t* content, charcount_t charLength);
        DEFINE_VTABLE_CTOR(ArenaLiteralString, JavascriptString);
        DECLARE_CONCRETE_STRING_CLASS;

    public:
        static ArenaLiteralString* New(StaticType* type, const wchar_t* content, charcount_t charLength, Recycler* recycler);
        static ArenaLiteralString* New(StaticType* type, const wchar_t* content, charcount_t charLength, ArenaAllocator* arena);
        virtual RecyclableObject * CloneToScriptContext(ScriptContext* requestContext) override;
    };
}
