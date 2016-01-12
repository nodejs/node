//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "errstr.h"
#include "Library\EngineInterfaceObject.h"
#include "Library\IntlEngineInterfaceExtensionObject.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(JavascriptNumber);

    Var JavascriptNumber::ToVarNoCheck(double value, ScriptContext* scriptContext)
    {
        return JavascriptNumber::NewInlined(value, scriptContext);
    }

    Var JavascriptNumber::ToVarWithCheck(double value, ScriptContext* scriptContext)
    {
#if FLOATVAR
        if (IsNan(value))
        {
            value = JavascriptNumber::NaN;
        }
#endif
        return JavascriptNumber::NewInlined(value, scriptContext);
    }

    Var JavascriptNumber::ToVarInPlace(double value, ScriptContext* scriptContext, JavascriptNumber *result)
    {
        return InPlaceNew(value, scriptContext, result);
    }

    Var JavascriptNumber::ToVarInPlace(int64 value, ScriptContext* scriptContext, JavascriptNumber *result)
    {
        return InPlaceNew((double)value, scriptContext, result);
    }


    Var JavascriptNumber::ToVarMaybeInPlace(double value, ScriptContext* scriptContext, JavascriptNumber *result)
    {
        if (result)
        {
            return InPlaceNew(value, scriptContext, result);
        }

        return ToVarNoCheck(value, scriptContext);
    }

    Var JavascriptNumber::ToVarInPlace(int32 nValue, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {
        if (!TaggedInt::IsOverflow(nValue))
        {
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return InPlaceNew(static_cast<double>(nValue), scriptContext, result);
    }

    Var JavascriptNumber::ToVarInPlace(uint32 nValue, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {
        if (!TaggedInt::IsOverflow(nValue))
        {
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return InPlaceNew(static_cast<double>(nValue), scriptContext, result);
    }

    Var JavascriptNumber::ToVarIntCheck(double value,ScriptContext* scriptContext)
    {
        //
        // Check if a well-known value:
        // - This significantly cuts down on the below floating-point to integer conversions.
        //

        if (value == 0.0)
        {
            if(IsNegZero(value))
            {
                return scriptContext->GetLibrary()->GetNegativeZero();
            }
            return TaggedInt::ToVarUnchecked(0);
        }
        if (value == 1.0)
        {
            return TaggedInt::ToVarUnchecked(1);
        }

        //
        // Check if number can be reduced back into a TaggedInt:
        // - This avoids extra GC.
        //

        int nValue      = (int) value;
        double dblCheck = (double) nValue;
        if ((dblCheck == value) && (!TaggedInt::IsOverflow(nValue)))
        {
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return JavascriptNumber::NewInlined(value,scriptContext);
    }

    bool JavascriptNumber::TryGetInt32OrUInt32Value(const double value, int32 *const int32Value, bool *const isInt32)
    {
        Assert(int32Value);
        Assert(isInt32);

        if(value <= 0)
        {
            return *isInt32 = TryGetInt32Value(value, int32Value);
        }

        const uint32 i = static_cast<uint32>(value);
        if(static_cast<double>(i) != value)
        {
            return false;
        }

        *int32Value = i;
        *isInt32 = static_cast<int32>(i) >= 0;
        return true;
    }

    bool JavascriptNumber::IsInt32(const double value)
    {
        int32 i;
        return TryGetInt32Value(value, &i);
    }

    bool JavascriptNumber::IsInt32OrUInt32(const double value)
    {
        int32 i;
        bool isInt32;
        return TryGetInt32OrUInt32Value(value, &i, &isInt32);
    }

    bool JavascriptNumber::IsInt32_NoChecks(const Var number)
    {
        Assert(number);
        Assert(Is(number));

        return IsInt32(GetValue(number));
    }

    bool JavascriptNumber::IsInt32OrUInt32_NoChecks(const Var number)
    {
        Assert(number);
        Assert(Is(number));

        return IsInt32OrUInt32(GetValue(number));
    }

    int32 JavascriptNumber::GetNonzeroInt32Value_NoTaggedIntCheck(const Var object)
    {
        Assert(object);
        Assert(!TaggedInt::Is(object));

        int32 i;
        return Is_NoTaggedIntCheck(object) && TryGetInt32Value(GetValue(object), &i) ? i : 0;
    }

#if _M_IX86

    extern "C" double __cdecl __libm_sse2_pow(double, double);

    static const double d1_0 = 1.0;

    __declspec(naked)
    double JavascriptNumber::DirectPow(double x, double y)
    {
        UNREFERENCED_PARAMETER(x);
        UNREFERENCED_PARAMETER(y);

        // This function is called directly from jitted, float-preferenced code.
        // It looks for x and y in xmm0 and xmm1 and returns the result in xmm0.
        // Check for pow(1, Infinity/NaN) and return NaN in that case; otherwise,
        // go to the fast CRT helper.
        __asm {
            // check y for 1.0
            ucomisd xmm1, d1_0
            jne pow_full
            jp pow_full
            ret
       pow_full:
            // Check y for non-finite value
            pextrw eax, xmm1, 3
            not eax
            test eax, 0x7ff0
            je y_infinite

       normal:
            jmp dword ptr [__libm_sse2_pow]

        y_infinite:
            // Check for |x| == 1
            movsd xmm2, xmm0
            andpd xmm2, AbsDoubleCst
            movsd xmm3, d1_0
            ucomisd xmm2, xmm3
            lahf
            test ah, 68
            jp normal

            movsd xmm0, JavascriptNumber::k_Nan
            ret
        }
    }


#elif defined(_M_AMD64) || defined(_M_ARM32_OR_ARM64)

    double JavascriptNumber::DirectPow(double x, double y)
    {
        if(y == 1.0)
        {
            return x;
        }

        // For AMD64/ARM calling convention already uses SSE2/VFP registers so we don't have to use assembler.
        // We can't just use "if (0 == y)" because NaN compares
        // equal to 0 according to our compilers.
        int32 intY;
        if (0 == NumberUtilities::LuLoDbl(y) && 0 == (NumberUtilities::LuHiDbl(y) & 0x7FFFFFFF))
        {
            // pow(x, 0) = 1 even if x is NaN.
            return 1;
        }
        else if (1.0 == fabs(x) && !NumberUtilities::IsFinite(y))
        {
            // pow([+/-] 1, Infinity) = NaN according to javascript, but not for CRT pow.
            return JavascriptNumber::NaN;
        }
        else if(TryGetInt32Value(y, &intY))
        {
            return ::pow(x, intY);
        }

        return ::pow(x, y);
    }

#else

    double JavascriptNumber::DirectPow(double x, double y)
    {
        UNREFERENCED_PARAMETER(x);
        UNREFERENCED_PARAMETER(y);

        AssertMsg(0, "DirectPow NYI");
        return 0;
    }

#endif

    Var JavascriptNumber::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        //
        // Determine if called as a constructor or a function.
        //

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        Var result;

        if (args.Info.Count > 1)
        {
            if (TaggedInt::Is(args[1]) || JavascriptNumber::Is(args[1]))
            {
                result = args[1];
            }
            else if (JavascriptNumberObject::Is(args[1]))
            {
                result = JavascriptNumber::ToVarNoCheck(JavascriptNumberObject::FromVar(args[1])->GetValue(), scriptContext);
            }
            else
            {
                result = JavascriptNumber::ToVarNoCheck(JavascriptConversion::ToNumber(args[1], scriptContext), scriptContext);
            }
        }
        else
        {
            result = TaggedInt::ToVarUnchecked(0);
        }

        if (callInfo.Flags & CallFlags_New)
        {
            JavascriptNumberObject* obj = scriptContext->GetLibrary()->CreateNumberObject(result);
            result = obj;
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(result), nullptr, scriptContext) :
            result;
    }

    ///----------------------------------------------------------------------------
    /// ParseInt() returns an integer value dictated by the interpretation of the
    /// given string argument according to the given radix argument, as described
    /// in (ES6.0: S20.1.2.13).
    ///
    /// Note: This is the same as the global parseInt() function as described in
    ///       (ES6.0: S18.2.5)
    ///
    /// We actually reuse GlobalObject::EntryParseInt, so no implementation here.
    ///----------------------------------------------------------------------------
    //Var JavascriptNumber::EntryParseInt(RecyclableObject* function, CallInfo callInfo, ...)

    ///----------------------------------------------------------------------------
    /// ParseFloat() returns a Number value dictated by the interpretation of the
    /// given string argument as a decimal literal, as described in
    /// (ES6.0: S20.1.2.12).
    ///
    /// Note: This is the same as the global parseFloat() function as described in
    ///       (ES6.0: S18.2.4)
    ///
    /// We actually reuse GlobalObject::EntryParseFloat, so no implementation here.
    ///----------------------------------------------------------------------------
    //Var JavascriptNumber::EntryParseFloat(RecyclableObject* function, CallInfo callInfo, ...)

    ///----------------------------------------------------------------------------
    /// IsNaN() return true if the given value is a Number value and is NaN, as
    /// described in (ES6.0: S20.1.2.4).
    /// Note: This is the same as the global isNaN() function as described in
    ///       (ES6.0: S18.2.3) except that it does not coerce the argument to
    ///       Number, instead returns false if not already a Number.
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsNaN(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(IsNaNCount);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        return JavascriptBoolean::ToVar(
            JavascriptNumber::IsNan(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// IsFinite() returns true if the given value is a Number value and is not
    /// one of NaN, +Infinity, or -Infinity, as described in (ES6.0: S20.1.2.2).
    /// Note: This is the same as the global isFinite() function as described in
    ///       (ES6.0: S18.2.2) except that it does not coerce the argument to
    ///       Number, instead returns false if not already a Number.
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsFinite(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(IsFiniteCount);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        return JavascriptBoolean::ToVar(
            NumberUtilities::IsFinite(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// IsInteger() returns true if the given value is a Number value and is an
    /// integer, as described in (ES6.0: S20.1.2.3).
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsInteger(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(IsIntegerCount);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        double number = JavascriptConversion::ToNumber(args[1], scriptContext);

        return JavascriptBoolean::ToVar(
            number == JavascriptConversion::ToInteger(args[1], scriptContext) &&
            NumberUtilities::IsFinite(number),
            scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// IsSafeInteger() returns true if the given value is a Number value and is an
    /// integer, as described in (ES6.0: S20.1.2.5).
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsSafeInteger(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(IsSafeIntegerCount);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {
            return scriptContext->GetLibrary()->GetFalse();
        }

        double number = JavascriptConversion::ToNumber(args[1], scriptContext);

        return JavascriptBoolean::ToVar(
            number == JavascriptConversion::ToInteger(args[1], scriptContext) &&
            number >= Js::Math::MIN_SAFE_INTEGER && number <= Js::Math::MAX_SAFE_INTEGER,
            scriptContext);
    }

    Var JavascriptNumber::EntryToExponential(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toExponential");
        }

        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToExp is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToExponential, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toExponential");
        }

        JavascriptString * nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
            return nanF;

        // If the Fraction param. is not present we have to output as many fractional digits as we can
        int fractionDigits = -1;

        if(args.Info.Count > 1)
        {
            //use the first arg as the fraction digits, ignore the rest.
            Var aFractionDigits = args[1];

            // shortcut for tagged int's
            if(TaggedInt::Is(aFractionDigits))
            {
                fractionDigits = TaggedInt::ToInt32(aFractionDigits);
            }
            else if(JavascriptOperators::GetTypeId(aFractionDigits) == TypeIds_Undefined)
                ; // fraction digits = -1
            else
            {
                fractionDigits = (int)JavascriptConversion::ToInteger(aFractionDigits, scriptContext);
            }
            if( fractionDigits < 0 || fractionDigits >20 )
            {
                JavascriptError::ThrowRangeError(scriptContext, JSERR_FractionOutOfRange);
            }
        }

        return FormatDoubleToString(value, Js::NumberUtilities::FormatExponential, fractionDigits, scriptContext);
    }

    Var JavascriptNumber::EntryToFixed(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toFixed");
        }
        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToFixed is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToFixed, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toFixed");
        }
        int fractionDigits = 0;
        bool isFractionDigitsInfinite = false;
        if(args.Info.Count > 1)
        {
            //use the first arg as the fraction digits, ignore the rest.
            Var aFractionDigits = args[1];

            // shortcut for tagged int's
            if(TaggedInt::Is(aFractionDigits))
            {
                fractionDigits = TaggedInt::ToInt32(aFractionDigits);
            }
            else if(JavascriptOperators::GetTypeId(aFractionDigits) == TypeIds_Undefined)
            {
                // fraction digits = 0
            }
            else
            {
                double fractionDigitsRaw = JavascriptConversion::ToInteger(aFractionDigits, scriptContext);
                isFractionDigitsInfinite =
                    fractionDigitsRaw == JavascriptNumber::NEGATIVE_INFINITY ||
                    fractionDigitsRaw == JavascriptNumber::POSITIVE_INFINITY;
                fractionDigits = (int)fractionDigitsRaw;
            }
        }

        if (fractionDigits < 0 || fractionDigits > 20)
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FractionOutOfRange);
        }

        if(IsNan(value))
        {
            return ToStringNan(scriptContext);
        }
        if(value >= 1e21)
        {
            return ToStringRadix10(value, scriptContext);
        }

        return FormatDoubleToString(value, NumberUtilities::FormatFixed, fractionDigits, scriptContext);

    }

    Var JavascriptNumber::EntryToPrecision(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toPrecision");
        }
        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToPrec is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToPrecision, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toPrecision");
        }
        if(args.Info.Count < 2 || JavascriptOperators::GetTypeId(args[1]) == TypeIds_Undefined)
        {
            return JavascriptConversion::ToString(args[0], scriptContext);
        }

        int precision;
        Var aPrecision = args[1];
        if(TaggedInt::Is(aPrecision))
        {
            precision = TaggedInt::ToInt32(aPrecision);
        }
        else
        {
            precision = (int) JavascriptConversion::ToInt32(aPrecision, scriptContext);
        }

        JavascriptString * nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
        {
            return nanF;
        }

        if(precision < 1 || precision > 21)
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_PrecisionOutOfRange);
        }
        return FormatDoubleToString(value, NumberUtilities::FormatPrecision, precision, scriptContext);
    }

    Var JavascriptNumber::EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toLocaleString");
        }

#ifdef ENABLE_INTL_OBJECT
        if(CONFIG_FLAG(IntlBuiltIns) && scriptContext->GetConfig()->IsIntlEnabled()){

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {
                IntlEngineInterfaceExtensionObject* intlExtenionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = intlExtenionObject->GetNumberToLocaleString();
                if (func)
                {
                    return func->CallFunction(args);
                }
                // Initialize Number.prototype.toLocaleString
                scriptContext->GetLibrary()->InitializeIntlForNumberPrototype();
                func = intlExtenionObject->GetNumberToLocaleString();
                if (func)
                {
                    return func->CallFunction(args);
                }
                AssertMsg(false, "Intl code didn't initialized Number.prototype.toLocaleString method.");
            }
        }
#endif

        double value;
        if (!GetThisValue(args[0], &value))
        {
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToLocaleString, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toLocaleString");
        }

        return JavascriptNumber::ToLocaleString(value, scriptContext);
    }

    Var JavascriptNumber::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toString");
        }

        // Optimize base 10 of TaggedInt numbers
        if (TaggedInt::Is(args[0]) && (args.Info.Count == 1 || (TaggedInt::Is(args[1]) && TaggedInt::ToInt32(args[1]) == 10)))
        {
            return scriptContext->GetIntegerString(args[0]);
        }

        double value;
        if (!GetThisValue(args[0], &value))
        {
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToString, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.toString");
        }

        int radix = 10;
        if(args.Info.Count > 1)
        {
            //use the first arg as the radix, ignore the rest.
            Var aRadix = args[1];

           // shortcut for tagged int's
            if(TaggedInt::Is(aRadix))
            {
                radix = TaggedInt::ToInt32(aRadix);
            }
            else if(!JavascriptOperators::GetTypeId(aRadix) == TypeIds_Undefined)
            {
                radix = (int)JavascriptConversion::ToInteger(aRadix,scriptContext);
            }

        }
        if(10 == radix)
        {
            return ToStringRadix10(value, scriptContext);
        }

        if( radix < 2 || radix >36 )
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FunctionArgument_Invalid, L"Number.prototype.toString");
        }

        return ToStringRadixHelper(value, radix, scriptContext);
    }

    Var JavascriptNumber::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var value = args[0];

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.valueOf");
        }

        //avoid creation of a new Number
        if (TaggedInt::Is(value) || JavascriptNumber::Is_NoTaggedIntCheck(value))
        {
            return value;
        }
        else if (JavascriptNumberObject::Is(value))
        {
            JavascriptNumberObject* obj = JavascriptNumberObject::FromVar(value);
            return CrossSite::MarshalVar(scriptContext, obj->Unwrap());
        }
        else if (Js::JavascriptOperators::GetTypeId(value) == TypeIds_Int64Number)
        {
            return value;
        }
        else if (Js::JavascriptOperators::GetTypeId(value) == TypeIds_UInt64Number)
        {
            return value;
        }
        else
        {
            if (JavascriptOperators::GetTypeId(value) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(value)->InvokeBuiltInOperationRemotely(EntryValueOf, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, L"Number.prototype.valueOf");
        }
    }

    static const int bufSize = 256;

    JavascriptString* JavascriptNumber::ToString(double value, ScriptContext* scriptContext)
    {
        wchar_t szBuffer[bufSize];
        int cchWritten = swprintf_s(szBuffer, _countof(szBuffer), L"%g", value);

        return JavascriptString::NewCopyBuffer(szBuffer, cchWritten, scriptContext);
    }

    JavascriptString* JavascriptNumber::ToStringNanOrInfiniteOrZero(double value, ScriptContext* scriptContext)
    {
        JavascriptString* nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
        {
            return nanF;
        }

        if (IsZero(value))
        {
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForCharA('0');
        }

        return nullptr;
    }

    JavascriptString* JavascriptNumber::ToStringRadix10(double value, ScriptContext* scriptContext)
    {
        JavascriptString* string = ToStringNanOrInfiniteOrZero(value, scriptContext);
        if (string != nullptr)
        {
            return string;
        }

        string = scriptContext->GetLastNumberToStringRadix10(value);
        if (string == nullptr)
        {
            wchar_t szBuffer[bufSize];

            if(!Js::NumberUtilities::FNonZeroFiniteDblToStr(value, szBuffer, bufSize))
            {
                Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }
            string = JavascriptString::NewCopySz(szBuffer, scriptContext);
            scriptContext->SetLastNumberToStringRadix10(value, string);
        }
        return string;
    }

    JavascriptString* JavascriptNumber::ToStringRadixHelper(double value, int radix, ScriptContext* scriptContext)
    {
        Assert(radix != 10);
        Assert(radix >= 2 && radix <= 36);

        JavascriptString* string = ToStringNanOrInfiniteOrZero(value, scriptContext);
        if (string != nullptr)
        {
            return string;
        }

        wchar_t szBuffer[bufSize];

        if (!Js::NumberUtilities::FNonZeroFiniteDblToStr(value, radix, szBuffer, _countof(szBuffer)))
        {
            Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
        }

        return JavascriptString::NewCopySz(szBuffer, scriptContext);
    }

    BOOL JavascriptNumber::GetThisValue(Var aValue, double* pDouble)
    {
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);

        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {
            return FALSE;
        }

        if (TaggedInt::Is(aValue))
        {
            *pDouble = TaggedInt::ToDouble(aValue);
            return TRUE;
        }
        else if (Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_Int64Number)
        {
            *pDouble = (double)JavascriptInt64Number::FromVar(aValue)->GetValue();
            return TRUE;
        }
        else if (Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_UInt64Number)
        {
            *pDouble = (double)JavascriptUInt64Number::FromVar(aValue)->GetValue();
            return TRUE;
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(aValue))
        {
            *pDouble = JavascriptNumber::GetValue(aValue);
            return TRUE;
        }
        else if (JavascriptNumberObject::Is(aValue))
        {
            JavascriptNumberObject* obj = JavascriptNumberObject::FromVar(aValue);
            *pDouble = obj->GetValue();
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }

    JavascriptString* JavascriptNumber::ToLocaleStringNanOrInfinite(double value, ScriptContext* scriptContext)
    {
        if (!NumberUtilities::IsFinite(value))
        {
            if (IsNan(value))
            {
                return ToStringNan(scriptContext);
            }

            BSTR bstr = nullptr;
            if (IsPosInf(value))
            {
                bstr = BstrGetResourceString(IDS_INFINITY);
            }
            else
            {
                AssertMsg(IsNegInf(value), "bad handling of infinite number");
                bstr = BstrGetResourceString(IDS_MINUSINFINITY);
            }

            if (bstr == nullptr)
            {
                Js::JavascriptError::ThrowTypeError(scriptContext, VBSERR_InternalError);
            }
            JavascriptString* str = JavascriptString::NewCopyBuffer(bstr, SysStringLen(bstr), scriptContext);
            SysFreeString(bstr);
            return str;
        }
        return nullptr;
    }

    JavascriptString* JavascriptNumber::ToLocaleString(double value, ScriptContext* scriptContext)
    {
        WCHAR   szRes[bufSize];
        WCHAR * pszRes = NULL;
        WCHAR * pszToBeFreed = NULL;
        int     count;

        if (!Js::NumberUtilities::IsFinite(value))
        {
            //
            // +- Infinity : use the localized string
            // NaN would be returned as NaN
            //
            return ToLocaleStringNanOrInfinite(value, scriptContext);
        }

        JavascriptString *result = nullptr;

        JavascriptString *dblStr = JavascriptString::FromVar(FormatDoubleToString(value, NumberUtilities::FormatFixed, -1, scriptContext));
        const wchar_t* szValue = dblStr->GetSz();

        count = GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, szValue, NULL, NULL, 0);

        if( count <= 0 )
        {
            return dblStr;
        }
        else
        {
            if( count > bufSize )
            {
                pszRes = pszToBeFreed = HeapNewArray(wchar_t, count);
            }
            else
            {
                pszRes = szRes;
            }

            int newCount = GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, szValue, NULL, pszRes, count);

            if (newCount <= 0 )
            {
                AssertMsg(false, "GetNumberFormatEx failed");
                JavascriptError::ThrowError(scriptContext, VBSERR_InternalError);
            }
            else
            {
                result = JavascriptString::NewCopySz(pszRes, scriptContext);
            }
        }

        if (pszToBeFreed)
        {
            HeapDeleteArray(count, pszToBeFreed);
        }

        return result;
    }

    Var JavascriptNumber::CloneToScriptContext(Var aValue, ScriptContext* requestContext)
    {
        return JavascriptNumber::New(JavascriptNumber::GetValue(aValue), requestContext);
    }

#if !FLOATVAR
    Var JavascriptNumber::BoxStackNumber(Var instance, ScriptContext* scriptContext)
    {
        if (ThreadContext::IsOnStack(instance) && JavascriptNumber::Is(instance))
        {
            return BoxStackInstance(JavascriptNumber::FromVar(instance), scriptContext);
        }
        else
        {
            return instance;
        }
    }

    Var JavascriptNumber::BoxStackInstance(Var instance, ScriptContext* scriptContext)
    {
        Assert(ThreadContext::IsOnStack(instance));
        double value = JavascriptNumber::FromVar(instance)->GetValue();
        return JavascriptNumber::New(value, scriptContext);
    }

    JavascriptNumber * JavascriptNumber::NewUninitialized(Recycler * recycler)
    {
        return RecyclerNew(recycler, JavascriptNumber, VirtualTableInfoCtorValue);
    }
#endif
}
