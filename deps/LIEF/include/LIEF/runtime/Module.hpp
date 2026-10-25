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
#ifndef LIEF_RUNTIME_MODULE_H
#define LIEF_RUNTIME_MODULE_H
#include <ostream>
#include <fstream>
#include <cstdint>
#include <cstring>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/compiler_attributes.hpp"

#include <memory>
#include <string>
#include <vector>

namespace LIEF {
class Binary;
namespace runtime {

namespace details {
class ModuleIt;
class Module;
}

/// This class represents an in-memory module which can be an executable
/// or a library
class LIEF_API Module {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::forward_iterator_tag, Module,
                                  std::ptrdiff_t, const Module*, const Module&> {
    public:
    using implementation = details::ModuleIt;
    using iterator_facade_base::operator++;

    LIEF_API Iterator();

    LIEF_API Iterator(const Iterator&);
    LIEF_API Iterator& operator=(const Iterator&);

    LIEF_API Iterator(Iterator&&) noexcept;
    LIEF_API Iterator& operator=(Iterator&&) noexcept;

    LIEF_API Iterator(std::unique_ptr<details::ModuleIt> impl);
    LIEF_API ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator++();

    LIEF_API const Module& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Module* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the module at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Module> yield();

    private:
    void load() const;

    std::unique_ptr<details::ModuleIt> impl_;
    mutable std::unique_ptr<Module> cached_;
  };
  Module() = delete;
  Module(std::unique_ptr<details::Module> impl);

  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;

  Module(Module&&) noexcept;
  Module& operator=(Module&&) noexcept;

  virtual std::unique_ptr<Module> clone() const;

  /// Base address where the module is loaded in memory
  uint64_t imagebase() const;

  /// Virtual size of the current module
  uint64_t size() const;

  /// End address of the module
  uint64_t end() const {
    return imagebase() + size();
  }

  /// Name of the module (e.g. `libc.so.6, kernel32.dll, libsystem_c.dylib`)
  std::string name() const;

  /// Path of the module
  std::string path() const;

  /// Check if the current module contains the given address
  bool contains(uintptr_t addr) const {
    const uint64_t base = imagebase();
    return base <= addr && addr < (base + size());
  }

  /// Return the content of the module as it is currently mapped in memory.
  ///
  /// The returned buffer spans imagebase() over size() bytes. An empty buffer
  /// is returned if the imagebase or the size is null.
  std::vector<uint8_t> dump() const {
    const uint64_t base = imagebase();
    const uint64_t nbytes = size();
    if (base == 0 || nbytes == 0) {
      return {};
    }
    std::vector<uint8_t> out(nbytes);
    std::memcpy(out.data(), reinterpret_cast<const void*>(base), nbytes);
    return out;
  }

  /// Same as dump() but also writes the content into the file located at
  /// `filepath`.
  std::vector<uint8_t> dump(const std::string& filepath) const {
    std::vector<uint8_t> out = dump();
    std::ofstream ofs(filepath, std::ios::out | std::ios::binary);
    if (ofs) {
      ofs.write(reinterpret_cast<const char*>(out.data()),
                static_cast<std::streamsize>(out.size()));
    }
    return out;
  }

  /// Same as dump() but also writes the content into the given output stream.
  std::vector<uint8_t> dump(std::ostream& os) const {
    std::vector<uint8_t> out = dump();
    os.write(reinterpret_cast<const char*>(out.data()),
             static_cast<std::streamsize>(out.size()));
    return out;
  }

  std::string to_string() const;

  /// This function can be used to **downcast** a Module instance
  template<class T>
  const T* as() const LIEF_LIFETIMEBOUND {
    static_assert(std::is_base_of<Module, T>::value, "Require Module inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* as() LIEF_LIFETIMEBOUND {
    return const_cast<T*>(static_cast<const Module*>(this)->as<T>());
  }

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Module& M) {
    os << M.to_string();
    return os;
  }

  virtual ~Module();

  protected:
  std::unique_ptr<details::Module> impl_;
};

using modules_t = iterator_range<Module::Iterator>;

/// Return an iterator over the different modules loaded in the current
/// process
LIEF_API modules_t modules();

/// Find the module with the given name
inline std::unique_ptr<Module> module_from_name(const std::string& name) {
  auto range = modules();
  for (auto it = range.begin(); it != range.end(); ++it) {
    if (it->name() == name) {
      return it.yield();
    }
  }
  return nullptr;
}

/// Find the module with the given path
inline std::unique_ptr<Module> module_from_path(const std::string& path) {
  auto range = modules();
  for (auto it = range.begin(); it != range.end(); ++it) {
    if (it->path() == path) {
      return it.yield();
    }
  }
  return nullptr;
}

/// Find the module that encompasses the given virtual address (absolute)
inline std::unique_ptr<Module> module_from_addr(uintptr_t addr) {
  auto range = modules();
  for (auto it = range.begin(); it != range.end(); ++it) {
    if (it->contains(addr)) {
      return it.yield();
    }
  }
  return nullptr;
}

}
}
#endif
