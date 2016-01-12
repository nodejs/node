//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Library\EngineInterfaceObject.h"
#include "Library\IntlEngineInterfaceExtensionObject.h"
#ifdef ENABLE_BASIC_TELEMETRY
#include "ScriptContextTelemetry.h"
#endif

namespace Js
{
    JavascriptDate::JavascriptDate(double value, DynamicType * type)
        : DynamicObject(type), m_date(value, type->GetScriptContext())
    {
        Assert(IsDateTypeId(type->GetTypeId()));
    }

    JavascriptDate::JavascriptDate(DynamicType * type)
        : DynamicObject(type), m_date(0, type->GetScriptContext())
    {
        Assert(type->GetTypeId() == TypeIds_Date);
    }

    bool JavascriptDate::Is(Var aValue)
    {
        // All WinRT Date's are also implicitly Javascript dates
        return IsDateTypeId(JavascriptOperators::GetTypeId(aValue));
    }

    JavascriptDate* JavascriptDate::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'Date'");

        return static_cast<JavascriptDate *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptDate::GetDateData(JavascriptDate* date, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {
        return JavascriptNumber::ToVarIntCheck(date->m_date.GetDateData(dd, false, scriptContext), scriptContext);
    }

    Var JavascriptDate::GetUTCDateData(JavascriptDate* date, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {
        return JavascriptNumber::ToVarIntCheck(date->m_date.GetDateData(dd, true, scriptContext), scriptContext);
    }

    Var JavascriptDate::SetDateData(JavascriptDate* date, Arguments args, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {
        return JavascriptNumber::ToVarNoCheck(date->m_date.SetDateData(args, dd, false, scriptContext), scriptContext);
    }

    Var JavascriptDate::SetUTCDateData(JavascriptDate* date, Arguments args, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {
        return JavascriptNumber::ToVarNoCheck(date->m_date.SetDateData(args, dd, true, scriptContext), scriptContext);
    }

    Var JavascriptDate::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        //
        // Determine if called as a constructor or a function.
        //

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        if (!(callInfo.Flags & CallFlags_New))
        {
            //
            // Called as a function.
            //

            //
            // Be sure the latest time zone info is loaded
            //

            // ES5 15.9.2.1: Date() should returns a string exactly the same as (new Date().toString()).
            JavascriptDate* pDate = NewInstanceAsConstructor(args, scriptContext, /* forceCurrentDate */ true);

            return JavascriptDate::ToString(pDate);
        }
        else
        {
            //
            // Called as a constructor.
            //
            RecyclableObject* pNew = NewInstanceAsConstructor(args, scriptContext);
            return isCtorSuperCall ?
                JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pNew, nullptr, scriptContext) :
                pNew;
        }
    }

    JavascriptDate* JavascriptDate::NewInstanceAsConstructor(Js::Arguments args, ScriptContext* scriptContext, bool forceCurrentDate)
    {
        Assert(scriptContext);

        double timeValue = 0;
        JavascriptDate* pDate;

        pDate = scriptContext->GetLibrary()->CreateDate();


        //
        // [15.9.3.3]
        // No arguments passed. Return the current time
        //
        if (forceCurrentDate || args.Info.Count == 1)
        {
            pDate->m_date.SetTvUtc(DateImplementation::NowFromHiResTimer(scriptContext));
            return pDate;
        }

        //
        // [15.9.3.2]
        // One argument given
        // If string parse it and use that timeValue
        // Else convert to Number and use that as timeValue
        //
        if (args.Info.Count == 2)
        {
            if (JavascriptDate::Is(args[1]))
            {
                JavascriptDate* dateObject = JavascriptDate::FromVar(args[1]);
                timeValue = ((dateObject)->m_date).m_tvUtc;
            }
            else
            {
                Var value = JavascriptConversion::ToPrimitive(args[1], Js::JavascriptHint::None, scriptContext);
                if (JavascriptString::Is(value))
                {
                    timeValue = ParseHelper(scriptContext, JavascriptString::FromVar(value));
                }
                else
                {
                    timeValue = JavascriptConversion::ToNumber(value, scriptContext);
                }
            }

            pDate->m_date.SetTvUtc(timeValue);
            return pDate;
        }

        //
        // [15.9.3.1]
        // Date called with two to seven arguments
        //

        const int parameterCount = 7;
        double values[parameterCount];

        for (uint i=1; i < args.Info.Count && i < parameterCount+1; i++)
        {
            double curr = JavascriptConversion::ToNumber(args[i], scriptContext);
            values[i-1] = curr;
            if (JavascriptNumber::IsNan(curr) || !NumberUtilities::IsFinite(curr))
            {
                pDate->m_date.SetTvUtc(curr);
                return pDate;
            }
        }

        for (uint i=0; i < parameterCount; i++)
        {
            if ( i >= args.Info.Count-1 )
            {
                values[i] = ( i == 2 );
                continue;
            }
            // MakeTime (ES5 15.9.1.11) && MakeDay (ES5 15.9.1.12) always
            // call ToInteger (which is same as JavascriptConversion::ToInteger) on arguments.
            // All are finite (not Inf or Nan) as we check them explicitly in the previous loop.
            // +-0 & +0 are same in this context.
#pragma prefast(suppress:6001, "value index i < args.Info.Count - 1 are initialized")
            values[i] = JavascriptConversion::ToInteger(values[i]);
        }

        // adjust the year
        if (values[0] < 100 && values[0] >= 0)
            values[0] += 1900;

        // Get the local time value.
        timeValue = DateImplementation::TvFromDate(values[0], values[1], values[2] - 1,
            values[3] * 3600000 + values[4] * 60000 + values[5] * 1000 + values[6]);

        // Set the time.
        pDate->m_date.SetTvLcl(timeValue);

        return pDate;
    }

    // Date.prototype[@@toPrimitive] as described in ES6 spec (Draft May 22, 2014) 20.3.4.45
    Var JavascriptDate::EntrySymbolToPrimitive(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // One argument given will be hint
        //The allowed values for hint are "default", "number", and "string"
        if (args.Info.Count == 2)
        {
            if (JavascriptString::Is(args[1]))
            {
                JavascriptString* StringObject = JavascriptString::FromVar(args[1]);

                if (wcscmp(StringObject->UnsafeGetBuffer(), L"default") == 0 || wcscmp(StringObject->UnsafeGetBuffer(), L"string") == 0)
                {
                    // Date objects, are unique among built-in ECMAScript object in that they treat "default" as being equivalent to "string"
                    // If hint is the string value "string" or the string value "default", then
                    // Let tryFirst be "string".
                    return JavascriptConversion::OrdinaryToPrimitive(args[0], JavascriptHint::HintString/*tryFirst*/, scriptContext);
                }
                // Else if hint is the string value "number", then
                // Let tryFirst be "number".
                else if(wcscmp(StringObject->UnsafeGetBuffer(), L"number") == 0)
                {
                    return JavascriptConversion::OrdinaryToPrimitive(args[0], JavascriptHint::HintNumber/*tryFirst*/, scriptContext);
                }
                //anything else should throw a type error
            }
        }
        else if (args.Info.Count == 1)
        {
            //7.1.1 ToPrimitive Note: Date objects treat no hint as if the hint were String.
            return JavascriptConversion::OrdinaryToPrimitive(args[0], JavascriptHint::HintString/*tryFirst*/, scriptContext);
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_Invalid, L"Date[Symbol.toPrimitive]");
    }

    Var JavascriptDate::EntryGetDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetDate, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getDate");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetDate();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetDay(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetDay, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getDay");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetDay();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetFullYear, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getFullYear");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetFullYear();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetYear, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getYear");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetYear();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetHours, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getHours");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetHours();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetMilliseconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getMilliseconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetDateMilliSeconds();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetMinutes, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getMinutes");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetMinutes();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetMonth, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getMonth");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetMonth();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetSeconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getSeconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {
            return date->m_date.GetSeconds();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetTime(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetTime, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getTime");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptNumber::ToVarNoCheck(date->GetTime(), scriptContext);
    }

    Var JavascriptDate::EntryGetTimezoneOffset(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetTimezoneOffset, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getTimezoneOffset");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetDateData(date, DateImplementation::DateData::TimezoneOffset, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCDate, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCDate");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Date, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCDay(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCDay, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCDay");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Day, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCFullYear, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCFullYear");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::FullYear, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCHours, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCHours");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Hours, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCMilliseconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCMilliseconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Milliseconds, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCMinutes, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCMinutes");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Minutes, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCMonth, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCMonth");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Month, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetUTCSeconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getUTCSeconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Seconds, scriptContext);
    }

    Var JavascriptDate::EntryGetVarDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryGetVarDate, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.getVarDate");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateVariantDate(
            DateImplementation::VarDateFromJsUtcTime(date->GetTime(), scriptContext)
            );
    }

    Var JavascriptDate::EntryParse(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        double dblRetVal = JavascriptNumber::NaN;

        if (args.Info.Count > 1)
        {
            // We convert to primitive value based on hint == String, which JavascriptConversion::ToString does.
            JavascriptString *pParseString = JavascriptConversion::ToString(args[1], scriptContext);
            dblRetVal = ParseHelper(scriptContext, pParseString);
        }

        return JavascriptNumber::ToVarNoCheck(dblRetVal,scriptContext);
    }

    Var JavascriptDate::EntryNow(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        double dblRetVal = DateImplementation::NowInMilliSeconds(scriptContext);
        return JavascriptNumber::ToVarNoCheck(dblRetVal,scriptContext);
    }

    Var JavascriptDate::EntryUTC(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        double dblRetVal = DateImplementation::DateFncUTC(scriptContext,args);
        return JavascriptNumber::ToVarNoCheck(dblRetVal,scriptContext);
    }

    double JavascriptDate::ParseHelper(ScriptContext *scriptContext, JavascriptString *str)
    {
#ifdef ENABLE_BASIC_TELEMETRY
        double milliseconds = -1;
        try
        {
            milliseconds = DateImplementation::UtcTimeFromStr(scriptContext, str);
            scriptContext->GetTelemetry().GetKnownMethodTelemetry().JavascriptDate_ParseHelper(scriptContext, str, milliseconds, false);
        }
        catch(...)
        {
            scriptContext->GetTelemetry().GetKnownMethodTelemetry().JavascriptDate_ParseHelper(scriptContext, str, milliseconds, true);
            throw;
        }
        return milliseconds;
#else
        return DateImplementation::UtcTimeFromStr(scriptContext, str);
#endif
    }

    Var JavascriptDate::EntrySetDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetDate, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setDate");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Date, scriptContext);
    }

    Var JavascriptDate::EntrySetFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetFullYear, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setFullYear");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::FullYear, scriptContext);
    }

    Var JavascriptDate::EntrySetYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetYear, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setYear");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Year, scriptContext);
    }

    Var JavascriptDate::EntrySetHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetHours, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setHours");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Hours, scriptContext);
    }

    Var JavascriptDate::EntrySetMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetMilliseconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setMilliseconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Milliseconds, scriptContext);
    }

    Var JavascriptDate::EntrySetMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetMinutes, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setMinutes");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Minutes, scriptContext);
    }

    Var JavascriptDate::EntrySetMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetMonth, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setMonth");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Month, scriptContext);
    }

    Var JavascriptDate::EntrySetSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetSeconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setSeconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Seconds, scriptContext);
    }

    Var JavascriptDate::EntrySetTime(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetTime, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setTime");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        double value;
        if (args.Info.Count > 1)
        {
            value = JavascriptConversion::ToNumber(args[1], scriptContext);
            if (Js::NumberUtilities::IsFinite(value))
            {
                value = JavascriptConversion::ToInteger(value);
            }
        }
        else
        {
            value = JavascriptNumber::NaN;
        }

        date->m_date.SetTvUtc(value);

        return JavascriptNumber::ToVarNoCheck(value, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetUTCDate, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setUTCDate");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Date, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetUTCFullYear, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setUTCFullYear");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::FullYear, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetUTCHours, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setUTCHours");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Hours, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetUTCMilliseconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setUTCMilliseconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Milliseconds, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetUTCMinutes, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setUTCMinutes");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Minutes, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetUTCMonth, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setUTCMonth");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Month, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntrySetUTCSeconds, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.setUTCSeconds");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Seconds, scriptContext);
    }

    Var JavascriptDate::EntryToDateString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToDateString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toDateString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Default,
            DateImplementation::DateTimeFlag::NoTime);
    }

    Var JavascriptDate::EntryToISOString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(DateToISOStringCount);

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToISOString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toISOString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetISOString();
    }

    Var JavascriptDate::EntryToJSON(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, L"Data.prototype.toJSON");
        }
        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"Date.prototype.toJSON");
        }

        Var result;
        if (TryInvokeRemotely(EntryToJSON, scriptContext, args, &result))
        {
            return result;
        }

        Var num = JavascriptConversion::ToPrimitive(thisObj, JavascriptHint::HintNumber, scriptContext);
        if (JavascriptNumber::Is(num)
            && !NumberUtilities::IsFinite(JavascriptNumber::GetValue(num)))
        {
            return scriptContext->GetLibrary()->GetNull();
        }

        Var toISO = JavascriptOperators::GetProperty(thisObj, PropertyIds::toISOString, scriptContext, NULL);
        if (!JavascriptConversion::IsCallable(toISO))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, scriptContext->GetPropertyName(PropertyIds::toISOString)->GetBuffer());
        }
        RecyclableObject* toISOFunc = RecyclableObject::FromVar(toISO);
        return toISOFunc->GetEntryPoint()(toISOFunc, 1, thisObj);
    }

    Var JavascriptDate::EntryToLocaleDateString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToLocaleDateString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toLocaleDateString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->GetConfig()->IsIntlEnabled()){

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {
                IntlEngineInterfaceExtensionObject* extensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = extensionObject->GetDateToLocaleDateString();
                if (func)
                {
                    return func->CallFunction(args);
                }

                // Initialize Date.prototype.toLocaleDateString
                scriptContext->GetLibrary()->InitializeIntlForDatePrototype();
                func = extensionObject->GetDateToLocaleDateString();
                if (func)
                {
                    return func->CallFunction(args);
                }
                AssertMsg(false, "Intl code didn't initialized Date.prototype.toLocaleDateString method.");
            }
        }
#endif

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Locale,
            DateImplementation::DateTimeFlag::NoTime);
    }

    Var JavascriptDate::EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToLocaleString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toLocaleString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->GetConfig()->IsIntlEnabled()){

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {
                IntlEngineInterfaceExtensionObject* extensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = extensionObject->GetDateToLocaleString();
                if (func)
                {
                    return func->CallFunction(args);
                }
                // Initialize Date.prototype.toLocaleString
                scriptContext->GetLibrary()->InitializeIntlForDatePrototype();
                func = extensionObject->GetDateToLocaleString();
                if (func)
                {
                    return func->CallFunction(args);
                }
                AssertMsg(false, "Intl code didn't initialized Date.prototype.toLocaleString method.");
            }
        }
#endif

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return JavascriptDate::ToLocaleString(date);
    }

    JavascriptString* JavascriptDate::ToLocaleString(JavascriptDate* date)
    {
        return date->m_date.GetString(DateImplementation::DateStringFormat::Locale);
    }

    JavascriptString* JavascriptDate::ToString(JavascriptDate* date)
    {
        Assert(date);
        return date->m_date.GetString(DateImplementation::DateStringFormat::Default);
    }

    Var JavascriptDate::EntryToLocaleTimeString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToLocaleTimeString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toLocaleTimeString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->GetConfig()->IsIntlEnabled()){

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {
                IntlEngineInterfaceExtensionObject* extensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = extensionObject->GetDateToLocaleTimeString();
                if (func)
                {
                    return func->CallFunction(args);
                }
                // Initialize Date.prototype.toLocaleTimeString
                scriptContext->GetLibrary()->InitializeIntlForDatePrototype();
                func = extensionObject->GetDateToLocaleTimeString();
                if (func)
                {
                    return func->CallFunction(args);
                }
                AssertMsg(false, "Intl code didn't initialized String.prototype.toLocaleTimeString method.");
            }
        }
#endif

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Locale,
            DateImplementation::DateTimeFlag::NoDate);
    }

    Var JavascriptDate::EntryToTimeString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToTimeString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toTimeString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);


        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Default,
            DateImplementation::DateTimeFlag::NoDate);
    }

    // CONSIDER: ToGMTString and ToUTCString is the same, but currently the profiler use the entry point address to identify
    // the entry point. So we will have to make the function different. Consider using FunctionInfo to identify the function
    Var JavascriptDate::EntryToGMTString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        return JavascriptFunction::CallFunction<true>(function, JavascriptDate::EntryToUTCString, args);
    }

    Var JavascriptDate::EntryToUTCString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToUTCString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toUTCString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::GMT,
            DateImplementation::DateTimeFlag::None);
    }

    Var JavascriptDate::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryValueOf, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.valueOf");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        double value = date->m_date.GetMilliSeconds();
        return JavascriptNumber::ToVarNoCheck(value, scriptContext);
    }

    Var JavascriptDate::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {
            Var result;
            if (TryInvokeRemotely(EntryToString, scriptContext, args, &result))
            {
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, L"Date.prototype.toString");
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return JavascriptDate::ToString(date);
    }

    BOOL JavascriptDate::TryInvokeRemotely(JavascriptMethod entryPoint, ScriptContext * scriptContext, Arguments & args, Var * result)
    {
        if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
        {
            if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(entryPoint, args, result))
            {
                return TRUE;
            }
        }
        return FALSE;
    }

    BOOL JavascriptDate::ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext)
    {
        if (hint == JavascriptHint::None)
        {
            hint = JavascriptHint::HintString;
        }

        return DynamicObject::ToPrimitive(hint, result, requestContext);
    }

    BOOL JavascriptDate::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        ENTER_PINNED_SCOPE(JavascriptString, valueStr);
        valueStr = this->m_date.GetString(DateImplementation::DateStringFormat::Default);
        stringBuilder->Append(valueStr->GetString(), valueStr->GetLength());
        LEAVE_PINNED_SCOPE();
        return TRUE;
    }

    BOOL JavascriptDate::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"Object, (Date)");
        return TRUE;
    }
} // namespace Js
