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

#ifndef SRC_ANDROID_INTERNAL_LAZY_LIBRARY_LOADER_H_
#define SRC_ANDROID_INTERNAL_LAZY_LIBRARY_LOADER_H_

#include <stddef.h>
#include <stdint.h>

namespace perfetto {
namespace android_internal {

// Dynamically loads (once) the libperfetto_android_internal.so and looks up the
// given symbol name. It never unloads the library once loaded. Doing so is very
// bug-prone (see b/137280403).
// |name| can be either a symbol name (e.g. DoFoo) or a (partially or fully)
// qualified name (e.g., android_internal::DoFoo). In the latter case the
// namespace is dropped only the symbol name is retained (in other words this
// function assumes all symbols are declared as extern "C").
void* LazyLoadFunction(const char* name);

// Convenience wrapper to avoid the reinterpret_cast boilerplate. Boils down to:
// FunctionType name = reinterpret_cast<FunctionType>(LazyLoadFunction(...)).
// Can be used both for member fields and local variables.
#define PERFETTO_LAZY_LOAD(FUNCTION, VAR_NAME)                          \
  decltype(&FUNCTION) VAR_NAME = reinterpret_cast<decltype(&FUNCTION)>( \
      ::perfetto::android_internal::LazyLoadFunction(#FUNCTION))

}  // namespace android_internal
}  // namespace perfetto

#endif  // SRC_ANDROID_INTERNAL_LAZY_LIBRARY_LOADER_H_
