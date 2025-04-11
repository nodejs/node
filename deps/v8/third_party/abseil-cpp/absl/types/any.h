//
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
// any.h
// -----------------------------------------------------------------------------
//
// Historical note: Abseil once provided an implementation of `absl::any` as a
// polyfill for `std::any` prior to C++17. Now that C++17 is required,
// `absl::any` is an alias for `std::any`.

#ifndef ABSL_TYPES_ANY_H_
#define ABSL_TYPES_ANY_H_

#include <any>  // IWYU pragma: export

#include "absl/base/config.h"

// Include-what-you-use cleanup required for these headers.
#include "absl/base/attributes.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
using std::any;
using std::any_cast;
using std::bad_any_cast;
using std::make_any;
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TYPES_ANY_H_
