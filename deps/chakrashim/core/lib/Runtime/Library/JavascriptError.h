//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct RestrictedErrorStrings
    {
        BSTR restrictedErrStr;
        BSTR referenceStr;
        BSTR capabilitySid;
    };

    class JavascriptError : public DynamicObject
    {
    private:
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptError);

        ErrorTypeEnum m_errorType;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptError, DynamicObject);

    public:

        JavascriptError(DynamicType* type, BOOL isExternalError = FALSE, BOOL isPrototype = FALSE) :
            DynamicObject(type), originalRuntimeErrorMessage(nullptr), isExternalError(isExternalError), isPrototype(isPrototype), isStackPropertyRedefined(false)
        {
            Assert(type->GetTypeId() == TypeIds_Error);
            exceptionObject = nullptr;
            m_errorType = kjstCustomError;
        }

        static bool Is(Var aValue);
        static bool IsRemoteError(Var aValue);

        ErrorTypeEnum GetErrorType() { return m_errorType; }

        virtual bool HasDebugInfo();

        static JavascriptError* FromVar(Var aValue)
        {
            AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptError'");

            return static_cast<JavascriptError *>(RecyclableObject::FromVar(aValue));
        }

        void SetNotEnumerable(PropertyId propertyId);

        static Var NewInstance(RecyclableObject* function, JavascriptError* pError, CallInfo callInfo, Arguments args);
        class EntryInfo
        {
        public:
            static FunctionInfo NewErrorInstance;
            static FunctionInfo NewEvalErrorInstance;
            static FunctionInfo NewRangeErrorInstance;
            static FunctionInfo NewReferenceErrorInstance;
            static FunctionInfo NewSyntaxErrorInstance;
            static FunctionInfo NewTypeErrorInstance;
            static FunctionInfo NewURIErrorInstance;
#ifdef ENABLE_PROJECTION
            static FunctionInfo NewWinRTErrorInstance;
#endif
            static FunctionInfo ToString;
        };

        static Var NewErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewEvalErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewRangeErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewReferenceErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewSyntaxErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewTypeErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewURIErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
#ifdef ENABLE_PROJECTION
        static Var NewWinRTErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
#endif

        static Var EntryToString(RecyclableObject* function, CallInfo callInfo, ...);

        static void __declspec(noreturn) MapAndThrowError(ScriptContext* scriptContext, HRESULT hr);
        static void __declspec(noreturn) MapAndThrowError(ScriptContext* scriptContext, HRESULT hr, ErrorTypeEnum errorType, EXCEPINFO *ei, IErrorInfo * perrinfo = nullptr, RestrictedErrorStrings * proerrstr = nullptr, bool useErrInfoDescription = false);
        static void __declspec(noreturn) MapAndThrowError(ScriptContext* scriptContext, JavascriptError *pError, long hCode, EXCEPINFO* pei, bool useErrInfoDescription = false);
        static JavascriptError* MapError(ScriptContext* scriptContext, ErrorTypeEnum errorType, IErrorInfo * perrinfo = nullptr, RestrictedErrorStrings * proerrstr = nullptr);

#define THROW_ERROR_DECL(err_method) \
        static void __declspec(noreturn) err_method(ScriptContext* scriptContext, long hCode, EXCEPINFO* ei, IErrorInfo* perrinfo = nullptr, RestrictedErrorStrings* proerrstr = nullptr, bool useErrInfoDescription = false); \
        static void __declspec(noreturn) err_method(ScriptContext* scriptContext, long hCode, PCWSTR varName = nullptr); \
        static void __declspec(noreturn) err_method(ScriptContext* scriptContext, long hCode, JavascriptString* varName); \
        static void __declspec(noreturn) err_method##Var(ScriptContext* scriptContext, long hCode, ...);

        THROW_ERROR_DECL(ThrowError)
        THROW_ERROR_DECL(ThrowEvalError)
        THROW_ERROR_DECL(ThrowRangeError)
        THROW_ERROR_DECL(ThrowReferenceError)
        THROW_ERROR_DECL(ThrowSyntaxError)
        THROW_ERROR_DECL(ThrowTypeError)
        THROW_ERROR_DECL(ThrowURIError)

#undef THROW_ERROR_DECL
        static void __declspec(noreturn) ThrowDispatchError(ScriptContext* scriptContext, HRESULT hCode, PCWSTR message);
        static void __declspec(noreturn) ThrowOutOfMemoryError(ScriptContext *scriptContext);
        static void __declspec(noreturn) ThrowParserError(ScriptContext* scriptContext, HRESULT hrParser, CompileScriptException* se);
        static ErrorTypeEnum MapParseError(long hCode);
        static JavascriptError* MapParseError(ScriptContext* scriptContext, long hCode);
        static HRESULT GetRuntimeError(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage);
        static HRESULT GetRuntimeErrorWithScriptEnter(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage);
        static void __declspec(noreturn) ThrowStackOverflowError(ScriptContext *scriptContext, PVOID returnAddress = false);
        static void SetErrorMessageProperties(JavascriptError *pError, long errCode, PCWSTR message, ScriptContext* scriptContext);
        static void SetErrorMessage(JavascriptError *pError, long errCode, PCWSTR varName, ScriptContext* scriptContext);
        static void SetErrorMessage(JavascriptError *pError, HRESULT hr, ScriptContext* scriptContext, va_list argList);
        static void SetErrorType(JavascriptError *pError, ErrorTypeEnum errorType);

        static bool ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, PropertyId propertyId);
        static bool ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, uint32 index);
        static bool ThrowCantAssignIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext);
        static bool ThrowCantExtendIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext);
        static bool ThrowCantDeleteIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext, PCWSTR varName);
        static bool ThrowIfStrictModeUndefinedSetter(PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext);
        static bool ThrowIfNotExtensibleUndefinedSetter(PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext);

        BOOL IsExternalError() const { return isExternalError; }
        BOOL IsPrototype() const { return isPrototype; }
        bool IsStackPropertyRedefined() const { return isStackPropertyRedefined; }
        void SetStackPropertyRedefined(const bool value) { isStackPropertyRedefined = value; }
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        void SetJavascriptExceptionObject(JavascriptExceptionObject *exceptionObject)
        {
            Assert(exceptionObject);
            this->exceptionObject = exceptionObject;
        }

        JavascriptExceptionObject *GetJavascriptExceptionObject() { return exceptionObject; }

        static DWORD GetAdjustedResourceStringHr(DWORD hr, bool isFormatString);

        static long GetErrorNumberFromResourceID(long resourceId);

        JavascriptError* CreateNewErrorOfSameType(JavascriptLibrary* targetJavascriptLibrary);
        JavascriptError* CloneErrorMsgAndNumber(JavascriptLibrary* targetJavascriptLibrary);
        static void TryThrowTypeError(ScriptContext * checkScriptContext, ScriptContext * scriptContext, long hCode, PCWSTR varName = nullptr);

    private:

        BOOL isExternalError;
        BOOL isPrototype;
        bool isStackPropertyRedefined;
        wchar_t const * originalRuntimeErrorMessage;
        JavascriptExceptionObject *exceptionObject;

#ifdef ERROR_TRACE
        static void Trace(const wchar_t *form, ...) // const
        {
            if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::ErrorPhase))
            {
                va_list argptr;
                va_start(argptr, form);
                Output::Print(L"Error: ");
                Output::VPrint(form, argptr);
                Output::Flush();
            }
        }
#endif
    };
}
