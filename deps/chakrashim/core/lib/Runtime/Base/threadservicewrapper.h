//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
// This class is used to communicate between ThreadContext and JavascriptThreadService

class ThreadServiceWrapper abstract
{
public:
    virtual bool ScheduleNextCollectOnExit() = 0;
    virtual void ScheduleFinishConcurrent() = 0;
    virtual void SetForceOneIdleCollection() = 0;
};

