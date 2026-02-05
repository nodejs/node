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
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/FunctionOverride.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/FunctionOverrideInfo.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF::PE {
using image_bdd_info_t = FunctionOverride::image_bdd_info_t;

FunctionOverride::FunctionOverride() :
  DynamicFixup(KIND::FUNCTION_OVERRIDE)
{}

FunctionOverride::FunctionOverride(const FunctionOverride& other) :
  DynamicFixup(other),
  bdd_info_(other.bdd_info_)
{
  if (!other.overriding_info_.empty()) {
    overriding_info_.reserve(other.overriding_info_.size());
    overriding_info_.reserve(other.overriding_info_.size());
    std::transform(other.overriding_info_.begin(), other.overriding_info_.end(),
                   std::back_inserter(overriding_info_),
      [] (const std::unique_ptr<FunctionOverrideInfo>& func) {
        return std::make_unique<FunctionOverrideInfo>(*func);
      }
    );
  }
}

FunctionOverride& FunctionOverride::operator=(const FunctionOverride& other) {
  if (this == &other) {
    return *this;
  }
  DynamicFixup::operator=(other);
  bdd_info_ = other.bdd_info_;

  if (!other.overriding_info_.empty()) {
    overriding_info_.reserve(other.overriding_info_.size());
    overriding_info_.reserve(other.overriding_info_.size());
    std::transform(other.overriding_info_.begin(), other.overriding_info_.end(),
                   std::back_inserter(overriding_info_),
      [] (const std::unique_ptr<FunctionOverrideInfo>& func) {
        return std::make_unique<FunctionOverrideInfo>(*func);
      }
    );
  }
  return *this;
}

FunctionOverride::FunctionOverride(FunctionOverride&&) = default;
FunctionOverride& FunctionOverride::operator=(FunctionOverride&&) = default;

FunctionOverride::~FunctionOverride() = default;

std::string FunctionOverride::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << format("Function Override ({}) {{\n", overriding_info_.size());
  for (const FunctionOverrideInfo& info : func_overriding_info()) {
    oss << indent(info.to_string(), 2);
    if (const image_bdd_info_t* bdd_info = find_bdd_info(info)) {
      oss << format("  BDD Version: {} ({} bytes, offset=0x{:08x})\n",
                    bdd_info->version, bdd_info->original_size,
                    bdd_info->original_offset);
      for (size_t i = 0; i < bdd_info->relocations.size(); ++i) {
        const image_bdd_dynamic_relocation_t& R = bdd_info->relocations[i];
        oss << format("    [{:04d}] L={:04d}, R={:04d}, V=0x{:08x}\n", i,
                      R.left, R.right, R.value);
      }
    } else {
      oss << "  <Missing IMAGE_BDD_INFO>\n";
    }
  }
  oss << "}\n";
  return oss.str();
}

std::unique_ptr<FunctionOverride>
  FunctionOverride::parse(Parser& ctx, SpanStream& strm)
{
  auto FuncOverrideSize = strm.read<uint32_t>();
  if (!FuncOverrideSize) {
    LIEF_DEBUG("Error: {}: {}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto FuncOverrideInfo = strm.slice(strm.pos(), *FuncOverrideSize);
  if (FuncOverrideInfo) {
    strm.increment_pos(*FuncOverrideSize);
  } else {
    LIEF_WARN("Can't slice FuncOverrideInfo");
  }

  auto BDDInfo = strm.slice(strm.pos());
  if (BDDInfo) {
    strm.increment_pos(BDDInfo->size());
  } else {
    LIEF_WARN("Can't slice BDDInfo");
  }

  auto func = std::make_unique<FunctionOverride>();
  while (FuncOverrideInfo && *FuncOverrideInfo) {
    auto override_info = FunctionOverrideInfo::parse(ctx, *FuncOverrideInfo);
    if (override_info == nullptr) {
      LIEF_INFO("Failed to parse all overriding info");
      break;
    }
    func->overriding_info_.push_back(std::move(override_info));
  }

  while (BDDInfo && *BDDInfo) {
    if (!parse_bdd_info(ctx, *BDDInfo, *func)) {
      break;
    }
  }

  return func;
}

ok_error_t FunctionOverride::parse_bdd_info(
  Parser&/*ctx*/, SpanStream& strm, FunctionOverride& func)
{
  image_bdd_info_t bdd_info;
  bdd_info.original_offset = strm.pos();

  auto Version = strm.read<uint32_t>();
  if (!Version) {
    LIEF_WARN("Can't read IMAGE_BDD_INFO.Version");
    return make_error_code(Version.error());
  }
  bdd_info.version = *Version;

  auto BDDSize = strm.read<uint32_t>();
  if (!BDDSize) {
    LIEF_WARN("Can't read IMAGE_BDD_INFO.BDDSize");
    return make_error_code(BDDSize.error());
  }

  if (*Version != 1) {
    LIEF_DEBUG("IMAGE_BDD_INFO unsupported version: {}", *Version);

    if (!strm.read_data(bdd_info.payload, *BDDSize)) {
      LIEF_WARN("Can't read IMAGE_BDD_INFO payload");
    }
    return ok();
  }

  LIEF_DEBUG("IMAGE_BDD_INFO version={}, size={}", *Version, *BDDSize);

  bdd_info.original_size = *BDDSize;

  auto payload = strm.slice(strm.pos(), *BDDSize);
  if (!payload) {
    LIEF_WARN("Can't slice IMAGE_BDD_INFO payload");
    return make_error_code(payload.error());
  }

  while (payload) {
    auto Left = payload->read<uint16_t>();
    if (!Left) {
      break;
    }

    auto Right = payload->read<uint16_t>();
    if (!Right) {
      break;
    }

    auto Value = payload->read<uint32_t>();
    if (!Value) {
      break;
    }

    bdd_info.relocations.push_back({*Left, *Right, *Value});
  }
  func.bdd_info_.push_back(std::move(bdd_info));

  strm.increment_pos(*BDDSize);

  return ok();
}

image_bdd_info_t* FunctionOverride::find_bdd_info(uint32_t offset) {
  // The current `parse_bdd_info` function parses the bdd_info in sequence
  // with increasing offset on the stream. This means that the func.bdd_info_
  // is already sorted by image_bdd_info_t::original_offset. We can leverage
  // this property to perform a binary search in O(log2(N)) instead of a O(N)
  auto it = std::lower_bound(bdd_info_.begin(), bdd_info_.end(),
    image_bdd_info_t{0, 0, offset, {}, {}},
    [] (const image_bdd_info_t& LHS, const image_bdd_info_t& RHS) {
      return LHS.original_offset < RHS.original_offset;
    });

  if (it == bdd_info_.end()) {
    return nullptr;
  }
  return &*it;
}

image_bdd_info_t* FunctionOverride::find_bdd_info(const FunctionOverrideInfo& info) {
  return find_bdd_info(info.bdd_offset());
}
}
