// Copyright 2025 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/casts.h"

#include <cstdlib>
#include <string>

#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"

#ifdef ABSL_INTERNAL_HAS_CXA_DEMANGLE
#include <cxxabi.h>
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace base_internal {

namespace {

std::string DemangleCppString(const char* mangled) {
  std::string out;
  int status = 0;
  char* demangled = nullptr;
#ifdef ABSL_INTERNAL_HAS_CXA_DEMANGLE
  demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
#endif
  if (status == 0 && demangled != nullptr) {
    out.append(demangled);
    free(demangled);
  } else {
    out.append(mangled);
  }
  return out;
}

}  // namespace

void BadDownCastCrash(const char* source_type, const char* target_type) {
  ABSL_RAW_LOG(FATAL, "down cast from %s to %s failed",
               DemangleCppString(source_type).c_str(),
               DemangleCppString(target_type).c_str());
}

}  // namespace base_internal

ABSL_NAMESPACE_END
}  // namespace absl
