// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/icu_util.h"

#if defined(_WIN32)
#include "src/base/win32-headers.h"
#endif

#if defined(V8_INTL_SUPPORT)
#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "src/base/build_config.h"
#include "src/base/file-utils.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

#define ICU_UTIL_DATA_FILE 0
#define ICU_UTIL_DATA_STATIC 1

#endif

namespace v8 {

namespace internal {

#if defined(V8_INTL_SUPPORT) && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
namespace {
#if V8_OS_POSIX && !V8_OS_ZOS
base::OS::MemoryMappedFile* g_icu_data_file = nullptr;

void free_icu_data_file() { delete g_icu_data_file; }
#else
char* g_icu_data_ptr = nullptr;

void free_icu_data_ptr() { delete[] g_icu_data_ptr; }
#endif

}  // namespace
#endif

bool InitializeICUDefaultLocation(const char* exec_path,
                                  const char* icu_data_file) {
#if !defined(V8_INTL_SUPPORT)
  return true;
#elif ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  if (icu_data_file) {
    return InitializeICU(icu_data_file);
  }
#if defined(V8_TARGET_LITTLE_ENDIAN)
  std::unique_ptr<char[]> icu_data_file_default =
      base::RelativePath(exec_path, "icudtl.dat");
#elif defined(V8_TARGET_BIG_ENDIAN)
  std::unique_ptr<char[]> icu_data_file_default =
      base::RelativePath(exec_path, "icudtb.dat");
#else
#error Unknown byte ordering
#endif
  return InitializeICU(icu_data_file_default.get());
#else
  return InitializeICU(nullptr);
#endif
}

bool InitializeICU(const char* icu_data_file) {
#if !defined(V8_INTL_SUPPORT)
  return true;
#else
#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC
  // Use bundled ICU data.
  return true;
#elif ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  if (!icu_data_file) return false;

// On Windows MemoryMappedFile decodes the path as UTF-8 while fopen uses the
// ANSI code page, and on z/OS MemoryMappedFile::open mishandles a missing
// file, so both keep reading the file into a heap buffer.
#if V8_OS_POSIX && !V8_OS_ZOS
  if (g_icu_data_file) return true;

  std::unique_ptr<base::OS::MemoryMappedFile> file(
      base::OS::MemoryMappedFile::open(
          icu_data_file, base::OS::MemoryMappedFile::FileMode::kReadOnly));
  // Empty files are opened but not mapped.
  if (!file || !file->memory()) return false;

  g_icu_data_file = file.release();
  atexit(free_icu_data_file);
  const void* data = g_icu_data_file->memory();
#else
  if (g_icu_data_ptr) return true;

  FILE* inf = base::Fopen(icu_data_file, "rb");
  if (!inf) return false;

  fseek(inf, 0, SEEK_END);
  size_t size = ftell(inf);
  rewind(inf);

  if (size == 0) {
    base::Fclose(inf);
    return false;
  }

  g_icu_data_ptr = new char[size];
  if (fread(g_icu_data_ptr, 1, size, inf) != size) {
    delete[] g_icu_data_ptr;
    g_icu_data_ptr = nullptr;
    base::Fclose(inf);
    return false;
  }
  base::Fclose(inf);

  atexit(free_icu_data_ptr);
  const void* data = g_icu_data_ptr;
#endif

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(data, &err);
  // Never try to load ICU data from files.
  udata_setFileAccess(UDATA_ONLY_PACKAGES, &err);
  return err == U_ZERO_ERROR;
#endif
#endif
}

#undef ICU_UTIL_DATA_FILE
#undef ICU_UTIL_DATA_STATIC

}  // namespace internal
}  // namespace v8
