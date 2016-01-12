//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


namespace UnifiedRegex
{
    template <typename C>
    struct Chars
    {
        typedef C Char;
    };

    template <>
    struct Chars<uint8>
    {
        typedef uint8 Char;
        typedef uint8 UChar;

        static const int CharWidth = sizeof(char) * 8;
        static const int NumChars = 1 << CharWidth;
        static const uint MaxUChar = (uint8)-1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {
            return (uint)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u) {
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)i;
        }

        // Char to wchar_t
        static inline wchar_t CTW(Char c)
        {
            return (wchar_t)c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {
            return UTC(CTU(c) + n);
        }
    };

    template <>
    struct Chars<char>
    {
        typedef char Char;
        typedef uint8 UChar;

        static const int CharWidth = sizeof(char) * 8;
        static const int NumChars = 1 << CharWidth;
        static const uint MaxUChar = (uint8)-1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {
            return (uint8)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u) {
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)(uint8)i;
        }

        // Char to wchar_t
        static inline wchar_t CTW(Char c)
        {
            return (wchar_t)(uint8)c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {
            return UTC(CTU(c) + n);
        }
    };


    template <>
    struct Chars<wchar_t>
    {
        typedef wchar_t Char;
        typedef uint16 UChar;

        static const int CharWidth = sizeof(wchar_t) * 8;
        static const int NumChars = 1 << CharWidth;
        static const uint MaxUChar = (uint16)-1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {
            return (uint16)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u)
        {
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)(uint16)i;
        }

        // Char to wchar_t
        static inline wchar_t CTW(Char c)
        {
            return c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {
            return UTC(CTU(c) + n);
        }
    };

    template <>
    struct Chars<codepoint_t>
    {
        typedef codepoint_t Char;
        typedef codepoint_t UChar;

        static const int CharWidth = sizeof(codepoint_t) * 8;
        static const int NumChars = 0x110000;
        static const uint MaxUChar = (NumChars) - 1;
        static const uint MaxUCharAscii = (1 << 7) - 1;
        static const Char MinChar = (Char)0;
        static const Char MaxChar = (Char)MaxUChar;

        // Char to unsigned int
        static inline uint CTU(Char c)
        {
            Assert(c <= MaxChar);
            return (codepoint_t)c;
        }

        // Unsigned int to Char
        static inline Char UTC(uint u)
        {
            Assert(u <= MaxUChar);
            return (Char)u;
        }

        // int to Char
        static inline Char ITC(int i) {
            Assert(i >= 0 && i <= MaxUChar);
            return (Char)(codepoint_t)i;
        }

        // Char to wchar_t
        static inline wchar_t CTW(Char c)
        {
            Assert(c < Chars<wchar_t>::MaxUChar);
            return (wchar_t)c;
        }

        // Offset, same buffer
        static inline CharCount OSB(const Char* ph, const Char* pl)
        {
            Assert(ph >= pl && ph - pl <= MaxCharCount);
            return (CharCount)(ph - pl);
        }

        static inline Char Shift(Char c, int n)
        {
            return UTC(CTU(c) + n);
        }
    };
}
