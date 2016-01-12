//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Event
{
private:
    const HANDLE handle;

public:
    Event(const bool autoReset, const bool signaled = false);

private:
    Event(const Event &) : handle(0)
    {
    }

    Event &operator =(const Event &)
    {
        return *this;
    }

public:
    ~Event()
    {
        CloseHandle(handle);
    }

public:
    HANDLE Handle() const
    {
        return handle;
    }

    void Set() const
    {
        SetEvent(handle);
    }

    void Reset() const
    {
        ResetEvent(handle);
    }

    bool Wait(const unsigned int milliseconds = INFINITE) const;
};
