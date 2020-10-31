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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_UTILS_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_UTILS_H_

#include <stdint.h>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"

#include "src/trace_processor/importers/common/args_tracker.h"

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
#include <json/value.h>
#else
namespace Json {
class Value {};
}  // namespace Json
#endif

namespace perfetto {
namespace trace_processor {
namespace json {

// Returns whether JSON related functioanlity is supported with the current
// build flags.
bool IsJsonSupported();

enum class TimeUnit { kNs = 1, kUs = 1000, kMs = 1000000 };
base::Optional<int64_t> CoerceToTs(TimeUnit unit, const Json::Value& value);
base::Optional<int64_t> CoerceToInt64(const Json::Value& value);
base::Optional<uint32_t> CoerceToUint32(const Json::Value& value);

// Parses the given JSON string into a JSON::Value object.
// This function should only be called if |IsJsonSupported()| returns true.
base::Optional<Json::Value> ParseJsonString(base::StringView raw_string);

// Flattens the given Json::Value and adds each leaf node to the bound args
// inserter. Note:
//  * |flat_key| and |key| should be non-empty and will be used to prefix the
//    keys of all leaf nodes in the JSON.
//  * |storage| is used to intern all strings (e.g. keys and values).
//  * This function should only be called if |IsJsonSupported()| returns true.
bool AddJsonValueToArgs(const Json::Value& value,
                        base::StringView flat_key,
                        base::StringView key,
                        TraceStorage* storage,
                        ArgsTracker::BoundInserter* inserter);

}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_UTILS_H_
