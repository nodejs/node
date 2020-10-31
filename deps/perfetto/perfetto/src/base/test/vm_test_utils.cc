/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/base/test/vm_test_utils.h"

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/utils.h"

#include <memory>

#include <errno.h>
#include <string.h>

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <vector>

#include <Windows.h>
#include <Psapi.h>
#else  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <sys/mman.h>
#include <sys/stat.h>
#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"

namespace perfetto {
namespace base {
namespace vm_test_utils {

bool IsMapped(void* start, size_t size) {
  PERFETTO_CHECK(size % kPageSize == 0);
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  int retries = 5;
  int number_of_entries = 4000;  // Just a guess.
  PSAPI_WORKING_SET_INFORMATION* ws_info = nullptr;

  std::vector<char> buffer;
  for (;;) {
    size_t buffer_size =
      sizeof(PSAPI_WORKING_SET_INFORMATION) +
      (number_of_entries * sizeof(PSAPI_WORKING_SET_BLOCK));

    buffer.resize(buffer_size);
    ws_info = reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(&buffer[0]);

    // On success, |buffer_| is populated with info about the working set of
    // |process|. On ERROR_BAD_LENGTH failure, increase the size of the
    // buffer and try again.
    if (QueryWorkingSet(GetCurrentProcess(), &buffer[0], buffer_size))
      break;  // Success

    PERFETTO_CHECK(GetLastError() == ERROR_BAD_LENGTH);

    number_of_entries = ws_info->NumberOfEntries;

    // Maybe some entries are being added right now. Increase the buffer to
    // take that into account. Increasing by 10% should generally be enough.
    number_of_entries *= 1.1;

    PERFETTO_CHECK(--retries > 0);  // If we're looping, eventually fail.
  }

  void* end = reinterpret_cast<char*>(start) + size;
  // Now scan the working-set information looking for the addresses.
  unsigned pages_found = 0;
  for (unsigned i = 0; i < ws_info->NumberOfEntries; ++i) {
    void* address =
        reinterpret_cast<void*>(ws_info->WorkingSetInfo[i].VirtualPage *
        kPageSize);
    if (address >= start && address < end)
      ++pages_found;
  }

  if (pages_found * kPageSize == size)
    return true;
  return false;
#elif PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)
  // Fuchsia doesn't yet support paging (b/119503290).
  return true;
#else
#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
  using PageState = char;
  static constexpr PageState kIncoreMask = MINCORE_INCORE;
#else
  using PageState = unsigned char;
  static constexpr PageState kIncoreMask = 1;
#endif
  const size_t num_pages = size / kPageSize;
  std::unique_ptr<PageState[]> page_states(new PageState[num_pages]);
  memset(page_states.get(), 0, num_pages * sizeof(PageState));
  int res = mincore(start, size, page_states.get());
  // Linux returns ENOMEM when an unmapped memory range is passed.
  // MacOS instead returns 0 but leaves the page_states empty.
  if (res == -1 && errno == ENOMEM)
    return false;
  PERFETTO_CHECK(res == 0);
  for (size_t i = 0; i < num_pages; i++) {
    if (!(page_states[i] & kIncoreMask))
      return false;
  }
  return true;
#endif
}

}  // namespace vm_test_utils
}  // namespace base
}  // namespace perfetto
