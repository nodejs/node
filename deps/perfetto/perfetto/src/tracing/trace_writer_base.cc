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

#include "perfetto/tracing/trace_writer_base.h"

namespace perfetto {

// This destructor needs to be defined in a dedicated translation unit and
// cannot be merged together with the other ones in virtual_destructors.cc.
// This is because trace_writer_base.h/cc  is part of a separate target
// (src/public:common) that is linked also by other part of the codebase.

TraceWriterBase::~TraceWriterBase() = default;

}  // namespace perfetto
