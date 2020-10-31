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

#ifndef SRC_TRACE_PROCESSOR_STORAGE_METADATA_H_
#define SRC_TRACE_PROCESSOR_STORAGE_METADATA_H_

#include <stddef.h>

#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto {
namespace trace_processor {
namespace metadata {

// Compile time list of metadata items.
// clang-format off
#define PERFETTO_TP_METADATA(F)                                               \
  F(benchmark_description,             KeyType::kSingle,  Variadic::kString), \
  F(benchmark_name,                    KeyType::kSingle,  Variadic::kString), \
  F(benchmark_start_time_us,           KeyType::kSingle,  Variadic::kInt),    \
  F(benchmark_had_failures,            KeyType::kSingle,  Variadic::kInt),    \
  F(benchmark_label,                   KeyType::kSingle,  Variadic::kString), \
  F(benchmark_story_name,              KeyType::kSingle,  Variadic::kString), \
  F(benchmark_story_run_index,         KeyType::kSingle,  Variadic::kInt),    \
  F(benchmark_story_run_time_us,       KeyType::kSingle,  Variadic::kInt),    \
  F(benchmark_story_tags,              KeyType::kMulti,   Variadic::kString), \
  F(statsd_triggering_subscription_id, KeyType::kSingle,  Variadic::kInt),    \
  F(trace_uuid,                        KeyType::kSingle,  Variadic::kString), \
  F(system_name,                       KeyType::kSingle,  Variadic::kString), \
  F(system_version,                    KeyType::kSingle,  Variadic::kString), \
  F(system_release,                    KeyType::kSingle,  Variadic::kString), \
  F(system_machine,                    KeyType::kSingle,  Variadic::kString), \
  F(android_build_fingerprint,         KeyType::kSingle,  Variadic::kString), \
  F(trace_size_bytes,                  KeyType::kSingle,  Variadic::kInt), \
  F(all_data_source_started_ns,        KeyType::kSingle,  Variadic::kInt)
// clang-format on

// Compile time list of metadata items.
// clang-format off
#define PERFETTO_TP_METADATA_KEY_TYPES(F) \
  F(kSingle, "single"),                   \
  F(kMulti,  "multi")
// clang-format

// Ignore GCC warning about a missing argument for a variadic macro parameter.
#pragma GCC system_header

#define PERFETTO_TP_META_TYPE_ENUM(varname, ...) varname
enum class KeyType : size_t {
  PERFETTO_TP_METADATA_KEY_TYPES(PERFETTO_TP_META_TYPE_ENUM),
  kNumKeyTypes,
};

#define PERFETTO_TP_META_TYPE_NAME(_, name, ...) name
constexpr char const* kKeyTypeNames[] = {
  PERFETTO_TP_METADATA_KEY_TYPES(PERFETTO_TP_META_TYPE_NAME)
};

// Declares an enum of literals (one for each item). The enum values of each
// literal corresponds to the string index in the arrays below.
#define PERFETTO_TP_META_ENUM(name, ...) name
enum KeyIDs : size_t {
  PERFETTO_TP_METADATA(PERFETTO_TP_META_ENUM),
  kNumKeys
};

// The code below declares an array for each property:
// name, key type, value type.

#define PERFETTO_TP_META_NAME(name, ...) #name
constexpr char const* kNames[] = {
  PERFETTO_TP_METADATA(PERFETTO_TP_META_NAME)};

#define PERFETTO_TP_META_KEYTYPE(_, type, ...) type
constexpr KeyType kKeyTypes[] = {
    PERFETTO_TP_METADATA(PERFETTO_TP_META_KEYTYPE)};

#define PERFETTO_TP_META_VALUETYPE(_, __, type, ...) type
constexpr Variadic::Type kValueTypes[] = {
    PERFETTO_TP_METADATA(PERFETTO_TP_META_VALUETYPE)};

}  // namespace metadata
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_STORAGE_METADATA_H_
