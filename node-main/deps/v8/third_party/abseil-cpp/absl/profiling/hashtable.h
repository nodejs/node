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

#ifndef ABSL_PROFILING_HASHTABLE_H_
#define ABSL_PROFILING_HASHTABLE_H_

#include <cstdint>
#include <string>

#include "absl/container/internal/hashtablez_sampler.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

absl::StatusOr<std::string> MarshalHashtableProfile();

namespace debugging_internal {

absl::StatusOr<std::string> MarshalHashtableProfile(
    container_internal::HashtablezSampler& sampler, absl::Time now);

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_PROFILING_HASHTABLE_H_
