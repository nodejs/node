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

#ifndef SRC_ANDROID_INTERNAL_DROPBOX_SERVICE_H_
#define SRC_ANDROID_INTERNAL_DROPBOX_SERVICE_H_

namespace perfetto {
namespace android_internal {

extern "C" {

// Saves the passed file into Dropbox with the given tag.
// Takes ownership of the passed file descriptor.
bool __attribute__((visibility("default")))
SaveIntoDropbox(const char* tag, int fd);

}  // extern "C"

}  // namespace android_internal
}  // namespace perfetto

#endif  // SRC_ANDROID_INTERNAL_DROPBOX_SERVICE_H_
