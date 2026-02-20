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
#include <fstream>
#include <climits>
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "logging.hpp"
#include "LIEF/DEX/File.hpp"
#include "LIEF/DEX/utils.hpp"
#include "LIEF/DEX/instructions.hpp"
#include "LIEF/DEX/Class.hpp"
#include "LIEF/DEX/Method.hpp"
#include "LIEF/DEX/Prototype.hpp"
#include "LIEF/DEX/hash.hpp"
#include "LIEF/DEX/Type.hpp"
#include "LIEF/DEX/Field.hpp"
#include "DEX/Structures.hpp"

#if defined(LIEF_JSON_SUPPORT)
#include "visitors/json.hpp"
#endif

namespace LIEF {
namespace DEX {

File::File() :
  name_{"classes.dex"}
{}
File::~File() = default;

dex_version_t File::version() const {
  Header::magic_t m = header().magic();
  span<const uint8_t> data(m.data(), m.size());
  SpanStream stream(data);
  return DEX::version(stream);
}

std::string File::save(const std::string& path, bool deoptimize) const {
  if (path.empty()) {
    if (!name().empty()) {
      return save(name());
    }
    return save("classes.dex");
  }

  if (std::ofstream ifs{path, std::ios::binary | std::ios::trunc}) {
    if (deoptimize) {
      const std::vector<uint8_t> raw = this->raw(deoptimize);
      ifs.write(reinterpret_cast<const char*>(raw.data()), raw.size());
    } else {
      ifs.write(reinterpret_cast<const char*>(original_data_.data()), original_data_.size());
    }
    return path;
  }

  return "";
}


std::vector<uint8_t> File::raw(bool deoptimize) const {
  if (!deoptimize) {
    return original_data_;
  }
  dex2dex_info_t dex2dex_info = this->dex2dex_info();

  if (dex2dex_info.empty()) {
    return original_data_;
  }

  std::vector<uint8_t> raw = original_data_;

  for (const std::unique_ptr<Method>& method : methods_) {
    if (method->bytecode().empty()) {
      continue;
    }
    const uint32_t code_item_offset = method->code_offset();
    const uint8_t* inst_start = raw.data() + code_item_offset;
    uint8_t* inst_ptr = raw.data() + code_item_offset;
    uint8_t* inst_end = inst_ptr + method->bytecode().size();
    dex2dex_method_info_t meth_info = method->dex2dex_info();

    while (inst_ptr < inst_end) {
      uint16_t dex_pc = (inst_ptr - inst_start) / sizeof(uint16_t);
      auto opcode = static_cast<OPCODES>(*inst_ptr);
      uint32_t value = UINT_MAX;

      if (meth_info.find(dex_pc) != std::end(meth_info)) {
        value = meth_info[dex_pc];
      }

      // Skip packed-switch, sparse-switch, fill-array instructions
      if (is_switch_array(inst_ptr, inst_end)) {
        inst_ptr += switch_array_size(inst_ptr, inst_end);
        continue;
      }

      switch(opcode) {
        case OPCODES::OP_NOP:
          {
            //deoptimize_nop(inst_ptr, 0);
            break;
          }

        case OPCODES::OP_RETURN_VOID_NO_BARRIER:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] return-void-no-barrier -> return-void", dex_pc);
            deoptimize_return(inst_ptr, 0);
            break;
          }

        case OPCODES::OP_IGET_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iget-quick -> iget@0x{:x}", dex_pc, value);
            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iget-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IGET);
            break;
          }

        case OPCODES::OP_IGET_WIDE_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iget-wide-quick -> iget-wide@{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iget-wide-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IGET_WIDE);
            break;
          }

        case OPCODES::OP_IGET_OBJECT_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iget-object-quick -> iget-object@{:d}", dex_pc, value);
            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iget-object-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IGET_OBJECT);
            break;
          }

        case OPCODES::OP_IPUT_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iput-quick -> iput@{:d}", dex_pc, value);
            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iput-quick)",
                  method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IPUT);
            break;
          }

        case OPCODES::OP_IPUT_WIDE_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iput-wide-quick -> iput-wide@{:d}", dex_pc, value);
            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iput-wide-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IPUT_WIDE);
            break;
          }

        case OPCODES::OP_IPUT_OBJECT_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iput-object-quick -> iput-objecte@{:d}", dex_pc, value);
            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iput-object-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IPUT_OBJECT);
            break;
          }

        case OPCODES::OP_INVOKE_VIRTUAL_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] invoke-virtual-quick -> invoke-virtual@{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (invoke-virtual-quick)",
                  method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_invoke_virtual(inst_ptr, value, OPCODES::OP_INVOKE_VIRTUAL);
            break;
          }

        case OPCODES::OP_INVOKE_VIRTUAL_RANGE_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] invoke-virtual-quick/range -> invoke-virtual/range @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (invoke-virtual-quick/range)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_invoke_virtual(inst_ptr, value, OPCODES::OP_INVOKE_VIRTUAL_RANGE);
            break;
          }

        case OPCODES::OP_IPUT_BOOLEAN_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iput-boolean-quick -> iput-boolean@{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iput-boolean-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IPUT_BOOLEAN);
            break;
          }

        case OPCODES::OP_IPUT_BYTE_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iput-byte-quick -> iput-byte @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iput-byte-quick)",
                  method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IPUT_BYTE);
            break;
          }

        case OPCODES::OP_IPUT_CHAR_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iput-char-quick -> iput-char @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iput-char-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IPUT_CHAR);
            break;
          }

        case OPCODES::OP_IPUT_SHORT_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iput-short-quick -> iput-short @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iput-short)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IPUT_SHORT);
            break;
          }

        case OPCODES::OP_IGET_BOOLEAN_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iget-boolean-quick -> iget-boolean @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iget-boolean-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IGET_BOOLEAN);
            break;
          }

        case OPCODES::OP_IGET_BYTE_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iget-byte-quick -> iget-byte @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iget-byte-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IGET_BYTE);
            break;
          }

        case OPCODES::OP_IGET_CHAR_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iget-char-quick -> iget-char @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iget-char-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IGET_CHAR);
            break;
          }

        case OPCODES::OP_IGET_SHORT_QUICK:
          {
            LIEF_TRACE("{}.{}", method->cls()->fullname(), method->name());
            LIEF_TRACE("[{:06x}] iget-short-quick -> iget-short @{:d}", dex_pc, value);

            if (static_cast<int32_t>(value) == -1) {
              LIEF_WARN("Unable to resolve instruction: {}.{} at 0x{:04x} (iget-short-quick)",
                        method->cls()->fullname(), method->name(), dex_pc);
              break;
            }
            deoptimize_instance_field_access(inst_ptr, value, OPCODES::OP_IGET_SHORT);
            break;
          }
        default:
          {
          }
      }
      inst_ptr += inst_size_from_opcode(opcode);
    }
  }

  return raw;
}

void File::deoptimize_nop(uint8_t* inst_ptr, uint32_t /*value*/) {
  *inst_ptr = OPCODES::OP_CHECK_CAST;
}

void File::deoptimize_return(uint8_t* inst_ptr, uint32_t /*value*/) {
  *inst_ptr = OPCODES::OP_RETURN_VOID;
}

void File::deoptimize_invoke_virtual(uint8_t* inst_ptr, uint32_t value, OPCODES new_inst) {
  *inst_ptr = new_inst;
  reinterpret_cast<uint16_t*>(inst_ptr)[1] = value;
}

void File::deoptimize_instance_field_access(uint8_t* inst_ptr, uint32_t value, OPCODES new_inst) {
  *inst_ptr = new_inst;
  reinterpret_cast<uint16_t*>(inst_ptr)[1] = value;
}

const std::string& File::name() const {
  return name_;
}


const std::string& File::location() const {
  return location_;
}

const Header& File::header() const {
  return header_;
}

Header& File::header() {
  return const_cast<Header&>(static_cast<const File*>(this)->header());
}

File::it_const_classes File::classes() const {
  return class_list_;
}

File::it_classes File::classes() {
  return class_list_;
}

bool File::has_class(const std::string& class_name) const {
  return classes_.find(Class::fullname_normalized(class_name)) != std::end(classes_);
}

const Class* File::get_class(const std::string& class_name) const {
  auto it_cls = classes_.find(Class::fullname_normalized(class_name));
  if (it_cls == std::end(classes_)) {
    return nullptr;
  }
  return it_cls->second;
}

Class* File::get_class(const std::string& class_name) {
  return const_cast<Class*>(static_cast<const File*>(this)->get_class(class_name));
}


const Class* File::get_class(size_t index) const {
  if (index >= classes_.size()) {
    return nullptr;
  }
  return class_list_[index].get();
}

Class* File::get_class(size_t index) {
  return const_cast<Class*>(static_cast<const File*>(this)->get_class(index));
}


dex2dex_info_t File::dex2dex_info() const {
  dex2dex_info_t info;
  for (const auto& p : classes_) {
    dex2dex_class_info_t class_info = p.second->dex2dex_info();
    if (!class_info.empty()) {
      info.emplace(p.second, std::move(class_info));
    }
  }
  return info;
}

std::string File::dex2dex_json_info() const {

#if defined(LIEF_JSON_SUPPORT)
  json mapping = json::object();

  // Iter over the class quickened
  for (const auto& class_map : dex2dex_info()) {
    const Class* clazz = class_map.first;
    const std::string& class_name = clazz->fullname();
    mapping[class_name] = json::object();

    const dex2dex_class_info_t& class_info = class_map.second;
    // Iter over the method within the class
    for (const auto& method_map : class_info) {

      // Index of the method within the Dex File
      uint32_t index = method_map.first->index();

      mapping[class_name][std::to_string(index)] = json::object();

      for (const auto& pc_index : method_map.second) {
        mapping[class_name][std::to_string(index)][std::to_string(pc_index.first)] = pc_index.second;
      }
    }
  }
  return mapping.dump();
#else
  return "";
#endif
}

File::it_const_methods File::methods() const {
  return methods_;
}

File::it_methods File::methods() {
  return methods_;
}

File::it_const_fields File::fields() const {
  return fields_;
}

File::it_fields File::fields() {
  return fields_;
}

File::it_const_strings File::strings() const {
  return strings_;
}

File::it_strings File::strings() {
  return strings_;
}

File::it_const_types File::types() const {
  return types_;
}

File::it_types File::types() {
  return types_;
}

File::it_const_prototypes File::prototypes() const {
  return prototypes_;
}

File::it_prototypes File::prototypes() {
  return prototypes_;
}

const MapList& File::map() const {
  return map_;
}

MapList& File::map() {
  return map_;
}

void File::name(const std::string& name) {
  name_ = name;
}

void File::location(const std::string& location) {
  location_ = location;
}

void File::add_class(std::unique_ptr<Class> cls) {
  classes_.emplace(cls->fullname(), cls.get());
  class_list_.push_back(std::move(cls));
}

void File::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const File& file) {
  os << "DEX File " << file.name() << " Version: " << std::dec << file.version();
  if (!file.location().empty()) {
    os << " - " << file.location();
  }
  os << '\n';

  os << "Header" << '\n';
  os << "======" << '\n';

  os << file.header();

  os << '\n';

  os << "Map" << '\n';
  os << "===" << '\n';

  os << file.map();

  os << '\n';
  return os;
}





}
}
