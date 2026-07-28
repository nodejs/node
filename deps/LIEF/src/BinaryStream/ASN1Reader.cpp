/* Copyright 2021 - 2025 R. Thomas
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
#include "LIEF/BinaryStream/ASN1Reader.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "logging.hpp"

#include <array>
#include <mbedtls/platform.h>
#include <mbedtls/asn1.h>
#include <mbedtls/error.h>
#include <mbedtls/oid.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/bignum.h>

extern "C" {
int mbedtls_x509_get_name(unsigned char **p, const unsigned char *end,
                          mbedtls_x509_name *cur);
int mbedtls_x509_get_time(unsigned char **p, const unsigned char *end,
                          mbedtls_x509_time *t);

int mbedtls_x509_get_serial(unsigned char **p, const unsigned char *end,
                            mbedtls_x509_buf *serial);
}

namespace LIEF {

inline void free_names(mbedtls_x509_name& names) {
  mbedtls_x509_name *name_cur;
  name_cur = names.next;
  while (name_cur != nullptr) {
    mbedtls_x509_name *name_prv = name_cur;
    name_cur = name_cur->next;
    mbedtls_free(name_prv);
  }
}

result<bool> ASN1Reader::is_tag(int tag) {
  size_t out = 0;
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_tag(&p, end, &out, tag);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret == MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
    return false;
  }

  if (ret != 0) {
    return make_error_code(lief_errors::read_error);
  }

  return true;
}

result<size_t> ASN1Reader::read_tag(int tag) {
  size_t out = 0;

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_tag(&p, end, &out, tag);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret == MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) {
    return make_error_code(lief_errors::asn1_bad_tag);
  }

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("mbedtls_asn1_get_tag: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return out;
}


result<size_t> ASN1Reader::read_len() {
  size_t len = 0;

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_len(&p, end, &len);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret != 0) {
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return len;
}

result<std::string> ASN1Reader::read_alg() {
  mbedtls_asn1_buf alg_oid;
  std::array<char, 256> oid_str = {0};

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_alg_null(&p, end, &alg_oid);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret != 0) {
    return make_error_code(lief_errors::read_error);
  }

  ret = mbedtls_oid_get_numeric_string(oid_str.data(), oid_str.size(), &alg_oid);
  if (ret <= 0) {
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return std::string(oid_str.data());
}

result<std::string> ASN1Reader::read_oid() {
  mbedtls_asn1_buf buf;
  std::array<char, 256> oid_str = {0};

  auto len = read_tag(MBEDTLS_ASN1_OID);
  if (!len) {
    return make_error_code(len.error());
  }

  buf.len = len.value();
  buf.p   = stream_.p();
  buf.tag = MBEDTLS_ASN1_OID;

  int ret = mbedtls_oid_get_numeric_string(oid_str.data(), oid_str.size(), &buf);
  if (ret == MBEDTLS_ERR_OID_BUF_TOO_SMALL) {
    LIEF_DEBUG("asn1_read_oid: mbedtls_oid_get_numeric_string return MBEDTLS_ERR_OID_BUF_TOO_SMALL");
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(buf.len);
  return std::string(oid_str.data());
}


result<bool> ASN1Reader::read_bool() {
  int value;

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_bool(&p, end, &value);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("mbedtls_asn1_get_bool: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return static_cast<bool>(value);
}

result<std::vector<uint8_t>> ASN1Reader::read_large_int() {
  std::vector<uint8_t> value;
  mbedtls_mpi mpi;

  mbedtls_mpi_init(&mpi);

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_mpi(&p, end, &mpi);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("mbedtls_asn1_get_mpi: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  value.resize(mbedtls_mpi_size(&mpi));

  ret = mbedtls_mpi_write_binary_le(&mpi, value.data(), value.size());

  mbedtls_mpi_free(&mpi);

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("mbedtls_asn1_get_mpi: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return value;
}



result<int64_t> ASN1Reader::read_int64() {
  int64_t value = 0;
  mbedtls_mpi mpi;

  mbedtls_mpi_init(&mpi);

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_mpi(&p, end, &mpi);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("mbedtls_asn1_get_mpi: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  if (mbedtls_mpi_size(&mpi) > sizeof(uint64_t)) {
    mbedtls_mpi_free(&mpi);
    LIEF_INFO("MPI value can be stored on a 64-bit integer");
    return make_error_code(lief_errors::read_error);
  }

  ret = mbedtls_mpi_write_binary_le(&mpi, reinterpret_cast<uint8_t*>(&value),
                                    sizeof(value));

  mbedtls_mpi_free(&mpi);

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("mbedtls_asn1_get_mpi: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return value;
}


result<int32_t> ASN1Reader::read_int() {
  int32_t value = 0;

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_int(&p, end, &value);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("mbedtls_asn1_get_int: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return value;
}

result<std::vector<uint8_t>> ASN1Reader::read_bitstring() {
  mbedtls_asn1_bitstring bs = {0, 0, nullptr};

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_asn1_get_bitstring(&p, end, &bs);

  if (ret == MBEDTLS_ERR_ASN1_OUT_OF_DATA) {
    return make_error_code(lief_errors::read_out_of_bound);
  }

  if (ret == MBEDTLS_ERR_ASN1_LENGTH_MISMATCH) {
    stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
    return std::vector<uint8_t>{bs.p, bs.p + bs.len};
  }

  if (ret != 0) {
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return std::vector<uint8_t>{bs.p, bs.p + bs.len};
}


result<std::vector<uint8_t>> ASN1Reader::read_octet_string() {
  auto tag = read_tag(MBEDTLS_ASN1_OCTET_STRING);
  if (!tag) {
    return make_error_code(tag.error());
  }
  std::vector<uint8_t> raw = {stream_.p(), stream_.p() + tag.value()};
  stream_.increment_pos(tag.value());
  return raw;
}

result<std::unique_ptr<mbedtls_x509_crt>> ASN1Reader::read_cert() {
  std::unique_ptr<mbedtls_x509_crt> ca{new mbedtls_x509_crt{}};
  mbedtls_x509_crt_init(ca.get());

  uint8_t* p               = stream_.p();
  const uint8_t* end       = stream_.end();
  const uintptr_t buff_len = reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(p);

  int ret = mbedtls_x509_crt_parse_der(ca.get(), p, /* buff len */ buff_len);
  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_DEBUG("asn1_read_cert(): {} (pos={})", strerr.c_str(), stream_.pos());
    return make_error_code(lief_errors::read_error);
  }
  if (ca->raw.len <= 0) {
    return make_error_code(lief_errors::read_error);
  }
  stream_.increment_pos(ca->raw.len);
  return ca;
}

result<std::string> ASN1Reader::x509_read_names() {
  mbedtls_x509_name name;
  std::memset(&name, 0, sizeof(name));

  auto tag = read_tag(/* Name */
                      MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
  if (!tag) {
    LIEF_INFO("Wrong tag: 0x{:x} for x509_read_names (pos: {:d})",
              *stream_.peek<uint8_t>(), stream_.pos());
    return make_error_code(tag.error());
  }

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = p + tag.value();
  int ret = mbedtls_x509_get_name(&p, end, &name);
  if (ret != 0) {
    free_names(name);
    LIEF_DEBUG("mbedtls_x509_get_name failed with {:d}", ret);
    return make_error_code(lief_errors::read_error);
  }
  std::array<char, 1024> buffer = {0};
  ret = mbedtls_x509_dn_gets(buffer.data(), buffer.size(), &name);
  free_names(name);

  if (ret < 0) {
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return std::string(buffer.data());
}

result<std::vector<uint8_t>> ASN1Reader::x509_read_serial() {
  mbedtls_x509_buf serial;

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_x509_get_serial(&p, end, &serial);

  if (ret != 0) {
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return std::vector<uint8_t>{serial.p, serial.p + serial.len};
}

result<std::unique_ptr<mbedtls_x509_time>> ASN1Reader::x509_read_time() {
  std::unique_ptr<mbedtls_x509_time> tm{new mbedtls_x509_time{}};

  const uint8_t* cur_p = stream_.p();
  uint8_t* p           = stream_.p();
  const uint8_t* end   = stream_.end();

  int ret = mbedtls_x509_get_time(&p, end, tm.get());

  if (ret != 0) {
    std::string strerr(1024, 0);
    mbedtls_strerror(ret, const_cast<char*>(strerr.data()), strerr.size());
    LIEF_INFO("mbedtls_x509_get_time: {}", strerr.c_str());
    return make_error_code(lief_errors::read_error);
  }

  stream_.increment_pos(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(cur_p));
  return tm;
}

result<std::string> ASN1Reader::read_utf8_string() {
  auto tag = read_tag(MBEDTLS_ASN1_UTF8_STRING);
  if (!tag) {
    return make_error_code(tag.error());
  }
  std::string raw = {stream_.p(), stream_.p() + tag.value()};
  stream_.increment_pos(tag.value());
  return raw;
}


std::string ASN1Reader::get_str_tag() {
  if (auto tag = stream_.peek<uint8_t>()) {
    return tag2str(*tag);
  }
  return "MBEDTLS_ASN1_UNKNOWN";
}


std::string ASN1Reader::tag2str(int tag) {
#define HANDLE(X) do {     \
  if (tag & MBEDTLS_##X) { \
    tag_str += " | " #X;    \
  }                        \
} while(0)

  std::string tag_str;

  HANDLE(ASN1_BOOLEAN);
  HANDLE(ASN1_INTEGER);
  HANDLE(ASN1_BIT_STRING);
  HANDLE(ASN1_OCTET_STRING);
  HANDLE(ASN1_NULL);
  HANDLE(ASN1_OID);
  HANDLE(ASN1_ENUMERATED);
  HANDLE(ASN1_UTF8_STRING);
  HANDLE(ASN1_SEQUENCE);
  HANDLE(ASN1_SET);
  HANDLE(ASN1_PRINTABLE_STRING);
  HANDLE(ASN1_T61_STRING);
  HANDLE(ASN1_IA5_STRING);
  HANDLE(ASN1_UTC_TIME);
  HANDLE(ASN1_GENERALIZED_TIME);
  HANDLE(ASN1_UNIVERSAL_STRING);
  HANDLE(ASN1_BMP_STRING);
  HANDLE(ASN1_PRIMITIVE);
  HANDLE(ASN1_CONSTRUCTED);
  HANDLE(ASN1_CONTEXT_SPECIFIC);

  if (tag_str.size() < 3) {
    return "MBEDTLS_ASN1_UNKNOWN";
  }

  return tag_str.substr(3);
#undef HANDLE
}

}
