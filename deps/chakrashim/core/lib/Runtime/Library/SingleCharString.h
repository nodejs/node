//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // A JavascriptString containing a single char.
    // Caution: GetString and GetSz return interior pointers.
    // So, if allocated in Recycler memory, these objects must
    // remain pinned to prevent orphaning via interior pointers (GetString, GetSz)
    class SingleCharString sealed : public JavascriptString
    {
    public:
        static SingleCharString* New(wchar_t ch, ScriptContext* scriptContext);
        static SingleCharString* New(wchar_t ch, ScriptContext* scriptContext, ArenaAllocator* arena);

    protected:
        DEFINE_VTABLE_CTOR(SingleCharString, JavascriptString);
        DECLARE_CONCRETE_STRING_CLASS;

    private:
        SingleCharString(wchar_t ch, StaticType * type);
        wchar_t m_buff[2]; // the 2nd is always NULL so that GetSz works
    };

} // namespace Js
