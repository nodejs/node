//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template<
        bool CheckLocal,
        bool CheckProto,
        bool CheckAccessor,
        bool CheckMissing,
        bool CheckPolymorphicInlineCache,
        bool CheckTypePropertyCache,
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable,
        bool ReturnOperationInfo>
    __inline bool CacheOperators::TryGetProperty(
        Var const instance,
        const bool isRoot,
        RecyclableObject *const object,
        const PropertyId propertyId,
        Var *const propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo * operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(!CheckTypePropertyCache || !isRoot);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(IsPolymorphicInlineCacheAvailable == !!propertyValueInfo->GetPolymorphicInlineCache());
        Assert(!ReturnOperationInfo || operationInfo);

        if(CheckLocal || CheckProto || CheckAccessor)
        {
            InlineCache *const inlineCache = IsInlineCacheAvailable ? propertyValueInfo->GetInlineCache() : nullptr;
            if(IsInlineCacheAvailable)
            {
                if (inlineCache->TryGetProperty<CheckLocal, CheckProto, CheckAccessor, CheckMissing, ReturnOperationInfo>(
                        instance,
                        object,
                        propertyId,
                        propertyValue,
                        requestContext,
                        operationInfo))
                {
                    return true;
                }
                if(ReturnOperationInfo)
                {
                    operationInfo->isPolymorphic = inlineCache->HasDifferentType(object->GetType());
                }
            }
            else if(ReturnOperationInfo)
            {
                operationInfo->isPolymorphic = true;
            }

            if(CheckPolymorphicInlineCache)
            {
                Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
                PolymorphicInlineCache *const polymorphicInlineCache =
                    IsPolymorphicInlineCacheAvailable
                        ?   propertyValueInfo->GetPolymorphicInlineCache()
                        :   propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(
                                propertyValueInfo->GetInlineCacheIndex());
                if ((IsPolymorphicInlineCacheAvailable || polymorphicInlineCache) &&
                    polymorphicInlineCache->TryGetProperty<
                            CheckLocal,
                            CheckProto,
                            CheckAccessor,
                            CheckMissing,
                            IsInlineCacheAvailable,
                            ReturnOperationInfo
                        >(
                            instance,
                            object,
                            propertyId,
                            propertyValue,
                            requestContext,
                            operationInfo,
                            inlineCache
                        ))
                {
                    return true;
                }
            }
        }

        if(!CheckTypePropertyCache)
        {
            return false;
        }

        TypePropertyCache *const typePropertyCache = object->GetType()->GetPropertyCache();
        if(!typePropertyCache ||
            !typePropertyCache->TryGetProperty(
                    CheckMissing,
                    object,
                    propertyId,
                    propertyValue,
                    requestContext,
                    ReturnOperationInfo ? operationInfo : nullptr,
                    propertyValueInfo))
        {
            return false;
        }

        if(!ReturnOperationInfo || operationInfo->cacheType == CacheType_TypeProperty)
        {
            return true;
        }

        // The property access was cached in an inline cache. Get the proper property operation info.
        PretendTryGetProperty<IsInlineCacheAvailable, IsPolymorphicInlineCacheAvailable>(
            object->GetType(),
            operationInfo,
            propertyValueInfo);
        return true;
    }

    template<
        bool CheckLocal,
        bool CheckLocalTypeWithoutProperty,
        bool CheckAccessor,
        bool CheckPolymorphicInlineCache,
        bool CheckTypePropertyCache,
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable,
        bool ReturnOperationInfo>
    __inline bool CacheOperators::TrySetProperty(
        RecyclableObject *const object,
        const bool isRoot,
        const PropertyId propertyId,
        Var propertyValue,
        ScriptContext *const requestContext,
        const PropertyOperationFlags propertyOperationFlags,
        PropertyCacheOperationInfo * operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(!CheckTypePropertyCache || !isRoot);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(IsPolymorphicInlineCacheAvailable == !!propertyValueInfo->GetPolymorphicInlineCache());
        Assert(!ReturnOperationInfo || operationInfo);

        if(CheckLocal || CheckLocalTypeWithoutProperty || CheckAccessor)
        {
            InlineCache *const inlineCache = IsInlineCacheAvailable ? propertyValueInfo->GetInlineCache() : nullptr;
            if(IsInlineCacheAvailable)
            {
                if (inlineCache->TrySetProperty<CheckLocal, CheckLocalTypeWithoutProperty, CheckAccessor, ReturnOperationInfo>(
                        object,
                        propertyId,
                        propertyValue,
                        requestContext,
                        operationInfo,
                        propertyOperationFlags))
                {
                    return true;
                }
                if(ReturnOperationInfo)
                {
                    operationInfo->isPolymorphic = inlineCache->HasDifferentType(object->GetType());
                }
            }
            else if(ReturnOperationInfo)
            {
                operationInfo->isPolymorphic = true;
            }

            if(CheckPolymorphicInlineCache)
            {
                Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
                PolymorphicInlineCache *const polymorphicInlineCache =
                    IsPolymorphicInlineCacheAvailable
                        ?   propertyValueInfo->GetPolymorphicInlineCache()
                        :   propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(
                                propertyValueInfo->GetInlineCacheIndex());
                if ((IsPolymorphicInlineCacheAvailable || polymorphicInlineCache) &&
                    polymorphicInlineCache->TrySetProperty<
                            CheckLocal,
                            CheckLocalTypeWithoutProperty,
                            CheckAccessor,
                            IsInlineCacheAvailable,
                            ReturnOperationInfo
                        >(
                            object,
                            propertyId,
                            propertyValue,
                            requestContext,
                            operationInfo,
                            inlineCache,
                            propertyOperationFlags
                        ))
                {
                    return true;
                }
            }
        }

        if(!CheckTypePropertyCache)
        {
            return false;
        }

        TypePropertyCache *const typePropertyCache = object->GetType()->GetPropertyCache();
        if(!typePropertyCache ||
            !typePropertyCache->TrySetProperty(
                object,
                propertyId,
                propertyValue,
                requestContext,
                ReturnOperationInfo ? operationInfo : nullptr,
                propertyValueInfo))
        {
            return false;
        }

        if(!ReturnOperationInfo || operationInfo->cacheType == CacheType_TypeProperty)
        {
            return true;
        }

        // The property access was cached in an inline cache. Get the proper property operation info.
        PretendTrySetProperty<IsInlineCacheAvailable, IsPolymorphicInlineCacheAvailable>(
            object->GetType(),
            object->GetType(),
            operationInfo,
            propertyValueInfo);
        return true;
    }

    template<
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable>
    __inline void CacheOperators::PretendTryGetProperty(
        Type *const type,
        PropertyCacheOperationInfo *operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(!IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetPolymorphicInlineCache());
        Assert(operationInfo);

        if (IsInlineCacheAvailable && propertyValueInfo->GetInlineCache()->PretendTryGetProperty(type, operationInfo))
        {
            return;
        }

        Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
        PolymorphicInlineCache *const polymorphicInlineCache =
            IsPolymorphicInlineCacheAvailable
                ? propertyValueInfo->GetPolymorphicInlineCache()
                : propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(propertyValueInfo->GetInlineCacheIndex());
        if (IsPolymorphicInlineCacheAvailable || polymorphicInlineCache)
        {
            polymorphicInlineCache->PretendTryGetProperty(type, operationInfo);
        }
    }

    template<
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable>
    __inline void CacheOperators::PretendTrySetProperty(
        Type *const type,
        Type *const oldType,
        PropertyCacheOperationInfo * operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(!IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetPolymorphicInlineCache());
        Assert(operationInfo);

        if (IsInlineCacheAvailable && propertyValueInfo->GetInlineCache()->PretendTrySetProperty(type, oldType, operationInfo))
        {
            return;
        }

        Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
        PolymorphicInlineCache *const polymorphicInlineCache =
            IsPolymorphicInlineCacheAvailable
                ? propertyValueInfo->GetPolymorphicInlineCache()
                : propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(propertyValueInfo->GetInlineCacheIndex());
        if (IsPolymorphicInlineCacheAvailable || polymorphicInlineCache)
        {
            polymorphicInlineCache->PretendTrySetProperty(type, oldType, operationInfo);
        }
    }

    template<
        bool IsAccessor,
        bool IsRead,
        bool IncludeTypePropertyCache>
    __inline void CacheOperators::Cache(
        const bool isProto,
        DynamicObject *const objectWithProperty,
        const bool isRoot,
        Type *const type,
        Type *const typeWithoutProperty,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        const bool isMissing,
        const int requiredAuxSlotCapacity,
        const PropertyValueInfo *const info,
        ScriptContext *const requestContext)
    {
        CompileAssert(!IsAccessor || !IncludeTypePropertyCache);
        Assert(info);
        Assert(objectWithProperty);

        if(!IsAccessor)
        {
            if(!isProto)
            {
                Assert(type == objectWithProperty->GetType());
            }
            else
            {
                Assert(IsRead);
                Assert(type != objectWithProperty->GetType());
            }
        }
        else
        {
            Assert(!isRoot); // could still be root object, but the parameter will be false and shouldn't be used for accessors
            Assert(!typeWithoutProperty);
            Assert(requiredAuxSlotCapacity == 0);
        }

        if(IsRead)
        {
            Assert(!typeWithoutProperty);
            Assert(requiredAuxSlotCapacity == 0);
            Assert(CanCachePropertyRead(objectWithProperty, requestContext));

            if(!IsAccessor && isProto && PropertyValueInfo::PrototypeCacheDisabled(info))
            {
                return;
            }
        }
        else
        {
            Assert(CanCachePropertyWrite(objectWithProperty, requestContext));

            // TODO(ianhall): the following assert would let global const properties slip through when they shadow
            // a global property. Reason being DictionaryTypeHandler::IsWritable cannot tell if it should check
            // the global property or the global let/const.  Fix this by updating IsWritable to recognize isRoot.

            // Built-in Function.prototype properties 'length', 'arguments', and 'caller' are special cases.
            Assert(
                objectWithProperty->IsWritable(propertyId) ||
                (isRoot && RootObjectBase::FromVar(objectWithProperty)->IsLetConstGlobal(propertyId)) ||
                JavascriptFunction::IsBuiltinProperty(objectWithProperty, propertyId));
        }

        const bool includeTypePropertyCache = IncludeTypePropertyCache && !isRoot;
        bool createTypePropertyCache = false;
        PolymorphicInlineCache *polymorphicInlineCache = info->GetPolymorphicInlineCache();
        if(!polymorphicInlineCache && info->GetFunctionBody())
        {
            polymorphicInlineCache = info->GetFunctionBody()->GetPolymorphicInlineCache(info->GetInlineCacheIndex());
        }
        InlineCache *const inlineCache = info->GetInlineCache();
        if(inlineCache)
        {
            const bool tryCreatePolymorphicInlineCache = !polymorphicInlineCache && info->GetFunctionBody();
            if((includeTypePropertyCache || tryCreatePolymorphicInlineCache) &&
                inlineCache->HasDifferentType<IsAccessor>(isProto, type, typeWithoutProperty))
            {
                if(tryCreatePolymorphicInlineCache)
                {
                    polymorphicInlineCache =
                        info->GetFunctionBody()->CreateNewPolymorphicInlineCache(
                            info->GetInlineCacheIndex(),
                            propertyId,
                            inlineCache);
                }
                if(includeTypePropertyCache)
                {
                    createTypePropertyCache = true;
                }
            }

            if(!IsAccessor)
            {
                if(!isProto)
                {
                    inlineCache->CacheLocal(
                        type,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        typeWithoutProperty,
                        requiredAuxSlotCapacity,
                        requestContext);
                }
                else
                {
                    inlineCache->CacheProto(
                        objectWithProperty,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        isMissing,
                        type,
                        requestContext);
                }
            }
            else
            {
                inlineCache->CacheAccessor(
                    IsRead,
                    propertyId,
                    propertyIndex,
                    isInlineSlot,
                    type,
                    objectWithProperty,
                    isProto,
                    requestContext);
            }
        }

        if(polymorphicInlineCache)
        {
            // Don't resize a polymorphic inline cache from full JIT because it currently doesn't rejit to use the new
            // polymorphic inline cache. Once resized, bailouts would populate only the new set of caches and full JIT would
            // continue to use to old set of caches.
            Assert(!info->AllowResizingPolymorphicInlineCache() || info->GetFunctionBody());
            if((includeTypePropertyCache && !createTypePropertyCache || info->AllowResizingPolymorphicInlineCache()) &&
                polymorphicInlineCache->HasDifferentType<IsAccessor>(isProto, type, typeWithoutProperty))
            {
                if(info->AllowResizingPolymorphicInlineCache() && polymorphicInlineCache->CanAllocateBigger())
                {
                    polymorphicInlineCache =
                        info->GetFunctionBody()->CreateBiggerPolymorphicInlineCache(
                            info->GetInlineCacheIndex(),
                            propertyId);
                }
                if(includeTypePropertyCache)
                {
                    createTypePropertyCache = true;
                }
            }

            if(!IsAccessor)
            {
                if(!isProto)
                {
                    polymorphicInlineCache->CacheLocal(
                        type,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        typeWithoutProperty,
                        requiredAuxSlotCapacity,
                        requestContext);
                }
                else
                {
                    polymorphicInlineCache->CacheProto(
                        objectWithProperty,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        isMissing,
                        type,
                        requestContext);
                }
            }
            else
            {
                polymorphicInlineCache->CacheAccessor(
                    IsRead,
                    propertyId,
                    propertyIndex,
                    isInlineSlot,
                    type,
                    objectWithProperty,
                    isProto,
                    requestContext);
            }
        }

        if(!includeTypePropertyCache)
        {
            return;
        }
        Assert(!IsAccessor);

        TypePropertyCache *typePropertyCache = type->GetPropertyCache();
        if(!typePropertyCache)
        {
            if(!createTypePropertyCache)
            {
                return;
            }
            typePropertyCache = type->CreatePropertyCache();
        }

        if(isProto)
        {
            typePropertyCache->Cache(
                propertyId,
                propertyIndex,
                isInlineSlot,
                info->IsWritable() && info->IsStoreFieldCacheEnabled(),
                isMissing,
                objectWithProperty,
                type);

            typePropertyCache = objectWithProperty->GetType()->GetPropertyCache();
            if(!typePropertyCache)
            {
                return;
            }
        }

        typePropertyCache->Cache(
            propertyId,
            propertyIndex,
            isInlineSlot,
            info->IsWritable() && info->IsStoreFieldCacheEnabled());
    }
}
