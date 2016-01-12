//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

namespace Js {
    class JavascriptEnumerator : public RecyclableObject
    {
        friend class CrossSite;
        friend class ExternalObject;
    protected:
        DEFINE_VTABLE_CTOR_ABSTRACT(JavascriptEnumerator, RecyclableObject);
        virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) = 0;

        JavascriptEnumerator() { /* Do nothing, needed by the vtable ctor for ForInObjectEnumeratorWrapper */ }

    public:
        JavascriptEnumerator(ScriptContext* scriptContext);

        //
        // Returns item index for all nonnamed Enumerators
        //optional override
        //
        virtual uint32 GetCurrentItemIndex() { return Constants::InvalidSourceIndex; }

        //
        // Returns the current index
        //
        virtual Var GetCurrentIndex() = 0;

        //
        // Returns the current value
        //
        virtual Var GetCurrentValue() = 0;

        //
        // Moves to next element
        //
        virtual BOOL MoveNext(PropertyAttributes* attributes = nullptr) = 0;

        //
        // Sets the enumerator to its initial position
        //
        virtual void Reset() = 0;

        //
        // Moves to the next element and gets the current value.
        // PropertyId: Sets the propertyId of the current value.
        // In some cases, i.e. arrays, propertyId is not returned successfully.
        // Returns: NULL if there are no more elements.
        //
        // Note: in the future we  might want to enumerate specialPropertyIds
        // If that code is added in this base class use JavaScriptRegExpEnumerator.h/cpp
        // as a reference and then remove it. If you have already made the edits before
        // seeing this comment please just consolidate the changes.
        virtual Var GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr)
        {
            propertyId = Constants::NoProperty;
            if (MoveNext(attributes))
            {
                Var currentIndex = GetCurrentIndex();
                return currentIndex;
            }
            return NULL;
        }

        virtual Var GetCurrentBothAndMoveNext(PropertyId& propertyId, Var* currentValueRef)
        {
            propertyId = Constants::NoProperty;

            if (MoveNext())
            {
                Var currentIndex = GetCurrentIndex();
                *currentValueRef = GetCurrentValue();
                return currentIndex;
            }
            return NULL;
        }

        virtual bool GetCurrentPropertyId(PropertyId *propertyId)
        {
           *propertyId = Constants::NoProperty;
            return false;
        };

        virtual BOOL IsCrossSiteEnumerator()
        {
            return false;
        }

        static bool Is(Var aValue);
        static JavascriptEnumerator* FromVar(Var varValue);
    };
}
