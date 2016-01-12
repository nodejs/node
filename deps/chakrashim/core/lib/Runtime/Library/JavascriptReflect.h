//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//  Implements Reflect object.
//----------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Reflect object is an ordinary object; we just need to provide a place holder for all
    // the static entry points.
    class JavascriptReflect
    {
    protected:
    public:
        class EntryInfo
        {
        public:
            static FunctionInfo DefineProperty;
            static FunctionInfo DeleteProperty;
            static FunctionInfo Enumerate;
            static FunctionInfo Get;
            static FunctionInfo GetOwnPropertyDescriptor;
            static FunctionInfo GetPrototypeOf;
            static FunctionInfo Has;
            static FunctionInfo IsExtensible;
            static FunctionInfo OwnKeys;
            static FunctionInfo PreventExtensions;
            static FunctionInfo Set;
            static FunctionInfo SetPrototypeOf;
            static FunctionInfo Apply;
            static FunctionInfo Construct;
        };

        static Var EntryDefineProperty(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryDeleteProperty(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEnumerate(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGet(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetOwnPropertyDescriptor(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetPrototypeOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryHas(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIsExtensible(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryOwnKeys(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryPreventExtensions(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySet(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetPrototypeOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryApply(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryConstruct(RecyclableObject* function, CallInfo callInfo, ...);
    };
}
