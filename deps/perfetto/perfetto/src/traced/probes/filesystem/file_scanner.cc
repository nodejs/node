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

#include "src/traced/probes/filesystem/file_scanner.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "protos/perfetto/trace/filesystem/inode_file_map.pbzero.h"
#include "src/traced/probes/filesystem/inode_file_data_source.h"

namespace perfetto {
namespace {

std::string JoinPaths(const std::string& one, const std::string& other) {
  std::string result;
  result.reserve(one.size() + other.size() + 1);
  result += one;
  if (!result.empty() && result.back() != '/')
    result += '/';
  result += other;
  return result;
}

}  // namespace

FileScanner::FileScanner(std::vector<std::string> root_directories,
                         Delegate* delegate,
                         uint32_t scan_interval_ms,
                         uint32_t scan_steps)
    : delegate_(delegate),
      scan_interval_ms_(scan_interval_ms),
      scan_steps_(scan_steps),
      queue_(std::move(root_directories)),
      weak_factory_(this) {}

FileScanner::FileScanner(std::vector<std::string> root_directories,
                         Delegate* delegate)
    : FileScanner(std::move(root_directories),
                  delegate,
                  0 /* scan_interval_ms */,
                  0 /* scan_steps */) {}

void FileScanner::Scan() {
  while (!Done())
    Step();
  delegate_->OnInodeScanDone();
}
void FileScanner::Scan(base::TaskRunner* task_runner) {
  PERFETTO_DCHECK(scan_interval_ms_ && scan_steps_);
  Steps(scan_steps_);
  if (Done())
    return delegate_->OnInodeScanDone();
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner->PostDelayedTask(
      [weak_this, task_runner] {
        if (!weak_this)
          return;
        weak_this->Scan(task_runner);
      },
      scan_interval_ms_);
}

void FileScanner::NextDirectory() {
  std::string directory = std::move(queue_.back());
  queue_.pop_back();
  current_dir_handle_.reset(opendir(directory.c_str()));
  if (!current_dir_handle_) {
    PERFETTO_DPLOG("opendir %s", directory.c_str());
    current_directory_.clear();
    return;
  }
  current_directory_ = std::move(directory);

  struct stat buf;
  if (fstat(dirfd(current_dir_handle_.get()), &buf) != 0) {
    PERFETTO_DPLOG("fstat %s", current_directory_.c_str());
    current_dir_handle_.reset();
    current_directory_.clear();
    return;
  }

  if (S_ISLNK(buf.st_mode)) {
    current_dir_handle_.reset();
    current_directory_.clear();
    return;
  }
  current_block_device_id_ = buf.st_dev;
}

void FileScanner::Step() {
  if (!current_dir_handle_) {
    if (queue_.empty())
      return;
    NextDirectory();
  }

  if (!current_dir_handle_)
    return;

  struct dirent* entry = readdir(current_dir_handle_.get());
  if (entry == nullptr) {
    current_dir_handle_.reset();
    return;
  }

  std::string filename = entry->d_name;
  if (filename == "." || filename == "..")
    return;

  std::string filepath = JoinPaths(current_directory_, filename);

  protos::pbzero::InodeFileMap_Entry_Type type =
      protos::pbzero::InodeFileMap_Entry_Type_UNKNOWN;
  // Readdir and stat not guaranteed to have directory info for all systems
  if (entry->d_type == DT_DIR) {
    // Continue iterating through files if current entry is a directory
    queue_.emplace_back(filepath);
    type = protos::pbzero::InodeFileMap_Entry_Type_DIRECTORY;
  } else if (entry->d_type == DT_REG) {
    type = protos::pbzero::InodeFileMap_Entry_Type_FILE;
  }

  if (!delegate_->OnInodeFound(current_block_device_id_, entry->d_ino, filepath,
                               type)) {
    queue_.clear();
    current_dir_handle_.reset();
  }
}

void FileScanner::Steps(uint32_t n) {
  for (uint32_t i = 0; i < n && !Done(); ++i)
    Step();
}

bool FileScanner::Done() {
  return !current_dir_handle_ && queue_.empty();
}

FileScanner::Delegate::~Delegate() = default;

}  // namespace perfetto
