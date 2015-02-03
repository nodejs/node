// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

static const char* GetArchOption() {
#if _M_IX86_FP == 0
  return "IA32";
#elif _M_IX86_FP == 1
  return "SSE";
#elif _M_IX86_FP == 2
#  if defined(__AVX2__)
  return "AVX2";
#  elif defined(__AVX__)
  return "AVX";
#  else
  return "SSE2";
#  endif
#else
  return "UNSUPPORTED OPTION";
#endif
}

int main() {
  printf("/arch:%s\n", GetArchOption());
  return 0;
}
