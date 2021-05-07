// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-statistics-collector.h"

#include <string>

#include "include/cppgc/name-provider.h"
#include "src/heap/cppgc/free-list.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/raw-heap.h"
#include "src/heap/cppgc/stats-collector.h"

namespace cppgc {
namespace internal {

namespace {

std::string GetNormalPageSpaceName(size_t index) {
  // Check that space is not a large object space.
  DCHECK_NE(RawHeap::kNumberOfRegularSpaces - 1, index);
  // Handle regular normal page spaces.
  if (index < RawHeap::kNumberOfRegularSpaces) {
    return "NormalPageSpace" + std::to_string(index);
  }
  // Space is a custom space.
  return "CustomSpace" +
         std::to_string(index - RawHeap::kNumberOfRegularSpaces);
}

HeapStatistics::SpaceStatistics* InitializeSpace(HeapStatistics* stats,
                                                 std::string name) {
  stats->space_stats.emplace_back();
  HeapStatistics::SpaceStatistics* space_stats = &stats->space_stats.back();
  space_stats->name = std::move(name);

  if (!NameProvider::HideInternalNames()) {
    const size_t num_types = GlobalGCInfoTable::Get().NumberOfGCInfos();
    space_stats->object_stats.num_types = num_types;
    space_stats->object_stats.type_name.resize(num_types);
    space_stats->object_stats.type_count.resize(num_types);
    space_stats->object_stats.type_bytes.resize(num_types);
  }

  return space_stats;
}

void FinalizePage(HeapStatistics::SpaceStatistics* space_stats,
                  HeapStatistics::PageStatistics** page_stats) {
  if (*page_stats) {
    DCHECK_NOT_NULL(space_stats);
    space_stats->physical_size_bytes += (*page_stats)->physical_size_bytes;
    space_stats->used_size_bytes += (*page_stats)->used_size_bytes;
  }
  *page_stats = nullptr;
}

void FinalizeSpace(HeapStatistics* stats,
                   HeapStatistics::SpaceStatistics** space_stats,
                   HeapStatistics::PageStatistics** page_stats) {
  FinalizePage(*space_stats, page_stats);
  if (*space_stats) {
    DCHECK_NOT_NULL(stats);
    stats->physical_size_bytes += (*space_stats)->physical_size_bytes;
    stats->used_size_bytes += (*space_stats)->used_size_bytes;
  }
  *space_stats = nullptr;
}

void RecordObjectType(HeapStatistics::SpaceStatistics* space_stats,
                      HeapObjectHeader* header, size_t object_size) {
  if (!NameProvider::HideInternalNames()) {
    // Detailed names available.
    GCInfoIndex gc_info_index = header->GetGCInfoIndex();
    space_stats->object_stats.type_count[gc_info_index]++;
    space_stats->object_stats.type_bytes[gc_info_index] += object_size;
    if (space_stats->object_stats.type_name[gc_info_index].empty()) {
      space_stats->object_stats.type_name[gc_info_index] =
          header->GetName().value;
    }
  }
}

}  // namespace

HeapStatistics HeapStatisticsCollector::CollectStatistics(HeapBase* heap) {
  HeapStatistics stats;
  stats.detail_level = HeapStatistics::DetailLevel::kDetailed;
  current_stats_ = &stats;

  Traverse(&heap->raw_heap());
  FinalizeSpace(current_stats_, &current_space_stats_, &current_page_stats_);

  DCHECK_EQ(heap->stats_collector()->allocated_memory_size(),
            stats.physical_size_bytes);
  return stats;
}

bool HeapStatisticsCollector::VisitNormalPageSpace(NormalPageSpace* space) {
  DCHECK_EQ(0u, space->linear_allocation_buffer().size());

  FinalizeSpace(current_stats_, &current_space_stats_, &current_page_stats_);

  current_space_stats_ =
      InitializeSpace(current_stats_, GetNormalPageSpaceName(space->index()));

  space->free_list().CollectStatistics(current_space_stats_->free_list_stats);

  return false;
}

bool HeapStatisticsCollector::VisitLargePageSpace(LargePageSpace* space) {
  FinalizeSpace(current_stats_, &current_space_stats_, &current_page_stats_);

  current_space_stats_ = InitializeSpace(current_stats_, "LargePageSpace");

  return false;
}

bool HeapStatisticsCollector::VisitNormalPage(NormalPage* page) {
  DCHECK_NOT_NULL(current_space_stats_);
  FinalizePage(current_space_stats_, &current_page_stats_);
  current_space_stats_->page_stats.emplace_back(
      HeapStatistics::PageStatistics{kPageSize, 0});
  current_page_stats_ = &current_space_stats_->page_stats.back();
  return false;
}

bool HeapStatisticsCollector::VisitLargePage(LargePage* page) {
  DCHECK_NOT_NULL(current_space_stats_);
  FinalizePage(current_space_stats_, &current_page_stats_);
  HeapObjectHeader* object_header = page->ObjectHeader();
  size_t object_size = page->PayloadSize();
  RecordObjectType(current_space_stats_, object_header, object_size);
  size_t allocated_size = LargePage::AllocationSize(object_size);
  current_space_stats_->physical_size_bytes += allocated_size;
  current_space_stats_->used_size_bytes += object_size;
  current_space_stats_->page_stats.emplace_back(
      HeapStatistics::PageStatistics{allocated_size, object_size});

  return true;
}

bool HeapStatisticsCollector::VisitHeapObjectHeader(HeapObjectHeader* header) {
  DCHECK(!header->IsLargeObject());
  DCHECK_NOT_NULL(current_space_stats_);
  DCHECK_NOT_NULL(current_page_stats_);
  if (header->IsFree()) return true;
  size_t object_size = header->GetSize();
  RecordObjectType(current_space_stats_, header, object_size);
  current_page_stats_->used_size_bytes += object_size;
  return true;
}

}  // namespace internal
}  // namespace cppgc
