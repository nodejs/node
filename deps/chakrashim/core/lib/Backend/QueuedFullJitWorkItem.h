//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct CodeGenWorkItem;

class QueuedFullJitWorkItem : public JsUtil::DoublyLinkedListElement<QueuedFullJitWorkItem>
{
private:
    CodeGenWorkItem *const workItem;

public:
    QueuedFullJitWorkItem(CodeGenWorkItem *const workItem);

public:
    CodeGenWorkItem *WorkItem() const;
};
