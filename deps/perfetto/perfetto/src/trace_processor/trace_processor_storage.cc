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

#include "perfetto/trace_processor/trace_processor_storage.h"

#include "src/trace_processor/trace_processor_storage_impl.h"

namespace perfetto {
namespace trace_processor {

// static
std::unique_ptr<TraceProcessorStorage> TraceProcessorStorage::CreateInstance(
    const Config& config) {
  return std::unique_ptr<TraceProcessorStorage>(
      new TraceProcessorStorageImpl(config));
}

TraceProcessorStorage::~TraceProcessorStorage() = default;

}  // namespace trace_processor
}  // namespace perfetto
