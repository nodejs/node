//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    enum EscapingOperation : BYTE
    {
        EscapingOperation_NotEscape,
        EscapingOperation_Escape,
        EscapingOperation_Count
    };

    class WritableStringBuffer
    {
    public:
        WritableStringBuffer(_In_count_(length) wchar_t* str, _In_ charcount_t length) : m_pszString(str), m_pszCurrentPtr(str), m_length(length) {}

        void Append(wchar_t c);
        void Append(const wchar_t * str, charcount_t countNeeded);
        void AppendLarge(const wchar_t * str, charcount_t countNeeded);
    private:
        wchar_t* m_pszString;
        wchar_t* m_pszCurrentPtr;
        charcount_t m_length;
#if DBG
        charcount_t GetCount()
        {
            Assert(m_pszCurrentPtr >= m_pszString);
            Assert(m_pszCurrentPtr - m_pszString <= MaxCharCount);
            return static_cast<charcount_t>(m_pszCurrentPtr - m_pszString);
        }
#endif
    };

    class JSONString : public JavascriptString
    {
    public:
        static JSONString* New(JavascriptString* originalString, charcount_t start, charcount_t extraChars);
        virtual const wchar_t* GetSz() override;
    protected:
        DEFINE_VTABLE_CTOR(JSONString, JavascriptString);
        DECLARE_CONCRETE_STRING_CLASS;
    private:
        JavascriptString* m_originalString;
        charcount_t m_start; /* start of the escaping operation */

    private:
        JSONString(JavascriptString* originalString, charcount_t start, charcount_t length);
        static const WCHAR escapeMap[128];
        static const BYTE escapeMapCount[128];
    public:
        template <EscapingOperation op>
        static Js::JavascriptString* Escape(Js::JavascriptString* value, uint start = 0, WritableStringBuffer* outputString = nullptr)
        {
            uint len = value->GetLength();

            if (0 == len)
            {
                Js::ScriptContext* scriptContext = value->GetScriptContext();
                return scriptContext->GetLibrary()->GetQuotesString();
            }
            else
            {
                const wchar_t* szValue = value->GetSz();
                return EscapeNonEmptyString<op, Js::JSONString, Js::ConcatStringWrapping<L'"', L'"'>, Js::JavascriptString*>(value, szValue, start, len, outputString);
            }
        }

        template <EscapingOperation op, class TJSONString, class TConcatStringWrapping, class TJavascriptString>
        static TJavascriptString EscapeNonEmptyString(Js::JavascriptString* value, const wchar_t* szValue, uint start, charcount_t len, WritableStringBuffer* outputString)
        {
            charcount_t extra = 0;
            TJavascriptString result;

            // Optimize for the case when we don't need to change anything, just wrap with quotes.
            // If we realize we need to change the inside of the string, start over in "modification needed" mode.
            if (op == EscapingOperation_Escape)
            {
                outputString->Append(L'\"');
                if (start != 0)
                {
                    outputString->AppendLarge(szValue, start);
                }
            }
            const wchar* endSz = szValue + len;
            const wchar* startSz = szValue + start;
            const wchar* lastFlushSz = startSz;
            for (const wchar* current = startSz; current < endSz; current++)
            {
                WCHAR wch = *current;

                if (op == EscapingOperation_Count)
                {
                    if (wch < _countof(escapeMap))
                    {
                        extra = UInt32Math::Add(extra, escapeMapCount[(char)wch]);
                    }
                }
                else
                {
                    WCHAR specialChar;
                    if (wch < _countof(escapeMap))
                    {
                        specialChar = escapeMap[(char)wch];
                    }
                    else
                    {
                        specialChar = '\0';
                    }

                    if (specialChar != '\0')
                    {
                        if (op == EscapingOperation_Escape)
                        {
                            outputString->AppendLarge(lastFlushSz, (charcount_t)(current - lastFlushSz));
                            lastFlushSz = current + 1;
                            outputString->Append(L'\\');
                            outputString->Append(specialChar);
                            if (specialChar == L'u')
                            {
                                wchar_t bf[5];
                                _ltow_s(wch, bf, _countof(bf), 16);
                                size_t count = wcslen(bf);
                                if (count < 4)
                                {
                                    if (count == 1)
                                    {
                                        outputString->Append(L"000", 3);
                                    }
                                    else if (count == 2)
                                    {
                                        outputString->Append(L"00", 2);
                                    }
                                    else
                                    {
                                        outputString->Append(L"0", 1);
                                    }
                                }
                                outputString->Append(bf, (charcount_t)count);
                            }
                        }
                        else
                        {
                            charcount_t i = (charcount_t)(current - startSz);
                            return EscapeNonEmptyString<EscapingOperation_Count, TJSONString, TConcatStringWrapping, TJavascriptString>(value, szValue, i ? i - 1 : 0, len, outputString);
                        }
                    }
                }
            } // for.

            if (op == EscapingOperation_Escape)
            {
                if (lastFlushSz < endSz)
                {
                    outputString->AppendLarge(lastFlushSz, (charcount_t)(endSz - lastFlushSz));
                }
                outputString->Append(L'\"');
                result = nullptr;
            }
            else if (op == EscapingOperation_Count)
            {
                result = TJSONString::New(value, start, extra);
            }
            else
            {
                // If we got here, we don't need to change the inside, just wrap the string with quotes.
                result = TConcatStringWrapping::New(value);
            }

            return result;
        }

        static WCHAR* EscapeNonEmptyString(ArenaAllocator* allocator, const wchar_t* szValue)
        {
            WCHAR* result = nullptr;
            StringProxy::allocator = allocator;
            charcount_t len = (charcount_t)wcslen(szValue);
            StringProxy* proxy = EscapeNonEmptyString<EscapingOperation_NotEscape, StringProxy, StringProxy, StringProxy*>(nullptr, szValue, 0, len, nullptr);
            result = proxy->GetResult(szValue, len);
            StringProxy::allocator = nullptr;
            return result;
        }

        // This class has the same interface (with respect to the EscapeNonEmptyString method) as JSONString and TConcatStringWrapping
        // It is used in scenario where we want to use the JSON escaping capability without having a script context.
        class StringProxy
        {
        public:
            static ArenaAllocator* allocator;

            StringProxy()
            {
                this->m_needEscape = false;
            }

            StringProxy(int start, int extra) : m_start(start), m_extra(extra)
            {
                this->m_needEscape = true;
            }

            static StringProxy* New(Js::JavascriptString* value)
            {
                // Case 1: The string do not need to be escaped at all
                Assert(value == nullptr);
                Assert(allocator != nullptr);
                return Anew(allocator, StringProxy);
            }

            static StringProxy* New(Js::JavascriptString* value, uint start, uint length)
            {
                // Case 2: The string requires escaping, and the length is computed
                Assert(value == nullptr);
                Assert(allocator != nullptr);
                return Anew(allocator, StringProxy, start, length);
            }

            WCHAR* GetResult(const WCHAR* originalString, charcount_t originalLength)
            {
                if (this->m_needEscape)
                {
                    charcount_t unescapedStringLength = originalLength + m_extra + 2 /* for the quotes */;
                    WCHAR* buffer = AnewArray(allocator, WCHAR, unescapedStringLength + 1); /* for terminating null */
                    buffer[unescapedStringLength] = '\0';
                    WritableStringBuffer stringBuffer(buffer, unescapedStringLength);
                    StringProxy* proxy = JSONString::EscapeNonEmptyString<EscapingOperation_Escape, StringProxy, StringProxy, StringProxy*>(nullptr, originalString, m_start, originalLength, &stringBuffer);
                    Assert(proxy == nullptr);
                    Assert(buffer[unescapedStringLength] == '\0');
                    return buffer;
                }
                else
                {
                    WCHAR* buffer = AnewArray(allocator, WCHAR, originalLength + 3); /* quotes and terminating null */
                    buffer[0] = L'\"';
                    buffer[originalLength + 1] = L'\"';
                    buffer[originalLength + 2] = L'\0';
                    js_wmemcpy_s(buffer + 1, originalLength, originalString, originalLength);
                    return buffer;
                }
            }

        private:
            int m_extra;
            int m_start;
            bool m_needEscape;
        };
    };
}
