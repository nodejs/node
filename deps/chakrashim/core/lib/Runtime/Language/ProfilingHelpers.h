//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
#if ENABLE_PROFILE_INFO
    class ProfilingHelpers
    {
    public:
        static Var ProfiledLdElem(const Var base, const Var varIndex, FunctionBody *const functionBody, const ProfileId profileId);
        static Var ProfiledLdElem_FastPath(JavascriptArray *const array, const Var varIndex, ScriptContext *const scriptContext, LdElemInfo *const ldElemInfo = nullptr);

    public:
        static void ProfiledStElem_DefaultFlags(const Var base, const Var varIndex, const Var value, FunctionBody *const functionBody, const ProfileId profileId);
        static void ProfiledStElem(const Var base, const Var varIndex, const Var value, FunctionBody *const functionBody, const ProfileId profileId, const PropertyOperationFlags flags);
        static void ProfiledStElem_FastPath(JavascriptArray *const array, const Var varIndex, const Var value, ScriptContext *const scriptContext, const PropertyOperationFlags flags, StElemInfo *const stElemInfo = nullptr);

    public:
        static JavascriptArray *ProfiledNewScArray(const uint length, FunctionBody *const functionBody, const ProfileId profileId);

    public:
        static Var ProfiledNewScObjArray_Jit(const Var callee, void *const framePointer, const ProfileId profileId, const ProfileId arrayProfileId, CallInfo callInfo, ...);
        static Var ProfiledNewScObjArraySpread_Jit(const Js::AuxArray<uint32> *spreadIndices, const Var callee, void *const framePointer, const ProfileId profileId, const ProfileId arrayProfileId, CallInfo callInfo, ...);
        static Var ProfiledNewScObjArray(const Var callee, const Arguments args, ScriptFunction *const caller, const ProfileId profileId, const ProfileId arrayProfileId);

    public:
        static Var ProfiledNewScObject(const Var callee, const Arguments args, FunctionBody *const callerFunctionBody, const ProfileId profileId, const InlineCacheIndex inlineCacheIndex = Constants::NoInlineCacheIndex, const Js::AuxArray<uint32> *spreadIndices = nullptr);

    public:
        static void ProfileLdSlot(const Var value, FunctionBody *const functionBody, const ProfileId profileId);

    public:
        static Var ProfiledLdFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer);
        static Var ProfiledLdSuperFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer, const Var thisInstance);
        static Var ProfiledLdFldForTypeOf_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer);
        static Var ProfiledLdRootFldForTypeOf_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer);
        static Var ProfiledLdFld_CallApplyTarget_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer);
        static Var ProfiledLdMethodFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer);
        static Var ProfiledLdRootFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer);
        static Var ProfiledLdRootMethodFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, void *const framePointer);
        template<bool Root, bool Method, bool CallApplyTarget> static Var ProfiledLdFld(const Var instance, const PropertyId propertyId, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FunctionBody *const functionbody, const Var thisInstance);
        template<bool Root, bool Method, bool CallApplyTarget> static Var ProfiledLdFldForTypeOf(const Var instance, const PropertyId propertyId, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FunctionBody *const functionbody);

    public:
        static void ProfiledStFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, const Var value, void *const framePointer);
        static void ProfiledStSuperFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, const Var value, void *const framePointer, const Var thisInstance);
        static void ProfiledStFld_Strict_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, const Var value, void *const framePointer);
        static void ProfiledStRootFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, const Var value, void *const framePointer);
        static void ProfiledStRootFld_Strict_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, const Var value, void *const framePointer);
        template<bool Root> static void ProfiledStFld(const Var instance, const PropertyId propertyId, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, const Var value, const PropertyOperationFlags flags, ScriptFunction *const scriptFunction, const Var thisInstance);

    public:
        static void ProfiledInitFld_Jit(const Var instance, const PropertyId propertyId, const InlineCacheIndex inlineCacheIndex, const Var value, void *const framePointer);
        static void ProfiledInitFld(RecyclableObject *const object, const PropertyId propertyId, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, const Var value, FunctionBody *const functionBody);

    private:
        static void UpdateFldInfoFlagsForGetSetInlineCandidate(RecyclableObject *const object, FldInfoFlags &fldInfoFlags, const CacheType cacheType, InlineCache *const inlineCache, FunctionBody *const functionBody);
        static void UpdateFldInfoFlagsForCallApplyInlineCandidate(RecyclableObject *const object, FldInfoFlags &fldInfoFlags, const CacheType cacheType, InlineCache *const inlineCache, FunctionBody *const functionBody);
        static InlineCache *GetInlineCache(ScriptFunction *const scriptFunction, const InlineCacheIndex inlineCacheIndex);
    };
#endif
}
