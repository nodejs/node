//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_NATIVE_CODEGEN
namespace Js
{
    ObjTypeSpecFldInfo* ObjTypeSpecFldInfo::CreateFrom(uint id, InlineCache* cache, uint cacheId, EntryPointInfo *entryPoint,
        FunctionBody* const topFunctionBody, FunctionBody *const functionBody, FieldAccessStatsPtr inlineCacheStats)
    {
        if (cache->IsEmpty())
        {
            return nullptr;
        }

        InlineCache localCache(*cache);

        // Need to keep a reference to the types before memory allocation in case they are tagged
        Type * type = nullptr;
        Type * typeWithoutProperty = nullptr;
        Js::Type * propertyOwnerType;
        bool isLocal = localCache.IsLocal();
        bool isProto = localCache.IsProto();
        bool isAccessor = localCache.IsAccessor();
        bool isGetter = localCache.IsGetterAccessor();
        if (isLocal)
        {
            type = TypeWithoutAuxSlotTag(localCache.u.local.type);
            if (localCache.u.local.typeWithoutProperty)
            {
                typeWithoutProperty = TypeWithoutAuxSlotTag(localCache.u.local.typeWithoutProperty);
            }
            propertyOwnerType = type;
        }
        else if (isProto)
        {
            type = TypeWithoutAuxSlotTag(localCache.u.proto.type);
            propertyOwnerType = localCache.u.proto.prototypeObject->GetType();
        }
        else
        {
            if (PHASE_OFF(Js::FixAccessorPropsPhase, functionBody))
            {
                return nullptr;
            }

            type = TypeWithoutAuxSlotTag(localCache.u.accessor.type);
            propertyOwnerType = localCache.u.accessor.object->GetType();
        }

        ScriptContext* scriptContext = functionBody->GetScriptContext();
        Recycler *const recycler = scriptContext->GetRecycler();

        Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(cacheId);
        uint16 slotIndex = Constants::NoSlot;
        bool usesAuxSlot = false;
        DynamicObject* prototypeObject = nullptr;
        PropertyGuard* propertyGuard = nullptr;
        JitTimeConstructorCache* ctorCache = nullptr;
        Var fieldValue = nullptr;
        bool keepFieldValue = false;
        bool isFieldValueFixed = false;
        bool isMissing = false;
        bool isBuiltIn = false;

        // Save untagged type pointers, remembering whether the original type was tagged.
        // The type pointers must be untagged so that the types cannot be collected during JIT.

        if (isLocal)
        {
            slotIndex = localCache.u.local.slotIndex;
            if (type != localCache.u.local.type)
            {
                usesAuxSlot = true;
            }
            if (typeWithoutProperty)
            {
                Assert(entryPoint->GetJitTransferData() != nullptr);
                entryPoint->GetJitTransferData()->AddJitTimeTypeRef(typeWithoutProperty, recycler);

                // These shared property guards are registered on the main thread and checked during entry point installation
                // (see NativeCodeGenerator::CheckCodeGenDone) to ensure that no property became read-only while we were
                // JIT-ing on the background thread.
                propertyGuard = entryPoint->RegisterSharedPropertyGuard(propertyId, scriptContext);
            }
        }
        else if (isProto)
        {
            prototypeObject = localCache.u.proto.prototypeObject;
            slotIndex = localCache.u.proto.slotIndex;
            if (type != localCache.u.proto.type)
            {
                usesAuxSlot = true;
                fieldValue = prototypeObject->GetAuxSlot(slotIndex);
            }
            else
            {
                fieldValue = prototypeObject->GetInlineSlot(slotIndex);
            }
            isMissing = localCache.u.proto.isMissing;
            propertyGuard = entryPoint->RegisterSharedPropertyGuard(propertyId, scriptContext);
        }
        else
        {
            slotIndex = localCache.u.accessor.slotIndex;
            if (type != localCache.u.accessor.type)
            {
                usesAuxSlot = true;
                fieldValue = localCache.u.accessor.object->GetAuxSlot(slotIndex);
            }
            else
            {
                fieldValue = localCache.u.accessor.object->GetInlineSlot(slotIndex);
            }
        }

        // Keep the type alive until the entry point is installed. Note that this is longer than just during JIT, for which references
        // from the JitTimeData would have been enough.
        Assert(entryPoint->GetJitTransferData() != nullptr);
        entryPoint->GetJitTransferData()->AddJitTimeTypeRef(type, recycler);

        bool allFixedPhaseOFF = PHASE_OFF(Js::FixedMethodsPhase, topFunctionBody) & PHASE_OFF(Js::UseFixedDataPropsPhase, topFunctionBody);

        if (!allFixedPhaseOFF)
        {
            Assert(propertyOwnerType != nullptr);
            if (Js::DynamicType::Is(propertyOwnerType->GetTypeId()))
            {
                Js::DynamicTypeHandler* propertyOwnerTypeHandler = ((Js::DynamicType*)propertyOwnerType)->GetTypeHandler();
                Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(cacheId);
                Js::PropertyRecord const * const fixedPropertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);
                Var fixedProperty = nullptr;
                Js::JavascriptFunction* functionObject = nullptr;

                if (isLocal || isProto)
                {
                    if (typeWithoutProperty == nullptr)
                    {
                        // Since we don't know if this cache is used for LdMethodFld, we may fix up (use as fixed) too
                        // aggressively here, if we load a function that we don't actually call.  This happens in the case
                        // of constructors (e.g. Object.defineProperty or Object.create).
                        // TODO (InlineCacheCleanup): if we don't zero some slot(s) in the inline cache, we could store a bit there
                        // indicating if this cache is used by LdMethodFld, and only ask then. We could even store a bit in the cache
                        // indicating that the value loaded is a fixed function (or at least may still be). That last bit could work
                        // even if we zero inline caches, since we would always set it when populating the cache.
                        propertyOwnerTypeHandler->TryUseFixedProperty(fixedPropertyRecord, &fixedProperty, (Js::FixedPropertyKind)(Js::FixedPropertyKind::FixedMethodProperty | Js::FixedPropertyKind::FixedDataProperty), scriptContext);
                    }
                }
                else
                {
                    propertyOwnerTypeHandler->TryUseFixedAccessor(fixedPropertyRecord, &fixedProperty, (Js::FixedPropertyKind)(Js::FixedPropertyKind::FixedAccessorProperty), isGetter, scriptContext);
                }

                if (fixedProperty != nullptr && propertyGuard == nullptr)
                {
                    propertyGuard = entryPoint->RegisterSharedPropertyGuard(propertyId, scriptContext);
                }

                if (fixedProperty != nullptr && Js::JavascriptFunction::Is(fixedProperty))
                {
                    functionObject = (Js::JavascriptFunction *)fixedProperty;
                    if (PHASE_VERBOSE_TRACE(Js::FixedMethodsPhase, functionBody))
                    {
                        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                        wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                        Js::DynamicObject* protoObject = isProto ? prototypeObject : nullptr;
                        Output::Print(L"FixedFields: function %s (%s) cloning cache with fixed method: %s (%s), function: 0x%p, body: 0x%p (cache id: %d, layout: %s, type: 0x%p, proto: 0x%p, proto type: 0x%p)\n",
                            functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer),
                            fixedPropertyRecord->GetBuffer(), functionObject->GetFunctionInfo()->GetFunctionProxy() ?
                            functionObject->GetFunctionInfo()->GetFunctionProxy()->GetDebugNumberSet(debugStringBuffer2) : L"(null)", functionObject, functionObject->GetFunctionInfo(),
                            cacheId, isProto ? L"proto" : L"local", type, protoObject, protoObject != nullptr ? protoObject->GetType() : nullptr);
                        Output::Flush();
                    }

                    if (PHASE_VERBOSE_TESTTRACE(Js::FixedMethodsPhase, functionBody))
                    {
                        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                        wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                        Output::Print(L"FixedFields: function %s (%s) cloning cache with fixed method: %s (%s) (cache id: %d, layout: %s)\n",
                            functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer), fixedPropertyRecord->GetBuffer(), functionObject->GetFunctionInfo()->GetFunctionProxy() ?
                            functionObject->GetFunctionInfo()->GetFunctionProxy()->GetDebugNumberSet(debugStringBuffer2) : L"(null)", functionObject, functionObject->GetFunctionInfo(),
                            cacheId, isProto ? L"proto" : L"local");
                        Output::Flush();
                    }

                    // We don't need to check for a singleton here. We checked that the singleton still existed
                    // when we obtained the fixed field value inside TryUseFixedProperty. Since we don't need the
                    // singleton instance later in this function, we don't care if the instance got collected.
                    // We get the type handler from the propertyOwnerType, which we got from the cache. At runtime,
                    // if the object is collected, it is by definition unreachable and thus the code in the function
                    // we're about to emit cannot reach the object to try to access any of this object's properties.

                    ConstructorCache* runtimeConstructorCache = functionObject->GetConstructorCache();
                    if (runtimeConstructorCache->IsSetUpForJit() && runtimeConstructorCache->GetScriptContext() == scriptContext)
                    {
                        FunctionInfo* functionInfo = functionObject->GetFunctionInfo();
                        Assert(functionInfo != nullptr);
                        Assert((functionInfo->GetAttributes() & FunctionInfo::ErrorOnNew) == 0);

                        Assert(!runtimeConstructorCache->IsDefault());

                        if (runtimeConstructorCache->IsNormal())
                        {
                            Assert(runtimeConstructorCache->GetType()->GetIsShared());
                            // If we populated the cache with initial type before calling the constructor, but then didn't end up updating the cache
                            // after the constructor and shrinking (and locking) the inline slot capacity, we must lock it now.  In that case it is
                            // also possible that the inline slot capacity was shrunk since we originally cached it, so we must update it also.
#if DBG_DUMP
                            if (!runtimeConstructorCache->GetType()->GetTypeHandler()->GetIsInlineSlotCapacityLocked())
                            {
                                if (PHASE_TRACE(Js::FixedNewObjPhase, functionBody))
                                {
                                    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                                    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                                    Output::Print(L"FixedNewObj: function %s (%s) ctor cache for %s (%s) about to be cloned has unlocked inline slot count: guard value = 0x%p, type = 0x%p, slots = %d, inline slots = %d\n",
                                        functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer), fixedPropertyRecord->GetBuffer(), functionObject->GetFunctionInfo()->GetFunctionBody() ?
                                        functionObject->GetFunctionInfo()->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2) : L"(null)",
                                        runtimeConstructorCache->GetRawGuardValue(), runtimeConstructorCache->GetType(),
                                        runtimeConstructorCache->GetSlotCount(), runtimeConstructorCache->GetInlineSlotCount());
                                    Output::Flush();
                                }
                            }
#endif
                            runtimeConstructorCache->GetType()->GetTypeHandler()->EnsureInlineSlotCapacityIsLocked();
                            runtimeConstructorCache->UpdateInlineSlotCount();
                        }

                        // We must keep the runtime cache alive as long as this entry point exists and may try to dereference it.
                        entryPoint->RegisterConstructorCache(runtimeConstructorCache, recycler);
                        ctorCache = RecyclerNew(recycler, JitTimeConstructorCache, functionObject, runtimeConstructorCache);

                        if (PHASE_TRACE(Js::FixedNewObjPhase, functionBody))
                        {
                            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                            wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                            Output::Print(L"FixedNewObj: function %s (%s) cloning ctor cache for %s (%s): guard value = 0x%p, type = 0x%p, slots = %d, inline slots = %d\n",
                                functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer), fixedPropertyRecord->GetBuffer(), functionObject->GetFunctionInfo()->GetFunctionBody() ?
                                functionObject->GetFunctionInfo()->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2) : L"(null)", functionObject, functionObject->GetFunctionInfo(),
                                runtimeConstructorCache->GetRawGuardValue(), runtimeConstructorCache->IsNormal() ? runtimeConstructorCache->GetType() : nullptr,
                                runtimeConstructorCache->GetSlotCount(), runtimeConstructorCache->GetInlineSlotCount());
                            Output::Flush();
                        }
                    }
                    else
                    {
                        if (!runtimeConstructorCache->IsDefault())
                        {
                            if (PHASE_TRACE(Js::FixedNewObjPhase, functionBody))
                            {
                                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                                wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                                Output::Print(L"FixedNewObj: function %s (%s) skipping ctor cache for %s (%s), because %s (guard value = 0x%p, script context = %p).\n",
                                    functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer), fixedPropertyRecord->GetBuffer(), functionObject->GetFunctionInfo()->GetFunctionBody() ?
                                    functionObject->GetFunctionInfo()->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2) : L"(null)", functionObject, functionObject->GetFunctionInfo(),
                                    runtimeConstructorCache->IsEmpty() ? L"cache is empty (or has been cleared)" :
                                    runtimeConstructorCache->IsInvalidated() ? L"cache is invalidated" :
                                    runtimeConstructorCache->SkipDefaultNewObject() ? L"default new object isn't needed" :
                                    runtimeConstructorCache->NeedsTypeUpdate() ? L"cache needs to be updated" :
                                    runtimeConstructorCache->NeedsUpdateAfterCtor() ? L"cache needs update after ctor" :
                                    runtimeConstructorCache->IsPolymorphic() ? L"cache is polymorphic" :
                                    runtimeConstructorCache->GetScriptContext() != functionBody->GetScriptContext() ? L"script context mismatch" : L"of an unexpected situation",
                                    runtimeConstructorCache->GetRawGuardValue(), runtimeConstructorCache->GetScriptContext());
                                Output::Flush();
                            }
                        }
                    }

                    isBuiltIn = Js::JavascriptLibrary::GetBuiltinFunctionForPropId(propertyId) != Js::BuiltinFunction::None;
                }

                if (fixedProperty != nullptr)
                {
                    Assert(fieldValue == nullptr || fieldValue == fixedProperty);
                    fieldValue = fixedProperty;
                    isFieldValueFixed = true;
                    keepFieldValue = true;
                }
            }
        }

        FixedFieldInfo* fixedFieldInfoArray = RecyclerNewArrayZ(recycler, FixedFieldInfo, 1);

        fixedFieldInfoArray[0].fieldValue = fieldValue;
        fixedFieldInfoArray[0].type = type;
        fixedFieldInfoArray[0].nextHasSameFixedField = false;

        ObjTypeSpecFldInfo* info;

        // If we stress equivalent object type spec, let's pretend that every inline cache was polymorphic and equivalent.
        bool forcePoly = false;
        if ((!PHASE_OFF(Js::EquivObjTypeSpecByDefaultPhase, topFunctionBody) ||
            PHASE_STRESS(Js::EquivObjTypeSpecPhase, topFunctionBody))
            && !PHASE_OFF(Js::EquivObjTypeSpecPhase, topFunctionBody))
        {
            Assert(topFunctionBody->HasDynamicProfileInfo());
            auto profileData = topFunctionBody->GetAnyDynamicProfileInfo();
            // We only support equivalent fixed fields on loads from prototype, and no equivalence on missing properties
            forcePoly |= !profileData->IsEquivalentObjTypeSpecDisabled() && (!isFieldValueFixed || isProto) && !isMissing;
        }

        if (isFieldValueFixed)
        {
            // Fixed field checks allow us to assume a specific type ID, but the assumption is only
            // valid if we lock the type. Otherwise, the type ID may change out from under us without
            // evolving the type.
            if (DynamicType::Is(type->GetTypeId()))
            {
                DynamicType *dynamicType = static_cast<DynamicType*>(type);
                if (!dynamicType->GetIsLocked())
                {
                    dynamicType->LockType();
                }
            }
        }

        if (forcePoly)
        {
            uint16 typeCount = 1;
            Js::Type** types = RecyclerNewArray(recycler, Js::Type*, typeCount);
            types[0] = type;
            EquivalentTypeSet* typeSet = RecyclerNew(recycler, EquivalentTypeSet, types, typeCount);

            info = RecyclerNew(recycler, ObjTypeSpecFldInfo,
                id, type->GetTypeId(), typeWithoutProperty, typeSet, usesAuxSlot, isProto, isAccessor, isFieldValueFixed, keepFieldValue, false/*doesntHaveEquivalence*/, false, slotIndex, propertyId,
                prototypeObject, propertyGuard, ctorCache, fixedFieldInfoArray, 1);

            if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
            {
                const PropertyRecord* propertyRecord = scriptContext->GetPropertyName(propertyId);
                Output::Print(L"Created ObjTypeSpecFldInfo: id %u, property %s(#%u), slot %u, type set: 0x%p\n",
                    id, propertyRecord->GetBuffer(), propertyId, slotIndex, type);
                Output::Flush();
            }
        }
        else
        {
            info = RecyclerNew(recycler, ObjTypeSpecFldInfo,
                id, type->GetTypeId(), typeWithoutProperty, usesAuxSlot, isProto, isAccessor, isFieldValueFixed, keepFieldValue, isBuiltIn, slotIndex, propertyId,
                prototypeObject, propertyGuard, ctorCache, fixedFieldInfoArray);

            if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
            {
                const PropertyRecord* propertyRecord = scriptContext->GetPropertyName(propertyId);
                Output::Print(L"Created ObjTypeSpecFldInfo: id %u, property %s(#%u), slot %u, type: 0x%p\n",
                    id, propertyRecord->GetBuffer(), propertyId, slotIndex, type);
                Output::Flush();
            }
        }

        return info;
    }

    ObjTypeSpecFldInfo* ObjTypeSpecFldInfo::CreateFrom(uint id, PolymorphicInlineCache* cache, uint cacheId, EntryPointInfo *entryPoint,
        FunctionBody* const topFunctionBody, FunctionBody *const functionBody, FieldAccessStatsPtr inlineCacheStats)
    {

#ifdef FIELD_ACCESS_STATS
#define IncInlineCacheCount(counter) if (inlineCacheStats) { inlineCacheStats->counter++; }
#else
#define IncInlineCacheCount(counter)
#endif

        Assert(topFunctionBody->HasDynamicProfileInfo());
        auto profileData = topFunctionBody->GetAnyDynamicProfileInfo();

        bool gatherDataForInlining = cache->GetCloneForJitTimeUse() && functionBody->PolyInliningUsingFixedMethodsAllowedByConfigFlags(topFunctionBody);

        if (PHASE_OFF(Js::EquivObjTypeSpecPhase, topFunctionBody) || profileData->IsEquivalentObjTypeSpecDisabled())
        {
            if (!gatherDataForInlining)
            {
                return nullptr;
            }
        }

        Assert(cache->GetSize() < MAXUINT16);
        Js::InlineCache* inlineCaches = cache->GetInlineCaches();
        Js::DynamicObject* prototypeObject = nullptr;
        Js::DynamicObject* accessorOwnerObject = nullptr;
        Js::TypeId typeId = TypeIds_Limit;
        uint16 polyCacheSize = (uint16)cache->GetSize();
        uint16 firstNonEmptyCacheIndex = MAXUINT16;
        uint16 slotIndex = 0;
        bool areEquivalent = true;
        bool usesAuxSlot = false;
        bool isProto = false;
        bool isAccessor = false;
        bool isGetterAccessor = false;
        bool isAccessorOnProto = false;

        bool stress = PHASE_STRESS(Js::EquivObjTypeSpecPhase, topFunctionBody);
        bool areStressEquivalent = stress;

        uint16 typeCount = 0;
        for (uint16 i = 0; (areEquivalent || stress || gatherDataForInlining) && i < polyCacheSize; i++)
        {
            InlineCache& inlineCache = inlineCaches[i];
            if (inlineCache.IsEmpty()) continue;

            if (firstNonEmptyCacheIndex == MAXUINT16)
            {
                if (inlineCache.IsLocal())
                {
                    typeId = TypeWithoutAuxSlotTag(inlineCache.u.local.type)->GetTypeId();
                    usesAuxSlot = TypeHasAuxSlotTag(inlineCache.u.local.type);
                    slotIndex = inlineCache.u.local.slotIndex;
                    // We don't support equivalent object type spec for adding properties.
                    areEquivalent = inlineCache.u.local.typeWithoutProperty == nullptr;
                    gatherDataForInlining = false;
                }
                // Missing properties cannot be treated as equivalent, because for objects with SDTH or DTH, we don't change the object's type
                // when we add a property.  We also don't invalidate proto inline caches (and guards) unless the property being added exists on the proto chain.
                // Missing properties by definition do not exist on the proto chain, so in the end we could have an EquivalentObjTypeSpec cache hit on a
                // property that once was missing, but has since been added. (See OS Bugs 280582).
                else if (inlineCache.IsProto() && !inlineCache.u.proto.isMissing)
                {
                    isProto = true;
                    typeId = TypeWithoutAuxSlotTag(inlineCache.u.proto.type)->GetTypeId();
                    usesAuxSlot = TypeHasAuxSlotTag(inlineCache.u.proto.type);
                    slotIndex = inlineCache.u.proto.slotIndex;
                    prototypeObject = inlineCache.u.proto.prototypeObject;
                }
                else
                {
                    if (!PHASE_OFF(Js::FixAccessorPropsPhase, functionBody))
                    {
                        isAccessor = true;
                        isGetterAccessor = inlineCache.IsGetterAccessor();
                        isAccessorOnProto = inlineCache.u.accessor.isOnProto;
                        accessorOwnerObject = inlineCache.u.accessor.object;
                        typeId = TypeWithoutAuxSlotTag(inlineCache.u.accessor.type)->GetTypeId();
                        usesAuxSlot = TypeHasAuxSlotTag(inlineCache.u.accessor.type);
                        slotIndex = inlineCache.u.accessor.slotIndex;
                    }
                    else
                    {
                        areEquivalent = false;
                        areStressEquivalent = false;
                    }
                    gatherDataForInlining = false;
                }

                // If we're stressing equivalent object type spec then let's keep trying to find a cache that we could use.
                if (!stress || areStressEquivalent)
                {
                    firstNonEmptyCacheIndex = i;
                }
            }
            else
            {
                if (inlineCache.IsLocal())
                {
                    if (isProto || isAccessor || inlineCache.u.local.typeWithoutProperty != nullptr || slotIndex != inlineCache.u.local.slotIndex ||
                        typeId != TypeWithoutAuxSlotTag(inlineCache.u.local.type)->GetTypeId() || usesAuxSlot != TypeHasAuxSlotTag(inlineCache.u.local.type))
                    {
                        areEquivalent = false;
                    }
                    gatherDataForInlining = false;
                }
                else if (inlineCache.IsProto())
                {
                    if (!isProto || isAccessor || prototypeObject != inlineCache.u.proto.prototypeObject || slotIndex != inlineCache.u.proto.slotIndex ||
                        typeId != TypeWithoutAuxSlotTag(inlineCache.u.proto.type)->GetTypeId() || usesAuxSlot != TypeHasAuxSlotTag(inlineCache.u.proto.type))
                    {
                        areEquivalent = false;
                    }
                }
                else
                {
                    // Supporting equivalent obj type spec only for those polymorphic accessor property operations for which
                    // 1. the property is on the same prototype, and
                    // 2. the types are equivalent.
                    //
                    // This is done to keep the equivalence check helper as-is
                    if (!isAccessor || isGetterAccessor != inlineCache.IsGetterAccessor() || !isAccessorOnProto || !inlineCache.u.accessor.isOnProto || accessorOwnerObject != inlineCache.u.accessor.object ||
                        slotIndex != inlineCache.u.accessor.slotIndex || typeId != TypeWithoutAuxSlotTag(inlineCache.u.accessor.type)->GetTypeId() || usesAuxSlot != TypeHasAuxSlotTag(inlineCache.u.accessor.type))
                    {
                        areEquivalent = false;
                    }
                    gatherDataForInlining = false;
                }
            }
            typeCount++;
        }

        if (firstNonEmptyCacheIndex == MAXUINT16)
        {
            IncInlineCacheCount(emptyPolyInlineCacheCount);
            return nullptr;
        }

        if (cache->GetIgnoreForEquivalentObjTypeSpec())
        {
            areEquivalent = areStressEquivalent = false;
        }

        gatherDataForInlining = gatherDataForInlining && (typeCount <= 4); // Only support 4-way (max) polymorphic inlining
        if (!areEquivalent && !areStressEquivalent)
        {
            IncInlineCacheCount(nonEquivPolyInlineCacheCount);
            cache->SetIgnoreForEquivalentObjTypeSpec(true);
            if (!gatherDataForInlining)
            {
                return nullptr;
            }
        }

        Assert(firstNonEmptyCacheIndex < polyCacheSize);
        Assert(typeId != TypeIds_Limit);
        IncInlineCacheCount(equivPolyInlineCacheCount);

        // If we're stressing equivalent object type spec and the types are not equivalent, let's grab the first one only.
        if (stress && (areEquivalent != areStressEquivalent))
        {
            polyCacheSize = firstNonEmptyCacheIndex + 1;
        }

        ScriptContext* scriptContext = functionBody->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        uint16 fixedFunctionCount = 0;

        // Need to create a local array here and not allocate one from the recycler,
        // as the allocation may trigger a GC which can clear the inline caches.
        FixedFieldInfo localFixedFieldInfoArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = { 0 };

        // For polymorphic field loads we only support fixed functions on prototypes. This helps keep the equivalence check helper simple.
        // Since all types in the polymorphic cache share the same prototype, it's enough to grab the fixed function from the prototype object.
        Var fixedProperty = nullptr;
        if ((isProto || isAccessorOnProto) && (areEquivalent || areStressEquivalent))
        {
            const Js::PropertyRecord* propertyRecord = scriptContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(cacheId));
            if (isProto)
            {
                prototypeObject->GetDynamicType()->GetTypeHandler()->TryUseFixedProperty(propertyRecord, &fixedProperty, (Js::FixedPropertyKind)(Js::FixedPropertyKind::FixedMethodProperty | Js::FixedPropertyKind::FixedDataProperty), scriptContext);
            }
            else if (isAccessorOnProto)
            {
                accessorOwnerObject->GetDynamicType()->GetTypeHandler()->TryUseFixedAccessor(propertyRecord, &fixedProperty, Js::FixedPropertyKind::FixedAccessorProperty, isGetterAccessor, scriptContext);
            }

            localFixedFieldInfoArray[0].fieldValue = fixedProperty;
            localFixedFieldInfoArray[0].type = nullptr;
            localFixedFieldInfoArray[0].nextHasSameFixedField = false;

            // TODO (ObjTypeSpec): Enable constructor caches on equivalent polymorphic field loads with fixed functions.
        }

        // Let's get the types.
        Js::Type* localTypes[MaxPolymorphicInlineCacheSize];
        uint16 typeNumber = 0;
        Js::JavascriptFunction* fixedFunctionObject = nullptr;
        for (uint16 i = firstNonEmptyCacheIndex; i < polyCacheSize; i++)
        {
            InlineCache& inlineCache = inlineCaches[i];
            if (inlineCache.IsEmpty()) continue;

            localTypes[typeNumber] = inlineCache.IsLocal() ? TypeWithoutAuxSlotTag(inlineCache.u.local.type) :
                inlineCache.IsProto() ? TypeWithoutAuxSlotTag(inlineCache.u.proto.type) :
                TypeWithoutAuxSlotTag(inlineCache.u.accessor.type);

            if (gatherDataForInlining)
            {
                inlineCache.TryGetFixedMethodFromCache(functionBody, cacheId, &fixedFunctionObject);
                if (!fixedFunctionObject || !fixedFunctionObject->GetFunctionInfo()->HasBody())
                {
                    if (!(areEquivalent || areStressEquivalent))
                    {
                        // If we reach here only because we are gathering data for inlining, and one of the Inline Caches doesn't have a fixedfunction object, return.
                        return nullptr;
                    }
                    else
                    {
                        // If one of the inline caches doesn't have a fixed function object, abort gathering inlining data.
                        gatherDataForInlining = false;
                        typeNumber++;
                        continue;
                    }
                }

                // We got a fixed function object from the cache.

                localFixedFieldInfoArray[typeNumber].type = localTypes[typeNumber];
                localFixedFieldInfoArray[typeNumber].fieldValue = fixedFunctionObject;
                localFixedFieldInfoArray[typeNumber].nextHasSameFixedField = false;
                fixedFunctionCount++;
            }

            typeNumber++;
        }

        if (isAccessor && gatherDataForInlining)
        {
            Assert(fixedFunctionCount <= 1);
        }

        if (stress && (areEquivalent != areStressEquivalent))
        {
            typeCount = 1;
        }

        AnalysisAssert(typeNumber == typeCount);

        // Now that we've copied all material info into local variables, we can start allocating without fear
        // that a garbage collection will clear any of the live inline caches.

        FixedFieldInfo* fixedFieldInfoArray;
        if (gatherDataForInlining)
        {
            fixedFieldInfoArray = RecyclerNewArrayZ(recycler, FixedFieldInfo, fixedFunctionCount);
            memcpy(fixedFieldInfoArray, localFixedFieldInfoArray, fixedFunctionCount * sizeof(FixedFieldInfo));
        }
        else
        {
            fixedFieldInfoArray = RecyclerNewArrayZ(recycler, FixedFieldInfo, 1);
            memcpy(fixedFieldInfoArray, localFixedFieldInfoArray, 1 * sizeof(FixedFieldInfo));
        }

        Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(cacheId);
        Js::PropertyGuard* propertyGuard = entryPoint->RegisterSharedPropertyGuard(propertyId, scriptContext);

        // For polymorphic, non-equivalent objTypeSpecFldInfo's, hasFixedValue is true only if each of the inline caches has a fixed function for the given cacheId, or
        // in the case of an accessor cache, only if the there is only one version of the accessor.
        bool hasFixedValue = gatherDataForInlining ||
            ((isProto || isAccessorOnProto) && (areEquivalent || areStressEquivalent) && localFixedFieldInfoArray[0].fieldValue);

        bool doesntHaveEquivalence = !(areEquivalent || areStressEquivalent);

        EquivalentTypeSet* typeSet = nullptr;
        auto jitTransferData = entryPoint->GetJitTransferData();
        Assert(jitTransferData != nullptr);
        if (areEquivalent || areStressEquivalent)
        {
            for (uint16 i = 0; i < typeCount; i++)
            {
                jitTransferData->AddJitTimeTypeRef(localTypes[i], recycler);
                if (hasFixedValue)
                {
                    // Fixed field checks allow us to assume a specific type ID, but the assumption is only
                    // valid if we lock the type. Otherwise, the type ID may change out from under us without
                    // evolving the type.
                    if (DynamicType::Is(localTypes[i]->GetTypeId()))
                    {
                        DynamicType *dynamicType = static_cast<DynamicType*>(localTypes[i]);
                        if (!dynamicType->GetIsLocked())
                        {
                            dynamicType->LockType();
                        }
                    }
                }
            }

            Js::Type** types = RecyclerNewArray(recycler, Js::Type*, typeCount);
            memcpy(types, localTypes, typeCount * sizeof(Js::Type*));
            typeSet = RecyclerNew(recycler, EquivalentTypeSet, types, typeCount);
        }

        ObjTypeSpecFldInfo* info = RecyclerNew(recycler, ObjTypeSpecFldInfo,
            id, typeId, nullptr, typeSet, usesAuxSlot, isProto, isAccessor, hasFixedValue, hasFixedValue, doesntHaveEquivalence, true, slotIndex, propertyId,
            prototypeObject, propertyGuard, nullptr, fixedFieldInfoArray, fixedFunctionCount/*, nullptr, nullptr, nullptr*/);

        if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
        {
            if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
            {
                if (typeSet)
                {
                    const PropertyRecord* propertyRecord = scriptContext->GetPropertyName(propertyId);
                    Output::Print(L"Created ObjTypeSpecFldInfo: id %u, property %s(#%u), slot %u, type set: ",
                        id, propertyRecord->GetBuffer(), propertyId, slotIndex);
                    for (uint16 ti = 0; ti < typeCount - 1; ti++)
                    {
                        Output::Print(L"0x%p, ", typeSet->GetType(ti));
                    }
                    Output::Print(L"0x%p\n", typeSet->GetType(typeCount - 1));
                    Output::Flush();
                }
            }
        }

        return info;

#undef IncInlineCacheCount
    }

    Js::JavascriptFunction* ObjTypeSpecFldInfo::GetFieldValueAsFixedFunction() const
    {
        Assert(HasFixedValue());
        Assert(IsMono() || (IsPoly() && !DoesntHaveEquivalence()));
        Assert(this->fixedFieldInfoArray[0].fieldValue != nullptr && Js::JavascriptFunction::Is(this->fixedFieldInfoArray[0].fieldValue));

        return Js::JavascriptFunction::FromVar(this->fixedFieldInfoArray[0].fieldValue);
    }

    Js::JavascriptFunction* ObjTypeSpecFldInfo::GetFieldValueAsFixedFunction(uint i) const
    {
        Assert(HasFixedValue());
        Assert(IsPoly());
        Assert(this->fixedFieldInfoArray[i].fieldValue != nullptr && Js::JavascriptFunction::Is(this->fixedFieldInfoArray[i].fieldValue));

        return Js::JavascriptFunction::FromVar(this->fixedFieldInfoArray[i].fieldValue);
    }

    Js::JavascriptFunction* ObjTypeSpecFldInfo::GetFieldValueAsFunction() const
    {
        Assert(IsMono() || (IsPoly() && !DoesntHaveEquivalence()));
        Assert(this->fixedFieldInfoArray[0].fieldValue != nullptr && JavascriptFunction::Is(this->fixedFieldInfoArray[0].fieldValue));

        return JavascriptFunction::FromVar(this->fixedFieldInfoArray[0].fieldValue);
    }

    Js::JavascriptFunction* ObjTypeSpecFldInfo::GetFieldValueAsFunctionIfAvailable() const
    {
        Assert(IsMono() || (IsPoly() && !DoesntHaveEquivalence()));
        return this->fixedFieldInfoArray[0].fieldValue != nullptr && JavascriptFunction::Is(this->fixedFieldInfoArray[0].fieldValue) ?
            JavascriptFunction::FromVar(this->fixedFieldInfoArray[0].fieldValue) : nullptr;
    }

    Js::JavascriptFunction* ObjTypeSpecFldInfo::GetFieldValueAsFixedFunctionIfAvailable() const
    {
        Assert(HasFixedValue());
        Assert(IsMono() || (IsPoly() && !DoesntHaveEquivalence()));
        return GetFieldValueAsFunctionIfAvailable();
    }

    Js::JavascriptFunction* ObjTypeSpecFldInfo::GetFieldValueAsFixedFunctionIfAvailable(uint i) const
    {
        Assert(HasFixedValue());
        Assert(IsPoly());
        return this->fixedFieldInfoArray[i].fieldValue != nullptr && JavascriptFunction::Is(this->fixedFieldInfoArray[i].fieldValue) ?
            JavascriptFunction::FromVar(this->fixedFieldInfoArray[i].fieldValue) : nullptr;
    }

    Js::Var ObjTypeSpecFldInfo::GetFieldValueAsFixedDataIfAvailable() const
    {
        Assert(HasFixedValue() && this->fixedFieldCount == 1);
        return this->fixedFieldInfoArray[0].fieldValue;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    const wchar_t* ObjTypeSpecFldInfo::GetCacheLayoutString() const
    {
        return IsLoadedFromProto() ? L"proto" : UsesAccessor() ? L"flags" : L"local";
    }
#endif

    ObjTypeSpecFldInfoArray::ObjTypeSpecFldInfoArray()
        : infoArray(nullptr)
#if DBG
        , infoCount(0)
#endif
    {
    }

    void ObjTypeSpecFldInfoArray::EnsureArray(Recycler *const recycler, FunctionBody *const functionBody)
    {
        Assert(recycler != nullptr);
        Assert(functionBody != nullptr);
        Assert(functionBody->GetInlineCacheCount() != 0);

        if (this->infoArray)
        {
            Assert(functionBody->GetInlineCacheCount() == this->infoCount);
            return;
        }

        this->infoArray = RecyclerNewArrayZ(recycler, ObjTypeSpecFldInfo*, functionBody->GetInlineCacheCount());
#if DBG
        this->infoCount = functionBody->GetInlineCacheCount();
#endif
    }

    ObjTypeSpecFldInfo* ObjTypeSpecFldInfoArray::GetInfo(FunctionBody *const functionBody, const uint index) const
    {
        Assert(functionBody);
        Assert(this->infoArray == nullptr || functionBody->GetInlineCacheCount() == this->infoCount);
        Assert(index < functionBody->GetInlineCacheCount());
        return this->infoArray ? this->infoArray[index] : nullptr;
    }

    void ObjTypeSpecFldInfoArray::SetInfo(Recycler *const recycler, FunctionBody *const functionBody,
        const uint index, ObjTypeSpecFldInfo* info)
    {
        Assert(recycler);
        Assert(functionBody);
        Assert(this->infoArray == nullptr || functionBody->GetInlineCacheCount() == this->infoCount);
        Assert(index < functionBody->GetInlineCacheCount());
        Assert(info);

        EnsureArray(recycler, functionBody);
        this->infoArray[index] = info;
    }

    void ObjTypeSpecFldInfoArray::Reset()
    {
        this->infoArray = nullptr;
#if DBG
        this->infoCount = 0;
#endif
    }

    FunctionCodeGenJitTimeData::FunctionCodeGenJitTimeData(FunctionInfo *const functionInfo, EntryPointInfo *const entryPoint, bool isInlined) :
        functionInfo(functionInfo), entryPointInfo(entryPoint), globalObjTypeSpecFldInfoCount(0), globalObjTypeSpecFldInfoArray(nullptr),
        weakFuncRef(nullptr), inlinees(nullptr), inlineeCount(0), ldFldInlineeCount(0), isInlined(isInlined), isAggressiveInliningEnabled(false),
#ifdef FIELD_ACCESS_STATS
        inlineCacheStats(nullptr),
#endif
        profiledIterations(GetFunctionBody() ? GetFunctionBody()->GetProfiledIterations() : 0),
        next(0)
    {
    }

    FunctionInfo *FunctionCodeGenJitTimeData::GetFunctionInfo() const
    {
        return this->functionInfo;
    }

    FunctionBody *FunctionCodeGenJitTimeData::GetFunctionBody() const
    {
        return this->functionInfo->GetFunctionBody();
    }

    bool FunctionCodeGenJitTimeData::IsPolymorphicCallSite(const ProfileId profiledCallSiteId) const
    {
        Assert(GetFunctionBody());
        Assert(profiledCallSiteId < GetFunctionBody()->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId]->next != nullptr : false;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetInlinee(const ProfileId profiledCallSiteId) const
    {
        Assert(GetFunctionBody());
        Assert(profiledCallSiteId < GetFunctionBody()->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId] : nullptr;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetJitTimeDataFromFunctionInfo(FunctionInfo *polyFunctionInfo) const
    {
        const FunctionCodeGenJitTimeData *next = this;
        while (next && next->functionInfo != polyFunctionInfo)
        {
            next = next->next;
        }
        return next;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetLdFldInlinee(const InlineCacheIndex inlineCacheIndex) const
    {
        Assert(GetFunctionBody());
        Assert(inlineCacheIndex < GetFunctionBody()->GetInlineCacheCount());

        return ldFldInlinees ? ldFldInlinees[inlineCacheIndex] : nullptr;
    }

    FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::AddInlinee(
        Recycler *const recycler,
        const ProfileId profiledCallSiteId,
        FunctionInfo *const inlinee,
        bool isInlined)
    {
        Assert(recycler);
        const auto functionBody = GetFunctionBody();
        Assert(functionBody);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());
        Assert(inlinee);

        if (!inlinees)
        {
            inlinees = RecyclerNewArrayZ(recycler, FunctionCodeGenJitTimeData *, functionBody->GetProfiledCallSiteCount());
        }

        FunctionCodeGenJitTimeData *inlineeData = nullptr;
        if (!inlinees[profiledCallSiteId])
        {
            inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr /* entryPoint */, isInlined);
            inlinees[profiledCallSiteId] = inlineeData;
            if (++inlineeCount == 0)
            {
                Js::Throw::OutOfMemory();
            }
        }
        else
        {
            inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr /* entryPoint */, isInlined);
            // This is polymorphic, chain the data.
            inlineeData->next = inlinees[profiledCallSiteId];
            inlinees[profiledCallSiteId] = inlineeData;
        }
        return inlineeData;
    }

    FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::AddLdFldInlinee(
        Recycler *const recycler,
        const InlineCacheIndex inlineCacheIndex,
        FunctionInfo *const inlinee)
    {
        Assert(recycler);
        const auto functionBody = GetFunctionBody();
        Assert(functionBody);
        Assert(inlineCacheIndex < GetFunctionBody()->GetInlineCacheCount());
        Assert(inlinee);

        if (!ldFldInlinees)
        {
            ldFldInlinees = RecyclerNewArrayZ(recycler, FunctionCodeGenJitTimeData *, GetFunctionBody()->GetInlineCacheCount());
        }

        const auto inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr);
        Assert(!ldFldInlinees[inlineCacheIndex]);
        ldFldInlinees[inlineCacheIndex] = inlineeData;
        if (++ldFldInlineeCount == 0)
        {
            Js::Throw::OutOfMemory();
        }
        return inlineeData;
    }

    uint FunctionCodeGenJitTimeData::InlineeCount() const
    {
        return inlineeCount;
    }

#ifdef FIELD_ACCESS_STATS
    void FunctionCodeGenJitTimeData::EnsureInlineCacheStats(Recycler* recycler)
    {
        this->inlineCacheStats = RecyclerNew(recycler, FieldAccessStats);
    }

    void FunctionCodeGenJitTimeData::AddInlineeInlineCacheStats(FunctionCodeGenJitTimeData* inlineeJitTimeData)
    {
        Assert(this->inlineCacheStats != nullptr);
        Assert(inlineeJitTimeData != nullptr && inlineeJitTimeData->inlineCacheStats != nullptr);
        this->inlineCacheStats->Add(inlineeJitTimeData->inlineCacheStats);
    }
#endif

    uint16 FunctionCodeGenJitTimeData::GetProfiledIterations() const
    {
        return profiledIterations;
    }
}
#endif
