//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Types\NullTypeHandler.h"

namespace Js
{
    Var JavascriptObject::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        if (args.Info.Count > 1)
        {
            switch (JavascriptOperators::GetTypeId(args[1]))
            {
            case TypeIds_Undefined:
            case TypeIds_Null:
                // Break to return a new object
                break;

            case TypeIds_StringObject:
            case TypeIds_Function:
            case TypeIds_Array:
            case TypeIds_ES5Array:
            case TypeIds_RegEx:
            case TypeIds_NumberObject:
            case TypeIds_Date:
            case TypeIds_BooleanObject:
            case TypeIds_Error:
            case TypeIds_Object:
            case TypeIds_Arguments:
            case TypeIds_ActivationObject:
            case TypeIds_SymbolObject:
                return isCtorSuperCall ?
                    JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(args[1]), nullptr, scriptContext) :
                    args[1];

            default:
                RecyclableObject* result = nullptr;
                if (FALSE == JavascriptConversion::ToObject(args[1], scriptContext, &result))
                {
                    // JavascriptConversion::ToObject should only return FALSE for null and undefined.
                    Assert(false);
                }

                return isCtorSuperCall ?
                    JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), result, nullptr, scriptContext) :
                    result;
            }
        }

        if (callInfo.Flags & CallFlags_NotUsed)
        {
            return args[0];
        }
        Var newObj = scriptContext->GetLibrary()->CreateObject(true);
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(newObj), nullptr, scriptContext) :
            newObj;
    }

    Var JavascriptObject::EntryHasOwnProperty(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        RecyclableObject* dynamicObject = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Object.prototype.hasOwnProperty");
        }

        // no property specified
        if (args.Info.Count == 1)
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        const PropertyRecord* propertyRecord;
        JavascriptConversion::ToPropertyKey(args[1], scriptContext, &propertyRecord);

        if (JavascriptOperators::HasOwnProperty(dynamicObject, propertyRecord->GetPropertyId(), scriptContext))
        {
            return scriptContext->GetLibrary()->GetTrue();
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    Var JavascriptObject::EntryPropertyIsEnumerable(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        RecyclableObject* dynamicObject = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Object.prototype.propertyIsEnumerable");
        }

        if (args.Info.Count >= 2)
        {
            const PropertyRecord* propertyRecord;
            JavascriptConversion::ToPropertyKey(args[1], scriptContext, &propertyRecord);
            PropertyId propertyId = propertyRecord->GetPropertyId();

            PropertyDescriptor currentDescriptor;
            BOOL isCurrentDescriptorDefined = JavascriptOperators::GetOwnPropertyDescriptor(dynamicObject, propertyId, scriptContext, &currentDescriptor);
            if (isCurrentDescriptorDefined == TRUE)
            {
                if (currentDescriptor.IsEnumerable())
                {
                    return scriptContext->GetLibrary()->GetTrue();
                }
            }
        }
        return scriptContext->GetLibrary()->GetFalse();
    }

    BOOL JavascriptObject::ChangePrototype(RecyclableObject* object, RecyclableObject* newPrototype, bool shouldThrow, ScriptContext* scriptContext)
    {
        // 8.3.2    [[SetInheritance]] (V)
        // When the [[SetInheritance]] internal method of O is called with argument V the following steps are taken:
        // 1.   Assert: Either Type(V) is Object or Type(V) is Null.
        Assert(JavascriptOperators::IsObject(object));
        Assert(JavascriptOperators::IsObjectOrNull(newPrototype));

        if (JavascriptProxy::Is(object))
        {
            JavascriptProxy* proxy = JavascriptProxy::FromVar(object);
            CrossSite::ForceCrossSiteThunkOnPrototypeChain(newPrototype);
            return proxy->SetPrototypeTrap(newPrototype, shouldThrow);
        }
        // 2.   Let extensible be the value of the [[Extensible]] internal data property of O.
        // 3.   Let current be the value of the [[Prototype]] internal data property of O.
        // 4.   If SameValue(V, current), then return true.
        if (newPrototype == JavascriptObject::GetPrototypeOf(object, scriptContext))
        {
            return TRUE;
        }

        // 5.   If extensible is false, then return false.
        if (!object->IsExtensible())
        {
            if (shouldThrow)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NonExtensibleObject);
            }
            return FALSE;
        }

        // 6.   If V is not null, then
        //  a.  Let p be V.
        //  b.  Repeat, while p is not null
        //      i.  If SameValue(p, O) is true, then return false.
        //      ii. Let nextp be the result of calling the [[GetInheritance]] internal method of p with no arguments.
        //      iii.    ReturnIfAbrupt(nextp).
        //      iv. Let  p be nextp.
        if (IsPrototypeOf(object, newPrototype, scriptContext)) // Reject cycle
        {
            if (shouldThrow)
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_CyclicProtoValue);
            }
            return FALSE;
        }

        // 7.   Set the value of the [[Prototype]] internal data property of O to V.
        // 8.   Return true.

        // Notify old prototypes that they are being removed from a prototype chain. This triggers invalidating protocache, etc.
        if (!JavascriptProxy::Is(object))
        {
            JavascriptOperators::MapObjectAndPrototypes<true>(object->GetPrototype(), [=](RecyclableObject* obj)
            {
                obj->RemoveFromPrototype(scriptContext);
            });
        }

        // Examine new prototype chain. If it brings in any non-WritableData property, we need to invalidate related caches.
        if (!JavascriptOperators::CheckIfObjectAndPrototypeChainHasOnlyWritableDataProperties(newPrototype))
        {
            // Invalidate fast prototype chain writable data test flag
            object->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();

            // Invalidate StoreField/PropertyGuards for any non-WritableData property in the new chain
            JavascriptOperators::MapObjectAndPrototypes<true>(newPrototype, [=](RecyclableObject* obj)
            {
                if (!obj->HasOnlyWritableDataProperties())
                {
                    obj->AddToPrototype(scriptContext);
                }
            });
        }

        // Set to new prototype
        if (object->IsExternal() || (DynamicType::Is(object->GetTypeId()) && (DynamicObject::FromVar(object))->IsCrossSiteObject()))
        {
            CrossSite::ForceCrossSiteThunkOnPrototypeChain(newPrototype);
        }
        object->SetPrototype(newPrototype);
        return TRUE;
    }

    Var JavascriptObject::EntryIsPrototypeOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        // no property specified
        if (args.Info.Count == 1 || !JavascriptOperators::IsObject(args[1]))
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        RecyclableObject* dynamicObject = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Object.prototype.isPrototypeOf");
        }
        RecyclableObject* value = RecyclableObject::FromVar(args[1]);

        if (dynamicObject->GetTypeId() == TypeIds_GlobalObject)
        {
            dynamicObject = RecyclableObject::FromVar(static_cast<Js::GlobalObject*>(dynamicObject)->ToThis());
        }

        while (JavascriptOperators::GetTypeId(value) != TypeIds_Null)
        {
            value = JavascriptOperators::GetPrototype(value);
            if (dynamicObject == value)
            {
                return scriptContext->GetLibrary()->GetTrue();
            }
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    // 19.1.3.5 - Object.prototype.toLocaleString as of ES6 (6.0)
    Var JavascriptObject::EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Assert(!(callInfo.Flags & CallFlags_New));
        AssertMsg(args.Info.Count, "Should always have implicit 'this'");

        Var thisValue = args[0];
        RecyclableObject* dynamicObject = nullptr;

        if (FALSE == JavascriptConversion::ToObject(thisValue, scriptContext, &dynamicObject))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Object.prototype.toLocaleString");
        }

        Var toStringVar = nullptr;
        if (!JavascriptOperators::GetProperty(thisValue, dynamicObject, Js::PropertyIds::toString, &toStringVar, scriptContext) || !JavascriptConversion::IsCallable(toStringVar))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"Object.prototype.toLocaleString");
        }

        RecyclableObject* toStringFunc = RecyclableObject::FromVar(toStringVar);
        return toStringFunc->GetEntryPoint()(toStringFunc, CallInfo(CallFlags_Value, 1), thisValue);
    }

    Var JavascriptObject::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count, "Should always have implicit 'this'");
        return ToStringHelper(args[0], scriptContext);
    }

    JavascriptString* JavascriptObject::ToStringTagHelper(Var thisArg, ScriptContext* scriptContext, TypeId type)
    {
        JavascriptString* tag = nullptr;
        bool addTilde = true;
        bool isES6ToStringTagEnabled = scriptContext->GetConfig()->IsES6ToStringTagEnabled();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        if (isES6ToStringTagEnabled && RecyclableObject::Is(thisArg))
        {
            RecyclableObject* thisArgObject = RecyclableObject::FromVar(thisArg);

            if (JavascriptOperators::HasProperty(thisArgObject, PropertyIds::_symbolToStringTag)) // Let hasTag be the result of HasProperty(O, @@toStringTag).
            {
                Var tagVar;

                try
                {
                    tagVar = JavascriptOperators::GetProperty(thisArgObject, PropertyIds::_symbolToStringTag, scriptContext); // Let tag be the result of Get(O, @@toStringTag).
                }
                catch (JavascriptExceptionObject*)
                {
                    // tag = "???"
                    return library->CreateStringFromCppLiteral(L"[object ???]"); // If tag is an abrupt completion, let tag be NormalCompletion("???").
                }

                if (!JavascriptString::Is(tagVar))
                {
                    // tag = "???"
                    return library->CreateStringFromCppLiteral(L"[object ???]"); // If Type(tag) is not String, let tag be "???".
                }

                tag = JavascriptString::FromVar(tagVar);
            }
        }


        // If tag is any of "Arguments", "Array", "Boolean", "Date", "Error", "Function", "Number", "RegExp", or "String" and
        // SameValue(tag, builtinTag) is false, then let tag be the string value "~" concatenated with the current value of tag.
        switch (type)
        {
            case TypeIds_Arguments:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Arguments") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object Arguments]");
                }
                break;
            case TypeIds_Array:
            case TypeIds_ES5Array:
            case TypeIds_NativeIntArray:
#if ENABLE_COPYONACCESS_ARRAY
            case TypeIds_CopyOnAccessNativeIntArray:
#endif
            case TypeIds_NativeFloatArray:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Array") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object Array]");
                }
                break;
            case TypeIds_Boolean:
            case TypeIds_BooleanObject:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Boolean") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object Boolean]");
                }
                break;
            case TypeIds_Date:
            case TypeIds_WinRTDate:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Date") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object Date]");
                }
                break;
            case TypeIds_Error:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Error") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object Error]");
                }
                break;

            case TypeIds_Function:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Function") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object Function]");
                }
                break;
            case TypeIds_Number:
            case TypeIds_Int64Number:
            case TypeIds_UInt64Number:
            case TypeIds_Integer:
            case TypeIds_NumberObject:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Number") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object Number]");
                }
                break;

            case TypeIds_RegEx:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"RegExp") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object RegExp]");
                }
                break;

            case TypeIds_String:
            case TypeIds_StringObject:
                if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"String") == 0)
                {
                    return library->CreateStringFromCppLiteral(L"[object String]");
                }
                break;

            case TypeIds_Proxy:
                if (JavascriptOperators::IsArray(JavascriptProxy::FromVar(thisArg)->GetTarget()))
                {
                    if (!isES6ToStringTagEnabled || tag == nullptr || wcscmp(tag->UnsafeGetBuffer(), L"Array") == 0)
                    {
                        return library->CreateStringFromCppLiteral(L"[object Array]");
                    }
                }
                //otherwise, fall though
            case TypeIds_Object:
            default:
                if (tag == nullptr)
                {
                    // Else, let builtinTag be "Object".
                    // If hasTag is false, then let tag be builtinTag.
                    return library->GetObjectDisplayString(); // "[object Object]"
                }
                addTilde = false;
                break;
        }

        Assert(tag != nullptr);
        Assert(isES6ToStringTagEnabled);

        CompoundString::Builder<32> stringBuilder(scriptContext);

        if (addTilde)
            stringBuilder.AppendChars(L"[object ~");
        else
            stringBuilder.AppendChars(L"[object ");

        stringBuilder.AppendChars(tag);
        stringBuilder.AppendChars(L']');

        return stringBuilder.ToString();
    }

    Var JavascriptObject::LegacyToStringHelper(ScriptContext* scriptContext, TypeId type)
    {
        JavascriptLibrary* library = scriptContext->GetLibrary();
        switch (type)
        {
            case TypeIds_ArrayBuffer:
                return library->CreateStringFromCppLiteral(L"[object ArrayBuffer]");
            case TypeIds_Int8Array:
                return library->CreateStringFromCppLiteral(L"[object Int8Array]");
            case TypeIds_Uint8Array:
                return library->CreateStringFromCppLiteral(L"[object Uint8Array]");
            case TypeIds_Uint8ClampedArray:
                return library->CreateStringFromCppLiteral(L"[object Uint8ClampedArray]");
            case TypeIds_Int16Array:
                return library->CreateStringFromCppLiteral(L"[object Int16Array]");
            case TypeIds_Uint16Array:
                return library->CreateStringFromCppLiteral(L"[object Uint16Array]");
            case TypeIds_Int32Array:
                return library->CreateStringFromCppLiteral(L"[object Int32Array]");
            case TypeIds_Uint32Array:
                return library->CreateStringFromCppLiteral(L"[object Uint32Array]");
            case TypeIds_Float32Array:
                return library->CreateStringFromCppLiteral(L"[object Float32Array]");
            case TypeIds_Float64Array:
                return library->CreateStringFromCppLiteral(L"[object Float64Array]");
            case TypeIds_Symbol:
            case TypeIds_SymbolObject:
                return library->CreateStringFromCppLiteral(L"[object Symbol]");
            case TypeIds_Map:
                return library->CreateStringFromCppLiteral(L"[object Map]");
            case TypeIds_Set:
                return library->CreateStringFromCppLiteral(L"[object Set]");
            case TypeIds_WeakMap:
                return library->CreateStringFromCppLiteral(L"[object WeakMap]");
            case TypeIds_WeakSet:
                return library->CreateStringFromCppLiteral(L"[object WeakSet]");
            case TypeIds_Generator:
                return library->CreateStringFromCppLiteral(L"[object Generator]");
            default:
                AssertMsg(false, "We should never be here");
                return library->GetUndefined();
        }
    }

    Var JavascriptObject::ToStringHelper(Var thisArg, ScriptContext* scriptContext)
    {
        TypeId type = JavascriptOperators::GetTypeId(thisArg);
        JavascriptLibrary* library = scriptContext->GetLibrary();
        switch (type)
        {
        case TypeIds_Undefined:
            return library->CreateStringFromCppLiteral(L"[object Undefined]");
        case TypeIds_Null:
            return library->CreateStringFromCppLiteral(L"[object Null]");
        case TypeIds_Enumerator:
        case TypeIds_Proxy:
        case TypeIds_Object:
            if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
            {
                // Math, Object and JSON handled by toStringTag now,
                return ToStringTagHelper(thisArg, scriptContext, type);
            }

            if (thisArg == scriptContext->GetLibrary()->GetMathObject())
            {
                return library->CreateStringFromCppLiteral(L"[object Math]");
            }
            else if (thisArg == library->GetJSONObject())
            {
                return library->CreateStringFromCppLiteral(L"[object JSON]");
            }

        default:
        {
            RecyclableObject* obj = RecyclableObject::FromVar(thisArg);
            if (!obj->CanHaveInterceptors())
            {
                //this will handle printing Object for non interceptor cases
                return library->GetObjectDisplayString();
            }
            // otherwise, fall through.
            RecyclableObject* recyclableObject = Js::RecyclableObject::FromVar(thisArg);

            JavascriptString* name = scriptContext->GetLibrary()->CreateStringFromCppLiteral(L"[object ");
            name = JavascriptString::Concat(name, recyclableObject->GetClassName(scriptContext));
            name = JavascriptString::Concat(name, scriptContext->GetLibrary()->CreateStringFromCppLiteral(L"]"));
            return name;
        }

        case TypeIds_HostObject:
            AssertMsg(false, "Host object should never be here");
            return library->GetUndefined();

        case TypeIds_StringIterator:
        case TypeIds_ArrayIterator:
        case TypeIds_MapIterator:
        case TypeIds_SetIterator:
        case TypeIds_DataView:
        case TypeIds_Promise:
        case TypeIds_Boolean:
        case TypeIds_BooleanObject:
        case TypeIds_Date:
        case TypeIds_WinRTDate:
        case TypeIds_Error:
        case TypeIds_Number:
        case TypeIds_Int64Number:
        case TypeIds_UInt64Number:
        case TypeIds_Integer:
        case TypeIds_NumberObject:
        case TypeIds_RegEx:
        case TypeIds_Array:
        case TypeIds_ES5Array:
        case TypeIds_NativeIntArray:
#if ENABLE_COPYONACCESS_ARRAY
        case TypeIds_CopyOnAccessNativeIntArray:
#endif
        case TypeIds_NativeFloatArray:
        case TypeIds_Function:
        case TypeIds_String:
        case TypeIds_StringObject:
        case TypeIds_Arguments:
            return ToStringTagHelper(thisArg, scriptContext, type);

        case TypeIds_GlobalObject:
            {
                GlobalObject* globalObject = static_cast<Js::GlobalObject*>(thisArg);
                AssertMsg(globalObject == thisArg, "Should be the global object");

                Var toThis = globalObject->ToThis();
                if (toThis == globalObject)
                {
                    return library->GetObjectDisplayString();
                }
                else
                {
                    return ToStringHelper(toThis, scriptContext);
                }
            }
        case TypeIds_HostDispatch:
            {
                RecyclableObject* hostDispatchObject = RecyclableObject::FromVar(thisArg);
                DynamicObject* remoteObject = hostDispatchObject->GetRemoteObject();
                if (remoteObject)
                {
                    return ToStringHelper(remoteObject, scriptContext);
                }
                else
                {
                    Var result;
                    Js::Var values[1];
                    Js::CallInfo info(Js::CallFlags_Value, 1);
                    Js::Arguments args(info, values);
                    values[0] = thisArg;
                    if (hostDispatchObject->InvokeBuiltInOperationRemotely(EntryToString, args, &result))
                    {
                        return result;
                    }
                }
                return library->GetObjectDisplayString();
            }

        case TypeIds_ArrayBuffer:
        case TypeIds_Int8Array:
        case TypeIds_Uint8Array:
        case TypeIds_Uint8ClampedArray:
        case TypeIds_Int16Array:
        case TypeIds_Uint16Array:
        case TypeIds_Int32Array:
        case TypeIds_Uint32Array:
        case TypeIds_Float32Array:
        case TypeIds_Float64Array:
        case TypeIds_Symbol:
        case TypeIds_SymbolObject:
        case TypeIds_Map:
        case TypeIds_Set:
        case TypeIds_WeakMap:
        case TypeIds_WeakSet:
        case TypeIds_Generator:
            if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
            {
                JavascriptString* toStringValue = nullptr;
                if (!scriptContext->GetThreadContext()->IsScriptActive())
                {
                    // Note we need this for typed Arrays in the debugger b/c they invoke a function call to get the toStringTag
                    BEGIN_JS_RUNTIME_CALL_EX(scriptContext, false);
                    toStringValue = ToStringTagHelper(thisArg, scriptContext, type);
                    END_JS_RUNTIME_CALL(scriptContext);
                }
                else
                {
                    toStringValue = ToStringTagHelper(thisArg, scriptContext, type);
                }
                return toStringValue;

            }
            else
            {
                return LegacyToStringHelper(scriptContext, type);
            }

        }
    }

    // -----------------------------------------------------------
    // Object.prototype.valueOf
    //    1.    Let O be the result of calling ToObject passing the this value as the argument.
    //    2.    If O is the result of calling the Object constructor with a host object (15.2.2.1), then
    //    a.    Return either O or another value such as the host object originally passed to the constructor. The specific result that is returned is implementation-defined.
    //    3.    Return O.
    // -----------------------------------------------------------

    Var JavascriptObject::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        TypeId argType = JavascriptOperators::GetTypeId(args[0]);

        // throw a TypeError if TypeId is null or undefined, and apply ToObject to the 'this' value otherwise.

        if ((argType == TypeIds_Null) || (argType == TypeIds_Undefined))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Object.prototype.valueOf");
        }
        else
        {
            return JavascriptOperators::ToObject(args[0], scriptContext);
        }
    }

    Var JavascriptObject::EntryGetOwnPropertyDescriptor(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        RecyclableObject* obj = nullptr;
        if (args.Info.Count < 2)
        {
            obj = RecyclableObject::FromVar(JavascriptOperators::ToObject(scriptContext->GetLibrary()->GetUndefined(), scriptContext));
        }
        else
        {
            // Convert the argument to object first
            obj = RecyclableObject::FromVar(JavascriptOperators::ToObject(args[1], scriptContext));
        }

        // If the object is HostDispatch try to invoke the operation remotely
        if (obj->GetTypeId() == TypeIds_HostDispatch)
        {
            Var result;
            if (obj->InvokeBuiltInOperationRemotely(EntryGetOwnPropertyDescriptor, args, &result))
            {
                return result;
            }
        }

        Var propertyKey = args.Info.Count > 2 ? args[2] : obj->GetLibrary()->GetUndefined();

        return JavascriptObject::GetOwnPropertyDescriptorHelper(obj, propertyKey, scriptContext);
    }

    Var JavascriptObject::GetOwnPropertyDescriptorHelper(RecyclableObject* obj, Var propertyKey, ScriptContext* scriptContext)
    {
        const PropertyRecord* propertyRecord;
        JavascriptConversion::ToPropertyKey(propertyKey, scriptContext, &propertyRecord);
        PropertyId propertyId = propertyRecord->GetPropertyId();

        obj->ThrowIfCannotGetOwnPropertyDescriptor(propertyId);

        PropertyDescriptor propertyDescriptor;
        BOOL isPropertyDescriptorDefined;
        isPropertyDescriptorDefined = JavascriptObject::GetOwnPropertyDescriptorHelper(obj, propertyId, scriptContext, propertyDescriptor);
        if (!isPropertyDescriptorDefined)
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }

        return JavascriptOperators::FromPropertyDescriptor(propertyDescriptor, scriptContext);
    }

    __inline BOOL JavascriptObject::GetOwnPropertyDescriptorHelper(RecyclableObject* obj, PropertyId propertyId, ScriptContext* scriptContext, PropertyDescriptor& propertyDescriptor)
    {
        BOOL isPropertyDescriptorDefined;
        if (obj->CanHaveInterceptors())
        {
            isPropertyDescriptorDefined = obj->HasOwnProperty(propertyId) ?
                JavascriptOperators::GetOwnPropertyDescriptor(obj, propertyId, scriptContext, &propertyDescriptor) : obj->GetDefaultPropertyDescriptor(propertyDescriptor);
        }
        else
        {
            isPropertyDescriptorDefined = JavascriptOperators::GetOwnPropertyDescriptor(obj, propertyId, scriptContext, &propertyDescriptor) ||
                obj->GetDefaultPropertyDescriptor(propertyDescriptor);
        }
        return isPropertyDescriptorDefined;
    }

    Var JavascriptObject::EntryGetPrototypeOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectGetPrototypeOfCount);

        // 19.1.2.9
        // Object.getPrototypeOf ( O )
        // When the getPrototypeOf function is called with argument O, the following steps are taken:
        RecyclableObject *object = nullptr;

        // 1. Let obj be ToObject(O).
        // 2. ReturnIfAbrupt(obj).
        if (args.Info.Count < 2 || !JavascriptConversion::ToObject(args[1], scriptContext, &object))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Object.getPrototypeOf");
        }

        // 3. Return obj.[[GetPrototypeOf]]().
        return CrossSite::MarshalVar(scriptContext, GetPrototypeOf(object, scriptContext));
    }

    Var JavascriptObject::EntrySetPrototypeOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));
        ScriptContext* scriptContext = function->GetScriptContext();

        // 19.1.2.18
        // Object.setPrototypeOf ( O, proto )
        // When the setPrototypeOf function is called with arguments O and proto, the following steps are taken:
        // 1. Let O be RequireObjectCoercible(O).
        // 2. ReturnIfAbrupt(O).
        // 3. If Type(proto) is neither Object or Null, then throw a TypeError exception.
        long errCode = NOERROR;

        if (args.Info.Count < 2 || !JavascriptConversion::CheckObjectCoercible(args[1], scriptContext))
        {
            errCode = JSERR_FunctionArgument_NeedObject;
        }
        else if (args.Info.Count < 3 || !JavascriptOperators::IsObjectOrNull(args[2]))
        {
            errCode = JSERR_FunctionArgument_NotObjectOrNull;
        }

        if (errCode != NOERROR)
        {
            JavascriptError::ThrowTypeError(scriptContext, errCode, L"Object.setPrototypeOf");
        }

        // 4. If Type(O) is not Object, return O.
        if (!JavascriptOperators::IsObject(args[1]))
        {
            return args[1];
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[1]);
#endif
        RecyclableObject* object = RecyclableObject::FromVar(args[1]);
        RecyclableObject* newPrototype = RecyclableObject::FromVar(args[2]);

        // 5. Let status be O.[[SetPrototypeOf]](proto).
        // 6. ReturnIfAbrupt(status).
        // 7. If status is false, throw a TypeError exception.
        ChangePrototype(object, newPrototype, /*shouldThrow*/true, scriptContext);

        // 8. Return O.
        return object;
    }

    Var JavascriptObject::EntrySeal(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectSealCount);

        // Spec update in Rev29 under section 19.1.2.17
        if (args.Info.Count < 2)
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }
        else if (!JavascriptOperators::IsObject(args[1]))
        {
            return args[1];
        }


        RecyclableObject *object = RecyclableObject::FromVar(args[1]);

        GlobalObject* globalObject = object->GetLibrary()->GetGlobalObject();
        if (globalObject != object && globalObject && (globalObject->ToThis() == object))
        {
            globalObject->Seal();
        }

        object->Seal();
        return object;
    }

    Var JavascriptObject::EntryFreeze(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectFreezeCount);

        // Spec update in Rev29 under section 19.1.2.5
        if (args.Info.Count < 2)
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }
        else if (!JavascriptOperators::IsObject(args[1]))
        {
            return args[1];
        }

        RecyclableObject *object = RecyclableObject::FromVar(args[1]);

        GlobalObject* globalObject = object->GetLibrary()->GetGlobalObject();
        if (globalObject != object && globalObject && (globalObject->ToThis() == object))
        {
            globalObject->Freeze();
        }

        object->Freeze();
        return object;
    }

    Var JavascriptObject::EntryPreventExtensions(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectPreventExtensionCount);

        // Spec update in Rev29 under section 19.1.2.15
        if (args.Info.Count < 2)
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }
        else if (!JavascriptOperators::IsObject(args[1]))
        {
            return args[1];
        }

        RecyclableObject *object = RecyclableObject::FromVar(args[1]);

        GlobalObject* globalObject = object->GetLibrary()->GetGlobalObject();
        if (globalObject != object && globalObject && (globalObject->ToThis() == object))
        {
            globalObject->PreventExtensions();
        }

        object->PreventExtensions();

        return object;
    }

    Var JavascriptObject::EntryIsSealed(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectIsSealedCount);

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            return scriptContext->GetLibrary()->GetTrue();
        }

        RecyclableObject *object = RecyclableObject::FromVar(args[1]);

        BOOL isSealed = object->IsSealed();

        GlobalObject* globalObject = object->GetLibrary()->GetGlobalObject();
        if (isSealed && globalObject != object && globalObject && (globalObject->ToThis() == object))
        {
            isSealed = globalObject->IsSealed();
        }

        return scriptContext->GetLibrary()->GetTrueOrFalse(isSealed);
    }

    Var JavascriptObject::EntryIsFrozen(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectIsFrozenCount);

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            return scriptContext->GetLibrary()->GetTrue();
        }

        RecyclableObject *object = RecyclableObject::FromVar(args[1]);

        BOOL isFrozen = object->IsFrozen();

        GlobalObject* globalObject = object->GetLibrary()->GetGlobalObject();
        if (isFrozen && globalObject != object && globalObject && (globalObject->ToThis() == object))
        {
            isFrozen = globalObject->IsFrozen();
        }

        return scriptContext->GetLibrary()->GetTrueOrFalse(isFrozen);
    }

    Var JavascriptObject::EntryIsExtensible(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectIsExtensibleCount);

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        RecyclableObject *object = RecyclableObject::FromVar(args[1]);
        BOOL isExtensible = object->IsExtensible();

        GlobalObject* globalObject = object->GetLibrary()->GetGlobalObject();
        if (isExtensible && globalObject != object && globalObject && (globalObject->ToThis() == object))
        {
            isExtensible = globalObject->IsExtensible();
        }

        return scriptContext->GetLibrary()->GetTrueOrFalse(isExtensible);
    }

    Var JavascriptObject::EntryGetOwnPropertyNames(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectGetOwnPropertyNamesCount);

        Var tempVar = args.Info.Count < 2 ? scriptContext->GetLibrary()->GetUndefined() : args[1];
        RecyclableObject *object = RecyclableObject::FromVar(JavascriptOperators::ToObject(tempVar, scriptContext));

        if (object->GetTypeId() == TypeIds_HostDispatch)
        {
            Var result;
            if (object->InvokeBuiltInOperationRemotely(EntryGetOwnPropertyNames, args, &result))
            {
                return result;
            }
        }

        return JavascriptOperators::GetOwnPropertyNames(object, scriptContext);
    }

    Var JavascriptObject::EntryGetOwnPropertySymbols(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var tempVar = args.Info.Count < 2 ? scriptContext->GetLibrary()->GetUndefined() : args[1];
        RecyclableObject *object = RecyclableObject::FromVar(JavascriptOperators::ToObject(tempVar, scriptContext));

        if (object->GetTypeId() == TypeIds_HostDispatch)
        {
            Var result;
            if (object->InvokeBuiltInOperationRemotely(EntryGetOwnPropertySymbols, args, &result))
            {
                return result;
            }
        }

        return JavascriptOperators::GetOwnPropertySymbols(object, scriptContext);
    }

    Var JavascriptObject::EntryKeys(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectKeysCount);

        Var tempVar = args.Info.Count < 2 ? scriptContext->GetLibrary()->GetUndefined() : args[1];
        RecyclableObject *object = RecyclableObject::FromVar(JavascriptOperators::ToObject(tempVar, scriptContext));

        if (object->GetTypeId() == TypeIds_HostDispatch)
        {
            Var result;
            if (object->InvokeBuiltInOperationRemotely(EntryKeys, args, &result))
            {
                return result;
            }
        }
        return JavascriptOperators::GetOwnEnumerablePropertyNames(object, scriptContext);
    }

    Var JavascriptObject::CreateOwnSymbolPropertiesHelper(RecyclableObject* object, ScriptContext* scriptContext)
    {
        return CreateKeysHelper(object, scriptContext, TRUE, true /*includeSymbolsOnly */, false, true /*includeSpecialProperties*/);
    }

    Var JavascriptObject::CreateOwnStringPropertiesHelper(RecyclableObject* object, ScriptContext* scriptContext)
    {
        return CreateKeysHelper(object, scriptContext, TRUE, false, true /*includeStringsOnly*/, true /*includeSpecialProperties*/);
    }

    Var JavascriptObject::CreateOwnStringSymbolPropertiesHelper(RecyclableObject* object, ScriptContext* scriptContext)
    {
        return CreateKeysHelper(object, scriptContext, TRUE, true/*includeSymbolsOnly*/, true /*includeStringsOnly*/, true /*includeSpecialProperties*/);
    }

    Var JavascriptObject::CreateOwnEnumerableStringPropertiesHelper(RecyclableObject* object, ScriptContext* scriptContext)
    {
        return CreateKeysHelper(object, scriptContext, FALSE, false, true/*includeStringsOnly*/, false);
    }

    Var JavascriptObject::CreateOwnEnumerableStringSymbolPropertiesHelper(RecyclableObject* object, ScriptContext* scriptContext)
    {
        return CreateKeysHelper(object, scriptContext, FALSE, true/*includeSymbolsOnly*/, true/*includeStringsOnly*/, false);
    }

    // 9.1.12 [[OwnPropertyKeys]] () in RC#4 dated April 3rd 2015.
    Var JavascriptObject::CreateKeysHelper(RecyclableObject* object, ScriptContext* scriptContext, BOOL includeNonEnumerable, bool includeSymbolProperties, bool includeStringProperties, bool includeSpecialProperties)
    {
        //1. Let keys be a new empty List.
        //2. For each own property key P of O that is an integer index, in ascending numeric index order
        //      a. Add P as the last element of keys.
        //3. For each own property key P of O that is a String but is not an integer index, in property creation order
        //      a. Add P as the last element of keys.
        //4. For each own property key P of O that is a Symbol, in property creation order
        //      a. Add P as the last element of keys.
        //5. Return keys.
        AssertMsg(includeStringProperties || includeSymbolProperties, "Should either get string or symbol properties.");

        Var enumeratorVar;
        JavascriptArray* newArr = scriptContext->GetLibrary()->CreateArray(0);
        JavascriptArray* newArrForSymbols = scriptContext->GetLibrary()->CreateArray(0);

        if (!object->GetEnumerator(includeNonEnumerable, &enumeratorVar, scriptContext, false, includeSymbolProperties))
        {
            return newArr;  // Return an empty array if we don't have an enumerator
        }

        JavascriptEnumerator *pEnumerator = JavascriptEnumerator::FromVar(enumeratorVar);
        RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();
        Var propertyName = nullptr;
        PropertyId propertyId;
        uint32 propertyIndex = 0;
        uint32 symbolIndex = 0;
        const PropertyRecord* propertyRecord;
        JavascriptSymbol* symbol;

        while ((propertyName = pEnumerator->GetCurrentAndMoveNext(propertyId)) != NULL)
        {
            if (!JavascriptOperators::IsUndefinedObject(propertyName, undefined)) //There are some code paths in which GetCurrentIndex can return undefined
            {
                if (includeSymbolProperties)
                {
                    propertyRecord = scriptContext->GetPropertyName(propertyId);

                    if (propertyRecord->IsSymbol())
                    {
                        symbol = scriptContext->GetLibrary()->CreateSymbol(propertyRecord);
                        newArrForSymbols->DirectSetItemAt(symbolIndex++, CrossSite::MarshalVar(scriptContext, symbol));
                        continue;
                    }
                }
                if (includeStringProperties)
                {
                    newArr->DirectSetItemAt(propertyIndex++, CrossSite::MarshalVar(scriptContext, propertyName));
                }
            }
        }

        // Special properties
        if (includeSpecialProperties && includeStringProperties)
        {
            uint32 index = 0;
            while (object->GetSpecialPropertyName(index, &propertyName, scriptContext))
            {
                if (!JavascriptOperators::IsUndefinedObject(propertyName, undefined))
                {
                    newArr->DirectSetItemAt(propertyIndex++, propertyName);
                }
                index++;
            }
        }

        // Append all the symbols at the end of list
        uint32 totalSymbols = newArrForSymbols->GetLength();
        for (uint32 symIndex = 0; symIndex < totalSymbols; symIndex++)
        {
            newArr->DirectSetItemAt(propertyIndex++, newArrForSymbols->DirectGetItem(symIndex));
        }

        return newArr;
    }

    // args[1] this object to operate on.
    // args[2] property name.
    // args[3] object that attributes for the new descriptor.
    Var JavascriptObject::EntryDefineProperty(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Object.defineProperty");
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[1]);
#endif
        RecyclableObject* obj = RecyclableObject::FromVar(args[1]);

        // If the object is HostDispatch try to invoke the operation remotely
        if (obj->GetTypeId() == TypeIds_HostDispatch)
        {
            if (obj->InvokeBuiltInOperationRemotely(EntryDefineProperty, args, NULL))
            {
                return obj;
            }
        }

        Var propertyKey = args.Info.Count > 2 ? args[2] : obj->GetLibrary()->GetUndefined();
        PropertyRecord const * propertyRecord;
        JavascriptConversion::ToPropertyKey(propertyKey, scriptContext, &propertyRecord);

        Var descVar = args.Info.Count > 3 ? args[3] : obj->GetLibrary()->GetUndefined();
        PropertyDescriptor propertyDescriptor;
        if (!JavascriptOperators::ToPropertyDescriptor(descVar, &propertyDescriptor, scriptContext))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_PropertyDescriptor_Invalid, scriptContext->GetPropertyName(propertyRecord->GetPropertyId())->GetBuffer());
        }

        if (CONFIG_FLAG(UseFullName))
        {
            ModifyGetterSetterFuncName(propertyRecord, propertyDescriptor, scriptContext);
        }

        DefineOwnPropertyHelper(obj, propertyRecord->GetPropertyId(), propertyDescriptor, scriptContext);

        return obj;
    }

    Var JavascriptObject::EntryDefineProperties(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectDefinePropertiesCount);

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Object.defineProperties");
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[1]);
#endif
        RecyclableObject *object = RecyclableObject::FromVar(args[1]);

        // If the object is HostDispatch try to invoke the operation remotely
        if (object->GetTypeId() == TypeIds_HostDispatch)
        {
            if (object->InvokeBuiltInOperationRemotely(EntryDefineProperties, args, NULL))
            {
                return object;
            }
        }

        Var propertiesVar = args.Info.Count > 2 ? args[2] : object->GetLibrary()->GetUndefined();
        RecyclableObject* properties = nullptr;
        if (FALSE == JavascriptConversion::ToObject(propertiesVar, scriptContext, &properties))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NullOrUndefined, L"Object.defineProperties");
        }

        return DefinePropertiesHelper(object, properties, scriptContext);
    }

    // args[1] property name.
    // args[2] function object to use as the getter function.
    Var JavascriptObject::EntryDefineGetter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // For browser interop, simulate LdThis by calling OP implementation directly.
        // Do not have module id here so use the global id, 0.
        //
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
        Var thisArg = JavascriptOperators::OP_GetThisNoFastPath(args[0], 0, scriptContext);
        RecyclableObject* obj = RecyclableObject::FromVar(thisArg);

        Var propertyKey = args.Info.Count > 1 ? args[1] : obj->GetLibrary()->GetUndefined();
        const PropertyRecord* propertyRecord;
        JavascriptConversion::ToPropertyKey(propertyKey, scriptContext, &propertyRecord);

        Var getterFunc = args.Info.Count > 2 ? args[2] : obj->GetLibrary()->GetUndefined();

        if (!JavascriptConversion::IsCallable(getterFunc))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"Object.prototype.__defineGetter__");
        }

        PropertyDescriptor propertyDescriptor;
        propertyDescriptor.SetEnumerable(true);
        propertyDescriptor.SetConfigurable(true);
        propertyDescriptor.SetGetter(getterFunc);

        DefineOwnPropertyHelper(obj, propertyRecord->GetPropertyId(), propertyDescriptor, scriptContext);

        return obj->GetLibrary()->GetUndefined();
    }

    // args[1] property name.
    // args[2] function object to use as the setter function.
    Var JavascriptObject::EntryDefineSetter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // For browser interop, simulate LdThis by calling OP implementation directly.
        // Do not have module id here so use the global id, 0.
        //
        Var thisArg = JavascriptOperators::OP_GetThisNoFastPath(args[0], 0, scriptContext);
        RecyclableObject* obj = RecyclableObject::FromVar(thisArg);

        Var propertyKey = args.Info.Count > 1 ? args[1] : obj->GetLibrary()->GetUndefined();
        const PropertyRecord* propertyRecord;
        JavascriptConversion::ToPropertyKey(propertyKey, scriptContext, &propertyRecord);

        Var setterFunc = args.Info.Count > 2 ? args[2] : obj->GetLibrary()->GetUndefined();

        if (!JavascriptConversion::IsCallable(setterFunc))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, L"Object.prototype.__defineSetter__");
        }

        PropertyDescriptor propertyDescriptor;
        propertyDescriptor.SetEnumerable(true);
        propertyDescriptor.SetConfigurable(true);
        propertyDescriptor.SetSetter(setterFunc);

        DefineOwnPropertyHelper(obj, propertyRecord->GetPropertyId(), propertyDescriptor, scriptContext);

        return obj->GetLibrary()->GetUndefined();
    }

    // args[1] property name.
    Var JavascriptObject::EntryLookupGetter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        RecyclableObject* obj = nullptr;
        if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Object.prototype.__lookupGetter__");
        }

        Var propertyKey = args.Info.Count > 1 ? args[1] : obj->GetLibrary()->GetUndefined();
        const PropertyRecord* propertyRecord;
        JavascriptConversion::ToPropertyKey(propertyKey, scriptContext, &propertyRecord);

        Var getter = nullptr;
        Var unused = nullptr;
        if (JavascriptOperators::GetAccessors(obj, propertyRecord->GetPropertyId(), scriptContext, &getter, &unused))
        {
            if (getter != nullptr)
            {
                return getter;
            }
        }

        return obj->GetLibrary()->GetUndefined();
    }

    // args[1] property name.
    Var JavascriptObject::EntryLookupSetter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        RecyclableObject* obj = nullptr;
        if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Object.prototype.__lookupSetter__");
        }

        Var propertyKey = args.Info.Count > 1 ? args[1] : obj->GetLibrary()->GetUndefined();
        const PropertyRecord* propertyRecord;
        JavascriptConversion::ToPropertyKey(propertyKey, scriptContext, &propertyRecord);

        Var unused = nullptr;
        Var setter = nullptr;
        if (JavascriptOperators::GetAccessors(obj, propertyRecord->GetPropertyId(), scriptContext, &unused, &setter))
        {
            if (setter != nullptr)
            {
                return setter;
            }
        }

        return obj->GetLibrary()->GetUndefined();
    }

    Var JavascriptObject::EntryIs(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var x = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        Var y = args.Info.Count > 2 ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        return JavascriptBoolean::ToVar(JavascriptConversion::SameValue(x, y), scriptContext);
    }

    //ES6 19.1.2.1
    Var JavascriptObject::EntryAssign(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // 1. Let to be ToObject(target).
        // 2. ReturnIfAbrupt(to).
        // 3  If only one argument was passed, return to.
        RecyclableObject* to = nullptr;
        if (args.Info.Count == 1 || !JavascriptConversion::ToObject(args[1], scriptContext, &to))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Object.assign");
        }

        if (args.Info.Count < 3)
        {
            return to;
        }

        // 4. Let sources be the List of argument values starting with the second argument.
        // 5. For each element nextSource of sources, in ascending index order,
        for (unsigned int i = 2; i < args.Info.Count; i++)
        {
            //      a. If nextSource is undefined or null, let keys be an empty List.
            //      b. Else,
            //          i.Let from be ToObject(nextSource).
            //          ii.ReturnIfAbrupt(from).
            //          iii.Let keys be from.[[OwnPropertyKeys]]().
            //          iv.ReturnIfAbrupt(keys).
            if (JavascriptOperators::IsUndefinedOrNullType(JavascriptOperators::GetTypeId(args[i])))
            {
                continue;
            }

            RecyclableObject* from = nullptr;
            if (!JavascriptConversion::ToObject(args[i], scriptContext, &from))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"Object.assign");
            }

#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(from);
#endif

            // if proxy, take slow path by calling [[OwnPropertyKeys]] on source
            if (JavascriptProxy::Is(from))
            {
                AssignForProxyObjects(from, to, scriptContext);
            }
            // else use enumerator to extract keys from source
            else
            {
                AssignForGenericObjects(from, to, scriptContext);
            }
        }

        // 6. Return to.
        return to;
    }

    void JavascriptObject::AssignForGenericObjects(RecyclableObject* from, RecyclableObject* to, ScriptContext* scriptContext)
    {
        Var enumeratorVar = nullptr;
        if (!from->GetEnumerator(FALSE /*only enumerable properties*/, &enumeratorVar, scriptContext, true, true))
        {
            //nothing to enumerate, continue with the nextSource.
            return;
        }

        JavascriptEnumerator *pEnumerator = JavascriptEnumerator::FromVar(enumeratorVar);
        PropertyId nextKey = Constants::NoProperty;
        Var propValue = nullptr;
        Var propertyVar = nullptr;

        //enumerate through each property of properties and fetch the property descriptor
        while ((propertyVar = pEnumerator->GetCurrentAndMoveNext(nextKey)) != NULL)
        {
            if (nextKey == Constants::NoProperty)
            {
                if (JavascriptOperators::IsUndefinedObject(propertyVar)) //There are some code paths in which GetCurrentIndex can return undefined
                {
                    continue;
                }

                PropertyRecord const * propertyRecord = nullptr;
                JavascriptString* propertyName = JavascriptString::FromVar(propertyVar);

                scriptContext->GetOrAddPropertyRecord(propertyName->GetString(), propertyName->GetLength(), &propertyRecord);
                nextKey = propertyRecord->GetPropertyId();
            }

            if (!JavascriptOperators::GetOwnProperty(from, nextKey, &propValue, scriptContext))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedObject, L"Object.assign");
            }

            if (!JavascriptOperators::SetProperty(to, to, nextKey, propValue, scriptContext, PropertyOperationFlags::PropertyOperation_ThrowIfNonWritable))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedObject, L"Object.assign");
            }
        }
    }

    void JavascriptObject::AssignForProxyObjects(RecyclableObject* from, RecyclableObject* to, ScriptContext* scriptContext)
    {
        Var keysResult = JavascriptOperators::GetOwnEnumerablePropertyNamesSymbols(from, scriptContext);
        JavascriptArray *keys;

        if (JavascriptArray::Is(keysResult))
        {
            keys = JavascriptArray::FromVar(keysResult);
        }
        else
        {
            return;
        }
        //      c. Repeat for each element nextKey of keys in List order,
        //          i. Let desc be from.[[GetOwnProperty]](nextKey).
        //          ii. ReturnIfAbrupt(desc).
        //          iii. if desc is not undefined and desc.[[Enumerable]] is true, then
        //              1. Let propValue be Get(from, nextKey).
        //              2. ReturnIfAbrupt(propValue).
        //              3. Let status be Set(to, nextKey, propValue, true);
        //              4. ReturnIfAbrupt(status).
        uint32 length = keys->GetLength();
        Var nextKey;
        const PropertyRecord* propertyRecord = nullptr;
        PropertyId propertyId;
        Var propValue = nullptr;
        for (uint32 j = 0; j < length; j++)
        {
            PropertyDescriptor propertyDescriptor;
            nextKey = keys->DirectGetItem(j);
            AssertMsg(JavascriptSymbol::Is(nextKey) || JavascriptString::Is(nextKey), "Invariant check during ownKeys proxy trap should make sure we only get property key here. (symbol or string primitives)");
            // Spec doesn't strictly call for us to use ToPropertyKey but since we know nextKey is already a symbol or string primitive, ToPropertyKey will be a nop and return us the propertyRecord
            JavascriptConversion::ToPropertyKey(nextKey, scriptContext, &propertyRecord);
            propertyId = propertyRecord->GetPropertyId();
            AssertMsg(propertyId != Constants::NoProperty, "AssignForProxyObjects - OwnPropertyKeys returned a propertyId with value NoPrpoerty.");
            if (JavascriptOperators::GetOwnPropertyDescriptor(from, propertyRecord->GetPropertyId(), scriptContext, &propertyDescriptor))
            {
                if (propertyDescriptor.IsEnumerable())
                {
                    if (!JavascriptOperators::GetOwnProperty(from, propertyId, &propValue, scriptContext))
                    {
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedObject, L"Object.assign");
                    }
                    if (!JavascriptOperators::SetProperty(to, to, propertyId, propValue, scriptContext))
                    {
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedObject, L"Object.assign");
                    }
                }
            }
        }
    }

    //ES5 15.2.3.5
    Var JavascriptObject::EntryCreate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Recycler *recycler = scriptContext->GetRecycler();


        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ObjectCreateCount);

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NotObjectOrNull, L"Object.create");
        }

        TypeId typeId = JavascriptOperators::GetTypeId(args[1]);
        if (typeId != TypeIds_Null && !JavascriptOperators::IsObjectType(typeId))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NotObjectOrNull, L"Object.create");
        }

        //Create a new DynamicType with first argument as prototype and non shared type
        RecyclableObject *prototype = RecyclableObject::FromVar(args[1]);
        DynamicType *objectType = DynamicType::New(scriptContext, TypeIds_Object, prototype, nullptr, NullTypeHandler<false>::GetDefaultInstance(), false);

        //Create a new Object using this type.
        DynamicObject* object = DynamicObject::New(recycler, objectType);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(object));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {
            object = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(object));
        }
#endif

        if (args.Info.Count > 2 && JavascriptOperators::GetTypeId(args[2]) != TypeIds_Undefined)
        {
            RecyclableObject* properties = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[2], scriptContext, &properties))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NullOrUndefined, L"Object.create");
            }
            return DefinePropertiesHelper(object, properties, scriptContext);
        }
        return object;
    }

    Var JavascriptObject::DefinePropertiesHelper(RecyclableObject *object, RecyclableObject* props, ScriptContext *scriptContext)
    {
        if (JavascriptProxy::Is(props))
        {
            return DefinePropertiesHelperForProxyObjects(object, props, scriptContext);
        }
        else
        {
            return DefinePropertiesHelperForGenericObjects(object, props, scriptContext);
        }
    }


    Var JavascriptObject::DefinePropertiesHelperForGenericObjects(RecyclableObject *object, RecyclableObject* props, ScriptContext *scriptContext)
    {
        size_t descSize = 16;
        size_t descCount = 0;
        struct DescriptorMap
        {
            PropertyRecord const * propRecord;
            PropertyDescriptor descriptor;
            Var originalVar;
        };

        Var tempVar = nullptr;
        if (!props->GetEnumerator(FALSE, &tempVar, scriptContext, false, true))
        {
            return object;
        }

        JavascriptEnumerator *pEnumerator = JavascriptEnumerator::FromVar(tempVar);

        ENTER_PINNED_SCOPE(DescriptorMap, descriptors);
        descriptors = RecyclerNewArray(scriptContext->GetRecycler(), DescriptorMap, descSize);

        PropertyId propId;
        PropertyRecord const * propertyRecord;
        JavascriptString* propertyName = nullptr;
        RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

        //enumerate through each property of properties and fetch the property descriptor
        while ((tempVar = pEnumerator->GetCurrentAndMoveNext(propId)) != NULL)
        {
            if (propId == Constants::NoProperty) //try current property id query first
            {
                if (!JavascriptOperators::IsUndefinedObject(tempVar, undefined)) //There are some enumerators returning propertyName but not propId
                {
                    propertyName = JavascriptString::FromVar(tempVar);
                    scriptContext->GetOrAddPropertyRecord(propertyName->GetString(), propertyName->GetLength(), &propertyRecord);
                    propId = propertyRecord->GetPropertyId();
                }
                else
                {
                    continue;
                }
            }
            else
            {
                propertyRecord = scriptContext->GetPropertyName(propId);
            }

            if (descCount == descSize)
            {
                //reallocate - consider linked list of DescriptorMap if the descSize is too high
                descSize = AllocSizeMath::Mul(descCount, 2);
                __analysis_assume(descSize == descCount * 2);
                DescriptorMap *temp = RecyclerNewArray(scriptContext->GetRecycler(), DescriptorMap, descSize);

                for (size_t i = 0; i < descCount; i++)
                {
                    temp[i] = descriptors[i];
                }
                descriptors = temp;
            }

            tempVar = JavascriptOperators::GetProperty(props, propId, scriptContext);

            if (!JavascriptOperators::ToPropertyDescriptor(tempVar, &descriptors[descCount].descriptor, scriptContext))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_PropertyDescriptor_Invalid, scriptContext->GetPropertyName(propId)->GetBuffer());
            }
            // In proxy, we need to get back the original ToPropertDescriptor var in [[defineProperty]] trap.
            descriptors[descCount].originalVar = tempVar;

            if (CONFIG_FLAG(UseFullName))
            {
                ModifyGetterSetterFuncName(propertyRecord, descriptors[descCount].descriptor, scriptContext);
            }

            descriptors[descCount].propRecord = propertyRecord;

            descCount++;
        }

        //Once all the property descriptors are in place set each property descriptor to the object
        for (size_t i = 0; i < descCount; i++)
        {
            DefineOwnPropertyHelper(object, descriptors[i].propRecord->GetPropertyId(), descriptors[i].descriptor, scriptContext);
        }

        LEAVE_PINNED_SCOPE();

        return object;
    }

    //ES5 15.2.3.7
    Var JavascriptObject::DefinePropertiesHelperForProxyObjects(RecyclableObject *object, RecyclableObject* props, ScriptContext *scriptContext)
    {
        Assert(JavascriptProxy::Is(props));

        //1.  If Type(O) is not Object throw a TypeError exception.
        //2.  Let props be ToObject(Properties).

        size_t descCount = 0;
        struct DescriptorMap
        {
            PropertyRecord const * propRecord;
            PropertyDescriptor descriptor;
        };

        //3.  Let keys be props.[[OwnPropertyKeys]]().
        //4.  ReturnIfAbrupt(keys).
        //5.  Let descriptors be an empty List.
        JavascriptArray* keys;
        Var ownKeysResult = JavascriptOperators::GetOwnEnumerablePropertyNamesSymbols(props, scriptContext);
        if (JavascriptArray::Is(ownKeysResult))
        {
            keys = JavascriptArray::FromVar(ownKeysResult);
        }
        else
        {
            return object;
        }
        uint32 length = keys->GetLength();

        ENTER_PINNED_SCOPE(DescriptorMap, descriptors);
        descriptors = RecyclerNewArray(scriptContext->GetRecycler(), DescriptorMap, length);

        //6.  Repeat for each element nextKey of keys in List order,
        //    1.  Let propDesc be props.[[GetOwnProperty]](nextKey).
        //    2.  ReturnIfAbrupt(propDesc).
        //    3.  If propDesc is not undefined and propDesc.[[Enumerable]] is true, then
        //        1.  Let descObj be Get(props, nextKey).
        //        2.  ReturnIfAbrupt(descObj).
        //        3.  Let desc be ToPropertyDescriptor(descObj).
        //        4.  ReturnIfAbrupt(desc).
        //        5.  Append the pair(a two element List) consisting of nextKey and desc to the end of descriptors.
        Var nextKey;

        const PropertyRecord* propertyRecord = nullptr;
        PropertyId propertyId;
        Var descObj;
        for (uint32 j = 0; j < length; j++)
        {
            PropertyDescriptor propertyDescriptor;
            nextKey = keys->DirectGetItem(j);
            AssertMsg(JavascriptSymbol::Is(nextKey) || JavascriptString::Is(nextKey), "Invariant check during ownKeys proxy trap should make sure we only get property key here. (symbol or string primitives)");
            JavascriptConversion::ToPropertyKey(nextKey, scriptContext, &propertyRecord);
            propertyId = propertyRecord->GetPropertyId();
            AssertMsg(propertyId != Constants::NoProperty, "DefinePropertiesHelper - OwnPropertyKeys returned a propertyId with value NoPrpoerty.");

            if (JavascriptOperators::GetOwnPropertyDescriptor(props, propertyRecord->GetPropertyId(), scriptContext, &propertyDescriptor))
            {
                if (propertyDescriptor.IsEnumerable())
                {
                    descObj = JavascriptOperators::GetProperty(props, propertyId, scriptContext);

                    if (!JavascriptOperators::ToPropertyDescriptor(descObj, &descriptors[descCount].descriptor, scriptContext))
                    {
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_PropertyDescriptor_Invalid, scriptContext->GetPropertyName(propertyId)->GetBuffer());
                    }

                    if (CONFIG_FLAG(UseFullName))
                    {
                        ModifyGetterSetterFuncName(propertyRecord, descriptors[descCount].descriptor, scriptContext);
                    }

                    descriptors[descCount].propRecord = propertyRecord;

                    descCount++;
                }
            }
        }

        //7.  For each pair from descriptors in list order,
        //    1.  Let P be the first element of pair.
        //    2.  Let desc be the second element of pair.
        //    3.  Let status be DefinePropertyOrThrow(O, P, desc).
        //    4.  ReturnIfAbrupt(status).

        for (size_t i = 0; i < descCount; i++)
        {
            DefineOwnPropertyHelper(object, descriptors[i].propRecord->GetPropertyId(), descriptors[i].descriptor, scriptContext);
        }

        LEAVE_PINNED_SCOPE();

        //8.  Return O.
        return object;
    }

    Var JavascriptObject::GetPrototypeOf(RecyclableObject* obj, ScriptContext* scriptContext)
    {
        return obj->IsExternal() ? obj->GetConfigurablePrototype(scriptContext) : obj->GetPrototype();
    }

    //
    // Check if "proto" is a prototype of "object" (on its prototype chain).
    //
    bool JavascriptObject::IsPrototypeOf(RecyclableObject* proto, RecyclableObject* object, ScriptContext* scriptContext)
    {
        return JavascriptOperators::MapObjectAndPrototypesUntil<false>(object, [=](RecyclableObject* obj)
        {
            return obj == proto;
        });
    }

    static const size_t ConstructNameGetSetLength = 5;    // 5 = 1 ( for .) + 3 (get or set) + 1 for null)

    /*static*/
    wchar_t * JavascriptObject::ConstructName(const PropertyRecord * propertyRecord, const wchar_t * getOrSetStr, ScriptContext* scriptContext)
    {
        Assert(propertyRecord);
        Assert(scriptContext);
        wchar_t * finalName = nullptr;
        size_t propertyLength = (size_t)propertyRecord->GetLength();
        if (propertyLength > 0)
        {
            size_t totalChars;
            if (SizeTAdd(propertyLength, ConstructNameGetSetLength, &totalChars) == S_OK)
            {
                finalName = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, totalChars);
                Assert(finalName != nullptr);
                const wchar_t* propertyName = propertyRecord->GetBuffer();
                Assert(propertyName != nullptr);
                wcscpy_s(finalName, totalChars, propertyName);

                Assert(getOrSetStr != nullptr);
                Assert(wcslen(getOrSetStr) == 4);

                wcscpy_s(finalName + propertyLength, ConstructNameGetSetLength, getOrSetStr);
            }
        }
        return finalName;
    }

    /*static*/
    wchar_t * JavascriptObject::ConstructAccessorNameES6(const PropertyRecord * propertyRecord, const wchar_t * getOrSetStr, ScriptContext* scriptContext)
    {
        Assert(propertyRecord);
        Assert(scriptContext);
        wchar_t * finalName = nullptr;
        size_t propertyLength = static_cast<size_t>(propertyRecord->GetLength() + 1); //+ 1 (for null terminator)
        if (propertyLength > 0)
        {
            size_t totalChars;
            const size_t getSetLength = 4;    // 4 = 3 (get or set) +1 (for space)
            if (SizeTAdd(propertyLength, getSetLength, &totalChars) == S_OK)
            {
                finalName = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, totalChars);
                Assert(finalName != nullptr);

                Assert(getOrSetStr != nullptr);
                Assert(wcslen(getOrSetStr) == 4);
                wcscpy_s(finalName, totalChars, getOrSetStr);

                const wchar_t* propertyName = propertyRecord->GetBuffer();
                Assert(propertyName != nullptr);
                js_wmemcpy_s(finalName + getSetLength, propertyLength, propertyName, propertyLength);

            }
        }
        return finalName;
    }

    /*static*/
    void JavascriptObject::ModifyGetterSetterFuncName(const PropertyRecord * propertyRecord, const PropertyDescriptor& descriptor, ScriptContext* scriptContext)
    {
        Assert(scriptContext);
        Assert(propertyRecord);
        if (descriptor.GetterSpecified() || descriptor.SetterSpecified())
        {
            if (descriptor.GetterSpecified()
                && Js::ScriptFunction::Is(descriptor.GetGetter())
                && _wcsicmp(Js::ScriptFunction::FromVar(descriptor.GetGetter())->GetFunctionProxy()->GetDisplayName(), L"get") == 0)
            {
                // modify to name.get
                wchar_t* finalName;
                if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
                {
                    finalName = ConstructAccessorNameES6(propertyRecord, L"get ", scriptContext);
                }
                else
                {
                    finalName = ConstructName(propertyRecord, L".get", scriptContext);
                }
                if (finalName != nullptr)
                {
                    FunctionProxy::SetDisplayNameFlags flags = (FunctionProxy::SetDisplayNameFlags) (FunctionProxy::SetDisplayNameFlagsDontCopy | FunctionProxy::SetDisplayNameFlagsRecyclerAllocated);

                    Js::ScriptFunction::FromVar(descriptor.GetGetter())->GetFunctionProxy()->SetDisplayName(finalName, propertyRecord->GetLength() + 4 /*".get" or "get "*/, flags);
                }
            }

            if (descriptor.SetterSpecified()
                && Js::ScriptFunction::Is(descriptor.GetSetter())
                && _wcsicmp(Js::ScriptFunction::FromVar(descriptor.GetSetter())->GetFunctionProxy()->GetDisplayName(), L"set") == 0)
            {
                // modify to name.set
                wchar_t* finalName;
                if (scriptContext->GetConfig()->IsES6FunctionNameEnabled())
                {
                    finalName = ConstructAccessorNameES6(propertyRecord, L"set ", scriptContext);
                }
                else
                {
                    finalName = ConstructName(propertyRecord, L".set", scriptContext);
                }

                if (finalName != nullptr)
                {
                    FunctionProxy::SetDisplayNameFlags flags = (FunctionProxy::SetDisplayNameFlags) (FunctionProxy::SetDisplayNameFlagsDontCopy | FunctionProxy::SetDisplayNameFlagsRecyclerAllocated);

                    Js::ScriptFunction::FromVar(descriptor.GetSetter())->GetFunctionProxy()->SetDisplayName(finalName, propertyRecord->GetLength() + 4 /*".set" or "set "*/, flags);
                }
            }
        }
    }

    BOOL JavascriptObject::DefineOwnPropertyHelper(RecyclableObject* obj, PropertyId propId, const PropertyDescriptor& descriptor, ScriptContext* scriptContext, bool throwOnError /* = true*/)
    {
        BOOL returnValue;
        obj->ThrowIfCannotDefineProperty(propId, descriptor);

        Type* oldType = obj->GetType();
        obj->ClearWritableDataOnlyDetectionBit();

        // HostDispatch: it doesn't support changing property attributes and default attributes are not per ES5,
        // so there is no benefit in using ES5 DefineOwnPropertyDescriptor for it, use old implementation.
        if (TypeIds_HostDispatch != obj->GetTypeId())
        {
            if (DynamicObject::IsAnyArray(obj))
            {
                returnValue = JavascriptOperators::DefineOwnPropertyForArray(
                    JavascriptArray::FromAnyArray(obj), propId, descriptor, throwOnError, scriptContext);
            }
            else
            {
                returnValue = JavascriptOperators::DefineOwnPropertyDescriptor(obj, propId, descriptor, throwOnError, scriptContext);
                if (propId == PropertyIds::__proto__)
                {
                    scriptContext->GetLibrary()->GetObjectPrototypeObject()->PostDefineOwnProperty__proto__(obj);
                }
            }
        }
        else
        {
            returnValue = JavascriptOperators::SetPropertyDescriptor(obj, propId, descriptor);
        }

        if (propId == PropertyIds::_symbolSpecies && obj == scriptContext->GetLibrary()->GetArrayConstructor())
        {
            scriptContext->GetLibrary()->SetArrayObjectHasUserDefinedSpecies(true);
        }

        if (obj->IsWritableDataOnlyDetectionBitSet())
        {
            if (obj->GetType() == oldType)
            {
                // Also, if the object's type has not changed, we need to ensure that
                // the cached property string for this property, if any, does not
                // specify this object's type.
                scriptContext->InvalidatePropertyStringCache(propId, obj->GetType());
            }
        }

        if (descriptor.IsAccessorDescriptor())
        {
            scriptContext->optimizationOverrides.SetSideEffects(Js::SideEffects_Accessor);
        }
        return returnValue;
    }
}
