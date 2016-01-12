//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

QueuedFullJitWorkItem::QueuedFullJitWorkItem(CodeGenWorkItem *const workItem) : workItem(workItem)
{
    Assert(workItem->GetJitMode() == ExecutionMode::FullJit);
}

CodeGenWorkItem *QueuedFullJitWorkItem::WorkItem() const
{
    return workItem;
}
