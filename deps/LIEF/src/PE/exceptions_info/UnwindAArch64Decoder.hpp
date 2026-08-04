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
#ifndef LIEF_PE_RUNTIME_FUNCTION_AARCH64_UNWIND_DECODED_H
#define LIEF_PE_RUNTIME_FUNCTION_AARCH64_UNWIND_DECODED_H
#include <cassert>
#include <spdlog/fmt/fmt.h>
#include "LIEF/errors.hpp"
#include "LIEF/PE/exceptions_info/UnwindCodeAArch64.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include <ostream>

namespace LIEF {
class BinaryStream;
}

namespace LIEF::PE::unwind_aarch64 {


/// This class is a helper to decode the content of the AArch64 unwind code.
///
/// It is partially based on the LLVM implementation (especially the `11100111`
/// -- save_any_reg operation) by Eli Friedman and
class Decoder {
  public:
  struct outstream_opt_t {
    bool newline = true;
  };
  Decoder() = delete;
  Decoder(BinaryStream& stream) :
    ostream_(nullptr),
    stream_(&stream)
  {}

  Decoder(BinaryStream& stream, std::ostream& os) :
    ostream_(&os),
    stream_(&stream)
  {}

  Decoder(BinaryStream& stream, std::ostream& os, const outstream_opt_t& opt) :
    ostream_(&os),
    stream_(&stream),
    opt_(opt)
  {}

  ok_error_t run(bool prologue);

  template<OPCODES>
  bool decode(bool is_prologue);

  Decoder& log(const std::string& out, bool flush = true) {
    if (ostream_ == nullptr) {
      return *this;
    }

    (*ostream_) << out;

    if (flush) {
      if (opt_.newline) {
        (*ostream_) << '\n';
      }
      (*ostream_) << std::flush;
    }
    return *this;
  }

  template <typename... Args>
  Decoder& log(const char *fmt, const Args &... args) {
    return log(fmt::format(fmt::runtime(fmt), args...));
  }

  template <typename... Args>
  Decoder& lognf(const char *fmt, const Args &... args) {
    return log(fmt::format(fmt::runtime(fmt), args...), /*flush=*/false);
  }

  template<class T>
  T read() {
    return stream_->read<T>().value_or(0);
  }

  uint8_t read_u8() {
    return read<uint8_t>();
  }

  Decoder& rewind(size_t count) {
    assert(((int64_t)stream_->pos() - (int64_t)count) >= 0);
    stream_->decrement_pos(count);
    return *this;
  }

  private:
  std::ostream* ostream_ = nullptr;
  BinaryStream* stream_ = nullptr;
  outstream_opt_t opt_;
};
}

#endif


