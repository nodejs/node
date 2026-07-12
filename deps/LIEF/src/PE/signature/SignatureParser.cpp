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

#include <cstring>
#include <fstream>
#include <memory>

#include <mbedtls/x509_crt.h>

#include "LIEF/utils.hpp"


#include "LIEF/BinaryStream/VectorStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/BinaryStream/ASN1Reader.hpp"

#include "LIEF/PE/utils.hpp"

#include "LIEF/PE/signature/SignatureParser.hpp"
#include "LIEF/PE/signature/Signature.hpp"
#include "LIEF/PE/signature/SpcIndirectData.hpp"
#include "LIEF/PE/signature/GenericContent.hpp"
#include "LIEF/PE/signature/PKCS9TSTInfo.hpp"

#include "LIEF/PE/signature/Attribute.hpp"
#include "LIEF/PE/signature/attributes/ContentType.hpp"
#include "LIEF/PE/signature/attributes/GenericType.hpp"
#include "LIEF/PE/signature/attributes/SpcSpOpusInfo.hpp"
#include "LIEF/PE/signature/attributes/PKCS9CounterSignature.hpp"
#include "LIEF/PE/signature/attributes/PKCS9MessageDigest.hpp"
#include "LIEF/PE/signature/attributes/PKCS9AtSequenceNumber.hpp"
#include "LIEF/PE/signature/attributes/PKCS9SigningTime.hpp"
#include "LIEF/PE/signature/attributes/MsSpcNestedSignature.hpp"
#include "LIEF/PE/signature/attributes/MsSpcStatementType.hpp"
#include "LIEF/PE/signature/attributes/MsManifestBinaryID.hpp"
#include "LIEF/PE/signature/attributes/SigningCertificateV2.hpp"
#include "LIEF/PE/signature/attributes/SpcRelaxedPeMarkerCheck.hpp"
#include "LIEF/PE/signature/attributes/MsCounterSign.hpp"

#include "LIEF/PE/signature/OIDToString.hpp"

#include "logging.hpp"
#include "messages.hpp"
#include "internal_utils.hpp"

namespace LIEF {
namespace PE {

inline uint8_t stream_get_tag(BinaryStream& stream) {
  if (auto tag = stream.peek<uint8_t>()) {
    return *tag;
  }
  return 0;
}

result<Signature> SignatureParser::parse(const std::string& path) {
  std::ifstream binary(path, std::ios::in | std::ios::binary);
  if (!binary) {
    LIEF_ERR("Can't open {}", path);
    return make_error_code(lief_errors::file_error);
  }
  binary.unsetf(std::ios::skipws);
  binary.seekg(0, std::ios::end);
  const auto size = static_cast<uint64_t>(binary.tellg());
  binary.seekg(0, std::ios::beg);
  std::vector<uint8_t> raw_blob(size, 0);
  binary.read(reinterpret_cast<char*>(raw_blob.data()), size);
  return SignatureParser::parse(std::move(raw_blob));
}

result<Signature> SignatureParser::parse(std::vector<uint8_t> data, bool skip_header) {
  if (data.size() < 10) {
    return make_error_code(lief_errors::read_error);
  }
  auto stream = std::make_unique<VectorStream>(std::move(data));
  if (skip_header) {
    stream->increment_pos(8);
  }

  auto sig = SignatureParser::parse_signature(*stream);
  if (!sig) {
    LIEF_ERR("Error while parsing the signature");
    return make_error_code(sig.error());
  }
  return sig.value();
}


result<Signature> SignatureParser::parse(BinaryStream& stream, bool skip_header) {
  if (skip_header) {
    stream.increment_pos(8);
  }
  return parse_signature(stream);
}

result<Signature> SignatureParser::parse_signature(BinaryStream& stream) {
  Signature signature;
  if (VectorStream::classof(stream)) {
    signature.original_raw_signature_ = static_cast<VectorStream&>(stream).content();
  } else if (SpanStream::classof(stream)) {
    signature.original_raw_signature_ = static_cast<SpanStream&>(stream).content();
  }
  ASN1Reader asn1r(stream);
  auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  auto oid = asn1r.read_oid();
  if (!oid) {
    LIEF_INFO("Can't read OID value (pos: {})", stream.pos());
    return make_error_code(oid.error());
  }
  std::string& oid_str = oid.value();

  if (oid_str != /* pkcs7-signedData */ "1.2.840.113549.1.7.2") {
    LIEF_INFO("Expecting OID pkcs7-signed-data at {:d} but got {}",
              stream.pos(), oid_to_string(oid_str));
    return make_error_code(lief_errors::read_error);
  }

  tag = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 0);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }
  tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  /*
   * Defined in https://tools.ietf.org/html/rfc2315
   * SignedData ::= SEQUENCE {
   *   version          Version,
   *   digestAlgorithms DigestAlgorithmIdentifiers,
   *   contentInfo      ContentInfo,
   *   certificates
   *     [0] IMPLICIT ExtendedCertificatesAndCertificates OPTIONAL,
   *   crls
   *     [1] IMPLICIT CertificateRevocationLists OPTIONAL,
   *   signerInfos SignerInfos
   * }
   *
   * Version ::= INTEGER
   * DigestAlgorithmIdentifiers ::= SET OF DigestAlgorithmIdentifier
   * SignerInfos ::= SET OF SignerInfo
   *
   *
   * SignerInfo ::= SEQUENCE {
   *      version Version,
   *      issuerAndSerialNumber IssuerAndSerialNumber,
   *      digestAlgorithm DigestAlgorithmIdentifier,
   *      authenticatedAttributes
   *        [0] IMPLICIT Attributes OPTIONAL,
   *      digestEncryptionAlgorithm
   *        DigestEncryptionAlgorithmIdentifier,
   *      encryptedDigest EncryptedDigest,
   *      unauthenticatedAttributes
   *        [1] IMPLICIT Attributes OPTIONAL
   * }
   *
   */

  // ==============================================================================
  // Version
  //
  // Version ::= INTEGER
  // ==============================================================================
  auto version = asn1r.read_int();
  if (!version) {
    LIEF_INFO("Can't parse version (pos: {:d})", stream.pos());
    return make_error_code(version.error());
  }
  const int32_t version_val = version.value();
  LIEF_DEBUG("pkcs7-signed-data.version: {:d}", version_val);
  if (version_val != 1) {
    LIEF_INFO("pkcs7-signed-data.version is not 1 ({:d})", version_val);
    return make_error_code(lief_errors::not_supported);
  }
  signature.version_ = version_val;

  // ==============================================================================
  // Digest Algorithms
  //
  // DigestAlgorithmIdentifiers ::= SET OF DigestAlgorithmIdentifier
  // ==============================================================================
  tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }
  const uintptr_t end_set = stream.pos() + tag.value();
  std::vector<oid_t> algorithms;
  while (stream.pos() < end_set) {
    const size_t current_p = stream.pos();
    auto alg = asn1r.read_alg();
    if (!alg) {
      LIEF_INFO("Can't parse signed data digest algorithm (pos: {:d})", stream.pos());
      break;
    }
    if (stream.pos() == current_p) {
      break;
    }
    LIEF_DEBUG("pkcs7-signed-data.digest-algorithms: {}", oid_to_string(alg.value()));
    algorithms.push_back(std::move(alg.value()));
  }

  if (algorithms.empty()) {
    LIEF_INFO("pkcs7-signed-data.digest-algorithms no algorithms found");
    return make_error_code(lief_errors::read_error);
  }

  if (algorithms.size() > 1) {
    LIEF_INFO("pkcs7-signed-data.digest-algorithms {:d} algorithms found. Expecting only 1", algorithms.size());
    return make_error_code(lief_errors::read_error);
  }

  ALGORITHMS algo = algo_from_oid(algorithms.back());
  if (algo == ALGORITHMS::UNKNOWN) {
    LIEF_WARN("LIEF does not handle algorithm {}", algorithms.back());
  } else {
    signature.digest_algorithm_ = algo;
  }


  // Content Info
  // =========================================================
  tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} can't parse content info (pos: {:d})",
              asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  /* Content Info */ {
    const size_t raw_content_size = tag.value();
    SpanStream content_info_stream{stream.p(), tag.value()};

    range_t range = {0, 0};
    if (auto content_info = parse_content_info(content_info_stream, range)) {
      if (!SpcIndirectData::classof(&content_info->value())) {
        content_info_stream.setpos(0);
        ASN1Reader asn1r(content_info_stream);
        auto content_type = asn1r.read_oid();
        const std::string& ctype_str = content_type.value();
        LIEF_WARN("Expecting SPC_INDIRECT_DATA at {:d} but got {}",
                  stream.pos(), oid_to_string(ctype_str));
        return make_error_code(lief_errors::read_error);
      }
      signature.content_info_       = std::move(*content_info);
      signature.content_info_start_ = stream.pos() + range.start;
      signature.content_info_end_   = stream.pos() + range.end;
      LIEF_DEBUG("ContentInfo range: {:d} -> {:d}",
                 signature.content_info_start_, signature.content_info_end_);
    } else {
      LIEF_INFO("Fail to parse pkcs7-signed-data.content-info");
    }
    stream.increment_pos(raw_content_size);
  }

  // X509 Certificates (optional)
  // =========================================================

  tag = asn1r.read_tag(/* certificates */
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC);
  if (tag) {
    LIEF_DEBUG("Parse pkcs7-signed-data.certificates offset: {:d}", stream.pos());
    SpanStream certificate_stream{stream.p(), tag.value()};
    stream.increment_pos(tag.value());

    auto certificates = parse_certificates(certificate_stream);
    if (certificates) {
      // Makes chain
      signature.certificates_ = std::move(*certificates);
    } else {
      LIEF_INFO("Fail to parse pkcs7-signed-data.certificates");
    }
  }

  // CRLS (optional)
  // =========================================================
  tag = asn1r.read_tag(/* certificates */
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC | 1);
  if (tag) {
    LIEF_DEBUG("Parse pkcs7-signed-data.crls offset: {:d}", stream.pos());
    const size_t crls_size = tag.value();
    // TODO(romain): Process crls certificates
    stream.increment_pos(crls_size);
  }

  // SignerInfos
  // =========================================================
  tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET);
  if (tag) {
    LIEF_DEBUG("Parse pkcs7-signed-data.signer-infos offset: {:d}", stream.pos());
    const size_t raw_content_size = tag.value();
    SpanStream signers_stream{stream.p(), raw_content_size};
    stream.increment_pos(raw_content_size);
    auto signer_info = parse_signer_infos(signers_stream);
    if (!signer_info) {
      LIEF_INFO("Fail to parse pkcs7-signed-data.signer-infos");
    } else {
      signature.signers_ = std::move(signer_info.value());
    }
  }

  // Tied signer info with x509 certificates
  for (SignerInfo& signer : signature.signers_) {
    const x509* crt = signature.find_crt_issuer(signer.issuer(), as_vector(signer.serial_number()));
    if (crt != nullptr) {
      signer.cert_ = std::make_unique<x509>(*crt);
    } else {
      LIEF_INFO("Can't find x509 certificate associated with signer '{}'", signer.issuer());
    }
    const auto* cs = static_cast<const PKCS9CounterSignature*>(signer.get_attribute(Attribute::TYPE::PKCS9_COUNTER_SIGNATURE));
    if (cs != nullptr) {
      SignerInfo& cs_signer = const_cast<PKCS9CounterSignature*>(cs)->signer_;
      const x509* crt = signature.find_crt_issuer(cs_signer.issuer(), as_vector(cs_signer.serial_number()));
      if (crt != nullptr) {
        cs_signer.cert_ = std::make_unique<x509>(*crt);
      } else {
        LIEF_INFO("Can't find x509 certificate associated with signer '{}'", signer.issuer());
      }
    }
  }

  return signature;
}

result<ContentInfo>
SignatureParser::parse_content_info(BinaryStream& stream, range_t& range) {
  // ==============================================================================
  // ContentInfo
  // ContentInfo ::= SEQUENCE {
  //   contentType ContentType,
  //   content     [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL
  // }
  // ContentType ::= OBJECT IDENTIFIER
  // ==============================================================================
  ContentInfo content_info;
  ASN1Reader asn1r(stream);

  auto content_type = asn1r.read_oid();
  if (!content_type) {
    LIEF_INFO("Can't parse content-info.content-type (pos: {:d})", stream.pos());
    return make_error_code(content_type.error());
  }

  const std::string& ctype_str = content_type.value();
  LIEF_DEBUG("content-info.content-type: {}", oid_to_string(ctype_str));

  if (ctype_str == /* SPC_INDIRECT_DATA_CONTEXT */ "1.3.6.1.4.1.311.2.1.4") {
    auto spc_indirect_data = parse_spc_indirect_data(stream, range);
    if (spc_indirect_data) {
      content_info.value_ = std::move(*spc_indirect_data);
      return content_info;
    }

    LIEF_WARN("Can't parse SPC_INDIRECT_DATA (pos={})", stream.pos());
    auto generic = std::make_unique<GenericContent>(ctype_str);
    stream.setpos(0);
    generic->raw_ = {stream.p(), stream.end()};
    content_info.value_ = std::move(generic);
    return content_info;
  }

  if (ctype_str == /* PKCS9 TSTInfo */ "1.2.840.113549.1.9.16.1.4") {
    auto tstinfo = parse_pkcs9_tstinfo(stream);
    if (tstinfo) {
      content_info.value_ = std::move(*tstinfo);
      return content_info;
    }
    LIEF_WARN("Can't parse PKCS9 TSTInfo (pos={})", stream.pos());
    auto generic = std::make_unique<GenericContent>(ctype_str);
    stream.setpos(0);
    generic->raw_ = {stream.p(), stream.end()};
    content_info.value_ = std::move(generic);
    return content_info;
  }

  LIEF_INFO("ContentInfo: Unknown OID ({})", ctype_str);
  auto generic = std::make_unique<GenericContent>(ctype_str);
  generic->raw_ = {stream.p(), stream.end()};
  content_info.value_ = std::move(generic);
  return content_info;
}

result<std::unique_ptr<SpcIndirectData>>
SignatureParser::parse_spc_indirect_data(BinaryStream& stream, range_t& range) {
  // ==============================================================================
  // Then process SpcIndirectDataContent, which has this structure:
  //
  // SpcIndirectDataContent ::= SEQUENCE {
  //  data          SpcAttributeTypeAndOptionalValue,
  //  messageDigest DigestInfo
  // }
  //
  // SpcAttributeTypeAndOptionalValue ::= SEQUENCE {
  //  type  ObjectID, // Should be SPC_PE_IMAGE_DATA
  //  value [0] EXPLICIT ANY OPTIONAL
  // }
  //
  // DigestInfo ::= SEQUENCE {
  //  digestAlgorithm  AlgorithmIdentifier,
  //  digest           OCTETSTRING
  // }
  //
  // AlgorithmIdentifier ::= SEQUENCE {
  //  algorithm  ObjectID,
  //  parameters [0] EXPLICIT ANY OPTIONAL
  // }
  // ==============================================================================
  ASN1Reader asn1r(stream);
  auto indirect_data = std::make_unique<SpcIndirectData>();
  auto tag = asn1r.read_tag(/* [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL */
                            MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }
  range.end   = stream.size();

  tag = asn1r.read_tag(/* SpcIndirectDataContent */
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  range.start = stream.pos();
  tag = asn1r.read_tag(/* SpcAttributeTypeAndOptionalValue */
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  // SpcAttributeTypeAndOptionalValue.type
  auto spc_attr_type = asn1r.read_oid();
  if (!spc_attr_type) {
    LIEF_INFO("Can't parse spc-attribute-type-and-optional-value.type (pos: {:d})", stream.pos());
    return make_error_code(spc_attr_type.error());
  }
  const std::string& spc_attr_type_str = spc_attr_type.value();
  LIEF_DEBUG("spc-attribute-type-and-optional-value.type: {}", oid_to_string(spc_attr_type_str));
  if (spc_attr_type_str == /* SPC_PE_IMAGE_DATA */ "1.3.6.1.4.1.311.2.1.15") {
    tag = asn1r.read_tag(/* SpcPeImageData ::= SEQUENCE */
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);

    if (!tag) {
      LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
      return make_error_code(tag.error());
    }

    /* SpcPeImageData */ {
      const size_t length = tag.value();
      SpanStream spc_data_stream{ stream.p(), length };
      stream.increment_pos(spc_data_stream.size());
      if (auto spc_data = parse_spc_pe_image_data(spc_data_stream)) {
        const SpcPeImageData& spc_data_value = *spc_data;
        indirect_data->file_ = spc_data_value.file;
        indirect_data->flags_ = spc_data_value.flags;
      }
      else {
        LIEF_INFO("Can't parse SpcPeImageData");
      }
    }
  }
  else if (spc_attr_type_str == /* SPC_LINK_(TYPE_2) */ "1.3.6.1.4.1.311.2.1.25" ||
           spc_attr_type_str == /* SPC_LINK_(TYPE_3) */ "1.3.6.1.4.1.311.2.1.28")
  {

    tag = asn1r.read_tag(/* SpcLink / URL ::= STRING */
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_INTEGER);

    if (!tag) {
      LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
      return make_error_code(tag.error());
    }
    const size_t length = tag.value();
    SpanStream spc_data_stream{ stream.p(), length };
    stream.increment_pos(spc_data_stream.size());
    if (auto link = parse_spc_link(spc_data_stream)) {
      LIEF_DEBUG("SpcIndirectData.url = {}", *link);
      indirect_data->url_ = link.value();
    } else {
      LIEF_INFO("Can't parse SpcLink");
    }
  }
  else {
    LIEF_WARN("Expecting OID SPC_PE_IMAGE_DATA or SPC_LINK at {:d} but got {}",
              stream.pos(), oid_to_string(spc_attr_type_str));
    return make_error_code(lief_errors::read_error);
  }

  // ================================================
  // DigestInfo ::= SEQUENCE
  // ================================================
  tag = asn1r.read_tag(/* DigestInfo */
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);

  if (!tag) {
    LIEF_INFO("Wrong tag {} for DigestInfo ::= SEQUENCE (pos: {:d})",
              asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  auto alg_identifier = asn1r.read_alg();
  if (!alg_identifier) {
    LIEF_INFO("Can't parse SignedData.contentInfo.messageDigest.digestAlgorithm (pos: {:d})",
              stream.pos());
    return make_error_code(alg_identifier.error());
  }

  LIEF_DEBUG("spc-indirect-data-content.digest-algorithm {}", oid_to_string(alg_identifier.value()));
  ALGORITHMS algo = algo_from_oid(alg_identifier.value());
  if (algo == ALGORITHMS::UNKNOWN) {
    LIEF_WARN("LIEF does not handle {}", alg_identifier.value());
  } else {
    indirect_data->digest_algorithm_ = algo;
  }

  // From the documentation:
  //  The value must match the digestAlgorithm value specified
  //  in SignerInfo and the parent PKCS #7 digestAlgorithms fields.
  auto digest = asn1r.read_octet_string();
  if (!digest) {
    LIEF_INFO("Can't parse SignedData.contentInfo.messageDigest.digest (pos={})",
              stream.pos());
    return make_error_code(digest.error());
  }
  indirect_data->digest_ = std::move(digest.value());
  LIEF_DEBUG("spc-indirect-data-content.digest: {}",
             hex_dump(indirect_data->digest_));
  return indirect_data;
}

result<SignatureParser::x509_certificates_t> SignatureParser::parse_certificates(BinaryStream& stream) {
  ASN1Reader asn1r(stream);

  x509_certificates_t certificates;
  const uint64_t cert_end_p = stream.size();
  while (stream.pos() < cert_end_p) {
    auto cert = asn1r.read_cert();
    if (!cert) {
      LIEF_INFO("Can't parse X509 cert pkcs7-signed-data.certificates (pos: {:d})", stream.pos());
      return certificates;
    }

    std::unique_ptr<mbedtls_x509_crt> cert_p = std::move(cert.value());

    if constexpr (lief_logging_debug) {
      std::array<char, 1024> buffer = {0};
      mbedtls_x509_crt_info(buffer.data(), buffer.size(), "", cert_p.get());
      LIEF_DEBUG("\n{}\n", buffer.data());
    }
    certificates.emplace_back(cert_p.release());
  }
  return certificates;
}


result<SignatureParser::signer_infos_t> SignatureParser::parse_signer_infos(BinaryStream& stream) {
  const uintptr_t end_set = stream.size();

  signer_infos_t infos;

  ASN1Reader asn1r(stream);

  while (stream.pos() < end_set) {
    SignerInfo signer;
    const size_t current_p = stream.pos();

    auto tag = asn1r.read_tag(/* SignerInfo */
                              MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (!tag) {
      LIEF_INFO("Wrong tag: {} for pkcs7-signed-data.signer-infos SEQUENCE (pos: {:d})",
                asn1r.get_str_tag(), stream.pos());
      break;
    }

    // =======================================================
    // version Version
    // =======================================================
    auto version = asn1r.read_int();
    if (!version) {
      LIEF_INFO("Can't parse pkcs7-signed-data.signer-info.version (pos: {:d})", stream.pos());
      break;
    }

    int32_t version_val = version.value();
    LIEF_DEBUG("pkcs7-signed-data.signer-info.version: {}", version_val);
    if (version_val != 1) {
      LIEF_DEBUG("pkcs7-signed-data.signer-info.version: Bad version ({:d})", version_val);
      break;
    }
    signer.version_ = version_val;

    // =======================================================
    // IssuerAndSerialNumber ::= SEQUENCE {
    //   issuer       Name,
    //   serialNumber CertificateSerialNumber
    // }
    //
    // For Name see: https://github.com/ARMmbed/mbedtls/blob/9e4d4387f07326fff227a40f76c25e5181b1b1e2/library/x509_crt.c#L1180
    // =======================================================
    tag = asn1r.read_tag(/* Name */
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (!tag) {
      LIEF_INFO("Wrong tag: {} for "
                "pkcs7-signed-data.signer-infos.issuer-and-serial-number.issuer (pos: {:d})",
                asn1r.get_str_tag(), stream.pos());
      break;
    }

    auto issuer = asn1r.x509_read_names();
    if (!issuer) {
      LIEF_INFO("Can't parse pkcs7-signed-data.signer-infos.issuer-and-serial-number.issuer (pos: {:d})",
                stream.pos());
      break;
    }

    LIEF_DEBUG("pkcs7-signed-data.signer-infos.issuer-and-serial-number.issuer: {} (pos: {:d})",
               issuer.value(), stream.pos());
    signer.issuer_ = std::move(issuer.value());

    auto sn = asn1r.x509_read_serial();
    if (!sn) {
      LIEF_INFO("Can't parse pkcs7-signed-data.signer-infos.issuer-and-serial-number.serial-number (pos: {:d})",
                stream.pos());
      break;
    }

    LIEF_DEBUG("pkcs7-signed-data.signer-infos.issuer-and-serial-number.serial-number {}", hex_dump(sn.value()));
    signer.serialno_ = std::move(sn.value());

    // =======================================================
    // Digest Encryption Algorithm
    // =======================================================
    {
      auto digest_alg = asn1r.read_alg();

      if (!digest_alg) {
        LIEF_INFO("Can't parse pkcs7-signed-data.signer-infos.digest-algorithm (pos: {:d})", stream.pos());
        break;
      }
      LIEF_DEBUG("pkcs7-signed-data.signer-infos.digest-algorithm: {}", oid_to_string(digest_alg.value()));

      ALGORITHMS dg_algo = algo_from_oid(digest_alg.value());
      if (dg_algo == ALGORITHMS::UNKNOWN) {
        LIEF_WARN("LIEF does not handle algorithm {}", digest_alg.value());
      } else {
        signer.digest_algorithm_ = dg_algo;
      }
    }

    // =======================================================
    // Authenticated Attributes
    // =======================================================
    {
      const uint64_t auth_attr_start = stream.pos();
      tag = asn1r.read_tag(/* authenticatedAttributes [0] IMPLICIT Attributes OPTIONAL */
                           MBEDTLS_ASN1_CONSTRUCTED      |
                           MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0);
      if (tag) {
        const uint64_t auth_attr_end = stream.pos() + tag.value();
        SpanStream auth_stream(stream.p(), tag.value());
        stream.increment_pos(auth_stream.size());
        auto authenticated_attributes = parse_attributes(auth_stream);
        if (!authenticated_attributes) {
          LIEF_INFO("Fail to parse pkcs7-signed-data.signer-infos.authenticated-attributes");
        } else {
          signer.raw_auth_data_ = {stream.start() + auth_attr_start, stream.start() + auth_attr_end};
          signer.authenticated_attributes_ = std::move(authenticated_attributes.value());
        }
      }
    }

    // =======================================================
    // Digest Encryption Algorithm
    // =======================================================
    {
      auto digest_enc_alg = asn1r.read_alg();
      if (!digest_enc_alg) {
        LIEF_INFO("Can't parse pkcs7-signed-data.signer-infos.digest-encryption-algorithm (pos: {:d})",
                  stream.pos());
        return make_error_code(digest_enc_alg.error());
      }
      LIEF_DEBUG("pkcs7-signed-data.signer-infos.digest-encryption-algorithm: {}",
                 oid_to_string(digest_enc_alg.value()));

      ALGORITHMS dg_enc_algo = algo_from_oid(digest_enc_alg.value());
      if (dg_enc_algo == ALGORITHMS::UNKNOWN) {
        LIEF_WARN("LIEF does not handle algorithm {}", digest_enc_alg.value());
      } else {
        signer.digest_enc_algorithm_ = dg_enc_algo;
      }
    }

    // =======================================================
    // Encrypted Digest
    // =======================================================
    {
      auto enc_digest = asn1r.read_octet_string();
      if (!enc_digest) {
        LIEF_INFO("Can't parse pkcs7-signed-data.signer-infos.encrypted-digest (pos: {:d})", stream.pos());
        return make_error_code(enc_digest.error());
      }
      LIEF_DEBUG("pkcs7-signed-data.signer-infos.encrypted-digest: {}",
                 hex_dump(enc_digest.value()).substr(0, 10));
      signer.encrypted_digest_ = enc_digest.value();
    }

    // =======================================================
    // Unauthenticated Attributes
    // =======================================================
    {
      tag = asn1r.read_tag(/* unauthenticatedAttributes [1] IMPLICIT Attributes OPTIONAL */
                           MBEDTLS_ASN1_CONSTRUCTED      |
                           MBEDTLS_ASN1_CONTEXT_SPECIFIC | 1);
      if (tag) {
        SpanStream unauth_stream(stream.p(), tag.value());
        stream.increment_pos(unauth_stream.size());
        auto unauthenticated_attributes = parse_attributes(unauth_stream);
        if (!unauthenticated_attributes) {
          LIEF_INFO("Fail to parse pkcs7-signed-data.signer-infos.unauthenticated-attributes");
        } else {
          signer.unauthenticated_attributes_ = std::move(unauthenticated_attributes.value());
        }
      }
    }
    infos.push_back(std::move(signer));

    if (stream.pos() <= current_p) {
      break;
    }
  }
  return infos;
}


result<SignatureParser::attributes_t>
SignatureParser::parse_attributes(BinaryStream& stream) {
  // Attributes ::= SET OF Attribute
  //
  // Attribute ::= SEQUENCE
  // {
  //    type       EncodedObjectID,
  //    values     AttributeSetValue
  // }
  attributes_t attributes;
  ASN1Reader asn1r(stream);

  while (stream) {
    auto tag = asn1r.read_tag(/* Attribute ::= SEQUENCE */
                              MBEDTLS_ASN1_SEQUENCE | MBEDTLS_ASN1_CONSTRUCTED);
    if (!tag) {
      LIEF_INFO("Can't parse attribute (pos: {:d})", stream.pos());
      break;
    }

    auto oid = asn1r.read_oid();
    if (!oid) {
      LIEF_INFO("Can't parse attribute.type (pos: {:d})", stream.pos());
      break;
    }
    tag = asn1r.read_tag(/* AttributeSetValue */
                         MBEDTLS_ASN1_SET | MBEDTLS_ASN1_CONSTRUCTED);
    if (!tag) {
      LIEF_DEBUG("attribute.values: Unable to get set for {}", oid.value());
      break;
    }
    const size_t value_set_size = tag.value();
    LIEF_DEBUG("attribute.values: {} ({:d} bytes)", oid_to_string(oid.value()), value_set_size);

    SpanStream value_stream(stream.p(), value_set_size);

    while (value_stream) {
      const uint64_t current_p = value_stream.pos();
      const std::string& oid_str = oid.value();

      if (oid_str == /* contentType */ "1.2.840.113549.1.9.3") {
        auto res = parse_content_type(value_stream);
        if (!res || res.value() == nullptr) {
          LIEF_INFO("Can't parse content-type attribute");
        } else {
          attributes.push_back(std::move(res.value()));
        }
      }

      else if (oid_str == /* SpcSpOpusInfo */ "1.3.6.1.4.1.311.2.1.12") {
        auto res = parse_spc_sp_opus_info(value_stream);
        if (!res) {
          LIEF_INFO("Can't parse spc-sp-opus-info attribute");
        } else {
          SpcSpOpusInfo info = std::move(res.value());
          attributes.push_back(std::make_unique<PE::SpcSpOpusInfo>(std::move(info.program_name),
                                                                   std::move(info.more_info)));
        }
      }

      else if (oid_str == /* Ms-CounterSign */ "1.3.6.1.4.1.311.3.3.1") {
        if (auto res = parse_ms_counter_sign(value_stream)) {
          attributes.push_back(std::move(*res));
        } else {
          LIEF_INFO("Can't parse ms-counter-sign attribute");
        }
      }

      else if (oid_str == /* pkcs9-CounterSignature */ "1.2.840.113549.1.9.6") {
        auto res = parse_pkcs9_counter_sign(value_stream);
        if (!res) {
          LIEF_INFO("Can't parse pkcs9-counter-sign attribute");
        } else {
          const std::vector<SignerInfo>& signers = res.value();
          if (signers.empty()) {
            LIEF_INFO("Can't parse signer info associated with the pkcs9-counter-sign");
          } else if (signers.size() > 1) {
            LIEF_INFO("More than one signer info associated with the pkcs9-counter-sign");
          } else {
            attributes.push_back(std::make_unique<PKCS9CounterSignature>(signers.back()));
          }
        }
      }

      else if (oid_str == /* Ms-SpcNestedSignature */ "1.3.6.1.4.1.311.2.4.1") {
        auto res = parse_ms_spc_nested_signature(value_stream);
        if (!res) {
          LIEF_INFO("Can't parse ms-spc-nested-signature attribute");
        } else {
          attributes.push_back(std::make_unique<MsSpcNestedSignature>(std::move(res.value())));
        }
      }

      else if (oid_str == /* pkcs9-MessageDigest */ "1.2.840.113549.1.9.4") {
        auto res = parse_pkcs9_message_digest(value_stream);
        if (!res) {
          LIEF_INFO("Can't parse pkcs9-message-digest attribute");
        } else {
          attributes.push_back(std::make_unique<PKCS9MessageDigest>(std::move(res.value())));
        }
      }

      else if (oid_str == /* Ms-SpcStatementType */ "1.3.6.1.4.1.311.2.1.11") {
        auto res = parse_ms_spc_statement_type(value_stream);
        if (!res) {
          LIEF_INFO("Can't parse ms-spc-statement-type attribute");
        } else {
          attributes.push_back(std::make_unique<MsSpcStatementType>(std::move(res.value())));
        }
      }

      else if (oid_str == /* pkcs9-at-SequenceNumber */ "1.2.840.113549.1.9.25.4") {
        auto res = parse_pkcs9_at_sequence_number(value_stream);
        if (!res) {
          LIEF_INFO("Can't parse pkcs9-at-SequenceNumber attribute");
        } else {
          attributes.push_back(std::make_unique<PKCS9AtSequenceNumber>(*res));
        }
      }

      else if (oid_str == /* pkcs9-signing-time */ "1.2.840.113549.1.9.5") {
        auto res = parse_pkcs9_signing_time(value_stream);
        if (!res) {
          LIEF_INFO("Can't parse pkcs9-signing-time attribute");
        } else {
          attributes.push_back(std::make_unique<PKCS9SigningTime>(*res));
        }
      }

      else if (oid_str == /* szOID_PLATFORM_MANIFEST_BINARY_ID */ "1.3.6.1.4.1.311.10.3.28") {
        if (auto res = parse_ms_platform_manifest_binary_id(value_stream)) {
          attributes.push_back(std::move(*res));
        } else {
          LIEF_INFO("Can't parse ms-szoid-platform-manifest-binary-id");
        }
      }

      else if (oid_str == /* SPC_RELAXED_PE_MARKER_CHECK_OBJID */ "1.3.6.1.4.1.311.2.6.1") {
        if (auto res = parse_spc_relaxed_pe_marker_check(value_stream)) {
          attributes.push_back(std::move(*res));
        } else {
          LIEF_INFO("Can't parse spc-relaxed-pe-marker-check");
        }
      }

      else if (oid_str == /* SIGNING_CERTIFICATE_V2 */ "1.2.840.113549.1.9.16.2.47") {
        if (auto res = parse_signing_certificate_v2(value_stream)) {
          attributes.push_back(std::move(*res));
        } else {
          LIEF_INFO("Can't parse signing-certificate-v2");
        }
      }

      else {
        LIEF_INFO("Unknown OID: {}", oid_str);
        attributes.push_back(std::make_unique<GenericType>(oid_str, value_stream.content()));
        break;
      }

      if (current_p >= value_stream.pos()) {
        LIEF_INFO("Endless loop detected!");
        break;
      }

      if (value_stream.pos() < value_stream.size()) {
        const uint32_t delta = value_stream.size() - value_stream.pos();
        LIEF_INFO("{}: {} bytes left", oid_str, delta);
      }
    }
    stream.increment_pos(value_set_size);
  }
  return attributes;
}

result<std::unique_ptr<Attribute>> SignatureParser::parse_content_type(BinaryStream& stream) {
  /*
   *
   * ContentType ::= OBJECT IDENTIFIER
   * Content type as defined in https://tools.ietf.org/html/rfc2315#section-6.8
   */

  ASN1Reader asn1r(stream);
  auto oid = asn1r.read_oid();
  if (!oid) {
    LIEF_INFO("Can't parse content-type.oid (pos: {:d})", stream.pos());
    return make_error_code(oid.error());
  }
  const std::string& oid_str = oid.value();
  LIEF_DEBUG("content-type.oid: {}", oid_to_string(oid_str));
  LIEF_DEBUG("content-type remaining bytes: {}", stream.size() - stream.pos());
  return std::unique_ptr<Attribute>{new ContentType{oid_str}};
}

result<SignatureParser::SpcSpOpusInfo> SignatureParser::parse_spc_sp_opus_info(BinaryStream& stream) {
  // SpcSpOpusInfo ::= SEQUENCE {
  //   programName        [0] EXPLICIT SpcString OPTIONAL,
  //   moreInfo           [1] EXPLICIT SpcLink   OPTIONAL
  // }
  LIEF_DEBUG("Parse spc-sp-opus-info");
  ASN1Reader asn1r(stream);
  SpcSpOpusInfo info;
  auto tag = asn1r.read_tag(/* SpcSpOpusInfo ::= SEQUENCE */
                            MBEDTLS_ASN1_SEQUENCE | MBEDTLS_ASN1_CONSTRUCTED);
  if (!tag) {
    LIEF_INFO("Wrong tag for  spc-sp-opus-info SEQUENCE : {} (pos: {:d})",
              asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  tag = asn1r.read_tag(/* programName [0] EXPLICIT SpcString OPTIONAL */
                       MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED);
  if (tag) {
    SpanStream spc_string_stream(stream.p(), tag.value());
    auto program_name = parse_spc_string(spc_string_stream);
    if (!program_name) {
      LIEF_INFO("Fail to parse spc-sp-opus-info.program-name");
    } else {
      info.program_name = program_name.value();
    }
    stream.increment_pos(spc_string_stream.size());
  }
  tag = asn1r.read_tag(/* moreInfo [1] EXPLICIT SpcLink OPTIONAL */
                       MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 1);
  if (tag) {
    SpanStream spc_link_stream(stream.p(), tag.value());
    auto more_info = parse_spc_link(spc_link_stream);
    if (!more_info) {
      LIEF_INFO("Fail to parse spc-sp-opus-info.more-info");
    } else {
      info.more_info = more_info.value();
    }
    stream.increment_pos(spc_link_stream.size());
  }
  return info;
}

result<std::unique_ptr<Attribute>> SignatureParser::parse_ms_counter_sign(BinaryStream& stream) {
  /*
   *
   * SignedData ::= SEQUENCE {
   *   version          Version,
   *   digestAlgorithms DigestAlgorithmIdentifiers,
   *   contentInfo      ContentInfo,
   *   certificates     [0] IMPLICIT ExtendedCertificatesAndCertificates OPTIONAL,
   *   crls             [1] IMPLICIT CertificateRevocationLists OPTIONAL,
   *   signerInfos      SignerInfos
   * }
   *
   * Version ::= INTEGER
   * DigestAlgorithmIdentifiers ::= SET OF DigestAlgorithmIdentifier
   *
   */
  auto counter_sig = std::make_unique<MsCounterSign>();
  ASN1Reader asn1r(stream);
  auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  auto oid = asn1r.read_oid();
  if (!oid) {
    LIEF_INFO("Can't read OID value (pos: {})", stream.pos());
    return make_error_code(oid.error());
  }
  std::string& oid_str = oid.value();

  if (oid_str != /* pkcs7-signedData */ "1.2.840.113549.1.7.2") {
    LIEF_INFO("Expecting OID pkcs7-signed-data at {:d} but got {}",
              stream.pos(), oid_to_string(oid_str));
    return make_error_code(lief_errors::read_error);
  }

  tag = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 0);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }
  tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }
  /* ===========================================================================
   * SEQUENCE {
   *  version Version
   *  ...
   * }
   *
   * Version ::= INTEGER
   * ===========================================================================
   */
  {
    auto version = asn1r.read_int();
    if (!version) {
      LIEF_INFO("Can't parse version (pos: {:d})", stream.pos());
      return make_error_code(version.error());
    }
    const int32_t version_val = version.value();
    LIEF_DEBUG("ms_counter_sig.pkcs7-signed-data.version: {:d}", version_val);
    counter_sig->version_ = version.value();
  }

  /* ===========================================================================
   * SEQUENCE {
   *  ...
   *  digestAlgorithms  DigestAlgorithmIdentifiers,
   *  ...
   * }
   *
   * DigestAlgorithmIdentifiers ::= SET OF DigestAlgorithmIdentifier
   * ===========================================================================
   */
  {
    auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET);
    if (!tag) {
      LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
      return make_error_code(tag.error());
    }
    const uintptr_t end_set = stream.pos() + tag.value();
    std::vector<oid_t> algorithms;
    while (stream.pos() < end_set) {
      const size_t current_p = stream.pos();
      auto alg = asn1r.read_alg();
      if (!alg) {
        LIEF_INFO("Can't parse signed data digest algorithm (pos: {:d})", stream.pos());
        break;
      }

      if (stream.pos() == current_p) {
        break;
      }
      LIEF_DEBUG("ms_counter_sig.pkcs7-signed-data.digest-algorithms: {}", oid_to_string(alg.value()));
      algorithms.push_back(std::move(alg.value()));
    }

    if (algorithms.empty()) {
      LIEF_INFO("ms_counter_sig.pkcs7-signed-data.digest-algorithms no algorithms found");
      return make_error_code(lief_errors::read_error);
    }

    if (algorithms.size() > 1) {
      LIEF_INFO("ms_counter_sig.pkcs7-signed-data.digest-algorithms multiple algorithms");
      return make_error_code(lief_errors::read_error);
    }
    counter_sig->digest_algorithm_ = algo_from_oid(algorithms.back());
  }

  /* ===========================================================================
   * SEQUENCE {
   *  ...
   *  contentInfo ContentInfo,
   *  ...
   * }
   * ===========================================================================
   */
  {
    auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (!tag) {
      LIEF_INFO("Wrong tag: {} can't parse content info (pos: {:d})",
                asn1r.get_str_tag(), stream.pos());
      return make_error_code(tag.error());
    }
    const size_t raw_content_size = tag.value();
    SpanStream content_info_stream{stream.p(), tag.value()};

    range_t range = {0, 0};
    if (auto content_info = parse_content_info(content_info_stream, range)) {
      counter_sig->content_info_ = std::move(*content_info);
      //signature.content_info_start_ = stream.pos() + range.start;
      //signature.content_info_end_   = stream.pos() + range.end;
      LIEF_DEBUG("ContentInfo range: {} -> {}", range.start, range.end);
    } else {
      LIEF_INFO("Fail to parse ms_counter_sig.pkcs7-signed-data.content-info");
    }
    stream.increment_pos(raw_content_size);
  }

  /* ===========================================================================
   * SEQUENCE {
   *  ...
   *  certificates [0]  CertificateSet OPTIONAL,
   *  ...
   * }
   * ===========================================================================
   */
  {
    auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED |
                              MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0);
    if (tag) {
      LIEF_DEBUG("ms_counter_sig.pkcs7-signed-data.certificates (offset={}, size={})",
                 stream.pos(), *tag);
      SpanStream certificate_stream{stream.p(), *tag};
      stream.increment_pos(*tag);

      auto certificates = parse_certificates(certificate_stream);
      if (certificates) {
        counter_sig->certificates_ = std::move(*certificates);
      } else {
        LIEF_INFO("Fail to parse ms_counter_sig.pkcs7-signed-data.certificates");
      }
    }
  }
  /* ===========================================================================
   * SEQUENCE {
   *  ...
   *  crls [1]  CertificateRevocationLists OPTIONAL,
   *  ...
   * }
   * ===========================================================================
   */
  {
    auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED |
                              MBEDTLS_ASN1_CONTEXT_SPECIFIC | 1);
    if (tag) {
      LIEF_DEBUG("ms_counter_sig.pkcs7-signed-data.crls offset: {:d}", stream.pos());
      // TODO(romain): process
      SpanStream crls_stream{stream.p(), tag.value()};
      stream.increment_pos(tag.value());
    }
  }

  /* ===========================================================================
   * SEQUENCE {
   *  ...
   *  signerInfos       SignerInfos
   * }
   *
   * SignerInfos ::= SET OF SignerInfo
   * ===========================================================================
   */
  {
    auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET);
    if (tag) {
      LIEF_DEBUG("ms_counter_sig.pkcs7-signed-data.signer-infos offset: {:d}", stream.pos());
      const size_t raw_content_size = tag.value();
      SpanStream signers_stream{stream.p(), raw_content_size};
      stream.increment_pos(raw_content_size);
      if (auto signer_info = parse_signer_infos(signers_stream)) {
        counter_sig->signers_ = std::move(signer_info.value());
      } else {
        LIEF_INFO("Fail to parse ms_counter_sig.pkcs7-signed-data.signer-infos");
      }
    }
  }

  LIEF_DEBUG("Ms-CounterSignature remaining bytes: {}", stream.size() - stream.pos());
  return counter_sig;
}

result<SignatureParser::signer_infos_t> SignatureParser::parse_pkcs9_counter_sign(BinaryStream& stream) {
  // counterSignature ATTRIBUTE ::= {
  //          WITH SYNTAX SignerInfo
  //          ID pkcs-9-at-counterSignature
  //  }
  LIEF_DEBUG("Parsing pkcs9-CounterSign ({} bytes)", stream.size());
  auto counter_sig = parse_signer_infos(stream);
  if (!counter_sig) {
    LIEF_INFO("Fail to parse pkcs9-counter-signature");
    return make_error_code(counter_sig.error());
  }
  LIEF_DEBUG("pkcs9-counter-signature remaining bytes: {}", stream.size() - stream.pos());
  return counter_sig.value();
}

result<Signature> SignatureParser::parse_ms_spc_nested_signature(BinaryStream& stream) {
  // SET of pkcs7-signed data
  LIEF_DEBUG("Parsing Ms-SpcNestedSignature ({} bytes)", stream.size());
  auto sign = SignatureParser::parse(stream, /* skip header */ false);
  if (!sign) {
    LIEF_INFO("Ms-SpcNestedSignature finished with errors");
    return make_error_code(sign.error());
  }
  LIEF_DEBUG("ms-spc-nested-signature remaining bytes: {}", stream.size() - stream.pos());
  return sign.value();
}

result<std::vector<uint8_t>> SignatureParser::parse_pkcs9_message_digest(BinaryStream& stream) {
  ASN1Reader asn1r(stream);
  auto digest = asn1r.read_octet_string();
  if (!digest) {
    LIEF_INFO("Can't process OCTET STREAM for attribute.pkcs9-message-digest (pos: {})",
              stream.pos());
    return make_error_code(digest.error());
  }
  const std::vector<uint8_t>& raw_digest = digest.value();
  LIEF_DEBUG("attribute.pkcs9-message-digest {}", hex_dump(raw_digest));
  LIEF_DEBUG("pkcs9-message-digest remaining bytes: {}", stream.size() - stream.pos());
  return raw_digest;
}

result<oid_t> SignatureParser::parse_ms_spc_statement_type(BinaryStream& stream) {
  // SpcStatementType ::= SEQUENCE of OBJECT IDENTIFIER
  LIEF_DEBUG("Parsing Ms-SpcStatementType ({} bytes)", stream.size());
  ASN1Reader asn1r(stream);
  auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag for ms-spc-statement-type: {} (pos: {:d})",
              asn1r.get_str_tag(), stream.pos());
    return make_error_code(tag.error());
  }

  auto oid = asn1r.read_oid();
  if (!oid) {
    LIEF_INFO("Can't parse ms-spc-statement-type.oid (pos: {:d})", stream.pos());
    return make_error_code(oid.error());
  }
  const oid_t& oid_str = oid.value();
  LIEF_DEBUG("ms-spc-statement-type.oid: {}", oid_to_string(oid_str));
  LIEF_DEBUG("ms-spc-statement-type remaining bytes: {}", stream.size() - stream.pos());
  return oid_str;
}

result<int32_t> SignatureParser::parse_pkcs9_at_sequence_number(BinaryStream& stream) {
  LIEF_DEBUG("Parsing pkcs9-at-SequenceNumber ({} bytes)", stream.size());
  ASN1Reader asn1r(stream);
  auto value = asn1r.read_int();
  if (!value) {
    LIEF_INFO("pkcs9-at-sequence-number: Can't parse integer");
    return make_error_code(value.error());
  }
  LIEF_DEBUG("pkcs9-at-sequence-number.int: {}", value.value());
  LIEF_DEBUG("pkcs9-at-sequence-number remaining bytes: {}", stream.size() - stream.pos());
  return value.value();
}

result<std::string> SignatureParser::parse_spc_string(BinaryStream& stream) {
  // SpcString ::= CHOICE {
  //     unicode                 [0] IMPLICIT BMPSTRING,
  //     ascii                   [1] IMPLICIT IA5STRING
  // }
  LIEF_DEBUG("Parse SpcString ({} bytes)", stream.size());
  ASN1Reader asn1r(stream);
  auto choice = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0);
  if (choice) {
    LIEF_DEBUG("SpcString: Unicode choice");
    const size_t length = choice.value();
    LIEF_DEBUG("spc-string.program-name length: {} (pos: {})", length, stream.pos());

    if (!stream.can_read<char16_t>(length / sizeof(char16_t))) {
      LIEF_INFO("Can't read spc-string.program-name");
      return make_error_code(lief_errors::read_error);
    }
    ToggleEndianness endian(stream);

    auto progname = endian->read_u16string(length / sizeof(char16_t));
    if (!progname) {
      LIEF_INFO("Can't read spc-string.program-name");
      return make_error_code(lief_errors::read_error);
    }

    return u16tou8(*progname);
  }

  if ((choice = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | 1))) {
    LIEF_DEBUG("SpcString: ASCII choice");
    const size_t length = choice.value();
    const char* str = stream.read_array<char>(length);
    if (str == nullptr) {
      LIEF_INFO("Can't read spc-string.program-name");
      return make_error_code(lief_errors::read_error);
    }
    std::string u8progname{str, str + length};
    LIEF_DEBUG("spc-string.program-name: {}", u8progname);
    return u8progname;
  }

  LIEF_INFO("Can't select choice for SpcString (pos: {})", stream.pos());
  return make_error_code(lief_errors::read_error);
}

result<std::string> SignatureParser::parse_spc_link(BinaryStream& stream) {
  // SpcLink ::= CHOICE {
  //     url                     [0] IMPLICIT IA5STRING,
  //     moniker                 [1] IMPLICIT SpcSerializedObject,
  //     file                    [2] EXPLICIT SpcString
  // }
  LIEF_DEBUG("Parse SpcLink ({} bytes)", stream.size());
  ASN1Reader asn1r(stream);
  auto choice = asn1r.read_tag(/* url */ MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0);
  if (choice) {
    const size_t length = choice.value();
    const char* str = stream.read_array<char>(length);
    if (str == nullptr) {
      LIEF_INFO("Can't read spc-link.url");
      return make_error_code(lief_errors::read_error);
    }
    std::string url{str, str + length};
    LIEF_DEBUG("spc-link.url: {}", url);
    return url;
  }

  if ((choice = asn1r.read_tag(/* moniker */ MBEDTLS_ASN1_CONTEXT_SPECIFIC | 1))) {
    LIEF_INFO("Parsing spc-link.moniker is not supported");
    return make_error_code(lief_errors::not_supported);
  }

  if ((choice = asn1r.read_tag(/* file */ MBEDTLS_ASN1_CONTEXT_SPECIFIC | 2))) {
    LIEF_INFO("Parsing spc-link.file is not supported");
    return make_error_code(lief_errors::not_supported);
  }

  LIEF_INFO("Corrupted choice for spc-link (choice: 0x{:x})", stream_get_tag(stream));
  return make_error_code(lief_errors::corrupted);
}


result<SignatureParser::time_t> SignatureParser::parse_pkcs9_signing_time(BinaryStream& stream) {
  // See: https://tools.ietf.org/html/rfc2985#page-20
  // UTCTIME           :171116220536Z
  ASN1Reader asn1r(stream);
  auto tm = asn1r.x509_read_time();
  if (!tm) {
    LIEF_INFO("Can't read pkcs9-signing-time (pos: {})", stream.pos());
    return make_error_code(tm.error());
  }
  std::unique_ptr<mbedtls_x509_time> time = std::move(tm.value());
  LIEF_DEBUG("pkcs9-signing-time {}/{}/{}", time->day, time->mon, time->year);
  return SignatureParser::time_t{time->year, time->mon, time->day,
                                 time->hour, time->min, time->sec};
}

result<std::unique_ptr<Attribute>> SignatureParser::parse_ms_platform_manifest_binary_id(BinaryStream& stream) {
  // SET of UTF8STRING
  LIEF_DEBUG("Parsing szOID_PLATFORM_MANIFEST_BINARY_ID ({} bytes)", stream.size());
  ASN1Reader asn1r(stream);
  auto res = asn1r.read_utf8_string();
  if (res) {
    LIEF_DEBUG("  ID: {} (pos={})", *res, stream.pos());
    LIEF_DEBUG("ms-platform-manifest-binary-id remaining bytes: {}", stream.size() - stream.pos());
    return std::make_unique<MsManifestBinaryID>(std::move(*res));
  }
  return make_error_code(res.error());
}

result<std::unique_ptr<PKCS9TSTInfo>> SignatureParser::parse_pkcs9_tstinfo(BinaryStream& stream) {
  // TODO(romain): Expose an API for this structure
  auto tstinfo = std::make_unique<PKCS9TSTInfo>();

  ASN1Reader asn1r(stream);
  {
    auto tag = asn1r.read_tag(/* [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL */
                              MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED);

    if (!tag) {
      LIEF_INFO("Wrong tag for pkcs9-tstinfo: {} (pos: {:d})",
                asn1r.get_str_tag(), stream.pos());
      return make_error_code(tag.error());
    }
  }

  auto raw = asn1r.read_octet_string();
  if (!raw) {
    LIEF_INFO("Can't read pkcs9-tstinfo encapsulated content");
    return make_error_code(raw.error());
  }
  SpanStream substream(*raw);
  ASN1Reader reader(substream);

  auto tag = reader.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);

  if (!tag) {
    LIEF_INFO("Wrong tag for pkcs9-tstinfo: {} (pos: {:d})",
              reader.get_str_tag(), substream.pos());
    return make_error_code(tag.error());
  }
  /* version INTEGER  { v1(1) } */ {
    if (auto res = reader.read_int()) {
      uint32_t version = *res;
      LIEF_DEBUG("pkcs9-tstinfo.version: {}", version);
    } else {
      LIEF_INFO("Can't read pkcs9-tstinfo.version (pos: {})", substream.pos());
      return make_error_code(res.error());
    }
  }

  /* policy TSAPolicyId */ {
    if (auto res = reader.read_oid()) {
      LIEF_DEBUG("pkcs9-tstinfo.policy: {}", *res);
    } else {
      LIEF_INFO("Can't read pkcs9-tstinfo.policy (pos: {})", substream.pos());
      return make_error_code(res.error());
    }
  }

  /* messageImprint MessageImprint */ {
    auto tag = reader.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (!tag) {
      LIEF_INFO("Wrong tag for pkcs9-tstinfo.message_imprint: {} (pos: {:d})",
                reader.get_str_tag(), substream.pos());
      return make_error_code(tag.error());
    }

    if (auto res = reader.read_alg()) {
      LIEF_DEBUG("pkcs9-tstinfo.message_imprint.hash_algorithm: {}", *res);
      // TODO
    } else {
      LIEF_INFO("Can't read pkcs9-tstinfo.message_imprint.hash_algorithm (pos: {})",
                 substream.pos());
      return make_error_code(res.error());
    }

    if (auto res = reader.read_octet_string()) {
      LIEF_DEBUG("pkcs9-tstinfo.message_imprint.hash_message: {}", hex_dump(*res));
      // TODO
    } else {
      LIEF_INFO("Can't read pkcs9-tstinfo.message_imprint.hash_message (pos: {})",
                 substream.pos());
      return make_error_code(res.error());
    }
  }

  /* serial_number INTEGER */ {
    if (auto res = reader.read_large_int()) {
      LIEF_DEBUG("pkcs9-tstinfo.serial_number: {}", hex_dump(*res));
      // TODO
    } else {
      LIEF_INFO("Can't read pkcs9-tstinfo.serial_number (pos: {} {} {})",
                 substream.pos(), to_string(get_error(res)), substream.size());
      return make_error_code(res.error());
    }
  }

  /* genTime GeneralizedTime */ {
    if (auto tag = reader.read_tag(MBEDTLS_ASN1_GENERALIZED_TIME)) {
      substream.increment_pos(*tag);
      //std::unique_ptr<mbedtls_x509_time> time = std::move(*res);
      //LIEF_DEBUG("pkcs9-tstinfo.gen_time {}/{}/{}",
      //           time->day, time->mon, time->year);
      // TODO
    } else {
      LIEF_INFO("Can't read pkcs9-tstinfo.gen_time (pos: {})",
                 substream.pos());
      return make_error_code(tag.error());
    }
  }

  if (substream.pos() >= substream.size()) {
    return tstinfo;
  }


  /* accuracy Accuracy (OPTIONAL) */ {
    auto res = reader.is_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (res && *res) {
      if (auto tag = reader.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
        substream.increment_pos(*tag);
        // TODO(romain): Read accuracy
      } else {
        LIEF_INFO("Can't read pkcs9-tstinfo.accuracy (pos: {})", substream.pos());
        return make_error_code(tag.error());
      }
    }
  }

  /* ordering BOOLEAN (OPTIONAL) */ {
    auto res = reader.is_tag(MBEDTLS_ASN1_BOOLEAN);
    if (res && *res) {
      if (auto ordering = reader.read_bool()) {
        LIEF_DEBUG("pkcs9-tstinfo.ordering: {}", *ordering);
        // TODO(romain): add field
      } else {
        LIEF_INFO("Can't read pkcs9-tstinfo.ordering (pos: {})", substream.pos());
        return make_error_code(ordering.error());
      }
    }
  }

  /* nonce INTEGER (OPTIONAL) */ {
    auto res = reader.is_tag(MBEDTLS_ASN1_INTEGER);
    if (res && *res) {
      if (auto nonce = reader.read_int64()) {
        LIEF_DEBUG("pkcs9-tstinfo.nonce: {}", *nonce);
        // TODO(romain): add field
      } else {
        LIEF_INFO("Can't read pkcs9-tstinfo.nonce (pos: {})", substream.pos());
        return make_error_code(nonce.error());
      }
    }
  }

  /* tsa [0] GeneralName (OPTIONAL) */ {
    static constexpr auto CTX_SPEC_0 = MBEDTLS_ASN1_CONSTRUCTED |
                                       MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0;
    auto res = reader.is_tag(CTX_SPEC_0);
    if (/* optional check */ res && *res) {
      if (auto ctx = reader.read_tag(CTX_SPEC_0)) {
        //LIEF_ERR("LEN: {} ({} / {})", *ctx, substream.pos(), substream.size());
        //LIEF_ERR("pos: {} {:b}", substream.pos(), *substream.peek<uint8_t>());
        //LIEF_ERR("foo: {:b}", MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC | 4);
        if (auto choice = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC | 4)) {

        } else {
          LIEF_INFO("error: {}", to_string(get_error(choice)));
        }
        substream.increment_pos(*ctx);
        // TODO
      } else {
        LIEF_INFO("Can't read pkcs9-tstinfo.tsa (pos: {})", substream.pos());
        return make_error_code(ctx.error());
      }
    }
  }

  LIEF_DEBUG("pkcs9-tstinfo remaining bytes: {}", substream.size() - substream.pos());
  return tstinfo;
}

result<std::unique_ptr<Attribute>> SignatureParser::parse_spc_relaxed_pe_marker_check(BinaryStream& stream) {
  auto attr = std::make_unique<SpcRelaxedPeMarkerCheck>();
  ASN1Reader asn1r(stream);
  if (auto res = asn1r.read_int()) {
    int value = *res;
    attr->value(value);
    LIEF_DEBUG("spc-relaxed-pe-marker-check: {}", value);
  } else {
    LIEF_INFO("Fail to read spc-relaxed-pe-marker-check");
    return make_error_code(res.error());
  }

  LIEF_DEBUG("spc-relaxed-pe-marker-check remaining bytes: {}", stream.size() - stream.pos());
  return attr;
}


result<std::unique_ptr<Attribute>>
SignatureParser::parse_signing_certificate_v2(BinaryStream& stream) {
  auto scertv2 = std::make_unique<SigningCertificateV2>();
  ASN1Reader asn1r(stream);
  uint64_t nextpos = stream.pos();
  /* certs SEQUENCE OF ESSCertIDv2 */ {
    auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);

    if (!tag) {
      LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
      return make_error_code(tag.error());
    }

    nextpos = stream.pos() + tag.value();

    /* ESSCertIDv2 ::= SEQUENCE */ {
      auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
      if (!tag) {
        LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
        return make_error_code(tag.error());
      }

      /* hashAlgorithm AlgorithmIdentifier DEFAULT {algorithm id-sha256} */
      if (auto res = asn1r.read_alg()) {
        LIEF_DEBUG("SigningCertificateV2.certs.hashAlgorithm: {}", *res);
      } else {
        // Empty -> default value
        if (!asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
          LIEF_INFO("Wrong tag: {} (pos: {:d})", asn1r.get_str_tag(), stream.pos());
          return make_error_code(lief_errors::read_error);
        }
        // Default is algorithm id-sha256
      }
      /* certHash OCTET STRING */
      if (auto res = asn1r.read_octet_string()) {
        LIEF_DEBUG("SigningCertificateV2.certs.certHash: {}", hex_dump(*res));
      } else {
        LIEF_INFO("Can't read SigningCertificateV2.certs.certHash. {} (pos: {:d})",
                  asn1r.get_str_tag(), stream.pos());
        return make_error_code(res.error());
      }

      /* issuerSerial IssuerSerial OPTIONAL */
      tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
      if (tag && *tag) {
        /* issuer       GeneralNames */
        if (auto tag = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
          // TODO(romain): Parse "GeneralNames"
          stream.increment_pos(*tag);
        } else {
          LIEF_INFO("Can't read SigningCertificateV2.certs.issuerSerial.issuer {} (pos: {:d})",
                    asn1r.get_str_tag(), stream.pos());
          return make_error_code(tag.error());
        }
        /* serialNumber CertificateSerialNumber := INTEGER */
        if (auto serial = asn1r.read_large_int()) {
          LIEF_DEBUG("SigningCertificateV2.certs.issuerSerial.serial: {}",
                     hex_dump(*serial));
        } else {
          LIEF_INFO("Can't read SigningCertificateV2.certs.issuerSerial.serial {} (pos: {:d})",
                    asn1r.get_str_tag(), stream.pos());
          return make_error_code(serial.error());
        }
      }
    }
  }
  {
    stream.setpos(nextpos);
    /* policies SEQUENCE OF PolicyInformation OPTIONAL */
    // TODO(romain): to parse
  }
  //auto res = reader.is_tag(MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  LIEF_DEBUG("signing-certificate-v2 remaining bytes: {}", stream.size() - stream.pos());
  return scertv2;
}

result<SignatureParser::SpcPeImageData>
SignatureParser::parse_spc_pe_image_data(BinaryStream& stream) {
  LIEF_DEBUG("Parsing SpcPeImageData");
  ASN1Reader asn1r(stream);

  /* flags SpcPeImageFlags DEFAULT { includeResources } */
  auto flags = asn1r.read_bitstring();
  if (flags && !flags->empty()) {
    LIEF_DEBUG("SpcPeImageData.flags: {}", hex_dump(*flags));
  }

  /* file SpcLink [0] Optional. /!\ According to the documentation, it is not
   * optional.
   */
  if (auto file = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED      |
                                 MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0))
  {
    if (auto url = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0))
    {
      LIEF_DEBUG("SpcPeImageData.url");
      //LIEF_WARN(SUBMISSION_MSG);
    }
    else if (auto moniker = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | 1))
    {
      LIEF_DEBUG("SpcPeImageData.moniker");
      //LIEF_WARN(SUBMISSION_MSG);
    }
    else if (auto file = asn1r.read_tag(MBEDTLS_ASN1_CONSTRUCTED      |
                                        MBEDTLS_ASN1_CONTEXT_SPECIFIC | 2))
    {
      LIEF_DEBUG("SpcPeImageData.file");
      if (auto unicode = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0))
      {
        size_t len = *unicode;
        LIEF_DEBUG("SpcPeImageData.file.unicode (len: {})", len);
        if (len > 0) {
          //LIEF_WARN(SUBMISSION_MSG);
        }
      }
      else if (auto ascii = asn1r.read_tag(MBEDTLS_ASN1_CONTEXT_SPECIFIC | 1))
      {
        LIEF_DEBUG("SpcPeImageData.file.ascii");
        //LIEF_WARN(SUBMISSION_MSG);
      } else {
        LIEF_INFO("SpcPeImageData.file: unknown type");
      }

    }
  }

  // SpcPeImageData ::= SEQUENCE {
  //   flags SpcPeImageFlags DEFAULT { includeResources },
  //   file  SpcLink
  // }
  //
  // SpcPeImageFlags ::= BIT STRING {
  //   includeResources          (0),
  //   includeDebugInfo          (1),
  //   includeImportAddressTable (2)
  // }
  //
  // SpcLink ::= CHOICE {
  //   url     [0] IMPLICIT IA5STRING,
  //   moniker [1] IMPLICIT SpcSerializedObject,
  //   file    [2] EXPLICIT SpcString
  // }
  //
  // SpcString ::= CHOICE {
  //   unicode [0] IMPLICIT BMPSTRING,
  //   ascii   [1] IMPLICIT IA5STRING
  // }
  return {};
}


}
}
