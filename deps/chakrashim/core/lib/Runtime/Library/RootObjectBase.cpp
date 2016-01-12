//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    RootObjectInlineCache::RootObjectInlineCache(InlineCacheAllocator * allocator):
        inlineCache(nullptr), refCount(1)
    {
        inlineCache = AllocatorNewZ(InlineCacheAllocator,
            allocator, Js::InlineCache);
    }

    RootObjectBase::RootObjectBase(DynamicType * type) :
        DynamicObject(type), hostObject(nullptr), loadInlineCacheMap(nullptr), loadMethodInlineCacheMap(nullptr), storeInlineCacheMap(nullptr)
    {}

    RootObjectBase::RootObjectBase(DynamicType * type, ScriptContext* scriptContext) :
        DynamicObject(type, scriptContext), hostObject(nullptr), loadInlineCacheMap(nullptr), loadMethodInlineCacheMap(nullptr), storeInlineCacheMap(nullptr)
    {}

    bool RootObjectBase::Is(Var var)
    {
        return RecyclableObject::Is(var) && RootObjectBase::Is(RecyclableObject::FromVar(var));
    }

    bool RootObjectBase::Is(RecyclableObject* obj)
    {
        TypeId id = obj->GetTypeId();
        return id == TypeIds_GlobalObject || id == TypeIds_ModuleRoot;
    }

    RootObjectBase * RootObjectBase::FromVar(Var var)
    {
        Assert(RootObjectBase::Is(var));
        return static_cast<Js::RootObjectBase *>(var);
    }

    HostObjectBase * RootObjectBase::GetHostObject() const
    {
        Assert(hostObject == nullptr || Js::JavascriptOperators::GetTypeId(hostObject) == TypeIds_HostObject);

        return this->hostObject;
    }

    void RootObjectBase::SetHostObject(HostObjectBase * hostObject)
    {
        Assert(hostObject == nullptr || Js::JavascriptOperators::GetTypeId(hostObject) == TypeIds_HostObject);
        this->hostObject = hostObject;
    }

    Js::InlineCache *
    RootObjectBase::GetInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore)
    {
        Js::RootObjectInlineCache * rootObjectInlineCache = GetRootInlineCache(propertyRecord, isLoadMethod, isStore);
        return rootObjectInlineCache->GetInlineCache();
    }

    Js::RootObjectInlineCache*
    RootObjectBase::GetRootInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore)
    {
        RootObjectInlineCacheMap * inlineCacheMap = isStore ? storeInlineCacheMap :
            isLoadMethod ? loadMethodInlineCacheMap : loadInlineCacheMap;
        Js::RootObjectInlineCache * rootObjectInlineCache;
        if (inlineCacheMap == nullptr)
        {
            Recycler * recycler = this->GetLibrary()->GetRecycler();
            inlineCacheMap = RecyclerNew(recycler, RootObjectInlineCacheMap, recycler);
            if (isStore)
            {
                this->storeInlineCacheMap = inlineCacheMap;
            }
            else if (isLoadMethod)
            {
                this->loadMethodInlineCacheMap = inlineCacheMap;
            }
            else
            {
                this->loadInlineCacheMap = inlineCacheMap;
            }
        }
        else if (inlineCacheMap->TryGetValue(propertyRecord, &rootObjectInlineCache))
        {
            rootObjectInlineCache->AddRef();
            return rootObjectInlineCache;
        }

        Recycler * recycler = this->GetLibrary()->GetRecycler();
        rootObjectInlineCache = RecyclerNewLeaf(recycler, RootObjectInlineCache, this->GetScriptContext()->GetInlineCacheAllocator());
        inlineCacheMap->Add(propertyRecord, rootObjectInlineCache);

        return rootObjectInlineCache;
    }

    // TODO: Switch to take PropertyRecord instead once we clean up the function body to hold onto propertyRecord
    // instead of propertyId.
    void
    RootObjectBase::ReleaseInlineCache(Js::PropertyId propertyId, bool isLoadMethod, bool isStore, bool isShutdown)
    {
        RootObjectInlineCacheMap * inlineCacheMap = isStore ? storeInlineCacheMap :
            isLoadMethod ? loadMethodInlineCacheMap : loadInlineCacheMap;
        bool found = false;
        inlineCacheMap->RemoveIfWithKey(propertyId,
            [this, isShutdown, &found](PropertyRecord const * propertyRecord, RootObjectInlineCache * rootObjectInlineCache)
            {
                found = true;
                if (rootObjectInlineCache->Release() == 0)
                {
                    // If we're not shutting down, we need to remove this cache from thread context's invalidation list (if any),
                    // and release memory back to the arena.  During script context shutdown, we leave everything in place, because
                    // the inline cache arena will stay alive until script context is destroyed (as in destructor called as opposed to
                    // Close called) and thus the invalidation lists are safe to keep references to caches from this script context.
                    if (!isShutdown)
                    {
                        rootObjectInlineCache->GetInlineCache()->RemoveFromInvalidationList();
                        AllocatorDelete(InlineCacheAllocator, this->GetScriptContext()->GetInlineCacheAllocator(), rootObjectInlineCache->GetInlineCache());
                    }
                    return true; // Remove from the map
                }
                return false; // don't remove from the map
            }
        );
        Assert(found);
    }

    BOOL
    RootObjectBase::EnsureProperty(PropertyId propertyId)
    {
        if (!RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId))
        {
            this->InitProperty(propertyId, this->GetLibrary()->GetUndefined(),
                static_cast<Js::PropertyOperationFlags>(PropertyOperation_PreInit | PropertyOperation_SpecialValue));
        }
        return true;
    }

    BOOL
    RootObjectBase::EnsureNoRedeclProperty(PropertyId propertyId)
    {
        RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId);
        return true;
    }

    BOOL
    RootObjectBase::HasOwnPropertyCheckNoRedecl(PropertyId propertyId)
    {
        bool noRedecl = false;
        if (!GetTypeHandler()->HasRootProperty(this, propertyId, &noRedecl))
        {
            return FALSE;
        }
        else if (noRedecl)
        {
            JavascriptError::ThrowReferenceError(GetScriptContext(), ERRRedeclaration);
        }
        return true;
    }


    BOOL
    RootObjectBase::HasRootProperty(PropertyId propertyId)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->HasRootProperty(this, propertyId);
    }

    BOOL
    RootObjectBase::GetRootProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL
    RootObjectBase::GetRootPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL
    RootObjectBase::SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->SetRootProperty(this, propertyId, value, flags, info);
    }

    DescriptorFlags
    RootObjectBase::GetRootSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootSetter(this, propertyId, setterValue, info, requestContext);
    }

    BOOL
    RootObjectBase::DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->DeleteRootProperty(this, propertyId, flags);
    }

    PropertyIndex
    RootObjectBase::GetRootPropertyIndex(PropertyId propertyId)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        Assert(propertyId != Constants::NoProperty);
        return GetTypeHandler()->GetRootPropertyIndex(this->GetScriptContext()->GetPropertyName(propertyId));
    }

    void
    RootObjectBase::EnsureNoProperty(PropertyId propertyId)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        bool isDeclared = false;
        if (GetTypeHandler()->HasRootProperty(this, propertyId, nullptr, &isDeclared) && isDeclared)
        {
            JavascriptError::ThrowReferenceError(this->GetScriptContext(), ERRRedeclaration);
        }
    }

#if DBG
    bool
    RootObjectBase::IsLetConstGlobal(PropertyId propertyId)
    {
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->IsLetConstGlobal(this, propertyId);
    }
#endif
}
