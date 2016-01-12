// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "v8.h"
#include "v8chakra.h"
#include "jsrtutils.h"
#include "v8-debug.h"
#include <algorithm>

namespace v8 {

bool g_exposeGC = false;
bool g_useStrict = false;
ArrayBuffer::Allocator* g_arrayBufferAllocator = nullptr;

const char *V8::GetVersion() {
  static char versionStr[32] = {};

  if (versionStr[0] == '\0') {
    HMODULE hModule;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          TEXT(NODE_ENGINE), &hModule)) {
      WCHAR filename[_MAX_PATH];
      DWORD len = GetModuleFileNameW(hModule, filename, _countof(filename));
      if (len > 0) {
        DWORD dwHandle = 0;
        DWORD size = GetFileVersionInfoSizeExW(0, filename, &dwHandle);
        if (size > 0) {
          std::unique_ptr<BYTE[]> info(new BYTE[size]);
          if (GetFileVersionInfoExW(0, filename, dwHandle, size, info.get())) {
            UINT len = 0;
            VS_FIXEDFILEINFO* vsfi = nullptr;
            if (VerQueryValueW(info.get(),
                               L"\\", reinterpret_cast<LPVOID*>(&vsfi), &len)) {
              sprintf_s(versionStr, "%d.%d.%d.%d",
                        HIWORD(vsfi->dwFileVersionMS),
                        LOWORD(vsfi->dwFileVersionMS),
                        HIWORD(vsfi->dwFileVersionLS),
                        LOWORD(vsfi->dwFileVersionLS));
            }
          }
        }
      }
    }
  }

  return versionStr;
}

void Isolate::SetFatalErrorHandler(FatalErrorCallback that) {
  // CONSIDER: Ignoring for now, since we don't have an equivalent concept.
}

void V8::SetFlagsFromString(const char* str, int length) {
  // CHAKRA-TODO
}

static bool equals(const char* str, const char* pat) {
  return strcmp(str, pat) == 0;
}

template <size_t N>
static bool startsWith(const char* str, const char (&prefix)[N]) {
  return strncmp(str, prefix, N - 1) == 0;
}

void V8::SetFlagsFromCommandLine(int *argc, char **argv, bool remove_flags) {
  for (int i = 1; i < *argc; i++) {
    // Note: Node now exits on invalid options. We may not recognize V8 flags
    // and fail here, causing Node to exit.
    char *arg = argv[i];
    if (equals("--expose-gc", arg) || equals("--expose_gc", arg)) {
      g_exposeGC = true;
      if (remove_flags) {
        argv[i] = nullptr;
      }
    } else if (equals("--use-strict", arg) || equals("--use_strict", arg)) {
      g_useStrict = true;
      if (remove_flags) {
        argv[i] = nullptr;
      }
    } else if (remove_flags &&
               (startsWith(
                 arg, "--debug")  // Ignore some flags to reduce unit test noise
                || startsWith(arg, "--harmony")
                || startsWith(arg, "--stack-size="))) {
      argv[i] = nullptr;
    }
  }

  if (remove_flags) {
    char** end = std::remove(argv + 1, argv + *argc, nullptr);
    *argc = end - argv;
  }
}

bool V8::Initialize() {
#ifndef NODE_ENGINE_CHAKRACORE
  if (g_EnableDebug && JsStartDebugging() != JsNoError) {
    return false;
  }
#endif
  return true;
}

void V8::SetEntropySource(EntropySource entropy_source) {
  // CHAKRA-TODO
}

void V8::SetArrayBufferAllocator(ArrayBuffer::Allocator* allocator) {
  CHAKRA_VERIFY(!g_arrayBufferAllocator);
  g_arrayBufferAllocator = allocator;
}

bool V8::IsDead() {
  // CHAKRA-TODO
  return false;
}

bool V8::Dispose() {
  jsrt::IsolateShim::DisposeAll();
  Debug::Dispose();
  return true;
}

void V8::TerminateExecution(Isolate* isolate) {
  jsrt::IsolateShim::FromIsolate(isolate)->DisableExecution();
}

bool V8::IsExeuctionDisabled(Isolate* isolate) {
  return jsrt::IsolateShim::FromIsolate(isolate)->IsExeuctionDisabled();
}

void V8::CancelTerminateExecution(Isolate* isolate) {
  jsrt::IsolateShim::FromIsolate(isolate)->EnableExecution();
}

void V8::FromJustIsNothing() {
  jsrt::Fatal("v8::FromJust: %s", "Maybe value is Nothing.");
}

void V8::ToLocalEmpty() {
  jsrt::Fatal("v8::ToLocalChecked: %s", "Empty MaybeLocal.");
}

}  // namespace v8
