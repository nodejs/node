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

  void AddCountToBlock(size_t block_id, double count) {
    if (block_counts_by_id_.size() <= block_id) {
      // std::vector initializes new data to zero when resizing.
      block_counts_by_id_.resize(block_id + 1);
    }
    block_counts_by_id_[block_id] += count;
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
  const char* filename = FLAG_turbo_profiling_log_file;
  if (filename == nullptr) return *data.get();
  std::ifstream file(filename);
  CHECK_WITH_MSG(file.good(), "Can't read log file");
  for (std::string line; std::getline(file, line);) {
    std::string token;
    std::istringstream line_stream(line);
    if (!std::getline(line_stream, token, ',')) continue;
    if (token == ProfileDataFromFileConstants::kBlockCounterMarker) {
      // Any line starting with kBlockCounterMarker is a block usage count.
      // As defined by Logger::BasicBlockCounterEvent, the format is:
      //   literal kBlockCounterMarker , builtin_name , block_id , usage_count
      std::string builtin_name;
      CHECK(std::getline(line_stream, builtin_name, ','));
      CHECK(std::getline(line_stream, token, ','));
      char* end = nullptr;
      uint32_t id = static_cast<uint32_t>(strtoul(token.c_str(), &end, 0));
      CHECK(errno == 0 && end != token.c_str());
      std::getline(line_stream, token, ',');
      CHECK(line_stream.eof());
      double count = strtod(token.c_str(), &end);
      CHECK(errno == 0 && end != token.c_str());
      ProfileDataFromFileInternal& counters_and_hash =
          (*data.get())[builtin_name];
      // We allow concatenating data from several Isolates, so we might see the
      // same block multiple times. Just sum them all.
      counters_and_hash.AddCountToBlock(id, count);
    } else if (token == ProfileDataFromFileConstants::kBuiltinHashMarker) {
      // Any line starting with kBuiltinHashMarker is a function hash record.
      // As defined by Logger::BuiltinHashEvent, the format is:
      //   literal kBuiltinHashMarker , builtin_name , hash
      std::string builtin_name;
      CHECK(std::getline(line_stream, builtin_name, ','));
      std::getline(line_stream, token, ',');
      CHECK(line_stream.eof());
      char* end = nullptr;
      int hash = static_cast<int>(strtol(token.c_str(), &end, 0));
      CHECK(errno == 0 && end != token.c_str());
      ProfileDataFromFileInternal& counters_and_hash =
          (*data.get())[builtin_name];
      // We allow concatenating data from several Isolates, but expect them all
      // to be running the same build. Any file with mismatched hashes for a
      // function is considered ill-formed.
      CHECK_IMPLIES(counters_and_hash.hash_has_value(),
                    counters_and_hash.hash() == hash);
      counters_and_hash.set_hash(hash);
    }
  }
  for (const auto& pair : *data.get()) {
    // Every function is required to have a hash in the log.
    CHECK(pair.second.hash_has_value());
  }
  if (data.get()->size() == 0) {
    PrintF(
        "No basic block counters were found in log file.\n"
        "Did you build with v8_enable_builtins_profiling=true\n"
        "and run with --turbo-profiling-log-builtins?\n");
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
