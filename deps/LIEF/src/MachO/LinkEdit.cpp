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
#include "logging.hpp"
#include "LIEF/errors.hpp"

#include "LIEF/MachO/AtomInfo.hpp"
#include "LIEF/MachO/CodeSignature.hpp"
#include "LIEF/MachO/CodeSignatureDir.hpp"
#include "LIEF/MachO/DataInCode.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/FunctionStarts.hpp"
#include "LIEF/MachO/FunctionVariants.hpp"
#include "LIEF/MachO/FunctionVariantFixups.hpp"
#include "LIEF/MachO/LinkEdit.hpp"
#include "LIEF/MachO/LinkerOptHint.hpp"
#include "LIEF/MachO/SegmentSplitInfo.hpp"
#include "LIEF/MachO/SymbolCommand.hpp"
#include "LIEF/MachO/TwoLevelHints.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"

namespace LIEF {
namespace MachO {

/* The DyldInfo object has span fields (rebase_opcodes_, ...) that point to segment data.
 * When resizing the ``SegmentCommand.data_`` we can break this span as the internal buffer of ``data_``
 * might be relocated.
 *
 * The following helpers keep an internal consistent state of the data
 */

inline ok_error_t update_span(span<uint8_t>& sp, uintptr_t original_data_addr,
                              uintptr_t original_data_end, std::vector<uint8_t>& new_data)
{
  auto span_data_addr = reinterpret_cast<uintptr_t>(sp.data());
  const bool is_encompassed = original_data_addr <= span_data_addr && span_data_addr < original_data_end;
  if (!is_encompassed) {
    return ok();
  }

  const uintptr_t original_size = original_data_end - original_data_addr;
  /*
   * Resize of the container without relocating
   */
  if (new_data.data() == sp.data() && new_data.size() >= original_size) {
    return ok();
  }

  const uintptr_t delta = span_data_addr - original_data_addr;
  const bool fit_in_data = delta < new_data.size() && (delta + original_size) <= new_data.size();
  if (!fit_in_data) {
    sp = {new_data.data(), static_cast<size_t>(0)};
    return make_error_code(lief_errors::corrupted);
  }

  sp = {new_data.data() + delta, sp.size()};
  return ok();
}

/// @param[in] offset    Offset where the insertion took place
/// @param[in] size      Size of the inserted data
inline ok_error_t update_span(span<uint8_t>& sp, uintptr_t original_data_addr, uintptr_t original_data_end,
                              size_t offset, size_t size, std::vector<uint8_t>& new_data)
{
  auto span_data_addr = reinterpret_cast<uintptr_t>(sp.data());
  const bool is_encompassed = original_data_addr <= span_data_addr && span_data_addr < original_data_end;
  if (!is_encompassed) {
    // No need to re-span
    return ok();
  }
  // Original relative offset of the span
  const uintptr_t original_rel_offset = span_data_addr - original_data_addr;
  uintptr_t delta_offset = 0;

  // If the insertion took place BEFORE our span,
  // we need to append the insertion size in the new span
  if (offset <= original_rel_offset) {
    delta_offset = size;
  }

  const bool fit_in_data = (original_rel_offset + delta_offset) < new_data.size() &&
                           (original_rel_offset + delta_offset + sp.size()) < new_data.size();
  if (!fit_in_data) {
    sp = {new_data.data(), static_cast<size_t>(0)};
    return make_error_code(lief_errors::corrupted);
  }

  sp = {new_data.data() + original_rel_offset + delta_offset, sp.size()};
  return ok();
}

LinkEdit& LinkEdit::operator=(LinkEdit other) {
  swap(other);
  return *this;
}

void LinkEdit::swap(LinkEdit& other) noexcept {
  SegmentCommand::swap(other);
  std::swap(dyld_,           other.dyld_);
  std::swap(chained_fixups_, other.chained_fixups_);
}

void LinkEdit::update_data(const update_fnc_t& f) {
  const auto original_data_addr     = reinterpret_cast<uintptr_t>(data_.data());
  const auto original_data_size     = static_cast<size_t>(data_.size());
  const uintptr_t original_data_end = original_data_addr + original_data_size;
  f(data_);
  if (dyld_ != nullptr) {
    if (!update_span(dyld_->rebase_opcodes_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning rebase opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->bind_opcodes_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning bind opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->weak_bind_opcodes_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning weak bind opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->lazy_bind_opcodes_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning lazy bind opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->export_trie_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the export trie in segment {}", name_);
    }
  }

  if (chained_fixups_ != nullptr) {
    if (!update_span(chained_fixups_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the Dyld Chained fixups in segment {}", name_);
    }
  }

  if (exports_trie_ != nullptr) {
    if (!update_span(exports_trie_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the Dyld Exports Trie in segment {}", name_);
    }
  }

  if (symtab_ != nullptr) {
    if (!update_span(symtab_->symbol_table_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_SYMTAB.n_list in segment {}", name_);
    }
    if (!update_span(symtab_->string_table_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_SYMTAB.string_table in segment {}", name_);
    }
  }

  if (fstarts_ != nullptr) {
    if (!update_span(fstarts_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_FUNCTION_STARTS in segment {}", name_);
    }
  }

  if (data_code_ != nullptr) {
    if (!update_span(data_code_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_DATA_IN_CODE in segment {}", name_);
    }
  }

  if (seg_split_ != nullptr) {
    if (!update_span(seg_split_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_SEGMENT_SPLIT_INFO in segment {}", name_);
    }
  }

  if (two_lvl_hint_ != nullptr) {
    if (!update_span(two_lvl_hint_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_TWOLEVEL_HINTS in segment {}", name_);
    }
  }

  if (linker_opt_ != nullptr) {
    if (!update_span(linker_opt_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_LINKER_OPTIMIZATION_HINT in segment {}", name_);
    }
  }

  if (code_sig_ != nullptr) {
    if (!update_span(code_sig_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_CODE_SIGNATURE in segment {}", name_);
    }
  }

  if (code_sig_dir_ != nullptr) {
    if (!update_span(code_sig_dir_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_DYLIB_CODE_SIGN_DRS in segment {}", name_);
    }
  }

  if (atom_info_ != nullptr) {
    if (!update_span(atom_info_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_ATOM_INFO in segment {}", name_);
    }
  }

  if (func_variants_ != nullptr) {
    if (!update_span(func_variants_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_FUNCTION_VARIANTS in segment {}", name_);
    }
  }

  if (func_variant_fixups_ != nullptr) {
    if (!update_span(func_variant_fixups_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_FUNCTION_VARIANT_FIXUPS in segment {}", name_);
    }
  }
}

void LinkEdit::update_data(const update_fnc_ws_t& f, size_t where, size_t size) {
  const auto original_data_addr     = reinterpret_cast<uintptr_t>(data_.data());
  const auto original_data_size     = static_cast<size_t>(data_.size());
  const uintptr_t original_data_end = original_data_addr + original_data_size;
  f(data_, where, size);
  if (dyld_ != nullptr) {
    if (!update_span(dyld_->rebase_opcodes_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning rebase opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->bind_opcodes_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning bind opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->weak_bind_opcodes_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning weak bind opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->lazy_bind_opcodes_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning lazy bind opcodes in segment {}", name_);
    }
    if (!update_span(dyld_->export_trie_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the export trie in segment {}", name_);
    }
  }

  if (chained_fixups_ != nullptr) {
    if (!update_span(chained_fixups_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the Dyld Chained fixups in segment {}", name_);
    }
  }

  if (exports_trie_ != nullptr) {
    if (!update_span(exports_trie_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the Dyld Exports Trie in segment {}", name_);
    }
  }

  if (symtab_ != nullptr) {
    if (!update_span(symtab_->symbol_table_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_SYMTAB.n_list in segment {}", name_);
    }
    if (!update_span(symtab_->string_table_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_SYMTAB.string_table in segment {}", name_);
    }
  }

  if (fstarts_ != nullptr) {
    if (!update_span(fstarts_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_FUNCTION_STARTS in segment {}", name_);
    }
  }

  if (data_code_ != nullptr) {
    if (!update_span(data_code_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_DATA_IN_CODE in segment {}", name_);
    }
  }

  if (seg_split_ != nullptr) {
    if (!update_span(seg_split_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_SEGMENT_SPLIT_INFO in segment {}", name_);
    }
  }

  if (two_lvl_hint_ != nullptr) {
    if (!update_span(two_lvl_hint_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_TWOLEVEL_HINTS in segment {}", name_);
    }
  }

  if (linker_opt_ != nullptr) {
    if (!update_span(linker_opt_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_LINKER_OPTIMIZATION_HINT in segment {}", name_);
    }
  }

  if (code_sig_ != nullptr) {
    if (!update_span(code_sig_->content_, original_data_addr, original_data_end, where, size, data_)) {
      LIEF_WARN("Error while re-spanning the LC_CODE_SIGNATURE in segment {}", name_);
    }
  }

  if (code_sig_dir_ != nullptr) {
    if (!update_span(code_sig_dir_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_DYLIB_CODE_SIGN_DRS in segment {}", name_);
    }
  }

  if (atom_info_ != nullptr) {
    if (!update_span(atom_info_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_ATOM_INFO in segment {}", name_);
    }
  }

  if (func_variants_ != nullptr) {
    if (!update_span(func_variants_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_FUNCTION_VARIANTS in segment {}", name_);
    }
  }

  if (func_variant_fixups_ != nullptr) {
    if (!update_span(func_variant_fixups_->content_, original_data_addr, original_data_end, data_)) {
      LIEF_WARN("Error while re-spanning the LC_FUNCTION_VARIANT_FIXUPS in segment {}", name_);
    }
  }
}

}
}
