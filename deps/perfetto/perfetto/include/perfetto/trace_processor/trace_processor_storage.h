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

#ifndef INCLUDE_PERFETTO_TRACE_PROCESSOR_TRACE_PROCESSOR_STORAGE_H_
#define INCLUDE_PERFETTO_TRACE_PROCESSOR_TRACE_PROCESSOR_STORAGE_H_

#include <stdint.h>

#include <memory>

#include "perfetto/base/export.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"

namespace perfetto {
namespace trace_processor {

// Coordinates the loading of traces from an arbitrary source.
class PERFETTO_EXPORT TraceProcessorStorage {
 public:
  // Creates a new instance of TraceProcessorStorage.
  static std::unique_ptr<TraceProcessorStorage> CreateInstance(const Config&);

  virtual ~TraceProcessorStorage();

  // The entry point to push trace data into the processor. The trace format
  // will be automatically discovered on the first push call. It is possible
  // to make queries between two pushes.
  // Returns the Ok status if parsing has been succeeding so far, and Error
  // status if some unrecoverable error happened. If this happens, the
  // TraceProcessor will ignore the following Parse() requests, drop data on the
  // floor and return errors forever.
  virtual util::Status Parse(std::unique_ptr<uint8_t[]>, size_t) = 0;

  // When parsing a bounded file (as opposite to streaming from a device) this
  // function should be called when the last chunk of the file has been passed
  // into Parse(). This allows to flush the events queued in the ordering stage,
  // without having to wait for their time window to expire.
  virtual void NotifyEndOfFile() = 0;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACE_PROCESSOR_TRACE_PROCESSOR_STORAGE_H_
