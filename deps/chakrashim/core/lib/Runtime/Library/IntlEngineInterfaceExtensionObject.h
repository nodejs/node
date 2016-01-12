//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#ifdef ENABLE_INTL_OBJECT
namespace Js
{
    enum IntlInitializationType : USHORT
    {
        Intl = 0,
        StringPrototype,
        DatePrototype,
        NumberPrototype,
        Classifier
    };

    class IntlEngineInterfaceExtensionObject : public EngineExtensionObjectBase
    {
    public:
        IntlEngineInterfaceExtensionObject(ScriptContext* scriptContext);
        void Initialize();
        void InjectIntlLibraryCode(_In_ ScriptContext * scriptContext, DynamicObject* intlObject, IntlInitializationType intlInitializationType);

        JavascriptFunction* GetDateToLocaleString() { return dateToLocaleString; }
        JavascriptFunction* GetDateToLocaleTimeString() { return dateToLocaleTimeString; }
        JavascriptFunction* GetDateToLocaleDateString() { return dateToLocaleDateString; }
        JavascriptFunction* GetNumberToLocaleString() { return numberToLocaleString; }
        JavascriptFunction* GetStringLocaleCompare() { return stringLocaleCompare; }
        static void __cdecl InitializeIntlNativeInterfaces(DynamicObject* intlNativeInterfaces, DeferredTypeHandlerBase * typeHandler, DeferredInitializeMode mode);

#if DBG
        void DumpByteCode() override;
#endif
        class EntryInfo
        {
        public:
            static NoProfileFunctionInfo Intl_RaiseAssert;

            static NoProfileFunctionInfo Intl_IsWellFormedLanguageTag;
            static NoProfileFunctionInfo Intl_NormalizeLanguageTag;
            static NoProfileFunctionInfo Intl_ResolveLocaleLookup;
            static NoProfileFunctionInfo Intl_ResolveLocaleBestFit;
            static NoProfileFunctionInfo Intl_GetDefaultLocale;
            static NoProfileFunctionInfo Intl_GetExtensions;
            static NoProfileFunctionInfo Intl_CompareString;
            static NoProfileFunctionInfo Intl_CurrencyDigits;
            static NoProfileFunctionInfo Intl_FormatNumber;

            static NoProfileFunctionInfo Intl_CacheNumberFormat;
            static NoProfileFunctionInfo Intl_CreateDateTimeFormat;

            static NoProfileFunctionInfo Intl_BestFitFormatter;
            static NoProfileFunctionInfo Intl_LookupMatcher;
            static NoProfileFunctionInfo Intl_FormatDateTime;
            static NoProfileFunctionInfo Intl_ValidateAndCanonicalizeTimeZone;
            static NoProfileFunctionInfo Intl_GetDefaultTimeZone;
            static NoProfileFunctionInfo Intl_GetPatternForLocale;

            static NoProfileFunctionInfo Intl_RegisterBuiltInFunction;
            static NoProfileFunctionInfo Intl_GetHiddenObject;
            static NoProfileFunctionInfo Intl_SetHiddenObject;
            static NoProfileFunctionInfo Intl_BuiltIn_SetPrototype;
            static NoProfileFunctionInfo Intl_BuiltIn_GetArrayLength;
            static NoProfileFunctionInfo Intl_BuiltIn_RegexMatch;
            static NoProfileFunctionInfo Intl_BuiltIn_CallInstanceFunction;
        };

        static Var EntryIntl_RaiseAssert(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryIntl_IsWellFormedLanguageTag(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_NormalizeLanguageTag(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_ResolveLocaleLookup(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_ResolveLocaleBestFit(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_GetDefaultLocale(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_GetExtensions(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_CompareString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_CurrencyDigits(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_FormatNumber(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryIntl_CacheNumberFormat(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_CreateDateTimeFormat(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryIntl_FormatDateTime(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_ValidateAndCanonicalizeTimeZone(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_GetDefaultTimeZone(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryIntl_RegisterBuiltInFunction(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_GetHiddenObject(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIntl_SetHiddenObject(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryIntl_BuiltIn_SetPrototype(RecyclableObject *function, CallInfo callInfo, ...);
        static Var EntryIntl_BuiltIn_GetArrayLength(RecyclableObject *function, CallInfo callInfo, ...);
        static Var EntryIntl_BuiltIn_RegexMatch(RecyclableObject *function, CallInfo callInfo, ...);
        static Var EntryIntl_BuiltIn_CallInstanceFunction(RecyclableObject *function, CallInfo callInfo, ...);

    private:
        JavascriptFunction* dateToLocaleString;
        JavascriptFunction* dateToLocaleTimeString;
        JavascriptFunction* dateToLocaleDateString;
        JavascriptFunction* numberToLocaleString;
        JavascriptFunction* stringLocaleCompare;

        DynamicObject* intlNativeInterfaces;
        FunctionBody* intlByteCode;

        bool wasInitialized;
        void EnsureIntlByteCode(_In_ ScriptContext * scriptContext);
        static void deletePrototypePropertyHelper(ScriptContext* scriptContext, DynamicObject* intlObject, Js::PropertyId objectPropertyId, Js::PropertyId getterFunctionId);
        static WindowsGlobalizationAdapter* GetWindowsGlobalizationAdapter(_In_ ScriptContext*);
        static void prepareWithFractionIntegerDigits(ScriptContext* scriptContext, Windows::Globalization::NumberFormatting::INumberRounderOption* rounderOptions,
            Windows::Globalization::NumberFormatting::INumberFormatterOptions* formatterOptions, uint16 minFractionDigits, uint16 maxFractionDigits, uint16 minIntegerDigits);
        static void prepareWithSignificantDigits(ScriptContext* scriptContext, Windows::Globalization::NumberFormatting::INumberRounderOption* rounderOptions, Windows::Globalization::NumberFormatting::INumberFormatter *numberFormatter,
            Windows::Globalization::NumberFormatting::INumberFormatterOptions* formatterOptions, uint16 minSignificantDigits, uint16 maxSignificantDigits);

        void cleanUpIntl(ScriptContext* scriptContext, DynamicObject* intlObject);

    };
}
#endif
