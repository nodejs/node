// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/exec/coverage.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "experimental/redline/base/log.h"

namespace redline {

namespace {

inline bool GetBit(const uint8_t* bits, uint32_t index) {
  return (bits[index / 8] >> (index % 8)) & 1;
}

inline void SetBit(uint8_t* bits, uint32_t index) {
  bits[index / 8] |= static_cast<uint8_t>(1u << (index % 8));
}

inline void ClearBit(uint8_t* bits, uint32_t index) {
  bits[index / 8] &= static_cast<uint8_t>(~(1u << (index % 8)));
}

// Bit b of the result is set if byte b of `word` (little-endian) is not zero.
inline uint8_t NonZeroBytes(uint64_t word) {
  constexpr uint64_t kLow7 = 0x7f7f7f7f7f7f7f7fULL;
  // The high bit of each byte: set if the byte is not zero.
  const uint64_t high = (((word & kLow7) + kLow7) | word) & ~kLow7;
  // Moves the 8 high bits into the top byte, in byte order.
  return static_cast<uint8_t>(((high >> 7) * 0x0102040810204080ULL) >> 56);
}

}  // namespace

// static
std::unique_ptr<CoverageMap> CoverageMap::Create(int id, std::string* error) {
  std::string name =
      StringPrintf("/shm_id_%d_%d", static_cast<int>(getpid()), id);
  shm_unlink(name.c_str());
  int fd = shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC,
                    S_IRUSR | S_IWUSR);
  if (fd < 0) {
    *error =
        StringPrintf("shm_open(%s) failed: %s", name.c_str(), strerror(errno));
    return nullptr;
  }
  if (ftruncate(fd, static_cast<off_t>(kShmSize)) != 0) {
    *error =
        StringPrintf("ftruncate(%s) failed: %s", name.c_str(), strerror(errno));
    close(fd);
    shm_unlink(name.c_str());
    return nullptr;
  }
  void* mapping =
      mmap(nullptr, kShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (mapping == MAP_FAILED) {
    *error = StringPrintf("mmap(%s) failed: %s", name.c_str(), strerror(errno));
    shm_unlink(name.c_str());
    return nullptr;
  }
  memset(mapping, 0, kShmSize);
  return std::unique_ptr<CoverageMap>(new CoverageMap(id, name, mapping));
}

CoverageMap::CoverageMap(int id, std::string shm_name, void* mapping)
    : id_(id),
      shm_name_(std::move(shm_name)),
      shmem_(static_cast<uint8_t*>(mapping)) {}

CoverageMap::~CoverageMap() {
  munmap(shmem_, kShmSize);
  shm_unlink(shm_name_.c_str());
}

// static
uint32_t CoverageMap::Bucket(uint8_t count) {
  if (count <= 3) {
    return count == 0 ? 0 : static_cast<uint32_t>(count) - 1;
  }
  if (count < 8) {
    return 3;
  }
  if (count < 16) {
    return 4;
  }
  if (count < 32) {
    return 5;
  }
  return count < 128 ? 6 : 7;
}

namespace {

// Checks the header that the target wrote. Returns an empty string if it is
// valid, else the reason.
std::string CheckHeader(const cov::Header& header) {
  if (header.magic != cov::kMagic) {
    return "The target did not write the coverage header of Redline; is it "
           "built with v8_redline=true and coverage instrumentation "
           "(sanitizer_coverage_flags)?";
  }
  if (header.version != cov::kVersion) {
    return StringPrintf(
        "The target writes version %u of the coverage header, but Redline "
        "reads version %u; rebuild Redline or the target",
        header.version, cov::kVersion);
  }
  const auto mode = static_cast<cov::Mode>(header.mode);
  if (mode != cov::Mode::kTracePcGuard && mode != cov::Mode::kInlineBoolFlag &&
      mode != cov::Mode::kInline8bitCounters) {
    return "The target has no coverage instrumentation "
           "(sanitizer_coverage_flags)";
  }
  if (header.header_size != cov::kHeaderSize ||
      header.shm_size != CoverageMap::kShmSize ||
      header.bits_per_edge != cov::BitsPerEdge(mode) ||
      header.max_edges !=
          cov::MaxEdges(header.shm_size, header.bits_per_edge) ||
      header.num_edges == 0 || header.num_edges > header.max_edges ||
      header.num_regions > cov::kMaxRegions ||
      header.build_id_size > cov::kMaxBuildIdSize) {
    return "The target wrote an invalid coverage header";
  }
  // The regions follow each other, from edge 1 to the last edge.
  uint32_t next = 1;
  for (uint32_t i = 0; i < header.num_regions; i++) {
    const cov::Region& region = header.regions[i];
    if (region.kind < static_cast<uint32_t>(cov::RegionKind::kCode) ||
        region.kind > static_cast<uint32_t>(cov::RegionKind::kOptimized) ||
        region.first != next || region.count > header.num_edges - next + 1) {
      return "The target wrote an invalid coverage region";
    }
    next += region.count;
  }
  if (next != header.num_edges + 1) {
    return "The coverage regions of the target do not hold all edges";
  }
  return "";
}

}  // namespace

bool CoverageMap::FinishInitialization(std::string* error) {
  cov::Header header;
  memcpy(&header, shmem_, sizeof(header));
  std::string problem = CheckHeader(header);
  if (!problem.empty()) {
    *error = problem;
    return false;
  }
  if (real_edges_ != 0) {
    // A new process of the same target must write the same header.
    if (memcmp(&header, &header_, sizeof(header)) != 0) {
      *error = StringPrintf(
          "The target changed its coverage header (%u -> %u edges); mixing "
          "binaries?",
          real_edges_, header.num_edges);
      return false;
    }
    return true;
  }
  header_ = header;
  const uint32_t real_edges = header.num_edges;
  const bool counters = header.bits_per_edge == 8;
  regions_.clear();
  for (uint32_t i = 0; i < header.num_regions; i++) {
    regions_.push_back({static_cast<RegionKind>(header.regions[i].kind),
                        header.regions[i].first, header.regions[i].count});
  }
  counters_mode_ = counters;
  real_edges_ = real_edges;
  counted_edges_ = real_edges;
  watched_.clear();
  if (counters_mode_) {
    for (const Region& region : regions_) {
      if (region.kind != RegionKind::kOptimized) {
        continue;
      }
      if (region.first + region.count - 1 == real_edges) {
        counted_edges_ = region.first - 1;
      }
      if (watch_.optimized) {
        for (uint32_t i = 0; i < region.count; i++) {
          watched_.push_back(region.first + i);
        }
      }
    }
    for (uint32_t edge : watch_.edges) {
      if (edge != 0 && edge <= real_edges) {
        watched_.push_back(edge);
      }
    }
    std::sort(watched_.begin(), watched_.end());
    watched_.erase(std::unique(watched_.begin(), watched_.end()),
                   watched_.end());
    // Runs of consecutive watched edges, for Refresh().
    watch_runs_.clear();
    for (uint32_t j = 0; j < watched_.size(); j++) {
      if (!watch_runs_.empty() &&
          watch_runs_.back().edge + watch_runs_.back().length == watched_[j]) {
        watch_runs_.back().length++;
      } else {
        watch_runs_.push_back({watched_[j], j, 1});
      }
    }
  } else if (!watch_.edges.empty()) {
    *error =
        "Watched edges need a d8 with coverage counters "
        "(sanitizer_coverage_flags=\"inline-8bit-counters,pc-table\")";
    return false;
  }
  const uint64_t features =
      static_cast<uint64_t>(real_edges) + uint64_t{kBuckets} * watched_.size();
  if (features >= (uint64_t{1} << 31)) {
    *error = "Too many watched edges";
    return false;
  }
  num_edges_ = static_cast<uint32_t>(features);
  auto round_up = [](uint32_t bits_needed) {
    uint32_t bytes = (bits_needed + 7) / 8;
    // A multiple of 8 bytes, so that the scans can use 64-bit words.
    return bytes + 7 - ((bytes - 1) % 8);
  };
  bitmap_size_ = round_up(num_edges_ + 1);
  edge_bitmap_size_ = round_up(real_edges_ + 1);
  derived_.assign(counters_mode_ ? bitmap_size_ : 0, 0);
  virgin_bits_.assign(bitmap_size_, 0xff);
  crash_bits_.assign(bitmap_size_, 0xff);
  ResetState();
  return true;
}

const char* CoverageMap::mode_name() const {
  switch (static_cast<cov::Mode>(header_.mode)) {
    case cov::Mode::kTracePcGuard:
      return "trace-pc-guard";
    case cov::Mode::kInlineBoolFlag:
      return "inline-bool-flag";
    case cov::Mode::kInline8bitCounters:
      return "inline-8bit-counters";
    case cov::Mode::kNone:
      break;
  }
  return "none";
}

std::string CoverageMap::build_id() const {
  std::string hex;
  for (uint32_t i = 0; i < header_.build_id_size; i++) {
    hex += StringPrintf("%02x", header_.build_id[i]);
  }
  return hex;
}

double CoverageMap::coverage_fraction() const {
  if (counted_edges_ == 0) {
    return 0.0;
  }
  return static_cast<double>(found_edges_) /
         static_cast<double>(counted_edges_ + 1);
}

CoverageMap::FeatureKind CoverageMap::KindOf(uint32_t feature) const {
  if (feature == 0 || feature > num_edges_) {
    return FeatureKind::kNone;
  }
  if (feature > real_edges_) {
    return FeatureKind::kBucket;
  }
  for (const Region& region : regions_) {
    if (feature >= region.first && feature - region.first < region.count) {
      switch (region.kind) {
        case RegionKind::kCode:
          return FeatureKind::kCode;
        case RegionKind::kBuiltins:
          return FeatureKind::kBuiltins;
        case RegionKind::kOptimized:
          return FeatureKind::kOptimized;
      }
    }
  }
  return FeatureKind::kCode;
}

uint32_t CoverageMap::EdgeOf(uint32_t feature) const {
  if (feature <= real_edges_) {
    return feature;
  }
  const uint32_t j = (feature - bucket_base()) / kBuckets;
  return j < watched_.size() ? watched_[j] : 0;
}

void CoverageMap::ClearBitmap() {
  if (bitmap_size_ == 0) {
    return;
  }
  if (counters_mode_) {
    memset(shm_counters(), 0, static_cast<size_t>(real_edges_) + 1);
    memset(derived_.data(), 0, derived_.size());
  } else {
    memset(shm_edges(), 0, bitmap_size_);
  }
}

void CoverageMap::Refresh() {
  if (!counters_mode_) {
    return;
  }
  uint8_t* out = derived_.data();
  const uint8_t* counters = shm_counters();
  // Hit bits: counter i maps to bit i, so byte k of the bitmap holds the
  // counters 8k to 8k+7. cov::MaxEdges() leaves room for the last word.
  // NonZeroBytes() makes the 8 bits of a byte at once.
  const uint32_t words = real_edges_ / 8 + 1;
  for (uint32_t k = 0; k < words; k++) {
    uint64_t word;
    memcpy(&word, counters + static_cast<size_t>(k) * 8, 8);
    out[k] = word == 0 ? 0 : NonZeroBytes(word);
  }
  memset(out + words, 0, derived_.size() - words);
  // Index 0 is never an edge, and bits beyond the last edge are not edges.
  ClearBit(out, 0);
  for (uint32_t i = real_edges_ + 1; i < words * 8; i++) {
    ClearBit(out, i);
  }
  // Bucket features of the watched edges. Most counters are zero, so each
  // run of consecutive watched edges is read 8 counters at a time.
  const uint32_t base = bucket_base();
  for (const WatchRun& run : watch_runs_) {
    const uint8_t* run_counters = counters + run.edge;
    for (uint32_t i = 0; i < run.length;) {
      if (i + 8 <= run.length) {
        uint64_t word;
        memcpy(&word, run_counters + i, 8);
        if (word == 0) {
          i += 8;
          continue;
        }
      }
      const uint8_t count = run_counters[i];
      if (count != 0) {
        SetBit(out, base + (run.index + i) * kBuckets + Bucket(count));
      }
      i++;
    }
  }
}

uint32_t CoverageMap::HitCount(uint32_t edge) const {
  if (edge == 0 || edge > real_edges_) {
    return 0;
  }
  if (counters_mode_) {
    return shm_counters()[edge];
  }
  return GetBit(shm_edges(), edge) ? 1 : 0;
}

uint32_t CoverageMap::ScanNewEdges(uint8_t* virgin_bits, uint32_t size_bytes,
                                   std::vector<uint32_t>* out) {
  uint32_t count = 0;
  const uint8_t* edges = bits();
  for (uint32_t offset = 0; offset < size_bytes; offset += 8) {
    uint64_t current, virgin;
    memcpy(&current, edges + offset, 8);
    memcpy(&virgin, virgin_bits + offset, 8);
    uint64_t fresh = current & virgin;
    if (fresh == 0) {
      continue;
    }
    while (fresh != 0) {
      uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(fresh));
      uint32_t index = offset * 8 + bit;
      ClearBit(virgin_bits, index);
      if (out != nullptr) {
        out->push_back(index);
      }
      if (index <= counted_edges_) {
        count++;
      }
      fresh &= fresh - 1;
    }
  }
  return count;
}

std::vector<uint32_t> CoverageMap::EvaluateNewEdges() {
  std::vector<uint32_t> new_edges;
  if (bitmap_size_ == 0) {
    return new_edges;
  }
  found_edges_ += ScanNewEdges(virgin_bits_.data(), bitmap_size_, &new_edges);
  return new_edges;
}

bool CoverageMap::EvaluateCrash() {
  if (bitmap_size_ == 0) {
    return false;
  }
  std::vector<uint32_t> fresh;
  ScanNewEdges(crash_bits_.data(), edge_bitmap_size_, &fresh);
  return !fresh.empty();
}

bool CoverageMap::AllEdgesSet(const std::vector<uint32_t>& edges) const {
  for (uint32_t index : edges) {
    if (index == 0 || index > num_edges_ || !GetBit(bits(), index)) {
      return false;
    }
  }
  return true;
}

std::vector<uint32_t> CoverageMap::MissingEdges(
    const std::vector<uint32_t>& edges) const {
  std::vector<uint32_t> missing;
  for (uint32_t index : edges) {
    if (index == 0 || index > num_edges_ || !GetBit(bits(), index)) {
      missing.push_back(index);
    }
  }
  return missing;
}

std::vector<uint32_t> CoverageMap::CurrentEdges() const {
  std::vector<uint32_t> result;
  const uint8_t* edges = bits();
  for (uint32_t offset = 0; offset < bitmap_size_; offset += 8) {
    uint64_t current;
    memcpy(&current, edges + offset, 8);
    while (current != 0) {
      uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(current));
      uint32_t index = offset * 8 + bit;
      if (index != 0 && index <= num_edges_) {
        result.push_back(index);
      }
      current &= current - 1;
    }
  }
  return result;
}

uint32_t CoverageMap::CountCurrentEdges() const {
  uint32_t count = 0;
  const uint8_t* edges = bits();
  const uint32_t full_bytes = (counted_edges_ + 1) / 8;
  uint32_t offset = 0;
  for (; offset + 8 <= full_bytes; offset += 8) {
    uint64_t current;
    memcpy(&current, edges + offset, 8);
    count += static_cast<uint32_t>(__builtin_popcountll(current));
  }
  for (uint32_t index = offset * 8; index <= counted_edges_; index++) {
    if (GetBit(edges, index)) {
      count++;
    }
  }
  if (count > 0 && GetBit(edges, 0)) {
    count--;
  }
  return count;
}

void CoverageMap::ForgetEdges(const std::vector<uint32_t>& edges) {
  for (uint32_t index : edges) {
    if (index == 0 || index > num_edges_) {
      continue;
    }
    if (!GetBit(virgin_bits_.data(), index)) {
      SetBit(virgin_bits_.data(), index);
      if (index <= counted_edges_) {
        found_edges_--;
      }
    }
  }
}

void CoverageMap::MarkSeen(const std::vector<uint32_t>& edges) {
  for (uint32_t index : edges) {
    if (index == 0 || index > num_edges_) {
      continue;
    }
    if (GetBit(virgin_bits_.data(), index)) {
      ClearBit(virgin_bits_.data(), index);
      if (index <= counted_edges_) {
        found_edges_++;
      }
    }
  }
}

void CoverageMap::ResetState() {
  if (bitmap_size_ == 0) {
    return;
  }
  std::fill(virgin_bits_.begin(), virgin_bits_.end(), 0xff);
  std::fill(crash_bits_.begin(), crash_bits_.end(), 0xff);
  ClearBit(virgin_bits_.data(), 0);
  ClearBit(crash_bits_.data(), 0);
  for (uint32_t index = num_edges_ + 1; index < bitmap_size_ * 8; index++) {
    ClearBit(virgin_bits_.data(), index);
    ClearBit(crash_bits_.data(), index);
  }
  found_edges_ = 0;
}

}  // namespace redline
