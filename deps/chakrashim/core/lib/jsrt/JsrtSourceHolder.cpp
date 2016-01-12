//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "JsrtSourceHolder.h"

namespace Js
{
    // Helper function for converting a Unicode script to utf8.
    // If heapAlloc is true the returned buffer must be freed with HeapDelete.
    // Otherwise scriptContext must be provided and GCed object is returned.
    void JsrtSourceHolder::ScriptToUtf8(_When_(heapAlloc, _In_opt_) _When_(!heapAlloc, _In_) Js::ScriptContext *scriptContext,
        _In_z_ const wchar_t *script, _Outptr_result_buffer_(*utf8Length) utf8char_t **utf8Script, _Out_ size_t *utf8Length,
        _Out_ size_t *scriptLength, _Out_opt_ size_t *utf8AllocLength, _In_ bool heapAlloc)
    {
        Assert(utf8Script != nullptr);
        Assert(utf8Length != nullptr);
        Assert(scriptLength != nullptr);

        *utf8Script = nullptr;
        *utf8Length = 0;
        *scriptLength = 0;

        if (utf8AllocLength != nullptr)
        {
            *utf8AllocLength = 0;
        }

        size_t length = wcslen(script);
        if (length > UINT_MAX)
        {
            Js::JavascriptError::ThrowOutOfMemoryError(nullptr);
        }

        size_t cbUtf8Buffer = (length + 1) * 3;
        if (cbUtf8Buffer > UINT_MAX)
        {
            Js::JavascriptError::ThrowOutOfMemoryError(nullptr);
        }

        if (!heapAlloc)
        {
            Assert(scriptContext != nullptr);
            *utf8Script = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), utf8char_t, cbUtf8Buffer);
        }
        else
        {
            *utf8Script = HeapNewArray(utf8char_t, cbUtf8Buffer);
        }

        Assert(length < MAXLONG);
        *utf8Length = utf8::EncodeIntoAndNullTerminate(*utf8Script, script, static_cast<charcount_t>(length));
        *scriptLength = length;

        if (utf8AllocLength != nullptr)
        {
            *utf8AllocLength = cbUtf8Buffer;
        }
    }

    void JsrtSourceHolder::EnsureSource(MapRequestFor requestedFor, const wchar_t* reasonString)
    {
        if (this->mappedSource != nullptr)
        {
            return;
        }

        Assert(scriptLoadCallback != nullptr);
        Assert(this->mappedSource == nullptr);

        const wchar_t *source = nullptr;
        size_t sourceLength = 0;
        utf8char_t *utf8Source = nullptr;
        size_t utf8Length = 0;
        size_t utf8AllocLength = 0;

        if (!scriptLoadCallback(sourceContext, &source))
        {
            // Assume out of memory
            Js::JavascriptError::ThrowOutOfMemoryError(nullptr);
        }

        JsrtSourceHolder::ScriptToUtf8(nullptr, source, &utf8Source, &utf8Length, &sourceLength, &utf8AllocLength, true);

        this->mappedSource = utf8Source;
        this->mappedSourceByteLength = utf8Length;
        this->mappedAllocLength = utf8AllocLength;

        this->scriptLoadCallback = nullptr;

#if ENABLE_DEBUG_CONFIG_OPTIONS
        AssertMsg(reasonString != nullptr, "Reason string for why we are mapping the source was not provided.");
        JS_ETW(EventWriteJSCRIPT_SOURCEMAPPING((uint32)wcslen(reasonString), reasonString, (ushort)requestedFor));
#endif
    }

    void JsrtSourceHolder::Finalize(bool isShutdown)
    {
        if (scriptUnloadCallback == nullptr)
        {
            return;
        }

        scriptUnloadCallback(sourceContext);

        if (this->mappedSource != nullptr)
        {
            HeapDeleteArray(this->mappedAllocLength, this->mappedSource);
            this->mappedSource = nullptr;
        }

        // Don't allow load or unload again after told to unload.
        scriptLoadCallback = nullptr;
        scriptUnloadCallback = nullptr;
        sourceContext = NULL;
    }
};
