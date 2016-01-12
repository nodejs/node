//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class SubString sealed : public JavascriptString
    {
        void const * originalFullStringReference;          // Only here to prevent recycler to free this buffer.
        SubString(void const * originalFullStringReference, const wchar_t* subString, charcount_t length, ScriptContext *scriptContext);

    protected:
        DEFINE_VTABLE_CTOR(SubString, JavascriptString);
        DECLARE_CONCRETE_STRING_CLASS;

    public:
        static JavascriptString* New(JavascriptString* string, charcount_t start, charcount_t length);
        static JavascriptString* New(const wchar_t* stringStr, charcount_t start, charcount_t length, ScriptContext *scriptContext);
        virtual const wchar_t* GetSz() override;
        virtual void const * GetOriginalStringReference() override;
        virtual size_t GetAllocatedByteCount() const override;
        virtual bool IsSubstring() const override;
    };
}
