/* Copyright 2021 - 2025 R. Thomas
 * Copyright 2021 - 2025 Quarkslab
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
#include "LIEF/errors.hpp"

const char* to_string(lief_errors err) {
  switch (err) {
    case lief_errors::read_error:               return "read_error";
    case lief_errors::not_found:                return "not_found";
    case lief_errors::not_implemented:          return "not_implemented";
    case lief_errors::not_supported:            return "not_supported";
    case lief_errors::corrupted:                return "corrupted";
    case lief_errors::conversion_error:         return "conversion_error";
    case lief_errors::read_out_of_bound:        return "read_out_of_bound";
    case lief_errors::asn1_bad_tag:             return "asn1_bad_tag";
    case lief_errors::file_error:               return "file_error";
    case lief_errors::file_format_error:        return "file_format_error";
    case lief_errors::parsing_error:            return "parsing_error";
    case lief_errors::build_error:              return "build_error";
    case lief_errors::data_too_large:           return "data_too_large";
    case lief_errors::require_extended_version: return "require_extended_version";
    default: return "error";
  }
  return "error";
}

