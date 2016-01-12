//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"
#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif


const HashTbl::KWD HashTbl::g_mptkkwd[tkLimKwd] =
{
    { knopNone,0,knopNone,0 },
#define KEYWORD(tk,f,prec2,nop2,prec1,nop1,name) \
    { nop2,kopl##prec2,nop1,kopl##prec1 },
#define TOK_DCL(tk,prec2,nop2,prec1,nop1) \
    { nop2,kopl##prec2,nop1,kopl##prec1 },
#include "keywords.h"
};

const HashTbl::ReservedWordInfo HashTbl::s_reservedWordInfo[tkID] =
{
    { nullptr, fidNil },
#define KEYWORD(tk,f,prec2,nop2,prec1,nop1,name) \
        { &g_ssym_##name, f },
#include "keywords.h"
};

HashTbl * HashTbl::Create(uint cidHash, ErrHandler * perr)
{
    HashTbl * phtbl;

    if (nullptr == (phtbl = HeapNewNoThrow(HashTbl,perr)))
        return nullptr;
    if (!phtbl->Init(cidHash))
    {
        delete phtbl;
        return nullptr;
    }

    return phtbl;
}


BOOL HashTbl::Init(uint cidHash)
{
    // cidHash must be a power of two
    Assert(cidHash > 0 && 0 == (cidHash & (cidHash - 1)));

    long cb;

    /* Allocate and clear the hash bucket table */
    m_luMask = cidHash - 1;
    m_luCount = 0;

    // (Bug 1117873 - Windows OS Bugs)
    // Prefast: Verify that cidHash * sizeof(Ident *) does not cause an integer overflow
    // NoReleaseAllocator( ) takes long - so check for LONG_MAX
    // Win8 730594 - Use intsafe function to check for overflow.
    uint cbTemp;
    if (FAILED(UIntMult(cidHash, sizeof(Ident *), &cbTemp)) || cbTemp > LONG_MAX)
        return FALSE;

    cb = cbTemp;
    if (nullptr == (m_prgpidName = (Ident **)m_noReleaseAllocator.Alloc(cb)))
        return FALSE;
    memset(m_prgpidName, 0, cb);

#if PROFILE_DICTIONARY
    stats = DictionaryStats::Create(typeid(this).name(), cidHash);
#endif

    return TRUE;
}

void HashTbl::Grow()
{
    // Grow the bucket size by grow factor
    // Has the side-effect of inverting the order the pids appear in their respective buckets.
    uint cidHash = m_luMask + 1;
    uint n_cidHash = cidHash * GrowFactor;
    Assert(n_cidHash > 0 && 0 == (n_cidHash & (n_cidHash - 1)));

    // Win8 730594 - Use intsafe function to check for overflow.
    uint cbTemp;
    if (FAILED(UIntMult(n_cidHash, sizeof(Ident *), &cbTemp)) || cbTemp > LONG_MAX)
        // It is fine to exit early here, we will just have a potentially densely populated hash table
        return;
    long cb = cbTemp;
    uint n_luMask = n_cidHash - 1;

    IdentPtr *n_prgpidName = (IdentPtr *)m_noReleaseAllocator.Alloc(cb);
    if (n_prgpidName == nullptr)
        // It is fine to exit early here, we will just have a potentially densely populated hash table
        return;

    // Clear the array
    memset(n_prgpidName, 0, cb);

    // Place each entry its new bucket.
    for (uint i = 0; i < cidHash; i++)
    {
        for (IdentPtr pid = m_prgpidName[i], next = pid ? pid->m_pidNext : nullptr; pid; pid = next, next = pid ? pid->m_pidNext : nullptr)
        {
            ulong luHash = pid->m_luHash;
            ulong luIndex = luHash & n_luMask;
            pid->m_pidNext = n_prgpidName[luIndex];
            n_prgpidName[luIndex] = pid;
        }
    }

    Assert(CountAndVerifyItems(n_prgpidName, n_cidHash, n_luMask) == m_luCount);

    // Update the table fields.
    m_prgpidName = n_prgpidName;
    m_luMask= n_luMask;

#if PROFILE_DICTIONARY
    if(stats)
    {
        int emptyBuckets = 0;
        for (uint i = 0; i < n_cidHash; i++)
        {
            if(m_prgpidName[i] == nullptr)
            {
                emptyBuckets++;
            }
        }
        stats->Resize(n_cidHash, emptyBuckets);
    }
#endif
}

#if DEBUG
uint HashTbl::CountAndVerifyItems(IdentPtr *buckets, uint bucketCount, uint mask)
{
    uint count = 0;
    for (uint i = 0; i < bucketCount; i++)
        for (IdentPtr pid = buckets[i]; pid; pid = pid->m_pidNext)
        {
            Assert((pid->m_luHash & mask) == i);
            count++;
        }
    return count;
}
#endif

#pragma warning(push)
#pragma warning(disable:4740) // flow in or out of inline asm code suppresses global optimization
tokens Ident::Tk(bool isStrictMode)
{
    const tokens token = (tokens)m_tk;
    if (token == tkLim)
    {
        m_tk = tkNone;
        const ulong luHash = this->m_luHash;
        const LPCOLESTR prgch = Psz();
        const ulong cch = Cch();
        #include "kwds_sw.h"

        #define KEYWORD(tk,f,prec2,nop2,prec1,nop1,name) \
            LEqual_##name: \
                if (cch == g_ssym_##name.cch && \
                        0 == memcmp(g_ssym_##name.sz, prgch, cch * sizeof(OLECHAR))) \
                { \
                    if (f) \
                        this->m_grfid |= f; \
                    this->m_tk = tk; \
                    return ((f & fidKwdRsvd) || (isStrictMode && (f & fidKwdFutRsvd))) ? tk : tkID; \
                } \
                goto LDefault;
        #include "keywords.h"
LDefault:
        return tkID;
    }
    else if (token == tkNone || !(m_grfid & fidKwdRsvd))
    {
        if ( !isStrictMode || !(m_grfid & fidKwdFutRsvd))
        {
            return tkID;
        }
    }
    return token;
}
#pragma warning(pop)

void Ident::SetTk(tokens token, ushort grfid)
{
    Assert(token != tkNone && token < tkID);
    if (m_tk == tkLim)
    {
        m_tk = (ushort)token;
        m_grfid |= grfid;
    }
    else
    {
        Assert(m_tk == token);
        Assert((m_grfid & grfid) == grfid);
    }
}

IdentPtr HashTbl::PidFromTk(tokens token)
{
    Assert(token > tkNone && token < tkID);
    __analysis_assume(token > tkNone && token < tkID);
    // Create a pid so we can create a name node
    IdentPtr rpid = m_rpid[token];
    if (nullptr == rpid)
    {
        StaticSym const * sym = s_reservedWordInfo[token].sym;
        Assert(sym != nullptr);
        rpid = this->PidHashNameLenWithHash(sym->sz, sym->cch, sym->luHash);
        rpid->SetTk(token, s_reservedWordInfo[token].grfid);
        m_rpid[token] = rpid;
    }
    return rpid;
}

template <typename CharType>
IdentPtr HashTbl::PidHashNameLen(CharType const * prgch, ulong cch)
{
    // NOTE: We use case sensitive hash during compilation, but the runtime
    // uses case insensitive hashing so it can do case insensitive lookups.
    ulong luHash = CaseSensitiveComputeHashCch(prgch, cch);
    return PidHashNameLenWithHash(prgch, cch, luHash);
};
template IdentPtr HashTbl::PidHashNameLen<utf8char_t>(utf8char_t const * prgch, ulong cch);
template IdentPtr HashTbl::PidHashNameLen<char>(char const * prgch, ulong cch);
template IdentPtr HashTbl::PidHashNameLen<wchar_t>(wchar_t const * prgch, ulong cch);

template <typename CharType>
IdentPtr HashTbl::PidHashNameLenWithHash(_In_reads_(cch) CharType const * prgch, long cch, ulong luHash)
{
    Assert(cch >= 0);
    AssertArrMemR(prgch, cch);
    Assert(luHash == CaseSensitiveComputeHashCch(prgch, cch));

    IdentPtr * ppid;
    IdentPtr pid;
    long cb;
    long bucketCount;


#if PROFILE_DICTIONARY
    int depth = 0;
#endif

    pid = this->FindExistingPid(prgch, cch, luHash, &ppid, &bucketCount
#if PROFILE_DICTIONARY
                                , depth
#endif
        );
    if (pid)
    {
        return pid;
    }

    if (bucketCount > BucketLengthLimit && m_luCount > m_luMask)
    {
        Grow();

        // ppid is now invalid because the Grow() moves the entries around.
        // Find the correct ppid by repeating the find of the end of the bucket
        // the new item will be placed in.
        // Note this is similar to the main find loop but does not count nor does it
        // look at the entries because we already proved above the entry is not in the
        // table, we just want to find the end of the bucket.
        ppid = &m_prgpidName[luHash & m_luMask];
        while (*ppid)
            ppid = &(*ppid)->m_pidNext;
    }


#if PROFILE_DICTIONARY
    ++depth;
    if (stats)
        stats->Insert(depth);
#endif

    //Windows OS Bug 1795286 : CENTRAL PREFAST RUN: inetcore\scriptengines\src\src\core\hash.cpp :
    //               'sizeof((*pid))+((cch+1))*sizeof(OLECHAR)' may be smaller than
    //               '((cch+1))*sizeof(OLECHAR)'. This can be caused by integer overflows
    //               or underflows. This could yield an incorrect buffer all
    /* Allocate space for the identifier */
    ULONG Len;

    if (FAILED(ULongAdd(cch, 1, &Len)) ||
        FAILED(ULongMult(Len, sizeof(OLECHAR), &Len)) ||
        FAILED(ULongAdd(Len, sizeof(*pid), &Len)) ||
        FAILED(ULongToLong(Len, &cb)))
    {
        cb = 0;
        m_perr->Throw(ERRnoMemory);
    }


    if (nullptr == (pid = (IdentPtr)m_noReleaseAllocator.Alloc(cb)))
        m_perr->Throw(ERRnoMemory);

    /* Insert the identifier into the hash list */
    *ppid = pid;

    // Increment the number of entries in the table.
    m_luCount++;

    /* Fill in the identifier record */
    pid->m_pidNext = nullptr;
    pid->m_tk = tkLim;
    pid->m_grfid = fidNil;
    pid->m_luHash = luHash;
    pid->m_cch = cch;
    pid->m_pidRefStack = nullptr;
    pid->m_propertyId = Js::Constants::NoProperty;
    pid->assignmentState = NotAssigned;

    HashTbl::CopyString(pid->m_sz, prgch, cch);

    return pid;
}

template <typename CharType>
IdentPtr HashTbl::FindExistingPid(
    CharType const * prgch,
    long cch,
    ulong luHash,
    IdentPtr **pppInsert,
    long *pBucketCount
#if PROFILE_DICTIONARY
    , int& depth
#endif
    )
{
    long bucketCount;
    IdentPtr pid;
    IdentPtr *ppid = &m_prgpidName[luHash & m_luMask];

    /* Search the hash table for an existing match */
    ppid = &m_prgpidName[luHash & m_luMask];

    for (bucketCount = 0; nullptr != (pid = *ppid); ppid = &pid->m_pidNext, bucketCount++)
    {
        if (pid->m_luHash == luHash && (int)pid->m_cch == cch &&
            HashTbl::CharsAreEqual(pid->m_sz, prgch, cch))
        {
            return pid;
        }
#if PROFILE_DICTIONARY
        ++depth;
#endif
    }

    if (pBucketCount)
    {
        *pBucketCount = bucketCount;
    }
    if (pppInsert)
    {
        *pppInsert = ppid;
    }

    return nullptr;
}

template IdentPtr HashTbl::FindExistingPid<utf8char_t>(
    utf8char_t const * prgch, long cch, ulong luHash, IdentPtr **pppInsert, long *pBucketCount
#if PROFILE_DICTIONARY
    , int& depth
#endif
    );
template IdentPtr HashTbl::FindExistingPid<char>(
    char const * prgch, long cch, ulong luHash, IdentPtr **pppInsert, long *pBucketCount
#if PROFILE_DICTIONARY
    , int& depth
#endif
    );
template IdentPtr HashTbl::FindExistingPid<wchar_t>(
    wchar_t const * prgch, long cch, ulong luHash, IdentPtr **pppInsert, long *pBucketCount
#if PROFILE_DICTIONARY
    , int& depth
#endif
    );

bool HashTbl::Contains(_In_reads_(cch) LPCOLESTR prgch, long cch)
{
    ulong luHash = CaseSensitiveComputeHashCch(prgch, cch);

    for (auto pid = m_prgpidName[luHash & m_luMask]; pid; pid = pid->m_pidNext)
    {
        if (pid->m_luHash == luHash && (int)pid->m_cch == cch &&
            HashTbl::CharsAreEqual(pid->m_sz, prgch, cch))
        {
            return true;
        }
    }

    return false;
}

#include "hashfunc.cpp"




#pragma warning(push)
#pragma warning(disable:4740)  // flow in or out of inline asm code suppresses global optimization
// Decide if token is keyword by string matching -
// This method is used during colorizing when scanner isn't interested in storing the actual id and does not care about conversion of escape sequences
tokens HashTbl::TkFromNameLenColor(_In_reads_(cch) LPCOLESTR prgch, ulong cch)
{
    ulong luHash = CaseSensitiveComputeHashCch(prgch, cch);

    // look for a keyword
#include "kwds_sw.h"

    #define KEYWORD(tk,f,prec2,nop2,prec1,nop1,name) \
        LEqual_##name: \
            if (cch == g_ssym_##name.cch && \
                    0 == memcmp(g_ssym_##name.sz, prgch, cch * sizeof(OLECHAR))) \
            { \
                return tk; \
            } \
            goto LDefault;
#include "keywords.h"

LDefault:
    return tkID;
}
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable:4740)  // flow in or out of inline asm code suppresses global optimization

// Decide if token is keyword by string matching -
// This method is used during colorizing when scanner isn't interested in storing the actual id and does not care about conversion of escape sequences
tokens HashTbl::TkFromNameLen(_In_reads_(cch) LPCOLESTR prgch, ulong cch, bool isStrictMode)
{
    ulong luHash = CaseSensitiveComputeHashCch(prgch, cch);

    // look for a keyword
#include "kwds_sw.h"

    #define KEYWORD(tk,f,prec2,nop2,prec1,nop1,name) \
        LEqual_##name: \
            if (cch == g_ssym_##name.cch && \
                    0 == memcmp(g_ssym_##name.sz, prgch, cch * sizeof(OLECHAR))) \
            { \
                return ((f & fidKwdRsvd) || (isStrictMode && (f & fidKwdFutRsvd))) ? tk : tkID; \
            } \
            goto LDefault;
#include "keywords.h"

LDefault:
    return tkID;
}

#pragma warning(pop)
