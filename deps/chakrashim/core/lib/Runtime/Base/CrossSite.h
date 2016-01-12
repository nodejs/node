//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class DOMFastPathInfo;
namespace Js
{
    class CrossSite
    {
        friend class ExternalType;
        friend class DOMFastPathInfo;
    public:
        static bool IsThunk(JavascriptMethod thunk);
        static BOOL NeedMarshalVar(Var instance, ScriptContext * requestContext);
        static Var DefaultThunk(RecyclableObject* function, CallInfo callInfo, ...);
        static Var ProfileThunk(RecyclableObject* function, CallInfo callInfo, ...);
        static Var MarshalVar(ScriptContext* scriptContext, Var value, bool fRequestWrapper = false);
        static Var MarshalEnumerator(ScriptContext* scriptContext, Var value);
        static void MarshalDynamicObjectAndPrototype(ScriptContext * scriptContext, DynamicObject * object);
        static void ForceCrossSiteThunkOnPrototypeChain(RecyclableObject* object);

        // If we know the type of the object, use this to do a virtual table check instead of a virtual call
        template <typename T>
        static BOOL IsCrossSiteObjectTyped(T * obj)
        {
            BOOL ret = VirtualTableInfo<CrossSiteObject<T>>::HasVirtualTable(obj);
            Assert(ret || VirtualTableInfo<T>::HasVirtualTable(obj));
            Assert(ret == obj->IsCrossSiteObject());
            return ret;
        }
    private:
        static Var MarshalVarInner(ScriptContext* scriptContext, __in Js::RecyclableObject* object, bool fRequestWrapper);
        static Var CommonThunk(RecyclableObject * function, JavascriptMethod entryPoint, Arguments args);

        static void MarshalDynamicObject(ScriptContext * scriptContext, DynamicObject * object);
        static void MarshalPrototypeChain(ScriptContext * scriptContext, DynamicObject * object);
        static Var MarshalFrameDisplay(ScriptContext* scriptContext, FrameDisplay *display);

        static bool DoRequestWrapper(Js::RecyclableObject* object, bool fRequestWrapper);
    };
};

