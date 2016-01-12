//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

#define ASSERT_THREAD() AssertMsg(mainThreadId == GetCurrentThreadContextId(), \
    "Cannot use this member of BackgroundParser from thread other than the creating context's current thread")

#if ENABLE_NATIVE_CODEGEN
BackgroundParser::BackgroundParser(Js::ScriptContext *scriptContext)
    :   JsUtil::WaitableJobManager(scriptContext->GetThreadContext()->GetJobProcessor()),
        scriptContext(scriptContext),
        unprocessedItemsHead(nullptr),
        unprocessedItemsTail(nullptr),
        failedBackgroundParseItem(nullptr),
        pendingBackgroundItems(0)
{
    Processor()->AddManager(this);

#if DBG
    this->mainThreadId = GetCurrentThreadContextId();
#endif
}

BackgroundParser::~BackgroundParser()
{
    JsUtil::JobProcessor *processor = Processor();
    if (processor->ProcessesInBackground())
    {
        static_cast<JsUtil::BackgroundJobProcessor*>(processor)->IterateBackgroundThreads([&](JsUtil::ParallelThreadData *threadData)->bool {
            if (threadData->parser)
            {
                threadData->parser->Release();
                threadData->parser = nullptr;
            }
            return false;
        });
    }
    processor->RemoveManager(this);
}

BackgroundParser * BackgroundParser::New(Js::ScriptContext *scriptContext)
{
    return HeapNew(BackgroundParser, scriptContext);
}

void BackgroundParser::Delete(BackgroundParser *backgroundParser)
{
    HeapDelete(backgroundParser);
}

bool BackgroundParser::Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData)
{
    BackgroundParseItem *backgroundItem = static_cast<BackgroundParseItem*>(job);

    if (failedBackgroundParseItem)
    {
        if (backgroundItem->GetParseNode()->ichMin > failedBackgroundParseItem->GetParseNode()->ichMin)
        {
            return true;
        }
    }

    if (threadData->parser == nullptr || threadData->canDecommit)
    {
        if (threadData->parser != nullptr)
        {
            // "canDecommit" means the previous parse finished.
            // Don't leave a parser with stale state in the thread data, or we'll mess up the bindings.
            threadData->backgroundPageAllocator.DecommitNow();
            this->OnDecommit(threadData);
        }
        threadData->canDecommit = false;

        // Lazily create a parser instance for this thread from the thread's page allocator.
        // It will stay around until the main thread's current parser instance goes away, which will free
        // the background thread to decommit its pages.
        threadData->parser = Anew(threadData->threadArena, Parser, this->scriptContext, backgroundItem->IsStrictMode(), &threadData->backgroundPageAllocator, true);
        threadData->pse = Anew(threadData->threadArena, CompileScriptException);
        threadData->parser->PrepareScanner(backgroundItem->GetParseContext()->fromExternal);
    }

    Parser *parser = threadData->parser;

    return this->Process(backgroundItem, parser, threadData->pse);
}

bool BackgroundParser::Process(JsUtil::Job *const job, Parser *parser, CompileScriptException *pse)
{
    BackgroundParseItem *backgroundItem = static_cast<BackgroundParseItem*>(job);

    Assert(parser->GetCurrBackgroundParseItem() == nullptr);
    parser->SetCurrBackgroundParseItem(backgroundItem);
    backgroundItem->SetParser(parser);

    HRESULT hr = parser->ParseFunctionInBackground(backgroundItem->GetParseNode(), backgroundItem->GetParseContext(), backgroundItem->IsDeferred(), pse);
    backgroundItem->SetMaxBlockId(parser->GetLastBlockId());
    backgroundItem->SetHR(hr);
    if (FAILED(hr))
    {
        backgroundItem->SetPSE(pse);
    }
    backgroundItem->SetCompleted(true);
    parser->SetCurrBackgroundParseItem(nullptr);
    return hr == S_OK;
}

void BackgroundParser::JobProcessed(JsUtil::Job *const job, const bool succeeded)
{
    // This is called from inside a lock, so we can mess with background parser attributes.
    BackgroundParseItem *backgroundItem = static_cast<BackgroundParseItem*>(job);
    this->RemoveFromUnprocessedItems(backgroundItem);
    --this->pendingBackgroundItems;
    if (!succeeded)
    {
        Assert(FAILED(backgroundItem->GetHR()) || failedBackgroundParseItem);

        if (FAILED(backgroundItem->GetHR()))
        {
            if (!failedBackgroundParseItem)
            {
                failedBackgroundParseItem = backgroundItem;
            }
            else
            {
                // If syntax errors are detected on multiple threads, the lexically earlier one should win.
                CompileScriptException *newPse = backgroundItem->GetPSE();
                CompileScriptException *oldPse = failedBackgroundParseItem->GetPSE();

                if (newPse->line < oldPse->line ||
                    (newPse->line == oldPse->line && newPse->ichMinLine < oldPse->ichMinLine))
                {
                    failedBackgroundParseItem = backgroundItem;
                }
            }
        }
    }
}

void BackgroundParser::OnDecommit(JsUtil::ParallelThreadData *threadData)
{
    if (threadData->parser)
    {
        threadData->parser->Release();
        threadData->parser = nullptr;
    }
}

BackgroundParseItem * BackgroundParser::NewBackgroundParseItem(Parser *parser, ParseNode *parseNode, bool isDeferred)
{
    BackgroundParseItem *item = Anew(parser->GetAllocator(), BackgroundParseItem, this, parser, parseNode, isDeferred);
    parser->AddBackgroundParseItem(item);
    return item;
}

bool BackgroundParser::ParseBackgroundItem(Parser *parser, ParseNode *parseNode, bool isDeferred)
{
    ASSERT_THREAD();

    AutoPtr<BackgroundParseItem> workItemAutoPtr(this->NewBackgroundParseItem(parser, parseNode, isDeferred));
    if ((BackgroundParseItem*) workItemAutoPtr == nullptr)
    {
        // OOM, just skip this work item and return.
        // TODO: Raise an OOM parse-time exception.
        return false;
    }

    parser->PrepareForBackgroundParse();

    BackgroundParseItem * backgroundItem = workItemAutoPtr.Detach();
    this->AddToParseQueue(backgroundItem, false, this->Processor()->ProcessesInBackground());

    return true;
}

BackgroundParseItem *BackgroundParser::GetJob(BackgroundParseItem *workitem) const
{
    return workitem;
}

bool BackgroundParser::WasAddedToJobProcessor(JsUtil::Job *const job) const
{
    ASSERT_THREAD();
    Assert(job);

    return static_cast<BackgroundParseItem*>(job)->IsInParseQueue();
}

void BackgroundParser::BeforeWaitForJob(BackgroundParseItem *const item) const
{
}

void BackgroundParser::AfterWaitForJob(BackgroundParseItem *const item) const
{
}

void BackgroundParser::AddToParseQueue(BackgroundParseItem *const item, bool prioritize, bool lock)
{
    AutoOptionalCriticalSection autoLock(lock ? Processor()->GetCriticalSection() : nullptr);
    ++this->pendingBackgroundItems;
    Processor()->AddJob(item, prioritize);   // This one can throw (really unlikely though), OOM specifically.
    this->AddUnprocessedItem(item);
    item->OnAddToParseQueue();
}

void BackgroundParser::AddUnprocessedItem(BackgroundParseItem *const item)
{
    if (this->unprocessedItemsTail == nullptr)
    {
        this->unprocessedItemsHead = item;
    }
    else
    {
        this->unprocessedItemsTail->SetNextUnprocessedItem(item);
    }
    item->SetPrevUnprocessedItem(this->unprocessedItemsTail);
    this->unprocessedItemsTail = item;
}

void BackgroundParser::RemoveFromUnprocessedItems(BackgroundParseItem *const item)
{
    if (this->unprocessedItemsHead == item)
    {
        this->unprocessedItemsHead = item->GetNextUnprocessedItem();
    }
    else
    {
        item->GetPrevUnprocessedItem()->SetNextUnprocessedItem(item->GetNextUnprocessedItem());
    }
    if (this->unprocessedItemsTail == item)
    {
        this->unprocessedItemsTail = item->GetPrevUnprocessedItem();
    }
    else
    {
        item->GetNextUnprocessedItem()->SetPrevUnprocessedItem(item->GetPrevUnprocessedItem());
    }
    item->SetNextUnprocessedItem(nullptr);
    item->SetPrevUnprocessedItem(nullptr);
}

BackgroundParseItem *BackgroundParser::GetNextUnprocessedItem() const
{
    BackgroundParseItem *item;
    bool background = this->Processor()->ProcessesInBackground();
    for (item = this->unprocessedItemsHead; item; item = item->GetNextUnprocessedItem())
    {
        if (!background || !static_cast<JsUtil::BackgroundJobProcessor*>(Processor())->IsBeingProcessed(item))
        {
            return item;
        }
    }
    return nullptr;
}

BackgroundParseItem::BackgroundParseItem(JsUtil::JobManager *const manager, Parser *const parser, ParseNode *parseNode, bool defer)
    : JsUtil::Job(manager),
      maxBlockId((uint)-1),
      strictMode(parser->IsStrictMode()),
      parseNode(parseNode),
      parser(nullptr),
      nextItem(nullptr),
      nextUnprocessedItem(nullptr),
      prevUnprocessedItem(nullptr),
      pse(nullptr),
      regExpNodes(nullptr),
      completed(false),
      inParseQueue(false),
      isDeferred(defer)
{
    parser->CaptureContext(&parseContext);
}

void BackgroundParseItem::OnAddToParseQueue()
{
    this->inParseQueue = true;
}

void BackgroundParseItem::OnRemoveFromParseQueue()
{
    this->inParseQueue = false;
}

void BackgroundParseItem::AddRegExpNode(ParseNode *const pnode, ArenaAllocator *alloc)
{
    if (regExpNodes == nullptr)
    {
        regExpNodes = Anew(alloc, NodeDList, alloc);
    }

    regExpNodes->Append(pnode);
}
#endif
