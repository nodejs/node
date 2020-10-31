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

#ifndef INCLUDE_PERFETTO_TRACING_PLATFORM_H_
#define INCLUDE_PERFETTO_TRACING_PLATFORM_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>

#include "perfetto/base/export.h"

namespace perfetto {

namespace base {
class TaskRunner;
}  // namespace base

// This abstract class is used to abstract dependencies on platform-specific
// primitives that cannot be implemented by the perfetto codebase and must be
// provided or overridden by the embedder.
// This is, for instance, for cases where we want to use some particular
// base:: class in Chrome and provide instead POSIX fallbacks for other
// embedders.

// Base class for thread-local objects. This is to get a basic object vtable and
// delegate destruction to the embedder. See Platform::CreateThreadLocalObject.
class PlatformThreadLocalObject {
 public:
  // Implemented by perfetto internal code. The embedder must call this when
  // implementing GetOrCreateThreadLocalObject() to create an instance for the
  // first time on each thread.
  static std::unique_ptr<PlatformThreadLocalObject> CreateInstance();
  virtual ~PlatformThreadLocalObject();
};

class PERFETTO_EXPORT Platform {
 public:
  // Embedders can use this unless they have custom needs (e.g. Chrome wanting
  // to use its own base class for TLS).
  static Platform* GetDefaultPlatform();

  virtual ~Platform();

  // Creates a thread-local object. The embedder must:
  // - Create an instance per-thread calling ThreadLocalObject::CreateInstance.
  // - Own the lifetime of the returned object as long as the thread is alive.
  // - Destroy it when the thread exits.
  // Perfetto requires only one thread-local object overall (obviously, one
  // instance per-thread) from the embedder.
  using ThreadLocalObject = ::perfetto::PlatformThreadLocalObject;
  virtual ThreadLocalObject* GetOrCreateThreadLocalObject() = 0;

  // Creates a sequenced task runner. The easiest implementation is to create
  // a new thread (e.g. use base::ThreadTaskRunner) but this can also be
  // implemented in some more clever way (e.g. using chromiums's scheduler).
  struct CreateTaskRunnerArgs {};
  virtual std::unique_ptr<base::TaskRunner> CreateTaskRunner(
      const CreateTaskRunnerArgs&) = 0;

  // Used to derive the producer name. Mostly relevant when using the
  // kSystemBackend mode. It can be an arbitrary string when using the
  // in-process mode.
  virtual std::string GetCurrentProcessName() = 0;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_PLATFORM_H_
