//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class BasicBlock;
class FlowEdge;
class Loop;
class Region;
class Func;

class AddPropertyCacheBucket
{
private:
    Js::Type* initialType;
    Js::Type* finalType;
public:
    AddPropertyCacheBucket() : initialType(nullptr), finalType(nullptr)
#if DBG
        , deadStoreUnavailableInitialType(nullptr), deadStoreUnavailableFinalType(nullptr)
#endif
    {
    }

    AddPropertyCacheBucket(const AddPropertyCacheBucket& bucket) :
        initialType(bucket.initialType), finalType(bucket.finalType)
#if DBG
        , deadStoreUnavailableInitialType(bucket.deadStoreUnavailableInitialType)
        , deadStoreUnavailableFinalType(bucket.deadStoreUnavailableFinalType)
#endif
    {
    }

    bool operator!=(const AddPropertyCacheBucket& bucket) const
    {
        return this->initialType != bucket.initialType || this->finalType != bucket.finalType;
    }

    bool operator==(const AddPropertyCacheBucket& bucket) const
    {
        return this->initialType == bucket.initialType && this->finalType == bucket.finalType;
    }

    void Copy(AddPropertyCacheBucket *pNew) const
    {
        pNew->initialType = this->initialType;
        pNew->finalType = this->finalType;
#if DBG
        pNew->deadStoreUnavailableInitialType = this->deadStoreUnavailableInitialType;
        pNew->deadStoreUnavailableFinalType = this->deadStoreUnavailableFinalType;
#endif
    }

    Js::Type *GetInitialType() const { return this->initialType; }
    Js::Type *GetFinalType() const { return this->finalType; }
    void SetInitialType(Js::Type *type) { this->initialType = type; }
    void SetFinalType(Js::Type *type)  { this->finalType = type; }

#if DBG_DUMP
    void Dump() const;
#endif

#ifdef DBG
    Js::Type * deadStoreUnavailableInitialType;
    Js::Type * deadStoreUnavailableFinalType;
#endif
};

class ObjTypeGuardBucket
{
private:
    BVSparse<JitArenaAllocator>* guardedPropertyOps;
    bool needsMonoCheck;

public:
    ObjTypeGuardBucket() : guardedPropertyOps(nullptr), needsMonoCheck(false) {}

    ObjTypeGuardBucket(BVSparse<JitArenaAllocator>* guardedPropertyOps) : needsMonoCheck(false)
    {
        this->guardedPropertyOps = (guardedPropertyOps != nullptr ? guardedPropertyOps->CopyNew() : nullptr);
    }

    void Copy(ObjTypeGuardBucket *pNew) const
    {
        pNew->guardedPropertyOps = this->guardedPropertyOps ? this->guardedPropertyOps->CopyNew() : nullptr;
        pNew->needsMonoCheck = this->needsMonoCheck;
    }

    BVSparse<JitArenaAllocator> *GetGuardedPropertyOps() const  { return this->guardedPropertyOps; }
    void SetGuardedPropertyOps(BVSparse<JitArenaAllocator> *guardedPropertyOps) { this->guardedPropertyOps = guardedPropertyOps; }
    void AddToGuardedPropertyOps(uint propertyOpId) { Assert(this->guardedPropertyOps != nullptr); this->guardedPropertyOps->Set(propertyOpId); }

    bool NeedsMonoCheck() const { return this->needsMonoCheck; }
    void SetNeedsMonoCheck(bool value) { this->needsMonoCheck = value; }

#if DBG_DUMP
    void Dump() const;
#endif
};

class ObjWriteGuardBucket
{
private:
    BVSparse<JitArenaAllocator>* writeGuards;

public:
    ObjWriteGuardBucket() : writeGuards(nullptr) {}

    ObjWriteGuardBucket(BVSparse<JitArenaAllocator>* writeGuards) { this->writeGuards = (writeGuards != nullptr ? writeGuards->CopyNew() : nullptr); }

    void Copy(ObjWriteGuardBucket *pNew) const
    {
        pNew->writeGuards = this->writeGuards ? this->writeGuards->CopyNew() : nullptr;
    }

    BVSparse<JitArenaAllocator> *GetWriteGuards() const  { return this->writeGuards; }
    void SetWriteGuards(BVSparse<JitArenaAllocator> *writeGuards) { this->writeGuards = writeGuards; }
    void AddToWriteGuards(uint writeGuardId) { Assert(this->writeGuards != nullptr); this->writeGuards->Set(writeGuardId); }

#if DBG_DUMP
    void Dump() const;
#endif
};

class FlowGraph
{
    friend Loop;

public:
    static FlowGraph * New(Func *func, JitArenaAllocator *alloc);

    FlowGraph(Func *func, JitArenaAllocator *fgAlloc) :
        func(func),
        alloc(fgAlloc),
        blockList(nullptr),
        blockCount(0),
        tailBlock(nullptr),
        loopList(nullptr),
        catchLabelStack(nullptr),
        hasBackwardPassInfo(false),
        hasLoop(false),
        implicitCallFlags(Js::ImplicitCall_HasNoInfo)
    {
    }

    void Build(void);
    void Destroy(void);

    void         RunPeeps();
    BasicBlock * AddBlock(IR::Instr * firstInstr, IR::Instr * lastInstr, BasicBlock * nextBlock);
    FlowEdge *   AddEdge(BasicBlock * predBlock, BasicBlock * succBlock);
    BasicBlock * InsertCompensationCodeForBlockMove(FlowEdge * edge, // Edge where compensation code needs to be inserted
                                                    bool insertCompensationBlockToLoopList = false,
                                                    bool sinkBlockLoop  = false // Loop to which compensation block belongs
                                                    );
    BasicBlock * InsertAirlockBlock(FlowEdge * edge);
    void         InsertCompBlockToLoopList(Loop *loop, BasicBlock* compBlock, BasicBlock* targetBlock, bool postTarget);
    void         RemoveUnreachableBlocks();
    bool         RemoveUnreachableBlock(BasicBlock *block, GlobOpt * globOpt = nullptr);
    IR::Instr *  RemoveInstr(IR::Instr *instr, GlobOpt * globOpt);
    void         RemoveBlock(BasicBlock *block, GlobOpt * globOpt = nullptr, bool tailDuping = false);
    BasicBlock * SetBlockTargetAndLoopFlag(IR::LabelInstr * labelInstr);
    Func*        GetFunc() { return func;};
    static void  SafeRemoveInstr(IR::Instr *instr);
    void         SortLoopLists();
    FlowEdge *   FindEdge(BasicBlock *predBlock, BasicBlock *succBlock);

#if DBG_DUMP
    void         Dump();
    void         Dump(bool verbose, const wchar_t *form);
#endif

    JitArenaAllocator *       alloc;
    BasicBlock *              blockList;
    BasicBlock *              tailBlock;
    Loop *                    loopList;
    SList<IR::LabelInstr*> *  catchLabelStack;
    bool                      hasBackwardPassInfo;
    bool                      hasLoop;
    Js::ImplicitCallFlags     implicitCallFlags;
private:
    void        FindLoops(void);
    bool        CanonicalizeLoops(void);
    void        BuildLoop(BasicBlock *headBlock, BasicBlock *tailBlock, Loop *parentLoop = nullptr);
    void        WalkLoopBlocks(BasicBlock *block, Loop *loop, JitArenaAllocator *tempAlloc);
    void        AddBlockToLoop(BasicBlock *block, Loop *loop);
    void        UpdateRegionForBlock(BasicBlock *block, Region **blockToRegion);
    Region *    PropagateRegionFromPred(BasicBlock *block, BasicBlock *predBlock, Region *predRegion, IR::Instr * &tryInstr);
    IR::Instr * PeepCm(IR::Instr *instr);
    IR::Instr * PeepTypedCm(IR::Instr *instr);
    void        MoveBlocksBefore(BasicBlock *blockStart, BasicBlock *blockEnd, BasicBlock *insertBlock);
    bool        UnsignedCmpPeep(IR::Instr *cmpInstr);
    bool        IsUnsignedOpnd(IR::Opnd *src, IR::Opnd **pShrSrc1);

#if DBG
    void        VerifyLoopGraph();
#endif

private:
    void        InsertInlineeOnFLowEdge(IR::BranchInstr *instrBr, IR::Instr *inlineeEndInstr, IR::Instr *instrBytecode, Func* origBrFunc, uint32 origByteCodeOffset, uint32 origBranchSrcSymId);

private:
    Func *                 func;
    unsigned int           blockCount;

};

class BasicBlock
{
    friend class FlowGraph;
    friend class Loop;

public:
    static BasicBlock * New(FlowGraph * graph);

    void AddPred(FlowEdge * edge, FlowGraph * graph);
    void AddSucc(FlowEdge * edge, FlowGraph * graph);
    void RemovePred(BasicBlock *block, FlowGraph * graph);
    void RemoveSucc(BasicBlock *block, FlowGraph * graph);
    void RemoveDeadPred(BasicBlock *block, FlowGraph * graph);
    void RemoveDeadSucc(BasicBlock *block, FlowGraph * graph);
    void UnlinkPred(BasicBlock *block);
    void UnlinkSucc(BasicBlock *block);

    void UnlinkInstr(IR::Instr * Instr);
    void RemoveInstr(IR::Instr * instr);
    void InsertInstrBefore(IR::Instr *newInstr, IR::Instr *beforeThisInstr);
    void InsertInstrAfter(IR::Instr *newInstr, IR::Instr *afterThisInstr);
    void InsertAfter(IR::Instr * newInstr);
    void InvertBranch(IR::BranchInstr *branch);

    IR::Instr * GetFirstInstr(void) const
    {
        return firstInstr;
    }

    void SetFirstInstr(IR::Instr * instr)
    {
        firstInstr = instr;
    }

    IR::Instr * GetLastInstr(void)
    {
        BasicBlock *blNext = this->next;
        if (blNext)
        {
            return blNext->firstInstr->m_prev;
        }
        else
        {
            return this->func->m_exitInstr;
        }
    }

    void SetLastInstr(IR::Instr * instr)
    {
        // Intentionally empty
    }

    SListBaseCounted<FlowEdge *> * GetPredList(void)
    {
        return &predList;
    }

    SListBaseCounted<FlowEdge *> * GetSuccList(void)
    {
        return &succList;
    }

    SListBaseCounted<FlowEdge *> * GetDeadPredList(void)
    {
        return &deadPredList;
    }

    SListBaseCounted<FlowEdge *> * GetDeadSuccList(void)
    {
        return &deadSuccList;
    }

    unsigned int GetBlockNum(void) const
    {
        return number;
    }

    void SetBlockNum(unsigned int num)
    {
        number = num;
    }

    BasicBlock * GetPrev()
    {
        BasicBlock *block = this;

        do {
            block = block->prev;
        } while (block->isDeleted);

        return block;
    }

    BasicBlock * GetNext()
    {
        BasicBlock *block = this;

        do {
            block = block->next;
        } while (block && block->isDeleted);

        return block;
    }
    uint IncrementDataUseCount()
    {
         return ++this->dataUseCount;
    }
    uint DecrementDataUseCount()
    {
        Assert(this->dataUseCount != 0);
         return --this->dataUseCount;
    }
    uint GetDataUseCount()
    {
         return this->dataUseCount;
    }
    void SetDataUseCount(uint count)
    {
         this->dataUseCount = count;
    }

    bool IsLandingPad();

#if DBG_DUMP

    void DumpHeader(bool insertCR = true);
    void Dump();

#endif

public:
    BasicBlock *         next;
    BasicBlock *         prev;
    Loop *               loop;
    uint8                isDeleted:1;
    uint8                isDead:1;
    uint8                isLoopHeader:1;
    uint8                hasCall:1;
    uint8                isVisited:1;
    uint8                isAirLockCompensationBlock:1;

#ifdef DBG
    uint8                isBreakBlock:1;
    uint8                isAirLockBlock:1;
    uint8                isBreakCompensationBlockAtSink:1;
    uint8                isBreakCompensationBlockAtSource:1;
#endif

    // Deadstore data
    BVSparse<JitArenaAllocator> *              upwardExposedUses;
    BVSparse<JitArenaAllocator> *              upwardExposedFields;
    BVSparse<JitArenaAllocator> *              typesNeedingKnownObjectLayout;
    BVSparse<JitArenaAllocator> *              fieldHoistCandidates;
    BVSparse<JitArenaAllocator> *              slotDeadStoreCandidates;
    TempNumberTracker *                     tempNumberTracker;
    TempObjectTracker *                     tempObjectTracker;
#if DBG
    TempObjectVerifyTracker *               tempObjectVerifyTracker;
#endif
    HashTable<AddPropertyCacheBucket> *     stackSymToFinalType;
    HashTable<ObjTypeGuardBucket> *         stackSymToGuardedProperties; // Dead store pass only
    HashTable<ObjWriteGuardBucket> *        stackSymToWriteGuardsMap; // Backward pass only
    BVSparse<JitArenaAllocator> *           noImplicitCallUses;
    BVSparse<JitArenaAllocator> *           noImplicitCallNoMissingValuesUses;
    BVSparse<JitArenaAllocator> *           noImplicitCallNativeArrayUses;
    BVSparse<JitArenaAllocator> *           noImplicitCallJsArrayHeadSegmentSymUses;
    BVSparse<JitArenaAllocator> *           noImplicitCallArrayLengthSymUses;
    BVSparse<JitArenaAllocator> *           cloneStrCandidates;
    Loop * backwardPassCurrentLoop;

    // Global optimizer data
    GlobOptBlockData                        globOptData;

    // Bailout data
    BVSparse<JitArenaAllocator> *           byteCodeUpwardExposedUsed;
#if DBG
    StackSym **                             byteCodeRestoreSyms;
#endif

    IntOverflowDoesNotMatterRange *         intOverflowDoesNotMatterRange;

private:
    BasicBlock(JitArenaAllocator * alloc, Func *func) :
        next(nullptr),
        prev(nullptr),
        firstInstr(nullptr),
        number(k_InvalidNum),
        loop(nullptr),
        isDeleted(false),
        isDead(false),
        isLoopHeader(false),
        hasCall(false),
        upwardExposedUses(nullptr),
        upwardExposedFields(nullptr),
        typesNeedingKnownObjectLayout(nullptr),
        slotDeadStoreCandidates(nullptr),
        tempNumberTracker(nullptr),
        tempObjectTracker(nullptr),
#if DBG
        tempObjectVerifyTracker(nullptr),
#endif
        stackSymToFinalType(nullptr),
        stackSymToGuardedProperties(nullptr),
        stackSymToWriteGuardsMap(nullptr),
        noImplicitCallUses(nullptr),
        noImplicitCallNoMissingValuesUses(nullptr),
        noImplicitCallNativeArrayUses(nullptr),
        noImplicitCallJsArrayHeadSegmentSymUses(nullptr),
        noImplicitCallArrayLengthSymUses(nullptr),
        cloneStrCandidates(nullptr),
        byteCodeUpwardExposedUsed(nullptr),
        isAirLockCompensationBlock(false),
#if DBG
        byteCodeRestoreSyms(nullptr),
        isBreakBlock(false),
        isAirLockBlock(false),
        isBreakCompensationBlockAtSource(false),
        isBreakCompensationBlockAtSink(false),
#endif
        fieldHoistCandidates(nullptr),
        dataUseCount(0),
        intOverflowDoesNotMatterRange(nullptr),
        func(func),
        globOptData(func)
    {
    }

    void RemovePred(BasicBlock *block, FlowGraph * graph, bool doCleanSucc, bool moveToDead = false);
    void RemoveSucc(BasicBlock *block, FlowGraph * graph, bool doCleanPred, bool moveToDead = false);
    void UnlinkPred(BasicBlock *block, bool doCleanSucc);
    void UnlinkSucc(BasicBlock *block, bool doCleanPred);

#if DBG_DUMP
    bool Contains(IR::Instr * instr);
#endif
private:
    IR::Instr *          firstInstr;
    SListBaseCounted<FlowEdge *> predList;
    SListBaseCounted<FlowEdge *> succList;
    SListBaseCounted<FlowEdge *> deadPredList;
    SListBaseCounted<FlowEdge *> deadSuccList;
    Func *               func;
    unsigned int         number;
    uint dataUseCount;

    static const unsigned int k_InvalidNum = (unsigned)-1;
};

class FlowEdge
{
public:
    static FlowEdge * New(FlowGraph * graph);

    FlowEdge() :
        predBlock(nullptr),
        succBlock(nullptr),
        pathDependentInfo(nullptr)
    {
    }

    BasicBlock * GetPred(void) const
    {
        return predBlock;
    }

    void SetPred(BasicBlock * block)
    {
        predBlock = block;
    }

    BasicBlock * GetSucc(void) const
    {
        return succBlock;
    }

    void SetSucc(BasicBlock * block)
    {
        succBlock = block;
    }

    PathDependentInfo * GetPathDependentInfo() const
    {
        return pathDependentInfo;
    }

    void SetPathDependentInfo(const PathDependentInfo &info, JitArenaAllocator *const alloc)
    {
        Assert(info.HasInfo());

        if (!pathDependentInfo)
        {
            pathDependentInfo = JitAnew(alloc, PathDependentInfo, info);
        }
        else
        {
            *pathDependentInfo = info;
        }
    }

    void ClearPathDependentInfo(JitArenaAllocator * alloc)
    {
        JitAdelete(alloc, pathDependentInfo);
        pathDependentInfo = nullptr;
    }

private:
    BasicBlock *         predBlock;
    BasicBlock *         succBlock;

    // Only valid during globopt
    PathDependentInfo * pathDependentInfo;
};

class Loop
{
    friend FlowGraph;
private:
    typedef JsUtil::BaseDictionary<SymID, StackSym *, JitArenaAllocator, PowerOf2SizePolicy> FieldHoistSymMap;
    typedef JsUtil::BaseDictionary<PropertySym *, Value *, JitArenaAllocator> InitialValueFieldMap;

    Js::ImplicitCallFlags implicitCallFlags;
    Js::LoopFlags loopFlags;
    BasicBlock *        headBlock;
public:
    Func *              topFunc;
    uint32              loopNumber;
    SList<BasicBlock *> blockList;
    Loop *              next;
    Loop *              parent;
    BasicBlock *        landingPad;
    IR::LabelInstr *    loopTopLabel;
    BVSparse<JitArenaAllocator> *varSymsOnEntry;
    BVSparse<JitArenaAllocator> *int32SymsOnEntry;
    BVSparse<JitArenaAllocator> *lossyInt32SymsOnEntry; // see GlobOptData::liveLossyInt32Syms
    BVSparse<JitArenaAllocator> *float64SymsOnEntry;
    BVSparse<JitArenaAllocator> *liveFieldsOnEntry;
    // SIMD_JS
    // live syms upon entering loop header (from pred merge + forced syms + used before defs in loop)
    BVSparse<JitArenaAllocator> *simd128F4SymsOnEntry;
    BVSparse<JitArenaAllocator> *simd128I4SymsOnEntry;

    BVSparse<JitArenaAllocator> *symsUsedBeforeDefined;                // stack syms that are live in the landing pad, and used before they are defined in the loop
    BVSparse<JitArenaAllocator> *likelyIntSymsUsedBeforeDefined;       // stack syms that are live in the landing pad with a likely-int value, and used before they are defined in the loop
    BVSparse<JitArenaAllocator> *likelyNumberSymsUsedBeforeDefined;    // stack syms that are live in the landing pad with a likely-number value, and used before they are defined in the loop
    // SIMD_JS
    BVSparse<JitArenaAllocator> *likelySimd128F4SymsUsedBeforeDefined;    // stack syms that are live in the landing pad with a likely-Simd128F4 value, and used before they are defined in the loop
    BVSparse<JitArenaAllocator> *likelySimd128I4SymsUsedBeforeDefined;    // stack syms that are live in the landing pad with a likely-Simd128I4 value, and used before they are defined in the loop

    BVSparse<JitArenaAllocator> *forceFloat64SymsOnEntry;
    // SIMD_JS
    // syms need to be forced to certain type due to hoisting
    BVSparse<JitArenaAllocator> *forceSimd128F4SymsOnEntry;
    BVSparse<JitArenaAllocator> *forceSimd128I4SymsOnEntry;

    BVSparse<JitArenaAllocator> *symsDefInLoop;
    BailOutInfo *       bailOutInfo;
    IR::BailOutInstr *  toPrimitiveSideEffectCheck;
    BVSparse<JitArenaAllocator> * fieldHoistCandidates;
    BVSparse<JitArenaAllocator> * liveInFieldHoistCandidates;
    BVSparse<JitArenaAllocator> * fieldHoistCandidateTypes;
    SListBase<IR::Instr *> prepassFieldHoistInstrCandidates;
    FieldHoistSymMap fieldHoistSymMap;
    IR::Instr *         endDisableImplicitCall;
    BVSparse<JitArenaAllocator> * hoistedFields;
    BVSparse<JitArenaAllocator> * hoistedFieldCopySyms;
    BVSparse<JitArenaAllocator> * liveOutFields;
    ValueNumber         firstValueNumberInLoop;
    JsArrayKills        jsArrayKills;
    BVSparse<JitArenaAllocator> *fieldKilled;
    BVSparse<JitArenaAllocator> *fieldPRESymStore;
    InitialValueFieldMap initialValueFieldMap;

    InductionVariableSet *inductionVariables;
    BasicBlock *dominatingLoopCountableBlock;
    LoopCount *loopCount;
    SymIdToStackSymMap *loopCountBasedBoundBaseSyms;

    bool                isDead : 1;
    bool                hasDeadStoreCollectionPass : 1;
    bool                hasDeadStorePrepass : 1;
    bool                hasCall : 1;
    bool                hasHoistedFields : 1;
    bool                needImplicitCallBailoutChecksForJsArrayCheckHoist : 1;
    bool                allFieldsKilled : 1;
    bool                isLeaf : 1;
    bool                isProcessed : 1; // Set and reset at varying places according to the phase we're in.
                                         // For example, in the lowerer, it'll be set to true when we process the loopTop for a certain loop
    struct MemCopyCandidate;
    struct MemSetCandidate;
    struct MemOpCandidate
    {
        SymID base;
        SymID index;
        byte count;
        bool bIndexAlreadyChanged;
        enum MemOpType
        {
            MEMSET,
            MEMCOPY
        } type;
        bool IsMemSet() const { return type == MEMSET; }
        bool IsMemCopy() const { return type == MEMCOPY; }
        struct Loop::MemCopyCandidate* AsMemCopy();
        struct Loop::MemSetCandidate* AsMemSet();
        MemOpCandidate(MemOpType type) :
            type(type)
        {
        }
    };

    struct MemSetCandidate : public MemOpCandidate
    {
        BailoutConstantValue constant;
        StackSym* varSym;

        MemSetCandidate() : MemOpCandidate(MemOpCandidate::MEMSET), varSym(nullptr) {}
    };

    struct MemCopyCandidate : public MemOpCandidate
    {
        SymID ldBase;
        StackSym* transferSym;
        byte ldCount;
        MemCopyCandidate() : MemOpCandidate(MemOpCandidate::MEMCOPY) {}
    };

#define FOREACH_MEMOP_CANDIDATES_EDITING(data, loop, iterator) FOREACH_SLISTCOUNTED_ENTRY_EDITING(Loop::MemOpCandidate*, data, loop->memOpInfo->candidates, iterator)
#define NEXT_MEMOP_CANDIDATE_EDITING NEXT_SLISTCOUNTED_ENTRY_EDITING
#define FOREACH_MEMOP_CANDIDATES(data, loop) FOREACH_SLISTCOUNTED_ENTRY(Loop::MemOpCandidate*, data, loop->memOpInfo->candidates)
#define NEXT_MEMOP_CANDIDATE NEXT_SLISTCOUNTED_ENTRY

#define MEMOP_CANDIDATE_TYPE_CHECK(candidate, data, type) if(candidate->Is ## type()) {Loop:: ## type ## Candidate* data = candidate->As## type();

#define FOREACH_MEMCOPY_CANDIDATES_EDITING(data, loop, iterator) {FOREACH_MEMOP_CANDIDATES_EDITING(_memopCandidate, loop, iterator) {MEMOP_CANDIDATE_TYPE_CHECK(_memopCandidate, data, MemCopy)
#define NEXT_MEMCOPY_CANDIDATE_EDITING }}NEXT_MEMOP_CANDIDATE_EDITING}
#define FOREACH_MEMCOPY_CANDIDATES(data, loop) {FOREACH_MEMOP_CANDIDATES(_memopCandidate, loop) {MEMOP_CANDIDATE_TYPE_CHECK(_memopCandidate, data, MemCopy)
#define NEXT_MEMCOPY_CANDIDATE }}NEXT_MEMOP_CANDIDATE}

#define FOREACH_MEMSET_CANDIDATES_EDITING(data, loop, iterator) {FOREACH_MEMOP_CANDIDATES_EDITING(_memopCandidate, loop, iterator) {MEMOP_CANDIDATE_TYPE_CHECK(_memopCandidate, data, MemSet)
#define NEXT_MEMSET_CANDIDATE_EDITING }}NEXT_MEMOP_CANDIDATE_EDITING}
#define FOREACH_MEMSET_CANDIDATES(data, loop) {FOREACH_MEMOP_CANDIDATES(_memopCandidate, loop) {MEMOP_CANDIDATE_TYPE_CHECK(_memopCandidate, data, MemSet)
#define NEXT_MEMSET_CANDIDATE }}NEXT_MEMOP_CANDIDATE}

    typedef struct
    {
        byte unroll : 7;
        byte isIncremental : 1;
    } InductionVariableChangeInfo;

    typedef JsUtil::BaseDictionary<SymID, InductionVariableChangeInfo, JitArenaAllocator> InductionVariableChangeInfoMap;
    typedef JsUtil::BaseDictionary<byte, IR::Opnd*, JitArenaAllocator> InductionVariableOpndPerUnrollMap;
    typedef SListCounted<MemOpCandidate *>  MemOpList;
    typedef struct
    {
        MemOpList *candidates;
        bool doMemOp : 1;
        BVSparse<JitArenaAllocator> *inductionVariablesUsedAfterLoop;
        InductionVariableChangeInfoMap *inductionVariableChangeInfoMap;
        InductionVariableOpndPerUnrollMap *inductionVariableOpndPerUnrollMap;
        // This assumes that all memop operation use the same index and has the same length
        // Temporary map to reuse existing startIndexOpnd while emiting
        // 0 = !increment & !alreadyChanged, 1 = !increment & alreadyChanged, 2 = increment & !alreadyChanged, 3 = increment & alreadyChanged
        IR::RegOpnd* startIndexOpndCache[4];
    } MemOpInfo;

    MemOpInfo *memOpInfo;

    struct RegAlloc
    {
        Lifetime **                 loopTopRegContent;      // Save off the state of the registers at the loop top
        BVSparse<JitArenaAllocator> *  symRegUseBv;         // If a lifetime was live in a reg into the loop, did the reg get used before being spilled?
        BVSparse<JitArenaAllocator> *  defdInLoopBv;        // Was a lifetime defined in the loop?
        BVSparse<JitArenaAllocator> *  liveOnBackEdgeSyms;  // Is a lifetime live on the back-edge of the loop?
        BitVector                   regUseBv;               // Registers used in this loop so far
        uint32                      loopStart;              // loopTopLabel->GetNumber()
        uint32                      loopEnd;                // loopTailBranch->GetNumber()
        uint32                      helperLength;           // Number of instrs in helper code in loop
        SList<Lifetime *>        *  extendedLifetime;       // Lifetimes to extend for this loop
        SList<Lifetime **>       *  exitRegContentList;     // Linked list of regContents for the exit edges
        bool                        hasNonOpHelperCall;
        bool                        hasCall;
        bool                        hasAirLock;             // Do back-edges have airlock blocks?
    } regAlloc;

public:
    Loop(JitArenaAllocator * alloc, Func *func)
        : topFunc(func),
        blockList(alloc),
        parent(nullptr),
        landingPad(nullptr),
        loopTopLabel(nullptr),
        symsUsedBeforeDefined(nullptr),
        likelyIntSymsUsedBeforeDefined(nullptr),
        likelyNumberSymsUsedBeforeDefined(nullptr),
        likelySimd128F4SymsUsedBeforeDefined(nullptr),
        likelySimd128I4SymsUsedBeforeDefined(nullptr),
        forceFloat64SymsOnEntry(nullptr),
        forceSimd128F4SymsOnEntry(nullptr),
        forceSimd128I4SymsOnEntry(nullptr),
        symsDefInLoop(nullptr),
        fieldHoistCandidateTypes(nullptr),
        fieldHoistSymMap(alloc),
        needImplicitCallBailoutChecksForJsArrayCheckHoist(false),
        inductionVariables(nullptr),
        dominatingLoopCountableBlock(nullptr),
        loopCount(nullptr),
        loopCountBasedBoundBaseSyms(nullptr),
        isDead(false),
        allFieldsKilled(false),
        isLeaf(true),
        isProcessed(false),
        initialValueFieldMap(alloc)
    {
        this->loopNumber = ++func->loopCount;
    }

    void                SetHeadBlock(BasicBlock *block) { headBlock = block; }
    BasicBlock *        GetHeadBlock() const { Assert(headBlock == blockList.Head()); return headBlock; }
    bool                IsDescendentOrSelf(Loop const * loop) const;

    bool                EnsureMemOpVariablesInitialized();

    Js::ImplicitCallFlags GetImplicitCallFlags();
    void                SetImplicitCallFlags(Js::ImplicitCallFlags flags);
    Js::LoopFlags GetLoopFlags() const { return loopFlags; }
    void SetLoopFlags(Js::LoopFlags val) { loopFlags = val; }
    bool                CanHoistInvariants();
    bool                CanDoFieldCopyProp();
    bool                CanDoFieldHoist();
    void                SetHasCall();
    IR::LabelInstr *    GetLoopTopInstr() const;
    void                SetLoopTopInstr(IR::LabelInstr * loopTop);
    Func *              GetFunc() const { return GetLoopTopInstr()->m_func; }
#if DBG_DUMP
    bool                GetHasCall() const { return hasCall; }
    uint                GetLoopNumber() const;
#endif
private:
    void                InsertLandingPad(FlowGraph *fg);
    bool                RemoveBreakBlocks(FlowGraph *fg);
};

// Structure definition cannot be inside Loop in order to use it as a parameter in GlobOpt
struct MemOpEmitData
{
    Loop::MemOpCandidate* candidate;
    IR::Instr* stElemInstr;
    BasicBlock* block;
    Loop::InductionVariableChangeInfo inductionVar;
    IR::BailOutKind bailOutKind;
};

struct MemSetEmitData : public MemOpEmitData
{
};

struct MemCopyEmitData : public MemOpEmitData
{
    IR::Instr* ldElemInstr;
};

#define FOREACH_BLOCK_IN_FUNC(block, func)\
    FOREACH_BLOCK(block, func->m_fg)
#define NEXT_BLOCK_IN_FUNC\
    NEXT_BLOCK;

#define FOREACH_BLOCK_IN_FUNC_DEAD_OR_ALIVE(block, func)\
    FOREACH_BLOCK_DEAD_OR_ALIVE(block, func->m_fg)
#define NEXT_BLOCK_IN_FUNC_DEAD_OR_ALIVE\
    NEXT_BLOCK_DEAD_OR_ALIVE;

#define FOREACH_BLOCK_BACKWARD_IN_FUNC(block, func) \
    FOREACH_BLOCK_BACKWARD(block, func->m_fg)

#define NEXT_BLOCK_BACKWARD_IN_FUNC \
    NEXT_BLOCK_BACKWARD;

#define FOREACH_BLOCK_BACKWARD_IN_FUNC_DEAD_OR_ALIVE(block, func) \
    FOREACH_BLOCK_BACKWARD_DEAD_OR_ALIVE(block, func->m_fg)

#define NEXT_BLOCK_BACKWARD_IN_FUNC_DEAD_OR_ALIVE \
    NEXT_BLOCK_BACKWARD_DEAD_OR_ALIVE;

#define FOREACH_BLOCK_IN_FUNC_EDITING(block, func)\
    FOREACH_BLOCK_EDITING(block, func->m_fg)
#define NEXT_BLOCK_IN_FUNC_EDITING\
    NEXT_BLOCK_EDITING;

#define FOREACH_BLOCK_BACKWARD_IN_FUNC_EDITING(block, func)\
    FOREACH_BLOCK_BACKWARD_EDITING(block, func->m_fg)
#define NEXT_BLOCK_BACKWARD_IN_FUNC_EDITING\
    NEXT_BLOCK_BACKWARD_EDITING;

#define FOREACH_BLOCK_ALL(block, graph) \
    for (BasicBlock *block = graph->blockList;\
        block != nullptr;\
        block = block->next)\
    {

#define NEXT_BLOCK_ALL \
    }

#define FOREACH_BLOCK(block, graph)\
    FOREACH_BLOCK_ALL(block, graph) \
        if (block->isDeleted) { continue; }

#define NEXT_BLOCK \
    NEXT_BLOCK_ALL

#define FOREACH_BLOCK_DEAD_OR_ALIVE(block, graph)\
    FOREACH_BLOCK_ALL(block, graph) \
        if (block->isDeleted && !block->isDead) { continue; }

#define NEXT_BLOCK_DEAD_OR_ALIVE \
    NEXT_BLOCK_ALL

#define FOREACH_BLOCK_BACKWARD(block, graph)\
    FOREACH_BLOCK_BACKWARD_IN_RANGE(block, graph->tailBlock, nullptr)

#define NEXT_BLOCK_BACKWARD \
    NEXT_BLOCK_BACKWARD_IN_RANGE

#define FOREACH_BLOCK_BACKWARD_DEAD_OR_ALIVE(block, graph)\
    FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, graph->tailBlock, nullptr)

#define NEXT_BLOCK_BACKWARD_DEAD_OR_ALIVE \
    NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE

#define FOREACH_BLOCK_BACKWARD_IN_RANGE_ALL(block, blockList, blockLast)\
{\
    BasicBlock * blockStop = blockLast? ((BasicBlock *)blockLast)->prev : nullptr; \
    for (BasicBlock *block = blockList;\
        block != blockStop;\
        block = block->prev)\
    {

#define NEXT_BLOCK_BACKWARD_IN_RANGE_ALL \
    }}

#define FOREACH_BLOCK_BACKWARD_IN_RANGE(block, blockList, blockLast) \
    FOREACH_BLOCK_BACKWARD_IN_RANGE_ALL(block, blockList, blockLast) \
        if (block->isDeleted) { continue; }

#define NEXT_BLOCK_BACKWARD_IN_RANGE \
    NEXT_BLOCK_BACKWARD_IN_RANGE_ALL

#define FOREACH_BLOCK_BACKWARD_IN_RANGE_ALL_EDITING(block, blockList, blockLast, blockPrev)\
{\
    BasicBlock *blockPrev;\
    BasicBlock * blockStop = blockLast? ((BasicBlock *)blockLast)->prev : nullptr; \
    for (BasicBlock *block = blockList;\
        block != blockStop;\
        block = blockPrev)\
    {\
        blockPrev = block->prev;

#define NEXT_BLOCK_BACKWARD_IN_RANGE_ALL_EDITING \
    }}

#define FOREACH_BLOCK_BACKWARD_IN_RANGE_EDITING(block, blockList, blockLast, blockPrev) \
    FOREACH_BLOCK_BACKWARD_IN_RANGE_ALL_EDITING(block, blockList, blockLast, blockPrev) \
        if (block->isDeleted) { continue; }

#define NEXT_BLOCK_BACKWARD_IN_RANGE_EDITING \
    NEXT_BLOCK_BACKWARD_IN_RANGE_ALL_EDITING

#define FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, blockList, blockLast) \
    FOREACH_BLOCK_BACKWARD_IN_RANGE_ALL(block, blockList, blockLast) \
        if (block->isDeleted && !block->isDead) { continue; }

#define NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE \
    NEXT_BLOCK_BACKWARD_IN_RANGE_ALL

#define FOREACH_BLOCK_EDITING(block, graph)\
    {\
        BasicBlock *blockNext;\
        for (BasicBlock *block = graph->blockList;\
            block != nullptr;\
            block = blockNext)\
        {\
            blockNext = block->next; \
            if (block->isDeleted) { continue; }
#define NEXT_BLOCK_EDITING \
}}

#define FOREACH_BLOCK_BACKWARD_EDITING(block, graph)\
    {\
        BasicBlock *blockPrev;\
        for (BasicBlock *block = graph->tailBlock;\
            block != nullptr;\
            block = blockPrev)\
        {\
            blockPrev = block->prev; \
            if (block->isDeleted) { continue; }
#define NEXT_BLOCK_BACKWARD_EDITING \
}}

#define FOREACH_BLOCK_IN_LIST(block, list)\
    FOREACH_SLIST_ENTRY(BasicBlock*, block, list)\
    {\
        if (block->isDeleted) { continue; }

#define NEXT_BLOCK_IN_LIST \
    NEXT_SLIST_ENTRY \
    }

#define FOREACH_BLOCK_IN_LIST_EDITING(block, list, iter)\
    FOREACH_SLIST_ENTRY_EDITING(BasicBlock*, block, list, iter)\
    {\
        if (block->isDeleted) { continue; }

#define NEXT_BLOCK_IN_LIST_EDITING \
    NEXT_SLIST_ENTRY_EDITING \
    }

#define FOREACH_SUCCESSOR_EDGE(edge, block)\
    FOREACH_EDGE_IN_LIST(edge, block->GetSuccList())
#define NEXT_SUCCESSOR_EDGE\
    NEXT_EDGE_IN_LIST

#define FOREACH_SUCCESSOR_EDGE_EDITING(edge, bloc, iter)\
    FOREACH_EDGE_IN_LIST_EDITING(edge, block->GetSuccList(), iter)
#define NEXT_SUCCESSOR_EDGE_EDITING\
    NEXT_EDGE_IN_LIST_EDITING

#define FOREACH_PREDECESSOR_EDGE(edge, block)\
    FOREACH_EDGE_IN_LIST(edge, block->GetPredList())
#define NEXT_PREDECESSOR_EDGE\
    NEXT_EDGE_IN_LIST

#define FOREACH_PREDECESSOR_EDGE_EDITING(edge, block, iter)\
    FOREACH_EDGE_IN_LIST_EDITING(edge, block->GetPredList(), iter)
#define NEXT_PREDECESSOR_EDGE_EDITING\
    NEXT_EDGE_IN_LIST_EDITING

#define FOREACH_EDGE_IN_LIST(edge, list)\
    FOREACH_SLISTBASECOUNTED_ENTRY(FlowEdge*, edge, list)\
    {
#define NEXT_EDGE_IN_LIST\
    NEXT_SLISTBASECOUNTED_ENTRY }

#define FOREACH_EDGE_IN_LIST_EDITING(edge, list, iter)\
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, list, iter)\
    {\

#define NEXT_EDGE_IN_LIST_EDITING\
    NEXT_SLISTBASECOUNTED_ENTRY_EDITING }

#define FOREACH_SUCCESSOR_BLOCK(blockSucc, block)\
    FOREACH_EDGE_IN_LIST(__edge, block->GetSuccList())\
    {\
        BasicBlock * blockSucc = __edge->GetSucc(); \
        AnalysisAssert(blockSucc);

#define NEXT_SUCCESSOR_BLOCK\
    }\
    NEXT_EDGE_IN_LIST

#define FOREACH_SUCCESSOR_BLOCK_EDITING(blockSucc, block, iter)\
    FOREACH_EDGE_IN_LIST_EDITING(__edge, block->GetSuccList(), iter)\
    {\
        BasicBlock * blockSucc = __edge->GetSucc(); \
        AnalysisAssert(blockSucc);

#define NEXT_SUCCESSOR_BLOCK_EDITING\
    }\
    NEXT_EDGE_IN_LIST_EDITING

#define FOREACH_DEAD_SUCCESSOR_BLOCK(blockSucc, block)\
    FOREACH_EDGE_IN_LIST(__edge, block->GetDeadSuccList())\
    {\
        BasicBlock * blockSucc = __edge->GetSucc(); \
        AnalysisAssert(blockSucc);

#define NEXT_DEAD_SUCCESSOR_BLOCK\
    }\
    NEXT_EDGE_IN_LIST

#define FOREACH_PREDECESSOR_BLOCK(blockPred, block)\
    FOREACH_EDGE_IN_LIST(__edge, block->GetPredList())\
    {\
        BasicBlock * blockPred = __edge->GetPred(); \
        AnalysisAssert(blockPred);

#define NEXT_PREDECESSOR_BLOCK\
    }\
    NEXT_EDGE_IN_LIST

#define FOREACH_DEAD_PREDECESSOR_BLOCK(blockPred, block)\
    FOREACH_EDGE_IN_LIST(__edge, block->GetDeadPredList())\
    {\
        BasicBlock * blockPred = __edge->GetPred(); \
        AnalysisAssert(blockPred);

#define NEXT_DEAD_PREDECESSOR_BLOCK\
    }\
    NEXT_EDGE_IN_LIST

#define FOREACH_BLOCK_IN_LOOP(block, loop)\
    FOREACH_BLOCK_IN_LIST(block, &loop->blockList)
#define NEXT_BLOCK_IN_LOOP \
    NEXT_BLOCK_IN_LIST

#define FOREACH_BLOCK_IN_LOOP_EDITING(block, loop, iter)\
    FOREACH_BLOCK_IN_LIST_EDITING(block, &loop->blockList, iter)
#define NEXT_BLOCK_IN_LOOP_EDITING \
    NEXT_BLOCK_IN_LIST_EDITING


#define FOREACH_LOOP_IN_FUNC_EDITING(loop, func)\
    FOREACH_LOOP_EDITING(loop, func->m_fg)
#define NEXT_LOOP_IN_FUNC_EDITING\
    NEXT_LOOP_EDITING;

#define FOREACH_LOOP_EDITING(loop, graph)\
        {\
        Loop* loopNext;\
        for (Loop* loop = graph->loopList;\
            loop != nullptr;\
            loop = loopNext)\
                {\
            loopNext = loop->next;
#define NEXT_LOOP_EDITING \
}}
