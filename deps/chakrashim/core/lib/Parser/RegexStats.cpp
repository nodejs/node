//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

#if ENABLE_REGEX_CONFIG_OPTIONS

namespace UnifiedRegex
{
    const wchar_t* RegexStats::PhaseNames[RegexStats::NumPhases] = { L"parse", L"compile", L"execute" };
    const wchar_t* RegexStats::UseNames[RegexStats::NumUses] = { L"match", L"exec", L"test", L"replace", L"split", L"search" };

    RegexStats::RegexStats(RegexPattern* pattern)
        : pattern(pattern)
        , inputLength(0)
        , numCompares(0)
        , numPushes(0)
        , numPops(0)
        , stackHWM(0)
        , numInsts(0)
    {
        for (int i = 0; i < NumPhases; i++)
            phaseTicks[i] = 0;
        for (int i = 0; i < NumUses; i++)
            useCounts[i] = 0;
    }

    void RegexStats::Print(DebugWriter* w, RegexStats* totals, Ticks ticksPerMillisecond)
    {
        if (pattern == 0)
            w->PrintEOL(L"TOTAL");
        else
            pattern->Print(w);
        w->EOL();
        w->Indent();

        for (int i = 0; i < NumPhases; i++)
        {
            double ms = (double)phaseTicks[i] / (double)ticksPerMillisecond;
            if (totals == 0 || totals->phaseTicks[i] == 0)
                w->PrintEOL(L"%-12s: %10.4fms", PhaseNames[i], ms);
            else
            {
                double pc = (double)phaseTicks[i] * 100.0  / (double)totals->phaseTicks[i];
                w->PrintEOL(L"%-12s: %10.4fms (%10.4f%%)", PhaseNames[i], ms, pc);
            }
        }

        for (int i = 0; i < NumUses; i++)
        {
            if (useCounts[i] > 0)
            {
                if (totals == 0 || totals->useCounts[i] == 0)
                    w->PrintEOL(L"#%-11s: %10I64u", UseNames[i], useCounts[i]);
                else
                {
                    double pc = (double)useCounts[i] * 100.0 / (double)totals->useCounts[i];
                    w->PrintEOL(L"#%-11s: %10I64u   (%10.4f%%)", UseNames[i], useCounts[i], pc);
                }
            }
        }

        if (inputLength > 0)
        {
            double r = (double)numCompares * 100.0 / (double)inputLength;
            if (totals == 0 || totals->numCompares == 0)
                w->PrintEOL(L"numCompares : %10.4f%%", r);
            else
            {
                double pc = (double)numCompares * 100.0 / (double)totals->numCompares;
                w->PrintEOL(L"numCompares : %10.4f%%  (%10.4f%%)", r, pc);
            }
        }

        if (totals == 0 || totals->inputLength == 0)
            w->PrintEOL(L"inputLength : %10I64u", inputLength);
        else
        {
            double pc = (double)inputLength * 100.0 / (double)totals->inputLength;
            w->PrintEOL(L"inputLength : %10I64u   (%10.4f%%)", inputLength, pc);
        }

        if (totals == 0 || totals->numPushes == 0)
            w->PrintEOL(L"numPushes   : %10I64u", numPushes);
        else
        {
            double pc = (double)numPushes * 100.0 / (double)totals->numPushes;
            w->PrintEOL(L"numPushes   : %10I64u   (%10.4f%%)", numPushes, pc);
        }

        if (totals == 0 || totals->numPops == 0)
            w->PrintEOL(L"numPops     : %10I64u", numPops);
        else
        {
            double pc = (double)numPops * 100.0 / (double)totals->numPops;
            w->PrintEOL(L"numPops     : %10I64u   (%10.4f%%)", numPops, pc);
        }

        if (totals == 0 || totals->stackHWM == 0)
            w->PrintEOL(L"stackHWM    : %10I64u", stackHWM);
        else
        {
            double pc = (double)stackHWM * 100.0 / (double)totals->stackHWM;
            w->PrintEOL(L"stackHWM    : %10I64u   (%10.4f%%)", stackHWM, pc);
        }

        if (totals == 0 || totals->numInsts == 0)
            w->PrintEOL(L"numInsts    : %10I64u", numInsts);
        else
        {
            double pc = (double)numInsts * 100.0 / (double)totals->numInsts;
            w->PrintEOL(L"numInsts    : %10I64u   (%10.4f%%)", numInsts, pc);
        }

        w->Unindent();
    }

    void RegexStats::Add(RegexStats* other)
    {
        for (int i = 0; i < NumPhases; i++)
            phaseTicks[i] += other->phaseTicks[i];
        for (int i = 0; i < NumUses; i++)
            useCounts[i] += other->useCounts[i];
        inputLength += other->inputLength;
        numCompares += other->numCompares;
        numPushes += other->numPushes;
        numPops += other->numPops;
        if (other->stackHWM > stackHWM)
            stackHWM = other->stackHWM;
        numInsts += other->numInsts;
    }

    RegexStats::Ticks RegexStatsDatabase::Now()
    {
        LARGE_INTEGER tmp;
        if (QueryPerformanceCounter(&tmp))
            return tmp.QuadPart;
        else
        {
            Assert(false);
            return 0;
        }
    }

    RegexStats::Ticks RegexStatsDatabase::Freq()
    {
        LARGE_INTEGER tmp;
        if (QueryPerformanceFrequency(&tmp))
        {
            return tmp.QuadPart / 1000;
        }
        else
        {
            Assert(false);
            return 1;
        }
    }

    RegexStatsDatabase::RegexStatsDatabase(ArenaAllocator* allocator)
        : start(0), allocator(allocator)
    {
        ticksPerMillisecond = Freq();
        map = Anew(allocator, RegexStatsMap, allocator, 17);
    }

    RegexStats* RegexStatsDatabase::GetRegexStats(RegexPattern* pattern)
    {
        Js::InternalString str = pattern->GetSource();
        RegexStats *res;
        if (!map->TryGetValue(str, &res))
        {
            res = Anew(allocator, RegexStats, pattern);
            map->Add(str, res);
        }
        return res;
    }

    void RegexStatsDatabase::BeginProfile()
    {
        start = Now();
    }

    void RegexStatsDatabase::EndProfile(RegexStats* stats, RegexStats::Phase phase)
    {
        stats->phaseTicks[phase] += Now() - start;
    }

    void RegexStatsDatabase::Print(DebugWriter* w)
    {
        RegexStats totals(0);

        Output::Print(L"Regular Expression Statistics\n");
        Output::Print(L"=============================\n");

        for (int i = 0; i < map->Count(); i++)
            totals.Add(map->GetValueAt(i));

        for (int i = 0; i < map->Count(); i++)
            map->GetValueAt(i)->Print(w, &totals, ticksPerMillisecond);

        totals.Print(w, 0, ticksPerMillisecond);

        allocator->Free(w, sizeof(DebugWriter));
    }
}

#endif
