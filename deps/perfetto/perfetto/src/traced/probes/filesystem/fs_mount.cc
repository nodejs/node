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

#include "src/traced/probes/filesystem/fs_mount.h"

#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_splitter.h"

namespace perfetto {

std::multimap<BlockDeviceID, std::string> ParseMounts(const char* path) {
  std::string data;
  if (!base::ReadFile(path, &data)) {
    PERFETTO_ELOG("Failed to read %s", path);
    return {};
  }
  std::multimap<BlockDeviceID, std::string> device_to_mountpoints;

  for (base::StringSplitter lines(std::move(data), '\n'); lines.Next();) {
    base::StringSplitter words(&lines, ' ');
    if (!words.Next() || !words.Next()) {
      PERFETTO_DLOG("Invalid mount point: %s.", lines.cur_token());
      continue;
    }
    const char* mountpoint = words.cur_token();
    struct stat buf {};
    if (stat(mountpoint, &buf) == -1) {
      PERFETTO_PLOG("stat %s", mountpoint);
      continue;
    }
    device_to_mountpoints.emplace(buf.st_dev, mountpoint);
  }
  return device_to_mountpoints;
}

}  // namespace perfetto
