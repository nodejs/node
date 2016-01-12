//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef PROFILE_STRINGS

namespace Js
{
    enum ConcatType
    {
        ConcatType_Unknown,
        ConcatType_CompoundString,
        ConcatType_ConcatTree,
        ConcatType_BufferString
    };

    class StringProfiler
    {
    private:
        ArenaAllocator allocator;
        ThreadContextId mainThreadId;
        uint discardedWrongThread;  // If called on the wrong thread, then statistics are inaccurate

        // Profiling strings records the frequency of each length of string.
        // It also records whether the characters in the string may be encoded
        // as 7-bit ASCII, 8-bit ASCII or if they really require 16bit encoding
        enum RequiredEncoding
        {
            ASCII7bit,
            ASCII8bit,
            Unicode16bit
        };

        struct StringMetrics
        {
            uint count7BitASCII;
            uint count8BitASCII;
            uint countUnicode;

            void Accumulate( RequiredEncoding encoding )
            {
                switch(encoding)
                {
                case ASCII7bit:
                    this->count7BitASCII++;
                    break;
                case ASCII8bit:
                    this->count8BitASCII++;
                    break;
                case Unicode16bit:
                    this->countUnicode++;
                    break;
                }
            }

            uint Total() const
            {
                return this->count7BitASCII +
                    this->count8BitASCII +
                    this->countUnicode;
            }

            void Accumulate(StringMetrics& rhs)
            {
                this->count7BitASCII += rhs.count7BitASCII;
                this->count8BitASCII += rhs.count8BitASCII;
                this->countUnicode   += rhs.countUnicode;
            }
        };

        struct ConcatMetrics
        {
            uint compoundStringCount;
            uint concatTreeCount;
            uint bufferStringBuilderCount;
            uint unknownCount;

            ConcatMetrics() {}

            ConcatMetrics(ConcatType concatType)
                : compoundStringCount(0), concatTreeCount(0), bufferStringBuilderCount(0), unknownCount(0)
            {
                this->Accumulate(concatType);
            }

            void Accumulate(ConcatType concatType)
            {
                switch(concatType)
                {
                case ConcatType_CompoundString:
                    this->compoundStringCount++;
                    break;
                case ConcatType_ConcatTree:
                    this->concatTreeCount++;
                    break;
                case ConcatType_BufferString:
                    this->bufferStringBuilderCount++;
                    break;
                case ConcatType_Unknown:
                    this->unknownCount++;
                    break;
                }
            }
            uint Total() const
            {
                return this->compoundStringCount +
                    this->concatTreeCount +
                    this->bufferStringBuilderCount +
                    this->unknownCount;
            }

        };

        uint embeddedNULChars;   // Total number of embedded NUL chars in all strings
        uint embeddedNULStrings; // Number of strings with at least one embedded NUL char

        uint emptyStrings;      // # of requests for zero-length strings (literals or BufferStrings)
        uint singleCharStrings; // # of requests for single-char strings (literals of BufferStrings)

        JsUtil::BaseDictionary<uint, StringMetrics, ArenaAllocator> stringLengthMetrics;

        struct UintUintPair
        {
            uint first;
            uint second;

            bool operator==(UintUintPair const& other) const
            {
                return this->first == other.first && this->second == other.second;
            }

            operator uint() const
            {
                return this->first | (this->second << 16);
            }
        };

        JsUtil::BaseDictionary< UintUintPair, ConcatMetrics, ArenaAllocator, PrimeSizePolicy > stringConcatMetrics;

        bool IsOnWrongThread() const;

        static RequiredEncoding GetRequiredEncoding( const wchar_t* sz, uint length );
        static uint CountEmbeddedNULs( const wchar_t* sz, uint length );

        class HistogramIndex
        {
            UintUintPair* index; // index of "length" and "frequency"
            uint count;

        public:
            HistogramIndex( ArenaAllocator* allocator, uint size );

            void Add( uint len, uint freq );
            void SortDescending();
            uint Get( uint i ) const;
            uint Count() const;

        private:
            static int __cdecl CompareDescending( const void* lhs, const void* rhs );
        };

        static void BuildIndex( unsigned int len, StringMetrics metrics, HistogramIndex* histogram, uint* total );
        static void PrintOne( unsigned int len, StringMetrics metrics, uint totalCount );

        static void PrintUintOrLarge( uint val );
        static void PrintOneConcat( UintUintPair const& key, const ConcatMetrics& metrics);

        void RecordNewString( const wchar_t* sz, uint length );
        void RecordConcatenation( uint lenLeft, uint lenRight, ConcatType type);

        static const uint k_MaxConcatLength = 20; // Strings longer than this are just "large"

    public:
        StringProfiler(PageAllocator * pageAllocator);

        void PrintAll();

        static void RecordNewString( ScriptContext* scriptContext, const wchar_t* sz, uint length );
        static void RecordConcatenation( ScriptContext* scriptContext, uint lenLeft, uint lenRight, ConcatType type = ConcatType_Unknown);
        static void RecordEmptyStringRequest( ScriptContext* scriptContext );
        static void RecordSingleCharStringRequest( ScriptContext* scriptContext );
    };
} // namespace Js

#endif
