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
#include <cassert>

#include "LIEF/utils.hpp"

#include "LIEF/COFF/AuxiliarySymbol.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryCLRToken.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryFunctionDefinition.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliarybfAndefSymbol.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryWeakExternal.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryFile.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliarySectionDefinition.hpp"
#include "LIEF/COFF/Symbol.hpp"

#include "internal_utils.hpp"

namespace LIEF::COFF {

std::unique_ptr<AuxiliarySymbol>
  AuxiliarySymbol::parse(Symbol& sym, std::vector<uint8_t> payload)
{
  assert(payload.size() >= 18);
  const TYPE ty = get_aux_type(sym);

  switch (ty) {
    case TYPE::CLR_TOKEN:
      return AuxiliaryCLRToken::parse(payload);

    case TYPE::FUNC_DEF:
      return AuxiliaryFunctionDefinition::parse(payload);

    case TYPE::BF_AND_EF:
      return AuxiliarybfAndefSymbol::parse(sym, std::move(payload));

    case TYPE::WEAK_EXTERNAL:
      return AuxiliaryWeakExternal::parse(payload);

    case TYPE::FILE:
      return AuxiliaryFile::parse(payload);

    case TYPE::SEC_DEF:
      return AuxiliarySectionDefinition::parse(payload);

    case TYPE::UNKNOWN:
      return std::make_unique<AuxiliarySymbol>(std::move(payload));
  }

  return std::make_unique<AuxiliarySymbol>(std::move(payload));
}

AuxiliarySymbol::TYPE AuxiliarySymbol::get_aux_type(const Symbol& sym) {
  if (sym.storage_class() == Symbol::STORAGE_CLASS::STATIC) {
    return TYPE::SEC_DEF;
  }

  if (sym.is_external() && sym.is_absolute()) {
    return TYPE::SEC_DEF;
  }

  if (sym.storage_class() == Symbol::STORAGE_CLASS::CLR_TOKEN) {
    return TYPE::CLR_TOKEN;
  }

  if (sym.is_undefined() || sym.is_weak_external()) {
    return TYPE::WEAK_EXTERNAL;
  }

  if (sym.is_external() &&
      sym.base_type() == Symbol::BASE_TYPE::TY_NULL &&
      sym.complex_type() == Symbol::COMPLEX_TYPE::TY_FUNCTION &&
      !Symbol::is_reversed_sec_idx(sym.section_idx()))
  {
    return TYPE::FUNC_DEF;
  }

  if (sym.is_function_line_info()) {
    return TYPE::BF_AND_EF;
  }

  if (sym.is_file_record()) {
    return TYPE::FILE;
  }

  return TYPE::UNKNOWN;
}

std::string AuxiliarySymbol::to_string() const {
  std::string out = "AuxiliarySymbol {\n";
  out += indent(dump(payload()), 2);
  out += "}\n";
  return out;
}


}
