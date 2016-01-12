//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename TPropertyIndex>
    class DictionaryPropertyDescriptor
    {
        template <typename T>
        friend class DictionaryPropertyDescriptor;
    public:
        DictionaryPropertyDescriptor(TPropertyIndex dataSlot, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(dataSlot), Getter(NoSlots), Setter(NoSlots), Attributes(PropertyDynamicTypeDefaults),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(false) { }

        DictionaryPropertyDescriptor(TPropertyIndex getterSlot, TPropertyIndex setterSlot, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(NoSlots), Getter(getterSlot), Setter(setterSlot), Attributes(PropertyDynamicTypeDefaults),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(true) { }

        DictionaryPropertyDescriptor(TPropertyIndex dataSlot, PropertyAttributes attributes, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(dataSlot), Getter(NoSlots), Setter(NoSlots), Attributes(attributes),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(false) { }

        DictionaryPropertyDescriptor(TPropertyIndex getterSlot, TPropertyIndex setterSlot, PropertyAttributes attributes, bool isInitialized = false, bool isFixed = false, bool usedAsFixed = false) :
            Data(NoSlots), Getter(getterSlot), Setter(setterSlot), Attributes(attributes),
            PreventFalseReference(true), IsInitialized(isInitialized), IsOnlyOneAccessorInitialized(false), IsFixed(isFixed), UsedAsFixed(usedAsFixed), IsShadowed(false), IsAccessor(true) { }

        // this is for initialization.
        DictionaryPropertyDescriptor() : Data(NoSlots), Getter(NoSlots), Setter(NoSlots), Attributes(PropertyDynamicTypeDefaults),
            PreventFalseReference(true), IsInitialized(false), IsOnlyOneAccessorInitialized(false), IsFixed(false), UsedAsFixed(false), IsShadowed(false), IsAccessor(false) { }

        template <typename TPropertyIndexFrom>
        void CopyFrom(DictionaryPropertyDescriptor<TPropertyIndexFrom>& descriptor);

        // SimpleDictionaryPropertyDescriptor is allocated by a dictionary along with the PropertyRecord
        // so it cannot be allocated as leaf, tag the lower bit to prevent false reference.
        bool PreventFalseReference:1;

        bool IsInitialized:1;
        bool IsOnlyOneAccessorInitialized:1;
        bool IsFixed:1;
        bool UsedAsFixed:1;
        bool IsShadowed : 1;
        bool IsAccessor : 1;
        PropertyAttributes Attributes;
    private:
        TPropertyIndex Data;
        // CONSIDER: sharing the Data slot with one of these and use the attributes to tell it apart.
        TPropertyIndex Getter;
        TPropertyIndex Setter;
    public:
        template <bool allowLetConstGlobal>
        TPropertyIndex GetDataPropertyIndex() const;
        TPropertyIndex GetGetterPropertyIndex() const;
        TPropertyIndex GetSetterPropertyIndex() const;
        void ConvertToData();
        bool ConvertToGetterSetter(TPropertyIndex& nextSlotIndex);

        bool HasNonLetConstGlobal() const
        {
            return (this->Attributes & PropertyLetConstGlobal) == 0 || this->IsShadowed;
        }
        void AddShadowedData(TPropertyIndex& nextPropertyIndex, bool addingLetConstGlobal);
    private:
        static const TPropertyIndex NoSlots = PropertyIndexRanges<TPropertyIndex>::NoSlots;

    public:
#if DBG
        bool SanityCheckFixedBits()
        {
            return
                ((!this->IsFixed && !this->UsedAsFixed) ||
                (!(this->Attributes & PropertyDeleted) && (this->Data != NoSlots || this->Getter != NoSlots || this->Setter != NoSlots)));
        }
#endif
    };


    template <typename TPropertyIndex>
    template <bool allowLetConstGlobal>
    TPropertyIndex DictionaryPropertyDescriptor<TPropertyIndex>::GetDataPropertyIndex() const
    {
        // If it is let const global, the data slot is the let const property, and if we allow let const global,
        // we already return that the Getter/Setter slot may be doubled as the Data Slot
        // so only return it if we allow let const
        bool const isLetConstGlobal = (this->Attributes & PropertyLetConstGlobal) != 0;
        if (isLetConstGlobal)
        {
            Assert(this->Data != NoSlots);  // Should always have slot for LetConstGlobal if specified
            if (allowLetConstGlobal)
            {
                return this->Data;
            }
            else if (this->IsShadowed && !this->IsAccessor)
            {
                // if it is a let const global, if the setter slot is empty, then the Getter slot must be
                // the shadowed data slot, return that.
                return this->Getter;
            }
        }
        else
        {
            Assert(!this->IsAccessor || this->Data == NoSlots);
            return this->Data;
        }
        return NoSlots;
    }

    template <typename TPropertyIndex>
    TPropertyIndex DictionaryPropertyDescriptor<TPropertyIndex>::GetGetterPropertyIndex() const
    {
        // Need to check data property index first
        Assert(GetDataPropertyIndex<false>() == NoSlots);
        return this->Getter;
    }

    template <typename TPropertyIndex>
    TPropertyIndex DictionaryPropertyDescriptor<TPropertyIndex>::GetSetterPropertyIndex() const
    {
        // Need to check data property index first
        Assert(GetDataPropertyIndex<false>() == NoSlots);
        return this->Setter;
    }

    template <typename TPropertyIndex>
    void DictionaryPropertyDescriptor<TPropertyIndex>::ConvertToData()
    {
        if (this->IsAccessor)
        {
            Assert(this->Getter != NoSlots && this->Setter != NoSlots);
            this->IsAccessor = false;
            if (this->IsShadowed)
            {
                Assert(this->Data != NoSlots);
            }
            else
            {
                Assert(this->Data == NoSlots);
                this->Data = this->Getter;
                this->Getter = NoSlots;
            }
        }
        Assert(GetDataPropertyIndex<false>() != NoSlots);
    }

    template <typename TPropertyIndex>
    void DictionaryPropertyDescriptor<TPropertyIndex>::AddShadowedData(TPropertyIndex& nextPropertyIndex, bool addingLetConstGlobal)
    {
        Assert(!this->IsShadowed);
        this->IsShadowed = true;
        if (this->IsAccessor)
        {
            Assert(this->Data == NoSlots);
        }
        else if (addingLetConstGlobal)
        {
            this->Getter = this->Data;
            this->Data = nextPropertyIndex++;
        }
        else
        {
            this->Getter = nextPropertyIndex++;
        }
        Assert(GetDataPropertyIndex<false>() != NoSlots);
    }

    template <typename TPropertyIndex>
    bool DictionaryPropertyDescriptor<TPropertyIndex>::ConvertToGetterSetter(TPropertyIndex& nextPropertyIndex)
    {
        // Initial descriptor state and corresponding conversion can be one of the following:
        //
        // | State                              | Data    | Getter   | Setter   | Conversion                                                                                        |
        // |------------------------------------|---------|----------|----------|---------------------------------------------------------------------------------------------------|
        // | Data property                      | valid   | NoSlots? | NoSlots? | Move Data to Getter, set Data to NoSlots, create new slot for Setter if necessary, set IsAccessor |
        // | LetConstGlobal                     | valid   | NoSlots? | NoSlots? | Create new slots for Getter and Setter if necessary, set IsAccessor, set IsShadowed               |
        // | Data property + LetConstGlobal     | valid   | valid    | NoSlots? | Create new slot for Setter if necessary, set IsAccessor                                           |
        // | Accessor property                  | NoSlots | valid    | valid    | Nothing                                                                                           |
        // | Accessor property + LetConstGlobal | valid   | valid    | valid    | Nothing                                                                                           |
        // |------------------------------------|---------|----------|----------|---------------------------------------------------------------------------------------------------|
        //
        // NOTE: Do not create slot for Getter/Setter if they are already valid; possible after previous conversion from Accessor to Data, or deletion of Accessor, etc.

        if (this->IsAccessor)
        {
            // Accessor property
            // Accessor property + LetConstGlobal
            Assert(this->Getter != NoSlots && this->Setter != NoSlots);
            return false;
        }

        this->IsAccessor = true;
        if (this->Attributes & PropertyLetConstGlobal)
        {
            if (this->IsShadowed)
            {
                // Data property + LetConstGlobal
                Assert(this->Getter != NoSlots);
            }
            else
            {
                // LetConstGlobal
                this->IsShadowed = true;
            }
        }
        else
        {
            // Data property
            Assert(this->Data != NoSlots);
            Assert(this->Getter == NoSlots);
            this->Getter = this->Data;
            this->Data = NoSlots;
        }

        bool addedPropertyIndex = false;
        if (this->Getter == NoSlots)
        {
            this->Getter = nextPropertyIndex++;
            addedPropertyIndex = true;
        }
        if (this->Setter == NoSlots)
        {
            this->Setter = nextPropertyIndex++;
            addedPropertyIndex = true;
        }
        Assert(this->GetGetterPropertyIndex() != NoSlots || this->GetSetterPropertyIndex() != NoSlots);
        return addedPropertyIndex;
    }

    template <typename TPropertyIndex>
    template <typename TPropertyIndexFrom>
    void DictionaryPropertyDescriptor<TPropertyIndex>::CopyFrom(DictionaryPropertyDescriptor<TPropertyIndexFrom>& descriptor)
    {
        this->Attributes = descriptor.Attributes;
        this->Data = (descriptor.Data == DictionaryPropertyDescriptor<TPropertyIndexFrom>::NoSlots) ? NoSlots : descriptor.Data;
        this->Getter = (descriptor.Getter == DictionaryPropertyDescriptor<TPropertyIndexFrom>::NoSlots) ? NoSlots : descriptor.Getter;
        this->Setter = (descriptor.Setter == DictionaryPropertyDescriptor<TPropertyIndexFrom>::NoSlots) ? NoSlots : descriptor.Setter;

        this->IsInitialized = descriptor.IsInitialized;
        this->IsFixed = descriptor.IsFixed;
        this->UsedAsFixed = descriptor.UsedAsFixed;
        this->IsAccessor = descriptor.IsAccessor;
    }
}
