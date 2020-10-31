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

#ifndef SRC_TRACED_PROBES_FTRACE_KALLSYMS_KERNEL_SYMBOL_MAP_H_
#define SRC_TRACED_PROBES_FTRACE_KALLSYMS_KERNEL_SYMBOL_MAP_H_

#include <stdint.h>
#include <array>
#include <forward_list>
#include <vector>

namespace perfetto {

namespace base {
class StringView;
}

// A parser and memory-efficient container for /proc/kallsyms.
// It can store a full kernel symbol table in ~1.2MB of memory and perform fast
// lookups using logarithmic binary searches + bounded linear scans.
//
// /proc/kallsyms is a ~10 MB text file that contains the map of kernel symbols,
// as follows:
// ffffff8f77682f8c t el0_sync_invalid
// ffffff8f77683060 t el0_irq_invalid
// ...
// In a typipcal Android kernel, it consists of 213K lines. Out of these, only
// 116K are interesting for the sake of symbolizing kernel functions, the rest
// are .rodata (variables), weak or other useless symbols.
// Still, even keeping around 116K pointers would require 116K * 8 ~= 1 MB of
// memory, without accounting for any strings for the symbols names.
// The SUM(str.len) for the 116K symbol names adds up to 2.7 MB (without
// counting their addresses).
// However consider the following:
// - Symbol addresses are mostly contiguous. Modulo the initial KASLR loading
//   address, most symbols are few hundreds bytes apart from each other.
// - Symbol names are made of tokens that are quite frequent (token: the result
//   of name.split('_')). If we tokenize the 2.7 MB of strings, the resulting
//   SUM(distinct_token.len) goes down 2.7MB -> 146 KB. This is because tokens
//   like "get", "set" or "event" show up thousands of times.
// - Symbol names are ASCII strings using only 7 out of 8 bits.
//
// In the light of this, the in-memory architecture of this data structure is
// as follows:
// We keep two tables around: (1) a token table and (2) a symbol table. Both
// table are a flat byte vector with some sparse lookaside index to make lookups
// faster and avoid full linear scans.
//
// Token table
// -----------
// The token table is a flat char buffer. Tokens are variable size (>0). Each
// token is identified by its ordinality, so token id 3 is the 3rd token in
// the table. All tokens are concatenated together.
// Given the ASCII encoding, the MSB is used as a terminator. So instead of
// wasting an extra NUL byte for each string, the last char of each token has
// the MSB set.
// Furthermore, a lookaside index stores the offset of tokens (i.e. Token N
// starts at offset O in the buffer) to allow fast lookups. In order to avoid
// wasting too much memory, the index is sparse and track the offsets of only
// one every kTokenIndexSamplinig tokens.
// When looking up a token ID N, the table seeks at the offset of the closest
// token <= N, and then scans linearly the next (at most kTokenIndexSamplinig)
// tokens, counting the MSBs found, until the right token id is found.
// buf:   set*get*kernel*load*fpsimd*return*wrapper*el0*skip*sync*neon*bit*aes
//        ^                   ^                         ^
//        |                   |                         |
// index: 0@0                 4@15                      8@21

// Symbol table
// ------------
// The symbol table is a flat char buffer that stores for each symbol: its
// address + the list of token indexes in the token table. The main caveats are
// that:
// - Symbol addresses are delta encoded (delta from prev symbol's addr).
// - Both delta addresses and token indexes are var-int encoded.
// - The LSB of token indexes is used as EOF marker (i.e. the next varint is
//   the delta-addr for the next symbol). This time the LSB is used because of
//   the varint encoding.
// At parsing time symbols are ordered by address and tokens are sorted by
// frequency, so that the top used 64 tokens can be represented with 1 byte.
// (Rationale for 64: 1 byte = 8 bits. The MSB bit of each byte is used for the
// varint encoding, the LSB bit of each number is used as end-of-tokens marker.
// There are 6 bits left -> 64 indexes can be represented using one byte).
// In summary the symbol table looks as follows:
//
// Base address: 0xbeef0000
// Symbol buffer:
// 0 1|0  4|0  6|1    // 0xbeef0000: 1,4,6 -> get_fpsimd_wrapper
// 8 7|0  3|1         // 0xbeef0008: 7,3   -> el0_load
// ...
// Like in the case of the token table, a lookaside index keeps track of the
// offset of one every kSymIndexSamplinig addresses.
// The Lookup(ADDR) function operates as follows:
// 1. Performs a logarithmic binary search in the symbols index, finding the
//    offset of the closest addres <= ADDR.
// 2. Skip over at most kSymIndexSamplinig until the symbol is found.
// 3. For each token index, lookup the corresponding token string and
//    concatenate them to build the symbol name.

class KernelSymbolMap {
 public:
  // The two constants below are changeable only for the benchmark use.
  // Trades off size of the root |index_| vs worst-case linear scans size.
  // A higher number makes the index more sparse.
  static size_t kSymIndexSampling;

  // Trades off size of the TokenTable |index_| vs worst-case linear scans size.
  static size_t kTokenIndexSampling;

  // Parses a kallsyms file. Returns the number of valid symbols decoded.
  size_t Parse(const std::string& kallsyms_path);

  // Looks up the closest symbol (i.e. the one with the highest address <=
  // |addr|) from its absolute 64-bit address.
  // Returns an empty string if the symbol is not found (which can happen only
  // if the passed |addr| is < min(addr)).
  std::string Lookup(uint64_t addr);

  // Returns the numberr of valid symbols decoded.
  size_t num_syms() const { return num_syms_; }

  // Returns the size in bytes used by the adddress table (without counting
  // the tokens).
  size_t addr_bytes() const { return buf_.size() + index_.size() * 8; }

  // Returns the total memory usage in bytes.
  size_t size_bytes() const { return addr_bytes() + tokens_.size_bytes(); }

  // Token table.
  class TokenTable {
   public:
    using TokenId = uint32_t;
    TokenTable();
    ~TokenTable();
    TokenId Add(const std::string&);
    base::StringView Lookup(TokenId);
    size_t size_bytes() const { return buf_.size() + index_.size() * 4; }

    void shrink_to_fit() {
      buf_.shrink_to_fit();
      index_.shrink_to_fit();
    }

   private:
    TokenId num_tokens_ = 0;

    std::vector<char> buf_;  // Token buffer.

    // The value i-th in the vector contains the offset (within |buf_|) of the
    // (i * kTokenIndexSamplinig)-th token.
    std::vector<uint32_t> index_;
  };

 private:
  TokenTable tokens_;  // Token table.

  uint64_t base_addr_ = 0;    // Address of the first symbol (after sorting).
  size_t num_syms_ = 0;       // Number of valid symbols stored.
  std::vector<uint8_t> buf_;  // Symbol buffer.

  // The key is (address - base_addr_), the value is the byte offset in |buf_|
  // where the symbol entry starts (i.e. the start of the varint that tells the
  // delta from the previous symbol).
  std::vector<std::pair<uint32_t /*rel_addr*/, uint32_t /*offset*/>> index_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_KALLSYMS_KERNEL_SYMBOL_MAP_H_
