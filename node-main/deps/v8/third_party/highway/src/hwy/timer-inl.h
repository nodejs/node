// Copyright 2023 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// DEPRECATED, use timer.h instead.

#include "hwy/timer.h"

#if defined(HIGHWAY_HWY_TIMER_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_TIMER_INL_H_
#undef HIGHWAY_HWY_TIMER_INL_H_
#else
#define HIGHWAY_HWY_TIMER_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace timer {

// Deprecated aliases so that old code still compiles. Prefer to use
// `hwy::timer::*` from timer.h because that does not require highway.h.
using Ticks = hwy::timer::Ticks;

inline Ticks Start() { return hwy::timer::Start(); }
inline Ticks Stop() { return hwy::timer::Stop(); }

}  // namespace timer

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // per-target include guard
