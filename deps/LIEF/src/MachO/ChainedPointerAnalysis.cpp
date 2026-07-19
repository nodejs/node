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
#include <cassert>

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/MachO/ChainedPointerAnalysis.hpp"
#include <spdlog/fmt/fmt.h>

namespace LIEF::MachO {
std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_rebase_t& chain)
{
  os << fmt::format("target: 0x{:010x} high8: 0x{:02x}, next: 0x{:03x}, "
                    "bind: {}, auth: {}",
    chain.target, chain.high8, chain.next, (bool)chain.bind, (bool)chain.auth
  );
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_bind_t& chain)
{
  os << fmt::format("ordinal: 0x{:04x} zero: 0x{:04x}, addend: 0x{:05x}, "
                    "next: 0x{:03x} bind: {}, auth: {}",
    chain.ordinal, chain.zero, chain.addend, chain.next,
    (bool)chain.bind, (bool)chain.auth
  );
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_rebase_t& chain)
{
  os << fmt::format("target: 0x{:08x} diversity: 0x{:04x}, addr_div: {}, "
                    "key: 0x{:x} next: 0x{:03x} bind: {}, auth: {}",
    chain.target, chain.diversity, chain.addr_div, chain.key, chain.next,
    (bool)chain.bind, (bool)chain.auth
  );
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_bind_t& chain)
{
  os << fmt::format("ordinal: 0x{:04x} zero: 0x{:04x}, diversity: 0x{:04x}, "
                    "addr_div: {} key: 0x{:x} next: 0x{:03x} bind: {}, auth: {}",
    chain.ordinal, chain.zero, chain.diversity, chain.addr_div, chain.key,
    chain.next, (bool)chain.bind, (bool)chain.auth
  );
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_64_rebase_t& chain)
{
  os << fmt::format("target: 0x{:010x} high8: 0x{:02x}, reserved: 0x{:02x}, "
                    "next: 0x{:04x} bind: {}",
    chain.target, chain.high8, chain.reserved, chain.next, (bool)chain.bind);
  return os;
}
std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_bind24_t& chain)
{

  os << fmt::format("ordinal: 0x{:06x} zero: 0x{:02x}, addend: 0x{:05x}, "
                    "next: 0x{:03x} bind: {}, auth: {}",
    chain.ordinal, chain.zero, chain.addend, chain.next, (bool)chain.bind,
    (bool)chain.auth);
  return os;
}
std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_bind24_t& chain)
{
  os << fmt::format("ordinal: 0x{:06x} zero: 0x{:02x}, diversity: 0x{:04x}, "
                    "addr_div: {}, key: 0x{:x}, next: 0x{:03x} bind: {}, auth: {}",
    chain.ordinal, chain.zero, chain.diversity, chain.addr_div, chain.key,
    chain.next, (bool)chain.bind, (bool)chain.auth);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_64_bind_t& chain)
{
  os << fmt::format("ordinal: 0x{:06x} addend: 0x{:04x}, reserved: 0x{:05x}, "
                    "next: 0x{:04x} bind: {}",
    chain.ordinal, chain.addend, chain.reserved, chain.next, (bool)chain.bind);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_64_kernel_cache_rebase_t& chain)
{
  os << fmt::format("target: 0x{:08x} cache_level: {}, diversity: 0x{:04x}, "
                    "addr_div: {} key: {} next: 0x{:03x}, auth: {}",
    chain.target, chain.cache_level, chain.diversity, chain.addr_div,
    chain.key, chain.next, (bool)chain.is_auth);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_32_rebase_t& chain)
{
  os << fmt::format("target: 0x{:08x} next: 0x{:02x}, bind: {}",
    chain.target, chain.next, (bool)chain.bind);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_32_bind_t& chain)
{
  os << fmt::format("ordinal: 0x{:05x} addend: 0x{:02x}, next: 0x{:x}, bind: {}",
    chain.ordinal, chain.addend, chain.next, (bool)chain.bind);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_32_cache_rebase_t& chain)
{
  os << fmt::format("target: 0x{:06x}, next: 0x{:x}",
                     chain.target, chain.next);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_32_firmware_rebase_t& chain)
{
  os << fmt::format("target: 0x{:06x}, next: 0x{:x}",
                     chain.target, chain.next);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_segmented_rebase_t& chain)
{
  os << fmt::format("segment offset: 0x{:06x}, segment index: {}, next: 0x{:x}",
                     chain.target_seg_offset, chain.target_seg_index, chain.next);
  return os;
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_segmented_rebase_t& chain)
{
  os << fmt::format("segment offset: 0x{:06x}, segment index: {}, next: 0x{:x}, "
                    "addr_div: {} key: {} diversity: {}, auth: {}",
                     chain.target_seg_offset, chain.target_seg_index, chain.next,
                     chain.addr_div, chain.key, chain.diversity, chain.auth);
  return os;
}

template<class T>
ChainedPointerAnalysis::union_pointer_t create_impl(const T& value) {
  ChainedPointerAnalysis::union_pointer_t out;
  out.raw = 0;
  out.type = ChainedPointerAnalysis::PTR_TYPE::UNKNOWN;

  if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_REBASE;
    out.arm64e_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_bind_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND;
    out.arm64e_bind = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_REBASE;
    out.arm64e_auth_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_bind_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND;
    out.arm64e_auth_bind = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_64_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_64_REBASE;
    out.ptr_64_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_bind24_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND24;
    out.arm64e_bind24 = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_bind24_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND24;
    out.arm64e_auth_bind24 = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_64_bind_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_64_BIND;
    out.ptr_64_bind = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_64_kernel_cache_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_64_KERNEL_CACHE_REBASE;
    out.ptr_64_kernel_cache_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_32_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_REBASE;
    out.ptr_32_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_32_bind_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_BIND;
    out.ptr_32_bind = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_32_cache_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_CACHE_REBASE;
    out.ptr_32_cache_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_32_firmware_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_FIRMWARE_REBASE;
    out.ptr_32_firmware_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_segmented_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_SEGMENTED_REBASE;
    out.ptr_arm64e_segmented_rebase = value;
  }

  else if constexpr (std::is_same_v<T,
      ChainedPointerAnalysis::dyld_chained_ptr_arm64e_auth_segmented_rebase_t>)
  {
    out.type = ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_SEGMENTED_REBASE;
    out.ptr_arm64e_auth_segmented_rebase = value;
  }

  return out;
}


ChainedPointerAnalysis::union_pointer_t ChainedPointerAnalysis::get_as(DYLD_CHAINED_PTR_FORMAT fmt) const {
  switch (fmt) {
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_KERNEL:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
      {
        if ((bool)dyld_chained_ptr_arm64e_auth_rebase().auth &&
            (bool)dyld_chained_ptr_arm64e_auth_rebase().bind)
        {
          if (fmt == DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24) {
            return create_impl(dyld_chained_ptr_arm64e_auth_bind24());
          }
          return create_impl(dyld_chained_ptr_arm64e_auth_bind());
        }

        if ( dyld_chained_ptr_arm64e_auth_rebase().auth &&
            !dyld_chained_ptr_arm64e_auth_rebase().bind)
        {
          return create_impl(dyld_chained_ptr_arm64e_auth_rebase());
        }
        if (!dyld_chained_ptr_arm64e_auth_rebase().auth &&
             dyld_chained_ptr_arm64e_auth_rebase().bind)
        {
          if (fmt == DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24) {
            return create_impl(dyld_chained_ptr_arm64e_bind24());
          }
          return create_impl(dyld_chained_ptr_arm64e_bind());
        }

        if (!dyld_chained_ptr_arm64e_auth_rebase().auth &&
            !dyld_chained_ptr_arm64e_auth_rebase().bind)
        {
          return create_impl(dyld_chained_ptr_arm64e_rebase());
        }

        return {};
      }

    case DYLD_CHAINED_PTR_FORMAT::PTR_64:
    case DYLD_CHAINED_PTR_FORMAT::PTR_64_OFFSET:
      {
        if (dyld_chained_ptr_64_rebase().bind) {
          return create_impl(dyld_chained_ptr_64_bind());
        }
        return create_impl(dyld_chained_ptr_64_rebase());
      }

    case DYLD_CHAINED_PTR_FORMAT::PTR_32:
      {
        if (dyld_chained_ptr_32_bind().bind) {
          return create_impl(dyld_chained_ptr_32_bind());
        }
        return create_impl(dyld_chained_ptr_32_rebase());
      }

    case DYLD_CHAINED_PTR_FORMAT::PTR_32_CACHE:
      return create_impl(dyld_chained_ptr_32_cache_rebase());

    case DYLD_CHAINED_PTR_FORMAT::PTR_32_FIRMWARE:
      return create_impl(dyld_chained_ptr_32_firmware_rebase());

    case DYLD_CHAINED_PTR_FORMAT::PTR_64_KERNEL_CACHE:
      return create_impl(dyld_chained_ptr_64_kernel_cache_rebase());

    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SEGMENTED:
      {
        if (dyld_chained_ptr_arm64e_segmented_rebase().auth) {
          return create_impl(dyld_chained_ptr_arm64e_auth_segmented_rebase());
        }
        return create_impl(dyld_chained_ptr_arm64e_segmented_rebase());
      }

    default:
      return {};
  }
  return {};
}

uint32_t ChainedPointerAnalysis::union_pointer_t::next() const {
  switch (type) {
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_REBASE:
      return arm64e_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND:
      return arm64e_bind.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_REBASE:
      return arm64e_auth_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND:
      return arm64e_auth_bind.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_64_REBASE:
      return ptr_64_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND24:
      return arm64e_bind24.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND24:
      return arm64e_auth_bind24.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_64_BIND:
      return ptr_64_bind.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_64_KERNEL_CACHE_REBASE:
      return ptr_64_kernel_cache_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_REBASE:
      return ptr_32_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_BIND:
      return ptr_32_bind.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_CACHE_REBASE:
      return ptr_32_cache_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_FIRMWARE_REBASE:
      return ptr_32_firmware_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_SEGMENTED_REBASE:
      return ptr_arm64e_segmented_rebase.next;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_SEGMENTED_REBASE:
      return ptr_arm64e_auth_segmented_rebase.next;
    case PTR_TYPE::UNKNOWN:
      return 0;
  }
  return 0;
}

result<uint32_t> ChainedPointerAnalysis::union_pointer_t::ordinal() const {
  switch (type) {
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND:
      return arm64e_bind.ordinal;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND:
      return arm64e_auth_bind.ordinal;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND24:
      return arm64e_bind24.ordinal;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND24:
      return arm64e_auth_bind24.ordinal;
    case PTR_TYPE::DYLD_CHAINED_PTR_64_BIND:
      return ptr_64_bind.ordinal;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_BIND:
      return ptr_32_bind.ordinal;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_64_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_32_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_64_KERNEL_CACHE_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_32_FIRMWARE_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_32_CACHE_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_SEGMENTED_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_SEGMENTED_REBASE:
    case PTR_TYPE::UNKNOWN:
      return make_error_code(lief_errors::not_found);
  }
  return make_error_code(lief_errors::not_found);
}

bool ChainedPointerAnalysis::union_pointer_t::is_auth() const {
  switch (type) {
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_REBASE:
      return arm64e_rebase.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND:
      return arm64e_bind.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_REBASE:
      return arm64e_auth_rebase.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND:
      return arm64e_auth_bind.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND24:
      return arm64e_bind24.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND24:
      return arm64e_auth_bind24.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_SEGMENTED_REBASE:
      return ptr_arm64e_segmented_rebase.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_SEGMENTED_REBASE:
      return ptr_arm64e_auth_segmented_rebase.auth;
    case PTR_TYPE::DYLD_CHAINED_PTR_64_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_64_BIND:
    case PTR_TYPE::DYLD_CHAINED_PTR_64_KERNEL_CACHE_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_32_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_32_BIND:
    case PTR_TYPE::DYLD_CHAINED_PTR_32_CACHE_REBASE:
    case PTR_TYPE::DYLD_CHAINED_PTR_32_FIRMWARE_REBASE:
    case PTR_TYPE::UNKNOWN:
      return false;
  }
  return false;
}

result<uint64_t> ChainedPointerAnalysis::union_pointer_t::target() const {
  switch (type) {
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_REBASE:
      return arm64e_rebase.unpack_target();
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_REBASE:
      return arm64e_auth_rebase.target;
    case PTR_TYPE::DYLD_CHAINED_PTR_64_REBASE:
      return ptr_64_rebase.unpack_target();
    case PTR_TYPE::DYLD_CHAINED_PTR_64_KERNEL_CACHE_REBASE:
      return ptr_64_kernel_cache_rebase.target;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_REBASE:
      return ptr_32_rebase.target;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_CACHE_REBASE:
      return ptr_32_cache_rebase.target;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_FIRMWARE_REBASE:
      return ptr_32_firmware_rebase.target;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_SEGMENTED_REBASE:
      return ptr_arm64e_segmented_rebase.target_seg_offset;
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_SEGMENTED_REBASE:
      return ptr_arm64e_auth_segmented_rebase.target_seg_offset;
    case PTR_TYPE::DYLD_CHAINED_PTR_32_BIND:
    case PTR_TYPE::DYLD_CHAINED_PTR_64_BIND:
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND24:
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND:
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND24:
    case PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND:
    case PTR_TYPE::UNKNOWN:
      return make_error_code(lief_errors::not_found);
  }
  return make_error_code(lief_errors::not_found);
}

std::ostream& operator<<(std::ostream& os,
    const ChainedPointerAnalysis::union_pointer_t& ptr)
{
  switch (ptr.type) {
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_REBASE:
      os << ptr.arm64e_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND:
      os << ptr.arm64e_bind;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_REBASE:
      os << ptr.arm64e_auth_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND:
      os << ptr.arm64e_auth_bind;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_64_REBASE:
      os << ptr.ptr_64_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_BIND24:
      os << ptr.arm64e_bind24;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_BIND24:
      os << ptr.arm64e_auth_bind24;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_64_BIND:
      os << ptr.ptr_64_bind;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_64_KERNEL_CACHE_REBASE:
      os << ptr.ptr_64_kernel_cache_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_REBASE:
      os << ptr.ptr_32_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_BIND:
      os << ptr.ptr_32_bind;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_CACHE_REBASE:
      os << ptr.ptr_32_cache_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_32_FIRMWARE_REBASE:
      os << ptr.ptr_32_firmware_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_SEGMENTED_REBASE:
      os << ptr.ptr_arm64e_segmented_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::DYLD_CHAINED_PTR_ARM64E_AUTH_SEGMENTED_REBASE:
      os << ptr.ptr_arm64e_auth_segmented_rebase;
      break;
    case ChainedPointerAnalysis::PTR_TYPE::UNKNOWN:
      break;
  }
  return os;
}

uint64_t ChainedPointerAnalysis::walk_chain(
    BinaryStream& stream, DYLD_CHAINED_PTR_FORMAT format,
    const std::function<int(uint64_t, const union_pointer_t& ptr)>& callback)
{
  const size_t ptr_sizeof = ChainedPointerAnalysis::ptr_size(format);
  const size_t stride = ChainedPointerAnalysis::stride(format);
  assert(ptr_sizeof == 8 || ptr_sizeof == 4);
  const uint64_t start_pos = stream.pos();
  while (1) {
    uint64_t value = ptr_sizeof == sizeof(uint64_t) ?
                                   stream.peek<uint64_t>().value_or(0) :
                                   stream.peek<uint32_t>().value_or(0);
    if (value == 0) {
      break;
    }
    ChainedPointerAnalysis analysis(value, ptr_sizeof);
    union_pointer_t ptr = analysis.get_as(format);
    const uint64_t offset = stream.pos();
    if (callback(offset, ptr)) {
      return start_pos - stream.pos();
    }

    const uint32_t next = ptr.next() * stride;
    if (next == 0) {
      break;
    }

    if (!stream.can_read(stream.pos() + next, ptr_sizeof)) {
      break;
    }

    stream.increment_pos(next);
  }
  return stream.pos() - start_pos;
}
}
