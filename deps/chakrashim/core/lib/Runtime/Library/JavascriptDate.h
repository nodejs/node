//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptDate : public DynamicObject
    {
    protected:
        DateImplementation m_date;

        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptDate, DynamicObject, m_date);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptDate);

    public:
        JavascriptDate(double value, DynamicType * type);
        JavascriptDate(DynamicType * type);

        static bool Is(Var aValue);

        double GetTime() { return m_date.GetMilliSeconds(); }
        static JavascriptDate* FromVar(Var aValue);

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo GetDate;
            static FunctionInfo GetDay;
            static FunctionInfo GetFullYear;
            static FunctionInfo GetYear;
            static FunctionInfo GetHours;
            static FunctionInfo GetMilliseconds;
            static FunctionInfo GetMinutes;
            static FunctionInfo GetMonth;
            static FunctionInfo GetSeconds;
            static FunctionInfo GetTime;
            static FunctionInfo GetTimezoneOffset;
            static FunctionInfo GetUTCDate;
            static FunctionInfo GetUTCDay;
            static FunctionInfo GetUTCFullYear;
            static FunctionInfo GetUTCHours;
            static FunctionInfo GetUTCMilliseconds;
            static FunctionInfo GetUTCMinutes;
            static FunctionInfo GetUTCMonth;
            static FunctionInfo GetUTCSeconds;
            static FunctionInfo GetVarDate;
            static FunctionInfo Now;
            static FunctionInfo Parse;
            static FunctionInfo SetDate;
            static FunctionInfo SetFullYear;
            static FunctionInfo SetYear;
            static FunctionInfo SetHours;
            static FunctionInfo SetMilliseconds;
            static FunctionInfo SetMinutes;
            static FunctionInfo SetMonth;
            static FunctionInfo SetSeconds;
            static FunctionInfo SetTime;
            static FunctionInfo SetUTCDate;
            static FunctionInfo SetUTCFullYear;
            static FunctionInfo SetUTCHours;
            static FunctionInfo SetUTCMilliseconds;
            static FunctionInfo SetUTCMinutes;
            static FunctionInfo SetUTCMonth;
            static FunctionInfo SetUTCSeconds;
            static FunctionInfo ToDateString;
            static FunctionInfo ToISOString;
            static FunctionInfo ToJSON;
            static FunctionInfo ToLocaleDateString;
            static FunctionInfo ToLocaleString;
            static FunctionInfo ToLocaleTimeString;
            static FunctionInfo ToString;
            static FunctionInfo ToTimeString;
            static FunctionInfo ToUTCString;
            static FunctionInfo ToGMTString;
            static FunctionInfo UTC;
            static FunctionInfo ValueOf;
            static FunctionInfo SymbolToPrimitive;
        };
        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetDate(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetDay(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetFullYear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetYear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetHours(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetMilliseconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetMinutes(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetMonth(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetSeconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetTime(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetTimezoneOffset(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCDate(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCDay(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCFullYear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCHours(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCMilliseconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCMinutes(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCMonth(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetUTCSeconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetVarDate(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryNow(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryParse(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetDate(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetFullYear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetYear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetHours(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetMilliseconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetMinutes(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetMonth(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetSeconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetTime(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUTCDate(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUTCFullYear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUTCHours(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUTCMilliseconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUTCMinutes(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUTCMonth(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySetUTCSeconds(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToDateString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToISOString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToJSON(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToLocaleDateString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToLocaleTimeString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToTimeString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToUTCString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToGMTString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryUTC(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySymbolToPrimitive(RecyclableObject* function, CallInfo callInfo, ...);

        static JavascriptString* ToLocaleString(JavascriptDate* date);
        static JavascriptString* ToString(JavascriptDate* date);

        virtual BOOL ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

    private:
        static Var GetDateData(JavascriptDate* date, DateImplementation::DateData dd, ScriptContext* scriptContext);
        static Var GetUTCDateData(JavascriptDate* date, DateImplementation::DateData dd, ScriptContext* scriptContext);
        static Var SetDateData(JavascriptDate* date, Arguments args, DateImplementation::DateData dd, ScriptContext* scriptContext);
        static Var SetUTCDateData(JavascriptDate* date, Arguments args, DateImplementation::DateData dd, ScriptContext* scriptContext);

        static double ParseHelper(ScriptContext *scriptContext, JavascriptString *str);
        static JavascriptDate* NewInstanceAsConstructor(Arguments args, ScriptContext* scriptContext, bool forceCurrentDate = false);

        static BOOL TryInvokeRemotely(JavascriptMethod entryPoint, ScriptContext * scriptContext, Arguments & args, Var * result);
    };

} // namespace Js
