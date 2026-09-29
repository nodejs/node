/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#include "LIEF/runtime/Process.hpp"
#include "LIEF/runtime/Memory.hpp"
#include "LIEF/runtime/utils.hpp"
#include "logging.hpp"

#include <sys/mman.h>

namespace LIEF::runtime {

inline int get_posix_flags(uint32_t flags) {
  int prot = PROT_NONE;
  if (flags & Memory::P_READ) {
    prot |= PROT_READ;
  }
  if (flags & Memory::P_WRITE) {
    prot |= PROT_WRITE;
  }
  if (flags & Memory::P_EXEC) {
    prot |= PROT_EXEC;
  }
  return prot;
}

inline int get_posix_mmap_flags(uint32_t flags) {
  int ret = 0;
  if (flags & Memory::MP_PRIVATE) {
    ret |= MAP_PRIVATE;
  }

  if (flags & Memory::MP_ANONYMOUS) {
    ret |= MAP_ANONYMOUS;
  }

  if (flags & Memory::MP_SHARED) {
    ret |= MAP_SHARED;
  }

  if (flags & Memory::MP_FIXED) {
    ret |= MAP_FIXED;
  }

  if (flags & Memory::MP_JIT) {
    ret |= MAP_JIT;
  }

  return ret;
}

std::string Memory::perm_str(uint32_t flags) {
  std::string flags_str = "---";
  if (flags & P_READ) {
    flags_str[0] = 'r';
  }

  if (flags & P_WRITE) {
    flags_str[1] = 'w';
  }

  if (flags & P_EXEC) {
    flags_str[2] = 'x';
  }
  return flags_str;
}


uintptr_t Memory::Chunk::page_start() const {
  if (!is_valid()) {
    return 0;
  }
  return runtime::page_start(addr(), Process::page_size());
}

uintptr_t Memory::Chunk::page_end() const {
  if (!is_valid()) {
    return 0;
  }
  return runtime::page_end(addr() + size(), Process::page_size());
}

std::string Memory::Chunk::to_string() const {
  if (!is_valid()) {
    return "<empty chunk>";
  }

  if (size() == 0) {
    return fmt::format("[0x{:016x}, ...]", addr());
  }

  return fmt::format("[0x{:016x}, 0x{:016x}]", addr(), addr() + size());
}

optional<Memory::Chunk> Memory::mmap(size_t size, uint32_t flags, uint32_t perms) {
  if (size == 0) {
    return nullopt();
  }

  void* ret = ::mmap(/*__addr=*/nullptr, /*__len=*/size, get_posix_flags(perms),
                     get_posix_mmap_flags(flags),
                     /*__fd=*/-1, /*__offset=*/0);
  if (ret == nullptr || reinterpret_cast<intptr_t>(ret) == -1) {
    LIEF_ERR("mmap failed: {}", strerror(errno));
    return nullopt();
  }
  return Chunk(ret, size, perms);
}

ok_error_t Memory::munmap(Chunk& C) {
  if (!C.is_valid()) {
    LIEF_DEBUG("Error: {}:{} - Invalid chunk", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::runtime_error);
  }

  if (C.size() == 0) {
    LIEF_DEBUG("Error: {}:{} - Missing chunk size", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::runtime_error);
  }

  int ret = ::munmap(C.addr_ptr(), C.size());
  if (ret != 0) {
    LIEF_ERR("munmap {} failed: {}", C.to_string(), strerror(errno));
    return make_error_code(lief_errors::runtime_error);
  }

  C.addr_ = nullptr;
  C.size_ = 0;
  return ok();
}

ok_error_t Memory::mprotect(Memory::Chunk& C, uint32_t flags) {
  if (!C.is_valid()) {
    LIEF_DEBUG("Error: {}:{} - Invalid chunk", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::runtime_error);
  }

  if (C.size() == 0) {
    LIEF_DEBUG("Error: {}:{} - Missing chunk size", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::runtime_error);
  }

  int posix_flags = get_posix_flags(flags);
  const uintptr_t pstart = C.page_start();
  const uintptr_t pend = C.page_end();
  const uintptr_t len = pend - pstart;

  int ret = ::mprotect((void*)pstart, len, posix_flags);
  if (ret != 0) {
    LIEF_ERR("mprotect [0x{:016x}, 0x{:016x}] -> {} ({:04b}) failed: {} ({})",
             pstart, pend, perm_str(flags), posix_flags, ret, strerror(errno));
    return make_error_code(lief_errors::runtime_error);
  }
  return ok();
}

Memory::Chunk& Memory::Chunk::cache_flush() {
  const uintptr_t pstart = page_start();
  const uintptr_t pend = page_end();
  __builtin___clear_cache((char*)pstart, (char*)pend);
  return *this;
}

}
