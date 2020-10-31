/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_IPC_BASIC_TYPES_H_
#define INCLUDE_PERFETTO_EXT_IPC_BASIC_TYPES_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "perfetto/protozero/cpp_message_obj.h"

namespace perfetto {
namespace ipc {

using ProtoMessage = ::protozero::CppMessageObj;
using ServiceID = uint32_t;
using MethodID = uint32_t;
using ClientID = uint64_t;
using RequestID = uint64_t;

// This determines the maximum size allowed for an IPC message. Trying to send
// or receive a larger message will hit DCHECK(s) and auto-disconnect.
constexpr size_t kIPCBufferSize = 128 * 1024;

constexpr uid_t kInvalidUid = static_cast<uid_t>(-1);

}  // namespace ipc
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_IPC_BASIC_TYPES_H_
