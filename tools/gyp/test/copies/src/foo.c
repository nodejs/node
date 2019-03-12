// Copyright (c) 2015 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
int f() { return 42; }

#ifdef _MSC_VER
// link.exe gets confused by sourceless shared libraries and needs this
// to become unconfused.
int __stdcall _DllMainCRTStartup(
    unsigned hInst, unsigned reason, void* reserved) {
  return 1;
}
#endif
