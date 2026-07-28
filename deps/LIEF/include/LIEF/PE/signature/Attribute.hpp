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
#ifndef LIEF_PE_ATTRIBUTES_H
#define LIEF_PE_ATTRIBUTES_H
#include <memory>
#include <string>
#include <ostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

/// Interface over PKCS #7 attribute
class LIEF_API Attribute : public Object {

  friend class Parser;
  friend class SignatureParser;

  public:
  enum class TYPE {
    UNKNOWN = 0,
    CONTENT_TYPE,
    GENERIC_TYPE,
    SIGNING_CERTIFICATE_V2,

    SPC_SP_OPUS_INFO,
    SPC_RELAXED_PE_MARKER_CHECK,

    MS_COUNTER_SIGN,
    MS_SPC_NESTED_SIGN,
    MS_SPC_STATEMENT_TYPE,
    MS_PLATFORM_MANIFEST_BINARY_ID,

    PKCS9_AT_SEQUENCE_NUMBER,
    PKCS9_COUNTER_SIGNATURE,
    PKCS9_MESSAGE_DIGEST,
    PKCS9_SIGNING_TIME,
  };

  Attribute() = delete;
  Attribute(const Attribute&) = default;
  Attribute& operator=(const Attribute&) = default;

  virtual std::unique_ptr<Attribute> clone() const = 0;

  /// Concrete type of the attribute
  virtual TYPE type() const {
    return type_;
  }

  /// Print information about the underlying attribute
  virtual std::string print() const = 0;

  void accept(Visitor& visitor) const override;

  ~Attribute() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const Attribute& attribute) {
    os << attribute.print();
    return os;
  }

  template<class T>
  const T* cast() const {
    static_assert(std::is_base_of<Attribute, T>::value,
                  "Require Attribute inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* cast() {
    return const_cast<T*>(static_cast<const Attribute*>(this)->cast<T>());
  }

  protected:
  Attribute(TYPE type) :
    type_(type)
  {}
  TYPE type_ = TYPE::UNKNOWN;
};

LIEF_API const char* to_string(Attribute::TYPE e);

}
}

#endif
