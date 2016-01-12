//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
        // for enumerators, the scriptContext of the enumerator is different
        // from the scriptContext of the object.
#if !defined(USED_IN_STATIC_LIB)
#define DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(T) \
    friend class Js::CrossSiteEnumerator<T>; \
    virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) override \
    { \
        Assert(GetScriptContext() == scriptContext); \
        AssertMsg(VirtualTableInfo<T>::HasVirtualTable(this) || VirtualTableInfo<Js::CrossSiteEnumerator<T>>::HasVirtualTable(this), "Derived class need to define marshal to script context"); \
        VirtualTableInfo<Js::CrossSiteEnumerator<T>>::SetVirtualTable(this); \
    }
#else
#define DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(T) \
    virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) override {Assert(FALSE);}
#endif
    template <typename T>
    class CrossSiteEnumerator : public T
    {
    private:
        DEFINE_VTABLE_CTOR(CrossSiteEnumerator<T>, T);

    public:
        virtual Var GetCurrentIndex() override;
        virtual Var GetCurrentValue() override;
        virtual void Reset() override;
        virtual BOOL MoveNext(PropertyAttributes* attributes = nullptr) override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;
        virtual BOOL IsCrossSiteEnumerator() override
        {
            return true;
        }

    };

    template<typename T>
    Var CrossSiteEnumerator<T>::GetCurrentIndex()
    {
        Var result = __super::GetCurrentIndex();
        if (result)
        {
            result = CrossSite::MarshalVar(GetScriptContext(), result);
        }
        return result;
    }

    template <typename T>
    Var CrossSiteEnumerator<T>::GetCurrentValue()
    {
        Var result = __super::GetCurrentValue();
        if (result)
        {
            result = CrossSite::MarshalVar(GetScriptContext(), result);
        }
        return result;
    }

    template <typename T>
    BOOL CrossSiteEnumerator<T>::MoveNext(PropertyAttributes* attributes)
    {
        return __super::MoveNext(attributes);
    }

    template <typename T>
    void CrossSiteEnumerator<T>::Reset()
    {
        __super::Reset();
    }

    template <typename T>
    Var CrossSiteEnumerator<T>::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        Var result = __super::GetCurrentAndMoveNext(propertyId, attributes);
        if (result)
        {
            result = CrossSite::MarshalVar(GetScriptContext(), result);
        }
        return result;
    }

};
