//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename T>
    class CrossSiteObject : public T
    {
    private:
        DEFINE_VTABLE_CTOR(CrossSiteObject<T>, T);

    public:
        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
        virtual BOOL InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags = PropertyOperation_None, PropertyValueInfo* info = NULL) override;
        virtual BOOL SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any) override;
        virtual BOOL InitPropertyScoped(PropertyId propertyId, Var value) override;
        virtual BOOL InitFuncScoped(PropertyId propertyId, Var value) override;

        virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual DescriptorFlags GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext) override;
        virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags flags) override;
        virtual BOOL GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext * requestContext, bool preferSnapshotSemantics, bool enumSymbols = false) override;
        virtual Var GetHostDispatchVar() override;

        virtual DescriptorFlags GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual DescriptorFlags GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags) override;
        virtual void RemoveFromPrototype(ScriptContext * requestContext) override;
        virtual void AddToPrototype(ScriptContext * requestContext) override;
        virtual void SetPrototype(RecyclableObject* newPrototype) override;

        virtual BOOL IsCrossSiteObject() const override { return TRUE; }
        virtual void MarshalToScriptContext(ScriptContext * requestContext) override
        {
            AssertMsg(false, "CrossSite::MarshalVar should have handled this");
        }
    };

    template <typename T>
    BOOL CrossSiteObject<T>::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        originalInstance = CrossSite::MarshalVar(GetScriptContext(), originalInstance);
        BOOL result = __super::GetProperty(originalInstance, propertyId, value, info, requestContext);
        if (result)
        {
            *value = CrossSite::MarshalVar(requestContext, *value);
        }
        return result;
    }

    template <typename T>
    BOOL CrossSiteObject<T>::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        BOOL result = __super::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
        if (result)
        {
            *value = CrossSite::MarshalVar(requestContext, *value);
        }
        return result;
    }

    template <typename T>
    BOOL CrossSiteObject<T>::GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {
        BOOL result = __super::GetAccessors(propertyId, getter, setter, requestContext);
        if (result)
        {
            if (*getter != nullptr)
            {
                *getter = CrossSite::MarshalVar(requestContext, *getter);
            }
            if (*setter != nullptr)
            {
                *setter = CrossSite::MarshalVar(requestContext, *setter);
            }
        }
        return result;
    }

    template <typename T>
    BOOL CrossSiteObject<T>::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        originalInstance = CrossSite::MarshalVar(GetScriptContext(), originalInstance);
        BOOL result = __super::GetPropertyReference(originalInstance, propertyId, value, info, requestContext);
        if (result)
        {
            *value = CrossSite::MarshalVar(requestContext, *value);
        }
        return result;
    }

    template <typename T>
    BOOL CrossSiteObject<T>::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        value = CrossSite::MarshalVar(GetScriptContext(), value);
        return __super::SetProperty(propertyId, value, flags, info);
    }

    template <typename T>
    BOOL CrossSiteObject<T>::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        value = CrossSite::MarshalVar(GetScriptContext(), value);
        return __super::SetProperty(propertyNameString, value, flags, info);
    }

    template <typename T>
    BOOL CrossSiteObject<T>::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {
        value = CrossSite::MarshalVar(GetScriptContext(), value);
        return __super::InitProperty(propertyId, value, flags, info);
    }

    template <typename T>
    BOOL CrossSiteObject<T>::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects = SideEffects_Any)
    {
        value = CrossSite::MarshalVar(GetScriptContext(), value);
        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    template <typename T>
    BOOL CrossSiteObject<T>::InitPropertyScoped(PropertyId propertyId, Var value)
    {
        value = CrossSite::MarshalVar(GetScriptContext(), value);
        return __super::InitPropertyScoped(propertyId, value);
    }

    template <typename T>
    BOOL CrossSiteObject<T>::InitFuncScoped(PropertyId propertyId, Var value)
    {
        value = CrossSite::MarshalVar(GetScriptContext(), value);
        return __super::InitFuncScoped(propertyId, value);
    }

    template <typename T>
    BOOL CrossSiteObject<T>::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {
        originalInstance = CrossSite::MarshalVar(GetScriptContext(), originalInstance);
        BOOL result = __super::GetItem(originalInstance, index, value, requestContext);
        if (result)
        {
            *value = CrossSite::MarshalVar(requestContext, *value);
        }
        return result;
    }

    template <typename T>
    BOOL CrossSiteObject<T>::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {
        originalInstance = CrossSite::MarshalVar(GetScriptContext(), originalInstance);
        BOOL result = __super::GetItemReference(originalInstance, index, value, requestContext);
        if (result)
        {
            *value = CrossSite::MarshalVar(requestContext, *value);
        }
        return result;
    }

    template <typename T>
    DescriptorFlags CrossSiteObject<T>::GetItemSetter(uint32 index, Var *setterValue, ScriptContext* requestContext)
    {
        DescriptorFlags flags = __super::GetItemSetter(index, setterValue, requestContext);
        if ((flags & Accessor) == Accessor && *setterValue)
        {
            *setterValue = CrossSite::MarshalVar(requestContext, *setterValue);
        }
        return flags;
    }

    template <typename T>
    BOOL CrossSiteObject<T>::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {
        value = CrossSite::MarshalVar(GetScriptContext(), value);
        return __super::SetItem(index, value, flags);
    }

    template <typename T>
    DescriptorFlags CrossSiteObject<T>::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags flags = __super::GetSetter(propertyId, setterValue, info, requestContext);
        if ((flags & Accessor) == Accessor && *setterValue)
        {
            PropertyValueInfo::SetNoCache(info, this);
            *setterValue = CrossSite::MarshalVar(requestContext, *setterValue);
        }
        return flags;
    }

    template <typename T>
    DescriptorFlags CrossSiteObject<T>::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        DescriptorFlags flags = __super::GetSetter(propertyNameString, setterValue, info, requestContext);
        if ((flags & Accessor) == Accessor && *setterValue)
        {
            PropertyValueInfo::SetNoCache(info, this);
            *setterValue = CrossSite::MarshalVar(requestContext, *setterValue);
        }
        return flags;
    }

    template <typename T>
    BOOL CrossSiteObject<T>::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {
        if (getter != nullptr)
        {
            getter = CrossSite::MarshalVar(GetScriptContext(), getter);
        }
        if (setter != nullptr)
        {
            setter = CrossSite::MarshalVar(GetScriptContext(), setter);
        }
        return __super::SetAccessors(propertyId, getter, setter, flags);
    }

    template <typename T>
    void CrossSiteObject<T>::RemoveFromPrototype(ScriptContext * requestContext)
    {
        if (GetScriptContext() == requestContext)
        {
            __super::RemoveFromPrototype(requestContext);
        }
        // else do nothing because we never cache cross-context
    }

    template <typename T>
    void CrossSiteObject<T>::AddToPrototype(ScriptContext * requestContext)
    {
        if (GetScriptContext() == requestContext)
        {
            __super::AddToPrototype(requestContext);
        }
        // else do nothing because we never cache cross-context
    }

    template <typename T>
    void CrossSiteObject<T>::SetPrototype(RecyclableObject* newPrototype)
    {
        newPrototype = (RecyclableObject*)CrossSite::MarshalVar(GetScriptContext(), newPrototype);
        __super::SetPrototype(newPrototype);
    }

    template <typename T>
    BOOL CrossSiteObject<T>::GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext * requestContext, bool preferSnapshotSemantics, bool enumSymbols)
    {
        BOOL result = __super::GetEnumerator(enumNonEnumerable, enumerator, requestContext, preferSnapshotSemantics, enumSymbols);
        if (result)
        {
            *enumerator = CrossSite::MarshalEnumerator(requestContext, *enumerator);
        }
        return result;
    }

    template <typename T>
    Var CrossSiteObject<T>::GetHostDispatchVar()
    {
        Var hostDispatch = __super::GetHostDispatchVar();
        AssertMsg(hostDispatch, "hostDispatch");
        hostDispatch = CrossSite::MarshalVar(GetScriptContext(), hostDispatch);
        return hostDispatch;
    }
}
