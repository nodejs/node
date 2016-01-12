//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "JSONScanner.h"

using namespace Js;

namespace JSON
{
    // -------- Scanner implementation ------------//
    JSONScanner::JSONScanner()
        : inputText(0), inputLen(0), pToken(0), stringBuffer(0), allocator(0), allocatorObject(0),
        currentRangeCharacterPairList(0), stringBufferLength(0), currentIndex(0)
    {
    }

    void JSONScanner::Finalizer()
    {
        // All dynamic memory allocated by this object is on the arena - either the one this object owns or by the
        // one shared with JSON parser - here we will deallocate ours. The others will be deallocated when JSONParser
        // goes away which should happen right after this.
        if (this->allocatorObject != nullptr)
        {
            // We created our own allocator, so we have to free it
            this->scriptContext->ReleaseTemporaryGuestAllocator(allocatorObject);
        }
    }

    void JSONScanner::Init(const wchar_t* input, uint len, Token* pOutToken, Js::ScriptContext* sc, const wchar_t* current, ArenaAllocator* allocator)
    {
        // Note that allocator could be nullptr from JSONParser, if we could not reuse an allocator, keep our own
        inputText = input;
        currentChar = current;
        inputLen = len;
        pToken = pOutToken;
        scriptContext = sc;
        this->allocator = allocator;
    }

    tokens JSONScanner::Scan()
    {
        pTokenString = currentChar;

        while (currentChar < inputText + inputLen)
        {
            switch(ReadNextChar())
            {
            case 0:
                //EOF
                currentChar--;
                return (pToken->tk = tkEOF);

            case '\t':
            case '\r':
            case '\n':
            case ' ':
                //WS - keep looping
                break;

            case '"':
                //check for string
                return ScanString();

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                //decimal digit starts a number
                {
                    currentChar--;

                    // we use StrToDbl() here for compat with the rest of the engine. StrToDbl() accept a larger syntax.
                    // Verify first the JSON grammar.
                    const wchar_t* saveCurrentChar = currentChar;
                    if(!IsJSONNumber())
                    {
                        Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRbadNumber);
                    }
                    currentChar = saveCurrentChar;
                    double val;
                    const wchar_t* end;
                    val = Js::NumberUtilities::StrToDbl(currentChar, &end, scriptContext);
                    if(currentChar == end)
                    {
                        Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRbadNumber);
                    }
                    AssertMsg(!Js::JavascriptNumber::IsNan(val), "Bad result from string to double conversion");
                    pToken->tk = tkFltCon;
                    pToken->SetDouble(val, false);
                    currentChar = end;
                    return tkFltCon;
                }

            case ',':
                return (pToken->tk = tkComma);

            case ':':
                return (pToken->tk = tkColon);

            case '[':
                return (pToken->tk = tkLBrack);

            case ']':
                return (pToken->tk = tkRBrack);

            case '-':
                return (pToken->tk = tkSub);

            case 'n':
                //check for 'null'
                if (currentChar + 2 < inputText + inputLen  && currentChar[0] == 'u' && currentChar[1] == 'l' && currentChar[2] == 'l')
                {
                    currentChar += 3;
                    return (pToken->tk = tkNULL);
                }
                Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRillegalChar);

            case 't':
                //check for 'true'
                if (currentChar + 2 < inputText + inputLen  && currentChar[0] == 'r' && currentChar[1] == 'u' && currentChar[2] == 'e')
                {
                    currentChar += 3;
                    return (pToken->tk = tkTRUE);
                }
                Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRillegalChar);

            case 'f':
                //check for 'false'
                if (currentChar + 3 < inputText + inputLen  && currentChar[0] == 'a' && currentChar[1] == 'l' && currentChar[2] == 's' && currentChar[3] == 'e')
                {
                    currentChar += 4;
                    return (pToken->tk = tkFALSE);
                }
                Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRillegalChar);

            case '{':
                return (pToken->tk = tkLCurly);

            case '}':
                return (pToken->tk = tkRCurly);

            default:
                Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRillegalChar);
            }

        }

        return (pToken->tk = tkEOF);
    }

    bool JSONScanner::IsJSONNumber()
    {
        bool firstDigitIsAZero = false;
        if (PeekNextChar() == '0')
        {
            firstDigitIsAZero = true;
            currentChar++;
        }

        //partial verification of number JSON grammar.
        while (currentChar < inputText + inputLen)
        {
            switch(ReadNextChar())
            {
            case 0:
                return false;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (firstDigitIsAZero)
                {
                    return false;
                }
                break;

            case '.':
                {
                    // at least one digit after '.'
                    if(currentChar < inputText + inputLen)
                    {
                        wchar_t nch = ReadNextChar();
                        if('0' <= nch && nch <= '9')
                        {
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }
                //case 'E':
                //case 'e':
                //    return true;
            default:
                return true;
            }

            firstDigitIsAZero = false;
        }
        return true;
    }

    tokens JSONScanner::ScanString()
    {
        wchar_t ch;

        this->currentIndex = 0;
        this->currentString = const_cast<wchar_t*>(currentChar);
        bool endFound = false;
        bool isStringDirectInputTextMapped = true;
        LPCWSTR bulkStart = currentChar;
        uint bulkLength = 0;

        while (currentChar < inputText + inputLen)
        {
            ch = ReadNextChar();
            int tempHex;

            if (ch == '"')
            {
                //end of the string
                endFound = true;
                break;
            }
            else if (ch <= 0x1F)
            {
                //JSON doesn't accept \u0000 - \u001f range, LS(\u2028) and PS(\u2029) are ok
                Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRillegalChar);
            }
            else if ( 0 == ch )
            {
                currentChar--;
                Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRnoStrEnd);
            }
            else if ('\\' == ch)
            {
                //JSON escape sequence in a string \", \/, \\, \b, \f, \n, \r, \t, unicode seq
                // unlikely V5.8 regular chars are not escaped, i.e '\g'' in a string is illegal not 'g'
                if (currentChar >= inputText + inputLen )
                {
                    Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRnoStrEnd);
                }

                ch = ReadNextChar();
                switch (ch)
                {
                case 0:
                    currentChar--;
                    Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRnoStrEnd);

                case '"':
                case '/':
                case '\\':
                    //keep ch
                    break;

                case 'b':
                    ch = 0x08;
                    break;

                case 'f':
                    ch = 0x0C;
                    break;

                case 'n':
                    ch = 0x0A;
                    break;

                case 'r':
                    ch = 0x0D;
                    break;

                case 't':
                    ch = 0x09;
                    break;

                case 'u':
                    {
                        int chcode;
                        // 4 hex digits
                        if (currentChar + 3 >= inputText + inputLen)
                        {
                            //no room left for 4 hex chars
                            Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRnoStrEnd);

                        }
                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {
                            Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRbadHexDigit);
                        }
                        chcode = tempHex * 0x1000;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {
                            Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRbadHexDigit);
                        }
                        chcode += tempHex * 0x0100;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {
                            Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRbadHexDigit);
                        }
                        chcode += tempHex * 0x0010;

                        if (!Js::NumberUtilities::FHexDigit((WCHAR)ReadNextChar(), &tempHex))
                        {
                            Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRbadHexDigit);
                        }
                        chcode += tempHex;
                        AssertMsg(chcode == (chcode & 0xFFFF), "Bad unicode code");
                        ch = (wchar_t)chcode;
                    }
                    break;

                default:
                    // Any other '\o' is an error in JSON
                    Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRillegalChar);
                }

                // flush
                this->GetCurrentRangeCharacterPairList()->Add(RangeCharacterPair((uint)(bulkStart - inputText), bulkLength, ch));

                uint oldIndex = currentIndex;
                currentIndex += bulkLength;
                currentIndex++;

                if (currentIndex < oldIndex)
                {
                    // Overflow
                    Js::Throw::OutOfMemory();
                }

                // mark the mode as 'string transformed' (no direct mapping in inputText possible)
                isStringDirectInputTextMapped = false;

                // reset (to next char)
                bulkStart = currentChar;
                bulkLength = 0;
            }
            else
            {
                // continue
                bulkLength++;
            }
        }

        if (!endFound)
        {
            // no ending '"' found
            Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRnoStrEnd);
        }

        if (isStringDirectInputTextMapped == false)
        {
            // If the last bulk is not ended with an escape character, make sure that is
            // not built into the final unescaped string
            bool shouldSkipLastCharacter = false;
            if (bulkLength > 0)
            {
                shouldSkipLastCharacter = true;
                this->GetCurrentRangeCharacterPairList()->Add(RangeCharacterPair((uint)(bulkStart - inputText), bulkLength, L'\0'));
                uint oldIndex = currentIndex;
                currentIndex += bulkLength;
                if (currentIndex < oldIndex)
                {
                    // Overflow
                    Js::Throw::OutOfMemory();
                }
            }

            this->BuildUnescapedString(shouldSkipLastCharacter);
            this->GetCurrentRangeCharacterPairList()->Clear();
            this->currentString = this->stringBuffer;
        }
        else
        {
            // make currentIndex the length (w/o the \0)
            currentIndex = bulkLength;

            OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, L"ScanString(): direct-mapped string as '%.*s'\n",
                GetCurrentStringLen(), GetCurrentString());
        }

        return (pToken->tk = tkStrCon);
    }

    void JSONScanner::BuildUnescapedString(bool shouldSkipLastCharacter)
    {
        AssertMsg(this->allocator != nullptr, "We must have built the allocator");
        AssertMsg(this->currentRangeCharacterPairList != nullptr, "We must have built the currentRangeCharacterPairList");
        AssertMsg(this->currentRangeCharacterPairList->Count() > 0, "We need to build the current string only because we have escaped characters");

        // Step 1: Ensure the buffer has sufficient space
        int requiredSize = this->GetCurrentStringLen();
        if (requiredSize > this->stringBufferLength)
        {
            if (this->stringBuffer)
            {
                AdeleteArray(this->allocator, this->stringBufferLength, this->stringBuffer);
                this->stringBuffer = nullptr;
            }

            this->stringBuffer = AnewArray(this->allocator, wchar_t, requiredSize);
            this->stringBufferLength = requiredSize;
        }

        // Step 2: Copy the data to the buffer
        int totalCopied = 0;
        wchar_t* begin_copy = this->stringBuffer;
        int lastCharacterIndex = this->currentRangeCharacterPairList->Count() - 1;
        for (int i = 0; i <= lastCharacterIndex; i++)
        {
            RangeCharacterPair data = this->currentRangeCharacterPairList->Item(i);
            int charactersToCopy = data.m_rangeLength;
            js_wmemcpy_s(begin_copy, charactersToCopy, this->inputText + data.m_rangeStart, charactersToCopy);
            begin_copy += charactersToCopy;
            totalCopied += charactersToCopy;

            if (i == lastCharacterIndex && shouldSkipLastCharacter)
            {
                continue;
            }

            *begin_copy = data.m_char;
            begin_copy++;
            totalCopied++;
        }

        if (totalCopied != requiredSize)
        {
            OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, L"BuildUnescapedString(): allocated size = %d != copying size %d\n", requiredSize, totalCopied);
            AssertMsg(totalCopied == requiredSize, "BuildUnescapedString(): The allocated size and copying size should match.");
        }

        OUTPUT_TRACE_DEBUGONLY(Js::JSONPhase, L"BuildUnescapedString(): unescaped string as '%.*s'\n", GetCurrentStringLen(), this->stringBuffer);
    }

    JSONScanner::RangeCharacterPairList* JSONScanner::GetCurrentRangeCharacterPairList(void)
    {
        if (this->currentRangeCharacterPairList == nullptr)
        {
            if (this->allocator == nullptr)
            {
                this->allocatorObject = this->scriptContext->GetTemporaryGuestAllocator(L"JSONScanner");
                this->allocator = this->allocatorObject->GetAllocator();
            }

            this->currentRangeCharacterPairList = Anew(this->allocator, RangeCharacterPairList, this->allocator, 4);
        }

        return this->currentRangeCharacterPairList;
    }
} // namespace JSON
