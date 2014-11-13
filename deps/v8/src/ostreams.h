// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OSTREAMS_H_
#define V8_OSTREAMS_H_

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ostream>  // NOLINT
#include <streambuf>

#include "include/v8config.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

class OFStreamBase : public std::streambuf {
 protected:
  explicit OFStreamBase(FILE* f);
  virtual ~OFStreamBase();

  virtual int_type sync() FINAL;
  virtual int_type overflow(int_type c) FINAL;

 private:
  FILE* const f_;

  DISALLOW_COPY_AND_ASSIGN(OFStreamBase);
};


// An output stream writing to a file.
class OFStream FINAL : private virtual OFStreamBase, public std::ostream {
 public:
  explicit OFStream(FILE* f);
  ~OFStream();

 private:
  DISALLOW_COPY_AND_ASSIGN(OFStream);
};


// Wrappers to disambiguate uint16_t and uc16.
struct AsUC16 {
  explicit AsUC16(uint16_t v) : value(v) {}
  uint16_t value;
};


struct AsReversiblyEscapedUC16 {
  explicit AsReversiblyEscapedUC16(uint16_t v) : value(v) {}
  uint16_t value;
};


// Writes the given character to the output escaping everything outside of
// printable/space ASCII range. Additionally escapes '\' making escaping
// reversible.
std::ostream& operator<<(std::ostream& os, const AsReversiblyEscapedUC16& c);

// Writes the given character to the output escaping everything outside
// of printable ASCII range.
std::ostream& operator<<(std::ostream& os, const AsUC16& c);

}  // namespace internal
}  // namespace v8

#endif  // V8_OSTREAMS_H_
