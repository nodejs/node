// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_DUPLICATE_FINDER_H_
#define V8_PARSING_DUPLICATE_FINDER_H_

#include "src/base/hashmap.h"
#include "src/collector.h"

namespace v8 {
namespace internal {

// DuplicateFinder discovers duplicate symbols.
class DuplicateFinder {
 public:
  DuplicateFinder() : backing_store_(16), map_(&Match) {}

  bool AddOneByteSymbol(Vector<const uint8_t> key);
  bool AddTwoByteSymbol(Vector<const uint16_t> key);

 private:
  bool AddSymbol(Vector<const uint8_t> key, bool is_one_byte);
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

  // Backing store used to store strings used as hashmap keys.
  SequenceCollector<unsigned char> backing_store_;
  base::CustomMatcherHashMap map_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_DUPLICATE_FINDER_H_
