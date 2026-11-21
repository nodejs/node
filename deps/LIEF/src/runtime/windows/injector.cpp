/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "LIEF/runtime/windows/injector.hpp"
#include "LIEF/utils.hpp"

#include "logging.hpp"

#include <windows.h>
#include <WinBase.h>

namespace LIEF::runtime::windows {

bool injection_context_t::validate() const {
  return true;
}

std::string injection_context_t::to_string() const {
  return "";
}

static ok_error_t inject_library(const PROCESS_INFORMATION& pi,
                                 const injection_context_t& ctx) {
  auto u16library = u8tou16(ctx.library);
  if (!u16library) {
    LIEF_ERR("Can't convert '{}' into an utf-16 string", ctx.library);
    return make_error_code(u16library.error());
  }

  auto LoadLibraryWp =
      reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<void*>(
          GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")
      ));

  if (LoadLibraryWp == nullptr) {
    LIEF_ERR("Can't resolve 'LoadLibraryW'");
    return make_error_code(lief_errors::runtime_error);
  }

  LPVOID proc_lib_buffer =
      VirtualAllocEx(pi.hProcess, /*lpAddress=*/NULL, MAX_PATH,
                     MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  if (proc_lib_buffer == nullptr) {
    LIEF_ERR("Can't allocated {} bytes in the created process", MAX_PATH);
    return make_error_code(lief_errors::runtime_error);
  }
  size_t bytes_written = 0;
  int ret =
      WriteProcessMemory(pi.hProcess, proc_lib_buffer,
                         reinterpret_cast<const wchar_t*>(u16library->data()),
                         (u16library->size() + 1) * sizeof(char16_t),
                         &bytes_written);

  if (ret == 0) {
    LIEF_ERR("Failed to write '{}' into {}", ctx.library, pi.dwProcessId);
    return make_error_code(lief_errors::runtime_error);
  }

  HANDLE loadlibrary_thread = CreateRemoteThread(
      pi.hProcess, /*lpThreadAttributes=*/NULL, /*dwStackSize=*/0,
      /*lpStartAddress=*/(LPTHREAD_START_ROUTINE)LoadLibraryWp, proc_lib_buffer,
      /*dwCreationFlags=*/0, /*lpThreadId=*/NULL
  );

  if (loadlibrary_thread == 0) {
    LIEF_ERR("Failed to create remote thread ({})", GetLastError());
    return make_error_code(lief_errors::runtime_error);
  }

  if (WaitForSingleObject(loadlibrary_thread, INFINITE) == WAIT_FAILED) {
    LIEF_ERR("Failed to wait remote thread ({})", GetLastError());
    return make_error_code(lief_errors::runtime_error);
  }

  return ok();
}

std::u16string env_string(const injection_context_t& ctx) {
  std::string out;

  if (ctx.env.empty()) {
    return u"";
  }

  for (const auto& [k, v] : ctx.env) {
    if (k.empty() || v.empty()) {
      LIEF_DEBUG("Error: {}:{} Skipping value={}, key={}", __FUNCTION__, __LINE__,
                 k, v);
      continue;
    }

    out += std::move(k) + '=' + std::move(v) + '\0';
  }

  if (out.empty()) {
    return u"";
  }


  if (auto u16 = u8tou16(out)) {
    return std::move(*u16) + u"\0\0";
  }

  return u"";
}

ok_error_t inject_spawn(const injection_context_t& ctx) {
  LIEF_DEBUG("{}: target='{}', library='{}', args={}", __FUNCTION__,
             ctx.target_path, ctx.library, ctx.args);

  if (!ctx.validate()) {
    return make_error_code(lief_errors::inconsistent);
  }

  auto u16target = u8tou16(ctx.target_path);
  if (!u16target) {
    LIEF_ERR("Can't convert '{}' into an utf-16 string", ctx.target_path);
    return make_error_code(u16target.error());
  }

  auto u16args = u8tou16(ctx.args);
  if (!u16args) {
    LIEF_ERR("Can't convert '{}' into an utf-16 string", ctx.args);
    return make_error_code(u16args.error());
  }

  std::u16string env;
  if (!ctx.env.empty()) {
    env = env_string(ctx);
    // LIEF_DEBUG("Env:\n{}", dump((const uint8_t*)env.data(), env.size() *
    // sizeof(char16_t)));
  }

  STARTUPINFOW si = {};
  PROCESS_INFORMATION pi = {};

  si.cb = sizeof(STARTUPINFOW);

  DWORD dwCreationFlags = CREATE_SUSPENDED;

  if (!env.empty()) {
    dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
  }

  BOOL ret = CreateProcessW(
      /*lpApplicationName=*/u16target->empty() ?
          NULL :
          reinterpret_cast<const wchar_t*>(u16target->c_str()),
      /*lpCommandLine=*/u16args->empty() ?
          NULL :
          reinterpret_cast<wchar_t*>(u16args->data()),
      /*lpProcessAttributes=*/NULL,
      /*lpThreadAttributes=*/NULL,
      /*bInheritHandles=*/true, dwCreationFlags,
      /*lpEnvironment=*/env.empty() ? NULL : env.data(),
      /*lpCurrentDirectory=*/NULL,
      /*lpStartupInfo=*/&si,
      /*lpProcessInformation=*/&pi
  );

  if (!ret) {
    LIEF_ERR("CreateProcessW() failed with error: {}", GetLastError());
    return make_error_code(lief_errors::runtime_error);
  }

  LIEF_DEBUG("'{} {}' created: pid: {}, tid: {}", ctx.target_path, ctx.args,
             pi.dwProcessId, pi.dwThreadId);

  if (!ctx.library.empty()) {
    auto is_ok = inject_library(pi, ctx);
    if (!is_ok) {
      return make_error_code(is_ok.error());
    }
  }


  if (int ret = ResumeThread(pi.hThread); ret < 0) {
    LIEF_ERR("ResumeThread() failed with error: {}", GetLastError());
    return make_error_code(lief_errors::runtime_error);
  }

  if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED) {
    LIEF_ERR("WaitForSingleObject failed with error: {}", GetLastError());
    return make_error_code(lief_errors::runtime_error);
  }

  // CloseHandle(pi.hProcess);
  // CloseHandle(pi.hThread);

  return ok();
}
}
