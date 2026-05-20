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
#ifndef LIEF_PE_SIGNATURE_PARSER_H
#define LIEF_PE_SIGNATURE_PARSER_H
#include <memory>
#include <string>
#include <array>

#include "LIEF/errors.hpp"

#include "LIEF/PE/signature/Signature.hpp"
#include "LIEF/PE/signature/OIDToString.hpp"

namespace LIEF {
class BinaryStream;
class VectorStream;

namespace PE {
class Parser;
class Attribute;
class SpcIndirectData;
class PKCS9TSTInfo;

class LIEF_API SignatureParser {
  friend class Parser;
  struct SpcPeImageData {
    uint32_t flags;
    std::string file;
  };

  struct SpcSpOpusInfo {
    std::string program_name;
    std::string more_info;
  };
  struct range_t {
    uint64_t start = 0;
    uint64_t end = 0;
  };

  public:
  using attributes_t = std::vector<std::unique_ptr<Attribute>>;
  using signer_infos_t = std::vector<SignerInfo>;
  using x509_certificates_t = std::vector<x509>;
  using time_t = std::array<int32_t, 6>;

  /// Parse a PKCS #7 signature given a raw blob
  static result<Signature> parse(std::vector<uint8_t> data, bool skip_header = false);

  /// Parse a PKCS #7 signature given a BinaryStream
  static result<Signature> parse(BinaryStream& stream, bool skip_header = false);

  /// Parse a PKCS #7 signature from a file path
  static result<Signature> parse(const std::string& path);
  SignatureParser(const SignatureParser&) = delete;
  SignatureParser& operator=(const SignatureParser&) = delete;
  private:

  ~SignatureParser() = default;
  SignatureParser() = default;

  static result<Signature> parse_signature(BinaryStream& stream);

  static result<ContentInfo> parse_content_info(BinaryStream& stream, range_t& range);
  static result<x509_certificates_t> parse_certificates(BinaryStream& stream);
  static result<signer_infos_t> parse_signer_infos(BinaryStream& stream);
  static result<attributes_t> parse_attributes(BinaryStream& stream);
  static result<std::unique_ptr<Attribute>> parse_content_type(BinaryStream& stream);

  static result<signer_infos_t> parse_pkcs9_counter_sign(BinaryStream& stream);
  static result<std::vector<uint8_t>> parse_pkcs9_message_digest(BinaryStream& stream);
  static result<int32_t> parse_pkcs9_at_sequence_number(BinaryStream& stream);
  static result<time_t> parse_pkcs9_signing_time(BinaryStream& stream);
  static result<std::unique_ptr<PKCS9TSTInfo>> parse_pkcs9_tstinfo(BinaryStream& stream);

  static result<std::unique_ptr<Attribute>> parse_ms_counter_sign(BinaryStream& stream);
  static result<Signature> parse_ms_spc_nested_signature(BinaryStream& stream);
  static result<oid_t> parse_ms_spc_statement_type(BinaryStream& stream);
  static result<SpcSpOpusInfo> parse_spc_sp_opus_info(BinaryStream& stream);
  static result<std::string> parse_spc_string(BinaryStream& stream);
  static result<std::string> parse_spc_link(BinaryStream& stream);
  static result<std::unique_ptr<Attribute>> parse_spc_relaxed_pe_marker_check(BinaryStream& stream);
  static result<SpcPeImageData> parse_spc_pe_image_data(BinaryStream& stream);
  static result<std::unique_ptr<SpcIndirectData>> parse_spc_indirect_data(BinaryStream& stream, range_t& range);
  static result<std::unique_ptr<Attribute>> parse_ms_platform_manifest_binary_id(BinaryStream& stream);

  static result<std::unique_ptr<Attribute>> parse_signing_certificate_v2(BinaryStream& stream);
};

}
}

#endif
