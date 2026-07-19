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
#ifndef LIEF_PE_LOAD_CONFIGURATION_ENCLAVE_IMPORT_H
#define LIEF_PE_LOAD_CONFIGURATION_ENCLAVE_IMPORT_H
#include <string>
#include <array>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

namespace LIEF {
class BinaryStream;
namespace PE {
class Parser;
class EnclaveConfiguration;

/// Defines an entry in the array of images that an enclave can import.
class LIEF_API EnclaveImport {
  public:
  using short_id_t = std::array<uint8_t, 16>;
  using long_id_t = std::array<uint8_t, 32>;

  enum class TYPE : uint32_t {
    /// None of the identifiers of the image need to match the value in the
    /// import record.
    NONE = 0x00000000,

    /// The value of the enclave unique identifier of the image must match the
    /// value in the import record. Otherwise, loading of the image fails.
    UNIQUE_ID = 0x00000001,

    /// The value of the enclave author identifier of the image must match the
    /// value in the import record. Otherwise, loading of the image fails. If
    /// this flag is set and the import record indicates an author identifier
    /// of all zeros, the imported image must be part of the Windows installation.
    AUTHOR_ID = 0x00000002,

 	  /// The value of the enclave family identifier of the image must match the
    /// value in the import record. Otherwise, loading of the image fails.
    FAMILY_ID = 0x00000003,

    /// The value of the enclave image identifier of the image must match the
    /// value in the import record. Otherwise, loading of the image fails.
    IMAGE_ID = 0x00000004,
  };

  EnclaveImport() = default;
  EnclaveImport(const EnclaveImport&) = default;
  EnclaveImport& operator=(const EnclaveImport&) = default;

  EnclaveImport(EnclaveImport&&) = default;
  EnclaveImport& operator=(EnclaveImport&&) = default;

  /// The type of identifier of the image that must match the value in the import
  /// record.
  TYPE type() const {
    return type_;
  }

  /// The minimum enclave security version that each image must have for the
  /// image to be imported successfully. The image is rejected unless its enclave
  /// security version is equal to or greater than the minimum value in the
  /// import record. Set the value in the import record to zero to turn off the
  /// security version check.
  uint32_t min_security_version() const {
    return min_security_version_;
  }

  /// The unique identifier of the primary module for the enclave, if the type()
  /// is TYPE::UNIQUE_ID. Otherwise, the author identifier of the primary module
  /// for the enclave.
  const long_id_t& id() const {
    return id_;
  }

  /// The family identifier of the primary module for the enclave.
  const short_id_t& family_id() const {
    return family_id_;
  }

  /// The image identifier of the primary module for the enclave.
  const short_id_t& image_id() const {
    return image_id_;
  }

  /// The relative virtual address of a NULL-terminated string that contains the
  /// same value found in the import directory for the image.
  uint32_t import_name_rva() const {
    return import_name_rva_;
  }

  /// Resolved import name
  const std::string& import_name() const {
    return import_name_;
  }

  /// Reserved. Should be 0
  uint32_t reserved() const {
    return reserved_;
  }

  EnclaveImport& type(TYPE ty) {
    type_ = ty;
    return *this;
  }

  EnclaveImport& min_security_version(uint32_t value) {
    min_security_version_ = value;
    return *this;
  }

  EnclaveImport& id(const long_id_t& value) {
    id_ = value;
    return *this;
  }

  EnclaveImport& family_id(const short_id_t& value) {
    family_id_ = value;
    return *this;
  }

  EnclaveImport& image_id(const short_id_t& value) {
    image_id_ = value;
    return *this;
  }

  EnclaveImport& import_name_rva(uint32_t value) {
    import_name_rva_ = value;
    return *this;
  }

  EnclaveImport& reserved(uint32_t value) {
    reserved_ = value;
    return *this;
  }

  EnclaveImport& import_name(std::string name) {
    import_name_ = std::move(name);
    return *this;
  }

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const EnclaveImport& meta)
  {
    os << meta.to_string();
    return os;
  }

  /// \private
  LIEF_LOCAL static result<EnclaveImport>
    parse(Parser& ctx, BinaryStream& stream);

  private:
  TYPE type_ = TYPE::NONE;
  uint32_t min_security_version_ = 0;
  long_id_t id_;
  short_id_t family_id_ = {0};
  short_id_t image_id_ = {0};
  uint32_t import_name_rva_ = 0;
  uint32_t reserved_ = 0;

  std::string import_name_;
};

LIEF_API const char* to_string(EnclaveImport::TYPE e);

}
}

#endif
