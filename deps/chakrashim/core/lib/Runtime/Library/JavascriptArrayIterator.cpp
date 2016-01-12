//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptArrayIterator::JavascriptArrayIterator(DynamicType* type, Var iterable, JavascriptArrayIteratorKind kind):
        DynamicObject(type),
        m_iterableObject(iterable),
        m_nextIndex(0),
        m_kind(kind)
    {
        Assert(type->GetTypeId() == TypeIds_ArrayIterator);
        if (m_iterableObject == this->GetLibrary()->GetUndefined())
        {
            m_iterableObject = nullptr;
        }
    }

    bool JavascriptArrayIterator::Is(Var aValue)
    {
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return typeId == TypeIds_ArrayIterator;
    }

    JavascriptArrayIterator* JavascriptArrayIterator::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptArrayIterator'");

        return static_cast<JavascriptArrayIterator *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptArrayIterator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var thisObj = args[0];

        if (!JavascriptArrayIterator::Is(thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedArrayIterator, L"Array Iterator.prototype.next");
        }

        JavascriptArrayIterator* iterator = JavascriptArrayIterator::FromVar(thisObj);
        Var iterable = iterator->m_iterableObject;

        if (iterable == nullptr)
        {
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        int64 length;
        bool bArray = false;
        JavascriptArray* pArr = nullptr;

        if (DynamicObject::IsAnyArray(iterable) && !JavascriptArray::FromAnyArray(iterable)->IsCrossSiteObject())
        {
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(iterable);
#endif
            pArr = JavascriptArray::FromAnyArray(iterable);
            length = pArr->GetLength();
            bArray = true;
        }
        else
        {
            length = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(iterable, scriptContext), scriptContext);
        }

        int64 index = iterator->m_nextIndex;

        if (index >= length)
        {
            // Nulling out the m_iterableObject field is important so that the iterator
            // does not keep the iterable object alive after iteration is completed.
            iterator->m_iterableObject = nullptr;
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        iterator->m_nextIndex += 1;

        if (iterator->m_kind == JavascriptArrayIteratorKind::Key)
        {
            return library->CreateIteratorResultObjectValueFalse(JavascriptNumber::ToVar(index, scriptContext));
        }

        Var value;
        if (index <= UINT_MAX)
        {
            if (bArray)
            {
                if (JavascriptArray::Is(pArr))
                {
                    value = pArr->DirectGetItem((uint32)index);
                }
                else
                {
                    if (!JavascriptOperators::GetOwnItem(pArr, (uint32) index, &value, scriptContext))
                    {
                        value = library->GetUndefined();
                    }
                }
            }
            else
            {
                value = JavascriptOperators::OP_GetElementI_UInt32(iterable, (uint32)index, scriptContext);
            }
        }
        else
        {
            value = JavascriptOperators::OP_GetElementI(iterable, JavascriptNumber::ToVar(index, scriptContext), scriptContext);
        }

        if (iterator->m_kind == JavascriptArrayIteratorKind::Value)
        {
            return library->CreateIteratorResultObjectValueFalse(value);
        }

        Assert(iterator->m_kind == JavascriptArrayIteratorKind::KeyAndValue);

        JavascriptArray* keyValueTuple = library->CreateArray(2);

        keyValueTuple->SetItem(0, JavascriptNumber::ToVar(index, scriptContext), PropertyOperation_None);
        keyValueTuple->SetItem(1, value, PropertyOperation_None);

        return library->CreateIteratorResultObjectValueFalse(keyValueTuple);
    }
} //namespace Js
