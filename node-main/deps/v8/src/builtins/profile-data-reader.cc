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

#ifdef LOG_BUILTIN_BLOCK_COUNT
  void AddBlockExecutionCount(size_t block_id, uint64_t executed_count) {
    executed_count_.emplace(block_id, executed_count);
  }
#endif

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
#ifdef LOG_BUILTIN_BLOCK_COUNT
  if (v8_flags.turbo_log_builtins_count_input) {
    std::ifstream raw_count_file(
        v8_flags.turbo_log_builtins_count_input.value());
    CHECK_WITH_MSG(raw_count_file.good(),
                   "Can't read raw count file for log builtin hotness.");
    for (std::string line; std::getline(raw_count_file, line);) {
      std::string token;
      std::istringstream line_stream(line);
      if (!std::getline(line_stream, token, '\t')) continue;
      if (token == ProfileDataFromFileConstants::kBlockCounterMarker) {
        // Any line starting with kBlockCounterMarker is a basic block execution
        // count. The format is:
        //   literal kBlockCounterMarker \t builtin_name \t block_id \t count
        std::string builtin_name;
        CHECK(std::getline(line_stream, builtin_name, '\t'));
        std::string block_id_str;
        CHECK(std::getline(line_stream, block_id_str, '\t'));
        char* end = nullptr;
        errno = 0;
        uint32_t block_id =
            static_cast<uint32_t>(strtoul(block_id_str.c_str(), &end, 10));
        CHECK(errno == 0);
        std::string executed_count_str;
        CHECK(std::getline(line_stream, executed_count_str, '\t'));
        uint64_t executed_count = static_cast<uint64_t>(
            strtoul(executed_count_str.c_str(), &end, 10));
        CHECK(errno == 0 && end != token.c_str());
        std::getline(line_stream, token, '\t');
        ProfileDataFromFileInternal& block_count = (*data.get())[builtin_name];
        block_count.AddBlockExecutionCount(block_id, executed_count);
        CHECK(line_stream.eof());
      } else if (token == ProfileDataFromFileConstants::kBuiltinHashMarker) {
        // Any line starting with kBuiltinHashMarker is a function hash record.
        // As defined by V8FileLogger::BuiltinHashEvent, the format is:
        //   literal kBuiltinHashMarker \t builtin_name \t hash
        std::string builtin_name;
        CHECK(std::getline(line_stream, builtin_name, '\t'));
        std::getline(line_stream, token, '\t');
        CHECK(line_stream.eof());
        char* end = nullptr;
        int hash = static_cast<int>(strtol(token.c_str(), &end, 0));
        CHECK(errno == 0 && end != token.c_str());
        ProfileDataFromFileInternal& block_count = (*data.get())[builtin_name];
        CHECK_IMPLIES(block_count.hash_has_value(), block_count.hash() == hash);
        block_count.set_hash(hash);
      }
    }
  }
#endif
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
