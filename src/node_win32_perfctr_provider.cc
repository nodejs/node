// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_counters.h"
#include <perflib.h>
#include "node_win32_perfctr_provider.h"

#define __INIT_node_perfctr_provider_IMP
#include <node_perfctr_provider.h>

typedef ULONG (WINAPI *PerfStartProviderExFunc)(
    __in LPGUID ProviderGuid,
    __in_opt PPERF_PROVIDER_CONTEXT ProviderContext,
    __out PHANDLE Provider
    );

typedef ULONG (WINAPI *PerfStopProviderFunc)(
    __in HANDLE ProviderHandle
    );

typedef ULONG (WINAPI *PerfSetCounterSetInfoFunc)(
    __in HANDLE ProviderHandle,
    __inout_bcount(TemplateSize) PPERF_COUNTERSET_INFO Template,
    __in ULONG TemplateSize
    );

typedef PPERF_COUNTERSET_INSTANCE (WINAPI *PerfCreateInstanceFunc)(
    __in HANDLE ProviderHandle,
    __in LPCGUID CounterSetGuid,
    __in PCWSTR Name,
    __in ULONG Id
    );

typedef ULONG (WINAPI *PerfDeleteInstanceFunc)(
    __in HANDLE Provider,
    __in PPERF_COUNTERSET_INSTANCE InstanceBlock
    );

typedef ULONG (WINAPI *PerfSetULongCounterValueFunc)(
    __in HANDLE Provider,
    __inout PPERF_COUNTERSET_INSTANCE Instance,
    __in ULONG CounterId,
    __in ULONG Value
    );

typedef ULONG (WINAPI *PerfSetULongLongCounterValueFunc)(
    __in HANDLE Provider,
    __inout PPERF_COUNTERSET_INSTANCE Instance,
    __in ULONG CounterId,
    __in ULONGLONG Value
    );

typedef ULONG (WINAPI *PerfIncrementULongCounterValueFunc)(
    __in HANDLE Provider,
    __inout PPERF_COUNTERSET_INSTANCE Instance,
    __in ULONG CounterId,
    __in ULONG Value
    );

typedef ULONG (WINAPI *PerfIncrementULongLongCounterValueFunc)(
    __in HANDLE Provider,
    __inout PPERF_COUNTERSET_INSTANCE Instance,
    __in ULONG CounterId,
    __in ULONGLONG Value
    );

typedef ULONG (WINAPI *PerfDecrementULongCounterValueFunc)(
    __in HANDLE Provider,
    __inout PPERF_COUNTERSET_INSTANCE Instance,
    __in ULONG CounterId,
    __in ULONG Value
    );

typedef ULONG (WINAPI *PerfDecrementULongLongCounterValueFunc)(
    __in HANDLE Provider,
    __inout PPERF_COUNTERSET_INSTANCE Instance,
    __in ULONG CounterId,
    __in ULONGLONG Value
    );


HMODULE advapimod;
PerfStartProviderExFunc perfctr_startProvider;
PerfStopProviderFunc perfctr_stopProvider;
PerfSetCounterSetInfoFunc perfctr_setCounterSetInfo;
PerfCreateInstanceFunc perfctr_createInstance;
PerfDeleteInstanceFunc perfctr_deleteInstance;
PerfSetULongCounterValueFunc perfctr_setULongValue;
PerfSetULongLongCounterValueFunc perfctr_setULongLongValue;
PerfIncrementULongCounterValueFunc perfctr_incrementULongValue;
PerfIncrementULongLongCounterValueFunc perfctr_incrementULongLongValue;
PerfDecrementULongCounterValueFunc perfctr_decrementULongValue;
PerfDecrementULongLongCounterValueFunc perfctr_decrementULongLongValue;

PPERF_COUNTERSET_INSTANCE perfctr_instance;


#define NODE_COUNTER_HTTP_SERVER_REQUEST     1
#define NODE_COUNTER_HTTP_SERVER_RESPONSE    2
#define NODE_COUNTER_HTTP_CLIENT_REQUEST     3
#define NODE_COUNTER_HTTP_CLIENT_RESPONSE    4
#define NODE_COUNTER_SERVER_CONNS            5
#define NODE_COUNTER_NET_BYTES_SENT          6
#define NODE_COUNTER_NET_BYTES_RECV          7
#define NODE_COUNTER_GC_PERCENTTIME          8
#define NODE_COUNTER_PIPE_BYTES_SENT         9
#define NODE_COUNTER_PIPE_BYTES_RECV         10


namespace node {


EXTERN_C DECLSPEC_SELECTANY HANDLE NodeCounterProvider = NULL;

void InitPerfCountersWin32() {
  ULONG status;
  PERF_PROVIDER_CONTEXT providerContext;

  // create instance name using pid
#define INST_MAX_LEN       32
#define INST_PREFIX_LEN    5
#define INST_PREFIX        L"node_"

  wchar_t Inst[INST_MAX_LEN];
  DWORD pid = GetCurrentProcessId();
  wcscpy_s(Inst, INST_MAX_LEN, INST_PREFIX);
  _itow_s(pid, Inst + INST_PREFIX_LEN, INST_MAX_LEN - INST_PREFIX_LEN, 10);

  advapimod = LoadLibrary("advapi32.dll");
  if (advapimod) {
    perfctr_startProvider = (PerfStartProviderExFunc)
      GetProcAddress(advapimod, "PerfStartProviderEx");
    perfctr_stopProvider = (PerfStopProviderFunc)
      GetProcAddress(advapimod, "PerfStopProvider");
    perfctr_setCounterSetInfo = (PerfSetCounterSetInfoFunc)
      GetProcAddress(advapimod, "PerfSetCounterSetInfo");
    perfctr_createInstance = (PerfCreateInstanceFunc)
      GetProcAddress(advapimod, "PerfCreateInstance");
    perfctr_deleteInstance = (PerfDeleteInstanceFunc)
      GetProcAddress(advapimod, "PerfDeleteInstance");
    perfctr_setULongValue = (PerfSetULongCounterValueFunc)
      GetProcAddress(advapimod, "PerfSetULongCounterValue");
    perfctr_setULongLongValue = (PerfSetULongLongCounterValueFunc)
      GetProcAddress(advapimod, "PerfSetULongLongCounterValue");
    perfctr_incrementULongValue = (PerfIncrementULongCounterValueFunc)
      GetProcAddress(advapimod, "PerfIncrementULongCounterValue");
    perfctr_incrementULongLongValue = (PerfIncrementULongLongCounterValueFunc)
      GetProcAddress(advapimod, "PerfIncrementULongLongCounterValue");
    perfctr_decrementULongValue = (PerfDecrementULongCounterValueFunc)
      GetProcAddress(advapimod, "PerfDecrementULongCounterValue");
    perfctr_decrementULongLongValue = (PerfDecrementULongLongCounterValueFunc)
      GetProcAddress(advapimod, "PerfDecrementULongLongCounterValue");

    ZeroMemory(&providerContext, sizeof(providerContext));
    providerContext.ContextSize = sizeof(providerContext);

    if (!perfctr_startProvider ||
        !perfctr_setCounterSetInfo ||
        !perfctr_createInstance) {
      NodeCounterProvider = NULL;
      return;
    }

    status = perfctr_startProvider(&NodeCounterSetGuid,
                                   &providerContext,
                                   &NodeCounterProvider);
    if (status != ERROR_SUCCESS) {
      NodeCounterProvider = NULL;
      return;
    }

    status = perfctr_setCounterSetInfo(NodeCounterProvider,
                                       &NodeCounterSetInfo.CounterSet,
                                       sizeof(NodeCounterSetInfo));
    if (status != ERROR_SUCCESS) {
      perfctr_stopProvider(NodeCounterProvider);
      NodeCounterProvider = NULL;
      return;
    }

    perfctr_instance = perfctr_createInstance(NodeCounterProvider,
                                              &NodeCounterSetGuid,
                                              Inst,
                                              1);
    if (perfctr_instance == NULL) {
      perfctr_stopProvider(NodeCounterProvider);
      NodeCounterProvider = NULL;
    }
  }
}


void TermPerfCountersWin32() {
  if (NodeCounterProvider != NULL &&
    perfctr_stopProvider != NULL) {
    perfctr_stopProvider(NodeCounterProvider);
    NodeCounterProvider = NULL;
  }

  if (advapimod) {
    FreeLibrary(advapimod);
    advapimod = NULL;
  }
}


void NODE_COUNT_HTTP_SERVER_REQUEST() {
  if (NodeCounterProvider != NULL && perfctr_incrementULongValue != NULL) {
    perfctr_incrementULongValue(NodeCounterProvider,
                                perfctr_instance,
                                NODE_COUNTER_HTTP_SERVER_REQUEST,
                                1);
  }
}


void NODE_COUNT_HTTP_SERVER_RESPONSE() {
  if (NodeCounterProvider != NULL && perfctr_incrementULongValue != NULL) {
    perfctr_incrementULongValue(NodeCounterProvider,
                                perfctr_instance,
                                NODE_COUNTER_HTTP_SERVER_RESPONSE,
                                1);
  }
}


void NODE_COUNT_HTTP_CLIENT_REQUEST() {
  if (NodeCounterProvider != NULL && perfctr_incrementULongValue != NULL) {
    perfctr_incrementULongValue(NodeCounterProvider,
                                perfctr_instance,
                                NODE_COUNTER_HTTP_CLIENT_REQUEST,
                                1);
  }
}


void NODE_COUNT_HTTP_CLIENT_RESPONSE() {
  if (NodeCounterProvider != NULL && perfctr_incrementULongValue != NULL) {
    perfctr_incrementULongValue(NodeCounterProvider,
                                perfctr_instance,
                                NODE_COUNTER_HTTP_CLIENT_RESPONSE,
                                1);
  }
}


void NODE_COUNT_SERVER_CONN_OPEN() {
  if (NodeCounterProvider != NULL && perfctr_incrementULongValue != NULL) {
    perfctr_incrementULongValue(NodeCounterProvider,
                                perfctr_instance,
                                NODE_COUNTER_SERVER_CONNS,
                                1);
  }
}


void NODE_COUNT_SERVER_CONN_CLOSE() {
  if (NodeCounterProvider != NULL && perfctr_decrementULongValue != NULL) {
    perfctr_decrementULongValue(NodeCounterProvider,
                                perfctr_instance,
                                NODE_COUNTER_SERVER_CONNS,
                                1);
  }
}


void NODE_COUNT_NET_BYTES_SENT(int bytes) {
  if (NodeCounterProvider != NULL && perfctr_incrementULongLongValue != NULL) {
    perfctr_incrementULongLongValue(NodeCounterProvider,
                                    perfctr_instance,
                                    NODE_COUNTER_NET_BYTES_SENT,
                                    static_cast<ULONGLONG>(bytes));
  }
}


void NODE_COUNT_NET_BYTES_RECV(int bytes) {
  if (NodeCounterProvider != NULL && perfctr_incrementULongLongValue != NULL) {
    perfctr_incrementULongLongValue(NodeCounterProvider,
                                    perfctr_instance,
                                    NODE_COUNTER_NET_BYTES_RECV,
                                    static_cast<ULONGLONG>(bytes));
  }
}


uint64_t NODE_COUNT_GET_GC_RAWTIME() {
  LARGE_INTEGER timegc;
  if (QueryPerformanceCounter(&timegc)) {
    return timegc.QuadPart;
  } else {
    return static_cast<uint64_t>(GetTickCount());
  }
}


void NODE_COUNT_GC_PERCENTTIME(unsigned int percent) {
  if (NodeCounterProvider != NULL && perfctr_setULongValue != NULL) {
    perfctr_setULongValue(NodeCounterProvider,
                          perfctr_instance,
                          NODE_COUNTER_GC_PERCENTTIME,
                          percent);
  }
}


void NODE_COUNT_PIPE_BYTES_SENT(int bytes) {
  if (NodeCounterProvider != NULL && perfctr_incrementULongLongValue != NULL) {
    perfctr_incrementULongLongValue(NodeCounterProvider,
                                    perfctr_instance,
                                    NODE_COUNTER_PIPE_BYTES_SENT,
                                    static_cast<ULONGLONG>(bytes));
  }
}


void NODE_COUNT_PIPE_BYTES_RECV(int bytes) {
  if (NodeCounterProvider != NULL && perfctr_incrementULongLongValue != NULL) {
    perfctr_incrementULongLongValue(NodeCounterProvider,
                                    perfctr_instance,
                                    NODE_COUNTER_PIPE_BYTES_RECV,
                                    static_cast<ULONGLONG>(bytes));
  }
}


}
