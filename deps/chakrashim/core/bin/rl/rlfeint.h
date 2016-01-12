//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// RLFEINT.H

#define RLFE_VERSION 1
#undef UNICODE
#undef _UNICODE


#define PIPE_NAME "\\\\.\\pipe\\rlfepipe"
#define PIPE_BUF_SIZE 1024 * 16
#define PIPE_SEP_CHAR '|'

// RLFE thread reads first three bytes:
// byte 1 = command code
// bytes 2/3 = HI/lo length of command (not including first three bytes)
//
// Then depending on command, reads 'length' bytes and processes appropriately
//
// Note: we use | as a separator because we allow \ in directory names

enum RLFE_COMMAND {
    RLFE_INIT = 0,
    RLFE_ADD_ROOT,
    RLFE_ADD_TEST,
    RLFE_ADD_LOG,
    RLFE_TEST_STATUS,
    RLFE_THREAD_DIR,
    RLFE_THREAD_STATUS,
    RLFE_DONE,
    RLFE_COUNT
};

#ifndef _RL_STATS_DEFINED
#define _RL_STATS_DEFINED
// Statistics index.
typedef enum RL_STATS {
    RLS_TOTAL = 0, // overall stats
    RLS_EXE, // should == RLS_TOTAL at this time
    RLS_BASELINES, // baseline stats (== total if !FBaseDiff)
    RLS_DIFFS, // diff stats (== total if !FBaseDiff)
    RLS_COUNT
};
#endif

enum RLFE_STATUS {
    RLFES_PASSED,
    RLFES_FAILED,
    RLFES_DIFFED
};
