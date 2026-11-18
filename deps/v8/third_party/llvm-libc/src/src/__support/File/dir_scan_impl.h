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

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_DIR_SCAN_IMPL_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_DIR_SCAN_IMPL_H

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/struct_dirent.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/new.h"
#include "src/__support/CPP/unique_ptr.h"
#include "src/__support/File/dir_scan_result.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/stdlib/qsort_util.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

template <typename DirType> struct DirCloser {
  LIBC_INLINE constexpr DirCloser() = default;

  LIBC_INLINE constexpr void operator()(DirType *d) const {
    if (d != nullptr)
      (void)d->close();
  }
};

// TODO: Replace with a dynamic array container once available.
class ScannedEntries {
  cpp::unique_ptr<struct dirent *[]> entries;
  size_t count = 0;
  size_t capacity = 0;

  LIBC_INLINE bool ensure_capacity() {
    if (LIBC_LIKELY(count < capacity))
      return true;

    constexpr size_t INITIAL_CAPACITY = 8;
    constexpr size_t MAX_CAPACITY =
        cpp::numeric_limits<size_t>::max() / sizeof(struct dirent *);
    if (LIBC_UNLIKELY(capacity >= MAX_CAPACITY / 2))
      return false;

    size_t new_capacity = (capacity == 0) ? INITIAL_CAPACITY : capacity * 2;

    CheckedPtr<struct dirent *[]> bigger_buffer =
        make_unique_for_overwrite<struct dirent *[]>(new_capacity);
    if (LIBC_UNLIKELY(!bigger_buffer))
      return false;

    if (entries)
      inline_memcpy(bigger_buffer.get(), entries.get(),
                    count * sizeof(struct dirent *));

    capacity = new_capacity;
    entries = bigger_buffer.take_unique();
    return true;
  }

public:
  LIBC_INLINE constexpr ScannedEntries() = default;
  ScannedEntries(const ScannedEntries &) = delete;
  ScannedEntries &operator=(const ScannedEntries &) = delete;
  ScannedEntries(ScannedEntries &&) = delete;
  ScannedEntries &operator=(ScannedEntries &&) = delete;

  LIBC_INLINE ~ScannedEntries() {
    // Match the uint8_t[] dynamic allocation type when freeing entries.
    for (size_t i = 0; i < count; ++i)
      delete[] reinterpret_cast<uint8_t *>(entries[i]);
  }

  LIBC_INLINE constexpr size_t size() const { return count; }
  LIBC_INLINE constexpr struct dirent **data() const { return entries.get(); }

  [[nodiscard]] LIBC_INLINE bool
  push_back(cpp::unique_ptr<uint8_t[]> entry_buf) {
    if (LIBC_UNLIKELY(!ensure_capacity()))
      return false;

    entries[count++] = reinterpret_cast<struct dirent *>(entry_buf.release());
    return true;
  }

  LIBC_INLINE struct dirent **release() {
    count = 0;
    capacity = 0;
    return entries.release();
  }
};

template <typename DirType>
LIBC_INLINE ErrorOr<DirScanResult> scan_impl(
    const char *name, int (*filter)(const struct dirent *) = nullptr,
    int (*compare)(const struct dirent **, const struct dirent **) = nullptr) {
  const auto res_open = DirType::open(name);
  if (LIBC_UNLIKELY(!res_open))
    return LIBC_NAMESPACE::Error(res_open.error());

  cpp::unique_ptr<DirType, DirCloser<DirType>> dir(res_open.value());
  ScannedEntries entries;

  constexpr size_t MAX_ENTRIES =
      static_cast<size_t>(cpp::numeric_limits<int>::max());

  while (true) {
    const auto res_read = dir->read();
    if (LIBC_UNLIKELY(!res_read))
      return LIBC_NAMESPACE::Error(res_read.error());

    struct dirent *const entry = res_read.value();
    if (entry == nullptr)
      break;

    if (filter != nullptr && !filter(entry))
      continue;

    // Scandir must return the number of entries as int, anything
    // above that is non-representable.
    if (LIBC_UNLIKELY(entries.size() == MAX_ENTRIES))
      return LIBC_NAMESPACE::Error(EOVERFLOW);

    const size_t reclen = DirType::reclen(entry);
    // Allocate only the variable-length record size (reclen) to save memory
    // whilst heap allocation guarantees max_align_t alignment for struct
    // dirent.
    CheckedPtr<uint8_t[]> new_entry_buf =
        make_unique_for_overwrite<uint8_t[]>(reclen);
    if (LIBC_UNLIKELY(!new_entry_buf))
      return LIBC_NAMESPACE::Error(ENOMEM);

    inline_memcpy(new_entry_buf.get(), entry, reclen);
    if (LIBC_UNLIKELY(!entries.push_back(new_entry_buf.take_unique())))
      return LIBC_NAMESPACE::Error(ENOMEM);
  }

  dir.reset();

  const size_t num_entries = entries.size();
  if (compare != nullptr && num_entries > 1) {
    // POSIX scandir/alphasort specifies 'const struct dirent **' rather than
    // 'const struct dirent * const *'.  Because the inner pointer is non-const,
    // converting from the sort engine's read-only 'const void *' requires
    // const_cast to satisfy C++ qualification rules (see Austin Group #142:
    // https://austingroupbugs.net/view.php?id=142).
    auto cmp_fn = [compare](const void *a, const void *b) {
      auto left = static_cast<const struct dirent **>(const_cast<void *>(a));
      auto right = static_cast<const struct dirent **>(const_cast<void *>(b));
      return compare(left, right);
    };
    internal::unstable_sort(entries.data(), num_entries,
                            sizeof(struct dirent *), cmp_fn);
  }

  return DirScanResult{entries.release(), num_entries};
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_DIR_SCAN_IMPL_H
