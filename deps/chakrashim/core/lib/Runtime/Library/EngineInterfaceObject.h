//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_PROJECTION)

namespace Js
{
    class WindowsGlobalizationAdapter;

    enum EngineInterfaceExtensionKind
    {
        EngineInterfaceExtensionKind_Intl = 0,
        EngineInterfaceExtensionKind_WinRTPromise = 1,
        MaxEngineInterfaceExtensionKind = EngineInterfaceExtensionKind_WinRTPromise
    };

    class EngineExtensionObjectBase
    {
    public:
        EngineExtensionObjectBase(EngineInterfaceExtensionKind kind, Js::ScriptContext * context) :
            extensionKind(kind),
            scriptContext(context)
        {
        }

        EngineInterfaceExtensionKind GetExtensionKind() const { return extensionKind; }
        ScriptContext* GetScriptContext() const { return scriptContext; }
        virtual void Initialize() = 0;
#if DBG
        virtual void DumpByteCode() = 0;
#endif

    protected:
        EngineInterfaceExtensionKind extensionKind;
        ScriptContext* scriptContext;
    };

#define EngineInterfaceObject_CommonFunctionProlog(function, callInfo) \
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault); \
        RUNTIME_ARGUMENTS(args, callInfo); \
        Assert(!(callInfo.Flags & CallFlags_New)); \
        unsigned argCount = args.Info.Count; \
        ScriptContext* scriptContext = function->GetScriptContext(); \
        AssertMsg(argCount > 0, "Should always have implicit 'this'"); \

    class EngineInterfaceObject : public DynamicObject
    {
    private:
        DEFINE_VTABLE_CTOR(EngineInterfaceObject, DynamicObject);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(EngineInterfaceObject);

        DynamicObject* commonNativeInterfaces;

        EngineExtensionObjectBase* engineExtensions[MaxEngineInterfaceExtensionKind + 1];

    public:
        EngineInterfaceObject(DynamicType * type) : DynamicObject(type) {}
        DynamicObject* GetCommonNativeInterfaces() const { return commonNativeInterfaces; }
        EngineExtensionObjectBase* GetEngineExtension(EngineInterfaceExtensionKind extensionKind) const;
        void SetEngineExtension(EngineInterfaceExtensionKind extensionKind, EngineExtensionObjectBase* extensionObject);

        static EngineInterfaceObject* New(Recycler * recycler, DynamicType * type);
        static bool Is(Var aValue);
        static EngineInterfaceObject* FromVar(Var aValue);

        void Initialize();

        static void __cdecl InitializeCommonNativeInterfaces(DynamicObject* engineInterface, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

        class EntryInfo
        {
        public:
            static NoProfileFunctionInfo GetErrorMessage;
            static NoProfileFunctionInfo LogDebugMessage;
            static NoProfileFunctionInfo TagPublicLibraryCode;


#ifndef GlobalBuiltIn
#define GlobalBuiltIn(global, method) \
            static NoProfileFunctionInfo Intl_BuiltIn_##global##_##method##; \

#define GlobalBuiltInConstructor(global)

#define BuiltInRaiseException(exceptionType, exceptionID) \
     static NoProfileFunctionInfo Intl_BuiltIn_raise##exceptionID;

#define BuiltInRaiseException1(exceptionType, exceptionID) BuiltInRaiseException(exceptionType, exceptionID)
#define BuiltInRaiseException2(exceptionType, exceptionID) BuiltInRaiseException(exceptionType, exceptionID)
#define BuiltInRaiseException3(exceptionType, exceptionID) BuiltInRaiseException(exceptionType, exceptionID##_3)

#include "EngineInterfaceObjectBuiltIns.h"

#undef BuiltInRaiseException
#undef BuiltInRaiseException1
#undef BuiltInRaiseException2
#undef BuiltInRaiseException3
#undef GlobalBuiltInConstructor
#undef GlobalBuiltIn
#endif
        };

        static Var Entry_GetErrorMessage(RecyclableObject *function, CallInfo callInfo, ...);
        static Var Entry_LogDebugMessage(RecyclableObject *function, CallInfo callInfo, ...);
        static Var Entry_TagPublicLibraryCode(RecyclableObject *function, CallInfo callInfo, ...);
#ifdef ENABLE_PROJECTION
        static Var EntryPromise_EnqueueTask(RecyclableObject *function, CallInfo callInfo, ...);
#endif

#ifndef GlobalBuiltIn
#define GlobalBuiltIn(global, method)

#define GlobalBuiltInConstructor(global)

#define BuiltInRaiseException(exceptionType, exceptionID) \
        static Var EntryIntl_BuiltIn_raise##exceptionID(RecyclableObject *function, CallInfo callInfo, ...);

#define BuiltInRaiseException1(exceptionType, exceptionID) BuiltInRaiseException(exceptionType, exceptionID)
#define BuiltInRaiseException2(exceptionType, exceptionID) BuiltInRaiseException(exceptionType, exceptionID)
#define BuiltInRaiseException3(exceptionType, exceptionID) BuiltInRaiseException(exceptionType, exceptionID##_3)

#include "EngineInterfaceObjectBuiltIns.h"

#undef BuiltInRaiseException
#undef BuiltInRaiseException1
#undef BuiltInRaiseException2
#undef BuiltInRaiseException3
#undef GlobalBuiltInConstructor
#undef GlobalBuiltIn
#endif

    };
}

#endif // ENABLE_INTL_OBJECT || ENABLE_PROJECTION
