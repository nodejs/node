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
#ifndef LIEF_OVERFLOW_CHECK_H
#define LIEF_OVERFLOW_CHECK_H
#include <cstdint>

namespace LIEF {
/// This function checks if the addition (A + B) is safe.
/// It returns true if:
/// - A + B > C
/// - A + B overflows
template<class T>
inline bool check_overflow(uint32_t A, uint32_t B, T C) {
  return A > C || (B > (C - A));
}

template<class T>
inline bool check_overflow(uint64_t A, uint64_t B, T C) {
  return A > C || (B > (C - A));
}
}
#endif
