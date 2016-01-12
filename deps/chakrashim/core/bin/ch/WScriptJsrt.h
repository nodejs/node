//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class WScriptJsrt
{
public:
    static bool Initialize();

    class CallbackMessage : public MessageBase
    {
        JsValueRef m_function;

        CallbackMessage(CallbackMessage const&);

    public:
        CallbackMessage(unsigned int time, JsValueRef function);
        ~CallbackMessage();

        HRESULT Call(LPCWSTR fileName);
    };

    static void AddMessageQueue(MessageQueue *messageQueue);

    static LPCWSTR ConvertErrorCodeToMessage(JsErrorCode errorCode)
    {
        switch (errorCode)
        {
        case (JsErrorCode::JsErrorInvalidArgument) :
            return L"TypeError: InvalidArgument";
        case (JsErrorCode::JsErrorNullArgument) :
            return L"TypeError: NullArgument";
        case (JsErrorCode::JsErrorArgumentNotObject) :
            return L"TypeError: ArgumentNotAnObject";
        case (JsErrorCode::JsErrorOutOfMemory) :
            return L"OutOfMemory";
        case (JsErrorCode::JsErrorScriptException) :
            return L"ScriptError";
        case (JsErrorCode::JsErrorScriptCompile) :
            return L"SyntaxError";
        case (JsErrorCode::JsErrorFatal) :
            return L"FatalError";
        default:
            AssertMsg(false, "Unexpected JsErrorCode");
            return nullptr;
        }
    }

    static bool PrintException(LPCWSTR fileName, JsErrorCode jsErrorCode);
    static JsValueRef LoadScript(JsValueRef callee, LPCWSTR fileName, size_t fileNameLength, LPCWSTR fileContent, LPCWSTR scriptInjectType);
    static DWORD_PTR GetNextSourceContext();

private:
    static bool CreateArgumentsObject(JsValueRef *argsObject);
    static bool CreateNamedFunction(const wchar_t*, JsNativeFunction callback, JsValueRef* functionVar);
    static JsValueRef __stdcall EchoCallback(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef __stdcall QuitCallback(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef __stdcall LoadScriptFileCallback(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef __stdcall LoadScriptCallback(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef __stdcall SetTimeoutCallback(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef __stdcall ClearTimeoutCallback(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static MessageQueue *messageQueue;
    static DWORD_PTR sourceContext;
};
