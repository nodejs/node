// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#undef WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
// shellapi.h requires windows.h to be included first.
#include <shellapi.h>
// clang-format on

#include <vector>

#include "src/base/platform/platform-win32.h"
#include "src/base/platform/platform.h"
#include "src/d8/d8.h"

namespace v8 {

// Save the allocated utf8 filenames, and we will free them when exit.
static std::vector<char*> utf8_filenames;

// Convert utf-16 encoded string to utf-8 encoded.
static char* ConvertUtf16StringToUtf8(const wchar_t* str) {
  // On Windows wchar_t must be a 16-bit value.
  static_assert(sizeof(wchar_t) == 2, "wrong wchar_t size");
  int len =
      WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, FALSE);
  DCHECK_LT(0, len);
  char* utf8_str = new char[len];
  utf8_filenames.push_back(utf8_str);
  WideCharToMultiByte(CP_UTF8, 0, str, -1, utf8_str, len, nullptr, FALSE);
  return utf8_str;
}

void Shell::PreProcessUnicodeFilenameArg(char* argv[], int i) {
  int argc;
  wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
  argv[i] = ConvertUtf16StringToUtf8(wargv[i]);
  LocalFree(wargv);
}

void Shell::AddOSMethods(Isolate* isolate, Local<ObjectTemplate> os_templ) {}

base::OwnedVector<char> Shell::ReadCharsFromTcpPort(const char* name) {
  // TODO(leszeks): No reason this shouldn't exist on windows.
  return {};
}

void Shell::FileExists(const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(i::ValidateCallbackInfo(info));
  Isolate* isolate = info.GetIsolate();
  if (info.Length() < 1) {
    isolate->ThrowError("exists() takes one argument");
    return;
  }
  String::Utf8Value file_name(isolate, info[0]);
  if (*file_name == nullptr) {
    isolate->ThrowError(
        "d8.file.exists(): String conversion of argument failed.");
    return;
  }

  std::wstring wide_name = v8::base::OS::ConvertUtf8StringToUtf16(*file_name);
  DWORD attr = GetFileAttributesW(wide_name.c_str());
  bool exists = (attr != INVALID_FILE_ATTRIBUTES);

  info.GetReturnValue().Set(v8::Boolean::New(isolate, exists));
}

bool Shell::ChangeWorkingDirectory(const std::string& path, bool print_error) {
  std::wstring wide_path = v8::base::OS::ConvertUtf8StringToUtf16(path.c_str());
  bool success = SetCurrentDirectoryW(wide_path.c_str()) != 0;

  if (!success && print_error) {
    fprintf(stderr, "Failed to change directory to %s\n", path.c_str());
  }
  return success;
}

void Shell::FreeUnicodeFilenameArgs() {
  for (char* utf8_str : utf8_filenames) {
    delete[] utf8_str;
  }
  utf8_filenames.clear();
}

}  // namespace v8
