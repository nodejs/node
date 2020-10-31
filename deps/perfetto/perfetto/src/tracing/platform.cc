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

#include "perfetto/tracing/platform.h"
#include "perfetto/tracing/internal/tracing_tls.h"
#include "perfetto/tracing/trace_writer_base.h"

namespace perfetto {

PlatformThreadLocalObject::~PlatformThreadLocalObject() = default;
Platform::~Platform() = default;

// static
std::unique_ptr<PlatformThreadLocalObject>
PlatformThreadLocalObject::CreateInstance() {
  return std::unique_ptr<PlatformThreadLocalObject>(new internal::TracingTLS());
}

}  // namespace perfetto
