// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The coverage runtime of a d8 built with v8_redline=true. It started as a
// copy of src/fuzzilli/cov.cc. It writes the shared memory of cov.h.
//
// The compiler adds code to each edge of the C++ code. The mode comes from
// sanitizer_coverage_flags:
// - trace-pc-guard: an edge calls __sanitizer_cov_trace_pc_guard() with a
//   guard that holds the index of the edge. The callback sets the bit of the
//   edge and sets the guard to 0, so the edge does not call it again.
//   sanitizer_cov_reset_edgeguards() sets the guards again after each
//   execution.
// - inline-bool-flag: an edge sets a flag in the memory of d8.
//   sanitizer_cov_flush_bool_flags() sets the bits of the set flags and
//   clears the flags after each execution.
// - inline-8bit-counters: an edge increments a counter in the memory of d8.
//   sanitizer_cov_flush_bool_flags() adds the counters to the counters of the
//   shared memory, with a maximum of 255, and clears them. The counter of a
//   code edge thus holds the number of hits modulo 256.
// In the inline modes, the shared memory changes only when d8 flushes. Thus
// the crash signal handlers also flush (see
// sanitizer_cov_install_crash_flush_handlers()).
//
// d8 has no other instrumented libraries, so all its C++ edges are in one
// region. The builtins blocks and the optimized code features follow in their
// own regions.
//
// The module constructors of the compiler call the init callbacks before the
// static initializers of this file. Thus the global variables of this file
// must not need a static initializer (-Wglobal-constructors).

#include "experimental/redline/d8/cov.h"

#include <fcntl.h>
#include <inttypes.h>
#include <link.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"

#if defined(V8_USE_ADDRESS_SANITIZER) || defined(V8_USE_MEMORY_SANITIZER)
// For __sanitizer_set_death_callback, see
// sanitizer_cov_install_crash_flush_handlers.
#include <sanitizer/common_interface_defs.h>
#endif

namespace redline::cov {
namespace {

// The state of the runtime. It is all zero at startup, without a static
// initializer.
struct State {
  Header* header;  // the start of the shared memory, or nullptr
  uint8_t* data;   // the data of the shared memory
  Mode mode;
  // trace-pc-guard: the guards of d8, and the index of their first edge.
  uint32_t* guards_start;
  uint32_t* guards_stop;
  uint32_t guards_first;
  // inline-bool-flag and inline-8bit-counters: the flags or the counters of
  // d8, and the index of their first edge.
  uint8_t* inline_start;
  uint8_t* inline_stop;
  uint32_t inline_first;
  uint32_t builtins_first;
  uint32_t builtins_count;
  uint32_t optimized_first;
  uint32_t optimized_count;
};
State state;

[[noreturn]] void Fatal(const char* message) {
  fprintf(stderr, "[COV] %s\n", message);
  _exit(-1);
}

struct ProgramInfo {
  uintptr_t load_bias;
  uint32_t build_id_size;
  uint8_t build_id[kMaxBuildIdSize];
};

// Reads the load bias and the GNU build ID of the main program. The first
// object that dl_iterate_phdr() reports is the main program.
int ReadProgramInfo(struct dl_phdr_info* info, size_t, void* out_data) {
  ProgramInfo* out = static_cast<ProgramInfo*>(out_data);
  out->load_bias = info->dlpi_addr;
  for (int i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) & phdr = info->dlpi_phdr[i];
    if (phdr.p_type != PT_NOTE) {
      continue;
    }
    const uint8_t* note =
        reinterpret_cast<const uint8_t*>(info->dlpi_addr + phdr.p_vaddr);
    const uint8_t* const end = note + phdr.p_memsz;
    while (note + sizeof(ElfW(Nhdr)) <= end) {
      const ElfW(Nhdr)* note_header = reinterpret_cast<const ElfW(Nhdr)*>(note);
      const uint8_t* name = note + sizeof(ElfW(Nhdr));
      const uint8_t* desc = name + ((note_header->n_namesz + 3) & ~3u);
      if (note_header->n_type == NT_GNU_BUILD_ID &&
          note_header->n_namesz == 4 && memcmp(name, "GNU", 4) == 0 &&
          note_header->n_descsz <= kMaxBuildIdSize) {
        out->build_id_size = note_header->n_descsz;
        memcpy(out->build_id, desc, note_header->n_descsz);
        return 1;
      }
      note = desc + ((note_header->n_descsz + 3) & ~3u);
    }
  }
  return 1;
}

// Maps the shared memory of SHM_ID (or an anonymous region) and writes its
// header.
void InitializeShm(Mode mode) {
  size_t size = kShmSize;
  void* mapping = MAP_FAILED;
  const char* shm_key = getenv("SHM_ID");
  if (shm_key == nullptr) {
    fputs("[COV] no shared memory bitmap available, skipping\n", stderr);
    mapping = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  } else {
    const int fd = shm_open(shm_key, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
      Fatal("Failed to open shared memory region");
    }
    // The parent decides the size. A larger mapping would fault.
    struct stat shm_stat;
    if (fstat(fd, &shm_stat) != 0 ||
        shm_stat.st_size < static_cast<off_t>(kHeaderSize) + 16 ||
        shm_stat.st_size > static_cast<off_t>(UINT32_MAX)) {
      Fatal("The shared memory region has an invalid size");
    }
    size = static_cast<size_t>(shm_stat.st_size);
    mapping = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
  }
  if (mapping == MAP_FAILED) {
    Fatal("Failed to mmap shared memory region");
  }
  Header* header = static_cast<Header*>(mapping);
  memset(header, 0, kHeaderSize);
  header->magic = kMagic;
  header->version = kVersion;
  header->header_size = kHeaderSize;
  header->shm_size = static_cast<uint32_t>(size);
  header->mode = static_cast<uint32_t>(mode);
  header->bits_per_edge = BitsPerEdge(mode);
  header->max_edges = MaxEdges(size, header->bits_per_edge);
  ProgramInfo program = {};
  dl_iterate_phdr(&ReadProgramInfo, &program);
  header->build_id_size = program.build_id_size;
  memcpy(header->build_id, program.build_id, program.build_id_size);
  state.header = header;
  state.data = static_cast<uint8_t*>(mapping) + kHeaderSize;
  state.mode = mode;
}

// The init callbacks call this function with the mode of their
// instrumentation. The cov_init_*() functions call it with Mode::kNone.
void EnsureShm(Mode mode) {
  if (state.header == nullptr) {
    InitializeShm(mode);
    return;
  }
  if (mode != Mode::kNone && mode != state.mode) {
    Fatal("Cannot mix coverage modes (sanitizer_coverage_flags)");
  }
}

// Adds a region of `count` edges after the edges of the earlier regions.
// Returns the index of its first edge.
uint32_t AddRegion(RegionKind kind, uint64_t count, const char* what) {
  Header* header = state.header;
  if (count > header->max_edges - header->num_edges) {
    fprintf(stderr,
            "[COV] Error: Insufficient amount of edges left for %s coverage "
            "(the shared memory has %u bytes).\n",
            what, header->shm_size);
    _exit(-1);
  }
  if (header->num_regions >= kMaxRegions) {
    Fatal("Too many coverage regions");
  }
  const uint32_t first = header->num_edges + 1;
  header->regions[header->num_regions] = {static_cast<uint32_t>(kind), first,
                                          static_cast<uint32_t>(count)};
  header->num_regions++;
  header->num_edges += static_cast<uint32_t>(count);
  return first;
}

// Adds one hit of edge `index`: sets its bit, or increments its counter to at
// most 255.
__attribute__((no_sanitize("coverage"))) inline void MarkEdge(uint32_t index) {
  if (state.mode == Mode::kInline8bitCounters) {
    uint8_t& counter = state.data[index];
    if (counter != 255) {
      counter = static_cast<uint8_t>(counter + 1);
    }
  } else {
    state.data[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
  }
}

__attribute__((no_sanitize("coverage"))) inline bool IsEdgeSet(uint32_t index) {
  if (state.mode == Mode::kInline8bitCounters) {
    return state.data[index] != 0;
  }
  return (state.data[index >> 3] >> (index & 7)) & 1;
}

void LogInitialization(const char* what) {
  const char* shm_key = getenv("SHM_ID");
  fprintf(stderr,
          "[COV] edge %s initialized. Shared memory: %s with %u edges\n", what,
          shm_key ? shm_key : "anonymous shmem", state.header->num_edges);
}

void InitGuards(uint32_t* start, uint32_t* stop) {
  EnsureShm(Mode::kTracePcGuard);
  // No need to initialize again if it's the same region, which is something
  // that appears to happen on e.g. macOS.
  if (state.guards_start == start && state.guards_stop == stop) {
    return;
  }
  if (state.guards_start != nullptr) {
    Fatal(
        "Multiple initialization of shmem! This is probably not intended! "
        "Currently only one edge region is supported");
  }
  const uint32_t first =
      AddRegion(RegionKind::kCode, static_cast<uint64_t>(stop - start), "code");
  for (uint32_t* guard = start; guard < stop; guard++) {
    *guard = first + static_cast<uint32_t>(guard - start);
  }
  state.guards_first = first;
  state.guards_stop = stop;
  state.guards_start = start;
  LogInitialization("counters");
}

void InitInlineRegion(Mode mode, uint8_t* start, uint8_t* stop) {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // The inline flag stores and counter increments cannot grant access to the
  // default pkey like __sanitizer_cov_trace_pc_guard does. Thus they would
  // fault in sandboxed execution mode.
  USE(mode, start, stop);
  Fatal(
      "inline-bool-flag and inline-8bit-counters coverage are not supported "
      "with sandbox hardware support, use trace-pc-guard instead");
#else
  EnsureShm(mode);
  // As for trace-pc-guard, the same region may be reported multiple times.
  if (state.inline_start == start) {
    return;
  }
  if (state.inline_start != nullptr) {
    Fatal(
        "Multiple initialization of shmem! This is probably not intended! "
        "Currently only one edge region is supported");
  }
  state.inline_first =
      AddRegion(RegionKind::kCode, static_cast<uint64_t>(stop - start), "code");
  // Fully initialize the region before FlushInline() can see it.
  state.inline_stop = stop;
  state.inline_start = start;
  LogInitialization(mode == Mode::kInline8bitCounters ? "counters" : "flags");
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
}

// Moves the flags or the counters of d8 into the shared memory (if `report`
// is true) and clears them. Must be async-signal-safe, because the crash
// signal handlers also call it.
__attribute__((no_sanitize("coverage"))) void FlushInline(bool report) {
  uint8_t* p = state.inline_start;
  if (p == nullptr) {
    return;
  }
  uint8_t* const stop = state.inline_stop;
  uint8_t* const out = report ? state.data : nullptr;
  const bool counters = state.mode == Mode::kInline8bitCounters;
  uint32_t index = state.inline_first;
  for (; p < stop; p++, index++) {
    // Only a small fraction of all edges runs in one execution, so skip
    // eight zero bytes at a time where possible. The bytes are cleared one by
    // one, so that a byte that another thread sets is not lost.
    if ((reinterpret_cast<uintptr_t>(p) & 7) == 0 && stop - p >= 8) {
      uint64_t word;
      memcpy(&word, p, sizeof(word));
      if (word == 0) {
        p += 7;
        index += 7;
        continue;
      }
    }
    const uint8_t value = *p;
    if (value == 0) {
      continue;
    }
    *p = 0;
    if (out == nullptr) {
      continue;
    }
    if (counters) {
      const unsigned sum = static_cast<unsigned>(out[index]) + value;
      out[index] = static_cast<uint8_t>(sum > 255 ? 255 : sum);
    } else {
      out[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
    }
  }
}

constexpr int kCrashSignals[] = {SIGSEGV, SIGBUS, SIGILL,
                                 SIGTRAP, SIGFPE, SIGABRT};
struct sigaction previous_crash_handlers[std::size(kCrashSignals)];

__attribute__((no_sanitize("coverage"))) void CrashFlushSignalHandler(
    int signo, siginfo_t* info, void*) {
  FlushInline(true);

  // Reinstall the handler that was active before ours and let it handle the
  // signal. Then the crash behaves exactly as before: same signal, same
  // stack dump, same exit status.
  for (size_t i = 0; i < std::size(kCrashSignals); i++) {
    if (kCrashSignals[i] == signo) {
      sigaction(signo, &previous_crash_handlers[i], nullptr);
      break;
    }
  }
  // For hardware faults, returning re-executes the faulting instruction, which
  // then raises the signal again. Signals sent by software (e.g. by abort())
  // must be raised again explicitly. The same is true for SIGTRAP, as the
  // program counter points after the trapping instruction (int3). The signal
  // is blocked while this handler runs, so it is delivered once we return.
  if (info->si_code <= 0 || signo == SIGTRAP || signo == SIGABRT) {
    raise(signo);
  }
}

void InstallCrashFlushHandlers() {
  // Only required if the shared memory is not updated in real time.
  if (state.inline_start == nullptr) {
    return;
  }
  // Installing the handlers twice would make them defer to themselves.
  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;
#if defined(V8_USE_ADDRESS_SANITIZER) || defined(V8_USE_MEMORY_SANITIZER)
  // Errors detected by a sanitizer only end in a signal (SIGABRT) with
  // abort_on_error=1. Flush from the sanitizer's death callback as well, which
  // runs in any case.
  __sanitizer_set_death_callback(&sanitizer_cov_flush_bool_flags);
#endif
  // Without an alternate signal stack, signal handlers cannot run after a
  // native stack overflow. Then the coverage of such crashes would be lost.
  // The stack dumping and Wasm trap handlers also use SA_ONSTACK, so they
  // also run on this stack. This covers only the main thread. The coverage
  // of a stack overflow in a Worker thread is lost.
  v8::base::OS::EnsureAlternativeSignalStackIsAvailableForCurrentThread();
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = &CrashFlushSignalHandler;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&action.sa_mask);
  for (size_t i = 0; i < std::size(kCrashSignals); i++) {
    if (sigaction(kCrashSignals[i], &action, &previous_crash_handlers[i]) !=
        0) {
      Fatal("Failed to install crash signal handler");
    }
  }
}

// With REDLINE_PC_TABLE=FILE, writes the PC table of the compiler to FILE.
// The first line has the build ID and the number of edges. Then each line has
// the link-time address of an edge and its flags (1 for a function entry).
// Redline maps the addresses to source lines (tools/edge_map.py).
void WritePcTable(const uintptr_t* pcs_beg, const uintptr_t* pcs_end) {
  const char* path = getenv("REDLINE_PC_TABLE");
  if (path == nullptr || *path == '\0') {
    return;
  }
  // A repeated call with the same table does nothing, like a repeated call of
  // __sanitizer_cov_trace_pc_guard_init with the same region.
  static const uintptr_t* table = nullptr;
  if (table == pcs_beg) {
    return;
  }
  if (table != nullptr) {
    Fatal("Only one PC table is supported");
  }
  table = pcs_beg;
  ProgramInfo program = {};
  dl_iterate_phdr(&ReadProgramInfo, &program);
  char build_id[2 * kMaxBuildIdSize + 1] = "";
  static constexpr char kHex[] = "0123456789abcdef";
  for (uint32_t i = 0; i < program.build_id_size; i++) {
    build_id[2 * i] = kHex[program.build_id[i] >> 4];
    build_id[2 * i + 1] = kHex[program.build_id[i] & 15];
  }
  build_id[2 * program.build_id_size] = '\0';
  FILE* file = fopen(path, "w");
  if (file == nullptr) {
    fprintf(stderr, "[COV] Cannot write the PC table to %s\n", path);
    _exit(-1);
  }
  const size_t count = static_cast<size_t>(pcs_end - pcs_beg) / 2;
  fprintf(file, "# redline-pc-table v1 build_id=%s count=%zu\n", build_id,
          count);
  for (const uintptr_t* entry = pcs_beg; entry + 1 < pcs_end; entry += 2) {
    fprintf(file, "0x%" PRIxPTR " %" PRIuPTR "\n", entry[0] - program.load_bias,
            entry[1]);
  }
  fclose(file);
}

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
// We need to allow the coverage instrumentation to run in sandboxed execution
// mode, for example to be able to run sandboxed C++ code. As the shared memory
// and the edge guards are tagged with the default pkey, we may need to
// temporarily grant access to the default pkey. These helper functions take
// care of this. We use inline assembly and avoid any function calls to
// minimize performance impact. See src/fuzzilli/cov.cc for the alternatives.
inline uint32_t GrantDefaultPkeyAccessIfNecessary() {
  uint32_t pkru;
  asm volatile(
      "xor %%ecx, %%ecx\n"
      "rdpkru\n"
      : "=a"(pkru)
      :
      : "ecx", "edx");
  if (pkru & 3) {
    // We don't have (write) access to the default pkey currently.
    uint32_t new_pkru = pkru & ~3u;
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

inline void RestorePreviousPkeyAccessIfNecessary(uint32_t old_pkru) {
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

struct PendingOptimizedCodeCoverage {
  v8::base::Mutex mutex;
  std::vector<uint32_t> edges;
};

PendingOptimizedCodeCoverage& GetPendingOptimizedCodeCoverage() {
  static PendingOptimizedCodeCoverage* pending =
      new PendingOptimizedCodeCoverage();
  return *pending;
}

}  // namespace
}  // namespace redline::cov

namespace cov = redline::cov;

// The callbacks of the compiler instrumentation.

__attribute__((visibility("default"))) extern "C" void
__sanitizer_cov_trace_pc_guard_init(uint32_t* start, uint32_t* stop) {
  cov::InitGuards(start, stop);
}

__attribute__((visibility("default"))) extern "C" void
__sanitizer_cov_trace_pc_guard(uint32_t* guard) {
  const uint32_t index = *guard;
  // The guard is 0 before the initialization and after the first hit of the
  // edge in this execution. A race of two threads on one edge also gives 0.
  if (index == 0) {
    return;
  }
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  const uint32_t old_pkru = cov::GrantDefaultPkeyAccessIfNecessary();
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  cov::state.data[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
  *guard = 0;
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  cov::RestorePreviousPkeyAccessIfNecessary(old_pkru);
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
}

__attribute__((visibility("default"))) extern "C" void
__sanitizer_cov_bool_flag_init(bool* start, bool* stop) {
  cov::InitInlineRegion(cov::Mode::kInlineBoolFlag,
                        reinterpret_cast<uint8_t*>(start),
                        reinterpret_cast<uint8_t*>(stop));
}

__attribute__((visibility("default"))) extern "C" void
__sanitizer_cov_8bit_counters_init(uint8_t* start, uint8_t* stop) {
  cov::InitInlineRegion(cov::Mode::kInline8bitCounters, start, stop);
}

// Called with -fsanitize-coverage=pc-table. [pcs_beg, pcs_end) holds one pair
// {pc, flags} per edge, in the order of the edges.
__attribute__((visibility("default"))) extern "C" void __sanitizer_cov_pcs_init(
    const uintptr_t* pcs_beg, const uintptr_t* pcs_end) {
  if (cov::state.header != nullptr) {
    cov::state.header->flags |= cov::kFlagPcTable;
  }
  cov::WritePcTable(pcs_beg, pcs_end);
}

// The functions of cov.h.

__attribute__((visibility("default"))) void redline_cov_enable() {
  // This function exists solely to force the linker to include this object
  // file.
}

// This runs after every REPRL execution and touches every guard, so it must
// not be instrumented itself. The loop bounds are copied to locals so that the
// compiler can vectorize the loop.
__attribute__((visibility("default"), no_sanitize("coverage"))) void
sanitizer_cov_reset_edgeguards() {
  uint32_t* const start = cov::state.guards_start;
  uint32_t* const stop = cov::state.guards_stop;
  const uint32_t first = cov::state.guards_first;
  if (start == nullptr) {
    return;
  }
  const size_t count = static_cast<size_t>(stop - start);
  for (size_t i = 0; i < count; i++) {
    start[i] = first + static_cast<uint32_t>(i);
  }
}

__attribute__((visibility("default"), no_sanitize("coverage"))) void
sanitizer_cov_flush_bool_flags() {
  cov::FlushInline(true);
}

__attribute__((visibility("default"), no_sanitize("coverage"))) void
sanitizer_cov_discard_bool_flags() {
  cov::FlushInline(false);
}

// IMPORTANT: this is order-sensitive. The handlers defer to the handlers that
// were installed before them, so they must be installed after handlers that do
// not forward signals (e.g. V8's in-process stack dumper) and before handlers
// that forward unhandled signals to the previous handler (e.g. the Wasm trap
// handler). See Shell::Main.
__attribute__((visibility("default"))) void
sanitizer_cov_install_crash_flush_handlers() {
  cov::InstallCrashFlushHandlers();
}

// The number of code edges that are set.
__attribute__((visibility("default"))) uint32_t
sanitizer_cov_count_discovered_edges() {
  const cov::Header* header = cov::state.header;
  if (header == nullptr) {
    return 0;
  }
  uint32_t count = 0;
  for (uint32_t r = 0; r < header->num_regions; r++) {
    const cov::Region& region = header->regions[r];
    if (region.kind != static_cast<uint32_t>(cov::RegionKind::kCode)) {
      continue;
    }
    for (uint32_t i = 0; i < region.count; i++) {
      if (cov::IsEdgeSet(region.first + i)) {
        count++;
      }
    }
  }
  return count;
}

void cov_init_builtins_edges(uint32_t num_edges) {
  // This function should only be called once. A second call would reserve a
  // second region.
  CHECK_EQ(cov::state.builtins_count, 0u);
  cov::EnsureShm(cov::Mode::kNone);
  cov::state.builtins_first =
      cov::AddRegion(cov::RegionKind::kBuiltins, num_edges, "builtins");
  cov::state.builtins_count = num_edges;
  fprintf(stderr, "[COV] Additional %u edges for builtins initialized.\n",
          num_edges);
}

bool cov_has_builtins_edges() {
  return cov::state.builtins_count > 0 && cov::state.header != nullptr;
}

// In counters mode, the counter of a block is 1 if the block ran. The caller
// reports each executed block once, without the number of runs.
void cov_set_builtin_edge(uint32_t block_index) {
  cov::MarkEdge(cov::state.builtins_first + block_index);
}

void cov_init_optimized_code_edges(uint32_t num_edges) {
  CHECK_EQ(cov::state.optimized_count, 0u);
  cov::EnsureShm(cov::Mode::kNone);
  cov::state.optimized_first =
      cov::AddRegion(cov::RegionKind::kOptimized, num_edges, "optimized code");
  cov::state.optimized_count = num_edges;
  fprintf(stderr, "[COV] Additional %u edges for optimized code initialized.\n",
          num_edges);
}

void cov_add_optimized_code_coverage_edges(const std::set<uint32_t>& hashes) {
  if (cov::state.optimized_count == 0) {
    return;
  }
  auto& pending = cov::GetPendingOptimizedCodeCoverage();
  v8::base::MutexGuard lock(&pending.mutex);
  pending.edges.insert(pending.edges.end(), hashes.begin(), hashes.end());
}

// In counters mode, the counter of a feature is the number of compiled graphs
// that contain it.
size_t cov_update_and_reset_optimized_code_coverage() {
  if (cov::state.optimized_count == 0) {
    return 0;
  }
  auto& pending = cov::GetPendingOptimizedCodeCoverage();
  v8::base::MutexGuard lock(&pending.mutex);
  for (uint32_t hash : pending.edges) {
    cov::MarkEdge((hash % cov::state.optimized_count) +
                  cov::state.optimized_first);
  }
  const size_t count = pending.edges.size();
  pending.edges.clear();
  return count;
}
