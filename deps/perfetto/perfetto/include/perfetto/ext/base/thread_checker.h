/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_BASE_THREAD_CHECKER_H_
#define INCLUDE_PERFETTO_EXT_BASE_THREAD_CHECKER_H_

#include "perfetto/base/build_config.h"

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <pthread.h>
#endif
#include <atomic>

#include "perfetto/base/export.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace base {

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
using ThreadID = unsigned long;
#else
using ThreadID = pthread_t;
#endif

class PERFETTO_EXPORT ThreadChecker {
 public:
  ThreadChecker();
  ~ThreadChecker();
  ThreadChecker(const ThreadChecker&);
  ThreadChecker& operator=(const ThreadChecker&);
  bool CalledOnValidThread() const PERFETTO_WARN_UNUSED_RESULT;
  void DetachFromThread();

 private:
  mutable std::atomic<ThreadID> thread_id_;
};

#if PERFETTO_DCHECK_IS_ON() && !PERFETTO_BUILDFLAG(PERFETTO_CHROMIUM_BUILD)
// TODO(primiano) Use Chromium's thread checker in Chromium.
#define PERFETTO_THREAD_CHECKER(name) base::ThreadChecker name;
#define PERFETTO_DCHECK_THREAD(name) \
  PERFETTO_DCHECK((name).CalledOnValidThread())
#define PERFETTO_DETACH_FROM_THREAD(name) (name).DetachFromThread()
#else
#define PERFETTO_THREAD_CHECKER(name)
#define PERFETTO_DCHECK_THREAD(name)
#define PERFETTO_DETACH_FROM_THREAD(name)
#endif  // PERFETTO_DCHECK_IS_ON()

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_THREAD_CHECKER_H_
