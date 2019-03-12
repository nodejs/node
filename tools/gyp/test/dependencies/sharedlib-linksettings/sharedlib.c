/*
 * Copyright (c) 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef _WIN32
__declspec(dllexport)
#endif
int sharedLibFunc() {
  /*
   * This will fail to compile if TEST_DEFINE was not obtained from sharedlib's
   * link_settings.
   */
  return TEST_DEFINE;
}
