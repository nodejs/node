//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ArgumentsObjectEnumerator::ArgumentsObjectEnumerator(ArgumentsObject* argumentsObject, ScriptContext* requestContext, BOOL enumNonEnumerable, bool enumSymbols)
        : JavascriptEnumerator(requestContext),
        argumentsObject(argumentsObject),
        enumNonEnumerable(enumNonEnumerable),
        enumSymbols(enumSymbols)
    {
        Reset();
    }

    Var ArgumentsObjectEnumerator::GetCurrentIndex()
    {
        if (!doneFormalArgs)
        {
            return argumentsObject->GetScriptContext()->GetIntegerString(formalArgIndex);
        }
        return objectEnumerator->GetCurrentIndex();
    }

    Var ArgumentsObjectEnumerator::GetCurrentValue()
    {
        if (!doneFormalArgs)
        {
            Var value = nullptr;
            argumentsObject->GetItem(argumentsObject, formalArgIndex, &value, argumentsObject->GetScriptContext());
            return value;
        }
        return objectEnumerator->GetCurrentValue();
    }

    BOOL ArgumentsObjectEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        if (!doneFormalArgs)
        {
            formalArgIndex = argumentsObject->GetNextFormalArgIndex(formalArgIndex, this->enumNonEnumerable, attributes);
            if (formalArgIndex != JavascriptArray::InvalidIndex
                && formalArgIndex < argumentsObject->GetNumberOfArguments())
            {
                return true;
            }

            doneFormalArgs = true;
        }
        return objectEnumerator->MoveNext(attributes);
    }

    void ArgumentsObjectEnumerator::Reset()
    {
        formalArgIndex = JavascriptArray::InvalidIndex;
        doneFormalArgs = false;

        Var enumerator;
        argumentsObject->DynamicObject::GetEnumerator(enumNonEnumerable, &enumerator, GetScriptContext(), true, enumSymbols);
        objectEnumerator = (Js::JavascriptEnumerator*)enumerator;
    }

    bool ArgumentsObjectEnumerator::GetCurrentPropertyId(PropertyId *pPropertyId)
    {
        if (!doneFormalArgs)
        {
            *pPropertyId = Constants::NoProperty;
            return false;
        }
        return objectEnumerator->GetCurrentPropertyId(pPropertyId);
    }

    //---------------------- ES5ArgumentsObjectEnumerator -------------------------------

    ES5ArgumentsObjectEnumerator::ES5ArgumentsObjectEnumerator(ArgumentsObject* argumentsObject, ScriptContext* requestcontext, BOOL enumNonEnumerable, bool enumSymbols)
        : ArgumentsObjectEnumerator(argumentsObject, requestcontext, enumNonEnumerable, enumSymbols),
        enumeratedFormalsInObjectArrayCount(0)
    {
        this->Reset();
    }

    BOOL ES5ArgumentsObjectEnumerator::MoveNext(PropertyAttributes* attributes)
    {
        // Formals:
        // - deleted => not in objectArray && not connected -- do not enum, do not advance
        // - connected,     in objectArray -- if (enumerable) enum it, advance objectEnumerator
        // - disconnected =>in objectArray -- if (enumerable) enum it, advance objectEnumerator

        if (!doneFormalArgs)
        {
            ES5HeapArgumentsObject* es5HAO = static_cast<ES5HeapArgumentsObject*>(argumentsObject);
            formalArgIndex = es5HAO->GetNextFormalArgIndexHelper(formalArgIndex, this->enumNonEnumerable, attributes);
            if (formalArgIndex != JavascriptArray::InvalidIndex)
            {
                if (formalArgIndex < argumentsObject->GetNumberOfArguments())
                {
                    if (argumentsObject->HasObjectArrayItem(formalArgIndex))
                    {
                        BOOL tempResult = objectEnumerator->MoveNext(attributes);
                        AssertMsg(tempResult, "We advanced objectEnumerator->MoveNext() too many times.");
                    }

                    return TRUE;
                }
            }

            doneFormalArgs = true;
        }

        return objectEnumerator->MoveNext(attributes);
    }

    void ES5ArgumentsObjectEnumerator::Reset()
    {
        __super::Reset();
        this->enumeratedFormalsInObjectArrayCount = 0;
    }
}
