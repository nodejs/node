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

#ifndef ABSL_UTILITY_UTILITY_H_
#define ABSL_UTILITY_UTILITY_H_

#include <cstddef>
#include <cstdlib>
#include <tuple>
#include <utility>

#include "absl/base/config.h"

// TODO(b/290784225): Include what you use cleanup required.
#include "absl/meta/type_traits.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// Historical note: Abseil once provided implementations of these
// abstractions for platforms that had not yet provided them. Those
// platforms are no longer supported. New code should simply use the
// the ones from std directly.
using std::apply;
using std::exchange;
using std::forward;
using std::in_place;
using std::in_place_index;
using std::in_place_index_t;
using std::in_place_t;
using std::in_place_type;
using std::in_place_type_t;
using std::index_sequence;
using std::index_sequence_for;
using std::integer_sequence;
using std::make_from_tuple;
using std::make_index_sequence;
using std::make_integer_sequence;
using std::move;

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_UTILITY_UTILITY_H_
