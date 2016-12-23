// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_DUPLICATE_FINDER_H_
#define V8_PARSING_DUPLICATE_FINDER_H_

#include "src/base/hashmap.h"
#include "src/collector.h"

namespace v8 {
namespace internal {

class UnicodeCache;

// DuplicateFinder discovers duplicate symbols.
class DuplicateFinder {
 public:
  explicit DuplicateFinder(UnicodeCache* constants)
      : unicode_constants_(constants), backing_store_(16), map_(&Match) {}

  int AddOneByteSymbol(Vector<const uint8_t> key, int value);
  int AddTwoByteSymbol(Vector<const uint16_t> key, int value);
  // Add a a number literal by converting it (if necessary)
  // to the string that ToString(ToNumber(literal)) would generate.
  // and then adding that string with AddOneByteSymbol.
  // This string is the actual value used as key in an object literal,
  // and the one that must be different from the other keys.
  int AddNumber(Vector<const uint8_t> key, int value);

 private:
  int AddSymbol(Vector<const uint8_t> key, bool is_one_byte, int value);
  // Backs up the key and its length in the backing store.
  // The backup is stored with a base 127 encoding of the
  // length (plus a bit saying whether the string is one byte),
  // followed by the bytes of the key.
  uint8_t* BackupKey(Vector<const uint8_t> key, bool is_one_byte);

  // Compare two encoded keys (both pointing into the backing store)
  // for having the same base-127 encoded lengths and representation.
  // and then having the same 'length' bytes following.
  static bool Match(void* first, void* second);
  // Creates a hash from a sequence of bytes.
  static uint32_t Hash(Vector<const uint8_t> key, bool is_one_byte);
  // Checks whether a string containing a JS number is its canonical
  // form.
  static bool IsNumberCanonical(Vector<const uint8_t> key);

  // Size of buffer. Sufficient for using it to call DoubleToCString in
  // from conversions.h.
  static const int kBufferSize = 100;

  UnicodeCache* unicode_constants_;
  // Backing store used to store strings used as hashmap keys.
  SequenceCollector<unsigned char> backing_store_;
  base::CustomMatcherHashMap map_;
  // Buffer used for string->number->canonical string conversions.
  char number_buffer_[kBufferSize];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_DUPLICATE_FINDER_H_
