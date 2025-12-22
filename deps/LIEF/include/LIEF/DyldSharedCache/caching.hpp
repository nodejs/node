/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_DSC_CACHING_H
#define LIEF_DSC_CACHING_H
#include <string>
#include "LIEF/visibility.h"

namespace LIEF {
namespace dsc {

/// Enable globally cache/memoization. One can also leverage this function
/// by setting the environment variable `DYLDSC_ENABLE_CACHE` to `1`
///
/// By default, LIEF will use the directory specified by the environment
/// variable `DYLDSC_CACHE_DIR` as its cache-root directory:
///
/// ```bash
/// DYLDSC_ENABLE_CACHE=1 DYLDSC_CACHE_DIR=/tmp/my_dir ./my-program
/// ```
///
/// Otherwise, if `DYLDSC_CACHE_DIR` is not set, LIEF will use the following
/// directory (in this priority):
///
/// 1. System or user cache directory
///   - macOS: `DARWIN_USER_TEMP_DIR` / `DARWIN_USER_CACHE_DIR` + `/dyld_shared_cache`
///   - Linux: `${XDG_CACHE_HOME}/dyld_shared_cache`
///   - Windows: `%LOCALAPPDATA%\dyld_shared_cache`
/// 2. Home directory
///   - macOS/Linux: `$HOME/.dyld_shared_cache`
///   - Windows: `%USERPROFILE%\.dyld_shared_cache`
///
///
/// \see LIEF::dsc::DyldSharedCache::enable_caching for a finer granularity
LIEF_API bool enable_cache();

/// Same behavior as enable_cache() but with a
/// user-provided cache directory
LIEF_API bool enable_cache(const std::string& dir);
}
}
#endif

