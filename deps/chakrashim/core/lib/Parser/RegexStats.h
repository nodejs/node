//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#if ENABLE_REGEX_CONFIG_OPTIONS

namespace UnifiedRegex
{
    struct RegexStats
    {
        typedef long long Ticks;

        enum Phase
        {
            Parse,
            Compile,
            Execute,
            NumPhases
        };

        static const wchar_t* PhaseNames[NumPhases];

        enum Use
        {
            Match,
            Exec,
            Test,
            Replace,
            Split,
            Search,
            NumUses
        };

        static const wchar_t* UseNames[NumUses];

        RegexPattern* pattern; // null => total record

        // Time spent on regex
        Ticks phaseTicks[NumPhases];
        // How is regex used?
        uint64 useCounts[NumUses];
        // Total input length
        uint64 inputLength;
        // Total chars looked at (may be > length if backtrack, < length if using Boyer-Moore)
        uint64 numCompares;
        // Number of continuation stack pushes
        uint64 numPushes;
        // Number of continuation stack pops
        uint64 numPops;
        // Continuation stack high-water-mark
        uint64 stackHWM;
        // Number of instructions executed
        uint64 numInsts;

        RegexStats(RegexPattern* pattern);

        void Print(DebugWriter* w, RegexStats* totals, Ticks ticksPerMillisecond);
        void Add(RegexStats* other);
    };

    typedef JsUtil::BaseDictionary<Js::InternalString, RegexStats*, ArenaAllocator, PrimeSizePolicy, DefaultComparer, JsUtil::DictionaryEntry> RegexStatsMap;

    class RegexStatsDatabase
    {
    private:
        RegexStats::Ticks ticksPerMillisecond;
        RegexStats::Ticks start;
        ArenaAllocator* allocator;
        RegexStatsMap* map;

        static RegexStats::Ticks Now();
        static RegexStats::Ticks Freq();

    public:
        RegexStatsDatabase(ArenaAllocator* allocator);

        RegexStats* GetRegexStats(RegexPattern* pattern);

        void BeginProfile();
        void EndProfile(RegexStats* stats, RegexStats::Phase phase);

        void Print(DebugWriter* w);
    };
}

#endif

