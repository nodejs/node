//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    Var JavascriptEnumeratorIterator::Create(JavascriptEnumerator* enumerator, RecyclableObject* target, ScriptContext* scriptContext)
    {
        Recycler* recycler = scriptContext->GetThreadContext()->GetRecycler();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        return (JavascriptEnumeratorIterator*)RecyclerNew(recycler,
            JavascriptEnumeratorIterator,
            library->GetJavascriptEnumeratorIteratorType(),
            enumerator,
            target);
    }


    JavascriptEnumeratorIterator::JavascriptEnumeratorIterator(DynamicType* type, JavascriptEnumerator* enumerator, RecyclableObject* target) :
        DynamicObject(type),
        enumerator(enumerator),
        object(target),
        objectIndex(0)
    {
        Assert(type->GetTypeId() == TypeIds_JavascriptEnumeratorIterator);
    }

    bool JavascriptEnumeratorIterator::Is(Var obj)
    {
        return JavascriptOperators::GetTypeId(obj) == TypeIds_JavascriptEnumeratorIterator;
    }

    JavascriptEnumeratorIterator* JavascriptEnumeratorIterator::FromVar(Var obj)
    {
        Assert(JavascriptEnumeratorIterator::Is(obj));
        return static_cast<JavascriptEnumeratorIterator*>(obj);
    }

    Var JavascriptEnumeratorIterator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        AssertMsg(args.Info.Count > 0, "must have this pointer");
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Iterator.next");
        if (!JavascriptEnumeratorIterator::Is(args[0]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedArrayIterator, L"Iterator.next");
        }
        JavascriptEnumeratorIterator* iterator = JavascriptEnumeratorIterator::FromVar(args[0]);
        return iterator->InternalGetNext();
    }

    Var JavascriptEnumeratorIterator::InternalGetNext()
    {
        PropertyId propertyId;
        Var index = nullptr;
        JavascriptLibrary* library = GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        if (enumerator != nullptr)
        {
            index = enumerator->GetCurrentAndMoveNext(propertyId);
            if (index == nullptr)
            {
                // when done with iterator, cleanup the enumerator to avoid GC pressure.
                enumerator = nullptr;
            }
            else
            {
                const PropertyRecord* propertyRecord = scriptContext->GetThreadContext()->GetPropertyName(propertyId);
                Var name = index;
                // This iterator is created using ForInObjectEnumeratorWrapper that doesn't enum symbols.
                // If this changes in future, remove this assert.
                AssertMsg(!propertyRecord->IsSymbol(), "JavascriptEnumeratorIterator created from ForInObjectEnumeratorWrapper shouldn't enumerate over symbols.");
                return  library->CreateIteratorResultObjectValueFalse(name);
            }
        }

        Assert(index == nullptr);
        Var propertyName = nullptr;
        if (object != nullptr)
        {
            if (object->GetSpecialPropertyName(objectIndex, &propertyName, scriptContext))
            {
                if (!JavascriptOperators::IsUndefinedObject(propertyName, library->GetUndefined()))
                {
                    objectIndex++;
                    return library->CreateIteratorResultObjectValueFalse(propertyName);
                }
            }
            object = nullptr;
        }
        return library->CreateIteratorResultObjectUndefinedTrue();
    }
}
