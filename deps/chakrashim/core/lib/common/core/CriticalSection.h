//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class CriticalSection
{
public:
    CriticalSection(DWORD spincount = 0)
    {
#pragma prefast(suppress:6031, "InitializeCriticalSectionAndSpinCount always succeed since Vista. No need to check return value");
        ::InitializeCriticalSectionAndSpinCount(&cs, spincount);
    }
    ~CriticalSection() { ::DeleteCriticalSection(&cs); }
    BOOL TryEnter() { return ::TryEnterCriticalSection(&cs); }
    void Enter() { ::EnterCriticalSection(&cs); }
    void Leave() { ::LeaveCriticalSection(&cs); }
#if DBG
    bool IsLocked() const { return cs.OwningThread == (HANDLE)::GetCurrentThreadId(); }
#endif
private:
    CRITICAL_SECTION cs;
};

//FakeCriticalSection mimics CriticalSection apis
class FakeCriticalSection
{
public:
    FakeCriticalSection(DWORD spincount = 0) { /*do nothing*/spincount++; }
    ~FakeCriticalSection() {}
    BOOL TryEnter() { return true; }
    void Enter() {}
    void Leave() {}
#if DBG
    bool IsLocked() const { return true; }
#endif
};

class AutoCriticalSection
{
public:
    AutoCriticalSection(CriticalSection * cs) : cs(cs) { cs->Enter(); }
    ~AutoCriticalSection() { cs->Leave(); }
private:
    CriticalSection * cs;
};

class AutoOptionalCriticalSection
{
public:
    AutoOptionalCriticalSection(CriticalSection * cs) : cs(cs)
    {
        if (cs)
        {
            cs->Enter();
        }
    }

    ~AutoOptionalCriticalSection()
    {
        if (cs)
        {
            cs->Leave();
        }
    }

private:
    CriticalSection * cs;
};

template <class SyncObject = FakeCriticalSection >
class AutoRealOrFakeCriticalSection
{
public:
    AutoRealOrFakeCriticalSection(SyncObject * cs) : cs(cs) { cs->Enter(); }
    ~AutoRealOrFakeCriticalSection() { cs->Leave(); }
private:
    SyncObject * cs;
};

template <class SyncObject = FakeCriticalSection >
class AutoOptionalRealOrFakeCriticalSection
{
public:
    AutoOptionalRealOrFakeCriticalSection(SyncObject * cs) : cs(cs)
    {
        if (cs)
        {
            cs->Enter();
        }
    }

    ~AutoOptionalRealOrFakeCriticalSection()
    {
        if (cs)
        {
            cs->Leave();
        }
    }

private:
    SyncObject * cs;
};

