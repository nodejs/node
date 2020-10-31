/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PROFILING_PERF_REGS_PARSING_H_
#define SRC_PROFILING_PERF_REGS_PARSING_H_

#include <stdint.h>
#include <unwindstack/Regs.h>

#include <memory>

#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace profiling {

// Returns a bitmask for sampling the userspace register set, used when
// configuring perf events.
uint64_t PerfUserRegsMaskForArch(unwindstack::ArchEnum arch);

// Converts the raw sampled register bytes to libunwindstack's representation
// (correct arch-dependent subclass). Advances |data| pointer to past the
// register data. The unique_ptr can be empty, if there were no userspace
// registers to sample (i.e. we've sampled a kernel thread).
// TODO(rsavitski): come up with a better signature (also consider how much to
// isolate libunwindstack types).
std::unique_ptr<unwindstack::Regs> ReadPerfUserRegsData(const char** data);

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_PERF_REGS_PARSING_H_
