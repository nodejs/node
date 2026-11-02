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
      shmem_(static_cast<ShmemData*>(mapping)) {}

CoverageMap::~CoverageMap() {
  munmap(shmem_, kShmSize);
  shm_unlink(shm_name_.c_str());
}

bool CoverageMap::FinishInitialization(std::string* error) {
  uint32_t num_edges = shmem_->num_edges;
  if (num_edges == 0) {
    *error =
        "The target did not publish an edge count; is it built with "
        "coverage instrumentation (sanitizer_coverage_flags) and "
        "v8_fuzzilli=true?";
    return false;
  }
  // Index 0 is unused; the bitmap must hold indices 0..num_edges.
  if (num_edges > kMaxEdges) {
    *error = StringPrintf("Too many edges (%u) for the shared memory region",
                          num_edges);
    return false;
  }
  if (num_edges_ != 0 && num_edges_ != num_edges) {
    *error = StringPrintf(
        "The target changed its edge count (%u -> %u); mixing binaries?",
        num_edges_, num_edges);
    return false;
  }
  if (num_edges_ == num_edges) {
    return true;
  }

  num_edges_ = num_edges;
  uint32_t bitmap_size = (num_edges + 1 + 7) / 8;
  // Round up to a multiple of 8 bytes so that the scans can use 64-bit
  // words.
  bitmap_size += 7 - ((bitmap_size - 1) % 8);
  bitmap_size_ = bitmap_size;
  virgin_bits_.assign(bitmap_size_, 0xff);
  crash_bits_.assign(bitmap_size_, 0xff);
  ResetState();
  return true;
}

double CoverageMap::coverage_fraction() const {
  if (num_edges_ == 0) {
    return 0.0;
  }
  return static_cast<double>(found_edges_) /
         static_cast<double>(num_edges_ + 1);
}

void CoverageMap::ClearBitmap() {
  if (bitmap_size_ == 0) {
    return;
  }
  memset(shm_edges(), 0, bitmap_size_);
}

uint32_t CoverageMap::ScanNewEdges(uint8_t* virgin_bits,
                                   std::vector<uint32_t>* out) {
  uint32_t count = 0;
  const uint8_t* edges = shm_edges();
  for (uint32_t offset = 0; offset < bitmap_size_; offset += 8) {
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
      count++;
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
  found_edges_ += ScanNewEdges(virgin_bits_.data(), &new_edges);
  return new_edges;
}

bool CoverageMap::EvaluateCrash() {
  if (bitmap_size_ == 0) {
    return false;
  }
  return ScanNewEdges(crash_bits_.data(), nullptr) > 0;
}

bool CoverageMap::AllEdgesSet(const std::vector<uint32_t>& edges) const {
  for (uint32_t index : edges) {
    if (index == 0 || index > num_edges_ || !GetBit(shm_edges(), index)) {
      return false;
    }
  }
  return true;
}

std::vector<uint32_t> CoverageMap::MissingEdges(
    const std::vector<uint32_t>& edges) const {
  std::vector<uint32_t> missing;
  for (uint32_t index : edges) {
    if (index == 0 || index > num_edges_ || !GetBit(shm_edges(), index)) {
      missing.push_back(index);
    }
  }
  return missing;
}

std::vector<uint32_t> CoverageMap::CurrentEdges() const {
  std::vector<uint32_t> result;
  const uint8_t* edges = shm_edges();
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
  const uint8_t* edges = shm_edges();
  for (uint32_t offset = 0; offset < bitmap_size_; offset += 8) {
    uint64_t current;
    memcpy(&current, edges + offset, 8);
    count += static_cast<uint32_t>(__builtin_popcountll(current));
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
      found_edges_--;
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
      found_edges_++;
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
