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

#include <type_traits>

#include "logging.hpp"

#include "LIEF/utils.hpp"

#include "LIEF/DEX.hpp"
#include "LIEF/ELF/Symbol.hpp"

#include "LIEF/OAT/Class.hpp"
#include "LIEF/OAT/Binary.hpp"
#include "LIEF/OAT/Method.hpp"
#include "OAT/Structures.hpp"

#include "Header.tcc"
#include "Object.tcc"

#include "oat_64.tcc"
#include "oat_79.tcc"
#include "oat_124.tcc"
#include "oat_131.tcc"
#include "LIEF/OAT/Parser.hpp"

namespace LIEF {
namespace OAT {


template<>
void Parser::parse_dex_files<details::OAT88_t>() {
  return parse_dex_files<details::OAT79_t>();
}

template<>
void Parser::parse_oat_classes<details::OAT88_t>() {
  return parse_oat_classes<details::OAT79_t>();
}


// Parse Binary
// ============
template<>
void Parser::parse_binary<details::OAT64_t>() {

  std::vector<uint8_t> raw_oat;
  auto& oat = oat_binary();
  const auto* oat_data = oat.get_symbol("oatdata")->as<ELF::Symbol>();
  if (oat_data != nullptr) {
    raw_oat.reserve(oat_data->size());

    span<const uint8_t> raw_data =
      oat.get_content_from_virtual_address(oat_data->value(), oat_data->size());

    std::copy(std::begin(raw_data), std::end(raw_data),
              std::back_inserter(raw_oat));

    data_address_ = oat_data->value();
    data_size_    = oat_data->size();
  }

  const auto* oat_exec = oat.get_symbol("oatexec")->as<ELF::Symbol>();
  if (oat_exec != nullptr) {

    exec_start_ = oat_exec->value();
    exec_size_  = oat_exec->size();

    span<const uint8_t> raw_oatexec =
      oat.get_content_from_virtual_address(oat_exec->value(), oat_exec->size());

    uint32_t padding = exec_start_ - (data_address_ + data_size_);

    raw_oat.reserve(raw_oat.size() + oat_exec->size() + padding);
    raw_oat.insert(std::end(raw_oat), padding, 0);

    std::copy(std::begin(raw_oatexec), std::end(raw_oatexec),
              std::back_inserter(raw_oat));
  }

  uint32_t padding = align(raw_oat.size(), sizeof(uint32_t) * 8) - raw_oat.size();
  raw_oat.insert(std::end(raw_oat), padding, 0);

  stream_ = std::make_unique<VectorStream>(std::move(raw_oat));

  parse_header<details::OAT64_t>();
  parse_dex_files<details::OAT64_t>();
  parse_oat_classes<details::OAT64_t>();
}

template<>
void Parser::parse_binary<details::OAT79_t>() {

  std::vector<uint8_t> raw_oat;
  auto& oat = oat_binary();
  const auto* oat_data = oat.get_symbol("oatdata")->as<ELF::Symbol>();
  if (oat_data != nullptr) {
    raw_oat.reserve(oat_data->size());

    span<const uint8_t> raw_data =
      oat.get_content_from_virtual_address(oat_data->value(), oat_data->size());
    std::move(std::begin(raw_data), std::end(raw_data),
              std::back_inserter(raw_oat));

    data_address_ = oat_data->value();
    data_size_    = oat_data->size();
  }

  const auto* oat_exec = oat.get_symbol("oatexec")->as<ELF::Symbol>();
  if (oat_exec != nullptr) {

    exec_start_ = oat_exec->value();
    exec_size_  = oat_exec->size();

    span<const uint8_t> raw_oatexec =
      oat.get_content_from_virtual_address(oat_exec->value(), oat_exec->size());

    uint32_t padding = exec_start_ - (data_address_ + data_size_);

    raw_oat.reserve(raw_oat.size() + oat_exec->size() + padding);
    raw_oat.insert(std::end(raw_oat), padding, 0);

    std::copy(std::begin(raw_oatexec), std::end(raw_oatexec),
              std::back_inserter(raw_oat));
  }

  uint32_t padding = align(raw_oat.size(), sizeof(uint32_t) * 8) - raw_oat.size();
  raw_oat.insert(std::end(raw_oat), padding, 0);

  stream_ = std::make_unique<VectorStream>(std::move(raw_oat));


  parse_header<details::OAT79_t>();
  parse_dex_files<details::OAT79_t>();

  parse_type_lookup_table<details::OAT79_t>();
  parse_oat_classes<details::OAT79_t>();
}

template<>
void Parser::parse_binary<details::OAT88_t>() {
  std::vector<uint8_t> raw_oat;
  auto& oat = oat_binary();
  const auto* oat_data = oat.get_symbol("oatdata")->as<ELF::Symbol>();
  if (oat_data != nullptr) {
    raw_oat.reserve(oat_data->size());

    span<const uint8_t> raw_data =
      oat.get_content_from_virtual_address(oat_data->value(), oat_data->size());
    std::copy(std::begin(raw_data), std::end(raw_data),
              std::back_inserter(raw_oat));

    data_address_ = oat_data->value();
    data_size_    = oat_data->size();
  }

  const auto* oat_exec = oat.get_symbol("oatexec")->as<ELF::Symbol>();
  if (oat_exec != nullptr) {
    exec_start_ = oat_exec->value();
    exec_size_  = oat_exec->size();

    span<const uint8_t> raw_oatexec =
      oat.get_content_from_virtual_address(oat_exec->value(), oat_exec->size());

    uint32_t padding = exec_start_ - (data_address_ + data_size_);

    raw_oat.reserve(raw_oat.size() + oat_exec->size() + padding);
    raw_oat.insert(std::end(raw_oat), padding, 0);

    std::copy(std::begin(raw_oatexec), std::end(raw_oatexec),
              std::back_inserter(raw_oat));
  }

  uint32_t padding = align(raw_oat.size(), sizeof(uint32_t) * 8) - raw_oat.size();
  raw_oat.insert(std::end(raw_oat), padding, 0);

  stream_ = std::make_unique<VectorStream>(std::move(raw_oat));


  parse_header<details::OAT88_t>();
  parse_dex_files<details::OAT88_t>();

  parse_type_lookup_table<details::OAT88_t>();
  parse_oat_classes<details::OAT88_t>();
}

template<>
void Parser::parse_binary<details::OAT124_t>() {
  std::vector<uint8_t> raw_oat;
  auto& oat = oat_binary();
  const auto* oat_data = oat.get_symbol("oatdata")->as<ELF::Symbol>();
  if (oat_data != nullptr) {
    raw_oat.reserve(oat_data->size());

    span<const uint8_t> raw_data =
      oat.get_content_from_virtual_address(oat_data->value(), oat_data->size());
    std::copy(std::begin(raw_data), std::end(raw_data),
              std::back_inserter(raw_oat));

    data_address_ = oat_data->value();
    data_size_    = oat_data->size();
  }

  const auto* oat_exec = oat.get_symbol("oatexec")->as<ELF::Symbol>();
  if (oat_exec != nullptr) {
    exec_start_ = oat_exec->value();
    exec_size_  = oat_exec->size();

    span<const uint8_t> raw_oatexec =
      oat.get_content_from_virtual_address(oat_exec->value(), oat_exec->size());

    uint32_t padding = exec_start_ - (data_address_ + data_size_);

    raw_oat.reserve(raw_oat.size() + oat_exec->size() + padding);
    raw_oat.insert(std::end(raw_oat), padding, 0);

    std::copy(std::begin(raw_oatexec), std::end(raw_oatexec),
              std::back_inserter(raw_oat));
  }

  uint32_t padding = align(raw_oat.size(), sizeof(uint32_t) * 8) - raw_oat.size();
  raw_oat.insert(std::end(raw_oat), padding, 0);

  stream_ = std::make_unique<VectorStream>(std::move(raw_oat));

  parse_header<details::OAT124_t>();
  parse_dex_files<details::OAT124_t>();
  if (oat_binary().has_vdex()) {
    parse_type_lookup_table<details::OAT124_t>();
    parse_oat_classes<details::OAT124_t>();
  }
}

template<>
void Parser::parse_binary<details::OAT131_t>() {
  std::vector<uint8_t> raw_oat;
  auto& oat = oat_binary();
  const auto* oat_data = oat.get_symbol("oatdata")->as<ELF::Symbol>();
  if (oat_data != nullptr) {
    raw_oat.reserve(oat_data->size());

    span<const uint8_t> raw_data =
      oat.get_content_from_virtual_address(oat_data->value(), oat_data->size());
    std::copy(std::begin(raw_data), std::end(raw_data),
              std::back_inserter(raw_oat));

    data_address_ = oat_data->value();
    data_size_    = oat_data->size();
  }

  const auto* oat_exec = oat.get_symbol("oatexec")->as<ELF::Symbol>();
  if (oat_exec != nullptr) {
    exec_start_ = oat_exec->value();
    exec_size_  = oat_exec->size();

    span<const uint8_t> raw_oatexec =
      oat.get_content_from_virtual_address(oat_exec->value(), oat_exec->size());

    uint32_t padding = exec_start_ - (data_address_ + data_size_);

    raw_oat.reserve(raw_oat.size() + oat_exec->size() + padding);
    raw_oat.insert(std::end(raw_oat), padding, 0);

    std::copy(std::begin(raw_oatexec), std::end(raw_oatexec),
              std::back_inserter(raw_oat));
  }

  uint32_t padding = align(raw_oat.size(), sizeof(uint32_t) * 8) - raw_oat.size();
  raw_oat.insert(std::end(raw_oat), padding, 0);

  stream_ = std::make_unique<VectorStream>(std::move(raw_oat));

  parse_header<details::OAT131_t>();
  parse_dex_files<details::OAT131_t>();

  if (oat_binary().has_vdex()) {
    parse_type_lookup_table<details::OAT131_t>();
    parse_oat_classes<details::OAT131_t>();
  } else {
    LIEF_WARN("No VDEX found. Can't parse the OAT Classes and the Lookup Table");
  }
}


template<>
void Parser::parse_binary<details::OAT138_t>() {
  return parse_binary<details::OAT131_t>();
}



template<typename OAT_T>
void Parser::parse_header() {
  using oat_header = typename OAT_T::oat_header;
  auto& oat = oat_binary();

  LIEF_DEBUG("Parsing OAT header");
  const auto res_oat_hdr = stream_->peek<oat_header>(0);
  if (!res_oat_hdr) {
    return;
  }
  const auto oat_hdr = std::move(*res_oat_hdr);
  oat.header_ = &oat_hdr;
  LIEF_DEBUG("Nb dex files: #{:d}", oat.header_.nb_dex_files());
  //LIEF_DEBUG("OAT version: {}", oat_hdr.oat_version);

  parse_header_keys<OAT_T>();
}


template<typename OAT_T>
void Parser::parse_header_keys() {
  using oat_header = typename OAT_T::oat_header;
  auto& oat = oat_binary();
  const uint64_t keys_offset = sizeof(oat_header);
  const size_t keys_size = oat.header_.key_value_size();

  std::string key_values;

  const char* keys_start = stream_->peek_array<char>(keys_offset, keys_size);
  if (keys_start != nullptr) {
    key_values = {keys_start, keys_size};
  }

  for (HEADER_KEYS key : header_keys_list) {
    std::string key_str = std::string{'\0'} + Header::key_to_string(key);

    size_t pos = key_values.find(key_str);

    if (pos != std::string::npos) {
      std::string value = std::string{key_values.data() + pos + key_str.size() + 1};
      oat.header_.dex2oat_context_.emplace(key, value);
    }
  }
}



template<typename OAT_T>
void Parser::parse_type_lookup_table() {
  //using oat_header           = typename OAT_T::oat_header;
  //using dex_file             = typename OAT_T::dex_file;
  //using lookup_table_entry_t = typename OAT_T::lookup_table_entry_t;


  //VLOG(VDEBUG) << "Parsing TypeLookupTable";
  //for (size_t i = 0; i < oat.dex_files_.size(); ++i) {

  //  const DexFile* oat_dex_file = oat.oat_dex_files_[i];
  //  uint64_t tlt_offset = oat_dex_file->lookup_table_offset();

  //  VLOG(VDEBUG) << "Getting TypeLookupTable for DexFile "
  //                << oat_dex_file->location()
  //                << " (#" << std::dec << oat_dex_file->dex_file().header().nb_classes() << ")";
  //  for (size_t j = 0; j < oat_dex_file->dex_file().header().nb_classes();) {
  //    const lookup_table_entry_t* entry = reinterpret_cast<const lookup_table_entry_t*>(stream_->read(tlt_offset, sizeof(lookup_table_entry_t)));

  //    if (entry->str_offset) {
  //      uint64_t string_offset = oat_dex_file->dex_offset() + entry->str_offset;
  //      std::pair<uint64_t, uint64_t> len_size = stream_->read_uleb128(string_offset);
  //      string_offset += len_size.second;
  //      std::string class_name = stream_->get_string(string_offset);
  //      //VLOG(VDEBUG) << "    " << "#" << std::dec << j << " " << class_name;
  //      ++j;
  //    }
  //    tlt_offset += sizeof(lookup_table_entry_t);
  //  }
  //}
}


template<typename OAT_T>
void Parser::parse_oat_classes() {
  LIEF_DEBUG("Parsing OAT Classes");
  auto& oat = oat_binary();
  for (size_t dex_idx = 0; dex_idx < oat.oat_dex_files_.size(); ++dex_idx) {
    std::unique_ptr<DexFile>& oat_dex_file = oat.oat_dex_files_[dex_idx];
    const DEX::File* dex_file_ptr = oat_dex_file->dex_file();
    if (dex_file_ptr == nullptr) {
      LIEF_ERR("Can't find the original DEX File associated with the OAT DEX File #{}", dex_idx);
      continue;
    }

    const DEX::File& dex_file = *dex_file_ptr;

    const std::vector<uint32_t>& classes_offsets = oat_dex_file->classes_offsets();
    uint32_t nb_classes = dex_file.header().nb_classes();
    LIEF_DEBUG("Dealing with DexFile #{:d} (#classes: {:d})", dex_idx, nb_classes);

    for (size_t class_idx = 0; class_idx < nb_classes; ++class_idx) {
      const DEX::Class* cls = dex_file.get_class(class_idx);
      if (cls == nullptr) {
        LIEF_ERR("Can't find the class at index #{}", class_idx);
        continue;
      }
      if (cls->index() >= classes_offsets.size()) {
        LIEF_WARN("cls.index() is not valid");
        continue;
      }
      uint32_t oat_class_offset = classes_offsets[cls->index()];
      stream_->setpos(oat_class_offset);

      // OAT Status
      auto res_status  = stream_->read<int16_t>();
      if (!res_status) {
        break;
      }
      auto status = static_cast<OAT_CLASS_STATUS>(*res_status);

      // OAT Type
      auto res_type = stream_->read<int16_t>();
      if (!res_type) {
        break;
      }

      auto type = static_cast<OAT_CLASS_TYPES>(*res_type);

      // Bitmap (if type is "some compiled")
      uint32_t method_bitmap_size = 0;
      std::vector<uint32_t> bitmap;

      if (type == OAT_CLASS_TYPES::OAT_CLASS_SOME_COMPILED) {
        if (auto res = stream_->read<uint32_t>()) {
          method_bitmap_size = *res;
        } else {
          break;
        }
        const uint32_t nb_entries = method_bitmap_size / sizeof(uint32_t);

        const auto* raw = stream_->read_array<uint32_t>(nb_entries);
        if (raw != nullptr) {
          bitmap = {raw, raw + nb_entries};
        }
      }

      auto oat_class = std::make_unique<Class>(status, type, const_cast<DEX::Class*>(cls), bitmap);

      Class& oat_cls_ref = *oat_class;
      oat.add_class(std::move(oat_class));

      // Methods Offsets
      const uint64_t method_offsets = stream_->pos();
      parse_oat_methods<OAT_T>(method_offsets, oat_cls_ref, *cls);
    }
  }
}

template<typename OAT_T>
void Parser::parse_oat_methods(uint64_t methods_offsets, Class& clazz, const DEX::Class& dex_class) {
  using oat_quick_method_header = typename OAT_T::oat_quick_method_header;
  DEX::Class::it_const_methods methods = dex_class.methods();

  auto& oat = oat_binary();
  for (const DEX::Method& method : methods) {
    if (!clazz.is_quickened(method)) {
      continue;
    }

    uint32_t computed_index = clazz.method_offsets_index(method);
    auto code_off = stream_->peek<uint32_t>(methods_offsets + computed_index * sizeof(uint32_t));
    if (!code_off) {
      break;
    }

    // Offset of the Quick method header relative to the beginning of oatexec
    uint32_t quick_method_header_off = *code_off - sizeof(oat_quick_method_header);
    quick_method_header_off &= ~1u;

    const auto res_quick_header = stream_->peek<oat_quick_method_header>(quick_method_header_off);
    if (!res_quick_header) {
      break;
    }

    const auto quick_header = std::move(*res_quick_header);

    uint32_t vmap_table_offset = *code_off - quick_header.vmap_table_offset;

    auto oat_method = std::make_unique<Method>(const_cast<DEX::Method*>(&method), &clazz);

    if (quick_header.code_size > 0) {

      const auto* code = stream_->peek_array<uint8_t>(*code_off, quick_header.code_size);
      if (code != nullptr) {
        oat_method->quick_code_ = {code, code + quick_header.code_size};
      }
    }

    // Quickened with "Optimizing compiler"
    if (quick_header.code_size > 0 && vmap_table_offset > 0) {
    }

    // Quickened with "dex2dex"
    if (quick_header.code_size == 0 && vmap_table_offset > 0) {
      stream_->setpos(vmap_table_offset);

      for (size_t pc = 0, round = 0; pc < method.bytecode().size(); ++round) {
        if (stream_->pos() >= stream_->size()) {
          break;
        }
        auto res_new_pc = stream_->read_uleb128();
        if (!res_new_pc) {
          break;
        }
        auto new_pc = static_cast<uint32_t>(*res_new_pc);

        if (new_pc <= pc && round > 0) {
          break;
        }

        pc = new_pc;


        if (stream_->pos() >= stream_->size()) {
          break;
        }
        auto res_index = stream_->read_uleb128();
        if (!res_index) {
          break;
        }

        auto index = static_cast<uint32_t>(*res_index);
        oat_method->dex_method()->insert_dex2dex_info(pc, index);
      }

    }
    clazz.methods_.push_back(oat_method.get());
    oat.methods_.push_back(std::move(oat_method));
  }

}

}
}
