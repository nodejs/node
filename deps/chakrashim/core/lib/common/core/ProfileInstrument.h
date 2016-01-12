//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef PROFILE_EXEC
#include "DataStructures\FixedStack.h"
#include "DataStructures\Tree.h"
namespace Js
{

    //
    // Type of each timestamp.
    //

    typedef long long           TimeStamp;


    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// struct TimeEntry
    ///
    /// Simple pair of (tag, timestamp).
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------


    struct TimeEntry
    {
        Phase          tag;
        TimeStamp           time;

        explicit TimeEntry(Phase profileTag = InvalidPhase, TimeStamp curTime = 0) :
            tag(profileTag),
            time(curTime)
        {}
    };


    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// struct UnitData
    ///
    /// Stores the elemental data for individual calls. Accumulates the inclusive,
    /// exclusive sums, count, and maximum.
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------


    struct UnitData
    {
    // Data
    public:
                uint                count;
                TimeStamp           incl;
                TimeStamp           excl;
                TimeStamp           max;

    // Constructor
    public:
        UnitData();

    // Methods
    public:
                void                Add(TimeStamp, TimeStamp);
    };

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class Profiler
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    class Profiler
    {

        friend class ScriptContextProfiler;
    // Local types
    private:

        typedef TreeNode<UnitData, PhaseCount> TypeNode;


    // Data
    private:

        //
        // Maximum depth of the stack. This should be some safe limit
        //
        static const int        MaxStackDepth   = 20;

        //
        // Controls the scale of the measuring clock. a value of 1000 indicates
        // millisecond, 1000,000 implies micro-seconds.
        //
        static const int        FrequencyScale  = 1000;

        //
        // Used for printing the report
        //
        static const int        TabWidth        = 1;

        static const int        PhaseNameWidth = 27;

                FixedStack<TimeEntry, MaxStackDepth>
                                timeStack;
                TimeStamp       inclSumAtLevel[PhaseCount];

                TypeNode        rootNode;
                TypeNode        *curNode;

                ArenaAllocator  *alloc;


    // Constructor
    public:
                Profiler(ArenaAllocator *alloc);

    // Implementation
    private:

        static  TimeStamp       GetTime();
        static  TimeStamp       GetFrequency();

                template<class FVisit>
        static  void            ForEachNode(Phase tag, TypeNode *curNode, FVisit visit, Phase parentTag = InvalidPhase);

                void            Push(TimeEntry entry);
                void            Pop(TimeEntry entry);
                void            PrintTree(TypeNode *curNode, TypeNode *baseNode, int, TimeStamp freq);
                void            MergeTree(TypeNode *toNode, TypeNode *fromNode);

    // Methods
    public:

                class SuspendRecord
                {
                private:
                    Phase phase[MaxStackDepth];
                    uint count;

                    friend class Profiler;
                };

                void            Begin(Phase tag);
                void            End(Phase tag);
                void            EndAllUpTo(Phase tag);
                void            Print(Phase baseTag);
                void            Merge(Profiler * profiler);

                void            Suspend(Phase tag, SuspendRecord * suspendRecord);
                void            Resume(SuspendRecord * suspendRecord);
    };

} //namespace Js

#define ASYNC_HOST_OPERATION_START(threadContext) {Js::Profiler::SuspendRecord __suspendRecord;  bool wasInAsync = threadContext->AsyncHostOperationStart(&__suspendRecord)
#define ASYNC_HOST_OPERATION_END(threadContext) threadContext->AsyncHostOperationEnd(wasInAsync, &__suspendRecord); }
#elif DBG
#define ASYNC_HOST_OPERATION_START(threadContext) { bool wasInAsync = threadContext->AsyncHostOperationStart(null)
#define ASYNC_HOST_OPERATION_END(threadContext) threadContext->AsyncHostOperationEnd(wasInAsync, null)
#else
#define ASYNC_HOST_OPERATION_START(threadContext)
#define ASYNC_HOST_OPERATION_END(threadContext)
#endif
