/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/profiling/deobfuscator.h"
#include "perfetto/ext/base/string_splitter.h"

#include "perfetto/ext/base/optional.h"

namespace perfetto {
namespace profiling {
namespace {

base::Optional<std::pair<std::string, std::string>> ParseClass(
    std::string line) {
  base::StringSplitter ss(std::move(line), ' ');

  if (!ss.Next()) {
    PERFETTO_ELOG("Missing deobfuscated name.");
    return base::nullopt;
  }
  std::string deobfuscated_name(ss.cur_token(), ss.cur_token_size());

  if (!ss.Next() || ss.cur_token_size() != 2 ||
      strncmp("->", ss.cur_token(), 2) != 0) {
    PERFETTO_ELOG("Missing ->");
    return base::nullopt;
  }

  if (!ss.Next()) {
    PERFETTO_ELOG("Missing obfuscated name.");
    return base::nullopt;
  }
  std::string obfuscated_name(ss.cur_token(), ss.cur_token_size());
  if (obfuscated_name.size() == 0) {
    PERFETTO_ELOG("Empty obfuscated name.");
    return base::nullopt;
  }
  if (obfuscated_name.back() != ':') {
    PERFETTO_ELOG("Expected colon.");
    return base::nullopt;
  }

  obfuscated_name.resize(obfuscated_name.size() - 1);
  if (ss.Next()) {
    PERFETTO_ELOG("Unexpected data.");
    return base::nullopt;
  }
  return std::make_pair(std::move(obfuscated_name),
                        std::move(deobfuscated_name));
}

base::Optional<std::pair<std::string, std::string>> ParseMember(
    std::string line) {
  base::StringSplitter ss(std::move(line), ' ');

  if (!ss.Next()) {
    PERFETTO_ELOG("Missing type name.");
    return base::nullopt;
  }
  std::string type_name(ss.cur_token(), ss.cur_token_size());

  if (!ss.Next()) {
    PERFETTO_ELOG("Missing deobfuscated name.");
    return base::nullopt;
  }
  std::string deobfuscated_name(ss.cur_token(), ss.cur_token_size());

  if (!ss.Next() || ss.cur_token_size() != 2 ||
      strncmp("->", ss.cur_token(), 2) != 0) {
    PERFETTO_ELOG("Missing ->");
    return base::nullopt;
  }

  if (!ss.Next()) {
    PERFETTO_ELOG("Missing obfuscated name.");
    return base::nullopt;
  }
  std::string obfuscated_name(ss.cur_token(), ss.cur_token_size());

  if (ss.Next()) {
    PERFETTO_ELOG("Unexpected data.");
    return base::nullopt;
  }
  return std::make_pair(std::move(obfuscated_name),
                        std::move(deobfuscated_name));
}

}  // namespace

bool ProguardParser::AddLine(std::string line) {
  if (line.length() == 0)
    return true;
  bool is_member = line[0] == ' ';
  if (is_member && !current_class_) {
    PERFETTO_ELOG("Failed to parse proguard map. Saw member before class.");
    return false;
  }
  if (!is_member) {
    std::string obfuscated_name;
    std::string deobfuscated_name;
    auto opt_pair = ParseClass(std::move(line));
    if (!opt_pair)
      return false;
    std::tie(obfuscated_name, deobfuscated_name) = *opt_pair;
    auto p = mapping_.emplace(std::move(obfuscated_name),
                              std::move(deobfuscated_name));
    if (!p.second) {
      PERFETTO_ELOG("Duplicate class.");
      return false;
    }
    current_class_ = &p.first->second;
  } else {
    std::string obfuscated_name;
    std::string deobfuscated_name;
    auto opt_pair = ParseMember(std::move(line));
    if (!opt_pair)
      return false;
    std::tie(obfuscated_name, deobfuscated_name) = *opt_pair;
    // TODO(fmayer): Teach this to properly parse methods.
    if (deobfuscated_name.find("(") != std::string::npos) {
      // Skip functions, as they will trigger the "Duplicate member" below.
      return true;
    }
    auto p = current_class_->deobfuscated_fields.emplace(obfuscated_name,
                                                         deobfuscated_name);
    if (!p.second && p.first->second != deobfuscated_name) {
      PERFETTO_ELOG("Member redefinition: %s.%s",
                    current_class_->deobfuscated_name.c_str(),
                    deobfuscated_name.c_str());
      return false;
    }
  }
  return true;
}

}  // namespace profiling
}  // namespace perfetto
