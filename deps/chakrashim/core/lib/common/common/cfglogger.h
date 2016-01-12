//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifdef CONTROL_FLOW_GUARD_LOGGER
class CFGLogger
{
public:
    static void Enable();
    static void __fastcall GuardCheck(_In_ uintptr_t Target);
private:
    CFGLogger() {}
    ~CFGLogger();

    typedef void(__fastcall * PfnGuardCheckFunction)(_In_ uintptr_t Target);
    static PfnGuardCheckFunction oldGuardCheck;
    static bool inGuard;
    static CriticalSection cs;
    static JsUtil::BaseDictionary<uintptr_t, uint, NoCheckHeapAllocator> guardCheckRecord;
    static CFGLogger Instance;
};

#endif
