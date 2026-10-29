// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FUZZILLI_COV_H_
#define V8_FUZZILLI_COV_H_

// This file is defining functions to handle coverage which are needed for
// fuzzilli fuzzer It communicates coverage bitmap with fuzzilli through shared
// memory
// https://clang.llvm.org/docs/SanitizerCoverage.html

#include <cstdint>
#include <vector>

inline void cov_set_edge(uint8_t* edges, uint32_t index) {
  const uint32_t byte_index = index >> 3;
  const uint32_t bit_index = index & 7;
  edges[byte_index] |= static_cast<uint8_t>(1 << bit_index);
}

inline bool cov_is_edge_set(const uint8_t* edges, uint32_t index) {
  const uint32_t byte_index = index >> 3;
  const uint32_t bit_index = index & 7;
  return (edges[byte_index] & (1 << bit_index)) != 0;
}

void fuzzilli_cov_enable();
void sanitizer_cov_reset_edgeguards();
uint32_t sanitizer_cov_count_discovered_edges();
void cov_init_builtins_edges(uint32_t num_edges);
bool cov_has_builtins_edges();
void cov_set_builtin_edge(uint32_t block_index);

#endif  // V8_FUZZILLI_COV_H_
