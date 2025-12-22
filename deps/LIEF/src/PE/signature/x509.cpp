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
#include <map>
#include <fstream>

#include <mbedtls/platform.h>
#include "mbedtls/x509_crt.h"
#include "mbedtls/asn1.h"
#include "mbedtls/oid.h"
#include "mbedtls/error.h"

#include "logging.hpp"
#include "frozen.hpp"
#include "internal_utils.hpp"

#include "LIEF/Visitor.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/BinaryStream/ASN1Reader.hpp"

#include "LIEF/PE/signature/x509.hpp"
#include "LIEF/PE/signature/RsaInfo.hpp"
#include "LIEF/PE/EnumToString.hpp"
#include "LIEF/PE/signature/OIDToString.hpp"

namespace {
  // Copy this function from mbedtls since it is not exported
  inline int x509_get_current_time( mbedtls_x509_time *now )
  {
      struct tm *lt, tm_buf;
      mbedtls_time_t tt;
      int ret = 0;

      tt = mbedtls_time(nullptr);
      lt = mbedtls_platform_gmtime_r( &tt, &tm_buf );

      if (lt == nullptr) {
          ret = -1;
      } else {
          now->year = lt->tm_year + 1900;
          now->mon  = lt->tm_mon  + 1;
          now->day  = lt->tm_mday;
          now->hour = lt->tm_hour;
          now->min  = lt->tm_min;
          now->sec  = lt->tm_sec;
      }

      return( ret );
  }

/* mbedtls escapes non printable character with '?' which can be an issue as described
 * in https://github.com/lief-project/LIEF/issues/703.
 * As there is no way to programmatically tweak this behavior, here is a copy
 * of the original function (from <src>/library/x509.c) which skips the non printable character.
 * It seems that Windows follows this behavior as descbied in the Github's issue
 */
int lief_mbedtls_x509_dn_gets( char *buf, size_t size, const mbedtls_x509_name *dn )
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i, n;
    unsigned char c, merge = 0;
    const mbedtls_x509_name *name;
    const char *short_name = NULL;
    //char s[MBEDTLS_X509_MAX_DN_NAME_SIZE];
    char *p;

    //memset( s, 0, sizeof( s ) );

    name = dn;
    p = buf;
    n = size;

    while( name != NULL )
    {
        if( !name->oid.p )
        {
            name = name->next;
            continue;
        }

        if( name != dn )
        {
            ret = mbedtls_snprintf( p, n, merge ? " + " : ", " );
            MBEDTLS_X509_SAFE_SNPRINTF;
        }

        ret = mbedtls_oid_get_attr_short_name( &name->oid, &short_name );

        if( ret == 0 ) {
            ret = mbedtls_snprintf( p, n, "%s=", short_name );
        } else {
            // Get OID numeric string and return x509 friendly name or OID numeric string
            std::string oid_str(64, 0);
            int size = mbedtls_oid_get_numeric_string(oid_str.data(), oid_str.size(), &name->oid);
            if (size >= 0 && size != MBEDTLS_ERR_OID_BUF_TOO_SMALL) {
              oid_str.resize(size);
              const char* friendly_name = LIEF::PE::oid_to_string(oid_str);
              ret = mbedtls_snprintf(p, n, "%s=", friendly_name);
            } else {
              ret = size;
            }
        }
        MBEDTLS_X509_SAFE_SNPRINTF;

        std::string out;
        out.reserve(200);
        for( i = 0; i < name->val.len; i++ )
        {
            if( i >= MBEDTLS_X509_MAX_DN_NAME_SIZE - 1 )
                break;

            c = name->val.p[i];

            // Special characters requiring escaping, RFC 1779
            if( c && strchr( ",=+<>#;\"\\", c ) )
            {
                if( i + 1 >= MBEDTLS_X509_MAX_DN_NAME_SIZE - 1 )
                  break;
                out.push_back('\\');
            }

            if( c < 32 )
              continue;
            //else s[i] = c;
            out.push_back(c);
        }
        //s[i] = '\0';
        ret = mbedtls_snprintf( p, n, "%s", out.c_str() );
        MBEDTLS_X509_SAFE_SNPRINTF;

        merge = name->private_next_merged;
        name = name->next;
    }

    return( (int) ( size - n ) );
}

}


namespace LIEF {
namespace PE {

inline x509::VERIFICATION_FLAGS from_mbedtls_err(uint32_t err) {
  CONST_MAP(uint32_t, x509::VERIFICATION_FLAGS, 20) MBEDTLS_ERR_TO_LIEF = {
    { MBEDTLS_X509_BADCERT_EXPIRED,       x509::VERIFICATION_FLAGS::BADCERT_EXPIRED},
    { MBEDTLS_X509_BADCERT_REVOKED,       x509::VERIFICATION_FLAGS::BADCERT_REVOKED},
    { MBEDTLS_X509_BADCERT_CN_MISMATCH,   x509::VERIFICATION_FLAGS::BADCERT_CN_MISMATCH},
    { MBEDTLS_X509_BADCERT_NOT_TRUSTED,   x509::VERIFICATION_FLAGS::BADCERT_NOT_TRUSTED},
    { MBEDTLS_X509_BADCRL_NOT_TRUSTED,    x509::VERIFICATION_FLAGS::BADCRL_NOT_TRUSTED},
    { MBEDTLS_X509_BADCRL_EXPIRED,        x509::VERIFICATION_FLAGS::BADCRL_EXPIRED},
    { MBEDTLS_X509_BADCERT_MISSING,       x509::VERIFICATION_FLAGS::BADCERT_MISSING},
    { MBEDTLS_X509_BADCERT_SKIP_VERIFY,   x509::VERIFICATION_FLAGS::BADCERT_SKIP_VERIFY},
    { MBEDTLS_X509_BADCERT_OTHER,         x509::VERIFICATION_FLAGS::BADCERT_OTHER},
    { MBEDTLS_X509_BADCERT_FUTURE,        x509::VERIFICATION_FLAGS::BADCERT_FUTURE},
    { MBEDTLS_X509_BADCRL_FUTURE,         x509::VERIFICATION_FLAGS::BADCRL_FUTURE},
    { MBEDTLS_X509_BADCERT_KEY_USAGE,     x509::VERIFICATION_FLAGS::BADCERT_KEY_USAGE},
    { MBEDTLS_X509_BADCERT_EXT_KEY_USAGE, x509::VERIFICATION_FLAGS::BADCERT_EXT_KEY_USAGE},
    { MBEDTLS_X509_BADCERT_NS_CERT_TYPE,  x509::VERIFICATION_FLAGS::BADCERT_NS_CERT_TYPE},
    { MBEDTLS_X509_BADCERT_BAD_MD,        x509::VERIFICATION_FLAGS::BADCERT_BAD_MD},
    { MBEDTLS_X509_BADCERT_BAD_PK,        x509::VERIFICATION_FLAGS::BADCERT_BAD_PK},
    { MBEDTLS_X509_BADCERT_BAD_KEY,       x509::VERIFICATION_FLAGS::BADCERT_BAD_KEY},
    { MBEDTLS_X509_BADCRL_BAD_MD,         x509::VERIFICATION_FLAGS::BADCRL_BAD_MD},
    { MBEDTLS_X509_BADCRL_BAD_PK,         x509::VERIFICATION_FLAGS::BADCRL_BAD_PK},
    { MBEDTLS_X509_BADCRL_BAD_KEY,        x509::VERIFICATION_FLAGS::BADCRL_BAD_KEY},
  };
  x509::VERIFICATION_FLAGS flags = x509::VERIFICATION_FLAGS::OK;
  for (const auto& p : MBEDTLS_ERR_TO_LIEF) {
    if ((err & p.first) == p.first) {
      flags |= p.second;
    }
  }
  return flags;
}

inline x509::date_t from_mbedtls(const mbedtls_x509_time& time) {
  return {
    time.year,
    time.mon,
    time.day,
    time.hour,
    time.min,
    time.sec
  };
}

x509::certificates_t x509::parse(const std::string& path) {

  std::ifstream cert_fs(path);
  if (!cert_fs) {
    LIEF_WARN("Can't open {}", path);
    return {};
  }
  cert_fs.unsetf(std::ios::skipws);
  cert_fs.seekg(0, std::ios::end);
  const size_t size = cert_fs.tellg();
  cert_fs.seekg(0, std::ios::beg);

  std::vector<uint8_t> raw(size + 1, 0);
  cert_fs.read(reinterpret_cast<char*>(raw.data()), raw.size());
  return x509::parse(raw);
}

x509::certificates_t x509::parse(const std::vector<uint8_t>& content) {
  std::unique_ptr<mbedtls_x509_crt> ca{new mbedtls_x509_crt{}};
  mbedtls_x509_crt_init(ca.get());
  int ret = mbedtls_x509_crt_parse(ca.get(), content.data(), content.size());
  if (ret != 0) {
    if (ret < 0) {
      std::string strerr(1024, 0);
      mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
      LIEF_WARN("Failed to parse certificate blob: '{}' ({})", strerr, ret);
      return {};
    }
    // If ret > 0, it contains the number of certificates that the parser did not
    // manage to parse
    LIEF_WARN("{} certificates are not parsed", ret);
  }
  std::vector<x509> crts;

  mbedtls_x509_crt* prev = nullptr;
  mbedtls_x509_crt* current = ca.release();
  while (current != nullptr && current != prev) {
    mbedtls_x509_crt* next = current->next;
    current->next = nullptr;
    crts.emplace_back(current);
    prev = current;
    current = next;
  }
  return crts;
}


bool x509::check_time(const date_t& before, const date_t& after) {
  // Implementation taken
  // from https://github.com/ARMmbed/mbedtls/blob/1c54b5410fd48d6bcada97e30cac417c5c7eea67/library/x509.c#L926-L962
  if (before[0] > after[0]) {
    LIEF_DEBUG("{} > {}", before[0], after[0]);
    return false;
  }

  if (
      before[0] == after[0] &&
      before[1]  > after[1]
     )
  {
    LIEF_DEBUG("{} > {}", before[1], after[1]);
    return false;
  }

  if (
      before[0] == after[0] &&
      before[1] == after[1] &&
      before[2]  > after[2]
     )
  {
    LIEF_DEBUG("{} > {}", before[2], after[2]);
    return false;
  }

  if (
      before[0] == after[0] &&
      before[1] == after[1] &&
      before[2] == after[2] &&
      before[3]  > after[3]
     )
  {
    LIEF_DEBUG("{} > {}", before[3], after[3]);
    return false;
  }

  if (
      before[0] == after[0] &&
      before[1] == after[1] &&
      before[2] == after[2] &&
      before[3] == after[3] &&
      before[4]  > after[4]
     )
  {
    LIEF_DEBUG("{} > {}", before[4], after[4]);
    return false;
  }

  if (
      before[0] == after[0] &&
      before[1] == after[1] &&
      before[2] == after[2] &&
      before[3] == after[3] &&
      before[4] == after[4] &&
      before[5]  > after[5]
     )
  {
    LIEF_DEBUG("{} > {}", before[5], after[5]);
    return false;
  }

  if (
      before[0] == after[0] &&
      before[1] == after[1] &&
      before[2] == after[2] &&
      before[3] == after[3] &&
      before[4] == after[4] &&
      before[5] == after[5] &&
      before[6]  > after[6]
     )
  {
    LIEF_DEBUG("{} > {}", before[6], after[6]);
    return false;
  }

  return true;
}

bool x509::time_is_past(const date_t& to) {
  mbedtls_x509_time now;

  if (x509_get_current_time(&now) != 0) {
    return true;
  }
  // check_time(): true if now < to else false
  return !check_time(from_mbedtls(now), to);
}

bool x509::time_is_future(const date_t& from) {
  mbedtls_x509_time now;

  if (x509_get_current_time(&now) != 0) {
    return true;
  }
  return check_time(from_mbedtls(now), from);
}

x509::x509() = default;

x509::x509(mbedtls_x509_crt* ca) :
  x509_cert_{ca}
{}

x509::x509(const x509& other) :
  Object::Object{other}
{
  auto* crt = new mbedtls_x509_crt{};
  mbedtls_x509_crt_init(crt);
  int ret = mbedtls_x509_crt_parse_der(crt, other.x509_cert_->raw.p,
                                       other.x509_cert_->raw.len);
  if (ret != 0) {
    LIEF_WARN("Failed to copy x509 certificate");
    delete crt;
    return;
  }

  x509_cert_ = crt;
}

x509& x509::operator=(x509 other) {
  swap(other);
  return *this;
}


void x509::swap(x509& other) {
  std::swap(x509_cert_, other.x509_cert_);
}

uint32_t x509::version() const {
  return x509_cert_->version;
}

std::vector<uint8_t> x509::serial_number() const {
  return {x509_cert_->serial.p,
          x509_cert_->serial.p + x509_cert_->serial.len};
}

oid_t x509::signature_algorithm() const {
  std::array<char, 256> oid_str;
  mbedtls_oid_get_numeric_string(oid_str.data(), oid_str.size(), &x509_cert_->sig_oid);
  return oid_t{oid_str.data()};

}

x509::date_t x509::valid_from() const {
  return from_mbedtls(x509_cert_->valid_from);
}

x509::date_t x509::valid_to() const {
  return from_mbedtls(x509_cert_->valid_to);
}


std::string x509::issuer() const {
  std::array<char, 1024> buffer;
  lief_mbedtls_x509_dn_gets(buffer.data(), buffer.size(), &x509_cert_->issuer);
  return buffer.data();
}

std::string x509::subject() const {
  std::array<char, 1024> buffer;
  lief_mbedtls_x509_dn_gets(buffer.data(), buffer.size(), &x509_cert_->subject);
  return buffer.data();
}

std::vector<uint8_t> x509::raw() const {
  return {x509_cert_->raw.p,
          x509_cert_->raw.p + x509_cert_->raw.len};
}


x509::KEY_TYPES x509::key_type() const {
  CONST_MAP(mbedtls_pk_type_t, x509::KEY_TYPES, 7) mtype2asi = {
    {MBEDTLS_PK_NONE,       KEY_TYPES::NONE       },
    {MBEDTLS_PK_RSA,        KEY_TYPES::RSA        },
    {MBEDTLS_PK_ECKEY,      KEY_TYPES::ECKEY      },
    {MBEDTLS_PK_ECKEY_DH,   KEY_TYPES::ECKEY_DH   },
    {MBEDTLS_PK_ECDSA,      KEY_TYPES::ECDSA      },
    {MBEDTLS_PK_RSA_ALT,    KEY_TYPES::RSA_ALT    },
    {MBEDTLS_PK_RSASSA_PSS, KEY_TYPES::RSASSA_PSS },
  };

  mbedtls_pk_context* ctx = &(x509_cert_->pk);
  mbedtls_pk_type_t type  = mbedtls_pk_get_type(ctx);

  const auto it_key = mtype2asi.find(type);
  if (it_key != std::end(mtype2asi)) {
    return it_key->second;
  }
  return KEY_TYPES::NONE;
}


std::unique_ptr<RsaInfo> x509::rsa_info() const {
  if (key_type() == KEY_TYPES::RSA) {
    mbedtls_rsa_context* rsa_ctx = mbedtls_pk_rsa(x509_cert_->pk);
    return std::unique_ptr<RsaInfo>{new RsaInfo{rsa_ctx}};
  }
  return nullptr;
}

std::vector<uint8_t> pkcs1_15_unpad(const std::vector<uint8_t>& input) {
  // EB = 00 || BT || PS || 00 || D
  if (input.size() < 10) {
    return {};
  }

  // According ot the RFC, the leading byte must be 0:
  if (input[0] != 0x00) {
    return {};
  }
  const uint8_t BT = input[1];
  if (BT != 0 && BT != 1 && BT != 2) {
    return {};
  }

  // We only support "1" type (signature)
  if (BT != 1) {
    return {};
  }

  auto padding_start = input.begin() + 2;
  auto padding_end = std::find_if(padding_start, input.end(),
                                  [] (uint8_t x) { return x != 0xFF; });
  if (padding_end == input.end()) {
    return {};
  }

  auto content_start = padding_end + 1;
  size_t content_size  = input.size() - (content_start - input.begin());
  if (content_start == input.end()) {
    return {};
  }

  SpanStream stream(&*content_start, content_size);
  ASN1Reader asn1r(stream);

  {
    auto res = asn1r.read_tag(MBEDTLS_ASN1_SEQUENCE | MBEDTLS_ASN1_CONSTRUCTED);
    if (!res) {
      return stream.content();
    }
  }

  /* digestAlgorithm DigestAlgorithmIdentifier */ {
    auto res = asn1r.read_alg();
    if (!res) {
      return stream.content();
    }

    LIEF_DEBUG("PKCS1 1.5 padding - Algo: {}", *res);
  }

  /* digest Digest */ {
    auto res = asn1r.read_octet_string();
    if (!res) {
      return stream.content();
    }
    LIEF_DEBUG("PKCS1 1.5 padding - Digest: {}", hex_dump(*res));
    return *res;
  }

  return stream.content();
}

bool x509::check_signature(const std::vector<uint8_t>& hash, const std::vector<uint8_t>& signature, ALGORITHMS algo) const {
  CONST_MAP(ALGORITHMS, mbedtls_md_type_t, 5) LIEF2MBED_MD = {
    //{ALGORITHMS::MD2, MBEDTLS_MD_MD2},
    //{ALGORITHMS::MD4, MBEDTLS_MD_MD4},
    {ALGORITHMS::MD5, MBEDTLS_MD_MD5},

    {ALGORITHMS::SHA_1,   MBEDTLS_MD_SHA1},
    {ALGORITHMS::SHA_256, MBEDTLS_MD_SHA256},
    {ALGORITHMS::SHA_384, MBEDTLS_MD_SHA384},
    {ALGORITHMS::SHA_512, MBEDTLS_MD_SHA512},
  };

  auto it_md = LIEF2MBED_MD.find(algo);
  if (it_md == std::end(LIEF2MBED_MD)) {
    LIEF_ERR("Can't find algorithm {}", to_string(algo));
    return false;
  }
  mbedtls_pk_context& ctx = x509_cert_->pk;
  int ret = mbedtls_pk_verify(&ctx,
    /* MD_HASH_ALGO       */ it_md->second,
    /* Input Hash         */ hash.data(), hash.size(),
    /* Signature provided */ signature.data(), signature.size());

  /* If the verification failed with mbedtls_pk_verify it
   * does notnecessity means that the signatures don't match.
   *
   * For RSA public-key scheme, mbedtls encodes the hash with rsa_rsassa_pkcs1_v15_encode() so that it expands
   * the hash value with encoded data. On some samples, this encoding failed.
   *
   * In the approach below, we manually decrypt and unpad the output of the DEC(signature)
   * as defined in the RFC #2313
   */
  if (ret != 0) {
    if (mbedtls_pk_get_type(&ctx) == MBEDTLS_PK_RSA) {
      auto* ctx_rsa = reinterpret_cast<mbedtls_rsa_context*>(ctx.private_pk_ctx);
      if ((ctx_rsa->private_len * 8) < 100 || (ctx_rsa->private_len * 8) > 2048llu * 10) {
        LIEF_INFO("RSA Key length is not valid ({} bits)", ctx_rsa->private_len * 8);
        return false;
      }
      std::vector<uint8_t> decrypted(ctx_rsa->private_len);

      int ret_rsa_public = mbedtls_rsa_public(ctx_rsa, signature.data(), decrypted.data());
      if (ret_rsa_public != 0) {
        std::string strerr(1024, 0);
        mbedtls_strerror(ret_rsa_public, const_cast<char*>(strerr.data()), strerr.size());
        LIEF_INFO("RSA public key operation failed: '{}'", strerr);
        return false;
      }

      std::vector<uint8_t> unpadded = pkcs1_15_unpad(decrypted);

      if (unpadded == hash) {
        return true;
      }
    }
    if (ret != 0) {
      std::string strerr(1024, 0);
      mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
      LIEF_INFO("decrypt() failed with error: '{}'", strerr);
      return false;
    }
    return true;
  }
  return true;
}


x509::VERIFICATION_FLAGS x509::is_trusted_by(const std::vector<x509>& ca) const {
  if (ca.empty()) {
    LIEF_WARN("Certificate chain is empty");
    return VERIFICATION_FLAGS::BADCERT_MISSING;
  }
  std::vector<x509> ca_list = ca; // Explicit copy since we will modify mbedtls_x509_crt->next
  for (size_t i = 0; i < ca_list.size() - 1; ++i) {
    ca_list[i].x509_cert_->next = ca_list[i + 1].x509_cert_;
  }

  VERIFICATION_FLAGS result = VERIFICATION_FLAGS::OK;
  uint32_t flags = 0;
  mbedtls_x509_crt_profile profile = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_MD5)   |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1)   |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA224) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF, /* Any PK alg    */
    0xFFFFFFF, /* Any curve     */
    1          /* Min RSA key   */,
  };

  int ret = mbedtls_x509_crt_verify_with_profile(
      /* crt          */ x509_cert_,
      /* Trusted CA   */ ca_list.front().x509_cert_,
      /* CA's CRLs    */ nullptr,
      /* profile      */ &profile,
      /* Common Name  */ nullptr,
      /* Verification */ &flags,
      /* verification function */ nullptr,
      /* verification params   */ nullptr);

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    std::string out(1024, 0);
    mbedtls_x509_crt_verify_info(const_cast<char*>(out.data()), out.size(), "", flags);
    LIEF_WARN("X509 verify failed with: {} (0x{:x})\n{}", strerr, ret, out);
    result = from_mbedtls_err(flags);
  }

  // Clear the chain since ~x509() will delete each object
  for (size_t i = 0; i < ca_list.size(); ++i) {
    ca_list[i].x509_cert_->next = nullptr;
  }
  return result;
}

x509::VERIFICATION_FLAGS x509::verify(const x509& ca) const {
  uint32_t flags = 0;
  VERIFICATION_FLAGS result = VERIFICATION_FLAGS::OK;
  mbedtls_x509_crt_profile profile = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1)   |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA224) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF, /* Any PK alg    */
    0xFFFFFFF, /* Any curve     */
    1          /* Min RSA key */,
  };

  int ret = mbedtls_x509_crt_verify_with_profile(
      /* crt          */ ca.x509_cert_,
      /* Trusted CA   */ x509_cert_,
      /* CA's CRLs    */ nullptr,
      /* profile      */ &profile,
      /* Common Name  */ nullptr,
      /* Verification */ &flags,
      /* verification function */ nullptr,
      /* verification params   */ nullptr);

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    std::string out(1024, 0);
    mbedtls_x509_crt_verify_info(const_cast<char*>(out.data()), out.size(), "", flags);
    LIEF_WARN("X509 verify failed with: {} (0x{:x})\n{}", strerr, ret, out);
    result = from_mbedtls_err(flags);
  }
  return result;
}

std::vector<oid_t> x509::ext_key_usage() const {
  if ((x509_cert_->private_ext_types & MBEDTLS_X509_EXT_EXTENDED_KEY_USAGE) == 0) {
    return {};
  }
  mbedtls_asn1_sequence* current = &x509_cert_->ext_key_usage;
  std::vector<oid_t> oids;
  while (current != nullptr) {
    char oid_str[256] = {0};
    int ret = mbedtls_oid_get_numeric_string(oid_str, sizeof(oid_str), &current->buf);
    if (ret != MBEDTLS_ERR_OID_BUF_TOO_SMALL) {
      LIEF_DEBUG("OID: {}", oid_str);
      oids.emplace_back(oid_str);
    } else {
      std::string strerr(1024, 0);
      mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
      LIEF_WARN("{}", strerr);
    }
    if (current->next == current) {
      break;
    }
    current = current->next;
  }
  return oids;
}

std::vector<oid_t> x509::certificate_policies() const {
  if ((x509_cert_->private_ext_types & MBEDTLS_OID_X509_EXT_CERTIFICATE_POLICIES) == 0) {
    return {};
  }

  mbedtls_x509_sequence& policies = x509_cert_->certificate_policies;
  mbedtls_asn1_sequence* current = &policies;
  std::vector<oid_t> oids;
  while (current != nullptr) {
    char oid_str[256] = {0};
    int ret = mbedtls_oid_get_numeric_string(oid_str, sizeof(oid_str), &current->buf);
    if (ret != MBEDTLS_ERR_OID_BUF_TOO_SMALL) {
      oids.emplace_back(oid_str);
    } else {
      std::string strerr(1024, 0);
      mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
      LIEF_WARN("{}", strerr);
    }
    if (current->next == current) {
      break;
    }
    current = current->next;
  }
  return oids;
}

bool x509::is_ca() const {
  if ((x509_cert_->private_ext_types & MBEDTLS_X509_EXT_BASIC_CONSTRAINTS) == 0) {
    return true;
  }
  return x509_cert_->private_ca_istrue != 0;
}

std::vector<x509::KEY_USAGE> x509::key_usage() const {
  static const std::map<uint32_t, KEY_USAGE> MBEDTLS_MAP = {
    {MBEDTLS_X509_KU_DIGITAL_SIGNATURE, KEY_USAGE::DIGITAL_SIGNATURE},
    {MBEDTLS_X509_KU_NON_REPUDIATION,   KEY_USAGE::NON_REPUDIATION},
    {MBEDTLS_X509_KU_KEY_ENCIPHERMENT,  KEY_USAGE::KEY_ENCIPHERMENT},
    {MBEDTLS_X509_KU_DATA_ENCIPHERMENT, KEY_USAGE::DATA_ENCIPHERMENT},
    {MBEDTLS_X509_KU_KEY_AGREEMENT,     KEY_USAGE::KEY_AGREEMENT},
    {MBEDTLS_X509_KU_KEY_CERT_SIGN,     KEY_USAGE::KEY_CERT_SIGN},
    {MBEDTLS_X509_KU_CRL_SIGN,          KEY_USAGE::CRL_SIGN},
    {MBEDTLS_X509_KU_ENCIPHER_ONLY,     KEY_USAGE::ENCIPHER_ONLY},
    {MBEDTLS_X509_KU_DECIPHER_ONLY,     KEY_USAGE::DECIPHER_ONLY},
  };

  if ((x509_cert_->private_ext_types & MBEDTLS_X509_EXT_KEY_USAGE) == 0) {
    return {};
  }

  const uint32_t ku = x509_cert_->private_key_usage;
  std::vector<KEY_USAGE> usages;
  for (const auto& p : MBEDTLS_MAP) {
    if ((ku & p.first) > 0) {
      usages.push_back(p.second);
    }
  }
  return usages;
}

std::vector<uint8_t> x509::signature() const {
  mbedtls_x509_buf sig =  x509_cert_->private_sig;
  return {sig.p, sig.p + sig.len};
}

void x509::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

x509::~x509() {
  mbedtls_x509_crt_free(x509_cert_);
  delete x509_cert_;
}

std::ostream& operator<<(std::ostream& os, const x509& x509_cert) {
  std::vector<char> buffer(2048, 0);
  int ret = mbedtls_x509_crt_info(buffer.data(), buffer.size(), "", x509_cert.x509_cert_);
  if (ret < 0) {
    os << "Can't print certificate information\n";
    return os;
  }
  std::string crt_str(buffer.data());
  os << crt_str;
  return os;
}

}
}
