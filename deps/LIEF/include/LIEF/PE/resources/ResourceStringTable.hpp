/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab

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
#ifndef LIEF_PE_RESOURCE_STRING_TABLE_H
#define LIEF_PE_RESOURCE_STRING_TABLE_H
#include <string>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/optional.hpp"

namespace LIEF {
class BinaryStream;
namespace PE {

/// This class represents the `StringTable` structure. This structure
/// can be seen as a dictionary of key, values with key and values defined a
/// utf-16 string.
class LIEF_API ResourceStringTable : public Object {
  public:
  /// An entry in this table which is composed of an UTF-16 key and an UTF-16
  /// value.
  struct entry_t {
    std::u16string key;
    std::u16string value;

    /// Key in utf8 representation
    std::string key_u8() const;

    /// Value in utf8 representation
    std::string value_u8() const;

    bool is_defined() const {
      return !key.empty() || !value.empty();
    }

    operator bool() const {
      return is_defined();
    }

    std::string to_string() const {
      return key_u8() + ": " + value_u8();
    }

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const entry_t& entry)
    {
      os << entry.to_string();
      return os;
    }
  };

  using entries_t = std::vector<entry_t>;
  using it_entries = ref_iterator<entries_t&>;
  using it_const_entries = const_ref_iterator<const entries_t&>;

  ResourceStringTable() = default;

  static result<ResourceStringTable> parse(BinaryStream& stream);

  ResourceStringTable(const ResourceStringTable&) = default;
  ResourceStringTable& operator=(const ResourceStringTable&) = default;

  ResourceStringTable(ResourceStringTable&&) = default;
  ResourceStringTable& operator=(ResourceStringTable&&) = default;

  ~ResourceStringTable() override = default;

  /// An 8-digit hexadecimal number stored as a Unicode string. The four most
  /// significant digits represent the language identifier. The four least
  /// significant digits represent the code page for which the data is formatted.
  /// Each Microsoft Standard Language identifier contains two parts:
  /// the low-order 10 bits specify the major language, and the high-order 6
  /// bits specify the sublanguage.
  const std::u16string& key() const {
    return key_;
  }

  /// The type of data in the version resource:
  /// * `1` if it contains text data
  /// * `0` if it contains binary
  uint16_t type() const {
    return type_;
  }

  /// The key as an utf8 string
  std::string key_u8() const;

  /// Iterator over the different entry_t element of this table
  it_entries entries() {
    return entries_;
  }

  it_const_entries entries() const {
    return entries_;
  }

  optional<std::u16string> get(const std::u16string& key) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
      [&key] (const entry_t& entry) {
        return entry.key == key;
      }
    );
    if (it == entries_.end()) {
      return nullopt();
    }

    return it->value;
  }

  optional<std::string> get(const std::string& key) const;

  ResourceStringTable& key(std::u16string value) {
    key_ = std::move(value);
    return *this;
  }

  ResourceStringTable& type(uint16_t value) {
    type_ = value;
    return *this;
  }

  void add_entry(entry_t entry) {
    entries_.push_back(std::move(entry));
  }

  void add_entry(std::u16string key, std::u16string value) {
    entries_.push_back(entry_t{std::move(key), std::move(value)});
  }

  void accept(Visitor& visitor) const override;

  optional<std::string> operator[](const std::string& str) const {
    return get(str);
  }

  optional<std::u16string> operator[](const std::u16string& str) const {
    return get(str);
  }

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const ResourceStringTable& table);

  private:
  uint16_t type_ = 0;
  std::u16string key_;
  std::vector<entry_t> entries_;
};

}
}

#endif
