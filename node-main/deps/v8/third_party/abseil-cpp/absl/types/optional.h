// Copyright 2017 The Abseil Authors.
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
//
// -----------------------------------------------------------------------------
// optional.h
// -----------------------------------------------------------------------------
//
// Historical note: Abseil once provided an implementation of `absl::optional`
// as a polyfill for `std::optional` prior to C++17. Now that C++17 is required,
// `absl::optional` is an alias for `std::optional`.

#ifndef ABSL_TYPES_OPTIONAL_H_
#define ABSL_TYPES_OPTIONAL_H_

#include <optional>

#include "absl/base/config.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
using std::bad_optional_access;
using std::optional;
using std::make_optional;
using std::nullopt_t;
using std::nullopt;
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TYPES_OPTIONAL_H_
