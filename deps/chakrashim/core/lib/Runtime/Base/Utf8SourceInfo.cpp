//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Debug\DiagProbe.h"
#include "Debug\BreakpointProbe.h"
#include "Debug\DebugDocument.h"

namespace Js
{

    // if m_cchLength < 0 it came from an external source.
    // If m_cbLength > abs(m_cchLength) then m_utf8Source contains non-ASCII (multi-byte encoded) characters.

    Utf8SourceInfo::Utf8SourceInfo(ISourceHolder* mappableSource, int32 cchLength, SRCINFO const* srcInfo, DWORD_PTR secondaryHostSourceContext, ScriptContext* scriptContext) :
        sourceHolder(mappableSource),
        m_cchLength(cchLength),
        m_pOriginalSourceInfo(nullptr),
        m_srcInfo(srcInfo),
        m_secondaryHostSourceContext(secondaryHostSourceContext),
        m_debugDocument(nullptr),
        m_sourceInfoId(scriptContext->GetThreadContext()->NewSourceInfoNumber()),
        m_hasHostBuffer(false),
        m_isCesu8(false),
        m_isLibraryCode(false),
        m_isXDomain(false),
        m_isXDomainString(false),
        m_scriptContext(scriptContext),
        m_lineOffsetCache(nullptr),
        m_deferredFunctionsDictionary(nullptr),
        m_deferredFunctionsInitialized(false),
        debugModeSource(nullptr),
        debugModeSourceIsEmpty(false),
        debugModeSourceLength(0),
        callerUtf8SourceInfo(nullptr)
    {
        if (!sourceHolder->IsDeferrable())
        {
            this->debugModeSource = this->sourceHolder->GetSource(L"Entering Debug Mode");
            this->debugModeSourceLength = this->sourceHolder->GetByteLength(L"Entering Debug Mode");
            this->debugModeSourceIsEmpty = !this->HasSource() || this->debugModeSource == nullptr;
        }
    }

    LPCUTF8 Utf8SourceInfo::GetSource(const wchar_t * reason) const
    {
        AssertMsg(this->sourceHolder != nullptr, "We have no source mapper.");
        if (this->m_scriptContext->IsInDebugMode())
        {
            AssertMsg(this->debugModeSource != nullptr || this->debugModeSourceIsEmpty, "Debug mode source should have been set by this point.");
            return debugModeSource;
        }
        else
        {
            return sourceHolder->GetSource(reason == nullptr ? L"Utf8SourceInfo::GetSource" : reason);
        }
    }

    size_t Utf8SourceInfo::GetCbLength(const wchar_t * reason) const
    {
        AssertMsg(this->sourceHolder != nullptr, "We have no source mapper.");
        if (this->m_scriptContext->IsInDebugMode())
        {
            AssertMsg(this->debugModeSource != nullptr || this->debugModeSourceIsEmpty, "Debug mode source should have been set by this point.");
            return debugModeSourceLength;
        }
        else
        {
            return sourceHolder->GetByteLength(reason == nullptr ? L"Utf8SourceInfo::GetSource" : reason);
        }
    }


    void
    Utf8SourceInfo::Dispose(bool isShutdown)
    {
        ClearDebugDocument();
        this->debugModeSource = nullptr;
        if (this->m_hasHostBuffer)
        {
            PERF_COUNTER_DEC(Basic, ScriptCodeBufferCount);
            HeapFree(GetProcessHeap(), 0 , m_pHostBuffer);
            m_pHostBuffer = nullptr;
        }
    };

    void
    Utf8SourceInfo::SetHostBuffer(BYTE * pcszCode)
    {
        Assert(!this->m_hasHostBuffer);
        Assert(this->m_pHostBuffer == nullptr);
        this->m_hasHostBuffer = true;
        this->m_pHostBuffer = pcszCode;
    }
    enum
    {
        fsiHostManaged = 0x01,
        fsiScriptlet   = 0x02,
        fsiDeferredParse = 0x04
    };

    void Utf8SourceInfo::RemoveFunctionBody(FunctionBody* functionBody)
    {
        Assert(this->functionBodyDictionary);

        const LocalFunctionId functionId = functionBody->GetLocalFunctionId();
        Assert(functionBodyDictionary->Item(functionId) == functionBody);

        functionBodyDictionary->Remove(functionId);
        functionBody->SetIsFuncRegistered(false);
    }

    void Utf8SourceInfo::SetFunctionBody(FunctionBody * functionBody)
    {
        Assert(this->m_scriptContext == functionBody->GetScriptContext());
        Assert(this->functionBodyDictionary);

        // Only register a function body when source info is ready. Note that m_pUtf8Source can still be null for lib script.
        Assert(functionBody->GetSourceIndex() != Js::Constants::InvalidSourceIndex);
        Assert(!functionBody->GetIsFuncRegistered());

        const LocalFunctionId functionId = functionBody->GetLocalFunctionId();
        FunctionBody* oldFunctionBody = nullptr;
        if (functionBodyDictionary->TryGetValue(functionId, &oldFunctionBody)) {
            Assert(oldFunctionBody != functionBody);
            oldFunctionBody->SetIsFuncRegistered(false);
        }

        functionBodyDictionary->Item(functionId, functionBody);
        functionBody->SetIsFuncRegistered(true);
    }

    void Utf8SourceInfo::EnsureInitialized(int initialFunctionCount)
    {
        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();
        Recycler* recycler = threadContext->GetRecycler();

        if (this->functionBodyDictionary == nullptr)
        {
            // This collection is allocated with leaf allocation policy. The references to the function body
            // here does not keep the function alive. However, the functions remove themselves at finalize
            // so if a function actually is in this map, it means that it is alive.
            this->functionBodyDictionary = RecyclerNew(recycler, FunctionBodyDictionary, recycler,
                initialFunctionCount, threadContext->GetEtwRundownCriticalSection());
        }

        if (CONFIG_FLAG(DeferTopLevelTillFirstCall) && !m_deferredFunctionsInitialized)
        {
            Assert(this->m_deferredFunctionsDictionary == nullptr);
            this->m_deferredFunctionsDictionary = RecyclerNew(recycler, DeferredFunctionsDictionary, recycler,
                initialFunctionCount, threadContext->GetEtwRundownCriticalSection());
            m_deferredFunctionsInitialized = true;
        }
    }

    Utf8SourceInfo*
    Utf8SourceInfo::NewWithHolder(ScriptContext* scriptContext, ISourceHolder* sourceHolder, int32 length, SRCINFO const* srcInfo)
    {
        // TODO: make this finalizable? Or have a finalizable version which would HeapDelete the string? Is this needed?
        DWORD_PTR secondaryHostSourceContext = Js::Constants::NoHostSourceContext;
        if (srcInfo->sourceContextInfo->IsDynamic())
        {
            secondaryHostSourceContext = scriptContext->GetThreadContext()->GetDebugManager()->AllocateSecondaryHostSourceContext();
        }

        Recycler * recycler = scriptContext->GetRecycler();
        Utf8SourceInfo* toReturn = RecyclerNewFinalized(recycler,
            Utf8SourceInfo, sourceHolder, length, SRCINFO::Copy(recycler, srcInfo), secondaryHostSourceContext, scriptContext);

        if (scriptContext->IsInDebugMode())
        {
            toReturn->debugModeSource = sourceHolder->GetSource(L"Debug Mode Loading");
            toReturn->debugModeSourceLength = sourceHolder->GetByteLength(L"Debug Mode Loading");
            toReturn->debugModeSourceIsEmpty = toReturn->debugModeSource == nullptr || sourceHolder->IsEmpty();
        }

        return toReturn;
    }

    Utf8SourceInfo*
    Utf8SourceInfo::New(ScriptContext* scriptContext, LPCUTF8 utf8String, int32 length, size_t numBytes, SRCINFO const* srcInfo)
    {
        utf8char_t * newUtf8String = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), utf8char_t, numBytes + 1);
        js_memcpy_s(newUtf8String, numBytes + 1, utf8String, numBytes + 1);
        return NewWithNoCopy(scriptContext, newUtf8String, length, numBytes, srcInfo);
    }

    Utf8SourceInfo*
    Utf8SourceInfo::NewWithNoCopy(ScriptContext* scriptContext, LPCUTF8 utf8String, int32 length, size_t numBytes, SRCINFO const * srcInfo)
    {
        ISourceHolder* sourceHolder = RecyclerNew(scriptContext->GetRecycler(), SimpleSourceHolder, utf8String, numBytes);

        return NewWithHolder(scriptContext, sourceHolder, length, srcInfo);
    }


    Utf8SourceInfo*
    Utf8SourceInfo::Clone(ScriptContext* scriptContext, const Utf8SourceInfo* sourceInfo)
    {
        Utf8SourceInfo* newSourceInfo = Utf8SourceInfo::NewWithHolder(scriptContext, sourceInfo->GetSourceHolder()->Clone(scriptContext), sourceInfo->m_cchLength,
             SRCINFO::Copy(scriptContext->GetRecycler(), sourceInfo->GetSrcInfo()));
        newSourceInfo->m_isXDomain = sourceInfo->m_isXDomain;
        newSourceInfo->m_isXDomainString = sourceInfo->m_isXDomainString;
        newSourceInfo->m_isLibraryCode = sourceInfo->m_isLibraryCode;
        newSourceInfo->SetIsCesu8(sourceInfo->GetIsCesu8());
        newSourceInfo->m_lineOffsetCache = sourceInfo->m_lineOffsetCache;
        if (scriptContext->IsInDebugMode())
        {
            newSourceInfo->SetInDebugMode(true);
        }
        return newSourceInfo;
    }

    Utf8SourceInfo*
    Utf8SourceInfo::CloneNoCopy(ScriptContext* scriptContext, const Utf8SourceInfo* sourceInfo, SRCINFO const* srcInfo)
    {
        Utf8SourceInfo* newSourceInfo = Utf8SourceInfo::NewWithHolder(scriptContext, sourceInfo->GetSourceHolder(), sourceInfo->m_cchLength,
             srcInfo ? srcInfo : sourceInfo->GetSrcInfo());
        newSourceInfo->m_isXDomain = sourceInfo->m_isXDomain;
        newSourceInfo->m_isXDomainString = sourceInfo->m_isXDomainString;
        newSourceInfo->m_isLibraryCode = sourceInfo->m_isLibraryCode;
        newSourceInfo->SetIsCesu8(sourceInfo->GetIsCesu8());
        if (sourceInfo->m_hasHostBuffer)
        {
            // Keep the host buffer alive via the original source info
            newSourceInfo->m_pOriginalSourceInfo = sourceInfo;
        }
        newSourceInfo->EnsureInitialized(sourceInfo->GetFunctionBodyCount());
        return newSourceInfo;
    }

    HRESULT Utf8SourceInfo::EnsureLineOffsetCacheNoThrow()
    {
        HRESULT hr = S_OK;
        // This is a double check, otherwise we would have to have a private function, and add an assert.
        // Basically the outer check is for try/catch, inner check (inside EnsureLineOffsetCache) is for that method as its public.
        if (this->m_lineOffsetCache == nullptr)
        {
            BEGIN_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED
            {
                this->EnsureLineOffsetCache();
            }
            END_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NOASSERT(hr);
        }
        return hr;
    }

    void Utf8SourceInfo::EnsureLineOffsetCache()
    {
        if (this->m_lineOffsetCache == nullptr)
        {
            LPCUTF8 sourceStart = this->GetSource(L"Utf8SourceInfo::AllocateLineOffsetCache");
            LPCUTF8 sourceEnd = sourceStart + this->GetCbLength(L"Utf8SourceInfo::AllocateLineOffsetCache");

            LPCUTF8 sourceAfterBOM = sourceStart;
            charcount_t startChar = FunctionBody::SkipByteOrderMark(sourceAfterBOM /* byref */);
            int64 byteStartOffset = (sourceAfterBOM - sourceStart);

            Recycler* recycler = this->m_scriptContext->GetRecycler();
            this->m_lineOffsetCache = RecyclerNew(recycler, JsUtil::LineOffsetCache<Recycler>, recycler, sourceAfterBOM, sourceEnd, startChar, (int)byteStartOffset);
        }
    }

    Js::FunctionBody* Utf8SourceInfo::FindFunction(Js::LocalFunctionId id) const
    {
        Js::FunctionBody *matchedFunctionBody = nullptr;

        if (this->functionBodyDictionary)
        {
            // Ignore return value - OK if function is not found.
            this->functionBodyDictionary->TryGetValue(id, &matchedFunctionBody);

            if (matchedFunctionBody == nullptr || matchedFunctionBody->IsPartialDeserializedFunction())
            {
                return nullptr;
            }
        }

        return matchedFunctionBody;
    }

    void Utf8SourceInfo::GetLineInfoForCharPosition(charcount_t charPosition, charcount_t *outLineNumber, charcount_t *outColumn, charcount_t *outLineByteOffset, bool allowSlowLookup)
    {
        AssertMsg(this->m_lineOffsetCache != nullptr || allowSlowLookup, "LineOffsetCache wasn't created, EnsureLineOffsetCache should have been called.");
        AssertMsg(outLineNumber != nullptr && outColumn != nullptr && outLineByteOffset != nullptr, "Expected out parameter's can't be a nullptr.");

        charcount_t lineCharOffset = 0;
        int line = 0;
        if (this->m_lineOffsetCache == nullptr)
        {
            LPCUTF8 sourceStart = this->GetSource(L"Utf8SourceInfo::AllocateLineOffsetCache");
            LPCUTF8 sourceEnd = sourceStart + this->GetCbLength(L"Utf8SourceInfo::AllocateLineOffsetCache");

            LPCUTF8 sourceAfterBOM = sourceStart;
            lineCharOffset = FunctionBody::SkipByteOrderMark(sourceAfterBOM /* byref */);
            Assert((sourceAfterBOM - sourceStart) < MAXUINT32);
            charcount_t byteStartOffset = (charcount_t)(sourceAfterBOM - sourceStart);

            line = JsUtil::LineOffsetCache<Recycler>::FindLineForCharacterOffset(sourceAfterBOM, sourceEnd, lineCharOffset, byteStartOffset, charPosition);

            *outLineByteOffset = byteStartOffset;
        }
        else
        {
            line = this->m_lineOffsetCache->GetLineForCharacterOffset(charPosition, &lineCharOffset, outLineByteOffset);
        }

        Assert(charPosition >= lineCharOffset);

        *outLineNumber = line;
        *outColumn = charPosition - lineCharOffset;
    }

    void Utf8SourceInfo::CreateLineOffsetCache(const JsUtil::LineOffsetCache<Recycler>::LineOffsetCacheItem *items, charcount_t numberOfItems)
    {
        AssertMsg(this->m_lineOffsetCache == nullptr, "LineOffsetCache is already initialized!");
        Recycler* recycler = this->m_scriptContext->GetRecycler();
        this->m_lineOffsetCache = RecyclerNew(recycler, JsUtil::LineOffsetCache<Recycler>, recycler, items, numberOfItems);
    }

    DWORD_PTR Utf8SourceInfo::GetHostSourceContext() const
    {
        return m_srcInfo->sourceContextInfo->dwHostSourceContext;
    }

    bool Utf8SourceInfo::IsDynamic() const
    {
        return m_srcInfo->sourceContextInfo->IsDynamic();
    }

    SourceContextInfo* Utf8SourceInfo::GetSourceContextInfo() const
    {
        return this->m_srcInfo->sourceContextInfo;
    }

    // Get's the first function in the function body dictionary
    // Used if the caller want's any function in this source info
    Js::FunctionBody* Utf8SourceInfo::GetAnyParsedFunction()
    {
        if (this->functionBodyDictionary != nullptr && this->functionBodyDictionary->Count() > 0)
        {
            FunctionBody* functionBody = nullptr;
            int i = 0;
            do
            {
                functionBody = this->functionBodyDictionary->GetValueAt(i);
                if (functionBody != nullptr && functionBody->GetByteCode() == nullptr && !functionBody->GetIsFromNativeCodeModule()) functionBody = nullptr;
                i++;
            }
            while (functionBody == nullptr && i < this->functionBodyDictionary->Count());

            return functionBody;
        }

        return nullptr;
    }


    bool Utf8SourceInfo::IsHostManagedSource() const
    {
        return ((this->m_srcInfo->grfsi & fsiHostManaged) == fsiHostManaged);
    }

    void Utf8SourceInfo::SetCallerUtf8SourceInfo(Utf8SourceInfo* callerUtf8SourceInfo)
    {
        this->callerUtf8SourceInfo = callerUtf8SourceInfo;
    }

    Utf8SourceInfo* Utf8SourceInfo::GetCallerUtf8SourceInfo() const
    {
        return this->callerUtf8SourceInfo;
    }

    void Utf8SourceInfo::TrackDeferredFunction(Js::LocalFunctionId functionID, Js::ParseableFunctionInfo *function)
    {
        if (this->m_scriptContext->DoUndeferGlobalFunctions())
        {
            Assert(m_deferredFunctionsInitialized);
            if (this->m_deferredFunctionsDictionary != nullptr)
            {
                this->m_deferredFunctionsDictionary->Add(functionID, function);
            }
        }
    }

    void Utf8SourceInfo::StopTrackingDeferredFunction(Js::LocalFunctionId functionID)
    {
        if (this->m_scriptContext->DoUndeferGlobalFunctions())
        {
            Assert(m_deferredFunctionsInitialized);
            if (this->m_deferredFunctionsDictionary != nullptr)
            {
                this->m_deferredFunctionsDictionary->Remove(functionID);
            }
        }
    }

    void Utf8SourceInfo::ClearDebugDocument(bool close)
    {
        if (this->m_debugDocument != nullptr)
        {
            if (close)
            {
                m_debugDocument->CloseDocument();
            }

            this->m_debugDocument = nullptr;
        }
    }

    bool Utf8SourceInfo::GetDebugDocumentName(BSTR * sourceName)
    {
        if (this->HasDebugDocument() && this->GetDebugDocument()->HasDocumentText())
        {
            // ToDo (SaAgarwa): Fix for JsRT debugging
            IDebugDocumentText *documentText = static_cast<IDebugDocumentText *>(this->GetDebugDocument()->GetDocumentText());
            if (documentText->GetName(DOCUMENTNAMETYPE_URL, sourceName) == S_OK)
            {
                return true;
            }
        }
        return false;
    }
}
