//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ObjectPrototypeObject::ObjectPrototypeObject(DynamicType* type) : DynamicObject(type)
    {
        __proto__Enabled = GetScriptContext()->GetConfig()->Is__proto__Enabled();
    }

    ObjectPrototypeObject * ObjectPrototypeObject::New(Recycler * recycler, DynamicType * type)
    {
        return NewObject<ObjectPrototypeObject>(recycler, type);
    }

    Var ObjectPrototypeObject::Entry__proto__getter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));
        ScriptContext* scriptContext = function->GetScriptContext();

#if !FLOATVAR
        // Mark temp number will stack allocate number that is used as the object ptr.
        // So we should box it before call ToObject on it.
        Var arg0 = JavascriptNumber::BoxStackNumber(args[0], scriptContext);
#else
        Var arg0 = args[0];
#endif
        // B.2.2.1.1
        // get Object.prototype.__proto__
        // The value of the [[Get]] attribute is a built-in function that requires no arguments. It performs the following steps:

        // 1. Let O be ToObject(this value).
        // 2. ReturnIfAbrupt(O).
        RecyclableObject* object;
        if (args.Info.Count < 1 || !JavascriptConversion::ToObject(arg0, scriptContext, &object))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, L"Object.prototype.__proto__");
        }

        // 3. Return O.[[GetPrototypeOf]]().
        return JavascriptObject::GetPrototypeOf(object, scriptContext);
    }

    Var ObjectPrototypeObject::Entry__proto__setter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));
        ScriptContext* scriptContext = function->GetScriptContext();

        // B.2.2.1.2
        // set Object.prototype.__proto__ ( proto )
        // The value of the [[Set]] attribute is a built-in function that takes an argument proto. It performs the following steps:

#if !FLOATVAR
        // Mark temp number will stack allocate number that is used as the object ptr.
        // So we should box it before call ToObject on it.
        Var arg0 = JavascriptNumber::BoxStackNumber(args[0], scriptContext);
#else
        Var arg0 = args[0];
#endif

        // 1. Let O be RequireObjectCoercible(this value).
        // 2. ReturnIfAbrupt(O).
        // 3. If Type(proto) is neither Object nor Null, return undefined.
        // 4. If Type(O) is not Object, return undefined.
        if (args.Info.Count < 1 || !JavascriptConversion::CheckObjectCoercible(arg0, scriptContext))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, L"Object.prototype.__proto__");
        }
        else if (args.Info.Count < 2 || !JavascriptOperators::IsObjectOrNull(args[1]) || !JavascriptOperators::IsObject(arg0))
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }

        RecyclableObject* object = RecyclableObject::FromVar(arg0);
        RecyclableObject* newPrototype = RecyclableObject::FromVar(args[1]);

        // 5. Let status be O.[[SetPrototypeOf]](proto).
        // 6. ReturnIfAbrupt(status).
        // 7. If status is false, throw a TypeError exception.
        JavascriptObject::ChangePrototype(object, newPrototype, /*shouldThrow*/true, scriptContext);

        // 8.   Return undefined.
        return scriptContext->GetLibrary()->GetUndefined();
    }

    BOOL ObjectPrototypeObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {
        const BOOL result = __super::DeleteProperty(propertyId, flags);
        if (result && propertyId == PropertyIds::__proto__)
        {
            this->__proto__Enabled = false;
        }

        return result;
    }

    void ObjectPrototypeObject::PostDefineOwnProperty__proto__(RecyclableObject* obj)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        if (obj == this && scriptContext->GetConfig()->Is__proto__Enabled())
        {
            Var getter, setter;

            // __proto__Enabled is now only used by diagnostics to decide displaying __proto__ or [prototype].
            // We consider __proto__ is enabled when original getter and setter are both in place.
            this->__proto__Enabled =
                this->GetAccessors(PropertyIds::__proto__, &getter, &setter, scriptContext)
                && getter == scriptContext->GetLibrary()->Get__proto__getterFunction()
                && setter == scriptContext->GetLibrary()->Get__proto__setterFunction();
        }
    }
}
