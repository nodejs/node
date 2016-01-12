//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

ExpirableObject::ExpirableObject(ThreadContext* threadContext):
    isUsed(false),
    registrationHandle(nullptr)
{
    if (threadContext)
    {
        threadContext->RegisterExpirableObject(this);
    }
}

void ExpirableObject::Finalize(bool isShutdown)
{
    if (!isShutdown && this->registrationHandle != nullptr)
    {
        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

        threadContext->UnregisterExpirableObject(this);
    }
}

void ExpirableObject::Dispose(bool isShutdown)
{
    if (!isShutdown && this->registrationHandle == nullptr)
    {
        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();
        threadContext->DisposeExpirableObject(this);
    }
}

void ExpirableObject::EnterExpirableCollectMode()
{
    this->isUsed = false;
}

bool ExpirableObject::IsObjectUsed()
{
    return this->isUsed;
}

void ExpirableObject::SetIsObjectUsed()
{
    this->isUsed = true;
}
