//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSet::JavascriptSet(DynamicType* type)
        : DynamicObject(type)
    {
    }

    JavascriptSet* JavascriptSet::New(ScriptContext* scriptContext)
    {
        JavascriptSet* set = scriptContext->GetLibrary()->CreateSet();
        set->set = RecyclerNew(scriptContext->GetRecycler(), SetDataSet, scriptContext->GetRecycler());

        return set;
    }

    bool JavascriptSet::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Set;
    }

    JavascriptSet* JavascriptSet::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSet'");

        return static_cast<JavascriptSet *>(RecyclableObject::FromVar(aValue));
    }

    JavascriptSet::SetDataList::Iterator JavascriptSet::GetIterator()
    {
        return list.GetIterator();
    }

    Var JavascriptSet::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Set");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(SetCount);

        JavascriptSet* setObject = nullptr;

        if (callInfo.Flags & CallFlags_New)
        {
            setObject = library->CreateSet();
        }
        else
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set", L"Set");
        }
        Assert(setObject != nullptr);

        Var iterable = (args.Info.Count > 1) ? args[1] : library->GetUndefined();

        RecyclableObject* iter = nullptr;
        RecyclableObject* adder = nullptr;

        if (JavascriptConversion::CheckObjectCoercible(iterable, scriptContext))
        {
            iter = JavascriptOperators::GetIterator(iterable, scriptContext);
            Var adderVar = JavascriptOperators::GetProperty(setObject, PropertyIds::add, scriptContext);
            if (!JavascriptConversion::IsCallable(adderVar))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
            }
            adder = RecyclableObject::FromVar(adderVar);
        }

        if (setObject->set != nullptr)
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_ObjectIsAlreadyInitialized, L"Set", L"Set");
        }


        setObject->set = RecyclerNew(scriptContext->GetRecycler(), SetDataSet, scriptContext->GetRecycler());

        if (iter != nullptr)
        {
            Var nextItem;

            while (JavascriptOperators::IteratorStepAndValue(iter, scriptContext, &nextItem))
            {
                adder->GetEntryPoint()(adder, CallInfo(CallFlags_Value, 2), setObject, nextItem);
            }
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), setObject, nullptr, scriptContext) :
            setObject;
    }

    Var JavascriptSet::EntryAdd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.add", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        Var value = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptNumber::Is(value) && JavascriptNumber::IsNegZero(JavascriptNumber::GetValue(value)))
        {
            // Normalize -0 to +0
            value = JavascriptNumber::New(0.0, scriptContext);
        }

        set->Add(value);

        return set;
    }

    Var JavascriptSet::EntryClear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.clear", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        set->Clear();

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptSet::EntryDelete(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.delete", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        Var value = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        bool didDelete = set->Delete(value);

        return scriptContext->GetLibrary()->CreateBoolean(didDelete);
    }

    Var JavascriptSet::EntryForEach(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Set.prototype.forEach");

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.forEach", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);


        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"Set.prototype.forEach");
        }
        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);

        Var thisArg = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        auto iterator = set->GetIterator();

        while (iterator.Next())
        {
            Var value = iterator.Current();

            callBackFn->GetEntryPoint()(callBackFn, CallInfo(CallFlags_Value, 4), thisArg, value, value, args[0]);
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptSet::EntryHas(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.has", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);


        Var value = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        bool hasValue = set->Has(value);

        return scriptContext->GetLibrary()->CreateBoolean(hasValue);
    }

    Var JavascriptSet::EntrySizeGetter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.size", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        int size = set->Size();

        return JavascriptNumber::ToVar(size, scriptContext);
    }

    Var JavascriptSet::EntryEntries(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.entries", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateSetIterator(set, JavascriptSetIteratorKind::KeyAndValue);
    }

    Var JavascriptSet::EntryValues(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (!JavascriptSet::Is(args[0]))
        {
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, L"Set.prototype.values", L"Set");
        }

        JavascriptSet* set = JavascriptSet::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateSetIterator(set, JavascriptSetIteratorKind::Value);
    }

    Var JavascriptSet::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    void JavascriptSet::Add(Var value)
    {
        if (!set->ContainsKey(value))
        {
            SetDataNode* node = list.Append(value, GetScriptContext()->GetRecycler());
            set->Add(value, node);
        }
    }

    void JavascriptSet::Clear()
    {
        // TODO: (Consider) Should we clear the set here and leave it as large as it has grown, or
        // toss it away and create a new empty set, letting it grow as needed?
        list.Clear();
        set->Clear();
    }

    bool JavascriptSet::Delete(Var value)
    {
        if (set->ContainsKey(value))
        {
            SetDataNode* node = set->Item(value);
            list.Remove(node);
            return set->Remove(value);
        }
        return false;
    }

    bool JavascriptSet::Has(Var value)
    {
        return set->ContainsKey(value);
    }

    int JavascriptSet::Size()
    {
        return set->Count();
    }

    BOOL JavascriptSet::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Set");
        return TRUE;
    }
}
