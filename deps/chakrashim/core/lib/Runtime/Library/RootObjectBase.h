//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class RootObjectInlineCache
    {
    public:
        RootObjectInlineCache(InlineCacheAllocator * allocator);
        uint AddRef() { return ++refCount; }
        uint Release() { Assert(refCount != 0); return --refCount; }
        Js::InlineCache * GetInlineCache() const { return inlineCache; }
        uint GetRefCount() { return refCount; }
    private:
        uint refCount;
        Js::InlineCache * inlineCache;
    };

    class RootObjectBase: public DynamicObject
    {
    public:
        HostObjectBase * GetHostObject() const;
        void SetHostObject(HostObjectBase * hostObject);

        Js::InlineCache * GetInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore);
        Js::RootObjectInlineCache * GetRootInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore);
        void ReleaseInlineCache(PropertyId propertyId, bool isLoadMethod, bool isStore, bool isShutdown);

        virtual BOOL EnsureProperty(PropertyId propertyId) override;
        virtual BOOL EnsureNoRedeclProperty(PropertyId propertyId) override sealed;
        virtual BOOL HasOwnPropertyCheckNoRedecl(PropertyId propertyId) override sealed;

        // These are special "Root" versions of the property APIs that allow access
        // to global let and const variables, which are stored as properties on the
        // root object, but are hidden from normal property API access.
        virtual BOOL HasRootProperty(PropertyId propertyId);
        virtual BOOL GetRootProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL GetRootPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info);
        virtual DescriptorFlags GetRootSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext);
        virtual BOOL DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags);

        PropertyIndex GetRootPropertyIndex(PropertyId propertyId);

        void EnsureNoProperty(PropertyId propertyId);

        template <typename Fn>
        void MapLetConstGlobals(Fn fn);

#if DBG
        bool IsLetConstGlobal(PropertyId propertyId);
#endif

        static bool Is(Var var);
        static bool Is(RecyclableObject * obj);
        static RootObjectBase * FromVar(Var var);

    protected:
        DEFINE_VTABLE_CTOR(RootObjectBase, DynamicObject);

        // We shouldn't create a instance of this object, only derive from it, hide the constructor
        RootObjectBase(DynamicType * type);
        RootObjectBase(DynamicType * type, ScriptContext* scriptContext);

        HostObjectBase * hostObject;

        typedef JsUtil::BaseDictionary<PropertyRecord const *, RootObjectInlineCache *, Recycler> RootObjectInlineCacheMap;
        RootObjectInlineCacheMap * loadInlineCacheMap;
        RootObjectInlineCacheMap * loadMethodInlineCacheMap;
        RootObjectInlineCacheMap * storeInlineCacheMap;
    };

    template <typename Fn>
    void
    RootObjectBase::MapLetConstGlobals(Fn fn)
    {
        int index = 0;
        const PropertyRecord* propertyRecord;
        Var value;
        bool isConst;

        while (GetTypeHandler()->NextLetConstGlobal(index, this, &propertyRecord, &value, &isConst))
        {
            fn(propertyRecord, value, isConst);
        }
    }
};
