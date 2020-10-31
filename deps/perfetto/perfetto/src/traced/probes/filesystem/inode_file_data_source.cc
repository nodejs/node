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

#include "src/traced/probes/filesystem/inode_file_data_source.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <queue>
#include <unordered_map>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_writer.h"

#include "protos/perfetto/config/inode_file/inode_file_config.pbzero.h"
#include "protos/perfetto/trace/filesystem/inode_file_map.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "src/traced/probes/filesystem/file_scanner.h"

namespace perfetto {
namespace {
constexpr uint32_t kScanIntervalMs = 10000;  // 10s
constexpr uint32_t kScanDelayMs = 10000;     // 10s
constexpr uint32_t kScanBatchSize = 15000;

uint32_t OrDefault(uint32_t value, uint32_t def) {
  return value ? value : def;
}

std::string DbgFmt(const std::vector<std::string>& values) {
  if (values.empty())
    return "";

  std::string result;
  for (auto it = values.cbegin(); it != values.cend() - 1; ++it)
    result += *it + ",";

  result += values.back();
  return result;
}

class StaticMapDelegate : public FileScanner::Delegate {
 public:
  StaticMapDelegate(
      std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>* map)
      : map_(map) {}
  ~StaticMapDelegate() {}

 private:
  bool OnInodeFound(BlockDeviceID block_device_id,
                    Inode inode_number,
                    const std::string& path,
                    InodeFileMap_Entry_Type type) {
    std::unordered_map<Inode, InodeMapValue>& inode_map =
        (*map_)[block_device_id];
    inode_map[inode_number].SetType(type);
    inode_map[inode_number].AddPath(path);
    return true;
  }
  void OnInodeScanDone() {}
  std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>* map_;
};

}  // namespace

// static
const ProbesDataSource::Descriptor InodeFileDataSource::descriptor = {
    /*name*/ "linux.inode_file_map",
    /*flags*/ Descriptor::kFlagsNone,
};

void CreateStaticDeviceToInodeMap(
    const std::string& root_directory,
    std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>*
        static_file_map) {
  StaticMapDelegate delegate(static_file_map);
  FileScanner scanner({root_directory}, &delegate);
  scanner.Scan();
}

void InodeFileDataSource::FillInodeEntry(InodeFileMap* destination,
                                         Inode inode_number,
                                         const InodeMapValue& inode_map_value) {
  auto* entry = destination->add_entries();
  entry->set_inode_number(inode_number);
  entry->set_type(static_cast<protos::pbzero::InodeFileMap_Entry_Type>(
      inode_map_value.type()));
  for (const auto& path : inode_map_value.paths())
    entry->add_paths(path.c_str());
}

InodeFileDataSource::InodeFileDataSource(
    DataSourceConfig ds_config,
    base::TaskRunner* task_runner,
    TracingSessionID session_id,
    std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>*
        static_file_map,
    LRUInodeCache* cache,
    std::unique_ptr<TraceWriter> writer)
    : ProbesDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      static_file_map_(static_file_map),
      cache_(cache),
      writer_(std::move(writer)),
      weak_factory_(this) {
  using protos::pbzero::InodeFileConfig;
  InodeFileConfig::Decoder cfg(ds_config.inode_file_config_raw());
  for (auto mp = cfg.scan_mount_points(); mp; ++mp)
    scan_mount_points_.insert((*mp).ToStdString());
  for (auto mpm = cfg.mount_point_mapping(); mpm; ++mpm) {
    InodeFileConfig::MountPointMappingEntry::Decoder entry(*mpm);
    std::vector<std::string> scan_roots;
    for (auto scan_root = entry.scan_roots(); scan_root; ++scan_root)
      scan_roots.push_back((*scan_root).ToStdString());
    std::string mountpoint = entry.mountpoint().ToStdString();
    mount_point_mapping_.emplace(mountpoint, std::move(scan_roots));
  }
  scan_interval_ms_ = OrDefault(cfg.scan_interval_ms(), kScanIntervalMs);
  scan_delay_ms_ = OrDefault(cfg.scan_delay_ms(), kScanDelayMs);
  scan_batch_size_ = OrDefault(cfg.scan_batch_size(), kScanBatchSize);
  do_not_scan_ = cfg.do_not_scan();
}

InodeFileDataSource::~InodeFileDataSource() = default;

void InodeFileDataSource::Start() {
  // Nothing special to do, this data source is only reacting to on-demand
  // events such as OnInodes().
}

void InodeFileDataSource::AddInodesFromStaticMap(
    BlockDeviceID block_device_id,
    std::set<Inode>* inode_numbers) {
  // Check if block device id exists in static file map
  auto static_map_entry = static_file_map_->find(block_device_id);
  if (static_map_entry == static_file_map_->end())
    return;

  uint64_t system_found_count = 0;
  for (auto it = inode_numbers->begin(); it != inode_numbers->end();) {
    Inode inode_number = *it;
    // Check if inode number exists in static file map for given block device id
    auto inode_it = static_map_entry->second.find(inode_number);
    if (inode_it == static_map_entry->second.end()) {
      ++it;
      continue;
    }
    system_found_count++;
    it = inode_numbers->erase(it);
    FillInodeEntry(AddToCurrentTracePacket(block_device_id), inode_number,
                   inode_it->second);
  }
  PERFETTO_DLOG("%" PRIu64 " inodes found in static file map",
                system_found_count);
}

void InodeFileDataSource::AddInodesFromLRUCache(
    BlockDeviceID block_device_id,
    std::set<Inode>* inode_numbers) {
  uint64_t cache_found_count = 0;
  for (auto it = inode_numbers->begin(); it != inode_numbers->end();) {
    Inode inode_number = *it;
    auto value = cache_->Get(std::make_pair(block_device_id, inode_number));
    if (value == nullptr) {
      ++it;
      continue;
    }
    cache_found_count++;
    it = inode_numbers->erase(it);
    FillInodeEntry(AddToCurrentTracePacket(block_device_id), inode_number,
                   *value);
  }
  if (cache_found_count > 0)
    PERFETTO_DLOG("%" PRIu64 " inodes found in cache", cache_found_count);
}

void InodeFileDataSource::Flush(FlushRequestID,
                                std::function<void()> callback) {
  ResetTracePacket();
  writer_->Flush(callback);
}

void InodeFileDataSource::OnInodes(
    const base::FlatSet<InodeBlockPair>& inodes) {
  if (mount_points_.empty()) {
    mount_points_ = ParseMounts();
  }
  // Group inodes from FtraceMetadata by block device
  std::map<BlockDeviceID, std::set<Inode>> inode_file_maps;
  for (const auto& inodes_pair : inodes) {
    Inode inode_number = inodes_pair.first;
    BlockDeviceID block_device_id = inodes_pair.second;
    inode_file_maps[block_device_id].emplace(inode_number);
  }
  if (inode_file_maps.size() > 1)
    PERFETTO_DLOG("Saw %zu block devices.", inode_file_maps.size());

  // Write a TracePacket with an InodeFileMap proto for each block device id
  for (auto& inode_file_map_data : inode_file_maps) {
    BlockDeviceID block_device_id = inode_file_map_data.first;
    std::set<Inode>& inode_numbers = inode_file_map_data.second;
    PERFETTO_DLOG("Saw %zu unique inode numbers.", inode_numbers.size());

    // Add entries to InodeFileMap as inodes are found and resolved to their
    // paths/type
    AddInodesFromStaticMap(block_device_id, &inode_numbers);
    AddInodesFromLRUCache(block_device_id, &inode_numbers);

    if (do_not_scan_)
      inode_numbers.clear();

    // If we defined mount points we want to scan in the config,
    // skip inodes on other mount points.
    if (!scan_mount_points_.empty()) {
      bool process = true;
      auto range = mount_points_.equal_range(block_device_id);
      for (auto it = range.first; it != range.second; ++it) {
        if (scan_mount_points_.count(it->second) == 0) {
          process = false;
          break;
        }
      }
      if (!process)
        continue;
    }

    if (!inode_numbers.empty()) {
      // Try to piggy back the current scan.
      auto it = missing_inodes_.find(block_device_id);
      if (it != missing_inodes_.end()) {
        it->second.insert(inode_numbers.cbegin(), inode_numbers.cend());
      }
      next_missing_inodes_[block_device_id].insert(inode_numbers.cbegin(),
                                                   inode_numbers.cend());
      if (!scan_running_) {
        scan_running_ = true;
        auto weak_this = GetWeakPtr();
        task_runner_->PostDelayedTask(
            [weak_this] {
              if (!weak_this) {
                PERFETTO_DLOG("Giving up filesystem scan.");
                return;
              }
              weak_this.get()->FindMissingInodes();
            },
            scan_delay_ms_);
      }
    }
  }
}

InodeFileMap* InodeFileDataSource::AddToCurrentTracePacket(
    BlockDeviceID block_device_id) {
  seen_block_devices_.emplace(block_device_id);
  if (!has_current_trace_packet_ ||
      current_block_device_id_ != block_device_id) {
    if (has_current_trace_packet_)
      current_trace_packet_->Finalize();
    current_trace_packet_ = writer_->NewTracePacket();
    current_file_map_ = current_trace_packet_->set_inode_file_map();
    has_current_trace_packet_ = true;

    // Add block device id to InodeFileMap
    current_file_map_->set_block_device_id(
        static_cast<uint64_t>(block_device_id));
    // Add mount points to InodeFileMap
    auto range = mount_points_.equal_range(block_device_id);
    for (std::multimap<BlockDeviceID, std::string>::iterator it = range.first;
         it != range.second; ++it)
      current_file_map_->add_mount_points(it->second.c_str());
  }
  return current_file_map_;
}

void InodeFileDataSource::RemoveFromNextMissingInodes(
    BlockDeviceID block_device_id,
    Inode inode_number) {
  auto it = next_missing_inodes_.find(block_device_id);
  if (it == next_missing_inodes_.end())
    return;
  it->second.erase(inode_number);
}

bool InodeFileDataSource::OnInodeFound(BlockDeviceID block_device_id,
                                       Inode inode_number,
                                       const std::string& path,
                                       InodeFileMap_Entry_Type inode_type) {
  auto it = missing_inodes_.find(block_device_id);
  if (it == missing_inodes_.end())
    return true;

  size_t n = it->second.erase(inode_number);
  if (n == 0)
    return true;

  if (it->second.empty())
    missing_inodes_.erase(it);

  RemoveFromNextMissingInodes(block_device_id, inode_number);

  std::pair<BlockDeviceID, Inode> key{block_device_id, inode_number};
  auto cur_val = cache_->Get(key);
  if (cur_val) {
    cur_val->AddPath(path);
    FillInodeEntry(AddToCurrentTracePacket(block_device_id), inode_number,
                   *cur_val);
  } else {
    InodeMapValue new_val(InodeMapValue(inode_type, {path}));
    cache_->Insert(key, new_val);
    FillInodeEntry(AddToCurrentTracePacket(block_device_id), inode_number,
                   new_val);
  }
  PERFETTO_DLOG("Filled %s", path.c_str());
  return !missing_inodes_.empty();
}

void InodeFileDataSource::ResetTracePacket() {
  current_block_device_id_ = 0;
  current_file_map_ = nullptr;
  if (has_current_trace_packet_)
    current_trace_packet_->Finalize();
  has_current_trace_packet_ = false;
}

void InodeFileDataSource::OnInodeScanDone() {
  // Finalize the accumulated trace packets.
  ResetTracePacket();
  file_scanner_.reset();
  if (!missing_inodes_.empty()) {
    // At least write mount point mapping for inodes that are not found.
    for (const auto& p : missing_inodes_) {
      if (seen_block_devices_.count(p.first) == 0)
        AddToCurrentTracePacket(p.first);
    }
  }

  if (next_missing_inodes_.empty()) {
    scan_running_ = false;
  } else {
    auto weak_this = GetWeakPtr();
    PERFETTO_DLOG("Starting another filesystem scan.");
    task_runner_->PostDelayedTask(
        [weak_this] {
          if (!weak_this) {
            PERFETTO_DLOG("Giving up filesystem scan.");
            return;
          }
          weak_this->FindMissingInodes();
        },
        scan_delay_ms_);
  }
}

void InodeFileDataSource::AddRootsForBlockDevice(
    BlockDeviceID block_device_id,
    std::vector<std::string>* roots) {
  auto range = mount_points_.equal_range(block_device_id);
  for (auto it = range.first; it != range.second; ++it) {
    PERFETTO_DLOG("Trying to replace %s", it->second.c_str());
    auto replace_it = mount_point_mapping_.find(it->second);
    if (replace_it != mount_point_mapping_.end()) {
      roots->insert(roots->end(), replace_it->second.cbegin(),
                    replace_it->second.cend());
      return;
    }
  }

  for (auto it = range.first; it != range.second; ++it)
    roots->emplace_back(it->second);
}

void InodeFileDataSource::FindMissingInodes() {
  missing_inodes_ = std::move(next_missing_inodes_);
  std::vector<std::string> roots;
  for (auto& p : missing_inodes_)
    AddRootsForBlockDevice(p.first, &roots);

  PERFETTO_DCHECK(file_scanner_.get() == nullptr);
  auto weak_this = GetWeakPtr();
  PERFETTO_DLOG("Starting scan of %s", DbgFmt(roots).c_str());
  file_scanner_ = std::unique_ptr<FileScanner>(new FileScanner(
      std::move(roots), this, scan_interval_ms_, scan_batch_size_));

  file_scanner_->Scan(task_runner_);
}

base::WeakPtr<InodeFileDataSource> InodeFileDataSource::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

}  // namespace perfetto
