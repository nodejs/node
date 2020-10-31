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

#include "src/traced/probes/ftrace/kallsyms/kernel_symbol_map.h"

#include <inttypes.h>

#include <random>
#include <unordered_map>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/temp_file.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

using TokenId = KernelSymbolMap::TokenTable::TokenId;

std::string Lookup(KernelSymbolMap::TokenTable& tokens, TokenId id) {
  base::StringView sv = tokens.Lookup(id);
  std::string str;
  str.reserve(sv.size() + 1);
  for (const char c : sv)
    str.push_back(c & 0x7f);
  return str;
}

TEST(KernelSymbolMapTest, TokenTable) {
  KernelSymbolMap::TokenTable tokens;
  ASSERT_EQ(tokens.Add("a"), 1u);
  ASSERT_EQ(tokens.Add("bb"), 2u);
  ASSERT_EQ(tokens.Add("ccc"), 3u);
  ASSERT_EQ(tokens.Add("foobar"), 4u);
  ASSERT_EQ(tokens.Add("foobaz"), 5u);
  ASSERT_EQ(tokens.Add("_"), 6u);
  ASSERT_EQ(Lookup(tokens, 0), "");
  ASSERT_EQ(Lookup(tokens, 1), "a");
  ASSERT_EQ(Lookup(tokens, 2), "bb");
  ASSERT_EQ(Lookup(tokens, 3), "ccc");
  ASSERT_EQ(Lookup(tokens, 4), "foobar");
  ASSERT_EQ(Lookup(tokens, 5), "foobaz");
  ASSERT_EQ(Lookup(tokens, 6), "_");
  ASSERT_EQ(Lookup(tokens, 0), "");
  ASSERT_EQ(Lookup(tokens, 42), "<error>");
}

TEST(KernelSymbolMapTest, ManyTokens) {
  KernelSymbolMap::TokenTable tokens;
  std::unordered_map<TokenId, std::string> tok_map;
  static std::minstd_rand rng(0);
  for (int rep = 0; rep < 10000; rep++) {
    static const size_t kNameMax = 128;
    std::string sym_name;
    sym_name.reserve(kNameMax);
    size_t len = 1 + (rng() % kNameMax);
    for (size_t j = 0; j < len; j++)
      sym_name.push_back(rng() & 0x7f);
    TokenId id = tokens.Add(sym_name);
    ASSERT_EQ(tok_map.count(id), 0u);
    tok_map[id] = sym_name;
  }
  for (const auto& kv : tok_map) {
    ASSERT_EQ(Lookup(tokens, kv.first), kv.second);
  }
}

TEST(KernelSymbolMapTest, EdgeCases) {
  base::TempFile tmp = base::TempFile::Create();
  static const char kContents[] = R"(ffffff8f73e2fa10 t one
ffffff8f73e2fa20 t two_
ffffff8f73e2fa30 t _three  [module_name_ignored]
ffffff8f73e2fa40 x ignore
ffffff8f73e2fa40 t _fo_ur_
ffffff8f73e2fa50 t _five__.cfi
ffffff8f73e2fa60 t __si__x__
ffffff8f73e2fa70 t ___se___v_e__n___
ffffff8f73e2fa80 t _eight_omg_this_name_is_so_loooooooooooooooooooooooooooooooooooong_should_be_truncated_exactly_here_because_this_is_the_128_char_TRUNCATED_HERE
ffffff8f73e2fa90 t NiNe
)";
  base::WriteAll(tmp.fd(), kContents, sizeof(kContents));
  base::FlushFile(tmp.fd());

  KernelSymbolMap kallsyms;
  EXPECT_EQ(kallsyms.Lookup(0x42), "");

  kallsyms.Parse(tmp.path().c_str());
  EXPECT_EQ(kallsyms.num_syms(), 9u);

  // Test first exact lookups.
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa10ULL), "one");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa20ULL), "two_");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa30LL), "_three");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa40ULL), "_fo_ur_");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa50ULL), "_five__");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa60ULL), "__si__x__");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa70ULL), "___se___v_e__n___");
  EXPECT_EQ(
      kallsyms.Lookup(0xffffff8f73e2fa80ULL),
      "_eight_omg_this_name_is_so_loooooooooooooooooooooooooooooooooooong_"
      "should_be_truncated_exactly_here_because_this_is_the_128_char");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa90ULL), "NiNe");

  // Now check bound searches.
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa00ULL), "");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa11ULL), "one");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa19ULL), "one");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa71ULL), "___se___v_e__n___");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8f73e2fa91ULL), "NiNe");
  EXPECT_EQ(kallsyms.Lookup(0xffffff8fffffffffULL), "NiNe");
}

TEST(KernelSymbolMapTest, GoldenTest) {
  std::string fake_kallsyms;
  fake_kallsyms.reserve(8 * 1024 * 1024);
  static std::minstd_rand rng(0);
  static const size_t kNameMax = 64;
  std::map<uint64_t, std::string> symbols;
  for (int rep = 0; rep < 1000; rep++) {
    uint64_t addr = static_cast<uint64_t>(rng());
    if (symbols.count(addr))
      continue;
    static const char kCharset[] = "_abcdef";
    char sym_name[kNameMax + 1];  // Deliberately not initialized.
    size_t sym_name_len = 1 + (rng() % kNameMax);
    for (size_t i = 0; i < sym_name_len; i++)
      sym_name[i] = kCharset[rng() % strlen(kCharset)];
    sym_name[sym_name_len] = '\0';
    char line[kNameMax + 40];
    sprintf(line, "%" PRIx64 " t %s\n", addr, sym_name);
    symbols[addr] = sym_name;
    fake_kallsyms += line;
  }
  base::TempFile tmp = base::TempFile::Create();
  base::WriteAll(tmp.fd(), fake_kallsyms.data(), fake_kallsyms.size());
  base::FlushFile(tmp.fd());

  KernelSymbolMap kallsyms;
  kallsyms.Parse(tmp.path().c_str());
  ASSERT_EQ(kallsyms.num_syms(), symbols.size());
  for (const auto& kv : symbols) {
    ASSERT_EQ(kallsyms.Lookup(kv.first), kv.second);
  }
}

}  // namespace
}  // namespace perfetto
