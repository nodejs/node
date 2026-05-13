// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>

#include "absl/debugging/internal/symbolize.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace debugging_internal {

SymbolDecorator::Factory* SetSymbolDecoratorFactory(SymbolDecorator::Factory*) {
  return nullptr;
}
bool RegisterFileMappingHint(const void *, const void *, uint64_t, const char *) {
  return false;
}
bool GetFileMappingHint(const void **, const void **, uint64_t *, const char **) {
  return false;
}

}  // namespace debugging_internal

void InitializeSymbolizer(const char*) {}
bool Symbolize(const void *, char *, int) { return false; }

ABSL_NAMESPACE_END
}  // namespace absl
