// Copyright 2025 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/contrib/thread_pool/thread_pool.h"

#include "hwy/highway_export.h"

namespace hwy {
namespace pool {

// TODO: move implementation here.

HWY_CONTRIB_DLLEXPORT Shared& Shared::Get() {
  static Shared* shared = new Shared();
  return *shared;
}

}  // namespace pool
}  // namespace hwy
