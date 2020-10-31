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

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/metatrace.h"
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/proto_utils.h"

#include <inttypes.h>
#include <stdio.h>

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

namespace perfetto {

// On a Pixel 3 this gives an avg. lookup time of 600 ns and a memory usage
// of 1.1 MB for 65k symbols. See go/kallsyms-parser-bench.
size_t KernelSymbolMap::kSymIndexSampling = 16;
size_t KernelSymbolMap::kTokenIndexSampling = 4;

namespace {

using TokenId = KernelSymbolMap::TokenTable::TokenId;
constexpr size_t kSymNameMaxLen = 128;

// Reads a kallsyms file in blocks of 4 pages each and decode its lines using
// a simple FSM. Calls the passed lambda for each valid symbol.
// It skips undefined symbols and other useless stuff.
template <typename Lambda /* void(uint64_t, const char*) */>
void ForEachSym(const std::string& kallsyms_path, Lambda fn) {
  base::ScopedFile fd = base::OpenFile(kallsyms_path.c_str(), O_RDONLY);
  if (!fd) {
    PERFETTO_PLOG("Cannot open %s", kallsyms_path.c_str());
    return;
  }

  // /proc/kallsyms looks as follows:
  // 0000000000026a80 A bpf_trace_sds
  //
  // ffffffffc03a6000 T cpufreq_gov_powersave_init<TAB> [cpufreq_powersave]
  // ffffffffc035d000 T cpufreq_gov_userspace_init<TAB> [cpufreq_userspace]
  //
  // We parse it with a state machine that has four states, one for each column.
  // We don't care about the part in the square brackets and ignore everything
  // after the symbol name.

  static constexpr size_t kBufSize = 16 * 1024;
  base::PagedMemory buffer = base::PagedMemory::Allocate(kBufSize);
  enum { kSymAddr, kSymType, kSymName, kEatRestOfLine } state = kSymAddr;
  uint64_t sym_addr = 0;
  char sym_type = '\0';
  char sym_name[kSymNameMaxLen + 1];
  size_t sym_name_len = 0;
  for (;;) {
    char* buf = static_cast<char*>(buffer.Get());
    auto rsize = PERFETTO_EINTR(read(*fd, buf, kBufSize));
    if (rsize < 0) {
      PERFETTO_PLOG("read(%s) failed", kallsyms_path.c_str());
      return;
    }
    if (rsize == 0)
      return;  // EOF
    for (size_t i = 0; i < static_cast<size_t>(rsize); i++) {
      char c = buf[i];
      const bool is_space = c == ' ' || c == '\t';
      switch (state) {
        case kSymAddr:
          if (c >= '0' && c <= '9') {
            sym_addr = (sym_addr << 4) | static_cast<uint8_t>(c - '0');
          } else if (c >= 'a' && c <= 'f') {
            sym_addr = (sym_addr << 4) | static_cast<uint8_t>(c - 'a' + 10);
          } else if (is_space) {
            state = kSymType;
          } else if (c == '\0') {
            return;
          } else {
            PERFETTO_ELOG("kallsyms parser error: chr 0x%x @ off=%zu", c, i);
            return;
          }
          break;

        case kSymType:
          if (is_space)
            break;  // Eat leading spaces.
          sym_type = c;
          state = kSymName;
          sym_name_len = 0;
          break;

        case kSymName:
          if (is_space && sym_name_len == 0)
            break;  // Eat leading spaces.
          if (c && c != '\n' && !is_space && sym_name_len < kSymNameMaxLen) {
            sym_name[sym_name_len++] = c;
            break;
          }
          sym_name[sym_name_len] = '\0';
          fn(sym_addr, sym_type, sym_name);
          sym_addr = 0;
          sym_type = '\0';
          state = c == '\n' ? kSymAddr : kEatRestOfLine;
          break;

        case kEatRestOfLine:
          if (c == '\n')
            state = kSymAddr;
          break;
      }  // switch(state)
    }    // for (char in buf)
  }      // for (read chunk)
}

// Splits a symbol name into tokens using '_' as a separator, calling the passed
// lambda for each token. It splits tokens in a way that allows the original
// string to be rebuilt as-is by re-joining using a '_' between each token.
// For instance:
// _fo_a_b      ->  ["", fo, a, b]
// __fo_a_b     ->  [_, fo, a, b]
// __fo_a_b_    ->  [_, fo, a, b, ""]
// __fo_a_b____ ->  [_, fo, a, b, ___]
template <typename Lambda /* void(base::StringView) */>
void Tokenize(const char* name, Lambda fn) {
  const char* tok_start = name;
  bool is_start_of_token = true;
  bool tok_is_sep = false;
  for (const char* ptr = name;; ptr++) {
    const char c = *ptr;
    if (is_start_of_token) {
      tok_is_sep = *tok_start == '_';  // Deals with tokens made of '_'s.
      is_start_of_token = false;
    }
    // Scan until either the end of string or the next character (which is a '_'
    // in nominal cases, or anything != '_' for tokens made by 1+ '_').
    if (c == '\0' || (!tok_is_sep && c == '_') || (tok_is_sep && c != '_')) {
      size_t tok_len = static_cast<size_t>(ptr - tok_start);
      if (tok_is_sep && c != '\0')
        --tok_len;
      fn(base::StringView(tok_start, tok_len));
      if (c == '\0')
        return;
      tok_start = tok_is_sep ? ptr : ptr + 1;
      is_start_of_token = true;
    }
  }
}

}  // namespace

KernelSymbolMap::TokenTable::TokenTable() {
  // Insert a null token as id 0. We can't just add "" because the empty string
  // is special-cased and doesn't insert an actual token. So we push a string of
  // size one that contains only the null character instead.
  char null_tok = 0;
  Add(std::string(&null_tok, 1));
}

KernelSymbolMap::TokenTable::~TokenTable() = default;

// Adds a new token to the db. Does not dedupe identical token (with the
// exception of the empty string). The caller has to deal with that.
// Supports only ASCII characters in the range [1, 127].
// The last character of the token will have the MSB set.
TokenId KernelSymbolMap::TokenTable::Add(const std::string& token) {
  const size_t token_size = token.size();
  if (token_size == 0)
    return 0;
  TokenId id = num_tokens_++;

  const size_t buf_size_before_insertion = buf_.size();
  if (id % kTokenIndexSampling == 0)
    index_.emplace_back(buf_size_before_insertion);

  const size_t prev_size = buf_.size();
  buf_.resize(prev_size + token_size);
  char* tok_wptr = &buf_[prev_size];
  for (size_t i = 0; i < token_size - 1; i++) {
    PERFETTO_DCHECK((token.at(i) & 0x80) == 0);  // |token| must be ASCII only.
    *(tok_wptr++) = token.at(i) & 0x7f;
  }
  *(tok_wptr++) = token.at(token_size - 1) | 0x80;
  PERFETTO_DCHECK(tok_wptr == &buf_[buf_.size()]);
  return id;
}

// NOTE: the caller need to mask the returned chars with 0x7f. The last char of
// the StringView will have its MSB set (it's used as a EOF char internally).
base::StringView KernelSymbolMap::TokenTable::Lookup(TokenId id) {
  if (id == 0)
    return base::StringView();
  if (id > num_tokens_)
    return base::StringView("<error>");
  // We don't know precisely where the id-th token starts in the buffer. We
  // store only one position every kTokenIndexSampling. From there, the token
  // can be found with a linear scan of at most kTokenIndexSampling steps.
  size_t index_off = id / kTokenIndexSampling;
  PERFETTO_DCHECK(index_off < index_.size());
  TokenId cur_id = static_cast<TokenId>(index_off * kTokenIndexSampling);
  uint32_t begin = index_[index_off];
  PERFETTO_DCHECK(begin == 0 || buf_[begin - 1] & 0x80);
  const size_t buf_size = buf_.size();
  for (uint32_t off = begin; off < buf_size; ++off) {
    // Advance |off| until the end of the token (which has the MSB set).
    if ((buf_[off] & 0x80) == 0)
      continue;
    if (cur_id == id)
      return base::StringView(&buf_[begin], off - begin + 1);
    ++cur_id;
    begin = off + 1;
  }
  return base::StringView();
}

size_t KernelSymbolMap::Parse(const std::string& kallsyms_path) {
  PERFETTO_METATRACE_SCOPED(TAG_FTRACE, KALLSYMS_PARSE);
  using SymAddr = uint64_t;

  struct TokenInfo {
    uint32_t count = 0;
    TokenId id = 0;
  };

  // Note if changing the container: the code below relies on stable iterators.
  using TokenMap = std::map<std::string, TokenInfo>;
  using TokenMapPtr = TokenMap::value_type*;
  TokenMap tokens;

  // Keep the (ordered) list of tokens for each symbol.
  std::multimap<SymAddr, TokenMapPtr> symbols;

  ForEachSym(kallsyms_path, [&](SymAddr addr, char type, const char* name) {
    if (addr == 0 || (type != 't' && type != 'T') || name[0] == '$') {
      return;
    }

    // Split each symbol name in tokens, using '_' as a separator (so that
    // "foo_bar" -> ["foo", "bar"]). For each token hash:
    // 1. Keep track of the frequency of each token.
    // 2. Keep track of the list of token hashes for each symbol.
    Tokenize(name, [&tokens, &symbols, addr](base::StringView token) {
      // Strip the .cfi part if present.
      if (token.substr(token.size() - 4) == ".cfi")
        token = token.substr(0, token.size() - 4);
      auto it_and_ins = tokens.emplace(token.ToStdString(), TokenInfo{});
      it_and_ins.first->second.count++;
      symbols.emplace(addr, &*it_and_ins.first);
    });
  });

  // At this point we have broken down each symbol into a set of token hashes.
  // Now generate the token ids, putting high freq tokens first, so they use
  // only one byte to varint encode.

  // This block limits the lifetime of |tokens_by_freq|.
  {
    std::vector<TokenMapPtr> tokens_by_freq;
    tokens_by_freq.resize(tokens.size());
    size_t tok_idx = 0;
    for (auto& kv : tokens)
      tokens_by_freq[tok_idx++] = &kv;

    auto comparer = [](TokenMapPtr a, TokenMapPtr b) {
      PERFETTO_DCHECK(a && b);
      return b->second.count < a->second.count;
    };
    std::sort(tokens_by_freq.begin(), tokens_by_freq.end(), comparer);
    for (TokenMapPtr tinfo : tokens_by_freq) {
      tinfo->second.id = tokens_.Add(tinfo->first);
    }
  }
  tokens_.shrink_to_fit();

  buf_.resize(2 * 1024 * 1024);  // Based on real-word observations.
  base_addr_ = symbols.empty() ? 0 : symbols.begin()->first;
  SymAddr prev_sym_addr = base_addr_;
  uint8_t* wptr = buf_.data();

  for (auto it = symbols.begin(); it != symbols.end();) {
    const SymAddr sym_addr = it->first;

    // Find the iterator to the first token of the next symbol (or the end).
    auto sym_start = it;
    auto sym_end = it;
    while (sym_end != symbols.end() && sym_end->first == sym_addr)
      ++sym_end;

    // The range [sym_start, sym_end) has all the tokens for the current symbol.
    uint32_t size_before = static_cast<uint32_t>(wptr - buf_.data());

    // Make sure there is enough headroom to write the symbol.
    if (buf_.size() - size_before < 1024) {
      buf_.resize(buf_.size() + 32768);
      wptr = buf_.data() + size_before;
    }

    uint32_t sym_rel_addr = static_cast<uint32_t>(sym_addr - base_addr_);
    const size_t sym_num = num_syms_++;
    if (sym_num % kSymIndexSampling == 0)
      index_.emplace_back(std::make_pair(sym_rel_addr, size_before));
    PERFETTO_DCHECK(sym_addr >= prev_sym_addr);
    uint32_t delta = static_cast<uint32_t>(sym_addr - prev_sym_addr);
    wptr = protozero::proto_utils::WriteVarInt(delta, wptr);
    // Append all the token ids.
    for (it = sym_start; it != sym_end;) {
      PERFETTO_DCHECK(it->first == sym_addr);
      TokenId token_id = it->second->second.id << 1;
      ++it;
      token_id |= (it == sym_end) ? 1 : 0;  // Last one has LSB set to 1.
      wptr = protozero::proto_utils::WriteVarInt(token_id, wptr);
    }
    prev_sym_addr = sym_addr;
  }  // for (symbols)

  buf_.resize(static_cast<size_t>(wptr - buf_.data()));
  buf_.shrink_to_fit();

  PERFETTO_DLOG(
      "Loaded %zu kalllsyms entries. Mem usage: %zu B (addresses) + %zu B "
      "(tokens), total: %zu B",
      num_syms_, addr_bytes(), tokens_.size_bytes(), size_bytes());

  return num_syms_;
}

std::string KernelSymbolMap::Lookup(uint64_t sym_addr) {
  if (index_.empty() || sym_addr < base_addr_)
    return "";

  // First find the highest symbol address <= sym_addr.
  // Start with a binary search using the sparse index.

  const uint32_t sym_rel_addr = static_cast<uint32_t>(sym_addr - base_addr_);
  auto it = std::upper_bound(index_.cbegin(), index_.cend(),
                             std::make_pair(sym_rel_addr, 0u));
  if (it != index_.cbegin())
    --it;

  // Then continue with a linear scan (of at most kSymIndexSampling steps).
  uint32_t addr = it->first;
  uint32_t off = it->second;
  const uint8_t* rdptr = &buf_[off];
  const uint8_t* const buf_end = &buf_[buf_.size()];
  bool parsing_addr = true;
  const uint8_t* next_rdptr = nullptr;
  for (bool is_first_addr = true;; is_first_addr = false) {
    uint64_t v = 0;
    const auto* prev_rdptr = rdptr;
    rdptr = protozero::proto_utils::ParseVarInt(rdptr, buf_end, &v);
    if (rdptr == prev_rdptr)
      break;
    if (parsing_addr) {
      addr += is_first_addr ? 0 : static_cast<uint32_t>(v);
      parsing_addr = false;
      if (addr > sym_rel_addr)
        break;
      next_rdptr = rdptr;
    } else {
      // This is a token. Wait for the EOF maker.
      parsing_addr = (v & 1) == 1;
    }
  }

  if (!next_rdptr)
    return "";

  // The address has been found. Now rejoin the tokens to form the symbol name.

  rdptr = next_rdptr;
  std::string sym_name;
  sym_name.reserve(kSymNameMaxLen);
  for (bool eof = false, is_first_token = true; !eof; is_first_token = false) {
    uint64_t v = 0;
    const auto* old = rdptr;
    rdptr = protozero::proto_utils::ParseVarInt(rdptr, buf_end, &v);
    if (rdptr == old)
      break;
    eof = v & 1;
    base::StringView token = tokens_.Lookup(static_cast<TokenId>(v >> 1));
    if (!is_first_token)
      sym_name.push_back('_');
    for (size_t i = 0; i < token.size(); i++)
      sym_name.push_back(token.at(i) & 0x7f);
  }
  return sym_name;
}

}  // namespace perfetto
