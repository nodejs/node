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

#include "absl/base/attributes.h"

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace {

TEST(Attributes, RequireExplicitInit) {
  struct Agg {
    int f1;
    int f2 ABSL_REQUIRE_EXPLICIT_INIT;
  };
  Agg good1 ABSL_ATTRIBUTE_UNUSED = {1, 2};
#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
  Agg good2 ABSL_ATTRIBUTE_UNUSED(1, 2);
#endif
  Agg good3 ABSL_ATTRIBUTE_UNUSED{1, 2};
  Agg good4 ABSL_ATTRIBUTE_UNUSED = {1, 2};
  Agg good5 ABSL_ATTRIBUTE_UNUSED = Agg{1, 2};
  Agg good6[1] ABSL_ATTRIBUTE_UNUSED = {{1, 2}};
  Agg good7[1] ABSL_ATTRIBUTE_UNUSED = {Agg{1, 2}};
  union {
    Agg agg;
  } good8 ABSL_ATTRIBUTE_UNUSED = {{1, 2}};
  constexpr Agg good9 ABSL_ATTRIBUTE_UNUSED = {1, 2};
  constexpr Agg good10 ABSL_ATTRIBUTE_UNUSED{1, 2};
}

}  // namespace
