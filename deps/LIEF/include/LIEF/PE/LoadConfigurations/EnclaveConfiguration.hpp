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
#ifndef LIEF_PE_LOAD_CONFIGURATION_ENCLAVE_CONFIG_H
#define LIEF_PE_LOAD_CONFIGURATION_ENCLAVE_CONFIG_H
#include <memory>
#include <string>
#include <array>

#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PE/LoadConfigurations/EnclaveImport.hpp"

namespace LIEF {
class BinaryStream;
namespace PE {
class Parser;

/// This class represents the enclave configuration
class LIEF_API EnclaveConfiguration {
  public:
  static constexpr auto MIN_SIZE = 0x4C; // sizeof(IMAGE_ENCLAVE_CONFIG32)

  static constexpr auto POLICY_DEBUGGABLE = 0x00000001;
  static constexpr auto POLICY_STRICT_MEMORY = 0x00000002;

  using id_array_t = std::array<uint8_t, 16>;

  using imports_t = std::vector<EnclaveImport>;
  using it_imports = ref_iterator<imports_t&>;
  using it_const_imports = const_ref_iterator<const imports_t&>;

  EnclaveConfiguration() = default;
  EnclaveConfiguration(const EnclaveConfiguration&) = default;
  EnclaveConfiguration& operator=(const EnclaveConfiguration&) = default;

  EnclaveConfiguration(EnclaveConfiguration&&) = default;
  EnclaveConfiguration& operator=(EnclaveConfiguration&&) = default;

  std::unique_ptr<EnclaveConfiguration> clone() const {
    return std::unique_ptr<EnclaveConfiguration>(new EnclaveConfiguration(*this));
  }

  /// The size of the `IMAGE_ENCLAVE_CONFIG64/IMAGE_ENCLAVE_CONFIG32` structure,
  /// in bytes.
  uint32_t size() const {
    return size_;
  }

  /// The minimum size of the `IMAGE_ENCLAVE_CONFIG(32,64)` structure that the
  /// image loader must be able to process in order for the enclave to be usable.
  ///
  /// This member allows an enclave to inform an earlier version of the image
  /// loader that the image loader can safely load the enclave and ignore
  /// optional members added to `IMAGE_ENCLAVE_CONFIG(32,64)` for later versions
  /// of the enclave. If the size of `IMAGE_ENCLAVE_CONFIG(32,64)` that the image
  /// loader can process is less than `MinimumRequiredConfigSize`, the enclave
  /// cannot be run securely.
  ///
  /// If `MinimumRequiredConfigSize` is zero, the minimum size of the
  /// `IMAGE_ENCLAVE_CONFIG(32,64)` structure that the image loader must be able
  /// to process in order for the enclave to be usable is assumed to be the size
  /// of the structure through and including the `MinimumRequiredConfigSize` member.
  uint32_t min_required_config_size() const {
    return min_req_size_;
  }

  /// A flag that indicates whether the enclave permits debugging.
  uint32_t policy_flags() const {
    return policy_flags_;
  }

  /// Whether this enclave can be debugged
  bool is_debuggable() const {
    return (policy_flags_ & POLICY_DEBUGGABLE) != 0;
  }

  /// The RVA of the array of images that the enclave image may import, with
  /// identity information for each image.
  uint32_t import_list_rva() const {
    return imports_list_rva_;
  }

  /// The size of each image in the array of images that the import_list_rva()
  /// member points to.
  uint32_t import_entry_size() const {
    return import_entry_size_;
  }

  /// The number of images in the array of images that the import_list_rva()
  /// member points to.
  size_t nb_imports() const {
    return imports_.size();
  }

  /// Return an iterator over the enclave's imports
  it_imports imports() {
    return imports_;
  }

  it_const_imports imports() const {
    return imports_;
  }

  /// The family identifier that the author of the enclave assigned to the
  /// enclave.
  const id_array_t& family_id() const {
    return family_id_;
  }

  /// The image identifier that the author of the enclave assigned to the enclave.
  const id_array_t& image_id() const {
    return image_id_;
  }

  /// The version number that the author of the enclave assigned to the enclave.
  uint32_t image_version() const {
    return image_version_;
  }

  /// The security version number that the author of the enclave assigned to the
  /// enclave.
  uint32_t security_version() const {
    return security_version_;
  }

  /// The expected virtual size of the private address range for the enclave,
  /// in bytes.
  uint64_t enclave_size() const {
    return enclave_size_;
  }

  /// The maximum number of threads that can be created within the enclave.
  uint32_t nb_threads() const {
    return nb_threads_;
  }

  /// A flag that indicates whether the image is suitable for use as the primary
  /// image in the enclave.
  uint32_t enclave_flags() const {
    return enclave_flags_;
  }

  EnclaveConfiguration& size(uint32_t value) {
    size_ = value;
    return *this;
  }

  EnclaveConfiguration& min_required_config_size(uint32_t value) {
    min_req_size_ = value;
    return *this;
  }

  EnclaveConfiguration& policy_flags(uint32_t value) {
    policy_flags_ = value;
    return *this;
  }

  EnclaveConfiguration& import_list_rva(uint32_t value) {
    imports_list_rva_ = value;
    return *this;
  }

  EnclaveConfiguration& import_entry_size(uint32_t value) {
    import_entry_size_ = value;
    return *this;
  }

  EnclaveConfiguration& family_id(const id_array_t& value) {
    family_id_ = value;
    return *this;
  }

  EnclaveConfiguration& image_id(const id_array_t& value) {
    image_id_ = value;
    return *this;
  }

  EnclaveConfiguration& image_version(uint32_t value) {
    image_version_ = value;
    return *this;
  }

  EnclaveConfiguration& security_version(uint32_t value) {
    security_version_ = value;
    return *this;
  }

  EnclaveConfiguration& enclave_size(uint64_t value) {
    enclave_size_ = value;
    return *this;
  }

  EnclaveConfiguration& nb_threads(uint32_t value) {
    nb_threads_ = value;
    return *this;
  }

  EnclaveConfiguration& enclave_flags(uint32_t value) {
    enclave_flags_ = value;
    return *this;
  }

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const EnclaveConfiguration& meta)
  {
    os << meta.to_string();
    return os;
  }

  /// \private
  template<class PE_T>
  LIEF_LOCAL static std::unique_ptr<EnclaveConfiguration>
    parse(Parser& ctx, BinaryStream& stream);

  private:
  uint32_t size_ = 0;
  uint32_t min_req_size_ = 0;
  uint32_t policy_flags_ = 0;
  uint32_t imports_list_rva_ = 0;
  uint32_t import_entry_size_ = 0;
  id_array_t family_id_ = {0};
  id_array_t image_id_ = {0};
  uint32_t image_version_ = 0;
  uint32_t security_version_ = 0;
  uint64_t enclave_size_ = 0;
  uint32_t nb_threads_ = 0;
  uint32_t enclave_flags_ = 0;

  imports_t imports_;
};
}
}

#endif
