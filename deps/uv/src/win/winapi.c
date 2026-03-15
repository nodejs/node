/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>

#include "uv.h"
#include "internal.h"


/* Ntdll function pointers */
sRtlGetVersion pRtlGetVersion;
sRtlNtStatusToDosError pRtlNtStatusToDosError;
sNtDeviceIoControlFile pNtDeviceIoControlFile;
sNtQueryInformationFile pNtQueryInformationFile;
sNtSetInformationFile pNtSetInformationFile;
sNtQueryVolumeInformationFile pNtQueryVolumeInformationFile;
sNtQueryDirectoryFile pNtQueryDirectoryFile;
sNtQuerySystemInformation pNtQuerySystemInformation;
sNtQueryInformationProcess pNtQueryInformationProcess;

/* Powrprof.dll function pointer */
sPowerRegisterSuspendResumeNotification pPowerRegisterSuspendResumeNotification;

/* bcryptprimitives.dll function pointer */
sProcessPrng pProcessPrng;

/* User32.dll function pointer */
sSetWinEventHook pSetWinEventHook;

/* ws2_32.dll function pointer */
uv_sGetHostNameW pGetHostNameW;

/* api-ms-win-core-file-l2-1-4.dll function pointer */
sGetFileInformationByName pGetFileInformationByName;

void uv__winapi_init(void) {
  HMODULE ntdll_module;
  HMODULE powrprof_module;
  HMODULE user32_module;
  HMODULE ws2_32_module;
  HMODULE bcryptprimitives_module;
  HMODULE api_win_core_file_module;

  union {
    FARPROC proc;
    sRtlGetVersion pRtlGetVersion;
    sRtlNtStatusToDosError pRtlNtStatusToDosError;
    sNtDeviceIoControlFile pNtDeviceIoControlFile;
    sNtQueryInformationFile pNtQueryInformationFile;
    sNtSetInformationFile pNtSetInformationFile;
    sNtQueryVolumeInformationFile pNtQueryVolumeInformationFile;
    sNtQueryDirectoryFile pNtQueryDirectoryFile;
    sNtQuerySystemInformation pNtQuerySystemInformation;
    sNtQueryInformationProcess pNtQueryInformationProcess;
    sPowerRegisterSuspendResumeNotification pPowerRegisterSuspendResumeNotification;
    sProcessPrng pProcessPrng;
    sSetWinEventHook pSetWinEventHook;
    uv_sGetHostNameW pGetHostNameW;
    sGetFileInformationByName pGetFileInformationByName;
  } u;

  ntdll_module = GetModuleHandleW(L"ntdll.dll");
  if (ntdll_module == NULL) {
    uv_fatal_error(GetLastError(), "GetModuleHandleW");
  }

  u.proc = GetProcAddress(ntdll_module, "RtlGetVersion");
  pRtlGetVersion = u.pRtlGetVersion;

  u.proc = GetProcAddress(ntdll_module, "RtlNtStatusToDosError");
  pRtlNtStatusToDosError = u.pRtlNtStatusToDosError;
  if (pRtlNtStatusToDosError == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  u.proc = GetProcAddress(ntdll_module, "NtDeviceIoControlFile");
  pNtDeviceIoControlFile = u.pNtDeviceIoControlFile;
  if (pNtDeviceIoControlFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  u.proc = GetProcAddress(ntdll_module, "NtQueryInformationFile");
  pNtQueryInformationFile = u.pNtQueryInformationFile;
  if (pNtQueryInformationFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  u.proc = GetProcAddress(ntdll_module, "NtSetInformationFile");
  pNtSetInformationFile = u.pNtSetInformationFile;
  if (pNtSetInformationFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  u.proc = GetProcAddress(ntdll_module, "NtQueryVolumeInformationFile");
  pNtQueryVolumeInformationFile = u.pNtQueryVolumeInformationFile;
  if (pNtQueryVolumeInformationFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  u.proc = GetProcAddress(ntdll_module, "NtQueryDirectoryFile");
  pNtQueryDirectoryFile = u.pNtQueryDirectoryFile;
  if (pNtQueryDirectoryFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  u.proc = GetProcAddress(ntdll_module, "NtQuerySystemInformation");
  pNtQuerySystemInformation = u.pNtQuerySystemInformation;
  if (pNtQuerySystemInformation == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  u.proc = GetProcAddress(ntdll_module, "NtQueryInformationProcess");
  pNtQueryInformationProcess = u.pNtQueryInformationProcess;
  if (pNtQueryInformationProcess == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  powrprof_module = LoadLibraryExA("powrprof.dll",
                                   NULL,
                                   LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (powrprof_module != NULL) {
    u.proc = GetProcAddress(powrprof_module,
                            "PowerRegisterSuspendResumeNotification");
    pPowerRegisterSuspendResumeNotification =
        u.pPowerRegisterSuspendResumeNotification;
  }

  bcryptprimitives_module = LoadLibraryExA("bcryptprimitives.dll",
                                           NULL,
                                           LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (bcryptprimitives_module != NULL) {
    u.proc = GetProcAddress(bcryptprimitives_module, "ProcessPrng");
    pProcessPrng = u.pProcessPrng;
  }

  user32_module = GetModuleHandleW(L"user32.dll");
  if (user32_module != NULL) {
    u.proc = GetProcAddress(user32_module, "SetWinEventHook");
    pSetWinEventHook = u.pSetWinEventHook;
  }

  ws2_32_module = GetModuleHandleW(L"ws2_32.dll");
  if (ws2_32_module != NULL) {
    u.proc = GetProcAddress(ws2_32_module, "GetHostNameW");
    pGetHostNameW = u.pGetHostNameW;
  }

  api_win_core_file_module =
      GetModuleHandleW(L"api-ms-win-core-file-l2-1-4.dll");
  if (api_win_core_file_module != NULL) {
    u.proc = GetProcAddress(api_win_core_file_module,
                            "GetFileInformationByName");
    pGetFileInformationByName = u.pGetFileInformationByName;
  }
}
