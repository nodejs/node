//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
enum CollectionState
{
    Collection_Mark                 = 0x00000001,
    Collection_Sweep                = 0x00000002,
    Collection_Exit                 = 0x00000004,
    Collection_PreCollection        = 0x00000008,

    // Mark related states
    Collection_ResetMarks           = 0x00000010,
    Collection_FindRoots            = 0x00000020,
#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    Collection_Rescan               = 0x00000040,
#endif
    Collection_FinishMark           = 0x00000080,

    // Sweep related states
#ifdef CONCURRENT_GC_ENABLED
    Collection_ConcurrentSweepSetup = 0x00000100,
#endif
    Collection_TransferSwept        = 0x00000200,

    // State attributes
#ifdef PARTIAL_GC_ENABLED
    Collection_Partial              = 0x00001000,
#endif
#ifdef CONCURRENT_GC_ENABLED
    Collection_Concurrent           = 0x00002000,
    Collection_ExecutingConcurrent  = 0x00004000,
    Collection_FinishConcurrent     = 0x00008000,

    Collection_ConcurrentMark       = Collection_Concurrent | Collection_Mark,
    Collection_ConcurrentSweep      = Collection_Concurrent | Collection_Sweep,
#endif
    Collection_Parallel             = 0x00010000,

    Collection_PostCollectionCallback     = 0x00020000,

    Collection_WrapperCallback      = 0x00040000,

    // Actual states
    CollectionStateNotCollecting          = 0,                                                                // not collecting
    CollectionStateResetMarks             = Collection_Mark | Collection_ResetMarks,                          // reset marks
    CollectionStateFindRoots              = Collection_Mark | Collection_FindRoots,                           // finding roots
    CollectionStateMark                   = Collection_Mark,                                                  // marking (in thread)
    CollectionStateSweep                  = Collection_Sweep,                                                 // sweeping (in thread)
    CollectionStateTransferSwept          = Collection_Sweep | Collection_TransferSwept,                      // transfer swept objects (after concurrent sweep)
    CollectionStateExit                   = Collection_Exit,                                                  // exiting concurrent thread


#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    CollectionStateRescanFindRoots        = Collection_Mark | Collection_Rescan | Collection_FindRoots,       // rescan (after concurrent mark)
    CollectionStateRescanMark             = Collection_Mark | Collection_Rescan,                              // rescan (after concurrent mark)
#endif
#ifdef CONCURRENT_GC_ENABLED
    CollectionStateConcurrentResetMarks   = Collection_ConcurrentMark | Collection_ResetMarks | Collection_ExecutingConcurrent,    // concurrent reset mark
    CollectionStateConcurrentFindRoots    = Collection_ConcurrentMark | Collection_FindRoots | Collection_ExecutingConcurrent,     // concurrent findroot
    CollectionStateConcurrentMark         = Collection_ConcurrentMark | Collection_ExecutingConcurrent,                            // concurrent marking
    CollectionStateRescanWait             = Collection_ConcurrentMark | Collection_FinishConcurrent,                               // rescan (after concurrent mark)
    CollectionStateConcurrentFinishMark   = Collection_ConcurrentMark | Collection_ExecutingConcurrent | Collection_FinishConcurrent,

    CollectionStateSetupConcurrentSweep   = Collection_Sweep | Collection_ConcurrentSweepSetup,               // setting up concurrent sweep
    CollectionStateConcurrentSweep        = Collection_ConcurrentSweep | Collection_ExecutingConcurrent,      // concurrent sweep
    CollectionStateTransferSweptWait      = Collection_ConcurrentSweep | Collection_FinishConcurrent,         // transfer swept objects (after concurrent sweep)
#endif
    CollectionStateParallelMark           = Collection_Mark | Collection_Parallel,
    CollectionStateBackgroundParallelMark = Collection_ConcurrentMark | Collection_ExecutingConcurrent | Collection_Parallel,

    CollectionStatePostCollectionCallback = Collection_PostCollectionCallback,

    CollectionStateConcurrentWrapperCallback = Collection_Concurrent | Collection_ExecutingConcurrent | Collection_WrapperCallback,

};
