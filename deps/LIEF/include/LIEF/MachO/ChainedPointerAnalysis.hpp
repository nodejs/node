/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_MACHO_CHAINED_PTR_ANALYSIS_H
#define LIEF_MACHO_CHAINED_PTR_ANALYSIS_H
#include <memory>
#include <ostream>
#include <functional>
#include <cstring>

#include "LIEF/MachO/DyldChainedFormat.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class BinaryStream;
namespace MachO {
class LIEF_API ChainedPointerAnalysis {
  public:
  // DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E
  struct dyld_chained_ptr_arm64e_rebase_t
  {
    uint64_t  target : 43,
              high8  :  8,
              next   : 11,
              bind   :  1,
              auth   :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_rebase_t& chain);

    uint64_t unpack_target() const {
      return uint64_t(high8) | target;
    }
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E
  struct dyld_chained_ptr_arm64e_bind_t
  {
    uint64_t  ordinal : 16,
              zero    : 16,
              addend  : 19,
              next    : 11,
              bind    :  1,
              auth    :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_bind_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E
  struct dyld_chained_ptr_arm64e_auth_rebase_t
  {
    uint64_t  target    : 32,
              diversity : 16,
              addr_div  :  1,
              key       :  2,
              next      : 11,
              bind      :  1,
              auth      :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_auth_rebase_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E
  struct dyld_chained_ptr_arm64e_auth_bind_t
  {
    uint64_t  ordinal   : 16,
              zero      : 16,
              diversity : 16,
              addr_div  :  1,
              key       :  2,
              next      : 11,
              bind      :  1,
              auth      :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_auth_bind_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_64 & DYLD_CHAINED_PTR_FORMAT::PTR_64_OFFSET
  struct dyld_chained_ptr_64_rebase_t
  {
    uint64_t  target    : 36,
              high8     :  8,
              reserved  :  7,
              next      : 12,
              bind      :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_64_rebase_t& chain);

    uint64_t unpack_target() const {
      return uint64_t(high8) | target;
    }
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24
  struct dyld_chained_ptr_arm64e_bind24_t
  {
    uint64_t    ordinal : 24,
                zero    :  8,
                addend  : 19,
                next    : 11,
                bind    :  1,
                auth    :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_bind24_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24
  struct dyld_chained_ptr_arm64e_auth_bind24_t
  {
    uint64_t  ordinal   : 24,
              zero      :  8,
              diversity : 16,
              addr_div  :  1,
              key       :  2,
              next      : 11,
              bind      :  1,
              auth      :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_auth_bind24_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_64
  struct dyld_chained_ptr_64_bind_t
  {
    uint64_t  ordinal   : 24,
              addend    :  8,
              reserved  : 19,
              next      : 12,
              bind      :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_64_bind_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_64_KERNEL_CACHE
  struct dyld_chained_ptr_64_kernel_cache_rebase_t
  {
    uint64_t  target      : 30,
              cache_level :  2,
              diversity   : 16,
              addr_div    :  1,
              key         :  2,
              next        : 12,
              is_auth     :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_64_kernel_cache_rebase_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_32
  struct dyld_chained_ptr_32_rebase_t
  {
    uint32_t  target : 26,
              next   :  5,
              bind   :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_32_rebase_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_32
  struct dyld_chained_ptr_32_bind_t
  {
    uint32_t  ordinal : 20,
              addend  :  6,
              next    :  5,
              bind    :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_32_bind_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_32_CACHE
  struct dyld_chained_ptr_32_cache_rebase_t
  {
    uint32_t  target : 30,
              next   :  2;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_32_cache_rebase_t& chain);
  };

  // DYLD_CHAINED_PTR_FORMAT::PTR_32_FIRMWARE
  struct dyld_chained_ptr_32_firmware_rebase_t
  {
    uint32_t  target : 26,
              next   :  6;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_32_firmware_rebase_t& chain);
  };

  // DYLD_CHAINED_PTR_ARM64E_SEGMENTED
  struct dyld_chained_ptr_arm64e_segmented_rebase_t
  {
    uint32_t    target_seg_offset : 28,
                target_seg_index  :  4;
    uint32_t    padding           : 19,
                next              : 12,
                auth              :  1;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_segmented_rebase_t& chain);
  };

  // DYLD_CHAINED_PTR_ARM64E_SEGMENTED
  struct dyld_chained_ptr_arm64e_auth_segmented_rebase_t
  {
      uint32_t    target_seg_offset : 28,
                  target_seg_index  :  4;
      uint32_t    diversity         : 16,
                  addr_div          :  1,
                  key               :  2,
                  next              : 12,
                  auth              :  1;
    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const dyld_chained_ptr_arm64e_auth_segmented_rebase_t& chain);
  };


  enum class PTR_TYPE : uint64_t {
    UNKNOWN = 0,
    DYLD_CHAINED_PTR_ARM64E_REBASE,
    DYLD_CHAINED_PTR_ARM64E_BIND,
    DYLD_CHAINED_PTR_ARM64E_AUTH_REBASE,
    DYLD_CHAINED_PTR_ARM64E_AUTH_BIND,
    DYLD_CHAINED_PTR_64_REBASE,
    DYLD_CHAINED_PTR_ARM64E_BIND24,
    DYLD_CHAINED_PTR_ARM64E_AUTH_BIND24,
    DYLD_CHAINED_PTR_64_BIND,
    DYLD_CHAINED_PTR_64_KERNEL_CACHE_REBASE,
    DYLD_CHAINED_PTR_32_REBASE,
    DYLD_CHAINED_PTR_32_BIND,
    DYLD_CHAINED_PTR_32_CACHE_REBASE,
    DYLD_CHAINED_PTR_32_FIRMWARE_REBASE,
    DYLD_CHAINED_PTR_ARM64E_SEGMENTED_REBASE,
    DYLD_CHAINED_PTR_ARM64E_AUTH_SEGMENTED_REBASE,
  };

  static std::unique_ptr<ChainedPointerAnalysis> from_value(uint64_t value,
                                                            size_t size)
  {
    return std::unique_ptr<ChainedPointerAnalysis>(
        new ChainedPointerAnalysis(value, size));
  }

  static size_t stride(DYLD_CHAINED_PTR_FORMAT fmt) {
    switch (fmt) {
        case DYLD_CHAINED_PTR_FORMAT::NONE:
          return 0;
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SHARED_CACHE:
          return 8;

        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_KERNEL:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_FIRMWARE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_32_FIRMWARE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_64:
        case DYLD_CHAINED_PTR_FORMAT::PTR_64_OFFSET:
        case DYLD_CHAINED_PTR_FORMAT::PTR_32:
        case DYLD_CHAINED_PTR_FORMAT::PTR_32_CACHE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_64_KERNEL_CACHE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SEGMENTED:
            return 4;

        case DYLD_CHAINED_PTR_FORMAT::PTR_X86_64_KERNEL_CACHE:
            return 1;
    }
    return 0;
  }

  static size_t ptr_size(DYLD_CHAINED_PTR_FORMAT fmt) {
    switch (fmt) {
        case DYLD_CHAINED_PTR_FORMAT::NONE:
          return 0;
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_KERNEL:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_FIRMWARE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_64:
        case DYLD_CHAINED_PTR_FORMAT::PTR_64_OFFSET:
        case DYLD_CHAINED_PTR_FORMAT::PTR_64_KERNEL_CACHE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_X86_64_KERNEL_CACHE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SHARED_CACHE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SEGMENTED:
          return sizeof(uint64_t);

        case DYLD_CHAINED_PTR_FORMAT::PTR_32_FIRMWARE:
        case DYLD_CHAINED_PTR_FORMAT::PTR_32:
        case DYLD_CHAINED_PTR_FORMAT::PTR_32_CACHE:
          return sizeof(uint32_t);
    }
    return 0;
  }

  ChainedPointerAnalysis(uint64_t value, size_t size) :
    value_(value),
    size_(size)
  {}

  ChainedPointerAnalysis(const ChainedPointerAnalysis&) = default;
  ChainedPointerAnalysis& operator=(const ChainedPointerAnalysis&) = default;

  ChainedPointerAnalysis(ChainedPointerAnalysis&&) noexcept = default;
  ChainedPointerAnalysis& operator=(ChainedPointerAnalysis&&) noexcept = default;

  ~ChainedPointerAnalysis() = default;

  uint64_t value() const {
    return value_;
  }

  size_t size() const {
    return size_;
  }

  const dyld_chained_ptr_arm64e_rebase_t dyld_chained_ptr_arm64e_rebase() const {
    dyld_chained_ptr_arm64e_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_arm64e_bind_t dyld_chained_ptr_arm64e_bind() const {
    dyld_chained_ptr_arm64e_bind_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_arm64e_auth_rebase_t dyld_chained_ptr_arm64e_auth_rebase() const {
    dyld_chained_ptr_arm64e_auth_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_arm64e_auth_bind_t dyld_chained_ptr_arm64e_auth_bind() const {
    dyld_chained_ptr_arm64e_auth_bind_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_64_rebase_t dyld_chained_ptr_64_rebase() const {
    dyld_chained_ptr_64_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_arm64e_bind24_t dyld_chained_ptr_arm64e_bind24() const {
    dyld_chained_ptr_arm64e_bind24_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_arm64e_auth_bind24_t dyld_chained_ptr_arm64e_auth_bind24() const {
    dyld_chained_ptr_arm64e_auth_bind24_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_64_bind_t dyld_chained_ptr_64_bind() const {
    dyld_chained_ptr_64_bind_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_64_kernel_cache_rebase_t dyld_chained_ptr_64_kernel_cache_rebase() const {
    dyld_chained_ptr_64_kernel_cache_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_32_rebase_t dyld_chained_ptr_32_rebase() const {
    dyld_chained_ptr_32_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_32_bind_t dyld_chained_ptr_32_bind() const {
    dyld_chained_ptr_32_bind_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_32_cache_rebase_t dyld_chained_ptr_32_cache_rebase() const {
    dyld_chained_ptr_32_cache_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_32_firmware_rebase_t dyld_chained_ptr_32_firmware_rebase() const {
    dyld_chained_ptr_32_firmware_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_arm64e_segmented_rebase_t dyld_chained_ptr_arm64e_segmented_rebase() const {
    dyld_chained_ptr_arm64e_segmented_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  const dyld_chained_ptr_arm64e_auth_segmented_rebase_t dyld_chained_ptr_arm64e_auth_segmented_rebase() const {
    dyld_chained_ptr_arm64e_auth_segmented_rebase_t result;
    std::memcpy(&result, &value_, sizeof(result));
    return result;
  }

  struct union_pointer_t {
    PTR_TYPE type = PTR_TYPE::UNKNOWN;
    union {
      dyld_chained_ptr_arm64e_rebase_t arm64e_rebase;
      dyld_chained_ptr_arm64e_bind_t arm64e_bind;
      dyld_chained_ptr_arm64e_auth_rebase_t arm64e_auth_rebase;
      dyld_chained_ptr_arm64e_auth_bind_t arm64e_auth_bind;
      dyld_chained_ptr_64_rebase_t ptr_64_rebase;
      dyld_chained_ptr_arm64e_bind24_t arm64e_bind24;
      dyld_chained_ptr_arm64e_auth_bind24_t arm64e_auth_bind24;
      dyld_chained_ptr_64_bind_t ptr_64_bind;
      dyld_chained_ptr_64_kernel_cache_rebase_t ptr_64_kernel_cache_rebase;
      dyld_chained_ptr_32_rebase_t ptr_32_rebase;
      dyld_chained_ptr_32_bind_t ptr_32_bind;
      dyld_chained_ptr_32_cache_rebase_t ptr_32_cache_rebase;
      dyld_chained_ptr_32_firmware_rebase_t ptr_32_firmware_rebase;

      dyld_chained_ptr_arm64e_segmented_rebase_t ptr_arm64e_segmented_rebase;
      dyld_chained_ptr_arm64e_auth_segmented_rebase_t ptr_arm64e_auth_segmented_rebase;
      uint64_t raw;
    };
    uint32_t next() const;

    result<uint32_t> ordinal() const;
    result<uint64_t> target() const;

    bool is_bind() const {
      return (bool)ordinal();
    }
    bool is_auth() const;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const union_pointer_t& ptr);
  };

  static_assert(sizeof(union_pointer_t) == 16);

  union_pointer_t get_as(DYLD_CHAINED_PTR_FORMAT fmt) const;

  static uint64_t walk_chain(
      BinaryStream& stream, DYLD_CHAINED_PTR_FORMAT format,
      const std::function<int(uint64_t, const union_pointer_t& ptr)>& callback);

  private:
  uint64_t value_ = 0;
  size_t size_ = 0;
};
}
}
#endif
