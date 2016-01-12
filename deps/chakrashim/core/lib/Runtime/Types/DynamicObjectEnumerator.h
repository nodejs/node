//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename T, bool enumNonEnumerable, bool enumSymbols, bool snapShotSementics>
    class DynamicObjectEnumerator : public JavascriptEnumerator
    {
    protected:
        DynamicObject* object;
        JavascriptEnumerator* arrayEnumerator;
        DynamicType *initialType;
        T objectIndex;

        DynamicObjectEnumerator(ScriptContext* scriptContext)
            : JavascriptEnumerator(scriptContext)
        {
        }

        DEFINE_VTABLE_CTOR(DynamicObjectEnumerator, JavascriptEnumerator);
        DEFINE_MARSHAL_ENUMERATOR_TO_SCRIPT_CONTEXT(DynamicObjectEnumerator);

        void ResetHelper();
        void Initialize(DynamicObject* object);
        DynamicObjectEnumerator() { /* Do nothing, needed by the vtable ctor for ForInObjectEnumeratorWrapper */ }

    public:
        static JavascriptEnumerator* New(ScriptContext* scriptContext, DynamicObject* object);

    protected:
        DynamicType *GetTypeToEnumerate() const;

    public:
        virtual Var GetCurrentIndex() override;
        virtual Var GetCurrentValue() override;
        virtual BOOL MoveNext(PropertyAttributes* attributes = nullptr) override;
        virtual void Reset() override;
        virtual bool GetCurrentPropertyId(PropertyId *propertyId) override;
        virtual uint32 GetCurrentItemIndex() override;
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr) override;

        static uint32 GetOffsetOfObject() { return offsetof(DynamicObjectEnumerator, object); }
        static uint32 GetOffsetOfArrayEnumerator() { return offsetof(DynamicObjectEnumerator, arrayEnumerator); }
        static uint32 GetOffsetOfObjectIndex() { return offsetof(DynamicObjectEnumerator, objectIndex); }
    };

} // namespace Js
