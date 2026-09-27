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
#ifndef LIEF_RUNTIME_MEMORY_H
#define LIEF_RUNTIME_MEMORY_H
#include <ostream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <utility>

#include "LIEF/visibility.h"
#include "LIEF/optional.hpp"
#include "LIEF/logging.hpp"
#include "LIEF/compiler_attributes.hpp"

#include "LIEF/runtime/utils.hpp"

namespace LIEF {
namespace runtime {

/// This class exposes API to access and manage memory
class LIEF_API Memory {
  public:
  /// Flags used when creating a memory map (mmap).
  enum MMAP_FLAGS : uint32_t {
    MP_NONE = 0,

    /// Changes are private to this process (copy-on-write).
    MP_PRIVATE = 1 << 0,

    /// The mapping is not backed by any file.
    MP_ANONYMOUS = 1 << 1,

    /// Changes are shared.
    MP_SHARED = 1 << 2,

    /// Interpret the address as a fixed requirement.
    MP_FIXED = 1 << 3,

    /// Map for Just-In-Time code generation.
    MP_JIT = 1 << 4,
  };

  enum PERM : uint32_t {
    P_NONE = 0,
    P_READ = 1 << 0,
    P_WRITE = 1 << 1,
    P_EXEC = 1 << 2,
  };

  /// Represents a contiguous chunk of memory allocated or inspected by
  /// the runtime.
  class LIEF_API Chunk {
    public:
    friend class Memory;

    Chunk(void* addr, size_t size, uint32_t permissions) :
      addr_(addr),
      size_(size),
      permissions_(permissions) {}

    Chunk(void* addr, size_t size) :
      Chunk(addr, size, /*permissions=*/P_NONE) {}

    Chunk(void* addr) :
      Chunk(addr, /*size=*/0, /*permissions=*/P_NONE) {}

    /// Returns the start address of the memory chunk as an opaque pointer
    void* addr_ptr() {
      return addr_;
    }

    const void* addr_ptr() const {
      return addr_;
    }

    /// Returns the start address of the memory chunk.
    uintptr_t addr() const {
      return reinterpret_cast<uintptr_t>(addr_);
    }

    /// Returns the size of the memory chunk in bytes.
    size_t size() const {
      return size_;
    }

    /// Returns the current permissions of the memory chunk.
    uint32_t permissions() const {
      return permissions_;
    }

    /// Returns the address of the start of the page containing this chunk.
    uintptr_t page_start() const;


    /// Returns the address of the end of the page containing this chunk.
    uintptr_t page_end() const;

    /// Changes the permissions of the memory chunk.
    Chunk& change_permissions(uint32_t p) LIEF_LIFETIMEBOUND {
      if (!Memory::mprotect(*this, p)) {
        logging::err("mprotect failed for chunk: {}", to_string());
      }
      permissions_ = p;
      return *this;
    }

    /// Sets the permissions to Execute only.
    Chunk& make_x() LIEF_LIFETIMEBOUND {
      return change_permissions(P_EXEC);
    }

    /// Sets the permissions to Read and Write.
    Chunk& make_rw() LIEF_LIFETIMEBOUND {
      return change_permissions(P_READ | P_WRITE);
    }

    /// Sets the permissions to Read and Execute.
    Chunk& make_rx() LIEF_LIFETIMEBOUND {
      return change_permissions(P_READ | P_EXEC);
    }

    /// Sets the permissions to Read, Write, and Execute.
    Chunk& make_rwx() LIEF_LIFETIMEBOUND {
      return change_permissions(P_READ | P_WRITE | P_EXEC);
    }

    /// Sets the permissions to Read Only.
    Chunk& make_ro() LIEF_LIFETIMEBOUND {
      return change_permissions(P_READ);
    }

    /// Flushes the instruction cache for this memory chunk.
    /// This should be used when modifying code in memory (e.g., hooking, JIT).
    Chunk& cache_flush() LIEF_LIFETIMEBOUND;

    /// Check if this chunk is valid
    bool is_valid() const {
      return addr_ != nullptr;
    }

    operator bool() const {
      return is_valid();
    }

    ok_error_t deallocate() {
      return Memory::munmap(*this);
    }

    std::string to_string() const;

    friend LIEF_API std::ostream& operator<<(std::ostream& os, const Chunk& C) {
      os << C.to_string();
      return os;
    }

    protected:
    void* addr_ = 0;
    size_t size_ = 0;
    uint32_t permissions_ = P_NONE;
  };

  /// RAII interface to change the permission within a determined scope
  class LIEF_API ScopedPermissions {
    public:
    explicit ScopedPermissions(Chunk& chunk, uint32_t perms) :
      chunk_(&chunk),
      perms_(chunk.permissions()) {
      chunk_->change_permissions(perms);
    }

    ~ScopedPermissions() {
      chunk_->change_permissions(perms_);
    }

    private:
    Chunk* chunk_ = nullptr;
    uint32_t perms_ = P_NONE;
  };

  /// Allocate a memory chunk through mmap-like function
  static optional<Chunk> mmap(size_t size, uint32_t flags,
                              uint32_t permissions = P_NONE);

  /// Deallocate a mmaped memory chunk
  static ok_error_t munmap(Chunk& C);

  /// Sets the permission of the given memory chunk
  static ok_error_t mprotect(Chunk& C, uint32_t flags);

  /// Write the buffer at the address given in the third parameter.
  ///
  /// This function assumes that the memory pointed by `addr` has the correct
  /// permission to write this buffer.
  static ok_error_t write(const uint8_t* buffer, size_t size, uintptr_t addr) {
    std::memcpy(reinterpret_cast<void*>(addr), buffer, size);
    return ok();
  }

  /// Write the buffer at the address given in the third parameter.
  ///
  /// This function assumes that the memory pointed by `addr` has the correct
  /// permission to write this buffer.
  static ok_error_t write(const std::vector<uint8_t>& buffer, uintptr_t addr) {
    return write(buffer.data(), buffer.size(), addr);
  }

  /// Generic function to write a typed value
  template<class T,
           typename = typename std::enable_if<std::is_standard_layout<T>::value &&
                                              std::is_trivial<T>::value>::type>
  static ok_error_t write(const T& value, uintptr_t addr) {
    return write(reinterpret_cast<const uint8_t*>(&value), sizeof(T), addr);
  }

  /// Generic function to read a typed value
  template<class T>
  static T read(uintptr_t addr) {
    T out{};
    std::memcpy(&out, reinterpret_cast<const void*>(addr), sizeof(T));
    return out;
  }

  /// Read the content at the address pointed by the first parameter and write
  /// the result in the `std::vector` provided in the second parameter
  static void read(uintptr_t addr, std::vector<uint8_t>& out, size_t size) {
    if (out.size() < size) {
      std::vector<uint8_t> buffer(size);
      read(addr, buffer.data(), buffer.size());
      out = std::move(buffer);
      return;
    }
    read(addr, out.data(), size);
  }

  /// Read the content at the address pointed by the first parameter and write
  /// the result in the buffer provided in the second parameter.
  ///
  /// This function assumes that the buffer in the second parameter is large
  /// enough to contain the data being read.
  static void read(uintptr_t addr, uint8_t* out, size_t size) {
    std::memcpy(out, reinterpret_cast<const void*>(addr), size);
  }

  static std::string perm_str(uint32_t flags);

  static constexpr bool support_rwx() {
    return platform() != PLATFORMS::IOS;
  }
};

}
}
#endif
