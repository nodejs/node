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
#ifndef LIEF_ABSTRACT_FUNCTION_H
#define LIEF_ABSTRACT_FUNCTION_H

#include <vector>
#include <string>

#include "LIEF/Abstract/Symbol.hpp"
#include "LIEF/visibility.h"
#include "LIEF/enums.hpp"

namespace LIEF {

/// Class that represents a function in the binary
class LIEF_API Function : public Symbol {
  public:
  /// Flags used to characterize the semantics of the function
  enum class FLAGS : uint32_t {
    NONE = 0,
    /// The function acts as a constructor.
    ///
    /// Usually this flag is associated with functions
    /// that are located in the `.init_array`, `__mod_init_func` or `.tls` sections
    CONSTRUCTOR = 1 << 0,

    /// The function acts as a destructor.
    ///
    /// Usually this flag is associated with functions
    /// that are located in the `.fini_array` or `__mod_term_func` sections
    DESTRUCTOR = 1 << 1,

    /// The function is associated with Debug information
    DEBUG_INFO = 1 << 2,

    /// The function is exported by the binary and the address() method
    /// returns its virtual address in the binary
    EXPORTED = 1 << 3,

    /// The function is **imported** by the binary and the address() should return 0
    IMPORTED = 1 << 4,
  };

  public:
  Function() = default;
  Function(const std::string& name) :
    Symbol(name)
  {}
  Function(uint64_t address) :
    Function("", address)
  {}
  Function(const std::string& name, uint64_t address) :
    Symbol(name, address)
  {}
  Function(const std::string& name, uint64_t address, FLAGS flags) :
    Function(name, address)
  {
    flags_ = flags;
  }

  Function(const Function&) = default;
  Function& operator=(const Function&) = default;

  ~Function() override = default;

  /// List of FLAGS
  std::vector<FLAGS> flags_list() const;

  FLAGS flags() const {
    return flags_;
  }

  /// Add a flag to the current function
  Function& add(FLAGS f) {
    flags_ = (FLAGS)((uint32_t)flags_ | (uint32_t)f);
    return *this;
  }

  /// Check if the function has the given flag
  bool has(FLAGS f) const {
    return ((uint32_t)flags_ & (uint32_t)f) != 0;
  }

  /// Address of the current function. For functions that are set with the FLAGS::IMPORTED flag,
  /// this value is likely 0.
  uint64_t address() const {
    return value_;
  }

  void address(uint64_t address) {
    value_ = address;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Function& entry);

  protected:
  FLAGS flags_ = FLAGS::NONE;
};

LIEF_API const char* to_string(Function::FLAGS e);
}

ENABLE_BITMASK_OPERATORS(LIEF::Function::FLAGS);

#endif

