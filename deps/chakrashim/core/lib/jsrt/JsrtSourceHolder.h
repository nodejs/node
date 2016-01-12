//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class ISourceHolder;
namespace Js
{
    class JsrtSourceHolder sealed : public ISourceHolder
    {
    private:
        enum MapRequestFor { Source = 1, Length = 2 };

        JsSerializedScriptLoadSourceCallback scriptLoadCallback;
        JsSerializedScriptUnloadCallback scriptUnloadCallback;
        JsSourceContext sourceContext;

        utf8char_t const * mappedSource;
        size_t mappedSourceByteLength;
        size_t mappedAllocLength;

        // Wrapper methods with Asserts to ensure that we aren't trying to access unmapped source
        utf8char_t const * GetMappedSource()
        {
            AssertMsg(mappedSource != nullptr, "Our mapped source is nullptr, isSourceMapped (Assert above) should be false.");
            AssertMsg(scriptUnloadCallback != nullptr, "scriptUnloadCallback is null, this means that this object has been finalized.");
            return mappedSource;
        };

        size_t GetMappedSourceLength()
        {
            AssertMsg(mappedSource != nullptr, "Our mapped source is nullptr, isSourceMapped (Assert above) should be false.");
            AssertMsg(scriptUnloadCallback != nullptr, "scriptUnloadCallback is null, this means that this object has been finalized.");
            return mappedSourceByteLength;
        };

        void EnsureSource(MapRequestFor requestedFor, const wchar_t* reasonString);
    public:

        static void ScriptToUtf8(_When_(heapAlloc, _In_opt_) _When_(!heapAlloc, _In_) Js::ScriptContext *scriptContext,
            _In_z_ const wchar_t *script, _Outptr_result_buffer_(*utf8Length) LPUTF8 *utf8Script, _Out_ size_t *utf8Length,
            _Out_ size_t *scriptLength, _Out_opt_ size_t *utf8AllocLength = NULL, _In_ bool heapAlloc = false);

        JsrtSourceHolder(_In_ JsSerializedScriptLoadSourceCallback scriptLoadCallback,
            _In_ JsSerializedScriptUnloadCallback scriptUnloadCallback,
            _In_ JsSourceContext sourceContext) :
            scriptLoadCallback(scriptLoadCallback),
            scriptUnloadCallback(scriptUnloadCallback),
            sourceContext(sourceContext),
            mappedSourceByteLength(0),
            mappedSource(nullptr)
        {
            AssertMsg(scriptLoadCallback != nullptr, "script load callback given is null.");
            AssertMsg(scriptUnloadCallback != nullptr, "script unload callback given is null.");
        };

        virtual bool IsEmpty() override
        {
            return false;
        }

        // Following two methods do not attempt any source mapping
        LPCUTF8 GetSourceUnchecked()
        {
            return this->GetMappedSource();
        }

        // Following two methods are calls to EnsureSource before attempting to get the source
        virtual LPCUTF8 GetSource(const wchar_t* reasonString) override
        {
            this->EnsureSource(MapRequestFor::Source, reasonString);
            return this->GetMappedSource();
        }

        virtual size_t GetByteLength(const wchar_t* reasonString) override
        {
            this->EnsureSource(MapRequestFor::Length, reasonString);
            return this->GetMappedSourceLength();
        }

        virtual void Finalize(bool isShutdown) override;

        virtual void Dispose(bool isShutdown) override
        {
        }

        virtual void Mark(Recycler * recycler) override
        {
        }

        virtual bool Equals(ISourceHolder* other) override
        {
            return this == other ||
                (this->GetByteLength(L"Equal Comparison") == other->GetByteLength(L"Equal Comparison")
                    && (this->GetSource(L"Equal Comparison") == other->GetSource(L"Equal Comparison")
                        || memcmp(this->GetSource(L"Equal Comparison"), other->GetSource(L"Equal Comparison"), this->GetByteLength(L"Equal Comparison")) == 0));
        }

        virtual ISourceHolder* Clone(ScriptContext *scriptContext) override
        {
            return RecyclerNewFinalized(scriptContext->GetRecycler(), JsrtSourceHolder, this->scriptLoadCallback, this->scriptUnloadCallback, this->sourceContext);
        }

        virtual int GetHashCode() override
        {
            LPCUTF8 source = GetSource(L"Hash Code Calculation");
            size_t byteLength = GetByteLength(L"Hash Code Calculation");
            Assert(byteLength < MAXUINT32);
            return JsUtil::CharacterBuffer<utf8char_t>::StaticGetHashCode(source, (charcount_t)byteLength);
        }


        virtual bool IsDeferrable() override
        {
            return !PHASE_OFF1(Js::DeferSourceLoadPhase);
        }
    };
}
