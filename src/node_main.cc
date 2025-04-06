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

#include "node.h"
#include <cstdio>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <VersionHelpers.h>
#include <WinError.h>

namespace {
constexpr const char* SKIP_CHECK_VAR = "NODE_SKIP_PLATFORM_CHECK";
constexpr const char* SKIP_CHECK_VALUE = "1";
constexpr size_t SKIP_CHECK_STRLEN = sizeof(SKIP_CHECK_VALUE) - 1;

// Custom error codes
enum class NodeErrorCode {
  SUCCESS = 0,
  PLATFORM_CHECK_FAILED = ERROR_EXE_MACHINE_TYPE_MISMATCH,
  ARGUMENT_CONVERSION_FAILED = 2
};

bool CheckWindowsVersion() {
  char buf[SKIP_CHECK_STRLEN + 1];
  if (IsWindows10OrGreater()) return true;
  
  return (GetEnvironmentVariableA(SKIP_CHECK_VAR, buf, sizeof(buf)) == SKIP_CHECK_STRLEN &&
          strncmp(buf, SKIP_CHECK_VALUE, SKIP_CHECK_STRLEN) == 0);
}

void PrintPlatformError() {
  fprintf(stderr,
          "Node.js is only supported on Windows 10, Windows Server 2016, or higher.\n"
          "Setting the %s environment variable to 1 skips this check, but Node.js\n"
          "might not execute correctly. Any issues encountered on unsupported\n"
          "platforms will not be fixed.\n",
          SKIP_CHECK_VAR);
}

std::unique_ptr<char[]> ConvertWideToUTF8(const wchar_t* wide_str) {
  // Compute the size of the required buffer
  DWORD size = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, nullptr, 0, nullptr, nullptr);
  if (size == 0) {
    fprintf(stderr, "Error: Could not determine UTF-8 buffer size for argument conversion.\n");
    exit(static_cast<int>(NodeErrorCode::ARGUMENT_CONVERSION_FAILED));
  }

  // Allocate and convert
  auto utf8_str = std::make_unique<char[]>(size);
  DWORD result = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, utf8_str.get(), size, nullptr, nullptr);
  if (result == 0) {
    fprintf(stderr, "Error: Failed to convert command line argument to UTF-8.\n");
    exit(static_cast<int>(NodeErrorCode::ARGUMENT_CONVERSION_FAILED));
  }

  return utf8_str;
}
}  // namespace

int wmain(int argc, wchar_t* wargv[]) {
  // Check Windows version compatibility
  if (!CheckWindowsVersion()) {
    PrintPlatformError();
    return static_cast<int>(NodeErrorCode::PLATFORM_CHECK_FAILED);
  }

  // Convert argv to UTF8 using RAII
  std::vector<std::unique_ptr<char[]>> arg_storage;
  std::vector<char*> argv;
  
  arg_storage.reserve(argc);
  argv.reserve(argc + 1);

  for (int i = 0; i < argc; i++) {
    auto utf8_arg = ConvertWideToUTF8(wargv[i]);
    argv.push_back(utf8_arg.get());
    arg_storage.push_back(std::move(utf8_arg));
  }
  argv.push_back(nullptr);

  // Start Node.js
  return node::Start(argc, argv.data());
}
#else
// UNIX
int main(int argc, char* argv[]) {
  return node::Start(argc, argv);
}
#endif
