//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

/***************************************************************************
NoReleaseAllocator - allocator that never releases until it is destroyed
***************************************************************************/
class NoReleaseAllocator
{
public:
    NoReleaseAllocator(long cbFirst = 256, long cbMax = 0x4000 /*16K*/);
    ~NoReleaseAllocator(void) { FreeAll(); }

    void *Alloc(long cb);
    void FreeAll();
    void Clear() { FreeAll(); }

private:
    struct NraBlock
    {
        NraBlock * pblkNext;
        // ... DATA ...
    };
    NraBlock * m_pblkList;
    long m_ibCur;
    long m_ibMax;
    long m_cbMinBlock;
    long m_cbMaxBlock;

#if DEBUG
    long m_cbTotRequested;    // total bytes requested
    long m_cbTotAlloced;    // total bytes allocated including headers
    long m_cblk;            // number of blocks including big blocks
    long m_cpvBig;            // each generates its own big block
    long m_cpvSmall;        // put in a common block
#endif //DEBUG
};
