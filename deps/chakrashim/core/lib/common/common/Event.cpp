//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "Common\Event.h"

Event::Event(const bool autoReset, const bool signaled) : handle(CreateEvent(0, !autoReset, signaled, 0))
{
    if(!handle)
        Js::Throw::OutOfMemory();
}

bool Event::Wait(const unsigned int milliseconds) const
{
    const unsigned int result = WaitForSingleObject(handle, milliseconds);
    if(!(result == WAIT_OBJECT_0 || (result == WAIT_TIMEOUT && milliseconds != INFINITE)))
        Js::Throw::FatalInternalError();
    return result == WAIT_OBJECT_0;
}
