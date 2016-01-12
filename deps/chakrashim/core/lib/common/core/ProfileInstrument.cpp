//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#ifdef PROFILE_EXEC
#include "core\ProfileInstrument.h"

#define HIRES_PROFILER
namespace Js
{
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class Profiler::UnitData
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------


    ///----------------------------------------------------------------------------
    ///
    /// UnitData::UnitData
    ///
    /// Constructor
    ///
    ///----------------------------------------------------------------------------

    UnitData::UnitData()
    {
        this->incl  = 0;
        this->excl  = 0;
        this->max   = 0;
        this->count = 0;
    }

    ///----------------------------------------------------------------------------
    ///
    /// UnitData::Add
    ///
    ///----------------------------------------------------------------------------

    void
    UnitData::Add(TimeStamp incl, TimeStamp excl)
    {
        this->incl      += incl;
        this->excl      += excl;
        this->count++;
        if (incl > this->max)
        {
            this->max = incl;
        }
    }

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class Profiler
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Profiler
    ///
    /// Constructor
    ///
    ///----------------------------------------------------------------------------

    Profiler::Profiler(ArenaAllocator * allocator) :
        alloc(allocator),
        rootNode(NULL)
    {
        this->curNode = &this->rootNode;

        for(int i = 0; i < PhaseCount; i++)
        {
            this->inclSumAtLevel[i] = 0;
        }
    }


    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Begin
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Begin(Phase tag)
    {
        Push(TimeEntry(tag, GetTime()));
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::End
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::End(Phase tag)
    {
        Pop(TimeEntry(tag, GetTime()));
    }

    void
    Profiler::Suspend(Phase tag, SuspendRecord * suspendRecord)
    {
        suspendRecord->count = 0;
        Phase topTag;
        do
        {
            topTag = timeStack.Peek()->tag;
            Pop(TimeEntry(topTag, GetTime()));
            suspendRecord->phase[suspendRecord->count++] = topTag;
        } while(topTag != tag);
    }

    void
    Profiler::Resume(SuspendRecord * suspendRecord)
    {
        while (suspendRecord->count)
        {
            suspendRecord->count--;
            Begin(suspendRecord->phase[suspendRecord->count]);
        }
    }
    ///----------------------------------------------------------------------------
    ///
    /// Profiler::EndAllUpTo
    ///
    /// Ends all phases up to the specified phase. Useful for catching exceptions
    /// after a phase was started, and ending all intermediate phases until the
    /// first phase that was started.
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::EndAllUpTo(Phase tag)
    {
        Phase topTag;
        do
        {
            topTag = timeStack.Peek()->tag;
            Pop(TimeEntry(topTag, GetTime()));
        } while(topTag != tag);
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Push
    ///
    ///     1.  Push entry on stack.
    ///     2.  Update curNode
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Push(TimeEntry entry)
    {
        AssertMsg(NULL != curNode, "Profiler Stack Corruption");

        this->timeStack.Push(entry);
        if(!curNode->ChildExistsAt(entry.tag))
        {
            TypeNode * node = AnewNoThrow(this->alloc, TypeNode, curNode);
            // We crash if we run out of memory here and we don't care
            curNode->SetChildAt(entry.tag, node);
        }
        curNode = curNode->GetChildAt(entry.tag);
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Pop
    ///
    /// Core logic for the timer. Calculated the exclusive, inclusive times.
    /// There is a list inclSumAtLevel which stores accumulates the inclusive sum
    /// of all the tags that where 'pushed' after this tag.
    ///
    /// Consider the following calls. fx indicates Push and fx', the corresponding Pop
    ///
    /// f1
    ///     f2
    ///         f3
    ///         f3'
    ///     f2'
    ///     f4
    ///         f5
    ///         f5'
    ///     f4'
    /// f1'
    ///
    /// calculating the inclusive times are trivial. Let us calculate the exclusive
    /// time for f1. That would be
    ///         excl(f1) = incl(f1) - [incl(f2) + incl(f4)]
    ///
    /// Basically if a function is at level 'x' then we need to deduct from its
    /// exclusive times, the inclusive times of all the functions at level 'x + 1'
    /// We don't care about deeper levels. Hence 'inclSumAtLevel' array which accumulates
    /// the sum of variables at different levels.
    ///
    /// Reseting the next level is also required. In the above example, f3 and f5 are
    /// at the same level. if we don't reset level 3 when popping f2, then we will
    /// have wrong sums for f4. So once a tag has been popped, all sums at its higher
    /// levels is set to zero. (Of course we just need to reset the next level and
    /// all above levels will invariably remain zero)
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Pop(TimeEntry curEntry)
    {
        int         curLevel                = this->timeStack.Count();
        TimeEntry   *entry                   = this->timeStack.Pop();

        AssertMsg(entry->tag == curEntry.tag, "Profiler Stack corruption, push pop entries do not correspond to the same tag");

        TimeStamp   inclusive               = curEntry.time - entry->time;
        TimeStamp   exclusive               = inclusive - this->inclSumAtLevel[curLevel +1];

        Assert(inclusive >= 0);
        Assert(exclusive >= 0);

        this->inclSumAtLevel[curLevel + 1]  = 0;
        this->inclSumAtLevel[curLevel]     += inclusive;

        curNode->GetValue()->Add(inclusive, exclusive);
        curNode                             = curNode->GetParent();

        AssertMsg(curNode != NULL, "Profiler stack corruption");
    }

    void
    Profiler::Merge(Profiler * profiler)
    {
        MergeTree(&rootNode, &profiler->rootNode);
        if (profiler->timeStack.Count() > 1)
        {
            FixedStack<TimeEntry, MaxStackDepth> reverseStack;
            do
            {
                reverseStack.Push(*profiler->timeStack.Pop());
            }
            while (profiler->timeStack.Count() > 1);

            do
            {
                TimeEntry * entry = reverseStack.Pop();
                this->Push(*entry);
                profiler->timeStack.Push(*entry);
            }
            while (reverseStack.Count() != 0);
        }
    }

    void
    Profiler::MergeTree(TypeNode * toNode, TypeNode * fromNode)
    {
        UnitData * toData = toNode->GetValue();
        const UnitData * fromData = fromNode->GetValue();

        toData->count += fromData->count;
        toData->incl += fromData->incl;
        toData->excl += fromData->excl;
        if (fromData->max > toData->max)
        {
            toData->max = fromData->max;
        }
        for (int i = 0; i < PhaseCount; i++)
        {
            if (fromNode->ChildExistsAt(i))
            {
                TypeNode * fromChild = fromNode->GetChildAt(i);
                TypeNode * toChild;
                if (!toNode->ChildExistsAt(i))
                {
                    toChild = Anew(this->alloc, TypeNode, toNode);
                    toNode->SetChildAt(i, toChild);
                }
                else
                {
                    toChild = toNode->GetChildAt(i);
                }
                MergeTree(toChild, fromChild);
            }
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::PrintTree
    ///
    /// Private method that walks the tree and prints it recursively.
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::PrintTree(TypeNode *node, TypeNode *baseNode, int column, TimeStamp freq)
    {
        const UnitData *base        = baseNode->GetValue();

        for(int i = 0; i < PhaseCount; i++)
        {
            if(node->ChildExistsAt(i))
            {
                UnitData *data = node->GetChildAt(i)->GetValue();
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
                if( int(data->incl * 100 / base->incl) >= Configuration::Global.flags.ProfileThreshold) // threshold
#endif
                {

                    Output::SkipToColumn(column);

                    Output::Print(L"%-*s %7.1f %5d %7.1f %5d %7.1f %7.1f %5d\n",
                            (Profiler::PhaseNameWidth-column), PhaseNames[i],
                            (double)data->incl / freq ,                    // incl
                            int(data->incl * 100 / base->incl ),        // incl %
                            (double)data->excl / freq ,                    // excl
                            int(data->excl * 100 / base->incl ),        // excl %
                            (double)data->max  / freq ,                    // max
                            (double)data->incl / ( freq * data->count ),   // mean
                            int(data->count)                            // count
                           );
                }

                PrintTree(node->GetChildAt(i), baseNode, column + Profiler::TabWidth, freq);
            }
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::Print
    ///
    /// Pretty printer
    ///
    ///----------------------------------------------------------------------------

    void
    Profiler::Print(Phase baseTag)
    {
        if (baseTag == InvalidPhase)
        {
            baseTag = AllPhase;     // default to all phase
        }
        const TimeStamp freq = this->GetFrequency();

        bool foundNode = false;
        ForEachNode(baseTag, &rootNode, [&](TypeNode *const baseNode, const Phase parentTag)
        {
            if(!foundNode)
            {
                foundNode = true;
                Output::Print(L"%-*s:%7s %5s %7s %5s %7s %7s %5s\n",
                                (Profiler::PhaseNameWidth-0),
                                L"Profiler Report",
                                L"Incl",
                                L"(%)",
                                L"Excl",
                                L"(%)",
                                L"Max",
                                L"Mean",
                                L"Count"
                                );
                Output::Print(L"-------------------------------------------------------------------------------\n");
            }

            UnitData *data      = baseNode->GetValue();

            if(0 == data->count)
            {
                Output::Print(L"The phase : %s was never started", PhaseNames[baseTag]);
                return;
            }

            int indent = 0;

            if(parentTag != InvalidPhase)
            {
                TypeNode *const parentNode = baseNode->GetParent();
                Assert(parentNode);

                Output::Print(L"%-*s\n", (Profiler::PhaseNameWidth-0), PhaseNames[parentTag]);
                indent += Profiler::TabWidth;
            }

            if(indent)
            {
                Output::SkipToColumn(indent);
            }
            Output::Print(L"%-*s %7.1f %5d %7.1f %5d %7.1f %7.1f %5d\n",
                    (Profiler::PhaseNameWidth-indent),
                    PhaseNames[baseTag],
                    (double)data->incl / freq ,                 // incl
                    int(100),                                   // incl %
                    (double)data->excl / freq ,                 // excl
                    int(data->excl * 100 / data->incl ),        // excl %
                    (double)data->max  / freq ,                 // max
                    (double)data->incl / ( freq * data->count ),// mean
                    int(data->count)                            // count
                    );
            indent += Profiler::TabWidth;

            PrintTree(baseNode, baseNode, indent, freq);
        });

        if(foundNode)
        {
            Output::Print(L"-------------------------------------------------------------------------------\n");
            Output::Flush();
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::FindNode
    ///
    /// Does a tree traversal(DFS) and finds the first occurrence of the 'tag'
    ///
    ///----------------------------------------------------------------------------

    template<class FVisit>
    void
    Profiler::ForEachNode(Phase tag, TypeNode *node, FVisit visit, Phase parentTag)
    {
        AssertMsg(node != NULL, "Invalid usage: node must always be non null");

        for(int i = 0; i < PhaseCount; i++)
        {
            if(node->ChildExistsAt(i))
            {
                TypeNode * child = node->GetChildAt(i);
                if(i == tag)
                {
                    visit(child, parentTag);
                }
                else
                {
                    ForEachNode(tag, child, visit, static_cast<Phase>(i));
                }
            }
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// Profiler::GetTime
    ///
    ///----------------------------------------------------------------------------

    TimeStamp
    Profiler::GetTime()
    {
#if !defined HIRES_PROFILER && (defined(_M_IX86) || defined(_M_X64))
        return __rdtsc();
#else
        LARGE_INTEGER tmp;
        if(QueryPerformanceCounter(&tmp))
        {
            return tmp.QuadPart;
        }
        else
        {
            AssertMsg(0, "Could not get time. Don't know what to do");
            return 0;
        }
#endif
    }


    ///----------------------------------------------------------------------------
    ///
    /// Profiler::GetFrequency
    ///
    ///----------------------------------------------------------------------------

    TimeStamp
    Profiler::GetFrequency()
    {
#if !defined HIRES_PROFILER && (defined(_M_IX86) || defined(_M_X64))
        long long start, end;
        int CPUInfo[4];

        // Flush pipeline
        __cpuid(CPUInfo, 0);

        // Measure 1 second / 5
        start = GetTime();
        Sleep(1000/5);
        end = GetTime();

        return  ((end - start) * 5) / FrequencyScale;
#else
        LARGE_INTEGER tmp;
        if(QueryPerformanceFrequency(&tmp))
        {
            return tmp.QuadPart / FrequencyScale;
        }
        else
        {
            AssertMsg(0, "Could not get time. Don't know what to do");
            return 0;
        }
#endif
    }


} //namespace Js

#endif
