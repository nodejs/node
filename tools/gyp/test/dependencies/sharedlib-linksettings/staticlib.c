/*
 * Copyright (c) 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This will fail to compile if TEST_DEFINE was propagated from sharedlib to
 * staticlib.
 */
#ifdef TEST_DEFINE
#error TEST_DEFINE is defined!
#endif

#ifdef _WIN32
__declspec(dllimport)
#else
extern
#endif
int sharedLibFunc();

int staticLibFunc() {
  return sharedLibFunc();
}
