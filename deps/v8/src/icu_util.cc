// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/icu_util.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(V8_I18N_SUPPORT)
#include <stdio.h>
#include <stdlib.h>

#include "unicode/putil.h"
#include "unicode/udata.h"

#include "src/base/build_config.h"
#include "src/base/file-utils.h"

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

bool InitializeICUDefaultLocation(const char* exec_path,
                                  const char* icu_data_file) {
#if !defined(V8_I18N_SUPPORT)
  return true;
#else
#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  if (icu_data_file) {
    return InitializeICU(icu_data_file);
  }
  char* icu_data_file_default;
#if defined(V8_TARGET_LITTLE_ENDIAN)
  RelativePath(&icu_data_file_default, exec_path, "icudtl.dat");
#elif defined(V8_TARGET_BIG_ENDIAN)
  RelativePath(&icu_data_file_default, exec_path, "icudtb.dat");
#else
#error Unknown byte ordering
#endif
  bool result = InitializeICU(icu_data_file_default);
  free(icu_data_file_default);
  return result;
#else
  return InitializeICU(NULL);
#endif
#endif
}

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

}  // namespace internal
}  // namespace v8
