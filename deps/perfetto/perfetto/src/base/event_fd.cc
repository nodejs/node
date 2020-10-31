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

#include <stdint.h>
#include <unistd.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/event_fd.h"
#include "perfetto/ext/base/pipe.h"
#include "perfetto/ext/base/utils.h"

#if PERFETTO_USE_EVENTFD()
#include <sys/eventfd.h>
#endif

namespace perfetto {
namespace base {

EventFd::EventFd() {
#if PERFETTO_USE_EVENTFD()
  fd_.reset(eventfd(/* start value */ 0, EFD_CLOEXEC | EFD_NONBLOCK));
  PERFETTO_CHECK(fd_);
#else
  // Make the pipe non-blocking so that we never block the waking thread (either
  // the main thread or another one) when scheduling a wake-up.
  Pipe pipe = Pipe::Create(Pipe::kBothNonBlock);
  fd_ = std::move(pipe.rd);
  write_fd_ = std::move(pipe.wr);
#endif  // !PERFETTO_USE_EVENTFD()
}

EventFd::~EventFd() = default;

void EventFd::Notify() {
  const uint64_t value = 1;

#if PERFETTO_USE_EVENTFD()
  ssize_t ret = write(fd_.get(), &value, sizeof(value));
#else
  ssize_t ret = write(write_fd_.get(), &value, sizeof(uint8_t));
#endif

  if (ret <= 0 && errno != EAGAIN) {
    PERFETTO_DFATAL("write()");
  }
}

void EventFd::Clear() {
#if PERFETTO_USE_EVENTFD()
  uint64_t value;
  ssize_t ret = read(fd_.get(), &value, sizeof(value));
#else
  // Drain the byte(s) written to the wake-up pipe. We can potentially read
  // more than one byte if several wake-ups have been scheduled.
  char buffer[16];
  ssize_t ret = read(fd_.get(), &buffer[0], sizeof(buffer));
#endif
  if (ret <= 0 && errno != EAGAIN)
    PERFETTO_DPLOG("read()");
}

}  // namespace base
}  // namespace perfetto

#endif  // !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
