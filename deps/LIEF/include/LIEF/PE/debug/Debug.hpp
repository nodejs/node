
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
#ifndef LIEF_PE_DEBUG_H
#define LIEF_PE_DEBUG_H
#include <cstdint>
#include <ostream>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

namespace LIEF {
namespace PE {
class Parser;
class Builder;
class Section;

namespace details {
struct pe_debug;
}

/// This class represents a generic entry in the debug data directory.
/// For known types, this class is extended to provide a dedicated API
/// (see: CodeCodeView)
class LIEF_API Debug : public Object {
  friend class Parser;
  friend class Builder;

  public:
  /// The entry types
  enum class TYPES {
    UNKNOWN = 0,

    /// COFF Debug information
    COFF = 1,

    /// CodeView debug information that is used to store PDB info
    CODEVIEW = 2,

    /// Frame pointer omission information
    FPO = 3,

    /// Miscellaneous debug information
    MISC = 4,

    /// Debug information that is a *copy* of the `.pdata` section
    EXCEPTION = 5,

    /// (Reserved) Debug information used for fixup relocations
    FIXUP = 6,

    /// The mapping from an RVA in image to an RVA in source image.
    OMAP_TO_SRC = 7,

    /// The mapping from an RVA in source image to an RVA in image.
    OMAP_FROM_SRC = 8,

    /// Reserved for Borland.
    BORLAND = 9,

    /// Reserved
    RESERVED10 = 10,

    /// Reserved
    CLSID = 11,

    /// Visual C++ feature information.
    VC_FEATURE = 12,

    /// Profile Guided Optimization metadata
    POGO = 13,

    /// Incremental Link Time Code Generation metadata
    ILTCG = 14,

    MPX = 15,

    /// PE determinism or reproducibility information
    REPRO = 16,

    /// Checksum of the PDB file
    PDBCHECKSUM = 19,

    /// Extended DLL characteristics
    EX_DLLCHARACTERISTICS = 20,
  };
  Debug() = default;
  Debug(TYPES type) {
    type_ = type;
  }

  static span<uint8_t> get_payload(Section& section, uint32_t rva,
                                   uint32_t offset, uint32_t size);
  static span<uint8_t> get_payload(Section& section, const details::pe_debug& hdr);
  static span<uint8_t> get_payload(Section& section, const Debug& dbg) {
    return get_payload(section, dbg.addressof_rawdata(), dbg.pointerto_rawdata(),
                       dbg.sizeof_data());
  }

  Debug(const details::pe_debug& debug_s, Section* section);

  Debug(const Debug& other) = default;
  Debug& operator=(const Debug& other) = default;

  Debug(Debug&&) = default;
  Debug& operator=(Debug&&) = default;

  ~Debug() override = default;

  virtual std::unique_ptr<Debug> clone() const {
    return std::unique_ptr<Debug>(new Debug(*this));
  }

  /// Reserved should be 0
  uint32_t characteristics() const {
    return characteristics_;
  }

  /// The time and date when the debug data was created.
  uint32_t timestamp() const {
    return timestamp_;
  }

  /// The major version number of the debug data format.
  uint16_t major_version() const {
    return major_version_;
  }

  /// The minor version number of the debug data format.
  uint16_t minor_version() const {
    return minor_version_;
  }

  /// The format DEBUG_TYPES of the debugging information
  TYPES type() const {
    return type_;
  }

  /// Size of the debug data
  uint32_t sizeof_data() const {
    return sizeof_data_;
  }

  /// Address of the debug data relative to the image base
  uint32_t addressof_rawdata() const {
    return addressof_rawdata_;
  }

  /// File offset of the debug data
  uint32_t pointerto_rawdata() const {
    return pointerto_rawdata_;
  }

  /// The section where debug data is located
  const Section* section() const {
    return section_;
  }

  Section* section() {
    return section_;
  }

  /// Debug data associated with this entry
  span<uint8_t> payload();

  span<const uint8_t> payload() const {
    return const_cast<Debug*>(this)->payload();
  }

  void characteristics(uint32_t characteristics) {
    characteristics_ = characteristics;
  }

  void timestamp(uint32_t timestamp) {
    timestamp_ = timestamp;
  }

  void major_version(uint16_t major_version) {
    major_version_ = major_version;
  }

  void minor_version(uint16_t minor_version) {
    minor_version_ = minor_version;
  }

  void sizeof_data(uint32_t sizeof_data) {
    sizeof_data_ = sizeof_data;
  }

  void addressof_rawdata(uint32_t addressof_rawdata) {
    addressof_rawdata_ = addressof_rawdata;
  }

  void pointerto_rawdata(uint32_t pointerto_rawdata) {
    pointerto_rawdata_ = pointerto_rawdata;
  }

  template<class T>
  const T* as() const {
    static_assert(std::is_base_of<Debug, T>::value, "Require Debug inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* as() {
    return const_cast<T*>(static_cast<const Debug*>(this)->as<T>());
  }

  void accept(Visitor& visitor) const override;

  virtual std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const Debug& entry)
  {
    os << entry.to_string();
    return os;
  }

  protected:
  TYPES type_ = TYPES::UNKNOWN;
  uint32_t characteristics_ = 0;
  uint32_t timestamp_ = 0;
  uint16_t major_version_ = 0;
  uint16_t minor_version_ = 0;
  uint32_t sizeof_data_ = 0;
  uint32_t addressof_rawdata_ = 0;
  uint32_t pointerto_rawdata_ = 0;

  Section* section_ = nullptr;
};

LIEF_API const char* to_string(Debug::TYPES e);

}
}
#endif
