//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonExceptionsPch.h"

#ifdef EXCEPTION_CHECK
#include "ExceptionCheck.h"

__declspec(thread) ExceptionCheck::Data ExceptionCheck::data;

BOOL ExceptionCheck::IsEmpty()
{
    return (data.handledExceptionType == ExceptionType_None);
}

ExceptionCheck::Data ExceptionCheck::Save()
{
    ExceptionCheck::Data savedData = data;
    data = ExceptionCheck::Data();
    return savedData;
}

void ExceptionCheck::Restore(ExceptionCheck::Data& savedData)
{
    Assert(IsEmpty());
    data = savedData;
}

ExceptionCheck::Data ExceptionCheck::GetData()
{
    return data;
}

BOOL ExceptionCheck::CanHandleOutOfMemory()
{
    return (data.handledExceptionType == ExceptionType_DisableCheck) ||
        JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
        (data.handledExceptionType & ExceptionType_OutOfMemory);
}

BOOL ExceptionCheck::HasStackProbe()
{
    return  (data.handledExceptionType & ExceptionType_HasStackProbe);
}


BOOL ExceptionCheck::CanHandleStackOverflow(bool isExternal)
{
    return (JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() || isExternal) ||
        (data.handledExceptionType & ExceptionType_StackOverflow) ||
        (data.handledExceptionType == ExceptionType_DisableCheck);
}
void ExceptionCheck::SetHandledExceptionType(ExceptionType e)
{
    Assert((e & ExceptionType_DisableCheck) == 0 || e == ExceptionType_DisableCheck);
    Assert(IsEmpty());
#if DBG
    if(!(e == ExceptionType_None ||
         e == ExceptionType_DisableCheck ||
         !JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
         (e & ExceptionType_JavascriptException) == ExceptionType_JavascriptException ||
         e == ExceptionType_HasStackProbe))
    {
        Assert(false);
    }
#endif
    data.handledExceptionType = e;
}

ExceptionType ExceptionCheck::ClearHandledExceptionType()
{
    ExceptionType exceptionType = data.handledExceptionType;
    data.handledExceptionType = ExceptionType_None;
    Assert(IsEmpty());
    return exceptionType;
}

AutoHandledExceptionType::AutoHandledExceptionType(ExceptionType e)
{
    ExceptionCheck::SetHandledExceptionType(e);
}

AutoHandledExceptionType::~AutoHandledExceptionType()
{
    Assert(ExceptionCheck::GetData().handledExceptionType == ExceptionType_DisableCheck ||
        !JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
        ExceptionCheck::GetData().handledExceptionType == ExceptionType_HasStackProbe ||
        (ExceptionCheck::GetData().handledExceptionType & ExceptionType_JavascriptException) == ExceptionType_JavascriptException);
    ExceptionCheck::ClearHandledExceptionType();
}

AutoNestedHandledExceptionType::AutoNestedHandledExceptionType(ExceptionType e)
{
    savedData = ExceptionCheck::Save();
    ExceptionCheck::SetHandledExceptionType(e);
}
AutoNestedHandledExceptionType::~AutoNestedHandledExceptionType()
{
    Assert(ExceptionCheck::GetData().handledExceptionType == ExceptionType_DisableCheck ||
        !JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
        ExceptionCheck::GetData().handledExceptionType == ExceptionType_HasStackProbe ||
        (ExceptionCheck::GetData().handledExceptionType & ExceptionType_JavascriptException) == ExceptionType_JavascriptException);
    ExceptionCheck::ClearHandledExceptionType();
    ExceptionCheck::Restore(savedData);
}

AutoFilterExceptionRegion::AutoFilterExceptionRegion(ExceptionType e)
{
    savedData = ExceptionCheck::Save();
    ExceptionCheck::SetHandledExceptionType((ExceptionType)(~e & savedData.handledExceptionType));
}
AutoFilterExceptionRegion::~AutoFilterExceptionRegion()
{
    ExceptionCheck::ClearHandledExceptionType();
    ExceptionCheck::Restore(savedData);
}
#endif
