/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "perfetto/base/build_config.h"
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#include "perfetto/ext/base/pipe.h"

#include <sys/types.h>
#include <unistd.h>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace base {

Pipe::Pipe() = default;
Pipe::Pipe(Pipe&&) noexcept = default;
Pipe& Pipe::operator=(Pipe&&) = default;

Pipe Pipe::Create(Flags flags) {
  int fds[2];
  PERFETTO_CHECK(pipe(fds) == 0);
  Pipe p;
  p.rd.reset(fds[0]);
  p.wr.reset(fds[1]);

  PERFETTO_CHECK(fcntl(*p.rd, F_SETFD, FD_CLOEXEC) == 0);
  PERFETTO_CHECK(fcntl(*p.wr, F_SETFD, FD_CLOEXEC) == 0);

  if (flags == kBothNonBlock || flags == kRdNonBlock) {
    int cur_flags = fcntl(*p.rd, F_GETFL, 0);
    PERFETTO_CHECK(cur_flags >= 0);
    PERFETTO_CHECK(fcntl(*p.rd, F_SETFL, cur_flags | O_NONBLOCK) == 0);
  }

  if (flags == kBothNonBlock || flags == kWrNonBlock) {
    int cur_flags = fcntl(*p.wr, F_GETFL, 0);
    PERFETTO_CHECK(cur_flags >= 0);
    PERFETTO_CHECK(fcntl(*p.wr, F_SETFL, cur_flags | O_NONBLOCK) == 0);
  }
  return p;
}

}  // namespace base
}  // namespace perfetto

#endif  // !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
