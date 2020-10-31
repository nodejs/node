/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACING_CORE_PACKET_STREAM_VALIDATOR_H_
#define SRC_TRACING_CORE_PACKET_STREAM_VALIDATOR_H_

#include "perfetto/ext/tracing/core/slice.h"

namespace perfetto {

// Checks that the stream of trace packets sent by the producer is well formed.
// This includes:
//
// - Checking that the packets are not truncated.
// - There are no dangling bytes left over in the packets.
// - Any trusted fields (e.g., uid) are not set.
//
// Note that we only validate top-level fields in the trace proto; sub-messages
// are simply skipped.
class PacketStreamValidator {
 public:
  PacketStreamValidator() = delete;

  static bool Validate(const Slices&);
};

}  // namespace perfetto

#endif  // SRC_TRACING_CORE_PACKET_STREAM_VALIDATOR_H_
