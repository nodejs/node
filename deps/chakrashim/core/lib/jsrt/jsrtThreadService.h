//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "Base\ThreadServiceWrapperBase.h"

class JsrtThreadService : public ThreadServiceWrapperBase
{
public:
    JsrtThreadService();
    ~JsrtThreadService();

    bool Initialize(ThreadContext *threadContext);
    unsigned int Idle();

    // Does nothing, we don't force idle collection for JSRT
    void SetForceOneIdleCollection() override {}

private:
    bool CanScheduleIdleCollect() override { return true; }
    bool OnScheduleIdleCollect(uint ticks, bool scheduleAsTask) override;
    void OnFinishIdleCollect() override;
    bool ShouldFinishConcurrentCollectOnIdleCallback() override;

    unsigned int nextIdleTick;
};
