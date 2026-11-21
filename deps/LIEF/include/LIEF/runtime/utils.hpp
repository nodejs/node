/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_RUNTIME_UTILS_H
#define LIEF_RUNTIME_UTILS_H
#include <cstdint>
#include "LIEF/config.h"

#if defined(LIEF_RUNTIME_SUPPORT)
  #include "LIEF/runtime/config.h"
#endif

namespace LIEF {
namespace runtime {

enum class PLATFORMS : uint32_t {
  NONE = 0,
  LINUX,
  WINDOWS,
  ANDROID_,
  OSX,
  IOS,
};

enum class ARCH : uint32_t {
  NONE,
  X86_64,
  ARM64,
  RISCV64,
};

/// Whether the runtime features are enabled
static constexpr bool is_enabled() {
  return lief_runtime_support;
}

/// Platform for which the runtime is compiled
static constexpr PLATFORMS platform() {
#if !defined(LIEF_RUNTIME_SUPPORT)
  return PLATFORMS::NONE;
#else
  #if defined(LIEF_RUNTIME_PLATFORM_LINUX)
  return PLATFORMS::LINUX;
  #elif defined(LIEF_RUNTIME_PLATFORM_WINDOWS)
  return PLATFORMS::WINDOWS;
  #elif defined(LIEF_RUNTIME_PLATFORM_ANDROID)
  return PLATFORMS::ANDROID_;
  #elif defined(LIEF_RUNTIME_PLATFORM_OSX)
  return PLATFORMS::OSX;
  #elif defined(LIEF_RUNTIME_PLATFORM_IOS)
  return PLATFORMS::IOS;
  #else
  return PLATFORMS::NONE;
  #endif
#endif
}

/// Architecture for which the runtime is compiled
static constexpr ARCH arch() {
#if !defined(LIEF_RUNTIME_SUPPORT)
  return ARCH::NONE;
#else
  #if defined(LIEF_RUNTIME_ARCH_ARM64)
  return ARCH::ARM64;
  #elif defined(LIEF_RUNTIME_ARCH_X86_64)
  return ARCH::X86_64;
  #elif defined(LIEF_RUNTIME_ARCH_RISCV64)
  return ARCH::RISCV64;
  #else
  return ARCH::NONE;
  #endif

#endif
}

static inline constexpr uintptr_t page_mask(uintptr_t page_size) {
  return ~(page_size - 1);
}

static inline constexpr uintptr_t page_start(uintptr_t address,
                                             uintptr_t page_size) {
  return address & page_mask(page_size);
}

static inline constexpr uintptr_t page_align(uintptr_t address,
                                             uintptr_t page_size) {
  return (address + (page_size - 1)) & page_mask(page_size);
}

static inline constexpr uintptr_t page_offset(uintptr_t address,
                                              uintptr_t page_size) {
  return (address & ~page_mask(page_size));
}

static inline constexpr uintptr_t page_end(uintptr_t address,
                                           uintptr_t page_size) {
  return page_start(address + (page_size - 1), page_size);
}

}
}
#endif
