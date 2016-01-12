//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_NATIVE_CODEGEN
typedef DList<ParseNode*, ArenaAllocator> NodeDList;

struct BackgroundParseItem sealed : public JsUtil::Job
{
    BackgroundParseItem(JsUtil::JobManager *const manager, Parser *const parser, ParseNode *parseNode, bool defer);

    ParseContext *GetParseContext() { return &parseContext; }
    ParseNode *GetParseNode() const { return parseNode; }
    CompileScriptException *GetPSE() const { return pse; }
    HRESULT GetHR() const { return hr; }
    bool IsStrictMode() const { return strictMode; }
    bool Succeeded() const { return hr == S_OK; }
    bool IsInParseQueue() const { return inParseQueue; }
    bool IsDeferred() const { return isDeferred;}
    void SetHR(HRESULT hr) { this->hr = hr; }
    void SetCompleted(bool has) { completed = has; }
    void SetPSE(CompileScriptException *pse) { this->pse = pse; }

    uint GetMaxBlockId() const { return maxBlockId; }
    void SetMaxBlockId(uint blockId) { maxBlockId = blockId; }
    Parser *GetParser() const { return parser; }
    void SetParser(Parser *p) { parser = p; }
    BackgroundParseItem *GetNext() const { return nextItem; }
    void SetNext(BackgroundParseItem *item) { nextItem = item; }
    BackgroundParseItem *GetNextUnprocessedItem() const { return nextUnprocessedItem; }
    void SetNextUnprocessedItem(BackgroundParseItem *item) { nextUnprocessedItem = item; }
    BackgroundParseItem *GetPrevUnprocessedItem() const { return prevUnprocessedItem; }
    void SetPrevUnprocessedItem(BackgroundParseItem *item) { prevUnprocessedItem = item; }
    DList<ParseNode*, ArenaAllocator>* RegExpNodeList() { return regExpNodes; }

    void OnAddToParseQueue();
    void OnRemoveFromParseQueue();
    void AddRegExpNode(ParseNode *const pnode, ArenaAllocator *alloc);

private:
    ParseContext parseContext;
    Parser *parser;
    BackgroundParseItem *nextItem;
    BackgroundParseItem *nextUnprocessedItem;
    BackgroundParseItem *prevUnprocessedItem;
    ParseNode *parseNode;
    CompileScriptException *pse;
    NodeDList* regExpNodes;
    HRESULT hr;
    uint maxBlockId;
    bool isDeferred;
    bool strictMode;
    bool inParseQueue;
    bool completed;
};

class BackgroundParser sealed : public JsUtil::WaitableJobManager
{
public:
    BackgroundParser(Js::ScriptContext *scriptContext);
    ~BackgroundParser();

    static BackgroundParser * New(Js::ScriptContext *scriptContext);
    static void Delete(BackgroundParser *backgroundParser);

    volatile uint* GetPendingBackgroundItemsPtr() const { return (volatile uint*)&pendingBackgroundItems; }

    virtual bool Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData) override;
    virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override;
    virtual void OnDecommit(JsUtil::ParallelThreadData *threadData) override;

    bool Process(JsUtil::Job *const job, Parser *parser, CompileScriptException *pse);
    bool ParseBackgroundItem(Parser *parser, ParseNode *parseNode, bool isDeferred);
    BackgroundParseItem * NewBackgroundParseItem(Parser *parser, ParseNode *parseNode, bool isDeferred);

    BackgroundParseItem *GetJob(BackgroundParseItem *item) const;
    bool WasAddedToJobProcessor(JsUtil::Job *const job) const;
    void BeforeWaitForJob(BackgroundParseItem *const item) const;
    void AfterWaitForJob(BackgroundParseItem *const item) const;

    BackgroundParseItem *GetNextUnprocessedItem() const;
    void AddUnprocessedItem(BackgroundParseItem *const item);
    void RemoveFromUnprocessedItems(BackgroundParseItem *const item);

    void SetFailedBackgroundParseItem(BackgroundParseItem *item) { failedBackgroundParseItem = item; }
    BackgroundParseItem *GetFailedBackgroundParseItem() const { return failedBackgroundParseItem; }
    bool HasFailedBackgroundParseItem() const { return failedBackgroundParseItem != nullptr; }

private:
    void AddToParseQueue(BackgroundParseItem *const item, bool prioritize, bool lock);

private:
    Js::ScriptContext *scriptContext;
    uint pendingBackgroundItems;
    BackgroundParseItem *failedBackgroundParseItem;
    BackgroundParseItem *unprocessedItemsHead;
    BackgroundParseItem *unprocessedItemsTail;

#if DBG
    ThreadContextId mainThreadId;
#endif
};
#endif
