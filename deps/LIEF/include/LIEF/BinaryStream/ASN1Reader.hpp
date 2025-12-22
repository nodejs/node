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
#ifndef LIEF_ASN1_READER_H
#define LIEF_ASN1_READER_H

#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#include "LIEF/errors.hpp"

struct mbedtls_x509_crt;
struct mbedtls_x509_time;

namespace LIEF {
class BinaryStream;

class ASN1Reader {
  public:
  ASN1Reader() = delete;

  ASN1Reader(BinaryStream& stream) :
    stream_(stream)
  {}

  ASN1Reader(const ASN1Reader&) = delete;
  ASN1Reader& operator=(const ASN1Reader&) = delete;


  result<bool> is_tag(int tag);

  result<size_t> read_tag(int tag);
  result<size_t> read_len();
  result<std::string> read_alg();
  result<std::string> read_oid();
  result<bool> read_bool();
  result<int32_t> read_int();
  result<int64_t> read_int64();
  result<std::vector<uint8_t>> read_large_int();

  result<std::vector<uint8_t>> read_bitstring();
  result<std::vector<uint8_t>> read_octet_string();
  result<std::string> read_utf8_string();
  result<std::unique_ptr<mbedtls_x509_crt>> read_cert();
  result<std::string> x509_read_names();
  result<std::vector<uint8_t>> x509_read_serial();
  result<std::unique_ptr<mbedtls_x509_time>> x509_read_time();

  std::string get_str_tag();

  static std::string tag2str(int tag);

  private:
  BinaryStream& stream_;
};

}
#endif
