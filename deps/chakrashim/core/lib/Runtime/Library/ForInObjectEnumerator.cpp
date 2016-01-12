//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Types\DynamicObjectEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumeratorWPCache.h"
#include "Library\ForInObjectEnumerator.h"
#include "Library\NullEnumerator.h"

namespace Js
{

    ForInObjectEnumerator::ForInObjectEnumerator(RecyclableObject* object, ScriptContext * scriptContext, bool enumSymbols) :
        embeddedEnumerator(scriptContext),
        scriptContext(scriptContext),
        currentEnumerator(nullptr),
        propertyIds(nullptr),
        enumSymbols(enumSymbols)
    {
        Initialize(object, scriptContext);
    }

    void ForInObjectEnumerator::Clear()
    {
        // Only clear stuff that are not useful for the next enumerator
        propertyIds = nullptr;
        newPropertyStrings.Reset();
    }

    void ForInObjectEnumerator::Initialize(RecyclableObject* currentObject, ScriptContext * scriptContext)
    {
        Assert(propertyIds == nullptr);
        Assert(newPropertyStrings.Empty());
        Assert(this->GetScriptContext() == scriptContext);

        this->currentIndex = nullptr;

        if (currentObject == nullptr)
        {
            currentEnumerator = scriptContext->GetLibrary()->GetNullEnumerator();
            this->object = nullptr;
            this->baseObject = nullptr;
            this->baseObjectType = nullptr;
            this->firstPrototype = nullptr;
            return;
        }

        Assert(JavascriptOperators::GetTypeId(currentObject) != TypeIds_Null
            && JavascriptOperators::GetTypeId(currentObject) != TypeIds_Undefined);

        if (this->currentEnumerator != NULL &&
            !VirtualTableInfo<Js::NullEnumerator>::HasVirtualTable(this->currentEnumerator) &&
            this->object == currentObject &&
            this->baseObjectType == currentObject->GetType())
        {
            // We can re-use the enumerator, only if the 'object' and type from the previous enumeration
            // remains the same. If the previous enumeration involved prototype enumeration
            // 'object' and 'currentEnumerator' would represent the prototype. Hence,
            // we cannot re-use it. Null objects are always equal, therefore, the enumerator cannot
            // be re-used.
            currentEnumerator->Reset();
        }
        else
        {
            this->baseObjectType = currentObject->GetType();
            this->object = currentObject;

            GetCurrentEnumerator();
        }

        this->baseObject = currentObject;
        firstPrototype = GetFirstPrototypeWithEnumerableProperties(object);

        if (firstPrototype != nullptr)
        {
            Recycler *recyler = scriptContext->GetRecycler();
            propertyIds = RecyclerNew(recyler, BVSparse<Recycler>, recyler);
        }
    }

    RecyclableObject* ForInObjectEnumerator::GetFirstPrototypeWithEnumerableProperties(RecyclableObject* object)
    {
        RecyclableObject* firstPrototype = nullptr;
        if (JavascriptOperators::GetTypeId(object) != TypeIds_HostDispatch)
        {
            firstPrototype = object;
            while (true)
            {
                firstPrototype = firstPrototype->GetPrototype();

                if (JavascriptOperators::GetTypeId(firstPrototype) == TypeIds_Null)
                {
                    firstPrototype = nullptr;
                    break;
                }

                if (!DynamicType::Is(firstPrototype->GetTypeId())
                    || !DynamicObject::FromVar(firstPrototype)->GetHasNoEnumerableProperties())
                {
                    break;
                }
            }
        }

        return firstPrototype;
    }

    BOOL ForInObjectEnumerator::GetCurrentEnumerator()
    {
        Assert(object);
        ScriptContext* scriptContext = GetScriptContext();

        if (VirtualTableInfo<DynamicObject>::HasVirtualTable(object))
        {
            DynamicObject* dynamicObject = (DynamicObject*)object;
            if (!dynamicObject->GetTypeHandler()->EnsureObjectReady(dynamicObject))
            {
                return false;
            }
            dynamicObject->GetDynamicType()->PrepareForTypeSnapshotEnumeration();
            embeddedEnumerator.Initialize(dynamicObject, true);
            currentEnumerator = &embeddedEnumerator;
            return true;
        }

        if (!object->GetEnumerator(TRUE /*enumNonEnumerable*/, (Var *)&currentEnumerator, scriptContext, true /*preferSnapshotSemantics */, enumSymbols))
        {
            currentEnumerator = scriptContext->GetLibrary()->GetNullEnumerator();
            return false;
        }
        return true;
    }

    Var ForInObjectEnumerator::GetCurrentIndex()
    {
        if (currentIndex)
        {
            return currentIndex;
        }
        Assert(currentEnumerator != nullptr);
        return currentEnumerator->GetCurrentIndex();
    }

    Var ForInObjectEnumerator::GetCurrentValue()
    {
        return currentEnumerator->GetCurrentValue();
    }

    BOOL ForInObjectEnumerator::TestAndSetEnumerated(PropertyId propertyId)
    {
        Assert(propertyIds != nullptr);
        Assert(!Js::IsInternalPropertyId(propertyId));

        return !(propertyIds->TestAndSet(propertyId));
    }

    BOOL ForInObjectEnumerator::MoveNext()
    {
        PropertyId propertyId;
        currentIndex = GetCurrentAndMoveNext(propertyId);
        return currentIndex != NULL;
    }

    Var ForInObjectEnumerator::GetCurrentAndMoveNext(PropertyId& propertyId)
    {
        JavascriptEnumerator *pEnumerator = currentEnumerator;
        PropertyRecord const * propRecord;
        PropertyAttributes attributes = PropertyNone;

        while (true)
        {
            propertyId = Constants::NoProperty;
            currentIndex = pEnumerator->GetCurrentAndMoveNext(propertyId, &attributes);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(currentIndex);
#endif
            if (currentIndex)
            {
                if (firstPrototype == nullptr)
                {
                    // We are calculating correct shadowing for non-enumerable properties of the child object, we will receive
                    // both enumerable and non-enumerable properties from GetCurrentAndMoveNext so we need to check before we simply
                    // return here. If this property is non-enumerable we're going to skip it.
                    if (!(attributes & PropertyEnumerable))
                    {
                        continue;
                    }

                    // There are no prototype that has enumerable properties,
                    // don't need to keep track of the propertyIds we visited.
                    return currentIndex;
                }

                // Property Id does not exist.
                if (propertyId == Constants::NoProperty)
                {
                    if (!JavascriptString::Is(currentIndex)) //This can be undefined
                    {
                        continue;
                    }
                    JavascriptString *pString = JavascriptString::FromVar(currentIndex);
                    if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(pString))
                    {
                        // If we have a property string, it is assumed that the propertyId is being
                        // kept alive with the object
                        PropertyString * propertyString = (PropertyString *)pString;
                        propertyId = propertyString->GetPropertyRecord()->GetPropertyId();
                    }
                    else
                    {
                        ScriptContext* scriptContext = pString->GetScriptContext();
                        scriptContext->GetOrAddPropertyRecord(pString->GetString(), pString->GetLength(), &propRecord);
                        propertyId = propRecord->GetPropertyId();

                        // We keep the track of what is enumerated using a bit vector of propertyID.
                        // so the propertyId can't be collected until the end of the for in enumerator
                        // Keep a list of the property string.
                        newPropertyStrings.Prepend(GetScriptContext()->GetRecycler(), propRecord);
                    }
                }

                //check for shadowed property
                if (TestAndSetEnumerated(propertyId) //checks if the property is already enumerated or not
                    && (attributes & PropertyEnumerable))
                {
                    return currentIndex;
                }
            }
            else
            {
                if (object == baseObject)
                {
                    if (firstPrototype == nullptr)
                    {
                        return NULL;
                    }
                    object = firstPrototype;
                }
                else
                {
                    //walk the prototype chain
                    object = object->GetPrototype();
                    if ((object == NULL) || (JavascriptOperators::GetTypeId(object) == TypeIds_Null))
                    {
                        return NULL;
                    }
                }

                do
                {
                    if (!GetCurrentEnumerator())
                    {
                        return nullptr;
                    }

                    pEnumerator = currentEnumerator;
                    if (!VirtualTableInfo<Js::NullEnumerator>::HasVirtualTable(pEnumerator))
                    {
                        break;
                    }

                     //walk the prototype chain
                    object = object->GetPrototype();
                    if ((object == NULL) || (JavascriptOperators::GetTypeId(object) == TypeIds_Null))
                    {
                        return NULL;
                    }
                }
                while (true);
            }
        }
    }

    Var ForInObjectEnumerator::GetCurrentBothAndMoveNext(PropertyId& propertyId,Var *currentValueRef)
    {
        PropertyRecord const * propRecord;

        JavascriptEnumerator *pEnumerator = currentEnumerator;
        while (true)
        {
            propertyId = Constants::NoProperty;
            currentIndex = pEnumerator->GetCurrentBothAndMoveNext(propertyId,currentValueRef);
            if (currentIndex)
            {
                if (firstPrototype == nullptr)
                {
                    // There are no prototype that has enumerable properties,
                    // don't need to keep track of the propertyIds we visited.
                    return currentIndex;
                }

                // Property Id does not exist.
                if (propertyId == Constants::NoProperty)
                {
                    if ( !JavascriptString::Is(currentIndex) ) //This can be undefined
                    {
                        continue;
                    }

                    JavascriptString *pString = JavascriptString::FromVar(currentIndex);
                    if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(pString))
                    {
                        // If we have a property string, it is assumed that the propertyId is being
                        // kept alive with the object
                        PropertyString * propertyString = (PropertyString *)pString;
                        propertyId = propertyString->GetPropertyRecord()->GetPropertyId();
                    }
                    else
                    {
                        ScriptContext* scriptContext = pString->GetScriptContext();
                        scriptContext->GetOrAddPropertyRecord(pString->GetString(), pString->GetLength(), &propRecord);
                        propertyId = propRecord->GetPropertyId();

                        // We keep the track of what is enumerated using a bit vector of propertyID.
                        // so the propertyId can't be collected until the end of the for in enumerator
                        // Keep a list of the property string.
                        newPropertyStrings.Prepend(GetScriptContext()->GetRecycler(), propRecord);
                    }
                }

                //check for shadowed property
                if(TestAndSetEnumerated(propertyId)) //checks if the property is already enumerated or not
                {
                    return currentIndex;
                }
            }
            else
            {
                if (object == baseObject)
                {
                    if (firstPrototype == nullptr)
                    {
                        return NULL;
                    }
                    object = firstPrototype;
                }
                else
                {
                    //walk the prototype chain
                    object = object->GetPrototype();
                    if (JavascriptOperators::GetTypeId(object) == TypeIds_Null)
                    {
                        return NULL;
                    }
                }

                if (!GetCurrentEnumerator())
                {
                    return nullptr;
                }
                pEnumerator = (JavascriptEnumerator *)currentEnumerator;
            }

        }
    }

    void ForInObjectEnumerator::Reset()
    {
        object = baseObject;
        if (propertyIds)
        {
            propertyIds->ClearAll();
        }

        currentIndex = nullptr;
        currentEnumerator = nullptr;
        GetCurrentEnumerator();
    }

    BOOL ForInObjectEnumerator::CanBeReused()
    {
        return object == nullptr || (object->GetScriptContext() == GetScriptContext() && !JavascriptProxy::Is(object));
    }
}
