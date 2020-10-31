/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_UTIL_STATUS_MACROS_H_
#define SRC_TRACE_PROCESSOR_UTIL_STATUS_MACROS_H_

#include "perfetto/trace_processor/status.h"

// Evaluates |expr|, which should return a util::Status. If the status is an
// error status, returns the status from the current function.
#define RETURN_IF_ERROR(expr)                           \
  do {                                                  \
    util::Status status_macro_internal_status = (expr); \
    if (!status_macro_internal_status.ok())             \
      return status_macro_internal_status;              \
  } while (0)

#endif  // SRC_TRACE_PROCESSOR_UTIL_STATUS_MACROS_H_
