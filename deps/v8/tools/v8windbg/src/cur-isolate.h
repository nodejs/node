// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TOOLS_V8WINDBG_SRC_CUR_ISOLATE_H_
#define V8_TOOLS_V8WINDBG_SRC_CUR_ISOLATE_H_

#include <crtdbg.h>
#include <wrl/implements.h>

#include <string>
#include <vector>

#include "tools/v8windbg/base/utilities.h"
#include "tools/v8windbg/src/v8-debug-helper-interop.h"
#include "tools/v8windbg/src/v8windbg-extension.h"

HRESULT GetCurrentIsolate(WRL::ComPtr<IModelObject>& sp_result);

constexpr wchar_t kIsolateKey[] = L"v8::internal::Isolate::isolate_key_";
constexpr wchar_t kIsolate[] = L"v8::internal::Isolate";

class CurrIsolateAlias
    : public WRL::RuntimeClass<
          WRL::RuntimeClassFlags<WRL::RuntimeClassType::ClassicCom>,
          IModelMethod> {
 public:
  IFACEMETHOD(Call)
  (IModelObject* p_context_object, ULONG64 arg_count,
   _In_reads_(arg_count) IModelObject** pp_arguments, IModelObject** pp_result,
   IKeyStore** pp_metadata);
};

#endif  // V8_TOOLS_V8WINDBG_SRC_CUR_ISOLATE_H_
