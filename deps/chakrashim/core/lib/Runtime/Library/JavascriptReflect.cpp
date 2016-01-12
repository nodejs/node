//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Types\DynamicObjectEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumeratorWPCache.h"
#include "Library\ForInObjectEnumerator.h"

namespace Js
{
    Var JavascriptReflect::EntryDefineProperty(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.defineProperty");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.defineProperty");
        }

        Var propertyKey, attributes;
        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.defineProperty");
        }
        Var target = args[1];

        propertyKey = args.Info.Count > 2 ? args[2] : undefinedValue;
        PropertyRecord const * propertyRecord;
        JavascriptConversion::ToPropertyKey(propertyKey, scriptContext, &propertyRecord);

        attributes = args.Info.Count > 3 ? args[3] : undefinedValue;
        PropertyDescriptor propertyDescriptor;
        if (!JavascriptOperators::ToPropertyDescriptor(attributes, &propertyDescriptor, scriptContext))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_PropertyDescriptor_Invalid, scriptContext->GetPropertyName(propertyRecord->GetPropertyId())->GetBuffer());
        }

        // If the object is HostDispatch try to invoke the operation remotely
        BOOL defineResult;
        if (JavascriptOperators::GetTypeId(target) == TypeIds_HostDispatch)
        {
            defineResult = RecyclableObject::FromVar(target)->InvokeBuiltInOperationRemotely(EntryDefineProperty, args, nullptr);
        }
        else
        {
            defineResult = JavascriptObject::DefineOwnPropertyHelper(RecyclableObject::FromVar(target), propertyRecord->GetPropertyId(), propertyDescriptor, scriptContext, false);
        }

        return scriptContext->GetLibrary()->GetTrueOrFalse(defineResult);
    }

    Var JavascriptReflect::EntryDeleteProperty(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.deleteProperty");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.deleteProperty");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.deleteProperty");
        }
        Var target = args[1];
        Var propertyKey = args.Info.Count > 2 ? args[2] : undefinedValue;

        return JavascriptOperators::OP_DeleteElementI(target, propertyKey, scriptContext);
    }

    Var JavascriptReflect::EntryEnumerate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.enumerate");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.enumerate");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.enumerate");
        }
        Var target = args[1];

        Var iterator = nullptr;
        Recycler* recycler = scriptContext->GetRecycler();
        ForInObjectEnumeratorWrapper* forinEnumerator = RecyclerNew(recycler, Js::ForInObjectEnumeratorWrapper, RecyclableObject::FromVar(target), scriptContext);
        iterator = JavascriptEnumeratorIterator::Create(forinEnumerator, nullptr, scriptContext);
        return iterator;
    }

    Var JavascriptReflect::EntryGet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.get");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.get");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.get");
        }
        Var target = args[1];
        Var propertyKey = args.Info.Count > 2 ? args[2] : undefinedValue;

        Var receiver = args.Info.Count > 3 ? args[3] : target;

        return JavascriptOperators::GetElementIHelper(RecyclableObject::FromVar(target), propertyKey, receiver, scriptContext);
    }

    Var JavascriptReflect::EntryGetOwnPropertyDescriptor(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.getOwnPropertyDescriptor");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.getOwnPropertyDescriptor");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.getOwnPropertyDescriptor");
        }
        Var target = args[1];
        Var propertyKey = args.Info.Count > 2 ? args[2] : undefinedValue;

        if (JavascriptOperators::GetTypeId(target) == TypeIds_HostDispatch)
        {
            Var result;
            if (RecyclableObject::FromVar(target)->InvokeBuiltInOperationRemotely(EntryGetOwnPropertyDescriptor, args, &result))
            {
                return result;
            }
        }

        return JavascriptObject::GetOwnPropertyDescriptorHelper(RecyclableObject::FromVar(target), propertyKey, scriptContext);
    }

    Var JavascriptReflect::EntryGetPrototypeOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.getPrototypeOf");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.getPrototypeOf");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.getPrototypeOf");
        }
        Var target = args[1];
        return JavascriptObject::GetPrototypeOf(RecyclableObject::FromVar(target), scriptContext);
    }

    Var JavascriptReflect::EntryHas(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.has");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.has");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.has");
        }
        Var target = args[1];

        Var propertyKey = args.Info.Count > 2 ? args[2] : undefinedValue;
        BOOL result = JavascriptOperators::OP_HasItem(target, propertyKey, scriptContext);
        return scriptContext->GetLibrary()->GetTrueOrFalse(result);
    }

    Var JavascriptReflect::EntryIsExtensible(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.issExtensible");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.isExtensible");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.iesExtensible");
        }
        Var target = args[1];
        RecyclableObject *object = RecyclableObject::FromVar(target);
        BOOL isExtensible = object->IsExtensible();

        GlobalObject* globalObject = object->GetLibrary()->GetGlobalObject();
        if (isExtensible && globalObject != object && globalObject && (globalObject->ToThis() == object))
        {
            isExtensible = globalObject->IsExtensible();
        }

        return scriptContext->GetLibrary()->GetTrueOrFalse(isExtensible);
    }

    Var JavascriptReflect::EntryOwnKeys(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.ownKeys");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.ownKeys");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.ownKeys");
        }
        Var target = args[1];

        return JavascriptOperators::GetOwnPropertyKeys(target, scriptContext);
    }

    Var JavascriptReflect::EntryPreventExtensions(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.preventExtensions");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.preventExtensions");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.preventExtensions");
        }
        Var target = args[1];

        RecyclableObject* targetObj = RecyclableObject::FromVar(target);
        GlobalObject* globalObject = targetObj->GetLibrary()->GetGlobalObject();
        if (globalObject != targetObj && globalObject && (globalObject->ToThis() == targetObj))
        {
            globalObject->PreventExtensions();
        }

        return  scriptContext->GetLibrary()->GetTrueOrFalse(targetObj->PreventExtensions());
    }

    Var JavascriptReflect::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.set");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.set");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.set");
        }
        Var target = args[1];
        Var propertyKey = args.Info.Count > 2 ? args[2] : undefinedValue;
        Var value = args.Info.Count > 3 ? args[3] : undefinedValue;
        Var receiver = args.Info.Count > 4 ? args[4] : target;

        target = JavascriptOperators::ToObject(target, scriptContext);
        BOOL result = JavascriptOperators::SetElementIHelper(receiver, RecyclableObject::FromVar(target), propertyKey, value, scriptContext, PropertyOperationFlags::PropertyOperation_None);
        return scriptContext->GetLibrary()->GetTrueOrFalse(result);
    }

    Var JavascriptReflect::EntrySetPrototypeOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.setPrototypeOf");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if  (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.setPrototypeOf");
        }

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Reflect.setPrototypeOf");
        }
        Var target = args[1];
        target = JavascriptOperators::ToObject(target, scriptContext);

        if (args.Info.Count < 3 || !JavascriptOperators::IsObjectOrNull(args[2]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NotObjectOrNull, L"Object.setPrototypeOf");
        }

        RecyclableObject* newPrototype = RecyclableObject::FromVar(args[2]);
        BOOL changeResult = JavascriptObject::ChangePrototype(RecyclableObject::FromVar(target), newPrototype, /*validate*/false, scriptContext);

        return scriptContext->GetLibrary()->GetTrueOrFalse(changeResult);
    }

    Var JavascriptReflect::EntryApply(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.apply");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.apply");
        }

        Var target = args.Info.Count > 1 ? args[1] : undefinedValue;
        if (!JavascriptConversion::IsCallable(target))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"Reflect.apply");
        }
        Var thisArgument = args.Info.Count > 2 ? args[2] : undefinedValue;
        Var argArray = args.Info.Count > 3 ? args[3] : undefinedValue;

        return JavascriptFunction::ApplyHelper(RecyclableObject::FromVar(target), thisArgument, argArray, scriptContext);
    }

    static const int STACK_ARGS_ALLOCA_THRESHOLD = 8;
    Var JavascriptReflect::EntryConstruct(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"Reflect.construct");

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if (args.Info.Flags & CallFlags_New)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew, L"Reflect.construct");
        }

        Var target = args.Info.Count > 1 ? args[1] : undefinedValue;

        if (!JavascriptOperators::IsConstructor(target))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedConstructor, L"target");
        }

        Var newTarget = nullptr;
        if (scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled())
        {
            if (args.Info.Count > 3)
            {
                newTarget = args[3];
                if (!JavascriptOperators::IsConstructor(newTarget))
                {
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedConstructor, L"newTarget");
                }
            }
            else
            {
                newTarget = target;
            }
        }

        RecyclableObject* thisArg = RecyclableObject::FromVar(undefinedValue);
        if (newTarget != nullptr)
        {
            thisArg = JavascriptOperators::CreateFromConstructor(RecyclableObject::FromVar(newTarget), scriptContext);
        }

        Var argArray = args.Info.Count > 2 ? args[2] : undefinedValue;
        return JavascriptFunction::ConstructHelper(RecyclableObject::FromVar(target), thisArg, newTarget, argArray, scriptContext);
    }
}
