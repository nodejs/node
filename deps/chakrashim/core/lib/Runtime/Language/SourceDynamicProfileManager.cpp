//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_PROFILE_INFO
#ifdef ENABLE_WININET_PROFILE_DATA_CACHE
#include "activscp_private.h"
#endif
namespace Js
{
    ExecutionFlags
    SourceDynamicProfileManager::IsFunctionExecuted(Js::LocalFunctionId functionId)
    {
        if (cachedStartupFunctions == nullptr || cachedStartupFunctions->Length() <= functionId)
        {
            return ExecutionFlags_HasNoInfo;
        }
        return (ExecutionFlags)cachedStartupFunctions->Test(functionId);
    }

    DynamicProfileInfo *
    SourceDynamicProfileManager::GetDynamicProfileInfo(FunctionBody * functionBody)
    {
        Js::LocalFunctionId functionId = functionBody->GetLocalFunctionId();
        DynamicProfileInfo * dynamicProfileInfo = nullptr;
        if (dynamicProfileInfoMap.Count() > 0 && dynamicProfileInfoMap.TryGetValue(functionId, &dynamicProfileInfo))
        {
            if (dynamicProfileInfo->MatchFunctionBody(functionBody))
            {
                return dynamicProfileInfo;
            }

#if DBG_DUMP
            if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::DynamicProfilePhase))
            {
                Output::Print(L"TRACE: DynamicProfile: Profile data rejected for function %d in %s\n",
                    functionId, functionBody->GetSourceContextInfo()->url);
                Output::Flush();
            }
#endif
            // NOTE: We have profile mismatch, we can invalidate all other profile here.
        }
        return nullptr;
    }

    void
    SourceDynamicProfileManager::Reset(uint numberOfFunctions)
    {
        dynamicProfileInfoMap.Clear();
    }

    void SourceDynamicProfileManager::UpdateDynamicProfileInfo(LocalFunctionId functionId, DynamicProfileInfo * dynamicProfileInfo)
    {
        Assert(dynamicProfileInfo != nullptr);

        dynamicProfileInfoMap.Item(functionId, dynamicProfileInfo);
    }

    void SourceDynamicProfileManager::MarkAsExecuted(LocalFunctionId functionId)
    {
        Assert(startupFunctions != nullptr);
        Assert(functionId <= startupFunctions->Length());
        startupFunctions->Set(functionId);
    }

    void SourceDynamicProfileManager::EnsureStartupFunctions(uint numberOfFunctions)
    {
        Assert(numberOfFunctions != 0);
        if(!startupFunctions || numberOfFunctions > startupFunctions->Length())
        {
            BVFixed* oldStartupFunctions = this->startupFunctions;
            startupFunctions = BVFixed::New(numberOfFunctions, this->GetRecycler());
            if(oldStartupFunctions)
            {
                this->startupFunctions->Copy(oldStartupFunctions);
            }
        }
    }

    //
    // Enables re-use of profile managers across script contexts - on every re-use the
    // previous script contexts list of startup functions are transferred over as input to this new script context.
    //
    void SourceDynamicProfileManager::Reuse()
    {
        AssertMsg(profileDataCache == nullptr, "Persisted profiles cannot be re-used");
        cachedStartupFunctions = startupFunctions;
    }

    //
    // Loads the profile from the WININET cache
    //
    bool SourceDynamicProfileManager::LoadFromProfileCache(IActiveScriptDataCache* profileDataCache, LPCWSTR url)
    {
    #ifdef ENABLE_WININET_PROFILE_DATA_CACHE
        AssertMsg(CONFIG_FLAG(WininetProfileCache), "Profile caching should be enabled for us to get here");
        Assert(profileDataCache);
        AssertMsg(!IsProfileLoadedFromWinInet(), "Duplicate profile cache loading?");

        // Keep a copy of this and addref it
        profileDataCache->AddRef();
        this->profileDataCache = profileDataCache;

        IStream* readStream;
        HRESULT hr = profileDataCache->GetReadDataStream(&readStream);
        if(SUCCEEDED(hr))
        {
            Assert(readStream != nullptr);
            // stream reader owns the stream and will close it on destruction
            SimpleStreamReader streamReader(readStream);
            DWORD jscriptMajorVersion;
            DWORD jscriptMinorVersion;
            if(FAILED(AutoSystemInfo::GetJscriptFileVersion(&jscriptMajorVersion, &jscriptMinorVersion)))
            {
                return false;
            }

            DWORD majorVersion;
            if(!streamReader.Read(&majorVersion) || majorVersion != jscriptMajorVersion)
            {
                return false;
            }

            DWORD minorVersion;
            if(!streamReader.Read(&minorVersion) || minorVersion != jscriptMinorVersion)
            {
                return false;
            }

            uint numberOfFunctions;
            if(!streamReader.Read(&numberOfFunctions) || numberOfFunctions > MAX_FUNCTION_COUNT)
            {
                return false;
            }
            BVFixed* functions = BVFixed::New(numberOfFunctions, this->recycler);
            if(!streamReader.ReadArray(functions->GetData(), functions->WordCount()))
            {
                return false;
            }
            this->cachedStartupFunctions = functions;
            OUTPUT_TRACE(Js::DynamicProfilePhase, L"Profile load succeeded. Function count: %d  %s\n", numberOfFunctions, url);
#if DBG_DUMP
            if(PHASE_TRACE1(Js::DynamicProfilePhase) && Js::Configuration::Global.flags.Verbose)
            {
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, L"Profile loaded:\n");
                functions->Dump();
            }
#endif
            return true;
        }
        else if (hr == HRESULT_FROM_WIN32(ERROR_WRITE_PROTECT))
        {
            this->isNonCachableScript = true;
            OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, L"Profile load failed. Non-cacheable resource. %s\n", url);
        }
        else
        {
            OUTPUT_TRACE(Js::DynamicProfilePhase, L"Profile load failed. No read stream. %s\n", url);
        }
#endif
        return false;
    }

    //
    // Saves the profile to the WININET cache and returns the bytes written
    //
    uint SourceDynamicProfileManager::SaveToProfileCacheAndRelease(SourceContextInfo* info)
    {
        uint bytesWritten = 0;
#ifdef ENABLE_WININET_PROFILE_DATA_CACHE
        if(profileDataCache)
        {
            if(ShouldSaveToProfileCache(info))
            {
                OUTPUT_TRACE(Js::DynamicProfilePhase, L"Saving profile. Number of functions: %d Url: %s...\n", startupFunctions->Length(), info->url);

                bytesWritten = SaveToProfileCache();

                if(bytesWritten == 0)
                {
                    OUTPUT_TRACE(Js::DynamicProfilePhase, L"Profile saving FAILED\n");
                }
            }

            profileDataCache->Release();
            profileDataCache = nullptr;
        }
#endif
        return bytesWritten;
    }

    //
    // Saves the profile to the WININET cache
    //
    uint SourceDynamicProfileManager::SaveToProfileCache()
    {
        AssertMsg(CONFIG_FLAG(WininetProfileCache), "Profile caching should be enabled for us to get here");
        Assert(startupFunctions);

        uint bytesWritten = 0;
#ifdef ENABLE_WININET_PROFILE_DATA_CACHE
        //TODO: Add some diffing logic to not write unless necessary
        IStream* writeStream;
        HRESULT hr = profileDataCache->GetWriteDataStream(&writeStream);
        if(FAILED(hr))
        {
            return 0;
        }
        Assert(writeStream != nullptr);
        // stream writer owns the stream and will close it on destruction
        SimpleStreamWriter streamWriter(writeStream);

        DWORD jscriptMajorVersion;
        DWORD jscriptMinorVersion;
        if(FAILED(AutoSystemInfo::GetJscriptFileVersion(&jscriptMajorVersion, &jscriptMinorVersion)))
        {
            return 0;
        }

        if(!streamWriter.Write(jscriptMajorVersion))
        {
            return 0;
        }

        if(!streamWriter.Write(jscriptMinorVersion))
        {
            return 0;
        }

        if(!streamWriter.Write(startupFunctions->Length()))
        {
            return 0;
        }
        if(streamWriter.WriteArray(startupFunctions->GetData(), startupFunctions->WordCount()))
        {
            STATSTG stats;
            if(SUCCEEDED(writeStream->Stat(&stats, STATFLAG_NONAME)))
            {
                bytesWritten = stats.cbSize.LowPart;
                Assert(stats.cbSize.LowPart > 0);
                AssertMsg(stats.cbSize.HighPart == 0, "We should not be writing such long data that the high part is non-zero");
            }

            hr = profileDataCache->SaveWriteDataStream(writeStream);
            if(FAILED(hr))
            {
                return 0;
            }
#if DBG_DUMP
            if(PHASE_TRACE1(Js::DynamicProfilePhase) && Js::Configuration::Global.flags.Verbose)
            {
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, L"Saved profile:\n");
                startupFunctions->Dump();
            }
#endif
        }
#endif
        return bytesWritten;
    }

    //
    // Do not save the profile:
    //      - If it is a non-cacheable WININET resource
    //      - If there are no or small number of functions executed
    //      - If there is not substantial difference in number of functions executed.
    //
    bool SourceDynamicProfileManager::ShouldSaveToProfileCache(SourceContextInfo* info) const
    {
        if(isNonCachableScript)
        {
            OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, L"Skipping save of profile. Non-cacheable resource. %s\n", info->url);
            return false;
        }

        if(!startupFunctions || startupFunctions->Length() <= DEFAULT_CONFIG_MinProfileCacheSize)
        {
            OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, L"Skipping save of profile. Small number of functions. %s\n", info->url);
            return false;
        }

        if(cachedStartupFunctions)
        {
            AssertMsg(cachedStartupFunctions != startupFunctions, "Ensure they are not shallow copies of each other - Reuse() does this for dynamic sources. We should not be invoked for dynamic sources");
            uint numberOfBitsDifferent = cachedStartupFunctions->DiffCount(startupFunctions);
            uint saveThreshold = (cachedStartupFunctions->Length() * DEFAULT_CONFIG_ProfileDifferencePercent) / 100;
            if(numberOfBitsDifferent <= saveThreshold)
            {
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, L"Skipping save of profile. Number of functions different: %d %s\n", numberOfBitsDifferent, info->url);
                return false;
            }
            else
            {
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, L"Number of functions different: %d ", numberOfBitsDifferent);
            }
        }
        return true;
    }

    SourceDynamicProfileManager *
    SourceDynamicProfileManager::LoadFromDynamicProfileStorage(SourceContextInfo* info, ScriptContext* scriptContext, IActiveScriptDataCache* profileDataCache)
    {
        SourceDynamicProfileManager* manager = nullptr;
        Recycler* recycler = scriptContext->GetRecycler();

#ifdef DYNAMIC_PROFILE_STORAGE
        if(DynamicProfileStorage::IsEnabled() && info->url != nullptr)
        {
            manager = DynamicProfileStorage::Load(info->url, [recycler](char const * buffer, uint length) -> SourceDynamicProfileManager *
            {
                BufferReader reader(buffer, length);
                return SourceDynamicProfileManager::Deserialize(&reader, recycler);
            });
        }
#endif
        if(manager == nullptr)
        {
            manager = RecyclerNew(recycler, SourceDynamicProfileManager, recycler);
        }
        if(profileDataCache != nullptr)
        {
            bool profileLoaded = manager->LoadFromProfileCache(profileDataCache, info->url);
            if(profileLoaded)
            {
                JS_ETW(EventWriteJSCRIPT_PROFILE_LOAD(info->dwHostSourceContext, scriptContext));
            }
        }
        return manager;
    }

#ifdef DYNAMIC_PROFILE_STORAGE

    void
    SourceDynamicProfileManager::SaveDynamicProfileInfo(LocalFunctionId functionId, DynamicProfileInfo * dynamicProfileInfo)
    {
        Assert(dynamicProfileInfo->GetFunctionBody()->HasExecutionDynamicProfileInfo());
        dynamicProfileInfoMap.Item(functionId, dynamicProfileInfo);
    }

    template <typename T>
    SourceDynamicProfileManager *
    SourceDynamicProfileManager::Deserialize(T * reader, Recycler* recycler)
    {
        uint functionCount;
        if (!reader->Peek(&functionCount))
        {
            return nullptr;
        }

        BVFixed * startupFunctions = BVFixed::New(functionCount, recycler);
        if (!reader->ReadArray(((char *)startupFunctions),
            BVFixed::GetAllocSize(functionCount)))
        {
            return nullptr;
        }

        uint profileCount;

        if (!reader->Read(&profileCount))
        {
            return nullptr;
        }

        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

        SourceDynamicProfileManager * sourceDynamicProfileManager = RecyclerNew(threadContext->GetRecycler(), SourceDynamicProfileManager, recycler);

        sourceDynamicProfileManager->cachedStartupFunctions = startupFunctions;

#if DBG_DUMP
        if(Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {
            Output::Print(L"Loaded: Startup functions bit vector:");
            startupFunctions->Dump();
        }
#endif

        for (uint i = 0; i < profileCount; i++)
        {
            Js::LocalFunctionId functionId;
            DynamicProfileInfo * dynamicProfileInfo = DynamicProfileInfo::Deserialize(reader, recycler, &functionId);
            if (dynamicProfileInfo == nullptr || functionId >= functionCount)
            {
                return nullptr;
            }
            sourceDynamicProfileManager->dynamicProfileInfoMap.Add(functionId, dynamicProfileInfo);
        }
        return sourceDynamicProfileManager;
    }

    template <typename T>
    bool
    SourceDynamicProfileManager::Serialize(T * writer)
    {
        // To simulate behavior of in memory profile cache - let's keep functions marked as executed if they were loaded
        // to be so from the profile - this helps with ensure inlined functions are marked as executed.
        if(!this->startupFunctions)
        {
            this->startupFunctions = const_cast<BVFixed*>(this->cachedStartupFunctions);
        }
        else if(cachedStartupFunctions && this->cachedStartupFunctions->Length() == this->startupFunctions->Length())
        {
            this->startupFunctions->Or(cachedStartupFunctions);
        }

        if(this->startupFunctions)
        {
#if DBG_DUMP
             if(Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
            {
                Output::Print(L"Saving: Startup functions bit vector:");
                this->startupFunctions->Dump();
            }
#endif

            size_t bvSize = BVFixed::GetAllocSize(this->startupFunctions->Length()) ;
            if (!writer->WriteArray((char *)this->startupFunctions, bvSize)
                || !writer->Write(this->dynamicProfileInfoMap.Count()))
            {
                return false;
            }
        }

        for (int i = 0; i < this->dynamicProfileInfoMap.Count(); i++)
        {
            DynamicProfileInfo * dynamicProfileInfo = this->dynamicProfileInfoMap.GetValueAt(i);
            if (dynamicProfileInfo == nullptr || !dynamicProfileInfo->HasFunctionBody())
            {
                continue;
            }

            if (!dynamicProfileInfo->Serialize(writer))
            {
                return false;
            }
        }
        return true;
    }

    void
    SourceDynamicProfileManager::SaveToDynamicProfileStorage(wchar_t const * url)
    {
        Assert(DynamicProfileStorage::IsEnabled());
        BufferSizeCounter counter;
        if (!this->Serialize(&counter))
        {
            return;
        }

        if (counter.GetByteCount() > UINT_MAX)
        {
            // too big
            return;
        }

        char * record = DynamicProfileStorage::AllocRecord(static_cast<DWORD>(counter.GetByteCount()));
#if DBG_DUMP
        if (PHASE_STATS1(DynamicProfilePhase))
        {
            Output::Print(L"%-180s : %d bytes\n", url, counter.GetByteCount());
        }
#endif

        BufferWriter writer(DynamicProfileStorage::GetRecordBuffer(record), counter.GetByteCount());
        if (!this->Serialize(&writer))
        {
            Assert(false);
            DynamicProfileStorage::DeleteRecord(record);
        }

        DynamicProfileStorage::SaveRecord(url, record);
    }

#endif
};
#endif
