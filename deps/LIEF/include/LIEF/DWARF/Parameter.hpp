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
#ifndef LIEF_DWARF_PARAMETER_H
#define LIEF_DWARF_PARAMETER_H

#include "LIEF/visibility.h"

#include <memory>
#include <string>

namespace LIEF {
namespace dwarf {

class Type;

namespace details {
class Parameter;
}

/// This class represents a DWARF parameter which can be either:
/// - A regular function parameter (see: parameters::Formal)
/// - A template type parameter (see: parameters::TemplateType)
/// - A template value parameter (see: parameters::TemplateValue)
class LIEF_API Parameter {
  public:
  enum class KIND {
    UNKNOWN = 0,
    TEMPLATE_TYPE,  ///< DW_TAG_template_type_parameter
    TEMPLATE_VALUE, ///< DW_TAG_template_value_parameter
    FORMAL,         ///< DW_TAG_formal_parameter
  };
  Parameter() = delete;
  Parameter(Parameter&& other);
  Parameter& operator=(Parameter&& other);
  Parameter& operator=(const Parameter&) = delete;
  Parameter(const Parameter&) = delete;

  KIND kind() const;

  /// Name of the parameter
  std::string name() const;

  /// Type of this parameter
  std::unique_ptr<Type> type() const;

  template<class T>
  const T* as() const {
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  virtual ~Parameter();

  LIEF_LOCAL static
    std::unique_ptr<Parameter> create(std::unique_ptr<details::Parameter> impl);

  protected:
  Parameter(std::unique_ptr<details::Parameter> impl);
  std::unique_ptr<details::Parameter> impl_;
};

namespace parameters {

/// This class represents a regular function parameter.
///
/// For instance, given this prototype:
///
/// ```cpp
/// int main(int argc, const char** argv);
/// ```
///
/// The function `main` has two parameters::Formal parameters:
///
/// 1. `argc` (Parameter::name) typed as `int` (types::Base from Parameter::type)
/// 2. `argv` (Parameter::name) typed as `const char**`
///    (types::Const from Parameter::type)
class LIEF_API Formal : public Parameter {
  public:
  using Parameter::Parameter;
  static bool classof(const Parameter* P) {
    return P->kind() == Parameter::KIND::FORMAL;
  }

  ~Formal() override = default;
};


/// This class represents a template **value** parameter.
///
/// For instance, given this prototype:
///
/// ```cpp
/// template<int X = 5>
/// void generic();
/// ```
///
/// The function `generic` has one parameters::TemplateValue parameter: `X`.
class LIEF_API TemplateValue : public Parameter {
  public:
  using Parameter::Parameter;
  static bool classof(const Parameter* P) {
    return P->kind() == Parameter::KIND::TEMPLATE_VALUE;
  }

  ~TemplateValue() override = default;
};

/// This class represents a template **type** parameter.
///
/// For instance, given this prototype:
///
/// ```cpp
/// template<class Y>
/// void generic();
/// ```
///
/// The function `generic` has one parameters::TemplateType parameter: `Y`.
class LIEF_API TemplateType : public Parameter {
public:
  using Parameter::Parameter;
  static bool classof(const Parameter* P) {
    return P->kind() == Parameter::KIND::TEMPLATE_TYPE;
  }

  ~TemplateType() override = default;
};

}

}
}
#endif
