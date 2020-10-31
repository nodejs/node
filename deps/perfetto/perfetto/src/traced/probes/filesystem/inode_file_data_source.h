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

#ifndef SRC_TRACED_PROBES_FILESYSTEM_INODE_FILE_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_FILESYSTEM_INODE_FILE_DATA_SOURCE_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "perfetto/base/flat_set.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/traced/data_source_types.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/traced/probes/filesystem/file_scanner.h"
#include "src/traced/probes/filesystem/fs_mount.h"
#include "src/traced/probes/filesystem/lru_inode_cache.h"
#include "src/traced/probes/probes_data_source.h"

#include "protos/perfetto/trace/filesystem/inode_file_map.pbzero.h"

namespace perfetto {

using InodeFileMap = protos::pbzero::InodeFileMap;
class TraceWriter;

// Creates block_device_map for /system partition
void CreateStaticDeviceToInodeMap(
    const std::string& root_directory,
    std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>*
        static_file_map);

class InodeFileDataSource : public ProbesDataSource,
                            public FileScanner::Delegate {
 public:
  static const ProbesDataSource::Descriptor descriptor;

  InodeFileDataSource(
      DataSourceConfig,
      base::TaskRunner*,
      TracingSessionID,
      std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>*
          static_file_map,
      LRUInodeCache* cache,
      std::unique_ptr<TraceWriter> writer);

  ~InodeFileDataSource() override;

  base::WeakPtr<InodeFileDataSource> GetWeakPtr() const;

  // Called when Inodes are seen in the FtraceEventBundle
  void OnInodes(const base::FlatSet<InodeBlockPair>& inodes);

  // Search in /system partition and add inodes to InodeFileMap proto if found
  void AddInodesFromStaticMap(BlockDeviceID block_device_id,
                              std::set<Inode>* inode_numbers);

  // Search in LRUInodeCache and add inodes to InodeFileMap if found
  void AddInodesFromLRUCache(BlockDeviceID block_device_id,
                             std::set<Inode>* inode_numbers);

  virtual void FillInodeEntry(InodeFileMap* destination,
                              Inode inode_number,
                              const InodeMapValue& inode_map_value);

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

 protected:
  std::multimap<BlockDeviceID, std::string> mount_points_;

 private:
  InodeFileMap* AddToCurrentTracePacket(BlockDeviceID block_device_id);
  void ResetTracePacket();
  void FindMissingInodes();

  // Callbacks for dynamic filesystem scan.
  bool OnInodeFound(BlockDeviceID block_device_id,
                    Inode inode_number,
                    const std::string& path,
                    InodeFileMap_Entry_Type type) override;
  void OnInodeScanDone() override;

  void AddRootsForBlockDevice(BlockDeviceID block_device_id,
                              std::vector<std::string>* roots);
  void RemoveFromNextMissingInodes(BlockDeviceID block_device_id,
                                   Inode inode_number);

  std::set<std::string> scan_mount_points_;
  std::map<std::string, std::vector<std::string>> mount_point_mapping_;

  base::TaskRunner* task_runner_;
  std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>*
      static_file_map_;
  LRUInodeCache* cache_;
  std::unique_ptr<TraceWriter> writer_;
  std::map<BlockDeviceID, std::set<Inode>> missing_inodes_;
  std::map<BlockDeviceID, std::set<Inode>> next_missing_inodes_;
  std::set<BlockDeviceID> seen_block_devices_;
  BlockDeviceID current_block_device_id_;
  TraceWriter::TracePacketHandle current_trace_packet_;
  InodeFileMap* current_file_map_;
  bool has_current_trace_packet_ = false;
  bool scan_running_ = false;
  bool do_not_scan_ = false;
  uint32_t scan_interval_ms_ = 0;
  uint32_t scan_delay_ms_ = 0;
  uint32_t scan_batch_size_ = 0;
  std::unique_ptr<FileScanner> file_scanner_;
  base::WeakPtrFactory<InodeFileDataSource> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FILESYSTEM_INODE_FILE_DATA_SOURCE_H_
