// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/profile-data-reader.h"

#include <fstream>
#include <iostream>
#include <unordered_map>

#include "src/base/lazy-instance.h"
#include "src/flags/flags.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

namespace {

class ProfileDataFromFileInternal : public ProfileDataFromFile {
 public:
  bool hash_has_value() const { return hash_has_value_; }

  void set_hash(int hash) {
    hash_ = hash;
    hash_has_value_ = true;
  }

  void AddHintToBlock(size_t true_block_id, size_t false_block_id,
                      uint64_t hint) {
    CHECK_LT(hint, 2);
    block_hints_by_id.insert(std::make_pair(
        std::make_pair(true_block_id, false_block_id), hint != 0));
  }

 private:
  bool hash_has_value_ = false;
};

const std::unordered_map<std::string, ProfileDataFromFileInternal>&
EnsureInitProfileData() {
  static base::LeakyObject<
      std::unordered_map<std::string, ProfileDataFromFileInternal>>
      data;
  static bool initialized = false;

  if (initialized) return *data.get();
  initialized = true;
  const char* filename = v8_flags.turbo_profiling_input;
  if (filename == nullptr) return *data.get();
  std::ifstream file(filename);
  CHECK_WITH_MSG(file.good(), "Can't read log file");
  for (std::string line; std::getline(file, line);) {
    std::string token;
    std::istringstream line_stream(line);
    if (!std::getline(line_stream, token, ',')) continue;
    if (token == ProfileDataFromFileConstants::kBlockHintMarker) {
      // Any line starting with kBlockHintMarker is a basic block branch hint.
      // The format is:
      //   literal kBlockHintMarker , builtin_name , true_id , false_id , hint
      std::string builtin_name;
      CHECK(std::getline(line_stream, builtin_name, ','));
      CHECK(std::getline(line_stream, token, ','));
      char* end = nullptr;
      errno = 0;
      uint32_t true_id = static_cast<uint32_t>(strtoul(token.c_str(), &end, 0));
      CHECK(errno == 0 && end != token.c_str());
      CHECK(std::getline(line_stream, token, ','));
      uint32_t false_id =
          static_cast<uint32_t>(strtoul(token.c_str(), &end, 0));
      CHECK(errno == 0 && end != token.c_str());
      std::getline(line_stream, token, ',');
      CHECK(line_stream.eof());
      uint64_t hint = strtoul(token.c_str(), &end, 10);
      CHECK(errno == 0 && end != token.c_str());
      ProfileDataFromFileInternal& hints_and_hash = (*data.get())[builtin_name];
      // Only the first hint for each branch will be used.
      hints_and_hash.AddHintToBlock(true_id, false_id, hint);
      CHECK(line_stream.eof());
    } else if (token == ProfileDataFromFileConstants::kBuiltinHashMarker) {
      // Any line starting with kBuiltinHashMarker is a function hash record.
      // As defined by V8FileLogger::BuiltinHashEvent, the format is:
      //   literal kBuiltinHashMarker , builtin_name , hash
      std::string builtin_name;
      CHECK(std::getline(line_stream, builtin_name, ','));
      std::getline(line_stream, token, ',');
      CHECK(line_stream.eof());
      char* end = nullptr;
      int hash = static_cast<int>(strtol(token.c_str(), &end, 0));
      CHECK(errno == 0 && end != token.c_str());
      ProfileDataFromFileInternal& hints_and_hash = (*data.get())[builtin_name];
      // We allow concatenating data from several Isolates, but expect them all
      // to be running the same build. Any file with mismatched hashes for a
      // function is considered ill-formed.
      CHECK_IMPLIES(hints_and_hash.hash_has_value(),
                    hints_and_hash.hash() == hash);
      hints_and_hash.set_hash(hash);
    }
  }
  for (const auto& pair : *data.get()) {
    // Every function is required to have a hash in the log.
    CHECK(pair.second.hash_has_value());
  }

  return *data.get();
}

}  // namespace

const ProfileDataFromFile* ProfileDataFromFile::TryRead(const char* name) {
  const auto& data = EnsureInitProfileData();
  auto it = data.find(name);
  return it == data.end() ? nullptr : &it->second;
}

}  // namespace internal
}  // namespace v8
