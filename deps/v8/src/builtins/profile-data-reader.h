// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_PROFILE_DATA_READER_H_
#define V8_BUILTINS_PROFILE_DATA_READER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "src/common/globals.h"

namespace v8 {
namespace internal {

class ProfileDataFromFile {
 public:
  // A hash of the function's Graph before scheduling. Allows us to avoid using
  // profiling data if the function has been changed.
  int hash() const { return hash_; }

  // Returns the hint for a pair of blocks with the given IDs.
  BranchHint GetHint(size_t true_block_id, size_t false_block_id) const {
    auto it =
        block_hints_by_id.find(std::make_pair(true_block_id, false_block_id));
    if (it != block_hints_by_id.end()) {
      return it->second ? BranchHint::kTrue : BranchHint::kFalse;
    }
    return BranchHint::kNone;
  }

#ifdef LOG_BUILTIN_BLOCK_COUNT
  uint64_t GetExecutedCount(size_t block_id) const {
    if (executed_count_.count(block_id) == 0) return 0;
    return executed_count_.at(block_id);
  }
#endif

  // Load basic block profiling data for the builtin with the given name, if
  // such data exists. The returned vector is indexed by block ID, and its
  // values are the number of times each block was executed while profiling.
  static const ProfileDataFromFile* TryRead(const char* name);

 protected:
  int hash_ = 0;

  // Branch hints, indicated by true or false to reflect the hinted result of
  // the branch condition. The vector is indexed by the basic block ids of
  // the two destinations of the branch.
  std::map<std::pair<size_t, size_t>, bool> block_hints_by_id;

#ifdef LOG_BUILTIN_BLOCK_COUNT
  std::unordered_map<size_t, uint64_t> executed_count_;
#endif
};

// The following strings can't be static members of ProfileDataFromFile until
// C++ 17; see https://stackoverflow.com/q/8016780/839379 . So for now we use a
// namespace.
namespace ProfileDataFromFileConstants {

// Any line in a v8.log beginning with this string represents a basic block
// counter.
static constexpr char kBlockCounterMarker[] = "block";

// Any line in the profile beginning with this string represents a basic block
// branch hint.
static constexpr char kBlockHintMarker[] = "block_hint";

// Any line in a v8.log beginning with this string represents the hash of the
// function Graph for a builtin.
static constexpr char kBuiltinHashMarker[] = "builtin_hash";

}  // namespace ProfileDataFromFileConstants

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_PROFILE_DATA_READER_H_
