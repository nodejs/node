// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_PROFILE_DATA_READER_H_
#define V8_BUILTINS_PROFILE_DATA_READER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace v8 {
namespace internal {

class ProfileDataFromFile {
 public:
  // A hash of the function's Graph before scheduling. Allows us to avoid using
  // profiling data if the function has been changed.
  int hash() const { return hash_; }

  // Returns how many times the block with the given ID was executed during
  // profiling.
  double GetCounter(size_t block_id) const {
    // The profile data is allowed to omit blocks which were never hit, so be
    // careful to avoid out-of-bounds access.
    return block_id < block_counts_by_id_.size() ? block_counts_by_id_[block_id]
                                                 : 0;
  }

  // Load basic block profiling data for the builtin with the given name, if
  // such data exists. The returned vector is indexed by block ID, and its
  // values are the number of times each block was executed while profiling.
  static const ProfileDataFromFile* TryRead(const char* name);

 protected:
  int hash_ = 0;

  // How many times each block was executed, indexed by block ID. This vector
  // may be shorter than the total number of blocks; any omitted block should be
  // treated as a zero.
  std::vector<double> block_counts_by_id_;
};

// The following strings can't be static members of ProfileDataFromFile until
// C++ 17; see https://stackoverflow.com/q/8016780/839379 . So for now we use a
// namespace.
namespace ProfileDataFromFileConstants {

// Any line in a v8.log beginning with this string represents a basic block
// counter.
static constexpr char kBlockCounterMarker[] = "block";

// Any line in a v8.log beginning with this string represents the hash of the
// function Graph for a builtin.
static constexpr char kBuiltinHashMarker[] = "builtin_hash";

}  // namespace ProfileDataFromFileConstants

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_PROFILE_DATA_READER_H_
