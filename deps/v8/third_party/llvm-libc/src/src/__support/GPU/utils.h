//===---------------- Implementation of GPU utils ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_GPU_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_GPU_UTILS_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

#if !__has_include(<gpuintrin.h>)
#error "Unsupported compiler"
#endif

#include <gpuintrin.h>

namespace LIBC_NAMESPACE_DECL {
namespace gpu {

template <typename T> using Private = __gpu_private T;
template <typename T> using Constant = __gpu_constant T;
template <typename T> using Local = __gpu_local T;
template <typename T> using Global = __gpu_local T;

LIBC_INLINE uint32_t get_num_blocks_x() { return __gpu_num_blocks(0); }

LIBC_INLINE uint32_t get_num_blocks_y() { return __gpu_num_blocks(1); }

LIBC_INLINE uint32_t get_num_blocks_z() { return __gpu_num_blocks(2); }

LIBC_INLINE uint64_t get_num_blocks() {
  return get_num_blocks_x() * get_num_blocks_y() * get_num_blocks_z();
}

LIBC_INLINE uint32_t get_block_id_x() { return __gpu_block_id(0); }

LIBC_INLINE uint32_t get_block_id_y() { return __gpu_block_id(1); }

LIBC_INLINE uint32_t get_block_id_z() { return __gpu_block_id(2); }

LIBC_INLINE uint64_t get_block_id() {
  return get_block_id_x() + get_num_blocks_x() * get_block_id_y() +
         get_num_blocks_x() * get_num_blocks_y() * get_block_id_z();
}

LIBC_INLINE uint32_t get_num_threads_x() { return __gpu_num_threads(0); }

LIBC_INLINE uint32_t get_num_threads_y() { return __gpu_num_threads(1); }

LIBC_INLINE uint32_t get_num_threads_z() { return __gpu_num_threads(2); }

LIBC_INLINE uint64_t get_num_threads() {
  return get_num_threads_x() * get_num_threads_y() * get_num_threads_z();
}

LIBC_INLINE uint32_t get_thread_id_x() { return __gpu_thread_id(0); }

LIBC_INLINE uint32_t get_thread_id_y() { return __gpu_thread_id(1); }

LIBC_INLINE uint32_t get_thread_id_z() { return __gpu_thread_id(2); }

LIBC_INLINE uint64_t get_thread_id() {
  return get_thread_id_x() + get_num_threads_x() * get_thread_id_y() +
         get_num_threads_x() * get_num_threads_y() * get_thread_id_z();
}

LIBC_INLINE uint32_t get_lane_size() { return __gpu_num_lanes(); }

LIBC_INLINE uint32_t get_lane_id() { return __gpu_lane_id(); }

LIBC_INLINE uint64_t get_lane_mask() { return __gpu_lane_mask(); }

LIBC_INLINE uint32_t broadcast_value(uint64_t lane_mask, uint32_t x) {
  return __gpu_read_first_lane_u32(lane_mask, x);
}

LIBC_INLINE uint64_t ballot(uint64_t lane_mask, bool x) {
  return __gpu_ballot(lane_mask, x);
}

LIBC_INLINE void sync_threads() { __gpu_sync_threads(); }

LIBC_INLINE void sync_lane(uint64_t lane_mask) { __gpu_sync_lane(lane_mask); }

LIBC_INLINE uint32_t shuffle(uint64_t lane_mask, uint32_t idx, uint32_t x,
                             uint32_t width = __gpu_num_lanes()) {
  return __gpu_shuffle_idx_u32(lane_mask, idx, x, width);
}

LIBC_INLINE uint64_t shuffle(uint64_t lane_mask, uint32_t idx, uint64_t x,
                             uint32_t width = __gpu_num_lanes()) {
  return __gpu_shuffle_idx_u64(lane_mask, idx, x, width);
}

template <typename T>
LIBC_INLINE T *shuffle(uint64_t lane_mask, uint32_t idx, T *x,
                       uint32_t width = __gpu_num_lanes()) {
  return reinterpret_cast<T *>(__gpu_shuffle_idx_u64(
      lane_mask, idx, reinterpret_cast<uintptr_t>(x), width));
}

LIBC_INLINE uint64_t match_any(uint64_t lane_mask, uint32_t x) {
  return __gpu_match_any_u32(lane_mask, x);
}

LIBC_INLINE uint64_t match_all(uint64_t lane_mask, uint32_t x) {
  return __gpu_match_all_u32(lane_mask, x);
}

[[noreturn]] LIBC_INLINE void end_program() { __gpu_exit(); }

LIBC_INLINE bool is_first_lane(uint64_t lane_mask) {
  return __gpu_is_first_in_lane(lane_mask);
}

LIBC_INLINE uint32_t reduce(uint64_t lane_mask, uint32_t x) {
  return __gpu_lane_add_u32(lane_mask, x);
}

LIBC_INLINE uint32_t scan(uint64_t lane_mask, uint32_t x) {
  return __gpu_prefix_scan_add_u32(lane_mask, x);
}

LIBC_INLINE uint64_t fixed_frequency_clock() {
  return __builtin_readsteadycounter();
}

LIBC_INLINE uint64_t processor_clock() { return __builtin_readcyclecounter(); }

} // namespace gpu
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_GPU_UTILS_H
