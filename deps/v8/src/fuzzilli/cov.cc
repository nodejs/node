// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuzzilli/cov.h"

#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <iterator>

#include "src/base/platform/memory.h"
#include "src/base/platform/platform.h"
#include "src/sandbox/hardware-support.h"

#if defined(V8_USE_ADDRESS_SANITIZER) || defined(V8_USE_MEMORY_SANITIZER)
// For __sanitizer_set_death_callback, see
// sanitizer_cov_install_crash_flush_handlers.
#include <sanitizer/common_interface_defs.h>
#endif

#define SHM_SIZE 0x200000
#define MAX_EDGES ((SHM_SIZE - 4) * 8)

struct shmem_data {
  uint32_t num_edges;
  unsigned char edges[];
};

shmem_data* shmem;

uint32_t *edges_start, *edges_stop;
uint32_t builtins_start;
uint32_t builtins_edge_count;

namespace {
// Number of registered inline-bool-flag regions, see
// __sanitizer_cov_bool_flag_init.
size_t num_bool_flag_regions = 0;
}  // namespace

// We support two modes:
// 1. Single-DSO mode (standard V8):
//    - Assumes all instrumented code is in a single DSO.
//    - Optimization: `*guard = 0` in `__sanitizer_cov_trace_pc_guard` disables
//      the edge after the first hit. This prevents redundant writes to shared
//      memory for hot edges. Fuzzilli resets these guards between iterations
//      via `sanitizer_cov_reset_edgeguards`.
// 2. Multi-DSO mode (Chromium):
//    - Supports coverage for multiple DSOs (Chromium, libraries, etc.).
//    - Accumulates edges across all DSOs instead of crashing on re-init.
//    - Optimization DISABLED: We cannot easily reset guards for all loaded
//      DSOs (no global registry of all guard arrays). Thus, we leave
//      `*guard` non-zero ("edge persistence"). This means `trace_pc_guard`
//      writes to shared memory every time an edge is hit, which is slower but
//      necessary for correctness in this mode.
#ifdef USE_CHROMIUM_FUZZILLI
constexpr bool support_multi_dso = true;
#else
constexpr bool support_multi_dso = false;
#endif

static void initialize_shmem() {
  // Map the shared memory region
  const char* shm_key = getenv("SHM_ID");
  if (!shm_key) {
    fprintf(stderr, "[COV] no shared memory bitmap available, skipping\n");
    shmem =
        static_cast<shmem_data*>(mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE,
                                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
  } else {
    int fd = shm_open(shm_key, O_RDWR, S_IREAD | S_IWRITE);
    if (fd <= -1) {
      fprintf(stderr, "[COV] Failed to open shared memory region\n");
      _exit(-1);
    }

    shmem = static_cast<shmem_data*>(
        mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  }

  if (shmem == MAP_FAILED) {
    fprintf(stderr, "[COV] Failed to mmap shared memory region\n");
    _exit(-1);
  }
  shmem->num_edges = 0;
}

__attribute__((visibility("default"))) void fuzzilli_cov_enable() {
  // This function exists solely to force the linker to include this object
  // file.
}

// This runs after every REPRL execution and touches every guard, so it must
// not be instrumented itself: otherwise every loop iteration would invoke the
// coverage callback (~1.9M calls per execution). The loop bounds are copied to
// locals so that the compiler can vectorize the loop.
__attribute__((visibility("default"), no_sanitize("coverage"))) void
sanitizer_cov_reset_edgeguards() {
  uint32_t* const start = edges_start;
  uint32_t* const stop = edges_stop;
  if (start == nullptr || stop == nullptr || stop < start) return;
  const size_t count = std::min<size_t>(stop - start, MAX_EDGES);
  for (size_t i = 0; i < count; i++) {
    start[i] = static_cast<uint32_t>(i + 1);
  }
}

__attribute__((visibility("default"))) extern "C" void
__sanitizer_cov_trace_pc_guard_init(uint32_t* start, uint32_t* stop) {
  if (!shmem) {
    initialize_shmem();
  }

  // No need to initialize again if it's the same region, which is something
  // that appears to happen on e.g. macOS.
  if (edges_start == start && edges_stop == stop) {
    return;
  }

  if (num_bool_flag_regions > 0) {
    fprintf(stderr,
            "[COV] Cannot mix trace-pc-guard and inline-bool-flag coverage\n");
    _exit(-1);
  }

  if (!support_multi_dso && edges_start != nullptr) {
    // In single-DSO mode, we should initialize the shared memory region only
    // once. If we ever see a different region, we will overwrite the previous
    // one, which is probably not intended and as such we fail with an error.
    fprintf(stderr,
            "[COV] Multiple initialization of shmem!"
            " This is probably not intended! Currently only one edge"
            " region is supported\n");
    _exit(-1);
  }

  edges_start = start;
  edges_stop = stop;
  for (uint32_t* x = start; x < stop && shmem->num_edges < MAX_EDGES; x++) {
    *x = ++shmem->num_edges;
  }

  if (builtins_edge_count == 0) {
    builtins_start = 1 + shmem->num_edges;
  }

  const char* shm_key = getenv("SHM_ID");
  fprintf(stderr,
          "[COV] edge counters initialized. Shared memory: %s with %u edges\n",
          shm_key ? shm_key : "anonymous shmem", shmem->num_edges);
}

// Support for -fsanitize-coverage=inline-bool-flag.
//
// With trace-pc-guard, every executed edge calls __sanitizer_cov_trace_pc_guard
// (even after the guard has been zeroed), which writes directly into the shared
// memory bitmap. These calls are expensive: avoiding them reduces the CPU time
// of a typical execution by ~40%. With inline-bool-flag, the compiler instead
// emits an inline `if (!flag) flag = true;` for every edge, where the flags are
// bytes in d8's own memory. (The check is emitted by the compiler, it avoids
// dirtying the cache line of a flag that is already set.) The flags are
// translated into the shared memory bitmap (and cleared) only once per
// execution by sanitizer_cov_flush_bool_flags(). Flag i of a region is reported
// as bitmap index `first_index + i`, exactly like the guard values in
// trace-pc-guard mode, so the coverage bitmap layout (and Fuzzilli) do not
// change.
//
// Since the bitmap is no longer updated in real time, the flags must also be
// flushed when the process crashes, because Fuzzilli uses the coverage of
// crashing executions to deduplicate crashes. See
// sanitizer_cov_install_crash_flush_handlers().
namespace {

struct BoolFlagRegion {
  uint8_t* start;
  uint8_t* stop;
  // Bitmap index of the first flag of this region.
  uint32_t first_index;
};

constexpr size_t kMaxBoolFlagRegions = 64;
BoolFlagRegion bool_flag_regions[kMaxBoolFlagRegions];

// Same as cov_set_edge, but guaranteed to not be instrumented (cov_set_edge
// would be if it was not inlined).
__attribute__((no_sanitize("coverage"))) inline void SetEdge(uint8_t* bitmap,
                                                             uint32_t index) {
  bitmap[index >> 3] |= static_cast<uint8_t>(1 << (index & 7));
}

// Translates the set flags of a region into bitmap bits (unless `bitmap` is
// nullptr) and clears them. Must be async-signal-safe (it is also used from
// crash signal handlers).
__attribute__((no_sanitize("coverage"))) inline void FlushBoolFlagRegion(
    const BoolFlagRegion& region, uint8_t* bitmap) {
  uint8_t* p = region.start;
  uint8_t* const stop = region.stop;
  uint32_t index = region.first_index;
  // Only a small fraction of all edges is hit by an execution, so scan eight
  // flags at a time and only look at the individual flags of non-zero words.
  // Flags are cleared individually (instead of zeroing the whole word) so that
  // flags concurrently set by other threads are not lost.
  while (p < stop && (reinterpret_cast<uintptr_t>(p) & 7) != 0) {
    if (*p) {
      *p = 0;
      if (bitmap) SetEdge(bitmap, index);
    }
    p++;
    index++;
  }
  for (; stop - p >= 8; p += 8, index += 8) {
    uint64_t word;
    memcpy(&word, p, sizeof(word));
    if (word == 0) continue;
    for (int j = 0; j < 8; j++) {
      if (p[j]) {
        p[j] = 0;
        if (bitmap) SetEdge(bitmap, index + j);
      }
    }
  }
  for (; p < stop; p++, index++) {
    if (*p) {
      *p = 0;
      if (bitmap) SetEdge(bitmap, index);
    }
  }
}

}  // namespace

// Called by the module constructors emitted by the compiler for
// -fsanitize-coverage=inline-bool-flag, once per instrumented DSO and before
// any instrumented code of that DSO runs. [start, stop) are the (zero
// initialized) edge flags of the DSO, which the instrumented code sets when the
// corresponding edge is executed. This reserves a range of bitmap indices for
// the flags of the region (after the edges of previously registered regions)
// and registers the region, so that sanitizer_cov_flush_bool_flags() can
// translate its flags into the coverage bitmap.
__attribute__((visibility("default"))) extern "C" void
__sanitizer_cov_bool_flag_init(bool* start, bool* stop) {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // The inline flag stores cannot temporarily grant access to the default pkey
  // like __sanitizer_cov_trace_pc_guard does, so they would fault in sandboxed
  // execution mode. (This combination is also rejected by BUILD.gn.)
  fprintf(stderr,
          "[COV] inline-bool-flag coverage is not supported with sandbox "
          "hardware support, use trace-pc-guard instead\n");
  _exit(-1);
#else
  if (!shmem) {
    initialize_shmem();
  }

  uint8_t* region_start = reinterpret_cast<uint8_t*>(start);
  uint8_t* region_stop = reinterpret_cast<uint8_t*>(stop);
  // As for trace-pc-guard, the same region may be reported multiple times.
  for (size_t i = 0; i < num_bool_flag_regions; i++) {
    if (bool_flag_regions[i].start == region_start) return;
  }

  if (edges_start != nullptr) {
    fprintf(stderr,
            "[COV] Cannot mix trace-pc-guard and inline-bool-flag coverage\n");
    _exit(-1);
  }
  if (!support_multi_dso && num_bool_flag_regions > 0) {
    fprintf(stderr,
            "[COV] Multiple initialization of shmem!"
            " This is probably not intended! Currently only one edge"
            " region is supported\n");
    _exit(-1);
  }
  if (num_bool_flag_regions >= kMaxBoolFlagRegions) {
    fprintf(stderr, "[COV] Too many edge flag regions (at most %zu)\n",
            kMaxBoolFlagRegions);
    _exit(-1);
  }

  const size_t available = MAX_EDGES - shmem->num_edges;
  if (static_cast<size_t>(region_stop - region_start) > available) {
    // Like in trace-pc-guard mode, edges beyond MAX_EDGES are not reported.
    // This does not happen for d8 (~1.9M edges, MAX_EDGES is ~16.8M), but may
    // in multi-DSO mode.
    region_stop = region_start + available;
  }
  // Fully initialize the region before making it visible to
  // sanitizer_cov_flush_bool_flags(), which may run from a crash handler.
  BoolFlagRegion& region = bool_flag_regions[num_bool_flag_regions];
  region.start = region_start;
  region.stop = region_stop;
  region.first_index = shmem->num_edges + 1;
  shmem->num_edges += static_cast<uint32_t>(region_stop - region_start);
  num_bool_flag_regions++;

  if (builtins_edge_count == 0) {
    builtins_start = 1 + shmem->num_edges;
  }

  const char* shm_key = getenv("SHM_ID");
  fprintf(stderr,
          "[COV] edge flags initialized. Shared memory: %s with %u edges\n",
          shm_key ? shm_key : "anonymous shmem", shmem->num_edges);
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
}

__attribute__((visibility("default"), no_sanitize("coverage"))) void
sanitizer_cov_flush_bool_flags() {
  if (shmem == nullptr) return;
  for (size_t i = 0; i < num_bool_flag_regions; i++) {
    FlushBoolFlagRegion(bool_flag_regions[i], shmem->edges);
  }
}

__attribute__((visibility("default"), no_sanitize("coverage"))) void
sanitizer_cov_discard_bool_flags() {
  for (size_t i = 0; i < num_bool_flag_regions; i++) {
    FlushBoolFlagRegion(bool_flag_regions[i], nullptr);
  }
}

namespace {

constexpr int kCrashSignals[] = {SIGSEGV, SIGBUS, SIGILL,
                                 SIGTRAP, SIGFPE, SIGABRT};
struct sigaction previous_crash_handlers[std::size(kCrashSignals)];

__attribute__((no_sanitize("coverage"))) void CrashFlushSignalHandler(
    int signal, siginfo_t* info, void* context) {
  sanitizer_cov_flush_bool_flags();

  // Reinstall the handler that was active before ours and let it handle the
  // signal, so that the crash behaves exactly as before (same signal, same
  // stack dump, same exit status).
  for (size_t i = 0; i < std::size(kCrashSignals); i++) {
    if (kCrashSignals[i] == signal) {
      sigaction(signal, &previous_crash_handlers[i], nullptr);
      break;
    }
  }
  // For hardware faults, returning re-executes the faulting instruction, which
  // then raises the signal again. Signals sent by software (e.g. by abort())
  // must be raised again explicitly. The same is true for SIGTRAP, as the
  // program counter points after the trapping instruction (int3). The signal
  // is blocked while this handler runs, so it is delivered once we return.
  if (info->si_code <= 0 || signal == SIGTRAP || signal == SIGABRT) {
    raise(signal);
  }
}

}  // namespace

// IMPORTANT: this is order-sensitive. The handlers defer to the handlers that
// were installed before them, so they must be installed after handlers that do
// not forward signals (e.g. V8's in-process stack dumper) and before handlers
// that forward unhandled signals to the previous handler (e.g. the Wasm trap
// handler). See Shell::Main.
__attribute__((visibility("default"))) void
sanitizer_cov_install_crash_flush_handlers() {
  // Only required if the bitmap is not updated in real time.
  if (num_bool_flag_regions == 0) return;
  // Installing the handlers twice would make them defer to themselves.
  static bool installed = false;
  if (installed) return;
  installed = true;
#if defined(V8_USE_ADDRESS_SANITIZER) || defined(V8_USE_MEMORY_SANITIZER)
  // Errors detected by a sanitizer only end in a signal (SIGABRT) with
  // abort_on_error=1, which Fuzzilli requires for sanitizer builds. Flush from
  // the sanitizer's death callback as well, which runs in any case.
  __sanitizer_set_death_callback(&sanitizer_cov_flush_bool_flags);
#endif
  // Without an alternate signal stack, signal handlers cannot run after a
  // native stack overflow, and the coverage of such crashes would be lost.
  // (The stack dumping and Wasm trap handlers also use SA_ONSTACK, so they
  // will then run on this stack as well.) Note that this only covers the main
  // thread, so the coverage of a stack overflow in a Worker thread is lost.
  v8::base::OS::EnsureAlternativeSignalStackIsAvailableForCurrentThread();
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = &CrashFlushSignalHandler;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&action.sa_mask);
  for (size_t i = 0; i < std::size(kCrashSignals); i++) {
    if (sigaction(kCrashSignals[i], &action, &previous_crash_handlers[i]) !=
        0) {
      fprintf(stderr, "[COV] Failed to install crash signal handler\n");
      _exit(-1);
    }
  }
}

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
// We need to allow the coverage instrumentation to run in sandboxed execution
// mode, for example to be able to run sandboxed C++ code. As the coverage
// bitmap and the edge guards are tagged with the default pkey, we may need to
// temporarily grant access to the default pkey. These helper functions take
// care of this. We use inline assembly and avoid any function calls to
// minimize performance impact.
// Changing the pkey state frequently is fairly expensive. However, at the
// moment we have very very little C++ code that actually runs in sandboxed
// execution mode. As an alternative approach, we could also tag the coverage
// bitmap as "sandbox extension" memory such that both sandboxed and privileged
// code can access them. However, this interacts badly with signal handlers as
// they run without pkey access. As such, for that to work we would probably
// have to replace all signal handlers with an assembly trampoline that first
// restores full pkey access before jumping to the real handler code, or
// disable coverage instrumentation for signal handlers (probably not great).
static inline uint32_t GrantDefaultPkeyAccessIfNecessary() {
  uint32_t pkru;
  asm volatile(
      "xor %%ecx, %%ecx\n"
      "rdpkru\n"
      : "=a"(pkru)
      :
      : "ecx", "edx");
  if (pkru & 3) {
    // We don't have (write) access to the default pkey currently.
    uint32_t new_pkru = pkru & ~3;
    asm volatile(
        "xor %%ecx, %%ecx\n"
        "xor %%edx, %%edx\n"
        "wrpkru\n"
        :
        : "a"(new_pkru)
        : "ecx", "edx");
  }
  return pkru;
}

static inline void RestorePreviousPkeyAccessIfNecessary(uint32_t old_pkru) {
  if (old_pkru & 3) {
    asm volatile(
        "xor %%ecx, %%ecx\n"
        "xor %%edx, %%edx\n"
        "wrpkru\n"
        :
        : "a"(old_pkru)
        : "ecx", "edx");
  }
}
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

__attribute__((visibility("default"))) uint32_t
sanitizer_cov_count_discovered_edges() {
  uint32_t on_edges_counter = 0;
  for (uint32_t i = 1; i < builtins_start; ++i) {
    if (cov_is_edge_set(shmem->edges, i)) {
      ++on_edges_counter;
    }
  }
  return on_edges_counter;
}

__attribute__((visibility("default"))) extern "C" void
__sanitizer_cov_trace_pc_guard(uint32_t* guard) {
  uint32_t index = *guard;

  // This check is useful for two reasons:
  // * It can sometimes happen that this callback is invoked before coverage
  //   feedback is initialized, in which case shmem is likely nullptr (and the
  //   edges are all zero). So in that case this check prevents a crash.
  // * We can get here even for a disabled edge (*guard == 0), either because
  //   the compiler didn't insert a guard check already (it doesn't have to
  //   according to the documentation) or because of a small race: if this
  //   function executes in two threads for the same edge at the same time, the
  //   first thread might disable the edge (by setting the guard to zero) before
  //   the second thread fetches the guard value. In that case the second thread
  //   will see an index of zero here.
  if (!index) return;

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  uint32_t old_pkru = GrantDefaultPkeyAccessIfNecessary();
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

  cov_set_edge(shmem->edges, index);

#ifndef USE_CHROMIUM_FUZZILLI
  // This is a hot path, so use a macro instead of an if statement.
  *guard = 0;
#endif

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  RestorePreviousPkeyAccessIfNecessary(old_pkru);
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
}

void cov_init_builtins_edges(uint32_t num_edges) {
  // This function should only be called once. If called more than once, it
  // would incorrectly shift the `builtins_start` offset and allocate duplicate
  // space in the shared memory bitmap.
  CHECK(builtins_edge_count == 0);
  if (num_edges + shmem->num_edges > MAX_EDGES) {
    fprintf(stderr,
            "[COV] Error: Insufficient amount of edges left for builtins "
            "coverage.\n");
    exit(-1);
  }
  builtins_edge_count = num_edges;
  builtins_start = 1 + shmem->num_edges;
  shmem->num_edges += builtins_edge_count;
  fprintf(stderr, "[COV] Additional %d edges for builtins initialized.\n",
          num_edges);
}

bool cov_has_builtins_edges() {
  return builtins_edge_count > 0 && shmem != nullptr;
}

void cov_set_builtin_edge(uint32_t block_index) {
  cov_set_edge(shmem->edges, builtins_start + block_index);
}
