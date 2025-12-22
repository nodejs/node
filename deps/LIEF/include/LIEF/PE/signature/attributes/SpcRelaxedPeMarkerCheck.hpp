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
#ifndef LIEF_PE_ATTRIBUTES_SPC_RELAXED_PE_MARKER_CHECK_H
#define LIEF_PE_ATTRIBUTES_SPC_RELAXED_PE_MARKER_CHECK_H
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/PE/signature/Attribute.hpp"

namespace LIEF {
namespace PE {

class LIEF_API SpcRelaxedPeMarkerCheck : public Attribute {
  friend class Parser;
  friend class SignatureParser;

  public:
  SpcRelaxedPeMarkerCheck() :
    SpcRelaxedPeMarkerCheck(0)
  {}

  SpcRelaxedPeMarkerCheck(uint32_t value) :
    Attribute(Attribute::TYPE::SPC_RELAXED_PE_MARKER_CHECK),
    value_(value)
  {}

  SpcRelaxedPeMarkerCheck(const SpcRelaxedPeMarkerCheck&) = default;
  SpcRelaxedPeMarkerCheck& operator=(const SpcRelaxedPeMarkerCheck&) = default;

  uint32_t value() const {
    return value_;
  }

  void value(uint32_t v) {
    value_ = v;
  }

  std::unique_ptr<Attribute> clone() const override {
    return std::unique_ptr<Attribute>(new SpcRelaxedPeMarkerCheck{*this});
  }

  std::string print() const override {
    return "value=" + std::to_string(value_);
  }

  static bool classof(const Attribute* attr) {
    return attr->type() == Attribute::TYPE::SPC_RELAXED_PE_MARKER_CHECK;
  }

  void accept(Visitor& visitor) const override;

  ~SpcRelaxedPeMarkerCheck() override = default;

  private:
  uint32_t value_ = 0;
};

}
}

#endif
