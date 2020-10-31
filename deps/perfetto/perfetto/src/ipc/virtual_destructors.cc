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

#include "perfetto/ext/ipc/client.h"
#include "perfetto/ext/ipc/host.h"
#include "perfetto/ext/ipc/service.h"
#include "perfetto/ext/ipc/service_proxy.h"

// This translation unit contains the definitions for the destructor of pure
// virtual interfaces for the current build target. The alternative would be
// introducing a one-liner .cc file for each pure virtual interface, which is
// overkill. This is for compliance with -Wweak-vtables.

namespace perfetto {
namespace ipc {

Client::~Client() = default;
Host::~Host() = default;
Service::~Service() = default;
ServiceProxy::EventListener::~EventListener() = default;

}  // namespace ipc
}  // namespace perfetto
