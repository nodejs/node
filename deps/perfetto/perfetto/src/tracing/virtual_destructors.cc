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

#include "perfetto/tracing/internal/tracing_tls.h"
#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/tracing_backend.h"

// This translation unit contains the definitions for the destructor of pure
// virtual interfaces for the src/public:public target. The alternative would be
// introducing a one-liner .cc file for each pure virtual interface, which is
// overkill. This is for compliance with -Wweak-vtables.

namespace perfetto {
namespace internal {

TracingTLS::~TracingTLS() = default;

}  // namespace internal

TracingBackend::~TracingBackend() = default;
TracingSession::~TracingSession() = default;

}  // namespace perfetto
