//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct Utf8SourceInfo : public FinalizableObject
    {
        // TODO: Change this to LeafValueDictionary
        typedef JsUtil::SynchronizedDictionary<Js::LocalFunctionId, Js::FunctionBody*, Recycler> FunctionBodyDictionary;
        typedef JsUtil::SynchronizedDictionary<Js::LocalFunctionId, Js::ParseableFunctionInfo*, Recycler> DeferredFunctionsDictionary;

        friend class RemoteUtf8SourceInfo;
        friend class ScriptContext;
    public:
        bool HasSource() const { return !this->sourceHolder->IsEmpty(); }

        LPCUTF8 GetSource(const wchar_t * reason = nullptr) const;
        size_t GetCbLength(const wchar_t * reason = nullptr) const;

        ULONG GetParseFlags()
        {
            return this->parseFlags;
        }

        void SetParseFlags(ULONG parseFlags)
        {
            this->parseFlags = parseFlags;
        }

        ULONG GetByteCodeGenerationFlags()
        {
            return this->byteCodeGenerationFlags;
        }

        void SetByteCodeGenerationFlags(ULONG byteCodeGenerationFlags)
        {
            this->byteCodeGenerationFlags = byteCodeGenerationFlags;
        }

        bool IsInDebugMode()
        {
            return this->debugModeSource != nullptr || this->debugModeSourceIsEmpty;
        }

        // For Hybrid debugging purposes we need to have the source mapped in because chakra may be in a frozen state when the source would be needed.
        void SetInDebugMode(bool inDebugMode)
        {
            AssertMsg(this->sourceHolder != nullptr, "We have no source holder.");
            if (!this->sourceHolder->IsDeferrable())
            {
                //The source should be already loaded;
                AssertMsg(IsInDebugMode(), "Debug mode source should have been loaded as the source holder isn't deferrable.");
                return;
            }

            if (inDebugMode)
            {
                AssertMsg(!IsInDebugMode(), "Debug mode source should have not yet been set.");
                this->debugModeSource = this->sourceHolder->GetSource(L"Entering Debug Mode");
                this->debugModeSourceLength = this->sourceHolder->GetByteLength(L"Entering Debug Mode");
                this->debugModeSourceIsEmpty = !this->HasSource() || this->debugModeSource == nullptr;
                this->EnsureLineOffsetCache();
            }
            else
            {
                // If debugModeSourceIsEmpty is false, that means debugModeSource isn't nullptr or we aren't in debug mode.
                this->debugModeSourceIsEmpty = false;
                this->debugModeSource = nullptr;
                this->debugModeSourceLength = 0;
            }
        }

        void RetrieveSourceText(__out_ecount_full(cchLim - cchMin) LPOLESTR cpText, charcount_t cchMin, charcount_t cchLim) const
        {
            size_t cbMin = GetCbLength(L"Utf8SourceInfo::RetrieveSourceText") == GetCchLength() ? cchMin : utf8::CharacterIndexToByteIndex(GetSource(L"Utf8SourceInfo::RetrieveSourceText"), GetCbLength(L"Utf8SourceInfo::RetrieveSourceText"), cchMin, utf8::doAllowThreeByteSurrogates);
            utf8::DecodeInto(cpText, GetSource(L"Utf8SourceInfo::RetrieveSourceText") + cbMin, cchLim - cchMin, utf8::doAllowThreeByteSurrogates);
        }

        size_t CharacterIndexToByteIndex(charcount_t cchIndex) const
        {
            return cchIndex < m_cchLength ? (GetCbLength(L"CharacterIndexToByteIndex") == m_cchLength ?  cchIndex : utf8::CharacterIndexToByteIndex(this->GetSource(L"CharacterIndexToByteIndex"), GetCbLength(L"CharacterIndexToByteIndex"), cchIndex, utf8::doAllowThreeByteSurrogates)) : m_cchLength;
        }

        charcount_t ByteIndexToCharacterIndex(size_t cbIndex) const
        {
            return cbIndex < GetCbLength(L"CharacterIndexToByteIndex") ? static_cast< charcount_t>(GetCbLength(L"CharacterIndexToByteIndex") == m_cchLength ? cbIndex : utf8::ByteIndexIntoCharacterIndex(this->GetSource(L"CharacterIndexToByteIndex"), cbIndex, utf8::doAllowThreeByteSurrogates)) : static_cast< charcount_t >(GetCbLength(L"CharacterIndexToByteIndex"));
        }

        charcount_t GetCchLength() const
        {
            return m_cchLength;
        }

        void SetCchLength(charcount_t cchLength)
        {
            m_cchLength = cchLength;
        }

        const SRCINFO* GetSrcInfo() const
        {
            return m_srcInfo;
        }

        void EnsureInitialized(int initialFunctionCount);

        // The following 3 functions are on individual functions that exist within this
        // source info- for them to be called, byte code generation should have created
        // the function body in question, in which case, functionBodyDictionary needs to have
        // been initialized
        void SetFunctionBody(FunctionBody * functionBody);
        void RemoveFunctionBody(FunctionBody* functionBodyBeingRemoved);

        // The following functions could get called even if EnsureInitialized hadn't gotten called
        // (Namely in the OOM scenario), so we simply guard against that condition rather than
        // asserting
        int GetFunctionBodyCount() const
        {
            return (this->functionBodyDictionary ? this->functionBodyDictionary->Count() : 0);
        }

        // Gets a thread-unique id for this source info
        // (Behaves the same as the function number)
        uint GetSourceInfoId()
        {
            return m_sourceInfoId;
        }

        void ClearFunctions()
        {
            if (this->functionBodyDictionary)
            {
                this->functionBodyDictionary->Clear();
            }
        }

        bool HasFunctions() const
        {
            return (this->functionBodyDictionary ? this->functionBodyDictionary->Count() > 0 : false);
        }

        template <typename TDelegate>
        void MapFunction(TDelegate mapper) const
        {
            if (this->functionBodyDictionary)
            {
                this->functionBodyDictionary->Map([mapper] (Js::LocalFunctionId, Js::FunctionBody* functionBody) {
                    Assert(functionBody);
                    mapper(functionBody);
                });
            }
        }

        // Get's the first function in the function body dictionary
        // Used if the caller want's any function in this source info
        Js::FunctionBody* GetAnyParsedFunction();

        template <typename TDelegate>
        void MapFunctionUntil(TDelegate mapper) const
        {
            if (this->functionBodyDictionary)
            {
                this->functionBodyDictionary->MapUntil([mapper] (Js::LocalFunctionId, Js::FunctionBody* functionBody) {
                    Assert(functionBody);
                    return mapper(functionBody);
                });
            }
        }
        Js::FunctionBody* FindFunction(Js::LocalFunctionId id) const;

        template <typename TDelegate>
        Js::FunctionBody* FindFunction(TDelegate predicate) const
        {
            Js::FunctionBody* matchedFunctionBody = nullptr;

            // Function body collection could be null if we OOMed
            // during byte code generation but the source info didn't get
            // collected because of a false positive reference- we should
            // skip over these Utf8SourceInfos.
            if (this->functionBodyDictionary)
            {
                this->functionBodyDictionary->Map(
                    [&matchedFunctionBody, predicate] (Js::LocalFunctionId, Js::FunctionBody* functionBody)
                {
                    Assert(functionBody);
                    if (predicate(functionBody)) {
                        matchedFunctionBody = functionBody;
                        return true;
                    }

                    return false;
                });
            }

            return matchedFunctionBody;
        }

        void SetHostBuffer(BYTE * pcszCode);

        bool HasDebugDocument() const
        {
            return m_debugDocument != nullptr;
        }

        void SetDebugDocument(DebugDocument * document)
        {
            Assert(!HasDebugDocument());
            m_debugDocument = document;
        }

        DebugDocument* GetDebugDocument() const
        {
            Assert(HasDebugDocument());
            return m_debugDocument;
        }

        void ClearDebugDocument(bool close = true);

        void SetIsCesu8(bool isCesu8)
        {
            this->m_isCesu8 = isCesu8;
        }

        bool GetIsCesu8() const
        {
            return m_isCesu8;
        }

        DWORD_PTR GetSecondaryHostSourceContext() const
        {
            return m_secondaryHostSourceContext;
        }

        void SetIsLibraryCode()
        {
            m_isLibraryCode = true;
        }

        bool GetIsLibraryCode() const
        {
            return m_isLibraryCode;
        }

        bool GetIsXDomain() const { return m_isXDomain; }
        void SetIsXDomain() { m_isXDomain = true; }

        bool GetIsXDomainString() const { return m_isXDomainString; }
        void SetIsXDomainString() { m_isXDomainString = true; }

        DWORD_PTR GetHostSourceContext() const;
        bool IsDynamic() const;
        SourceContextInfo* GetSourceContextInfo() const;
        void SetSecondaryHostContext(DWORD_PTR hostSourceContext);

        bool IsHostManagedSource() const;

        static int StaticGetHashCode(__in const Utf8SourceInfo* const si)
        {
            return si->GetSourceHolder()->GetHashCode();
        }

        static bool StaticEquals(__in Utf8SourceInfo* s1, __in Utf8SourceInfo* s2)
        {
            if (s1 == nullptr || s2 == nullptr) return false;

            //If the source holders have the same pointer, we are expecting the sources to be equal
            return (s1 == s2) || s1->GetSourceHolder()->Equals(s2->GetSourceHolder());
        }

        virtual void Finalize(bool isShutdown) override { /* nothing */ }
        virtual void Dispose(bool isShutdown) override;
        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

        static Utf8SourceInfo* NewWithHolder(ScriptContext* scriptContext, ISourceHolder* sourceHolder, int32 length, SRCINFO const* srcInfo);
        static Utf8SourceInfo* New(ScriptContext* scriptContext, LPCUTF8 utf8String, int32 length, size_t numBytes, SRCINFO const* srcInfo);
        static Utf8SourceInfo* NewWithNoCopy(ScriptContext* scriptContext, LPCUTF8 utf8String, int32 length, size_t numBytes, SRCINFO const* srcInfo);
        static Utf8SourceInfo* Clone(ScriptContext* scriptContext, const Utf8SourceInfo* sourceinfo);
        static Utf8SourceInfo* CloneNoCopy(ScriptContext* scriptContext, const Utf8SourceInfo* sourceinfo, SRCINFO const * srcInfo);

        ScriptContext * GetScriptContext() const
        {
            return this->m_scriptContext;
        }

        void EnsureLineOffsetCache();
        HRESULT EnsureLineOffsetCacheNoThrow();
        void DeleteLineOffsetCache()
        {
            this->m_lineOffsetCache = nullptr;
        }

        void CreateLineOffsetCache(const JsUtil::LineOffsetCache<Recycler>::LineOffsetCacheItem *items, charcount_t numberOfItems);

        size_t GetLineCount()
        {
            return this->GetLineOffsetCache()->GetLineCount();
        }

        JsUtil::LineOffsetCache<Recycler> *GetLineOffsetCache()
        {
            AssertMsg(this->m_lineOffsetCache != nullptr, "LineOffsetCache wasn't created, EnsureLineOffsetCache should have been called.");
            return m_lineOffsetCache;
        }

        void GetLineInfoForCharPosition(charcount_t charPosition, charcount_t *outLineNumber, charcount_t *outColumn, charcount_t *outLineByteOffset, bool allowSlowLookup = false);

        void GetCharPositionForLineInfo(charcount_t lineNumber, charcount_t *outCharPosition, charcount_t *outByteOffset)
        {
            Assert(outCharPosition != nullptr && outByteOffset != nullptr);
            *outCharPosition = this->m_lineOffsetCache->GetCharacterOffsetForLine(lineNumber, outByteOffset);
        }

        void TrackDeferredFunction(Js::LocalFunctionId functionID, Js::ParseableFunctionInfo *function);
        void StopTrackingDeferredFunction(Js::LocalFunctionId functionID);

        template <class Fn>
        void UndeferGlobalFunctions(Fn fn)
        {
            if (this->m_scriptContext->DoUndeferGlobalFunctions())
            {
                Assert(m_deferredFunctionsInitialized);
                if (m_deferredFunctionsDictionary == nullptr)
                {
                    return;
                }

                DeferredFunctionsDictionary *tmp = this->m_deferredFunctionsDictionary;
                this->m_deferredFunctionsDictionary = nullptr;

                tmp->MapAndRemoveIf(fn);
            }
        }

        ISourceHolder* GetSourceHolder() const
        {
            return sourceHolder;
        }

        bool IsCesu8() const
        {
            return this->m_isCesu8;
        }

        void SetCallerUtf8SourceInfo(Utf8SourceInfo* callerUtf8SourceInfo);
        Utf8SourceInfo* GetCallerUtf8SourceInfo() const;

        bool GetDebugDocumentName(BSTR * sourceName);
    private:

        charcount_t m_cchLength;               // The number of characters encoded in m_utf8Source.
        ISourceHolder* sourceHolder;
        union
        {
            BYTE* m_pHostBuffer;  // Pointer to a host source buffer (null unless this is host code that we need to free)
            Utf8SourceInfo const* m_pOriginalSourceInfo; // Pointer to source info with original source text, created during cloning
        };

        FunctionBodyDictionary* functionBodyDictionary;
        DeferredFunctionsDictionary* m_deferredFunctionsDictionary;

        DebugDocument* m_debugDocument;

        const SRCINFO* m_srcInfo;
        DWORD_PTR m_secondaryHostSourceContext;

        LPCUTF8 debugModeSource;
        size_t debugModeSourceLength;

        ScriptContext* const m_scriptContext;   // Pointer to ScriptContext under which this source info was created

        // Line offset cache used for quickly finding line/column offsets.
        JsUtil::LineOffsetCache<Recycler>* m_lineOffsetCache;

        // Utf8SourceInfo of the caller, used for mapping eval/new Function node to its caller node for debugger
        Utf8SourceInfo* callerUtf8SourceInfo;

        bool m_deferredFunctionsInitialized : 1;
        bool m_isCesu8 : 1;
        bool m_hasHostBuffer : 1;
        bool m_isLibraryCode : 1;           // true, the current source belongs to the internal library code. Used for debug purpose to not show in debugger
        bool m_isXDomain : 1;
        // we found that m_isXDomain could cause regression without CORS, so the new flag is just for callee.caller in window.onerror
        bool m_isXDomainString : 1;
        bool debugModeSourceIsEmpty : 1;

        uint m_sourceInfoId;

        // Various flags preserved for Edit-and-Continue re-compile purpose
        ULONG parseFlags;
        ULONG byteCodeGenerationFlags;

        Utf8SourceInfo(ISourceHolder *sourceHolder, int32 cchLength, SRCINFO const* srcInfo, DWORD_PTR secondaryHostSourceContext, ScriptContext* scriptContext);
    };
}

template <>
struct DefaultComparer<Js::Utf8SourceInfo*>
{
    __inline static bool Equals(Js::Utf8SourceInfo* const& x, Js::Utf8SourceInfo* const& y)
    {
        return Js::Utf8SourceInfo::StaticEquals(x, y);
    }

    __inline static hash_t GetHashCode(Js::Utf8SourceInfo* const& s)
    {
        return Js::Utf8SourceInfo::StaticGetHashCode(s);
    }
};
