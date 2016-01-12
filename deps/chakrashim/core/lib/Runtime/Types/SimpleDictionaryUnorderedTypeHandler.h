//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // This is an unordered type handler (enumeration order of properties is nondeterministic). It is used when an object using
    // a SimpleDictionaryTypeHandler is determined to be used like a hashtable, to prevent unbounded memory growth.
    //
    // An object that is used as a hashtable will typically have a number of property adds and deletes, where the added
    // properties often don't have the same property name as a previously deleted property. Since SimpleDictionaryTypeHandler
    // does not remove deleted properties from its property map, and preserves the slot in the object (to preserve
    // enumeration order if a deleted property is added back), the property map and object continue to grow in size as the
    // script continues to add more property IDs to the object, even if there are corresponding deletes on different property
    // IDs to make room.
    //
    // At some point, SimpleDictionaryTypeHandler determines that it needs to stop the unbounded growth and start to reuse
    // property indexes from deleted properties for new properties, even if the property ID is different. That is when it
    // transitions into this unordered type handler.
    template<class TPropertyIndex, class TMapKey, bool IsNotExtensibleSupported>
    class SimpleDictionaryUnorderedTypeHandler sealed : public SimpleDictionaryTypeHandlerBase<TPropertyIndex, TMapKey, IsNotExtensibleSupported>
    {
        template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported> friend class SimpleDictionaryUnorderedTypeHandler;
        template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported> friend class SimpleDictionaryTypeHandlerBase;

    private:
        // A deleted property ID that will be reused for the next property add. The object's slot corresponding to this property
        // ID will have a tagged int that is the next deleted property ID in the chain.
        TPropertyIndex deletedPropertyIndex;

    public:
        DEFINE_GETCPPNAME();

    public:
        SimpleDictionaryUnorderedTypeHandler(Recycler * recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots);
        SimpleDictionaryUnorderedTypeHandler(ScriptContext * scriptContext, SimplePropertyDescriptor* propertyDescriptors, int propertyCount, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots);
        SimpleDictionaryUnorderedTypeHandler(Recycler* recycler, int slotCapacity, int propertyCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots);
        DEFINE_VTABLE_CTOR_NO_REGISTER(SimpleDictionaryUnorderedTypeHandler, SimpleDictionaryTypeHandlerBase);

    private:
        template<class OtherTPropertyIndex, class OtherTMapKey, bool OtherIsNotExtensibleSupported>
        void CopyUnorderedStateFrom(const SimpleDictionaryUnorderedTypeHandler<OtherTPropertyIndex, OtherTMapKey, OtherIsNotExtensibleSupported> &other)
        {
            CompileAssert(sizeof(TPropertyIndex) >= sizeof(OtherTPropertyIndex));
            if (other.deletedPropertyIndex != PropertyIndexRanges<OtherTPropertyIndex>::NoSlots)
            {
                deletedPropertyIndex = other.deletedPropertyIndex;
            }
        }

        bool IsReusablePropertyIndex(const TPropertyIndex propertyIndex);
        bool TryRegisterDeletedPropertyIndex(DynamicObject *const object, const TPropertyIndex propertyIndex);
        bool TryReuseDeletedPropertyIndex(DynamicObject *const object, TPropertyIndex *const propertyIndex);
        bool TryUndeleteProperty(
            DynamicObject *const object,
            const TPropertyIndex existingPropertyIndex,
            TPropertyIndex *const propertyIndex);
    };
}
