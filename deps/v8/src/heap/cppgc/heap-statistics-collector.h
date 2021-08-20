// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_STATISTICS_COLLECTOR_H_
#define V8_HEAP_CPPGC_HEAP_STATISTICS_COLLECTOR_H_

#include "include/cppgc/heap-statistics.h"
#include "src/heap/cppgc/heap-visitor.h"

namespace cppgc {
namespace internal {

class HeapStatisticsCollector : private HeapVisitor<HeapStatisticsCollector> {
  friend class HeapVisitor<HeapStatisticsCollector>;

 public:
  HeapStatistics CollectStatistics(HeapBase*);

 private:
  bool VisitNormalPageSpace(NormalPageSpace*);
  bool VisitLargePageSpace(LargePageSpace*);
  bool VisitNormalPage(NormalPage*);
  bool VisitLargePage(LargePage*);
  bool VisitHeapObjectHeader(HeapObjectHeader*);

  HeapStatistics* current_stats_;
  HeapStatistics::SpaceStatistics* current_space_stats_ = nullptr;
  HeapStatistics::PageStatistics* current_page_stats_ = nullptr;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_STATISTICS_COLLECTOR_H_
