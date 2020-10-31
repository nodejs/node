/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_UTIL_DESCRIPTORS_H_
#define SRC_TRACE_PROCESSOR_UTIL_DESCRIPTORS_H_

#include <algorithm>
#include <string>
#include <vector>

#include "perfetto/ext/base/optional.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"

namespace protozero {
struct ConstBytes;
}

namespace perfetto {
namespace trace_processor {

class FieldDescriptor {
 public:
  FieldDescriptor(std::string name,
                  uint32_t number,
                  uint32_t type,
                  std::string raw_type_name,
                  bool is_repeated);

  const std::string& name() const { return name_; }
  uint32_t number() const { return number_; }
  uint32_t type() const { return type_; }
  const std::string& raw_type_name() const { return raw_type_name_; }
  const std::string& resolved_type_name() const { return resolved_type_name_; }
  bool is_repeated() const { return is_repeated_; }

  void set_resolved_type_name(const std::string& resolved_type_name) {
    resolved_type_name_ = resolved_type_name;
  }

 private:
  std::string name_;
  uint32_t number_;
  uint32_t type_;
  std::string raw_type_name_;
  std::string resolved_type_name_;
  bool is_repeated_;
};

class ProtoDescriptor {
 public:
  enum class Type { kEnum = 0, kMessage = 1 };

  ProtoDescriptor(std::string package_name,
                  std::string full_name,
                  Type type,
                  base::Optional<uint32_t> parent_id);

  void AddField(FieldDescriptor descriptor) {
    PERFETTO_DCHECK(type_ == Type::kMessage);
    fields_.emplace_back(std::move(descriptor));
  }

  void AddEnumValue(int32_t integer_representation,
                    std::string string_representation) {
    PERFETTO_DCHECK(type_ == Type::kEnum);
    enum_values_.emplace_back(integer_representation,
                              std::move(string_representation));
  }

  base::Optional<uint32_t> FindFieldIdxByName(const std::string& name) const {
    PERFETTO_DCHECK(type_ == Type::kMessage);
    auto it = std::find_if(
        fields_.begin(), fields_.end(),
        [name](const FieldDescriptor& desc) { return desc.name() == name; });
    auto idx = static_cast<uint32_t>(std::distance(fields_.begin(), it));
    return idx < fields_.size() ? base::Optional<uint32_t>(idx) : base::nullopt;
  }

  base::Optional<uint32_t> FindFieldIdxByTag(const uint16_t tag_number) const {
    PERFETTO_DCHECK(type_ == Type::kMessage);
    auto it = std::find_if(fields_.begin(), fields_.end(),
                           [tag_number](const FieldDescriptor& desc) {
                             return desc.number() == tag_number;
                           });
    auto idx = static_cast<uint32_t>(std::distance(fields_.begin(), it));
    return idx < fields_.size() ? base::Optional<uint32_t>(idx) : base::nullopt;
  }

  base::Optional<std::string> FindEnumString(const int32_t value) const {
    PERFETTO_DCHECK(type_ == Type::kEnum);
    auto it =
        std::find_if(enum_values_.begin(), enum_values_.end(),
                     [value](const std::pair<int32_t, std::string>& enum_val) {
                       return enum_val.first == value;
                     });
    return it == enum_values_.end() ? base::nullopt
                                    : base::Optional<std::string>(it->second);
  }

  const std::string& package_name() const { return package_name_; }

  const std::string& full_name() const { return full_name_; }

  const std::vector<FieldDescriptor>& fields() const { return fields_; }
  std::vector<FieldDescriptor>* mutable_fields() { return &fields_; }

 private:
  std::string package_name_;
  std::string full_name_;
  const Type type_;
  base::Optional<uint32_t> parent_id_;
  std::vector<FieldDescriptor> fields_;
  std::vector<std::pair<int32_t, std::string>> enum_values_;
};

class DescriptorPool {
 public:
  util::Status AddFromFileDescriptorSet(
      const uint8_t* file_descriptor_set_proto,
      size_t size);

  base::Optional<uint32_t> FindDescriptorIdx(
      const std::string& full_name) const;

  const std::vector<ProtoDescriptor>& descriptors() const {
    return descriptors_;
  }

 private:
  void AddNestedProtoDescriptors(const std::string& package_name,
                                 base::Optional<uint32_t> parent_idx,
                                 protozero::ConstBytes descriptor_proto);
  void AddEnumProtoDescriptors(const std::string& package_name,
                               base::Optional<uint32_t> parent_idx,
                               protozero::ConstBytes descriptor_proto);

  util::Status AddExtensionField(const std::string& package_name,
                                 protozero::ConstBytes field_desc_proto);

  // Recursively searches for the given short type in all parent messages
  // and packages.
  base::Optional<uint32_t> ResolveShortType(const std::string& parent_path,
                                            const std::string& short_type);

  std::vector<ProtoDescriptor> descriptors_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_UTIL_DESCRIPTORS_H_
