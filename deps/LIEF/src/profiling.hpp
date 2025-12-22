/* Copyright 2024 - 2025 R. Thomas
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
#ifndef LIEF_PROFILING_UTILS_H
#define LIEF_PROFILING_UTILS_H
#include <spdlog/stopwatch.h>
#include <spdlog/fmt/chrono.h>
#include "logging.hpp"
#include <chrono>

using std::chrono::duration_cast;

namespace LIEF {
class Profile {
  public:
  explicit Profile(std::string msg) :
    msg_(std::move(msg))
  {
    sw_.reset();
  }

  ~Profile() {
    LIEF_DEBUG("{}: {}",
        msg_, duration_cast<std::chrono::milliseconds>(sw_.elapsed()));
  }
  private:
  spdlog::stopwatch sw_;
  std::string msg_;
};
}
#endif
