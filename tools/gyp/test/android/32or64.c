/* Copyright (c) 2014 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const char* getString()
{
#ifdef __LP64__
  return "Hello, 64-bit world!\n";
#else
  return "Hello, 32-bit world!\n";
#endif
}
