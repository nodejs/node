/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/android_internal/lazy_library_loader.h"

#include <dlfcn.h>
#include <stdlib.h>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"

namespace perfetto {
namespace android_internal {

namespace {

const char kLibName[] = "libperfetto_android_internal.so";

void* LoadLibraryOnce() {
#if !PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  // For testing only. Allows to use the version of the .so shipped in the
  // system (if any) with the standalone builds of perfetto. This is really
  // crash-prone and should not be used in production. The .so doesn't have a
  // stable ABI, hence the version of the library in the system and the code in
  // ToT can diverge.
  const char* env_var = getenv("PERFETTO_ENABLE_ANDROID_INTERNAL_LIB");
  if (!env_var || strcmp(env_var, "1")) {
    PERFETTO_ELOG(
        "android_internal functions can be used only with in-tree builds of "
        "perfetto.");
    return nullptr;
  }
#endif
  void* handle = dlopen(kLibName, RTLD_NOW);
  if (!handle)
    PERFETTO_PLOG("dlopen(%s) failed", kLibName);
  return handle;
}

}  // namespace

void* LazyLoadFunction(const char* name) {
  // Strip the namespace qualification from the full symbol name.
  const char* sep = strrchr(name, ':');
  const char* function_name = sep ? sep + 1 : name;
  static void* handle = LoadLibraryOnce();
  if (!handle)
    return nullptr;
  void* fn = dlsym(handle, function_name);
  if (!fn)
    PERFETTO_PLOG("dlsym(%s) failed", function_name);
  return fn;
}

}  // namespace android_internal
}  // namespace perfetto
