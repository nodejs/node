// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include entry point function that's excluded by removing C runtime libraries.
extern "C" void mainCRTStartup() {
}

// Still needed because the linker checks for existence of one of main, wmain,
// WinMain, or wMain to offer informative diagnositics.
int main() {
  return 0;
}
