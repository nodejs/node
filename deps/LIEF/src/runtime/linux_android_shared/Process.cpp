/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "linux_android_shared/Process.hpp"
#include "logging.hpp"
#include "fmt_formatter/error_code.hpp"
#include "fmt_formatter/filesystem.hpp"

#include <filesystem>
#include <fstream>

namespace LIEF::runtime::linux_android {

optional<std::string> cmdline() {
  static std::filesystem::path CMDLINE_FS("/proc/cmdline");
  std::error_code ec;
  std::uintmax_t fsize = std::filesystem::file_size(CMDLINE_FS, ec);
  if (ec) {
    LIEF_ERR("Failed to determine the file size of {} ({})", CMDLINE_FS, ec);
    return nullopt();
  }

  std::ifstream ifs(CMDLINE_FS, std::ios_base::binary);
  if (!ifs) {
    LIEF_ERR("Failed to open: {}", CMDLINE_FS);
    return nullopt();
  }

  std::string content;
  content.resize_and_overwrite(fsize, [&ifs](char* buff, size_t size) {
    ifs.read(buff, size);
    return ifs.gcount();
  });
  if (!content.empty() && content.back() == '\n') {
    content.pop_back();
  }
  return content;
}
}
