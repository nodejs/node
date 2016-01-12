//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "StdAfx.h"

LPCWSTR chakraDllName = L"chakracore.dll";

bool ChakraRTInterface::m_testHooksSetup = false;
bool ChakraRTInterface::m_testHooksInitialized = false;
bool ChakraRTInterface::m_usageStringPrinted = false;

ChakraRTInterface::ArgInfo ChakraRTInterface::m_argInfo = { 0 };
TestHooks ChakraRTInterface::m_testHooks = { 0 };
JsAPIHooks ChakraRTInterface::m_jsApiHooks = { 0 };

/*static*/
HINSTANCE ChakraRTInterface::LoadChakraDll(ArgInfo& argInfo)
{
    m_argInfo = argInfo;

    wchar_t filename[_MAX_PATH];
    wchar_t drive[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];

    wchar_t modulename[_MAX_PATH];
    GetModuleFileName(NULL, modulename, _MAX_PATH);
    _wsplitpath_s(modulename, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
    _wmakepath_s(filename, drive, dir, chakraDllName, nullptr);
    LPCWSTR dllName = filename;

    HINSTANCE library = LoadLibraryEx(dllName, nullptr, 0);
    if (library == nullptr)
    {
        int ret = GetLastError();
        fwprintf(stderr, L"FATAL ERROR: Unable to load %ls GetLastError=0x%x\n", chakraDllName, ret);
        return nullptr;
    }

    if (m_usageStringPrinted)
    {
        UnloadChakraDll(library);
        return nullptr;
    }

    if (!m_testHooksInitialized)
    {
        fwprintf(stderr, L"The binary %ls is not test enabled, please use %ls from debug/test flavor\n", chakraDllName, chakraDllName);
        UnloadChakraDll(library);
        return nullptr;
    }

    m_jsApiHooks.pfJsrtCreateRuntime = (JsAPIHooks::JsrtCreateRuntimePtr)GetProcAddress(library, "JsCreateRuntime");
    m_jsApiHooks.pfJsrtCreateContext = (JsAPIHooks::JsrtCreateContextPtr)GetProcAddress(library, "JsCreateContext");
    m_jsApiHooks.pfJsrtSetCurrentContext = (JsAPIHooks::JsrtSetCurrentContextPtr)GetProcAddress(library, "JsSetCurrentContext");
    m_jsApiHooks.pfJsrtGetCurrentContext = (JsAPIHooks::JsrtGetCurrentContextPtr)GetProcAddress(library, "JsGetCurrentContext");
    m_jsApiHooks.pfJsrtDisposeRuntime = (JsAPIHooks::JsrtDisposeRuntimePtr)GetProcAddress(library, "JsDisposeRuntime");
    m_jsApiHooks.pfJsrtCreateObject = (JsAPIHooks::JsrtCreateObjectPtr)GetProcAddress(library, "JsCreateObject");
    m_jsApiHooks.pfJsrtCreateExternalObject = (JsAPIHooks::JsrtCreateExternalObjectPtr)GetProcAddress(library, "JsCreateExternalObject");
    m_jsApiHooks.pfJsrtCreateFunction = (JsAPIHooks::JsrtCreateFunctionPtr)GetProcAddress(library, "JsCreateFunction");
    m_jsApiHooks.pfJsrtCreateNameFunction = (JsAPIHooks::JsCreateNamedFunctionPtr)GetProcAddress(library, "JsCreateNamedFunction");
    m_jsApiHooks.pfJsrtPointerToString = (JsAPIHooks::JsrtPointerToStringPtr)GetProcAddress(library, "JsPointerToString");
    m_jsApiHooks.pfJsrtSetProperty = (JsAPIHooks::JsrtSetPropertyPtr)GetProcAddress(library, "JsSetProperty");
    m_jsApiHooks.pfJsrtGetGlobalObject = (JsAPIHooks::JsrtGetGlobalObjectPtr)GetProcAddress(library, "JsGetGlobalObject");
    m_jsApiHooks.pfJsrtGetUndefinedValue = (JsAPIHooks::JsrtGetUndefinedValuePtr)GetProcAddress(library, "JsGetUndefinedValue");
    m_jsApiHooks.pfJsrtGetTrueValue = (JsAPIHooks::JsrtGetUndefinedValuePtr)GetProcAddress(library, "JsGetTrueValue");
    m_jsApiHooks.pfJsrtGetFalseValue = (JsAPIHooks::JsrtGetUndefinedValuePtr)GetProcAddress(library, "JsGetFalseValue");
    m_jsApiHooks.pfJsrtConvertValueToString = (JsAPIHooks::JsrtConvertValueToStringPtr)GetProcAddress(library, "JsConvertValueToString");
    m_jsApiHooks.pfJsrtConvertValueToNumber = (JsAPIHooks::JsrtConvertValueToNumberPtr)GetProcAddress(library, "JsConvertValueToNumber");
    m_jsApiHooks.pfJsrtConvertValueToBoolean = (JsAPIHooks::JsrtConvertValueToBooleanPtr)GetProcAddress(library, "JsConvertValueToBoolean");
    m_jsApiHooks.pfJsrtStringToPointer = (JsAPIHooks::JsrtStringToPointerPtr)GetProcAddress(library, "JsStringToPointer");
    m_jsApiHooks.pfJsrtBooleanToBool = (JsAPIHooks::JsrtBooleanToBoolPtr)GetProcAddress(library, "JsBooleanToBool");
    m_jsApiHooks.pfJsrtGetPropertyIdFromName = (JsAPIHooks::JsrtGetPropertyIdFromNamePtr)GetProcAddress(library, "JsGetPropertyIdFromName");
    m_jsApiHooks.pfJsrtGetProperty = (JsAPIHooks::JsrtGetPropertyPtr)GetProcAddress(library, "JsGetProperty");
    m_jsApiHooks.pfJsrtHasProperty = (JsAPIHooks::JsrtHasPropertyPtr)GetProcAddress(library, "JsHasProperty");
    m_jsApiHooks.pfJsrtRunScript = (JsAPIHooks::JsrtRunScriptPtr)GetProcAddress(library, "JsRunScript");
    m_jsApiHooks.pfJsrtCallFunction = (JsAPIHooks::JsrtCallFunctionPtr)GetProcAddress(library, "JsCallFunction");
    m_jsApiHooks.pfJsrtNumbertoDouble = (JsAPIHooks::JsrtNumberToDoublePtr)GetProcAddress(library, "JsNumberToDouble");
    m_jsApiHooks.pfJsrtNumbertoInt = (JsAPIHooks::JsrtNumberToIntPtr)GetProcAddress(library, "JsNumberToInt");
    m_jsApiHooks.pfJsrtDoubleToNumber = (JsAPIHooks::JsrtDoubleToNumberPtr)GetProcAddress(library, "JsDoubleToNumber");
    m_jsApiHooks.pfJsrtGetExternalData = (JsAPIHooks::JsrtGetExternalDataPtr)GetProcAddress(library, "JsGetExternalData");
    m_jsApiHooks.pfJsrtCreateArray = (JsAPIHooks::JsrtCreateArrayPtr)GetProcAddress(library, "JsCreateArray");
    m_jsApiHooks.pfJsrtSetException = (JsAPIHooks::JsrtSetExceptionPtr)GetProcAddress(library, "JsSetException");
    m_jsApiHooks.pfJsrtGetAndClearException = (JsAPIHooks::JsrtGetAndClearExceptiopnPtr)GetProcAddress(library, "JsGetAndClearException");
    m_jsApiHooks.pfJsrtCreateError = (JsAPIHooks::JsrtCreateErrorPtr)GetProcAddress(library, "JsCreateError");
    m_jsApiHooks.pfJsrtGetRuntime = (JsAPIHooks::JsrtGetRuntimePtr)GetProcAddress(library, "JsGetRuntime");
    m_jsApiHooks.pfJsrtRelease = (JsAPIHooks::JsrtReleasePtr)GetProcAddress(library, "JsRelease");
    m_jsApiHooks.pfJsrtAddRef = (JsAPIHooks::JsrtAddRefPtr)GetProcAddress(library, "JsAddRef");
    m_jsApiHooks.pfJsrtGetValueType = (JsAPIHooks::JsrtGetValueType)GetProcAddress(library, "JsGetValueType");
    m_jsApiHooks.pfJsrtSetIndexedProperty = (JsAPIHooks::JsrtSetIndexedPropertyPtr)GetProcAddress(library, "JsSetIndexedProperty");
    m_jsApiHooks.pfJsrtSerializeScript = (JsAPIHooks::JsrtSerializeScriptPtr)GetProcAddress(library, "JsSerializeScript");
    m_jsApiHooks.pfJsrtRunSerializedScript = (JsAPIHooks::JsrtRunSerializedScriptPtr)GetProcAddress(library, "JsRunSerializedScript");
    m_jsApiHooks.pfJsrtSetPromiseContinuationCallback = (JsAPIHooks::JsrtSetPromiseContinuationCallbackPtr)GetProcAddress(library, "JsSetPromiseContinuationCallback");
    m_jsApiHooks.pfJsrtGetContextOfObject = (JsAPIHooks::JsrtGetContextOfObject)GetProcAddress(library, "JsGetContextOfObject");

    return library;
}

/*static*/
void ChakraRTInterface::UnloadChakraDll(HINSTANCE library)
{
    Assert(library != nullptr);
    FARPROC pDllCanUnloadNow = GetProcAddress(library, "DllCanUnloadNow");
    if (pDllCanUnloadNow != nullptr)
    {
        pDllCanUnloadNow();
    }
    FreeLibrary(library);
}

/*static*/
HRESULT ChakraRTInterface::ParseConfigFlags()
{
    HRESULT hr = S_OK;

    if (m_testHooks.pfSetAssertToConsoleFlag)
    {
        SetAssertToConsoleFlag(true);
    }

    if (m_testHooks.pfSetConfigFlags)
    {
        hr = SetConfigFlags(m_argInfo.argc, m_argInfo.argv, &HostConfigFlags::flags);
        if (hr != S_OK && !m_usageStringPrinted)
        {
            m_argInfo.hostPrintUsage();
            m_usageStringPrinted = true;
        }
    }

    if (hr == S_OK)
    {
        Assert(m_argInfo.filename != nullptr);

        *(m_argInfo.filename) = nullptr;
        Assert(m_testHooks.pfGetFilenameFlag != nullptr);
        hr = GetFileNameFlag(m_argInfo.filename);
        if (hr != S_OK)
        {
            wprintf(L"Error: no script file specified.");
            m_argInfo.hostPrintUsage();
            m_usageStringPrinted = true;
        }
    }

    return S_OK;
}

/*static*/
HRESULT ChakraRTInterface::OnChakraCoreLoaded(TestHooks& testHooks)
{
    if (!m_testHooksInitialized)
    {
        m_testHooks = testHooks;
        m_testHooksSetup = true;
        m_testHooksInitialized = true;
        return ParseConfigFlags();
    }

    return S_OK;
}
