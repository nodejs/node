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
#ifndef LIEF_DEX_CODE_INFO_H
#define LIEF_DEX_CODE_INFO_H

#include <cstdint>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace DEX {
namespace details {
struct code_item;
}

class Parser;

class LIEF_API CodeInfo : public Object {
  friend class Parser;

  public:
  CodeInfo();
  CodeInfo(const details::code_item& codeitem);

  CodeInfo(const CodeInfo&);
  CodeInfo& operator=(const CodeInfo&);

  void accept(Visitor& visitor) const override;

  uint16_t nb_registers() const;

  ~CodeInfo() override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const CodeInfo& cinfo);

  private:
  uint16_t nb_registers_ = 0;
  uint16_t args_input_sizes_ = 0;
  uint16_t output_sizes_ = 0;

};

} // Namespace DEX
} // Namespace LIEF
#endif
