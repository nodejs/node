//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_ES6_CHAR_CLASSIFIER)

#include "strsafe.h"

#define __WRL_ASSERT__(cond) Assert(cond)

#include <wrl\implements.h>

#ifdef NTBUILD
using namespace Windows::Globalization;
using namespace Windows::Foundation::Collections;
#else
using namespace ABI::Windows::Globalization;
using namespace ABI::Windows::Foundation::Collections;
#endif

#define IfFailThrowHr(op) \
    if (FAILED(hr=(op))) \
    { \
        JavascriptError::MapAndThrowError(scriptContext, hr);\
    } \

#define IfNullReturnError(EXPR, ERROR) do { if (!(EXPR)) { return (ERROR); } } while(FALSE)
#define IfFailedReturn(EXPR) do { hr = (EXPR); if (FAILED(hr)) { return hr; }} while(FALSE)
#define IfFailedSetErrorCodeAndReturn(EXPR, hrVariable) do { hr = (EXPR); if (FAILED(hr)) { hrVariable = hr; return hr; }} while(FALSE)
#define IfFailedGoLabel(expr, label) if (FAILED(expr)) { goto label; }
#define IfFailedGo(expr) IfFailedGoLabel(expr, LReturn)

namespace Js
{
#ifdef ENABLE_INTL_OBJECT
    class HSTRINGIterator : public Microsoft::WRL::RuntimeClass<IIterator<HSTRING>>
    {

        HSTRING *items;
        uint32 length;
        boolean hasMore;
        uint32 currentPosition;

    public:
        HRESULT RuntimeClassInitialize(HSTRING *items, uint32 length)
        {
            this->items = items;
            this->currentPosition = 0;
            this->length = length;
            this->hasMore = currentPosition < this->length;

            return S_OK;
        }
        ~HSTRINGIterator()
        {
        }

        // IIterator
        IFACEMETHODIMP get_Current(_Out_ HSTRING *current)
        {
            if (current != nullptr)
            {
                if (hasMore)
                {
                    return WindowsDuplicateString(items[currentPosition], current);
                }
                else
                {
                    *current = nullptr;
                }
            }
            return E_BOUNDS;
        }

        IFACEMETHODIMP get_HasCurrent(_Out_ boolean *hasCurrent)
        {
            if (hasCurrent != nullptr)
            {
                *hasCurrent = hasMore;
            }
            return S_OK;
        }

        IFACEMETHODIMP MoveNext(_Out_opt_ boolean *hasCurrent) sealed
        {
            this->currentPosition++;

            this->hasMore = this->currentPosition < this->length;
            if (hasCurrent != nullptr)
            {
                *hasCurrent = hasMore;
            }
            return S_OK;
        }

        IFACEMETHODIMP GetMany(_In_ unsigned capacity,
                               _Out_writes_to_(capacity,*actual) HSTRING *value,
                               _Out_ unsigned *actual)
        {
            uint count = 0;
            while (this->hasMore)
            {
                if (count == capacity)
                {
                    break;
                }
                if (value != nullptr)
                {
                    get_Current(value + count);
                }

                count ++;
                this->MoveNext(nullptr);
            }
            if (actual != nullptr)
            {
                *actual = count;
            }

            return S_OK;
        }
        IFACEMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeName) sealed
        {
            *runtimeName = nullptr;
            HRESULT hr = S_OK;
            const wchar_t *name = L"Js.HSTRINGIterator";

            hr = WindowsCreateString(name, static_cast<UINT32>(wcslen(name)), runtimeName);
            return hr;
        }
        IFACEMETHOD(GetTrustLevel)(_Out_ TrustLevel* trustLvl)
        {
            *trustLvl = BaseTrust;
            return S_OK;
        }
        IFACEMETHOD(GetIids)(_Out_ ULONG *iidCount, _Outptr_result_buffer_(*iidCount) IID **)
        {
            iidCount;
            return E_NOTIMPL;
        }
    };

    class HSTRINGIterable : public Microsoft::WRL::RuntimeClass<IIterable<HSTRING>>
    {

        HSTRING *items;
        uint32 length;

    public:
        HRESULT RuntimeClassInitialize(HSTRING *string, uint32 length)
        {
            this->items = new HSTRING[length];

            if (this->items == nullptr)
            {
                return E_OUTOFMEMORY;
            }

            for(uint32 i = 0; i < length; i++)
            {
                this->items[i] = string[i];
            }
            this->length = length;

            return S_OK;
        }

        ~HSTRINGIterable()
        {
            if(this->items != nullptr)
            {
                delete [] items;
            }
        }

        IFACEMETHODIMP First(_Outptr_result_maybenull_ IIterator<HSTRING> **first)
        {
            return Microsoft::WRL::MakeAndInitialize<HSTRINGIterator>(first, this->items, this->length);
        }

        IFACEMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeName) sealed
        {
            *runtimeName = nullptr;
            HRESULT hr = S_OK;
            // Return type that does not exist in metadata
            const wchar_t *name = L"Js.HSTRINGIterable";
            hr = WindowsCreateString(name, static_cast<UINT32>(wcslen(name)), runtimeName);
            return hr;
        }
        IFACEMETHOD(GetTrustLevel)(_Out_ TrustLevel* trustLvl)
        {
            *trustLvl = BaseTrust;
            return S_OK;
        }
        IFACEMETHOD(GetIids)(_Out_ ULONG *iidCount, _Outptr_result_buffer_(*iidCount) IID **)
        {
            iidCount;
            return E_NOTIMPL;
        }
    };
#endif

    __inline DelayLoadWindowsGlobalization* WindowsGlobalizationAdapter::GetWindowsGlobalizationLibrary(_In_ ScriptContext* scriptContext)
    {
        return this->GetWindowsGlobalizationLibrary(scriptContext->GetThreadContext());
    }

    __inline DelayLoadWindowsGlobalization* WindowsGlobalizationAdapter::GetWindowsGlobalizationLibrary(_In_ ThreadContext* threadContext)
    {
        return threadContext->GetWindowsGlobalizationLibrary();
    }

    template<typename T>
    HRESULT WindowsGlobalizationAdapter::GetActivationFactory(DelayLoadWindowsGlobalization *delayLoadLibrary, LPCWSTR factoryName, T** instance)
    {
        *instance = nullptr;

        AutoCOMPtr<IActivationFactory> factory;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;
        HRESULT hr;

        // factoryName will never get truncated as the name of interfaces in Windows.globalization are not that long.
        IfFailedReturn(delayLoadLibrary->WindowsCreateStringReference(factoryName, static_cast<UINT32>(wcslen(factoryName)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(delayLoadLibrary->DllGetActivationFactory(hString, &factory));

        return factory->QueryInterface(__uuidof(T), reinterpret_cast<void**>(instance));
    }

#ifdef ENABLE_INTL_OBJECT
    HRESULT WindowsGlobalizationAdapter::EnsureCommonObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {
        HRESULT hr = S_OK;

        if (initializedCommonGlobObjects)
        {
            AssertMsg(hrForCommonGlobObjectsInit == S_OK, "If IntlGlobObjects are initialized, we should be returning S_OK.");
            return hrForCommonGlobObjectsInit;
        }
        else if (hrForCommonGlobObjectsInit != S_OK)
        {
            return hrForCommonGlobObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_Language, &languageFactory), hrForCommonGlobObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_Language, &languageStatics), hrForCommonGlobObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_DateTimeFormatting_DateTimeFormatter, &dateTimeFormatterFactory), hrForCommonGlobObjectsInit);

        hrForCommonGlobObjectsInit = S_OK;
        initializedCommonGlobObjects = true;

        return hr;
    }


    HRESULT WindowsGlobalizationAdapter::EnsureDateTimeFormatObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {
        HRESULT hr = S_OK;

        if (initializedDateTimeFormatObjects)
        {
            AssertMsg(hrForDateTimeFormatObjectsInit == S_OK, "If DateTimeFormatObjects are initialized, we should be returning S_OK.");
            return hrForDateTimeFormatObjectsInit;
        }
        else if (hrForDateTimeFormatObjectsInit != S_OK)
        {
            return hrForDateTimeFormatObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_Calendar, &calendarFactory), hrForDateTimeFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(this->CreateTimeZoneOnCalendar(library, &defaultTimeZoneCalendar), hrForDateTimeFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(this->CreateTimeZoneOnCalendar(library, &timeZoneCalendar), hrForDateTimeFormatObjectsInit);

        hrForDateTimeFormatObjectsInit = S_OK;
        initializedDateTimeFormatObjects = true;

        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::EnsureNumberFormatObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {
        HRESULT hr = S_OK;

        if (initializedNumberFormatObjects)
        {
            AssertMsg(hrForNumberFormatObjectsInit == S_OK, "If NumberFormatObjects are initialized, we should be returning S_OK.");
            return hrForNumberFormatObjectsInit;
        }
        else if (hrForNumberFormatObjectsInit != S_OK)
        {
            return hrForNumberFormatObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_CurrencyFormatter, &currencyFormatterFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_DecimalFormatter, &decimalFormatterFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_PercentFormatter, &percentFormatterFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_SignificantDigitsNumberRounder, &significantDigitsRounderActivationFactory), hrForNumberFormatObjectsInit);
        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Globalization_NumberFormatting_IncrementNumberRounder, &incrementNumberRounderActivationFactory), hrForNumberFormatObjectsInit);

        hrForNumberFormatObjectsInit = S_OK;
        initializedNumberFormatObjects = true;

        return hr;
    }

#endif

    HRESULT WindowsGlobalizationAdapter::EnsureDataTextObjectsInitialized(DelayLoadWindowsGlobalization *library)
    {
        HRESULT hr = S_OK;

        if (initializedCharClassifierObjects)
        {
            AssertMsg(hrForCharClassifierObjectsInit == S_OK, "If DataTextObjects are initialized, we should be returning S_OK.");
            return hrForCharClassifierObjectsInit;
        }
        else if (hrForCharClassifierObjectsInit != S_OK)
        {
            return hrForCharClassifierObjectsInit;
        }

        IfFailedSetErrorCodeAndReturn(GetActivationFactory(library, RuntimeClass_Windows_Data_Text_UnicodeCharacters, &unicodeStatics), hrForCharClassifierObjectsInit);

        hrForCharClassifierObjectsInit = S_OK;
        initializedCharClassifierObjects = true;

        return hr;
    }

#ifdef ENABLE_INTL_OBJECT
    HRESULT WindowsGlobalizationAdapter::CreateLanguage(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR languageTag, ILanguage** language)
    {
        HRESULT hr = S_OK;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;

        // OK for languageTag to get truncated as it would pass incomplete languageTag below which
        // will be rejected by globalization dll
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(languageTag, static_cast<UINT32>(wcslen(languageTag)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(this->languageFactory->CreateLanguage(hString, language));
        return hr;
    }

    boolean WindowsGlobalizationAdapter::IsWellFormedLanguageTag(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR languageTag)
    {
        boolean retVal;
        HRESULT hr;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;
        // OK for languageTag to get truncated as it would pass incomplete languageTag below which
        // will be rejected by globalization dll
        IfFailThrowHr(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(languageTag, static_cast<UINT32>(wcslen(languageTag)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailThrowHr(this->languageStatics->IsWellFormed(hString, &retVal));
        return retVal;
    }

        // OK for timeZoneId to get truncated as it would pass incomplete timeZoneId below which
        // will be rejected by globalization dll
    HRESULT WindowsGlobalizationAdapter::NormalizeLanguageTag(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR languageTag, HSTRING *result)
    {
        HRESULT hr;

        AutoCOMPtr<ILanguage> language;
        IfFailedReturn(CreateLanguage(scriptContext, languageTag, &language));

        IfFailedReturn(language->get_LanguageTag(result));
        return hr;
    }

    boolean WindowsGlobalizationAdapter::ValidateAndCanonicalizeTimeZone(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR timeZoneId, HSTRING *result)
    {
        HRESULT hr = S_OK;
        HSTRING timeZone;
        HSTRING_HEADER timeZoneHeader;

        // Construct HSTRING of timeZoneId passed
        IfFailThrowHr(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(timeZoneId, static_cast<UINT32>(wcslen(timeZoneId)), &timeZoneHeader, &timeZone));
        Assert(timeZone);

        // The warning is timeZone could be '0'. This is valid scenario and in that case, ChangeTimeZone() would
        // return error HR in which case we will throw.
#pragma warning(suppress:6387)
        // ChangeTimeZone should fail if this is not a valid time zone
        hr = timeZoneCalendar->ChangeTimeZone(timeZone);
        if (hr != S_OK)
        {
            return false;
        }
        // Retrieve canonicalize timeZone name
        IfFailThrowHr(timeZoneCalendar->GetTimeZone(result));
        return true;
    }

    void WindowsGlobalizationAdapter::GetDefaultTimeZoneId(_In_ ScriptContext* scriptContext, HSTRING *result)
    {
        HRESULT hr = S_OK;
        IfFailThrowHr(defaultTimeZoneCalendar->GetTimeZone(result));
    }

    HRESULT WindowsGlobalizationAdapter::CreateTimeZoneOnCalendar(_In_ DelayLoadWindowsGlobalization *library, __out::ITimeZoneOnCalendar**  result)
    {
        AutoCOMPtr<::ICalendar> calendar;

        HRESULT hr = S_OK;

        // initialize hard-coded default languages
        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, 1), 1);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, 1), 1);
        IfFailedReturn(library->WindowsCreateStringReference(L"en-US", static_cast<UINT32>(wcslen(L"en-US")), (headers), (arr)));
        Microsoft::WRL::ComPtr<IIterable<HSTRING>> defaultLanguages;
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&defaultLanguages, arr, 1));


        // Create calendar object
        IfFailedReturn(this->calendarFactory->CreateCalendarDefaultCalendarAndClock(defaultLanguages.Get(), &calendar));

        // Get ITimeZoneOnCalendar part of calendar object
        IfFailedReturn(calendar->QueryInterface(__uuidof(::ITimeZoneOnCalendar), reinterpret_cast<void**>(result)));

        return hr;
    }

    void WindowsGlobalizationAdapter::ClearTimeZoneCalendars()
    {
        if (this->timeZoneCalendar)
        {
            this->timeZoneCalendar.Detach()->Release();
        }

        if (this->defaultTimeZoneCalendar)
        {
            this->defaultTimeZoneCalendar.Detach()->Release();
        }
    }

    HRESULT WindowsGlobalizationAdapter::CreateCurrencyFormatterCode(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR currencyCode, NumberFormatting::ICurrencyFormatter** currencyFormatter)
    {
        HRESULT hr;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;

        // OK for currencyCode to get truncated as it would pass incomplete currencyCode below which
        // will be rejected by globalization dll
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(currencyCode, static_cast<UINT32>(wcslen(currencyCode)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(this->currencyFormatterFactory->CreateCurrencyFormatterCode(hString, currencyFormatter));
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateCurrencyFormatter(_In_ ScriptContext* scriptContext, PCWSTR* localeStrings, uint32 numLocaleStrings, _In_z_ PCWSTR currencyCode, NumberFormatting::ICurrencyFormatter** currencyFormatter)
    {
        HRESULT hr;
        HSTRING hString;
        HSTRING_HEADER hStringHdr;

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {
            // OK for localeString to get truncated as it would pass incomplete localeString below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        HSTRING geoString;
        HSTRING_HEADER geoStringHeader;
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(L"ZZ", 2, &geoStringHeader, &geoString));
        AnalysisAssert(geoString);
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(currencyCode, static_cast<UINT32>(wcslen(currencyCode)), &hStringHdr, &hString));
        AnalysisAssert(hString);
        IfFailedReturn(this->currencyFormatterFactory->CreateCurrencyFormatterCodeContext(hString, languages.Get(), geoString, currencyFormatter));
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateNumberFormatter(_In_ ScriptContext* scriptContext, PCWSTR* localeStrings, uint32 numLocaleStrings, NumberFormatting::INumberFormatter** numberFormatter)
    {
        HRESULT hr = S_OK;

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        HSTRING geoString;
        HSTRING_HEADER geoStringHeader;
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(L"ZZ", 2, &geoStringHeader, &geoString));
        AnalysisAssert(geoString);
        IfFailedReturn(this->decimalFormatterFactory->CreateDecimalFormatter(languages.Get(), geoString, numberFormatter));
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreatePercentFormatter(_In_ ScriptContext* scriptContext, PCWSTR* localeStrings, uint32 numLocaleStrings, NumberFormatting::INumberFormatter** numberFormatter)
    {
        HRESULT hr = S_OK;

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {
            // OK for localeString to get truncated as it would pass incomplete localeString below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        HSTRING geoString;
        HSTRING_HEADER geoStringHeader;
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(L"ZZ", 2, &geoStringHeader, &geoString));
        AnalysisAssert(geoString);
        IfFailedReturn(this->percentFormatterFactory->CreatePercentFormatter(languages.Get(), geoString, numberFormatter));

        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateDateTimeFormatter(_In_ ScriptContext* scriptContext, _In_z_ PCWSTR formatString, _In_z_ PCWSTR* localeStrings,
        uint32 numLocaleStrings, _In_opt_z_ PCWSTR calendar, _In_opt_z_ PCWSTR clock, _Out_ DateTimeFormatting::IDateTimeFormatter** result)
    {
        HRESULT hr = S_OK;

        if(numLocaleStrings == 0) return E_INVALIDARG;

        AnalysisAssert((calendar == nullptr && clock == nullptr) || (calendar != nullptr && clock != nullptr));

        HSTRING fsHString;
        HSTRING_HEADER fsHStringHdr;

        // OK for formatString to get truncated as it would pass incomplete formatString below which
        // will be rejected by globalization dll.
        IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(formatString, static_cast<UINT32>(wcslen(formatString)), &fsHStringHdr, &fsHString));
        AnalysisAssert(fsHString);

        AutoArrayPtr<HSTRING> arr(HeapNewArray(HSTRING, numLocaleStrings), numLocaleStrings);
        AutoArrayPtr<HSTRING_HEADER> headers(HeapNewArray(HSTRING_HEADER, numLocaleStrings), numLocaleStrings);
        for(uint32 i = 0; i< numLocaleStrings; i++)
        {
            // OK for localeString to get truncated as it would pass incomplete localeString below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(localeStrings[i], static_cast<UINT32>(wcslen(localeStrings[i])), (headers + i), (arr + i)));
        }

        Microsoft::WRL::ComPtr<IIterable<HSTRING>> languages(nullptr);
        IfFailedReturn(Microsoft::WRL::MakeAndInitialize<HSTRINGIterable>(&languages, arr, numLocaleStrings));

        if(clock == nullptr)
        {
            IfFailedReturn(this->dateTimeFormatterFactory->CreateDateTimeFormatterLanguages(fsHString, languages.Get(), result));
        }
        else
        {
            HSTRING geoString;
            HSTRING_HEADER geoStringHeader;
            HSTRING calString;
            HSTRING_HEADER calStringHeader;
            HSTRING clockString;
            HSTRING_HEADER clockStringHeader;

            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(L"ZZ", 2, &geoStringHeader, &geoString));
            AnalysisAssert(geoString);

            // OK for calendar/clock to get truncated as it would pass incomplete text below which
            // will be rejected by globalization dll.
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(calendar, static_cast<UINT32>(wcslen(calendar)), &calStringHeader, &calString));
            AnalysisAssert(calString);
            IfFailedReturn(GetWindowsGlobalizationLibrary(scriptContext)->WindowsCreateStringReference(clock, static_cast<UINT32>(wcslen(clock)), &clockStringHeader, &clockString));
            AnalysisAssert(clockString);
            IfFailedReturn(this->dateTimeFormatterFactory->CreateDateTimeFormatterContext(fsHString, languages.Get(), geoString, calString, clockString, result));
        }
        return hr;
    }

    HRESULT WindowsGlobalizationAdapter::CreateIncrementNumberRounder(_In_ ScriptContext* scriptContext, NumberFormatting::INumberRounder** numberRounder)
    {
        return incrementNumberRounderActivationFactory->ActivateInstance(reinterpret_cast<IInspectable**>(numberRounder));
    }

    HRESULT WindowsGlobalizationAdapter::CreateSignificantDigitsRounder(_In_ ScriptContext* scriptContext, NumberFormatting::INumberRounder** numberRounder)
    {
        return significantDigitsRounderActivationFactory->ActivateInstance(reinterpret_cast<IInspectable**>(numberRounder));
    }
#endif
}


#endif
