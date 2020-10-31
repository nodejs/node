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

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {

bool operator==(const ObfuscatedClass& a, const ObfuscatedClass& b);
bool operator==(const ObfuscatedClass& a, const ObfuscatedClass& b) {
  return a.deobfuscated_name == b.deobfuscated_name &&
         a.deobfuscated_fields == b.deobfuscated_fields;
}

namespace {

using ::testing::ElementsAre;

TEST(ProguardParserTest, ReadClass) {
  ProguardParser p;
  ASSERT_TRUE(p.AddLine(
      "android.arch.core.executor.ArchTaskExecutor -> android.arch.a.a.a:"));
  ASSERT_THAT(p.ConsumeMapping(),
              ElementsAre(std::pair<std::string, ObfuscatedClass>(
                  "android.arch.a.a.a",
                  "android.arch.core.executor.ArchTaskExecutor")));
}

TEST(ProguardParserTest, MissingColon) {
  ProguardParser p;
  ASSERT_FALSE(p.AddLine(
      "android.arch.core.executor.ArchTaskExecutor -> android.arch.a.a.a"));
}

TEST(ProguardParserTest, UnexpectedMember) {
  ProguardParser p;
  ASSERT_FALSE(
      p.AddLine("    android.arch.core.executor.TaskExecutor mDelegate -> b"));
}

TEST(ProguardParserTest, Member) {
  ProguardParser p;
  ASSERT_TRUE(p.AddLine(
      "android.arch.core.executor.ArchTaskExecutor -> android.arch.a.a.a:"));
  ASSERT_TRUE(
      p.AddLine("    android.arch.core.executor.TaskExecutor mDelegate -> b"));
  std::map<std::string, std::string> deobfuscated_fields{{"b", "mDelegate"}};
  ASSERT_THAT(
      p.ConsumeMapping(),
      ElementsAre(std::pair<std::string, ObfuscatedClass>(
          "android.arch.a.a.a", {"android.arch.core.executor.ArchTaskExecutor",
                                 std::move(deobfuscated_fields)})));
}

TEST(ProguardParserTest, Method) {
  ProguardParser p;
  ASSERT_TRUE(p.AddLine(
      "android.arch.core.executor.ArchTaskExecutor -> android.arch.a.a.a:"));
  ASSERT_TRUE(p.AddLine("    15:15:boolean isMainThread():116:116 -> b"));
}

TEST(ProguardParserTest, DuplicateClass) {
  ProguardParser p;
  ASSERT_TRUE(p.AddLine(
      "android.arch.core.executor.ArchTaskExecutor -> android.arch.a.a.a:"));
  ASSERT_FALSE(p.AddLine(
      "android.arch.core.executor.ArchTaskExecutor2 -> android.arch.a.a.a:"));
}

TEST(ProguardParserTest, DuplicateField) {
  ProguardParser p;
  ASSERT_TRUE(p.AddLine(
      "android.arch.core.executor.ArchTaskExecutor -> android.arch.a.a.a:"));
  ASSERT_TRUE(
      p.AddLine("    android.arch.core.executor.TaskExecutor mDelegate -> b"));
  ASSERT_FALSE(
      p.AddLine("    android.arch.core.executor.TaskExecutor mDelegate2 -> b"));
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
