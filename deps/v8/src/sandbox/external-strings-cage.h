// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_STRINGS_CAGE_H_
#define V8_SANDBOX_EXTERNAL_STRINGS_CAGE_H_

#include <stddef.h>

#include <memory>

#include "include/v8-internal.h"
#include "src/base/address-region.h"
#include "src/base/compiler-specific.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/utils/allocation.h"

namespace v8::internal {

#if defined(V8_ENABLE_SANDBOX) && defined(V8_ENABLE_MEMORY_CORRUPTION_API)

// Manages a virtual memory range for hosting external string contents, with an
// extra reservation at the end in order to fit any read past a string's buffer
// end using a corrupted length.
//
// Currently only used in memory_corruption_api-enabled builds, in order to
// distinguish external string OOB reads from other issues.
//
// Note: There's an additional memory overhead per each string, since we append
// a redzone and occupy whole pages for a string at the moment.
class V8_EXPORT_PRIVATE ExternalStringsCage final {
 public:
  // The maximum total length of strings (and additional redzones) that the cage
  // can fit. Chosen to fit a maximum UTF-16 string of length 2^32 and some
  // amount of extra space; increase this if it turns out to be insufficient for
  // testing of important use cases.
  static constexpr size_t kMaxContentsSize = (size_t{1} << 33) + GB;
  // The size of the guard region at the end of the cage. Chosen to cover an
  // arbitrary 32-bit offset for a UTF-16 string.
  static constexpr size_t kGuardRegionSize = size_t{1} << 33;

  template <typename T>
  class Deleter final {
   public:
    Deleter(ExternalStringsCage* cage, size_t size)
        : cage_(cage), size_(size) {}

    Deleter(const Deleter&) = delete;
    Deleter& operator=(const Deleter&) = delete;

    Deleter(Deleter&&) V8_NOEXCEPT = default;
    Deleter& operator=(Deleter&&) V8_NOEXCEPT = default;

    void operator()(T* p) const { cage_->Free(p, size_ * sizeof(T)); }

   private:
    ExternalStringsCage* const cage_;
    const size_t size_;
  };

  ExternalStringsCage();
  ~ExternalStringsCage();

  ExternalStringsCage(const ExternalStringsCage&) = delete;
  ExternalStringsCage& operator=(const ExternalStringsCage&) = delete;

  bool Initialize();

  // Allocates a buffer for a string of `size` characters with the `T` type.
  // Returns null if `size` is zero.
  template <typename T>
  std::unique_ptr<T[], Deleter<T>> Allocate(size_t size) {
    CHECK_LE(size, kMaxContentsSize / sizeof(T));
    return std::unique_ptr<T[], Deleter<T>>(
        static_cast<T*>(AllocateRaw(size * sizeof(T))), Deleter<T>(this, size));
  }

  // Makes the memory pages read-only.
  V8_EXPORT_PRIVATE void Seal(void* ptr, size_t size);

  base::AddressRegion reservation_region() const {
    CHECK(vm_cage_.IsReserved());
    return vm_cage_.region();
  }

 private:
  size_t GetAllocSize(size_t string_size) const;
  V8_EXPORT_PRIVATE void* AllocateRaw(size_t size);
  V8_EXPORT_PRIVATE void Free(void* ptr, size_t size);

  const size_t page_size_;
  VirtualMemoryCage vm_cage_;
};

#endif  // defined(V8_ENABLE_SANDBOX) &&
        // defined(V8_ENABLE_MEMORY_CORRUPTION_API)

}  // namespace v8::internal

#endif  // V8_SANDBOX_EXTERNAL_STRINGS_CAGE_H_
