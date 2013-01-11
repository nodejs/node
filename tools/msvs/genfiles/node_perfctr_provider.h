/* This file was auto-generated from src\res\node_perfctr_provider.man by ctrpp.exe */

#pragma once


EXTERN_C DECLSPEC_SELECTANY GUID NodeCounterProviderGuid = { 0x1e2e15d7, 0x3760, 0x470e, 0x86, 0x99, 0xb9, 0xdb, 0x52, 0x48, 0xed, 0xd5 };

EXTERN_C DECLSPEC_SELECTANY GUID NodeCounterSetGuid = { 0x3a22a8ec, 0x297c, 0x48ac, 0xab, 0x15, 0x33, 0xec, 0x93, 0x3, 0x3f, 0xd8 };


EXTERN_C DECLSPEC_SELECTANY HANDLE NodeCounterProvider = NULL;

EXTERN_C DECLSPEC_SELECTANY struct {
    PERF_COUNTERSET_INFO CounterSet;
    PERF_COUNTER_INFO Counter0;
    PERF_COUNTER_INFO Counter1;
    PERF_COUNTER_INFO Counter2;
    PERF_COUNTER_INFO Counter3;
    PERF_COUNTER_INFO Counter4;
    PERF_COUNTER_INFO Counter5;
    PERF_COUNTER_INFO Counter6;
    PERF_COUNTER_INFO Counter7;
    PERF_COUNTER_INFO Counter8;
    PERF_COUNTER_INFO Counter9;
} NodeCounterSetInfo = {
    { { 0x3a22a8ec, 0x297c, 0x48ac, 0xab, 0x15, 0x33, 0xec, 0x93, 0x3, 0x3f, 0xd8 }, { 0x1e2e15d7, 0x3760, 0x470e, 0x86, 0x99, 0xb9, 0xdb, 0x52, 0x48, 0xed, 0xd5 }, 10, PERF_COUNTERSET_MULTI_AGGREGATE },
    { 1, PERF_COUNTER_COUNTER, 0, sizeof(ULONG), PERF_DETAIL_NOVICE, 0, 0 },
    { 2, PERF_COUNTER_COUNTER, 0, sizeof(ULONG), PERF_DETAIL_NOVICE, 0, 0 },
    { 3, PERF_COUNTER_COUNTER, 0, sizeof(ULONG), PERF_DETAIL_NOVICE, 0, 0 },
    { 4, PERF_COUNTER_COUNTER, 0, sizeof(ULONG), PERF_DETAIL_NOVICE, 0, 0 },
    { 5, PERF_COUNTER_RAWCOUNT, 0, sizeof(ULONG), PERF_DETAIL_NOVICE, 0, 0 },
    { 6, PERF_COUNTER_BULK_COUNT, 0, sizeof(ULONGLONG), PERF_DETAIL_NOVICE, 4294967293, 0 },
    { 7, PERF_COUNTER_BULK_COUNT, 0, sizeof(ULONGLONG), PERF_DETAIL_NOVICE, 4294967293, 0 },
    { 8, PERF_COUNTER_RAWCOUNT, 0, sizeof(ULONG), PERF_DETAIL_NOVICE, 0, 0 },
    { 9, PERF_COUNTER_BULK_COUNT, 0, sizeof(ULONGLONG), PERF_DETAIL_NOVICE, 4294967293, 0 },
    { 10, PERF_COUNTER_BULK_COUNT, 0, sizeof(ULONGLONG), PERF_DETAIL_NOVICE, 4294967293, 0 },
};

EXTERN_C FORCEINLINE
VOID
CounterCleanup(
    VOID
    )
{
    if (NodeCounterProvider != NULL) {
        PerfStopProvider(NodeCounterProvider);
        NodeCounterProvider = NULL;
    }
}

EXTERN_C FORCEINLINE
ULONG
CounterInitialize(
    VOID
    )
{
    ULONG Status;
    PERF_PROVIDER_CONTEXT ProviderContext;

    ZeroMemory(&ProviderContext, sizeof(PERF_PROVIDER_CONTEXT));
    ProviderContext.ContextSize = sizeof(PERF_PROVIDER_CONTEXT);

    Status = PerfStartProviderEx(&NodeCounterProviderGuid,
                                 &ProviderContext,
                                 &NodeCounterProvider);
    if (Status != ERROR_SUCCESS) {
        NodeCounterProvider = NULL;
        return Status;
    }

    Status = PerfSetCounterSetInfo(NodeCounterProvider,
                                   &NodeCounterSetInfo.CounterSet,
                                   sizeof NodeCounterSetInfo);
    if (Status != ERROR_SUCCESS) {
        CounterCleanup();
        return Status;
    }
    return ERROR_SUCCESS;
}
