//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ScriptContext;
    struct InlineCache;
    class DebugManager;
    class CodeGenRecyclableData;
    struct ReturnedValue;
    typedef JsUtil::List<ReturnedValue*> ReturnedValueList;
}

struct IAuthorFileContext;

class HostScriptContext;
class ScriptSite;
class ThreadServiceWrapper;
struct IActiveScriptProfilerHeapEnum;
class DynamicProfileMutator;
class StackProber;

enum DisableImplicitFlags : BYTE
{
    DisableImplicitNoFlag               = 0x00,
    DisableImplicitCallFlag             = 0x01,
    DisableImplicitExceptionFlag        = 0x02,
    DisableImplicitCallAndExceptionFlag = DisableImplicitCallFlag | DisableImplicitExceptionFlag
};

enum ThreadContextFlags
{
    ThreadContextFlagNoFlag                        = 0x00000000,
    ThreadContextFlagCanDisableExecution           = 0x00000001,
    ThreadContextFlagEvalDisabled                  = 0x00000002,
    ThreadContextFlagNoJIT                         = 0x00000004,
};

const int LS_MAX_STACK_SIZE_KB = 300;

struct IProjectionContext
{
public:
    virtual HRESULT Close() = 0;
};

class ThreadContext;

class InterruptPoller abstract
{
    // Interface with a polling object located in the hosting layer.

public:
    InterruptPoller(ThreadContext *tc);

    void CheckInterruptPoll();
    void GetStatementCount(ULONG *pluHi, ULONG *pluLo);
    void ResetStatementCount() { lastResetTick = lastPollTick; }
    void StartScript() { lastResetTick = lastPollTick = ::GetTickCount(); }
    void EndScript() { lastResetTick = lastPollTick = 0;}
    bool IsDisabled() const { return isDisabled; }
    void SetDisabled(bool disable) { isDisabled = disable; }

    virtual void TryInterruptPoll(Js::ScriptContext *scriptContext) = 0;

    // Default: throw up QC dialog after 5M statements == 2 minutes
    static const DWORD TicksToStatements = (5000000 / 120000);

protected:
    ThreadContext *threadContext;
    DWORD lastPollTick;
    DWORD lastResetTick;
    bool isDisabled;
};

class AutoDisableInterrupt
{
private:
    InterruptPoller* interruptPoller;
    bool previousState;
public:
    AutoDisableInterrupt(InterruptPoller* interruptPoller, bool disable)
        : interruptPoller(interruptPoller)
    {
        if (interruptPoller != nullptr)
        {
            previousState = interruptPoller->IsDisabled();
            interruptPoller->SetDisabled(disable);
        }
    }
    ~AutoDisableInterrupt()
    {
        if (interruptPoller != nullptr)
        {
            interruptPoller->SetDisabled(previousState);
        }
    }
};

// This function is called before we step out of script (currently only for WinRT callout).
// Debugger would put a breakpoint on this function if they want to detect the point at which we step
// over the boundary.
// It is intentionally left blank and the next operation should be the callout.
extern "C" void* MarkerForExternalDebugStep();

#define PROBE_STACK(scriptContext, size) ((scriptContext)->GetThreadContext()->ProbeStack(size, scriptContext))
#define PROBE_STACK_NO_DISPOSE(scriptContext, size) ((scriptContext)->GetThreadContext()->ProbeStackNoDispose(size, scriptContext))
#define PROBE_STACK_PARTIAL_INITIALIZED_INTERPRETER_FRAME(scriptContext, size) ((scriptContext)->GetThreadContext()->ProbeStack(size, scriptContext, _ReturnAddress()))
#define PROBE_STACK_PARTIAL_INITIALIZED_BAILOUT_FRAME(scriptContext, size, returnAddress) ((scriptContext)->GetThreadContext()->ProbeStack(size, scriptContext, returnAddress))
#define PROBE_STACK_CALL(scriptContext, obj, size) ((scriptContext)->GetThreadContext()->ProbeStack(size, obj, scriptContext))
#define AssertInScript() Assert(ThreadContext::GetContextForCurrentThread()->IsScriptActive());
#define AssertNotInScript() Assert(!ThreadContext::GetContextForCurrentThread()->IsScriptActive());

#define LEAVE_SCRIPT_START_EX(scriptContext, stackProbe, leaveForHost, isFPUControlRestoreNeeded) \
        { \
            void * __frameAddr = nullptr; \
            GET_CURRENT_FRAME_ID(__frameAddr); \
            Js::LeaveScriptObject<stackProbe, leaveForHost, isFPUControlRestoreNeeded> __leaveScriptObject(scriptContext, __frameAddr);

#define LEAVE_SCRIPT_END_EX(scriptContext) \
            if (scriptContext != nullptr) \
                {   \
                    scriptContext->GetThreadContext()->DisposeOnLeaveScript(); \
                }\
        }

#define BEGIN_LEAVE_SCRIPT(scriptContext) \
        LEAVE_SCRIPT_START_EX(scriptContext, /* stackProbe */ true, /* leaveForHost */ true, /* isFPUControlRestoreNeeded */ false)

#define BEGIN_LEAVE_SCRIPT_SAVE_FPU_CONTROL(scriptContext) \
        LEAVE_SCRIPT_START_EX(scriptContext, /* stackProbe */ true, /* leaveForHost */ true, /* isFPUControlRestoreNeeded */ true)

// BEGIN_LEAVE_SCRIPT_INTERNAL is used when there are no explicit external call after leave script,
// but we might have external call when allocation memory doing QC or GC Dispose, which may enter script again.
// This will record the reentry as an implicit call (ImplicitCall_AsyncHostOperation)
#define BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext) \
        LEAVE_SCRIPT_START_EX(scriptContext, /* stackProbe */ true, /* leaveForHost */ false, /* isFPUControlRestoreNeeded */ false)

#define BEGIN_LEAVE_SCRIPT_NO_STACK_PROBE(scriptContext) \
        LEAVE_SCRIPT_START_EX(scriptContext, /* stackProbe */ false, /* leaveForHost */ true, /* isFPUControlRestoreNeeded */ false)

#define END_LEAVE_SCRIPT(scriptContext) \
        LEAVE_SCRIPT_END_EX(scriptContext)

#define END_LEAVE_SCRIPT_RESTORE_FPU_CONTROL(scriptContext) \
        LEAVE_SCRIPT_END_EX(scriptContext)

#define END_LEAVE_SCRIPT_INTERNAL(scriptContext) \
        LEAVE_SCRIPT_END_EX(scriptContext)

#define END_LEAVE_SCRIPT_NO_STACK_PROBE(scriptContext) \
        LEAVE_SCRIPT_END_EX(scriptContext)

#define BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext) \
        BEGIN_LEAVE_SCRIPT(scriptContext)

#define END_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext) \
        Assert(!scriptContext->HasRecordedException()); \
        END_LEAVE_SCRIPT(scriptContext)

// Keep in sync with CollectGarbageCallBackFlags in scriptdirect.idl

enum RecyclerCollectCallBackFlags
{
    Collect_Begin                    = 0x01,
    Collect_Begin_Concurrent         = 0x11,
    Collect_Begin_Partial            = 0x21,
    Collect_Begin_Concurrent_Partial = Collect_Begin_Concurrent | Collect_Begin_Partial,
    Collect_End                      = 0x02,
    Collect_Wait                     = 0x04     // callback can be from another thread
};
typedef void (__cdecl *RecyclerCollectCallBackFunction)(void * context, RecyclerCollectCallBackFlags flags);

// Keep in sync with WellKnownType in scriptdirect.idl

typedef enum WellKnownHostType
{
    WellKnownHostType_HTMLAllCollection     = 0,
    WellKnownHostType_Last                  = WellKnownHostType_HTMLAllCollection,
    WellKnownHostType_Invalid               = WellKnownHostType_Last+1
} WellKnownHostType;

#ifdef ENABLE_PROJECTION
class ExternalWeakReferenceCache
{
public:
    virtual void MarkNow(Recycler *recycler, bool inPartialCollect) = 0;
    virtual void ResolveNow(Recycler *recycler) = 0;
};
#if DBG_DUMP
class IProjectionContextMemoryInfo abstract
{
public:
    virtual void DumpCurrentStats(LPCWSTR headerMsg, bool forceDetailed) = 0;
    virtual void Release() = 0;
};
#endif
#endif

struct ThreadContextWatsonTelemetryBlock
{
    FILETIME lastScriptStartTime;
    FILETIME lastScriptEndTime;
};

class NativeLibraryEntryRecord
{
public:
    struct Entry
    {
        Js::RecyclableObject* function;
        Js::CallInfo callInfo;
        PCWSTR name;
        PVOID addr;
        Entry* next;
    };

private:
    Entry* head;

public:
    NativeLibraryEntryRecord() : head(nullptr)
    {
    }

    const Entry* Peek() const
    {
        return head;
    }

    void Push(_In_ Entry* e)
    {
        e->next = head;
        head = e;
    }

    void Pop()
    {
        head = head->next;
    }
};

class AutoTagNativeLibraryEntry
{
private:
    NativeLibraryEntryRecord::Entry entry;

public:
    AutoTagNativeLibraryEntry(Js::RecyclableObject* function, Js::CallInfo callInfo, PCWSTR name, void* addr);
    ~AutoTagNativeLibraryEntry();
};

struct JITStats
{
    uint lessThan5ms;
    uint within5And10ms;
    uint within10And20ms;
    uint within20And50ms;
    uint within50And100ms;
    uint within100And300ms;
    uint greaterThan300ms;
};

struct ParserStats
{
    uint64 lessThan1ms;
    uint64 within1And3ms;
    uint64 within3And10ms;
    uint64 within10And20ms;
    uint64 within20And50ms;
    uint64 within50And100ms;
    uint64 within100And300ms;
    uint64 greaterThan300ms;
};

class ParserTimer
{
private:
    Js::HiResTimer timer;
    ParserStats stats;
public:
    ParserTimer();
    ParserStats GetStats();
    void Reset();
    double Now();
    void LogTime(double ms);
};


class JITTimer
{
private:
    Js::HiResTimer timer;
    JITStats stats;
public:
    JITTimer();
    JITStats GetStats();
    void Reset();
    double Now();
    void LogTime(double ms);
};

#define AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, name) \
    AutoTagNativeLibraryEntry __tag(function, callInfo, name, _AddressOfReturnAddress())

class ThreadConfiguration
{
public:
    ThreadConfiguration(bool enableExperimentalFeatures)
    {
        CopyGlobalFlags();
        if (enableExperimentalFeatures)
        {
            EnableExperimentalFeatures();
        }
    }

#define DEFINE_FLAG(threadFlag, globalFlag) \
    public: \
        inline bool threadFlag() const { return m_##globalFlag##; } \
    \
    private: \
        bool m_##globalFlag##;
#define FLAG(threadFlag, globalFlag) DEFINE_FLAG(threadFlag, globalFlag)
#define FLAG_RELEASE(threadFlag, globalFlag) DEFINE_FLAG(threadFlag, globalFlag)
#include "ThreadConfigFlagsList.h"
#undef FLAG_RELEASE
#undef FLAG
#undef DEFINE_FLAG

private:
    void CopyGlobalFlags()
    {
        AutoCriticalSection autocs(&Js::Configuration::Global.flags.csExperimentalFlags);

#define FLAG(threadFlag, globalFlag) m_##globalFlag## = CONFIG_FLAG(globalFlag);
#define FLAG_RELEASE(threadFlag, globalFlag) m_##globalFlag## = CONFIG_FLAG_RELEASE(globalFlag);
#include "ThreadConfigFlagsList.h"
#undef FLAG_RELEASE
#undef FLAG
    }

    void EnableExperimentalFeatures()
    {
#define FLAG_REGOVR_EXP(type, name, ...) m_##name## = true;
#include "ConfigFlagsList.h"
#undef FLAG_REGOVR_EXP
    }
};

class ThreadContext sealed :
    public DefaultRecyclerCollectionWrapper,
    public JsUtil::DoublyLinkedListElement<ThreadContext>
{
public:
    static void GlobalInitialize();
    static const DWORD NoThread = 0xFFFFFFFF;

    struct CollectCallBack
    {
        RecyclerCollectCallBackFunction callback;
        void * context;
    };

    struct WorkerThread
    {
        // Abstract notion to hold onto threadHandle of worker thread
        HANDLE threadHandle;
        WorkerThread(HANDLE handle = nullptr) :threadHandle(handle){};
    };

    void ReleasePreReservedSegment();
    void IncrementThreadContextsWithPreReservedSegment();

    void SetCurrentThreadId(DWORD threadId) { this->currentThreadId = threadId; }
    DWORD GetCurrentThreadId() const { return this->currentThreadId; }
    void SetIsThreadBound()
    {
        if (this->recycler)
        {
            this->recycler->SetIsThreadBound();
        }
        this->isThreadBound = true;
    }
    bool GetIsThreadBound() const { return this->isThreadBound; }
    void SetStackProber(StackProber * stackProber);
    PBYTE GetScriptStackLimit() const;
    static DWORD GetStackLimitForCurrentThreadOffset() { return offsetof(ThreadContext, stackLimitForCurrentThread); }
    void * GetAddressOfStackLimitForCurrentThread()
    {
        FAULTINJECT_SCRIPT_TERMINATION

        return &this->stackLimitForCurrentThread;
    }
    void InitAvailableCommit();

    // This is always on for JSRT APIs.
    bool IsRentalThreadingEnabledInJSRT() const { return true; }

    IActiveScriptProfilerHeapEnum* GetHeapEnum();
    void SetHeapEnum(IActiveScriptProfilerHeapEnum* newHeapEnum);
    void ClearHeapEnum();

#ifdef ENABLE_BASIC_TELEMETRY
    Js::LanguageStats* GetLanguageStats()
    {
        return langTel.GetLanguageStats();
    }

    void ResetLangStats()
    {
        this->langTel.Reset();
    }
#endif

    bool CanPreReserveSegmentForCustomHeap();

#if ENABLE_NATIVE_CODEGEN
    // used by inliner. Maps Simd FuncInfo (library func) to equivalent opcode.
    typedef JsUtil::BaseDictionary<Js::FunctionInfo *, Js::OpCode, ArenaAllocator> FuncInfoToOpcodeMap;
    FuncInfoToOpcodeMap * simdFuncInfoToOpcodeMap;

    struct SimdFuncSignature
    {
        bool valid;
        uint argCount;          // actual arguments count (excluding this)
        ValueType returnType;
        ValueType *args;        // argument types
    };

    SimdFuncSignature *simdOpcodeToSignatureMap;

    void AddSimdFuncToMaps(Js::OpCode op, ...);
    Js::OpCode GetSimdOpcodeFromFuncInfo(Js::FunctionInfo * funcInfo);
    void GetSimdFuncSignatureFromOpcode(Js::OpCode op, SimdFuncSignature &funcSignature);
#endif

private:
    bool noScriptScope;

    Js::DebugManager * debugManager;

    static uint const MaxTemporaryArenaAllocators = 5;

    static CriticalSection s_csThreadContext;

    StackProber * GetStackProber() const { return this->stackProber; }
    PBYTE GetStackLimitForCurrentThread() const;
    void SetStackLimitForCurrentThread(PBYTE limit);

#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
    static uint numOfThreadContextsWithPreReserveSegment;
#endif

    // The current heap enumeration object being used during enumeration.
    IActiveScriptProfilerHeapEnum* heapEnum;

#ifdef ENABLE_BASIC_TELEMETRY
    Js::LanguageTelemetry langTel;
#endif

    struct PropertyGuardEntry
    {
    public:
        typedef JsUtil::BaseHashSet<RecyclerWeakReference<Js::PropertyGuard>*, Recycler, PowerOf2SizePolicy> PropertyGuardHashSet;
        // we do not have WeaklyReferencedKeyHashSet - hence use BYTE as a dummy value.
        typedef JsUtil::WeaklyReferencedKeyDictionary<Js::EntryPointInfo, BYTE> EntryPointDictionary;
        // The sharedGuard is strongly referenced and will be kept alive by ThreadContext::propertyGuards until it's invalidated or
        // the property record itself is collected.  If the code using the guard needs access to it after it's been invalidated, it
        // (the code) is responsible for keeping it alive.  Each unique guard, is weakly referenced, such that it can be reclaimed
        // if not referenced elsewhere even without being invalidated.  It's up to the owner of that guard to keep it alive as long
        // as necessary.
        Js::PropertyGuard* sharedGuard;
        PropertyGuardHashSet uniqueGuards;
        EntryPointDictionary* entryPoints;

        PropertyGuardEntry(Recycler* recycler) : sharedGuard(nullptr), uniqueGuards(recycler), entryPoints(nullptr) {}
    };

public:
    typedef JsUtil::BaseHashSet<const Js::PropertyRecord *, HeapAllocator, PrimeSizePolicy, const Js::PropertyRecord *,
        Js::PropertyRecordStringHashComparer, JsUtil::SimpleHashedEntry, JsUtil::AsymetricResizeLock> PropertyMap;

    typedef JsUtil::BaseHashSet<Js::CaseInvariantPropertyListWithHashCode*, Recycler, PowerOf2SizePolicy, Js::CaseInvariantPropertyListWithHashCode*, JsUtil::NoCaseComparer, JsUtil::SimpleDictionaryEntry>
        PropertyNoCaseSetType;
    typedef JsUtil::WeaklyReferencedKeyDictionary<Js::Type, bool> TypeHashSet;
    typedef JsUtil::BaseDictionary<Js::PropertyId, TypeHashSet *, Recycler, PowerOf2SizePolicy> PropertyIdToTypeHashSetDictionary;
    typedef JsUtil::WeaklyReferencedKeyDictionary<const Js::PropertyRecord, PropertyGuardEntry*, Js::PropertyRecordPointerComparer> PropertyGuardDictionary;

private:
    typedef JsUtil::BaseDictionary<uint, Js::SourceDynamicProfileManager*, Recycler, PowerOf2SizePolicy> SourceDynamicProfileManagerMap;
    typedef JsUtil::BaseDictionary<const wchar_t*, const Js::PropertyRecord*, Recycler, PowerOf2SizePolicy> SymbolRegistrationMap;

    class SourceDynamicProfileManagerCache
    {
    public:
        SourceDynamicProfileManagerCache() : refCount(0), sourceProfileManagerMap(nullptr) {}

        SourceDynamicProfileManagerMap* sourceProfileManagerMap;
        void AddRef() { refCount++; }
        uint Release() { Assert(refCount > 0); return --refCount; }
    private:
        uint refCount;              // For every script context using this cache, there is a ref count added.
    };

    typedef JsUtil::BaseDictionary<const WCHAR*, SourceDynamicProfileManagerCache*, Recycler, PowerOf2SizePolicy> SourceProfileManagersByUrlMap;

    struct RecyclableData
    {
        RecyclableData(Recycler *const recycler);
        Js::TempArenaAllocatorObject * temporaryArenaAllocators[MaxTemporaryArenaAllocators];
        Js::TempGuestArenaAllocatorObject * temporaryGuestArenaAllocators[MaxTemporaryArenaAllocators];

        Js::JavascriptExceptionObject * exceptionObject;
        bool propagateException;

        // We throw a JS catchable SO exception if we detect we might overflow the stack. Allocating this (JS)
        // object though might really overflow the stack. So use this thread global to identify them from the throw point
        // to where they are caught; where the stack has been unwound and it is safer to allocate the real exception
        // object and throw.
        Js::JavascriptExceptionObject soErrorObject;

        // We can't allocate an out of memory object...  So use this static as a way to identify
        // them from the throw point to where they are caught.
        Js::JavascriptExceptionObject oomErrorObject;

        // This is for JsRT scenario where a runtime is not usable after a suspend request, before a resume runtime call is made
        Js::JavascriptExceptionObject terminatedErrorObject;

        Js::JavascriptExceptionObject* unhandledExceptionObject;

        // Contains types that have property caches that need to be tracked, as the caches may need to be cleared. Types that
        // contain a property cache for a property that is on a prototype object will be tracked in this map since those caches
        // need to be cleared if for instance, the property is deleted from the prototype object.
        //
        // It is expected that over time, types that are deleted will eventually be removed by the weak reference hash sets when
        // they're searching through a bucket while registering a type or enumerating types to invalidate, or when a property ID
        // is reclaimed. If none of those happen, then this collection may contain weak reference handles to deleted objects
        // that would not get removed, but it would also not get any bigger.
        PropertyIdToTypeHashSetDictionary typesWithProtoPropertyCache;

        // The property guard dictionary contains property guards which need to be invalidated in response to properties changing
        // from writable to read-only and vice versa, properties being shadowed or unshadowed on prototypes, etc.  The dictionary
        // holds only weak references to property guards and their lifetimes are controlled by their creators (typically entry points).
        // When a guard is no longer needed it is garbage collected, but the weak references and dictionary entries remain, until
        // the guards for a given property get invalidated.
        // TODO: Create and use a self-cleaning weak reference dictionary, which would periodically remove any unused weak references.
        PropertyGuardDictionary propertyGuards;


        PropertyNoCaseSetType * caseInvariantPropertySet;

        JsUtil::List<Js::PropertyRecord const*>* boundPropertyStrings; // Recycler allocated list of property strings that we need to strongly reference so that they're not reclaimed

        SourceProfileManagersByUrlMap* sourceProfileManagersByUrl;

        // Used to register recyclable data that needs to be kept alive while jitting
        JsUtil::DoublyLinkedList<Js::CodeGenRecyclableData> codeGenRecyclableDatas;

        // Used to root old entry points so that they're not prematurely collected
        Js::FunctionEntryPointInfo* oldEntryPointInfo;

        // Used to store a mapping of string to Symbol for cross-realm Symbol registration
        // See ES6 (draft 22) 19.4.2.2
        SymbolRegistrationMap* symbolRegistrationMap;

        // Just holding the reference to the returnedValueList of the stepController. This way that list will not get recycled prematurely.
        Js::ReturnedValueList *returnedValueList;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        // use for autoProxy called from Debug.setAutoProxyName. we need to keep the buffer from GetSz() alive.
        LPCWSTR autoProxyName;
#endif
    };

    static ThreadContext * globalListLast;

    ThreadContextFlags threadContextFlags;
    DWORD currentThreadId;
    mutable PBYTE stackLimitForCurrentThread;
    StackProber * stackProber;
    bool isThreadBound;
    bool hasThrownPendingException;
    bool callDispose;
    bool isAllJITCodeInPreReservedRegion;

    AllocationPolicyManager * allocationPolicyManager;

    JsUtil::ThreadService threadService;
    PreReservedVirtualAllocWrapper preReservedVirtualAllocator;

    uint callRootLevel;

    // The thread page allocator is used by the recycler and need the background page queue
    PageAllocator::BackgroundPageQueue backgroundPageQueue;
    IdleDecommitPageAllocator pageAllocator;
    Recycler* recycler;

    // Fake RecyclerWeakReference for built-in properties
    class StaticPropertyRecordReference : public RecyclerWeakReference<const Js::PropertyRecord>
    {
    public:
        StaticPropertyRecordReference(const Js::PropertyRecord* propertyRecord)
        {
            strongRef = (char*)propertyRecord;
            strongRefHeapBlock = &CollectedRecyclerWeakRefHeapBlock::Instance;
        }
    };

    static const Js::PropertyRecord * const builtInPropertyRecords[];

    PropertyMap * propertyMap;
    PropertyNoCaseSetType * caseInvariantPropertySet;

    Js::ScriptContext * rootPendingClose;
    JsUtil::List<IProjectionContext *, ArenaAllocator>* pendingProjectionContextCloseList;
    Js::ScriptEntryExitRecord * entryExitRecord;
    Js::InterpreterStackFrame* leafInterpreterFrame;
    const Js::PropertyRecord * propertyNamesDirect[128];
    ArenaAllocator threadAlloc;
    ThreadServiceWrapper* threadServiceWrapper;
    uint functionCount;
    uint sourceInfoCount;

    Js::TypeId nextTypeId;
    uint32 polymorphicCacheState;
    Js::TypeId wellKnownHostTypeHTMLAllCollectionTypeId;

#ifdef ENABLE_PROJECTION
    SListBase<ExternalWeakReferenceCache *> externalWeakReferenceCacheList;
#if DBG_DUMP
    IProjectionContextMemoryInfo *projectionMemoryInformation;
#endif
#endif

#if ENABLE_NATIVE_CODEGEN
    JsUtil::JobProcessor *jobProcessor;
    Js::Var * bailOutRegisterSaveSpace;
    CodeGenNumberThreadAllocator * codeGenNumberThreadAllocator;
#endif

    RecyclerRootPtr<RecyclableData> recyclableData;
    uint temporaryArenaAllocatorCount;
    uint temporaryGuestArenaAllocatorCount;

#if DBG_DUMP || defined(PROFILE_EXEC)
    ScriptSite* topLevelScriptSite;
#endif

    Js::ScriptContext *scriptContextList;
    bool scriptContextEverRegistered;
    static size_t processNativeCodeSize;
    size_t nativeCodeSize;
    size_t sourceCodeSize;

    Js::HiResTimer hTimer;

    int stackProbeCount;
    // Count stack probes and poll for continuation every n probes
    static const int StackProbePollThreshold = 1000;

    ArenaAllocator inlineCacheThreadInfoAllocator;
    ArenaAllocator isInstInlineCacheThreadInfoAllocator;
    ArenaAllocator equivalentTypeCacheInfoAllocator;
    DListBase<Js::ScriptContext *> inlineCacheScriptContexts;
    DListBase<Js::ScriptContext *> isInstInlineCacheScriptContexts;
    DListBase<Js::EntryPointInfo *> equivalentTypeCacheEntryPoints;

    typedef SList<Js::InlineCache*> InlineCacheList;
    typedef JsUtil::BaseDictionary<Js::PropertyId, InlineCacheList*, ArenaAllocator> InlineCacheListMapByPropertyId;
    InlineCacheListMapByPropertyId protoInlineCacheByPropId;
    InlineCacheListMapByPropertyId storeFieldInlineCacheByPropId;

    uint registeredInlineCacheCount;
    uint unregisteredInlineCacheCount;

    typedef JsUtil::BaseDictionary<Js::Var, Js::IsInstInlineCache*, ArenaAllocator> IsInstInlineCacheListMapByFunction;
    IsInstInlineCacheListMapByFunction isInstInlineCacheByFunction;

    ArenaAllocator prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator;
    DListBase<Js::ScriptContext *> prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext;

    DListBase<CollectCallBack> collectCallBackList;
    CriticalSection csCollectionCallBack;
    bool hasCollectionCallBack;
    bool isOptimizedForManyInstances;
    bool bgJit;

    // We report library code to profiler only if called directly by user code. Not if called by library implementation.
    bool isProfilingUserCode;

    void* jsrtRuntime;

    bool hasUnhandledException;
    bool hasCatchHandler;
    DisableImplicitFlags disableImplicitFlags;

    // Used for identifying that any particular time, the caller chain has try/catch blocks belong to the user code.
    // If all try/catch blocks in the current stack marked as non-user code then this member will remain false.
    bool hasCatchHandlerToUserCode;

    Js::DelayLoadWinRtString delayLoadWinRtString;
#ifdef ENABLE_PROJECTION
    Js::DelayLoadWinRtError delayLoadWinRtError;
    Js::DelayLoadWinRtTypeResolution delayLoadWinRtTypeResolution;
    Js::DelayLoadWinRtRoParameterizedIID delayLoadWinRtRoParameterizedIID;
#endif
#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_ES6_CHAR_CLASSIFIER)
    Js::DelayLoadWindowsGlobalization delayLoadWindowsGlobalizationLibrary;
    Js::WindowsGlobalizationAdapter windowsGlobalizationAdapter;
#endif
#ifdef ENABLE_FOUNDATION_OBJECT
    Js::DelayLoadWinRtFoundation delayLoadWinRtFoundationLibrary;
    Js::WindowsFoundationAdapter windowsFoundationAdapter;
#endif
#ifdef _CONTROL_FLOW_GUARD
    Js::DelayLoadWinCoreMemory delayLoadWinCoreMemoryLibrary;
#endif
    Js::DelayLoadWinCoreProcessThreads delayLoadWinCoreProcessThreads;

    // Number of script context attached with probe manager.
    // This counter will be used as addref when the script context is created, this way we maintain the life of diagnostic object.
    // Once no script context available , diagnostic will go away.
    long crefSContextForDiag;

    Entropy entropy;

    JsUtil::Stack<HostScriptContext*>* hostScriptContextStack;

    //
    // Regex globals
    //
    UnifiedRegex::StandardChars<uint8>* standardUTF8Chars;
    UnifiedRegex::StandardChars<wchar_t>* standardUnicodeChars;

    Js::ImplicitCallFlags implicitCallFlags;

    __declspec(thread) static uint activeScriptSiteCount;
    bool isScriptActive;

    // To synchronize with ETW rundown, which needs to walk scriptContext/functionBody/entryPoint lists.
    CriticalSection csEtwRundown;

#ifdef _M_X64
    friend class Js::Amd64StackFrame;
    Js::Amd64ContextsManager amd64ContextsManager;
    Js::Amd64ContextsManager* GetAmd64ContextsManager() { return &amd64ContextsManager; }
#endif

    typedef JsUtil::BaseDictionary<Js::DynamicType const *, void *, HeapAllocator, PowerOf2SizePolicy> DynamicObjectEnumeratorCacheMap;
    DynamicObjectEnumeratorCacheMap dynamicObjectEnumeratorCacheMap;

    ThreadContextWatsonTelemetryBlock localTelemetryBlock;
    ThreadContextWatsonTelemetryBlock * telemetryBlock;

    NativeLibraryEntryRecord nativeLibraryEntry;

    UCrtC99MathApis ucrtC99MathApis;

    // Indicates the current loop depth as observed by the interpreter. The interpreter causes this value to be updated upon
    // entering and leaving a loop.
    uint8 loopDepth;

    const ThreadConfiguration configuration;

public:
    static ThreadContext * globalListFirst;

    static uint GetScriptSiteHolderCount() { return activeScriptSiteCount; }
    static uint IncrementActiveScriptSiteCount() { return ++activeScriptSiteCount; }
    static uint DecrementActiveScriptSiteCount() { return --activeScriptSiteCount; }

    static ThreadContext * GetThreadContextList() { return globalListFirst; }
    void ValidateThreadContext();

    bool IsInScript() const { return callRootLevel != 0; }
    uint GetCallRootLevel() const { return callRootLevel; }

    PageAllocator * GetPageAllocator() { return &pageAllocator; }

    AllocationPolicyManager * GetAllocationPolicyManager() { return allocationPolicyManager; }
    PreReservedVirtualAllocWrapper * GetPreReservedVirtualAllocator() { return &preReservedVirtualAllocator; }

    void ResetIsAllJITCodeInPreReservedRegion() { isAllJITCodeInPreReservedRegion = false; }
    bool IsAllJITCodeInPreReservedRegion() { return isAllJITCodeInPreReservedRegion; }

    CriticalSection* GetEtwRundownCriticalSection() { return &csEtwRundown; }

    UCrtC99MathApis* GetUCrtC99MathApis() { return &ucrtC99MathApis; }

    Js::DelayLoadWinRtString *GetWinRTStringLibrary();
#ifdef ENABLE_PROJECTION
    Js::DelayLoadWinRtError *GetWinRTErrorLibrary();
    Js::DelayLoadWinRtTypeResolution* GetWinRTTypeResolutionLibrary();
    Js::DelayLoadWinRtRoParameterizedIID* GetWinRTRoParameterizedIIDLibrary();
#endif
#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_ES6_CHAR_CLASSIFIER)
    Js::DelayLoadWindowsGlobalization *GetWindowsGlobalizationLibrary();
    Js::WindowsGlobalizationAdapter *GetWindowsGlobalizationAdapter();
#endif
#ifdef ENABLE_FOUNDATION_OBJECT
    Js::DelayLoadWinRtFoundation *GetWinRtFoundationLibrary();
    Js::WindowsFoundationAdapter *GetWindowsFoundationAdapter();
#endif
#ifdef _CONTROL_FLOW_GUARD
    Js::DelayLoadWinCoreMemory * GetWinCoreMemoryLibrary();
#endif
    Js::DelayLoadWinCoreProcessThreads * GetWinCoreProcessThreads();

    JITTimer JITTelemetry;
    ParserTimer ParserTelemetry;
    GUID activityId;
    void *tridentLoadAddress;

    void* GetTridentLoadAddress() const { return tridentLoadAddress;  }
    void SetTridentLoadAddress(void *loadAddress) { tridentLoadAddress = loadAddress; }

#ifdef ENABLE_DIRECTCALL_TELEMETRY
    DirectCallTelemetry directCallTelemetry;
#endif

    JITStats GetJITStats()
    {
        return JITTelemetry.GetStats();
    }

    void ResetJITStats()
    {
        JITTelemetry.Reset();
    }

    ParserStats GetParserStats()
    {
        return ParserTelemetry.GetStats();
    }

    void ResetParserStats()
    {
        ParserTelemetry.Reset();
    }


    double maxGlobalFunctionExecTime;
    double GetAndResetMaxGlobalFunctionExecTime()
    {
        double res = maxGlobalFunctionExecTime;
        this->maxGlobalFunctionExecTime = 0.0;
        return res;
    }

    bool IsCFGEnabled();
    void SetValidCallTargetForCFG(PVOID callTargetAddress, bool isSetValid = true);
    BOOL HasPreviousHostScriptContext();
    HostScriptContext* GetPreviousHostScriptContext() ;
    void PushHostScriptContext(HostScriptContext* topProvider);
    HostScriptContext* PopHostScriptContext();

    void SetInterruptPoller(InterruptPoller *poller) { interruptPoller = poller; }
    InterruptPoller *GetInterruptPoller() const { return interruptPoller; }
    BOOL HasInterruptPoller() const { return interruptPoller != nullptr; }
    void CheckScriptInterrupt();
    void CheckInterruptPoll();

    bool DoInterruptProbe(Js::FunctionBody *const func) const
    {
        return
            (this->TestThreadContextFlag(ThreadContextFlagCanDisableExecution) &&
             !PHASE_OFF(Js::InterruptProbePhase, func)) ||
            PHASE_ON(Js::InterruptProbePhase, func);
    }

    bool DoInterruptProbe() const
    {
        return
            (this->TestThreadContextFlag(ThreadContextFlagCanDisableExecution) &&
             !PHASE_OFF1(Js::InterruptProbePhase)) ||
            PHASE_ON1(Js::InterruptProbePhase);
    }

    bool EvalDisabled() const
    {
        return this->TestThreadContextFlag(ThreadContextFlagEvalDisabled);
    }

    bool NoJIT() const
    {
        return this->TestThreadContextFlag(ThreadContextFlagNoJIT);
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    Js::Var GetMemoryStat(Js::ScriptContext* scriptContext);
    void SetAutoProxyName(LPCWSTR objectName);
    LPCWSTR GetAutoProxyName() const { return recyclableData->autoProxyName; }
    Js::PropertyId handlerPropertyId = Js::Constants::NoProperty;
#endif

    void SetReturnedValueList(Js::ReturnedValueList *returnedValueList)
    {
        Assert(this->recyclableData != nullptr);
        this->recyclableData->returnedValueList = returnedValueList;
    }
#if DBG
    void EnsureNoReturnedValueList()
    {
        Assert(this->recyclableData == nullptr || this->recyclableData->returnedValueList == nullptr);
    }

    uint GetScriptContextCount() const { return this->scriptContextCount; }
#endif
    Js::ScriptContext* GetScriptContextList() const { return this->scriptContextList; }
    bool WasAnyScriptContextEverRegistered() const { return this->scriptContextEverRegistered; }

#if DBG_DUMP || defined(PROFILE_EXEC)
    void SetTopLevelScriptSite(ScriptSite* topScriptSite) { this->topLevelScriptSite = topScriptSite; }
    ScriptSite* GetTopLevelScriptSite () { return this->topLevelScriptSite; }
#endif
#if DBG || defined(PROFILE_EXEC)
    virtual bool AsyncHostOperationStart(void *) override;
    virtual void AsyncHostOperationEnd(bool wasInAsync, void *) override;
#endif
#if DBG
    bool IsInAsyncHostOperation() const;
#endif

    BOOL ReserveStaticTypeIds(__in int first, __in int last);
    Js::TypeId ReserveTypeIds(int count);
    Js::TypeId CreateTypeId();
    Js::TypeId GetNextTypeId() { return nextTypeId; }

    // Lookup the well known type registered with a Js::TypeId.
    //  typeId:   The type id to match
    //  returns:  The well known type which was previously registered via a call to SetWellKnownHostTypeId
    WellKnownHostType GetWellKnownHostType(Js::TypeId typeId);

    // Register a well known type to a Js::TypeId.
    //  wellKnownType:  The well known type which we should register
    //  typeId:         The type id which matches to the well known type
    void SetWellKnownHostTypeId(WellKnownHostType wellKnownType, Js::TypeId typeId);

    uint32 GetNextPolymorphicCacheState()
    {
        return ++polymorphicCacheState;
    };

    ~ThreadContext();
    void CloseForJSRT();

    //Call back is called for one or more handles
    //It does multiple callbacks (For example: separate call back for GC thread handle & JIT thread handles)
//    template<class Fn>
    //void ShutdownThreads(Fn callback);

    void ShutdownThreads()
    {
#if ENABLE_NATIVE_CODEGEN
        if (jobProcessor)
        {
            jobProcessor->Close();
        }
#endif
#ifdef CONCURRENT_GC_ENABLED
        if (this->recycler != nullptr)
        {
            this->recycler->ShutdownThread();
        }
#endif
    }



    Js::HiResTimer * GetHiResTimer() { return &hTimer; }
    ArenaAllocator* GetThreadAlloc() { return &threadAlloc; }
    static CriticalSection * GetCriticalSection() { return &s_csThreadContext; }

    ThreadContext(AllocationPolicyManager * allocationPolicyManager = nullptr, JsUtil::ThreadService::ThreadServiceCallback threadServiceCallback = nullptr, bool enableExperimentalFeatures = false);
    static void Add(ThreadContext *threadContext);

    ThreadConfiguration const * GetConfig() const { return &configuration; }

public:
    void SetTelemetryBlock(ThreadContextWatsonTelemetryBlock * telemetryBlock) { this->telemetryBlock = telemetryBlock; }

    static ThreadContext* GetContextForCurrentThread();

    Recycler* GetRecycler() { return recycler; }

    Recycler* EnsureRecycler();

    ThreadContext::CollectCallBack * AddRecyclerCollectCallBack(RecyclerCollectCallBackFunction callback, void * context);
    void RemoveRecyclerCollectCallBack(ThreadContext::CollectCallBack * collectCallBack);

    void AddToPendingProjectionContextCloseList(IProjectionContext *projectionContext);
    void RemoveFromPendingClose(IProjectionContext *projectionContext);
    void ClosePendingProjectionContexts();

    void AddToPendingScriptContextCloseList(Js::ScriptContext * scriptContext);
    void RemoveFromPendingClose(Js::ScriptContext * scriptContext);
    void ClosePendingScriptContexts();

    Js::PropertyRecord const * GetPropertyName(Js::PropertyId propertyId);
    Js::PropertyRecord const * GetPropertyNameLocked(Js::PropertyId propertyId);

private:
    template <bool locked> Js::PropertyRecord const * GetPropertyNameImpl(Js::PropertyId propertyId);
public:
    void FindPropertyRecord(Js::JavascriptString *pstName, Js::PropertyRecord const ** propertyRecord);
    void FindPropertyRecord(__in LPCWSTR propertyName, __in int propertyNameLength, Js::PropertyRecord const ** propertyRecord);
    const Js::PropertyRecord * FindPropertyRecord(const wchar_t * propertyName, int propertyNameLength);

    JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>* FindPropertyIdNoCase(Js::ScriptContext * scriptContext, LPCWSTR propertyName, int propertyNameLength);
    JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>* FindPropertyIdNoCase(Js::ScriptContext * scriptContext, JsUtil::CharacterBuffer<WCHAR> const& propertyName);
    bool FindExistingPropertyRecord(_In_ JsUtil::CharacterBuffer<WCHAR> const& propertyName, Js::CaseInvariantPropertyListWithHashCode** propertyRecord);
    void CleanNoCasePropertyMap();
    void ForceCleanPropertyMap();

    const Js::PropertyRecord * GetOrAddPropertyRecord(JsUtil::CharacterBuffer<wchar_t> propertyName)
    {
        return GetOrAddPropertyRecordImpl(propertyName, false);
    }
    const Js::PropertyRecord * GetOrAddPropertyRecordBind(JsUtil::CharacterBuffer<wchar_t> propertyName)
    {
        return GetOrAddPropertyRecordImpl(propertyName, true);
    }
    void AddBuiltInPropertyRecord(const Js::PropertyRecord *propertyRecord);

    void GetOrAddPropertyId(__in LPCWSTR propertyName, __in int propertyNameLength, Js::PropertyRecord const** propertyRecord);
    void GetOrAddPropertyId(JsUtil::CharacterBuffer<WCHAR> const& propertName, Js::PropertyRecord const** propertyRecord);
    Js::PropertyRecord const * UncheckedAddPropertyId(JsUtil::CharacterBuffer<WCHAR> const& propertyName, bool bind, bool isSymbol = false);
    Js::PropertyRecord const * UncheckedAddPropertyId(__in LPCWSTR propertyName, __in int propertyNameLength, bool bind = false, bool isSymbol = false);

#ifdef ENABLE_JS_ETW
    void EtwLogPropertyIdList();
#endif

private:
    const Js::PropertyRecord * GetOrAddPropertyRecordImpl(JsUtil::CharacterBuffer<wchar_t> propertyName, bool bind);
    void AddPropertyRecordInternal(const Js::PropertyRecord * propertyRecord);
    void BindPropertyRecord(const Js::PropertyRecord * propertyRecord);
    bool IsDirectPropertyName(const wchar_t * propertyName, int propertyNameLength);

    RecyclerWeakReference<const Js::PropertyRecord> * CreatePropertyRecordWeakRef(const Js::PropertyRecord * propertyRecord);
    void AddCaseInvariantPropertyRecord(const Js::PropertyRecord * propertyRecord);

    uint scriptContextCount;

public:
    void UncheckedAddBuiltInPropertyId();

    BOOL IsNumericPropertyId(Js::PropertyId propertyId, uint32* value);
    bool IsActivePropertyId(Js::PropertyId pid);
    void InvalidatePropertyRecord(const Js::PropertyRecord * propertyRecord);
    Js::PropertyId GetNextPropertyId();
    Js::PropertyId GetMaxPropertyId();
    uint GetHighestPropertyNameIndex() const;

    void SetThreadServiceWrapper(ThreadServiceWrapper*);
    ThreadServiceWrapper* GetThreadServiceWrapper();
    uint GetUnreleasedScriptContextCount(){return scriptContextCount;}

#ifdef ENABLE_PROJECTION
    void AddExternalWeakReferenceCache(ExternalWeakReferenceCache *externalWeakReferenceCache);
    void RemoveExternalWeakReferenceCache(ExternalWeakReferenceCache *externalWeakReferenceCache);
    virtual void MarkExternalWeakReferencedObjects(bool inPartialCollect) override;
    virtual void ResolveExternalWeakReferencedObjects() override;

#if DBG_DUMP
    void RegisterProjectionMemoryInformation(IProjectionContextMemoryInfo* projectionContextMemoryInfo);
    void DumpProjectionContextMemoryStats(LPCWSTR headerMsg, bool forceDetailed = false);
    IProjectionContextMemoryInfo* GetProjectionContextMemoryInformation();
#endif
#endif

    uint NewFunctionNumber() { return ++functionCount; }
    uint PeekNewFunctionNumber() { return functionCount + 1; }

    uint NewSourceInfoNumber() { return ++sourceInfoCount; }

    void AddCodeSize(size_t newCode)
    {
        ::InterlockedExchangeAdd(&nativeCodeSize, newCode);
        ::InterlockedExchangeAdd(&processNativeCodeSize, newCode);
    }
    void AddSourceSize(size_t  newCode) { sourceCodeSize += newCode; }
    void SubCodeSize(size_t  deadCode)
    {
        Assert(nativeCodeSize >= deadCode);
        Assert(processNativeCodeSize >= deadCode);
        ::InterlockedExchangeSubtract(&nativeCodeSize, deadCode);
        ::InterlockedExchangeSubtract(&processNativeCodeSize, deadCode);
    }
    void SubSourceSize(size_t deadCode) { Assert(sourceCodeSize >= deadCode); sourceCodeSize -= deadCode; }
    size_t  GetCodeSize() { return nativeCodeSize; }
    static size_t  GetProcessCodeSize() { return processNativeCodeSize; }
    size_t GetSourceSize() { return sourceCodeSize; }

    Js::ScriptEntryExitRecord * GetScriptEntryExit() const { return entryExitRecord; }
    void RegisterCodeGenRecyclableData(Js::CodeGenRecyclableData *const codeGenRecyclableData);
    void UnregisterCodeGenRecyclableData(Js::CodeGenRecyclableData *const codeGenRecyclableData);
#if ENABLE_NATIVE_CODEGEN
    BOOL IsNativeAddress(void *pCodeAddr);
    JsUtil::JobProcessor *GetJobProcessor();
    static void CloseSharedJobProcessor(const bool waitForThread);
    Js::Var * GetBailOutRegisterSaveSpace() const { return bailOutRegisterSaveSpace; }
    CodeGenNumberThreadAllocator * GetCodeGenNumberThreadAllocator() const
    {
        return codeGenNumberThreadAllocator;
    }
#endif
    void ResetFunctionCount() { Assert(this->GetScriptSiteHolderCount() == 0); this->functionCount = 0; }
    void PushEntryExitRecord(Js::ScriptEntryExitRecord *);
    void PopEntryExitRecord(Js::ScriptEntryExitRecord *);
    uint EnterScriptStart(Js::ScriptEntryExitRecord *, bool doCleanup);
    void EnterScriptEnd(Js::ScriptEntryExitRecord *, bool doCleanup);

    template <bool leaveForHost>
    void LeaveScriptStart(void *);
    template <bool leaveForHost>
    void LeaveScriptEnd(void *);
    void DisposeOnLeaveScript();

    void PushInterpreterFrame(Js::InterpreterStackFrame *interpreterFrame);
    Js::InterpreterStackFrame *PopInterpreterFrame();
    Js::InterpreterStackFrame *GetLeafInterpreterFrame() const { return leafInterpreterFrame; }

    Js::TempArenaAllocatorObject * GetTemporaryAllocator(LPCWSTR name);
    void ReleaseTemporaryAllocator(Js::TempArenaAllocatorObject * tempAllocator);

    Js::TempGuestArenaAllocatorObject * GetTemporaryGuestAllocator(LPCWSTR name);
    void ReleaseTemporaryGuestAllocator(Js::TempGuestArenaAllocatorObject * tempAllocator);

    // Should be called from script context, at the time when construction for scriptcontext is just done.
    void EnsureDebugManager();

    // Should be called from script context 's destructor,
    void ReleaseDebugManager();

    void RegisterScriptContext(Js::ScriptContext *scriptContext);
    void UnregisterScriptContext(Js::ScriptContext *scriptContext);

    // NoScriptScope
    void SetNoScriptScope(bool noScriptScope) { this->noScriptScope = noScriptScope; }
    bool IsNoScriptScope() { return this->noScriptScope; }

    Js::ScriptContext ** RegisterInlineCacheScriptContext(Js::ScriptContext * scriptContext);
    Js::ScriptContext ** RegisterIsInstInlineCacheScriptContext(Js::ScriptContext * scriptContext);
    Js::EntryPointInfo ** RegisterEquivalentTypeCacheEntryPoint(Js::EntryPointInfo * entryPoint);
    void UnregisterInlineCacheScriptContext(Js::ScriptContext ** scriptContext);
    void UnregisterIsInstInlineCacheScriptContext(Js::ScriptContext ** scriptContext);
    void UnregisterEquivalentTypeCacheEntryPoint(Js::EntryPointInfo ** entryPoint);
    void RegisterProtoInlineCache(Js::InlineCache * inlineCache, Js::PropertyId propertyId);
    void RegisterStoreFieldInlineCache(Js::InlineCache * inlineCache, Js::PropertyId propertyId);
    void NotifyInlineCacheBatchUnregistered(uint count);

#if DBG
    bool IsProtoInlineCacheRegistered(const Js::InlineCache * inlineCache, Js::PropertyId propertyId);
    bool IsStoreFieldInlineCacheRegistered(const Js::InlineCache * inlineCache, Js::PropertyId propertyId);
#endif

#if ENABLE_NATIVE_CODEGEN
    Js::PropertyGuard* RegisterSharedPropertyGuard(Js::PropertyId propertyId);
    void RegisterLazyBailout(Js::PropertyId propertyId, Js::EntryPointInfo* entryPoint);
    void RegisterUniquePropertyGuard(Js::PropertyId propertyId, Js::PropertyGuard* guard);
    void RegisterUniquePropertyGuard(Js::PropertyId propertyId, RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef);
    void RegisterConstructorCache(Js::PropertyId propertyId, Js::ConstructorCache* cache);
#endif

private:
    void RegisterInlineCache(InlineCacheListMapByPropertyId& inlineCacheMap, Js::InlineCache* inlineCache, Js::PropertyId propertyId);
    static bool IsInlineCacheRegistered(InlineCacheListMapByPropertyId& inlineCacheMap, const Js::InlineCache* inlineCache, Js::PropertyId propertyId);
    void InvalidateInlineCacheList(InlineCacheList *inlineCacheList);
    void CompactInlineCacheList(InlineCacheList *inlineCacheList);
    void CompactInlineCacheInvalidationLists();
    void CompactProtoInlineCaches();
    void CompactStoreFieldInlineCaches();

#if DBG
    static bool IsInlineCacheInList(const Js::InlineCache* inlineCache, const InlineCacheList* inlineCacheChain);
#endif

#if ENABLE_NATIVE_CODEGEN
    void InvalidateFixedFieldGuard(Js::PropertyGuard* guard);
    PropertyGuardEntry* EnsurePropertyGuardEntry(const Js::PropertyRecord* propertyRecord, bool& foundExistingEntry);
    void InvalidatePropertyGuardEntry(const Js::PropertyRecord* propertyRecord, PropertyGuardEntry* entry);
#endif

public:
    class AutoDisableExpiration
    {
    public:
        AutoDisableExpiration(ThreadContext* threadContext):
            _threadContext(threadContext),
            _oldExpirationDisabled(threadContext->disableExpiration)
        {
            _threadContext->disableExpiration = true;
        }

        ~AutoDisableExpiration()
        {
            _threadContext->disableExpiration = _oldExpirationDisabled;
        }

    private:
        ThreadContext* _threadContext;
        bool _oldExpirationDisabled;
    };

    void InvalidateProtoInlineCaches(Js::PropertyId propertyId);
    void InvalidateStoreFieldInlineCaches(Js::PropertyId propertyId);
    void InvalidateAllProtoInlineCaches();
    bool AreAllProtoInlineCachesInvalidated();
    void InvalidateAllStoreFieldInlineCaches();
    bool AreAllStoreFieldInlineCachesInvalidated();
    void InvalidatePropertyGuards(Js::PropertyId propertyId);
    void InvalidateAllPropertyGuards();
    void RegisterIsInstInlineCache(Js::IsInstInlineCache * inlineCache, Js::Var function);
    void UnregisterIsInstInlineCache(Js::IsInstInlineCache * inlineCache, Js::Var function);
#if DBG
    bool IsIsInstInlineCacheRegistered(Js::IsInstInlineCache * inlineCache, Js::Var function);
#endif

private:
    void InvalidateIsInstInlineCacheList(Js::IsInstInlineCache* inlineCacheList);
#if DBG
    bool IsIsInstInlineCacheInList(const Js::IsInstInlineCache* inlineCache, const Js::IsInstInlineCache* inlineCacheList);
#endif

public:
    void InvalidateIsInstInlineCachesForFunction(Js::Var function);
    void InvalidateAllIsInstInlineCaches();
    bool AreAllIsInstInlineCachesInvalidated() const;
#ifdef PERSISTENT_INLINE_CACHES
    void ClearInlineCachesWithDeadWeakRefs();
#endif
    void ClearInlineCaches();
    void ClearIsInstInlineCaches();
    void ClearEquivalentTypeCaches();
    void ClearScriptContextCaches();

    void RegisterTypeWithProtoPropertyCache(const Js::PropertyId propertyId, Js::Type *const type);
    void InvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId);
    void InternalInvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId);
    void InvalidateAllProtoTypePropertyCaches();

    Js::ScriptContext ** RegisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext(Js::ScriptContext * scriptContext);
    void UnregisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext(Js::ScriptContext ** scriptContext);
    void ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches();

    BOOL HasUnhandledException() const { return hasUnhandledException; }
    void SetHasUnhandledException() {hasUnhandledException = TRUE; }
    void ResetHasUnhandledException() {hasUnhandledException = FALSE; }
    void SetUnhandledExceptionObject(Js::JavascriptExceptionObject* exceptionObject) {recyclableData->unhandledExceptionObject  = exceptionObject; }
    Js::JavascriptExceptionObject* GetUnhandledExceptionObject() const  { return recyclableData->unhandledExceptionObject; };

    bool HasCatchHandler() const { return hasCatchHandler; }
    void SetHasCatchHandler(bool hasCatchHandler) { this->hasCatchHandler = hasCatchHandler; }

    bool IsUserCode() const { return this->hasCatchHandlerToUserCode; }
    void SetIsUserCode(bool set) { this->hasCatchHandlerToUserCode = set; }

    void QueueFreeOldEntryPointInfoIfInScript(Js::FunctionEntryPointInfo* oldEntryPointInfo)
    {
        if (this->IsInScript())
        {
            // Add it to the list only if it's not already in it
            if (oldEntryPointInfo->nextEntryPoint == nullptr && !oldEntryPointInfo->IsCleanedUp())
            {
                oldEntryPointInfo->nextEntryPoint = recyclableData->oldEntryPointInfo;
                recyclableData->oldEntryPointInfo = oldEntryPointInfo;
            }
        }
    }

    static BOOLEAN IsOnStack(void const *ptr);
    __declspec(noinline) bool IsStackAvailable(size_t size);
    __declspec(noinline) bool IsStackAvailableNoThrow(size_t size = Js::Constants::MinStackDefault);
    static bool IsCurrentStackAvailable(size_t size);
    void ProbeStackNoDispose(size_t size, Js::ScriptContext *scriptContext, PVOID returnAddress = nullptr);
    void ProbeStack(size_t size, Js::ScriptContext *scriptContext, PVOID returnAddress = nullptr);
    void ProbeStack(size_t size, Js::RecyclableObject * obj, Js::ScriptContext *scriptContext);
    void ProbeStack(size_t size);
    static void __stdcall ProbeCurrentStackNoDispose(size_t size, Js::ScriptContext *scriptContext);
    static void __stdcall ProbeCurrentStack(size_t size, Js::ScriptContext *scriptContext);
    static void __stdcall ProbeCurrentStack2(size_t size, Js::ScriptContext *scriptContext, uint32 u1, uint32 u2)
    {
        ProbeCurrentStack(size, scriptContext);
    }

#if ENABLE_PROFILE_INFO
    void ThreadContext::EnsureSourceProfileManagersByUrlMap();
    Js::SourceDynamicProfileManager* GetSourceDynamicProfileManager(_In_z_ const WCHAR* url, _In_ uint hash, _Inout_ bool* addref);
    uint ReleaseSourceDynamicProfileManagers(const WCHAR* url);
#endif

    void EnsureSymbolRegistrationMap();
    const Js::PropertyRecord* GetSymbolFromRegistrationMap(const wchar_t* stringKey);
    const Js::PropertyRecord* AddSymbolToRegistrationMap(const wchar_t* stringKey, charcount_t stringLength);

    inline void ClearPendingSOError()
    {
        this->GetPendingSOErrorObject()->ClearError();
    }

    inline void ClearPendingOOMError()
    {
        this->GetPendingOOMErrorObject()->ClearError();
    }

    Js::JavascriptExceptionObject *GetPendingSOErrorObject()
    {
        Assert(recyclableData->soErrorObject.IsPendingExceptionObject());
        return &recyclableData->soErrorObject;
    }

    Js::JavascriptExceptionObject *GetPendingOOMErrorObject()
    {
        Assert(recyclableData->oomErrorObject.IsPendingExceptionObject());
        return &recyclableData->oomErrorObject;
    }

    Js::JavascriptExceptionObject *GetPendingTerminatedErrorObject()
    {
        return &recyclableData->terminatedErrorObject;
    }

    Js::JavascriptExceptionObject* GetRecordedException()
    {
        return recyclableData->exceptionObject;
    }

    bool GetPropagateException()
    {
        return recyclableData->propagateException;
    }

    void SetHasThrownPendingException()
    {
        Assert(this->IsInScript());
        this->hasThrownPendingException = true;
    }

    void SetRecordedException(Js::JavascriptExceptionObject* exceptionObject, bool propagateToDebugger = false)
    {
        this->recyclableData->exceptionObject = exceptionObject;
        this->recyclableData->propagateException = propagateToDebugger;
    }

    Entropy& GetEntropy()
    {
        return entropy;
    }

    Js::ImplicitCallFlags * GetAddressOfImplicitCallFlags()
    {
        return &implicitCallFlags;
    }

    DisableImplicitFlags * GetAddressOfDisableImplicitFlags()
    {
        return &disableImplicitFlags;
    }

    Js::ImplicitCallFlags GetImplicitCallFlags()
    {
        return implicitCallFlags;
    }

    void SetImplicitCallFlags(Js::ImplicitCallFlags flags)
    {
        //Note: this action is inlined into JITed code in Lowerer::GenerateCallProfiling.
        //   if you change this, you might want to add it there too.
        implicitCallFlags = flags;
    }

    void ClearImplicitCallFlags();
    void ClearImplicitCallFlags(Js::ImplicitCallFlags flags);

    void AddImplicitCallFlags(Js::ImplicitCallFlags flags)
    {
        SetImplicitCallFlags((Js::ImplicitCallFlags)(implicitCallFlags | flags));
    }

    void CheckAndResetImplicitCallAccessorFlag();

    template <class Fn>
    __inline Js::Var ExecuteImplicitCall(Js::RecyclableObject * function, Js::ImplicitCallFlags flags, Fn implicitCall)
    {
        // For now, we will not allow Function that is marked as HasNoSideEffect to be called, and we will just bailout.
        // These function may still throw exceptions, so we will need to add checks with RecordImplicitException
        // so that we don't throw exception when disableImplicitCall is set before we allow these function to be called
        // as an optimization.  (These functions are valueOf and toString calls for built-in non primitive types)

        Js::FunctionInfo::Attributes attributes = Js::FunctionInfo::GetAttributes(function);

        // we can hoist out const method if we know the function doesn't have side effect,
        // and the value can be hoisted.
        if (this->HasNoSideEffect(function, attributes))
        {
            // Has no side effect means the function does not change global value or
            // will check for implicit call flags
            return implicitCall();
        }

        // Don't call the implicit call if disable implicit call
        if (IsDisableImplicitCall())
        {
            AddImplicitCallFlags(flags);
            // Return "undefined" just so we have a valid var, in case subsequent instructions are executed
            // before we bail out.
            return function->GetScriptContext()->GetLibrary()->GetUndefined();
        }

        if ((attributes & Js::FunctionInfo::HasNoSideEffect) != 0)
        {
            // Has no side effect means the function does not change global value or
            // will check for implicit call flags
            return implicitCall();
        }

        // Save and restore implicit flags around the implicit call

        Js::ImplicitCallFlags saveImplicitCallFlags = this->GetImplicitCallFlags();
        Js::Var result = implicitCall();
        this->SetImplicitCallFlags((Js::ImplicitCallFlags)(saveImplicitCallFlags | flags));
        return result;
    }
    bool HasNoSideEffect(Js::RecyclableObject * function) const;
    bool HasNoSideEffect(Js::RecyclableObject * function, Js::FunctionInfo::Attributes attr) const;
    bool RecordImplicitException();
    DisableImplicitFlags GetDisableImplicitFlags() const { return disableImplicitFlags; }
    void SetDisableImplicitFlags(DisableImplicitFlags flags) { disableImplicitFlags = flags; }
    bool IsDisableImplicitCall() const { return (disableImplicitFlags & DisableImplicitCallFlag) != 0; }
    bool IsDisableImplicitException() const { return (disableImplicitFlags & DisableImplicitExceptionFlag) != 0; }
    void DisableImplicitCall() { disableImplicitFlags = (DisableImplicitFlags)(disableImplicitFlags | DisableImplicitCallFlag); }
    void ClearDisableImplicitFlags() { disableImplicitFlags = DisableImplicitNoFlag; }

    virtual uint GetRandomNumber() override;

    // DefaultCollectWrapper
    virtual void PreCollectionCallBack(CollectionFlags flags) override;
    virtual void PreSweepCallback() override;
    virtual void WaitCollectionCallBack() override;
    virtual void PostCollectionCallBack() override;
    virtual BOOL ExecuteRecyclerCollectionFunction(Recycler * recycler, CollectionFunction function, CollectionFlags flags) override;
#ifdef FAULT_INJECTION
    virtual void DisposeScriptContextByFaultInjectionCallBack() override;
#endif
    virtual void DisposeObjects(Recycler * recycler) override;

    typedef DList<ExpirableObject*, ArenaAllocator> ExpirableObjectList;
    ExpirableObjectList* expirableObjectList;
    ExpirableObjectList* expirableObjectDisposeList;
    int numExpirableObjects;
    int expirableCollectModeGcCount;
    bool disableExpiration;

    bool InExpirableCollectMode();
    void TryEnterExpirableCollectMode();
    void TryExitExpirableCollectMode();
    void RegisterExpirableObject(ExpirableObject* object);
    void UnregisterExpirableObject(ExpirableObject* object);
    void DisposeExpirableObject(ExpirableObject* object);

    void * GetDynamicObjectEnumeratorCache(Js::DynamicType const * dynamicType);
    void AddDynamicObjectEnumeratorCache(Js::DynamicType const * dynamicType, void * cache);
public:
    bool IsScriptActive() const { return isScriptActive; }
    void SetIsScriptActive(bool isActive) { isScriptActive = isActive; }
    bool IsExecutionDisabled() const
    {
        return this->GetStackLimitForCurrentThread() == Js::Constants::StackLimitForScriptInterrupt;
    }
    void DisableExecution();
    void EnableExecution();
    bool TestThreadContextFlag(ThreadContextFlags threadContextFlag) const;
    void SetThreadContextFlag(ThreadContextFlags threadContextFlag);
    void ClearThreadContextFlag(ThreadContextFlags threadContextFlag);

    void SetForceOneIdleCollection();

    bool IsInThreadServiceCallback() const { return threadService.IsInCallback(); }

    Js::DebugManager * GetDebugManager() const { return this->debugManager; }

    const NativeLibraryEntryRecord::Entry* PeekNativeLibraryEntry() const { return this->nativeLibraryEntry.Peek(); }
    void PushNativeLibraryEntry(_In_ NativeLibraryEntryRecord::Entry* entry) { this->nativeLibraryEntry.Push(entry); }
    void PopNativeLibraryEntry() { this->nativeLibraryEntry.Pop(); }

    bool IsProfilingUserCode() const { return isProfilingUserCode; }
    void SetIsProfilingUserCode(bool value) { isProfilingUserCode = value; }

#if DBG_DUMP
    uint scriptSiteCount;
#endif

#ifdef BAILOUT_INJECTION
    uint bailOutByteCodeLocationCount;
#endif
#ifdef DYNAMIC_PROFILE_MUTATOR
    DynamicProfileMutator * dynamicProfileMutator;
#endif
    //
    // Regex helpers
    //
    UnifiedRegex::StandardChars<uint8>* GetStandardChars(__inout_opt uint8* dummy);
    UnifiedRegex::StandardChars<wchar_t>* GetStandardChars(__inout_opt wchar_t* dummy);

    bool IsOptimizedForManyInstances() const { return isOptimizedForManyInstances; }

    void OptimizeForManyInstances(const bool optimizeForManyInstances)
    {
        Assert(!recycler || optimizeForManyInstances == isOptimizedForManyInstances); // mode cannot be changed after recycler is created
        isOptimizedForManyInstances = optimizeForManyInstances;

    }

#if ENABLE_NATIVE_CODEGEN
    bool IsBgJitEnabled() const { return bgJit; }

    void EnableBgJit(const bool enableBgJit)
    {
        Assert(!jobProcessor || enableBgJit == bgJit);
        bgJit = enableBgJit;
    }
#endif

    void* GetJSRTRuntime() const { return jsrtRuntime; }
    void SetJSRTRuntime(void* runtime);

    bool CanBeFalsy(Js::TypeId typeId);
private:
    BOOL ExecuteRecyclerCollectionFunctionCommon(Recycler * recycler, CollectionFunction function, CollectionFlags flags);

    void DoInvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId, TypeHashSet *const typeHashSet);
    void InitializePropertyMaps();
    void CreateNoCasePropertyMap();

    InterruptPoller *interruptPoller;

    void CollectionCallBack(RecyclerCollectCallBackFlags flags);

    // Cache used by HostDispatch::GetBuiltInOperationFromEntryPoint
private:
    JsUtil::BaseDictionary<Js::JavascriptMethod, uint, ArenaAllocator, PowerOf2SizePolicy> entryPointToBuiltInOperationIdCache;

public:
    bool IsEntryPointToBuiltInOperationIdCacheInitialized()
    {
        return entryPointToBuiltInOperationIdCache.Count() != 0;
    }

    bool GetBuiltInOperationIdFromEntryPoint(Js::JavascriptMethod entryPoint, uint * id)
    {
        return entryPointToBuiltInOperationIdCache.TryGetValue(entryPoint, id);
    }

    void SetBuiltInOperationIdForEntryPoint(Js::JavascriptMethod entryPoint, uint id)
    {
        entryPointToBuiltInOperationIdCache.Add(entryPoint, id);
    }

    void ResetEntryPointToBuiltInOperationIdCache()
    {
        entryPointToBuiltInOperationIdCache.ResetNoDelete();
    }

    uint8 LoopDepth() const
    {
        return loopDepth;
    }

    void SetLoopDepth(const uint8 loopDepth)
    {
        this->loopDepth = loopDepth;
    }

    void IncrementLoopDepth()
    {
        if(loopDepth != UCHAR_MAX)
        {
            ++loopDepth;
        }
    }

    void DecrementLoopDepth()
    {
        if(loopDepth != 0)
        {
            --loopDepth;
        }
    }

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    static void ReportAndCheckLeaksOnProcessDetach();
#endif
#ifdef LEAK_REPORT
    void SetRootTrackerScriptContext(Js::ScriptContext * scriptContext);
    void ClearRootTrackerScriptContext(Js::ScriptContext * scriptContext);

private:
    Js::ScriptContext * rootTrackerScriptContext;

    DWORD threadId;
#endif
};

extern void(*InitializeAdditionalProperties)(ThreadContext *threadContext);

// Temporarily set script profiler isProfilingUserCode state, restore at destructor
class AutoProfilingUserCode
{
private:
    ThreadContext* threadContext;
    const bool oldIsProfilingUserCode;

public:
    AutoProfilingUserCode(ThreadContext* threadContext, bool isProfilingUserCode) :
        threadContext(threadContext),
        oldIsProfilingUserCode(threadContext->IsProfilingUserCode())
    {
        threadContext->SetIsProfilingUserCode(isProfilingUserCode);
    }

    ~AutoProfilingUserCode()
    {
        threadContext->SetIsProfilingUserCode(oldIsProfilingUserCode);
    }
};