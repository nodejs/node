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
sRtlNtStatusToDosError pRtlNtStatusToDosError;
sNtDeviceIoControlFile pNtDeviceIoControlFile;
sNtQueryInformationFile pNtQueryInformationFile;
sNtSetInformationFile pNtSetInformationFile;
sNtQueryVolumeInformationFile pNtQueryVolumeInformationFile;
sNtQuerySystemInformation pNtQuerySystemInformation;


/* Kernel32 function pointers */
sGetQueuedCompletionStatusEx pGetQueuedCompletionStatusEx;
sSetFileCompletionNotificationModes pSetFileCompletionNotificationModes;
sCreateSymbolicLinkW pCreateSymbolicLinkW;
sCancelIoEx pCancelIoEx;
sInitializeSRWLock pInitializeSRWLock;
sAcquireSRWLockShared pAcquireSRWLockShared;
sAcquireSRWLockExclusive pAcquireSRWLockExclusive;
sTryAcquireSRWLockShared pTryAcquireSRWLockShared;
sTryAcquireSRWLockExclusive pTryAcquireSRWLockExclusive;
sReleaseSRWLockShared pReleaseSRWLockShared;
sReleaseSRWLockExclusive pReleaseSRWLockExclusive;
sInitializeConditionVariable pInitializeConditionVariable;
sSleepConditionVariableCS pSleepConditionVariableCS;
sSleepConditionVariableSRW pSleepConditionVariableSRW;
sWakeAllConditionVariable pWakeAllConditionVariable;
sWakeConditionVariable pWakeConditionVariable;


void uv_winapi_init() {
  HMODULE ntdll_module;
  HMODULE kernel32_module;

  ntdll_module = GetModuleHandleA("ntdll.dll");
  if (ntdll_module == NULL) {
    uv_fatal_error(GetLastError(), "GetModuleHandleA");
  }

  pRtlNtStatusToDosError = (sRtlNtStatusToDosError) GetProcAddress(
      ntdll_module,
      "RtlNtStatusToDosError");
  if (pRtlNtStatusToDosError == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  pNtDeviceIoControlFile = (sNtDeviceIoControlFile) GetProcAddress(
      ntdll_module,
      "NtDeviceIoControlFile");
  if (pNtDeviceIoControlFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  pNtQueryInformationFile = (sNtQueryInformationFile) GetProcAddress(
      ntdll_module,
      "NtQueryInformationFile");
  if (pNtQueryInformationFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  pNtSetInformationFile = (sNtSetInformationFile) GetProcAddress(
      ntdll_module,
      "NtSetInformationFile");
  if (pNtSetInformationFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  pNtQueryVolumeInformationFile = (sNtQueryVolumeInformationFile)
      GetProcAddress(ntdll_module, "NtQueryVolumeInformationFile");
  if (pNtQueryVolumeInformationFile == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  pNtQuerySystemInformation = (sNtQuerySystemInformation) GetProcAddress(
      ntdll_module,
      "NtQuerySystemInformation");
  if (pNtQuerySystemInformation == NULL) {
    uv_fatal_error(GetLastError(), "GetProcAddress");
  }

  kernel32_module = GetModuleHandleA("kernel32.dll");
  if (kernel32_module == NULL) {
    uv_fatal_error(GetLastError(), "GetModuleHandleA");
  }

  pGetQueuedCompletionStatusEx = (sGetQueuedCompletionStatusEx) GetProcAddress(
      kernel32_module,
      "GetQueuedCompletionStatusEx");

  pSetFileCompletionNotificationModes = (sSetFileCompletionNotificationModes)
    GetProcAddress(kernel32_module, "SetFileCompletionNotificationModes");

  pCreateSymbolicLinkW = (sCreateSymbolicLinkW)
    GetProcAddress(kernel32_module, "CreateSymbolicLinkW");

  pCancelIoEx = (sCancelIoEx)
    GetProcAddress(kernel32_module, "CancelIoEx");

  pInitializeSRWLock = (sInitializeSRWLock)
    GetProcAddress(kernel32_module, "InitializeSRWLock");

  pAcquireSRWLockShared = (sAcquireSRWLockShared)
    GetProcAddress(kernel32_module, "AcquireSRWLockShared");

  pAcquireSRWLockExclusive = (sAcquireSRWLockExclusive)
    GetProcAddress(kernel32_module, "AcquireSRWLockExclusive");

  pTryAcquireSRWLockShared = (sTryAcquireSRWLockShared)
    GetProcAddress(kernel32_module, "TryAcquireSRWLockShared");

  pTryAcquireSRWLockExclusive = (sTryAcquireSRWLockExclusive)
    GetProcAddress(kernel32_module, "TryAcquireSRWLockExclusive");

  pReleaseSRWLockShared = (sReleaseSRWLockShared)
    GetProcAddress(kernel32_module, "ReleaseSRWLockShared");

  pReleaseSRWLockExclusive = (sReleaseSRWLockExclusive)
    GetProcAddress(kernel32_module, "ReleaseSRWLockExclusive");

  pInitializeConditionVariable = (sInitializeConditionVariable)
    GetProcAddress(kernel32_module, "InitializeConditionVariable");

  pSleepConditionVariableCS = (sSleepConditionVariableCS)
    GetProcAddress(kernel32_module, "SleepConditionVariableCS");

  pSleepConditionVariableSRW = (sSleepConditionVariableSRW)
    GetProcAddress(kernel32_module, "SleepConditionVariableSRW");

  pWakeAllConditionVariable = (sWakeAllConditionVariable)
    GetProcAddress(kernel32_module, "WakeAllConditionVariable");

  pWakeConditionVariable = (sWakeConditionVariable)
    GetProcAddress(kernel32_module, "WakeConditionVariable");
}
