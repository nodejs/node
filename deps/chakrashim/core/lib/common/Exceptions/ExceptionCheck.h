//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef EXCEPTION_CHECK

enum ExceptionType
{
    ExceptionType_None                = 0x00000000,
    ExceptionType_OutOfMemory         = 0x00000001,
    ExceptionType_StackOverflow       = 0x00000002,
    ExceptionType_JavascriptException = 0x00000004,
    ExceptionType_HasStackProbe       = 0x00000008,
    ExceptionType_DisableCheck        = 0x80000000,
    ExceptionType_All                 = 0x0FFFFFFF,
};

class ExceptionCheck
{
public:
    static void SetHandledExceptionType(ExceptionType e);
    static ExceptionType ClearHandledExceptionType();
    static BOOL CanHandleOutOfMemory();
    static BOOL CanHandleStackOverflow(bool isExternal);
    static BOOL HasStackProbe();

    struct Data
    {
        ExceptionType handledExceptionType;
    };

    static ExceptionCheck::Data Save();
    static void Restore(ExceptionCheck::Data& savedData);
    static ExceptionCheck::Data GetData();
private:
    static BOOL IsEmpty();
    __declspec(thread) static Data data;
};

class AutoHandledExceptionType
{
public:
    AutoHandledExceptionType(ExceptionType e);
    ~AutoHandledExceptionType();
};

class AutoNestedHandledExceptionType
{
public:
    AutoNestedHandledExceptionType(ExceptionType e);
    ~AutoNestedHandledExceptionType();
private:
    ExceptionCheck::Data savedData;
};

class AutoFilterExceptionRegion
{
public:
    AutoFilterExceptionRegion(ExceptionType e);
    ~AutoFilterExceptionRegion();
private:
    ExceptionCheck::Data savedData;
};

#define AssertCanHandleOutOfMemory() Assert(ExceptionCheck::CanHandleOutOfMemory())
#define AssertCanHandleStackOverflow() Assert(ExceptionCheck::CanHandleStackOverflow(false))
#define AssertCanHandleStackOverflowCall(isExternal) Assert(ExceptionCheck::CanHandleStackOverflow(isExternal))

#define DECLARE_EXCEPTION_CHECK_DATA \
    ExceptionCheck::Data __exceptionCheck;

#define SAVE_EXCEPTION_CHECK \
    __exceptionCheck = ExceptionCheck::Save();

#define RESTORE_EXCEPTION_CHECK \
    ExceptionCheck::Restore(__exceptionCheck);

#define AUTO_HANDLED_EXCEPTION_TYPE(type) AutoHandledExceptionType __autoHandledExceptionType(type)
#define AUTO_NESTED_HANDLED_EXCEPTION_TYPE(type) AutoNestedHandledExceptionType __autoNestedHandledExceptionType(type)
#define AUTO_FILTER_EXCEPTION_REGION(type) AutoFilterExceptionRegion __autoFilterExceptionRegion(type)
#define AUTO_NO_EXCEPTION_REGION AUTO_FILTER_EXCEPTION_REGION(ExceptionType_All)
#else
#define AssertCanHandleOutOfMemory()
#define AssertCanHandleStackOverflow()
#define AssertCanHandleStackOverflowCall(isExternal)
#define DECLARE_EXCEPTION_CHECK_DATA
#define SAVE_EXCEPTION_CHECK
#define RESTORE_EXCEPTION_CHECK
#define AUTO_HANDLED_EXCEPTION_TYPE(type)
#define AUTO_NESTED_HANDLED_EXCEPTION_TYPE(type)
#define AUTO_FILTER_EXCEPTION_REGION(type)
#define AUTO_NO_EXCEPTION_REGION
#endif

#if DBG
class DebugCheckNoException
{
public:
    DebugCheckNoException() : hasException(true) { SAVE_EXCEPTION_CHECK;}
    ~DebugCheckNoException() { Assert(!hasException); RESTORE_EXCEPTION_CHECK; }
    DECLARE_EXCEPTION_CHECK_DATA;
    bool hasException;
};
#define BEGIN_NO_EXCEPTION { DebugCheckNoException __debugCheckNoException;
#define END_NO_EXCEPTION __debugCheckNoException.hasException = false; }
#else
#define BEGIN_NO_EXCEPTION
#define END_NO_EXCEPTION
#endif
