/* Copyright 2021 - 2025 R. Thomas
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
#include "logging.hpp"
#include "LIEF/MachO/DyldChainedFormat.hpp"
#include "MachO/ChainedFixup.hpp"

namespace LIEF {
namespace MachO {
namespace details {

uint64_t sign_extended_addend(const dyld_chained_ptr_arm64e_bind24& bind) {
  const uint64_t addend19 = bind.addend;
  if ((addend19 & 0x40000) > 0) {
    return addend19 | 0xFFFFFFFFFFFC0000ULL;
  }
  return addend19;
}

uint64_t sign_extended_addend(const dyld_chained_ptr_arm64e_bind& bind) {
  const uint64_t addend19 = bind.addend;
  if ((addend19 & 0x40000) > 0) {
    return addend19 | 0xFFFFFFFFFFFC0000ULL;
  }
  return addend19;
}

uint64_t sign_extended_addend(const dyld_chained_ptr_64_bind& bind) {
  uint64_t addend27 = bind.addend;
  uint64_t top8     = addend27 & 0x00007F80000ULL;
  uint64_t bottom19 = addend27 & 0x0000007FFFFULL;
  return (top8 << 13) | (((bottom19 << 37) >> 37) & 0x00FFFFFFFFFFFFFF);
}

uint64_t dyld_chained_ptr_arm64e::sign_extended_addend() const {
  return details::sign_extended_addend(bind);
}

uint64_t dyld_chained_ptr_arm64e::unpack_target() const {
  return static_cast<uint64_t>(rebase.high8) << 56 | rebase.target;
}

void dyld_chained_ptr_arm64e::pack_target(uint64_t value) {
  return details::pack_target(rebase, value);
}


uint64_t dyld_chained_ptr_generic64::sign_extended_addend() const {
  return details::sign_extended_addend(bind);
}

uint64_t dyld_chained_ptr_generic64::unpack_target() const {
  return static_cast<uint64_t>(rebase.high8) << 56 | rebase.target;
}

void dyld_chained_ptr_generic64::pack_target(uint64_t value) {
  return details::pack_target(rebase, value);
}

bool chained_fixup::is_rebase(uint16_t ptr_format) const {
  /* This is a *modified* mirror of `MachOLoaded.cpp:ChainedFixupPointerOnDisk::isRebase`
   * As we don't need to compute targetRuntimeOffset, we removed the code sections
   * that referenced this variable
   */
  const auto ptr_fmt = static_cast<DYLD_CHAINED_PTR_FORMAT>(ptr_format);

  switch (ptr_fmt) {
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_KERNEL:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_FIRMWARE:
      {
        if (arm64e.bind.bind > 0) {
          return false;
        }

        if (arm64e.auth_rebase.auth > 0) {
          return true;
        }

        return true;
        break;
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_64:
    case DYLD_CHAINED_PTR_FORMAT::PTR_64_OFFSET:
      {
        return generic64.bind.bind == 0;
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_32:
      {
        return generic32.bind.bind == 0;
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_64_KERNEL_CACHE:
    case DYLD_CHAINED_PTR_FORMAT::PTR_X86_64_KERNEL_CACHE:
      {
        return true;
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_32_FIRMWARE:
      {
        return true;
      }
    case LIEF::MachO::DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SEGMENTED:
      return true;

    default:
      {
        LIEF_ERR("Unknown pointer format: 0x{:04x}", ptr_format);
        std::abort();
      }
  }
}

}
}
}
