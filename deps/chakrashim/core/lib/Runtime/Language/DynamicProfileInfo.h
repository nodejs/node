//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// DisableJit-TODO
#if ENABLE_PROFILE_INFO 

#ifdef DYNAMIC_PROFILE_MUTATOR
class DynamicProfileMutatorImpl;
#endif

#define PolymorphicInlineCacheUtilizationMinValue 0
#define PolymorphicInlineCacheUtilizationMaxValue 0xFF
#define PolymorphicInlineCacheUtilizationThreshold 0x80
#define PolymorphicInlineCacheUtilizationIncrement 10
#define PolymorphicInlineCacheUtilizationDecrement 1

namespace IR
{
    enum BailOutKind : uint
    {
    #define BAIL_OUT_KIND_LAST(n)               n
    #define BAIL_OUT_KIND(n, ...)               BAIL_OUT_KIND_LAST(n),
    #define BAIL_OUT_KIND_VALUE_LAST(n, v)      n = v
    #define BAIL_OUT_KIND_VALUE(n, v)           BAIL_OUT_KIND_VALUE_LAST(n, v),
    #include "BailOutKind.h"
    };
    ENUM_CLASS_HELPERS(BailOutKind, uint);

    CompileAssert(BailOutKind::BailOutKindEnd < BailOutKind::BailOutKindBitsStart);

    bool IsTypeCheckBailOutKind(BailOutKind kind);
    bool IsEquivalentTypeCheckBailOutKind(BailOutKind kind);
    BailOutKind EquivalentToMonoTypeCheckBailOutKind(BailOutKind kind);
}

#if ENABLE_DEBUG_CONFIG_OPTIONS
const char *GetBailOutKindName(IR::BailOutKind kind);
bool IsValidBailOutKindAndBits(IR::BailOutKind bailOutKind);
#endif

namespace Js
{
    enum CacheType : byte;
    enum SlotType : byte;

    struct PolymorphicCallSiteInfo;
    // Information about dynamic profile information loaded from cache.
    // Used to verify whether the loaded information matches the paired function body.
    class DynamicProfileFunctionInfo
    {
    public:
        Js::ArgSlot paramInfoCount;
        ProfileId ldElemInfoCount;
        ProfileId stElemInfoCount;
        ProfileId arrayCallSiteCount;
        ProfileId slotInfoCount;
        ProfileId callSiteInfoCount;
        ProfileId returnTypeInfoCount;
        ProfileId divCount;
        ProfileId switchCount;
        uint loopCount;
        uint fldInfoCount;
    };

    enum ThisType : BYTE
    {
        ThisType_Unknown = 0,
        ThisType_Simple,
        ThisType_Mapped
    };

    struct ThisInfo
    {
        ValueType valueType;
        ThisType thisType;

        ThisInfo() : thisType(ThisType_Unknown)
        {
        }
    };



    // TODO: include ImplicitCallFlags in this structure
    struct LoopFlags
    {
        // maintain the bits and the enum at the same time, it must match
        bool isInterpreted : 1;
        bool memopMinCountReached : 1;
        enum
        {
            INTERPRETED,
            MEMOP_MIN_COUNT_FOUND,
            COUNT
        };

        LoopFlags() :
            isInterpreted(false),
            memopMinCountReached(false)
        {
            CompileAssert((sizeof(LoopFlags) * 8) >= LoopFlags::COUNT);
        }
        // Right now supports up to 8 bits.
        typedef byte LoopFlags_t;
        LoopFlags(uint64 flags)
        {
            Assert(flags >> LoopFlags::COUNT == 0);
            LoopFlags_t* thisFlags = (LoopFlags_t *)this;
            CompileAssert(sizeof(LoopFlags_t) == sizeof(LoopFlags));
            *thisFlags = (LoopFlags_t)flags;
        }
    };

    enum FldInfoFlags : BYTE
    {
        FldInfo_NoInfo                      = 0x00,
        FldInfo_FromLocal                   = 0x01,
        FldInfo_FromProto                   = 0x02,
        FldInfo_FromLocalWithoutProperty    = 0x04,
        FldInfo_FromAccessor                = 0x08,
        FldInfo_Polymorphic                 = 0x10,
        FldInfo_FromInlineSlots             = 0x20,
        FldInfo_FromAuxSlots                = 0x40,
        FldInfo_InlineCandidate             = 0x80
    };

    struct FldInfo
    {
        typedef struct { ValueType::TSize f1; byte f2; byte f3; } TSize;

        ValueType valueType;
        FldInfoFlags flags;
        byte polymorphicInlineCacheUtilization;

        bool ShouldUsePolymorphicInlineCache()
        {
#if DBG
            if (PHASE_FORCE1(PolymorphicInlineCachePhase))
            {
                return true;
            }
#endif
            return polymorphicInlineCacheUtilization > PolymorphicInlineCacheUtilizationThreshold;
        }

        bool WasLdFldProfiled() const
        {
            return !valueType.IsUninitialized();
        }

        uint32 GetOffsetOfFlags() { return offsetof(FldInfo, flags); }
    };
    CompileAssert(sizeof(FldInfo::TSize) == sizeof(FldInfo));

    struct LdElemInfo
    {
        ValueType arrayType;
        ValueType elemType;

        union
        {
            struct
            {
                bool wasProfiled : 1;
                bool neededHelperCall : 1;
            };
            byte bits;
        };

        LdElemInfo() : bits(0)
        {
            wasProfiled = true;
        }

        void Merge(const LdElemInfo &other)
        {
            arrayType = arrayType.Merge(other.arrayType);
            elemType = elemType.Merge(other.elemType);
            bits |= other.bits;
        }

        ValueType GetArrayType() const
        {
            return arrayType;
        }

        ValueType GetElementType() const
        {
            return elemType;
        }

        bool WasProfiled() const
        {
            return wasProfiled;
        }

        bool LikelyNeedsHelperCall() const
        {
            return neededHelperCall;
        }
    };

    struct StElemInfo
    {
        ValueType arrayType;

        union
        {
            struct
            {
                bool wasProfiled : 1;
                bool createdMissingValue : 1;
                bool filledMissingValue : 1;
                bool neededHelperCall : 1;
                bool storedOutsideHeadSegmentBounds : 1;
                bool storedOutsideArrayBounds : 1;
            };
            byte bits;
        };

        StElemInfo() : bits(0)
        {
            wasProfiled = true;
        }

        void Merge(const StElemInfo &other)
        {
            arrayType = arrayType.Merge(other.arrayType);
            bits |= other.bits;
        }

        ValueType GetArrayType() const
        {
            return arrayType;
        }

        bool WasProfiled() const
        {
            return wasProfiled;
        }

        bool LikelyCreatesMissingValue() const
        {
            return createdMissingValue;
        }

        bool LikelyFillsMissingValue() const
        {
            return filledMissingValue;
        }

        bool LikelyNeedsHelperCall() const
        {
            return createdMissingValue || filledMissingValue || neededHelperCall || storedOutsideHeadSegmentBounds;
        }

        bool LikelyStoresOutsideHeadSegmentBounds() const
        {
            return createdMissingValue || storedOutsideHeadSegmentBounds;
        }

        bool LikelyStoresOutsideArrayBounds() const
        {
            return storedOutsideArrayBounds;
        }
    };

    struct ArrayCallSiteInfo
    {
        union {
            struct {
                byte isNotNativeInt : 1;
                byte isNotNativeFloat : 1;
#if ENABLE_COPYONACCESS_ARRAY
                byte isNotCopyOnAccessArray : 1;
                byte copyOnAccessArrayCacheIndex : 5;
#endif
            };
            byte bits;
        };
#if DBG
        uint functionNumber;
        ProfileId callSiteNumber;
#endif

        bool IsNativeIntArray() const { return !(bits & NotNativeIntBit) && !PHASE_OFF1(NativeArrayPhase); }
        bool IsNativeFloatArray() const { return !(bits & NotNativeFloatBit) && !PHASE_OFF1(NativeArrayPhase); }
        bool IsNativeArray() const { return IsNativeFloatArray(); }
        void SetIsNotNativeIntArray();
        void SetIsNotNativeFloatArray();
        void SetIsNotNativeArray();

        static uint32 GetOffsetOfBits() { return offsetof(ArrayCallSiteInfo, bits); }
        static byte const NotNativeIntBit = 1;
        static byte const NotNativeFloatBit = 2;
    };

    class DynamicProfileInfo
    {
    public:
        static DynamicProfileInfo* New(Recycler* recycler, FunctionBody* functionBody, bool persistsAcrossScriptContexts = false);
        void Initialize(FunctionBody *const functionBody);

    public:
        static bool IsEnabledForAtLeastOneFunction(const ScriptContext *const scriptContext);
        static bool IsEnabled(const FunctionBody *const functionBody);
    private:
        static bool IsEnabled_OptionalFunctionBody(const FunctionBody *const functionBody, const ScriptContext *const scriptContext);

    public:
        static bool IsEnabledForAtLeastOneFunction(const Js::Phase phase, const ScriptContext *const scriptContext);
        static bool IsEnabled(const Js::Phase phase, const FunctionBody *const functionBody);
    private:
        static bool IsEnabled_OptionalFunctionBody(const Js::Phase phase, const FunctionBody *const functionBody, const ScriptContext *const scriptContext);

    public:
        static bool EnableImplicitCallFlags(const FunctionBody *const functionBody);

        static Var EnsureDynamicProfileInfoThunk(RecyclableObject * function, CallInfo callInfo, ...);

#ifdef DYNAMIC_PROFILE_STORAGE
        bool HasFunctionBody() const { return hasFunctionBody; }
        FunctionBody * GetFunctionBody() const { Assert(hasFunctionBody); return functionBody; }
#endif

        void RecordElementLoad(FunctionBody* functionBody, ProfileId ldElemId, const LdElemInfo& info);
        void RecordElementLoadAsProfiled(FunctionBody *const functionBody, const ProfileId ldElemId);
        const LdElemInfo *GetLdElemInfo() const { return ldElemInfo; }

        void RecordElementStore(FunctionBody* functionBody, ProfileId stElemId, const StElemInfo& info);
        void RecordElementStoreAsProfiled(FunctionBody *const functionBody, const ProfileId stElemId);
        const StElemInfo *GetStElemInfo() const { return stElemInfo; }

        ArrayCallSiteInfo *GetArrayCallSiteInfo(FunctionBody *functionBody, ProfileId index) const;

        void RecordFieldAccess(FunctionBody* functionBody, uint fieldAccessId, Var object, FldInfoFlags flags);
        void RecordPolymorphicFieldAccess(FunctionBody *functionBody, uint fieldAccessid);
        bool HasPolymorphicFldAccess() const { return bits.hasPolymorphicFldAccess; }
        FldInfo * GetFldInfo(FunctionBody* functionBody, uint fieldAccessId) const;

        void RecordSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId, Var object);
        ValueType GetSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId) const;

        void RecordThisInfo(Var object, ThisType thisType);
        ThisInfo GetThisInfo() const;

        void RecordDivideResultType(FunctionBody* body, ProfileId divideId, Var object);
        ValueType GetDivideResultType(FunctionBody* body, ProfileId divideId) const;

        void RecordModulusOpType(FunctionBody* body, ProfileId profileId, bool isModByPowerOf2);
        bool IsModulusOpByPowerOf2(FunctionBody* body, ProfileId profileId) const;

        void RecordSwitchType(FunctionBody* body, ProfileId switchId, Var object);
        ValueType GetSwitchType(FunctionBody* body, ProfileId switchId) const;

        void RecordCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo * calleeFunctionInfo, JavascriptFunction* calleeFunction, ArgSlot actualArgCount, bool isConstructorCall, InlineCacheIndex ldFldInlnlineCacheId = Js::Constants::NoInlineCacheIndex);
        void RecordConstParameterAtCallSite(ProfileId callSiteId, int argNum);
        bool HasCallSiteInfo(FunctionBody* functionBody);
        bool HasCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId); // Does a particular callsite have ProfileInfo?
        FunctionInfo * GetCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, bool *isPolymorphicCall);
        uint16 GetConstantArgInfo(ProfileId callSiteId);
        uint GetLdFldCacheIndexFromCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId);
        bool GetPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, __inout_ecount(functionBodyArrayLength) FunctionBody** functionBodyArray, uint functionBodyArrayLength);

        bool RecordLdFldCallSiteInfo(FunctionBody* functionBody, RecyclableObject* callee, bool callApplyTarget);

        bool hasLdFldCallSiteInfo();

        void RecordReturnTypeOnCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, Var object);
        void RecordReturnType(FunctionBody* functionBody, ProfileId callSiteId, Var object);
        ValueType GetReturnType(FunctionBody* functionBody, Js::OpCode opcode, ProfileId callSiteId) const;

        void RecordParameterInfo(FunctionBody* functionBody, ArgSlot index, Var object);
        ValueType GetParameterInfo(FunctionBody* functionBody, ArgSlot index) const;

        void RecordLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum, ImplicitCallFlags flags);
        ImplicitCallFlags GetLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum) const;

        void RecordImplicitCallFlags(ImplicitCallFlags flags);
        ImplicitCallFlags GetImplicitCallFlags() const;

        static void Save(ScriptContext * scriptContext);

        void UpdateFunctionInfo(FunctionBody* functionBody, Recycler* allocator);
        void    ResetAllPolymorphicCallSiteInfo();

        bool CallSiteHasProfileData(ProfileId callSiteId)
        {
            return this->callSiteInfo[callSiteId].isPolymorphic
                || this->callSiteInfo[callSiteId].u.functionData.sourceId != NoSourceId
                || this->callSiteInfo[callSiteId].dontInline;
        }

        static bool IsProfiledCallOp(OpCode op);
        static bool IsProfiledReturnTypeOp(OpCode op);

        static FldInfoFlags FldInfoFlagsFromCacheType(CacheType cacheType);
        static FldInfoFlags FldInfoFlagsFromSlotType(SlotType slotType);
        static FldInfoFlags MergeFldInfoFlags(FldInfoFlags oldFlags, FldInfoFlags newFlags);

        const static uint maxPolymorphicInliningSize = 4;

#if DBG_DUMP
        static void DumpScriptContext(ScriptContext * scriptContext);
        static wchar_t const * GetImplicitCallFlagsString(ImplicitCallFlags flags);
#endif
#ifdef RUNTIME_DATA_COLLECTION
        static void DumpScriptContextToFile(ScriptContext * scriptContext);
#endif
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        static bool NeedProfileInfoList();
#endif
#ifdef DYNAMIC_PROFILE_MUTATOR
        friend class DynamicProfileMutatorImpl;
#endif
#if JS_PROFILE_DATA_INTERFACE
        friend class ProfileDataObject;
#endif

    private:
        // Have the dynamicProfileFunctionInfo after loaded from cache.
        // Replaced with the function body it is verified and matched (See DynamicProfileInfo::MatchFunctionBody)
        DynamicProfileFunctionInfo * dynamicProfileFunctionInfo;
        struct CallSiteInfo
        {
            uint16 isArgConstant : 13;
            uint16 isConstructorCall : 1;
            uint16 dontInline : 1;
            uint16 isPolymorphic : 1;
            ValueType returnType;
            InlineCacheIndex ldFldInlineCacheId;
            union
            {
                struct
                {
                    Js::SourceId sourceId;
                    Js::LocalFunctionId functionId;
                } functionData;
                // As of now polymorphic info is allocated only if the source Id is current
                PolymorphicCallSiteInfo* polymorphicCallSiteInfo;
            } u;
        } *callSiteInfo;
        ValueType * returnTypeInfo; // return type of calls for non inline call sites
        ValueType * divideTypeInfo;
        ValueType * switchTypeInfo;
        LdElemInfo * ldElemInfo;
        StElemInfo * stElemInfo;
        ArrayCallSiteInfo *arrayCallSiteInfo;
        ValueType * parameterInfo;
        FldInfo * fldInfo;
        ValueType * slotInfo;
        ImplicitCallFlags * loopImplicitCallFlags;
        ImplicitCallFlags implicitCallFlags;
        BVFixed* loopFlags;
        ThisInfo thisInfo;

        // TODO (jedmiad): Consider storing a pair of property ID bit vectors indicating which properties are
        // known to be non-fixed or non-equivalent. We could turn these on if we bailed out of fixed field type
        // checks and equivalent type checks in a way that indicates one of these failures as opposed to type
        // mismatch.

        struct Bits
        {
            bool disableAggressiveIntTypeSpec : 1;
            bool disableAggressiveIntTypeSpec_jitLoopBody : 1;
            bool disableAggressiveMulIntTypeSpec : 1;
            bool disableAggressiveMulIntTypeSpec_jitLoopBody : 1;
            bool disableDivIntTypeSpec : 1;
            bool disableDivIntTypeSpec_jitLoopBody : 1;
            bool disableLossyIntTypeSpec : 1;
            // TODO: put this flag in LoopFlags if we can find a reliable way to determine the loopNumber in bailout for a hoisted instr
            bool disableMemOp : 1;
            bool disableTrackCompoundedIntOverflow : 1;
            bool disableFloatTypeSpec : 1;
            bool disableCheckThis : 1;
            bool disableArrayCheckHoist : 1;
            bool disableArrayCheckHoist_jitLoopBody : 1;
            bool disableArrayMissingValueCheckHoist : 1;
            bool disableArrayMissingValueCheckHoist_jitLoopBody : 1;
            bool disableJsArraySegmentHoist : 1;
            bool disableJsArraySegmentHoist_jitLoopBody : 1;
            bool disableArrayLengthHoist : 1;
            bool disableArrayLengthHoist_jitLoopBody : 1;
            bool disableTypedArrayTypeSpec : 1;
            bool disableTypedArrayTypeSpec_jitLoopBody : 1;
            bool disableLdLenIntSpec : 1;
            bool disableBoundCheckHoist : 1;
            bool disableBoundCheckHoist_jitLoopBody : 1;
            bool disableLoopCountBasedBoundCheckHoist : 1;
            bool disableLoopCountBasedBoundCheckHoist_jitLoopBody : 1;
            bool hasPolymorphicFldAccess : 1;
            bool hasLdFldCallSite : 1; // getters, setters, .apply (possibly .call too in future)
            bool disableFloorInlining : 1;
            bool disableNoProfileBailouts : 1;
            bool disableSwitchOpt : 1;
            bool disableEquivalentObjTypeSpec : 1;
            bool disableObjTypeSpec_jitLoopBody : 1;
        } bits;

        uint32 m_recursiveInlineInfo; // Bit is set for each callsites where the function is called recursively
        BYTE currentInlinerVersion; // Used to detect when inlining profile changes
        uint32 polymorphicCacheState;
        bool hasFunctionBody;

#if DBG
        bool persistsAcrossScriptContexts;
#endif
        static JavascriptMethod EnsureDynamicProfileInfo(Js::ScriptFunction * function);
#if DBG_DUMP
        static void DumpList(SListBase<DynamicProfileInfo *> * profileInfoList, ArenaAllocator * dynamicProfileInfoAllocator);
        static void DumpProfiledValue(wchar_t const * name, uint * value, uint count);
        static void DumpProfiledValue(wchar_t const * name, ValueType * value, uint count);
        static void DumpProfiledValue(wchar_t const * name, CallSiteInfo * callSiteInfo, uint count);
        static void DumpProfiledValue(wchar_t const * name, ArrayCallSiteInfo * arrayCallSiteInfo, uint count);
        static void DumpProfiledValue(wchar_t const * name, ImplicitCallFlags * loopImplicitCallFlags, uint count);
        template<class TData, class FGetValueType>
        static void DumpProfiledValuesGroupedByValue(const wchar_t *const name, const TData *const data, const uint count, const FGetValueType GetValueType, ArenaAllocator *const dynamicProfileInfoAllocator);
        static void DumpFldInfoFlags(wchar_t const * name, FldInfo * fldInfo, uint count, FldInfoFlags value, wchar_t const * valueName);

        static void DumpLoopInfo(FunctionBody *fbody);
#endif

        bool IsPolymorphicCallSite(Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::LocalFunctionId oldFunctionId, Js::SourceId oldSourceId);
        void CreatePolymorphicDynamicProfileCallSiteInfo(FunctionBody * funcBody, ProfileId callSiteId, Js::LocalFunctionId functionId, Js::LocalFunctionId oldFunctionId, Js::SourceId sourceId, Js::SourceId oldSourceId);
        void ResetPolymorphicCallSiteInfo(ProfileId callSiteId, Js::LocalFunctionId functionId);
        void SetFunctionIdSlotForNewPolymorphicCall(ProfileId callSiteId, Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::FunctionBody *inliner);
        void RecordPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo * calleeFunctionInfo);
#ifdef RUNTIME_DATA_COLLECTION
        static CriticalSection s_csOutput;
        template <typename T>
        static void WriteData(T data, FILE * file);
        template <>
        static void WriteData<wchar_t const *>(wchar_t const * sz, FILE * file);
        template <>
        static void WriteData<FunctionInfo *>(FunctionInfo * functionInfo, FILE * file); // Not defined, to prevent accidentally writing function info
        template <>
        static void WriteData<FunctionBody *>(FunctionBody * functionInfo, FILE * file);
        template <typename T>
        static void WriteArray(uint count, T * arr, FILE * file);
#endif
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        FunctionBody * functionBody; // This will only be populated if NeedProfileInfoList is true
#endif
#ifdef DYNAMIC_PROFILE_STORAGE
        // Used by de-serialize
        DynamicProfileInfo();

        template <typename T>
        static DynamicProfileInfo * Deserialize(T * reader, Recycler* allocator, Js::LocalFunctionId * functionId);
        template <typename T>
        bool Serialize(T * writer);

        static void UpdateSourceDynamicProfileManagers(ScriptContext * scriptContext);
#endif
        static Js::LocalFunctionId const CallSiteMixed = (Js::LocalFunctionId)-1;
        static Js::LocalFunctionId const CallSiteCrossContext = (Js::LocalFunctionId)-2;
        static Js::LocalFunctionId const CallSiteNonFunction = (Js::LocalFunctionId)-3;
        static Js::LocalFunctionId const CallSiteNoInfo = (Js::LocalFunctionId)-4;
        static Js::LocalFunctionId const StartInvalidFunction = (Js::LocalFunctionId)-4;

        static Js::SourceId const NoSourceId        = (SourceId)-1;
        static Js::SourceId const BuiltInSourceId   = (SourceId)-2;
        static Js::SourceId const CurrentSourceId   = (SourceId)-3; // caller and callee in the same file
        static Js::SourceId const InvalidSourceId   = (SourceId)-4;

        bool MatchFunctionBody(FunctionBody * functionBody);

        DynamicProfileInfo(FunctionBody * functionBody);

        friend class SourceDynamicProfileManager;

    public:
        bool IsAggressiveIntTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableAggressiveIntTypeSpec_jitLoopBody
                    : this->bits.disableAggressiveIntTypeSpec;
        }

        void DisableAggressiveIntTypeSpec(const bool isJitLoopBody)
        {
            this->bits.disableAggressiveIntTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableAggressiveIntTypeSpec = true;
            }
        }

        bool IsAggressiveMulIntTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableAggressiveMulIntTypeSpec_jitLoopBody
                    : this->bits.disableAggressiveMulIntTypeSpec;
        }

        void DisableAggressiveMulIntTypeSpec(const bool isJitLoopBody)
        {
            this->bits.disableAggressiveMulIntTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableAggressiveMulIntTypeSpec = true;
            }
        }

        bool IsDivIntTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableDivIntTypeSpec_jitLoopBody
                    : this->bits.disableDivIntTypeSpec;
        }

        void DisableDivIntTypeSpec(const bool isJitLoopBody)
        {
            this->bits.disableDivIntTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableDivIntTypeSpec = true;
            }
        }

        bool IsLossyIntTypeSpecDisabled() const { return bits.disableLossyIntTypeSpec; }
        void DisableLossyIntTypeSpec() { this->bits.disableLossyIntTypeSpec = true; }
        LoopFlags GetLoopFlags(int loopNumber) const
        {
            Assert(loopFlags);
            return loopFlags->GetRange<LoopFlags>(loopNumber * LoopFlags::COUNT, LoopFlags::COUNT);
        }
        void SetLoopInterpreted(int loopNumber) { loopFlags->Set(loopNumber * LoopFlags::COUNT + LoopFlags::INTERPRETED); }
        void SetMemOpMinReached(int loopNumber) { loopFlags->Set(loopNumber * LoopFlags::COUNT + LoopFlags::MEMOP_MIN_COUNT_FOUND); }
        bool IsMemOpDisabled() const { return this->bits.disableMemOp; }
        void DisableMemOp() { this->bits.disableMemOp = true; }
        bool IsTrackCompoundedIntOverflowDisabled() const { return this->bits.disableTrackCompoundedIntOverflow; }
        void DisableTrackCompoundedIntOverflow() { this->bits.disableTrackCompoundedIntOverflow = true; }
        bool IsFloatTypeSpecDisabled() const { return this->bits.disableFloatTypeSpec; }
        void DisableFloatTypeSpec() { this->bits.disableFloatTypeSpec = true; }
        bool IsCheckThisDisabled() const { return this->bits.disableCheckThis; }
        void DisableCheckThis() { this->bits.disableCheckThis = true; }

        bool IsArrayCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableArrayCheckHoist_jitLoopBody
                    : this->bits.disableArrayCheckHoist;
        }

        void DisableArrayCheckHoist(const bool isJitLoopBody)
        {
            this->bits.disableArrayCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableArrayCheckHoist = true;
            }
        }

        bool IsArrayMissingValueCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableArrayMissingValueCheckHoist_jitLoopBody
                    : this->bits.disableArrayMissingValueCheckHoist;
        }

        void DisableArrayMissingValueCheckHoist(const bool isJitLoopBody)
        {
            this->bits.disableArrayMissingValueCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableArrayMissingValueCheckHoist = true;
            }
        }

        bool IsJsArraySegmentHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableJsArraySegmentHoist_jitLoopBody
                    : this->bits.disableJsArraySegmentHoist;
        }

        void DisableJsArraySegmentHoist(const bool isJitLoopBody)
        {
            this->bits.disableJsArraySegmentHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableJsArraySegmentHoist = true;
            }
        }

        bool IsArrayLengthHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableArrayLengthHoist_jitLoopBody
                    : this->bits.disableArrayLengthHoist;
        }

        void DisableArrayLengthHoist(const bool isJitLoopBody)
        {
            this->bits.disableArrayLengthHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableArrayLengthHoist = true;
            }
        }

        bool IsTypedArrayTypeSpecDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableTypedArrayTypeSpec_jitLoopBody
                    : this->bits.disableTypedArrayTypeSpec;
        }

        void DisableTypedArrayTypeSpec(const bool isJitLoopBody)
        {
            this->bits.disableTypedArrayTypeSpec_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableTypedArrayTypeSpec = true;
            }
        }

        bool IsLdLenIntSpecDisabled() const { return this->bits.disableLdLenIntSpec; }
        void DisableLdLenIntSpec() { this->bits.disableLdLenIntSpec = true; }

        bool IsBoundCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableBoundCheckHoist_jitLoopBody
                    : this->bits.disableBoundCheckHoist;
        }

        void DisableBoundCheckHoist(const bool isJitLoopBody)
        {
            this->bits.disableBoundCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableBoundCheckHoist = true;
            }
        }

        bool IsLoopCountBasedBoundCheckHoistDisabled(const bool isJitLoopBody) const
        {
            return
                isJitLoopBody
                    ? this->bits.disableLoopCountBasedBoundCheckHoist_jitLoopBody
                    : this->bits.disableLoopCountBasedBoundCheckHoist;
        }

        void DisableLoopCountBasedBoundCheckHoist(const bool isJitLoopBody)
        {
            this->bits.disableLoopCountBasedBoundCheckHoist_jitLoopBody = true;
            if (!isJitLoopBody)
            {
                this->bits.disableLoopCountBasedBoundCheckHoist = true;
            }
        }

        BYTE GetInlinerVersion() { return this->currentInlinerVersion; }
        uint32 GetPolymorphicCacheState() const { return this->polymorphicCacheState; }
        uint32 GetRecursiveInlineInfo() const { return this->m_recursiveInlineInfo; }
        void SetHasNewPolyFieldAccess(FunctionBody *functionBody);
        bool IsFloorInliningDisabled() const { return this->bits.disableFloorInlining; }
        void DisableFloorInlining() { this->bits.disableFloorInlining = true; }
        bool IsNoProfileBailoutsDisabled() const { return this->bits.disableNoProfileBailouts; }
        void DisableNoProfileBailouts() { this->bits.disableNoProfileBailouts = true; }
        bool IsSwitchOptDisabled() const { return this->bits.disableSwitchOpt; }
        void DisableSwitchOpt() { this->bits.disableSwitchOpt = true; }
        bool IsEquivalentObjTypeSpecDisabled() const { return this->bits.disableEquivalentObjTypeSpec; }
        void DisableEquivalentObjTypeSpec() { this->bits.disableEquivalentObjTypeSpec = true; }
        bool IsObjTypeSpecDisabledInJitLoopBody() const { return this->bits.disableObjTypeSpec_jitLoopBody; }
        void DisableObjTypeSpecInJitLoopBody() { this->bits.disableObjTypeSpec_jitLoopBody = true; }

        static bool IsCallSiteNoInfo(Js::LocalFunctionId functionId) { return functionId == CallSiteNoInfo; }
#if DBG_DUMP
        void Dump(FunctionBody* functionBody, ArenaAllocator * dynamicProfileInfoAllocator = nullptr);
#endif
    private:
        static void InstantiateForceInlinedMembers();
    };

    struct PolymorphicCallSiteInfo
    {
        Js::LocalFunctionId functionIds[DynamicProfileInfo::maxPolymorphicInliningSize];
        Js::SourceId sourceIds[DynamicProfileInfo::maxPolymorphicInliningSize];
        PolymorphicCallSiteInfo *next;
        bool GetFunction(uint index, Js::LocalFunctionId *functionId, Js::SourceId *sourceId)
        {
            Assert(index < DynamicProfileInfo::maxPolymorphicInliningSize);
            Assert(functionId);
            Assert(sourceId);
            if (DynamicProfileInfo::IsCallSiteNoInfo(functionIds[index]))
            {
                return false;
            }
            *functionId = functionIds[index];
            *sourceId = sourceIds[index];
            return true;
        }
    };

#ifdef DYNAMIC_PROFILE_STORAGE
    class BufferReader
    {
    public:
        BufferReader(__in_ecount(length) char const * buffer, size_t length) : current(buffer), lengthLeft(length) {}

        template <typename T>
        bool Read(T * data)
        {
            if (lengthLeft < sizeof(T))
            {
                return false;
            }
            *data = *(T *)current;
            current += sizeof(T);
            lengthLeft -= sizeof(T);
            return true;
        }

        template <typename T>
        bool Peek(T * data)
        {
            if (lengthLeft < sizeof(T))
            {
                return false;
            }
            *data = *(T *)current;
            return true;
        }

        template <typename T>
        bool ReadArray(__inout_ecount(len) T * data, size_t len)
        {
            size_t size = sizeof(T) * len;
            if (lengthLeft < size)
            {
                return false;
            }
            memcpy_s(data, size, current, size);
            current += size;
            lengthLeft -= size;
            return true;
        }
    private:
        char const * current;
        size_t lengthLeft;
    };

    class BufferSizeCounter
    {
    public:
        BufferSizeCounter() : count(0) {}
        size_t GetByteCount() const { return count; }
        template <typename T>
        bool Write(T const& data)
        {
            return WriteArray(&data, 1);
        }

#if DBG_DUMP
        void Log(DynamicProfileInfo* info) {}
#endif

        template <typename T>
        bool WriteArray(__in_ecount(len) T * data, size_t len)
        {
            count += sizeof(T) * len;
            return true;
        }
    private:
        size_t count;
    };

    class BufferWriter
    {
    public:
        BufferWriter(__in_ecount(length) char * buffer, size_t length) : current(buffer), lengthLeft(length) {}

        template <typename T>
        bool Write(T const& data)
        {
            return WriteArray(&data, 1);
        }

#if DBG_DUMP
        void Log(DynamicProfileInfo* info);
#endif
        template <typename T>
        bool WriteArray(__in_ecount(len) T * data, size_t len)
        {
            size_t size = sizeof(T) * len;
            if (lengthLeft < size)
            {
                return false;
            }
            memcpy_s(current, size, data, size);
            current += size;
            lengthLeft -= size;
            return true;
        }
    private:
        char * current;
        size_t lengthLeft;
    };
#endif
};
#endif
