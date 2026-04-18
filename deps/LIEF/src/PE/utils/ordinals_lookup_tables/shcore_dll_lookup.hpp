/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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
#ifndef LIEF_PE_SHCORE_DLL_LOOKUP_H
#define LIEF_PE_SHCORE_DLL_LOOKUP_H
#include <cstdint>

namespace LIEF {
namespace PE {

inline const char* shcore_dll_lookup(uint32_t i) {
  switch(i) {
  case 0x0002: return "CommandLineToArgvW";
  case 0x0003: return "CreateRandomAccessStreamOnFile";
  case 0x0004: return "CreateRandomAccessStreamOverStream";
  case 0x0005: return "CreateStreamOverRandomAccessStream";
  case 0x0006: return "DllCanUnloadNow";
  case 0x0007: return "DllGetActivationFactory";
  case 0x0008: return "DllGetClassObject";
  case 0x0009: return "GetCurrentProcessExplicitAppUserModelID";
  case 0x000a: return "GetDpiForMonitor";
  case 0x000b: return "GetDpiForShellUIComponent";
  case 0x000c: return "GetFeatureEnabledState";
  case 0x000d: return "GetProcessDpiAwareness";
  case 0x000e: return "GetProcessReference";
  case 0x000f: return "GetScaleFactorForDevice";
  case 0x0010: return "GetScaleFactorForMonitor";
  case 0x0011: return "IStream_Copy";
  case 0x0012: return "IStream_Read";
  case 0x0013: return "IStream_ReadStr";
  case 0x0014: return "IStream_Reset";
  case 0x0015: return "IStream_Size";
  case 0x0016: return "IStream_Write";
  case 0x0017: return "IStream_WriteStr";
  case 0x0018: return "IUnknown_AtomicRelease";
  case 0x0019: return "IUnknown_GetSite";
  case 0x001a: return "IUnknown_QueryService";
  case 0x001b: return "IUnknown_Set";
  case 0x001c: return "IUnknown_SetSite";
  case 0x001d: return "IsOS";
  case 0x001e: return "RecordFeatureError";
  case 0x001f: return "RecordFeatureUsage";
  case 0x0020: return "RegisterScaleChangeEvent";
  case 0x0021: return "RegisterScaleChangeNotifications";
  case 0x0022: return "RevokeScaleChangeNotifications";
  case 0x0023: return "SHAnsiToAnsi";
  case 0x0024: return "SHAnsiToUnicode";
  case 0x0025: return "SHCopyKeyA";
  case 0x0026: return "SHCopyKeyW";
  case 0x0027: return "SHCreateMemStream";
  case 0x0028: return "SHCreateStreamOnFileA";
  case 0x0029: return "SHCreateStreamOnFileEx";
  case 0x002a: return "SHCreateStreamOnFileW";
  case 0x002b: return "SHCreateThread";
  case 0x002c: return "SHCreateThreadRef";
  case 0x002d: return "SHCreateThreadWithHandle";
  case 0x002e: return "SHDeleteEmptyKeyA";
  case 0x002f: return "SHDeleteEmptyKeyW";
  case 0x0030: return "SHDeleteKeyA";
  case 0x0031: return "SHDeleteKeyW";
  case 0x0032: return "SHDeleteValueA";
  case 0x0033: return "SHDeleteValueW";
  case 0x0034: return "SHEnumKeyExA";
  case 0x0035: return "SHEnumKeyExW";
  case 0x0036: return "SHEnumValueA";
  case 0x0037: return "SHEnumValueW";
  case 0x0038: return "SHGetThreadRef";
  case 0x0039: return "SHGetValueA";
  case 0x003a: return "SHGetValueW";
  case 0x003b: return "SHOpenRegStream2A";
  case 0x003c: return "SHOpenRegStream2W";
  case 0x003d: return "SHOpenRegStreamA";
  case 0x003e: return "SHOpenRegStreamW";
  case 0x003f: return "SHQueryInfoKeyA";
  case 0x0040: return "SHQueryInfoKeyW";
  case 0x0041: return "SHQueryValueExA";
  case 0x0042: return "SHQueryValueExW";
  case 0x0043: return "SHRegDuplicateHKey";
  case 0x0044: return "SHRegGetIntW";
  case 0x0045: return "SHRegGetPathA";
  case 0x0046: return "SHRegGetPathW";
  case 0x0047: return "SHRegGetValueA";
  case 0x007a: return "SHRegGetValueFromHKCUHKLM";
  case 0x0048: return "SHRegGetValueW";
  case 0x0049: return "SHRegSetPathA";
  case 0x004a: return "SHRegSetPathW";
  case 0x004b: return "SHReleaseThreadRef";
  case 0x004c: return "SHSetThreadRef";
  case 0x004d: return "SHSetValueA";
  case 0x004e: return "SHSetValueW";
  case 0x004f: return "SHStrDupA";
  case 0x0050: return "SHStrDupW";
  case 0x0051: return "SHUnicodeToAnsi";
  case 0x0052: return "SHUnicodeToUnicode";
  case 0x0053: return "SetCurrentProcessExplicitAppUserModelID";
  case 0x0054: return "SetProcessDpiAwareness";
  case 0x0055: return "SetProcessReference";
  case 0x0056: return "SubscribeFeatureStateChangeNotification";
  case 0x0057: return "UnregisterScaleChangeEvent";
  case 0x0058: return "UnsubscribeFeatureStateChangeNotification";
  }
  return nullptr;
}


}
}

#endif

