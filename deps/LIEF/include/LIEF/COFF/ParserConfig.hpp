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
#ifndef LIEF_COFF_PARSER_CONFIG_H
#define LIEF_COFF_PARSER_CONFIG_H

#include "LIEF/visibility.h"

namespace LIEF {
namespace COFF {
/// Class used to configure the COFF parser
class LIEF_API ParserConfig {
  public:
  static const ParserConfig& default_conf() {
    static const ParserConfig DEFAULT;
    return DEFAULT;
  }

  static const ParserConfig& all() {
    // To be updated when there is options that are off by default
    return default_conf();
  }
};
}
}
#endif
