//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

#if DEBUG
#define DEBUG_TRASHMEM(pv, cb) memset(pv, 0xbc, cb)
#else
#define DEBUG_TRASHMEM
#endif //DEBUG

#if _WIN64
struct __ALIGN_FOO__ {
    int w1;
    double dbl;
};
#define ALIGN_FULL (offsetof(__ALIGN_FOO__, dbl))
#else
// Force check for 4 byte alignment to support Win98/ME
#define ALIGN_FULL 4
#endif // _WIN64

#define AlignFull(VALUE) (~(~((VALUE) + (ALIGN_FULL-1)) | (ALIGN_FULL-1)))

NoReleaseAllocator::NoReleaseAllocator(long cbFirst, long cbMax)
    : m_pblkList(NULL)
    , m_ibCur(0)
    , m_ibMax(0)
    , m_cbMinBlock(cbFirst)
    , m_cbMaxBlock(cbMax)
#if DEBUG
    , m_cbTotRequested(0)
    , m_cbTotAlloced(0)
    , m_cblk(0)
    , m_cpvBig(0)
    , m_cpvSmall(0)
#endif
{
    // require reasonable ranges
    Assert((0 < cbFirst) && (cbFirst < SHRT_MAX/2));
    Assert((0 < cbMax  ) && (cbMax   < SHRT_MAX));
}

void * NoReleaseAllocator::Alloc(long cb)
{
    Assert(cb > 0);
    if (cb <= 0)
        return NULL;

    const long kcbHead = AlignFull(sizeof(NoReleaseAllocator::NraBlock));
    void * pv;

    if (cb > m_ibMax - m_ibCur)
    {
        long cbBlock;
        long cbAlloc;
        NraBlock * pblk;

        if (cb >= m_cbMaxBlock)
        {
            // check for integer overflow before allocating (See WindowsSE #88972)
            cbAlloc = cb + kcbHead;
            if (cbAlloc < cb)
            {
                Assert(FALSE); // too big!
                return NULL;
            }

            // create a chunk just for this allocation
            pblk = (NraBlock *)malloc(cbAlloc);
            if (NULL == pblk)
                return NULL;
#if DEBUG
            m_cbTotAlloced   += cbAlloc;
            m_cbTotRequested += cb;
            m_cpvBig++;
            m_cblk++;
#endif //DEBUG
            if (m_ibCur < m_ibMax)
            {
                // There is still room in current block, so put the new block
                // after the current block.
                pblk->pblkNext = m_pblkList->pblkNext;
                m_pblkList->pblkNext = pblk;
            }
            else
            {
                // Link into front of the list.
                // Don't need to adjust m_ibCur and m_ibMax, because they
                // already have the correct relationship for this full block
                // (m_ibCur >= m_ibMax) and the actual values will not be
                // used.
                pblk->pblkNext = m_pblkList;
                m_pblkList = pblk;
            }
            DEBUG_TRASHMEM((byte *)pblk + kcbHead, cb);
            return (byte *)pblk + kcbHead;
        }

        cbBlock = cb;                 // requested size
        if (m_ibMax > cbBlock)        // at least current block size
            cbBlock = m_ibMax;
        cbBlock += cbBlock;           // *2 (can overflow, but checked below)
        if (m_cbMinBlock > cbBlock)   // at least minimum size
            cbBlock = m_cbMinBlock;
        if (cbBlock > m_cbMaxBlock)   // no larger than the max
            cbBlock = m_cbMaxBlock;
        if (cb > cbBlock)             // guarantee it's big enough
        {
            Assert(("Request too large", FALSE));
            return NULL;
        }

        // check for integer overflow before allocating (See WindowsSE #88972)
        cbAlloc = cbBlock + kcbHead;
        if ((cbAlloc < cbBlock) || (cbAlloc < cb))
        {
            Assert(FALSE); // too big!
            return NULL ;
        }

        // allocate a new block
        pblk = (NraBlock *)malloc(cbAlloc);
#ifdef MEM_TRACK
        RegisterAlloc((char*)pblk,cbAlloc);
#endif
        if (NULL == pblk)
            return NULL;
#if DEBUG
        m_cbTotAlloced += cbAlloc;
        m_cblk++;
#endif //DEBUG
        pblk->pblkNext = m_pblkList;
        m_pblkList = pblk;
        m_ibMax = cbBlock;
        m_ibCur = 0;
    }
    Assert(m_ibCur + cb <= m_ibMax);

#if DEBUG
    m_cbTotRequested += cb;
    m_cpvSmall++;
#endif //DEBUG
    pv = (byte *)m_pblkList + kcbHead + m_ibCur;
    DEBUG_TRASHMEM(pv, cb);
    m_ibCur += (long)AlignFull(cb);
    Assert(m_ibCur >= 0);
    return pv;
}

void NoReleaseAllocator::FreeAll(void)
{
    // Free all of the allocated blocks
    while (NULL != m_pblkList)
    {
        NraBlock * pblk = m_pblkList;
#pragma prefast(suppress:6001, "Not sure why it is complaining *m_plkList is uninitialized")
        m_pblkList = pblk->pblkNext;
        free(pblk);
    }

    // prepare for next round of allocations
    m_ibCur = m_ibMax = 0;
#if DEBUG
    m_cbTotRequested = 0;
    m_cbTotAlloced = 0;
    m_cblk = 0;
    m_cpvBig = 0;
    m_cpvSmall = 0;
#endif
}
