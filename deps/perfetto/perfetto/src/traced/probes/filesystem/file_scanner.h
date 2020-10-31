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

#ifndef SRC_TRACED_PROBES_FILESYSTEM_FILE_SCANNER_H_
#define SRC_TRACED_PROBES_FILESYSTEM_FILE_SCANNER_H_

#include <string>
#include <vector>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/traced/data_source_types.h"

namespace perfetto {

class FileScanner {
 public:
  class Delegate {
   public:
    virtual bool OnInodeFound(BlockDeviceID,
                              Inode,
                              const std::string&,
                              InodeFileMap_Entry_Type) = 0;
    virtual void OnInodeScanDone() = 0;
    virtual ~Delegate();
  };

  FileScanner(std::vector<std::string> root_directories,
              Delegate* delegate,
              uint32_t scan_interval_ms,
              uint32_t scan_steps);

  // Ctor when only the blocking version of Scan is used.
  FileScanner(std::vector<std::string> root_directories, Delegate* delegate);

  FileScanner(const FileScanner&) = delete;
  FileScanner& operator=(const FileScanner&) = delete;

  void Scan(base::TaskRunner* task_runner);
  void Scan();

 private:
  void NextDirectory();
  void Step();
  void Steps(uint32_t n);
  bool Done();

  Delegate* delegate_;
  const uint32_t scan_interval_ms_;
  const uint32_t scan_steps_;

  std::vector<std::string> queue_;
  base::ScopedDir current_dir_handle_;
  std::string current_directory_;
  BlockDeviceID current_block_device_id_;
  base::WeakPtrFactory<FileScanner> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FILESYSTEM_FILE_SCANNER_H_
