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

#include "src/android_internal/dropbox_service.h"

#include <android/os/DropBoxManager.h>
#include <utils/StrongPointer.h>

namespace perfetto {
namespace android_internal {

bool SaveIntoDropbox(const char* tag, int fd) {
  using android::os::DropBoxManager;
  android::sp<DropBoxManager> dropbox(new DropBoxManager());
  auto status = dropbox->addFile(android::String16(tag), fd, /*flags=*/0);
  return status.isOk();
}

}  // namespace android_internal
}  // namespace perfetto
