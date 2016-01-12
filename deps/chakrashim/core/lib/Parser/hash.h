//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct StaticSym;

/***************************************************************************
Hashing functions. Definitions in core\hashfunc.cpp.
***************************************************************************/
ULONG CaseSensitiveComputeHashCch(LPCOLESTR prgch, long cch);
ULONG CaseSensitiveComputeHashCch(LPCUTF8 prgch, long cch);
ULONG CaseInsensitiveComputeHash(LPCOLESTR posz);

enum
{
    fidNil        =     0x0000,
    fidKwdRsvd    = 0x0001,     // the keyword is a reserved word
    fidKwdFutRsvd = 0x0002,     // a future reserved word, but only in strict mode

    // Flags to identify tracked aliases of "eval"
    fidEval       =    0x0008,
    // Flags to identify tracked aliases of "let"
    fidLetOrConst   = 0x0010,     // ID has previously been used in a block-scoped declaration

    // This flag is used by the Parser CountDcls and FillDcls methods.
    // CountDcls sets the bit as it walks through the var decls so that
    // it can skip duplicates. FillDcls clears the bit as it walks through
    // again to skip duplicates.
    fidGlobalDcl  =    0x2000,

    fidUsed       =    0x4000  // name referenced by source code

};

struct BlockIdsStack
{
    int id;
    BlockIdsStack *prev;
};

class Span
{
    charcount_t m_ichMin;
    charcount_t m_ichLim;

public:
    Span(): m_ichMin((charcount_t)-1), m_ichLim((charcount_t)-1) { }
    Span(charcount_t ichMin, charcount_t ichLim): m_ichMin(ichMin), m_ichLim(ichLim) { }

    charcount_t GetIchMin() { return m_ichMin; }
    charcount_t GetIchLim() { Assert(m_ichMin != (charcount_t)-1); return m_ichLim; }
    void Set(charcount_t ichMin, charcount_t ichLim)
    {
        m_ichMin = ichMin;
        m_ichLim = ichLim;
    }

    operator bool() { return m_ichMin != -1; }
};

struct PidRefStack
{
    PidRefStack() : isAsg(false), isDynamic(false), id(0), span(), sym(nullptr), prev(nullptr) {}
    PidRefStack(int id) : isAsg(false), isDynamic(false), id(id), span(), sym(nullptr), prev(nullptr) {}

    charcount_t GetIchMin()   { return span.GetIchMin(); }
    charcount_t GetIchLim()   { return span.GetIchLim(); }
    int GetScopeId() const    { return id; }
    Symbol *GetSym() const    { return sym; }
    void SetSym(Symbol *sym)  { this->sym = sym; }
    bool IsAssignment() const { return isAsg; }
    bool IsDynamicBinding() const { return isDynamic; }
    void SetDynamicBinding()  { isDynamic = true; }

    void TrackAssignment(charcount_t ichMin, charcount_t ichLim);

    Symbol **GetSymRef()
    {
        return &sym;
    }

    bool           isAsg;
    bool           isDynamic;
    int            id;
    Span           span;
    Symbol        *sym;
    PidRefStack   *prev;
};

enum AssignmentState : byte {
    NotAssigned,
    AssignedOnce,
    AssignedMultipleTimes
};

struct Ident
{
    friend class HashTbl;

private:
    Ident * m_pidNext;   // next identifier in this hash bucket
    PidRefStack *m_pidRefStack;
    ushort m_tk;         // token# if identifier is a keyword
    ushort m_grfid;      // see fidXXX above
    ulong m_luHash;      // hash value

    ulong m_cch;                   // length of the identifier spelling
    Js::PropertyId m_propertyId;

    AssignmentState assignmentState;

    OLECHAR m_sz[]; // the spelling follows (null terminated)

    void SetTk(tokens tk, ushort grfid);
public:
    LPCOLESTR Psz(void)
    { return m_sz; }
    ulong Cch(void)
    { return m_cch; }
    tokens Tk(bool isStrictMode);
    ulong Hash(void)
    { return m_luHash; }

    PidRefStack *GetTopRef() const
    {
        return m_pidRefStack;
    }

    void SetTopRef(PidRefStack *ref)
    {
        m_pidRefStack = ref;
    }

    void PromoteAssignmentState()
    {
        if (assignmentState == NotAssigned)
        {
            assignmentState = AssignedOnce;
        }
        else if (assignmentState == AssignedOnce)
        {
            assignmentState = AssignedMultipleTimes;
        }
    }

    bool IsSingleAssignment()
    {
        return assignmentState == AssignedOnce;
    }

    PidRefStack *GetPidRefForScopeId(int scopeId)
    {
        PidRefStack *ref;
        for (ref = m_pidRefStack; ref; ref = ref->prev)
        {
            int refId = ref->GetScopeId();
            if (refId == scopeId)
            {
                return ref;
            }
            if (refId < scopeId)
            {
                break;
            }
        }
        return nullptr;
    }

    charcount_t GetTopIchMin() const
    {
        Assert(m_pidRefStack);
        return m_pidRefStack->GetIchMin();
    }

    charcount_t GetTopIchLim() const
    {
        Assert(m_pidRefStack);
        return m_pidRefStack->GetIchLim();
    }

    void PushPidRef(int blockId, PidRefStack *newRef)
    {
        AssertMsg(blockId >= 0, "Block Id's should be greater than 0");
        newRef->id = blockId;
        newRef->prev = m_pidRefStack;
        m_pidRefStack = newRef;
    }

    PidRefStack * RemovePrevPidRef(PidRefStack *ref)
    {
        PidRefStack *prevRef;
        if (ref == nullptr)
        {
            prevRef = m_pidRefStack;
            Assert(prevRef);
            m_pidRefStack = prevRef->prev;
        }
        else
        {
            prevRef = ref->prev;
            Assert(prevRef);
            ref->prev = prevRef->prev;
        }
        return prevRef;
    }

    PidRefStack * FindOrAddPidRef(ArenaAllocator *alloc, int scopeId, int maxScopeId = -1)
    {
        // If we were supplied with a maxScopeId, then we potentially need to look one more
        // scope level out. This can happen if we have a declaration in function scope shadowing
        // a parameter scope declaration. In this case we'd need to look beyond the body scope (scopeId)
        // to the outer parameterScope (maxScopeId).
        if (maxScopeId == -1)
        {
            maxScopeId = scopeId;
        }

        // If the stack is empty, or we are pushing to the innermost scope already,
        // we can go ahead and push a new PidRef on the stack.
        if (m_pidRefStack == nullptr || m_pidRefStack->id < maxScopeId)
        {
            PidRefStack *newRef = Anew(alloc, PidRefStack, scopeId);
            if (newRef == nullptr)
            {
                return nullptr;
            }
            newRef->prev = m_pidRefStack;
            m_pidRefStack = newRef;
            return newRef;
        }

        // Search for the corresponding PidRef, or the position to insert the new PidRef.
        PidRefStack *ref = m_pidRefStack;
        while (1)
        {
            // We may already have a ref for this scopeId.
            if (ref->id == scopeId)
            {
                return ref;
            }

            if (ref->id == maxScopeId
                // If we match the different maxScopeId, then this match is sufficent if it is a decl.
                // This is because the parameter scope decl would have been created before this point.
                && ref->sym != nullptr)
            {
                return ref;
            }

            if (ref->prev == nullptr || ref->prev->id < maxScopeId)
            {
                // No existing PidRef for this scopeId, so create and insert one at this position.
                PidRefStack *newRef = Anew(alloc, PidRefStack, scopeId);
                if (newRef == nullptr)
                {
                    return nullptr;
                }

                if (ref->id < scopeId)
                {
                    // Without parameter scope, we would have just pushed the ref instead of inserting.
                    // We effectively had a false positive match (a parameter scope ref with no sym)
                    // so we need to push the Pid rather than inserting.
                    newRef->prev = m_pidRefStack;
                    m_pidRefStack = newRef;
                }
                else
                {
                    newRef->prev = ref->prev;
                    ref->prev = newRef;
                }
                return newRef;
            }

            Assert(ref->prev->id <= ref->id);
            ref = ref->prev;
        }
    }

    Js::PropertyId GetPropertyId() const { return m_propertyId; }
    void SetPropertyId(Js::PropertyId id) { m_propertyId = id; }

    void SetIsEval() { m_grfid |= fidEval; }
    BOOL GetIsEval() const { return m_grfid & fidEval; }

    void SetIsLetOrConst() { m_grfid |= fidLetOrConst; }
    BOOL GetIsLetOrConst() const { return m_grfid & fidLetOrConst; }
};


/*****************************************************************************/

class HashTbl
{
public:
    static HashTbl * Create(uint cidHash, ErrHandler * perr);

    void Release(void)
    {
        delete this;
    }


    BOOL TokIsBinop(tokens tk, int *popl, OpCode *pnop)
    {
        const KWD *pkwd = KwdOfTok(tk);

        if (nullptr == pkwd)
            return FALSE;
        *popl = pkwd->prec2;
        *pnop = pkwd->nop2;
        return TRUE;
    }

    BOOL TokIsUnop(tokens tk, int *popl, OpCode *pnop)
    {
        const KWD *pkwd = KwdOfTok(tk);

        if (nullptr == pkwd)
            return FALSE;
        *popl = pkwd->prec1;
        *pnop = pkwd->nop1;
        return TRUE;
    }

    IdentPtr PidFromTk(tokens tk);
    IdentPtr PidHashName(LPCOLESTR psz)
    {
        size_t csz = wcslen(psz);
        Assert(csz <= ULONG_MAX);
        return PidHashNameLen(psz, static_cast<ulong>(csz));
    }

    template <typename CharType>
    IdentPtr PidHashNameLen(CharType const * psz, ulong cch);
    template <typename CharType>
    IdentPtr PidHashNameLenWithHash(_In_reads_(cch) CharType const * psz, long cch, ulong luHash);


    template <typename CharType>
    __inline IdentPtr FindExistingPid(
        CharType const * prgch,
        long cch,
        ulong luHash,
        IdentPtr **pppInsert,
        long *pBucketCount
#if PROFILE_DICTIONARY
        , int& depth
#endif
        );

    tokens TkFromNameLen(_In_reads_(cch) LPCOLESTR prgch, ulong cch, bool isStrictMode);
    tokens TkFromNameLenColor(_In_reads_(cch) LPCOLESTR prgch, ulong cch);
    NoReleaseAllocator* GetAllocator() {return &m_noReleaseAllocator;}

    bool Contains(_In_reads_(cch) LPCOLESTR prgch, long cch);
private:

    NoReleaseAllocator m_noReleaseAllocator;            // to allocate identifiers
    Ident ** m_prgpidName;        // hash table for names

    ulong m_luMask;                // hash mask
    ulong m_luCount;              // count of the number of entires in the hash table
    ErrHandler * m_perr;        // error handler to use
    IdentPtr m_rpid[tkLimKwd];

    HashTbl(ErrHandler * perr)
    {
        m_prgpidName = nullptr;
        m_perr = perr;
        memset(&m_rpid, 0, sizeof(m_rpid));
    }
    ~HashTbl(void) {}

    // Called to grow the number of buckets in the table to reduce the table density.
    void Grow();

    // Automatically grow the table if a bucket's length grows beyond BucketLengthLimit and the table is densely populated.
    static const uint BucketLengthLimit = 5;

    // When growing the bucket size we'll grow by GrowFactor. GrowFactor MUST be a power of 2.
    static const uint GrowFactor = 4;

#if DEBUG
    uint CountAndVerifyItems(IdentPtr *buckets, uint bucketCount, uint mask);
#endif

    static bool CharsAreEqual(__in_z LPCOLESTR psz1, __in_ecount(cch2) LPCOLESTR psz2, long cch2)
    {
        return memcmp(psz1, psz2, cch2 * sizeof(OLECHAR)) == 0;
    }
    static bool CharsAreEqual(__in_z LPCOLESTR psz1, LPCUTF8 psz2, long cch2)
    {
        return utf8::CharsAreEqual(psz1, psz2, cch2, utf8::doAllowThreeByteSurrogates);
    }
    static bool CharsAreEqual(__in_z LPCOLESTR psz1, __in_ecount(cch2) char const * psz2, long cch2)
    {
        while (cch2-- > 0)
        {
            if (*psz1++ != *psz2++)
                return false;
        }
        return true;
    }
    static void CopyString(__in_ecount(cch + 1) LPOLESTR psz1, __in_ecount(cch) LPCOLESTR psz2, long cch)
    {
        js_memcpy_s(psz1, cch * sizeof(OLECHAR), psz2, cch * sizeof(OLECHAR));
        psz1[cch] = 0;
    }
    static void CopyString(__in_ecount(cch + 1) LPOLESTR psz1, LPCUTF8 psz2, long cch)
    {
        utf8::DecodeIntoAndNullTerminate(psz1, psz2, cch);
    }
    static void CopyString(__in_ecount(cch + 1) LPOLESTR psz1, __in_ecount(cch) char const * psz2, long cch)
    {
        while (cch-- > 0)
            *(psz1++) = *psz2++;
        *psz1 = 0;
    }

    // note: on failure this may throw or return FALSE, depending on
    // where the failure occurred.
    BOOL Init(uint cidHash);

    /*************************************************************************/
    /* The following members are related to the keyword descriptor tables    */
    /*************************************************************************/
    struct KWD
    {
        OpCode nop2;
        byte prec2;
        OpCode nop1;
        byte prec1;
    };
    struct ReservedWordInfo
    {
        StaticSym const * sym;
        ushort grfid;
    };
    static const ReservedWordInfo s_reservedWordInfo[tkID];
    static const KWD g_mptkkwd[tkLimKwd];
    static const KWD * KwdOfTok(tokens tk)
    { return (unsigned int)tk < tkLimKwd ? g_mptkkwd + tk : nullptr; }

#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
};

