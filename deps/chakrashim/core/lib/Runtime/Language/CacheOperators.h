//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class CacheOperators
    {
    public:
        static void CachePropertyRead(Var startingObject, RecyclableObject * objectWithProperty, const bool isRoot, PropertyId propertyId, const bool isMissing, PropertyValueInfo* info, ScriptContext * requestContext);
        static void CachePropertyReadForGetter(PropertyValueInfo *info, Var originalInstance, JsUtil::CharacterBuffer<WCHAR> const& propertyName, ScriptContext* requestContext);
        static void CachePropertyReadForGetter(PropertyValueInfo *info, Var originalInstance, PropertyId propertyId, ScriptContext* requestContext);
        static void CachePropertyWrite(RecyclableObject * object, const bool isRoot, Type* typeWithoutProperty, PropertyId propertyId, PropertyValueInfo* info, ScriptContext * requestContext);

        template<
            bool IsAccessor,
            bool IsRead,
            bool IncludeTypePropertyCache>
        static void Cache(const bool isProto, DynamicObject *const objectWithProperty, const bool isRoot, Type *const type, Type *const typeWithoutProperty, const PropertyId propertyId, const PropertyIndex propertyIndex, const bool isInlineSlot, const bool isMissing, const int requiredAuxSlotCapacity, const PropertyValueInfo *const info, ScriptContext *const requestContext);

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
        static bool TryGetProperty(Var const instance, const bool isRoot, RecyclableObject *const object, const PropertyId propertyId, Var *const propertyValue, ScriptContext *const requestContext, PropertyCacheOperationInfo * operationInfo, PropertyValueInfo *const propertyValueInfo);
        template<
            bool CheckLocal,
            bool CheckLocalTypeWithoutProperty,
            bool CheckAccessor,
            bool CheckPolymorphicInlineCache,
            bool CheckTypePropertyCache,
            bool IsInlineCacheAvailable,
            bool IsPolymorphicInlineCacheAvailable,
            bool ReturnOperationInfo>
        static bool TrySetProperty(RecyclableObject *const object, const bool isRoot, const PropertyId propertyId, Var propertyValue, ScriptContext *const requestContext, const PropertyOperationFlags propertyOperationFlags, PropertyCacheOperationInfo * operationInfo, PropertyValueInfo *const propertyValueInfo);

        template<
            bool IsInlineCacheAvailable,
            bool IsPolymorphicInlineCacheAvailable>
        static void PretendTryGetProperty(Type *const type, PropertyCacheOperationInfo * operationInfo, PropertyValueInfo *const propertyValueInfo);
        template<
            bool IsInlineCacheAvailable,
            bool IsPolymorphicInlineCacheAvailable>
        static void PretendTrySetProperty(Type *const type, Type *const oldType, PropertyCacheOperationInfo * operationInfo, PropertyValueInfo *const propertyValueInfo);

#if DBG_DUMP
        static void TraceCache(InlineCache * inlineCache, const wchar_t * methodName, PropertyId propertyId, ScriptContext * requestContext, RecyclableObject * object);
        static void TraceCache(PolymorphicInlineCache * polymorphicInlineCache, const wchar_t * methodName, PropertyId propertyId, ScriptContext * requestContext, RecyclableObject * object);
#endif

    private:
        static bool CanCachePropertyRead(const PropertyValueInfo *info, RecyclableObject * object, ScriptContext * requestContext);
        static bool CanCachePropertyRead(RecyclableObject * object, ScriptContext * requestContext);
        static bool CanCachePropertyWrite(const PropertyValueInfo *info, RecyclableObject * object, ScriptContext * requestContext);
        static bool CanCachePropertyWrite(RecyclableObject * object, ScriptContext * requestContext);

#if DBG_DUMP
        static void TraceCacheCommon(const wchar_t * methodName, PropertyId propertyId, ScriptContext * requestContext, RecyclableObject * object);
#endif
    };
}
