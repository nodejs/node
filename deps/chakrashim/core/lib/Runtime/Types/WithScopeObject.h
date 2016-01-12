//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Js
{
    class WithScopeObject : public RecyclableObject
    {
        private:
            RecyclableObject *wrappedObject;

            void AssertAndFailFast() { AssertMsg(false, "This function should not be invoked"); Js::Throw::InternalError(); }
        protected:
            DEFINE_VTABLE_CTOR(WithScopeObject, RecyclableObject);

        public:
            WithScopeObject(RecyclableObject *wrappedObject, StaticType * type) : RecyclableObject(type), wrappedObject(wrappedObject) {}
            static bool Is(Var aValue);
            static WithScopeObject* FromVar(Var value);
            RecyclableObject *GetWrappedObject() { return wrappedObject; }
            virtual BOOL HasProperty(PropertyId propertyId) override;
            virtual BOOL HasOwnProperty(PropertyId propertyId) override;
            virtual BOOL SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override;
            virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
            virtual BOOL DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags) override;
            virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
            virtual DescriptorFlags GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override;

            // A WithScopeObject should never call the Functions defined below this comment
            virtual BOOL SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override { AssertAndFailFast(); return FALSE; };
            virtual DescriptorFlags GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext) override { AssertAndFailFast(); return None; };
            virtual int GetPropertyCount() override { AssertAndFailFast(); return 0; };
            virtual PropertyId GetPropertyId(PropertyIndex index) override { AssertAndFailFast();  return Constants::NoProperty; };
            virtual PropertyId GetPropertyId(BigPropertyIndex index) override { AssertAndFailFast(); return Constants::NoProperty;; };
            virtual BOOL SetInternalProperty(PropertyId internalPropertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags = PropertyOperation_None, PropertyValueInfo* info = NULL) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags = PropertyOperation_None, SideEffects possibleSideEffects = SideEffects_Any) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL IsFixedProperty(PropertyId propertyId) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL HasItem(uint32 index) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL HasOwnItem(uint32 index) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override { AssertAndFailFast(); return FALSE; };
            virtual DescriptorFlags GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext) override { AssertAndFailFast(); return None; };
            virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags flags) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags flags) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext* scriptContext, bool preferSnapshotSemantics = true, bool enumSymbols = false) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags = PropertyOperation_None) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL IsWritable(PropertyId propertyId) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL IsConfigurable(PropertyId propertyId) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL IsEnumerable(PropertyId propertyId) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL SetEnumerable(PropertyId propertyId, BOOL value) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL SetWritable(PropertyId propertyId, BOOL value) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL SetConfigurable(PropertyId propertyId, BOOL value) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL SetAttributes(PropertyId propertyId, PropertyAttributes attributes) override { AssertAndFailFast(); return FALSE; };
            virtual BOOL IsExtensible() override { AssertAndFailFast(); return FALSE; };
            virtual BOOL PreventExtensions() override { AssertAndFailFast(); return FALSE; };
            virtual BOOL Seal() override { AssertAndFailFast(); return FALSE; };
            virtual BOOL Freeze() override { AssertAndFailFast(); return FALSE; };
            virtual BOOL IsSealed() override { AssertAndFailFast(); return FALSE; };
            virtual BOOL IsFrozen() override { AssertAndFailFast(); return FALSE; };
            virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override { AssertAndFailFast(); return FALSE; };
            virtual Var GetTypeOfString(ScriptContext * requestContext) override { AssertAndFailFast(); return RecyclableObject::GetTypeOfString(requestContext); };
    };
} // namespace Js
