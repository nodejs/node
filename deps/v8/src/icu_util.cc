// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "icu_util.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(V8_I18N_SUPPORT)
#include <stdio.h>
#include <stdlib.h>

#include "unicode/putil.h"
#include "unicode/udata.h"

#define ICU_UTIL_DATA_FILE   0
#define ICU_UTIL_DATA_SHARED 1
#define ICU_UTIL_DATA_STATIC 2

#define ICU_UTIL_DATA_SYMBOL "icudt" U_ICU_VERSION_SHORT "_dat"
#define ICU_UTIL_DATA_SHARED_MODULE_NAME "icudt.dll"
#endif

namespace v8 {

namespace internal {

#if defined(V8_I18N_SUPPORT) && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
namespace {
char* g_icu_data_ptr = NULL;

void free_icu_data_ptr() {
  delete[] g_icu_data_ptr;
}

}  // namespace
#endif

bool InitializeICU(const char* icu_data_file) {
#if !defined(V8_I18N_SUPPORT)
  return true;
#else
#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_SHARED
  // We expect to find the ICU data module alongside the current module.
  HMODULE module = LoadLibraryA(ICU_UTIL_DATA_SHARED_MODULE_NAME);
  if (!module) return false;

  FARPROC addr = GetProcAddress(module, ICU_UTIL_DATA_SYMBOL);
  if (!addr) return false;

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(addr), &err);
  return err == U_ZERO_ERROR;
#elif ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC
  // Mac/Linux bundle the ICU data in.
  return true;
#elif ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  if (!icu_data_file) return false;

  if (g_icu_data_ptr) return true;

  FILE* inf = fopen(icu_data_file, "rb");
  if (!inf) return false;

  fseek(inf, 0, SEEK_END);
  size_t size = ftell(inf);
  rewind(inf);

  g_icu_data_ptr = new char[size];
  if (fread(g_icu_data_ptr, 1, size, inf) != size) {
    delete[] g_icu_data_ptr;
    g_icu_data_ptr = NULL;
    fclose(inf);
    return false;
  }
  fclose(inf);

  atexit(free_icu_data_ptr);

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(g_icu_data_ptr), &err);
  return err == U_ZERO_ERROR;
#endif
#endif
}

} }  // namespace v8::internal
