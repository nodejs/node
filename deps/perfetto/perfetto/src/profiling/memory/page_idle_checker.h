/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_PROFILING_MEMORY_PAGE_IDLE_CHECKER_H_
#define SRC_PROFILING_MEMORY_PAGE_IDLE_CHECKER_H_

#include <set>

#include <stddef.h>
#include <stdint.h>

#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace profiling {

uint64_t GetFirstPageShare(uint64_t addr, size_t size);
uint64_t GetLastPageShare(uint64_t addr, size_t size);

class PageIdleChecker {
 public:
  PageIdleChecker(base::ScopedFile page_idle_fd)
      : page_idle_fd_(std::move(page_idle_fd)) {}

  // Return number of bytes of allocation of size bytes starting at alloc that
  // are on unreferenced pages.
  // Return -1 on error.
  int64_t OnIdlePage(uint64_t addr, size_t size);

  void MarkPagesIdle();

 private:
  void MarkPageIdle(uint64_t virt_page_nr);
  // Return 1 if page is idle, 0 if it is not idle, or -1 on error.
  int IsPageIdle(uint64_t virt_page_nr);

  std::set<uint64_t> touched_virt_page_nrs_;

  base::ScopedFile page_idle_fd_;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_PAGE_IDLE_CHECKER_H_
