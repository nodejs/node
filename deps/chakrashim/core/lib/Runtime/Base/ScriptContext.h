//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "activprof.h"

#if DBG || ENABLE_REGEX_CONFIG_OPTIONS || defined(PROFILE_STRINGS)
#define NEED_MISC_ALLOCATOR
#endif

#define BuiltInFunctionsScriptId 0

class NativeCodeGenerator;
class BackgroundParser;
struct IActiveScriptDirect;
#ifdef ENABLE_BASIC_TELEMETRY
class ScriptContextTelemetry;
#endif
namespace Js
{
    class ScriptContext;
    class ScriptEditQuery;
    class MutationBreakpoint;
    class StringProfiler;
    class DebugContext;
    struct HaltCallback;
    struct DebuggerOptionsCallback;
}

// Created for every source buffer passed by host.
// This structure has all the details.
class SRCINFO
{
    // We currently don't free SRCINFO object so we don't want to add extra variables here.
    // In future, when we do make it freeable and will be able to allocate more than one per Module,
    // we can move variables m_isGlobalFunc and m_isEval from FunctionBody.cpp here.
public:
    SourceContextInfo * sourceContextInfo;
    ULONG dlnHost;             // Line number passed to ParseScriptText
    ULONG ulColumnHost;        // Column number on the line where the parse script text started
    ULONG lnMinHost;           // Line offset of first host-supplied line
    ULONG ichMinHost;          // Range of host supplied characters
    ULONG ichLimHost;
    ULONG ulCharOffset;        // Char offset of the source text relative to the document. (Populated using IActiveScriptContext)
    Js::ModuleID moduleID;
    ULONG grfsi;

    static SRCINFO* Copy(Recycler* recycler, const SRCINFO* srcInfo)
    {
        SRCINFO* copySrcInfo = RecyclerNew(recycler, SRCINFO, *srcInfo);
        return copySrcInfo;
    }

    SRCINFO* Clone(Js::ScriptContext* scriptContext) const;
};

struct CustomExternalObjectOperations
{
    size_t offsetOfOperationsUsage;
    DWORD operationFlagEquals;
    DWORD operationFlagStrictEquals;
};

enum ExternalJitData
{
    ExternalJitData_CustomExternalObjectOperations
};

class HostScriptContext
{
public:
    HostScriptContext(Js::ScriptContext* inScriptContext) { this->scriptContext = inScriptContext; }
    virtual void Delete() = 0;
    virtual HRESULT GetPreviousHostScriptContext(__deref_out HostScriptContext** ppUnkCaller) = 0;
    virtual HRESULT PushHostScriptContext() = 0;
    virtual void PopHostScriptContext() = 0;

    virtual HRESULT SetCaller(IUnknown *punkNew, IUnknown **ppunkPrev) = 0;
    virtual HRESULT GetDispatchExCaller(__deref_out void** dispatchExCaller) = 0;
    virtual void ReleaseDispatchExCaller(__in void* dispatchExCaler) = 0;
    virtual Js::ModuleRoot * GetModuleRoot(int moduleID) = 0;
    virtual HRESULT CheckCrossDomainScriptContext(__in Js::ScriptContext* scriptContext) = 0;

    virtual HRESULT GetHostContextUrl(__in DWORD_PTR hostSourceContext, __out BSTR& pUrl) = 0;
    virtual BOOL HasCaller() = 0;
    virtual void CleanDynamicCodeCache() = 0;
    virtual HRESULT VerifyDOMSecurity(Js::ScriptContext* targetContext, Js::Var obj) = 0;

    virtual HRESULT CheckEvalRestriction() = 0;
    virtual HRESULT HostExceptionFromHRESULT(HRESULT hr, Js::Var* outError) = 0;

    virtual HRESULT GetExternalJitData(ExternalJitData id, void *data) = 0;
    virtual HRESULT SetDispatchInvoke(Js::JavascriptMethod dispatchInvoke) = 0;
    virtual HRESULT ArrayBufferFromExternalObject(__in Js::RecyclableObject *obj,
        __out Js::ArrayBuffer **ppArrayBuffer) = 0;
    virtual Js::JavascriptError* CreateWinRTError(IErrorInfo* perrinfo, Js::RestrictedErrorStrings * proerrstr) = 0;
    virtual Js::JavascriptFunction* InitializeHostPromiseContinuationFunction() = 0;

    Js::ScriptContext* GetScriptContext() { return scriptContext; }

    virtual bool SetCrossSiteForFunctionType(Js::JavascriptFunction * function) = 0;
#if DBG_DUMP || defined(PROFILE_EXEC) || defined(PROFILE_MEM)
    virtual void EnsureParentInfo(Js::ScriptContext* scriptContext = NULL) = 0;
#endif
#if DBG
    virtual bool IsHostCrossSiteThunk(Js::JavascriptMethod address) = 0;
#endif
private:
    Js::ScriptContext* scriptContext;
};

namespace Js
{

#pragma pack(push, 1)
    struct StackFrameInfo
    {
        StackFrameInfo() { }
        StackFrameInfo(DWORD_PTR _scriptContextID
            , UINT32 _sourceLocationLineNumber
            , UINT32 _sourceLocationColumnNumber
            , UINT32 _methodIDOrNameIndex
            , UINT8 _isFrameIndex)
            : scriptContextID(_scriptContextID)
            , sourceLocationLineNumber(_sourceLocationLineNumber)
            , sourceLocationColumnNumber(_sourceLocationColumnNumber)
            , methodIDOrNameIndex(_methodIDOrNameIndex)
            , isFrameIndex(_isFrameIndex)
        { }

        DWORD_PTR scriptContextID;
        UINT32 sourceLocationLineNumber;
        UINT32 sourceLocationColumnNumber;
        UINT32 methodIDOrNameIndex;
        UINT8  isFrameIndex;
    };
#pragma pack(pop)

    class ProjectionConfiguration
    {
    public:
        ProjectionConfiguration() : targetVersion(0)
        {
        }

        DWORD GetTargetVersion() const { return this->targetVersion; }
        void SetTargetVersion(DWORD version) { this->targetVersion = version; }

        bool IsTargetWindows8() const           { return this->targetVersion == NTDDI_WIN8; }
        bool IsTargetWindowsBlueOrLater() const { return this->targetVersion >= NTDDI_WINBLUE; }

    private:
        DWORD targetVersion;
    };

    class ScriptConfiguration
    {
    public:
        ScriptConfiguration(const ThreadConfiguration * const threadConfig, const bool isOptimizedForManyInstances) :
#ifdef ENABLE_PROJECTION
            HostType(Configuration::Global.flags.HostType),
            WinRTConstructorAllowed(Configuration::Global.flags.WinRTConstructorAllowed),
#endif
            NoNative(Configuration::Global.flags.NoNative),
            isOptimizedForManyInstances(isOptimizedForManyInstances),
            threadConfig(threadConfig)
        {
        }

        // Version
        bool SupportsES3()                      const { return true; }
        bool SupportsES3Extensions()            const {
#ifdef ENABLE_PROJECTION
            return HostType != HostTypeApplication;
#else
            return true;
#endif
        }

#define FORWARD_THREAD_CONFIG(flag) inline bool flag() const { return threadConfig->flag(); }
#define FLAG(threadFlag, globalFlag) FORWARD_THREAD_CONFIG(threadFlag)
#define FLAG_RELEASE(threadFlag, globalFlag) FORWARD_THREAD_CONFIG(threadFlag)
#include "../Base/ThreadConfigFlagsList.h"
#undef FLAG_RELEASE
#undef FLAG
#undef FORWARD_THREAD_CONFIG

        bool SupportsCollectGarbage() const { return true; }
        bool IsTypedArrayEnabled() const { return true; }
        bool BindDeferredPidRefs() const { return IsLetAndConstEnabled(); }

        void ForceNoNative() { this->NoNative = true; }
        void ForceNative() { this->NoNative = false; }
        bool IsNoNative() const { return this->NoNative; }

        void SetCanOptimizeGlobalLookupFlag(BOOL f){ this->fCanOptimizeGlobalLookup = f;}
        BOOL CanOptimizeGlobalLookup() const { return this->fCanOptimizeGlobalLookup;}
        bool IsOptimizedForManyInstances() const { return isOptimizedForManyInstances; }
        bool IsBlockScopeEnabled() const { return true; }
        void CopyFrom(ScriptConfiguration& other)
        {
            this->NoNative = other.NoNative;
            this->fCanOptimizeGlobalLookup = other.fCanOptimizeGlobalLookup;
#ifdef ENABLE_PROJECTION
            this->HostType = other.HostType;
            this->WinRTConstructorAllowed = other.WinRTConstructorAllowed;
            this->projectionConfiguration = other.projectionConfiguration;
#endif
        }

#ifdef ENABLE_PROJECTION
        Number GetHostType() const    // Returns one of enum HostType values (see ConfigFlagsTable.h).
        {
            AssertMsg(this->HostType >= HostTypeMin && this->HostType <= HostTypeMax, "HostType value is out of valid range.");
            return this->HostType;
        }

        ProjectionConfiguration const * GetProjectionConfig() const
        {
            return &projectionConfiguration;
        }
        void SetHostType(long hostType) { this->HostType = hostType; }
        void SetWinRTConstructorAllowed(bool allowed) { this->WinRTConstructorAllowed = allowed; }
        void SetProjectionTargetVersion(DWORD version)
        {
            projectionConfiguration.SetTargetVersion(version);
        }
        bool IsWinRTEnabled()           const { return (GetHostType() == Js::HostTypeApplication) || (GetHostType() == Js::HostTypeWebview); }

        bool IsWinRTConstructorAllowed() const { return (GetHostType() != Js::HostTypeWebview) || this->WinRTConstructorAllowed; }
#endif
    private:

        // Per script configurations
        bool NoNative;
        BOOL fCanOptimizeGlobalLookup;
        const bool isOptimizedForManyInstances;
        const ThreadConfiguration * const threadConfig;

#ifdef ENABLE_PROJECTION
        Number HostType;    // One of enum HostType values (see ConfigFlagsTable.h).
        bool WinRTConstructorAllowed;  // whether allow constructor in webview host type. Also note that this is not a security feature.
        ProjectionConfiguration projectionConfiguration;
#endif
    };

    struct ScriptEntryExitRecord
    {
        BOOL hasCaller : 1;
        BOOL hasReentered : 1;
#if DBG_DUMP
        BOOL isCallRoot : 1;
#endif
#if DBG || defined(PROFILE_EXEC)
        BOOL leaveForHost : 1;
#endif
#if DBG
        BOOL leaveForAsyncHostOperation : 1;
#endif
#ifdef CHECK_STACKWALK_EXCEPTION
        BOOL ignoreStackWalkException: 1;
#endif
        Js::ImplicitCallFlags savedImplicitCallFlags;

        void * returnAddrOfScriptEntryFunction;
        void * frameIdOfScriptExitFunction; // the frameAddres in x86, the return address in amd64/arm_soc
        ScriptContext * scriptContext;
        struct ScriptEntryExitRecord * next;

#if defined(_M_IX86) && defined(DBG)
        void * scriptEntryFS0;
#endif
#ifdef EXCEPTION_CHECK
        ExceptionType handledExceptionType;
#endif
    };

    static const unsigned int EvalMRUSize = 15;
    typedef JsUtil::BaseDictionary<DWORD_PTR, SourceContextInfo *, Recycler, PowerOf2SizePolicy> SourceContextInfoMap;
    typedef JsUtil::BaseDictionary<uint, SourceContextInfo *, Recycler, PowerOf2SizePolicy> DynamicSourceContextInfoMap;

    typedef JsUtil::BaseDictionary<EvalMapString, ScriptFunction*, RecyclerNonLeafAllocator, PrimeSizePolicy> SecondLevelEvalCache;
    typedef TwoLevelHashRecord<FastEvalMapString, ScriptFunction*, SecondLevelEvalCache, EvalMapString> EvalMapRecord;
    typedef JsUtil::Cache<FastEvalMapString, EvalMapRecord*, RecyclerNonLeafAllocator, PrimeSizePolicy, JsUtil::MRURetentionPolicy<FastEvalMapString, EvalMRUSize>, FastEvalMapStringComparer> EvalCacheTopLevelDictionary;
    typedef SList<Js::FunctionProxy*, Recycler> FunctionReferenceList;
    typedef JsUtil::Cache<EvalMapString, ParseableFunctionInfo*, RecyclerNonLeafAllocator, PrimeSizePolicy, JsUtil::MRURetentionPolicy<EvalMapString, EvalMRUSize>> NewFunctionCache;
    typedef JsUtil::BaseDictionary<ParseableFunctionInfo*, ParseableFunctionInfo*, Recycler, PrimeSizePolicy, RecyclerPointerComparer> ParseableFunctionInfoMap;
    // This is the dictionary used by script context to cache the eval.
    typedef TwoLevelHashDictionary<FastEvalMapString, ScriptFunction*, EvalMapRecord, EvalCacheTopLevelDictionary, EvalMapString> EvalCacheDictionary;

    struct PropertyStringMap
    {
        PropertyString* strLen2[80];

        __inline static uint PStrMapIndex(wchar_t ch)
        {
            Assert(ch >= '0' && ch <= 'z');
            return ch - '0';
        }
    };

#ifdef ENABLE_DOM_FAST_PATH
    typedef JsUtil::BaseDictionary<Js::FunctionInfo*, IR::JnHelperMethod, ArenaAllocator, PowerOf2SizePolicy> DOMFastPathIRHelperMap;
#endif

    // valid if object!= NULL
    struct EnumeratedObjectCache {
        static const int kMaxCachedPropStrings=16;
        DynamicObject* object;
        DynamicType* type;
        PropertyString* propertyStrings[kMaxCachedPropStrings];
        int validPropStrings;
    };

    // Holder for all cached pointers. These are allocated on a guest arena
    // ensuring they cause the related objects to be pinned.
    struct Cache
    {
        JavascriptString * lastNumberToStringRadix10String;
        EnumeratedObjectCache enumObjCache;
        JavascriptString * lastUtcTimeFromStrString;
        TypePath* rootPath;
        EvalCacheDictionary* evalCacheDictionary;
        EvalCacheDictionary* indirectEvalCacheDictionary;
        NewFunctionCache* newFunctionCache;
        RegexPatternMruMap *dynamicRegexMap;
        SourceContextInfoMap* sourceContextInfoMap;   // maps host provided context cookie to the URL of the script buffer passed.
        DynamicSourceContextInfoMap* dynamicSourceContextInfoMap;
        SourceContextInfo* noContextSourceContextInfo;
        SRCINFO* noContextGlobalSourceInfo;
        SRCINFO const ** moduleSrcInfo;
    };

    class ScriptContext : public ScriptContextBase
    {
        friend class LowererMD;
        friend class RemoteScriptContext;
    public:
        static DWORD GetThreadContextOffset() { return offsetof(ScriptContext, threadContext); }
        static DWORD GetOptimizationOverridesOffset() { return offsetof(ScriptContext, optimizationOverrides); }
        static DWORD GetRecyclerOffset() { return offsetof(ScriptContext, recycler); }
        static DWORD GetNumberAllocatorOffset() { return offsetof(ScriptContext, numberAllocator); }
        static DWORD GetAsmIntDbValOffset() { return offsetof(ScriptContext, retAsmIntDbVal); }

        ScriptContext *next;
        ScriptContext *prev;
        double retAsmIntDbVal; // stores the double & float result for Asm interpreter

        AsmJsSIMDValue retAsmSimdVal; // stores raw simd result for Asm interpreter
        static DWORD GetAsmSimdValOffset() { return offsetof(ScriptContext, retAsmSimdVal); }

        ScriptContextOptimizationOverrideInfo optimizationOverrides;

        Js::JavascriptMethod CurrentThunk;
        Js::JavascriptMethod CurrentCrossSiteThunk;
        Js::JavascriptMethod DeferredParsingThunk;
        Js::JavascriptMethod DeferredDeserializationThunk;
        Js::JavascriptMethod DispatchDefaultInvoke;
        Js::JavascriptMethod DispatchProfileInoke;

        typedef HRESULT (*GetDocumentContextFunction)(
            ScriptContext *pContext,
            Js::FunctionBody *pFunctionBody,
            IDebugDocumentContext **ppDebugDocumentContext);
        GetDocumentContextFunction GetDocumentContext;

        typedef HRESULT (*CleanupDocumentContextFunction)(ScriptContext *pContext);
        CleanupDocumentContextFunction CleanupDocumentContext;

        const ScriptContextBase* GetScriptContextBase() const { return static_cast<const ScriptContextBase*>(this); }

        bool DoUndeferGlobalFunctions() const;

        bool IsUndeclBlockVar(Var var) const { return this->javascriptLibrary->IsUndeclBlockVar(var); }

        void TrackPid(const PropertyRecord* propertyRecord);
        void TrackPid(PropertyId propertyId);

        bool IsTrackedPropertyId(Js::PropertyId propertyId);
        void InvalidateHostObjects()
        {
            AssertMsg(!isClosed, "Host Object invalidation should occur before the engine is fully closed. Figure our how isClosed got set beforehand.");
            isInvalidatedForHostObjects = true;
        }
        bool IsInvalidatedForHostObjects()
        {
            return isInvalidatedForHostObjects;
        }

#ifdef ENABLE_JS_ETW
        void EmitStackTraceEvent(__in UINT64 operationID, __in USHORT maxFrameCount, bool emitV2AsyncStackEvent);
#endif

        void SetIsDiagnosticsScriptContext(bool set) { this->isDiagnosticsScriptContext = set; }
        bool IsDiagnosticsScriptContext() const { return this->isDiagnosticsScriptContext; }

        bool IsInNonDebugMode() const;
        bool IsInSourceRundownMode() const;
        bool IsInDebugMode() const;
        bool IsInDebugOrSourceRundownMode() const;
        bool IsRunningScript() const { return this->threadContext->GetScriptEntryExit() != nullptr; }

        typedef JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::WeakRefFreeListedRemovePolicy> CalleeSourceList;
        RecyclerRootPtr<CalleeSourceList> calleeUtf8SourceInfoList;
        void AddCalleeSourceInfoToList(Utf8SourceInfo* sourceInfo);
        bool HaveCalleeSources() { return calleeUtf8SourceInfoList && !calleeUtf8SourceInfoList->Empty(); }

        template<class TMapFunction>
        void MapCalleeSources(TMapFunction map)
        {
            if (this->HaveCalleeSources())
            {
                calleeUtf8SourceInfoList->Map([&](uint i, RecyclerWeakReference<Js::Utf8SourceInfo>* sourceInfoWeakRef)
                {
                    if (calleeUtf8SourceInfoList->IsItemValid(i))
                    {
                        Js::Utf8SourceInfo* sourceInfo = sourceInfoWeakRef->Get();
                        map(sourceInfo);
                    }
                });
            }
            if (calleeUtf8SourceInfoList)
            {
                calleeUtf8SourceInfoList.Unroot(this->GetRecycler());
            }
        }

#ifdef ASMJS_PLAT
        inline AsmJsCodeGenerator* GetAsmJsCodeGenerator() const{return asmJsCodeGenerator;}
        AsmJsCodeGenerator* InitAsmJsCodeGenerator();
#endif

        bool IsExceptionWrapperForBuiltInsEnabled();
        bool IsEnumerateNonUserFunctionsOnly() const { return m_enumerateNonUserFunctionsOnly; }
        bool IsTraceDomCall() const { return !!m_fTraceDomCall; }
        static bool IsExceptionWrapperForBuiltInsEnabled(ScriptContext* scriptContext);
        static bool IsExceptionWrapperForHelpersEnabled(ScriptContext* scriptContext);

        InlineCache * GetValueOfInlineCache() const { return valueOfInlineCache;}
        InlineCache * GetToStringInlineCache() const { return toStringInlineCache; }

        FunctionBody * GetFakeGlobalFuncForUndefer() const { return fakeGlobalFuncForUndefer; }
        void SetFakeGlobalFuncForUndefer(FunctionBody * func) { fakeGlobalFuncForUndefer.Root(func, GetRecycler()); }

    private:
        PropertyStringMap* propertyStrings[80];

        JavascriptFunction* GenerateRootFunction(ParseNodePtr parseTree, uint sourceIndex, Parser* parser, ulong grfscr, CompileScriptException * pse, const wchar_t *rootDisplayName);

        typedef void (*EventHandler)(ScriptContext *);
        ScriptContext ** entryInScriptContextWithInlineCachesRegistry;
        ScriptContext ** entryInScriptContextWithIsInstInlineCachesRegistry;
        ScriptContext ** registeredPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext;

        ArenaAllocator generalAllocator;
#ifdef ENABLE_BASIC_TELEMETRY
        ArenaAllocator telemetryAllocator;
#endif

        ArenaAllocator dynamicProfileInfoAllocator;
        InlineCacheAllocator inlineCacheAllocator;
        IsInstInlineCacheAllocator isInstInlineCacheAllocator;

        ArenaAllocator* interpreterArena;
        ArenaAllocator* guestArena;

        ArenaAllocator* diagnosticArena;
        void ** bindRefChunkCurrent;
        void ** bindRefChunkEnd;

        bool startupComplete; // Indicates if the heuristic startup phase for this script context is complete
        bool isInvalidatedForHostObjects;  // Indicates that we've invalidate all objects in the host so stop calling them.
        bool isEnumeratingRecyclerObjects; // Indicates this scriptContext is enumerating recycler objects. Used by recycler enumerating callbacks to filter out other unrelated scriptContexts.
        bool m_enumerateNonUserFunctionsOnly; // Indicates that recycler enumeration callback will consider only non-user functions (which are built-ins, external, winrt etc).

        ThreadContext* threadContext;
        TypeId  directHostTypeId;

        InlineCache * valueOfInlineCache;
        InlineCache * toStringInlineCache;

        typedef JsUtil::BaseHashSet<Js::PropertyId, ArenaAllocator> PropIdSetForConstProp;
        PropIdSetForConstProp * intConstPropsOnGlobalObject;
        PropIdSetForConstProp * intConstPropsOnGlobalUserObject;

        void * firstInterpreterFrameReturnAddress;
#ifdef SEPARATE_ARENA
        ArenaAllocator sourceCodeAllocator;
        ArenaAllocator regexAllocator;
#endif
#ifdef NEED_MISC_ALLOCATOR
        ArenaAllocator miscAllocator;
#endif

#if DBG
        JsUtil::BaseHashSet<void *, ArenaAllocator> bindRef;
        int m_iProfileSession;
#endif

#ifdef PROFILE_EXEC
        ScriptContextProfiler * profiler;
        bool isProfilerCreated;
        bool disableProfiler;
        bool ensureParentInfo;

        Profiler * CreateProfiler();
#endif
#ifdef PROFILE_MEM
        bool profileMemoryDump;
#endif
#ifdef PROFILE_STRINGS
        StringProfiler* stringProfiler;
#endif

        RecyclerRootPtr<FunctionBody> fakeGlobalFuncForUndefer;

public:
#ifdef PROFILE_TYPES
        int convertNullToSimpleCount;
        int convertNullToSimpleDictionaryCount;
        int convertNullToDictionaryCount;
        int convertDeferredToDictionaryCount;
        int convertDeferredToSimpleDictionaryCount;
        int convertSimpleToDictionaryCount;
        int convertSimpleToSimpleDictionaryCount;
        int convertPathToDictionaryCount1;
        int convertPathToDictionaryCount2;
        int convertPathToDictionaryCount3;
        int convertPathToDictionaryCount4;
        int convertPathToSimpleDictionaryCount;
        int convertSimplePathToPathCount;
        int convertSimpleDictionaryToDictionaryCount;
        int convertSimpleSharedDictionaryToNonSharedCount;
        int convertSimpleSharedToNonSharedCount;
        int simplePathTypeHandlerCount;
        int pathTypeHandlerCount;
        int promoteCount;
        int cacheCount;
        int branchCount;
        int maxPathLength;
        int typeCount[TypeIds_Limit];
        int instanceCount[TypeIds_Limit];
#endif


#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        __int64 bailOutRecordBytes;
        __int64 bailOutOffsetBytes;
        __int64 codeSize;
#endif

#ifdef  PROFILE_OBJECT_LITERALS
        int objectLiteralInstanceCount;
        int objectLiteralPathCount;
        int objectLiteralCount[TypePath::MaxPathTypeHandlerLength];
        int objectLiteralSimpleDictionaryCount;
        uint32 objectLiteralMaxLength;
        int objectLiteralPromoteCount;
        int objectLiteralCacheCount;
        int objectLiteralBranchCount;
#endif
#if DBG_DUMP
        uint byteCodeDataSize;
        uint byteCodeAuxiliaryDataSize;
        uint byteCodeAuxiliaryContextDataSize;
        uint byteCodeHistogram[OpCode::ByteCodeLast];
        uint32 forinCache;
        uint32 forinNoCache;
#endif
#ifdef BGJIT_STATS
        uint interpretedCount;
        uint funcJITCount;
        uint loopJITCount;
        uint bytecodeJITCount;
        uint interpretedCallsHighPri;
        uint maxFuncInterpret;
        uint jitCodeUsed;
        uint funcJitCodeUsed;
        uint speculativeJitCount;
#endif

#ifdef REJIT_STATS
        // Used to store bailout stats
        typedef JsUtil::BaseDictionary<uint, uint, ArenaAllocator> BailoutStatsMap;

        struct RejitStats
        {
            uint *m_rejitReasonCounts;
            BailoutStatsMap* m_bailoutReasonCounts;

            uint  m_totalRejits;
            uint  m_totalBailouts;

            RejitStats(ScriptContext *scriptContext) : m_totalRejits(0), m_totalBailouts(0)
            {
                m_rejitReasonCounts = AnewArrayZ(scriptContext->GeneralAllocator(), uint, NumRejitReasons);
                m_bailoutReasonCounts = Anew(scriptContext->GeneralAllocator(), BailoutStatsMap, scriptContext->GeneralAllocator());
            }
        };

        void LogDataForFunctionBody(Js::FunctionBody *body, uint idx, bool isRejit);

        void LogRejit(Js::FunctionBody *body, uint reason);
        void LogBailout(Js::FunctionBody *body, uint kind);

        // Used to centrally collect stats for all function bodies.
        typedef JsUtil::WeaklyReferencedKeyDictionary<const Js::FunctionBody, RejitStats*> RejitStatsMap;
        RejitStatsMap* rejitStatsMap;

        BailoutStatsMap *bailoutReasonCounts;
        uint *rejitReasonCounts;
#endif
#ifdef ENABLE_BASIC_TELEMETRY

    private:
        ScriptContextTelemetry* telemetry;
    public:
        ScriptContextTelemetry& GetTelemetry();
        bool HasTelemetry();

#endif
#ifdef INLINE_CACHE_STATS
        // Used to store inline cache stats

        struct CacheData
        {
            uint hits;
            uint misses;
            uint collisions;
            bool isGetCache;
            Js::PropertyId propertyId;

            CacheData() : hits(0), misses(0), collisions(0), isGetCache(false), propertyId(Js::PropertyIds::_none) { }
        };

        // This is a strongly referenced dictionary, since we want to know hit rates for dead caches.
        typedef JsUtil::BaseDictionary<const Js::PolymorphicInlineCache*, CacheData*, Recycler> CacheDataMap;
        CacheDataMap *cacheDataMap;

        void LogCacheUsage(Js::PolymorphicInlineCache *cache, bool isGet, Js::PropertyId propertyId, bool hit, bool collision);
#endif

#ifdef FIELD_ACCESS_STATS
        typedef SList<FieldAccessStatsPtr, Recycler> FieldAccessStatsList;

        struct FieldAccessStatsEntry
        {
            RecyclerWeakReference<FunctionBody>* functionBodyWeakRef;
            FieldAccessStatsList stats;

            FieldAccessStatsEntry(RecyclerWeakReference<FunctionBody>* functionBodyWeakRef, Recycler* recycler)
                : functionBodyWeakRef(functionBodyWeakRef), stats(recycler) {}
        };

        typedef JsUtil::BaseDictionary<uint, FieldAccessStatsEntry*, Recycler> FieldAccessStatsByFunctionNumberMap;

        FieldAccessStatsByFunctionNumberMap* fieldAccessStatsByFunctionNumber;

        void RecordFieldAccessStats(FunctionBody* functionBody, FieldAccessStatsPtr fieldAccessStats);
#endif

#ifdef MISSING_PROPERTY_STATS
        int missingPropertyMisses;
        int missingPropertyHits;
        int missingPropertyCacheAttempts;

        void RecordMissingPropertyMiss();
        void RecordMissingPropertyHit();
        void RecordMissingPropertyCacheAttempt();
#endif

        bool IsIntConstPropertyOnGlobalObject(Js::PropertyId propId);
        void TrackIntConstPropertyOnGlobalObject(Js::PropertyId propId);
        bool IsIntConstPropertyOnGlobalUserObject(Js::PropertyId propertyId);
        void TrackIntConstPropertyOnGlobalUserObject(Js::PropertyId propertyId);

private:
        //
        // Regex globals
        //
#if ENABLE_REGEX_CONFIG_OPTIONS
        UnifiedRegex::DebugWriter* regexDebugWriter;
        UnifiedRegex::RegexStatsDatabase* regexStatsDatabase;
#endif
        UnifiedRegex::TrigramAlphabet* trigramAlphabet;
        UnifiedRegex::RegexStacks *regexStacks;

        FunctionReferenceList* dynamicFunctionReference;
        uint dynamicFunctionReferenceDepth;

        JsUtil::Stack<Var>* operationStack;
        Recycler* recycler;
        RecyclerJavascriptNumberAllocator numberAllocator;

        ScriptConfiguration config;
        CharClassifier *charClassifier;

        // DisableJIT-TODO: Switch this to Dynamic thunk ifdef instead
#if ENABLE_NATIVE_CODEGEN
        InterpreterThunkEmitter* interpreterThunkEmitter;

        BackgroundParser *backgroundParser;
#ifdef ASMJS_PLAT
        InterpreterThunkEmitter* asmJsInterpreterThunkEmitter;
        AsmJsCodeGenerator* asmJsCodeGenerator;
        typedef JsUtil::BaseDictionary<void *, SList<AsmJsScriptFunction *>*, ArenaAllocator> AsmFunctionMap;
        AsmFunctionMap* asmJsEnvironmentMap;
        ArenaAllocator* debugTransitionAlloc;
#endif
        NativeCodeGenerator* nativeCodeGen;
#endif

        TIME_ZONE_INFORMATION timeZoneInfo;
        uint lastTimeZoneUpdateTickCount;
        DaylightTimeHelper daylightTimeHelper;

        HostScriptContext * hostScriptContext;
        HaltCallback* scriptEngineHaltCallback;
        EventHandler scriptStartEventHandler;
        EventHandler scriptEndEventHandler;
#ifdef FAULT_INJECTION
        EventHandler disposeScriptByFaultInjectionEventHandler;
#endif

        JsUtil::BaseDictionary<uint, JavascriptString *, ArenaAllocator> integerStringMap;

        double lastNumberToStringRadix10;
        double lastUtcTimeFromStr;

#if ENABLE_PROFILE_INFO
        bool referencesSharedDynamicSourceContextInfo;
#endif

        // We could delay the actual close after callRootLevel is 0.
        // this is to indicate the actual close is called once only.
        bool isScriptContextActuallyClosed;
#if DBG
        bool isInitialized;
        bool isCloningGlobal;
#endif
        bool fastDOMenabled;
        bool hasRegisteredInlineCache;
        bool hasRegisteredIsInstInlineCache;
        bool deferredBody;
        bool isPerformingNonreentrantWork;
        bool isDiagnosticsScriptContext;   // mentions that current script context belongs to the diagnostics OM.

        size_t sourceSize;

        void CleanSourceListInternal(bool calledDuringMark);
        typedef JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::FreeListedRemovePolicy> SourceList;
        RecyclerRootPtr<SourceList> sourceList;

        IActiveScriptProfilerHeapEnum* heapEnum;

        // Profiler Probes
        // In future these can be list of callbacks ?
        // Profiler Callback information
        IActiveScriptProfilerCallback *m_pProfileCallback;
        BOOL m_fTraceFunctionCall;
        BOOL m_fTraceNativeFunctionCall;
        DWORD m_dwEventMask;

        IActiveScriptProfilerCallback2 *m_pProfileCallback2;
        BOOL m_fTraceDomCall;
        BOOL m_inProfileCallback;

#if ENABLE_PROFILE_INFO
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        RecyclerRootPtr<SListBase<DynamicProfileInfo *>> profileInfoList;
#endif
#endif
#if DEBUG
        static int scriptContextCount;
#endif
        // List of weak reference dictionaries. We'll walk through them
        // and clean them up post-collection
        // IWeakReferenceDictionary objects are added to this list by calling
        // RegisterWeakReferenceDictionary. If you use JsUtil::WeakReferenceDictionary,
        // which also exposes the IWeakReferenceDictionary interface, it'll
        // automatically register the dictionary on the script context
        SListBase<JsUtil::IWeakReferenceDictionary*> weakReferenceDictionaryList;
        bool isWeakReferenceDictionaryListCleared;

        typedef void(*RaiseMessageToDebuggerFunctionType)(ScriptContext *, DEBUG_EVENT_INFO_TYPE, LPCWSTR, LPCWSTR);
        RaiseMessageToDebuggerFunctionType raiseMessageToDebuggerFunctionType;

        typedef void(*TransitionToDebugModeIfFirstSourceFn)(ScriptContext *, Utf8SourceInfo *);
        TransitionToDebugModeIfFirstSourceFn transitionToDebugModeIfFirstSourceFn;

#ifdef ENABLE_DOM_FAST_PATH
        // Theoretically we can put this in ThreadContext; don't want to keep the dictionary forever, and preserve the possibility of
        // using JavascriptFunction as key.
        DOMFastPathIRHelperMap* domFastPathIRHelperMap;
#endif


        ScriptContext(ThreadContext* threadContext);
        void InitializeAllocations();
        void InitializePreGlobal();
        void InitializePostGlobal();

        // Source Info
        void EnsureSourceContextInfoMap();
        void EnsureDynamicSourceContextInfoMap();

        uint moduleSrcInfoCount;
#ifdef RUNTIME_DATA_COLLECTION
        time_t createTime;
#endif
        wchar_t const * url;

        void PrintStats();
        BOOL LeaveScriptStartCore(void * frameAddress, bool leaveForHost);

        void InternalClose();

        DebugContext* debugContext;

    public:
        static const int kArrayMatchCh=72;
        static const int kMaxArrayMatchIndex=8192;
        short arrayMatchItems[kArrayMatchCh];
        bool arrayMatchInit;

#ifdef LEAK_REPORT
        LeakReport::UrlRecord * urlRecord;
        bool isRootTrackerScriptContext;
#endif

        DaylightTimeHelper *GetDaylightTimeHelper() { return &daylightTimeHelper; }

        bool IsClosed() const { return isClosed; }
        bool IsActuallyClosed() const { return isScriptContextActuallyClosed; }
#if ENABLE_NATIVE_CODEGEN
        bool IsClosedNativeCodeGenerator() const
        {
            return !nativeCodeGen || ::IsClosedNativeCodeGenerator(nativeCodeGen);
        }
#endif

        void SetDirectHostTypeId(TypeId typeId) {directHostTypeId = typeId; }
        TypeId GetDirectHostTypeId() const { return directHostTypeId; }

        TypePath* GetRootPath() { return cache->rootPath; }

#ifdef ENABLE_DOM_FAST_PATH
        DOMFastPathIRHelperMap* EnsureDOMFastPathIRHelperMap();
#endif
        wchar_t const * GetUrl() const { return url; }
        void SetUrl(BSTR bstr);
#ifdef RUNTIME_DATA_COLLECTION
        time_t GetCreateTime() const { return createTime; }
        uint GetAllocId() const { return allocId; }
#endif
        void InitializeArrayMatch()
        {
            if (!arrayMatchInit)
            {
                for (int i=0;i<kArrayMatchCh;i++)
                {
                    arrayMatchItems[i]= -1;
                }
                arrayMatchInit=true;
            }
        }

#ifdef HEAP_ENUMERATION_VALIDATION
        bool IsInitialized() { return this->isInitialized; }
#endif

        DebugContext* GetDebugContext() const { return this->debugContext; }

        uint callCount;

        // Guest arena allocated cache holding references that need to be pinned.
        Cache* cache;

        // if the current context is for webworker
        DWORD webWorkerId;

        static ScriptContext * New(ThreadContext * threadContext);
        static void Delete(ScriptContext* scriptContext);

        ~ScriptContext();

#ifdef PROFILE_TYPES
        void ProfileTypes();
#endif

#ifdef PROFILE_OBJECT_LITERALS
        void ProfileObjectLiteral();
#endif

        //
        // Regex helpers
        //
#if ENABLE_REGEX_CONFIG_OPTIONS
        UnifiedRegex::RegexStatsDatabase* GetRegexStatsDatabase();
        UnifiedRegex::DebugWriter* GetRegexDebugWriter();
#endif
        RegexPatternMruMap* GetDynamicRegexMap() const;

        UnifiedRegex::TrigramAlphabet* GetTrigramAlphabet() { return trigramAlphabet; }

        void SetTrigramAlphabet(UnifiedRegex::TrigramAlphabet * trigramAlphabet);

        UnifiedRegex::RegexStacks *RegexStacks();
        UnifiedRegex::RegexStacks *AllocRegexStacks();
        UnifiedRegex::RegexStacks *SaveRegexStacks();
        void RestoreRegexStacks(UnifiedRegex::RegexStacks *const contStack);

        void InitializeGlobalObject();
        JavascriptLibrary* GetLibrary() const { return javascriptLibrary; }
        const JavascriptLibraryBase* GetLibraryBase() const { return javascriptLibrary->GetLibraryBase(); }
#if DBG
        BOOL IsCloningGlobal() const { return isCloningGlobal;}
#endif
        void PushObject(Var object);
        Var PopObject();
        BOOL CheckObject(Var object);

        inline bool IsHeapEnumInProgress() { return GetRecycler()->IsHeapEnumInProgress(); }

        bool IsInterpreted() { return config.IsNoNative(); }
        void ForceNoNative() { config.ForceNoNative(); }
        void ForceNative() { config.ForceNative(); }
        ScriptConfiguration const * GetConfig(void) const { return &config; }
        CharClassifier const * GetCharClassifier(void) const;

        ThreadContext * GetThreadContext() const { return threadContext; }

        TIME_ZONE_INFORMATION * GetTimeZoneInfo()
        {
            uint tickCount = GetTickCount();
            if (tickCount - lastTimeZoneUpdateTickCount > 1000)
            {
                UpdateTimeZoneInfo();
                lastTimeZoneUpdateTickCount = tickCount;
            }
            return &timeZoneInfo;
        }
        void UpdateTimeZoneInfo();

        static const int MaxEvalSourceSize = 400;

        bool IsInEvalMap(FastEvalMapString const& key, BOOL isIndirect, ScriptFunction **ppFuncScript);
        void AddToEvalMap(FastEvalMapString const& key, BOOL isIndirect, ScriptFunction *pFuncScript);

        template <typename TCacheType>
        void CleanDynamicFunctionCache(TCacheType* cacheType);
        void CleanEvalMapCache(Js::EvalCacheTopLevelDictionary * cacheType);

        template <class TDelegate>
        void MapFunction(TDelegate mapper);

        template <class TDelegate>
        FunctionBody* FindFunction(TDelegate predicate);

        __inline bool EnableEvalMapCleanup() { return CONFIG_FLAG(EnableEvalMapCleanup); };
        void BeginDynamicFunctionReferences();
        void EndDynamicFunctionReferences();
        void RegisterDynamicFunctionReference(FunctionProxy* func);
        uint GetNextSourceContextId();

        bool IsInNewFunctionMap(EvalMapString const& key, ParseableFunctionInfo **ppFuncBody);
        void AddToNewFunctionMap(EvalMapString const& key, ParseableFunctionInfo *pFuncBody);

        SourceContextInfo * GetSourceContextInfo(DWORD_PTR hostSourceContext, IActiveScriptDataCache* profileDataCache);
        SourceContextInfo * GetSourceContextInfo(uint hash);
        SourceContextInfo * CreateSourceContextInfo(uint hash, DWORD_PTR hostSourceContext);
        SourceContextInfo * CreateSourceContextInfo(DWORD_PTR hostSourceContext, wchar_t const * url, size_t len,
            IActiveScriptDataCache* profileDataCache, wchar_t const * sourceMapUrl = nullptr, size_t sourceMapUrlLen = 0);

#if defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
        void ClearSourceContextInfoMaps()
        {
          if (this->cache != nullptr)
          {
              this->cache->sourceContextInfoMap = nullptr;
              this->cache->dynamicSourceContextInfoMap = nullptr;
#if ENABLE_PROFILE_INFO
              this->referencesSharedDynamicSourceContextInfo = false;
#endif
          }
        }
#endif

#if ENABLE_PROFILE_INFO
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        void ClearDynamicProfileList()
        {
            if (profileInfoList)
            {
                profileInfoList->Reset();
                profileInfoList.Unroot(this->recycler);
            }
        }

        SListBase<DynamicProfileInfo *> * GetProfileInfoList() { return profileInfoList; }
#endif
#endif

        SRCINFO const * GetModuleSrcInfo(Js::ModuleID moduleID);
        SourceContextInfoMap* GetSourceContextInfoMap()
        {
            return (this->cache ? this->cache->sourceContextInfoMap : nullptr);
        }
        DynamicSourceContextInfoMap* GetDynamicSourceContextInfoMap()
        {
            return (this->cache ? this->cache->dynamicSourceContextInfoMap : nullptr);
        }

        void SetFirstInterpreterFrameReturnAddress(void * returnAddress) { firstInterpreterFrameReturnAddress = returnAddress;}
        void *GetFirstInterpreterFrameReturnAddress() { return firstInterpreterFrameReturnAddress;}

        void CleanupWeakReferenceDictionaries();

        void Initialize();
        bool Close(bool inDestructor);
        void MarkForClose();
#ifdef ENABLE_PROJECTION
        void SetHostType(long hostType) { config.SetHostType(hostType); }
        void SetWinRTConstructorAllowed(bool allowed) { config.SetWinRTConstructorAllowed(allowed); }
        void SetProjectionTargetVersion(DWORD version) { config.SetProjectionTargetVersion(version); }
#endif
        void SetCanOptimizeGlobalLookupFlag(BOOL f){ config.SetCanOptimizeGlobalLookupFlag(f);}
        BOOL CanOptimizeGlobalLookup(){ return config.CanOptimizeGlobalLookup();}


        bool IsClosed() { return isClosed; }
        bool IsFastDOMEnabled() { return fastDOMenabled; }
        void SetFastDOMenabled();
        BOOL VerifyAlive(BOOL isJSFunction = FALSE, ScriptContext* requestScriptContext = nullptr);
        void VerifyAliveWithHostContext(BOOL isJSFunction, HostScriptContext* requestHostScriptContext);
        void AddFunctionBodyToPropIdMap(FunctionBody* body);

        void BindReference(void* addr);

        void InitPropertyStringMap(int i);
        PropertyString* AddPropertyString2(const Js::PropertyRecord* propertyRecord);
        PropertyString* CachePropertyString2(const Js::PropertyRecord* propertyRecord);
        PropertyString* GetPropertyString2(wchar_t ch1, wchar_t ch2);
        void FindPropertyRecord(__in LPCWSTR pszPropertyName, __in int propertyNameLength, PropertyRecord const** propertyRecord);
        JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>* FindPropertyIdNoCase(__in LPCWSTR pszPropertyName, __in int propertyNameLength);

        void FindPropertyRecord(JavascriptString* pstName, PropertyRecord const** propertyRecord);
        PropertyRecord const * GetPropertyName(PropertyId propertyId);
        PropertyRecord const * GetPropertyNameLocked(PropertyId propertyId);
        void GetOrAddPropertyRecord(JsUtil::CharacterBuffer<WCHAR> const& propName, PropertyRecord const** propertyRecord);
        template <size_t N> void GetOrAddPropertyRecord(const wchar_t(&propertyName)[N], PropertyRecord const** propertyRecord)
        {
            GetOrAddPropertyRecord(propertyName, N - 1, propertyRecord);
        }
        PropertyId GetOrAddPropertyIdTracked(JsUtil::CharacterBuffer<WCHAR> const& propName);
        template <size_t N> PropertyId GetOrAddPropertyIdTracked(const wchar_t(&propertyName)[N])
        {
            return GetOrAddPropertyIdTracked(propertyName, N - 1);
        }
        PropertyId GetOrAddPropertyIdTracked(__in_ecount(propertyNameLength) LPCWSTR pszPropertyName, __in int propertyNameLength);
        void GetOrAddPropertyRecord(__in_ecount(propertyNameLength) LPCWSTR pszPropertyName, __in int propertyNameLength, PropertyRecord const** propertyRecord);
        BOOL IsNumericPropertyId(PropertyId propertyId, uint32* value);

        void RegisterWeakReferenceDictionary(JsUtil::IWeakReferenceDictionary* weakReferenceDictionary);
        void ResetWeakReferenceDicitionaryList() { weakReferenceDictionaryList.Reset(); }

        BOOL ReserveStaticTypeIds(__in int first, __in int last);
        TypeId ReserveTypeIds(int count);
        TypeId CreateTypeId();

        WellKnownHostType GetWellKnownHostType(Js::TypeId typeId) { return threadContext->GetWellKnownHostType(typeId); }
        void SetWellKnownHostTypeId(WellKnownHostType wellKnownType, Js::TypeId typeId) { threadContext->SetWellKnownHostTypeId(wellKnownType, typeId); }

        JavascriptFunction* LoadScript(const wchar_t* script, SRCINFO const * pSrcInfo, CompileScriptException * pse, bool isExpression, bool disableDeferredParse, bool isByteCodeBufferForLibrary, Utf8SourceInfo** ppSourceInfo, const wchar_t *rootDisplayName, bool disableAsmJs = false);
        JavascriptFunction* LoadScript(LPCUTF8 script, size_t cb, SRCINFO const * pSrcInfo, CompileScriptException * pse, bool isExpression, bool disableDeferredParse, bool isByteCodeBufferForLibrary, Utf8SourceInfo** ppSourceInfo, const wchar_t *rootDisplayName, bool disableAsmJs = false);

        ArenaAllocator* GeneralAllocator() { return &generalAllocator; }

#ifdef ENABLE_BASIC_TELEMETRY
        ArenaAllocator* TelemetryAllocator() { return &telemetryAllocator; }
#endif

#ifdef SEPARATE_ARENA
        ArenaAllocator* SourceCodeAllocator() { return &sourceCodeAllocator; }
        ArenaAllocator* RegexAllocator() { return &regexAllocator; }
#else
        ArenaAllocator* SourceCodeAllocator() { return &generalAllocator; }
        ArenaAllocator* RegexAllocator() { return &generalAllocator; }
#endif
#ifdef NEED_MISC_ALLOCATOR
        ArenaAllocator* MiscAllocator() { return &miscAllocator; }
#endif
        InlineCacheAllocator* GetInlineCacheAllocator() { return &inlineCacheAllocator; }
        IsInstInlineCacheAllocator* GetIsInstInlineCacheAllocator() { return &isInstInlineCacheAllocator; }
        ArenaAllocator* DynamicProfileInfoAllocator() { return &dynamicProfileInfoAllocator; }

        ArenaAllocator* AllocatorForDiagnostics();

        Js::TempArenaAllocatorObject* GetTemporaryAllocator(LPCWSTR name);
        void ReleaseTemporaryAllocator(Js::TempArenaAllocatorObject* tempAllocator);
        Js::TempGuestArenaAllocatorObject* GetTemporaryGuestAllocator(LPCWSTR name);
        void ReleaseTemporaryGuestAllocator(Js::TempGuestArenaAllocatorObject* tempAllocator);

        bool EnsureInterpreterArena(ArenaAllocator **);
        void ReleaseInterpreterArena();

        ArenaAllocator* GetGuestArena() const
        {
            return guestArena;
        }

        void ReleaseGuestArena();

        Recycler* GetRecycler() const { return recycler; }
        RecyclerJavascriptNumberAllocator * GetNumberAllocator() { return &numberAllocator; }
#if ENABLE_NATIVE_CODEGEN
        NativeCodeGenerator * GetNativeCodeGenerator() const { return nativeCodeGen; }
#endif
#if ENABLE_BACKGROUND_PARSING
        BackgroundParser * GetBackgroundParser() const { return backgroundParser; }
#endif

        void OnScriptStart(bool isRoot, bool isScript);
        void OnScriptEnd(bool isRoot, bool isForcedEnd);

        template <bool stackProbe, bool leaveForHost>
        bool LeaveScriptStart(void * frameAddress);
        template <bool leaveForHost>
        void LeaveScriptEnd(void * frameAddress);

        HostScriptContext * GetHostScriptContext() const { return hostScriptContext; }
        void SetHostScriptContext(HostScriptContext *  hostScriptContext);
        void SetScriptEngineHaltCallback(HaltCallback* scriptEngine);
        void ClearHostScriptContext();

        IActiveScriptProfilerHeapEnum* GetHeapEnum();
        void SetHeapEnum(IActiveScriptProfilerHeapEnum* newHeapEnum);
        void ClearHeapEnum();

        void SetScriptStartEventHandler(EventHandler eventHandler);
        void SetScriptEndEventHandler(EventHandler eventHandler);
#ifdef FAULT_INJECTION
        void DisposeScriptContextByFaultInjection();
        void SetDisposeDisposeByFaultInjectionEventHandler(EventHandler eventHandler);
#endif
        EnumeratedObjectCache* GetEnumeratedObjectCache() { return &(cache->enumObjCache); }
        PropertyString* GetPropertyString(PropertyId propertyId);
        void InvalidatePropertyStringCache(PropertyId propertyId, Type* type);
        JavascriptString* GetIntegerString(Var aValue);
        JavascriptString* GetIntegerString(int value);
        JavascriptString* GetIntegerString(uint value);

        void CheckEvalRestriction();

        RecyclableObject* GetMissingPropertyResult(Js::RecyclableObject *instance, Js::PropertyId id);
        RecyclableObject* GetMissingItemResult(Js::RecyclableObject *instance, uint32 index);
        RecyclableObject* GetMissingParameterValue(Js::JavascriptFunction *function, uint32 paramIndex);
        RecyclableObject *GetNullPropertyResult(Js::RecyclableObject *instance, Js::PropertyId id);
        RecyclableObject *GetNullItemResult(Js::RecyclableObject *instance, uint32 index);

        bool HasRecordedException() const { return threadContext->GetRecordedException() != nullptr; }
        Js::JavascriptExceptionObject * GetAndClearRecordedException(bool *considerPassingToDebugger = nullptr);
        void RecordException(Js::JavascriptExceptionObject * exceptionObject, bool propagateToDebugger = false);
        __declspec(noreturn) void RethrowRecordedException(JavascriptExceptionObject::HostWrapperCreateFuncType hostWrapperCreateFunc);

#if ENABLE_NATIVE_CODEGEN
        BOOL IsNativeAddress(void * codeAddr);
#endif

        uint SaveSourceCopy(Utf8SourceInfo* sourceInfo, int cchLength, bool isCesu8);
        bool SaveSourceCopy(Utf8SourceInfo* const sourceInfo, int cchLength, bool isCesu8, uint * index);

        uint SaveSourceNoCopy(Utf8SourceInfo* sourceInfo, int cchLength, bool isCesu8);
        Utf8SourceInfo* CloneSourceCrossContext(Utf8SourceInfo* crossContextSourceInfo, SRCINFO const* srcInfo = nullptr);

        void CloneSources(ScriptContext* sourceContext);
        Utf8SourceInfo* GetSource(uint sourceIndex);

        uint SourceCount() const { return (uint)sourceList->Count(); }
        void CleanSourceList() { CleanSourceListInternal(false); }
        SourceList* GetSourceList() const { return sourceList; }
        bool IsItemValidInSourceList(int index);

        template <typename TFunction>
        void MapScript(TFunction mapper)
        {
            this->sourceList->Map([mapper] (int, RecyclerWeakReference<Utf8SourceInfo>* sourceInfoWeakReference)
            {
                Utf8SourceInfo* strongRef = sourceInfoWeakReference->Get();

                if (strongRef)
                {
                    mapper(strongRef);
                }
            });
        }

#ifdef CHECK_STACKWALK_EXCEPTION
        void SetIgnoreStackWalkException() {threadContext->GetScriptEntryExit()->ignoreStackWalkException = true; }
#endif

        // For debugging scenarios where execution will go to debugging manager and come back to engine again, enforce the current EER to have
        // 'hasCaller' property set, which will enable the stack walking across frames.
        // Do not call this directly, look for ENFORCE_ENTRYEXITRECORD_HASCALLER macro.
        void EnforceEERHasCaller() { threadContext->GetScriptEntryExit()->hasCaller = true; }

        void SetRaiseMessageToDebuggerFunction(RaiseMessageToDebuggerFunctionType function)
        {
            raiseMessageToDebuggerFunctionType = function;
        }

        void RaiseMessageToDebugger(DEBUG_EVENT_INFO_TYPE messageType, LPCWSTR message, LPCWSTR url)
        {
            if (raiseMessageToDebuggerFunctionType != nullptr)
            {
                raiseMessageToDebuggerFunctionType(this, messageType, message, url);
            }
        }

        void SetTransitionToDebugModeIfFirstSourceFn(TransitionToDebugModeIfFirstSourceFn function)
        {
            transitionToDebugModeIfFirstSourceFn = function;
        }

        void TransitionToDebugModeIfFirstSource(Utf8SourceInfo *sourceInfo)
        {
            if (transitionToDebugModeIfFirstSourceFn != nullptr)
            {
                transitionToDebugModeIfFirstSourceFn(this, sourceInfo);
            }
        }

        void AddSourceSize(size_t sourceSize)
        {
            this->sourceSize += sourceSize;
            this->threadContext->AddSourceSize(sourceSize);
        }

        size_t GetSourceSize()
        {
            return this->sourceSize;
        }

        BOOL SetDeferredBody(BOOL set)
        {
            bool old = this->deferredBody;
            this->deferredBody = !!set;
            return old;
        }

        BOOL GetDeferredBody(void) const
        {
            return this->deferredBody;
        }

    public:
        void RegisterAsScriptContextWithInlineCaches();
        void RegisterAsScriptContextWithIsInstInlineCaches();
        bool IsRegisteredAsScriptContextWithIsInstInlineCaches();
        void FreeLoopBody(void* codeAddress);
        void FreeFunctionEntryPoint(Js::JavascriptMethod method);

    private:
        void DoRegisterAsScriptContextWithInlineCaches();
        void DoRegisterAsScriptContextWithIsInstInlineCaches();
        uint CloneSource(Utf8SourceInfo* info);
    public:
        void RegisterProtoInlineCache(InlineCache *pCache, PropertyId propId);
        void InvalidateProtoCaches(const PropertyId propertyId);
        void InvalidateAllProtoCaches();
        void RegisterStoreFieldInlineCache(InlineCache *pCache, PropertyId propId);
        void InvalidateStoreFieldCaches(const PropertyId propertyId);
        void InvalidateAllStoreFieldCaches();
        void RegisterIsInstInlineCache(Js::IsInstInlineCache * cache, Js::Var function);
#if DBG
        bool IsIsInstInlineCacheRegistered(Js::IsInstInlineCache * cache, Js::Var function);
#endif
        void ClearInlineCaches();
        void ClearIsInstInlineCaches();
#ifdef PERSISTENT_INLINE_CACHES
        void ClearInlineCachesWithDeadWeakRefs();
#endif
        void ClearScriptContextCaches();
#if ENABLE_NATIVE_CODEGEN
        void RegisterConstructorCache(Js::PropertyId propertyId, Js::ConstructorCache* cache);
#endif

    public:
        void RegisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext();
    private:
        void DoRegisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext();
    public:
        void ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches();

    public:
        JavascriptString * GetLastNumberToStringRadix10(double value);
        void SetLastNumberToStringRadix10(double value, JavascriptString * str);
        bool GetLastUtcTimeFromStr(JavascriptString * str, double& dbl);
        void SetLastUtcTimeFromStr(JavascriptString * str, double value);
        bool IsNoContextSourceContextInfo(SourceContextInfo *sourceContextInfo) const
        {
            return sourceContextInfo == cache->noContextSourceContextInfo;
        }

        BOOL IsProfiling()
        {
            return (m_pProfileCallback != nullptr);
        }

        BOOL IsInProfileCallback()
        {
            return m_inProfileCallback;
        }

#if DBG
        SourceContextInfo const * GetNoContextSourceContextInfo() const { return cache->noContextSourceContextInfo; }

        int GetProfileSession()
        {
            AssertMsg(m_pProfileCallback != nullptr, "Asking for profile session when we aren't in one.");
            return m_iProfileSession;
        }

        void StartNewProfileSession()
        {
            AssertMsg(m_pProfileCallback != nullptr, "New Session when the profiler isn't set to any callback.");
            m_iProfileSession++;
        }

        void StopProfileSession()
        {
            AssertMsg(m_pProfileCallback == nullptr, "How to stop when there is still the callback out there");
        }

        bool hadProfiled;
        bool HadProfiled() const { return hadProfiled; }
#endif

        SRCINFO *AddHostSrcInfo(SRCINFO const *pSrcInfo);

        inline void CoreSetProfileEventMask(DWORD dwEventMask);
        typedef HRESULT (*RegisterExternalLibraryType)(Js::ScriptContext *pScriptContext);
        HRESULT RegisterProfileProbe(IActiveScriptProfilerCallback *pProfileCallback, DWORD dwEventMask, DWORD dwContext, RegisterExternalLibraryType RegisterExternalLibrary, JavascriptMethod dispatchInvoke);
        HRESULT SetProfileEventMask(DWORD dwEventMask);
        HRESULT DeRegisterProfileProbe(HRESULT hrReason, JavascriptMethod dispatchInvoke);

        HRESULT RegisterScript(Js::FunctionProxy *pFunctionBody, BOOL fRegisterScript = TRUE);

        // Register static and dynamic scripts
        HRESULT RegisterAllScripts();

        // Iterate through utf8sourceinfo and clear debug document if they are there.
        void EnsureClearDebugDocument();

        // To be called directly only when the thread context is shutting down
        void ShutdownClearSourceLists();

        HRESULT RegisterLibraryFunction(const wchar_t *pwszObjectName, const wchar_t *pwszFunctionName, Js::PropertyId functionPropertyId, JavascriptMethod entryPoint);

        HRESULT RegisterBuiltinFunctions(RegisterExternalLibraryType RegisterExternalLibrary);
        void RegisterDebugThunk(bool calledDuringAttach = true);
        void UnRegisterDebugThunk();

        void UpdateRecyclerFunctionEntryPointsForDebugger();
        void SetFunctionInRecyclerToProfileMode(bool enumerateNonUserFunctionsOnly = false);
        static void SetEntryPointToProfileThunk(JavascriptFunction* function);
        static void RestoreEntryPointFromProfileThunk(JavascriptFunction* function);

        static void RecyclerEnumClassEnumeratorCallback(void *address, size_t size);
        static void RecyclerFunctionCallbackForDebugger(void *address, size_t size);

        static ushort ProcessNameAndGetLength(Js::StringBuilder<ArenaAllocator>* nameBuffer, const WCHAR* name);

#ifdef ASMJS_PLAT
        void TransitionEnvironmentForDebugger(ScriptFunction * scriptFunction);
#endif

#if ENABLE_NATIVE_CODEGEN
        HRESULT RecreateNativeCodeGenerator();
#endif

        HRESULT OnDebuggerAttached();
        HRESULT OnDebuggerDetached();
        HRESULT OnDebuggerAttachedDetached(bool attach);
        void InitializeDebugging();
        bool IsForceNoNative();
        bool IsEnumeratingRecyclerObjects() const { return isEnumeratingRecyclerObjects; }

    private:
        class AutoEnumeratingRecyclerObjects
        {
        public:
            AutoEnumeratingRecyclerObjects(ScriptContext* scriptContext):
                m_scriptContext(scriptContext)
            {
                Assert(!m_scriptContext->IsEnumeratingRecyclerObjects());
                m_scriptContext->isEnumeratingRecyclerObjects = true;
            }

            ~AutoEnumeratingRecyclerObjects()
            {
                Assert(m_scriptContext->IsEnumeratingRecyclerObjects());
                m_scriptContext->isEnumeratingRecyclerObjects = false;
            }

        private:
            ScriptContext* m_scriptContext;
        };

#ifdef EDIT_AND_CONTINUE
    private:
        ScriptEditQuery* activeScriptEditQuery;

        void BeginScriptEditEnumFunctions(ScriptEditQuery* scriptEditQuery) { Assert(!activeScriptEditQuery); activeScriptEditQuery = scriptEditQuery; }
        void EndScriptEditEnumFunctions() { Assert(activeScriptEditQuery); activeScriptEditQuery = nullptr; }
    public:
        ScriptEditQuery* GetActiveScriptEditQuery() const { return activeScriptEditQuery; }

        class AutoScriptEditEnumFunctions
        {
        public:
            AutoScriptEditEnumFunctions(ScriptContext* scriptContext, ScriptEditQuery* scriptEditQuery) : m_scriptContext(scriptContext)
            {
                scriptContext->BeginScriptEditEnumFunctions(scriptEditQuery);
            }
            ~AutoScriptEditEnumFunctions() { m_scriptContext->EndScriptEditEnumFunctions(); }
        private:
            ScriptContext* m_scriptContext;
        };
#endif

    private:
        typedef JsUtil::BaseDictionary<JavascriptMethod, Js::PropertyId, ArenaAllocator, PrimeSizePolicy> BuiltinFunctionIdDictionary;
        BuiltinFunctionIdDictionary *m_pBuiltinFunctionIdMap;
        Js::PropertyId GetFunctionNumber(JavascriptMethod entryPoint);

        static const wchar_t* CopyString(const wchar_t* str, size_t charCount, ArenaAllocator* alloc);
        static charcount_t AppendWithEscapeCharacters(Js::StringBuilder<ArenaAllocator>* stringBuilder, const WCHAR* sourceString, charcount_t sourceStringLen, WCHAR escapeChar, WCHAR charToEscape);

    public:
#if DYNAMIC_INTERPRETER_THUNK
        JavascriptMethod GetNextDynamicAsmJsInterpreterThunk(PVOID* ppDynamicInterpreterThunk);
        JavascriptMethod GetNextDynamicInterpreterThunk(PVOID* ppDynamicInterpreterThunk);
        BOOL IsDynamicInterpreterThunk(void* address);
        void ReleaseDynamicInterpreterThunk(BYTE* address, bool addtoFreeList);
        void ReleaseDynamicAsmJsInterpreterThunk(BYTE* address, bool addtoFreeList);
#endif

        void SetProfileMode(BOOL fSet);
        static JavascriptMethod GetProfileModeThunk(JavascriptMethod entryPoint);
        static Var ProfileModeThunk_DebugModeWrapper(JavascriptFunction* function, ScriptContext* scriptContext, JavascriptMethod entryPoint, Arguments& args);
        BOOL GetProfileInfo(
            JavascriptFunction* function,
            PROFILER_TOKEN &scriptId,
            PROFILER_TOKEN &functionId);
        static Var DebugProfileProbeThunk(RecyclableObject* function, CallInfo callInfo, ...);
        static JavascriptMethod ProfileModeDeferredParse(ScriptFunction **function);
        static Var ProfileModeDeferredParsingThunk(RecyclableObject* function, CallInfo callInfo, ...);

        // Thunks for deferred deserialization of function bodies from the byte code cache
        static JavascriptMethod ProfileModeDeferredDeserialize(ScriptFunction* function);
        static Var ProfileModeDeferredDeserializeThunk(RecyclableObject* function, CallInfo callInfo, ...);

        HRESULT OnScriptCompiled(PROFILER_TOKEN scriptId, PROFILER_SCRIPT_TYPE type, IUnknown *pIDebugDocumentContext);
        HRESULT OnFunctionCompiled(
            PROFILER_TOKEN functionId,
            PROFILER_TOKEN scriptId,
            const WCHAR *pwszFunctionName,
            const WCHAR *pwszFunctionNameHint,
            IUnknown *pIDebugDocumentContext);
        HRESULT OnFunctionEnter(PROFILER_TOKEN scriptId, PROFILER_TOKEN functionId);
        HRESULT OnFunctionExit(PROFILER_TOKEN scriptId, PROFILER_TOKEN functionId);

        bool SetDispatchProfile(bool fSet, JavascriptMethod dispatchInvoke);
        HRESULT OnDispatchFunctionEnter(const WCHAR *pwszFunctionName);
        HRESULT OnDispatchFunctionExit(const WCHAR *pwszFunctionName);

        void OnStartupComplete();
        void SaveStartupProfileAndRelease(bool isSaveOnClose = false);

        static HRESULT FunctionExitSenderThunk(PROFILER_TOKEN functionId, PROFILER_TOKEN scriptId, ScriptContext *pScriptContext);
        static HRESULT FunctionExitByNameSenderThunk(const wchar_t *pwszFunctionName, ScriptContext *pScriptContext);

#if ENABLE_PROFILE_INFO
        void AddDynamicProfileInfo(FunctionBody * functionBody, WriteBarrierPtr<DynamicProfileInfo>* dynamicProfileInfo);
#endif
#if DBG || defined(RUNTIME_DATA_COLLECTION)
        uint allocId;
#endif

#ifdef PROFILE_EXEC
        void DisableProfiler();
        void SetRecyclerProfiler();
        void SetProfilerFromScriptContext(ScriptContext * scriptContext);
        void ProfileBegin(Js::Phase);
        void ProfileEnd(Js::Phase);
        void ProfileSuspend(Js::Phase, Js::Profiler::SuspendRecord * suspendRecord);
        void ProfileResume(Js::Profiler::SuspendRecord * suspendRecord);
        void ProfilePrint();
        bool IsProfilerCreated() const { return isProfilerCreated; }
#endif

#ifdef PROFILE_MEM
        void DisableProfileMemoryDumpOnDelete() { profileMemoryDump = false; }
#endif

#ifdef PROFILE_STRINGS
        StringProfiler * GetStringProfiler(); // May be null if string profiling not enabled
#endif

    public:
        void SetBuiltInLibraryFunction(JavascriptMethod entryPoint, JavascriptFunction* function);
        JavascriptFunction* GetBuiltInLibraryFunction(JavascriptMethod entryPoint);

    private:
        typedef JsUtil::BaseDictionary<JavascriptMethod, JavascriptFunction*, Recycler, PowerOf2SizePolicy> BuiltInLibraryFunctionMap;
        BuiltInLibraryFunctionMap* builtInLibraryFunctions;

#ifdef RECYCLER_PERF_COUNTERS
        size_t bindReferenceCount;
#endif

        ScriptContext * nextPendingClose;
    public:
        void SetNextPendingClose(ScriptContext * nextPendingClose);
        inline ScriptContext * GetNextPendingClose() const { return nextPendingClose; }

#ifdef ENABLE_MUTATION_BREAKPOINT
        // Keep track of all breakpoints in order to properly clean up on debugger detach
        bool HasMutationBreakpoints();
        void InsertMutationBreakpoint(Js::MutationBreakpoint *mutationBreakpoint);
#endif
    };

    class AutoDynamicCodeReference
    {
    public:
        AutoDynamicCodeReference(ScriptContext* scriptContext):
          m_scriptContext(scriptContext)
          {
              scriptContext->BeginDynamicFunctionReferences();
          }

          ~AutoDynamicCodeReference()
          {
              m_scriptContext->EndDynamicFunctionReferences();
          }

    private:
        ScriptContext* m_scriptContext;
    };

    template <typename TCacheType>
    void ScriptContext::CleanDynamicFunctionCache(TCacheType* cacheType)
    {
        // Remove eval map functions that haven't been recently used
        // TODO: Metric based on allocation size too? So don't clean if there hasn't been much allocated?

        cacheType->Clean([this](const TCacheType::KeyType& key, TCacheType::ValueType value) {
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            if (CONFIG_FLAG(DumpEvalStringOnRemoval))
            {
                Output::Print(L"EvalMap: Removing Dynamic Function String from dynamic function cache: %s\n", key.str.GetBuffer()); Output::Flush();
            }
#endif
        });
    }

    template <class TDelegate>
    void ScriptContext::MapFunction(TDelegate mapper)
    {
        if (this->sourceList)
        {
            this->sourceList->Map([&mapper](int, RecyclerWeakReference<Js::Utf8SourceInfo>* sourceInfo)
            {
                Utf8SourceInfo* sourceInfoStrongRef = sourceInfo->Get();
                if (sourceInfoStrongRef)
                {
                    sourceInfoStrongRef->MapFunction(mapper);
                }
            });
        }
    }

    template <class TDelegate>
    FunctionBody* ScriptContext::FindFunction(TDelegate predicate)
    {
        FunctionBody* functionBody = nullptr;

        this->sourceList->MapUntil([&functionBody, &predicate](int, RecyclerWeakReference<Js::Utf8SourceInfo>* sourceInfo) -> bool
        {
            Utf8SourceInfo* sourceInfoStrongRef = sourceInfo->Get();
            if (sourceInfoStrongRef)
            {
                functionBody = sourceInfoStrongRef->FindFunction(predicate);
                if (functionBody)
                {
                    return true;
                }
            }

            return false;
        });

        return functionBody;
    }
}

#define BEGIN_TEMP_ALLOCATOR(allocator, scriptContext, name) \
    Js::TempArenaAllocatorObject *temp##allocator = scriptContext->GetTemporaryAllocator(name); \
    ArenaAllocator * allocator = temp##allocator->GetAllocator();

#define END_TEMP_ALLOCATOR(allocator, scriptContext) \
    scriptContext->ReleaseTemporaryAllocator(temp##allocator);

#define DECLARE_TEMP_ALLOCATOR(allocator) \
    Js::TempArenaAllocatorObject *temp##allocator = nullptr; \
    ArenaAllocator * allocator = nullptr;

#define ACQUIRE_TEMP_ALLOCATOR(allocator, scriptContext, name) \
    temp##allocator = scriptContext->GetTemporaryAllocator(name); \
    allocator = temp##allocator->GetAllocator();

#define RELEASE_TEMP_ALLOCATOR(allocator, scriptContext) \
    if (temp##allocator) \
    scriptContext->ReleaseTemporaryAllocator(temp##allocator);

#define DECLARE_TEMP_GUEST_ALLOCATOR(allocator) \
    Js::TempGuestArenaAllocatorObject *tempGuest##allocator = nullptr; \
    ArenaAllocator * allocator = nullptr;

#define ACQUIRE_TEMP_GUEST_ALLOCATOR(allocator, scriptContext, name) \
    tempGuest##allocator = scriptContext->GetTemporaryGuestAllocator(name); \
    allocator = tempGuest##allocator->GetAllocator();

#define RELEASE_TEMP_GUEST_ALLOCATOR(allocator, scriptContext) \
    if (tempGuest##allocator) \
    scriptContext->ReleaseTemporaryGuestAllocator(tempGuest##allocator);
