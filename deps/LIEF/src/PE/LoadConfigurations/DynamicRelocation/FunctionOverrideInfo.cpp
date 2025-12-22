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
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/FunctionOverrideInfo.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"

namespace LIEF::PE {

FunctionOverrideInfo::FunctionOverrideInfo(
  uint32_t original_rva, uint32_t bdd_offset, uint32_t base_reloc_size
) :
  original_rva_(original_rva),
  bdd_offset_(bdd_offset),
  base_relocsz_(base_reloc_size)
{}

FunctionOverrideInfo::FunctionOverrideInfo(const FunctionOverrideInfo& other) :
  original_rva_(other.original_rva_),
  bdd_offset_(other.bdd_offset_),
  base_relocsz_(other.base_relocsz_),
  rvas_(other.rvas_)
{
  if (!other.relocations_.empty()) {
    relocations_.reserve(other.relocations_.size());
    std::transform(other.relocations_.begin(), other.relocations_.end(),
                   std::back_inserter(relocations_),
      [] (const std::unique_ptr<Relocation>& R) {
        return std::make_unique<Relocation>(*R);
      }
    );
  }
}

FunctionOverrideInfo& FunctionOverrideInfo::operator=(const FunctionOverrideInfo& other) {
  if (&other == this) {
    return *this;
  }

  original_rva_ = other.original_rva_;
  bdd_offset_ = other.bdd_offset_;
  base_relocsz_ = other.base_relocsz_;
  rvas_ = other.rvas_;

  if (!other.relocations_.empty()) {
    relocations_.reserve(other.relocations_.size());
    std::transform(other.relocations_.begin(), other.relocations_.end(),
                   std::back_inserter(relocations_),
      [] (const std::unique_ptr<Relocation>& R) {
        return std::make_unique<Relocation>(*R);
      }
    );
  }

  return *this;
}

FunctionOverrideInfo::FunctionOverrideInfo(FunctionOverrideInfo&&) = default;
FunctionOverrideInfo& FunctionOverrideInfo::operator=(FunctionOverrideInfo&&) = default;

FunctionOverrideInfo::~FunctionOverrideInfo() = default;

std::string FunctionOverrideInfo::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << format("Original RVA:   0x{:08x}\n", original_rva())
      << format("BDD Offset:     0x{:08x}\n", bdd_offset())
      << format("RVA array size: {}\n", rva_size())
      << format("Reloc size:     0x{:04x}\n", base_reloc_size());
  if (!rvas_.empty()) {
    oss << "RVAs:\n";
    for (size_t i = 0; i < rvas_.size(); ++i) {
      oss << format("  [{:04d}]: 0x{:08x}\n", i, rvas_[i]);
    }
  }
  if (!relocations_.empty()) {
    oss << "Fixup RVAs:\n";
    for (size_t i = 0; i < relocations_.size(); ++i) {
      const Relocation& R = *relocations_[i];
      oss << format("  [{:04d}]: RVA: 0x{:08x}\n", i, R.virtual_address());
      for (const RelocationEntry& E : R.entries()) {
        oss << format("    0x{:08x} ({})\n", E.address(), PE::to_string(E.type()));
      }
    }
  }
  return oss.str();
}

std::unique_ptr<FunctionOverrideInfo>
  FunctionOverrideInfo::parse(Parser& ctx, SpanStream& strm)
{
  auto OriginalRva = strm.read<uint32_t>();
  if (!OriginalRva) {
    LIEF_DEBUG("Can't read 'IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.OriginalRva");
    return nullptr;
  }

  auto BDDOffset = strm.read<uint32_t>();
  if (!BDDOffset) {
    LIEF_DEBUG("Can't read 'IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.BDDOffset");
    return nullptr;
  }

  auto RvaSize = strm.read<uint32_t>();
  if (!RvaSize) {
    LIEF_DEBUG("Can't read 'IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.RvaSize");
    return nullptr;
  }

  auto BaseRelocSize = strm.read<uint32_t>();
  if (!BaseRelocSize) {
    LIEF_DEBUG("Can't read 'IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.BaseRelocSize");
    return nullptr;
  }

  auto func_override_info = std::make_unique<FunctionOverrideInfo>(
    *OriginalRva, *BDDOffset, *BaseRelocSize
  );


  LIEF_DEBUG("OriginalRva: 0x{:08x}", *OriginalRva);

  if (*RvaSize % sizeof(uint32_t) != 0) {
    LIEF_WARN("IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.RvaSize misaligned: 0x{:08x}", *RvaSize);
  }

  const size_t count = *RvaSize / sizeof(uint32_t);
  const size_t reserved = std::min<size_t>(count, 100);
  func_override_info->rvas_.reserve(reserved);

  for (size_t i = 0; i < count; ++i) {
    auto RVA = strm.read<uint32_t>();
    if (!RVA) {
      LIEF_WARN("Cant read IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.RVAs[{}]", i);
      return func_override_info;
    }
    LIEF_DEBUG("IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.RVA[{}]: 0x{:08x}", i, *RVA);
    func_override_info->rvas_.push_back(*RVA);
  }

  auto BaseRelocs = strm.slice(strm.pos(), *BaseRelocSize);
  if (!BaseRelocs) {
    LIEF_WARN("Can't slice IMAGE_FUNCTION_OVERRIDE_DYNAMIC_RELOCATION.BaseRelocs");
    return func_override_info;
  }

  strm.increment_pos(*BaseRelocSize);

  Relocation::relocations_t relocations = Relocation::parse(ctx, *BaseRelocs);
  func_override_info->relocations_ = std::move(relocations);
  return func_override_info;
}

}
