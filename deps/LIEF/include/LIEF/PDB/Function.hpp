/* Copyright 2022 - 2026 R. Thomas
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
#ifndef LIEF_PDB_FUNCTION_H
#define LIEF_PDB_FUNCTION_H
#include <memory>
#include <string>
#include <ostream>

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"
#include "LIEF/debug_loc.hpp"
#include "LIEF/DebugDeclOpt.hpp"

namespace LIEF {
namespace pdb {

namespace details {
class Function;
class FunctionIt;
}

class LIEF_API Function {
  public:
  class Iterator final
    : public iterator_facade_base<Iterator, std::forward_iterator_tag, Function,
                                  std::ptrdiff_t, const Function*,
                                  const Function&> {
    public:
    using implementation = details::FunctionIt;
    using iterator_facade_base::operator++;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::FunctionIt> impl);

    LIEF_API Iterator(const Iterator&);
    LIEF_API Iterator& operator=(const Iterator&);

    LIEF_API Iterator(Iterator&&) noexcept;
    LIEF_API Iterator& operator=(Iterator&&) noexcept;

    LIEF_API ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator++();

    LIEF_API const Function& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const Function* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the function at the current position to the
    /// caller. Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<Function> yield();

    private:
    void load() const;

    std::unique_ptr<details::FunctionIt> impl_;
    mutable std::unique_ptr<Function> cached_;
  };
  Function(std::unique_ptr<details::Function> impl);
  ~Function();

  /// The name of the function (this name is usually demangled)
  std::string name() const;

  /// The **Relative** Virtual Address of the function
  uint32_t RVA() const;

  /// The size of the function
  uint32_t code_size() const;

  /// The name of the section in which this function is defined
  std::string section_name() const;

  /// Original source code location
  debug_location_t debug_location() const;

  /// Generate a C/C++ definition for this function
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Function& F) {
    os << F.to_string();
    return os;
  }

  private:
  std::unique_ptr<details::Function> impl_;
};

}
}
#endif
