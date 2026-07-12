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
#include "LIEF/MachO/ParserConfig.hpp"

namespace LIEF {
namespace MachO {

ParserConfig ParserConfig::deep() {
  ParserConfig conf;
  conf.parse_dyld_exports  = true;
  conf.parse_dyld_bindings = true;
  conf.parse_dyld_rebases  = true;
  conf.fix_from_memory     = true;
  return conf;
}

ParserConfig ParserConfig::quick() {
  ParserConfig conf;
  conf.parse_dyld_exports  = false;
  conf.parse_dyld_bindings = false;
  conf.parse_dyld_rebases  = false;
  conf.fix_from_memory     = false;
  return conf;
}


ParserConfig& ParserConfig::full_dyldinfo(bool flag) {
  if (flag) {
    parse_dyld_exports  = true;
    parse_dyld_bindings = true;
    parse_dyld_rebases  = true;
  } else {
    parse_dyld_exports  = false;
    parse_dyld_bindings = false;
    parse_dyld_rebases  = false;
  }
  return *this;
}


} //namespace MachO
}
