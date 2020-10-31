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

#ifndef INCLUDE_PERFETTO_PROFILING_DEOBFUSCATOR_H_
#define INCLUDE_PERFETTO_PROFILING_DEOBFUSCATOR_H_

#include <map>
#include <string>

namespace perfetto {
namespace profiling {

struct ObfuscatedClass {
  ObfuscatedClass(std::string d) : deobfuscated_name(std::move(d)) {}
  ObfuscatedClass(std::string d, std::map<std::string, std::string> f)
      : deobfuscated_name(std::move(d)), deobfuscated_fields(std::move(f)) {}

  std::string deobfuscated_name;
  std::map<std::string, std::string> deobfuscated_fields;
};

class ProguardParser {
 public:
  // A return value of false means this line failed to parse. This leaves the
  // parser in an undefined state and it should no longer be used.
  bool AddLine(std::string line);

  std::map<std::string, ObfuscatedClass> ConsumeMapping() {
    return std::move(mapping_);
  }

 private:
  std::map<std::string, ObfuscatedClass> mapping_;
  ObfuscatedClass* current_class_ = nullptr;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_PROFILING_DEOBFUSCATOR_H_
