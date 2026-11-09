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
// Only does something when built with -fsanitize-coverage=inline-bool-flag:
// transfers the edge flags set since the last call into the coverage bitmap
// and clears them. Must be called before the coverage bitmap is read.
void sanitizer_cov_flush_bool_flags();
// Only does something when built with -fsanitize-coverage=inline-bool-flag:
// clears the edge flags set since the last call without reporting them. Used
// to not attribute edges hit between two executions (e.g. during startup) to
// the next execution.
void sanitizer_cov_discard_bool_flags();
// Only does something when built with -fsanitize-coverage=inline-bool-flag:
// installs signal handlers that flush the edge flags into the coverage bitmap
// when the process crashes and then defer to the previously installed handler.
void sanitizer_cov_install_crash_flush_handlers();
uint32_t sanitizer_cov_count_discovered_edges();
void cov_init_builtins_edges(uint32_t num_edges);
bool cov_has_builtins_edges();
void cov_set_builtin_edge(uint32_t block_index);

#endif  // V8_FUZZILLI_COV_H_
