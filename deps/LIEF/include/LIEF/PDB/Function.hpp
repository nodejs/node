/* Copyright 2022 - 2025 R. Thomas
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

#include "LIEF/visibility.h"
#include "LIEF/debug_loc.hpp"

namespace LIEF {
namespace pdb {

namespace details {
class Function;
class FunctionIt;
}

class LIEF_API Function {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::unique_ptr<Function>;
    using difference_type = std::ptrdiff_t;
    using pointer = Function*;
    using reference = Function&;
    using implementation = details::FunctionIt;

    class LIEF_API PointerProxy {
      // Inspired from LLVM's iterator_facade_base
      friend class Iterator;
      public:
      pointer operator->() const { return R.get(); }

      private:
      value_type R;

      template <typename RefT>
      PointerProxy(RefT &&R) : R(std::forward<RefT>(R)) {} // NOLINT(bugprone-forwarding-reference-overload)
    };

    Iterator(const Iterator&);
    Iterator(Iterator&&);
    Iterator(std::unique_ptr<details::FunctionIt> impl);
    ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    Iterator& operator++();

    Iterator operator++(int) {
      Iterator tmp = *static_cast<Iterator*>(this);
      ++*static_cast<Iterator *>(this);
      return tmp;
    }

    std::unique_ptr<Function> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::FunctionIt> impl_;
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

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const Function& F)
  {
    os << F.to_string();
    return os;
  }

  private:
  std::unique_ptr<details::Function> impl_;
};

}
}
#endif

