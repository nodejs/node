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

#include "perfetto/ext/tracing/ipc/default_socket.h"

#include "perfetto/base/build_config.h"
#include "perfetto/ext/ipc/basic_types.h"
#include "perfetto/ext/tracing/core/basic_types.h"

#include <stdlib.h>

namespace perfetto {

static_assert(kInvalidUid == ipc::kInvalidUid, "kInvalidUid mismatching");

const char* GetProducerSocket() {
  static const char* name = getenv("PERFETTO_PRODUCER_SOCK_NAME");
  if (name == nullptr) {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
    name = "/dev/socket/traced_producer";
#else
    name = "/tmp/perfetto-producer";
#endif
  }
  return name;
}

const char* GetConsumerSocket() {
  static const char* name = getenv("PERFETTO_CONSUMER_SOCK_NAME");
  if (name == nullptr) {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
    name = "/dev/socket/traced_consumer";
#else
    name = "/tmp/perfetto-consumer";
#endif
  }
  return name;
}

}  // namespace perfetto
