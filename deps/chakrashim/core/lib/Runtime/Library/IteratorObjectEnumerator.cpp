//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    Var IteratorObjectEnumerator::Create(ScriptContext* scriptContext, Var iterator)
    {
        return RecyclerNew(scriptContext->GetRecycler(), IteratorObjectEnumerator, scriptContext, iterator);
    }

    IteratorObjectEnumerator::IteratorObjectEnumerator(ScriptContext* scriptContext, Var iterator) :
        JavascriptEnumerator(scriptContext),
        done(false),
        value(nullptr)
    {
        Assert(JavascriptOperators::IsObject(iterator));
        iteratorObject = RecyclableObject::FromVar(iterator);
    }

    void IteratorObjectEnumerator::EnsureIterator()
    {
        if (value == nullptr)
        {
            MoveNext();
        }
    }

    Var IteratorObjectEnumerator::GetCurrentIndex()
    {
        EnsureIterator();
        if (done)
        {
            return GetScriptContext()->GetLibrary()->GetUndefined();
        }
        return value;
    }

    Var IteratorObjectEnumerator::GetCurrentValue()
    {
        Assert(FALSE);
        return GetScriptContext()->GetLibrary()->GetUndefined();
    }

    BOOL IteratorObjectEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        ScriptContext* scriptContext = GetScriptContext();
        done = !JavascriptOperators::IteratorStepAndValue(iteratorObject, scriptContext, &value);

        if (attributes != nullptr)
        {
            *attributes = PropertyEnumerable;
        }

        return !done;
    }

    Var IteratorObjectEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {
        if (MoveNext(attributes))
        {
            Var currentIndex = GetCurrentIndex();
            ScriptContext* scriptContext = GetScriptContext();
            const PropertyRecord* propertyRecord = nullptr;
            if (!TaggedInt::Is(currentIndex) && JavascriptString::Is(currentIndex) &&
                VirtualTableInfo<Js::PropertyString>::HasVirtualTable(JavascriptString::FromVar(currentIndex)))
            {
                propertyRecord = ((PropertyString *)PropertyString::FromVar(currentIndex))->GetPropertyRecord();
            }
            else if (JavascriptSymbol::Is(currentIndex))
            {
                propertyRecord = JavascriptSymbol::FromVar(currentIndex)->GetValue();
            }
            else
            {
                JavascriptString* propertyName = JavascriptConversion::ToString(currentIndex, scriptContext);
                GetScriptContext()->GetOrAddPropertyRecord(propertyName->GetString(), propertyName->GetLength(), &propertyRecord);
            }

            propertyId = propertyRecord->GetPropertyId();
            return currentIndex;
        }
        return NULL;
    }

    void IteratorObjectEnumerator::Reset()
    {
        Assert(FALSE);
    }
};
