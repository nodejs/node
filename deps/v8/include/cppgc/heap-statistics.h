// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_HEAP_STATISTICS_H_
#define INCLUDE_CPPGC_HEAP_STATISTICS_H_

#include <memory>
#include <string>
#include <vector>

namespace cppgc {

/**
 * `HeapStatistics` contains memory consumption and utilization statistics for a
 * cppgc heap.
 */
struct HeapStatistics final {
  /**
   * Specifies the detail level of the heap statistics. Brief statistics contain
   * only the top-level allocated and used memory statistics for the entire
   * heap. Detailed statistics also contain a break down per space and page, as
   * well as freelist statistics and object type histograms. Note that used
   * memory reported by brief statistics and detailed statistics might differ
   * slightly.
   */
  enum DetailLevel : uint8_t {
    kBrief,
    kDetailed,
  };

  /**
   * Statistics of object types. For each type the statistics record its name,
   * how many objects of that type were allocated, and the overall size used by
   * these objects.
   */
  struct ObjectStatistics {
    /** Number of distinct types in the heap. */
    size_t num_types = 0;
    /** Name of each type in the heap. */
    std::vector<std::string> type_name;
    /** Number of allocated objects per each type. */
    std::vector<size_t> type_count;
    /** Overall size of allocated objects per each type. */
    std::vector<size_t> type_bytes;
  };

  /**
   * Page granularity statistics. For each page the statistics record the
   * allocated memory size and overall used memory size for the page.
   */
  struct PageStatistics {
    /** Overall amount of memory allocated for the page. */
    size_t physical_size_bytes = 0;
    /** Amount of memory actually used on the page. */
    size_t used_size_bytes = 0;
  };

  /**
   * Stastistics of the freelist (used only in non-large object spaces). For
   * each bucket in the freelist the statistics record the bucket size, the
   * number of freelist entries in the bucket, and the overall allocated memory
   * consumed by these freelist entries.
   */
  struct FreeListStatistics {
    /** bucket sizes in the freelist. */
    std::vector<size_t> bucket_size;
    /** number of freelist entries per bucket. */
    std::vector<size_t> free_count;
    /** memory size concumed by freelist entries per size. */
    std::vector<size_t> free_size;
  };

  /**
   * Space granularity statistics. For each space the statistics record the
   * space name, the amount of allocated memory and overall used memory for the
   * space. The statistics also contain statistics for each of the space's
   * pages, its freelist and the objects allocated on the space.
   */
  struct SpaceStatistics {
    /** The space name */
    std::string name;
    /** Overall amount of memory allocated for the space. */
    size_t physical_size_bytes = 0;
    /** Amount of memory actually used on the space. */
    size_t used_size_bytes = 0;
    /** Statistics for each of the pages in the space. */
    std::vector<PageStatistics> page_stats;
    /** Statistics for the freelist of the space. */
    FreeListStatistics free_list_stats;
    /** Statistics for object allocated on the space. Filled only when
     * NameProvider::HideInternalNames() is false. */
    ObjectStatistics object_stats;
  };

  /** Overall amount of memory allocated for the heap. */
  size_t physical_size_bytes = 0;
  /** Amount of memory actually used on the heap. */
  size_t used_size_bytes = 0;
  /** Detail level of this HeapStatistics. */
  DetailLevel detail_level;

  /** Statistics for each of the spaces in the heap. Filled only when
   * detail_level is kDetailed. */
  std::vector<SpaceStatistics> space_stats;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_HEAP_STATISTICS_H_
