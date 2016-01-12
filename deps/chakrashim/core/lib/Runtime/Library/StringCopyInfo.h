//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template <class T> class LargeStack;

namespace Js
{
    #pragma region StringCopyInfo
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class StringCopyInfo
    {
    private:
        JavascriptString *sourceString;
        wchar_t *destinationBuffer;
    #if DBG
        bool isInitialized;
    #endif

    public:
        StringCopyInfo();
        StringCopyInfo(JavascriptString *const sourceString, _Inout_count_(sourceString->m_charLength) wchar_t *const destinationBuffer);

    public:
        JavascriptString *SourceString() const;
        wchar_t *DestinationBuffer() const;

    private:
        static void InstantiateForceInlinedMembers();
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #pragma endregion

    #pragma region StringCopyInfoStack
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class StringCopyInfoStack
    {
    private:
        ScriptContext *const scriptContext;
        TempArenaAllocatorObject *allocator;
        LargeStack<StringCopyInfo> *stack;

    public:
        StringCopyInfoStack(ScriptContext *const scriptContext);
        ~StringCopyInfoStack();

    public:
        bool IsEmpty();
        void Push(const StringCopyInfo copyInfo);
        const StringCopyInfo Pop();

    private:
        void CreateStack();

        PREVENT_COPY(StringCopyInfoStack);
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #pragma endregion
}
