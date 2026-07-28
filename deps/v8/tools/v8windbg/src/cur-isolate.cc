// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/v8windbg/src/cur-isolate.h"

HRESULT GetIsolateLocation(WRL::ComPtr<IDebugHostContext>& sp_ctx,
                           Location* location) {
  auto sp_v8_module = Extension::Current()->GetV8Module(sp_ctx);
  if (sp_v8_module == nullptr) return E_FAIL;

  WRL::ComPtr<IDebugHostSymbol> sp_isolate_sym;
  RETURN_IF_FAIL(
      sp_v8_module->FindSymbolByName(kIsolateOffset, &sp_isolate_sym));
  SymbolKind kind;
  RETURN_IF_FAIL(sp_isolate_sym->GetSymbolKind(&kind));
  if (kind != SymbolData) return E_FAIL;
  WRL::ComPtr<IDebugHostData> sp_isolate_key_data;
  RETURN_IF_FAIL(sp_isolate_sym.As(&sp_isolate_key_data));
  RETURN_IF_FAIL(sp_isolate_key_data->GetLocation(location));
  return S_OK;
}

HRESULT GetCurrentIsolate(WRL::ComPtr<IModelObject>& sp_result) {
  sp_result = nullptr;

  // Get the current context
  WRL::ComPtr<IDebugHostContext> sp_host_context;
  RETURN_IF_FAIL(sp_debug_host->GetCurrentContext(&sp_host_context));

  Location isolate_addr;
  RETURN_IF_FAIL(GetIsolateLocation(sp_host_context, &isolate_addr));

  // If we got the isolate_key OK, then must have the V8 module loaded
  // Get the internal Isolate type from it
  WRL::ComPtr<IDebugHostType> sp_isolate_type, sp_isolate_ptr_type;
  RETURN_IF_FAIL(Extension::Current()
                     ->GetV8Module(sp_host_context)
                     ->FindTypeByName(kIsolate, &sp_isolate_type));
  RETURN_IF_FAIL(
      sp_isolate_type->CreatePointerTo(PointerStandard, &sp_isolate_ptr_type));

  RETURN_IF_FAIL(sp_data_model_manager->CreateTypedObject(
      sp_host_context.Get(), isolate_addr, sp_isolate_type.Get(), &sp_result));

  return S_OK;
}

IFACEMETHODIMP CurrIsolateAlias::Call(IModelObject* p_context_object,
                                      ULONG64 arg_count,
                                      IModelObject** pp_arguments,
                                      IModelObject** pp_result,
                                      IKeyStore** pp_metadata) noexcept {
  *pp_result = nullptr;
  WRL::ComPtr<IModelObject> sp_result;
  RETURN_IF_FAIL(GetCurrentIsolate(sp_result));
  *pp_result = sp_result.Detach();
  return S_OK;
}
