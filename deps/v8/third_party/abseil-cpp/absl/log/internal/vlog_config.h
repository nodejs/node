// Copyright 2022 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// vlog_config.h
// -----------------------------------------------------------------------------
//
// This header file defines `VLogSite`, a public primitive that represents
// the `--vmodule` state of a source file for the `VLOG` family of macros and
// related libraries.
// It also declares and defines multiple internal utilities used to implement
// `VLOG`, such as `VLogSiteManager`.

#ifndef ABSL_LOG_INTERNAL_VLOG_CONFIG_H_
#define ABSL_LOG_INTERNAL_VLOG_CONFIG_H_

// IWYU pragma: private, include "absl/log/log.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {

class SyntheticBinary;
class VLogSite;

int RegisterAndInitialize(VLogSite* absl_nonnull v);
void UpdateVLogSites();

// Represents the `--vmodule` state for a source file.
//
// When the file name is known at the callsite (`__FILE__`), prefer
// `VLOG_IS_ON(level)` so all callsites in the same file share a single
// `VLogSite`. Construct `VLogSite` directly only when the file name is
// determined at runtime (e.g. from `absl::SourceLocation` or language
// bindings).
//
// `VLogSite` objects must not be destroyed until the program exits; doing so
// will corrupt the global `VLogSite` list in `UpdateVLogSites()`.
class VLogSite final {
 public:
  // `f` must not be destroyed until the program exits.
  explicit constexpr VLogSite(const char* absl_nonnull f)
      : v_(kUninitialized), file_(f), next_(nullptr) {}
  VLogSite(const VLogSite&) = delete;
  VLogSite& operator=(const VLogSite&) = delete;

  // Inlining the function yields a ~3x performance improvement at the cost of a
  // 1.5x code size increase at the call site.
  // Takes locks but does not allocate memory.
  ABSL_ATTRIBUTE_ALWAYS_INLINE
  bool IsEnabled(int level) {
    int stale_v = v_.load(std::memory_order_relaxed);
    if (ABSL_PREDICT_TRUE(level > stale_v)) {
      return false;
    }

    // We put everything other than the fast path, i.e. vlogging is initialized
    // but not on, behind an out-of-line function to reduce code size.
    // "level" is almost always a call-site constant, so we can save a bit
    // of code space by special-casing for a few common levels.
#if ABSL_HAVE_BUILTIN(__builtin_constant_p) || defined(__GNUC__)
    if (__builtin_constant_p(level)) {
      if (level == 0) return SlowIsEnabled0();
      if (level == 1) return SlowIsEnabled1();
      if (level == 2) return SlowIsEnabled2();
      if (level == 3) return SlowIsEnabled3();
      if (level == 4) return SlowIsEnabled4();
      if (level == 5) return SlowIsEnabled5();
    }
#endif
    return SlowIsEnabled(level);
  }

 private:
  friend int log_internal::RegisterAndInitialize(VLogSite* absl_nonnull v);
  friend void log_internal::UpdateVLogSites();
  friend class log_internal::SyntheticBinary;
  static constexpr int kUninitialized = (std::numeric_limits<int>::max)();

  // SlowIsEnabled performs slower checks to determine whether a log site is
  // enabled, registering and initializing the site if needed. Because it is
  // expected to be called somewhat rarely (comparatively), it is not inlined to
  // save on code size.
  //
  // Takes locks but does not allocate memory.
  ABSL_ATTRIBUTE_NOINLINE bool SlowIsEnabled(int level);
  ABSL_ATTRIBUTE_NOINLINE bool SlowIsEnabled0();
  ABSL_ATTRIBUTE_NOINLINE bool SlowIsEnabled1();
  ABSL_ATTRIBUTE_NOINLINE bool SlowIsEnabled2();
  ABSL_ATTRIBUTE_NOINLINE bool SlowIsEnabled3();
  ABSL_ATTRIBUTE_NOINLINE bool SlowIsEnabled4();
  ABSL_ATTRIBUTE_NOINLINE bool SlowIsEnabled5();

  // `v_` is kept at offset 0 so the fast path accesses `site` without a struct
  // field displacement.
  // This object is too size-sensitive to use absl::string_view.
  std::atomic<int> v_;
  const char* absl_nonnull const file_;
  std::atomic<VLogSite*> next_;
};
static_assert(std::is_trivially_destructible_v<VLogSite>,
              "VLogSite must be trivially destructible");

#if ABSL_HAVE_CPP_ATTRIBUTE(gnu::visibility)
#define ABSL_LOG_INTERNAL_HIDDEN [[gnu::visibility("hidden")]]
#else
#define ABSL_LOG_INTERNAL_HIDDEN
#endif
// Prevents the x86-64 psABI from 16-byte-aligning char arrays of length >= 16.
#if ABSL_HAVE_CPP_ATTRIBUTE(gnu::aligned)
#define ABSL_LOG_INTERNAL_ALIGNED_1 [[gnu::aligned(1)]]
#else
#define ABSL_LOG_INTERNAL_ALIGNED_1
#endif

// TODO(b/576087803): Migrate to a C++20 class NTTP once Abseil requires C++20.
// Hidden visibility:
// 1. Keeps `site` and `kFile` out of `.dynsym`/`.dynstr` in shared libraries.
// 2. Allows direct PC-relative addressing instead of GOT indirection in PIC.
template <char... Cs>
struct ABSL_LOG_INTERNAL_HIDDEN VLogSiteHolder {
  ABSL_LOG_INTERNAL_ALIGNED_1 static constexpr char kFile[sizeof...(Cs) + 1] =
      {Cs..., '\0'};
  ABSL_CONST_INIT static inline VLogSite site{kFile};
};

#undef ABSL_LOG_INTERNAL_ALIGNED_1
#undef ABSL_LOG_INTERNAL_HIDDEN

template <typename F, size_t... Is>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline auto GetVLogSiteHolderImpl(
    F f, std::index_sequence<Is...>) {
  return VLogSiteHolder<f()[Is]...>{};
}

template <typename F>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline auto GetVLogSiteHolder(F f) {
  return GetVLogSiteHolderImpl(
      f, std::make_index_sequence<absl::string_view(f()).size()>{});
}

// Returns the current verbose log level of `file`.
// Does not allocate memory.
int VLogLevel(absl::string_view file);

// Registers a site `v` to get updated as `vmodule` and `v` change.  Also
// initializes the site based on their current values, and returns that result.
// Does not allocate memory.
int RegisterAndInitialize(VLogSite* absl_nonnull v);

// Allocates memory.
void UpdateVLogSites();

// Completely overwrites the saved value of `vmodule`.
// Allocates memory.
void UpdateVModule(absl::string_view vmodule);

// Updates the global verbosity level to `v` and returns the prior value.
// Allocates memory.
int UpdateGlobalVLogLevel(int v);

// Atomically prepends `module_pattern=log_level` to the start of vmodule.
// Returns the prior value for `module_pattern` if there was an exact match and
// `global_v` otherwise.
// Allocates memory.
int PrependVModule(absl::string_view module_pattern, int log_level);

// Registers `on_update` to be called whenever `v` or `vmodule` change.
// Allocates memory.
void OnVLogVerbosityUpdate(std::function<void()> cb);

// Does not allocate memory.
VLogSite* absl_nullable SetVModuleListHeadForTestOnly(
    VLogSite* absl_nullable v);

}  // namespace log_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_LOG_INTERNAL_VLOG_CONFIG_H_
