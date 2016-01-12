//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSetIterator::JavascriptSetIterator(DynamicType* type, JavascriptSet* set, JavascriptSetIteratorKind kind):
        DynamicObject(type),
        m_set(set),
        m_setIterator(set->GetIterator()),
        m_kind(kind)
    {
        Assert(type->GetTypeId() == TypeIds_SetIterator);
    }

    bool JavascriptSetIterator::Is(Var aValue)
    {
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return typeId == TypeIds_SetIterator;
    }

    JavascriptSetIterator* JavascriptSetIterator::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSetIterator'");

        return static_cast<JavascriptSetIterator *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptSetIterator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var thisObj = args[0];

        if (!JavascriptSetIterator::Is(thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedSetIterator, L"Set Iterator.prototype.next");
        }

        JavascriptSetIterator* iterator = JavascriptSetIterator::FromVar(thisObj);
        JavascriptSet* set = iterator->m_set;
        auto& setIterator = iterator->m_setIterator;

        if (set == nullptr || !setIterator.Next())
        {
            iterator->m_set = nullptr;
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        auto value = setIterator.Current();
        Var result;

        if (iterator->m_kind == JavascriptSetIteratorKind::KeyAndValue)
        {
            JavascriptArray* keyValueTuple = library->CreateArray(2);
            keyValueTuple->SetItem(0, value, PropertyOperation_None);
            keyValueTuple->SetItem(1, value, PropertyOperation_None);
            result = keyValueTuple;
        }
        else
        {
            Assert(iterator->m_kind == JavascriptSetIteratorKind::Value);
            result = value;
        }

        return library->CreateIteratorResultObjectValueFalse(result);
    }
} //namespace Js
