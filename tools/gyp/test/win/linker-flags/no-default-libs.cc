// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reference something in kernel32.dll. This will fail to link, verifying that
// GYP provides no default import library configuration.
// Note that we don't include Windows.h, as that will result in generating
// linker directives in the object file through #pragma comment(lib, ...).
typedef short BOOL;

extern "C" __declspec(dllimport)
BOOL CopyFileW(const wchar_t*, const wchar_t*, BOOL);


int main() {
  CopyFileW(0, 0, 0); // kernel32
  return 0;
}
