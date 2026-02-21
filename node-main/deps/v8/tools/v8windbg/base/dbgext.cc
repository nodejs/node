// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/v8windbg/base/dbgext.h"

#include <crtdbg.h>
#include <wrl/module.h>

#include "tools/v8windbg/base/utilities.h"

// See
// https://docs.microsoft.com/en-us/visualstudio/debugger/crt-debugging-techniques
// for the memory leak and debugger reporting macros used from <crtdbg.h>
_CrtMemState mem_old, mem_new, mem_diff;
int original_crt_dbg_flag = 0;

WRL::ComPtr<IDataModelManager> sp_data_model_manager;
WRL::ComPtr<IDebugHost> sp_debug_host;
WRL::ComPtr<IDebugControl5> sp_debug_control;
WRL::ComPtr<IDebugHostMemory2> sp_debug_host_memory;
WRL::ComPtr<IDebugHostSymbols> sp_debug_host_symbols;
WRL::ComPtr<IDebugHostExtensibility> sp_debug_host_extensibility;

extern "C" {

HRESULT
__stdcall DebugExtensionInitialize(PULONG /*pVersion*/, PULONG /*pFlags*/) {
  original_crt_dbg_flag = _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
  _CrtMemCheckpoint(&mem_old);

  WRL::ComPtr<IDebugClient> sp_debug_client;
  WRL::ComPtr<IHostDataModelAccess> sp_data_model_access;

  RETURN_IF_FAIL(DebugCreate(__uuidof(IDebugClient), &sp_debug_client));

  RETURN_IF_FAIL(sp_debug_client.As(&sp_data_model_access));
  RETURN_IF_FAIL(sp_debug_client.As(&sp_debug_control));

  RETURN_IF_FAIL(sp_data_model_access->GetDataModel(&sp_data_model_manager,
                                                    &sp_debug_host));

  RETURN_IF_FAIL(sp_debug_host.As(&sp_debug_host_memory));
  RETURN_IF_FAIL(sp_debug_host.As(&sp_debug_host_symbols));
  RETURN_IF_FAIL(sp_debug_host.As(&sp_debug_host_extensibility));

  return CreateExtension();
}

void __stdcall DebugExtensionUninitialize() {
  DestroyExtension();
  sp_debug_host = nullptr;
  sp_data_model_manager = nullptr;
  sp_debug_host_memory = nullptr;
  sp_debug_host_symbols = nullptr;
  sp_debug_host_extensibility = nullptr;

  _CrtMemCheckpoint(&mem_new);
  if (_CrtMemDifference(&mem_diff, &mem_old, &mem_new)) {
    _CrtMemDumpStatistics(&mem_diff);
  }
  _CrtSetDbgFlag(original_crt_dbg_flag);
}

HRESULT __stdcall DebugExtensionCanUnload(void) {
  if (!WRL::Module<WRL::InProc>::GetModule().Terminate()) {
    _RPTF0(_CRT_WARN, "Failed to unload WRL\n");
    return S_FALSE;
  }
  return S_OK;
}

void __stdcall DebugExtensionUnload() { return; }

}  // extern "C"
