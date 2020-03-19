// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/v8windbg/test/debug-callbacks.h"

namespace v8 {
namespace internal {
namespace v8windbg_test {

MyOutput::MyOutput(WRL::ComPtr<IDebugClient5> p_client) : p_client_(p_client) {
  p_client_->SetOutputCallbacks(this);
}

MyOutput::~MyOutput() { p_client_->SetOutputCallbacks(nullptr); }

HRESULT __stdcall MyOutput::QueryInterface(REFIID InterfaceId,
                                           PVOID* Interface) {
  return E_NOTIMPL;
}
ULONG __stdcall MyOutput::AddRef(void) { return 0; }
ULONG __stdcall MyOutput::Release(void) { return 0; }
HRESULT __stdcall MyOutput::Output(ULONG Mask, PCSTR Text) {
  if (Mask & DEBUG_OUTPUT_NORMAL) {
    log_ += Text;
  }
  return S_OK;
}

HRESULT __stdcall MyCallback::QueryInterface(REFIID InterfaceId,
                                             PVOID* Interface) {
  return E_NOTIMPL;
}
ULONG __stdcall MyCallback::AddRef(void) { return S_OK; }
ULONG __stdcall MyCallback::Release(void) { return S_OK; }
HRESULT __stdcall MyCallback::GetInterestMask(PULONG Mask) {
  *Mask = DEBUG_EVENT_BREAKPOINT | DEBUG_EVENT_CREATE_PROCESS;
  return S_OK;
}
HRESULT __stdcall MyCallback::Breakpoint(PDEBUG_BREAKPOINT Bp) {
  ULONG64 bp_offset;
  HRESULT hr = Bp->GetOffset(&bp_offset);
  if (FAILED(hr)) return hr;

  // Break on breakpoints? Seems reasonable.
  return DEBUG_STATUS_BREAK;
}
HRESULT __stdcall MyCallback::Exception(PEXCEPTION_RECORD64 Exception,
                                        ULONG FirstChance) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::CreateThread(ULONG64 Handle, ULONG64 DataOffset,
                                           ULONG64 StartOffset) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::ExitThread(ULONG ExitCode) { return E_NOTIMPL; }
HRESULT __stdcall MyCallback::ExitProcess(ULONG ExitCode) { return E_NOTIMPL; }
HRESULT __stdcall MyCallback::LoadModule(ULONG64 ImageFileHandle,
                                         ULONG64 BaseOffset, ULONG ModuleSize,
                                         PCSTR ModuleName, PCSTR ImageName,
                                         ULONG CheckSum, ULONG TimeDateStamp) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::UnloadModule(PCSTR ImageBaseName,
                                           ULONG64 BaseOffset) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::SystemError(ULONG Error, ULONG Level) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::SessionStatus(ULONG Status) { return E_NOTIMPL; }
HRESULT __stdcall MyCallback::ChangeDebuggeeState(ULONG Flags,
                                                  ULONG64 Argument) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::ChangeEngineState(ULONG Flags, ULONG64 Argument) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::ChangeSymbolState(ULONG Flags, ULONG64 Argument) {
  return E_NOTIMPL;
}
HRESULT __stdcall MyCallback::CreateProcessW(
    ULONG64 ImageFileHandle, ULONG64 Handle, ULONG64 BaseOffset,
    ULONG ModuleSize, PCSTR ModuleName, PCSTR ImageName, ULONG CheckSum,
    ULONG TimeDateStamp, ULONG64 InitialThreadHandle, ULONG64 ThreadDataOffset,
    ULONG64 StartOffset) {
  // Should fire once the target process is launched. Break to create
  // breakpoints, etc.
  return DEBUG_STATUS_BREAK;
}

}  // namespace v8windbg_test
}  // namespace internal
}  // namespace v8
