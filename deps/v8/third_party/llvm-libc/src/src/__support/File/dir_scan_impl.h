//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Templated implementation of the scandir logic for dependency injection.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_SCAN_IMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_SCAN_IMPL_H

#include "hdr/errno_macros.h"
#include "hdr/func/free.h"
#include "hdr/func/malloc.h"
#include "hdr/func/realloc.h"
#include "hdr/types/struct_dirent.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/error_or.h"
#include "src/stdlib/qsort_util.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

template <typename DirType>
ErrorOr<int> scan_impl(const char *name, struct dirent ***namelist,
                       int (*filter)(const struct dirent *),
                       int (*compare)(const struct dirent **,
                                      const struct dirent **)) {
  auto res_open = DirType::open(name);
  if (!res_open)
    return LIBC_NAMESPACE::Error(res_open.error());

  DirType *dir = res_open.value();

  int error_code = 0;
  struct dirent **entries = nullptr;
  size_t count = 0;
  size_t buffer_capacity = 0;

  auto free_entries = [&entries, &count]() {
    if (entries == nullptr)
      return;

    for (size_t i = 0; i < count; ++i)
      ::free(entries[i]);

    ::free(entries);
  };

  while (true) {
    auto res_read = dir->read();
    if (!res_read) {
      error_code = res_read.error();
      break;
    }

    struct dirent *entry = res_read.value();
    if (entry == nullptr)
      break;

    if (filter != nullptr && !filter(entry))
      continue;

    // Scandir must return the number of entries as int, anything
    // above that is non-representable.
    if (count == cpp::numeric_limits<int>::max()) {
      error_code = EOVERFLOW;
      break;
    }

    if (count >= buffer_capacity) {
      size_t new_capacity = (buffer_capacity == 0) ? 8 : buffer_capacity * 2;

      // Overflow check
      if (new_capacity >
          cpp::numeric_limits<size_t>::max() / sizeof(struct dirent *)) {
        error_code = ENOMEM;
        break;
      }

      struct dirent **bigger_buffer = static_cast<struct dirent **>(
          ::realloc(entries, new_capacity * sizeof(struct dirent *)));
      if (bigger_buffer == nullptr) {
        error_code = ENOMEM;
        break;
      }
      buffer_capacity = new_capacity;
      entries = bigger_buffer;
    }

    size_t reclen = DirType::reclen(entry);

    struct dirent *new_entry = static_cast<struct dirent *>(::malloc(reclen));
    if (new_entry == nullptr) {
      error_code = ENOMEM;
      break;
    }
    inline_memcpy(new_entry, entry, reclen);

    entries[count] = new_entry;
    count++;
  }

  dir->close();

  if (error_code != 0) {
    free_entries();
    return LIBC_NAMESPACE::Error(error_code);
  }

  if (compare != nullptr && count > 1) {
    auto cmp_fn = [compare](const void *a, const void *b) {
      auto left = static_cast<const struct dirent **>(const_cast<void *>(a));
      auto right = static_cast<const struct dirent **>(const_cast<void *>(b));
      return compare(left, right);
    };
    internal::unstable_sort(entries, count, sizeof(struct dirent *), cmp_fn);
  }

  *namelist = entries;
  return static_cast<int>(count);
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_SCAN_IMPL_H
