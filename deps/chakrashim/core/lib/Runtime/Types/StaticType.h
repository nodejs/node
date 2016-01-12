//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class StaticType : public Type
    {
    public:
        StaticType(StaticType * type) : Type(type) {}
        StaticType(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint) :
            Type(scriptContext, typeId, prototype, entryPoint)
        {
            Assert(StaticType::Is(typeId));
#ifdef HEAP_ENUMERATION_VALIDATION
            if (prototype) prototype->SetHeapEnumValidationCookie(HEAP_ENUMERATION_LIBRARY_OBJECT_COOKIE);
#endif
        }
        void SetDispatchInvoke(JavascriptMethod method) { Assert(typeId == TypeIds_HostDispatch); entryPoint = method; }
    public:
        static bool Is(TypeId typeId);
        static StaticType * New(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint);
    };
};
