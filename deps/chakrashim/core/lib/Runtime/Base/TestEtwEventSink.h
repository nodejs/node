//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef TEST_ETW_EVENTS
#define CREATE_EVENTSINK_PROC_NAME CreateEventSink

// Abstract class to be inherited to receive ETW event notifications
class TestEtwEventSink
{
public:
    virtual void WriteMethodEvent(const wchar_t* eventName,
        void* scriptContextId,
        void* methodStartAddress,
        uint64 methodSize,
        uint methodID,
        uint16 methodFlags,
        uint16 methodAddressRangeID,
        DWORD_PTR sourceID,
        uint line,
        uint column,
        const wchar_t* methodName) = 0;

    virtual void WriteSourceEvent(const wchar_t* eventName, uint64 sourceContext, void* scriptContextId, uint sourceFlags, const wchar_t* url) = 0;

    virtual void UnloadInstance() = 0;

    static bool IsLoaded();
    static bool Load();
    static void Unload();
    static TestEtwEventSink* Instance;

    typedef void (*RundownFunc)(bool start);

private:
    static char const * const TestEtwEventSink::CreateEventSinkProcName;
    typedef TestEtwEventSink * (*CreateEventSink)(RundownFunc rundown, bool trace);
};
#endif
